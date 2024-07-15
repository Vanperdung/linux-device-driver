#include "tft_ili9341.h"
#include "lcd.h"

void lcd_reset(struct ili9341_data *ili9341)
{
    gpiod_set_value(ili9341->reset, 0);
    msleep(20);
    gpiod_set_value(ili9341->reset, 1);
    msleep(200);
}
