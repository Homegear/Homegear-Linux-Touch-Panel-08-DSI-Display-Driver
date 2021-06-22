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
#include <linux/firmware.h>

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

#define INIT_CMDS_RETRIES 3 // reset usually fails each time, if it fails
#define CMD_RETRIES 5 // usually if it doesn't recover after the first or second failure, it won't recover at all

#define RETRY_DELAY 100

#define CMD_DELAY_TIME 100

//#define NO_ENTER_OFF 1
//#define NO_ENTER_SLEEP 1

#define USE_ORIG_GAMMA 1

//#define RETRY_INIT_CMD 1 // with a proper change in VC4 driver, it might work

//#define ENABLE_DITHERING 1
//#define SET_PIXELS_OFF 1

//#define INVERSION 1

//#define TEST_READ 1

#define MAX_INIT_CMDS_NO 500


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
    bool isOn;
};


static struct drm_display_mode default_mode =
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
    COMMAND_CMD(0x34, 0x00), // GPWR1/2 non overlap time 2.62us ?
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

    COMMAND_CMD(0x6C, 0x15), /* Set VCORE voltage = 1.5V */

    COMMAND_CMD(0x6E, 0x30), // VGH clamp 16.06       LCD SPEC  16  - power control 2
	//COMMAND_CMD(0x6E, 0x2A), /* di_pwr_reg=0 for power mode 2A, VGH clamp 18V */

    //COMMAND_CMD(0x6F, 0x33),  // old value - pumping ratio VGH=5x VGL=-3x
    COMMAND_CMD(0x6F, 0x37), // 33  VGH pumping ratio 3x, VGL=-3x   ¸Ä³É37=VGH pumping ratio 3x, VGL=-3x   - power control 3

    //COMMAND_CMD(0x8D, 0x87), // old value
    COMMAND_CMD(0x8D, 0x1F), // VGL clamp -12.03      LCD SPEC -12  - power control 4
	//COMMAND_CMD(0x8D, 0x1B), /* VGL clamp -10V */

    COMMAND_CMD(0x87, 0xBA), // LVD Function 1 - ESD?

    COMMAND_CMD(0x26, 0x76), // SDTiming Control
    COMMAND_CMD(0xB2, 0xD1), // Reload Gamma Setting
    COMMAND_CMD(0x35, 0x1F),
    COMMAND_CMD(0x33, 0x14),

    //COMMAND_CMD(0x3A, 0x24), POWER SAVING in ilitek-ili9881c driver
    COMMAND_CMD(0x3A, 0xA9), // power saving

    COMMAND_CMD(0x3B, 0x98), //C0  For 4003D  98 = ILI4003 - New value!!!!!!

    COMMAND_CMD(0x38, 0x01),
    COMMAND_CMD(0x39, 0x00),

    SWITCH_PAGE_CMD(0x01),

    COMMAND_CMD(0x22, 0x0A), // BGR, SS - Set Panel, Operation Mode and Data, Complement Setting  = BGR_PANEL & SS_PANEL for 0x0A - the 'Source Output Scan Direction' is backward for this setting
    //COMMAND_CMD(0x22, 0x08), // Set Panel, Operation Mode and Data, Complement Setting  = BGR_PANEL & SS_PANEL for 0x08 - the 'Source Output Scan Direction' is forward for this setting - what this does compared with the other one is to turn the screen 'upside-down' (for the rotated variant)

    // 0x25, 0x26, 0x27, 0x28 - blanking porch control

#ifdef INVERSION
    COMMAND_CMD(0x31, 0x04),
#else
    COMMAND_CMD(0x31, 0x00), //Column inversion - Display Inversion - default value 0x0
#endif // INVERSION

//  COMMAND_CMD(0x40, 0x33), //for EXT_CPCK_SEL for4003D - was commented out in the init sequence - also could be 0x53 - ILI4003D sel

#ifdef ENABLE_DITHERING
	COMMAND_CMD(0x34, 0x1), // dithering enable
