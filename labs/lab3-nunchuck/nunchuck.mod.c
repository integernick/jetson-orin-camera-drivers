#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x25f8bfc1, "module_layout" },
	{ 0xcc921354, "i2c_del_driver" },
	{ 0x7b1cfd9e, "i2c_register_driver" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0x89c9a638, "_dev_err" },
	{ 0xc3055d20, "usleep_range_state" },
	{ 0xdb3395d, "i2c_transfer_buffer_flags" },
	{ 0x85fd2dbe, "_dev_info" },
	{ 0x1fdc7df2, "_mcount" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cnintendo,nunchuk");
MODULE_ALIAS("of:N*T*Cnintendo,nunchukC*");
