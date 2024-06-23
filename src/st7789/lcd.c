#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <video/mipi_display.h>

#include "version.h"

#define DRV_NAME "st7789vfb"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define SCREEN_BPP 16
#define SCREEN_FPS 24

static bool init = true;
module_param(init, bool, 0);
MODULE_PARM_DESC(init, "Set to zero to bypass chip initialization");

enum st7789vfb_cmd {
	PORCTRL = 0xB2,
	GCTRL = 0xB7,
	VCOMS = 0xBB,
	LCM = 0xC0,
	VDVVRHEN = 0xC2,
	VRHS = 0xC3,
	VDVS = 0xC4,
	VCMOFSET = 0xC5,
	FRCTRL = 0xC6,
	PWCTRL1 = 0xD0,
	PVGAMCTRL = 0xE0,
	NVGAMCTRL = 0xE1,
};

struct st7789vfb_par {
	struct gpio_desc *pin_bl;
	struct gpio_desc *pin_dc;
	struct gpio_desc *pin_pwr;
	struct gpio_desc *pin_rst;
	struct fb_info *info;
	struct spi_device *spi;
	bool bl_status;
};

enum st7789vfb_kind {
	ST7789VFB_CMD = 0,
	ST7789VFB_DATA = 1,
};

static ssize_t bl_status_show(struct device *dev, struct device_attribute *attr,
			      char *buff)
{
	struct fb_info *info = dev_get_drvdata(dev);
	struct st7789vfb_par *par = info->par;
	return snprintf(buff, PAGE_SIZE, "%u\n", par->bl_status);
}

static ssize_t bl_status_store(struct device *dev,
			       struct device_attribute *attr, const char *buff,
			       size_t count)
{
	struct fb_info *info = dev_get_drvdata(dev);
	struct st7789vfb_par *par = info->par;
	uint tmp = 0;

	if (count > 0)
		sscanf(buff, "%u", &tmp);
	else
		return count;

	par->bl_status = (tmp == 1) ? true : false;

	if (!IS_ERR_OR_NULL(par->pin_bl)) {
		gpiod_set_value(par->pin_bl, par->bl_status ? 1 : 0);
	}
	dev_info(dev, "setting gpio to %d", par->bl_status ? 1 : 0);

	return count;
}

static DEVICE_ATTR_RW(bl_status);

static void st7789vfb_send(struct st7789vfb_par *par, enum st7789vfb_kind kind,
			   u8 *data, size_t len)
{
	struct spi_message msg;
	int state;
	int status;

	struct spi_transfer xfer = {
		.tx_buf = data,
		.len = len,
		.bits_per_word = 8,
	};

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	state = (kind == ST7789VFB_CMD) ? 0 : 1;

	gpiod_set_value(par->pin_dc, state);

	if (len > 1) {
		dev_dbg(par->info->device,
			"%s: sending %zu bytes (0x%02x 0x%02x 0x%02x 0x%02x)",
			__func__, len, data[0], data[1], data[2], data[3]);
	} else {
		dev_dbg(par->info->device, "%s: sending 0x%02x", __func__,
			*data);
	}

	status = spi_sync(par->spi, &msg);
	if (status < 0) {
		dev_err(par->info->device, "SPI sync failed (%d)", status);
	}
}

static inline void st7789vfb_send_data(struct st7789vfb_par *par, u8 *data,
				       size_t len)
{
	st7789vfb_send(par, ST7789VFB_DATA, data, len);
}

static inline void st7789vfb_send_cmd(struct st7789vfb_par *par, u8 cmd)
{
	st7789vfb_send(par, ST7789VFB_CMD, &cmd, 1);
}

static void st7789vfb_set_addr_win(struct st7789vfb_par *par, int xs, int ys,
				   int xe, int ye)
{
	u8 data[4];

	dev_dbg(par->info->device, "%s: xs=%d, ys=%d, xe=%d, ye=%d", __func__,
		xs, ys, xe, ye);
	st7789vfb_send_cmd(par, MIPI_DCS_SET_COLUMN_ADDRESS);
	data[0] = (xs >> 8) & 0xFF;
	data[1] = xs & 0xFF;
	data[2] = (xe >> 8) & 0xFF;
	data[3] = xe & 0xFF;
	st7789vfb_send_data(par, data, 4);

	st7789vfb_send_cmd(par, MIPI_DCS_SET_PAGE_ADDRESS);
	data[0] = (ys >> 8) & 0xFF;
	data[1] = ys & 0xFF;
	data[2] = (ye >> 8) & 0xFF;
	data[3] = ye & 0xFF;
	st7789vfb_send_data(par, data, 4);

	st7789vfb_send_cmd(par, MIPI_DCS_WRITE_MEMORY_START);
}

