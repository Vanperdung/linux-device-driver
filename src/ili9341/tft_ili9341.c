#include "tft_ili9341.h"
#include "lcd.h"

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

int lcd_open(struct inode *, struct file *);
int lcd_close(struct inode *, struct file *);
ssize_t lcd_read(struct file *, char __user *, size_t, loff_t *);
ssize_t lcd_write(struct file *, const char __user *, size_t, loff_t *);
long lcd_ioctl(struct file *, unsigned int, unsigned long);

struct file_operations ili9341_fops =
{
    .owner = THIS_MODULE,
    .read = lcd_read,
    .write = lcd_write,
    .unlocked_ioctl = lcd_ioctl,
    .open = lcd_open,
    .release = lcd_close,
};

int spi_write_cmd(struct ili9341_data *ili9341, uint8_t *cmd, int len)
{
    struct spi_transfer trans = 
    {
        .tx_buf = cmd,
        .len = len,
    };
    struct spi_message mess;

    spi_message_init(&mess);
    spi_message_add_tail(&trans, &mess);

    gpiod_set_value(ili9341->dc, 0);

#ifndef LCD_DISABLE_DEBUG    
    printk("%s: Sending %d bytes\n", __func__, len);
    
    for (int i = 0; i < len; i++)
    {
        printk("    [%d] = 0x%02x\n", (unsigned int)(cmd[i]));
    }

    int ret = spi_sync(ili9341->spi, &mess);
    if (ret < 0)
    {
        printk("SPI sync failed, status = %d\n", ret);
    }

    return ret;
#elif
    return spi_sync(ili9341->spi, &mess);
#endif
}

int spi_write_data(struct ili9341_data *ili9341, uint8_t *data, int len)
{
    struct spi_transfer trans = 
    {
        .tx_buf = data,
        .len = len,
    };
    struct spi_message mess;

    spi_message_init(&mess);
    spi_message_add_tail(&trans, &mess);

    gpiod_set_value(ili9341->dc, 1);

#ifndef LCD_DISABLE_DEBUG    
    printk("%s: Sending %d bytes\n", __func__, len);
    
    for (int i = 0; i < len; i++)
    {
        printk("    [%d] = 0x%02x\n", (unsigned int)(data[i]));
    }

    int ret = spi_sync(ili9341->spi, &mess);
    if (ret < 0)
    {
        printk("SPI sync failed, status = %d\n", ret);
    }
#elif
    int ret = spi_sync(ili9341->spi, &mess);
    gpiod_set_value(ili9341->dc, 0);
#endif
    return ret;
}

void spi_read_data(struct ili9341_data *ili9341, uint8_t *data, int len)
{
    return;
}

int lcd_open(struct inode *inode, struct file *file)
{
#ifndef LCD_DISABLE_DEBUG    
    printk("%s\n", __func__);
#endif

    struct ili9341_data *ili9341 = container_of(inode->i_cdev, struct ili9341_data, lcd_cdev);
    if (!ili9341)
    {
        printk("%s: Cant get private data\n", __func__);
        return -ENODEV;
    }

    file->private_data = ili9341;

    return 0;
}

int lcd_close(struct inode *inode, struct file *file)
{
#ifndef LCD_DISABLE_DEBUG    
    printk("%s\n", __func__);
#endif

    return 0;
}

ssize_t lcd_read(struct file *file, char __user * buff, size_t len, loff_t *offset)
{
#ifndef LCD_DISABLE_DEBUG    
    printk("%s\n", __func__);
#endif
    return 0;
}

ssize_t lcd_write(struct file *file, const char __user *buff, size_t len, loff_t *offset)
{
#ifndef LCD_DISABLE_DEBUG    
    printk("%s\n", __func__);
#endif
    return 0;
}

long lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
#ifndef LCD_DISABLE_DEBUG    
    printk("%s\n", __func__);
#endif

    struct ili9341_data *ili9341 = file->private_data;

    switch (cmd)
    {
        case LCD_WRITE_CMD:
            printk("%s:LCD_WRITE_CMD\n", __func__);
            break;
        
        case LCD_WRITE_DATA:
            printk("%s:LCD_WRITE_CMD\n", __func__);
            break;

        case LCD_RESET:
            printk("%s:LCD_RESET\n", __func__);
            lcd_reset(ili9341);
            break;
        default:
            break;
    }
    return 0;
}

int	ili9341_probe(struct spi_device *spi)
{
    struct device *dev = &spi->dev;
    struct ili9341_data *ili9341;
    int ret;

#ifndef LCD_DISABLE_DEBUG    
    printk("%s\n", __func__);
#endif

    ili9341 = devm_kzalloc(dev, sizeof(struct ili9341_data), GFP_KERNEL);
    if (!ili9341)
    {
        pr_err("Failed to allocate memory (%s,%d)\r\n", __func__, __LINE__);
        return -ENOMEM;
    }

    ili9341->spi = spi;
    spi_set_drvdata(spi, ili9341);    

    // LCD pin configurations
    ili9341->led = devm_gpiod_get_optional(dev, "led", GPIOD_OUT_HIGH);
    if (IS_ERR(ili9341->led))
    {
        dev_err(dev, "Failed to get gpio 'led'\r\n");
        ret = PTR_ERR(ili9341->led);
        return ret;
    }

    ili9341->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ili9341->reset))
    {
        dev_err(dev, "Failed to get gpio 'reset'\r\n");
        ret = PTR_ERR(ili9341->reset);
        return ret;
    }

    ili9341->dc = devm_gpiod_get_optional(dev, "dc", GPIOD_OUT_LOW);
    if (IS_ERR(ili9341->dc))
    {
        dev_err(dev, "Failed to get gpio 'dc'\r\n");
        ret = PTR_ERR(ili9341->dc);
        return ret;
    }

    ret = alloc_chrdev_region(&ili9341->lcd_dev_num, 0, 1, "ili9341");
    if (ret < 0)
    {
        dev_err(dev, "Failed to allocate chrdev region\r\n");
        return ret;
    }

    cdev_init(&ili9341->lcd_cdev, &ili9341_fops);

    ret = cdev_add(&ili9341->lcd_cdev, ili9341->lcd_dev_num, 1);
    if (ret < 0)
    {
        dev_err(dev, "Failed to add cdev\r\n");
        unregister_chrdev_region(ili9341->lcd_dev_num, 1);
        return ret;
    }

    ili9341->lcd_class = class_create("ili9341_spi");
    if (IS_ERR(ili9341->lcd_class))
    {
        ret = PTR_ERR(ili9341->lcd_class);
        dev_err(dev, "Failed to create class\r\n");
        cdev_del(&ili9341->lcd_cdev);
        unregister_chrdev_region(ili9341->lcd_dev_num, 1);
        return ret;
    }

    ili9341->lcd_dev = device_create(ili9341->lcd_class, NULL, ili9341->lcd_dev_num, NULL, "ili9341");
    if (IS_ERR(ili9341->lcd_dev))
    {
        dev_err(dev, "Failed to create device\r\n");
        class_destroy(ili9341->lcd_class);
        cdev_del(&ili9341->lcd_cdev);
        unregister_chrdev_region(ili9341->lcd_dev_num, 1);        
        return -ENOMEM;
    }

    return 0;
}

void ili9341_remove(struct spi_device *spi)
{
#ifndef LCD_DISABLE_DEBUG    
    printk("%s\n", __func__);
#endif
}

module_spi_driver(ili9341_driver);
MODULE_DESCRIPTION("ILI9341 SPI Display Driver");
MODULE_AUTHOR("Vanperdung");
MODULE_LICENSE("GPL");