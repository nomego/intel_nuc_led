/*
 * Intel NUC NUC8i7HVK (Hades) LED Control WMI Driver
 *
 * Copyright (C) 2020 Benjamin Vialle <benjamin@vialle.io>
 *
 * Forked from intel_nuc_led (https://github.com/nomego/intel_nuc_led)
 * Copyright (C) 2018 Patrik Kullman
 * 
 * Forked from intel_nuc_led (https://github.com/milesp20/intel_nuc_led)
 * Copyright (C) 2017 Miles Peterson
 *
 * Portions based on asus-wmi.c:
 * Copyright (C) 2010 Intel Corporation.
 * Copyright (C) 2010-2011 Corentin Chary <corentin.chary@gmail.com>
 *
 * Portions based on acpi_call.c:
 * Copyright (C) 2010: Michal Kottman
 *
 * Based on WMI Interface for Intel® NUC Products - WMI Specification, August 2018 rev 0.64
 * (specs/INTEL_WMI_LED_0.64.pdf)
 * 
 * Based on Intel Article ID 000023426
 * http://www.intel.com/content/www/us/en/support/boards-and-kits/intel-nuc-kits/000023426.html
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/acpi.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("Benjamin Vialle");
MODULE_DESCRIPTION("Intel NUC NUC8i7HVK (Hades) LED Control WMI Driver");
MODULE_LICENSE("GPL");
ACPI_MODULE_NAME("NUC_LED");

#include "nuc_led.h"

LED_INFO *leds;

static int nuc_led_get_indicator_items(u8 led_id, u8 indicator_id, u8 items,
				       u8 *indicator)
{
	struct acpi_args args = {
		.arg1 = NUCLED_WMI_METHODARG_GETINDICATOROPTIONVALUE,
		.arg2 = led_id,
		.arg3 = indicator_id,
		.arg4 = 0
	};
	struct acpi_buffer input;
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;
	u8 i;

	input.length = (acpi_size)sizeof(args);
	input.pointer = &args;

	for (i = 0; i < items; i++) {
		args.arg4 = i;
		status =
			wmi_evaluate_method(NUCLED_WMI_MGMT_GUID, 0,
					    NUCLED_WMI_METHODID_NEWGETLEDSTATUS,
					    &input, &output);

		if (ACPI_FAILURE(status)) {
			ACPI_EXCEPTION(
				(AE_INFO, status, "wmi_evaluate_method"));
			return -EIO;
		}

		// Always returns a buffer
		obj = (union acpi_object *)output.pointer;
		indicator[i] = obj->buffer.pointer[1];
		// pr_info("LED %i, ind id %d, item %d, val %d", led_id, indicator_id, i, obj->buffer.pointer[1]);
	}

	kfree(obj);

	return 0;
}

static int nuc_led_fill_indicator_values(LED_INFO *led)
{
	int ssize;
	switch (led->indicator_option) {
	case NUCLED_USAGE_TYPE_POWER_STATE:
		ssize = sizeof(struct power_state_indicator);
		break;
	case NUCLED_USAGE_TYPE_HDD_ACTIVITY:
		ssize = sizeof(struct hdd_activity_indicator);
		break;
	case NUCLED_USAGE_TYPE_ETHERNET:
		ssize = sizeof(struct ethernet_indicator);
		break;
	case NUCLED_USAGE_TYPE_WIFI:
		ssize = sizeof(struct wifi_indicator);
		break;
	case NUCLED_USAGE_TYPE_SOFTWARE:
		ssize = sizeof(struct software_indicator);
		break;
	case NUCLED_USAGE_TYPE_POWER_LIMIT:
		ssize = sizeof(struct power_limit_indicator);
		break;
	default:
		pr_warn("Unexpected indicator option %d\n",
			led->indicator_option);
		return 0;
	}
	led->indicator = vmalloc(ssize);
	return nuc_led_get_indicator_items(led->led_type, led->indicator_option,
					   ssize, led->indicator);
}

/* Get LED */
static int nuc_led_get_led(u8 led_id, LED_INFO *led)
{
	struct acpi_args args = {
		.arg1 = NUCLED_WMI_METHODARG_QUERYLEDCOLORTYPE, .arg2 = led_id
	};
	struct acpi_buffer input;
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;

	input.length = (acpi_size)sizeof(args);
	input.pointer = &args;

	// Per Intel docs, first instance is used (instance is indexed from 0)
	status = wmi_evaluate_method(NUCLED_WMI_MGMT_GUID, 0,
				     NUCLED_WMI_METHODID_QUERYLED, &input,
				     &output);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "wmi_evaluate_method"));
		return -EIO;
	}

	// Always returns a buffer
	obj = (union acpi_object *)output.pointer;

	led->led_type = led_id;
	led->name = led_names[led_id];
	led->led_color_type.flags = obj->buffer.pointer[1];

	// pr_info("LED %i (%s) - Color type blue_amber %i, blue_white %i, rgb %i", led_id, led->name, led->led_color_type.blue_amber, led->led_color_type.blue_white, led->led_color_type.rgb);

	args.arg1 = NUCLED_WMI_METHODARG_QUERYINDICATORSUPPORT;

	// Per Intel docs, first instance is used (instance is indexed from 0)
	status = wmi_evaluate_method(NUCLED_WMI_MGMT_GUID, 0,
				     NUCLED_WMI_METHODID_QUERYLED, &input,
				     &output);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "wmi_evaluate_method"));
		return -EIO;
	}

	obj = (union acpi_object *)output.pointer;
	led->usage_type = obj->buffer.pointer[1];

	// pr_info("LED %i - Got power_state %i, hdd_activity %i, ethernet %i, wifi %i, software %i, power_limit %i, disable %i", led_id, led->usage_type.power_state, led->usage_type.hdd_activity, led->usage_type.ethernet, led->usage_type.wifi, led->usage_type.software, led->usage_type.power_limit, led->usage_type.disable);

	args.arg1 = NUCLED_WMI_METHODARG_GETCURRENTINDICATOR;

	// Per Intel docs, first instance is used (instance is indexed from 0)
	status = wmi_evaluate_method(NUCLED_WMI_MGMT_GUID, 0,
				     NUCLED_WMI_METHODID_NEWGETLEDSTATUS,
				     &input, &output);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "wmi_evaluate_method"));
		return -EIO;
	}

	obj = (union acpi_object *)output.pointer;
	led->indicator_option = obj->buffer.pointer[1];

	// pr_info("LED %i - Got indicator option %i", led_id, obj->buffer.pointer[1]);

	kfree(obj);

	nuc_led_fill_indicator_values(led);

	return 0;
}

