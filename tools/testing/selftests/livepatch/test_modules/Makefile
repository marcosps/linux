LP_TESTMODS_DIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
#KDIR ?= /lib/modules/$(shell uname -r)/build
KDIR ?= $(abspath $(LP_TESTMODS_DIR)/../../../../..)

obj-m += mod_test.o

all:
	make -C $(KDIR) M=$(LP_TESTMODS_DIR) modules

clean:
	make -C $(KDIR) M=$(LP_TESTMODS_DIR) clean
