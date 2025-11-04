/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * ToolTip Region and Construction
 */

/* TODO(@ideasman42):
 * We may want to have a higher level API that initializes a timer,
 * checks for mouse motion and clears the tool-tip afterwards.
 * We never want multiple tool-tips at once
 * so this could be handled on the window / window-manager level.
 *
 * For now it's not a priority, so leave as-is.
 */

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <fmt/format.h>

#include "AS_essentials_library.hh"

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_image.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_paint.hh"
#include "BKE_path_templates.hh"
#include "BKE_screen.hh"
#include "BKE_vfont.hh"

#include "BIF_glutil.hh"

#include "DNA_vfont_types.h"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_state.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_thumbs.hh"

#include "MOV_read.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "BLF_api.hh"
#include "BLT_translation.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern_run.hh"
#endif

#include "ED_screen.hh"

#include "interface_intern.hh"
#include "interface_regions_intern.hh"

/* Portions of line height. */
#define UI_TIP_SPACER 0.3f
#define UI_TIP_PADDING_X 1.95f
#define UI_TIP_PADDING_Y 1.28f

#define UI_TIP_MAXWIDTH 600

struct uiTooltipFormat {
  uiTooltipStyle style;
  uiTooltipColorID color_id;
};

struct uiTooltipField {
  std::string text;
  std::string text_suffix;
  struct {
    /** X cursor position at the end of the last line. */
    uint x_pos;
    /** Number of lines, 1 or more with word-wrap. */
    uint lines;
  } geom;
  uiTooltipFormat format;
  std::optional<uiTooltipImage> image;
};

struct uiTooltipData {
  rcti bbox;
  blender::Vector<uiTooltipField> fields;
  uiFontStyle fstyle;
  int wrap_width;
  int toth, lineh;
};

BLI_STATIC_ASSERT(int(UI_TIP_LC_MAX) == int(UI_TIP_LC_ALERT) + 1, "invalid lc-max");

void UI_tooltip_text_field_add(uiTooltipData &data,
                               std::string text,
                               std::string suffix,
                               const uiTooltipStyle style,
                               const uiTooltipColorID color_id,
                               const bool is_pad)
{
  if (is_pad) {
    /* Add a spacer field before this one. */
    UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL, false);
  }

  uiTooltipField field{};
  field.format.style = style;
  field.format.color_id = color_id;
  field.text = std::move(text);
  field.text_suffix = std::move(suffix);
  data.fields.append(std::move(field));
}

void UI_tooltip_image_field_add(uiTooltipData &data, const uiTooltipImage &image_data)
{
  uiTooltipField field{};
  field.format.style = UI_TIP_STYLE_IMAGE;
  field.image = image_data;
  field.image->ibuf = IMB_dupImBuf(image_data.ibuf);
  data.fields.append(std::move(field));
}

/* -------------------------------------------------------------------- */
/** \name ToolTip Callbacks (Draw & Free)
 * \{ */

static void color_blend_f3_f3(float dest[3], const float source[3], const float fac)
{
  if (fac != 0.0f) {
    dest[0] = (1.0f - fac) * dest[0] + (fac * source[0]);
    dest[1] = (1.0f - fac) * dest[1] + (fac * source[1]);
    dest[2] = (1.0f - fac) * dest[2] + (fac * source[2]);
  }
}

static void ui_tooltip_region_draw_cb(const bContext * /*C*/, ARegion *region)
{
  uiTooltipData *data = static_cast<uiTooltipData *>(region->regiondata);
  const float pad_x = data->lineh * UI_TIP_PADDING_X;
  const float pad_y = data->lineh * UI_TIP_PADDING_Y;
  const uiWidgetColors *theme = ui_tooltip_get_theme();
  rcti bbox = data->bbox;
  float tip_colors[UI_TIP_LC_MAX][3];
  uchar drawcol[4] = {0, 0, 0, 255}; /* to store color in while drawing (alpha is always 255) */

  /* The color from the theme. */
  float *main_color = tip_colors[UI_TIP_LC_MAIN];
  float *value_color = tip_colors[UI_TIP_LC_VALUE];
  float *active_color = tip_colors[UI_TIP_LC_ACTIVE];
  float *normal_color = tip_colors[UI_TIP_LC_NORMAL];
  float *python_color = tip_colors[UI_TIP_LC_PYTHON];
  float *alert_color = tip_colors[UI_TIP_LC_ALERT];

  float background_color[3];

  wmOrtho2_region_pixelspace(region);

  /* Draw background. */
  ui_draw_tooltip_background(UI_style_get(), nullptr, &bbox);

  /* set background_color */
  rgb_uchar_to_float(background_color, theme->inner);

  /* `normal_color` is just tooltip text color. */
  rgb_uchar_to_float(main_color, theme->text);
  copy_v3_v3(normal_color, main_color);

  /* `value_color` mixes with some background for less strength. */
  copy_v3_v3(value_color, main_color);
  color_blend_f3_f3(value_color, background_color, 0.2f);

  /* `python_color` mixes with more background to be even dimmer. */
  copy_v3_v3(python_color, main_color);
  color_blend_f3_f3(python_color, background_color, 0.5f);

  /* `active_color` is a light blue, push a bit toward text color. */
  active_color[0] = 0.4f;
  active_color[1] = 0.55f;
  active_color[2] = 0.75f;
  color_blend_f3_f3(active_color, main_color, 0.3f);

  /* `alert_color` is red, push a bit toward text color. */
  UI_GetThemeColor3fv(TH_REDALERT, alert_color);
  color_blend_f3_f3(alert_color, main_color, 0.3f);

  /* Draw text. */

  /* Wrap most text typographically with hard width limit. */
  BLF_wordwrap(data->fstyle.uifont_id,
               data->wrap_width,
               BLFWrapMode(int(BLFWrapMode::Typographical) | int(BLFWrapMode::HardLimit)));

  /* Wrap paths with path-specific wrapping with hard width limit. */
  BLF_wordwrap(blf_mono_font,
               data->wrap_width,
               BLFWrapMode(int(BLFWrapMode::Path) | int(BLFWrapMode::HardLimit)));

  bbox.xmin += 0.5f * pad_x; /* add padding to the text */
  bbox.ymax -= 0.5f * pad_y;
  bbox.ymax -= BLF_descender(data->fstyle.uifont_id);

  for (int i = 0; i < data->fields.size(); i++) {
    const uiTooltipField *field = &data->fields[i];

    bbox.ymin = bbox.ymax - (data->lineh * field->geom.lines);
    if (field->format.style == UI_TIP_STYLE_HEADER) {
      uiFontStyleDraw_Params fs_params{};
      fs_params.align = UI_STYLE_TEXT_LEFT;
      fs_params.word_wrap = true;

      /* Draw header and active data (is done here to be able to change color). */
      rgb_float_to_uchar(drawcol, tip_colors[UI_TIP_LC_MAIN]);
      UI_fontstyle_set(&data->fstyle);
      UI_fontstyle_draw(
          &data->fstyle, &bbox, field->text.c_str(), field->text.size(), drawcol, &fs_params);

      /* Offset to the end of the last line. */
      if (!field->text_suffix.empty()) {
        const float xofs = field->geom.x_pos;
        const float yofs = data->lineh * (field->geom.lines - 1);
        bbox.xmin += xofs;
        bbox.ymax -= yofs;

        rgb_float_to_uchar(drawcol, tip_colors[UI_TIP_LC_ACTIVE]);
        UI_fontstyle_draw(&data->fstyle,
                          &bbox,
                          field->text_suffix.c_str(),
                          field->text_suffix.size(),
                          drawcol,
                          &fs_params);

        /* Undo offset. */
        bbox.xmin -= xofs;
        bbox.ymax += yofs;
      }
    }
    else if (field->format.style == UI_TIP_STYLE_MONO) {
      uiFontStyleDraw_Params fs_params{};
      fs_params.align = UI_STYLE_TEXT_LEFT;
      fs_params.word_wrap = true;
      uiFontStyle fstyle_mono = data->fstyle;
      fstyle_mono.uifont_id = blf_mono_font;

      UI_fontstyle_set(&fstyle_mono);
      /* XXX: needed because we don't have mono in 'U.uifonts'. */
      BLF_size(fstyle_mono.uifont_id, fstyle_mono.points * UI_SCALE_FAC);
      rgb_float_to_uchar(drawcol, tip_colors[int(field->format.color_id)]);
      UI_fontstyle_draw(
          &fstyle_mono, &bbox, field->text.c_str(), field->text.size(), drawcol, &fs_params);
    }
    else if (field->format.style == UI_TIP_STYLE_IMAGE && field->image.has_value()) {

      bbox.ymax -= field->image->height;

      if (field->image->background == uiTooltipImageBackground::Checkerboard_Themed) {
        imm_draw_box_checker_2d(float(bbox.xmin),
                                float(bbox.ymax),
                                float(bbox.xmin + field->image->width),
                                float(bbox.ymax + field->image->height));
      }
      else if (field->image->background == uiTooltipImageBackground::Checkerboard_Fixed) {
        const float checker_dark = UI_ALPHA_CHECKER_DARK / 255.0f;
        const float checker_light = UI_ALPHA_CHECKER_LIGHT / 255.0f;
        const float color1[4] = {checker_dark, checker_dark, checker_dark, 1.0f};
        const float color2[4] = {checker_light, checker_light, checker_light, 1.0f};
        imm_draw_box_checker_2d_ex(float(bbox.xmin + U.pixelsize),
                                   float(bbox.ymax + U.pixelsize),
                                   float(bbox.xmin + field->image->width),
                                   float(bbox.ymax + field->image->height),
                                   color1,
                                   color2,
                                   8);
      }

      GPU_blend((field->image->premultiplied) ? GPU_BLEND_ALPHA_PREMULT : GPU_BLEND_ALPHA);

      IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_3D_IMAGE_COLOR);
      immDrawPixelsTexScaledFullSize(&state,
                                     bbox.xmin,
                                     bbox.ymax,
                                     field->image->ibuf->x,
                                     field->image->ibuf->y,
                                     blender::gpu::TextureFormat::UNORM_8_8_8_8,
                                     true,
                                     field->image->ibuf->byte_buffer.data,
                                     1.0f,
                                     1.0f,
                                     float(field->image->width) / float(field->image->ibuf->x),
                                     float(field->image->height) / float(field->image->ibuf->y),
                                     (field->image->text_color) ? main_color : nullptr);

      if (field->image->border) {
        GPU_blend(GPU_BLEND_ALPHA);
        GPUVertFormat *format = immVertexFormat();
        uint pos = GPU_vertformat_attr_add(
            format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
        immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
        float border_color[4] = {1.0f, 1.0f, 1.0f, 0.15f};
        float bgcolor[4];
        UI_GetThemeColor4fv(TH_BACK, bgcolor);
        if (srgb_to_grayscale(bgcolor) > 0.5f) {
          border_color[0] = 0.0f;
          border_color[1] = 0.0f;
          border_color[2] = 0.0f;
        }
        immUniformColor4fv(border_color);
        imm_draw_box_wire_2d(pos,
                             float(bbox.xmin),
                             float(bbox.ymax),
                             float(bbox.xmin + field->image->width),
                             float(bbox.ymax + field->image->height));
        immUnbindProgram();
        GPU_blend(GPU_BLEND_NONE);
      }
    }
    else if (field->format.style == UI_TIP_STYLE_SPACER) {
      bbox.ymax -= data->lineh * UI_TIP_SPACER;
    }
    else {
      BLI_assert(field->format.style == UI_TIP_STYLE_NORMAL);
      uiFontStyleDraw_Params fs_params{};
      fs_params.align = UI_STYLE_TEXT_LEFT;
      fs_params.word_wrap = true;

      /* Draw remaining data. */
      rgb_float_to_uchar(drawcol, tip_colors[int(field->format.color_id)]);
      UI_fontstyle_set(&data->fstyle);
      UI_fontstyle_draw(
          &data->fstyle, &bbox, field->text.c_str(), field->text.size(), drawcol, &fs_params);
    }

    bbox.ymax -= data->lineh * field->geom.lines;
  }

  BLF_disable(data->fstyle.uifont_id, BLF_WORD_WRAP);
  BLF_disable(blf_mono_font, BLF_WORD_WRAP);
}