static int st7789vfb_write_vmem(struct st7789vfb_par *par, size_t offset,
				size_t len)
{
	u8 *vmem;
	u16 *tmp;
	size_t remain;
	size_t max_size;
	size_t amount;
	int i;

	/* Some controller can restrict the max transfer size
	 * in that case, split message in chunk
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	if (par->spi->master->max_transfer_size != NULL) {
		max_size = par->spi->master->max_transfer_size(par->spi);
	} else {
		max_size = len;
	}
#else
	max_size = len;
#endif
	remain = len;

	dev_dbg(par->info->device, "%s: offset=%zu len=%zu, max_size=%zu",
		__func__, offset, len, max_size);
	vmem = (u8 *)(par->info->screen_base + offset);

	tmp = (u16 *)(par->info->screen_base + offset);
	for (i = 0; i < len / 2; i++) {
		tmp[i] = cpu_to_be16(tmp[i]);
	}

	while (remain) {
		amount = min(max_size, remain);
		st7789vfb_send_data(par, vmem, amount);
		vmem += amount;
		remain -= amount;
	}

	return 0;
}

static void st7789vfb_update_display(struct st7789vfb_par *par,
				     unsigned int start_line,
				     unsigned int end_line)
{
	size_t offset, len;
	int err;

	dev_dbg(par->info->device, "%s: start_line=%u end_line=%u", __func__,
		start_line, end_line);

	if (start_line > end_line) {
		dev_warn(par->info->device,
			 "start_line=%u is larger than end_line=%u", start_line,
			 end_line);
		start_line = 0;
		end_line = par->info->var.yres - 1;
	}

	if (start_line > par->info->var.yres - 1 ||
	    end_line > par->info->var.yres - 1) {
		dev_warn(par->info->device,
			 "start_line=%u or end_line=%u is larger than max=%d",
			 start_line, end_line, par->info->var.yres - 1);
		start_line = 0;
		end_line = par->info->var.yres - 1;
	}

	st7789vfb_set_addr_win(par, 0, start_line, par->info->var.xres - 1,
			       end_line);
	offset = start_line * par->info->fix.line_length;
	len = (end_line - start_line + 1) * par->info->fix.line_length;
	err = st7789vfb_write_vmem(par, offset, len);
	if (err < 0) {
		dev_err(par->info->device, "Failed to write vmem");
	}
}

static char st7789vfb_pvgamctrl_data[] = {
	0xf0, 0x0c, 0x15, 0x0d, 0x0d, 0x2a, 0x3b,
	0x5c, 0x4b, 0x3b, 0x17, 0x15, 0x1c, 0x1f,
};

static char st7789vfb_nvgamctrl_data[] = {
	0xf0, 0x0c, 0x15, 0x0d, 0x0d, 0x2a, 0x3b,
	0x3c, 0x4b, 0x3b, 0x17, 0x15, 0x1c, 0x1f,
};

static int st7789vfb_setup_display(struct st7789vfb_par *par)
{
	u8 data[6];

	if (!init) {
		return 0;
	}

	gpiod_set_value(par->pin_rst, 1);
	msleep(30);
	gpiod_set_value(par->pin_rst, 0);
	msleep(120);

	st7789vfb_send_cmd(par, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(120);

	st7789vfb_send_cmd(par, MIPI_DCS_SET_ADDRESS_MODE);
	data[0] = 0;
	st7789vfb_send_data(par, data, 1);

	st7789vfb_send_cmd(par, MIPI_DCS_SET_PIXEL_FORMAT);
	data[0] = MIPI_DCS_PIXEL_FMT_16BIT;
	st7789vfb_send_data(par, data, 1);

	st7789vfb_send_cmd(par, PORCTRL);
	data[0] = 0x0c;
	data[1] = 0x0c;
	data[2] = 0x00;
	data[3] = 0x33;
	data[4] = 0x33;
	st7789vfb_send_data(par, data, 5);

	st7789vfb_send_cmd(par, GCTRL);
	data[0] = 0x35;
	st7789vfb_send_data(par, data, 1);

	st7789vfb_send_cmd(par, LCM);
	data[0] = 0x2c;
	st7789vfb_send_data(par, data, 1);

	st7789vfb_send_cmd(par, VDVVRHEN);
	data[0] = 0x01;
	st7789vfb_send_data(par, data, 1);

	st7789vfb_send_cmd(par, VRHS);
	data[0] = 0x1b;
	st7789vfb_send_data(par, data, 1);

	st7789vfb_send_cmd(par, VDVS);
	data[0] = 0x20;
	st7789vfb_send_data(par, data, 1);

	st7789vfb_send_cmd(par, FRCTRL);
	data[0] = 0x0f;
	st7789vfb_send_data(par, data, 1);

	st7789vfb_send_cmd(par, PWCTRL1);
	data[0] = 0xa4;
	data[1] = 0x71;
	st7789vfb_send_data(par, data, 2);

	st7789vfb_send_cmd(par, PVGAMCTRL);
	st7789vfb_send_data(par, st7789vfb_pvgamctrl_data, 14);

	st7789vfb_send_cmd(par, NVGAMCTRL);
	st7789vfb_send_data(par, st7789vfb_nvgamctrl_data, 14);

	st7789vfb_send_cmd(par, MIPI_DCS_SET_DISPLAY_ON);

	if (!IS_ERR_OR_NULL(par->pin_bl)) {
		gpiod_set_value(par->pin_bl, 1);
	}

	return 0;
}

static void st7789vfb_teardown_display(struct st7789vfb_par *par)
{
	st7789vfb_send_cmd(par, MIPI_DCS_SET_DISPLAY_OFF);
	st7789vfb_send_cmd(par, MIPI_DCS_ENTER_SLEEP_MODE);
	if (!IS_ERR_OR_NULL(par->pin_bl)) {
		gpiod_set_value(par->pin_bl, 0);
	}
	gpiod_set_value(par->pin_pwr, 0);
}

static ssize_t st7789vfb_write(struct fb_info *info, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	unsigned long total_size;
	unsigned long p = *ppos;
	unsigned int start_line;
	unsigned int end_line;
	u8 __iomem *dst;

	total_size = info->fix.smem_len;

	if (p > total_size)
		return -EINVAL;

	if (count + p > total_size)
		count = total_size - p;

	if (!count)
		return -EINVAL;

	dst = (void __force *)(info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		return -EFAULT;

	dev_dbg(info->device, "%s: writing %zu bytes at %lld", __func__, count,
		*ppos);

	start_line = p / info->fix.line_length;
	end_line = (p + count) / info->fix.line_length - 1;

	st7789vfb_update_display(info->par, start_line, end_line);

	*ppos += count;

	return count;
}

static int st7789vfb_blank(int blank, struct fb_info *info)
{
	struct st7789vfb_par *par = info->par;
	bool backlighted = true;
	u8 cmds[2][2] = {
		{ MIPI_DCS_SET_DISPLAY_OFF, MIPI_DCS_ENTER_SLEEP_MODE },
		{ MIPI_DCS_SET_DISPLAY_ON, MIPI_DCS_EXIT_SLEEP_MODE },
	};
	u8(*cmd)[2] = NULL;
	int i;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		backlighted = false;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
		cmd = &cmds[0];
		break;
	case FB_BLANK_UNBLANK:
		cmd = &cmds[1];
		break;
	default:
		return -1;
		break;
	}
	if (!IS_ERR_OR_NULL(par->pin_bl)) {
		gpiod_set_value(par->pin_bl, backlighted ? 1 : 0);
	}

	for (i = 0; i < 2; i++) {
		st7789vfb_send_cmd(par, (*cmd)[i]);
	}

	return 0;
}

static void st7789vfb_deferred_io(struct fb_info *info,
				  struct list_head *pagelist)
{
	st7789vfb_update_display(info->par, 0, SCREEN_HEIGHT - 1);
}

static struct fb_deferred_io st7789vfb_defio = {
	.deferred_io = st7789vfb_deferred_io,
};

static struct fb_ops st7789vfb_ops = {
	.owner = THIS_MODULE,
	.fb_write = st7789vfb_write,
	.fb_blank = st7789vfb_blank,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit,
};

static struct fb_var_screeninfo st7789vfb_var_screeninfo = {
	.xres = SCREEN_WIDTH,
	.yres = SCREEN_HEIGHT,
	.xres_virtual = SCREEN_WIDTH,
	.yres_virtual = SCREEN_HEIGHT,
	.bits_per_pixel = SCREEN_BPP,
	.nonstd = 1,
	/* RGB565 */
	.red = { .offset = 11, .length = 5 },
	.green = { .offset = 5, .length = 6 },
	.blue = { .offset = 0, .length = 5 },
	.transp = { .offset = 0, .length = 0 },
};

