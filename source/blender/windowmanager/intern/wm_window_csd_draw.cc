/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Window client-side-decorations (CSD) drawing.
 */

#include "DNA_vec_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_rect.h"

#include "GHOST_C-api.h"

#include "GPU_immediate.hh"
#include "GPU_state.hh"
#include "GPU_viewport.hh" /* #GLA_PIXEL_OFS */

#include "WM_api.hh"
#include "wm_window.hh"
#include "wm_window_private.hh" /* Own include. */

#include "UI_interface_c.hh"
#include "UI_resources.hh"

#include "BLF_api.hh"

/* -------------------------------------------------------------------- */
/** \name Window Title Bar Drawing
 *
 * For systems with client-side-decorations (CSD).
 * \{ */

void WM_window_csd_draw_titlebar_ex(const int win_size[2],
                                    const char win_state,
                                    const GHOST_CSD_Layout *csd_layout,
                                    const bool is_active,
                                    const uint16_t dpi,
                                    const char *title,
                                    const int font_id,
                                    const int font_size,
                                    const uchar border_color[3],
                                    const uchar text_color[3],
                                    const float alpha)
{
  GHOST_CSD_Elem csd_elems_orig[GHOST_kCSDType_NUM];

  const int fractional_scale[2] = {
      GHOST_CSD_DPI_FRACTIONAL_BASE,
      dpi,
  };
  const int csd_elems_num = WM_window_csd_layout_callback(
      win_size, fractional_scale, win_state, csd_layout, csd_elems_orig);

  if (csd_elems_num <= 0) {
    return;
  }

  if (border_color) {
    GPU_clear_color(
        border_color[0] / 255.0f, border_color[1] / 255.0f, border_color[2] / 255.0f, 1.0f);

    /* Window border, if needed. */
    if (win_state == GHOST_kWindowStateNormal) {
      const uchar border_outline_color[4] = {
          uchar(border_color[0] / 2),
          uchar(border_color[1] / 2),
          uchar(border_color[2] / 2),
          255,
      };
      const int border_outline_width = std::max<int>(
          1, WM_window_csd_fracitonal_scale_apply(2, fractional_scale));
      const rcti window_rect = {
          /*xmin*/ 0,
          /*xmax*/ win_size[0],
          /*ymin*/ 0,
          /*ymax*/ win_size[1],
      };

      wmWindowViewportTitle_ex(window_rect, 0);

      const uint shdr_pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
      immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
      immUniformColor4ubv(border_outline_color);

      float viewport[4];
      GPU_viewport_size_get_f(viewport);
      immUniform2fv("viewportSize", &viewport[2]);

      immUniform1f("lineWidth", border_outline_width);

      /* Pixel offsets are needed for the lines to display evenly. */
      immBegin(GPU_PRIM_LINES, 8);
      /* Left. */
      immVertex2f(shdr_pos, window_rect.xmin + 1, window_rect.ymin);
      immVertex2f(shdr_pos, window_rect.xmin + 1, window_rect.ymax);
      /* Top. */
      immVertex2f(shdr_pos, window_rect.xmin, window_rect.ymax - 1);
      immVertex2f(shdr_pos, window_rect.xmax, window_rect.ymax - 1);
      /* Right. */
      immVertex2f(shdr_pos, window_rect.xmax, window_rect.ymax);
      immVertex2f(shdr_pos, window_rect.xmax, window_rect.ymin);
      /* Bottom. */
      immVertex2f(shdr_pos, window_rect.xmax, window_rect.ymin);
      immVertex2f(shdr_pos, window_rect.xmin, window_rect.ymin);

      immEnd();

      immUnbindProgram();
    }
  }

  /* Flip the Y axis. */
  GHOST_CSD_Elem csd_elems[GHOST_kCSDType_NUM];
  for (int i = 0; i < GHOST_kCSDType_NUM; i++) {
    csd_elems[i].type = GHOST_kCSDTypeBody;
    csd_elems[i].bounds[0][0] = 0;
    csd_elems[i].bounds[0][1] = 0;
    csd_elems[i].bounds[1][0] = 0;
    csd_elems[i].bounds[1][1] = 0;
  }
  for (int i = 0; i < csd_elems_num; i++) {
    GHOST_CSD_Elem *elem = &csd_elems[csd_elems_orig[i].type];
    *elem = csd_elems_orig[i];
    elem->bounds[1][0] = win_size[1] - elem->bounds[1][0];
    elem->bounds[1][1] = win_size[1] - elem->bounds[1][1];
    std::swap(elem->bounds[1][0], elem->bounds[1][1]);
  }

  BLI_assert(csd_elems[GHOST_kCSDTypeTitlebar].type == GHOST_kCSDTypeTitlebar);
  const rcti title_rect = {
      /*xmin*/ csd_elems[GHOST_kCSDTypeTitlebar].bounds[0][0],
      /*xmax*/ csd_elems[GHOST_kCSDTypeTitlebar].bounds[0][1],
      /*ymin*/ csd_elems[GHOST_kCSDTypeTitlebar].bounds[1][0],
      /*ymax*/ csd_elems[GHOST_kCSDTypeTitlebar].bounds[1][1],
  };
  const int rect_size_y = BLI_rcti_size_y(&title_rect);

  wmWindowViewportTitle_ex(title_rect, 0.0f);
  if (title) {
    const float px_offset = -GLA_PIXEL_OFS;
    const size_t title_len = strlen(title);
    uchar color[4];
    if (!is_active) {
      if (border_color) {
        color[0] = uchar((int(border_color[0]) + int(text_color[0])) / 2);
        color[1] = uchar((int(border_color[1]) + int(text_color[1])) / 2);
        color[2] = uchar((int(border_color[2]) + int(text_color[2])) / 2);
      }
      else {
        color[0] = text_color[0] / 2;
        color[1] = text_color[1] / 2;
        color[2] = text_color[2] / 2;
      }
    }
    else {
      ARRAY_SET_ITEMS(color, UNPACK3(text_color), uchar(255 * alpha));
    }
    BLF_color4ubv(font_id, color);
    if (border_color == nullptr) {
      const float shadow_color[4] = {0.0f, 0.0f, 0.0f, alpha};
      BLF_enable(font_id, BLF_SHADOW);
      BLF_shadow(font_id, FontShadowType::Outline, shadow_color);
      BLF_shadow_offset(font_id, 0, 0);
    }
    BLF_enable(font_id, BLF_BOLD);
    BLF_size(font_id, WM_window_csd_fracitonal_scale_apply(int(font_size), fractional_scale));

    const int title_width = BLF_width(font_id, title, title_len);
    const int title_decender = -BLF_descender(font_id);

    const int title_height_max = BLF_height_max(font_id);
    const int offset_y = rect_size_y > title_height_max ? (rect_size_y - title_height_max) / 2 : 0;

    BLF_position(font_id,
                 float(title_rect.xmin + (BLI_rcti_cent_x(&title_rect) - (title_width / 2))) +
                     px_offset,
                 float(title_decender + offset_y) + px_offset,
                 0);

    BLF_draw(font_id, title, title_len);
    BLF_disable(font_id, BLF_BOLD);
    if (border_color == nullptr) {
      BLF_disable(font_id, BLF_SHADOW);
    }
  }

  /* Draw buttons (starting at the title region offset). */
  {
    constexpr int circle_segments = 16;
    GPUVertFormat *format = immVertexFormat();
    const uint shdr_pos = GPU_vertformat_attr_add(
        format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    GPU_blend(GPU_BLEND_ALPHA);

    GPU_polygon_smooth(true);

    const GHOST_TCSD_Type button_types[] = {
        GHOST_kCSDTypeButtonClose,
        GHOST_kCSDTypeButtonMaximize,
        GHOST_kCSDTypeButtonMinimize,
        GHOST_kCSDTypeButtonMenu,
    };

    const int button_icons[] = {
        ICON_X,
        (win_state == GHOST_kWindowStateMaximized) ? ICON_AREA_DOCK : ICON_CHECKBOX_DEHLT,
        ICON_DOT,
        ICON_BLENDER,
    };

    {
      const int button_margin = rect_size_y / 12;
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      if (border_color) {
        immUniformColor4f(1.0f, 1.0f, 1.0f, 0.15f);
      }
      else {
        immUniformColor4f(0.25f, 0.25f, 0.25f, 0.5f * alpha);
      }

      for (int i = 0; i < ARRAY_SIZE(button_types); i++) {
        const GHOST_TCSD_Type ty = button_types[i];
        if (UNLIKELY(csd_elems[ty].type == GHOST_kCSDTypeBody)) {
          continue;
        }
        const rcti butrect = {
            /*xmin*/ csd_elems[ty].bounds[0][0] - title_rect.xmin,
            /*xmax*/ csd_elems[ty].bounds[0][1] - title_rect.xmin,
            /*ymin*/ csd_elems[ty].bounds[1][0] - title_rect.ymin,
            /*ymax*/ csd_elems[ty].bounds[1][1] - title_rect.ymin,
        };
        const int but_radius = (BLI_rcti_size_x(&butrect) / 2) - button_margin;
        const int center[2] = {
            BLI_rcti_cent_x(&butrect),
            BLI_rcti_cent_y(&butrect),
        };
        imm_draw_circle_fill_2d(shdr_pos, UNPACK2(center), but_radius, circle_segments);
      }
      immUnbindProgram();
    }

    const float button_color[4] = {1.0f, 1.0f, 1.0f, alpha};
    const int icon_size = WM_window_csd_fracitonal_scale_apply(ICON_DEFAULT_HEIGHT,
                                                               fractional_scale);
    for (int i = 0; i < ARRAY_SIZE(button_types); i++) {
      const GHOST_TCSD_Type ty = button_types[i];
      if (UNLIKELY(csd_elems[ty].type == GHOST_kCSDTypeBody)) {
        continue;
      }
      const rcti butrect = {
          /*xmin*/ csd_elems[ty].bounds[0][0] - title_rect.xmin,
          /*xmax*/ csd_elems[ty].bounds[0][1] - title_rect.xmin,
          /*ymin*/ csd_elems[ty].bounds[1][0] - title_rect.ymin,
          /*ymax*/ csd_elems[ty].bounds[1][1] - title_rect.ymin,
      };

      const int xy[2] = {
          BLI_rcti_cent_x(&butrect) - (icon_size / 2),
          BLI_rcti_cent_y(&butrect) - (icon_size / 2),
      };
      BLF_draw_svg_icon(button_icons[i], UNPACK2(xy), icon_size, button_color, 0, false, nullptr);
    }

    GPU_polygon_smooth(false);

    GPU_blend(GPU_BLEND_NONE);
  }
}

void WM_window_csd_draw_titlebar(const wmWindow *win)
{
  BLI_assert(WM_window_is_csd(win));
  const blender::int2 win_size = WM_window_native_pixel_size(win);
  const GHOST_CSD_Layout *csd_layout = WM_window_csd_layout_get();
  const uint16_t dpi = GHOST_GetDPIHint(static_cast<GHOST_WindowHandle>(win->runtime->ghostwin));
  const char win_state = GHOST_TWindowState(win->windowstate);
  char *title = GHOST_GetTitle(static_cast<GHOST_WindowHandle>(win->runtime->ghostwin));
  const bool is_active = (win->active != 0);

  uchar border_color[3];
  blender::ui::theme::get_color_3ubv(TH_HEADER, border_color);

  uchar text_color[3];
  blender::ui::theme::get_color_3ubv(TH_TEXT_HI, text_color);

  const uiStyle *style = blender::ui::style_get_dpi();
  const uiFontStyle &fstyle = style->paneltitle;

  const int font_id = fstyle.uifont_id;
  const int font_size = fstyle.points;

  const float alpha = 1.0f;
  WM_window_csd_draw_titlebar_ex(win_size,
                                 win_state,
                                 csd_layout,
                                 is_active,
                                 dpi,
                                 title,
                                 font_id,
                                 font_size,
                                 border_color,
                                 text_color,
                                 alpha);
  if (title) {
    free(title);
  }
}

/** \} */
