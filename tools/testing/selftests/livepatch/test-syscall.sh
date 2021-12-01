#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2021 SUSE LLC

. $(dirname $0)/functions.sh

MOD_LIVEPATCH=test_klp_syscall

setup_config

# Test the situation of patching a heavily hammered function. Start processes
# calling the getpid syscall in loop, load the livepatch and verify it could
# patch the sys_getpid syscall function.
#
# - Start a number of processes calling getpid syscall. The number of processes
#   is the number of available CPUs.
#
# - Load the livepatch module that will patch the _sys_getpid function while
#   the userspace processes call it tirelessly. Check if the transition
#   finishes.

start_test "patch highly used syscall"

NR_CPUS=$(getconf _NPROCESSORS_ONLN)
for i in $(seq 1 $NR_CPUS); do
	./lp_test_getpid &
done

load_lp $MOD_LIVEPATCH
# At this point, the livepatch has completed the transition
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH

pkill lp_test_getpid

# Check if dmesg shows the patched function output
dmesg | grep -q 'sys_getpid live patched by livepatch_sys_getpid'
if [ $? -ne 0 ]; then
	echo -e "FAIL\n\n"
	die "livepatch kselftest(s) failed"
fi

echo "ok"
exit 0
