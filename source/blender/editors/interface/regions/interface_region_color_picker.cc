/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Color Picker Region & Color Utils
 */

#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"

#include "UI_interface_c.hh"
#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "BLT_translation.hh"

#include "IMB_colormanagement.hh"

#include "interface_intern.hh"

namespace blender::ui {

enum ePickerType {
  PICKER_TYPE_RGB = 0,
  PICKER_TYPE_HSV = 1,
};

enum ePickerSpace {
  PICKER_SPACE_LINEAR = 0,
  PICKER_SPACE_PERCEPTUAL = 1,
};

static char g_color_picker_type = PICKER_TYPE_HSV;
static char g_color_picker_space = PICKER_SPACE_PERCEPTUAL;

/* -------------------------------------------------------------------- */
/** \name Color Conversion
 * \{ */

static void ui_color_picker_rgb_round(float rgb[3])
{
  /* Handle small rounding errors in color space conversions. Doing these for
   * all color space conversions would be expensive, but for the color picker
   * we can do the extra work. */
  for (int i = 0; i < 3; i++) {
    if (fabsf(rgb[i]) < 5e-5f) {
      rgb[i] = 0.0f;
    }
    else if (fabsf(1.0f - rgb[i]) < 5e-5f) {
      rgb[i] = 1.0f;
    }
  }
}

void color_picker_rgb_to_hsv_compat(const float rgb[3], float r_cp[3])
{
  /* Convert RGB to HSV, remaining as compatible as possible with the existing
   * r_hsv value (for example when value goes to zero, preserve the hue). */
  switch (U.color_picker_type) {
    case USER_CP_CIRCLE_HSL:
      rgb_to_hsl_compat_v(rgb, r_cp);
      break;
    default:
      rgb_to_hsv_compat_v(rgb, r_cp);
      break;
  }
}

void color_picker_rgb_to_hsv(const float rgb[3], float r_cp[3])
{
  switch (U.color_picker_type) {
    case USER_CP_CIRCLE_HSL:
      rgb_to_hsl_v(rgb, r_cp);
      break;
    default:
      rgb_to_hsv_v(rgb, r_cp);
      break;
  }
}

void color_picker_hsv_to_rgb(const float r_cp[3], float rgb[3])
{
  switch (U.color_picker_type) {
    case USER_CP_CIRCLE_HSL:
      hsl_to_rgb_v(r_cp, rgb);
      break;
    default:
      hsv_to_rgb_v(r_cp, rgb);
      break;
  }
}

bool button_is_color_gamma(Button *but)
{
  if (but->rnaprop) {
    if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
      return true;
    }
  }

  return but->block->is_color_gamma_picker;
}

bool button_color_has_alpha(Button *but)
{
  if (but->rnaprop) {
    const PropertySubType prop_subtype = RNA_property_subtype(but->rnaprop);
    if (ELEM(prop_subtype, PROP_COLOR, PROP_COLOR_GAMMA)) {
      const int color_components_count = RNA_property_array_length(&but->rnapoin, but->rnaprop);
      if (color_components_count == 4) {
        return true;
      }
    }
  }

  return false;
}

static void ui_scene_linear_to_perceptual_space(const bool is_gamma, float rgb[3])
{
  /* Map to color picking space for HSV values and HSV cube/circle,
   * assuming it is more perceptually linear than the scene linear
   * space for intuitive color picking. */
  if (!is_gamma) {
    IMB_colormanagement_scene_linear_to_color_picking_v3(rgb, rgb);
    ui_color_picker_rgb_round(rgb);
  }
}

static void ui_perceptual_to_scene_linear_space(const bool is_gamma, float rgb[3])
{
  if (!is_gamma) {
    IMB_colormanagement_color_picking_to_scene_linear_v3(rgb, rgb);
    ui_color_picker_rgb_round(rgb);
  }
}

void scene_linear_to_perceptual_space(Button *but, float rgb[3])
{
  ui_scene_linear_to_perceptual_space(button_is_color_gamma(but), rgb);
}