#endif // ENABLE_DITHERING

    // Power control 1 - both following
    COMMAND_CMD(0x50, 0xC0),//8D - 5V
    COMMAND_CMD(0x51, 0xC0),//8A - 5V

    // VCOM control 1 - both following
    COMMAND_CMD(0x53, 0x43),//VCOM   816 mV
    COMMAND_CMD(0x55, 0x7A),// 1476 mV

    COMMAND_CMD(0x60, 0x28),// Source Timing Adjust SDT[5:0] - originally in the initialization sequence

    //COMMAND_CMD(0x60, 0x14), // Source Timing Adjust SDT[5:0] - default

    // settings from ilitek-ili9881c driver - timings
    //COMMAND_CMD(0x60, 0x15),
	//COMMAND_CMD(0x61, 0x01),
	//COMMAND_CMD(0x62, 0x0C),
	//COMMAND_CMD(0x63, 0x00),
	// ************************************

    COMMAND_CMD(0x2E, 0xC8),// Gate Number 0xC8 is the default - the number of lines to drive the LCD at an interval of 4 lines - the default is 1280

#ifdef USE_ORIG_GAMMA
// positive gamma correction
    COMMAND_CMD(0xA0, 0x01),
    COMMAND_CMD(0xA1, 0x11), /* VP251 */
    COMMAND_CMD(0xA2, 0x1C), /* VP247 */
    COMMAND_CMD(0xA3, 0x0E), /* VP243 */
    COMMAND_CMD(0xA4, 0x15), /* VP239 */
    COMMAND_CMD(0xA5, 0x28), /* VP231 */
    COMMAND_CMD(0xA6, 0x1C), /* VP219 */
    COMMAND_CMD(0xA7, 0x1E), /* VP203 */
    COMMAND_CMD(0xA8, 0x73), /* VP175 */
    COMMAND_CMD(0xA9, 0x1C), /* VP144 */

    COMMAND_CMD(0xAA, 0x26),//L111 /* VP111 */
    COMMAND_CMD(0xAB, 0x63),//L80  /* VP80 */
    COMMAND_CMD(0xAC, 0x18),//L52  /* VP52 */
    COMMAND_CMD(0xAD, 0x16),//L36  /* VP36 */
    COMMAND_CMD(0xAE, 0x4D),//L24  /* VP24 */
    COMMAND_CMD(0xAF, 0x1F),//L16  /* VP16 */
    COMMAND_CMD(0xB0, 0x2A),//L12  /* VP12 */
    COMMAND_CMD(0xB1, 0x4F),//L8   /* VP8 */
    COMMAND_CMD(0xB2, 0x5F),//L4   /* VP4 */
    COMMAND_CMD(0xB3, 0x39),//L0   /* VP0 */

// negative gamma correction
    COMMAND_CMD(0xC0, 0x01), /* VN255 GAMMA N */
    COMMAND_CMD(0xC1, 0x11), /* VN251 */
    COMMAND_CMD(0xC2, 0x1C), /* VN247 */
    COMMAND_CMD(0xC3, 0x0E), /* VN243 */
    COMMAND_CMD(0xC4, 0x15), /* VN239 */
    COMMAND_CMD(0xC5, 0x28), /* VN231 */
    COMMAND_CMD(0xC6, 0x1C), /* VN219 */
    COMMAND_CMD(0xC7, 0x1E), /* VN203 */
    COMMAND_CMD(0xC8, 0x73), /* VN175 */
    COMMAND_CMD(0xC9, 0x1C), /* VN144 */

    COMMAND_CMD(0xCA, 0x26),//L111 /* VN111 */
    COMMAND_CMD(0xCB, 0x63),//L80  /* VN80 */
    COMMAND_CMD(0xCC, 0x18),//L52  /* VN52 */
    COMMAND_CMD(0xCD, 0x16),//L36  /* VN36 */
    COMMAND_CMD(0xCE, 0x4D),//L24  /* VN24 */
    COMMAND_CMD(0xCF, 0x1F),//L16  /* VN16 */
    COMMAND_CMD(0xD0, 0x2A),//L12  /* VN12 */
    COMMAND_CMD(0xD1, 0x4F),//L8   /* VN8 */
    COMMAND_CMD(0xD2, 0x5F),//L4   /* VN4 */
    COMMAND_CMD(0xD3, 0x39),//L0   /* VN0 */
