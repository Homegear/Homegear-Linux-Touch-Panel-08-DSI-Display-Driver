#include <linux/version.h>
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
#else
#include <drm/drmP.h>
#endif
#if KERNEL_VERSION(5, 1, 0) <= LINUX_VERSION_CODE
#include <drm/drm_probe_helper.h>
#endif

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#include <linux/gpio.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
#define HAVE_PROC_OPS
#endif

#define DEFAULT_GPIO_RESET_PIN 13
#define DEFAULT_GPIO_BACKLIGHT_PIN 28

#define CMD_RETRIES 3
#define RETRY_DELAY 50


static atomic_t errorFlag = ATOMIC_INIT(0);
struct proc_dir_entry *procFile;


struct hgltp08_touchscreen
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

    .clock = 56535,

    #if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
    #else
    .vrefresh   = 50,
    #endif // KERNEL_VERSION


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


static struct hgltp08_touchscreen *panel_to_ts(struct drm_panel *panel)
{
    return container_of(panel, struct hgltp08_touchscreen, base);
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


static ssize_t procfile_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
  int ef;
  char buf[1];

  if (ubuf == NULL || count <= 0)
    return 0;

  if (ppos == NULL || *ppos > 0)
    return 0;//return -EINVAL;

  ef = atomic_read(&errorFlag);
  buf[0] = ef ? '1' : '0';

  if(copy_to_user(ubuf, buf, 1))
    return -EFAULT;

  *ppos = 1;

  return 1;
}


static struct proc_dir_entry *ent;

#ifdef HAVE_PROC_OPS
static const struct proc_ops proc_operations = {
      .proc_read  = procfile_read,
#else
static const struct file_operations proc_operations = {
      .owner = THIS_MODULE,
      .read  = procfile_read,
#endif
};


static int send_cmd_data(struct hgltp08_touchscreen *ctx, u8 cmd, u8 data)
{
    u8 buf[2] = { cmd, data };
    int ret;

    ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
    if (ret < 0)
    {
        printk(KERN_ALERT "MIPI DSI DCS write failed: %d\n", ret);

        return ret;
    }

    return 0;
}


static int switch_page(struct hgltp08_touchscreen *ctx, u8 page)
{
    u8 buf[4] = { 0xFF, 0x98, 0x81, page };
    int ret;

    ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
    if (ret < 0)
    {
        printk(KERN_ALERT "MIPI DSI DCS write failed for switching page: %d\n", ret);

        return ret;
    }

    return 0;
}

static int hgltp08_init_sequence(struct hgltp08_touchscreen *ctx)
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

static int hgltp08_prepare(struct drm_panel *panel)
{
    struct hgltp08_touchscreen *ctx = panel_to_ts(panel);
    struct mipi_dsi_device *dsi = ctx->dsi;
    int ret;
    int cmdcnt;
    bool slow_mode;

    if (ctx->prepared)
        return 0;

    printk(KERN_ALERT "Preparing!\n");


    procFile = proc_create("hgltp08", 0444, NULL, &proc_operations);


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

    cmdcnt = 0;
    do
    {
        ret = hgltp08_init_sequence(ctx);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt <= CMD_RETRIES);

    if (ret)
    {
        printk(KERN_ALERT "Couldn't send initialization commands!\n");

        return ret;
    }

    cmdcnt = 0;
    do
    {
        ret = switch_page(ctx, 0);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt <= CMD_RETRIES);

    if (ret)
    {
        printk(KERN_ALERT "Couldn't switch page!\n");

        return ret;
    }

    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt <= CMD_RETRIES);

    if (ret < 0)
    {
        printk(KERN_ALERT "Couldn't set tear on!\n");

        atomic_set(&errorFlag, 1);
    }

    if (!slow_mode)
        dsi->mode_flags &= ~(MIPI_DSI_MODE_LPM);

    msleep(125);

    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt <= CMD_RETRIES);

    if (ret)
    {
        printk(KERN_ALERT "Couldn't exit sleep mode!\n");

        atomic_set(&errorFlag, 1);
    }

    msleep(125);

    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_set_display_on(dsi);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt <= CMD_RETRIES);

    if (ret)
    {
        printk(KERN_ALERT "Couldn't set display on!\n");

        atomic_set(&errorFlag, 1);
    }

    msleep(20);

    ctx->prepared = true;

    printk(KERN_ALERT "Prepared!\n");

    return 0;
}


