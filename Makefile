ifneq ($(KERNELRELEASE),)
       obj-m := nuc_led.o
else
       KVERSION := $(shell uname -r)
       KDIR := /lib/modules/$(KVERSION)/build
       PWD := $(shell pwd)

.PHONY: clean default dkms-add dkms-build dkms-deb dkms-install dkms-rpm dkms-uninstall install

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

dkms-add:
	dkms add --force $(PWD)

dkms-build: dkms-add
	dkms build -m intel-nuc-led -v 1.0

dkms-deb: dkms-add
	dkms mkdeb intel-nuc-led/1.0 --source-only

dkms-install: dkms-build
	dkms install -m intel-nuc-led -v 1.0
	@depmod -a $(KVERSION)

dkms-rpm: dkms-add
	dkms mkrpm intel-nuc-led/1.0 --source-only

dkms-status:
	dkms status intel-nuc-led/1.0

dkms-uninstall:
	dkms remove -m intel-nuc-led -v 1.0 --all
	rm -rf /usr/src/intel-nuc-led-1.0/

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

rebuild:
	-rmmod nuc_led
	-dkms remove intel-nuc-led/1.0 --all
	${MAKE} dkms-install
	modprobe nuc_led
	cat /proc/acpi/nuc_led
endif