#else
	COMMAND_CMD(0xA0, 0x00),
	COMMAND_CMD(0xA1, 0x13), /* VP251 */
	COMMAND_CMD(0xA2, 0x23), /* VP247 */
	COMMAND_CMD(0xA3, 0x14), /* VP243 */
	COMMAND_CMD(0xA4, 0x16), /* VP239 */
	COMMAND_CMD(0xA5, 0x29), /* VP231 */
	COMMAND_CMD(0xA6, 0x1E), /* VP219 */
	COMMAND_CMD(0xA7, 0x1D), /* VP203 */
	COMMAND_CMD(0xA8, 0x86), /* VP175 */
	COMMAND_CMD(0xA9, 0x1E), /* VP144 */
	COMMAND_CMD(0xAA, 0x29), /* VP111 */
	COMMAND_CMD(0xAB, 0x74), /* VP80 */
	COMMAND_CMD(0xAC, 0x19), /* VP52 */
	COMMAND_CMD(0xAD, 0x17), /* VP36 */
	COMMAND_CMD(0xAE, 0x4B), /* VP24 */
	COMMAND_CMD(0xAF, 0x20), /* VP16 */
	COMMAND_CMD(0xB0, 0x26), /* VP12 */
	COMMAND_CMD(0xB1, 0x4C), /* VP8 */
	COMMAND_CMD(0xB2, 0x5D), /* VP4 */
	COMMAND_CMD(0xB3, 0x3F), /* VP0 */
	COMMAND_CMD(0xC0, 0x00), /* VN255 GAMMA N */
	COMMAND_CMD(0xC1, 0x13), /* VN251 */
	COMMAND_CMD(0xC2, 0x23), /* VN247 */
	COMMAND_CMD(0xC3, 0x14), /* VN243 */
	COMMAND_CMD(0xC4, 0x16), /* VN239 */
	COMMAND_CMD(0xC5, 0x29), /* VN231 */
	COMMAND_CMD(0xC6, 0x1E), /* VN219 */
	COMMAND_CMD(0xC7, 0x1D), /* VN203 */
	COMMAND_CMD(0xC8, 0x86), /* VN175 */
	COMMAND_CMD(0xC9, 0x1E), /* VN144 */
	COMMAND_CMD(0xCA, 0x29), /* VN111 */
	COMMAND_CMD(0xCB, 0x74), /* VN80 */
	COMMAND_CMD(0xCC, 0x19), /* VN52 */
	COMMAND_CMD(0xCD, 0x17), /* VN36 */
	COMMAND_CMD(0xCE, 0x4B), /* VN24 */
	COMMAND_CMD(0xCF, 0x20), /* VN16 */
	COMMAND_CMD(0xD0, 0x26), /* VN12 */
	COMMAND_CMD(0xD1, 0x4C), /* VN8 */
	COMMAND_CMD(0xD2, 0x5D), /* VN4 */
	COMMAND_CMD(0xD3, 0x3F), /* VN0 */
#endif

    SWITCH_PAGE_CMD(0x00),

    //COMMAND_CMD(0x55, 0x00), // This is the power save, 0x0 is the default, power save off

    COMMAND_CMD(0x35, 0x00),// TE ON, only V-Blanking, 0x34 TE OFF
    CMD_DELAY(0x11, 0x00, CMD_DELAY_TIME),// Sleep Out - sleep in is 0x10
    CMD_DELAY(0x29, 0x00, CMD_DELAY_TIME),// Display ON (OFF is 0x28)
    //CMD_DELAY(0x38, 0x00, CMD_DELAY_TIME), // Idle mode off

    // some other commands: 0x1 - software reset, 0x13 normal display mode on, 0x22 all pixels off 0x23 - all pixels on
    // 0x38 idle mode off, 0x39 idle mode on, 0x59 stop transition
    // 0x51 - display brightness
};


