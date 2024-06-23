#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "version.h"

#define DRV_NAME "mcp4018_bl"

#define MCP4018_BL_MAX_BRIGHTNESS 127
#define MCP4018_BL_DEF_BRIGHTNESS 63

struct mcp4018_bl {
	struct i2c_client *client;
	struct backlight_device *bl;
	unsigned int current_brightness;
};

static int mcp4018_bl_set(struct backlight_device *bl, int brightness)
{
	struct mcp4018_bl *chip = bl_get_data(bl);
	int ret;

	if (brightness > MCP4018_BL_MAX_BRIGHTNESS)
		brightness = MCP4018_BL_MAX_BRIGHTNESS;

	ret = i2c_smbus_write_byte(chip->client, brightness);
	if (ret < 0) {
		dev_err(&chip->client->dev, "failed to set brightness (%d)",
			ret);
	} else {
		chip->current_brightness = brightness;
	}

	return ret;
}

static int mcp4018_bl_update_status(struct backlight_device *bl)
{
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	return mcp4018_bl_set(bl, brightness);
}

static int mcp4018_bl_get_brightness(struct backlight_device *bl)
{
	struct mcp4018_bl *chip = bl_get_data(bl);
	int ret;

	ret = i2c_smbus_read_byte(chip->client);
	if (ret < 0) {
		dev_err(&chip->client->dev, "failed to get brightness (%d)",
			ret);
		return ret;
	}

	chip->current_brightness = ret;

	return chip->current_brightness;
}

static const struct backlight_ops mcp4018_bl_ops = {
	.update_status = mcp4018_bl_update_status,
	.get_brightness = mcp4018_bl_get_brightness,
};

static int mcp4018_bl_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct mcp4018_bl *chip;
	struct backlight_device *bl;
	struct backlight_properties props;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(struct mcp4018_bl),
			    GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev_info(&client->dev,
		 "Sagemcom MPC4018-based backlight controller %u.%u.%u",
		 VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

	chip->client = client;
	chip->current_brightness = 0;
	i2c_set_clientdata(client, chip);

	// First try if the i2c address respond
	// this is done because of second source responding on a different i2c address.
	ret = i2c_smbus_read_byte(chip->client);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"failed to detect chip at address %02X: err (%d)",
			chip->client->addr, ret);
		return ret;
	} else {
		dev_info(&client->dev,
			 "Detected chip at address %02X:", chip->client->addr);
	}

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = MCP4018_BL_MAX_BRIGHTNESS;
	props.brightness = MCP4018_BL_DEF_BRIGHTNESS;

	bl = devm_backlight_device_register(&client->dev,
					    dev_driver_string(&client->dev),
					    &client->dev, chip, &mcp4018_bl_ops,
					    &props);
	if (IS_ERR(bl)) {
		dev_err(&client->dev, "failed to register backlight");
		return PTR_ERR(bl);
	}

	chip->bl = bl;

	backlight_update_status(chip->bl);

	return 0;
}

static int mcp4018_bl_remove(struct i2c_client *client)
{
	struct mcp4018_bl *chip = i2c_get_clientdata(client);

	chip->bl->props.brightness = 0;
	backlight_update_status(chip->bl);

	return 0;
}

/* clang-format off */
static const struct of_device_id mcp4018_bl_of_ids[] = {
	{
		.compatible = "scom,mcp4018_bl",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mcp4018_bl_of_ids);

static const struct i2c_device_id mcp4018_bl_i2c_ids[] = {
	{ DRV_NAME, 0 },
	{}
};
/* clang-format on */

MODULE_DEVICE_TABLE(i2c, mcp4018_bl_i2c_ids);

static struct i2c_driver mcp4018_bl_i2c_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(mcp4018_bl_of_ids),
	},
	.probe = mcp4018_bl_probe,
	.remove = mcp4018_bl_remove,
	.id_table = mcp4018_bl_i2c_ids,
};

module_i2c_driver(mcp4018_bl_i2c_driver);

MODULE_DESCRIPTION("MCP4018-based backlight driver");
MODULE_AUTHOR("Nguyen Van Dung");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);