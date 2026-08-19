/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "DNA_userdef_types.h"

#include "UI_interface_c.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

constexpr const float indent_per_level = 8;

static void draw_channel(const bContext &C,
                         AnimationChannel &anim_channel,
                         const int level,
                         const float row_top)
{
  const bool is_summary = anim_channel.type() == DNAChannelType::SUMMARY;
  uchar col[4];
  /* TODO color based on selection state. */
  ui::theme::get_color_4ubv(TH_TEXT, col);
  ui::Block *block = ui::block_begin(&C, CTX_wm_region(&C), __func__, ui::EmbossType::None);
  /* While using layouts would be great I just can't get them to work. */
  /* const uiStyle *style = ui::style_get_dpi();
  ui::Layout &layout = ui::block_layout(block,
                                        ui::LayoutDirection::Horizontal,
                                        ui::LayoutType::Panel,
                                        level * indent_per_level,
                                        row_top,
                                        0,
                                        0,
                                        0,
                                        style);

  layout.label(anim_channel.label(), ICON_NONE); */
  ui::fontstyle_draw_simple(
      UI_FSTYLE_WIDGET, level * indent_per_level, row_top, anim_channel.label().data(), col);

  if (is_summary) {
    SummaryChannel &summary = reinterpret_cast<SummaryChannel &>(anim_channel);
    if (!summary.is_expanded()) {
      printf("expanded %d\n ", summary.is_expanded());
    }
    ui::Button *btn = ui::uiDefIconButBit(block,
                                          ui::ButtonType::IconToggle,
                                          SUMMARY_EXPANDED,
                                          ICON_RIGHTARROW,
                                          0,
                                          row_top,
                                          UI_UNIT_X,
                                          UI_UNIT_Y,
                                          anim_channel.flags_ptr(),
                                          0,
                                          0,
                                          TIP_("todo tooltip"));

    // ui::button_funcN_set(btn, );
    /* For adding operators. */
    // ui::uiDefIconButO()
  }
  ui::block_layout_resolve(block);
  ui::block_end(&C, block);
  ui::block_draw(&C, block);
}

static float draw_channel_tree_recurse(const bContext &C,
                                       AnimationChannel &anim_channel,
                                       const int level,
                                       float row_top,
                                       const float channel_step)
{
  draw_channel(C, anim_channel, level, row_top);
  row_top -= channel_step;

  if (anim_channel.type() == DNAChannelType::SUMMARY) {
    SummaryChannel &summary = reinterpret_cast<SummaryChannel &>(anim_channel);
    for (AnimationChannel *child : summary.children()) {
      row_top = draw_channel_tree_recurse(C, *child, level + 1, row_top, channel_step);
    }
  }
  return row_top;
}

void draw_channel_tree(const bContext &C, SummaryChannel &root, View2D &v2d)
{
  const float channel_step = ANIM_UI_get_channel_step();
  const float first_channel = ANIM_UI_get_first_channel_top(&v2d);
  draw_channel_tree_recurse(C, root, 0, first_channel, channel_step);
}

}  // namespace blender::ed::animfilter
