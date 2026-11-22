#include <linux/module.h>
#include <linux/kernel.h>

/*
 * Over-the-air live RAM pstore/ramoops reader for LS-4505 (MSM8909).
 * Physical 0xae000000 maps to lowmem virtual 0xee000000 (PAGE_OFFSET 0xc0000000
 * + 0x2e000000). Prints non-empty lines to dmesg and returns -EAGAIN to auto-unload.
 */
static uint start_off = 0x0;
static uint end_off = 0xc0000;
module_param(start_off, uint, 0444);
module_param(end_off, uint, 0444);

static int __init dump_init(void)
{
    const char *p = (const char *)0xee000000UL;
    uint i, line_start = start_off, printed = 0;
    char buf[192];

    if (end_off > 0x100000)
        end_off = 0x100000;

    printk(KERN_ERR "RAMOOPS_DUMP: magic @0=%02x%02x%02x%02x @40000=%02x%02x%02x%02x @80000=%02x%02x%02x%02x\n",
           (unsigned char)p[0], (unsigned char)p[1], (unsigned char)p[2], (unsigned char)p[3],
           (unsigned char)p[0x40000], (unsigned char)p[0x40001], (unsigned char)p[0x40002], (unsigned char)p[0x40003],
           (unsigned char)p[0x80000], (unsigned char)p[0x80001], (unsigned char)p[0x80002], (unsigned char)p[0x80003]);

    for (i = start_off; i < end_off && printed < 900; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c == '\n' || (i - line_start) >= 180 || c == 0) {
            int len = i - line_start;
            if (len > 4) {
                int j, ok = 1;
                for (j = 0; j < len; j++) {
                    unsigned char ch = (unsigned char)p[line_start + j];
                    if (ch < 0x09 || (ch > 0x0d && ch < 0x20) || ch > 0x7e) { ok = 0; break; }
                    buf[j] = (char)ch;
                }
                buf[len] = '\0';
                if (ok) {
                    printk(KERN_ERR "RO[%05x]: %s\n", line_start, buf);
                    printed++;
                }
            }
            line_start = i + 1;
        }
    }
    return -EAGAIN;
}
module_init(dump_init);
MODULE_LICENSE("GPL");
