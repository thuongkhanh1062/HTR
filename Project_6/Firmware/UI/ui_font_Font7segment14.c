/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --bpp 1 --size 14 --font C:/Users/nobit/SquareLine/assets/Seven Segment.ttf -o C:/Users/nobit/SquareLine/assets\ui_font_Font7segment14.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "ui.h"

#ifndef UI_FONT_FONT7SEGMENT14
#define UI_FONT_FONT7SEGMENT14 1
#endif

#if UI_FONT_FONT7SEGMENT14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0x40,

    /* U+0022 "\"" */
    0xc0,

    /* U+0023 "#" */
    0x28, 0xaf, 0xd4, 0xfd, 0x42, 0x0,

    /* U+0024 "$" */
    0x23, 0xa1, 0x8, 0x41, 0xc1, 0x8, 0x5c, 0x40,

    /* U+0025 "%" */
    0x2, 0x2, 0x44, 0xa4, 0xa8, 0x52, 0x15, 0x25,
    0x22, 0x40, 0x40,

    /* U+0026 "&" */
    0x70, 0x0, 0x0, 0x0, 0x80, 0x0, 0x0, 0x0,
    0x80, 0x0, 0x0, 0x0, 0x80, 0x0, 0x0, 0x0,
    0x71, 0xe3, 0x8f, 0xf, 0x82, 0x4, 0x10, 0x90,
    0x82, 0x4, 0x10, 0x90, 0x82, 0x4, 0x10, 0x90,
    0x82, 0x4, 0x10, 0x90, 0x72, 0x4, 0xf, 0x10,

    /* U+0028 "(" */
    0x78, 0x88, 0x88, 0x88, 0x87,

    /* U+0029 ")" */
    0xe1, 0x11, 0x11, 0x11, 0x1e,

    /* U+002A "*" */
    0x27, 0xdd, 0xf2, 0x0,

    /* U+002B "+" */
    0x10, 0x20, 0x47, 0x71, 0x2, 0x4, 0x0,

    /* U+002C "," */
    0x60,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x4, 0x20, 0x84, 0x10, 0x86, 0x10, 0x82, 0x0,

    /* U+0030 "0" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+0031 "1" */
    0xff, 0xc0,

    /* U+0032 "2" */
    0x70, 0x42, 0x17, 0x42, 0x10, 0x83, 0x80,

    /* U+0033 "3" */
    0xf0, 0x42, 0x1f, 0x4, 0x21, 0xf, 0x80,

    /* U+0034 "4" */
    0x8c, 0x63, 0x18, 0xb8, 0x21, 0x8, 0x42,

    /* U+0035 "5" */
    0x74, 0x21, 0x7, 0x4, 0x21, 0xb, 0x80,

    /* U+0036 "6" */
    0x74, 0x21, 0x7, 0x46, 0x31, 0x8b, 0x80,

    /* U+0037 "7" */
    0xf0, 0x42, 0x10, 0x84, 0x21, 0x8, 0x40,

    /* U+0038 "8" */
    0x74, 0x63, 0x17, 0x46, 0x31, 0x8b, 0x80,

    /* U+0039 "9" */
    0x74, 0x63, 0x17, 0x4, 0x21, 0xb, 0x80,

    /* U+003A ":" */
    0x90,

    /* U+003C "<" */
    0x5a, 0xaa, 0x50,

    /* U+003D "=" */
    0xfc, 0xf, 0xc0,

    /* U+003E ">" */
    0xa5, 0x55, 0xa0,

    /* U+003F "?" */
    0x70, 0x42, 0x10, 0xba, 0x10, 0x84, 0x20,

    /* U+0040 "@" */
    0x74, 0x67, 0x5a, 0xd6, 0xb2, 0x83, 0x80,

    /* U+0041 "A" */
    0x74, 0x63, 0x18, 0xba, 0x31, 0x8c, 0x62,

    /* U+0042 "B" */
    0x74, 0x63, 0x17, 0x46, 0x31, 0x8b, 0x80,

    /* U+0043 "C" */
    0x7c, 0x21, 0x8, 0x42, 0x10, 0x83, 0xc0,

    /* U+0044 "D" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+0045 "E" */
    0x7c, 0x21, 0x7, 0xc2, 0x10, 0x83, 0xc0,

    /* U+0046 "F" */
    0x7c, 0x21, 0x8, 0x3e, 0x10, 0x84, 0x20,

    /* U+0047 "G" */
    0x74, 0x21, 0x3, 0x46, 0x31, 0x8b, 0x80,

    /* U+0048 "H" */
    0x8c, 0x63, 0x18, 0xba, 0x31, 0x8c, 0x62,

    /* U+0049 "I" */
    0xff, 0xc0,

    /* U+004A "J" */
    0x8, 0x42, 0x10, 0xc6, 0x31, 0x8b, 0x80,

    /* U+004B "K" */
    0x8c, 0xad, 0x4c, 0x3a, 0x31, 0x8c, 0x62,

    /* U+004C "L" */
    0x84, 0x21, 0x8, 0x42, 0x10, 0x83, 0xc0,

    /* U+004D "M" */
    0x6d, 0x26, 0x4c, 0x99, 0x30, 0x60, 0xc1, 0x83,
    0x4,

    /* U+004E "N" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x40,

    /* U+004F "O" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+0050 "P" */
    0x74, 0x63, 0x18, 0xba, 0x10, 0x84, 0x20,

    /* U+0051 "Q" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x93, 0xc0,

    /* U+0052 "R" */
    0x74, 0x63, 0x17, 0x62, 0x96, 0x94, 0x40,

    /* U+0053 "S" */
    0x74, 0x21, 0x7, 0x4, 0x21, 0xb, 0x80,

    /* U+0054 "T" */
    0xee, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20,

    /* U+0055 "U" */
    0x8c, 0x63, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+0056 "V" */
    0x8c, 0x63, 0x18, 0xc6, 0x2a, 0x52, 0x80,

    /* U+0057 "W" */
    0x83, 0x6, 0xc, 0x18, 0x32, 0x64, 0xc9, 0x92,
    0xd8,

    /* U+0058 "X" */
    0x8c, 0x54, 0xa5, 0x29, 0x4a, 0x8c, 0x40,

    /* U+0059 "Y" */
    0x8c, 0x63, 0x18, 0xb8, 0x21, 0x8, 0x5c,

    /* U+005A "Z" */
    0xf0, 0x44, 0x20, 0x1, 0x8, 0x83, 0xc0,

    /* U+005B "[" */
    0x78, 0x88, 0x88, 0x88, 0x87,

    /* U+005C "\\" */
    0x82, 0x4, 0x10, 0x20, 0x41, 0x2, 0x8, 0x10,

    /* U+005D "]" */
    0xe1, 0x11, 0x11, 0x11, 0x1e,

    /* U+005E "^" */
    0x14, 0x18, 0xe8, 0x4,

    /* U+005F "_" */
    0xfe,

    /* U+0061 "a" */
    0x74, 0x63, 0x18, 0xba, 0x31, 0x8c, 0x62,

    /* U+0062 "b" */
    0x74, 0x63, 0x17, 0x46, 0x31, 0x8b, 0x80,

    /* U+0063 "c" */
    0x7c, 0x21, 0x8, 0x42, 0x10, 0x83, 0xc0,

    /* U+0064 "d" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+0065 "e" */
    0x7c, 0x21, 0x7, 0xc2, 0x10, 0x83, 0xc0,

    /* U+0066 "f" */
    0x7c, 0x21, 0x8, 0x3e, 0x10, 0x84, 0x20,

    /* U+0067 "g" */
    0x74, 0x21, 0x3, 0x46, 0x31, 0x8b, 0x80,

    /* U+0068 "h" */
    0x8c, 0x63, 0x18, 0xba, 0x31, 0x8c, 0x62,

    /* U+0069 "i" */
    0xff, 0xc0,

    /* U+006A "j" */
    0x8, 0x42, 0x10, 0xc6, 0x31, 0x8b, 0x80,

    /* U+006B "k" */
    0x8c, 0xad, 0x4c, 0x3a, 0x31, 0x8c, 0x62,

    /* U+006C "l" */
    0x84, 0x21, 0x8, 0x42, 0x10, 0x83, 0xc0,

    /* U+006D "m" */
    0x6d, 0x26, 0x4c, 0x99, 0x30, 0x60, 0xc1, 0x83,
    0x4,

    /* U+006E "n" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x40,

    /* U+006F "o" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+0070 "p" */
    0x74, 0x63, 0x18, 0xba, 0x10, 0x84, 0x20,

    /* U+0071 "q" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x93, 0xc0,

    /* U+0072 "r" */
    0x74, 0x63, 0x17, 0x62, 0x96, 0x94, 0x40,

    /* U+0073 "s" */
    0x74, 0x21, 0x7, 0x4, 0x21, 0xb, 0x80,

    /* U+0074 "t" */
    0xee, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20,

    /* U+0075 "u" */
    0x8c, 0x63, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+0076 "v" */
    0x8c, 0x63, 0x18, 0xc6, 0x2a, 0x52, 0x80,

    /* U+0077 "w" */
    0x83, 0x6, 0xc, 0x18, 0x32, 0x64, 0xc9, 0x92,
    0xd8,

    /* U+0078 "x" */
    0x8c, 0x54, 0xa5, 0x29, 0x4a, 0x8c, 0x40,

    /* U+0079 "y" */
    0x8c, 0x63, 0x18, 0xb8, 0x21, 0x8, 0x5c,

    /* U+007A "z" */
    0xf0, 0x44, 0x20, 0x1, 0x8, 0x83, 0xc0,

    /* U+007B "{" */
    0x3a, 0x10, 0x88, 0x21, 0x8, 0x41, 0xc0,

    /* U+007C "|" */
    0xff, 0xe0,

    /* U+007D "}" */
    0xe0, 0x84, 0x20, 0x88, 0x42, 0x17, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 67, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 41, .box_w = 1, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 63, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 4, .adv_w = 122, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 10, .adv_w = 112, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 18, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 29, .adv_w = 530, .box_w = 32, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 85, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 74, .adv_w = 85, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 79, .adv_w = 92, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 7},
    {.bitmap_index = 83, .adv_w = 148, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 90, .adv_w = 52, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 91, .adv_w = 91, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 92, .adv_w = 41, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 121, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 101, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 41, .box_w = 1, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 138, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 145, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 41, .box_w = 1, .box_h = 4, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 167, .adv_w = 65, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 170, .adv_w = 123, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 173, .adv_w = 65, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 183, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 190, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 197, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 211, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 218, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 232, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 41, .box_w = 1, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 248, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 255, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 262, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 153, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 285, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 292, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 299, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 306, .adv_w = 113, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 313, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 131, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 336, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 153, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 352, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 359, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 366, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 373, .adv_w = 85, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 378, .adv_w = 121, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 386, .adv_w = 85, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 391, .adv_w = 184, .box_w = 10, .box_h = 3, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 395, .adv_w = 140, .box_w = 7, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 396, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 403, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 417, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 424, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 431, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 438, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 445, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 41, .box_w = 1, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 454, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 468, .adv_w = 102, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 475, .adv_w = 153, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 484, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 491, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 498, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 505, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 512, .adv_w = 113, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 519, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 526, .adv_w = 131, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 535, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 542, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 549, .adv_w = 153, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 558, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 565, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 572, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 579, .adv_w = 108, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 586, .adv_w = 41, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 588, .adv_w = 108, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 7, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 40, .range_length = 19, .glyph_id_start = 8,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 60, .range_length = 36, .glyph_id_start = 27,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 97, .range_length = 29, .glyph_id_start = 63,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 4,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_Font7segment14 = {
#else
lv_font_t ui_font_Font7segment14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -5,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_FONT7SEGMENT14*/

