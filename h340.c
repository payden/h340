#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/genhd.h>

#define DRIVER_AUTHOR "Payden Sutherland <payden@paydensutherland.com>"
#define DRIVER_DESC "H340 HDD LED driver, that's a lot of acronyms"
#define H340_BASE 0x800
#define H340_GP1 0x4b

/* Okay, for now, I'm going to assume that ata ports 1-4 correspond to sd[a-d]
 * because I'm unsure how to iterate through ata devices and deduce their major minor */

#define SDA_DEV MKDEV(8, 0)
#define SDB_DEV MKDEV(8, 16)
#define SDC_DEV MKDEV(8, 32)
#define SDD_DEV MKDEV(8, 48)


struct tmp_stats {
	int reads;
	int writes;
};

static struct tmp_stats **h340_stats;

static struct task_struct *h340_thread;

static int h340_run(void *data) {
	int i;
	struct tmp_stats *stats;
	for(i=0;i<4;i++) {
		stats = *(h340_stats+i);
		printk(KERN_NOTICE "[h340][%d]: reads(%d) writes(%d)\n", i, stats->reads, stats->writes);
	}
	return 0;
}

static int __init h340_init(void)
{
	struct gendisk *gd;
	int i, partno = 0, cpu;
	dev_t dev;
	printk(KERN_NOTICE "H340 HDD LED Init.\n");
	h340_stats = (struct tmp_stats **)kmalloc(sizeof(*h340_stats) * 4, GFP_KERNEL);
	if(!h340_stats) {
		printk(KERN_NOTICE "Unable to allocate memory for h340_stats.\n");
		return -ENOMEM;
	}
	for(i=0;i<4;i++) {
		dev = MKDEV(8, i*3);
		printk("dev for %d is %d\n", i, (int)dev);
		gd = get_gendisk(dev, &partno);
		if(!gd) {
			printk(KERN_NOTICE "No disk, continuing for %d\n", i);
			continue;
		}
		*(h340_stats+i) = (struct tmp_stats *)kmalloc(sizeof(**h340_stats), GFP_KERNEL);
		cpu = part_stat_lock();
		part_round_stats(cpu, &gd->part0);
		part_stat_unlock();
		(*(h340_stats+i))->reads = part_stat_read(&gd->part0, ios[READ]);
		(*(h340_stats+i))->writes = part_stat_read(&gd->part0, ios[WRITE]);
	}

	h340_thread = kthread_create(h340_run, NULL, "h340d");
	wake_up_process(h340_thread);
	return 0;
}

static void __exit h340_exit(void)
{
	int i;
	struct tmp_stats *stats;
	for(i=0;i<4;i++) {
		stats = *(h340_stats+i);
		kfree(stats);
	}
	kfree(h340_stats);
	printk(KERN_NOTICE "H340 HDD LED Exiting.\n");
}

module_init(h340_init);
module_exit(h340_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
