/*******************************************************************************
 * Size: 14 px
 * Bpp: 4
 * Opts: --font C:\Windows\Fonts\simhei.ttf --size 14 --bpp 4 --format lvgl --no-compress -r 0x6674-0x6674 -o D:\CCWorkspace\ESP32-S3-RLCD-Monitor\firmware\components\ui_app\font_test.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef FONT_TEST
#define FONT_TEST 1
#endif

#if FONT_TEST

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t gylph_bitmap[] = {
    /* U+6674 "晴" */
    0x0, 0x0, 0x0, 0x11, 0x1f, 0x11, 0x10, 0x1e,
    0xaa, 0xe2, 0x88, 0x8f, 0x88, 0x81, 0x1f, 0x1,
    0xf0, 0xdd, 0xdf, 0xdd, 0xa0, 0x1f, 0x1, 0xe0,
    0x0, 0xe, 0x0, 0x0, 0x1f, 0x34, 0xe9, 0xff,
    0xff, 0xff, 0xf9, 0x1f, 0xcc, 0xe0, 0x11, 0x11,
    0x11, 0x0, 0x1f, 0x1, 0xe0, 0xec, 0xbb, 0xbf,
    0x20, 0x1f, 0x1, 0xe0, 0xe7, 0x77, 0x7e, 0x20,
    0x1f, 0x99, 0xf0, 0xe5, 0x44, 0x4d, 0x20, 0x1f,
    0x23, 0xf0, 0xed, 0xdd, 0xdf, 0x20, 0x1f, 0x0,
    0x0, 0xe1, 0x0, 0xc, 0x20, 0x0, 0x0, 0x0,
    0xe1, 0x0, 0x5c, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 224, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 26228, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

/*Store all the custom data of the font*/
static lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = gylph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
lv_font_t font_test = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 12,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0)
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if FONT_TEST*/

