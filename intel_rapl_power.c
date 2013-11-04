/*
 * intel_rapl_power.c - Sysfs entries for Running Average Power Limit interfaces
 *
 * Copyright (c) 2011 Razvan A. Lupusoru <razvan.lupusoru@gmail.com>
 *
 * This file is released under the GPLv2.
 *
 * Please read the included README.txt
 * For documentation on Platform Specific Power Management Support using RAPL
 * please consult the Intel 64 and IA-32 Architectures Software Developer's
 * Manual Volume 3 Section 14.7 (as of April 2011)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/processor.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>

/*#define __INTEL_RAPL_POWER_DEBUG*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Razvan A. Lupusoru");
MODULE_DESCRIPTION("Exposes RAPL Interfaces (power counters) from Sandy Bridge via sysfs");

/* 06_2AH : Intel Core Sandy Bridge
   06_2DH : Intel Xeon Sandy Bridge */

/* MSRs that are supported on both 062A and 062D */
#define MSR_RAPL_POWER_UNIT		0x606

#define MSR_PKG_RAPL_POWER_LIMIT	0x610
#define MSR_PKG_ENERGY_STATUS	0x611
#define MSR_PKG_PERF_STATUS		0x613
#define MSR_PKG_POWER_INFO		0x614

#define MSR_PP0_POWER_LIMIT		0x638
#define MSR_PP0_ENERGY_STATUS	0x639
#define MSR_PP0_POLICY			0x63A
#define MSR_PP0_PERF_STATUS		0x63B

/* MSRs that are supported on 062A */
#define MSR_PP1_POWER_LIMIT		0x640
#define MSR_PP1_ENERGY_STATUS	0x641
#define MSR_PP1_POLICY			0x642

/* MSRs that are supported on 062D */
#define MSR_DRAM_POWER_LIMIT	0x618
#define MSR_DRAM_ENERGY_STATUS	0x619
#define MSR_DRAM_PERF_STATUS	0x61B
#define MSR_DRAM_POWER_INFO		0x61C

enum rapl_unit {
	POWER_UNIT_DEFAULT, /* 0011b, 1/8 Watts */
	ENERGY_UNIT_DEFAULT, /* 10000b, 15.3 micro Joules */
	TIME_UNIT_DEFAULT, /* 1010b, 976 microseconds */
	UNIT_UNKNOWN
};

enum rapl_unit energy_unit_type;
enum rapl_unit power_unit_type;
enum rapl_unit time_unit_type;

/* Defined for use with MSR_RAPL_POWER_UNIT Register */
#define POWER_UNIT_OFFSET	0x00
#define POWER_UNIT_MASK		0x0F
#define ENERGY_UNIT_OFFSET	0x08
#define ENERGY_UNIT_MASK	0x1F
#define TIME_UNIT_OFFSET	0x10
#define TIME_UNIT_MASK		0xF

/* Defined for use with MSR_PKG_POWER_INFO Register */
#define THERMAL_SPEC_POWER_OFFSET	0x0
#define THERMAL_SPEC_POWER_MASK 	0x7FFF
#define MINIMUM_POWER_OFFSET		0x10
#define MINIMUM_POWER_MASK			0x7FFF
#define MAXIMUM_POWER_OFFSET		0x20
#define MAXIMUM_POWER_MASK			0x7FFF
#define MAXIMUM_TIME_WINDOW_OFFSET 	0x30
#define MAXIMUM_TIME_WINDOW_MASK 	0x3F

