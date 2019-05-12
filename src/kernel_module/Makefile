obj-m := gpiotiming_mod.o

UNAME_N := $(shell uname -n)
ifeq ($(UNAME_N),raspberrypi)
    ccflags-y += -DON_RASPBERRY_PI
else
    ccflags-y += -DON_ODROID_XU4
endif

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f *~

load: all
	sudo insmod gpiotiming_mod.ko

unload:
	sudo rmmod gpiotiming_mod.ko

