/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "BLF_api.h"

#include "BLT_translation.h"

#include "UI_interface.h"

#include "ED_datafiles.h"

#include "interface_intern.hh"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

static void fontstyle_set_ex(const uiFontStyle *fs, const float dpi_fac);

/* style + theme + layout-engine = UI */

/**
 * This is a complete set of layout rules, the 'state' of the Layout
 * Engine. Multiple styles are possible, defined via C or Python. Styles
 * get a name, and will typically get activated per region type, like
 * `Header`, or `Listview` or `Toolbar`. Properties of Style definitions
 * are:
 *
 * - default column properties, internal spacing, aligning, min/max width
 * - button alignment rules (for groups)
 * - label placement rules
 * - internal labeling or external labeling default
 * - default minimum widths for buttons/labels (in amount of characters)
 * - font types, styles and relative sizes for Panel titles, labels, etc.
 */

/* ********************************************** */

static uiStyle *ui_style_new(ListBase *styles, const char *name, short uifont_id)
{
  uiStyle *style = MEM_cnew<uiStyle>(__func__);

  BLI_addtail(styles, style);
  STRNCPY(style->name, name);

  style->panelzoom = 1.0; /* unused */

  style->paneltitle.uifont_id = uifont_id;
  style->paneltitle.points = UI_DEFAULT_TITLE_POINTS;
  style->paneltitle.shadow = 3;
  style->paneltitle.shadx = 0;
  style->paneltitle.shady = -1;
  style->paneltitle.shadowalpha = 0.5f;
  style->paneltitle.shadowcolor = 0.0f;

  style->grouplabel.uifont_id = uifont_id;
  style->grouplabel.points = UI_DEFAULT_TITLE_POINTS;
  style->grouplabel.shadow = 3;
  style->grouplabel.shadx = 0;
  style->grouplabel.shady = -1;
  style->grouplabel.shadowalpha = 0.5f;
  style->grouplabel.shadowcolor = 0.0f;

  style->widgetlabel.uifont_id = uifont_id;
  style->widgetlabel.points = UI_DEFAULT_TEXT_POINTS;
  style->widgetlabel.shadow = 3;
  style->widgetlabel.shadx = 0;
  style->widgetlabel.shady = -1;
  style->widgetlabel.shadowalpha = 0.5f;
  style->widgetlabel.shadowcolor = 0.0f;

  style->widget.uifont_id = uifont_id;
  style->widget.points = UI_DEFAULT_TEXT_POINTS;
  style->widget.shadow = 1;
  style->widget.shady = -1;
  style->widget.shadowalpha = 0.5f;
  style->widget.shadowcolor = 0.0f;

  style->columnspace = 8;
  style->templatespace = 5;
  style->boxspace = 5;
  style->buttonspacex = 8;
  style->buttonspacey = 2;
  style->panelspace = 8;
  style->panelouter = 4;

  return style;
}

static uiFont *uifont_to_blfont(int id)
{
  uiFont *font = static_cast<uiFont *>(U.uifonts.first);

  for (; font; font = font->next) {
    if (font->uifont_id == id) {
      return font;
    }
  }
  return static_cast<uiFont *>(U.uifonts.first);
}

/* *************** draw ************************ */

void UI_fontstyle_draw_ex(const uiFontStyle *fs,
                          const rcti *rect,
                          const char *str,
                          const size_t str_len,
                          const uchar col[4],
                          const struct uiFontStyleDraw_Params *fs_params,
                          int *r_xofs,
                          int *r_yofs,
                          struct ResultBLF *r_info)
{
  int xofs = 0, yofs;
  int font_flag = BLF_CLIPPING;

  UI_fontstyle_set(fs);

  /* set the flag */
  if (fs->shadow) {
    font_flag |= BLF_SHADOW;
    const float shadow_color[4] = {
        fs->shadowcolor, fs->shadowcolor, fs->shadowcolor, fs->shadowalpha};
    BLF_shadow(fs->uifont_id, fs->shadow, shadow_color);
    BLF_shadow_offset(fs->uifont_id, fs->shadx, fs->shady);
  }
  if (fs_params->word_wrap == 1) {
    font_flag |= BLF_WORD_WRAP;
  }
  if (fs->bold) {
    font_flag |= BLF_BOLD;
  }
  if (fs->italic) {
    font_flag |= BLF_ITALIC;
  }