static void ui_tooltip_region_free_cb(ARegion *region)
{
  /* Put ownership back into a unique pointer. */
  std::unique_ptr<uiTooltipData> data{static_cast<uiTooltipData *>(region->regiondata)};
  for (uiTooltipField &field : data->fields) {
    if (field.image && field.image->ibuf) {
      IMB_freeImBuf(field.image->ibuf);
    }
  }
  region->regiondata = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ToolTip Creation Utility Functions
 * \{ */

static std::string ui_tooltip_text_python_from_op(bContext *C,
                                                  wmOperatorType *ot,
                                                  PointerRNA *opptr)
{
  std::string str = WM_operator_pystring_ex(C, nullptr, false, false, ot, opptr);

  /* Avoid overly verbose tips (eg, arrays of 20 layers), exact limit is arbitrary. */
  return WM_operator_pystring_abbreviate(std::move(str), 32);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ToolTip Creation
 * \{ */

#ifdef WITH_PYTHON

static bool ui_tooltip_data_append_from_keymap(bContext *C, uiTooltipData &data, wmKeyMap *keymap)
{
  const int fields_len_init = data.fields.size();

  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
    wmOperatorType *ot = WM_operatortype_find(kmi->idname, true);
    if (!ot) {
      continue;
    }
    /* Tip. */
    UI_tooltip_text_field_add(data,
                              ot->description ? ot->description : ot->name,
                              {},
                              UI_TIP_STYLE_NORMAL,
                              UI_TIP_LC_MAIN,
                              true);

    /* Shortcut. */
    const std::string kmi_str = WM_keymap_item_to_string(kmi, false).value_or("None");
    UI_tooltip_text_field_add(data,
                              fmt::format(fmt::runtime(TIP_("Shortcut: {}")), kmi_str),
                              {},
                              UI_TIP_STYLE_NORMAL,
                              UI_TIP_LC_NORMAL);

    /* Python. */
    if (U.flag & USER_TOOLTIPS_PYTHON) {
      std::string str = ui_tooltip_text_python_from_op(C, ot, kmi->ptr);
      UI_tooltip_text_field_add(data,
                                fmt::format(fmt::runtime(TIP_("Python: {}")), str),
                                {},
                                UI_TIP_STYLE_MONO,
                                UI_TIP_LC_PYTHON);
    }
  }

  return (fields_len_init != data.fields.size());
}

#endif /* WITH_PYTHON */

static std::string ui_tooltip_with_period(blender::StringRef tip)
{
  if (tip.is_empty()) {
    return tip;
  }

  /* Already ends with punctuation. */
  const uint charcode = BLI_str_utf8_as_unicode_safe(
      BLI_str_find_prev_char_utf8(tip.data() + tip.size(), tip.data()));
  if (BLI_str_utf32_char_is_terminal_punctuation(charcode)) {
    return tip;
  }

  /* Contains a bullet Unicode character. */
  if (tip.find("\xe2\x80\xa2") != blender::StringRef::not_found) {
    return tip;
  }

  return fmt::format("{}{}", tip, ".");
}

/**
 * Special tool-system exception.
 */
static std::unique_ptr<uiTooltipData> ui_tooltip_data_from_tool(bContext *C,
                                                                uiBut *but,
                                                                bool is_quick_tip)
{
  if (but->optype == nullptr) {
    return nullptr;
  }
  /* While this should always be set for buttons as they are shown in the UI,
   * the operator search popup can create a button that has no properties, see: #112541. */
  if (but->opptr == nullptr) {
    return nullptr;
  }

  if (!STREQ(but->optype->idname, "WM_OT_tool_set_by_id")) {
    return nullptr;
  }

  /* Needed to get the space-data's type (below). */
  if (CTX_wm_space_data(C) == nullptr) {
    return nullptr;
  }

  char tool_id[MAX_NAME];
  RNA_string_get(but->opptr, "name", tool_id);
  BLI_assert(tool_id[0] != '\0');

  /* When false, we're in a different space type to the tool being set.
   * Needed for setting the fallback tool from the properties space.
   *
   * If we drop the hard coded 3D-view in properties hack, we can remove this check. */
  bool has_valid_context = true;
  const char *has_valid_context_error = IFACE_("Unsupported context");
  {
    ScrArea *area = CTX_wm_area(C);
    if (area == nullptr) {
      has_valid_context = false;
    }
    else {
      PropertyRNA *prop = RNA_struct_find_property(but->opptr, "space_type");
      if (RNA_property_is_set(but->opptr, prop)) {
        const int space_type_prop = RNA_property_enum_get(but->opptr, prop);
        if (space_type_prop != area->spacetype) {
          has_valid_context = false;
        }
      }
    }
  }

  /* We have a tool, now extract the info. */
  std::unique_ptr<uiTooltipData> data = std::make_unique<uiTooltipData>();

#ifdef WITH_PYTHON
  /* It turns out to be most simple to do this via Python since C
   * doesn't have access to information about non-active tools. */

  /* Title (when icon-only). */
  if (but->drawstr.empty()) {
    const char *expr_imports[] = {"bpy", "bl_ui", nullptr};
    char expr[256];
    SNPRINTF_UTF8(expr,
                  "bl_ui.space_toolsystem_common.item_from_id("
                  "bpy.context, "
                  "bpy.context.space_data.type, "
                  "'%s').label",
                  tool_id);
    char *expr_result = nullptr;
    bool is_error = false;

    if (has_valid_context == false) {
      expr_result = BLI_strdup(has_valid_context_error);
    }
    else if (BPY_run_string_as_string(C, expr_imports, expr, nullptr, &expr_result)) {
      if (STREQ(expr_result, "")) {
        MEM_freeN(expr_result);
        expr_result = nullptr;
      }
    }
    else {
      /* NOTE: this is an exceptional case, we could even remove it
       * however there have been reports of tooltips failing, so keep it for now. */
      expr_result = BLI_strdup(IFACE_("Internal error!"));
      is_error = true;
    }

    if (expr_result != nullptr) {
      /* NOTE: This is a very weak hack to get a valid translation most of the time...
       * Proper way to do would be to get i18n context from the item, somehow. */
      const char *label_str = CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, expr_result);
      if (label_str == expr_result) {
        label_str = IFACE_(expr_result);
      }

      if (label_str != expr_result) {
        MEM_freeN(expr_result);
        expr_result = BLI_strdup(label_str);
      }

      UI_tooltip_text_field_add(*data,
                                expr_result,
                                {},
                                UI_TIP_STYLE_NORMAL,
                                (is_error) ? UI_TIP_LC_ALERT : UI_TIP_LC_MAIN,
                                false);
      MEM_freeN(expr_result);
    }
  }

  /* Tip. */
  if (is_quick_tip == false) {
    const char *expr_imports[] = {"bpy", "bl_ui", nullptr};
    char expr[256];
    SNPRINTF_UTF8(expr,
                  "bl_ui.space_toolsystem_common.description_from_id("
                  "bpy.context, "
                  "bpy.context.space_data.type, "
                  "'%s')",
                  tool_id);

    char *expr_result = nullptr;
    bool is_error = false;

    if (has_valid_context == false) {
      expr_result = BLI_strdup(has_valid_context_error);
    }
    else if (BPY_run_string_as_string(C, expr_imports, expr, nullptr, &expr_result)) {
      if (STREQ(expr_result, "")) {
        MEM_freeN(expr_result);
        expr_result = nullptr;
      }
    }
    else {
      /* NOTE: this is an exceptional case, we could even remove it
       * however there have been reports of tooltips failing, so keep it for now. */
      expr_result = BLI_strdup(TIP_("Internal error!"));
      is_error = true;
    }

    if (expr_result != nullptr) {
      const std::string but_tip = ui_tooltip_with_period(expr_result);
      UI_tooltip_text_field_add(*data,
                                but_tip,
                                {},
                                UI_TIP_STYLE_NORMAL,
                                (is_error) ? UI_TIP_LC_ALERT : UI_TIP_LC_MAIN,
                                false);
      MEM_freeN(expr_result);
    }
  }

  /* Shortcut. */
  const bool show_shortcut = is_quick_tip == false &&
                             ((but->block->flag & UI_BLOCK_SHOW_SHORTCUT_ALWAYS) == 0);

  if (show_shortcut) {
    /* There are different kinds of shortcuts:
     *
     * - Direct access to the tool (as if the toolbar button is pressed).
     * - The key is assigned to the operator itself
     *   (bypassing the tool, executing the operator).
     *
     * Either way case it's useful to show the shortcut.
     */
    std::string shortcut = UI_but_string_get_operator_keymap(*C, *but);

    if (shortcut.empty()) {
      /* Check for direct access to the tool. */
      std::optional<std::string> shortcut_toolbar = WM_key_event_operator_string(
          C, "WM_OT_toolbar", blender::wm::OpCallContext::InvokeRegionWin, nullptr, true);
      if (shortcut_toolbar) {
        /* Generate keymap in order to inspect it.
         * NOTE: we could make a utility to avoid the keymap generation part of this. */
        const char *expr_imports[] = {
            "bpy", "bl_keymap_utils", "bl_keymap_utils.keymap_from_toolbar", nullptr};
        const char *expr =
            ("getattr("
             "bl_keymap_utils.keymap_from_toolbar.generate("
             "bpy.context, "
             "bpy.context.space_data.type), "
             "'as_pointer', lambda: 0)()");

        intptr_t expr_result = 0;

        if (has_valid_context == false) {
          shortcut = has_valid_context_error;
        }
        else if (BPY_run_string_as_intptr(C, expr_imports, expr, nullptr, &expr_result)) {
          if (expr_result != 0) {
            wmKeyMap *keymap = (wmKeyMap *)expr_result;
            LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
              if (STREQ(kmi->idname, but->optype->idname)) {
                char tool_id_test[MAX_NAME];
                RNA_string_get(kmi->ptr, "name", tool_id_test);
                if (STREQ(tool_id, tool_id_test)) {
                  std::string kmi_str = WM_keymap_item_to_string(kmi, false).value_or("");
                  shortcut = fmt::format("{}, {}", *shortcut_toolbar, kmi_str);
                  break;
                }
              }
            }
          }
        }
        else {
          BLI_assert(0);
        }
      }
    }

    if (!shortcut.empty()) {
      UI_tooltip_text_field_add(*data,
                                fmt::format(fmt::runtime(TIP_("Shortcut: {}")), shortcut),
                                {},
                                UI_TIP_STYLE_NORMAL,
                                UI_TIP_LC_VALUE,
                                true);
    }
  }

  if (show_shortcut) {
    /* Shortcut for Cycling
     *
     * As a second option, we may have a shortcut to cycle this tool group.
     *
     * Since some keymaps may use this for the primary means of binding keys,
     * it's useful to show these too.
     * Without this there is no way to know how to use a key to set the tool.
     *
     * This is a little involved since the shortcut may be bound to another tool in this group,
     * instead of the current tool on display. */

    char *expr_result = nullptr;
    size_t expr_result_len;

    {
      const char *expr_imports[] = {"bpy", "bl_ui", nullptr};
      char expr[256];
      SNPRINTF_UTF8(expr,
                    "'\\x00'.join("
                    "item.idname for item in bl_ui.space_toolsystem_common.item_group_from_id("
                    "bpy.context, "
                    "bpy.context.space_data.type, '%s', coerce=True) "
                    "if item is not None)",
                    tool_id);

      if (has_valid_context == false) {
        /* pass */
      }
      else if (BPY_run_string_as_string_and_len(
                   C, expr_imports, expr, nullptr, &expr_result, &expr_result_len))
      {
        /* pass. */
      }
    }

    if (expr_result != nullptr) {
      PointerRNA op_props;
      WM_operator_properties_create_ptr(&op_props, but->optype);
      RNA_boolean_set(&op_props, "cycle", true);

      std::optional<std::string> shortcut;

      const char *item_end = expr_result + expr_result_len;
      const char *item_step = expr_result;

      while (item_step < item_end) {
        RNA_string_set(&op_props, "name", item_step);
        shortcut = WM_key_event_operator_string(C,
                                                but->optype->idname,
                                                blender::wm::OpCallContext::InvokeRegionWin,
                                                static_cast<IDProperty *>(op_props.data),
                                                true);
        if (shortcut) {
          break;
        }
        item_step += strlen(item_step) + 1;
      }

      WM_operator_properties_free(&op_props);
      MEM_freeN(expr_result);

      if (shortcut) {
        UI_tooltip_text_field_add(*data,
                                  fmt::format(fmt::runtime(TIP_("Shortcut Cycle: {}")), *shortcut),
                                  {},
                                  UI_TIP_STYLE_NORMAL,
                                  UI_TIP_LC_VALUE,
                                  true);
      }
    }
  }

  /* Python */
  if ((is_quick_tip == false) && (U.flag & USER_TOOLTIPS_PYTHON)) {
    std::string str = ui_tooltip_text_python_from_op(C, but->optype, but->opptr);
    UI_tooltip_text_field_add(*data,
                              fmt::format(fmt::runtime(TIP_("Python: {}")), str),
                              {},
                              UI_TIP_STYLE_MONO,
                              UI_TIP_LC_PYTHON,
                              true);
  }

  /* Keymap */

  /* This is too handy not to expose somehow, let's be sneaky for now. */
  if ((is_quick_tip == false) && CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
    const char *expr_imports[] = {"bpy", "bl_ui", nullptr};
    char expr[256];
    SNPRINTF_UTF8(expr,
                  "getattr("
                  "bl_ui.space_toolsystem_common.keymap_from_id("
                  "bpy.context, "
                  "bpy.context.space_data.type, "
                  "'%s'), "
                  "'as_pointer', lambda: 0)()",
                  tool_id);

    intptr_t expr_result = 0;

    if (has_valid_context == false) {
      /* pass */
    }
    else if (BPY_run_string_as_intptr(C, expr_imports, expr, nullptr, &expr_result)) {
      if (expr_result != 0) {
        UI_tooltip_text_field_add(
            *data, TIP_("Tool Keymap:"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL, true);
        wmKeyMap *keymap = (wmKeyMap *)expr_result;
        ui_tooltip_data_append_from_keymap(C, *data, keymap);
      }
    }
    else {
      BLI_assert(0);
    }
  }
#else
  UNUSED_VARS(is_quick_tip, has_valid_context, has_valid_context_error);
#endif /* WITH_PYTHON */

  return data->fields.is_empty() ? nullptr : std::move(data);
}

static std::string ui_tooltip_color_string(const blender::float4 &color,
                                           const blender::StringRefNull title,
                                           const int max_title_len,
                                           const bool show_alpha,
                                           const bool show_hex = false)
{
  const int align = max_title_len - title.size();

  if (show_hex) {
    uchar hex[4];
    rgba_float_to_uchar(hex, color);
    if (show_alpha) {
      return fmt::format("{}:{: <{}} #{:02X}{:02X}{:02X}{:02X}",
                         title,
                         "",
                         align,
                         int(hex[0]),
                         int(hex[1]),
                         int(hex[2]),
                         int(hex[3]));
    }
    return fmt::format(
        "{}:{: <{}} #{:02X}{:02X}{:02X}", title, "", align, int(hex[0]), int(hex[1]), int(hex[2]));
  }

  if (show_alpha) {
    return fmt::format("{}:{: <{}} {:.3f}", title, "", align, color[3]);
  }

  return fmt::format(
      "{}:{: <{}} {:.3f}  {:.3f}  {:.3f}", title, "", align, color[0], color[1], color[2]);
};

void UI_tooltip_color_field_add(uiTooltipData &data,
                                const blender::float4 &original_color,
                                const bool has_alpha,
                                const bool is_gamma,
                                const ColorManagedDisplay *display,
                                const uiTooltipColorID color_id)
{
  blender::float4 scene_linear_color = original_color;
  blender::float4 display_color = original_color;
  blender::float4 srgb_color = original_color;

  if (is_gamma) {
    IMB_colormanagement_srgb_to_scene_linear_v3(scene_linear_color, scene_linear_color);
  }
  else {
    IMB_colormanagement_scene_linear_to_display_v3(
        display_color, display, DISPLAY_SPACE_COLOR_INSPECTION);
    IMB_colormanagement_scene_linear_to_srgb_v3(srgb_color, srgb_color);
  }

  float hsv[4];
  rgb_to_hsv_v(srgb_color, hsv);
  hsv[3] = srgb_color[3];

  const blender::StringRefNull hex_title = TIP_("Hex");
  const blender::StringRefNull rgb_title = (is_gamma) ? TIP_("sRGB") : TIP_("Display RGB");
  const blender::StringRefNull hsv_title = TIP_("HSV");
  const blender::StringRefNull alpha_title = TIP_("Alpha");
  const int max_title_len = std::max(
      {hex_title.size(), rgb_title.size(), hsv_title.size(), alpha_title.size()});

  const std::string hex_st = ui_tooltip_color_string(
      srgb_color, hex_title, max_title_len, has_alpha, true);
  const std::string rgba_st = ui_tooltip_color_string(
      display_color, rgb_title, max_title_len, false);
  const std::string hsv_st = ui_tooltip_color_string(hsv, hsv_title, max_title_len, false);
  const std::string alpha_st = ui_tooltip_color_string(
      scene_linear_color, alpha_title, max_title_len, true);

  const uiFontStyle *fs = &UI_style_get()->tooltip;
  BLF_size(blf_mono_font, fs->points * UI_SCALE_FAC);
  float w = BLF_width(blf_mono_font, hsv_st.c_str(), hsv_st.size());

  /* TODO: This clips wide gamut. Should make a float buffer and draw for display. */
  uiTooltipImage image_data;
  image_data.width = int(w);
  image_data.height = int(w / (has_alpha ? 4.0f : 3.0f));
  image_data.ibuf = IMB_allocImBuf(image_data.width, image_data.height, 32, IB_byte_data);
  image_data.border = true;
  image_data.premultiplied = false;

  if (scene_linear_color[3] == 1.0f) {
    /* No transparency so draw the entire area solid without checkerboard. */
    image_data.background = uiTooltipImageBackground::None;
    IMB_rectfill_area(
        image_data.ibuf, scene_linear_color, 1, 1, image_data.width, image_data.height);
  }
  else {
    image_data.background = uiTooltipImageBackground::Checkerboard_Fixed;
    /* Draw one half with transparency. */
    IMB_rectfill_area(image_data.ibuf,
                      scene_linear_color,
                      image_data.width / 2,
                      1,
                      image_data.width,
                      image_data.height);
    /* Draw the other half with a solid color. */
    scene_linear_color[3] = 1.0f;
    IMB_rectfill_area(
        image_data.ibuf, scene_linear_color, 1, 1, image_data.width / 2, image_data.height);
  }

  UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, color_id, false);
  UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, color_id, false);
  UI_tooltip_image_field_add(data, image_data);
  UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, color_id, false);
  UI_tooltip_text_field_add(data, rgba_st, {}, UI_TIP_STYLE_MONO, color_id, false);
  UI_tooltip_text_field_add(data, hsv_st, {}, UI_TIP_STYLE_MONO, color_id, false);
  if (has_alpha) {
    UI_tooltip_text_field_add(data, alpha_st, {}, UI_TIP_STYLE_MONO, color_id, false);
  }
  UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, color_id, false);
  UI_tooltip_text_field_add(data, hex_st, {}, UI_TIP_STYLE_MONO, color_id, false);

  /* Tooltip now owns a copy of the ImBuf, so we can delete ours. */
  IMB_freeImBuf(image_data.ibuf);
}