struct panel_command* fw_panel_cmds_init = NULL;
int fw_panel_cmds_init_size = 0;

static bool is_ignored(char c)
{
    return c == '\n' || c == '\r' || c == ' ' || c == '\t';
}


static int parse_val(char* param)
{
    long val;
    //int len;
    //char *endp;

    if (!param) return 0;

    /*
    len = strlen(param);
    if (len > 1 && param[0] == '0' && (param[1] == 'x' || param[1] == 'X'))
    {
        if (kstrtol(param + 2, 16, &val) != 0)
            val = 0;
    }
    else
    {
        if (kstrtol(param, 10, &val) != 0)
            val = 0;
    }
    */

    if (kstrtol(param, 0, &val) != 0) // simpler
        val = 0;

    return val;
}

static void parse_firmware(const char* data, int len)
{
    int i, to_pos, cmd_no;
    char *cmd_str;
    char *param1_str;
    char *param2_str;
    char *param3_str;
    char *str_save1;
    char *str_save2;
    char *dst;
    const char* delims = "(),";

    if (0 == len) return;

    dst = kmalloc(len + 1, GFP_KERNEL);
    if (!dst) return;

    str_save1 = dst;

    to_pos = 0;
    for (i = 0; i < len; ++i)
    {
        // skip over comments
        if (i < len - 1)
        {
            if (data[i] == '/' && data[i + 1] == '/')
            {
                i += 2;
                while (i < len && data[i] != '\n')
                    ++i;
            }
            else if (data[i] == '/' && data[i + 1] == '*')
            {
                i += 2;
                while (i < len - 1 && !(data[i] == '*' && data[i + 1] == '/'))
                    ++i;

                if (i < len - 1 && data[i] == '*' && data[i + 1] == '/')
                    i += 2;
            }

            if (i >= len) break;
        }

        if (is_ignored(data[i])) continue;

        dst[to_pos] = data[i];
        ++to_pos;
    }
    dst[to_pos] = 0;

    str_save2 = kmalloc(to_pos + 1, GFP_KERNEL);
    if (!str_save2)
    {
        kfree(dst);
        return;
    }
    strcpy(str_save2, dst);

    cmd_no = 0;
    // count the commands
    while((cmd_str = strsep(&dst, delims)) != NULL && cmd_no < MAX_INIT_CMDS_NO)
    {
        if (0 == cmd_str[0]) continue;

        if (0 == strcmp(cmd_str, "SWITCH_PAGE_CMD")) // one parameter
        {
            param1_str = strsep(&dst, delims);
            if (!param1_str)
            {
                printk(KERN_ALERT "SWITCH_PAGE_CMD has missing parameter\n");

                break;
            }

            ++cmd_no;
        }
        else if (0 == strcmp(cmd_str, "COMMAND_CMD")) // two parameters
        {
            param1_str = strsep(&dst, delims);
            if (!param1_str)
            {
                printk(KERN_ALERT "COMMAND_CMD has missing parameter\n");
                break;
            }
            param2_str = strsep(&dst, delims);
            if (!param2_str)
            {
                printk(KERN_ALERT "COMMAND_CMD has missing parameter\n");
                break;
            }

            ++cmd_no;
        }
        else if (0 == strcmp(cmd_str, "CMD_DELAY")) // three parameters
        {
            param1_str = strsep(&dst, delims);
            if (!param1_str)
            {
                printk(KERN_ALERT "CMD_DELAY has missing parameter\n");
                break;
            }
            param2_str = strsep(&dst, delims);
            if (!param2_str)
            {
                printk(KERN_ALERT "CMD_DELAY has missing parameter\n");
                break;
            }
            param3_str = strsep(&dst, delims);
            if (!param3_str)
            {
                printk(KERN_ALERT "CMD_DELAY has missing parameter\n");
                break;
            }

            ++cmd_no;
        }
    }
    kfree(str_save1);

    if (cmd_no > 0)
    {
        if (cmd_no >= MAX_INIT_CMDS_NO)
            printk(KERN_ALERT "The number of initialization commands in the firmware file is up to the limit, are you sure?\n");

        fw_panel_cmds_init = kmalloc(sizeof(struct panel_command) * cmd_no, GFP_KERNEL);
        if (!fw_panel_cmds_init)
        {
            kfree(str_save2);
            return;
        }
    }
    else
    {
        kfree(str_save2);
        return;
    }

    fw_panel_cmds_init_size = cmd_no;

    printk(KERN_ALERT "The number of initialization commands in the firmware file is: %d\n", cmd_no);

    dst = str_save2;
    cmd_no = 0;
    // now that all the garbage is removed, parse it
    while((cmd_str = strsep(&dst, delims)) != NULL && cmd_no < fw_panel_cmds_init_size)
    {
        if (0 == cmd_str[0]) continue;

        if (0 == strcmp(cmd_str, "SWITCH_PAGE_CMD")) // one parameter
        {
            param1_str = strsep(&dst, delims);
            if (!param1_str) break;

            fw_panel_cmds_init[cmd_no].cmd.cmd = 0xFF;
            fw_panel_cmds_init[cmd_no].cmd.data = parse_val(param1_str);
            fw_panel_cmds_init[cmd_no].delay = 0;

            ++cmd_no;
        }
        else if (0 == strcmp(cmd_str, "COMMAND_CMD")) // two parameters
        {
            param1_str = strsep(&dst, delims);
            if (!param1_str) break;
            param2_str = strsep(&dst, delims);
            if (!param2_str) break;

            fw_panel_cmds_init[cmd_no].cmd.cmd = parse_val(param1_str);
            fw_panel_cmds_init[cmd_no].cmd.data = parse_val(param2_str);
            fw_panel_cmds_init[cmd_no].delay = 0;

            ++cmd_no;
        }
        else if (0 == strcmp(cmd_str, "CMD_DELAY")) // three parameters
        {
            param1_str = strsep(&dst, delims);
            if (!param1_str) break;
            param2_str = strsep(&dst, delims);
            if (!param2_str) break;
            param3_str = strsep(&dst, delims);
            if (!param3_str) break;

            fw_panel_cmds_init[cmd_no].cmd.cmd = parse_val(param1_str);
            fw_panel_cmds_init[cmd_no].cmd.data = parse_val(param2_str);
            fw_panel_cmds_init[cmd_no].delay = parse_val(param3_str);

            ++cmd_no;
        }
    }

    kfree(str_save2);
}

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

  atomic_set(&errorFlag, 0);

  return 1;
}


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

    // mipi_dsi_generic_write could be used as well, the only difference is that mipi_dsi_generic_write allows zero parameters
    ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
    if (ret < 0)
    {
        printk(KERN_ALERT "MIPI DSI DCS write failed: %d\n", ret);

        return ret;
    }

    return 0;
}

