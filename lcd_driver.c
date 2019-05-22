#include <linux/backlight.h>

#include <linux/gpio/consumer.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>

#include <video/mipi_display.h>

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/videomode.h>

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>

#include <linux/gpio.h>

#define DEFAULT_GPIO_RESET_PIN 2

struct whatever_touchscreen
{
    struct drm_panel base;
    struct mipi_dsi_device *dsi;

    // WARNING: not all of them might be needed for a particular case!
    //struct backlight_device *backlight;
    //struct i2c_client *bridge_i2c;
    //struct i2c_adapter *ddc;

    int resetPin;
    int gpioResetD;

    bool prepared;
    bool enabled;
};


static const struct drm_display_mode default_mode =
{
    .clock      = 68700, // original
    //.clock      = 68121, // calculated for original timings
    //.clock      = 64215, // calculated for reduced timings
    //.clock        = 65699,

    .vrefresh	= 60,

    .hdisplay	= 800,
    .vdisplay	= 1280,

/*
	hfront-porch = <32>;
	hsync-len = <20>;
    hback-porch = <20>;

	vfront-porch = <8>;
	vsync-len = <4>;
	vback-porch = <10>;
*/

    // Adjusted clock: 83333
    // Adjusted h_sync_start: 2772
    // Adjusted h_sync_end: 2792
    // Adjusted htotal: 1066


    .hsync_start= 800 + 32,
    .hsync_end	= 800 + 32 + 20,
    .htotal		= 800 + 32 + 20 + 20, // 872


// those are guessed values, this probably could be better

// 1 for sync is too low - unless front porch is set to the lowest possible value, 9
// back porch lower than 9 results in a very odd effect, as each line is moved horizontally, shaking, resulting in a fuzzy image

// for front porch 9 and sync 1 the back porch seems to be either 9 or 10

// setting the clock 'non continuous' for those settings results in a shaking image!

/*
    .hsync_start= 800 + 9,
    .hsync_end	= 800 + 9 + 1,
    .htotal		= 800 + 9 + 1 + 10,
*/

    //Adjusted clock: 66666
    //Adjusted h_sync_start: 841
    //Adjusted h_sync_end: 843
    //Adjusted htotal: 853

/*
    .hsync_start= 800 + 10,
    .hsync_end	= 800 + 10 + 2,
    .htotal		= 800 + 10 + 2 + 10,
*/

// this is close to the original with timings, but the clock is too low?

    //Adjusted clock: 66666
    //Adjusted h_sync_start: 833
    //Adjusted h_sync_end: 853
    //Adjusted htotal: 873

// forcing a higher clock gets this for the same settings:

    // Adjusted clock: 83333
    // Adjusted h_sync_start: 988 - very late! Is this a problem?
    // Adjusted h_sync_end: 1008 - sync length 20 as needed
    // Adjusted htotal: 1028 - back porch 20 as needed

    /*
    .hsync_start= 800 + 1,
    .hsync_end	= 800 + 1 + 20,
    .htotal		= 800 + 1 + 20 + 20, // 841
    */

    //Adjusted clock: 66666
    //Adjusted h_sync_start: 833
    //Adjusted h_sync_end: 851
    //Adjusted htotal: 853


/*
    .hsync_start= 800 + 2,
    .hsync_end	= 800 + 2 + 18,
    .htotal		= 800 + 2 + 18 + 2, // 822
*/

// those seem to be good
    .vsync_start= 1280 + 8,
    .vsync_end	= 1280 + 8 + 4,
    .vtotal		= 1280 + 8 + 4 + 10, // 1302


    .width_mm = 170,
    .height_mm = 106,

    //.flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};


static struct whatever_touchscreen *panel_to_ts(struct drm_panel *panel)
{
    return container_of(panel, struct whatever_touchscreen, base);
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
            .cmd = 0xE0,        \
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
    SWITCH_PAGE_CMD(0),
    COMMAND_CMD(0xE1, 0x93),
    COMMAND_CMD(0xE2, 0x65),
    COMMAND_CMD(0xE3, 0xF8),
    COMMAND_CMD(0x80, 0x03),

    SWITCH_PAGE_CMD(4),
    COMMAND_CMD(0x2D, 0x03),

    SWITCH_PAGE_CMD(1),
    COMMAND_CMD(0x00, 0x00),//Set VCOM
    COMMAND_CMD(0x01, 0x6F),
    //Set Gamma Power);W_D( VGMP);W_D(VGMN);W_D(VGSP);W_D(VGSN)
    COMMAND_CMD(0x17, 0x00),
    COMMAND_CMD(0x18, 0xD7),//VGMP=4.8V
    COMMAND_CMD(0x19, 0x05),
    COMMAND_CMD(0x1A, 0x00),
    COMMAND_CMD(0x1B, 0xD7),//VGMN=-4.8V
    COMMAND_CMD(0x1C, 0x05),
    //Set Gate Power
    COMMAND_CMD(0x1F, 0x79),//VGH_Reg = 18V
    COMMAND_CMD(0x20, 0x2D),//VGL_Reg  = -12V
    COMMAND_CMD(0x21, 0x2d),//VGL_Reg2 = -12V
    COMMAND_CMD(0x22, 0x4F),
    COMMAND_CMD(0x26, 0xF1),//VDDD from IOVCC
    //SET PANEL
    COMMAND_CMD(0x37, 0x09),//SS=1 BGR=1
    //SET RGBCYC
    COMMAND_CMD(0x38, 0x04),//JDT=100 column inversion
    COMMAND_CMD(0x39, 0x08),//RGB_N
    COMMAND_CMD(0x3A, 0x12),
    COMMAND_CMD(0x3C, 0x78),//SET EQ3 for TE_H
    COMMAND_CMD(0x3E, 0x80),//SET CHGEN_OFF
    COMMAND_CMD(0x3F, 0x80),//SET CHGEN_OFF2
    //Set TCON
    COMMAND_CMD(0x40, 0x06),//RSO=800 RGB
    COMMAND_CMD(0x41, 0xA0),//LN=640->1280 line
    //power voltage
    COMMAND_CMD(0x55, 0x01),//DCDCM=0001
    COMMAND_CMD(0x56, 0x01),
    COMMAND_CMD(0x57, 0xA8),
    COMMAND_CMD(0x58, 0x0A),
    COMMAND_CMD(0x59, 0x2A),//VCL = -2.7V
    COMMAND_CMD(0x5A, 0x37),//VGH = 19V
    COMMAND_CMD(0x5B, 0x19),//VGL = -12V
    //Gamma
    COMMAND_CMD(0x5D, 0x70),
    COMMAND_CMD(0x5E, 0x50),
    COMMAND_CMD(0x5F, 0x3F),
    COMMAND_CMD(0x60, 0x31),
    COMMAND_CMD(0x61, 0x2D),
    COMMAND_CMD(0x62, 0x1D),
    COMMAND_CMD(0x63, 0x22),
    COMMAND_CMD(0x64, 0x0C),
    COMMAND_CMD(0x65, 0x25),
    COMMAND_CMD(0x66, 0x24),
    COMMAND_CMD(0x67, 0x24),
    COMMAND_CMD(0x68, 0x41),
    COMMAND_CMD(0x69, 0x2F),
    COMMAND_CMD(0x6A, 0x36),
    COMMAND_CMD(0x6B, 0x28),
    COMMAND_CMD(0x6C, 0x26),
    COMMAND_CMD(0x6D, 0x1C),
    COMMAND_CMD(0x6E, 0x08),
    COMMAND_CMD(0x6F, 0x02),
    COMMAND_CMD(0x70, 0x70),
    COMMAND_CMD(0x71, 0x50),
    COMMAND_CMD(0x72, 0x3F),
    COMMAND_CMD(0x73, 0x31),
    COMMAND_CMD(0x74, 0x2D),
    COMMAND_CMD(0x75, 0x1D),
    COMMAND_CMD(0x76, 0x22),
    COMMAND_CMD(0x77, 0x0C),
    COMMAND_CMD(0x78, 0x25),
    COMMAND_CMD(0x79, 0x24),
    COMMAND_CMD(0x7A, 0x24),
    COMMAND_CMD(0x7B, 0x41),
    COMMAND_CMD(0x7C, 0x2F),
    COMMAND_CMD(0x7D, 0x36),
    COMMAND_CMD(0x7E, 0x28),
    COMMAND_CMD(0x7F, 0x26),
    COMMAND_CMD(0x80, 0x1C),
    COMMAND_CMD(0x81, 0x08),
    COMMAND_CMD(0x82, 0x02),
    SWITCH_PAGE_CMD(2),//for GIP
    //GIP_L Pin mapping
    COMMAND_CMD(0x00, 0x00),
    COMMAND_CMD(0x01, 0x04),
    COMMAND_CMD(0x02, 0x06),
    COMMAND_CMD(0x03, 0x08),
    COMMAND_CMD(0x04, 0x0A),
    COMMAND_CMD(0x05, 0x0C),
    COMMAND_CMD(0x06, 0x0E),
    COMMAND_CMD(0x07, 0x17),
    COMMAND_CMD(0x08, 0x37),
    COMMAND_CMD(0x09, 0x1F),
    COMMAND_CMD(0x0A, 0x10),
    COMMAND_CMD(0x0B, 0x1F),
    COMMAND_CMD(0x0C, 0x1F),
    COMMAND_CMD(0x0D, 0x1F),
    COMMAND_CMD(0x0E, 0x1F),
    COMMAND_CMD(0x0F, 0x1F),
    COMMAND_CMD(0x10, 0x1F),
    COMMAND_CMD(0x11, 0x1F),
    COMMAND_CMD(0x12, 0x1F),
    COMMAND_CMD(0x13, 0x1F),
    COMMAND_CMD(0x14, 0x1F),
    COMMAND_CMD(0x15, 0x1F),
    //GIP_R Pin mapping
    COMMAND_CMD(0x16, 0x01),
    COMMAND_CMD(0x17, 0x05),
    COMMAND_CMD(0x18, 0x07),
    COMMAND_CMD(0x19, 0x09),
    COMMAND_CMD(0x1A, 0x0B),
    COMMAND_CMD(0x1B, 0x0D),
    COMMAND_CMD(0x1C, 0x0F),
    COMMAND_CMD(0x1D, 0x17),
    COMMAND_CMD(0x1E, 0x37),
    COMMAND_CMD(0x1F, 0x1F),
    COMMAND_CMD(0x20, 0x11),
    COMMAND_CMD(0x21, 0x1F),
    COMMAND_CMD(0x22, 0x1F),
    COMMAND_CMD(0x23, 0x1F),
    COMMAND_CMD(0x24, 0x1F),
    COMMAND_CMD(0x25, 0x1F),
    COMMAND_CMD(0x26, 0x1F),
    COMMAND_CMD(0x27, 0x1F),
    COMMAND_CMD(0x28, 0x1F),
    COMMAND_CMD(0x29, 0x13),
    COMMAND_CMD(0x2A, 0x1F),
    COMMAND_CMD(0x2B, 0x1F),
    //GIP Timing
    COMMAND_CMD(0x58, 0x10),
    COMMAND_CMD(0x59, 0x00),
    COMMAND_CMD(0x5A, 0x00),
    COMMAND_CMD(0x5B, 0x10),
    COMMAND_CMD(0x5C, 0x07),
    COMMAND_CMD(0x5D, 0x30),
    COMMAND_CMD(0x5E, 0x00),
    COMMAND_CMD(0x5F, 0x00),
    COMMAND_CMD(0x60, 0x30),
    COMMAND_CMD(0x61, 0x03),
    COMMAND_CMD(0x62, 0x04),
    COMMAND_CMD(0x63, 0x03),
    COMMAND_CMD(0x64, 0x6A),
    COMMAND_CMD(0x65, 0x75),
    COMMAND_CMD(0x66, 0x0D),
    COMMAND_CMD(0x67, 0xB3),
    COMMAND_CMD(0x68, 0x09),
    COMMAND_CMD(0x69, 0x06),
    COMMAND_CMD(0x6A, 0x6A),
    COMMAND_CMD(0x6B, 0x04),
    COMMAND_CMD(0x6C, 0x00),
    COMMAND_CMD(0x6D, 0x04),
    COMMAND_CMD(0x6E, 0x04),
    COMMAND_CMD(0x6F, 0x88),
    COMMAND_CMD(0x70, 0x00),
    COMMAND_CMD(0x71, 0x00),
    COMMAND_CMD(0x72, 0x06),
    COMMAND_CMD(0x73, 0x7B),
    COMMAND_CMD(0x74, 0x00),
    COMMAND_CMD(0x75, 0xBC),
    COMMAND_CMD(0x76, 0x00),
    COMMAND_CMD(0x77, 0x0D),
    COMMAND_CMD(0x78, 0x2C),
    COMMAND_CMD(0x79, 0x00),
    COMMAND_CMD(0x7A, 0x00),
    COMMAND_CMD(0x7B, 0x00),
    COMMAND_CMD(0x7C, 0x00),
    COMMAND_CMD(0x7D, 0x03),
    COMMAND_CMD(0x7E, 0x7B),
    COMMAND_CMD(0xE0, 0x04),
    COMMAND_CMD(0x2B, 0x2B),
    COMMAND_CMD(0x2E, 0x44),

    SWITCH_PAGE_CMD(0),
    COMMAND_CMD(0xE6, 0x02),
    COMMAND_CMD(0xE7, 0x02),
    //TE - something related with tear, but how to set?
    //COMMAND_CMD(0x34, 0x00), // TE off?
    COMMAND_CMD(0x35, 0x00), // TE on?
    CMD_DELAY(0x11, 0x00, 100),
    CMD_DELAY(0x29, 0x00, 100),
};


static int send_cmd_data(struct whatever_touchscreen *ctx, u8 cmd, u8 data)
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

static int switch_page(struct whatever_touchscreen *ctx, u8 page)
{
    return send_cmd_data(ctx, 0xE0, page);
}

static int whatever_init_sequence(struct whatever_touchscreen *ctx)
{
    int i, ret;

    ret = 0;

    for (i = 0; i < ARRAY_SIZE(panel_cmds_init); ++i)
    {
        const struct panel_command *cmd = &panel_cmds_init[i];

        ret = send_cmd_data(ctx, cmd->cmd.cmd, cmd->cmd.data);

        if (cmd->delay)
            msleep(cmd->delay);
    }

    return 0;
}


static int whatever_prepare(struct drm_panel *panel)
{
    struct whatever_touchscreen *ctx = panel_to_ts(panel);
    struct mipi_dsi_device *dsi = ctx->dsi;
    int ret;
    bool slow_mode;

    if (ctx->prepared)
        return 0;

    printk(KERN_ALERT "Preparing!\n");
    if (!dsi)
        printk(KERN_ALERT "No DSI device!\n");

    ctx->gpioResetD = gpio_request(ctx->resetPin, "gpioResetD");
    if (ctx->gpioResetD < 0)
    {
        ctx->gpioResetD = 0;
        printk(KERN_ALERT "Couldn't grab the gpio ResetD pin\n");
    }
    else ctx->gpioResetD = 1;

    if (ctx->gpioResetD) gpio_direction_output(ctx->resetPin, 1);


    msleep(125);

    if (ctx->gpioResetD) gpio_set_value_cansleep(ctx->resetPin, 0);
    msleep(20);
    if (ctx->gpioResetD) gpio_set_value_cansleep(ctx->resetPin, 1);

    msleep(125);

    slow_mode = dsi->mode_flags & MIPI_DSI_MODE_LPM;

    if (!slow_mode) dsi->mode_flags |= MIPI_DSI_MODE_LPM;

    ret = whatever_init_sequence(ctx);
    if (ret)
        return ret;

    ret = switch_page(ctx, 0);
    if (ret)
        return ret;

    if (!slow_mode) dsi->mode_flags &= ~(MIPI_DSI_MODE_LPM);

    msleep(125);

    //ret = mipi_dsi_dcs_set_tear_on(ctx->dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK/*MIPI_DSI_DCS_TEAR_MODE_VHBLANK*/);
	//if (ret)
	//	return ret;
	//mipi_dsi_dcs_set_tear_off(ctx->dsi);

    ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
    if (ret)
    {
        printk(KERN_ALERT "Couldn't exit sleep mode!\n");

        return ret;
    }

    msleep(125);

    ret = mipi_dsi_dcs_set_display_on(dsi);
    if (ret)
        return ret;

    msleep(20);

    ctx->prepared = true;

    return 0;
}


static int whatever_unprepare(struct drm_panel *panel)
{
    struct whatever_touchscreen *ctx = panel_to_ts(panel);
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

    if (ctx->gpioResetD) {
        gpio_free(ctx->resetPin); // find something different, this is GPL
        ctx->gpioResetD = 0;
    }

    ctx->prepared = false;

    return 0;
}


static int whatever_enable(struct drm_panel *panel)
{
    struct whatever_touchscreen *ctx = panel_to_ts(panel);

    if (ctx->enabled)
        return 0;

    // TODO: check the backlight functionality with this kernel!
    //backlight_enable(ctx->backlight);
    /*
    	if (ctx->backlight) {
    		ctx->backlight->props.state &= ~BL_CORE_FBBLANK;
    		ctx->backlight->props.power = FB_BLANK_UNBLANK;
    		backlight_update_status(ctx->backlight);
    	}
    */

    printk(KERN_ALERT "Enabling!\n");

    drm_panel_prepare(panel);

    mipi_dsi_dcs_set_display_on(ctx->dsi);

    ctx->enabled = true;

    return 0;
}

static int whatever_disable(struct drm_panel *panel)
{
    struct whatever_touchscreen *ctx = panel_to_ts(panel);

    if (!ctx->enabled)
        return 0;

    //backlight_disable(ctx->backlight);

    /*
    	if (ctx->backlight) {
    		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
    		ctx->backlight->props.state |= BL_CORE_FBBLANK;
    		backlight_update_status(ctx->backlight);
    	}
    */

    mipi_dsi_dcs_set_display_off(ctx->dsi);

    ctx->enabled = false;

    printk(KERN_ALERT "Disabled!\n");

    return 0;
}


static int whatever_get_modes(struct drm_panel *panel)
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

/*
static void whatever_panel_shutdown(struct device *dev)
{
	struct drm_panel *panel = dev_get_drvdata(dev);

	whatever_disable(panel);
}
*/


static const struct drm_panel_funcs whatever_drm_funcs =
{
    .disable = whatever_disable,
    .unprepare = whatever_unprepare,
    .prepare = whatever_prepare,
    .enable = whatever_enable,
    .get_modes = whatever_get_modes,
};

static int whatever_probe(struct mipi_dsi_device *dsi)
{
    int ret;
    const __be32 *prop;

    struct device *dev = &dsi->dev;
    struct whatever_touchscreen *ctx;

    printk(KERN_ALERT "Probing!\n");

    ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;
    memset(ctx, 0, sizeof(*ctx));

    ctx->resetPin = DEFAULT_GPIO_RESET_PIN;

    prop = of_get_property(dsi->dev.of_node, "reset", NULL);
    if (prop) {
        ctx->resetPin = be32_to_cpup(prop);
        printk(KERN_ALERT "Reset pin set to %d\n", ctx->resetPin);
    }
    else printk(KERN_ALERT "Reset pin not set, using default\n");

    mipi_dsi_set_drvdata(dsi, ctx);
    ctx->dsi = dsi;

    dsi->lanes = 4;
    dsi->format = MIPI_DSI_FMT_RGB888;

    // TODO: try other modes

    // also seems to work fine even if MIPI_DSI_MODE_LPM is not set - this makes the driver send dts commands in 'slow' mode
    // should not hurt to be set


/* DSI mode flags */

/* video mode */
//#define MIPI_DSI_MODE_VIDEO		BIT(0)
/* video burst mode */
//#define MIPI_DSI_MODE_VIDEO_BURST	BIT(1)
/* video pulse mode */
//#define MIPI_DSI_MODE_VIDEO_SYNC_PULSE	BIT(2)
/* enable auto vertical count mode */
//#define MIPI_DSI_MODE_VIDEO_AUTO_VERT	BIT(3)
/* enable hsync-end packets in vsync-pulse and v-porch area */
//#define MIPI_DSI_MODE_VIDEO_HSE		BIT(4)
/* disable hfront-porch area */
//#define MIPI_DSI_MODE_VIDEO_HFP		BIT(5)
/* disable hback-porch area */
//#define MIPI_DSI_MODE_VIDEO_HBP		BIT(6)
/* disable hsync-active area */
//#define MIPI_DSI_MODE_VIDEO_HSA		BIT(7)
/* flush display FIFO on vsync pulse */
//#define MIPI_DSI_MODE_VSYNC_FLUSH	BIT(8)
/* disable EoT packets in HS mode */
//#define MIPI_DSI_MODE_EOT_PACKET	BIT(9)
/* device supports non-continuous clock behavior (DSI spec 5.6.1) */
//#define MIPI_DSI_CLOCK_NON_CONTINUOUS	BIT(10)
/* transmit data in low power */
//#define MIPI_DSI_MODE_LPM		BIT(11)

    //dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_VSYNC_FLUSH | MIPI_DSI_MODE_VIDEO_AUTO_VERT;

    //dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_VSYNC_FLUSH; // this seems more tear free than other settings? Still can see tearing from time to time.

    //dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS  | MIPI_DSI_MODE_VIDEO_HSA /*| MIPI_DSI_MODE_VSYNC_FLUSH*/;

    //dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_VIDEO_AUTO_VERT/*| MIPI_DSI_MODE_VSYNC_FLUSH*/;

    //dsi->mode_flags |= MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_VSYNC_FLUSH /*| MIPI_DSI_CLOCK_NON_CONTINUOUS */ /*| MIPI_DSI_MODE_VIDEO_SYNC_PULSE*/ /*| MIPI_DSI_MODE_VIDEO_BURST*/;

    dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;

    printk(KERN_ALERT "DSI Device init for %s!\n", dsi->name);

    drm_panel_init(&ctx->base);

    ctx->base.dev = dev;
    ctx->base.funcs = &whatever_drm_funcs;

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

static int whatever_remove(struct mipi_dsi_device *dsi)
{
    struct whatever_touchscreen *ctx = mipi_dsi_get_drvdata(dsi);

    mipi_dsi_detach(dsi);
    drm_panel_remove(&ctx->base);

    kfree(ctx);

    printk(KERN_ALERT "Removed!\n");

    return 0;
}

/*
static void whatever_shutdown(struct mipi_dsi_device *dsi)
{
    struct device *dev;

    printk(KERN_ALERT "Shutdown!\n");

    dev = &dsi->dev;
	whatever_panel_shutdown(dev);
}
*/


static const struct of_device_id whatever_touchscreen_of_match[] =
{
    { .compatible = "divus,whatever" },
    { }
};
MODULE_DEVICE_TABLE(of, whatever_touchscreen_of_match);



static struct mipi_dsi_driver panel_whatever_dsi_driver =
{
    .driver = {
        .name = "panel_divus_whatever",
        .of_match_table = whatever_touchscreen_of_match,
    },
    .probe = whatever_probe,
    .remove = whatever_remove,
//	.shutdown = whatever_shutdown,
};
module_mipi_dsi_driver(panel_whatever_dsi_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Homegear GmbH <contact@homegear.email>");
MODULE_DESCRIPTION("Some test driver for a DSI panel");
MODULE_VERSION("0.01");
