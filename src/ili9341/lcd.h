#ifndef __LCD_H__
#define __LCD_H__

#include "tft_ili9341.h"

#define LCD_MAGIC 'i'
#define LCD_WRITE_CMD _IOW(LCD_MAGIC, 0, struct lcd_packet *)
#define LCD_WRITE_DATA _IOW(LCD_MAGIC, 1, struct lcd_packet *)
#define LCD_RESET _IOW(LCD_MAGIC, 2, int)
#define LCD_BACKLIGHT_CTRL _IOW(LCD_MAGIC, 3, bool)
#define LCD_DISPLAY_CTRL _IOW(LCD_MAGIC, 4, bool)
#define LCD_DRAW_H_LINE _IOW(LCD_MAGIC, 5, struct lcd_draw_line)
#define LCD_DRAW_V_LINE _IOW(LCD_MAGIC, 6, struct lcd_draw_line)
#define LCD_SET_CURSOR _IOW(LCD_MAGIC, 7, struct lcd_position)
#define LCD_WRITE_PIXEL _IOW(LCD_MAGIC, 8, struct lcd_position)
#define LCD_READ_PIXEL _IOR(LCD_MAGIC, 9, struct lcd_position)
#define LCD_SET_PARTIAL_WINDOW _IOW(LCD_MAGIC, 10, struct lcd_partial_window)
#define LCD_GET_WINDOW _IOR(LCD_MAGIC, 11, lcd_window)
#define LCD_DRAW_RECTANGLE _IOW(LCD_MAGIC, 12, struct lcd_draw_rectangle)
#define LCD_DRAW_BITMAP _IOW(LCD_MAGIC, 13, struct lcd_packet)

struct lcd_packet
{
    char __user *data;
    int len;
};

struct lcd_position
{
    uint16_t X_pos;
    uint16_t Y_pos;
};

struct lcd_window
{
    uint16_t width;
    uint16_t height;
};

struct lcd_partial_window
{
    struct lcd_position bottom_left;
    struct lcd_window partial_window;
};

struct lcd_draw_line
{
    struct lcd_position start_pos;
    uint16_t length;
    uint16_t color;
};

struct lcd_draw_rectangle
{
    struct lcd_partial_window rectangle;
    uint16_t color;
};

void lcd_reset(struct ili9341_data *ili9341);

#endif