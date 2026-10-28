#include "shell/splash.h"

#include <algorithm>
#include <cstring>

#include "core/ui_helpers.h"
#include "image/splash_bg.h"

namespace gui2_shell {

namespace {

// The loading wave is SpinKit's "wave" spinner: five bars that stretch to full
// height and shrink back, one tenth of the cycle apart. Every height below is
// in thousandths of a bar's full height, the way the CSS uses scaleY(0.4..1).
constexpr int kBarCount = 5;
constexpr uint32_t kCycleMs = 1200;
constexpr uint32_t kStaggerMs = 100;
constexpr int32_t kWaveLow = 400;
constexpr int32_t kWaveHigh = 1000;

// A splash that comes and goes in a blink reads as a glitch.
constexpr uint32_t kMinVisibleMs = 800;

constexpr uint32_t kBarColor = 0x0090CA;
constexpr uint32_t kStatusColor = 0x8E8E93;
constexpr uint32_t kDimText = 0x6E6E73;

int bar_base_height = 0;
int bar_bottom_y = 0;
uint32_t splash_started_ms = 0;

// CSS `ease-in-out` is cubic-bezier(.42, 0, .58, 1); LVGL has the same curve,
// in 0..1024 fixed point.
int32_t ease_in_out(int32_t progress) {
  return lv_cubic_bezier(progress, LV_BEZIER_VAL_FLOAT(0.42), 0, LV_BEZIER_VAL_FLOAT(0.58),
                         LV_BEZIER_VAL_FLOAT(1));
}

// 0%, 40% and 100% hold at 0.4, 20% reaches 1.0, and each segment is eased.
// The long hold returns the same value every tick, so LVGL skips the redraw.
int32_t wave_path(const lv_anim_t* anim) {
  const int32_t phase = lv_map(anim->act_time, 0, static_cast<int32_t>(anim->duration), 0, 1024);
  if (phase < 205) {
    return kWaveLow + ease_in_out(lv_map(phase, 0, 205, 0, 1024)) * (kWaveHigh - kWaveLow) / 1024;
  }
  if (phase < 410) {
    return kWaveHigh -
           ease_in_out(lv_map(phase, 205, 410, 0, 1024)) * (kWaveHigh - kWaveLow) / 1024;
  }
  return kWaveLow;
}

// The bars grow upwards from the credit line, so the bottom edge they are
// aligned to never moves.
void set_bar_height(void* var, int32_t permille) {
  lv_obj_t* bar = static_cast<lv_obj_t*>(var);
  const int height = std::max(1, bar_base_height * static_cast<int>(permille) / 1000);
  lv_obj_set_height(bar, height);
  lv_obj_set_y(bar, bar_bottom_y - height);
}

lv_obj_t* create_bar(lv_obj_t* parent, int x, int y, int width, int height) {
  lv_obj_t* bar = lv_obj_create(parent);
  lv_obj_set_pos(bar, x, y);
  lv_obj_set_size(bar, width, height);
  gui2_core::set_surface_style(bar, lv_color_hex(kBarColor), LV_OPA_COVER);
  lv_obj_set_style_radius(bar, width / 2, LV_PART_MAIN);
  lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
  lv_obj_set_clickable(bar, false);
  gui2_core::disable_scrolling(bar);
  return bar;
}

void start_wave(lv_obj_t* bar, uint32_t delay_ms) {
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, bar);
  lv_anim_set_exec_cb(&anim, set_bar_height);
  lv_anim_set_values(&anim, kWaveLow, kWaveHigh);
  lv_anim_set_duration(&anim, kCycleMs);
  lv_anim_set_delay(&anim, delay_ms);
  lv_anim_set_path_cb(&anim, wave_path);
  lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_early_apply(&anim, true);
  lv_anim_start(&anim);
}

}  // namespace

