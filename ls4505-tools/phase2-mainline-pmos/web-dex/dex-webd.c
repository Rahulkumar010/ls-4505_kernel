/*
 * dex-webd.c ? Ultra-lightweight Zero-Dependency HTTP + RFC-6455 WebSocket
 *              to VNC (RFB) Proxy for LS-4505 Mainline Linux "Web-DeX"
 *
 * Replaces 150 MB Python websockify + numpy stack with a ~35 KB static ARMv7
 * C daemon (< 500 KB RSS, TCP_NODELAY + writev single-segment framing).
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

/* ---- Minimal RFC-3174 SHA-1 + Base64 for Sec-WebSocket-Accept ---- */
typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} sha1_ctx;

static uint32_t rol32(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }

static void sha1_transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80], a, b, c, d, e, t;
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++)
        w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;            k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;            k = 0xCA62C1D6; }
        t = rol32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol32(b, 30); b = a; a = t;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_compute(const uint8_t *data, size_t len, uint8_t digest[20]) {
    sha1_ctx ctx;
    ctx.state[0] = 0x67452301; ctx.state[1] = 0xEFCDAB89;
    ctx.state[2] = 0x98BADCFE; ctx.state[3] = 0x10325476;
    ctx.state[4] = 0xC3D2E1F0; ctx.count = (uint64_t)len * 8;

    size_t i = 0;
    while (len - i >= 64) {
        sha1_transform(ctx.state, data + i);
        i += 64;
    }
    memset(ctx.buffer, 0, 64);
    memcpy(ctx.buffer, data + i, len - i);
    ctx.buffer[len - i] = 0x80;
    if (len - i >= 56) {
        sha1_transform(ctx.state, ctx.buffer);
        memset(ctx.buffer, 0, 64);
    }
    for (int j = 0; j < 8; j++)
        ctx.buffer[63 - j] = (uint8_t)(ctx.count >> (j * 8));
    sha1_transform(ctx.state, ctx.buffer);
    for (int j = 0; j < 5; j++) {
        digest[j * 4]     = (uint8_t)(ctx.state[j] >> 24);
        digest[j * 4 + 1] = (uint8_t)(ctx.state[j] >> 16);
        digest[j * 4 + 2] = (uint8_t)(ctx.state[j] >> 8);
        digest[j * 4 + 3] = (uint8_t)(ctx.state[j]);
    }
}

static void base64_encode_20(const uint8_t in[20], char out[29]) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int o = 0;
    for (int i = 0; i < 18; i += 3) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        out[o++] = tbl[(v >> 6) & 63];
        out[o++] = tbl[v & 63];
    }
    uint32_t v = ((uint32_t)in[18] << 16) | ((uint32_t)in[19] << 8);
    out[o++] = tbl[(v >> 18) & 63];
    out[o++] = tbl[(v >> 12) & 63];
    out[o++] = tbl[(v >> 6) & 63];
    out[o++] = '=';
    out[o] = '\0';
}

/* ---- Socket I/O helpers ---- */
static int write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int writev_all(int fd, struct iovec *iov, int iovcnt) {
    while (iovcnt > 0) {
        ssize_t n = writev(fd, iov, iovcnt);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        size_t rem = (size_t)n;
        while (iovcnt > 0 && rem >= iov[0].iov_len) {
            rem -= iov[0].iov_len;
            iov++;
            iovcnt--;
        }
        if (iovcnt > 0 && rem > 0) {
            iov[0].iov_base = (uint8_t *)iov[0].iov_base + rem;
            iov[0].iov_len -= rem;
        }
    }
    return 0;
}

static void set_nodelay(int fd) {
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}

static int connect_vnc(const char *host, int port) {
    for (int attempt = 0; attempt < 15; attempt++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return -1;
        set_nodelay(s);
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons((uint16_t)port);
        inet_pton(AF_INET, host, &sin.sin_addr);
        if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) == 0)
            return s;
        close(s);
        usleep(100000);
    }
    return -1;
}

