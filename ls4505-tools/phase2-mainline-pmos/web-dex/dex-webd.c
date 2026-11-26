/*
 * dex-webd.c — Ultra-lightweight Zero-Dependency HTTP + Salted-SHA1 Session
 *              Auth Gate + RFC-6455 WebSocket-to-VNC Proxy for LS-4505 Web-DeX
 *
 * Usage: dex-webd [listen_port] [web_root] [vnc_host] [vnc_port] [auth_file]
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

static uint8_t g_boot_secret[32];
static const char *g_epoch_file = "/run/dex-webd.epoch";

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

static void sha1_init(sha1_ctx *ctx) {
    ctx->state[0] = 0x67452301; ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE; ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0; ctx->count = 0;
}

static void sha1_update(sha1_ctx *ctx, const uint8_t *data, size_t len) {
    size_t idx = (size_t)((ctx->count >> 3) & 63);
    ctx->count += (uint64_t)len << 3;
    size_t i = 0;
    if (idx > 0) {
        size_t take = 64 - idx;
        if (len < take) {
            memcpy(ctx->buffer + idx, data, len);
            return;
        }
        memcpy(ctx->buffer + idx, data, take);
        sha1_transform(ctx->state, ctx->buffer);
        i = take;
    }
    while (len - i >= 64) {
        sha1_transform(ctx->state, data + i);
        i += 64;
    }
    if (len > i)
        memcpy(ctx->buffer, data + i, len - i);
}

static void sha1_final(sha1_ctx *ctx, uint8_t digest[20]) {
    uint64_t bits = ctx->count;
    size_t idx = (size_t)((bits >> 3) & 63);
    uint8_t pad[64];
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    size_t pad_len = (idx < 56) ? (56 - idx) : (120 - idx);
    sha1_update(ctx, pad, pad_len);
    uint8_t len_be[8];
    for (int j = 0; j < 8; j++)
        len_be[7 - j] = (uint8_t)(bits >> (j * 8));
    sha1_update(ctx, len_be, 8);
    for (int j = 0; j < 5; j++) {
        digest[j * 4]     = (uint8_t)(ctx->state[j] >> 24);
        digest[j * 4 + 1] = (uint8_t)(ctx->state[j] >> 16);
        digest[j * 4 + 2] = (uint8_t)(ctx->state[j] >> 8);
        digest[j * 4 + 3] = (uint8_t)(ctx->state[j]);
    }
}

static void sha1_compute(const uint8_t *data, size_t len, uint8_t digest[20]) {
    sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, digest);
}

static void hex_encode_20(const uint8_t in[20], char out[41]) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        out[i * 2]     = hex[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[in[i] & 0x0F];
    }
    out[40] = '\0';
}

static int ct_str_eq(const char *a, const char *b, size_t n) {
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++) {
        diff |= (uint8_t)(tolower((unsigned char)a[i]) ^ tolower((unsigned char)b[i]));
        if (a[i] == '\0' || b[i] == '\0')
            return (diff == 0 && a[i] == b[i]) ? 1 : 0;
    }
    return (diff == 0 && a[n] == '\0' && b[n] == '\0') ? 1 : 0;
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

static int load_auth_record(const char *auth_file, char *salt_out, size_t salt_sz, char hash_out[41]) {
    if (!auth_file || !auth_file[0]) return 0;
    FILE *f = fopen(auth_file, "r");
    if (!f) return 0;
    char line[256] = {0};
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    fclose(f);
    char *nl = strpbrk(line, "\r\n");
    if (nl) *nl = '\0';
    char *colon = strchr(line, ':');
    if (!colon || strlen(colon + 1) != 40) return 0;
    size_t slen = (size_t)(colon - line);
    if (slen == 0 || slen >= salt_sz) return 0;
    memcpy(salt_out, line, slen);
    salt_out[slen] = '\0';
    memcpy(hash_out, colon + 1, 41);
    return 1;
}

static void compute_session_token(const char *salt, const char *hash, char tok_out[41]) {
    char epoch[64] = "0";
    int fd = open(g_epoch_file, O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, epoch, sizeof(epoch) - 1);
        if (n > 0) epoch[n] = '\0';
        close(fd);
    }
    sha1_ctx ctx;
    uint8_t dig[20];
    sha1_init(&ctx);
    sha1_update(&ctx, g_boot_secret, sizeof(g_boot_secret));
    sha1_update(&ctx, (const uint8_t *)salt, strlen(salt));
    sha1_update(&ctx, (const uint8_t *)hash, strlen(hash));
    sha1_update(&ctx, (const uint8_t *)epoch, strlen(epoch));
    sha1_final(&ctx, dig);
    hex_encode_20(dig, tok_out);
}

static void bump_session_epoch(void) {
    uint8_t rnd[16];
    int ufd = open("/dev/urandom", O_RDONLY);
    if (ufd >= 0) {
        if (read(ufd, rnd, sizeof(rnd)) < 0) memset(rnd, 0x5A, sizeof(rnd));
        close(ufd);
    }
    uint8_t dig[20];
    char hex[41];
    sha1_compute(rnd, sizeof(rnd), dig);
    hex_encode_20(dig, hex);
    int fd = open(g_epoch_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        if (write(fd, hex, 40) < 0) { }
        close(fd);
    }
}

static int verify_password(const char *salt, const char *expected_hash, const char *password) {
    sha1_ctx ctx;
    uint8_t dig[20];
    char got_hex[41];
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t *)salt, strlen(salt));
    sha1_update(&ctx, (const uint8_t *)":", 1);
    sha1_update(&ctx, (const uint8_t *)password, strlen(password));
    sha1_final(&ctx, dig);
    hex_encode_20(dig, got_hex);
    return ct_str_eq(got_hex, expected_hash, 40);
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(const char *src, char *dst, size_t dst_sz) {
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 1 < dst_sz; i++) {
        if (src[i] == '+') {
            dst[o++] = ' ';
        } else if (src[i] == '%' && hex_val(src[i + 1]) >= 0 && hex_val(src[i + 2]) >= 0) {
            dst[o++] = (char)((hex_val(src[i + 1]) << 4) | hex_val(src[i + 2]));
            i += 2;
        } else {
            dst[o++] = src[i];
        }
    }
    dst[o] = '\0';
}

static int extract_cookie_token(const char *cookie_hdr, char out[41]) {
    const char *p = cookie_hdr;
    while (p && *p) {
        while (*p == ' ' || *p == ';' || *p == '\t') p++;
        if (strncmp(p, "dex_session=", 12) == 0) {
            p += 12;
            size_t i = 0;
            while (p[i] && p[i] != ';' && p[i] != ' ' && p[i] != '\r' && p[i] != '\n' && i < 40) {
                out[i] = p[i];
                i++;
            }
            out[i] = '\0';
            return (i == 40) ? 1 : 0;
        }
        p = strchr(p, ';');
    }
    return 0;
}

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

    size_t avail = init_len < sizeof(cbuf) ? init_len : sizeof(cbuf);
    if (avail > 0) memcpy(cbuf, init_buf, avail);

    for (;;) {
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
            whdr[0] = 0x82;
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
        if (p[0] == '\r' || p[0] == '\n') break;
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

static void send_login_page(int cfd, int show_error) {
    const char *err_html = show_error
        ? "<div style='background:#7f1d1d;color:#fecaca;padding:10px 12px;border-radius:8px;font-size:13px;margin-bottom:14px;border:1px solid #ef4444'>Incorrect password. Please try again.</div>"
        : "";
    char body[2048];
    int blen = snprintf(body, sizeof(body),
        "<!doctype html><html lang='en'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>LS-4505 Web-DeX — Unlock Desktop</title>"
        "<style>"
        "*{box-sizing:border-box}body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
        "background:radial-gradient(circle at 50%% 25%%,#1e293b,#090d16);color:#e2e8f0;font-family:system-ui,-apple-system,sans-serif}"
        ".card{width:360px;max-width:92vw;background:#111827;border:1px solid #334155;border-radius:14px;padding:28px;box-shadow:0 20px 50px rgba(0,0,0,.6)}"
        "h1{margin:0 0 6px;font-size:20px;color:#f8fafc}p{margin:0 0 20px;font-size:13px;color:#94a3b8}"
        "input{width:100%%;padding:11px 14px;border-radius:8px;border:1px solid #475569;background:#0b1120;color:#fff;font-size:15px;outline:none;margin-bottom:14px}"
        "input:focus{border-color:#3b82f6;box-shadow:0 0 0 2px rgba(59,130,246,.35)}"
        "button{width:100%%;padding:11px;border:0;border-radius:8px;background:#2563eb;color:#fff;font-size:15px;font-weight:600;cursor:pointer}"
        "button:hover{background:#1d4ed8}"
        "</style></head><body><form class='card' method='POST' action='/login'>"
        "<h1>LS-4505 Web-DeX</h1>"
        "<p>Mainline Linux 6.12.1 &bull; Enter password to unlock desktop</p>"
        "%s"
        "<input type='password' name='password' placeholder='Web-DeX Password' autofocus required>"
        "<button type='submit'>Unlock Desktop</button>"
        "</form></body></html>",
        err_html);

    char hdr[384];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n", blen);
    if (write_all(cfd, hdr, (size_t)hlen) == 0)
        write_all(cfd, body, (size_t)blen);
}

static void handle_client(int cfd, const char *web_root, const char *vnc_host, int vnc_port, const char *auth_file) {
    set_nodelay(cfd);
    char req[8192];
    size_t have = 0;
    char *hdr_end = NULL;

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

    char method[16] = {0}, raw_uri[1024] = {0};
    if (sscanf(req, "%15s %1023s", method, raw_uri) != 2) return;

    char raw_path[1024];
    snprintf(raw_path, sizeof(raw_path), "%s", raw_uri);
    char *query = strchr(raw_path, '?');
    if (query) { *query = '\0'; query++; }

    char salt[128] = {0}, expected_hash[41] = {0}, valid_tok[41] = {0};
    int auth_enabled = load_auth_record(auth_file, salt, sizeof(salt), expected_hash);
    int is_authed = 1;

    if (auth_enabled) {
        is_authed = 0;
        compute_session_token(salt, expected_hash, valid_tok);
        char cookie_hdr[1024] = {0}, got_tok[41] = {0};
        if (find_header(req, "Cookie", cookie_hdr, sizeof(cookie_hdr)) &&
            extract_cookie_token(cookie_hdr, got_tok) &&
            ct_str_eq(got_tok, valid_tok, 40)) {
            is_authed = 1;
        }
    }

    if (auth_enabled && strcmp(method, "POST") == 0 && strcmp(raw_path, "/login") == 0) {
        char cl_str[32] = {0};
        size_t content_len = 0;
        if (find_header(req, "Content-Length", cl_str, sizeof(cl_str))) {
            long v = atol(cl_str);
            if (v > 0 && v <= 2048) content_len = (size_t)v;
        }
        char body[2049];
        size_t bhave = leftover_len < content_len ? leftover_len : content_len;
        if (bhave > 0) memcpy(body, leftover, bhave);
        while (bhave < content_len) {
            ssize_t n = recv(cfd, body + bhave, content_len - bhave, 0);
            if (n <= 0) break;
            bhave += (size_t)n;
        }
        body[bhave] = '\0';

        char pw_raw[512] = {0}, pw[512] = {0};
        const char *p = strstr(body, "password=");
        if (p) {
            p += 9;
            size_t i = 0;
            while (p[i] && p[i] != '&' && i + 1 < sizeof(pw_raw)) {
                pw_raw[i] = p[i];
                i++;
            }
            pw_raw[i] = '\0';
            url_decode(pw_raw, pw, sizeof(pw));
        }

        if (verify_password(salt, expected_hash, pw)) {
            char resp[512];
            int rlen = snprintf(resp, sizeof(resp),
                "HTTP/1.1 303 See Other\r\n"
                "Set-Cookie: dex_session=%s; Path=/; HttpOnly; SameSite=Strict\r\n"
                "Location: /vnc.html?autoconnect=true&resize=remote&reconnect=true&reconnect_delay=1000\r\n"
                "Cache-Control: no-store\r\n"
                "Content-Length: 0\r\n\r\n", valid_tok);
            write_all(cfd, resp, (size_t)rlen);
        } else {
            sleep(1);
            const char *resp =
                "HTTP/1.1 303 See Other\r\n"
                "Location: /login?err=1\r\n"
                "Cache-Control: no-store\r\n"
                "Content-Length: 0\r\n\r\n";
            write_all(cfd, resp, strlen(resp));
        }
        memset(pw, 0, sizeof(pw));
        return;
    }

    if (auth_enabled && strcmp(raw_path, "/logout") == 0) {
        if (is_authed) bump_session_epoch();
        const char *resp =
            "HTTP/1.1 302 Found\r\n"
            "Set-Cookie: dex_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict\r\n"
            "Location: /login\r\n"
            "Cache-Control: no-store\r\n"
            "Content-Length: 0\r\n\r\n";
        write_all(cfd, resp, strlen(resp));
        return;
    }

    char upgrade_val[64] = {0}, ws_key[128] = {0}, ws_proto[128] = {0};
    find_header(req, "Upgrade", upgrade_val, sizeof(upgrade_val));
    if (strcasestr(upgrade_val, "websocket") &&
        find_header(req, "Sec-WebSocket-Key", ws_key, sizeof(ws_key))) {

        if (!is_authed) {
            const char *unauth =
                "HTTP/1.1 401 Unauthorized\r\n"
                "Cache-Control: no-store\r\n"
                "Content-Length: 13\r\n\r\nUnauthorized\n";
            write_all(cfd, unauth, strlen(unauth));
            return;
        }

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

    if (!is_authed) {
        if (strcmp(raw_path, "/login") == 0 || strcmp(raw_path, "/") == 0 || strcmp(raw_path, "/index.html") == 0) {
            int show_err = (query && strstr(query, "err=1")) ? 1 : 0;
            send_login_page(cfd, show_err);
        } else {
            const char *redir =
                "HTTP/1.1 302 Found\r\n"
                "Location: /login\r\n"
                "Cache-Control: no-store\r\n"
                "Content-Length: 0\r\n\r\n";
            write_all(cfd, redir, strlen(redir));
        }
        return;
    }

    if (strcmp(raw_path, "/") == 0 || strcmp(raw_path, "/index.html") == 0 || strcmp(raw_path, "/login") == 0) {
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
        "Cache-Control: private, max-age=300\r\n"
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
    if (argc == 4 && strcmp(argv[1], "--hash") == 0) {
        sha1_ctx ctx;
        uint8_t dig[20];
        char hex[41];
        sha1_init(&ctx);
        sha1_update(&ctx, (const uint8_t *)argv[2], strlen(argv[2]));
        sha1_update(&ctx, (const uint8_t *)":", 1);
        sha1_update(&ctx, (const uint8_t *)argv[3], strlen(argv[3]));
        sha1_final(&ctx, dig);
        hex_encode_20(dig, hex);
        printf("%s:%s\n", argv[2], hex);
        return 0;
    }

    int listen_port       = (argc > 1) ? atoi(argv[1]) : 80;
    const char *root      = (argc > 2) ? argv[2] : "/usr/share/novnc";
    const char *vhost     = (argc > 3) ? argv[3] : "127.0.0.1";
    int vport             = (argc > 4) ? atoi(argv[4]) : 5901;
    const char *auth_file = (argc > 5) ? argv[5] : "/etc/dex-webd.auth";
    if (argc > 6) g_epoch_file = argv[6];

    int ufd = open("/dev/urandom", O_RDONLY);
    if (ufd >= 0) {
        if (read(ufd, g_boot_secret, sizeof(g_boot_secret)) != sizeof(g_boot_secret))
            perror("urandom");
        close(ufd);
    }

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
            handle_client(cfd, root, vhost, vport, auth_file);
            close(cfd);
            _exit(0);
        }
        close(cfd);
    }
}
