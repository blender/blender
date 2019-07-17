/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 *
 * Color Picker Region & Color Utils
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_context.h"

#include "WM_types.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "ED_screen.h"

#include "IMB_colormanagement.h"

#include "interface_intern.h"

enum ePickerType {
  PICKER_TYPE_RGB = 0,
  PICKER_TYPE_HSV = 1,
  PICKER_TYPE_HEX = 2,
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
    if (fabsf(rgb[i]) < 1e-6f) {
      rgb[i] = 0.0f;
    }
    else if (fabsf(1.0f - rgb[i]) < 1e-6f) {
      rgb[i] = 1.0f;
    }
  }
}

void ui_rgb_to_color_picker_compat_v(const float rgb[3], float r_cp[3])
{
  switch (U.color_picker_type) {
    case USER_CP_CIRCLE_HSL:
      rgb_to_hsl_compat_v(rgb, r_cp);
      break;
    default:
      rgb_to_hsv_compat_v(rgb, r_cp);
      break;
  }
}

void ui_rgb_to_color_picker_v(const float rgb[3], float r_cp[3])
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

void ui_color_picker_to_rgb_v(const float r_cp[3], float rgb[3])
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

void ui_color_picker_to_rgb(float r_cp0, float r_cp1, float r_cp2, float *r, float *g, float *b)
{
  switch (U.color_picker_type) {
    case USER_CP_CIRCLE_HSL:
      hsl_to_rgb(r_cp0, r_cp1, r_cp2, r, g, b);
      break;
    default:
      hsv_to_rgb(r_cp0, r_cp1, r_cp2, r, g, b);
      break;
  }
}

/* Returns true if the button is for a color with gamma baked in,
 * or if it's a color picker for such a button. */
bool ui_but_is_color_gamma(uiBut *but)
{
  if (but->rnaprop) {
    if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
      return true;
    }
  }

  return but->block->is_color_gamma_picker;
}

void ui_scene_linear_to_color_picker_space(uiBut *but, float rgb[3])
{
  /* Map to color picking space for HSV values and HSV cube/circle,
   * assuming it is more perceptually linear then the scene linear
   * space for intuitive color picking. */
  if (!ui_but_is_color_gamma(but)) {
    IMB_colormanagement_scene_linear_to_color_picking_v3(rgb);
    ui_color_picker_rgb_round(rgb);
  }
}

