obj-m := hello.o

KDIR ?= /lib/modules/$(shell uname -r)/build

unexport sub_make_done

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