static int send_cmd_data_read(struct hgltp08_touchscreen *ctx, u8 cmd, u8 data, void *rdata, size_t size)
{
    u8 buf[2] = { cmd, data };
    int ret;

    ret = mipi_dsi_generic_read(ctx->dsi, buf, sizeof(buf), rdata, size);
    if (ret < 0)
    {
        printk(KERN_ALERT "MIPI DSI DCS read failed: %d\n", ret);

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
        printk(KERN_ALERT "MIPI DSI DCS write failed for switching to page: %d, return code: %d\n", (int)page, ret);

        return ret;
    }

    return 0;
}

static int hgltp08_init_sequence(struct hgltp08_touchscreen *ctx, const struct panel_command* panel_cmds, int len)
{
    int i, ret
#ifdef RETRY_INIT_CMD
    ,cmdcnt
#endif
    ;

    ret = 0;

    for (i = 0; i < len; ++i)
    {
        const struct panel_command *cmd = &panel_cmds[i];

#ifdef RETRY_INIT_CMD
        cmdcnt = 0;
        do
        {
#endif

            if (cmd->cmd.cmd == 0xFF)
                ret = switch_page(ctx, cmd->cmd.data);
            else
                ret = send_cmd_data(ctx, cmd->cmd.cmd, cmd->cmd.data);

#ifdef RETRY_INIT_CMD
            if (ret) msleep(RETRY_DELAY);
            ++cmdcnt;
        }
        while (ret && cmdcnt < CMD_RETRIES);
#endif

        if (ret)
        {
            printk(KERN_ALERT "Couldn't send initialization command!\n");

            //atomic_set(&errorFlag, 1); // don't set the error flag here, the sequence is retried and if it fails several times, then it is set

            return ret;
        }

        if (cmd->delay)
            msleep(cmd->delay);
    }

    return 0;
}