/* ---- Streaming RFC-6455 WebSocket <-> Raw TCP Proxy ---- */
static void ws_proxy_loop(int cfd, int vfd, const uint8_t *init_buf, size_t init_len) {
    uint8_t hdr[14];
    size_t hdr_have = 0, hdr_need = 2;
    uint64_t payload_rem = 0, payload_off = 0;
    uint8_t mask[4] = {0, 0, 0, 0};
    int masked = 0;
    uint8_t opcode = 0, data_mode = 0;
    uint8_t ping_buf[125];
    size_t ping_len = 0;
    enum { ST_HDR, ST_PAYLOAD } state = ST_HDR;

    uint8_t cbuf[65536];
    uint8_t vbuf[65535];

    /* Process any pipelined bytes read with HTTP headers */
    size_t avail = init_len < sizeof(cbuf) ? init_len : sizeof(cbuf);
    if (avail > 0) memcpy(cbuf, init_buf, avail);

    for (;;) {
        /* Drain cbuf through RFC-6455 frame state machine */
        size_t pos = 0;
        while (pos < avail) {
            if (state == ST_HDR) {
                while (pos < avail && hdr_have < hdr_need)
                    hdr[hdr_have++] = cbuf[pos++];
                if (hdr_have < 2) break;

                uint8_t len7 = hdr[1] & 0x7F;
                masked = (hdr[1] & 0x80) ? 1 : 0;
                size_t ext = (len7 == 126) ? 2 : (len7 == 127 ? 8 : 0);
                hdr_need = 2 + ext + (masked ? 4 : 0);
                if (hdr_have < hdr_need) continue;

                opcode = hdr[0] & 0x0F;
                if (opcode == 0x1 || opcode == 0x2) data_mode = 1;
                else if (opcode != 0x0) { /* control frame */ }

                if (len7 < 126) payload_rem = len7;
                else if (len7 == 126)
                    payload_rem = ((uint64_t)hdr[2] << 8) | hdr[3];
                else {
                    payload_rem = 0;
                    for (int i = 0; i < 8; i++)
                        payload_rem = (payload_rem << 8) | hdr[2 + i];
                }
                if (masked) memcpy(mask, hdr + 2 + ext, 4);
                payload_off = 0;
                if (opcode == 0x9) ping_len = 0;

                if (opcode == 0x8) {
                    /* Close frame */
                    uint8_t cls[2] = {0x88, 0x00};
                    write_all(cfd, cls, 2);
                    return;
                }
                if (payload_rem == 0) {
                    if (opcode == 0x9) {
                        uint8_t pong[2] = {0x8A, 0x00};
                        if (write_all(cfd, pong, 2) < 0) return;
                    }
                    hdr_have = 0; hdr_need = 2;
                    continue;
                }
                state = ST_PAYLOAD;
            } else {
                size_t chunk = avail - pos;
                if ((uint64_t)chunk > payload_rem) chunk = (size_t)payload_rem;
                uint8_t *p = cbuf + pos;
                if (masked) {
                    for (size_t i = 0; i < chunk; i++)
                        p[i] ^= mask[(payload_off + i) & 3];
                }
                if (opcode == 0x1 || opcode == 0x2 || (opcode == 0x0 && data_mode)) {
                    if (write_all(vfd, p, chunk) < 0) return;
                } else if (opcode == 0x9 && ping_len + chunk <= sizeof(ping_buf)) {
                    memcpy(ping_buf + ping_len, p, chunk);
                    ping_len += chunk;
                }
                pos += chunk;
                payload_off += chunk;
                payload_rem -= chunk;
                if (payload_rem == 0) {
                    if (opcode == 0x9) {
                        uint8_t phdr[2] = {0x8A, (uint8_t)ping_len};
                        struct iovec iov[2] = {{phdr, 2}, {ping_buf, ping_len}};
                        if (writev_all(cfd, iov, 2) < 0) return;
                    }
                    state = ST_HDR;
                    hdr_have = 0; hdr_need = 2;
                }
            }
        }
        avail = 0;

        struct pollfd pfds[2];
        pfds[0].fd = cfd; pfds[0].events = POLLIN; pfds[0].revents = 0;
        pfds[1].fd = vfd; pfds[1].events = POLLIN; pfds[1].revents = 0;

        int pr = poll(pfds, 2, -1);
        if (pr < 0) {
            if (errno == EINTR) continue;
            return;
        }
        if (pfds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t vn = recv(vfd, vbuf, sizeof(vbuf), 0);
            if (vn <= 0) return;
            uint8_t whdr[4];
            size_t whlen;
            whdr[0] = 0x82; /* FIN=1, binary */
            if (vn < 126) {
                whdr[1] = (uint8_t)vn;
                whlen = 2;
            } else {
                whdr[1] = 126;
                whdr[2] = (uint8_t)((vn >> 8) & 0xFF);
                whdr[3] = (uint8_t)(vn & 0xFF);
                whlen = 4;
            }
            struct iovec iov[2] = {{whdr, whlen}, {vbuf, (size_t)vn}};
            if (writev_all(cfd, iov, 2) < 0) return;
        }
        if (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t cn = recv(cfd, cbuf, sizeof(cbuf), 0);
            if (cn <= 0) return;
            avail = (size_t)cn;
        }
    }
}

