#include <linux/backlight.h>

#include <linux/gpio/consumer.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>

#include <video/mipi_display.h>

#include <linux/version.h>
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
#else
#include <drm/drmP.h>
#endif
#if KERNEL_VERSION(5, 1, 0) <= LINUX_VERSION_CODE
#include <drm/drm_probe_helper.h>
#endif

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/videomode.h>

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>

#include <linux/gpio.h>

#define DEFAULT_GPIO_RESET_PIN 13
#define DEFAULT_GPIO_BACKLIGHT_PIN 28


struct HG_LTP08_touchscreen
{
    struct drm_panel base;
    struct mipi_dsi_device *dsi;

    int resetPin;
    int gpioResetD;

    int backlightPin;
    int gpioBacklightD;

    bool prepared;
    bool enabled;
};


static const struct drm_display_mode default_mode =
{
    .hdisplay	= 800,
    .vdisplay	= 1280,

    .vrefresh   = 50,

    .clock = 56535,

#define FRONT_PORCH 18
#define SYNC_LEN 18
#define BACK_PORCH 18

    .hsync_start= 800 + FRONT_PORCH,
    .hsync_end	= 800 + FRONT_PORCH + SYNC_LEN,
    .htotal		= 800 + FRONT_PORCH + SYNC_LEN + BACK_PORCH,

#define VFRONT_PORCH 30
#define VSYNC_LEN 4
#define VBACK_PORCH 10

    .vsync_start= 1280 + VFRONT_PORCH,
    .vsync_end	= 1280 + VFRONT_PORCH + VSYNC_LEN,
    .vtotal		= 1280 + VFRONT_PORCH + VSYNC_LEN + VBACK_PORCH,

    .width_mm = 170,
    .height_mm = 106,

    .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
};


static struct HG_LTP08_touchscreen *panel_to_ts(struct drm_panel *panel)
{
    return container_of(panel, struct HG_LTP08_touchscreen, base);
}

struct panel_command
{
    struct cmd
    {
        u8	cmd;
        u8	data;
    } cmd;
    u8 delay;
};

#define SWITCH_PAGE_CMD(_page)  \
	{					        \
		.cmd= {			        \
            .cmd = 0xFF,        \
			.data = (_page),	\
		},				        \
		.delay = 0,             \
	}


#define CMD_DELAY(_cmd, _data, _delay)\
	{						    \
        .cmd = {    			\
            .cmd = (_cmd),      \
            .data = (_data),    \
        },  				    \
		.delay = (_delay),      \
	}

#define COMMAND_CMD(_cmd, _data) CMD_DELAY(_cmd, _data, 0)