static void reset_panel(struct hgltp08_touchscreen *ctx)
{
    msleep(20);

    if (ctx->gpioResetD)
        gpio_set_value_cansleep(ctx->resetPin, 0);
    msleep(20);
    if (ctx->gpioResetD)
        gpio_set_value_cansleep(ctx->resetPin, 1);

    msleep(CMD_DELAY_TIME);
}

static int hgltp08_prepare(struct drm_panel *panel)
{
    struct hgltp08_touchscreen *ctx = panel_to_ts(panel);
    struct mipi_dsi_device *dsi = ctx->dsi;
    int ret;
    int cmdcnt;
#ifdef TEST_READ
    u8 read_val;
#endif // TEST_READ

    if (ctx->prepared)
        return 0;

    printk(KERN_ALERT "Preparing!\n");

    if (!dsi)
        printk(KERN_ALERT "No DSI device!\n");

    atomic_set(&errorFlag, 0); // reset might bring it to life so clear the error flag

    cmdcnt = 0;
    do
    {
        reset_panel(ctx);

        if (fw_panel_cmds_init && fw_panel_cmds_init_size > 0)
            ret = hgltp08_init_sequence(ctx, fw_panel_cmds_init, fw_panel_cmds_init_size);
        else
            ret = hgltp08_init_sequence(ctx, panel_cmds_init, ARRAY_SIZE(panel_cmds_init));

        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt < INIT_CMDS_RETRIES);

    if (ret)
    {
        printk(KERN_ALERT "Couldn't send initialization commands!\n");

        atomic_set(&errorFlag, 1);

        return ret;
    }

    ctx->isOn = true;

#ifdef TEST_READ
    switch_page(ctx, 1);

    read_val = 0;
    ret = send_cmd_data_read(ctx, 0x34, 0x0, &read_val, 1);
    if (!ret)
    {
        printk(KERN_ALERT "Read val for dithering: %d\n", (int)read_val);
    }

    read_val = 0;
    ret = send_cmd_data_read(ctx, 0x31, 0x0, &read_val, 1);
    if (!ret)
    {
        printk(KERN_ALERT "Read val for display inversion: 0x%08x\n", (int)read_val);
    }

    switch_page(ctx, 0);
#endif // TEST_READ

    //COMMAND_CMD(0x35, 0x00) is TE on

    // also the init sequence above already does panel on and exit sleep mode, so no need to do it here again

    /*
    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt < CMD_RETRIES);

    if (ret < 0)
    {
        printk(KERN_ALERT "Couldn't set tear on!\n");

        atomic_set(&errorFlag, 1);
    }

    msleep(100);


    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt < CMD_RETRIES);



    if (ret)
    {
        printk(KERN_ALERT "Couldn't exit sleep mode!\n");

        atomic_set(&errorFlag, 1);

        return ret;
    }

    msleep(CMD_DELAY_TIME);
    */

    ctx->prepared = true;

    printk(KERN_ALERT "Prepared!\n");

    return 0;
}


