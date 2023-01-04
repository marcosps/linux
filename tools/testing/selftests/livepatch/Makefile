# SPDX-License-Identifier: GPL-2.0

TEST_FILES := settings \
		test_modules \
		functions.sh

TEST_GEN_FILES := test_programs/test_klp-call_getpid

TEST_PROGS := test-livepatch.sh \
	test-callbacks.sh \
	test-shadow-vars.sh \
	test-state.sh \
	test-ftrace.sh \
	test-sysfs.sh \
	test-syscall.sh

include ../lib.mk

make_modules:
	make -C test_modules all

clean_modules:
	make -C test_modules clean

all: make_modules

clean: clean_modules

.PHONY: all clean make_modules clean_modules