void UI_tooltip_uibut_python_add(uiTooltipData &data,
                                 bContext &C,
                                 uiBut &but,
                                 uiButExtraOpIcon *extra_icon)
{
  wmOperatorType *optype = extra_icon ? UI_but_extra_operator_icon_optype_get(extra_icon) :
                                        but.optype;
  PropertyRNA *rnaprop = extra_icon ? nullptr : but.rnaprop;
  std::string rna_struct = UI_but_string_get_rna_struct_identifier(but);
  std::string rna_prop = UI_but_string_get_rna_property_identifier(but);

  if (optype && !rnaprop) {
    PointerRNA *opptr = extra_icon ? UI_but_extra_operator_icon_opptr_get(extra_icon) :
                                     /* Allocated when needed, the button owns it. */
                                     UI_but_operator_ptr_ensure(&but);

    /* So the context is passed to field functions (some Python field functions use it). */
    WM_operator_properties_sanitize(opptr, false);

    std::string str = ui_tooltip_text_python_from_op(&C, optype, opptr);

    /* Operator info. */
    UI_tooltip_text_field_add(data,
                              fmt::format(fmt::runtime(TIP_("Python: {}")), str),
                              {},
                              UI_TIP_STYLE_MONO,
                              UI_TIP_LC_PYTHON,
                              true);
  }

  if (!optype && !rna_struct.empty()) {
    {
      UI_tooltip_text_field_add(
          data,
          rna_prop.empty() ?
              fmt::format(fmt::runtime(TIP_("Python: {}")), rna_struct) :
              fmt::format(fmt::runtime(TIP_("Python: {}.{}")), rna_struct, rna_prop),
          {},
          UI_TIP_STYLE_MONO,
          UI_TIP_LC_PYTHON,
          (data.fields.size() > 0));
    }

    if (but.rnapoin.owner_id) {
      std::optional<std::string> str = rnaprop ? RNA_path_full_property_py_ex(
                                                     &but.rnapoin, rnaprop, but.rnaindex, true) :
                                                 RNA_path_full_struct_py(&but.rnapoin);
      UI_tooltip_text_field_add(data, str.value_or(""), {}, UI_TIP_STYLE_MONO, UI_TIP_LC_PYTHON);
    }
  }
}