/* Defined for use with MSR_PKG_POWER_LIMIT Register */
#define PKG_POWER_LIMIT_LOCK_OFFSET			0x3F
#define PKG_POWER_LIMIT_LOCK_MASK			0x1
#define ENABLE_LIMIT_2_OFFSET				0x2F
#define ENABLE_LIMIT_2_MASK					0x1
#define PKG_CLAMPING_LIMIT_2_OFFSET			0x30
#define PKG_CLAMPING_LIMIT_2_MASK			0x1
#define PKG_POWER_LIMIT_2_OFFSET			0x20
#define PKG_POWER_LIMIT_2_MASK				0x7FFF
#define ENABLE_LIMIT_1_OFFSET				0xF
#define ENABLE_LIMIT_1_MASK					0x1
#define PKG_CLAMPING_LIMIT_1_OFFSET			0x10
#define PKG_CLAMPING_LIMIT_1_MASK			0x1
#define PKG_POWER_LIMIT_1_OFFSET			0x0
#define PKG_POWER_LIMIT_1_MASK				0x7FFF
#define TIME_WINDOW_POWER_LIMIT_1_OFFSET	0x11
#define TIME_WINDOW_POWER_LIMIT_1_MASK		0x7F
#define TIME_WINDOW_POWER_LIMIT_2_OFFSET	0x31
#define TIME_WINDOW_POWER_LIMIT_2_MASK		0x7F

static struct timer_list updatetimer;
static u64 /*beginjiffies, endjiffies,*/ energystart, energyend;

static int to_millijoules(u32 value)
{
	u32 low, high;
	high = value / 10000;
	low = value % 10000;
	return (high * 153 + low * 153 / 10000);
}

static int to_milliseconds(u32 value)
{
	u32 low, high;
	high = value / 1000;
	low = value % 1000;
	return (high * 976 + low * 976 / 1000);
}

static ssize_t joules_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u64 output;

	if(energy_unit_type == UNIT_UNKNOWN)
		return sprintf(buf, "-1\n");

	rdmsrl(MSR_PKG_ENERGY_STATUS, output);
	
	#ifdef __INTEL_RAPL_POWER_DEBUG
	printk(KERN_INFO "Intel RAPL Power Info: Complete data read from MSR_PKG_ENERGY_STATUS is 0x%llX (however, only the first 32 bits are relevant)",output);
	#endif
	
	return sprintf(buf, "%u\n",to_millijoules((u32)output));
}

static void update_watts_timer(unsigned long data) {
	mod_timer(&updatetimer, jiffies + msecs_to_jiffies(1000));
	/*beginjiffies = endjiffies;
	endjiffies = get_jiffies_64();*/
	energystart = energyend;
	rdmsrl(MSR_PKG_ENERGY_STATUS, energyend);
}

/* Returns result in milliwatts. This is done by converting the values read from the MSR_PKG_ENERGY_STATUS
   register to millijoules. Milliwatts = Millijoules / Seconds . Since timer updates value approximately
   every 1 second, there is no division when calculating. */
static ssize_t watts_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int result;
	
	if(energy_unit_type == UNIT_UNKNOWN)
		return sprintf(buf, "-1\n");
		
	result = to_millijoules((u32)energyend)-to_millijoules((u32)energystart);
	return sprintf(buf, "%u\n",result);
}

static ssize_t power_info_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u64 output;
	rdmsrl(MSR_PKG_POWER_INFO, output);
	
	#ifdef __INTEL_RAPL_POWER_DEBUG
	printk(KERN_INFO "Intel RAPL Power Info: Complete data read from MSR_PKG_POWER_INFO is 0x%llX",output);
	#endif
	
	if (strcmp(attr->attr.name, "thermal_spec_power_watts") == 0 && power_unit_type != UNIT_UNKNOWN) {
		
		return sprintf(buf, "%llu\n",((output >> THERMAL_SPEC_POWER_OFFSET) & THERMAL_SPEC_POWER_MASK)/8);
		
	} else if (strcmp(attr->attr.name, "minimum_power_watts") == 0  && power_unit_type != UNIT_UNKNOWN) {
		
		return sprintf(buf, "%llu\n",((output >> MINIMUM_POWER_OFFSET) & MINIMUM_POWER_MASK)/8);
		
	} else if (strcmp(attr->attr.name, "maximum_power_watts") == 0  && power_unit_type != UNIT_UNKNOWN) {

		return sprintf(buf, "%llu\n",((output >> MAXIMUM_POWER_OFFSET) & MAXIMUM_POWER_MASK)/8);
		
	} else if (strcmp(attr->attr.name, "maximum_time_window_milliseconds") == 0  && time_unit_type != UNIT_UNKNOWN) {

		return sprintf(buf, "%u\n",to_milliseconds((u32)((output >> MAXIMUM_TIME_WINDOW_OFFSET ) & MAXIMUM_TIME_WINDOW_MASK)));
	}
	
	return sprintf(buf, "-1\n");
}

