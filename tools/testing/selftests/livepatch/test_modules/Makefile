TESTMODS_DIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m += test_klp_atomic_replace.o \
	test_klp_callbacks_busy.o \
	test_klp_callbacks_demo.o \
	test_klp_callbacks_demo2.o \
	test_klp_callbacks_mod.o \
	test_klp_livepatch.o \
	test_klp_state.o \
	test_klp_state2.o \
	test_klp_state3.o \
	test_klp_shadow_vars.o

# Needed since the sub-Makefile will need to parse the M= variable again
unexport sub_make_done

modules:
	$(Q)$(MAKE) -C $(KDIR) M=$(TESTMODS_DIR) modules

clean:
	$(Q)$(MAKE) -C $(KDIR) M=$(TESTMODS_DIR) clean
