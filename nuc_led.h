/*
 * Intel NUC NUC8i7HVK (Hades) LED Control WMI Driver
 *
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

static unsigned int nuc_led_perms __read_mostly = S_IRUGO | S_IWUSR | S_IWGRP;
static unsigned int nuc_led_uid __read_mostly;
static unsigned int nuc_led_gid __read_mostly;

module_param(nuc_led_perms, uint, S_IRUGO | S_IWUSR | S_IWGRP);
module_param(nuc_led_uid, uint, 0);
module_param(nuc_led_gid, uint, 0);

MODULE_PARM_DESC(nuc_led_perms, "permissions on /proc/acpi/nuc_led");
MODULE_PARM_DESC(nuc_led_uid, "default owner of /proc/acpi/nuc_led");
MODULE_PARM_DESC(nuc_led_gid, "default owning group of /proc/acpi/nuc_led");

/* Intel NUC WMI GUID */
#define NUCLED_WMI_MGMT_GUID "8C5DA44C-CDC3-46B3-8619-4E26D34390B7"
MODULE_ALIAS("wmi:" NUCLED_WMI_MGMT_GUID);

/* LED Control Method ID */
#define NUCLED_WMI_METHODID_GETSTATE						0x01
#define NUCLED_WMI_METHODID_SETSTATE						0x02
#define NUCLED_WMI_METHODID_QUERYLED						0x03
#define NUCLED_WMI_METHODID_NEWGETLEDSTATUS					0x04
#define NUCLED_WMI_METHODID_SETINDICATOROPTIONLEDTYPE		0x05
#define NUCLED_WMI_METHODID_SETVALUEINDICATOROPTIONLEDTYPE	0x06
#define NUCLED_WMI_METHODID_APPNOTIFY						0x07
#define NUCLED_WMI_METHODID_GETHDMIADDR4CEC					0x07

/* NUCLED_WMI_METHODID_QUERYLED arguments */
#define NUCLED_WMI_METHODARG_QUERYLEDCOLORTYPE		0x01
#define NUCLED_WMI_METHODARG_QUERYINDICATORSUPPORT	0x02

/* NUCLED_WMI_METHODID_NEWGETLEDSTATUS arguments */
#define NUCLED_WMI_METHODARG_GETCURRENTINDICATOR		0x00
#define NUCLED_WMI_METHODARG_GETINDICATOROPTIONVALUE	0x01

/* proc interaction */
#define NUCLED_PROC_SET_INDICATOR			0x01
#define NUCLED_PROC_SETINDICATOROPTIONVALUE	0x02

/* Indicator options / usage types */
#define NUCLED_USAGE_TYPE_POWER_STATE	0x00
#define NUCLED_USAGE_TYPE_HDD_ACTIVITY	0x01
#define NUCLED_USAGE_TYPE_ETHERNET		0x02
#define NUCLED_USAGE_TYPE_WIFI			0x03
#define NUCLED_USAGE_TYPE_SOFTWARE		0x04
#define NUCLED_USAGE_TYPE_POWER_LIMIT	0x05
#define NUCLED_USAGE_TYPE_DISABLE		0x06

/* Return codes */
#define NUCLED_WMI_RETURN_SUCCESS		0x00
#define NUCLED_WMI_RETURN_NOSUPPORT		0xE1
#define NUCLED_WMI_RETURN_UNDEFINED		0xE2
#define NUCLED_WMI_RETURN_NORESPONSE	0xE3
#define NUCLED_WMI_RETURN_BADPARAM		0xE4
#define NUCLED_WMI_RETURN_UNEXPECTED	0xEF

typedef union {
	u8 flags;
	struct {
		u8 power : 1,
			hdd : 1,
			skull : 1,
			eyes : 1,
			front1 : 1,
			front2 : 1,
			front3 : 1,
			unused0 : 1;
	};
} LED_TYPES;