static ssize_t no_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	/* Do nothing */
	return -EPERM;
}

static ssize_t power_limit_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u64 output;
	rdmsrl(MSR_PKG_RAPL_POWER_LIMIT, output);
	
	#ifdef __INTEL_RAPL_POWER_DEBUG
	printk(KERN_INFO "Intel RAPL Power Info: Complete data read from MSR_PKG_RAPL_POWER_LIMIT is 0x%llX",output);
	#endif
	
	if (strcmp(attr->attr.name, "power_limit_lock") == 0) {
		return sprintf(buf, "%llu\n",(output >> PKG_POWER_LIMIT_LOCK_OFFSET) & PKG_POWER_LIMIT_LOCK_MASK);
	} else if (strcmp(attr->attr.name, "power_limit_1") == 0) {
		return sprintf(buf, "%llu\n",(output >> PKG_POWER_LIMIT_1_OFFSET) & PKG_POWER_LIMIT_1_MASK);
	} else if (strcmp(attr->attr.name, "power_limit_2") == 0) {
		return sprintf(buf, "%llu\n",(output >> PKG_POWER_LIMIT_2_OFFSET) & PKG_POWER_LIMIT_2_MASK);
	} else if (strcmp(attr->attr.name, "enable_limit_1") == 0) {
		return sprintf(buf, "%llu\n",(output >> ENABLE_LIMIT_1_OFFSET) & ENABLE_LIMIT_1_MASK);
	} else if (strcmp(attr->attr.name, "enable_limit_2") == 0) {
		return sprintf(buf, "%llu\n",(output >> ENABLE_LIMIT_2_OFFSET) & ENABLE_LIMIT_2_MASK);
	} else if (strcmp(attr->attr.name, "clamping_limit_1") == 0) {
		return sprintf(buf, "%llu\n",(output >> PKG_CLAMPING_LIMIT_1_OFFSET) & PKG_CLAMPING_LIMIT_1_MASK);
	} else if (strcmp(attr->attr.name, "clamping_limit_2") == 0) {
		return sprintf(buf, "%llu\n",(output >> PKG_CLAMPING_LIMIT_2_OFFSET) & PKG_CLAMPING_LIMIT_2_MASK);
	} else if (strcmp(attr->attr.name, "time_window_power_limit_1") == 0) {
		return sprintf(buf, "%llu\n",(output >> TIME_WINDOW_POWER_LIMIT_1_OFFSET) & TIME_WINDOW_POWER_LIMIT_1_MASK);
	} else if (strcmp(attr->attr.name, "time_window_power_limit_2") == 0) {
		return sprintf(buf, "%llu\n",(output >> TIME_WINDOW_POWER_LIMIT_2_OFFSET) & TIME_WINDOW_POWER_LIMIT_2_MASK);
	}
	
	return sprintf(buf, "-1\n");
}

