/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface_c.hh"
#include "UI_resources.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

static float draw_channel_tree_recurse(AnimationChannel &anim_channel,
                                      const int level,
                                      float row_top,
                                      const float channel_step)
{
  uchar col[4];
  /* TODO color based on selection state. */
  ui::theme::get_color_4ubv(TH_TEXT, col);
  ui::fontstyle_draw_simple(
      UI_FSTYLE_WIDGET, level * 10, row_top, anim_channel.label().data(), col);
  row_top -= channel_step;

  if (anim_channel.type() == ChannelType::SUMMARY) {
    SummaryChannel &summary = reinterpret_cast<SummaryChannel &>(anim_channel);
    for (AnimationChannel *child : summary.children()) {
      row_top = draw_channel_tree_recurse(*child, level + 1, row_top, channel_step);
    }
  }
  return row_top;
}

void draw_channel_tree(SummaryChannel &root)
{
  const float channel_step = ANIM_UI_get_channel_step();
  draw_channel_tree_recurse(root, 0, 0, channel_step);
}

}  // namespace blender::ed::animfilter
