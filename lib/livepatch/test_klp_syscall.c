// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 SUSE LLC
 * Author: Marcos Paulo de Souza <mpdesouza@suse.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/livepatch.h>

#include <linux/sched.h>
static int livepatch_sys_getpid(void)
{
	pr_info("sys_getpid live patched by %s\n", __func__);
	return task_tgid_vnr(current);
}

static struct klp_func funcs[] = {
	{
		.old_name = "__x64_sys_getpid",
		.new_func = livepatch_sys_getpid,
	}, { }
};

static struct klp_object objs[] = {
	{
		/* name being NULL means vmlinux */
		.funcs = funcs,
	}, { }
};

static struct klp_patch patch = {
	.mod = THIS_MODULE,
	.objs = objs,
};

static int test_klp_livepatch_init(void)
{
	return klp_enable_patch(&patch);
}

static void test_klp_livepatch_exit(void)
{
}

module_init(test_klp_livepatch_init);
module_exit(test_klp_livepatch_exit);
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");
MODULE_AUTHOR("Marcos Paulo de Souza <mpdesouza@suse.com>");
MODULE_DESCRIPTION("Livepatch test: syscall module");