static int hgltp08_unprepare(struct drm_panel *panel)
{
    struct hgltp08_touchscreen *ctx = panel_to_ts(panel);
    struct mipi_dsi_device *dsi = ctx->dsi;
    int ret;

    if (!ctx->prepared)
        return 0;

    printk(KERN_ALERT "Unprepare!\n");

    proc_remove(ent);

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


static int hgltp08_enable(struct drm_panel *panel)
{
    struct hgltp08_touchscreen *ctx = panel_to_ts(panel);
    int ret;
    int cmdcnt;

    if (ctx->enabled)
        return 0;

    printk(KERN_ALERT "Enabling!\n");

    cmdcnt = 0;
    do
    {
        ret = drm_panel_prepare(panel);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt <= CMD_RETRIES);

    if (ret < 0)
    {
        printk(KERN_ALERT "Couldn't prepare the panel!\n");

        atomic_set(&errorFlag, 1);
    }

    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_set_tear_on(ctx->dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt <= CMD_RETRIES);

    if (ret < 0)
    {
        printk(KERN_ALERT "Couldn't set tear on!\n");

        atomic_set(&errorFlag, 1);
    }

    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_set_display_on(ctx->dsi);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt <= CMD_RETRIES);

    if (ret)
    {
        printk(KERN_ALERT "Couldn't set display on!\n");

        atomic_set(&errorFlag, 1);
    }

    if (ctx->gpioBacklightD)
        gpio_set_value_cansleep(ctx->backlightPin, 1);

    ctx->enabled = true;

    printk(KERN_ALERT "Enabled!\n");

    return 0;
}

static int hgltp08_disable(struct drm_panel *panel)
{
    struct hgltp08_touchscreen *ctx = panel_to_ts(panel);

    if (!ctx->enabled)
        return 0;

    mipi_dsi_dcs_set_display_off(ctx->dsi);

    if (ctx->gpioBacklightD)
        gpio_set_value_cansleep(ctx->backlightPin, 0);

    ctx->enabled = false;

    printk(KERN_ALERT "Disabled!\n");

    return 0;
}

static int hgltp08_get_modes(struct drm_panel *panel
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
                            , struct drm_connector *connector)
{
    struct drm_display_mode *mode = drm_mode_duplicate(connector->dev, &default_mode);
#else
                            )
{
    static const u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
    struct drm_display_mode *mode = drm_mode_duplicate(panel->drm, &default_mode);
#endif // KERNEL_VERSION

    if (!mode)
    {
        printk(KERN_ALERT "failed to add mode %ux%ux\n", default_mode.hdisplay, default_mode.vdisplay);
        return -ENOMEM;
    }

    drm_mode_set_name(mode);

    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
#else
    drm_mode_probed_add(panel->connector, mode);

    panel->connector->display_info.width_mm = mode->width_mm;
    panel->connector->display_info.height_mm = mode->height_mm;

    panel->connector->display_info.bpc = 8;

    drm_display_info_set_bus_formats(&panel->connector->display_info, &bus_format, 1);
#endif // KERNEL_VERSION

    return 1;
}

static const struct drm_panel_funcs hgltp08_drm_funcs =
{
    .disable = hgltp08_disable,
    .unprepare = hgltp08_unprepare,
    .prepare = hgltp08_prepare,
    .enable = hgltp08_enable,
    .get_modes = hgltp08_get_modes,
};

static int hgltp08_probe(struct mipi_dsi_device *dsi)
{
    int ret;
    const __be32 *prop;

    struct device *dev = &dsi->dev;
    struct hgltp08_touchscreen *ctx;

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


#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
	drm_panel_init(&ctx->base, dev, &hgltp08_drm_funcs,
		       DRM_MODE_CONNECTOR_DSI);
#else
    drm_panel_init(&ctx->base);
    ctx->base.dev = dev;
    ctx->base.funcs = &hgltp08_drm_funcs;
#endif // KERNEL_VERSION

    drm_panel_add(&ctx->base);

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

static int hgltp08_remove(struct mipi_dsi_device *dsi)
{
    struct hgltp08_touchscreen *ctx = mipi_dsi_get_drvdata(dsi);

    mipi_dsi_detach(dsi);
    drm_panel_remove(&ctx->base);

    kfree(ctx);

    printk(KERN_ALERT "Removed!\n");

    return 0;
}


static const struct of_device_id hgltp08_touchscreen_of_match[] =
{
    { .compatible = "hgltp08" },
    { }
};
MODULE_DEVICE_TABLE(of, hgltp08_touchscreen_of_match);



static struct mipi_dsi_driver panel_hgltp08_dsi_driver =
{
    .driver = {
        .name = "panel_hgltp08",
        .of_match_table = hgltp08_touchscreen_of_match,
    },
    .probe = hgltp08_probe,
    .remove = hgltp08_remove,
};
module_mipi_dsi_driver(panel_hgltp08_dsi_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Homegear GmbH <contact@homegear.email>");
MODULE_DESCRIPTION("Homegear LTP08 Multitouch 8\" Display; black; WXGA 1280x800; Linux");
MODULE_VERSION("1.0.3");