  BLF_enable(fs->uifont_id, font_flag);

  if (fs_params->word_wrap == 1) {
    /* Draw from bound-box top. */
    yofs = BLI_rcti_size_y(rect) - BLF_height_max(fs->uifont_id);
  }
  else {
    /* Draw from bound-box center. */
    const int height = BLF_ascender(fs->uifont_id) + BLF_descender(fs->uifont_id);
    yofs = ceil(0.5f * (BLI_rcti_size_y(rect) - height));
  }

  if (fs_params->align == UI_STYLE_TEXT_CENTER) {
    xofs = floor(0.5f * (BLI_rcti_size_x(rect) - BLF_width(fs->uifont_id, str, str_len)));
  }
  else if (fs_params->align == UI_STYLE_TEXT_RIGHT) {
    xofs = BLI_rcti_size_x(rect) - BLF_width(fs->uifont_id, str, str_len);
  }

  yofs = MAX2(0, yofs);
  xofs = MAX2(0, xofs);

  BLF_clipping(fs->uifont_id, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
  BLF_position(fs->uifont_id, rect->xmin + xofs, rect->ymin + yofs, 0.0f);
  BLF_color4ubv(fs->uifont_id, col);

  BLF_draw_ex(fs->uifont_id, str, str_len, r_info);

  BLF_disable(fs->uifont_id, font_flag);

  if (r_xofs) {
    *r_xofs = xofs;
  }
  if (r_yofs) {
    *r_yofs = yofs;
  }
}

void UI_fontstyle_draw(const uiFontStyle *fs,
                       const rcti *rect,
                       const char *str,
                       const size_t str_len,
                       const uchar col[4],
                       const struct uiFontStyleDraw_Params *fs_params)
{
  UI_fontstyle_draw_ex(fs, rect, str, str_len, col, fs_params, nullptr, nullptr, nullptr);
}

void UI_fontstyle_draw_rotated(const uiFontStyle *fs,
                               const rcti *rect,
                               const char *str,
                               const uchar col[4])
{
  float height;
  int xofs, yofs;
  float angle;
  rcti txtrect;

  UI_fontstyle_set(fs);

  height = BLF_ascender(fs->uifont_id) + BLF_descender(fs->uifont_id);
  /* becomes x-offset when rotated */
  xofs = ceil(0.5f * (BLI_rcti_size_y(rect) - height));

  /* ignore UI_STYLE, always aligned to top */

  /* Rotate counter-clockwise for now (assumes left-to-right language). */
  xofs += height;
  yofs = BLF_width(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX) + 5;
  angle = M_PI_2;

  /* translate rect to vertical */
  txtrect.xmin = rect->xmin - BLI_rcti_size_y(rect);
  txtrect.ymin = rect->ymin - BLI_rcti_size_x(rect);
  txtrect.xmax = rect->xmin;
  txtrect.ymax = rect->ymin;

  /* clip is very strict, so we give it some space */
  /* clipping is done without rotation, so make rect big enough to contain both positions */
  BLF_clipping(fs->uifont_id,
               txtrect.xmin - 1,
               txtrect.ymin - yofs - xofs - 4,
               rect->xmax + 1,
               rect->ymax + 4);
  BLF_enable(fs->uifont_id, BLF_CLIPPING);
  BLF_position(fs->uifont_id, txtrect.xmin + xofs, txtrect.ymax - yofs, 0.0f);

  BLF_enable(fs->uifont_id, BLF_ROTATION);
  BLF_rotation(fs->uifont_id, angle);
  BLF_color4ubv(fs->uifont_id, col);

  if (fs->shadow) {
    BLF_enable(fs->uifont_id, BLF_SHADOW);
    const float shadow_color[4] = {
        fs->shadowcolor, fs->shadowcolor, fs->shadowcolor, fs->shadowalpha};
    BLF_shadow(fs->uifont_id, fs->shadow, shadow_color);
    BLF_shadow_offset(fs->uifont_id, fs->shadx, fs->shady);
  }

  BLF_draw(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);
  BLF_disable(fs->uifont_id, BLF_ROTATION);
  BLF_disable(fs->uifont_id, BLF_CLIPPING);
  if (fs->shadow) {
    BLF_disable(fs->uifont_id, BLF_SHADOW);
  }
}

void UI_fontstyle_draw_simple(
    const uiFontStyle *fs, float x, float y, const char *str, const uchar col[4])
{
  UI_fontstyle_set(fs);
  BLF_position(fs->uifont_id, x, y, 0.0f);
  BLF_color4ubv(fs->uifont_id, col);
  BLF_draw(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);
}

void UI_fontstyle_draw_simple_backdrop(const uiFontStyle *fs,
                                       float x,
                                       float y,
                                       const char *str,
                                       const float col_fg[4],
                                       const float col_bg[4])
{
  UI_fontstyle_set(fs);

  {
    const int width = BLF_width(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);
    const int height = BLF_height_max(fs->uifont_id);
    const int decent = BLF_descender(fs->uifont_id);
    const float margin = height / 4.0f;

    rctf rect;
    rect.xmin = x - margin;
    rect.xmax = x + width + margin;
    rect.ymin = (y + decent) - margin;
    rect.ymax = (y + decent) + height + margin;
    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_4fv(&rect, true, margin, col_bg);
  }

  BLF_position(fs->uifont_id, x, y, 0.0f);
  BLF_color4fv(fs->uifont_id, col_fg);
  BLF_draw(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);
}

/* ************** helpers ************************ */

const uiStyle *UI_style_get(void)
{
#if 0
  uiStyle *style = nullptr;
  /* offset is two struct uiStyle pointers */
  style = BLI_findstring(&U.uistyles, "Unifont Style", sizeof(style) * 2);
  return (style != nullptr) ? style : U.uistyles.first;
#else
  return static_cast<const uiStyle *>(U.uistyles.first);
#endif
}

const uiStyle *UI_style_get_dpi(void)
{
  const uiStyle *style = UI_style_get();
  static uiStyle _style;

  _style = *style;

  _style.paneltitle.shadx = short(UI_SCALE_FAC * _style.paneltitle.shadx);
  _style.paneltitle.shady = short(UI_SCALE_FAC * _style.paneltitle.shady);
  _style.grouplabel.shadx = short(UI_SCALE_FAC * _style.grouplabel.shadx);
  _style.grouplabel.shady = short(UI_SCALE_FAC * _style.grouplabel.shady);
  _style.widgetlabel.shadx = short(UI_SCALE_FAC * _style.widgetlabel.shadx);
  _style.widgetlabel.shady = short(UI_SCALE_FAC * _style.widgetlabel.shady);

  _style.columnspace = short(UI_SCALE_FAC * _style.columnspace);
  _style.templatespace = short(UI_SCALE_FAC * _style.templatespace);
  _style.boxspace = short(UI_SCALE_FAC * _style.boxspace);
  _style.buttonspacex = short(UI_SCALE_FAC * _style.buttonspacex);
  _style.buttonspacey = short(UI_SCALE_FAC * _style.buttonspacey);
  _style.panelspace = short(UI_SCALE_FAC * _style.panelspace);
  _style.panelouter = short(UI_SCALE_FAC * _style.panelouter);

  return &_style;
}

int UI_fontstyle_string_width(const uiFontStyle *fs, const char *str)
{
  UI_fontstyle_set(fs);
  return int(BLF_width(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX));
}

int UI_fontstyle_string_width_with_block_aspect(const uiFontStyle *fs,
                                                const char *str,
                                                const float aspect)
{
  /* FIXME(@ideasman42): the final scale of the font is rounded which should be accounted for.
   * Failing to do so causes bad alignment when zoomed out very far in the node-editor. */
  fontstyle_set_ex(fs, UI_SCALE_FAC / aspect);
  return int(BLF_width(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX) * aspect);
}

int UI_fontstyle_height_max(const uiFontStyle *fs)
{
  UI_fontstyle_set(fs);
  return BLF_height_max(fs->uifont_id);
}

/* ************** init exit ************************ */

void uiStyleInit()
{
  const uiStyle *style = static_cast<uiStyle *>(U.uistyles.first);

  /* Recover from uninitialized DPI. */
  if (U.dpi == 0) {
    U.dpi = 72;
  }
  CLAMP(U.dpi, 48, 144);

  /* Needed so that custom fonts are always first. */
  BLF_unload_all();

  uiFont *font_first = static_cast<uiFont *>(U.uifonts.first);

  /* default builtin */
  if (font_first == nullptr) {
    font_first = MEM_cnew<uiFont>(__func__);
    BLI_addtail(&U.uifonts, font_first);
  }

  if (U.font_path_ui[0]) {
    STRNCPY(font_first->filepath, U.font_path_ui);
    font_first->uifont_id = UIFONT_CUSTOM1;
  }
  else {
    STRNCPY(font_first->filepath, "default");
    font_first->uifont_id = UIFONT_DEFAULT;
  }

  LISTBASE_FOREACH (uiFont *, font, &U.uifonts) {
    const bool unique = false;

    if (font->uifont_id == UIFONT_DEFAULT) {
      font->blf_id = BLF_load_default(unique);
    }
    else {
      font->blf_id = BLF_load(font->filepath);
      if (font->blf_id == -1) {
        font->blf_id = BLF_load_default(unique);
      }
    }

    BLF_default_set(font->blf_id);

    if (font->blf_id == -1) {
      if (G.debug & G_DEBUG) {
        printf("%s: error, no fonts available\n", __func__);
      }
    }
  }

  if (style == nullptr) {
    style = ui_style_new(&U.uistyles, "Default Style", UIFONT_DEFAULT);
  }

  BLF_cache_flush_set_fn(UI_widgetbase_draw_cache_flush);

  BLF_default_size(style->widgetlabel.points);

  /* XXX, this should be moved into a style,
   * but for now best only load the monospaced font once. */
  BLI_assert(blf_mono_font == -1);
  /* Use unique font loading to avoid thread safety issues with mono font
   * used for render metadata stamp in threads. */
  if (U.font_path_ui_mono[0]) {
    blf_mono_font = BLF_load_unique(U.font_path_ui_mono);
  }
  if (blf_mono_font == -1) {
    const bool unique = true;
    blf_mono_font = BLF_load_mono_default(unique);
  }

  /* Set default flags based on UI preferences (not render fonts) */
  {
    const int flag_disable = (BLF_MONOCHROME | BLF_HINTING_NONE | BLF_HINTING_SLIGHT |
                              BLF_HINTING_FULL);
    int flag_enable = 0;

    if (U.text_render & USER_TEXT_HINTING_NONE) {
      flag_enable |= BLF_HINTING_NONE;
    }
    else if (U.text_render & USER_TEXT_HINTING_SLIGHT) {
      flag_enable |= BLF_HINTING_SLIGHT;
    }
    else if (U.text_render & USER_TEXT_HINTING_FULL) {
      flag_enable |= BLF_HINTING_FULL;
    }

    if (U.text_render & USER_TEXT_DISABLE_AA) {
      flag_enable |= BLF_MONOCHROME;
    }

    LISTBASE_FOREACH (uiFont *, font, &U.uifonts) {
      if (font->blf_id != -1) {
        BLF_disable(font->blf_id, flag_disable);
        BLF_enable(font->blf_id, flag_enable);
      }
    }
    if (blf_mono_font != -1) {
      BLF_disable(blf_mono_font, flag_disable);
      BLF_enable(blf_mono_font, flag_enable);
    }
  }

  /**
   * Second for rendering else we get threading problems,
   *
   * \note This isn't good that the render font depends on the preferences,
   * keep for now though, since without this there is no way to display many unicode chars.
   */
  if (blf_mono_font_render == -1) {
    const bool unique = true;
    blf_mono_font_render = BLF_load_mono_default(unique);
  }

  /* Load the fallback fonts last. */
  BLF_load_font_stack();
}

static void fontstyle_set_ex(const uiFontStyle *fs, const float dpi_fac)
{
  uiFont *font = uifont_to_blfont(fs->uifont_id);

  BLF_size(font->blf_id, fs->points * dpi_fac);
}

void UI_fontstyle_set(const uiFontStyle *fs)
{
  fontstyle_set_ex(fs, UI_SCALE_FAC);
}
