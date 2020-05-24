# DISCLAIMER

Use at your own risk! In the event that your computer breaks due to use of this software, the author will not
be held responisble for any damages caused!

# Intel NUC8i7HVK LED Control

This is a simple kernel module to control the LEDs on Intel NUC8i7HVK (Hades) kits.

This module is intended as a demonstration/proof-of-concept and may not be maintained further.  Perhaps
it can act as a jumping off point for a more polished and complete implementation.  For testing and basic
manipulation of the LEDs, it ought to work fine, but use with caution none the less. This
has only been tested on 5.6.x kernels.

This kernel module was initially developed by Miles Peterson (https://github.com/milesp20/intel_nuc_led)
for Intel NUC7i[x]BN and NUC6CAY LED Control, then forked for NIC8i[x]HVK LED control by Patrick Kullman
 (https://github.com/nomego/intel_nuc_led).


## Requirements

Requirements:

* Intel NUC8i7HVK
* Latest BIOS
* ACPI/WMI support in kernel
* LED(s) set to `SW Control` in BIOS

## Building

THe `nuc_led` kernel module supports building and installing "from source" directly or using `dkms`.

### 1. Installing Build Dependencies

Ubuntu:

```
sudo apt install build-essential git linux-headers-$(uname -r)

# DKMS dependencies
sudo apt install debhelper dkms
```

Redhat:

```
yum groupinstall "Development Tools"
yum install kernel-devel-$(uname -r)

# Install appropriate EPEL for DKMS if needed by your RHEL variant
yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm

yum install dkms
```

### 2. Fetching the module

```
git clone https://github.com/benjaminvialle/intel_nuc_led
cd intel_nuc_led
```

### 3a. Building and Installing Using DKMS (recommended)

```
sudo make dkms-install
```

Uninstall:

```
sudo make dkms-uninstall
```

### 3b. Building and Installing Using DKMS (packaged)

```
# Ubuntu
make dkms-deb

# RHEL
make dkms-rpm

# Install generated DEB/RPM from the folder specified in the output using system package manager
```

### 3c. Building and Installing "from source"

```
make clean
make install
```

### 4. Loading the module

```
sudo modprobe nuc_led
```

### 5. Updating the module

```
cd intel_nuc_led
sudo rmmod nuc_led
sudo make dkms-uninstall
git pull
sudo make dkms-install
sudo modprobe nuc_led
```


## Usage

This driver works via '/proc/acpi/nuc_led'.  To get current LED state:

```
cat /proc/acpi/nuc_led
```

Sample output:
```
LED 0 (Power) - Color type: RGB
  Supported indicators: Power state  HDD Activity  Ethernet  Wifi  Software  Power Limit  Disable
  Current indicator: Power state

        S0 (On): 50% Solid rgb(0,0,255) (10 dHz)
     S3 (Sleep): 100% Breathing rgb(0,0,255) (10 dHz)
     Ready mode: 0% Solid rgb(0,0,0) (0 dHz)
  S5 (Soft off): 0% Solid rgb(0,0,0) (0 dHz)


LED 2 (Skull) - Color type: RGB
  Supported indicators: Power state  HDD Activity  Ethernet  Wifi  Software  Power Limit  Disable
  Current indicator: Power state

        S0 (On): 50% Solid rgb(10,0,255) (10 dHz)
     S3 (Sleep): 100% Breathing rgb(0,0,255) (10 dHz)
     Ready mode: 0% Solid rgb(0,0,0) (0 dHz)
  S5 (Soft off): 0% Solid rgb(0,0,0) (0 dHz)


LED 3 (Eyes) - Color type: RGB
  Supported indicators: Power state  HDD Activity  Ethernet  Wifi  Software  Power Limit  Disable
  Current indicator: Power state

        S0 (On): 50% Breathing rgb(255,0,0) (3 dHz)
     S3 (Sleep): 100% Breathing rgb(0,0,255) (10 dHz)
     Ready mode: 0% Solid rgb(0,0,0) (0 dHz)
  S5 (Soft off): 0% Solid rgb(0,0,0) (0 dHz)


LED 4 (Front 1) - Color type: RGB
  Supported indicators: Power state  HDD Activity  Ethernet  Wifi  Software  Power Limit  Disable
  Current indicator: HDD Activity

  HDD LED: 50% rgb(255,0,0) Normally off, ON when active


LED 5 (Front 2) - Color type: RGB
  Supported indicators: Power state  HDD Activity  Ethernet  Wifi  Software  Power Limit  Disable
  Current indicator: Wifi

  Wifi LED: 50% rgb(0,10,250)


LED 6 (Front 3) - Color type: RGB
  Supported indicators: Power state  HDD Activity  Ethernet  Wifi  Software  Power Limit  Disable
  Current indicator: Power Limit

  Power Limit LED: Green to Red  50% rgb(0,0,255)
```


To change the LED state:

```
 echo '<action>,<led id>,<indicator id>,[setting],[value]' | sudo tee /proc/acpi/nuc_led > /dev/null
```

|Action              |Description                  |
|--------------------|-----------------------------|
|set_indicator       |Change LED indicator type.   |
|set_indicator_value |Change LED indicator setting.|


Example execution to set Front 2 LED (5) to Wifi indicator type (3):

    echo 'set_indicator,5,3' | sudo tee /proc/acpi/nuc_led

Example execution to set the Skull LED (2), Indicator type Power state (0), setting S0 RGB color red (3) to 10:

    echo 'set_indicator_value,2,0,3,10' | sudo tee /proc/acpi/nuc_led > /dev/null

Errors in passing parameters will appear as warnings in dmesg.
**NOTE** Not all warnings are implemented and may send unsupported data, which can inactivate the LEDs (or worse!)


You can change the owner, group and permissions of `/proc/acpi/nuc_led` by passing parameters to the nuc_led kernel module. Use:

* `nuc_led_uid` to set the owner (default is 0, root)
* `nuc_led_gid` to set the owning group (default is 0, root)
* `nuc_led_perms` to set the file permissions (default is r+w for group and user and r for others)

Note: Once an LED has been set to `SW Control` in the BIOS, it will remain off initially until a color is explicitly set, after which the set color is retained across reboots.