void perceptual_to_scene_linear_space(Button *but, float rgb[3])
{
  ui_perceptual_to_scene_linear_space(button_is_color_gamma(but), rgb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Picker
 * \{ */

static void ui_color_picker_update_from_rgb_linear(ColorPicker *cpicker,
                                                   const bool is_gamma,
                                                   const bool is_editing_sliders,
                                                   const float rgb_scene_linear[3])
{
  /* Note that we skip updating values if we are editing the same number sliders.
   * This avoids numerical drift from precision errors converting between color
   * space and between RGB and HSV. */

  /* Convert from RGB linear to RGB perceptual for number editing. */
  if (cpicker->is_init == false ||
      !(is_editing_sliders && g_color_picker_type == PICKER_TYPE_RGB &&
        g_color_picker_type == PICKER_SPACE_PERCEPTUAL))
  {
    copy_v3_v3(cpicker->rgb_perceptual_slider, rgb_scene_linear);
    ui_scene_linear_to_perceptual_space(is_gamma, cpicker->rgb_perceptual_slider);
  }

  /* Convert from RGB perceptual to HSV perceptual. */
  if (cpicker->is_init == false) {
    color_picker_rgb_to_hsv(cpicker->rgb_perceptual_slider, cpicker->hsv_perceptual_slider);
  }
  else if (!(is_editing_sliders && g_color_picker_type == PICKER_TYPE_HSV &&
             g_color_picker_space == PICKER_SPACE_PERCEPTUAL))
  {
    color_picker_rgb_to_hsv_compat(cpicker->rgb_perceptual_slider, cpicker->hsv_perceptual_slider);
  }

  /* Convert from RGB linear to HSV linear. */
  if (cpicker->is_init == false) {
    color_picker_rgb_to_hsv(rgb_scene_linear, cpicker->hsv_linear_slider);
  }
  else if (!(is_editing_sliders && g_color_picker_type == PICKER_TYPE_HSV &&
             g_color_picker_space == PICKER_SPACE_LINEAR))
  {
    color_picker_rgb_to_hsv_compat(rgb_scene_linear, cpicker->hsv_linear_slider);
  }

  ui_color_picker_rgb_round(cpicker->rgb_perceptual_slider);
  ui_color_picker_rgb_round(cpicker->hsv_perceptual_slider);
  ui_color_picker_rgb_round(cpicker->hsv_linear_slider);

  /* Convert from RGB to HSV in perceptually linear space for picker widgets. */
  float rgb_perceptual_slider[3];
  copy_v3_v3(rgb_perceptual_slider, rgb_scene_linear);
  ui_scene_linear_to_perceptual_space(is_gamma, rgb_perceptual_slider);

  if (cpicker->is_init == false) {
    color_picker_rgb_to_hsv(rgb_perceptual_slider, cpicker->hsv_perceptual);
    copy_v3_v3(cpicker->hsv_perceptual_init, cpicker->hsv_perceptual);
  }
  else {
    color_picker_rgb_to_hsv_compat(rgb_perceptual_slider, cpicker->hsv_perceptual);
  }

  cpicker->is_init = true;
}

void button_hsv_set(Button *but)
{
  float rgb_perceptual_slider[3];
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  float *hsv_perceptual = cpicker->hsv_perceptual;

  color_picker_hsv_to_rgb(hsv_perceptual, rgb_perceptual_slider);

  button_v3_set(but, rgb_perceptual_slider);
}

/* Updates all buttons who share the same color picker as the one passed. */
static void ui_update_color_picker_buts_rgba(Block *block,
                                             ColorPicker *cpicker,
                                             const bool is_editing_sliders,
                                             const float rgba_scene_linear[4])
{
  ui_color_picker_update_from_rgb_linear(
      cpicker, block->is_color_gamma_picker, is_editing_sliders, rgba_scene_linear);

  for (const std::unique_ptr<Button> &bt : block->buttons) {
    if (bt->custom_data != cpicker) {
      continue;
    }

    if (bt->rnaprop) {
      button_v4_set(bt.get(), rgba_scene_linear);
      /* original button that created the color picker already does undo
       * push, so disable it on RNA buttons in the color picker block */
      button_flag_disable(bt.get(), BUT_UNDO);
    }
    else if (bt->type == ButtonType::Text) {
      /* Hex text input field. */
      float rgba_hex[4];
      uchar rgba_hex_uchar[4];
      char col[16];

      /* Hex code is assumed to be in sRGB space (coming from other applications, web, etc...). */
      copy_v4_v4(rgba_hex, rgba_scene_linear);
      if (!block->is_color_gamma_picker) {
        IMB_colormanagement_scene_linear_to_srgb_v3(rgba_hex, rgba_hex);
        ui_color_picker_rgb_round(rgba_hex);
      }

      rgba_float_to_uchar(rgba_hex_uchar, rgba_hex);

      int col_len;
      if (cpicker->has_alpha) {
        col_len = SNPRINTF_UTF8_RLEN(
            col, "#%02X%02X%02X%02X", UNPACK4_EX((uint), rgba_hex_uchar, ));
      }
      else {
        col_len = SNPRINTF_UTF8_RLEN(col, "#%02X%02X%02X", UNPACK3_EX((uint), rgba_hex_uchar, ));
      }
      memcpy(bt->poin, col, col_len + 1); /* +1 offset for the # symbol. */
    }

    button_update(bt.get());
  }
}

static void ui_colorpicker_rgba_update_cb(bContext * /*C*/, void *picker_bt1, void *prop_bt1)
{
  Button *picker_but = static_cast<Button *>(picker_bt1);
  Block *block = picker_but->block;
  PopupBlockHandle *popup = block->handle;
  ColorPicker *cpicker = static_cast<ColorPicker *>(picker_but->custom_data);

  Button *prop_but = static_cast<Button *>(prop_bt1);
  PointerRNA ptr = prop_but->rnapoin;
  PropertyRNA *prop = prop_but->rnaprop;

  if (prop) {
    float rgba_scene_linear[4];

    zero_v4(rgba_scene_linear);
    RNA_property_float_get_array_at_most(
        &ptr, prop, rgba_scene_linear, ARRAY_SIZE(rgba_scene_linear));
    ui_update_color_picker_buts_rgba(block, cpicker, false, rgba_scene_linear);
  }

  if (popup) {
    popup->menuretval = RETURN_UPDATE;
  }
}

static void ui_colorpicker_hsv_perceptual_slider_update_cb(bContext * /*C*/, void *bt1, void *bt2)
{
  Button *but = static_cast<Button *>(bt1);
  PopupBlockHandle *popup = but->block->handle;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);

  /* Get RNA ptr/prop from the original color datablock button (bt2) since the HSV buttons (bt1)
   * do not directly point to it. */
  Button *prop_but = static_cast<Button *>(bt2);
  PointerRNA ptr = prop_but->rnapoin;
  PropertyRNA *prop = prop_but->rnaprop;
  float rgba_scene_linear[4];

  if (prop) {
    zero_v4(rgba_scene_linear);
    /* Get the current RGBA color for its (optional) Alpha component,
     * then update RGB components from the current HSV values. */
    RNA_property_float_get_array_at_most(
        &ptr, prop, rgba_scene_linear, ARRAY_SIZE(rgba_scene_linear));
    color_picker_hsv_to_rgb(cpicker->hsv_perceptual_slider, cpicker->rgb_perceptual_slider);
    copy_v3_v3(rgba_scene_linear, cpicker->rgb_perceptual_slider);
    ui_perceptual_to_scene_linear_space(but->block->is_color_gamma_picker, rgba_scene_linear);
    ui_update_color_picker_buts_rgba(but->block, cpicker, true, rgba_scene_linear);
  }

  if (popup) {
    popup->menuretval = RETURN_UPDATE;
  }
}

static void ui_colorpicker_hsv_linear_slider_update_cb(bContext * /*C*/, void *bt1, void *bt2)
{
  Button *but = static_cast<Button *>(bt1);
  PopupBlockHandle *popup = but->block->handle;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);

  /* Get RNA ptr/prop from the original color datablock button (bt2) since the HSV buttons (bt1)
   * do not directly point to it. */
  Button *prop_but = static_cast<Button *>(bt2);
  PointerRNA ptr = prop_but->rnapoin;
  PropertyRNA *prop = prop_but->rnaprop;
  float rgba_scene_linear[4];

  if (prop) {
    zero_v4(rgba_scene_linear);
    /* Get the current RGBA color for its (optional) Alpha component,
     * then update RGB components from the current HSV values. */
    RNA_property_float_get_array_at_most(
        &ptr, prop, rgba_scene_linear, ARRAY_SIZE(rgba_scene_linear));
    color_picker_hsv_to_rgb(cpicker->hsv_linear_slider, rgba_scene_linear);
    ui_update_color_picker_buts_rgba(but->block, cpicker, true, rgba_scene_linear);
  }

  if (popup) {
    popup->menuretval = RETURN_UPDATE;
  }
}

static void ui_colorpicker_rgb_perceptual_slider_update_cb(bContext * /*C*/, void *bt1, void *bt2)
{
  Button *but = static_cast<Button *>(bt1);
  PopupBlockHandle *popup = but->block->handle;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);

  /* Get RNA ptr/prop from the original color datablock button (bt2) since the HSV buttons (bt1)
   * do not directly point to it. */
  Button *prop_but = static_cast<Button *>(bt2);
  PointerRNA ptr = prop_but->rnapoin;
  PropertyRNA *prop = prop_but->rnaprop;
  float rgba_scene_linear[4];

  if (prop) {
    zero_v4(rgba_scene_linear);
    /* Get the current RGBA color for its (optional) Alpha component,
     * then update RGB components from the current HSV values. */
    RNA_property_float_get_array_at_most(
        &ptr, prop, rgba_scene_linear, ARRAY_SIZE(rgba_scene_linear));
    copy_v3_v3(rgba_scene_linear, cpicker->rgb_perceptual_slider);
    ui_perceptual_to_scene_linear_space(but->block->is_color_gamma_picker, rgba_scene_linear);
    color_picker_rgb_to_hsv(cpicker->rgb_perceptual_slider, cpicker->hsv_perceptual_slider);
    ui_update_color_picker_buts_rgba(but->block, cpicker, true, rgba_scene_linear);
  }

  if (popup) {
    popup->menuretval = RETURN_UPDATE;
  }
}

