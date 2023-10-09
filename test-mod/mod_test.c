#include <linux/module.h>
#include <linux/kernel.h>

static int mod_init(void)
{
	return 0;
}

static void mod_exit(void)
{
}

module_init(mod_init);
module_exit(mod_exit);
MODULE_LICENSE("GPL");