static ssize_t power_limit_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 userinput;
	u64 currentval, newval, mask = 0, offset = 0;
	int writeerror = 0;
	
	sscanf(buf, "%du", &userinput);
	rdmsrl(MSR_PKG_RAPL_POWER_LIMIT, currentval);
	
	if (strcmp(attr->attr.name, "power_limit_lock") == 0) {
		offset = PKG_POWER_LIMIT_LOCK_OFFSET;
		mask = PKG_POWER_LIMIT_LOCK_MASK;
	} else if (strcmp(attr->attr.name, "power_limit_1") == 0) {
		offset = PKG_POWER_LIMIT_1_OFFSET;
		mask = PKG_POWER_LIMIT_1_MASK;
	} else if (strcmp(attr->attr.name, "power_limit_2") == 0) {
		offset = PKG_POWER_LIMIT_2_OFFSET;
		mask = PKG_POWER_LIMIT_2_MASK;
	} else if (strcmp(attr->attr.name, "enable_limit_1") == 0) {
		offset = ENABLE_LIMIT_1_OFFSET;
		mask = ENABLE_LIMIT_1_MASK;
	} else if (strcmp(attr->attr.name, "enable_limit_2") == 0) {
		offset = ENABLE_LIMIT_2_OFFSET;
		mask = ENABLE_LIMIT_2_MASK;
	} else if (strcmp(attr->attr.name, "clamping_limit_1") == 0) {
		offset = PKG_CLAMPING_LIMIT_1_OFFSET;
		mask = PKG_CLAMPING_LIMIT_1_MASK;
	} else if (strcmp(attr->attr.name, "clamping_limit_2") == 0) {
		offset = PKG_CLAMPING_LIMIT_2_OFFSET;
		mask = PKG_CLAMPING_LIMIT_2_MASK;
	} else if (strcmp(attr->attr.name, "time_window_power_limit_1") == 0) {
		offset = TIME_WINDOW_POWER_LIMIT_1_OFFSET;
		mask = TIME_WINDOW_POWER_LIMIT_1_MASK;
	} else if (strcmp(attr->attr.name, "time_window_power_limit_2") == 0) {
		offset = TIME_WINDOW_POWER_LIMIT_2_OFFSET;
		mask = TIME_WINDOW_POWER_LIMIT_2_MASK;
	}
	
	newval = ((mask & userinput) << offset) + (currentval & (~(mask << offset)));
	writeerror = native_write_msr_safe((MSR_PKG_RAPL_POWER_LIMIT), (u32)((u64)(newval)), (u32)((u64)(newval) >> 32));

	if(writeerror) {
	        #ifdef __INTEL_RAPL_POWER_DEBUG
		printk(KERN_INFO "Intel RAPL Power Info: Writing 0x%llX to MSR_PKG_RAPL_POWER_LIMIT failed ",newval);
	        #endif
	        return (writeerror > 0 ? -writeerror : writeerror);
	}
	
	return count;
}

static struct kobj_attribute joules_attribute =
	__ATTR(total_energy_millijoules, 0444, joules_show, no_store);
static struct kobj_attribute watts_attribute =
	__ATTR(current_power_milliwatts, 0444, watts_show, no_store);
static struct kobj_attribute thermalspecpower_attribute =
	__ATTR(thermal_spec_power_watts, 0444, power_info_show, no_store);
static struct kobj_attribute minimumpower_attribute =
	__ATTR(minimum_power_watts, 0444, power_info_show, no_store);
static struct kobj_attribute maximumpower_attribute =
	__ATTR(maximum_power_watts, 0444, power_info_show, no_store);
static struct kobj_attribute maxtimewindow_attribute =
	__ATTR(maximum_time_window_milliseconds, 0444, power_info_show, no_store);

static struct kobj_attribute powerlimitlock_attribute =
	__ATTR(power_limit_lock, 0644, power_limit_show, power_limit_store);
static struct kobj_attribute powerlimitone_attribute =
	__ATTR(power_limit_1, 0644, power_limit_show, power_limit_store);
static struct kobj_attribute powerlimittwo_attribute =
	__ATTR(power_limit_2, 0644, power_limit_show, power_limit_store);
static struct kobj_attribute enablelimitone_attribute =
	__ATTR(enable_limit_1, 0644, power_limit_show, power_limit_store);
static struct kobj_attribute enablelimittwo_attribute =
	__ATTR(enable_limit_2, 0644, power_limit_show, power_limit_store);
static struct kobj_attribute clampinglimitone_attribute =
	__ATTR(clamping_limit_1, 0644, power_limit_show, power_limit_store);
static struct kobj_attribute clampinglimittwo_attribute =
	__ATTR(clamping_limit_2, 0644, power_limit_show, power_limit_store);
static struct kobj_attribute timewindowpowerlimitone_attribute =
	__ATTR(time_window_power_limit_1, 0644, power_limit_show, power_limit_store);