void ui_color_picker_to_scene_linear_space(uiBut *but, float rgb[3])
{
  if (!ui_but_is_color_gamma(but)) {
    IMB_colormanagement_color_picking_to_scene_linear_v3(rgb);
    ui_color_picker_rgb_round(rgb);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Picker
 * \{ */

/* for picker, while editing hsv */
void ui_but_hsv_set(uiBut *but)
{
  float col[3];
  ColorPicker *cpicker = but->custom_data;
  float *hsv = cpicker->color_data;

  ui_color_picker_to_rgb_v(hsv, col);

  ui_but_v3_set(but, col);
}

/* Updates all buttons who share the same color picker as the one passed
 * also used by small picker, be careful with name checks below... */
static void ui_update_color_picker_buts_rgb(uiBut *from_but,
                                            uiBlock *block,
                                            ColorPicker *cpicker,
                                            const float rgb[3])
{
  uiBut *bt;
  float *hsv = cpicker->color_data;

  /* Convert from RGB to HSV in perceptually linear space. */
  float tmp[3];
  copy_v3_v3(tmp, rgb);
  if (from_but) {
    ui_scene_linear_to_color_picker_space(from_but, tmp);
  }
  ui_rgb_to_color_picker_compat_v(tmp, hsv);

  /* this updates button strings,
   * is hackish... but button pointers are on stack of caller function */
  for (bt = block->buttons.first; bt; bt = bt->next) {
    if (bt->custom_data != cpicker) {
      continue;
    }

    if (bt->rnaprop) {
      ui_but_v3_set(bt, rgb);

      /* original button that created the color picker already does undo
       * push, so disable it on RNA buttons in the color picker block */
      UI_but_flag_disable(bt, UI_BUT_UNDO);
    }
    else if (STREQ(bt->str, "Hex: ")) {
      float rgb_hex[3];
      uchar rgb_hex_uchar[3];
      char col[16];

      /* Hex code is assumed to be in sRGB space
       * (coming from other applications, web, etc) */
      copy_v3_v3(rgb_hex, rgb);
      if (from_but && !ui_but_is_color_gamma(from_but)) {
        IMB_colormanagement_scene_linear_to_srgb_v3(rgb_hex);
        ui_color_picker_rgb_round(rgb_hex);
      }

      rgb_float_to_uchar(rgb_hex_uchar, rgb_hex);
      BLI_snprintf(col, sizeof(col), "%02X%02X%02X", UNPACK3_EX((uint), rgb_hex_uchar, ));

      strcpy(bt->poin, col);
    }
    else if (bt->str[1] == ' ') {
      if (bt->str[0] == 'R') {
        ui_but_value_set(bt, rgb[0]);
      }
      else if (bt->str[0] == 'G') {
        ui_but_value_set(bt, rgb[1]);
      }
      else if (bt->str[0] == 'B') {
        ui_but_value_set(bt, rgb[2]);
      }
      else if (bt->str[0] == 'H') {
        ui_but_value_set(bt, hsv[0]);
      }
      else if (bt->str[0] == 'S') {
        ui_but_value_set(bt, hsv[1]);
      }
      else if (bt->str[0] == 'V') {
        ui_but_value_set(bt, hsv[2]);
      }
      else if (bt->str[0] == 'L') {
        ui_but_value_set(bt, hsv[2]);
      }
    }

    ui_but_update(bt);
  }
}

static void ui_colorpicker_rna_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
  uiBut *but = (uiBut *)bt1;
  uiPopupBlockHandle *popup = but->block->handle;
  PropertyRNA *prop = but->rnaprop;
  PointerRNA ptr = but->rnapoin;
  float rgb[4];

  if (prop) {
    RNA_property_float_get_array(&ptr, prop, rgb);
    ui_update_color_picker_buts_rgb(but, but->block, but->custom_data, rgb);
  }

  if (popup) {
    popup->menuretval = UI_RETURN_UPDATE;
  }
}

static void ui_color_wheel_rna_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
  uiBut *but = (uiBut *)bt1;
  uiPopupBlockHandle *popup = but->block->handle;
  float rgb[3];
  ColorPicker *cpicker = but->custom_data;
  float *hsv = cpicker->color_data;

  ui_color_picker_to_rgb_v(hsv, rgb);

  /* hsv is saved in perceptually linear space so convert back */
  ui_color_picker_to_scene_linear_space(but, rgb);

  ui_update_color_picker_buts_rgb(but, but->block, cpicker, rgb);

  if (popup) {
    popup->menuretval = UI_RETURN_UPDATE;
  }
}

static void ui_colorpicker_hex_rna_cb(bContext *UNUSED(C), void *bt1, void *hexcl)
{
  uiBut *but = (uiBut *)bt1;
  uiPopupBlockHandle *popup = but->block->handle;
  ColorPicker *cpicker = but->custom_data;
  char *hexcol = (char *)hexcl;
  float rgb[3];

  hex_to_rgb(hexcol, rgb, rgb + 1, rgb + 2);

  /* Hex code is assumed to be in sRGB space (coming from other applications, web, etc) */
  if (!ui_but_is_color_gamma(but)) {
    IMB_colormanagement_srgb_to_scene_linear_v3(rgb);
    ui_color_picker_rgb_round(rgb);
  }

  ui_update_color_picker_buts_rgb(but, but->block, cpicker, rgb);

  if (popup) {
    popup->menuretval = UI_RETURN_UPDATE;
  }
}

static void ui_popup_close_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
  uiBut *but = (uiBut *)bt1;
  uiPopupBlockHandle *popup = but->block->handle;

  if (popup) {
    ColorPicker *cpicker = but->custom_data;
    BLI_assert(cpicker->is_init);
    popup->menuretval = (equals_v3v3(cpicker->color_data, cpicker->color_data_init) ?
                             UI_RETURN_CANCEL :
                             UI_RETURN_OK);
  }
}

static void ui_colorpicker_hide_reveal(uiBlock *block, enum ePickerType colormode)
{
  /* tag buttons */
  for (uiBut *bt = block->buttons.first; bt; bt = bt->next) {
    if ((bt->func == ui_colorpicker_rna_cb) && (bt->type == UI_BTYPE_NUM_SLIDER) &&
        (bt->rnaindex != 3)) {
      /* RGB sliders (color circle and alpha are always shown) */
      SET_FLAG_FROM_TEST(bt->flag, (colormode != PICKER_TYPE_RGB), UI_HIDDEN);
    }
    else if (bt->func == ui_color_wheel_rna_cb) {
      /* HSV sliders */
      SET_FLAG_FROM_TEST(bt->flag, (colormode != PICKER_TYPE_HSV), UI_HIDDEN);
    }
    else if (bt->func == ui_colorpicker_hex_rna_cb || bt->type == UI_BTYPE_LABEL) {
      /* HEX input or gamma correction status label */
      SET_FLAG_FROM_TEST(bt->flag, (colormode != PICKER_TYPE_HEX), UI_HIDDEN);
    }
  }
}

