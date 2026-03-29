// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/utsname.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick");
MODULE_DESCRIPTION("Hello module for Jetson Orin Nano");

static char *who = "World";
module_param(who, charp, 0644);
MODULE_PARM_DESC(who, "Recipient of the hello message");

static int __init hello_init(void)
{
    pr_info("Hello %s. You are currently using Linux %s.\n",
            who, utsname()->release);
    return 0;
}

static void __exit hello_exit(void)
{
    pr_info("Goodbye, %s!\n", who);
}

module_init(hello_init);
module_exit(hello_exit);