static void ui_colorpicker_hex_rna_cb(bContext * /*C*/, void *bt1, void *bt2)
{
  Button *but = static_cast<Button *>(bt1);
  PopupBlockHandle *popup = but->block->handle;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  char hexcol[128];
  button_string_get(but, hexcol, ARRAY_SIZE(hexcol));

  /* In case the current color contains an Alpha component but the Hex string does not, get the
   * current color to preserve the Alpha component.
   * Like #ui_colorpicker_hsv_perceptual_slider_update_cb, the original color datablock button
   * (bt2) is used since Hex Text Field button (bt1) doesn't directly point to it. */
  Button *prop_but = static_cast<Button *>(bt2);
  PointerRNA ptr = prop_but->rnapoin;
  PropertyRNA *prop = prop_but->rnaprop;

  float rgba[4];
  if (prop) {
    zero_v4(rgba);
    RNA_property_float_get_array_at_most(&ptr, prop, rgba, ARRAY_SIZE(rgba));
  }
  /* Override the current color with the parsed Hex string, preserving the original Alpha if the
   * hex string doesn't contain it. */
  const bool is_parsed = hex_to_rgba(hexcol, rgba, rgba + 1, rgba + 2, rgba + 3);

  /* Hex code is assumed to be in sRGB space (coming from other applications, web, etc...).
   * Only apply conversion if the hex string was successfully parsed. */
  if (is_parsed && !button_is_color_gamma(but)) {
    IMB_colormanagement_srgb_to_scene_linear_v3(rgba, rgba);
    ui_color_picker_rgb_round(rgba);
  }

  ui_update_color_picker_buts_rgba(but->block, cpicker, false, rgba);

  if (popup) {
    popup->menuretval = RETURN_UPDATE;
  }
}

static void ui_popup_close_cb(bContext * /*C*/, void *bt1, void * /*arg*/)
{
  Button *but = static_cast<Button *>(bt1);
  PopupBlockHandle *popup = but->block->handle;

  if (popup) {
    ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
    BLI_assert(cpicker->is_init);
    popup->menuretval = (equals_v3v3(cpicker->hsv_perceptual, cpicker->hsv_perceptual_init) ?
                             RETURN_CANCEL :
                             RETURN_OK);
  }
}