/* Get LEDs */
static int nuc_led_get_leds(void)
{
	struct acpi_args args = { .arg1 = 0 };
	struct acpi_buffer input;
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;

	LED_TYPES led_types;
	int flags, i, o = 0, num_leds;

	input.length = (acpi_size)sizeof(args);
	input.pointer = &args;

	// Per Intel docs, first instance is used (instance is indexed from 0)
	status = wmi_evaluate_method(NUCLED_WMI_MGMT_GUID, 0,
				     NUCLED_WMI_METHODID_QUERYLED, &input,
				     &output);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "wmi_evaluate_method"));
		return -EIO;
	}

	// Always returns a buffer
	obj = (union acpi_object *)output.pointer;
	flags = led_types.flags = obj->buffer.pointer[1];

	// pr_info("Got pwr %i, hdd %i, skull %i, eyes %i, front1 %i, front2 %i, front3 %i", led_types.power, led_types.hdd, led_types.skull, led_types.eyes, led_types.front1, led_types.front2, led_types.front3);

	num_leds = countSetBits(led_types.flags);
	// pr_info("Num leds: %i", num_leds);
	leds = vmalloc(sizeof(LED_INFO) * num_leds);

	for (i = 0; i < 8; i++) {
		if (flags & 0x01) {
			nuc_led_get_led(i, &leds[o++]);
		}
		flags = flags >> 1;
	}

	kfree(obj);

	return num_leds;
}

static int nuc_led_set_indicator(u8 led_id, u8 indicator_id)
{
	struct acpi_args args = { .arg1 = led_id, .arg2 = indicator_id };
	struct acpi_buffer input;
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status status;

	input.length = (acpi_size)sizeof(args);
	input.pointer = &args;

	status = wmi_evaluate_method(
		NUCLED_WMI_MGMT_GUID, 0,
		NUCLED_WMI_METHODID_SETINDICATOROPTIONLEDTYPE, &input, &output);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "wmi_evaluate_method"));
		return -EIO;
	}
	return 0;
}

