#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
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
__used
__attribute__((section("__versions"))) = {
	{ 0x3f584fcd, "module_layout" },
	{ 0xf5ea5d85, "mipi_dsi_driver_unregister" },
	{ 0x3a5b6826, "mipi_dsi_driver_register_full" },
	{ 0xfe990052, "gpio_free" },
	{ 0x9d245e4b, "mipi_dsi_dcs_enter_sleep_mode" },
	{ 0x1b602baa, "mipi_dsi_dcs_set_display_off" },
	{ 0x1dd71f7a, "drm_err" },
	{ 0x619204bd, "drm_mode_probed_add" },
	{ 0xf8dfc84, "drm_mode_set_name" },
	{ 0x7c44f433, "drm_mode_duplicate" },
	{ 0xc3a759c4, "kmalloc_caches" },
	{ 0xc51c7334, "_dev_err" },
	{ 0x449b332a, "mipi_dsi_attach" },
	{ 0x8613e271, "drm_panel_add" },
	{ 0x8104473d, "drm_panel_init" },
	{ 0xd68dfe81, "of_get_property" },
	{ 0x5f754e5a, "memset" },
	{ 0xbb83af41, "kmem_cache_alloc_trace" },
	{ 0xb583047d, "mipi_dsi_dcs_set_display_on" },
	{ 0xf4ce2362, "mipi_dsi_dcs_exit_sleep_mode" },
	{ 0x5688350f, "gpiod_set_raw_value_cansleep" },
	{ 0x6cbbece, "mipi_dsi_dcs_set_tear_on" },
	{ 0xf9a482f9, "msleep" },
	{ 0x127f5033, "gpiod_direction_output_raw" },
	{ 0x2a941df5, "gpio_to_desc" },
	{ 0x47229b5c, "gpio_request" },
	{ 0xf87a2279, "drm_dev_printk" },
	{ 0xee43fd9b, "___ratelimit" },
	{ 0x51d22187, "mipi_dsi_dcs_write_buffer" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0x7c32d0f0, "printk" },
	{ 0x37a0cba, "kfree" },
	{ 0xdab768bd, "drm_panel_remove" },
	{ 0x118ad98d, "mipi_dsi_detach" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=drm";

MODULE_ALIAS("of:N*T*Cdivus,whatever");
MODULE_ALIAS("of:N*T*Cdivus,whateverC*");

MODULE_INFO(srcversion, "B3ED8533DB2302AC942BD67");
