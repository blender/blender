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
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"

#include "BLT_translation.hh"

#include "IMB_colormanagement.hh"

#include "interface_intern.hh"

enum ePickerType {
  PICKER_TYPE_RGB = 0,
  PICKER_TYPE_HSV = 1,
};

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

void ui_color_picker_rgb_to_hsv_compat(const float rgb[3], float r_cp[3])
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

void ui_color_picker_rgb_to_hsv(const float rgb[3], float r_cp[3])
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

void ui_color_picker_hsv_to_rgb(const float r_cp[3], float rgb[3])
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

bool ui_but_is_color_gamma(uiBut *but)
{
  if (but->rnaprop) {
    if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
      return true;
    }
  }

  return but->block->is_color_gamma_picker;
}

bool ui_but_color_has_alpha(uiBut *but)
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

void ui_scene_linear_to_perceptual_space(uiBut *but, float rgb[3])
{
  /* Map to color picking space for HSV values and HSV cube/circle,
   * assuming it is more perceptually linear than the scene linear
   * space for intuitive color picking. */
  if (!ui_but_is_color_gamma(but)) {
    IMB_colormanagement_scene_linear_to_color_picking_v3(rgb, rgb);
    ui_color_picker_rgb_round(rgb);
  }
}

void ui_perceptual_to_scene_linear_space(uiBut *but, float rgb[3])
{
  if (!ui_but_is_color_gamma(but)) {
    IMB_colormanagement_color_picking_to_scene_linear_v3(rgb, rgb);
    ui_color_picker_rgb_round(rgb);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Picker
 * \{ */

static void ui_color_picker_update_hsv(ColorPicker *cpicker,
                                       uiBut *from_but,
                                       const float rgb_scene_linear[3])
{
  /* Convert from RGB to HSV in scene linear space color for number editing. */
  if (cpicker->is_init == false) {
    ui_color_picker_rgb_to_hsv(rgb_scene_linear, cpicker->hsv_scene_linear);
  }
  else {
    ui_color_picker_rgb_to_hsv_compat(rgb_scene_linear, cpicker->hsv_scene_linear);
  }

  /* Convert from RGB to HSV in perceptually linear space for picker widgets. */
  float rgb_perceptual[3];
  copy_v3_v3(rgb_perceptual, rgb_scene_linear);
  if (from_but) {
    ui_scene_linear_to_perceptual_space(from_but, rgb_perceptual);
  }

  if (cpicker->is_init == false) {
    ui_color_picker_rgb_to_hsv(rgb_perceptual, cpicker->hsv_perceptual);
    copy_v3_v3(cpicker->hsv_perceptual_init, cpicker->hsv_perceptual);
  }
  else {
    ui_color_picker_rgb_to_hsv_compat(rgb_perceptual, cpicker->hsv_perceptual);
  }

  cpicker->is_init = true;
}

void ui_but_hsv_set(uiBut *but)
{
  float rgb_perceptual[3];
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  float *hsv_perceptual = cpicker->hsv_perceptual;

  ui_color_picker_hsv_to_rgb(hsv_perceptual, rgb_perceptual);

  ui_but_v3_set(but, rgb_perceptual);
}

/* Updates all buttons who share the same color picker as the one passed. */
static void ui_update_color_picker_buts_rgba(uiBut *from_but,
                                             uiBlock *block,
                                             ColorPicker *cpicker,
                                             const float rgba_scene_linear[4])
{
  ui_color_picker_update_hsv(cpicker, from_but, rgba_scene_linear);

  LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
    if (bt->custom_data != cpicker) {
      continue;
    }

    if (bt->rnaprop) {
      ui_but_v4_set(bt, rgba_scene_linear);
      /* original button that created the color picker already does undo
       * push, so disable it on RNA buttons in the color picker block */
      UI_but_flag_disable(bt, UI_BUT_UNDO);
    }
    else if (bt->type == UI_BTYPE_TEXT) {
      /* Hex text input field. */
      float rgba_hex[4];
      uchar rgba_hex_uchar[4];
      char col[16];

      /* Hex code is assumed to be in sRGB space (coming from other applications, web, etc...). */
      copy_v4_v4(rgba_hex, rgba_scene_linear);
      if (from_but && !ui_but_is_color_gamma(from_but)) {
        IMB_colormanagement_scene_linear_to_srgb_v3(rgba_hex, rgba_hex);
        ui_color_picker_rgb_round(rgba_hex);
      }

      rgba_float_to_uchar(rgba_hex_uchar, rgba_hex);

      int col_len;
      if (cpicker->has_alpha) {
        col_len = SNPRINTF_RLEN(col, "#%02X%02X%02X%02X", UNPACK4_EX((uint), rgba_hex_uchar, ));
      }
      else {
        col_len = SNPRINTF_RLEN(col, "#%02X%02X%02X", UNPACK3_EX((uint), rgba_hex_uchar, ));
      }
      memcpy(bt->poin, col, col_len + 1); /* +1 offset for the # symbol. */
    }

    ui_but_update(bt);
  }
}

static void ui_colorpicker_rgba_update_cb(bContext * /*C*/, void *bt1, void * /*arg*/)
{
  uiBut *but = static_cast<uiBut *>(bt1);
  uiPopupBlockHandle *popup = but->block->handle;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  PointerRNA ptr = but->rnapoin;
  PropertyRNA *prop = but->rnaprop;
  float rgba_scene_linear[4];

  if (prop) {
    zero_v4(rgba_scene_linear);
    RNA_property_float_get_array_at_most(
        &ptr, prop, rgba_scene_linear, ARRAY_SIZE(rgba_scene_linear));
    ui_update_color_picker_buts_rgba(but, but->block, cpicker, rgba_scene_linear);
  }

  if (popup) {
    popup->menuretval = UI_RETURN_UPDATE;
  }
}

static void ui_colorpicker_hsv_update_cb(bContext * /*C*/, void *bt1, void *bt2)
{
  uiBut *but = static_cast<uiBut *>(bt1);
  uiPopupBlockHandle *popup = but->block->handle;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);

  /* Get RNA ptr/prop from the original color datablock button (bt2) since the HSV buttons (bt1)
   * do not directly point to it. */
  uiBut *color_but = static_cast<uiBut *>(bt2);
  PointerRNA color_ptr = color_but->rnapoin;
  PropertyRNA *color_prop = color_but->rnaprop;
  float rgba_scene_linear[4];

  if (color_prop) {
    zero_v4(rgba_scene_linear);
    /* Get the current RGBA color for its (optional) Alpha component,
     * then update RGB components from the current HSV values. */
    RNA_property_float_get_array_at_most(
        &color_ptr, color_prop, rgba_scene_linear, ARRAY_SIZE(rgba_scene_linear));
    ui_color_picker_hsv_to_rgb(cpicker->hsv_scene_linear, rgba_scene_linear);
    ui_update_color_picker_buts_rgba(but, but->block, cpicker, rgba_scene_linear);
  }

  if (popup) {
    popup->menuretval = UI_RETURN_UPDATE;
  }
}

static void ui_colorpicker_hex_rna_cb(bContext * /*C*/, void *bt1, void *bt2)
{
  uiBut *but = static_cast<uiBut *>(bt1);
  uiPopupBlockHandle *popup = but->block->handle;
  ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
  char hexcol[128];
  ui_but_string_get(but, hexcol, ARRAY_SIZE(hexcol));

  /* In case the current color contains an Alpha component but the Hex string does not, get the
   * current color to preserve the Alpha component.
   * Like #ui_colorpicker_hsv_update_cb, the original color datablock button (bt2) is used since
   * Hex Text Field button (bt1) doesn't directly point to it. */
  uiBut *color_but = static_cast<uiBut *>(bt2);
  PointerRNA color_ptr = color_but->rnapoin;
  PropertyRNA *color_prop = color_but->rnaprop;

  float rgba[4];
  if (color_prop) {
    zero_v4(rgba);
    RNA_property_float_get_array_at_most(&color_ptr, color_prop, rgba, ARRAY_SIZE(rgba));
  }
  /* Override current color with parsed the Hex string to preserve the original Alpha if the
   * hex string doesn't contain it. */
  hex_to_rgba(hexcol, rgba, rgba + 1, rgba + 2, rgba + 3);

  /* Hex code is assumed to be in sRGB space (coming from other applications, web, etc...). */
  if (!ui_but_is_color_gamma(but)) {
    IMB_colormanagement_srgb_to_scene_linear_v3(rgba, rgba);
    ui_color_picker_rgb_round(rgba);
  }

  ui_update_color_picker_buts_rgba(but, but->block, cpicker, rgba);

  if (popup) {
    popup->menuretval = UI_RETURN_UPDATE;
  }
}

static void ui_popup_close_cb(bContext * /*C*/, void *bt1, void * /*arg*/)
{
  uiBut *but = (uiBut *)bt1;
  uiPopupBlockHandle *popup = but->block->handle;

  if (popup) {
    ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
    BLI_assert(cpicker->is_init);
    popup->menuretval = (equals_v3v3(cpicker->hsv_perceptual, cpicker->hsv_perceptual_init) ?
                             UI_RETURN_CANCEL :
                             UI_RETURN_OK);
  }
}

static void ui_colorpicker_hide_reveal(uiBlock *block, ePickerType colormode)
{
  /* tag buttons */
  LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
    if ((bt->func == ui_colorpicker_rgba_update_cb) && (bt->type == UI_BTYPE_NUM_SLIDER) &&
        (bt->rnaindex != 3))
    {
      /* RGB sliders (color circle and alpha are always shown) */
      SET_FLAG_FROM_TEST(bt->flag, (colormode != PICKER_TYPE_RGB), UI_HIDDEN);
    }
    else if (bt->func == ui_colorpicker_hsv_update_cb) {
      /* HSV sliders */
      SET_FLAG_FROM_TEST(bt->flag, (colormode != PICKER_TYPE_HSV), UI_HIDDEN);
    }
  }
}