static int nuc_led_set_indicator_option(u8 led_id, u8 indicator_id, u8 item_id,
					u8 value)
{
	struct acpi_args args = { .arg1 = led_id,
				  .arg2 = indicator_id,
				  .arg3 = item_id,
				  .arg4 = value };
	struct acpi_buffer input;
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status status;

	input.length = (acpi_size)sizeof(args);
	input.pointer = &args;

	// Per Intel docs, first instance is used (instance is indexed from 0)
	status = wmi_evaluate_method(
		NUCLED_WMI_MGMT_GUID, 0,
		NUCLED_WMI_METHODID_SETVALUEINDICATOROPTIONLEDTYPE, &input,
		&output);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "wmi_evaluate_method"));
		return -EIO;
	}
	return 0;
}

static ssize_t acpi_proc_write(struct file *filp, const char __user *buff,
			       size_t len, loff_t *data)
{
	int i = 0;
	int ret = 0;
	char *input, *arg, *sep;
	//static int status = 0;
	u8 action, led_id, indicator_id, indicator_setting, setting_value;

	// Move buffer from user space to kernel space
	input = vmalloc(len);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, buff, len))
		return -EFAULT;

	// Strip new line
	input[len] = '\0';
	if (input[len - 1] == '\n')
		input[len - 1] = '\0';

	// Parse input string
	sep = input;
	while ((arg = strsep(&sep, ",")) && *arg) {
		switch (i) {
		case 0: // First arg: operation ("set_indicator" or "set_indicator_value")
			if (!strcmp(arg, "set_indicator")) {
				action = NUCLED_PROC_SET_INDICATOR;
			} else if (!strcmp(arg, "set_indicator_value")) {
				action = NUCLED_PROC_SETINDICATOROPTIONVALUE;
			} else {
				pr_warn("Invalid action (%s) while setting NUC LED state\n",
					arg);
				ret = -EINVAL;
			}
			break;

		case 1: // Second arg: LED ID
			if (kstrtou8(arg, 0, &led_id)) {
				pr_warn("Invalid LED ID (%s) while setting NUC LED state\n",
					arg);
				ret = -EINVAL;
			}
			break;

		case 2: // Third arg: indicator id
			if (kstrtou8(arg, 0, &indicator_id)) {
				pr_warn("Invalid indicator ID (%s) while setting NUC LED state\n",
					arg);
				ret = -EINVAL;
			}
			break;

		case 3: // Fourth arg (for set_indicator_value): indicator setting
			if (action == NUCLED_PROC_SET_INDICATOR) {
				pr_warn("Too many arguments for action set_indicator while setting NUC LED state\n");
				ret = -EOVERFLOW;
				break;
			}
			if (kstrtou8(arg, 0, &indicator_setting)) {
				pr_warn("Invalid indicator setting (%s) while setting NUC LED state\n",
					arg);
				ret = -EINVAL;
			}
			break;

		case 4: // Fifth arg (for set_indicator_value): indicator setting value
			if (action == NUCLED_PROC_SET_INDICATOR) {
				pr_warn("Too many arguments for action set_indicator while setting NUC LED state\n");
				ret = -EOVERFLOW;
				break;
			}
			if (kstrtou8(arg, 0, &setting_value)) {
				pr_warn("Invalid indicator setting (%s) while setting NUC LED state\n",
					arg);
				ret = -EINVAL;
			}
			break;

		default: // Too many args!
			pr_warn("Too many arguments while setting NUC LED state\n");
			ret = -EOVERFLOW;
			break;
		}

		if (ret != 0) {
			break;
		}

		// Track iterations
		i++;
	}

	vfree(input);

	if (ret == 0) {
		if (i != 3 && action == NUCLED_PROC_SET_INDICATOR) {
			pr_warn("Too few arguments (%d), needs 3, while setting NUC LED indicator\n",
				i);
			ret = -EINVAL;
		}

		if (i != 5 && action == NUCLED_PROC_SETINDICATOROPTIONVALUE) {
			pr_warn("Too few arguments (%d), needs 5, while setting NUC LED indicator\n",
				i);
			ret = -EINVAL;
		}
	}

	if (ret != 0) {
		return len;
	}

	switch (action) {
	case NUCLED_PROC_SET_INDICATOR:
		pr_info("Setting LED %i indicator to %i\n", led_id,
			indicator_id);
		nuc_led_set_indicator(led_id, indicator_id);
		break;
	case NUCLED_PROC_SETINDICATOROPTIONVALUE:
		pr_info("Setting LED %i indicator %i option %i to %i\n", led_id,
			indicator_id, indicator_setting, setting_value);
		nuc_led_set_indicator_option(led_id, indicator_id,
					     indicator_setting, setting_value);
		break;
	}

	/*
	status = nuc_led_set_state(led, brightness, blink_fade, color_state, &retval);
	if (status)
	{
		pr_warn("Unable to set NUC LED state: WMI call failed\n");
	}
	else
	{
		if (retval.brightness_return == NUCLED_WMI_RETURN_UNDEFINED)
		{
			if (led == NUCLED_WMI_POWER_LED_ID)
				pr_warn("Unable set NUC power LED state: not set for SW control\n");
			else if (led == NUCLED_WMI_RING_LED_ID)
				pr_warn("Unable set NUC ring LED state: not set for SW control\n");
		}
		else if (retval.brightness_return == NUCLED_WMI_RETURN_BADPARAM || retval.blink_fade_return == NUCLED_WMI_RETURN_BADPARAM ||
				retval.color_return == NUCLED_WMI_RETURN_BADPARAM)
		{
			pr_warn("Unable to set NUC LED state: invalid parameter\n");
		}
		else if (retval.brightness_return != NUCLED_WMI_RETURN_SUCCESS)
		{
			pr_warn("Unable to set NUC LED state: WMI call returned error (0x%02x)\n", retval.brightness_return);
		}
	}*/

	return len;
}