static void ui_colorpicker_create_mode_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
  uiBut *bt = bt1;
  short colormode = ui_but_value_get(bt);
  ui_colorpicker_hide_reveal(bt->block, colormode);
}

#define PICKER_H (7.5f * U.widget_unit)
#define PICKER_W (7.5f * U.widget_unit)
#define PICKER_SPACE (0.3f * U.widget_unit)
#define PICKER_BAR (0.7f * U.widget_unit)

#define PICKER_TOTAL_W (PICKER_W + PICKER_SPACE + PICKER_BAR)

static void ui_colorpicker_circle(uiBlock *block,
                                  PointerRNA *ptr,
                                  PropertyRNA *prop,
                                  ColorPicker *cpicker)
{
  uiBut *bt;

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
                      0.0,
                      0,
                      TIP_("Color"));
  UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
  bt->custom_data = cpicker;

  /* value */
  if (U.color_picker_type == USER_CP_CIRCLE_HSL) {
    bt = uiDefButR_prop(block,
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
                        UI_GRAD_L_ALT,
                        0,
                        "Lightness");
    UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
  }
  else {
    bt = uiDefButR_prop(block,
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
                        UI_GRAD_V_ALT,
                        0,
                        TIP_("Value"));
    UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
  }
  bt->custom_data = cpicker;
}

static void ui_colorpicker_square(
    uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, int type, ColorPicker *cpicker)
{
  uiBut *bt;
  int bartype = type + 3;

  /* HS square */
  bt = uiDefButR_prop(block,
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
                      type,
                      0,
                      TIP_("Color"));
  UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
  bt->custom_data = cpicker;

  /* value */
  bt = uiDefButR_prop(block,
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
                      bartype,
                      0,
                      TIP_("Value"));
  UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
  bt->custom_data = cpicker;
}