static void ui_colorpicker_hide_reveal(Block *block)
{
  const ePickerType type = ePickerType(g_color_picker_type);
  const ePickerSpace space = (block->is_color_gamma_picker) ? (type == PICKER_TYPE_RGB) ?
                                                              PICKER_SPACE_LINEAR :
                                                              PICKER_SPACE_PERCEPTUAL :
                                                              ePickerSpace(g_color_picker_space);

  /* tag buttons */
  for (const std::unique_ptr<Button> &bt : block->buttons) {
    if ((bt->func == ui_colorpicker_rgba_update_cb) && (bt->type == ButtonType::NumSlider) &&
        (bt->rnaindex != 3))
    {
      /* RGB sliders (color circle and alpha are always shown) */
      SET_FLAG_FROM_TEST(
          bt->flag, !(type == PICKER_TYPE_RGB && space == PICKER_SPACE_LINEAR), UI_HIDDEN);
    }
    else if (bt->func == ui_colorpicker_rgb_perceptual_slider_update_cb) {
      /* HSV sliders */
      SET_FLAG_FROM_TEST(
          bt->flag, !(type == PICKER_TYPE_RGB && space == PICKER_SPACE_PERCEPTUAL), UI_HIDDEN);
    }
    else if (bt->func == ui_colorpicker_hsv_perceptual_slider_update_cb) {
      /* HSV sliders */
      SET_FLAG_FROM_TEST(
          bt->flag, !(type == PICKER_TYPE_HSV && space == PICKER_SPACE_PERCEPTUAL), UI_HIDDEN);
    }
    else if (bt->func == ui_colorpicker_hsv_linear_slider_update_cb) {
      /* HSV sliders */
      SET_FLAG_FROM_TEST(
          bt->flag, !(type == PICKER_TYPE_HSV && space == PICKER_SPACE_LINEAR), UI_HIDDEN);
    }
  }
}

static void ui_colorpicker_update_type_space_cb(bContext * /*C*/, void *picker_bt1, void *prop_bt1)
{
  Button *picker_but = static_cast<Button *>(picker_bt1);
  Block *block = picker_but->block;
  ColorPicker *cpicker = static_cast<ColorPicker *>(picker_but->custom_data);

  Button *prop_but = static_cast<Button *>(prop_bt1);
  PointerRNA ptr = prop_but->rnapoin;
  PropertyRNA *prop = prop_but->rnaprop;

  float rgba_scene_linear[4];

  zero_v4(rgba_scene_linear);
  RNA_property_float_get_array_at_most(
      &ptr, prop, rgba_scene_linear, ARRAY_SIZE(rgba_scene_linear));
  ui_update_color_picker_buts_rgba(block, cpicker, false, rgba_scene_linear);

  ui_colorpicker_hide_reveal(picker_but->block);
}

#define PICKER_TOTAL_W (180.0f * UI_SCALE_FAC)
#define PICKER_BAR ((8.0f * UI_SCALE_FAC) + (6 * U.pixelsize))
#define PICKER_SPACE (8.0f * UI_SCALE_FAC)
#define PICKER_W (PICKER_TOTAL_W - PICKER_BAR - PICKER_SPACE)
#define PICKER_H PICKER_W

/** Get localized tooltips for the current color picker type.
 *
 * \param r_area_tooltip Tooltip describing the color area (e.g., "Hue/Saturation").
 * \param r_slider_tooltip Tooltip describing the slider (e.g., "Lightness" or "Value").
 */
static void ui_colorpicker_tooltips(ColorPicker *cpicker,
                                    const char **r_area_tooltip,
                                    const char **r_slider_tooltip)
{
  const char *name_hue = CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Hue");
  const char *name_sat = CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Saturation");
  const char *name_val = CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Value");
  const char *name_light = CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Lightness");
  const char *name_color = CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Color");

  const char *axis_x = nullptr;
  const char *axis_y = nullptr;
  const char *slider = nullptr;

  switch (U.color_picker_type) {
    case USER_CP_CIRCLE_HSL:
      axis_x = name_hue;
      axis_y = name_sat;
      slider = name_light;
      break;
    case USER_CP_CIRCLE_HSV:
      axis_x = name_hue;
      axis_y = name_sat;
      slider = name_val;
      break;
    case USER_CP_SQUARE_SV:
      axis_x = name_sat;
      axis_y = name_val;
      slider = name_hue;
      break;
    case USER_CP_SQUARE_HS:
      axis_x = name_hue;
      axis_y = name_sat;
      slider = name_val;
      break;
    case USER_CP_SQUARE_HV:
      axis_x = name_hue;
      axis_y = name_val;
      slider = name_sat;
      break;
    default:
      *r_area_tooltip = name_color;
      *r_slider_tooltip = name_val;
      return;
  }

  SNPRINTF_UTF8(cpicker->tooltip_area, "%s/%s", axis_x, axis_y);

  *r_area_tooltip = cpicker->tooltip_area;
  *r_slider_tooltip = slider;
}