static void ui_colorpicker_create_mode_cb(bContext * /*C*/, void *bt1, void * /*arg*/)
{
  uiBut *bt = static_cast<uiBut *>(bt1);
  const short colormode = ui_but_value_get(bt);
  ui_colorpicker_hide_reveal(bt->block, (ePickerType)colormode);
}

#define PICKER_TOTAL_W (180.0f * UI_SCALE_FAC)
#define PICKER_BAR ((8.0f * UI_SCALE_FAC) + (6 * U.pixelsize))
#define PICKER_SPACE (8.0f * UI_SCALE_FAC)
#define PICKER_W (PICKER_TOTAL_W - PICKER_BAR - PICKER_SPACE)
#define PICKER_H PICKER_W

static void ui_colorpicker_circle(uiBlock *block,
                                  PointerRNA *ptr,
                                  PropertyRNA *prop,
                                  ColorPicker *cpicker)
{
  uiBut *bt;
  uiButHSVCube *hsv_but;

  /* HS circle */
  bt = uiDefButR_prop(block,
                      UI_BTYPE_HSVCIRCLE,
                      0,
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
                      TIP_("Color"));
  UI_but_func_set(bt, ui_colorpicker_rgba_update_cb, bt, nullptr);
  bt->custom_data = cpicker;

  /* value */
  if (U.color_picker_type == USER_CP_CIRCLE_HSL) {
    hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                             UI_BTYPE_HSVCUBE,
                                             0,
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
                                             "Lightness");
    hsv_but->gradient_type = UI_GRAD_L_ALT;
    UI_but_func_set(hsv_but, ui_colorpicker_rgba_update_cb, hsv_but, nullptr);
  }
  else {
    hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                             UI_BTYPE_HSVCUBE,
                                             0,
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
                                             CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Value"));
    hsv_but->gradient_type = UI_GRAD_V_ALT;
    UI_but_func_set(hsv_but, ui_colorpicker_rgba_update_cb, hsv_but, nullptr);
  }
  hsv_but->custom_data = cpicker;
}