static const struct panel_command panel_cmds_init[] =
{
    SWITCH_PAGE_CMD(0x03),

    COMMAND_CMD(0x01, 0x00),
    COMMAND_CMD(0x02, 0x00),
    COMMAND_CMD(0x03, 0x53),
    COMMAND_CMD(0x04, 0x53),
    COMMAND_CMD(0x05, 0x13),
    COMMAND_CMD(0x06, 0x4),
    COMMAND_CMD(0x07, 0x2),
    COMMAND_CMD(0x08, 0x2),
    COMMAND_CMD(0x09, 0x0),
    COMMAND_CMD(0x0A, 0x0),
    COMMAND_CMD(0x0B, 0x0),
    COMMAND_CMD(0x0C, 0x0),
    COMMAND_CMD(0x0D, 0x0),
    COMMAND_CMD(0x0E, 0x0),
    COMMAND_CMD(0x0F, 0x0),
    COMMAND_CMD(0x10, 0x0),
    COMMAND_CMD(0x11, 0x0),
    COMMAND_CMD(0x12, 0x0),
    COMMAND_CMD(0x13, 0x0),
    COMMAND_CMD(0x14, 0x0),
    COMMAND_CMD(0x15, 0x0),
    COMMAND_CMD(0x16, 0x0),
    COMMAND_CMD(0x17, 0x0),
    COMMAND_CMD(0x18, 0x0),
    COMMAND_CMD(0x19, 0x0),
    COMMAND_CMD(0x1A, 0x0),
    COMMAND_CMD(0x1B, 0x0),
    COMMAND_CMD(0x1C, 0x0),
    COMMAND_CMD(0x1D, 0x0),
    COMMAND_CMD(0x1E, 0xC0),
    COMMAND_CMD(0x1F, 0x80),
    COMMAND_CMD(0x20, 0x02),
    COMMAND_CMD(0x21, 0x09),
    COMMAND_CMD(0x22, 0x00),
    COMMAND_CMD(0x23, 0x00),
    COMMAND_CMD(0x24, 0x00),
    COMMAND_CMD(0x25, 0x00),
    COMMAND_CMD(0x26, 0x00),
    COMMAND_CMD(0x27, 0x00),
    COMMAND_CMD(0x28, 0x55),
    COMMAND_CMD(0x29, 0x03),
    COMMAND_CMD(0x2A, 0x00),
    COMMAND_CMD(0x2B, 0x00),
    COMMAND_CMD(0x2C, 0x00),
    COMMAND_CMD(0x2D, 0x00),
    COMMAND_CMD(0x2E, 0x00),
    COMMAND_CMD(0x2F, 0x00),
    COMMAND_CMD(0x30, 0x00),
    COMMAND_CMD(0x31, 0x00),
    COMMAND_CMD(0x32, 0x00),
    COMMAND_CMD(0x33, 0x00),
    COMMAND_CMD(0x34, 0x00),
    COMMAND_CMD(0x35, 0x00),
    COMMAND_CMD(0x36, 0x00),
    COMMAND_CMD(0x37, 0x00),
    COMMAND_CMD(0x38, 0x3C),
    COMMAND_CMD(0x39, 0x00),
    COMMAND_CMD(0x3A, 0x00),
    COMMAND_CMD(0x3B, 0x00),
    COMMAND_CMD(0x3C, 0x00),
    COMMAND_CMD(0x3D, 0x00),
    COMMAND_CMD(0x3E, 0x00),
    COMMAND_CMD(0x3F, 0x00),
    COMMAND_CMD(0x40, 0x00),
    COMMAND_CMD(0x41, 0x00),
    COMMAND_CMD(0x42, 0x00),
    COMMAND_CMD(0x43, 0x00),
    COMMAND_CMD(0x44, 0x00),
    COMMAND_CMD(0x50, 0x01),
    COMMAND_CMD(0x51, 0x23),
    COMMAND_CMD(0x52, 0x45),
    COMMAND_CMD(0x53, 0x67),
    COMMAND_CMD(0x54, 0x89),
    COMMAND_CMD(0x55, 0xAB),
    COMMAND_CMD(0x56, 0x01),
    COMMAND_CMD(0x57, 0x23),
    COMMAND_CMD(0x58, 0x45),
    COMMAND_CMD(0x59, 0x67),
    COMMAND_CMD(0x5A, 0x89),
    COMMAND_CMD(0x5B, 0xAB),
    COMMAND_CMD(0x5C, 0xCD),
    COMMAND_CMD(0x5D, 0xEF),
    COMMAND_CMD(0x5E, 0x01),
    COMMAND_CMD(0x5F, 0x08),
    COMMAND_CMD(0x60, 0x02),
    COMMAND_CMD(0x61, 0x02),
    COMMAND_CMD(0x62, 0x0A),
    COMMAND_CMD(0x63, 0x15),
    COMMAND_CMD(0x64, 0x14),
    COMMAND_CMD(0x65, 0x02),
    COMMAND_CMD(0x66, 0x11),
    COMMAND_CMD(0x67, 0x10),
    COMMAND_CMD(0x68, 0x02),
    COMMAND_CMD(0x69, 0x0F),
    COMMAND_CMD(0x6A, 0x0E),
    COMMAND_CMD(0x6B, 0x02),
    COMMAND_CMD(0x6C, 0x0D),
    COMMAND_CMD(0x6D, 0x0C),
    COMMAND_CMD(0x6E, 0x06),
    COMMAND_CMD(0x6F, 0x02),
    COMMAND_CMD(0x70, 0x02),
    COMMAND_CMD(0x71, 0x02),
    COMMAND_CMD(0x72, 0x02),
    COMMAND_CMD(0x73, 0x02),
    COMMAND_CMD(0x74, 0x02),
    COMMAND_CMD(0x75, 0x06),
    COMMAND_CMD(0x76, 0x02),
    COMMAND_CMD(0x77, 0x02),
    COMMAND_CMD(0x78, 0x0A),
    COMMAND_CMD(0x79, 0x15),
    COMMAND_CMD(0x7A, 0x14),
    COMMAND_CMD(0x7B, 0x02),
    COMMAND_CMD(0x7C, 0x10),
    COMMAND_CMD(0x7D, 0x11),
    COMMAND_CMD(0x7E, 0x02),
    COMMAND_CMD(0x7F, 0x0C),
    COMMAND_CMD(0x80, 0x0D),
    COMMAND_CMD(0x81, 0x02),
    COMMAND_CMD(0x82, 0x0E),
    COMMAND_CMD(0x83, 0x0F),
    COMMAND_CMD(0x84, 0x08),
    COMMAND_CMD(0x85, 0x02),
    COMMAND_CMD(0x86, 0x02),
    COMMAND_CMD(0x87, 0x02),
    COMMAND_CMD(0x88, 0x02),
    COMMAND_CMD(0x89, 0x02),
    COMMAND_CMD(0x8A, 0x02),

    SWITCH_PAGE_CMD(0x04),

    COMMAND_CMD(0x6C, 0x15),
    COMMAND_CMD(0x6E, 0x30),
    COMMAND_CMD(0x6F, 0x33),
    COMMAND_CMD(0x8D, 0x87),
    COMMAND_CMD(0x87, 0xBA),
    COMMAND_CMD(0x26, 0x76),
    COMMAND_CMD(0xB2, 0xD1),
    COMMAND_CMD(0x35, 0x1F),
    COMMAND_CMD(0x33, 0x14),
    COMMAND_CMD(0x3A, 0xA9),
    COMMAND_CMD(0x38, 0x01),
    COMMAND_CMD(0x39, 0x00),

    SWITCH_PAGE_CMD(0x01),

    COMMAND_CMD(0x22, 0x0A),
    COMMAND_CMD(0x31, 0x00),
    COMMAND_CMD(0x50, 0xC0),
    COMMAND_CMD(0x51, 0xC0),
    COMMAND_CMD(0x53, 0x43),
    COMMAND_CMD(0x55, 0x7A),
    COMMAND_CMD(0x60, 0x28),
    COMMAND_CMD(0x2E, 0xC8),
    COMMAND_CMD(0xA0, 0x01),
    COMMAND_CMD(0xA1, 0x11),
    COMMAND_CMD(0xA2, 0x1C),
    COMMAND_CMD(0xA3, 0x0E),
    COMMAND_CMD(0xA4, 0x15),
    COMMAND_CMD(0xA5, 0x28),
    COMMAND_CMD(0xA6, 0x1C),
    COMMAND_CMD(0xA7, 0x1E),
    COMMAND_CMD(0xA8, 0x73),
    COMMAND_CMD(0xA9, 0x1C),
    COMMAND_CMD(0xAA, 0x26),
    COMMAND_CMD(0xAB, 0x63),
    COMMAND_CMD(0xAC, 0x18),
    COMMAND_CMD(0xAD, 0x16),
    COMMAND_CMD(0xAE, 0x4D),
    COMMAND_CMD(0xAF, 0x1F),
    COMMAND_CMD(0xB0, 0x2A),
    COMMAND_CMD(0xB1, 0x4F),
    COMMAND_CMD(0xB2, 0x5F),
    COMMAND_CMD(0xB3, 0x39),
    COMMAND_CMD(0xC0, 0x01),
    COMMAND_CMD(0xC1, 0x11),
    COMMAND_CMD(0xC2, 0x1C),
    COMMAND_CMD(0xC3, 0x0E),
    COMMAND_CMD(0xC4, 0x15),
    COMMAND_CMD(0xC5, 0x28),
    COMMAND_CMD(0xC6, 0x1C),
    COMMAND_CMD(0xC7, 0x1E),
    COMMAND_CMD(0xC8, 0x73),
    COMMAND_CMD(0xC9, 0x1C),
    COMMAND_CMD(0xCA, 0x26),
    COMMAND_CMD(0xCB, 0x63),
    COMMAND_CMD(0xCC, 0x18),
    COMMAND_CMD(0xCD, 0x16),
    COMMAND_CMD(0xCE, 0x4D),
    COMMAND_CMD(0xCF, 0x1F),
    COMMAND_CMD(0xD0, 0x2A),
    COMMAND_CMD(0xD1, 0x4F),
    COMMAND_CMD(0xD2, 0x5F),
    COMMAND_CMD(0xD3, 0x39),

    SWITCH_PAGE_CMD(0x00),

    COMMAND_CMD(0x55, 0x00),
    COMMAND_CMD(0x35, 0x00),
    CMD_DELAY(0x11, 0x00, 100),
    CMD_DELAY(0x29, 0x00, 100),
};