static struct kobj_attribute timewindowpowerlimittwo_attribute =
	__ATTR(time_window_power_limit_2, 0644, power_limit_show, power_limit_store);

static struct attribute *attrs[] = {
	&joules_attribute.attr,
	&watts_attribute.attr,
	&thermalspecpower_attribute.attr,
	&minimumpower_attribute.attr,
	&maximumpower_attribute.attr,
	&maxtimewindow_attribute.attr,
	&powerlimitlock_attribute.attr,
	&powerlimitone_attribute.attr,
	&powerlimittwo_attribute.attr,
	&enablelimitone_attribute.attr,
	&enablelimittwo_attribute.attr,
	&clampinglimitone_attribute.attr,
	&clampinglimittwo_attribute.attr,
	&timewindowpowerlimitone_attribute.attr,
	&timewindowpowerlimittwo_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int rapl_check_unit(void)
{
	u64 output;

	if (boot_cpu_data.x86 != 0x06 || (boot_cpu_data.x86_model != 0x2A && boot_cpu_data.x86_model != 0x2D)) {
		#ifdef __INTEL_RAPL_POWER_DEBUG
		printk(KERN_WARNING "Intel RAPL Power Info: processor is not Sandy Bridge");
		#endif
		energy_unit_type = UNIT_UNKNOWN;
		power_unit_type = UNIT_UNKNOWN;
		time_unit_type = UNIT_UNKNOWN;
		return -ENODEV;
	} else {
		rdmsrl(MSR_RAPL_POWER_UNIT, output);
	
		if (((output >> ENERGY_UNIT_OFFSET) & ENERGY_UNIT_MASK) == 0x10) {
			energy_unit_type = ENERGY_UNIT_DEFAULT;
		} else {
			#ifdef __INTEL_RAPL_POWER_DEBUG
			printk(KERN_WARNING "Intel RAPL Power Info: unknown units for energy");
			#endif
			energy_unit_type = UNIT_UNKNOWN;
		}
		
		if (((output >> POWER_UNIT_OFFSET) & POWER_UNIT_MASK) == 0x3) {
			power_unit_type = POWER_UNIT_DEFAULT;
		} else {
			#ifdef __INTEL_RAPL_POWER_DEBUG
			printk(KERN_WARNING "Intel RAPL Power Info: unknown units for power");
			#endif
			power_unit_type = UNIT_UNKNOWN;
		}
		
		if (((output >> TIME_UNIT_OFFSET) & TIME_UNIT_MASK) == 0xA) {
			time_unit_type = TIME_UNIT_DEFAULT;
		} else {
			#ifdef __INTEL_RAPL_POWER_DEBUG
			printk(KERN_WARNING "Intel RAPL Power Info: unknown units for time");
			#endif
			time_unit_type = UNIT_UNKNOWN;
		}
	}
	return 0;
}

static struct kobject *powerinfo_kobj;

static int __init powerinfoinit(void)
{
	int retval;

	retval = rapl_check_unit();
	if(retval) return retval;
	
	/*powerinfo_kobj = kobject_create_and_add("cpu_power_info", kset_find_obj(to_kset(kernel_kobj),"power"));*/
	powerinfo_kobj = kobject_create_and_add("cpu_power_info", kernel_kobj);
	if (!powerinfo_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(powerinfo_kobj, &attr_group);
	if (retval)
		kobject_put(powerinfo_kobj);

	if (!retval && energy_unit_type != UNIT_UNKNOWN) {
		setup_timer(&updatetimer, update_watts_timer, 0);
		mod_timer(&updatetimer, jiffies + msecs_to_jiffies(1000));
		/*endjiffies = get_jiffies_64();
		startjiffies = endjiffies - 1;*/
		rdmsrl(MSR_PKG_ENERGY_STATUS, energyend);
		energystart = energyend - 1;
	}
	
	return retval;
}

static void __exit powerinfoexit(void)
{
	kobject_put(powerinfo_kobj);
	del_timer(&updatetimer);
}

module_init(powerinfoinit);
module_exit(powerinfoexit);
