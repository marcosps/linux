// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2022 SUSE
 * Authors: Libor Pechacek <lpechacek@suse.cz>
 *          Nicolai Stange <nstange@suse.de>
 *          Marcos Paulo de Souza <mpdesouza@suse.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/livepatch.h>

#if defined(__x86_64__)
#define FN_PREFIX __x64_
#elif defined(__s390x__)
#define FN_PREFIX __s390x_
#elif defined(__aarch64__)
#define FN_PREFIX __arm64_
#else
/* powerpc does not select ARCH_HAS_SYSCALL_WRAPPER */
#define FN_PREFIX
#endif

struct klp_pid_t {
	pid_t pid;
	struct list_head list;
};
static LIST_HEAD(klp_pid_list);

/* Protects klp_pid_list */
static DEFINE_MUTEX(kpid_mutex);

static int klp_pids[NR_CPUS];
static unsigned int npids;
module_param_array(klp_pids, int, &npids, 0);

static ssize_t npids_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%u\n", npids);
}

static struct kobj_attribute klp_attr = __ATTR_RO(npids);
static struct kobject *klp_kobj;

static void free_klp_pid_list(void)
{
	struct klp_pid_t *kpid, *temp;

	mutex_lock(&kpid_mutex);
	list_for_each_entry_safe(kpid, temp, &klp_pid_list, list) {
		list_del(&kpid->list);
		kfree(kpid);
	}
	mutex_unlock(&kpid_mutex);
}

asmlinkage long lp_sys_getpid(void)
{
	struct klp_pid_t *kpid, *temp;

	/*
	 * For each thread calling getpid, check if the pid exists in
	 * klp_pid_list. If yes, decrement the npids variable and remove the pid
	 * from the list. npids will be later used to ensure that all pids
	 * transitioned to the liveaptched state.
	 */
	mutex_lock(&kpid_mutex);
	list_for_each_entry_safe(kpid, temp, &klp_pid_list, list) {
		if (current->pid == kpid->pid) {
			list_del(&kpid->list);
			kfree(kpid);
			npids--;
			break;
		}
	}
	mutex_unlock(&kpid_mutex);

	return task_tgid_vnr(current);
}

static struct klp_func vmlinux_funcs[] = {
	{
		.old_name = __stringify(FN_PREFIX) "sys_getpid",
		.new_func = lp_sys_getpid,
	}, {}
};

static struct klp_object objs[] = {
	{
		/* name being NULL means vmlinux */
		.funcs = vmlinux_funcs,
	}, {}
};

static struct klp_patch patch = {
	.mod = THIS_MODULE,
	.objs = objs,
};

static int livepatch_init(void)
{
	int ret;
	struct klp_pid_t *kpid;

	if (npids > 0) {
		int i;

		for (i = 0; i < npids; i++) {
			kpid = kmalloc(sizeof(struct klp_pid_t), GFP_KERNEL);
			if (!kpid)
				goto err_mem;

			kpid->pid = klp_pids[i];
			list_add(&kpid->list, &klp_pid_list);
		}
	}

	klp_kobj = kobject_create_and_add("test_klp_syscall", kernel_kobj);
	if (!klp_kobj)
		goto err_mem;

	ret = sysfs_create_file(klp_kobj, &klp_attr.attr);
	if (ret) {
		kobject_put(klp_kobj);
		goto out_klp_pid_list;
	}

	return klp_enable_patch(&patch);

err_mem:
	ret = -ENOMEM;
out_klp_pid_list:
	free_klp_pid_list();

	return ret;
}

static void livepatch_exit(void)
{
	free_klp_pid_list();
	kobject_put(klp_kobj);
}

module_init(livepatch_init);
module_exit(livepatch_exit);
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");
