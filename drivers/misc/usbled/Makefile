obj-m	:= usbled.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build 
PWD       := $(shell pwd) 

all: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) 

clean: 
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers

#make -C /lib/modules/`uname -r`/build M=`pwd` modules