/* ---- HTTP Static Server + Header Helpers ---- */
static const char *mime_for_path(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (!strcasecmp(ext, ".html") || !strcasecmp(ext, ".htm")) return "text/html; charset=utf-8";
    if (!strcasecmp(ext, ".js") || !strcasecmp(ext, ".mjs"))   return "text/javascript; charset=utf-8";
    if (!strcasecmp(ext, ".css"))   return "text/css; charset=utf-8";
    if (!strcasecmp(ext, ".svg"))   return "image/svg+xml";
    if (!strcasecmp(ext, ".png"))   return "image/png";
    if (!strcasecmp(ext, ".ico"))   return "image/x-icon";
    if (!strcasecmp(ext, ".json"))  return "application/json";
    if (!strcasecmp(ext, ".woff2")) return "font/woff2";
    if (!strcasecmp(ext, ".woff"))  return "font/woff";
    if (!strcasecmp(ext, ".ttf"))   return "font/ttf";
    return "application/octet-stream";
}

static int find_header(const char *hdrs, const char *name, char *out, size_t outsz) {
    size_t nlen = strlen(name);
    const char *p = hdrs;
    while ((p = strchr(p, '\n')) != NULL) {
        p++;
        if (strncasecmp(p, name, nlen) == 0 && p[nlen] == ':') {
            p += nlen + 1;
            while (*p == ' ' || *p == '\t') p++;
            size_t i = 0;
            while (p[i] && p[i] != '\r' && p[i] != '\n' && i + 1 < outsz) {
                out[i] = p[i];
                i++;
            }
            while (i > 0 && (out[i - 1] == ' ' || out[i - 1] == '\t')) i--;
            out[i] = '\0';
            return 1;
        }
    }
    return 0;
}