static int hgltp08_unprepare(struct drm_panel *panel)
{
    struct hgltp08_touchscreen *ctx = panel_to_ts(panel);
    struct mipi_dsi_device *dsi = ctx->dsi;
    int ret, cmdcnt;

    if (!ctx->prepared)
        return 0;

    printk(KERN_ALERT "Unpreparing!\n");

#ifndef NO_ENTER_SLEEP

// disable all unnecessary blocks inside the display module except interface communication
//  msleep(20);

    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt < CMD_RETRIES);

    if (ret)
    {
        printk(KERN_WARNING "failed to enter sleep mode: %d\n", ret);
        atomic_set(&errorFlag, 1);
    }

//    msleep(CMD_DELAY_TIME);
#endif // NO_ENTER_SLEEP

    if (ctx->gpioBacklightD)
        gpio_set_value_cansleep(ctx->backlightPin, 1);

#ifndef NO_ENTER_SLEEP
    msleep(CMD_DELAY_TIME);
#endif
//    if (ctx->gpioResetD)
//        gpio_set_value_cansleep(ctx->resetPin, 0);

    ctx->prepared = false;

    printk(KERN_ALERT "Unprepared!\n");

    return ret;
}


static int hgltp08_enable(struct drm_panel *panel)
{
    struct hgltp08_touchscreen *ctx = panel_to_ts(panel);
    int ret;
    int cmdcnt;

    if (ctx->enabled)
        return 0;

    cmdcnt = 0;
    do
    {
        ret = drm_panel_prepare(panel);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt < CMD_RETRIES);

    if (ret < 0)
    {
        printk(KERN_ALERT "Couldn't prepare the panel!\n");

        atomic_set(&errorFlag, 1);

        return ret;
    }

    if (!ctx->isOn)
    {
//      msleep(CMD_DELAY_TIME);

        cmdcnt = 0;
        do
        {
            ret = mipi_dsi_dcs_set_display_on(ctx->dsi);
            if (ret) msleep(RETRY_DELAY);
            ++cmdcnt;
        }
        while (ret && cmdcnt < CMD_RETRIES);

        if (ret)
        {
            printk(KERN_ALERT "Couldn't set display on!\n");

            atomic_set(&errorFlag, 1);
        }
        else ctx->isOn = true;

        msleep(CMD_DELAY_TIME);
    }

    if (ctx->gpioBacklightD)
        gpio_set_value_cansleep(ctx->backlightPin, 1);

    ctx->enabled = true;

    printk(KERN_ALERT "Enabled!\n");

    return ret;
}

