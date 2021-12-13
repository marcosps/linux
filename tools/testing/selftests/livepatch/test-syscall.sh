#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2021 SUSE LLC
# Author: Marcos Paulo de Souza <mpdesouza@suse.com>

. $(dirname $0)/functions.sh

MOD=test_lp_syscall

load_template_module $MOD
disable_lp $MOD
unload_lp $MOD

echo "ok"
exit 0