static void ui_colorpicker_square(uiBlock *block,
                                  PointerRNA *ptr,
                                  PropertyRNA *prop,
                                  eButGradientType type,
                                  ColorPicker *cpicker)
{
  uiButHSVCube *hsv_but;

  BLI_assert(type <= UI_GRAD_HS);

  /* HS square */
  hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                           UI_BTYPE_HSVCUBE,
                                           0,
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
                                           TIP_("Color"));
  hsv_but->gradient_type = type;
  UI_but_func_set(hsv_but, ui_colorpicker_rgba_update_cb, hsv_but, nullptr);
  hsv_but->custom_data = cpicker;

  /* value */
  hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                           UI_BTYPE_HSVCUBE,
                                           0,
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
                                           CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Value"));
  hsv_but->gradient_type = (eButGradientType)(type + 3);
  UI_but_func_set(hsv_but, ui_colorpicker_rgba_update_cb, hsv_but, nullptr);
  hsv_but->custom_data = cpicker;
}

/* a HS circle, V slider, rgb/hsv/hex sliders */
static void ui_block_colorpicker(uiBlock *block,
                                 uiBut *from_but,
                                 float rgba_scene_linear[4],
                                 bool show_picker)
{
  /* ePickerType */
  static char colormode = 1;
  uiBut *bt;
  int picker_width;
  static char hexcol[128];
  float softmin, softmax, hardmin, hardmax, step, precision;
  int yco;
  ColorPicker *cpicker = ui_block_colorpicker_create(block);
  PointerRNA *ptr = &from_but->rnapoin;
  PropertyRNA *prop = from_but->rnaprop;

  picker_width = PICKER_TOTAL_W;

  RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);
  RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
  RNA_property_float_get_array_at_most(ptr, prop, rgba_scene_linear, 4);

  ui_color_picker_update_hsv(cpicker, from_but, rgba_scene_linear);
  cpicker->has_alpha = ui_but_color_has_alpha(from_but);

  /* when the softmax isn't defined in the RNA,
   * using very large numbers causes sRGB/linear round trip to fail. */
  if (softmax == FLT_MAX) {
    softmax = 1.0f;
  }

  switch (U.color_picker_type) {
    case USER_CP_SQUARE_SV:
      ui_colorpicker_square(block, ptr, prop, UI_GRAD_SV, cpicker);
      break;
    case USER_CP_SQUARE_HS:
      ui_colorpicker_square(block, ptr, prop, UI_GRAD_HS, cpicker);
      break;
    case USER_CP_SQUARE_HV:
      ui_colorpicker_square(block, ptr, prop, UI_GRAD_HV, cpicker);
      break;

    /* user default */
    case USER_CP_CIRCLE_HSV:
    case USER_CP_CIRCLE_HSL:
    default:
      ui_colorpicker_circle(block, ptr, prop, cpicker);
      break;
  }

  /* mode */
  yco = -1.5f * UI_UNIT_Y;
  UI_block_align_begin(block);
  bt = uiDefButC(block,
                 UI_BTYPE_ROW,
                 0,
                 IFACE_("RGB"),
                 0,
                 yco,
                 picker_width / 2,
                 UI_UNIT_Y,
                 &colormode,
                 0.0,
                 float(PICKER_TYPE_RGB),
                 TIP_("Red, Green, Blue"));
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_drawflag_disable(bt, UI_BUT_TEXT_LEFT);
  UI_but_func_set(bt, ui_colorpicker_create_mode_cb, bt, nullptr);
  bt->custom_data = cpicker;
  bt = uiDefButC(block,
                 UI_BTYPE_ROW,
                 0,
                 IFACE_((U.color_picker_type == USER_CP_CIRCLE_HSL) ? "HSL" : "HSV"),
                 picker_width / 2,
                 yco,
                 picker_width / 2,
                 UI_UNIT_Y,
                 &colormode,
                 0.0,
                 float(PICKER_TYPE_HSV),
                 (U.color_picker_type == USER_CP_CIRCLE_HSL) ? TIP_("Hue, Saturation, Lightness") :
                                                               TIP_("Hue, Saturation, Value"));
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_drawflag_disable(bt, UI_BUT_TEXT_LEFT);
  UI_but_func_set(bt, ui_colorpicker_create_mode_cb, bt, nullptr);
  bt->custom_data = cpicker;
  UI_block_align_end(block);

  yco = -3.0f * UI_UNIT_Y;

  /* NOTE: don't disable UI_BUT_UNDO for RGBA values, since these don't add undo steps. */

  /* RGB values */
  UI_block_align_begin(block);
  bt = uiDefButR_prop(block,
                      UI_BTYPE_NUM_SLIDER,
                      0,
                      IFACE_("Red:"),
                      0,
                      yco,
                      picker_width,
                      UI_UNIT_Y,
                      ptr,
                      prop,
                      0,
                      0.0,
                      0.0,
                      TIP_("Red"));
  UI_but_number_slider_step_size_set(bt, 10);
  UI_but_number_slider_precision_set(bt, 3);
  UI_but_func_set(bt, ui_colorpicker_rgba_update_cb, bt, nullptr);
  bt->custom_data = cpicker;
  bt = uiDefButR_prop(block,
                      UI_BTYPE_NUM_SLIDER,
                      0,
                      IFACE_("Green:"),
                      0,
                      yco -= UI_UNIT_Y,
                      picker_width,
                      UI_UNIT_Y,
                      ptr,
                      prop,
                      1,
                      0.0,
                      0.0,
                      TIP_("Green"));
  UI_but_number_slider_step_size_set(bt, 10);
  UI_but_number_slider_precision_set(bt, 3);
  UI_but_func_set(bt, ui_colorpicker_rgba_update_cb, bt, nullptr);
  bt->custom_data = cpicker;
  bt = uiDefButR_prop(block,
                      UI_BTYPE_NUM_SLIDER,
                      0,
                      IFACE_("Blue:"),
                      0,
                      yco -= UI_UNIT_Y,
                      picker_width,
                      UI_UNIT_Y,
                      ptr,
                      prop,
                      2,
                      0.0,
                      0.0,
                      TIP_("Blue"));
  UI_but_number_slider_step_size_set(bt, 10);
  UI_but_number_slider_precision_set(bt, 3);
  UI_but_func_set(bt, ui_colorpicker_rgba_update_cb, bt, nullptr);
  bt->custom_data = cpicker;

  /* Could use:
   * uiItemFullR(col, ptr, prop, -1, 0, UI_ITEM_R_EXPAND | UI_ITEM_R_SLIDER, "", ICON_NONE);
   * but need to use UI_but_func_set for updating other fake buttons */

  /* HSV values */
  yco = -3.0f * UI_UNIT_Y;
  bt = uiDefButF(block,
                 UI_BTYPE_NUM_SLIDER,
                 0,
                 IFACE_("Hue:"),
                 0,
                 yco,
                 picker_width,
                 UI_UNIT_Y,
                 cpicker->hsv_scene_linear,
                 0.0,
                 1.0,
                 TIP_("Hue"));
  UI_but_number_slider_step_size_set(bt, 10);
  UI_but_number_slider_precision_set(bt, 3);
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_colorpicker_hsv_update_cb, bt, from_but);
  bt->custom_data = cpicker;
  bt = uiDefButF(block,
                 UI_BTYPE_NUM_SLIDER,
                 0,
                 IFACE_("Saturation:"),
                 0,
                 yco -= UI_UNIT_Y,
                 picker_width,
                 UI_UNIT_Y,
                 cpicker->hsv_scene_linear + 1,
                 0.0,
                 1.0,
                 TIP_("Saturation"));
  UI_but_number_slider_step_size_set(bt, 10);
  UI_but_number_slider_precision_set(bt, 3);
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_colorpicker_hsv_update_cb, bt, from_but);
  bt->custom_data = cpicker;
  if (U.color_picker_type == USER_CP_CIRCLE_HSL) {
    bt = uiDefButF(block,
                   UI_BTYPE_NUM_SLIDER,
                   0,
                   IFACE_("Lightness:"),
                   0,
                   yco -= UI_UNIT_Y,
                   picker_width,
                   UI_UNIT_Y,
                   cpicker->hsv_scene_linear + 2,
                   0.0,
                   1.0,
                   TIP_("Lightness"));
    UI_but_number_slider_step_size_set(bt, 10);
    UI_but_number_slider_precision_set(bt, 3);
  }
  else {
    bt = uiDefButF(block,
                   UI_BTYPE_NUM_SLIDER,
                   0,
                   CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "Value:"),
                   0,
                   yco -= UI_UNIT_Y,
                   picker_width,
                   UI_UNIT_Y,
                   cpicker->hsv_scene_linear + 2,
                   0.0,
                   softmax,
                   CTX_TIP_(BLT_I18NCONTEXT_COLOR, "Value"));
  }
  UI_but_number_slider_step_size_set(bt, 10);
  UI_but_number_slider_precision_set(bt, 3);
  UI_but_flag_disable(bt, UI_BUT_UNDO);

  bt->hardmax = hardmax; /* Not common but RGB may be over 1.0. */
  UI_but_func_set(bt, ui_colorpicker_hsv_update_cb, bt, from_but);
  bt->custom_data = cpicker;

  if (cpicker->has_alpha) {
    bt = uiDefButR_prop(block,
                        UI_BTYPE_NUM_SLIDER,
                        0,
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
    UI_but_number_slider_step_size_set(bt, 10);
    UI_but_number_slider_precision_set(bt, 3);
    UI_but_func_set(bt, ui_colorpicker_rgba_update_cb, bt, nullptr);
    bt->custom_data = cpicker;
  }
  else {
    rgba_scene_linear[3] = 1.0f;
  }

  UI_block_align_end(block);

  /* Hex color is in sRGB space. */
  float rgba_hex[4];
  uchar rgba_hex_uchar[4];

  copy_v4_v4(rgba_hex, rgba_scene_linear);

  if (!ui_but_is_color_gamma(from_but)) {
    IMB_colormanagement_scene_linear_to_srgb_v3(rgba_hex, rgba_hex);
    ui_color_picker_rgb_round(rgba_hex);
  }

  rgba_float_to_uchar(rgba_hex_uchar, rgba_hex);

  if (cpicker->has_alpha) {
    SNPRINTF(hexcol, "#%02X%02X%02X%02X", UNPACK4_EX((uint), rgba_hex_uchar, ));
  }
  else {
    SNPRINTF(hexcol, "#%02X%02X%02X", UNPACK3_EX((uint), rgba_hex_uchar, ));
  }

  yco -= UI_UNIT_Y * 1.5f;

  const int label_width = picker_width * 0.15f;
  const int eyedropper_offset = show_picker ? UI_UNIT_X * 1.25f : 0;
  const int text_width = picker_width - label_width - eyedropper_offset;

  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           IFACE_("Hex"),
           0,
           yco,
           label_width,
           UI_UNIT_Y,
           nullptr,
           0.0,
           0.0,
           nullptr);

  bt = uiDefBut(block,
                UI_BTYPE_TEXT,
                0,
                IFACE_(""),
                label_width,
                yco,
                text_width,
                UI_UNIT_Y,
                hexcol,
                0,
                cpicker->has_alpha ? 10 : 8,
                nullptr);
  const auto bt_tooltip_func = [](bContext & /*C*/, uiTooltipData &tip, void *has_alpha_ptr) {
    const bool *has_alpha = static_cast<bool *>(has_alpha_ptr);
    if (*has_alpha) {
      UI_tooltip_text_field_add(tip,
                                "Hex triplet for color with alpha (#RRGGBBAA).",
                                {},
                                UI_TIP_STYLE_HEADER,
                                UI_TIP_LC_NORMAL,
                                false);
    }
    else {
      UI_tooltip_text_field_add(tip,
                                "Hex triplet for color (#RRGGBB).",
                                {},
                                UI_TIP_STYLE_HEADER,
                                UI_TIP_LC_NORMAL,
                                false);
    }
    UI_tooltip_text_field_add(
        tip, "Gamma corrected", {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL, false);
  };
  UI_but_func_tooltip_custom_set(
      bt, bt_tooltip_func, static_cast<void *>(&cpicker->has_alpha), nullptr);
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_colorpicker_hex_rna_cb, bt, from_but);
  bt->custom_data = cpicker;

  if (show_picker) {
    bt = uiDefIconButO(block,
                       UI_BTYPE_BUT,
                       "UI_OT_eyedropper_color",
                       WM_OP_INVOKE_DEFAULT,
                       ICON_EYEDROPPER,
                       picker_width - UI_UNIT_X,
                       yco,
                       UI_UNIT_X,
                       UI_UNIT_Y,
                       nullptr);
    UI_but_flag_disable(bt, UI_BUT_UNDO);
    UI_but_drawflag_disable(bt, UI_BUT_ICON_LEFT);
    UI_but_func_set(bt, ui_popup_close_cb, bt, nullptr);
    bt->custom_data = cpicker;
  }

  ui_colorpicker_hide_reveal(block, (ePickerType)colormode);
}