static void ui_colorpicker_circle(Block *block,
                                  PointerRNA *ptr,
                                  PropertyRNA *prop,
                                  ColorPicker *cpicker)
{
  Button *bt;
  ButtonHSVCube *hsv_but;
  const char *circle_tooltip;
  const char *slider_tooltip;

  ui_colorpicker_tooltips(cpicker, &circle_tooltip, &slider_tooltip);

  /* Color circle (Hue/Saturation) */
  bt = uiDefButR_prop(block,
                      ButtonType::HsvCircle,
                      "",
                      0,
                      0,
                      PICKER_H,
                      PICKER_W,
                      ptr,
                      prop,
                      -1,
                      0.0,
                      0.0,
                      circle_tooltip);
  button_func_set(bt, ui_colorpicker_rgba_update_cb, bt, bt);
  bt->custom_data = cpicker;

  /* Slider (Lightness or Value, depending on color picker type) */
  hsv_but = static_cast<ButtonHSVCube *>(uiDefButR_prop(block,
                                                        ButtonType::HsvCube,
                                                        "",
                                                        PICKER_W + PICKER_SPACE,
                                                        0,
                                                        PICKER_BAR,
                                                        PICKER_H,
                                                        ptr,
                                                        prop,
                                                        -1,
                                                        0.0,
                                                        0.0,
                                                        slider_tooltip));
  hsv_but->gradient_type = (U.color_picker_type == USER_CP_CIRCLE_HSL) ? GRAD_L_ALT : GRAD_V_ALT;
  button_func_set(hsv_but, ui_colorpicker_rgba_update_cb, hsv_but, hsv_but);
  hsv_but->custom_data = cpicker;
}

static void ui_colorpicker_square(
    Block *block, PointerRNA *ptr, PropertyRNA *prop, eButGradientType type, ColorPicker *cpicker)
{
  ButtonHSVCube *hsv_but;
  const char *square_tooltip;
  const char *slider_tooltip;

  ui_colorpicker_tooltips(cpicker, &square_tooltip, &slider_tooltip);

  BLI_assert(type <= GRAD_HS);

  /* Color square (SV, HS or HV, depending on color picker type) */
  hsv_but = static_cast<ButtonHSVCube *>(uiDefButR_prop(block,
                                                        ButtonType::HsvCube,
                                                        "",
                                                        0,
                                                        PICKER_BAR + PICKER_SPACE,
                                                        PICKER_TOTAL_W,
                                                        PICKER_H,
                                                        ptr,
                                                        prop,
                                                        -1,
                                                        0.0,
                                                        0.0,
                                                        square_tooltip));
  hsv_but->gradient_type = type;
  button_func_set(hsv_but, ui_colorpicker_rgba_update_cb, hsv_but, hsv_but);
  hsv_but->custom_data = cpicker;

  /* Slider (Hue, Saturation or Value, depending on color picker type) */
  hsv_but = static_cast<ButtonHSVCube *>(uiDefButR_prop(block,
                                                        ButtonType::HsvCube,
                                                        "",
                                                        0,
                                                        0,
                                                        PICKER_TOTAL_W,
                                                        PICKER_BAR,
                                                        ptr,
                                                        prop,
                                                        -1,
                                                        0.0,
                                                        0.0,
                                                        slider_tooltip));
  hsv_but->gradient_type = eButGradientType(type + 3);
  button_func_set(hsv_but, ui_colorpicker_rgba_update_cb, hsv_but, hsv_but);
  hsv_but->custom_data = cpicker;
}