static struct fb_fix_screeninfo st7789vfb_fix_screeninfo = {

	.id = DRV_NAME,
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_PSEUDOCOLOR,
	.xpanstep = 0,
	.ypanstep = 0,
	.ywrapstep = 0,
	.line_length = SCREEN_WIDTH * SCREEN_BPP / 8,
	.accel = FB_ACCEL_NONE,
};

static int st7789vfb_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct st7789vfb_par *par = NULL;
	struct fb_info *info = NULL;
	u8 *vmem = NULL;
	int vmem_size = 0;
	int err = 0;

	dev_dbg(dev, "%s\n", __func__);

	spi->mode = SPI_MODE_3;
	err = spi_setup(spi);
	if (err < 0)
		return err;

	vmem_size = SCREEN_HEIGHT * SCREEN_WIDTH * SCREEN_BPP / 8;
	vmem = vzalloc(vmem_size);
	if (!vmem) {
		return -ENOMEM;
	}

	info = framebuffer_alloc(sizeof(*par), dev);
	if (!info) {
		dev_err(&spi->dev, "Couldn't allocate framebuffer\n");
		err = -ENOMEM;
		goto fb_alloc_error;
	}

	par = info->par;
	dev_info(
		dev,
		"Sagemcom fbdev driver for Sitronix st7789v on bcm63xx %u.%u.%u",
		VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;
	info->fbops = &st7789vfb_ops;
	info->var = st7789vfb_var_screeninfo;
	info->fix = st7789vfb_fix_screeninfo;

	info->fbdefio = &st7789vfb_defio;
	info->fbdefio->delay = HZ / SCREEN_FPS;

	info->screen_base = vmem;
	info->fix.smem_start = (unsigned long)vmem;
	info->fix.smem_len = vmem_size;

	fb_deferred_io_init(info);

	par->info = info;

	err = register_framebuffer(info);
	if (err) {
		dev_err(&spi->dev, "Could not register the framebuffer\n");
		goto fb_register_error;
	}

	par->pin_bl = devm_gpiod_get_index(dev, "backlight", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(par->pin_bl)) {
		dev_err(dev, "Fail to request backlight GPIO or already requested");
	}

	par->pin_pwr = devm_gpiod_get_index(dev, "power", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(par->pin_pwr)) {
		dev_err(dev, "Fail to request power GPIO");
		err = PTR_ERR(par->pin_pwr);
		goto error;
	}

	par->pin_rst = devm_gpiod_get_index(dev, "reset", 0, GPIOD_OUT_LOW);
	if (IS_ERR(par->pin_rst)) {
		dev_err(dev, "Fail to request reset GPIO");
		err = PTR_ERR(par->pin_rst);
		goto error;
	}

	par->pin_dc = devm_gpiod_get_index(dev, "datacom", 0, GPIOD_OUT_LOW);
	if (IS_ERR(par->pin_dc)) {
		dev_err(dev, "Fail to request datacom GPIO");
		err = PTR_ERR(par->pin_dc);
		goto error;
	}

	par->spi = spi;

	err = st7789vfb_setup_display(par);
	if (err < 0) {
		dev_err(dev, "failed to setup display");
		goto error;
	}

	dev_set_drvdata(dev, info);

	par->bl_status = 0;
	if (!IS_ERR_OR_NULL(par->pin_bl)) {
		err = device_create_file(dev, &dev_attr_bl_status);
		if (err) {
			dev_err(dev, "Fail to add bl_status file\n");
			goto error;
		}
	}

	return 0;

error:
	unregister_framebuffer(info);
fb_register_error:
	fb_deferred_io_cleanup(info);
	framebuffer_release(info);
fb_alloc_error:
	vfree(vmem);

	return err;
}

