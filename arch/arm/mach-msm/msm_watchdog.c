/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/mfd/pmic8058.h>
#include <linux/jiffies.h>
#include <mach/msm_iomap.h>

#define TCSR_BASE 0x16B00000
#define TCSR_WDT_CFG 0x30

#define WDT0_RST	(MSM_TMR0_BASE + 0x38)
#define WDT0_EN		(MSM_TMR0_BASE + 0x40)
#define WDT0_BARK_TIME	(MSM_TMR0_BASE + 0x4C)
#define WDT0_BITE_TIME	(MSM_TMR0_BASE + 0x5C)

/* Watchdog pet interval in ms */
#define PET_DELAY 300
static unsigned long delay_time;

#define DLOAD_MODE_ADDR 0x2A03E008
#define DLOAD_MAGIC_WRITE1 0xE47B337D
#define DLOAD_MAGIC_WRITE2 0xCE14091A

/*
 * On the kernel command line specify
 * msm_watchdog.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0);

static void *tcsr_base;
static void pet_watchdog(struct work_struct *work);
static DECLARE_DELAYED_WORK(dogwork_struct, pet_watchdog);

static void pet_watchdog(struct work_struct *work)
{
	writel(1, WDT0_RST);
	schedule_delayed_work(&dogwork_struct, delay_time);
}

static void start_watchdog_timer(void)
{
	/* 22000 ticks at 32768Hz = 671ms */
	writel(22000, WDT0_BARK_TIME);
	writel(22000, WDT0_BITE_TIME);
	writel(3, WDT0_EN);

	INIT_DELAYED_WORK(&dogwork_struct, pet_watchdog);
	schedule_delayed_work(&dogwork_struct, delay_time);
}

static void __exit exit_watchdog(void)
{
	writel(0, WDT0_EN);
	printk(KERN_INFO "MSM Watchdog Exit - Deactivated\n");
}

static int __init init_watchdog(void)
{
	if (enable) {
		printk(KERN_INFO "MSM Watchdog Initialized\n");

#ifdef CONFIG_MSM_WATCHDOG_DEBUG
		void *dload_mode_addr;
		dload_mode_addr = ioremap_nocache(DLOAD_MODE_ADDR, SZ_4K);
		writel(DLOAD_MAGIC_WRITE1, dload_mode_addr);
		writel(DLOAD_MAGIC_WRITE2, dload_mode_addr +
					sizeof(unsigned int));
		iounmap(dload_mode_addr);
		printk(KERN_INFO "MSM_WATCHDOG_DEBUG enabled\n");
#endif

		tcsr_base = ioremap_nocache(TCSR_BASE, SZ_4K);
		if (tcsr_base == NULL)
			return -ENOMEM;
		writel(3, tcsr_base + TCSR_WDT_CFG);
		delay_time = msecs_to_jiffies(PET_DELAY);
		start_watchdog_timer();
	} else {
		printk(KERN_INFO "MSM Watchdog Not Initialized\n");
	}

	return 0;
}

late_initcall(init_watchdog);
module_exit(exit_watchdog);
MODULE_DESCRIPTION("MSM Watchdog Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