static int send_cmd_data(struct HG_LTP08_touchscreen *ctx, u8 cmd, u8 data)
{
    u8 buf[2] = { cmd, data };
    int ret;

    ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
    if (ret < 0)
    {
        DRM_ERROR_RATELIMITED("MIPI DSI DCS write failed: %d\n", ret);

        return ret;
    }

    return 0;
}


static int switch_page(struct HG_LTP08_touchscreen *ctx, u8 page)
{
    u8 buf[4] = { 0xFF, 0x98, 0x81, page };
    int ret;

    ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
    if (ret < 0)
    {
        DRM_ERROR_RATELIMITED("MIPI DSI DCS write failed for switching page: %d\n", ret);

        return ret;
    }

    return 0;
}

static int HG_LTP08_init_sequence(struct HG_LTP08_touchscreen *ctx)
{
    int i, ret;

    ret = 0;

    for (i = 0; i < ARRAY_SIZE(panel_cmds_init); ++i)
    {
        const struct panel_command *cmd = &panel_cmds_init[i];

        if (cmd->cmd.cmd == 0xFF)
            ret = switch_page(ctx, cmd->cmd.data);
        else
            ret = send_cmd_data(ctx, cmd->cmd.cmd, cmd->cmd.data);

        if (cmd->delay)
            msleep(cmd->delay);
    }

    return 0;
}


