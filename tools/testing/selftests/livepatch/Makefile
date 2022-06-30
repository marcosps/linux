# SPDX-License-Identifier: GPL-2.0

TEST_FILES := settings \
		test_modules

# We need the test_modules dir in order to make gen_tar and install to work
TEST_GEN_PROGS_EXTENDED := test_modules/test_klp_atomic_replace.ko \
			test_modules/test_klp_callbacks_busy.ko \
			test_modules/test_klp_callbacks_demo.ko \
			test_modules/test_klp_callbacks_demo2.ko \
			test_modules/test_klp_callbacks_mod.ko \
			test_modules/test_klp_livepatch.ko \
			test_modules/test_klp_state.ko \
			test_modules/test_klp_state2.ko \
			test_modules/test_klp_state3.ko \
			test_modules/test_klp_shadow_vars.ko

TEST_PROGS_EXTENDED := functions.sh
TEST_PROGS := \
	test-livepatch.sh \
	test-callbacks.sh \
	test-shadow-vars.sh \
	test-state.sh \
	test-ftrace.sh \
	test-sysfs.sh

# override lib.mk's default rules
OVERRIDE_TARGETS := 1
override define CLEAN
	rm -f $(TEST_GEN_PROGS_EXTENDED)
	make -C test_modules clean
endef

include ../lib.mk

# Here we remove the test_modules path from the module.ko to match the Makefile
# rule in test_modules directory
%.ko:
	make -C test_modules $(notdir $@)
