/**
 * @file lv_conf.h
 * Configuration file for LVGL v9.2.2 (ESP32-P4 720x720 Powerwall 2 Simulator)
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 32

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DEF_REFR_PERIOD  16      /* 60 FPS */
#define LV_DPI_DEF          130

#define LV_USE_OS           LV_OS_NONE

/*========================
 * RENDERING CONFIGURATION
 *========================*/
#define LV_DRAW_BUF_STRIDE_ALIGN                1
#define LV_DRAW_BUF_ALIGN                       4
#define LV_USE_DRAW_SW                          1

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/
#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1

/*==================
 *   FONT USAGE
 *===================*/
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/*==================
 * WIDGET USAGE
 *================*/
#define LV_WIDGETS_HAS_DEFAULT_VALUE  1
#define LV_USE_ANIMIMG          1
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BUTTON           1
#define LV_USE_BUTTONMATRIX     1
#define LV_USE_CALENDAR         0
#define LV_USE_CANVAS           1
#define LV_USE_CHART            1
#define LV_USE_CHECKBOX         1
#define LV_USE_DROPDOWN         1
#define LV_USE_IMAGE            1
#define LV_USE_IMAGEBUTTON      1
#define LV_USE_KEYBOARD         0
#define LV_USE_LABEL            1
#define LV_USE_LED              1
#define LV_USE_LINE             1
#define LV_USE_LIST             1
#define LV_USE_MENU             0
#define LV_USE_MSGBOX           1
#define LV_USE_ROLLER           0
#define LV_USE_SCALE            1
#define LV_USE_SLIDER           1
#define LV_USE_SPAN             1
#define LV_USE_SPINBOX          0
#define LV_USE_SPINNER          1
#define LV_USE_SWITCH           1
#define LV_USE_TEXTAREA         1
#define LV_USE_TABLE            0
#define LV_USE_TABVIEW          0
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              0

/*==================
 * THEMES & LAYOUTS
 *==================*/
#define LV_USE_THEME_DEFAULT    1
#define LV_USE_THEME_SIMPLE     1
#define LV_USE_FLEX             1
#define LV_USE_GRID             1

/*==================
 * LIBS & DRIVERS
 *==================*/
#define LV_USE_FS_STDIO         1
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER      'A'
    #define LV_FS_STDIO_PATH        ""
    #define LV_FS_STDIO_CACHE_SIZE  0
#endif

#define LV_USE_LODEPNG          1

#define LV_USE_FREETYPE         1
#if LV_USE_FREETYPE
    #define LV_FREETYPE_USE_LVGL_PORT 0
    #define LV_FREETYPE_CACHE_FT_GLYPH_CNT 4096
#endif

#define LV_USE_SDL              1
#if LV_USE_SDL
    #define LV_SDL_INCLUDE_PATH     <SDL2/SDL.h>
    #define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_DIRECT
    #define LV_SDL_BUF_COUNT        1
    #define LV_SDL_ACCELERATED      1
    #define LV_SDL_FULLSCREEN       0
    #define LV_SDL_DIRECT_EXIT      1
    #define LV_SDL_MOUSEWHEEL_MODE  LV_SDL_MOUSEWHEEL_MODE_ENCODER
#endif

#endif /* LV_CONF_H */
