#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/io.h>

#define RAMOOPS_PHYS 0xae000000
#define RAMOOPS_SIZE 0x00100000

static void __iomem *ram_base;

static int ramoops_show(struct seq_file *m, void *v)
{
    int i;
    char c;
    for (i = 0; i < RAMOOPS_SIZE; i++) {
        c = readb(ram_base + i);
        if ((c >= 32 && c <= 126) || c == '\n' || c == '\t')
            seq_putc(m, c);
    }
    return 0;
}

static int ramoops_open(struct inode *inode, struct file *file)
{
    return single_open(file, ramoops_show, NULL);
}

static const struct file_operations ramoops_fops = {
    .owner = THIS_MODULE,
    .open = ramoops_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init dump_init(void)
{
    ram_base = (void __iomem *)phys_to_virt(RAMOOPS_PHYS);
    if (!ram_base)
        return -ENOMEM;
    proc_create("ramoops_dump", 0444, NULL, &ramoops_fops);
    return 0;
}

static void __exit dump_exit(void)
{
    remove_proc_entry("ramoops_dump", NULL);
    if (ram_base)
        /* lowmem */;
}

module_init(dump_init);
module_exit(dump_exit);
MODULE_LICENSE("GPL");