static int st7789vfb_remove(struct spi_device *spi)
{
	struct fb_info *info = dev_get_drvdata(&spi->dev);
	struct st7789vfb_par *par = info->par;

	if (!IS_ERR_OR_NULL(par->pin_bl)) {
		device_remove_file(&spi->dev, &dev_attr_bl_status);
	}
	st7789vfb_teardown_display(par);

	unregister_framebuffer(info);
	fb_deferred_io_cleanup(info);
	vfree(info->screen_base);
	framebuffer_release(info);

	dev_info(&spi->dev, "Driver st7789bfb uninitialized correctly");

	return 0;
}

static const struct spi_device_id st7789vfb_ids[] = {
	{ "st7789bfb", 0 },
	{},
};
MODULE_DEVICE_TABLE(spi, st7789vfb_ids);

#ifdef CONFIG_OF
static const struct of_device_id st7789vfb_match[] = {
	{
		.compatible = "scom,st7789vfb",
	},
	{},
};
MODULE_DEVICE_TABLE(of, st7789vfb_match);
#endif

static struct spi_driver st7789vfb_driver = {
	.driver =
		{
			.name = DRV_NAME,
			.of_match_table = st7789vfb_match,
		},
	.probe = st7789vfb_probe,
	.remove = st7789vfb_remove,
};
module_spi_driver(st7789vfb_driver);

MODULE_DESCRIPTION("Sitronix ST7789V framebuffer device (for bcm63xx)");
