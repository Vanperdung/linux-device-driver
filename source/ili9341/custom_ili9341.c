#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>

const struct spi_device_id ili9341_id[] = 
{
    { "custom, ili9341", 0 },
    {}
};

const struct of_device_id ili9341_of_match[] = 
{
    {.compatible = "nvd, custom_ili9341"},
    {}
};

int	ili9341_probe(struct spi_device *spi);
void ili9341_remove(struct spi_device *spi);

struct spi_driver ili9341_driver = 
{
    .id_table = ili9341_id,
    .probe = ili9341_probe,
    .remove =ili9341_remove,
    .driver = 
    {
        .name = "ili9341",
        .of_match_table = ili9341_of_match,
    },
};

MODULE_DEVICE_TABLE(of, ili9341_of_match);

int	ili9341_probe(struct spi_device *spi)
{
    printk("(%s):%d is called\r\n", __func__, __LINE__);
    return 0;
}

void ili9341_remove(struct spi_device *spi)
{
    printk("(%s):%d is called\r\n", __func__, __LINE__);
}

module_spi_driver(ili9341_driver);
MODULE_DESCRIPTION("ILI9341 SPI Display Driver");
MODULE_AUTHOR("Vanperdung");
MODULE_LICENSE("GPL");