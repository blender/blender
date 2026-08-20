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
#include "UI_tree_view.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"
namespace blender {

}  // namespace blender

namespace blender::ed::animfilter {

class ChannelItem : public ui::AbstractTreeViewItem {
  AnimationChannel &channel_;

 public:
  ChannelItem(AnimationChannel &channel) : channel_(channel) {}

  void build_row(ui::Layout &layout) override
  {
    layout.label(channel_.label(), ICON_NONE);
  }
};

class ChannelTreeView : public ui::AbstractTreeView {

  SummaryChannel &root_;

  void add_channel_children(ChannelItem &parent, Span<AnimationChannel *> channels)
  {
    for (AnimationChannel *channel : channels) {
      ChannelItem &item = parent.add_tree_item<ChannelItem>(*channel);
      if (channel->type() == DNAChannelType::SUMMARY) {
        SummaryChannel *summary = static_cast<SummaryChannel *>(channel);
        add_channel_children(item, summary->children());
      }
      item.uncollapse_by_default();
    }
  }

 public:
  ChannelTreeView(SummaryChannel &root) : root_(root)
  {
    /* TODO disable scroll bar. */
  }

  void build_tree() override
  {
    ChannelItem &root_item = add_tree_item<ChannelItem>(root_);
    add_channel_children(root_item, root_.children());
    root_item.uncollapse_by_default();
  }
};

constexpr const float indent_per_level = 8;

static void draw_channel(const bContext &C,
                         AnimationChannel &anim_channel,
                         const int level,
                         const float row_top)
{
  uchar col[4];
  /* TODO color based on selection state. */
  ui::theme::get_color_4ubv(TH_TEXT, col);
  ui::Block *block = ui::block_begin(&C, CTX_wm_region(&C), __func__, ui::EmbossType::None);

  ui::fontstyle_draw_simple(
      UI_FSTYLE_WIDGET, level * indent_per_level, row_top, anim_channel.label().data(), col);

  if (anim_channel.type() == DNAChannelType::SUMMARY) {
    SummaryChannel &summary = reinterpret_cast<SummaryChannel &>(anim_channel);
    if (!summary.is_expanded()) {
      printf("expanded %d\n ", summary.is_expanded());
    }
    /* For some reason this can't be clicked. */
    ui::uiDefIconButBit(block,
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

void draw_channel_tree(const bContext &C, SummaryChannel &root, Panel &panel)
{
  /* I imagine the syncing of channel scrolling with dope sheet scrolling will be a pain. */
  ui::Layout *layout = panel.layout;
  ui::block_layout_set_current(layout->block(), layout);
  ui::AbstractTreeView *tree = ui::block_add_view(
      *layout->block(), "channel_tree", std::make_unique<ChannelTreeView>(root));
  ui::TreeViewBuilder::build_tree_view(C, *tree, *layout, false);
}

void draw_channel_tree(const bContext &C, SummaryChannel &root, View2D &v2d)
{
  const float channel_step = ANIM_UI_get_channel_step();
  const float first_channel = ANIM_UI_get_first_channel_top(&v2d);
  draw_channel_tree_recurse(C, root, 0, first_channel, channel_step);
}

}  // namespace blender::ed::animfilter
