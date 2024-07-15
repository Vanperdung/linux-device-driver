#ifndef __ILI9341_H__
#define __ILI9341_H__

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>

struct ili9341_data
{
    struct spi_device *spi;
    struct device *lcd_dev;
    struct gpio_desc *dc;
    struct gpio_desc *reset;
    struct gpio_desc *led;
    dev_t lcd_dev_num;
    struct cdev lcd_cdev;
    struct class *lcd_class;
};

int spi_write_cmd(struct ili9341_data *ili9341, uint8_t *cmd, int len);
int spi_write_data(struct ili9341_data *ili9341, uint8_t *data, int len);
void spi_read_data(struct ili9341_data *ili9341, uint8_t *data, int len);

#endif