static int hgltp08_disable(struct drm_panel *panel)
{
    int cmdcnt;
    int ret;
    struct hgltp08_touchscreen *ctx = panel_to_ts(panel);

    if (!ctx->enabled)
        return 0;

    printk(KERN_ALERT "Disabling!\n");

    // display off should be enough
#ifdef SET_PIXELS_OFF
    cmdcnt = 0;
    do
    {
        ret = send_cmd_data(ctx, 0x22, 0x00);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt < CMD_RETRIES);

    if (ret)
    {
        printk(KERN_WARNING "failed to set pixels off %d\n", ret);
        atomic_set(&errorFlag, 1);
    }
#endif

#ifndef NO_ENTER_OFF
//  stop displaying the image data on the display device

#ifdef SET_PIXELS_OFF
    msleep(20);
#endif

    cmdcnt = 0;
    do
    {
        ret = mipi_dsi_dcs_set_display_off(ctx->dsi);
        if (ret) msleep(RETRY_DELAY);
        ++cmdcnt;
    }
    while (ret && cmdcnt < CMD_RETRIES);

    if (ret)
    {
        printk(KERN_ALERT "Couldn't set display off!\n");

        atomic_set(&errorFlag, 1);
    }

    ctx->isOn = false;

    msleep(CMD_DELAY_TIME);
#endif

    if (ctx->gpioBacklightD)
        gpio_set_value_cansleep(ctx->backlightPin, 0);

    ctx->enabled = false;

    printk(KERN_ALERT "Disabled!\n");

    return ret;
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
    const struct firmware* fw_entry = NULL;

    printk(KERN_ALERT "Probing!\n");

    procFile = proc_create("hgltp08", 0444, NULL, &proc_operations);

    // read overwrites of panel parameters from device tree

    prop = of_get_property(dsi->dev.of_node, "clock", NULL);
    if (prop)
    {
        default_mode.clock = be32_to_cpup(prop);
        printk(KERN_ALERT "clock set to %d\n", default_mode.clock);
    }
    else
        printk(KERN_ALERT "clock not set, using default\n");

    prop = of_get_property(dsi->dev.of_node, "hsync_start", NULL);
    if (prop)
    {
        default_mode.hsync_start = be32_to_cpup(prop);
        printk(KERN_ALERT "hsync_start set to %d\n", default_mode.hsync_start);
    }
    else
        printk(KERN_ALERT "hsync_start not set, using default\n");

    prop = of_get_property(dsi->dev.of_node, "hsync_end", NULL);
    if (prop)
    {
        default_mode.hsync_end = be32_to_cpup(prop);
        printk(KERN_ALERT "hsync_end set to %d\n", default_mode.hsync_end);
    }
    else
        printk(KERN_ALERT "hsync_end not set, using default\n");

    prop = of_get_property(dsi->dev.of_node, "htotal", NULL);
    if (prop)
    {
        default_mode.htotal = be32_to_cpup(prop);
        printk(KERN_ALERT "htotal set to %d\n", default_mode.htotal);
    }
    else
        printk(KERN_ALERT "htotal not set, using default\n");

    prop = of_get_property(dsi->dev.of_node, "vsync_start", NULL);
    if (prop)
    {
        default_mode.vsync_start = be32_to_cpup(prop);
        printk(KERN_ALERT "vsync_start set to %d\n", default_mode.vsync_start);
    }
    else
        printk(KERN_ALERT "vsync_start not set, using default\n");

    prop = of_get_property(dsi->dev.of_node, "vsync_end", NULL);
    if (prop)
    {
        default_mode.vsync_end = be32_to_cpup(prop);
        printk(KERN_ALERT "vsync_end set to %d\n", default_mode.vsync_end);
    }
    else
        printk(KERN_ALERT "vsync_end not set, using default\n");

    prop = of_get_property(dsi->dev.of_node, "vtotal", NULL);
    if (prop)
    {
        default_mode.vtotal = be32_to_cpup(prop);
        printk(KERN_ALERT "vtotal set to %d\n", default_mode.vtotal);
    }
    else
        printk(KERN_ALERT "vtotal not set, using default\n");

    // end panel

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

    //MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_CLOCK_NON_CONTINUOUS
    dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM;

    if(request_firmware(&fw_entry, "hgltp08.txt", dev) == 0)
    {
        //printk(KERN_ALERT "Firmware: %s\n", (char*)fw_entry->data);

        parse_firmware(fw_entry->data, fw_entry->size);
    }
    release_firmware(fw_entry);

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

    //dev_pm_qos_expose_flags(dev, PM_QOS_FLAG_NO_POWER_OFF);

    printk(KERN_ALERT "Probed!\n");

    return 0;
}

static int hgltp08_remove(struct mipi_dsi_device *dsi)
{
    struct hgltp08_touchscreen *ctx = mipi_dsi_get_drvdata(dsi);

    mipi_dsi_detach(dsi);
    drm_panel_remove(&ctx->base);

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

    kfree(ctx);

    if (fw_panel_cmds_init)
    {
        kfree(fw_panel_cmds_init);
        fw_panel_cmds_init = NULL;
        fw_panel_cmds_init_size = 0;
    }

    proc_remove(procFile);

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
MODULE_VERSION("1.0.25");