/* a HS circle, V slider, rgb/hsv/hex sliders */
static void block_colorpicker(const bContext * /*C*/,
                              Block *block,
                              Button *from_but,
                              float rgba_scene_linear[4],
                              bool show_picker)
{
  /* ePickerType */
  Button *bt;
  int picker_width;
  float softmin, softmax, hardmin, hardmax, step, precision;
  ColorPicker *cpicker = block_colorpicker_create(block);
  PointerRNA *ptr = &from_but->rnapoin;
  PropertyRNA *prop = from_but->rnaprop;

  picker_width = PICKER_TOTAL_W;

  RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);
  RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
  RNA_property_float_get_array_at_most(ptr, prop, rgba_scene_linear, 4);

  ui_color_picker_update_from_rgb_linear(
      cpicker, block->is_color_gamma_picker, false, rgba_scene_linear);
  cpicker->has_alpha = button_color_has_alpha(from_but);

  /* when the softmax isn't defined in the RNA,
   * using very large numbers causes sRGB/linear round trip to fail. */
  if (softmax == FLT_MAX) {
    softmax = 1.0f;
  }

  switch (U.color_picker_type) {
    case USER_CP_SQUARE_SV:
      ui_colorpicker_square(block, ptr, prop, GRAD_SV, cpicker);
      break;
    case USER_CP_SQUARE_HS:
      ui_colorpicker_square(block, ptr, prop, GRAD_HS, cpicker);
      break;
    case USER_CP_SQUARE_HV:
      ui_colorpicker_square(block, ptr, prop, GRAD_HV, cpicker);
      break;

    /* user default */
    case USER_CP_CIRCLE_HSV:
    case USER_CP_CIRCLE_HSL:
    default:
      ui_colorpicker_circle(block, ptr, prop, cpicker);
      break;
  }

  /* mode */
  int yco = -0.5f * UI_UNIT_Y;

  if (!block->is_color_gamma_picker) {
    auto colorspace_tip_func = [](bContext & /*C*/, TooltipData &tip, Button *but, void *space) {
      tooltip_text_field_add(tip, but->tip, {}, TIP_STYLE_HEADER, TIP_LC_NORMAL, false);
      tooltip_text_field_add(tip,
                             TIP_("Color Space: ") + std::string(static_cast<const char *>(space)),
                             {},
                             TIP_STYLE_NORMAL,
                             TIP_LC_ACTIVE,
                             false);
    };

    block_align_begin(block);

    bt = uiDefButC(block,
                   ButtonType::Row,
                   IFACE_("Linear"),
                   0,
                   yco -= UI_UNIT_Y,
                   picker_width * 0.5,
                   UI_UNIT_Y,
                   &g_color_picker_space,
                   0.0,
                   float(PICKER_TYPE_RGB),
                   TIP_("Scene linear values in the working color space"));
    button_flag_disable(bt, BUT_UNDO);
    button_drawflag_disable(bt, BUT_TEXT_LEFT);
    button_func_set(bt, ui_colorpicker_update_type_space_cb, bt, from_but);
    button_func_tooltip_custom_set(
        bt,
        colorspace_tip_func,
        const_cast<char *>(IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR)),
        nullptr);
    bt->custom_data = cpicker;

    bt = uiDefButC(block,
                   ButtonType::Row,
                   IFACE_("Perceptual"),
                   picker_width * 0.5,
                   yco,
                   picker_width * 0.5,
                   UI_UNIT_Y,
                   &g_color_picker_space,
                   0.0,
                   float(PICKER_TYPE_HSV),
                   TIP_("Perceptually uniform values, matching the color picker"));
    button_flag_disable(bt, BUT_UNDO);
    button_drawflag_disable(bt, BUT_TEXT_LEFT);
    button_func_set(bt, ui_colorpicker_update_type_space_cb, bt, from_but);
    button_func_tooltip_custom_set(
        bt,
        colorspace_tip_func,
        const_cast<char *>(IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_COLOR_PICKING)),
        nullptr);

    bt->custom_data = cpicker;

    block_align_end(block);

    yco -= 0.5f * UI_UNIT_X;
  }

  block_align_begin(block);

  bt = uiDefButC(block,
                 ButtonType::Row,
                 IFACE_("RGB"),
                 0,
                 yco -= UI_UNIT_Y,
                 picker_width * 0.5,
                 UI_UNIT_Y,
                 &g_color_picker_type,
                 0.0,
                 float(PICKER_TYPE_RGB),
                 TIP_("RGB values"));
  button_flag_disable(bt, BUT_UNDO);
  button_drawflag_disable(bt, BUT_TEXT_LEFT);
  button_func_set(bt, ui_colorpicker_update_type_space_cb, bt, from_but);
  bt->custom_data = cpicker;

  bt = uiDefButC(block,
                 ButtonType::Row,
                 (U.color_picker_type == USER_CP_CIRCLE_HSL) ? IFACE_("HSL") : IFACE_("HSV"),
                 picker_width * 0.5,
                 yco,
                 picker_width * 0.5,
                 UI_UNIT_Y,
                 &g_color_picker_type,
                 0.0,
                 float(PICKER_TYPE_HSV),
                 (U.color_picker_type == USER_CP_CIRCLE_HSL) ? TIP_("Hue, Saturation, Lightness") :
                                                               TIP_("Hue, Saturation, Value"));
  button_flag_disable(bt, BUT_UNDO);
  button_drawflag_disable(bt, BUT_TEXT_LEFT);
  button_func_set(bt, ui_colorpicker_update_type_space_cb, bt, from_but);
  bt->custom_data = cpicker;

  block_align_end(block);

  const int slider_yco = yco - 1.1f * UI_UNIT_Y;

  /* NOTE: don't disable BUT_UNDO for RGBA values, since these don't add undo steps. */

  /* RGB values */
  block_align_begin(block);
  const auto add_rgb_perceptual_slider =
      [&](const char *str, const char *tip, const int index, const int y) {
        bt = uiDefButR_prop(block,
                            ButtonType::NumSlider,
                            str,
                            0,
                            y,
                            picker_width,
                            UI_UNIT_Y,
                            ptr,
                            prop,
                            index,
                            0.0,
                            0.0,
                            tip);
        button_number_slider_step_size_set(bt, 10);
        button_number_slider_precision_set(bt, 3);
        button_func_set(bt, ui_colorpicker_rgba_update_cb, bt, bt);
        bt->custom_data = cpicker;
      };

  yco = slider_yco;
  add_rgb_perceptual_slider(IFACE_("Red:"), TIP_("Red"), 0, yco);
  add_rgb_perceptual_slider(IFACE_("Green:"), TIP_("Green"), 1, yco -= UI_UNIT_Y);
  add_rgb_perceptual_slider(IFACE_("Blue:"), TIP_("Blue"), 2, yco -= UI_UNIT_Y);

  /* HSV values */
  const auto add_hsv_perceptual_slider =
      [&](const char *str, const char *tip, const int index, const int y, const bool linear) {
        float *hsv_values = linear ? cpicker->hsv_linear_slider : cpicker->hsv_perceptual_slider;
        bt = uiDefButF(block,
                       ButtonType::NumSlider,
                       str,
                       0,
                       y,
                       picker_width,
                       UI_UNIT_Y,
                       hsv_values + index,
                       0.0,
                       1.0,
                       tip);
        if (index == 2) {
          bt->hardmax = hardmax; /* Not common but RGB may be over 1.0. */
        }
        button_number_slider_step_size_set(bt, 10);
        button_number_slider_precision_set(bt, 3);
        button_flag_disable(bt, BUT_UNDO);
        button_func_set(bt,
                        linear ? ui_colorpicker_hsv_linear_slider_update_cb :
                                 ui_colorpicker_hsv_perceptual_slider_update_cb,
                        bt,
                        from_but);
        bt->custom_data = cpicker;
      };

  yco = slider_yco;
  add_hsv_perceptual_slider(IFACE_("Hue:"), TIP_("Hue"), 0, yco, !block->is_color_gamma_picker);
  add_hsv_perceptual_slider(IFACE_("Saturation:"),
                            TIP_("Saturation"),
                            1,
                            yco -= UI_UNIT_Y,
                            !block->is_color_gamma_picker);
  if (U.color_picker_type == USER_CP_CIRCLE_HSL) {
    add_hsv_perceptual_slider(IFACE_("Lightness:"),
                              TIP_("Lightness"),
                              2,
                              yco -= UI_UNIT_Y,
                              !block->is_color_gamma_picker);
  }
  else {
    add_hsv_perceptual_slider(CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "Value:"),
                              CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Value"),
                              2,
                              yco -= UI_UNIT_Y,
                              !block->is_color_gamma_picker);
  }

  /* Could use:
   * col->prop(ptr, prop, -1, 0,ui::ITEM_R_EXPAND |ui::ITEM_R_SLIDER, "", ICON_NONE);
   * but need to use button_func_set for updating other fake buttons */

  if (!block->is_color_gamma_picker) {
    yco = slider_yco;

    /* Display RGB values */
    const auto add_rgb_perceptual_slider =
        [&](const char *str, const char *tip, const int index, const int y) {
          bt = uiDefButF(block,
                         ButtonType::NumSlider,
                         str,
                         0,
                         y,
                         picker_width,
                         UI_UNIT_Y,
                         cpicker->rgb_perceptual_slider + index,
                         hardmin,
                         hardmax,
                         tip);
          button_number_slider_step_size_set(bt, 10);
          button_number_slider_precision_set(bt, 3);
          bt->softmin = softmin;
          bt->softmax = softmax;
          button_flag_disable(bt, BUT_UNDO);
          button_func_set(bt, ui_colorpicker_rgb_perceptual_slider_update_cb, bt, from_but);
          bt->custom_data = cpicker;
        };

    add_rgb_perceptual_slider(IFACE_("Red:"), TIP_("Red"), 0, yco);
    add_rgb_perceptual_slider(IFACE_("Green:"), TIP_("Green"), 1, yco -= UI_UNIT_Y);
    add_rgb_perceptual_slider(IFACE_("Blue:"), TIP_("Blue"), 2, yco -= UI_UNIT_Y);

    yco = slider_yco;
    add_hsv_perceptual_slider(IFACE_("Hue:"), TIP_("Hue"), 0, yco, false);
    add_hsv_perceptual_slider(
        IFACE_("Saturation:"), TIP_("Saturation"), 1, yco -= UI_UNIT_Y, false);
    if (U.color_picker_type == USER_CP_CIRCLE_HSL) {
      add_hsv_perceptual_slider(
          IFACE_("Lightness:"), TIP_("Lightness"), 2, yco -= UI_UNIT_Y, false);
    }
    else {
      add_hsv_perceptual_slider(CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "Value:"),
                                CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Value"),
                                2,
                                yco -= UI_UNIT_Y,
                                false);
    }
  }

  if (cpicker->has_alpha) {
    bt = uiDefButR_prop(block,
                        ButtonType::NumSlider,
                        IFACE_("Alpha:"),
                        0,
                        yco -= UI_UNIT_Y,
                        picker_width,
                        UI_UNIT_Y,
                        ptr,
                        prop,
                        3,
                        0.0,
                        0.0,
                        TIP_("Alpha"));
    button_number_slider_step_size_set(bt, 10);
    button_number_slider_precision_set(bt, 3);
    button_func_set(bt, ui_colorpicker_rgba_update_cb, bt, bt);
    bt->custom_data = cpicker;
  }
  else {
    rgba_scene_linear[3] = 1.0f;
  }

  block_align_end(block);

  /* Hex color is in sRGB space. */
  float rgba_hex[4];
  uchar rgba_hex_uchar[4];

  copy_v4_v4(rgba_hex, rgba_scene_linear);

  if (!button_is_color_gamma(from_but)) {
    IMB_colormanagement_scene_linear_to_srgb_v3(rgba_hex, rgba_hex);
    ui_color_picker_rgb_round(rgba_hex);
  }

  rgba_float_to_uchar(rgba_hex_uchar, rgba_hex);

  if (cpicker->has_alpha) {
    SNPRINTF_UTF8(cpicker->hexcol, "#%02X%02X%02X%02X", UNPACK4_EX((uint), rgba_hex_uchar, ));
  }
  else {
    SNPRINTF_UTF8(cpicker->hexcol, "#%02X%02X%02X", UNPACK3_EX((uint), rgba_hex_uchar, ));
  }

  yco -= UI_UNIT_Y * 1.5f;

  const int label_width = picker_width * 0.15f;
  const int eyedropper_offset = show_picker ? UI_UNIT_X * 1.25f : 0;
  const int text_width = picker_width - label_width - eyedropper_offset;

  uiDefBut(block,
           ButtonType::Label,
           IFACE_("Hex"),
           0,
           yco,
           label_width,
           UI_UNIT_Y,
           nullptr,
           0.0,
           0.0,
           std::nullopt);

  bt = uiDefBut(block,
                ButtonType::Text,
                "",
                label_width,
                yco,
                text_width,
                UI_UNIT_Y,
                cpicker->hexcol,
                0,
                cpicker->has_alpha ? 10 : 8,
                std::nullopt);
  const auto bt_tooltip_func =
      [](bContext & /*C*/, TooltipData &tip, Button * /*but*/, void *has_alpha_ptr) {
        const bool *has_alpha = static_cast<bool *>(has_alpha_ptr);
        if (*has_alpha) {
          tooltip_text_field_add(tip,
                                 TIP_("Hex triplet for color with alpha (#RRGGBBAA)."),
                                 {},
                                 TIP_STYLE_HEADER,
                                 TIP_LC_NORMAL,
                                 false);
        }
        else {
          tooltip_text_field_add(tip,
                                 TIP_("Hex triplet for color (#RRGGBB)."),
                                 {},
                                 TIP_STYLE_HEADER,
                                 TIP_LC_NORMAL,
                                 false);
        }
        tooltip_text_field_add(
            tip, TIP_("Gamma corrected"), {}, TIP_STYLE_NORMAL, TIP_LC_NORMAL, false);
      };
  button_func_tooltip_custom_set(
      bt, bt_tooltip_func, static_cast<void *>(&cpicker->has_alpha), nullptr);
  button_flag_disable(bt, BUT_UNDO);
  button_func_set(bt, ui_colorpicker_hex_rna_cb, bt, from_but);
  bt->custom_data = cpicker;

  if (show_picker) {
    bt = uiDefIconButO(block,
                       ButtonType::But,
                       "UI_OT_eyedropper_color",
                       wm::OpCallContext::InvokeDefault,
                       ICON_EYEDROPPER,
                       picker_width - UI_UNIT_X,
                       yco,
                       UI_UNIT_X,
                       UI_UNIT_Y,
                       std::nullopt);
    button_flag_disable(bt, BUT_UNDO);
    button_drawflag_disable(bt, BUT_ICON_LEFT);
    button_func_set(bt, ui_popup_close_cb, bt, nullptr);
    bt->custom_data = cpicker;
  }

  ui_colorpicker_hide_reveal(block);
}