static void print_color(LED_RGB *rgb)
{
	sprintf(get_buffer_end(), "rgb(%d,%d,%d)", rgb->red, rgb->green,
		rgb->blue);
}

static void print_blink_led(BLINK_LED *led)
{
	sprintf(get_buffer_end(), "%d%% %s ", led->brightness,
		led_blink_behaviors[led->blink_behavior]);
	print_color(&led->color);
	sprintf(get_buffer_end(), " (%d dHz)", led->blink_freq);
}

static void print_flash_led(FLASH_LED *led)
{
	sprintf(get_buffer_end(), "%d%% ", led->brightness);
	print_color(&led->color);
}

static void print_led(LED_INFO *led)
{
	int i;
	struct power_state_indicator *power_state_ind;
	struct hdd_activity_indicator *hdd_activity_ind;
	struct ethernet_indicator *ethernet_ind;
	struct wifi_indicator *wifi_ind;
	struct software_indicator *software_ind;
	struct power_limit_indicator *power_limit_ind;

	sprintf(get_buffer_end(), "LED %i (%s) - Color type: %s\n",
		led->led_type, led->name,
		led_color_types[bitIndexToIndex(led->led_color_type.flags)]);

	sprintf(get_buffer_end(), "  Supported indicators: ");
	for (i = 1; i <= 128; i = i << 1) {
		if (led->usage_type & i) {
			sprintf(get_buffer_end(), "%s  ",
				led_usage_types[bitIndexToIndex(i)]);
		}
	}

	sprintf(get_buffer_end(), "\n  Current indicator: %s\n",
		led_usage_types[led->indicator_option]);

	switch (led->indicator_option) {
	case NUCLED_USAGE_TYPE_POWER_STATE:
		power_state_ind =
			(struct power_state_indicator *)led->indicator;

		sprintf(get_buffer_end(), "\n        S0 (On): ");
		print_blink_led(&power_state_ind->s0);
		sprintf(get_buffer_end(), "\n     S3 (Sleep): ");
		print_blink_led(&power_state_ind->s3);
		sprintf(get_buffer_end(), "\n     Ready mode: ");
		print_blink_led(&power_state_ind->ready_mode);
		sprintf(get_buffer_end(), "\n  S5 (Soft off): ");
		print_blink_led(&power_state_ind->s5);
		sprintf(get_buffer_end(), "\n");
		break;
	case NUCLED_USAGE_TYPE_HDD_ACTIVITY:
		hdd_activity_ind =
			(struct hdd_activity_indicator *)led->indicator;
		sprintf(get_buffer_end(), "\n  HDD LED: ");
		print_flash_led(&hdd_activity_ind->led);
		sprintf(get_buffer_end(), " %s\n",
			led_flash_behaviors[hdd_activity_ind->behavior]);
		break;
	case NUCLED_USAGE_TYPE_ETHERNET:
		ethernet_ind =
			(struct ethernet_indicator *)led->indicator;
		sprintf(get_buffer_end(), "\n  Ethernet LED: ");
		sprintf(get_buffer_end(), "%s  ",
			led_ethernet_type[ethernet_ind->type]);
		print_flash_led(&ethernet_ind->led);
		sprintf(get_buffer_end(), "\n");
		break;
	case NUCLED_USAGE_TYPE_WIFI:
		wifi_ind =
			(struct wifi_indicator *)led->indicator;
		sprintf(get_buffer_end(), "\n  Wifi LED: ");
		print_flash_led(&wifi_ind->led);
		sprintf(get_buffer_end(), "\n");
		break;
	case NUCLED_USAGE_TYPE_SOFTWARE:
		software_ind =
			(struct software_indicator *)led->indicator;
		sprintf(get_buffer_end(), "\n  Software LED: ");
		print_blink_led(&software_ind->led);
		sprintf(get_buffer_end(), "\n");
		break;
	case NUCLED_USAGE_TYPE_POWER_LIMIT:
		power_limit_ind =
			(struct power_limit_indicator *)led->indicator;
		sprintf(get_buffer_end(), "\n  Power Limit LED: ");
		sprintf(get_buffer_end(), "%s  ",
			led_power_limit_indication_scheme[power_limit_ind->indication_scheme]);
		print_flash_led(&power_limit_ind->led);
		sprintf(get_buffer_end(), "\n");
		break;
	default:
		break;
	}
}