typedef union {
	u8 flags;
	struct {
		u8 blue_amber : 1,
			blue_white : 1,
			rgb : 1,
			unused5 : 1,
			unused4 : 1,
			unused3 : 1,
			unused2 : 1,
			unused1 : 1;
	};
} LED_COLOR_TYPES;

typedef union {
	u8 flags;
	struct {
		u8 power_state : 1,
			hdd_activity : 1,
			ethernet : 1,
			wifi : 1,
			software : 1,
			power_limit : 1,
			disable : 1,
			unused1 : 1;
	};
} INDICATOR_OPTIONS;

typedef struct {
	u8 red;
	u8 green;
	u8 blue;
} LED_RGB;

typedef struct {
	u8 brightness;
	u8 blink_behavior;
	u8 blink_freq;
	LED_RGB color;
} BLINK_LED;

typedef struct {
	u8 brightness;
	LED_RGB color;
} FLASH_LED;

struct power_state_indicator {
	BLINK_LED s0;
	BLINK_LED s3;
	BLINK_LED ready_mode;
	BLINK_LED s5;
} __packed;

struct hdd_activity_indicator {
	FLASH_LED led;
	u8 behavior;
} __packed;

struct ethernet_indicator {
	u8 type;
	FLASH_LED led;
} __packed;

struct wifi_indicator {
	FLASH_LED led;
} __packed;

struct software_indicator {
	BLINK_LED led;
} __packed;

struct power_limit_indicator {
	u8 indication_scheme;
	FLASH_LED led;
} __packed;

extern struct proc_dir_entry *acpi_root_dir;

typedef struct {
	const char *name;
	u8 led_type;
	LED_COLOR_TYPES led_color_type;
	u8 usage_type;
	u8 indicator_option;
	u8 *indicator;
} LED_INFO;

struct acpi_args {
	u8 arg1;
	u8 arg2;
	u8 arg3;
	u8 arg4;
} __packed;

#define BUFFER_SIZE 4096
static char result_buffer[BUFFER_SIZE];
static char *get_buffer_end(void) {
	return result_buffer + strlen(result_buffer);
}

static const char *const led_names[] = {
	"Power",
	"HDD",
	"Skull",
	"Eyes",
	"Front 1",
	"Front 2",
	"Front 3",
};
static const char *const led_color_types[] = {"Blue/Amber", "Blue/White", "RGB"};
static const char *const led_usage_types[] = {"Power state", "HDD Activity", "Ethernet", "Wifi", "Software", "Power Limit", "Disable"};
static const char *const led_blink_behaviors[] = {"Solid", "Breathing", "Pulsing", "Strobing"};
static const char *const led_flash_behaviors[] = {"Normally off, ON when active", "Normally on, OFF when active"};
static const char *const led_ethernet_type[] = {"LAN1", "LAN2", "LAN1 + LAN2"};
static const char *const led_power_limit_indication_scheme[] = {"Green to Red", "Single Color"};

/* Convert blink/fade value to text */
static const char *const blink_fade_text[] = {"Off", "1Hz Blink", "0.25Hz Blink", "1Hz Fade", "Always On", "0.5Hz Blink", "0.25Hz Fade", "0.5Hz Fade"};

/* Convert color value to text */
static const char *const pwrcolor_text[] = {"Off", "Blue", "Amber"};
static const char *const ringcolor_text[] = {"Off", "Cyan", "Pink", "Yellow", "Blue", "Red", "Green", "White"};

unsigned int countSetBits(int n)
{
	unsigned int count = 0;
	while (n) {
		n &= (n - 1);
		count++;
	}
	return count;
}

unsigned int bitIndexToIndex(int bitIndex)
{
	switch (bitIndex) {
		case 1: return 0;
		case 2: return 1;
		case 4:	return 2;
		case 8:	return 3;
		case 16: return 4;
		case 32: return 5;
		case 64: return 6;
		case 128: return 7;
	}
	return 0;
}