static int ui_colorpicker_wheel_cb(const bContext * /*C*/, uiBlock *block, const wmEvent *event)
{
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
    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      if (but->type == UI_BTYPE_HSVCUBE && but->active == nullptr) {
        uiPopupBlockHandle *popup = block->handle;
        ColorPicker *cpicker = static_cast<ColorPicker *>(but->custom_data);
        float *hsv_perceptual = cpicker->hsv_perceptual;

        /* Get the RGBA Color. */
        float rgba_perceptual[4];
        ui_but_v4_get(but, rgba_perceptual);
        ui_scene_linear_to_perceptual_space(but, rgba_perceptual);

        /* Convert it to HSV. */
        ui_color_picker_rgb_to_hsv_compat(rgba_perceptual, hsv_perceptual);

        /* Increment/Decrement its value from mouse wheel input. */
        hsv_perceptual[2] = clamp_f(hsv_perceptual[2] + add, 0.0f, 1.0f);

        /* Convert it to linear space RGBA, and apply it back to the button. */
        float rgba_scene_linear[4];
        rgba_scene_linear[3] = rgba_perceptual[3]; /* Transfer Alpha component. */
        ui_color_picker_hsv_to_rgb(hsv_perceptual, rgba_scene_linear);
        ui_perceptual_to_scene_linear_space(but, rgba_scene_linear);
        ui_but_v4_set(but, rgba_scene_linear);

        /* Update all other Color Picker buttons to reflect the color change. */
        ui_update_color_picker_buts_rgba(but, block, cpicker, rgba_scene_linear);
        if (popup) {
          popup->menuretval = UI_RETURN_UPDATE;
        }

        return 1;
      }
    }
  }
  return 0;
}

uiBlock *ui_block_func_COLOR(bContext *C, uiPopupBlockHandle *handle, void *arg_but)
{
  uiBut *but = static_cast<uiBut *>(arg_but);
  uiBlock *block;

  block = UI_block_begin(C, handle->region, __func__, UI_EMBOSS);

  if (ui_but_is_color_gamma(but)) {
    block->is_color_gamma_picker = true;
  }

  copy_v3_v3(handle->retvec, but->editvec);

  ui_block_colorpicker(block, but, handle->retvec, true);

  block->flag = UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_OUT_1 | UI_BLOCK_MOVEMOUSE_QUIT;
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  UI_block_bounds_set_normal(block, 0.5 * UI_UNIT_X);

  block->block_event_func = ui_colorpicker_wheel_cb;
  block->direction = UI_DIR_UP;

  return block;
}

ColorPicker *ui_block_colorpicker_create(uiBlock *block)
{
  ColorPicker *cpicker = MEM_cnew<ColorPicker>(__func__);
  BLI_addhead(&block->color_pickers.list, cpicker);

  return cpicker;
}

/** \} */
