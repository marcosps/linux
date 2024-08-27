// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2024 Lukas Hruska <lhruska@suse.cz>

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>

const char *hello_msg = "Hello from";

static int hello_get(char *buffer, const struct kernel_param *kp)
{
	return sysfs_emit(buffer, "%s kernel module.\n", hello_msg);
}

#ifdef CONFIG_X86_KERNEL_IBT
static __attribute__((nocf_check)) int hello_get_alt(char *buffer, const struct kernel_param *kp)
{
	return sysfs_emit(buffer, "%s unused function.\n", hello_msg);
}

static int fail_get(char *buffer, const struct kernel_param *kp)
{
	int __attribute__((nocf_check)) (* volatile klpe_hello_get_alt)(char *, const struct kernel_param *) = hello_get_alt;
	return (*klpe_hello_get_alt)(buffer, kp);
}

static const struct kernel_param_ops fail_ops = {
	.get	= fail_get,
};

module_param_cb(fail, &fail_ops, NULL, 0400);
MODULE_PARM_DESC(fail, "Read only parameter failing the reader.");
#endif

static const struct kernel_param_ops hello_ops = {
	.get	= hello_get,
};

module_param_cb(hello, &hello_ops, NULL, 0400);
MODULE_PARM_DESC(hello, "Read only parameter greeting the reader.");

static int test_klp_extern_hello_init(void)
{
	return 0;
}

static void test_klp_extern_hello_exit(void)
{
}

module_init(test_klp_extern_hello_init);
module_exit(test_klp_extern_hello_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lukas Hruska <lhruska@suse.cz>");
MODULE_DESCRIPTION("Livepatch test: external symbol relocation - test module");