splash_view create_splash(const splash_options& options) {
  splash_view view;
  if (options.layer == nullptr || options.metrics == nullptr || options.text_font == nullptr ||
      options.small_font == nullptr)
    return view;

  const auto& metrics = *options.metrics;

  view.root = lv_obj_create(options.layer);
  lv_obj_set_pos(view.root, 0, 0);
  lv_obj_set_size(view.root, metrics.width, metrics.height);
  gui2_core::set_surface_style(view.root, lv_color_hex(0x000000), LV_OPA_COVER);
  lv_obj_set_style_radius(view.root, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(view.root, 0, LV_PART_MAIN);
  gui2_core::disable_scrolling(view.root);
  lv_obj_set_clickable(view.root, true);

  // The artwork is cut for this panel. Anywhere else it is scaled to cover
  // instead of sitting in a corner.
  lv_obj_t* artwork = lv_image_create(view.root);
  lv_image_set_src(artwork, &splash_bg);
  lv_obj_set_clickable(artwork, false);
  if (splash_bg.header.w == metrics.width && splash_bg.header.h == metrics.height) {
    lv_obj_set_pos(artwork, 0, 0);
  } else {
    const int32_t scale_w = metrics.width * LV_SCALE_NONE / splash_bg.header.w;
    const int32_t scale_h = metrics.height * LV_SCALE_NONE / splash_bg.header.h;
    lv_image_set_scale(artwork, std::max(scale_w, scale_h));
    lv_obj_center(artwork);
  }

  // The wave sits in the bottom-left corner, standing on the credit line: its
  // bottom edge is level with the bottom of "Based on ...", and the bars grow
  // upwards from there. The left edge lines up with the wordmark's margin.
  const int margin = gui2_core::ui_px(44);
  const int bar_height = gui2_core::ui_px(72);
  const int bar_width = std::max(2, bar_height * 6 / 40);
  const int gap = bar_height * 5 / 40;
  bar_base_height = bar_height;
  const int credit_bottom = metrics.height - gui2_core::ui_px(120);
  bar_bottom_y = credit_bottom;

  // Every bar starts at the shrunken height the cycle holds for most of its
  // time, so the first frames look the same as the CSS ones.
  const int rest_height = std::max(1, bar_height * kWaveLow / 1000);
  for (int i = 0; i < kBarCount; ++i) {
    const int x = margin + i * (bar_width + gap);
    lv_obj_t* bar = create_bar(view.root, x, bar_bottom_y - rest_height, bar_width, rest_height);
    start_wave(bar, static_cast<uint32_t>(i) * kStaggerMs);
  }

  // Version and device code, centred on the screen's axis under the credit
  // line, the way the stock splash showed them.
  lv_obj_t* meta = lv_obj_create(view.root);
  lv_obj_set_size(meta, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  gui2_core::set_surface_style(meta, lv_color_hex(0x000000), LV_OPA_TRANSP);
  lv_obj_set_style_pad_all(meta, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(meta, gui2_core::ui_px(6), LV_PART_MAIN);
  lv_obj_set_layout(meta, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(meta, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(meta, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  gui2_core::disable_scrolling(meta);
  lv_obj_set_clickable(meta, false);
  lv_obj_align(meta, LV_ALIGN_TOP_MID, 0, credit_bottom + gui2_core::ui_px(20));

  lv_obj_t* version = lv_label_create(meta);
  lv_label_set_text(version, options.version == nullptr ? "" : options.version);
  lv_obj_set_style_text_font(version, options.text_font, LV_PART_MAIN);
  lv_obj_set_style_text_color(version, lv_color_hex(kStatusColor), LV_PART_MAIN);

  lv_obj_t* device = lv_label_create(meta);
  lv_label_set_text(device, options.device == nullptr ? "" : options.device);
  lv_obj_set_style_text_font(device, options.small_font, LV_PART_MAIN);
  lv_obj_set_style_text_color(device, lv_color_hex(kDimText), LV_PART_MAIN);

  // Status line: bottom-left, under the "Based on ..." credit.
  view.status = lv_label_create(view.root);
  lv_label_set_text(view.status, "");
  lv_obj_set_style_text_font(view.status, options.text_font, LV_PART_MAIN);
  lv_obj_set_style_text_color(view.status, lv_color_hex(kStatusColor), LV_PART_MAIN);
  lv_obj_set_width(view.status, metrics.width - margin - gui2_core::ui_px(28));
  lv_label_set_long_mode(view.status, LV_LABEL_LONG_DOT);
  lv_obj_align(view.status, LV_ALIGN_BOTTOM_LEFT, margin, -gui2_core::ui_px(40));
  lv_obj_set_clickable(view.status, false);

  splash_started_ms = lv_tick_get();
  return view;
}

bool splash_intro_finished() {
  return splash_started_ms != 0 && lv_tick_elaps(splash_started_ms) >= kMinVisibleMs;
}

void update_splash(splash_view* view, const char* text) {
  if (view == nullptr || view->root == nullptr || view->status == nullptr) return;

  const char* shown = lv_label_get_text(view->status);
  if (text == nullptr || text[0] == '\0') {
    if (shown != nullptr && shown[0] != '\0') lv_label_set_text(view->status, "");
    return;
  }
  if (shown == nullptr || std::strcmp(shown, text) != 0)
    lv_label_set_text(view->status, text);
}

void dismiss_splash(splash_view* view) {
  if (view == nullptr || view->root == nullptr) return;
  // Deleting the root also drops every bar animation bound to it.
  lv_obj_delete(view->root);
  *view = {};
  splash_started_ms = 0;
}

}  // namespace gui2_shell
