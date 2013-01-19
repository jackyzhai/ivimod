# Makefile
# For IVI Kernel Module
# IVI Project, CERNET
# create:                                2008.11.05  Hong Zhang


#EXTRA_CFLAGS += $(DEBFLAGS)
#EXTRA_CFLAGS += -I..

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	+= ivi.o
ivi-objs:= ividev.o ivi_mapping.o ivi_trans.o

obj-m	+= ivi_portmapping.o
obj-m	+= ivi_stateful.o
obj-m	+= ivi_partialstate.o
else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

utils:
	cd utils && $(MAKE)
.PHONY: utils
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.markers Module.symvers modules.order cscope* .ivi*

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif

