obj-m := altra-hwmon.o
#altra-hwmon-y := altra-hwmon.o

ccflags-y := -Wno-error=incompatible-pointer-types

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(shell pwd) modules

modules_clean:
	$(MAKE) -C $(KDIR) M=$(shell pwd) clean

modules_install:
	$(MAKE) -C $(KDIR) M=$(shell pwd) modules_install

modules_load:
	modprobe altra-hwmon

modules_unload:
	modprobe -r altra-hwmon
