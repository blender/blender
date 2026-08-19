/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "DNA_userdef_types.h"

#include "UI_interface_c.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

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
  ui::fontstyle_draw_simple(
      UI_FSTYLE_WIDGET, level * 10, row_top, anim_channel.label().data(), col);

  if (is_summary) {
    ui::uiDefIconButBit(block,
                        ui::ButtonType::IconToggleN,
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
    /* For adding operators. */
    // ui::uiDefIconButO()
  }

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