static std::unique_ptr<uiTooltipData> ui_tooltip_data_from_button_or_extra_icon(
    bContext *C, uiBut *but, uiButExtraOpIcon *extra_icon, const bool is_quick_tip)
{
  char buf[512];

  wmOperatorType *optype = extra_icon ? UI_but_extra_operator_icon_optype_get(extra_icon) :
                                        but->optype;
  PropertyRNA *rnaprop = extra_icon ? nullptr : but->rnaprop;

  std::unique_ptr<uiTooltipData> data = std::make_unique<uiTooltipData>();

  /* Menus already show shortcuts, don't show them in the tool-tips. */
  const bool is_menu = ui_block_is_menu(but->block) && !ui_block_is_pie_menu(but->block);

  std::string but_label;
  std::string but_tip;
  std::string but_tip_label;
  std::string op_keymap;
  std::string prop_keymap;
  std::string rna_struct;
  std::string rna_prop;
  std::string enum_label;
  std::string enum_tip;

  if (extra_icon) {
    if (is_quick_tip) {
      but_label = UI_but_extra_icon_string_get_label(*extra_icon);
    }
    else {
      but_label = UI_but_extra_icon_string_get_label(*extra_icon);
      but_tip = UI_but_extra_icon_string_get_tooltip(*C, *extra_icon);
      if (!is_menu) {
        op_keymap = UI_but_extra_icon_string_get_operator_keymap(*C, *extra_icon);
      }
    }
  }
  else {
    const std::optional<EnumPropertyItem> enum_item = UI_but_rna_enum_item_get(*C, *but);
    if (is_quick_tip) {
      but_tip_label = UI_but_string_get_tooltip_label(*but);
      but_label = UI_but_string_get_label(*but);
      enum_label = enum_item ? enum_item->name : "";
    }
    else {
      but_label = UI_but_string_get_label(*but);
      but_tip_label = UI_but_string_get_tooltip_label(*but);
      but_tip = UI_but_string_get_tooltip(*C, *but);
      enum_label = enum_item ? enum_item->name : "";
      const char *description_c = enum_item ? enum_item->description : nullptr;
      enum_tip = description_c ? description_c : "";
      if (!is_menu) {
        op_keymap = UI_but_string_get_operator_keymap(*C, *but);
        prop_keymap = UI_but_string_get_property_keymap(*C, *but);
      }
      rna_struct = UI_but_string_get_rna_struct_identifier(*but);
      rna_prop = UI_but_string_get_rna_property_identifier(*but);
    }
  }

  /* Label: If there is a custom tooltip label, use that to override the label to display.
   * Otherwise fallback to the regular label. */
  if (!but_tip_label.empty()) {
    UI_tooltip_text_field_add(*data, but_tip_label, {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_NORMAL);
    if (!is_quick_tip) {
      UI_tooltip_text_field_add(*data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    }
  }
  /* Regular (non-custom) label. Only show when the button doesn't already show the label. Check
   * prefix instead of comparing because the button may include the shortcut. Buttons with dynamic
   * tool-tips also don't get their default label here since they can already provide more accurate
   * and specific tool-tip content. */
  else if (!but_label.empty() && !blender::StringRef(but->drawstr).startswith(but_label) &&
           !but->tip_func)
  {
    if (!enum_label.empty()) {
      UI_tooltip_text_field_add(*data,
                                fmt::format("{}: ", but_label),
                                enum_label,
                                UI_TIP_STYLE_HEADER,
                                UI_TIP_LC_NORMAL);
    }
    else {
      UI_tooltip_text_field_add(*data, but_label, {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_NORMAL);
    }
    UI_tooltip_text_field_add(*data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
  }

  /* Tip */
  if (!but_tip.empty()) {
    if (!enum_label.empty() && enum_label == but_label) {
      UI_tooltip_text_field_add(
          *data, fmt::format("{}: ", but_tip), enum_label, UI_TIP_STYLE_HEADER, UI_TIP_LC_NORMAL);
      UI_tooltip_text_field_add(*data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    }
    else {
      but_tip = ui_tooltip_with_period(but_tip);
      UI_tooltip_text_field_add(*data, but_tip, {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_NORMAL);
      if (but_label.empty()) {
        UI_tooltip_text_field_add(*data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
      }
    }

    /* special case enum rna buttons */
    if ((but->type == ButType::Row) && rnaprop && RNA_property_flag(rnaprop) & PROP_ENUM_FLAG) {
      UI_tooltip_text_field_add(*data,
                                TIP_("(Shift-Click/Drag to select multiple)"),
                                {},
                                UI_TIP_STYLE_NORMAL,
                                UI_TIP_LC_NORMAL);
    }
  }
  /* When there is only an enum label (no button label or tip), draw that as header. */
  else if (!enum_label.empty() && but_label.empty()) {
    UI_tooltip_text_field_add(
        *data, std::move(enum_label), {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_NORMAL);
  }

  /* Don't include further details if this is just a quick label tooltip. */
  if (is_quick_tip) {
    return data->fields.is_empty() ? nullptr : std::move(data);
  }

  /* Enum field label & tip. */
  if (!enum_tip.empty()) {
    UI_tooltip_text_field_add(
        *data, std::move(enum_tip), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_VALUE);
  }

  /* Operator shortcut. */
  if (!op_keymap.empty()) {
    UI_tooltip_text_field_add(*data,
                              fmt::format(fmt::runtime(TIP_("Shortcut: {}")), op_keymap),
                              {},
                              UI_TIP_STYLE_NORMAL,
                              UI_TIP_LC_VALUE,
                              !data->fields.is_empty());
  }

  /* Property context-toggle shortcut. */
  if (!prop_keymap.empty()) {
    UI_tooltip_text_field_add(*data,
                              fmt::format(fmt::runtime(TIP_("Shortcut: {}")), prop_keymap),
                              {},
                              UI_TIP_STYLE_NORMAL,
                              UI_TIP_LC_VALUE,
                              true);
  }

  if (ELEM(but->type, ButType::Text, ButType::SearchMenu)) {
    /* Better not show the value of a password. */
    if ((rnaprop && (RNA_property_subtype(rnaprop) == PROP_PASSWORD)) == 0) {
      /* Full string. */
      ui_but_string_get(but, buf, sizeof(buf));
      if (buf[0]) {
        UI_tooltip_text_field_add(*data,
                                  fmt::format(fmt::runtime(TIP_("Value: {}")), buf),
                                  {},
                                  UI_TIP_STYLE_NORMAL,
                                  UI_TIP_LC_VALUE,
                                  true);
      }
    }
  }

  if (rnaprop) {
    const int unit_type = UI_but_unit_type_get(but);

    if (unit_type == PROP_UNIT_ROTATION) {
      if (RNA_property_type(rnaprop) == PROP_FLOAT) {
        float value = RNA_property_array_check(rnaprop) ?
                          RNA_property_float_get_index(&but->rnapoin, rnaprop, but->rnaindex) :
                          RNA_property_float_get(&but->rnapoin, rnaprop);
        UI_tooltip_text_field_add(*data,
                                  fmt::format(fmt::runtime(TIP_("Radians: {}")), value),
                                  {},
                                  UI_TIP_STYLE_NORMAL,
                                  UI_TIP_LC_VALUE);
      }
    }

    if (but->flag & UI_BUT_DRIVEN) {
      if (ui_but_anim_expression_get(but, buf, sizeof(buf))) {
        UI_tooltip_text_field_add(*data,
                                  fmt::format(fmt::runtime(TIP_("Expression: {}")), buf),
                                  {},
                                  UI_TIP_STYLE_NORMAL,
                                  UI_TIP_LC_NORMAL);
      }
    }

    if (but->rnapoin.owner_id) {
      const ID *id = but->rnapoin.owner_id;
      if (ID_IS_LINKED(id)) {
        blender::StringRefNull assets_path = blender::asset_system::essentials_directory_path();
        const bool is_builtin = BLI_path_contains(assets_path.c_str(), id->lib->filepath);
        const blender::StringRef title = is_builtin ? TIP_("Built-in Asset") : TIP_("Library");
        const blender::StringRef lib_path = id->lib->filepath;
        const blender::StringRef path = is_builtin ? lib_path.substr(assets_path.size()) :
                                                     id->lib->filepath;
        UI_tooltip_text_field_add(
            *data, fmt::format("{}: {}", title, path), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
      }
    }
  }

  /* Warn on path validity errors. */
  if (ELEM(but->type, ButType::Text) &&
      /* Check red-alert, if the flag is not set, then this was suppressed. */
      (but->flag & UI_BUT_REDALERT))
  {
    if (rnaprop) {
      PropertySubType subtype = RNA_property_subtype(rnaprop);

      /* If relative paths are used when unsupported (will already display red-alert). */
      if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH)) {
        if ((RNA_property_flag(rnaprop) & PROP_PATH_SUPPORTS_BLEND_RELATIVE) == 0) {
          if (BLI_path_is_rel(but->drawstr.c_str())) {
            UI_tooltip_text_field_add(*data,
                                      "Warning: the blend-file relative path prefix \"//\" "
                                      "is not supported for this property.",
                                      {},
                                      UI_TIP_STYLE_NORMAL,
                                      UI_TIP_LC_ALERT);
          }
        }
      }

      /* We include PROP_NONE here because some plain string properties are used
       * as parts of paths. For example, the sub-paths in the compositor's File
       * Output node. */
      if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME, PROP_NONE)) {
        /* Template parse errors, for paths that support it. */
        if ((RNA_property_flag(rnaprop) & PROP_PATH_SUPPORTS_TEMPLATES) != 0) {
          const std::string path = RNA_property_string_get(&but->rnapoin, rnaprop);
          if (BKE_path_contains_template_syntax(path)) {
            const std::optional<blender::bke::path_templates::VariableMap> variables =
                BKE_build_template_variables_for_prop(C, &but->rnapoin, rnaprop);
            BLI_assert(variables.has_value());

            const blender::Vector<blender::bke::path_templates::Error> errors =
                BKE_path_validate_template(path, *variables);

            if (!errors.is_empty()) {
              std::string error_message("Path template error(s):");
              for (const blender::bke::path_templates::Error &error : errors) {
                error_message += "\n  - " + BKE_path_template_error_to_string(error, path);
              }
              UI_tooltip_text_field_add(
                  *data, error_message, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
            }
          }
        }
      }
    }
  }

  /* Button is disabled, we may be able to tell user why. */
  if ((but->flag & UI_BUT_DISABLED) || extra_icon) {
    const char *disabled_msg_orig = nullptr;
    const char *disabled_msg = nullptr;
    bool disabled_msg_free = false;

    /* If operator poll check failed, it can give pretty precise info why. */
    if (optype) {
      const blender::wm::OpCallContext opcontext = extra_icon ?
                                                       extra_icon->optype_params->opcontext :
                                                       but->opcontext;
      wmOperatorCallParams call_params{};
      call_params.optype = optype;
      call_params.opcontext = opcontext;
      CTX_wm_operator_poll_msg_clear(C);
      ui_but_context_poll_operator_ex(C, but, &call_params);
      disabled_msg_orig = CTX_wm_operator_poll_msg_get(C, &disabled_msg_free);
      disabled_msg = TIP_(disabled_msg_orig);
    }
    /* Alternatively, buttons can store some reasoning too. */
    else if (!extra_icon && but->disabled_info) {
      disabled_msg = TIP_(but->disabled_info);
    }

    if (disabled_msg && disabled_msg[0]) {
      UI_tooltip_text_field_add(*data,
                                fmt::format(fmt::runtime(TIP_("Disabled: {}")), disabled_msg),
                                {},
                                UI_TIP_STYLE_NORMAL,
                                UI_TIP_LC_ALERT);
    }
    if (disabled_msg_free) {
      MEM_freeN(disabled_msg_orig);
    }
  }

  if (U.flag & USER_TOOLTIPS_PYTHON) {
    UI_tooltip_uibut_python_add(*data, *C, *but, extra_icon);
  }

  if (but->type == ButType::Color) {
    const ColorManagedDisplay *display = UI_but_cm_display_get(*but);

    float color[4];
    ui_but_v3_get(but, color);
    color[3] = 1.0f;
    bool has_alpha = false;

    if (but->rnaprop) {
      BLI_assert(but->rnaindex == -1);
      has_alpha = RNA_property_array_length(&but->rnapoin, but->rnaprop) >= 4;
      if (has_alpha) {
        color[3] = RNA_property_float_get_index(&but->rnapoin, but->rnaprop, 3);
      }
    }

    UI_tooltip_color_field_add(
        *data, color, has_alpha, ui_but_is_color_gamma(but), display, UI_TIP_LC_NORMAL);
  }

  /* If the last field is a spacer, remove it. */
  while (!data->fields.is_empty() && data->fields.last().format.style == UI_TIP_STYLE_SPACER) {
    data->fields.pop_last();
  }

  return data->fields.is_empty() ? nullptr : std::move(data);
}

static std::unique_ptr<uiTooltipData> ui_tooltip_data_from_gizmo(bContext *C, wmGizmo *gz)
{
  std::unique_ptr<uiTooltipData> data = std::make_unique<uiTooltipData>();

  /* TODO(@ideasman42): a way for gizmos to have their own descriptions (low priority). */

  /* Operator Actions */
  {
    const bool use_drag = gz->drag_part != -1 && gz->highlight_part != gz->drag_part;
    struct GizmoOpActions {
      int part;
      const char *prefix;
    };
    GizmoOpActions gzop_actions[] = {
        {
            gz->highlight_part,
            use_drag ? CTX_TIP_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Click") : nullptr,
        },
        {
            use_drag ? gz->drag_part : -1,
            use_drag ? CTX_TIP_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Drag") : nullptr,
        },
    };

    for (int i = 0; i < ARRAY_SIZE(gzop_actions); i++) {
      wmGizmoOpElem *gzop = (gzop_actions[i].part != -1) ?
                                WM_gizmo_operator_get(gz, gzop_actions[i].part) :
                                nullptr;
      if (gzop != nullptr) {
        /* Description */
        std::string info = WM_operatortype_description_or_name(C, gzop->type, &gzop->ptr);

        if (!info.empty()) {
          UI_tooltip_text_field_add(
              *data,
              gzop_actions[i].prefix ? fmt::format("{}: {}", gzop_actions[i].prefix, info) : info,
              {},
              UI_TIP_STYLE_HEADER,
              UI_TIP_LC_VALUE,
              false);
        }

        /* Shortcut */
        {
          IDProperty *prop = static_cast<IDProperty *>(gzop->ptr.data);
          std::optional<std::string> shortcut_str = WM_key_event_operator_string(
              C, gzop->type->idname, blender::wm::OpCallContext::InvokeDefault, prop, true);
          if (shortcut_str) {
            UI_tooltip_text_field_add(
                *data,
                fmt::format(fmt::runtime(TIP_("Shortcut: {}")), *shortcut_str),
                {},
                UI_TIP_STYLE_NORMAL,
                UI_TIP_LC_VALUE,
                true);
          }
        }
      }
    }
  }

  /* Property Actions */
  for (const wmGizmoProperty &gz_prop : gz->target_properties) {
    if (gz_prop.prop != nullptr) {
      const char *info = RNA_property_ui_description(gz_prop.prop);
      if (info && info[0]) {
        UI_tooltip_text_field_add(*data, info, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_VALUE, true);
      }
    }
  }

  return data->fields.is_empty() ? nullptr : std::move(data);
}

static std::unique_ptr<uiTooltipData> ui_tooltip_data_from_custom_func(bContext *C, uiBut *but)
{
  /* Create tooltip data. */
  std::unique_ptr<uiTooltipData> data = std::make_unique<uiTooltipData>();

  /* Create fields from custom callback. */
  but->tip_custom_func(*C, *data, but, but->tip_arg);

  return data->fields.is_empty() ? nullptr : std::move(data);
}

static ARegion *ui_tooltip_create_with_data(bContext *C,
                                            std::unique_ptr<uiTooltipData> data_uptr,
                                            const float init_position[2],
                                            const rcti *init_rect_overlap)
{
  wmWindow *win = CTX_wm_window(C);
  const blender::int2 win_size = WM_window_native_pixel_size(win);
  rcti rect_i;
  FontFlags font_flag = BLF_NONE;

  /* Create area region. */
  ARegion *region = ui_region_temp_add(CTX_wm_screen(C));

  static ARegionType type;
  memset(&type, 0, sizeof(ARegionType));
  type.draw = ui_tooltip_region_draw_cb;
  type.free = ui_tooltip_region_free_cb;
  type.regionid = RGN_TYPE_TEMPORARY;
  region->runtime->type = &type;
  /* Move ownership to region data. The region type free callback puts it back into a unique
   * pointer for save freeing. */
  region->regiondata = data_uptr.release();

  uiTooltipData *data = static_cast<uiTooltipData *>(region->regiondata);

  /* Set font, get bounding-box. */
  const uiStyle *style = UI_style_get();
  data->fstyle = style->tooltip; /* copy struct */
  BLF_size(data->fstyle.uifont_id, data->fstyle.points * UI_SCALE_FAC);
  int h = BLF_height_max(data->fstyle.uifont_id);
  const float pad_x = h * UI_TIP_PADDING_X;
  const float pad_y = h * UI_TIP_PADDING_Y;

  UI_fontstyle_set(&data->fstyle);

  data->wrap_width = min_ii(UI_TIP_MAXWIDTH * UI_SCALE_FAC, win_size[0] - pad_x);

  font_flag |= BLF_WORD_WRAP;
  BLF_enable(data->fstyle.uifont_id, font_flag);
  BLF_enable(blf_mono_font, font_flag);
  BLF_wordwrap(data->fstyle.uifont_id,
               data->wrap_width,
               BLFWrapMode(int(BLFWrapMode::Typographical) | int(BLFWrapMode::HardLimit)));
  BLF_wordwrap(blf_mono_font,
               data->wrap_width,
               BLFWrapMode(int(BLFWrapMode::Path) | int(BLFWrapMode::HardLimit)));

  int i, fonth, fontw;
  for (i = 0, fontw = 0, fonth = 0; i < data->fields.size(); i++) {
    uiTooltipField *field = &data->fields[i];
    ResultBLF info = {0};
    int w = 0;
    int x_pos = 0;
    int font_id;

    if (field->format.style == UI_TIP_STYLE_MONO) {
      BLF_size(blf_mono_font, data->fstyle.points * UI_SCALE_FAC);
      font_id = blf_mono_font;
    }
    else {
      font_id = data->fstyle.uifont_id;
    }

    if (!field->text.empty()) {
      w = BLF_width(font_id, field->text.c_str(), field->text.size(), &info);
    }

    /* check for suffix (enum label) */
    if (!field->text_suffix.empty()) {
      x_pos = info.width;
      w = max_ii(w,
                 x_pos + BLF_width(font_id, ": ", BLF_DRAW_STR_DUMMY_MAX) +
                     BLF_width(font_id, field->text_suffix.c_str(), BLF_DRAW_STR_DUMMY_MAX));
    }

    fonth += h * info.lines;

    if (field->format.style == UI_TIP_STYLE_SPACER) {
      fonth += h * UI_TIP_SPACER;
    }

    if (field->format.style == UI_TIP_STYLE_IMAGE && field->image) {
      fonth += field->image->height;
      w = max_ii(w, field->image->width);
    }

    fontw = max_ii(fontw, w);

    field->geom.lines = info.lines;
    field->geom.x_pos = x_pos;
  }

  BLF_disable(data->fstyle.uifont_id, font_flag);
  BLF_disable(blf_mono_font, font_flag);

  data->toth = fonth;
  data->lineh = h;

  /* Compute position. */
  {
    rctf rect_fl;
    rect_fl.xmin = init_position[0] - (h * 0.2f) - (pad_x * 0.5f);
    rect_fl.xmax = rect_fl.xmin + fontw;
    rect_fl.ymax = init_position[1] - (h * 0.2f) - (pad_y * 0.5f);
    rect_fl.ymin = rect_fl.ymax - fonth;
    BLI_rcti_rctf_copy(&rect_i, &rect_fl);
  }

  // #define USE_ALIGN_Y_CENTER

  /* Clamp to window bounds. */
  {
    /* Ensure at least 5 pixels above screen bounds.
     * #UI_UNIT_Y is just a guess to be above the menu item. */
    if (init_rect_overlap != nullptr) {
      const int pad = max_ff(1.0f, U.pixelsize) * 5;
      rcti init_rect;
      init_rect.xmin = init_rect_overlap->xmin - pad;
      init_rect.xmax = init_rect_overlap->xmax + pad;
      init_rect.ymin = init_rect_overlap->ymin - pad;
      init_rect.ymax = init_rect_overlap->ymax + pad;
      rcti rect_clamp;
      rect_clamp.xmin = pad_x + pad;
      rect_clamp.xmax = win_size[0] - pad_x - pad;
      rect_clamp.ymin = pad_y + pad;
      rect_clamp.ymax = win_size[1] - pad_y - pad;
      /* try right. */
      const int size_x = BLI_rcti_size_x(&rect_i);
      const int size_y = BLI_rcti_size_y(&rect_i);
      const int cent_overlap_x = BLI_rcti_cent_x(&init_rect);
#ifdef USE_ALIGN_Y_CENTER
      const int cent_overlap_y = BLI_rcti_cent_y(&init_rect);
#endif
      struct {
        rcti xpos;
        rcti xneg;
        rcti ypos;
        rcti yneg;
      } rect;

      { /* xpos */
        rcti r = rect_i;
        r.xmin = init_rect.xmax;
        r.xmax = r.xmin + size_x;
#ifdef USE_ALIGN_Y_CENTER
        r.ymin = cent_overlap_y - (size_y / 2);
        r.ymax = r.ymin + size_y;
#else
        r.ymin = init_rect.ymax - BLI_rcti_size_y(&rect_i);
        r.ymax = init_rect.ymax;
        r.ymin -= UI_POPUP_MARGIN;
        r.ymax -= UI_POPUP_MARGIN;
#endif
        rect.xpos = r;
      }
      { /* xneg */
        rcti r = rect_i;
        r.xmin = init_rect.xmin - size_x;
        r.xmax = r.xmin + size_x;
#ifdef USE_ALIGN_Y_CENTER
        r.ymin = cent_overlap_y - (size_y / 2);
        r.ymax = r.ymin + size_y;
#else
        r.ymin = init_rect.ymax - BLI_rcti_size_y(&rect_i);
        r.ymax = init_rect.ymax;
        r.ymin -= UI_POPUP_MARGIN;
        r.ymax -= UI_POPUP_MARGIN;
#endif
        rect.xneg = r;
      }
      { /* ypos */
        rcti r = rect_i;
        r.xmin = cent_overlap_x - (size_x / 2);
        r.xmax = r.xmin + size_x;
        r.ymin = init_rect.ymax;
        r.ymax = r.ymin + size_y;
        rect.ypos = r;
      }
      { /* yneg */
        rcti r = rect_i;
        r.xmin = cent_overlap_x - (size_x / 2);
        r.xmax = r.xmin + size_x;
        r.ymin = init_rect.ymin - size_y;
        r.ymax = r.ymin + size_y;
        rect.yneg = r;
      }

      bool found = false;
      for (int j = 0; j < 4; j++) {
        const rcti *r = (&rect.xpos) + j;
        if (BLI_rcti_inside_rcti(&rect_clamp, r)) {
          rect_i = *r;
          found = true;
          break;
        }
      }
      if (!found) {
        /* Fallback, we could pick the best fallback, for now just use xpos. */
        int offset_dummy[2];
        rect_i = rect.xpos;
        BLI_rcti_clamp(&rect_i, &rect_clamp, offset_dummy);
      }
    }
    else {
      const int clamp_pad_x = int((5.0f * UI_SCALE_FAC) + (pad_x * 0.5f));
      const int clamp_pad_y = int((7.0f * UI_SCALE_FAC) + (pad_y * 0.5f));
      rcti rect_clamp;
      rect_clamp.xmin = clamp_pad_x;
      rect_clamp.xmax = win_size[0] - clamp_pad_x;
      rect_clamp.ymin = clamp_pad_y;
      rect_clamp.ymax = win_size[1] - clamp_pad_y;
      int offset_dummy[2];
      BLI_rcti_clamp(&rect_i, &rect_clamp, offset_dummy);
    }
  }

#undef USE_ALIGN_Y_CENTER

  /* add padding */
  BLI_rcti_pad(&rect_i, int(round(pad_x * 0.5f)), int(round(pad_y * 0.5f)));

  /* widget rect, in region coords */
  {
    /* Compensate for margin offset, visually this corrects the position. */
    const int margin = UI_POPUP_MARGIN;
    if (init_rect_overlap != nullptr) {
      BLI_rcti_translate(&rect_i, margin, margin / 2);
    }

    data->bbox.xmin = margin;
    data->bbox.xmax = BLI_rcti_size_x(&rect_i) + margin;
    data->bbox.ymin = margin;
    data->bbox.ymax = BLI_rcti_size_y(&rect_i) + margin;

    /* region bigger for shadow */
    region->winrct.xmin = rect_i.xmin - margin;
    region->winrct.xmax = rect_i.xmax + margin;
    region->winrct.ymin = rect_i.ymin - margin;
    region->winrct.ymax = rect_i.ymax + margin;
  }

  /* Adds sub-window. */
  ED_region_floating_init(region);

  /* notify change and redraw */
  ED_region_tag_redraw(region);

  return region;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ToolTip Public API
 * \{ */

ARegion *UI_tooltip_create_from_button_or_extra_icon(
    bContext *C, ARegion *butregion, uiBut *but, uiButExtraOpIcon *extra_icon, bool is_quick_tip)
{
  wmWindow *win = CTX_wm_window(C);
  float init_position[2];

  if (but->drawflag & UI_BUT_NO_TOOLTIP) {
    return nullptr;
  }
  std::unique_ptr<uiTooltipData> data = nullptr;

  if (!is_quick_tip && but->tip_custom_func) {
    data = ui_tooltip_data_from_custom_func(C, but);
  }

  if (data == nullptr) {
    data = ui_tooltip_data_from_tool(C, but, is_quick_tip);
  }

  if (data == nullptr) {
    data = ui_tooltip_data_from_button_or_extra_icon(C, but, extra_icon, is_quick_tip);
  }

  if (data == nullptr) {
    data = ui_tooltip_data_from_button_or_extra_icon(C, but, nullptr, is_quick_tip);
  }

  if (data == nullptr) {
    return nullptr;
  }

  const bool is_no_overlap = UI_but_has_quick_tooltip(but) || UI_but_is_tool(but);
  rcti init_rect;
  if (is_no_overlap) {
    rctf overlap_rect_fl;
    init_position[0] = BLI_rctf_cent_x(&but->rect);
    init_position[1] = BLI_rctf_cent_y(&but->rect);
    if (butregion) {
      ui_block_to_window_fl(butregion, but->block, &init_position[0], &init_position[1]);
      ui_block_to_window_rctf(butregion, but->block, &overlap_rect_fl, &but->rect);
    }
    else {
      overlap_rect_fl = but->rect;
    }
    BLI_rcti_rctf_copy_round(&init_rect, &overlap_rect_fl);
  }
  else if (but->type == ButType::Label && BLI_rctf_size_y(&but->rect) > UI_UNIT_Y) {
    init_position[0] = win->eventstate->xy[0];
    init_position[1] = win->eventstate->xy[1] - (UI_POPUP_MARGIN / 2);
  }
  else {
    init_position[0] = BLI_rctf_cent_x(&but->rect);
    init_position[1] = but->rect.ymin;
    if (butregion) {
      ui_block_to_window_fl(butregion, but->block, &init_position[0], &init_position[1]);
      init_position[0] = win->eventstate->xy[0];
    }
    init_position[1] -= (UI_POPUP_MARGIN / 2);
  }

  ARegion *region = ui_tooltip_create_with_data(
      C, std::move(data), init_position, is_no_overlap ? &init_rect : nullptr);

  return region;
}

ARegion *UI_tooltip_create_from_button(bContext *C,
                                       ARegion *butregion,
                                       uiBut *but,
                                       bool is_quick_tip)
{
  return UI_tooltip_create_from_button_or_extra_icon(C, butregion, but, nullptr, is_quick_tip);
}

ARegion *UI_tooltip_create_from_gizmo(bContext *C, wmGizmo *gz)
{
  wmWindow *win = CTX_wm_window(C);
  float init_position[2] = {float(win->eventstate->xy[0]), float(win->eventstate->xy[1])};

  std::unique_ptr<uiTooltipData> data = ui_tooltip_data_from_gizmo(C, gz);
  if (data == nullptr) {
    return nullptr;
  }

  /* TODO(@harley): Julian preferred that the gizmo callback return the 3D bounding box
   * which we then project to 2D here. Would make a nice improvement. */
  if (gz->type->screen_bounds_get) {
    rcti bounds;
    if (gz->type->screen_bounds_get(C, gz, &bounds)) {
      init_position[0] = bounds.xmin;
      init_position[1] = bounds.ymin;
    }
  }

  return ui_tooltip_create_with_data(C, std::move(data), init_position, nullptr);
}

static void ui_tooltip_from_image(Image &ima, uiTooltipData &data)
{
  if (ima.filepath[0]) {
    char root[FILE_MAX];
    BLI_path_split_dir_part(ima.filepath, root, FILE_MAX);
    UI_tooltip_text_field_add(data, root, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
  }

  std::string image_type;
  switch (ima.source) {
    case IMA_SRC_FILE:
      image_type = TIP_("Single Image");
      break;
    case IMA_SRC_SEQUENCE:
      image_type = TIP_("Image Sequence");
      break;
    case IMA_SRC_MOVIE:
      image_type = TIP_("Movie");
      break;
    case IMA_SRC_GENERATED:
      image_type = TIP_("Generated");
      break;
    case IMA_SRC_VIEWER:
      image_type = TIP_("Viewer");
      break;
    case IMA_SRC_TILED:
      image_type = TIP_("UDIM Tiles");
      break;
  }
  UI_tooltip_text_field_add(data, image_type, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);

  short w;
  short h;
  ImBuf *ibuf = BKE_image_preview(&ima, 200.0f * UI_SCALE_FAC, &w, &h);

  if (ibuf) {
    UI_tooltip_text_field_add(
        data, fmt::format("{} \u00D7 {}", w, h), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
  }

  if (BKE_image_has_anim(&ima)) {
    MovieReader *anim = static_cast<ImageAnim *>(ima.anims.first)->anim;
    if (anim) {
      int duration = MOV_get_duration_frames(anim, IMB_TC_RECORD_RUN);
      UI_tooltip_text_field_add(
          data, fmt::format("Frames: {}", duration), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
    }
  }

  UI_tooltip_text_field_add(
      data, ima.colorspace_settings.name, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);

  UI_tooltip_text_field_add(data,
                            fmt::format(fmt::runtime(TIP_("Users: {}")), ima.id.us),
                            {},
                            UI_TIP_STYLE_NORMAL,
                            UI_TIP_LC_NORMAL);

  if (ibuf) {
    uiTooltipImage image_data;
    image_data.width = ibuf->x;
    image_data.height = ibuf->y;
    image_data.ibuf = ibuf;
    image_data.border = true;
    image_data.background = uiTooltipImageBackground::Checkerboard_Themed;
    image_data.premultiplied = true;
    UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    UI_tooltip_image_field_add(data, image_data);
    IMB_freeImBuf(ibuf);
  }
}

static void ui_tooltip_from_clip(MovieClip &clip, uiTooltipData &data)
{
  if (clip.filepath[0]) {
    char root[FILE_MAX];
    BLI_path_split_dir_part(clip.filepath, root, FILE_MAX);
    UI_tooltip_text_field_add(data, root, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
  }

  std::string image_type;
  switch (clip.source) {
    case IMA_SRC_SEQUENCE:
      image_type = TIP_("Image Sequence");
      break;
    case IMA_SRC_MOVIE:
      image_type = TIP_("Movie");
      break;
  }
  UI_tooltip_text_field_add(data, image_type, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);

  if (clip.anim) {
    MovieReader *anim = clip.anim;

    UI_tooltip_text_field_add(
        data,
        fmt::format("{} \u00D7 {}", MOV_get_image_width(anim), MOV_get_image_height(anim)),
        {},
        UI_TIP_STYLE_NORMAL,
        UI_TIP_LC_NORMAL);

    UI_tooltip_text_field_add(
        data,
        fmt::format("Frames: {}", MOV_get_duration_frames(anim, IMB_TC_RECORD_RUN)),
        {},
        UI_TIP_STYLE_NORMAL,
        UI_TIP_LC_NORMAL);

    ImBuf *ibuf = MOV_decode_preview_frame(anim);

    if (ibuf) {
      /* Resize. */
      float scale = (200.0f * UI_SCALE_FAC) / float(std::max(ibuf->x, ibuf->y));
      IMB_scale(ibuf, scale * ibuf->x, scale * ibuf->y, IMBScaleFilter::Box, false);
      IMB_byte_from_float(ibuf);

      uiTooltipImage image_data;
      image_data.width = ibuf->x;
      image_data.height = ibuf->y;
      image_data.ibuf = ibuf;
      image_data.border = true;
      image_data.background = uiTooltipImageBackground::Checkerboard_Themed;
      image_data.premultiplied = true;
      UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
      UI_tooltip_text_field_add(data, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
      UI_tooltip_image_field_add(data, image_data);
      IMB_freeImBuf(ibuf);
    }
  }
}

static void ui_tooltip_from_vfont(const VFont &font, uiTooltipData &data)
{
  if (BKE_vfont_is_builtin(&font)) {
    /* In memory font previews are currently not supported,
     *  don't attempt to handle as a file. */
    return;
  }
  if (!font.filepath[0]) {
    /* These may be packed files, currently not supported. */
    return;
  }

  char filepath_abs[FILE_MAX];
  STRNCPY(filepath_abs, font.filepath);
  BLI_path_abs(filepath_abs, ID_BLEND_PATH_FROM_GLOBAL(&font.id));

  if (!BLI_exists(filepath_abs)) {
    UI_tooltip_text_field_add(
        data, TIP_("File not found"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
    return;
  }

  float color[4];
  const uiWidgetColors *theme = ui_tooltip_get_theme();
  rgba_uchar_to_float(color, theme->text);
  ImBuf *ibuf = IMB_font_preview(filepath_abs, 256 * UI_SCALE_FAC, color, "ABCDabefg&0123");
  if (ibuf) {
    uiTooltipImage image_data;
    image_data.width = ibuf->x;
    image_data.height = ibuf->y;
    image_data.ibuf = ibuf;
    image_data.border = false;
    image_data.background = uiTooltipImageBackground::None;
    image_data.premultiplied = false;
    image_data.text_color = true;
    UI_tooltip_image_field_add(data, image_data);
    IMB_freeImBuf(ibuf);
  }
}

static std::unique_ptr<uiTooltipData> ui_tooltip_data_from_search_item_tooltip_data(ID *id)
{
  std::unique_ptr<uiTooltipData> data = std::make_unique<uiTooltipData>();
  const ID_Type type_id = GS(id->name);

  UI_tooltip_text_field_add(*data, id->name + 2, {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_MAIN);

  if (type_id == ID_IM) {
    ui_tooltip_from_image(*reinterpret_cast<Image *>(id), *data);
  }
  else if (type_id == ID_MC) {
    ui_tooltip_from_clip(*reinterpret_cast<MovieClip *>(id), *data);
  }
  else if (type_id == ID_VF) {
    ui_tooltip_from_vfont(*reinterpret_cast<VFont *>(id), *data);
  }
  else {
    UI_tooltip_text_field_add(
        *data,
        fmt::format(fmt::runtime(TIP_("Choose {} data-block to be assigned to this user")),
                    BKE_idtype_idcode_to_name(GS(id->name))),
        {},
        UI_TIP_STYLE_NORMAL,
        UI_TIP_LC_NORMAL);
  }

  /** Additional info about the item (e.g. library name of a linked data-block). */
  if (ID_IS_LINKED(id)) {
    UI_tooltip_text_field_add(*data,
                              fmt::format(fmt::runtime(TIP_("Source library: {}\n{}")),
                                          id->lib->id.name + 2,
                                          id->lib->filepath),
                              {},
                              UI_TIP_STYLE_NORMAL,
                              UI_TIP_LC_NORMAL);
  }

  return data->fields.is_empty() ? nullptr : std::move(data);
}

ARegion *UI_tooltip_create_from_search_item_generic(bContext *C,
                                                    const ARegion *searchbox_region,
                                                    const rcti *item_rect,
                                                    ID *id)
{
  std::unique_ptr<uiTooltipData> data = ui_tooltip_data_from_search_item_tooltip_data(id);
  if (data == nullptr) {
    return nullptr;
  }

  const wmWindow *win = CTX_wm_window(C);
  float init_position[2];
  init_position[0] = win->eventstate->xy[0];
  init_position[1] = item_rect->ymin + searchbox_region->winrct.ymin - (UI_POPUP_MARGIN / 2);

  return ui_tooltip_create_with_data(C, std::move(data), init_position, nullptr);
}

void UI_tooltip_free(bContext *C, bScreen *screen, ARegion *region)
{
  ui_region_temp_remove(C, screen, region);
}

/** \} */
