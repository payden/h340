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
#define H340_GP5 0x4f

#define SAMPLE_RATE 50
#define HD1_RED 1 << 7
#define HD1_BLUE 1 << 6
#define HD2_RED 1 << 3
#define HD2_BLUE 1 << 2
#define HD3_RED 1 << 1
#define HD3_BLUE 1 << 0
#define HD4_RED 1 << 1
#define HD4_BLUE 1 << 4



struct tmp_stats {
	int reads;
	int writes;
};

static struct tmp_stats **h340_stats;

static struct task_struct *h340_thread;

static int h340_run(void *data) {
	int i, cpu;
	int partno = 0;
	int blues[] = {6, 2, 0, 4};
	int reds[] = {7, 3, 1, 1};
	unsigned long int reads;
	unsigned long int writes;
	unsigned char gp5_val, gp1_val;
	struct gendisk *gd;
	struct tmp_stats *stats;
	dev_t dev;
	for(;;) {
		gp5_val = gp1_val = 0;
		if(kthread_should_stop()) {
			outb(gp5_val, H340_BASE+H340_GP5);
			outb(gp1_val, H340_BASE+H340_GP1);
			do_exit(0);
		}
		for(i=0;i<4;i++) {
			dev = MKDEV(8, i*16);
			gd = get_gendisk(dev, &partno);
			if(!gd) {
				continue;
			}
			stats = *(h340_stats+i);
			cpu = part_stat_lock();
			part_round_stats(cpu, &gd->part0);
			part_stat_unlock();
			reads = part_stat_read(&gd->part0, ios[READ]);
			writes = part_stat_read(&gd->part0, ios[WRITE]);
			if(reads > stats->reads) {
				if(i == 4) {
					gp1_val |= blues[i];
				}
				else {
					gp5_val |= blues[i];
				}
				stats->reads = reads;
			}
			stats->writes = writes;
		}
		outb(gp5_val, H340_BASE+H340_GP5);
		outb(gp1_val, H340_BASE+H340_GP1);
		msleep(SAMPLE_RATE);
	}
	return 0;
}

static int __init h340_init(void)
{
	struct gendisk *gd;
	int i, cpu;
	int partno = 0;
	dev_t dev;
	printk(KERN_NOTICE "H340 HDD LED Init.\n");
	h340_stats = (struct tmp_stats **)kmalloc(sizeof(*h340_stats) * 4, GFP_KERNEL);
	if(!h340_stats) {
		printk(KERN_NOTICE "Unable to allocate memory for h340_stats.\n");
		return -ENOMEM;
	}
	for(i=0;i<4;i++) {
		*(h340_stats+i) = (struct tmp_stats *)kmalloc(sizeof(**h340_stats), GFP_KERNEL);
		memset(*(h340_stats+i), 0, sizeof(**h340_stats));
	}
	for(i=0;i<4;i++) {
		dev = MKDEV(8, i*16);
		gd = get_gendisk(dev, &partno);
		if(!gd) {
			printk(KERN_NOTICE "No disk, continuing for %d\n", i);
			continue;
		}
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