/* a HS circle, V slider, rgb/hsv/hex sliders */
static void ui_block_colorpicker(uiBlock *block, uiBut *from_but, float rgba[4], bool show_picker)
{
  /* ePickerType */
  static char colormode = 1;
  uiBut *bt;
  int width, butwidth;
  static char hexcol[128];
  float softmin, softmax, hardmin, hardmax, step, precision;
  int yco;
  ColorPicker *cpicker = ui_block_colorpicker_create(block);
  float *hsv = cpicker->color_data;
  PointerRNA *ptr = &from_but->rnapoin;
  PropertyRNA *prop = from_but->rnaprop;

  width = PICKER_TOTAL_W;
  butwidth = width - 1.5f * UI_UNIT_X;

  /* sneaky way to check for alpha */
  rgba[3] = FLT_MAX;

  RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);
  RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
  RNA_property_float_get_array(ptr, prop, rgba);

  float rgb_perceptual[3];
  copy_v3_v3(rgb_perceptual, rgba);
  ui_scene_linear_to_color_picker_space(from_but, rgb_perceptual);
  ui_rgb_to_color_picker_v(rgb_perceptual, hsv);
  if (cpicker->is_init == false) {
    copy_v3_v3(cpicker->color_data_init, cpicker->color_data);
    cpicker->is_init = true;
  }

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
                 width / 3,
                 UI_UNIT_Y,
                 &colormode,
                 0.0,
                 (float)PICKER_TYPE_RGB,
                 0,
                 0,
                 "");
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_colorpicker_create_mode_cb, bt, NULL);
  bt->custom_data = cpicker;
  bt = uiDefButC(block,
                 UI_BTYPE_ROW,
                 0,
                 IFACE_((U.color_picker_type == USER_CP_CIRCLE_HSL) ? "HSL" : "HSV"),
                 width / 3,
                 yco,
                 width / 3,
                 UI_UNIT_Y,
                 &colormode,
                 0.0,
                 PICKER_TYPE_HSV,
                 0,
                 0,
                 "");
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_colorpicker_create_mode_cb, bt, NULL);
  bt->custom_data = cpicker;
  bt = uiDefButC(block,
                 UI_BTYPE_ROW,
                 0,
                 IFACE_("Hex"),
                 2 * width / 3,
                 yco,
                 width / 3,
                 UI_UNIT_Y,
                 &colormode,
                 0.0,
                 PICKER_TYPE_HEX,
                 0,
                 0,
                 "");
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_colorpicker_create_mode_cb, bt, NULL);
  bt->custom_data = cpicker;
  UI_block_align_end(block);

  yco = -3.0f * UI_UNIT_Y;
  if (show_picker) {
    bt = uiDefIconButO(block,
                       UI_BTYPE_BUT,
                       "UI_OT_eyedropper_color",
                       WM_OP_INVOKE_DEFAULT,
                       ICON_EYEDROPPER,
                       butwidth + 10,
                       yco,
                       UI_UNIT_X,
                       UI_UNIT_Y,
                       NULL);
    UI_but_flag_disable(bt, UI_BUT_UNDO);
    UI_but_drawflag_disable(bt, UI_BUT_ICON_LEFT);
    UI_but_func_set(bt, ui_popup_close_cb, bt, NULL);
    bt->custom_data = cpicker;
  }

  /* Note: don't disable UI_BUT_UNDO for RGBA values, since these don't add undo steps. */

  /* RGB values */
  UI_block_align_begin(block);
  bt = uiDefButR_prop(block,
                      UI_BTYPE_NUM_SLIDER,
                      0,
                      IFACE_("R:"),
                      0,
                      yco,
                      butwidth,
                      UI_UNIT_Y,
                      ptr,
                      prop,
                      0,
                      0.0,
                      0.0,
                      0,
                      3,
                      TIP_("Red"));
  UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
  bt->custom_data = cpicker;
  bt = uiDefButR_prop(block,
                      UI_BTYPE_NUM_SLIDER,
                      0,
                      IFACE_("G:"),
                      0,
                      yco -= UI_UNIT_Y,
                      butwidth,
                      UI_UNIT_Y,
                      ptr,
                      prop,
                      1,
                      0.0,
                      0.0,
                      0,
                      3,
                      TIP_("Green"));
  UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
  bt->custom_data = cpicker;
  bt = uiDefButR_prop(block,
                      UI_BTYPE_NUM_SLIDER,
                      0,
                      IFACE_("B:"),
                      0,
                      yco -= UI_UNIT_Y,
                      butwidth,
                      UI_UNIT_Y,
                      ptr,
                      prop,
                      2,
                      0.0,
                      0.0,
                      0,
                      3,
                      TIP_("Blue"));
  UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
  bt->custom_data = cpicker;

  /* Could use:
   * uiItemFullR(col, ptr, prop, -1, 0, UI_ITEM_R_EXPAND | UI_ITEM_R_SLIDER, "", ICON_NONE);
   * but need to use UI_but_func_set for updating other fake buttons */

  /* HSV values */
  yco = -3.0f * UI_UNIT_Y;
  UI_block_align_begin(block);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM_SLIDER,
                 0,
                 IFACE_("H:"),
                 0,
                 yco,
                 butwidth,
                 UI_UNIT_Y,
                 hsv,
                 0.0,
                 1.0,
                 10,
                 3,
                 TIP_("Hue"));
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_color_wheel_rna_cb, bt, hsv);
  bt->custom_data = cpicker;
  bt = uiDefButF(block,
                 UI_BTYPE_NUM_SLIDER,
                 0,
                 IFACE_("S:"),
                 0,
                 yco -= UI_UNIT_Y,
                 butwidth,
                 UI_UNIT_Y,
                 hsv + 1,
                 0.0,
                 1.0,
                 10,
                 3,
                 TIP_("Saturation"));
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_color_wheel_rna_cb, bt, hsv);
  bt->custom_data = cpicker;
  if (U.color_picker_type == USER_CP_CIRCLE_HSL) {
    bt = uiDefButF(block,
                   UI_BTYPE_NUM_SLIDER,
                   0,
                   IFACE_("L:"),
                   0,
                   yco -= UI_UNIT_Y,
                   butwidth,
                   UI_UNIT_Y,
                   hsv + 2,
                   0.0,
                   1.0,
                   10,
                   3,
                   TIP_("Lightness"));
  }
  else {
    bt = uiDefButF(block,
                   UI_BTYPE_NUM_SLIDER,
                   0,
                   IFACE_("V:"),
                   0,
                   yco -= UI_UNIT_Y,
                   butwidth,
                   UI_UNIT_Y,
                   hsv + 2,
                   0.0,
                   softmax,
                   10,
                   3,
                   TIP_("Value"));
  }
  UI_but_flag_disable(bt, UI_BUT_UNDO);

  bt->hardmax = hardmax; /* not common but rgb  may be over 1.0 */
  UI_but_func_set(bt, ui_color_wheel_rna_cb, bt, hsv);
  bt->custom_data = cpicker;

  UI_block_align_end(block);

  if (rgba[3] != FLT_MAX) {
    bt = uiDefButR_prop(block,
                        UI_BTYPE_NUM_SLIDER,
                        0,
                        IFACE_("A: "),
                        0,
                        yco -= UI_UNIT_Y,
                        butwidth,
                        UI_UNIT_Y,
                        ptr,
                        prop,
                        3,
                        0.0,
                        0.0,
                        0,
                        3,
                        TIP_("Alpha"));
    UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
    bt->custom_data = cpicker;
  }
  else {
    rgba[3] = 1.0f;
  }

  /* Hex color is in sRGB space. */
  float rgb_hex[3];
  uchar rgb_hex_uchar[3];

  copy_v3_v3(rgb_hex, rgba);

  if (!ui_but_is_color_gamma(from_but)) {
    IMB_colormanagement_scene_linear_to_srgb_v3(rgb_hex);
    ui_color_picker_rgb_round(rgb_hex);
  }

  rgb_float_to_uchar(rgb_hex_uchar, rgb_hex);
  BLI_snprintf(hexcol, sizeof(hexcol), "%02X%02X%02X", UNPACK3_EX((uint), rgb_hex_uchar, ));

  yco = -3.0f * UI_UNIT_Y;
  bt = uiDefBut(block,
                UI_BTYPE_TEXT,
                0,
                IFACE_("Hex: "),
                0,
                yco,
                butwidth,
                UI_UNIT_Y,
                hexcol,
                0,
                8,
                0,
                0,
                TIP_("Hex triplet for color (#RRGGBB)"));
  UI_but_flag_disable(bt, UI_BUT_UNDO);
  UI_but_func_set(bt, ui_colorpicker_hex_rna_cb, bt, hexcol);
  bt->custom_data = cpicker;
  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           IFACE_("(Gamma Corrected)"),
           0,
           yco - UI_UNIT_Y,
           butwidth,
           UI_UNIT_Y,
           NULL,
           0.0,
           0.0,
           0,
           0,
           "");

  ui_colorpicker_hide_reveal(block, colormode);
}

