// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 SUSE LLC
// Author: Marcos Paulo de Souza <mpdesouza@suse.com>

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#if defined(__x86_64__)
#define SYS_PREFIX "__x64_"
#elif defined(__s390__)
#define SYS_PREFIX "__s390x_"
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/livepatch.h>

static asmlinkage long klp_sys_getpid(void)
{
	return task_tgid_vnr(current);
}

static struct klp_func funcs[] = {
	{
		.old_name = SYS_PREFIX "sys_getpid",
		.new_func = klp_sys_getpid,
	}, {}
};

static struct klp_object objs[] = {
	{
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
MODULE_DESCRIPTION("Livepatch kselftest: template module");