static int HG_LTP08_prepare(struct drm_panel *panel)
{
    struct HG_LTP08_touchscreen *ctx = panel_to_ts(panel);
    struct mipi_dsi_device *dsi = ctx->dsi;
    int ret;
    bool slow_mode;

    if (ctx->prepared)
        return 0;

    printk(KERN_ALERT "Preparing!\n");
    if (!dsi)
        printk(KERN_ALERT "No DSI device!\n");


    ctx->gpioBacklightD = gpio_request(ctx->backlightPin, "gpioBacklightD");
    if (ctx->gpioBacklightD < 0)
    {
        ctx->gpioBacklightD = 0;
        printk(KERN_ALERT "Couldn't grab the gpio BacklightD pin\n");
    }
    else
        ctx->gpioBacklightD = 1;

    if (ctx->gpioBacklightD)
        gpio_direction_output(ctx->backlightPin, 1);


    ctx->gpioResetD = gpio_request(ctx->resetPin, "gpioResetD");
    if (ctx->gpioResetD < 0)
    {
        ctx->gpioResetD = 0;
        printk(KERN_ALERT "Couldn't grab the gpio ResetD pin\n");
    }
    else
        ctx->gpioResetD = 1;

    if (ctx->gpioResetD)
        gpio_direction_output(ctx->resetPin, 1);

    msleep(125);

    if (ctx->gpioResetD)
        gpio_set_value_cansleep(ctx->resetPin, 0);
    msleep(20);
    if (ctx->gpioResetD)
        gpio_set_value_cansleep(ctx->resetPin, 1);

    msleep(125);

    slow_mode = dsi->mode_flags & MIPI_DSI_MODE_LPM;

    if (!slow_mode)
        dsi->mode_flags |= MIPI_DSI_MODE_LPM;

    ret = HG_LTP08_init_sequence(ctx);
    if (ret)
        return ret;

    ret = switch_page(ctx, 0);
    if (ret)
        return ret;

    ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
    if (ret < 0)
    {
        printk(KERN_ALERT "Couldn't set tear on!\n");

        return ret;
    }

    if (!slow_mode)
        dsi->mode_flags &= ~(MIPI_DSI_MODE_LPM);

    msleep(125);

    ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
    if (ret)
    {
        printk(KERN_ALERT "Couldn't exit sleep mode!\n");

        return ret;
    }

    msleep(125);

    ret = mipi_dsi_dcs_set_display_on(dsi);
    if (ret)
    {
        printk(KERN_ALERT "Couldn't set display on!\n");
        return ret;
    }

    msleep(20);

    ctx->prepared = true;

    printk(KERN_ALERT "Prepared!\n");

    return 0;
}


