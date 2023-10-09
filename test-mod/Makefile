TESTMODS_DIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KDIR ?= $(abs_srctree)

obj-m += mod_test.o

#unexport sub_make_done

all:
	make -C $(KDIR) M=$(TESTMODS_DIR) modules

clean:
	make -C $(KDIR) M=$(TESTMODS_DIR) clean
