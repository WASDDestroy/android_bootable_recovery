#ifndef GUI2_SHELL_SPLASH_H
#define GUI2_SHELL_SPLASH_H

#include "core/ui_metrics.h"
#include "lvgl.h"

namespace gui2_shell {

struct splash_options {
  lv_obj_t* layer = nullptr;
  const gui2_core::ui_metrics* metrics = nullptr;
  // Font of the status line and of the version under the artwork.
  const lv_font_t* text_font = nullptr;
  // Font of the device code, one step smaller than text_font.
  const lv_font_t* small_font = nullptr;
  const char* version = nullptr;
  const char* device = nullptr;
};

struct splash_view {
  lv_obj_t* root = nullptr;
  lv_obj_t* status = nullptr;
};

// Full-screen artwork, the loading wave and the status line under it.
splash_view create_splash(const splash_options& options);

// True once the splash has been visible long enough to be removed.
bool splash_intro_finished();

// Sets the bottom-left status line; an empty string clears it again.
void update_splash(splash_view* view, const char* text);

// Removes the splash and its animations at once, without a fade.
void dismiss_splash(splash_view* view);

}  // namespace gui2_shell

#endif  // GUI2_SHELL_SPLASH_H
