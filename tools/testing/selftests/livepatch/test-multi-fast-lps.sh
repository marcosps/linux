#!/bin/bash

. $(dirname $0)/functions.sh

MOD_LIVEPATCH=test_klp_syscallXX
MOD_DIR=/lib/modules/$(uname -r)/kernel/lib/livepatch

setup_config

# Load 15 livepatches all addressing the same functions, check if the transition
# happened before loading the next one.
#
# - Start loading livepatches from 1 to 15, all the same, one after another. As
#   soon as the transision finishes, the next one it loaded.
#
# - Unload the livepatches in the same order they were defined.

start_test "load multiple livepatches one after another"

load_mods() {
	for i in $(seq -w 15); do
		NEW_FMOD=$MOD_DIR/$MOD_LIVEPATCH.ko

		# Change klp_test_syscallXX in .modinfo and in .gnu.linkonce.this_module
		# The first one is used to check if the module wasn't being
		# loaded already, while the second one is used to list the
		# module in procmodules
		sed -i "s/test_klp_syscall../test_klp_syscall$i/g" $NEW_FMOD
		load_lp $MOD_LIVEPATCH
	done
}

local_unload_mod() {
	disable_lp "$1"
	unload_lp "$1"
}

load_mods

# At this point, all livepatches were applied and transicioned correctly
# Now unload them all
for i in $(seq -w 15); do
	local_unload_mod "test_klp_syscall$i"
done

# Save test as before, but now unloading the livepatches in the inverse order
load_mods

for i in $(seq -w 15 -1 1); do
	local_unload_mod "test_klp_syscall$i"
done

echo "ok"
exit 0
