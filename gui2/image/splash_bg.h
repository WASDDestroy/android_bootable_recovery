#ifndef GUI2_IMAGE_SPLASH_BG_H
#define GUI2_IMAGE_SPLASH_BG_H

#include "lvgl.h"

// The boot artwork, generated from image/splash_bg.png by gen_splash_bg.py.
// The pixels are RGB888 in .rodata and lv_image draws them in place, so the
// splash needs no image decoder and no allocation for its background.
extern const lv_image_dsc_t splash_bg;

#endif  // GUI2_IMAGE_SPLASH_BG_H
