// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author: Marcos Paulo de Souza <marcos@mpdesouza.com>
 *
 * Simple console implementation for experimenting legacy and NBCON variants
 * (TODO).
 *
 * For testing, load the sconsole module, and read from file /dev/sconsole after
 * generating information o /dev/kmsg (dmesg). Take a look into your current
 * loglevel otherwise you might end up not recording the information.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/console.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>

struct scon_msg {
	struct list_head list;
	size_t len;
	char msg[] __counted_by(len);
};

/* List of all collected messages from printk */
static LIST_HEAD(msgs);

static ssize_t scon_read(struct file *f, char __user *buf, size_t count,
		loff_t *fpos)
{
	struct scon_msg *smsg;

	if (list_empty(&msgs))
		return 0;

	/* Get the first scon_msg from the list */
	smsg = list_first_entry(&msgs, struct scon_msg, list);

	/* only read the first message */
	if (count > smsg->len)
		count = smsg->len;

	if (copy_to_user(buf, smsg->msg, count))
		return -ERESTARTSYS;

	/* Only remove the record if it was copied to userspace correctly */
	list_del(&smsg->list);
	kfree(smsg);

	return count;
}

static void write_msg(struct console *con, const char *msg, unsigned int len)
{
	/*
	 * We need to allocate memory enough for the message, and use
	 * GFP_ATOMIC since we can't sleep when begin called by the console
	 * write callpath.
	 */
	struct scon_msg *smsg = kzalloc(sizeof(struct scon_msg) + sizeof(char) + len + 1,
			GFP_ATOMIC);

	if (!smsg) {
		pr_err("failed to allocate message!\n");
		return;
	}

	smsg->len = len + 1;
	strscpy(smsg->msg, msg, len);
	/*
	 * FIXME: To make cat command to print messages in different lines... maybe
	 * unnecessary?
	 */
	smsg->msg[len - 1] = '\n';
	/* Always add new messages to the end of the messages list */
	list_add_tail(&smsg->list, &msgs);
}

static struct file_operations scon_fops = {
	.read = scon_read,
};

static struct miscdevice scon_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sconsole",
	.mode = S_IRUGO,
	.fops = &scon_fops,
};

static struct console scon = {
	.name = "simple_console",
	/* Print all messages since boot */
	.flags = CON_PRINTBUFFER,
	.write = write_msg,
};

static int sconsole_init(void)
{
	int ret;

	/*
	 * Register the miscdevice that will be responsible for reading the
	 * console messages.
	 */
	ret = misc_register(&scon_misc);
	if (ret)
		return ret;

	/* Registers simple console so kernel can start writing messages to it. */
	register_console_force(&scon);

	return 0;
}

static void sconsole_exit(void)
{
	struct scon_msg *cur, *tmp;

	/* Stop receiving new messages */
	unregister_console(&scon);
	/*
	 * Stop receiving new userspace requests to read and delete entries from
	 * msgs list.
	 */
	misc_deregister(&scon_misc);

	/* Remove all memory allocated to the entries not read */
	list_for_each_entry_safe(cur, tmp, &msgs, list) {
		list_del(&cur->list);
		kfree(cur);
	}
}

module_init(sconsole_init);
module_exit(sconsole_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marcos Paulo de Souza <marcos@mpdesouza.com>");
MODULE_DESCRIPTION("Simple console module");