static void handle_client(int cfd, const char *web_root, const char *vnc_host, int vnc_port) {
    set_nodelay(cfd);
    char req[8192];
    size_t have = 0;
    const char *hdr_end = NULL;

    while (have + 1 < sizeof(req)) {
        ssize_t n = recv(cfd, req + have, sizeof(req) - 1 - have, 0);
        if (n <= 0) return;
        have += (size_t)n;
        req[have] = '\0';
        hdr_end = strstr(req, "\r\n\r\n");
        if (hdr_end) break;
    }
    if (!hdr_end) return;

    size_t hdr_bytes = (size_t)(hdr_end - req) + 4;
    const uint8_t *leftover = (const uint8_t *)req + hdr_bytes;
    size_t leftover_len = have - hdr_bytes;

    char method[16] = {0}, raw_path[1024] = {0};
    if (sscanf(req, "%15s %1023s", method, raw_path) != 2) return;

    /* Strip query string */
    char *q = strchr(raw_path, '?');
    if (q) *q = '\0';

    /* Check WebSocket upgrade */
    char upgrade_val[64] = {0}, ws_key[128] = {0}, ws_proto[128] = {0};
    find_header(req, "Upgrade", upgrade_val, sizeof(upgrade_val));
    if (strcasestr(upgrade_val, "websocket") &&
        find_header(req, "Sec-WebSocket-Key", ws_key, sizeof(ws_key))) {

        int vfd = connect_vnc(vnc_host, vnc_port);
        if (vfd < 0) {
            const char *err = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 15\r\n\r\nXvnc not ready\n";
            write_all(cfd, err, strlen(err));
            return;
        }

        char combined[256];
        snprintf(combined, sizeof(combined), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", ws_key);
        uint8_t sha[20];
        char accept_b64[29];
        sha1_compute((const uint8_t *)combined, strlen(combined), sha);
        base64_encode_20(sha, accept_b64);

        int want_binary_proto = 0;
        if (find_header(req, "Sec-WebSocket-Protocol", ws_proto, sizeof(ws_proto)) &&
            strcasestr(ws_proto, "binary")) {
            want_binary_proto = 1;
        }

        char resp[512];
        int rlen = snprintf(resp, sizeof(resp),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "%s"
            "\r\n",
            accept_b64,
            want_binary_proto ? "Sec-WebSocket-Protocol: binary\r\n" : "");
        if (write_all(cfd, resp, (size_t)rlen) == 0)
            ws_proxy_loop(cfd, vfd, leftover, leftover_len);
        close(vfd);
        return;
    }

    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        const char *r = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        write_all(cfd, r, strlen(r));
        return;
    }

    if (strcmp(raw_path, "/") == 0 || strcmp(raw_path, "/index.html") == 0) {
        const char *redir =
            "HTTP/1.1 302 Found\r\n"
            "Location: /vnc.html?autoconnect=true&resize=remote&reconnect=true&reconnect_delay=1000\r\n"
            "Cache-Control: no-store\r\n"
            "Content-Length: 0\r\n\r\n";
        write_all(cfd, redir, strlen(redir));
        return;
    }

    if (strstr(raw_path, "..") || raw_path[0] != '/') {
        const char *r = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        write_all(cfd, r, strlen(r));
        return;
    }

    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s%s", web_root, raw_path);
    struct stat st;
    int ffd = open(full_path, O_RDONLY);
    if (ffd < 0 || fstat(ffd, &st) < 0 || !S_ISREG(st.st_mode)) {
        if (ffd >= 0) close(ffd);
        const char *r = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
        write_all(cfd, r, strlen(r));
        return;
    }

    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Cache-Control: public, max-age=300\r\n"
        "Connection: close\r\n\r\n",
        mime_for_path(full_path), (long long)st.st_size);
    if (write_all(cfd, hdr, (size_t)hlen) == 0 && strcmp(method, "GET") == 0) {
        uint8_t fbuf[32768];
        ssize_t fn;
        while ((fn = read(ffd, fbuf, sizeof(fbuf))) > 0) {
            if (write_all(cfd, fbuf, (size_t)fn) < 0) break;
        }
    }
    close(ffd);
}

int main(int argc, char **argv) {
    int listen_port   = (argc > 1) ? atoi(argv[1]) : 80;
    const char *root  = (argc > 2) ? argv[2] : "/usr/share/novnc";
    const char *vhost = (argc > 3) ? argv[3] : "127.0.0.1";
    int vport         = (argc > 4) ? atoi(argv[4]) : 5901;

    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }
    int one = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons((uint16_t)listen_port);

    if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(sfd, 64) < 0) {
        perror("listen");
        return 1;
    }

    for (;;) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        pid_t pid = fork();
        if (pid == 0) {
            close(sfd);
            handle_client(cfd, root, vhost, vport);
            close(cfd);
            _exit(0);
        }
        close(cfd);
    }
}
