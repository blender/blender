/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Draw the IME composition preview: the pre-edit text at the cursor, with its underlines.
 * Shared by the editors which support IME, #ARegionType::cursor_ime
 * although it's only loosely coupled.
 */

#ifdef WITH_INPUT_IME

#  include <algorithm>
#  include <optional>

#  include "BLI_assert.hh"
#  include "BLI_index_range.hh"
#  include "BLI_string_utf8.hh"

#  include "DNA_screen_types.h"
#  include "DNA_vec_types.h"

#  include "BKE_screen.hh"

#  include "BLF_api.hh"
#  include "GPU_immediate.hh"
#  include "GPU_state.hh"

#  include "ED_ime.hh"
#  include "UI_resources.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

namespace blender::ed::ime {

/**
 * Draw the preview at the cursor.
 *
 * Helper for #region_overlay_draw.
 */
static void cursor_overlay_draw(const wmIMEData &ime_data,
                                const rcti &cursor_rect,
                                const int font_size,
                                const int region_size[2],
                                const float pixelsize)
{
  BLI_assert(!ime_data.composite.empty());
  const char *str = ime_data.composite.c_str();
  const int str_len = int(ime_data.composite.size());

  const int line_height = cursor_rect.ymax - cursor_rect.ymin;

  /* NOTE(@ideasman42): the font is currently hard-coded & mono,
   * which matches the text editor & console but 3D text or text strips
   * could use custom fonts. Variable width support would be a little more involved here. */
  const int font_id = blf_mono_font;
  BLF_size(font_id, font_size);
  const int char_width = std::max(int(BLF_fixed_width(font_id)), 1);

  auto column_from_byte_offset_fn = [&](const int64_t byte_offset) {
    return BLI_str_utf8_offset_to_column(str, size_t(str_len), int(byte_offset));
  };
  const int columns = column_from_byte_offset_fn(str_len);

  /* Snap the line width to whole pixels, every edge below is an integer so the rectangles
   * rasterize to exact pixel bounds, see #wmOrtho2_region_pixelspace. */
  const int border = std::max(int(pixelsize), 1);

  /* Place a one-line box at the cursor, where the text lands once the composition is accepted,
   * so accepting it doesn't shift the text sideways. Clamped inside the region. */
  const int box_width = columns * char_width;
  const int box_x = std::clamp(cursor_rect.xmin, 0, std::max(region_size[0] - box_width, 0));
  const int box_y = std::clamp(cursor_rect.ymin, 0, std::max(region_size[1] - line_height, 0));
  const rcti box = {
      .xmin = box_x,
      .xmax = box_x + box_width,
      .ymin = box_y,
      .ymax = box_y + line_height,
  };

  /* Lift the baseline by the descender so glyphs fit the box, the underlines fill the gap
   * below. */
  const int baseline_y = box.ymin - BLF_descender(font_id);
  const int underline_y = box.ymin;

  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  /* Draw the background & outline, so the preview covers whatever is behind it.
   * The outline doesn't extend left, that would overlap the cursor and the text before it. */
  immUniformThemeColor(TH_SHADE1);
  immRectf(pos, box.xmin, box.ymin - border, box.xmax + border, box.ymax + border);
  immUniformThemeColor(TH_BACK);
  immRectf(pos, box.xmin, box.ymin, box.xmax, box.ymax);
  immUnbindProgram();

  ui::theme::font_theme_color_set(font_id, TH_TEXT);
  BLF_position(font_id, box.xmin, baseline_y, 0);
  BLF_draw_mono(font_id, str, size_t(str_len), char_width, 1);

  pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Draw a thin underline across the whole composite. */
  immUniformThemeColor(TH_TEXT);
  immRectf(pos, box.xmin, underline_y, box.xmax, underline_y + border);

  /* Draw the thick line under the selection. */
  if (const std::optional<IndexRange> sel_range = WM_window_IME_composite_select_range(&ime_data))
  {
    /* Byte offsets convert to columns in order, so the start can't come after the end. */
    const IndexRange sel_cols = IndexRange::from_begin_end(
                                    column_from_byte_offset_fn(sel_range->start()),
                                    column_from_byte_offset_fn(sel_range->one_after_last()))
                                    .intersect(IndexRange(columns));
    if (!sel_cols.is_empty()) {
      immRectf(pos,
               box.xmin + int(sel_cols.start()) * char_width,
               underline_y,
               box.xmin + int(sel_cols.one_after_last()) * char_width,
               underline_y + (2 * border));
    }
  }

  /* Draw the cursor within the composite. */
  if (!ime_data.composite.empty() && (ime_data.cursor_pos != -1)) {
    const int cursor_width = 2 * border;
    const int cursor_x = box.xmin +
                         std::min(column_from_byte_offset_fn(ime_data.cursor_pos) * char_width,
                                  std::max(box_width - cursor_width, 0));
    immUniformThemeColor(TH_WIDGET_TEXT_CURSOR);
    immRectf(pos, cursor_x, box.ymin, cursor_x + cursor_width, box.ymax);
  }
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

void region_overlay_draw(wmWindow *win,
                         const ARegion *region,
                         const float pixelsize,
                         const ARegionIMECursor &cursor)
{
  const wmIMEData *ime_data = WM_window_IME_data_get(win, region);
  if (ime_data == nullptr) {
    return;
  }
  const int region_size[2] = {region->winx, region->winy};
  cursor_overlay_draw(*ime_data, cursor.rect, cursor.font_size, region_size, pixelsize);
}

}  // namespace blender::ed::ime

#endif /* WITH_INPUT_IME */
