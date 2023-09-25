#include <linux/module.h>
#include <linux/kernel.h>

int hello_init(void)
{
	return 0;
}

void hello_exit(void)
{
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