static int ui_colorpicker_wheel_cb(const bContext * /*C*/, Block *block, const wmEvent *event)
{
  PopupBlockHandle *popup = block->handle;
  bool mouse_in_region = popup && BLI_rcti_isect_pt(&popup->region->winrct,
                                                    float(event->xy[0]),
                                                    float(event->xy[1]));

  if (popup && !mouse_in_region && (ISMOUSE_WHEEL(event->type) || event->type == MOUSEPAN)) {
    /* Exit and save color if moving mouse wheel or trackpad panning while outside the popup. */
    popup->menuretval = RETURN_OK;
    return 1;
  }

  /* Increase/Decrease the Color HSV Value component using the mouse wheel. */
  float add = 0.0f;

  switch (event->type) {
    case WHEELUPMOUSE:
      add = 0.05f;
      break;
    case WHEELDOWNMOUSE:
      add = -0.05f;
      break;
    case MOUSEPAN:
      add = 0.005f * WM_event_absolute_delta_y(event) / UI_SCALE_FAC;
      break;
    default:
      break;
  }

  if (add != 0.0f) {
    for (const std::unique_ptr<Button> &but : block->buttons) {
      if (but->type == ButtonType::HsvCube && but->active == nullptr) {
        ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
        float *hsv_perceptual = cpicker->hsv_perceptual;

        /* Get the RGBA Color. */
        float rgba_perceptual[4];
        button_v4_get(but.get(), rgba_perceptual);
        ui_scene_linear_to_perceptual_space(block->is_color_gamma_picker, rgba_perceptual);

        /* Convert it to HSV. */
        color_picker_rgb_to_hsv_compat(rgba_perceptual, hsv_perceptual);

        /* Increment/Decrement its value from mouse wheel input. */
        hsv_perceptual[2] = clamp_f(hsv_perceptual[2] + add, 0.0f, 1.0f);

        /* Convert it to linear space RGBA, and apply it back to the button. */
        float rgba_scene_linear[4];
        rgba_scene_linear[3] = rgba_perceptual[3]; /* Transfer Alpha component. */
        color_picker_hsv_to_rgb(hsv_perceptual, rgba_scene_linear);
        perceptual_to_scene_linear_space(but.get(), rgba_scene_linear);
        button_v4_set(but.get(), rgba_scene_linear);

        /* Update all other Color Picker buttons to reflect the color change. */
        ui_update_color_picker_buts_rgba(block, cpicker, false, rgba_scene_linear);
        if (popup) {
          popup->menuretval = RETURN_UPDATE;
        }

        return 1;
      }
    }
  }
  return 0;
}

Block *block_func_COLOR(bContext *C, PopupBlockHandle *handle, void *arg_but)
{
  Button *but = static_cast<Button *>(arg_but);
  Block *block;

  block = block_begin(C, handle->region, __func__, EmbossType::Emboss);

  if (button_is_color_gamma(but)) {
    block->is_color_gamma_picker = true;
  }

  copy_v3_v3(handle->retvec, but->editvec);

  block_colorpicker(C, block, but, handle->retvec, true);

  block->flag = BLOCK_LOOP | BLOCK_KEEP_OPEN | BLOCK_OUT_1 | BLOCK_MOVEMOUSE_QUIT;
  block_theme_style_set(block, BLOCK_THEME_STYLE_POPUP);
  block_bounds_set_normal(block, 0.5 * UI_UNIT_X);

  block->block_event_func = ui_colorpicker_wheel_cb;
  block->direction = UI_DIR_UP;

  return block;
}

ColorPicker *block_colorpicker_create(Block *block)
{
  ColorPicker *cpicker = MEM_new_zeroed<ColorPicker>(__func__);
  BLI_addhead(&block->color_pickers.list, cpicker);

  return cpicker;
}

/** \} */

}  // namespace blender::ui