static ssize_t acpi_proc_read(struct file *filp, char __user *buff,
			      size_t count, loff_t *off)
{
	ssize_t ret;

	int i, len = 0;
	int num_leds = 0;
	// Clear buffer
	memset(result_buffer, 0, BUFFER_SIZE);

	num_leds = nuc_led_get_leds();

	for (i = 0; i < num_leds; i++) {
		print_led(&leds[i]);
		if (i + 1 < num_leds) {
			sprintf(get_buffer_end(), "\n\n");
		}
	}

	// Return buffer via proc
	len = strlen(result_buffer);
	ret = simple_read_from_buffer(buff, count, off, result_buffer, len + 1);

	return ret;
}

static struct proc_ops proc_acpi_operations = {
	.proc_read = acpi_proc_read,
	.proc_write = acpi_proc_write,
};

/* Init & unload */
static int __init init_nuc_led(void)
{
	struct proc_dir_entry *acpi_entry;
	kuid_t uid;
	kgid_t gid;

	// Make sure LED control WMI GUID exists
	if (!wmi_has_guid(NUCLED_WMI_MGMT_GUID)) {
		pr_warn("Intel NUC LED WMI GUID not found\n");
		return -ENODEV;
	}

	// Verify the user parameters
	uid = make_kuid(&init_user_ns, nuc_led_uid);
	gid = make_kgid(&init_user_ns, nuc_led_gid);

	if (!uid_valid(uid) || !gid_valid(gid)) {
		pr_warn("Intel NUC LED control driver got an invalid UID or GID\n");
		return -EINVAL;
	}

	// Create nuc_led ACPI proc entry
	acpi_entry = proc_create("nuc_led", nuc_led_perms, acpi_root_dir,
				 &proc_acpi_operations);

	if (acpi_entry == NULL) {
		pr_warn("Intel NUC LED control driver could not create proc entry\n");
		return -ENOMEM;
	}

	proc_set_user(acpi_entry, uid, gid);

	pr_info("Intel NUC LED control driver loaded\n");

	return 0;
}

static void __exit unload_nuc_led(void)
{
	remove_proc_entry("nuc_led", acpi_root_dir);
	pr_info("Intel NUC LED control driver unloaded\n");
}

module_init(init_nuc_led);
module_exit(unload_nuc_led);