static int ui_colorpicker_small_wheel_cb(const bContext *UNUSED(C),
                                         uiBlock *block,
                                         const wmEvent *event)
{
  float add = 0.0f;

  if (event->type == WHEELUPMOUSE) {
    add = 0.05f;
  }
  else if (event->type == WHEELDOWNMOUSE) {
    add = -0.05f;
  }

  if (add != 0.0f) {
    uiBut *but;

    for (but = block->buttons.first; but; but = but->next) {
      if (but->type == UI_BTYPE_HSVCUBE && but->active == NULL) {
        uiPopupBlockHandle *popup = block->handle;
        float rgb[3];
        ColorPicker *cpicker = but->custom_data;
        float *hsv = cpicker->color_data;

        ui_but_v3_get(but, rgb);
        ui_scene_linear_to_color_picker_space(but, rgb);
        ui_rgb_to_color_picker_compat_v(rgb, hsv);

        hsv[2] = clamp_f(hsv[2] + add, 0.0f, 1.0f);

        ui_color_picker_to_rgb_v(hsv, rgb);
        ui_color_picker_to_scene_linear_space(but, rgb);
        ui_but_v3_set(but, rgb);

        ui_update_color_picker_buts_rgb(but, block, cpicker, rgb);
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
  uiBut *but = arg_but;
  uiBlock *block;
  bool show_picker = true;

  block = UI_block_begin(C, handle->region, __func__, UI_EMBOSS);

  if (ui_but_is_color_gamma(but)) {
    block->is_color_gamma_picker = true;
  }

  if (but->block) {
    /* if color block is invoked from a popup we wouldn't be able to set color properly
     * this is because color picker will close popups first and then will try to figure
     * out active button RNA, and of course it'll fail
     */
    show_picker = (but->block->flag & UI_BLOCK_POPUP) == 0;
  }

  copy_v3_v3(handle->retvec, but->editvec);

  ui_block_colorpicker(block, but, handle->retvec, show_picker);

  block->flag = UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_OUT_1 | UI_BLOCK_MOVEMOUSE_QUIT;
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  UI_block_bounds_set_normal(block, 0.5 * UI_UNIT_X);

  block->block_event_func = ui_colorpicker_small_wheel_cb;

  /* and lets go */
  block->direction = UI_DIR_UP;

  return block;
}

ColorPicker *ui_block_colorpicker_create(struct uiBlock *block)
{
  ColorPicker *cpicker = MEM_callocN(sizeof(ColorPicker), "color_picker");
  BLI_addhead(&block->color_pickers.list, cpicker);

  return cpicker;
}

/** \} */
