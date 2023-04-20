LP_TESTMODS_DIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m += mod_test.o

all:
	$(info Makefile at $(shell pwd))
	$(info srctree $(srctree), abs_srctree $(abs_srctree), abs_objtree $(abs_objtree))
	make -C $(KDIR) M=$(LP_TESTMODS_DIR) modules

clean:
	make -C $(KDIR) M=$(LP_TESTMODS_DIR) clean