static int HG_LTP08_unprepare(struct drm_panel *panel)
{
    struct HG_LTP08_touchscreen *ctx = panel_to_ts(panel);
    struct mipi_dsi_device *dsi = ctx->dsi;
    int ret;

    if (!ctx->prepared)
        return 0;

    printk(KERN_ALERT "Unprepare!\n");

    ret = mipi_dsi_dcs_set_display_off(dsi);
    if (ret)
        printk(KERN_WARNING "failed to set display off: %d\n", ret);

    ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
    if (ret)
        printk(KERN_WARNING "failed to enter sleep mode: %d\n", ret);

    msleep(120);

    if (ctx->gpioBacklightD)
        gpio_set_value_cansleep(ctx->backlightPin, 0);

    if (ctx->gpioResetD)
    {
        gpio_free(ctx->resetPin);
        ctx->gpioResetD = 0;
    }

    if (ctx->gpioBacklightD)
    {
        gpio_free(ctx->backlightPin);
        ctx->gpioBacklightD = 0;
    }

    ctx->prepared = false;

    return 0;
}


static int HG_LTP08_enable(struct drm_panel *panel)
{
    struct HG_LTP08_touchscreen *ctx = panel_to_ts(panel);
    int ret;

    if (ctx->enabled)
        return 0;

    printk(KERN_ALERT "Enabling!\n");

    drm_panel_prepare(panel);

    ret = mipi_dsi_dcs_set_tear_on(ctx->dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
    if (ret < 0)
    {
        printk(KERN_ALERT "Couldn't set tear on!\n");
        return ret;
    }

    mipi_dsi_dcs_set_display_on(ctx->dsi);

    if (ctx->gpioBacklightD)
        gpio_set_value_cansleep(ctx->backlightPin, 1);

    ctx->enabled = true;

    printk(KERN_ALERT "Enabled!\n");

    return 0;
}

static int HG_LTP08_disable(struct drm_panel *panel)
{
    struct HG_LTP08_touchscreen *ctx = panel_to_ts(panel);

    if (!ctx->enabled)
        return 0;

    mipi_dsi_dcs_set_display_off(ctx->dsi);

    if (ctx->gpioBacklightD)
        gpio_set_value_cansleep(ctx->backlightPin, 0);

    ctx->enabled = false;

    printk(KERN_ALERT "Disabled!\n");

    return 0;
}


static int HG_LTP08_get_modes(struct drm_panel *panel)
{
    static const u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
    struct drm_display_mode *mode;

    mode = drm_mode_duplicate(panel->drm, &default_mode);
    if (!mode)
    {
        DRM_ERROR("failed to add mode %ux%ux@%u\n",
                  default_mode.hdisplay, default_mode.vdisplay,
                  default_mode.vrefresh);
        return -ENOMEM;
    }

    drm_mode_set_name(mode);

    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

    drm_mode_probed_add(panel->connector, mode);

    panel->connector->display_info.width_mm = mode->width_mm;
    panel->connector->display_info.height_mm = mode->height_mm;

    panel->connector->display_info.bpc = 8;

    drm_display_info_set_bus_formats(&panel->connector->display_info, &bus_format, 1);

    return 1;
}

static const struct drm_panel_funcs HG_LTP08_drm_funcs =
{
    .disable = HG_LTP08_disable,
    .unprepare = HG_LTP08_unprepare,
    .prepare = HG_LTP08_prepare,
    .enable = HG_LTP08_enable,
    .get_modes = HG_LTP08_get_modes,
};

static int HG_LTP08_probe(struct mipi_dsi_device *dsi)
{
    int ret;
    const __be32 *prop;

    struct device *dev = &dsi->dev;
    struct HG_LTP08_touchscreen *ctx;

    printk(KERN_ALERT "Probing!\n");

    ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;
    memset(ctx, 0, sizeof(*ctx));

    ctx->backlightPin = DEFAULT_GPIO_BACKLIGHT_PIN;

    prop = of_get_property(dsi->dev.of_node, "backlight", NULL);
    if (prop)
    {
        ctx->backlightPin = be32_to_cpup(prop);
        printk(KERN_ALERT "Backlight pin set to %d\n", ctx->backlightPin);
    }
    else
        printk(KERN_ALERT "Backlight pin not set, using default\n");


    ctx->resetPin = DEFAULT_GPIO_RESET_PIN;

    prop = of_get_property(dsi->dev.of_node, "reset", NULL);
    if (prop)
    {
        ctx->resetPin = be32_to_cpup(prop);
        printk(KERN_ALERT "Reset pin set to %d\n", ctx->resetPin);
    }
    else
        printk(KERN_ALERT "Reset pin not set, using default\n");

    mipi_dsi_set_drvdata(dsi, ctx);
    ctx->dsi = dsi;

    dsi->lanes = 4;
    dsi->format = MIPI_DSI_FMT_RGB888;

    dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE /* | MIPI_DSI_MODE_LPM*/;

    printk(KERN_ALERT "DSI Device init for %s!\n", dsi->name);

    drm_panel_init(&ctx->base);

    ctx->base.dev = dev;
    ctx->base.funcs = &HG_LTP08_drm_funcs;

    ret = drm_panel_add(&ctx->base);
    if (ret < 0)
    {
        dev_err(dev, "drm_panel_add() failed: %d\n", ret);

        kfree(ctx);

        return ret;
    }

    ret = mipi_dsi_attach(dsi);
    if (ret < 0)
    {
        dev_err(dev, "mipi_dsi_attach() failed: %d\n", ret);

        drm_panel_remove(&ctx->base);
        kfree(ctx);
        return ret;
    }

    printk(KERN_ALERT "Probed!\n");

    return 0;
}

static int HG_LTP08_remove(struct mipi_dsi_device *dsi)
{
    struct HG_LTP08_touchscreen *ctx = mipi_dsi_get_drvdata(dsi);

    mipi_dsi_detach(dsi);
    drm_panel_remove(&ctx->base);

    kfree(ctx);

    printk(KERN_ALERT "Removed!\n");

    return 0;
}

static const struct of_device_id HG_LTP08_touchscreen_of_match[] =
{
    { .compatible = "HG_LTP08" },
    { }
};
MODULE_DEVICE_TABLE(of, HG_LTP08_touchscreen_of_match);



static struct mipi_dsi_driver panel_HG_LTP08_dsi_driver =
{
    .driver = {
        .name = "panel_HG_LTP08",
        .of_match_table = HG_LTP08_touchscreen_of_match,
    },
    .probe = HG_LTP08_probe,
    .remove = HG_LTP08_remove,
};
module_mipi_dsi_driver(panel_HG_LTP08_dsi_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Homegear GmbH <contact@homegear.email>");
MODULE_DESCRIPTION("Homegear LTP08 Multitouch 8\" Display; black; WXGA 1280x800; Linux");
MODULE_VERSION("1.0");
