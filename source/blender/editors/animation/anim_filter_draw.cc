/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_rect.hh"

#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "DNA_userdef_types.h"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "UI_interface_c.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_tree_view.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"
#include "ED_keyframes_draw.hh"

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
      if (channel->type() == AnimationChannelType::SUMMARY) {
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

  if (anim_channel.type() == AnimationChannelType::SUMMARY) {
    // SummaryChannel &summary = reinterpret_cast<SummaryChannel &>(anim_channel);
    /* For some reason this can't be clicked. */
    /* ui::uiDefIconButBit(block,
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
                        TIP_("todo tooltip")); */

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

  if (anim_channel.type() == AnimationChannelType::SUMMARY) {
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
  ui::AbstractTreeView *tree_view = ui::block_add_view(
      *layout->block(), "channel_tree", std::make_unique<ChannelTreeView>(root));
  ui::TreeViewBuilder::build_tree_view(C, *tree_view, *layout, /*add_box=*/false);
}

void draw_channel_tree(const bContext &C, SummaryChannel &root, View2D &v2d)
{
  const float channel_step = ANIM_UI_get_channel_step();
  const float first_channel = ANIM_UI_get_first_channel_top(&v2d);
  draw_channel_tree_recurse(C, root, 0, first_channel, channel_step);
}

static void draw_channel_keys(AnimationChannel &channel, const float y, View2D &v2d)
{
  Span<KeyPoint> datapoints = channel.datapoints();
  if (datapoints.is_empty()) {
    return;
  }
  GPU_blend(GPU_BLEND_ALPHA);

  GPUVertFormat *format = immVertexFormat();
  KeyframeShaderBindings sh_bindings;

  sh_bindings.pos_id = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  sh_bindings.size_id = GPU_vertformat_attr_add(format, "size", gpu::VertAttrType::SFLOAT_32);
  sh_bindings.color_id = GPU_vertformat_attr_add(
      format, "color", gpu::VertAttrType::UNORM_8_8_8_8);
  sh_bindings.outline_color_id = GPU_vertformat_attr_add(
      format, "outlineColor", gpu::VertAttrType::UNORM_8_8_8_8);
  sh_bindings.flags_id = GPU_vertformat_attr_add(format, "flags", gpu::VertAttrType::UINT_32);

  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", 1.0f);
  immUniform2f("ViewportSize", BLI_rcti_size_x(&v2d.mask) + 1, BLI_rcti_size_y(&v2d.mask) + 1);
  immBegin(GPU_PRIM_POINTS, datapoints.size());

  for (const KeyPoint &point : datapoints) {
    draw_keyframe_shape(point.frame,
                        y,
                        U.widget_unit * 0.5f,
                        point.is_selected,
                        BEZT_KEYTYPE_KEYFRAME,
                        KEYFRAME_SHAPE_BOTH,
                        1.0,
                        &sh_bindings,
                        KEYFRAME_HANDLE_AUTO_CLAMP,
                        KEYFRAME_EXTREME_NONE);
  }

  immEnd();
  GPU_program_point_size(false);
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

static float draw_keys_recurse(const bContext &C,
                               AnimationChannel &channel,
                               View2D &v2d,
                               float row_top,
                               const float channel_step)
{
  draw_channel_keys(channel, row_top - channel_step / 2.0, v2d);
  row_top -= channel_step;

  if (channel.type() == AnimationChannelType::SUMMARY) {
    /* TODO ignore if channel is collapsed. */
    SummaryChannel *summary = static_cast<SummaryChannel *>(&channel);
    for (AnimationChannel *child : summary->children()) {
      row_top = draw_keys_recurse(C, *child, v2d, row_top, channel_step);
    }
  }
  return row_top;
}

static float draw_key_area_background_recurse(const bContext &C,
                                              AnimationChannel &channel,
                                              View2D &v2d,
                                              float row_top,
                                              const float channel_step,
                                              const uint pos)
{
  const float skip = ANIM_UI_get_channel_skip();
  /* TODO get correct color. */
  immUniformThemeColor(TH_CHANNEL);
  immRectf(pos,
           v2d.cur.xmin,
           row_top - skip / 2.0f,
           v2d.cur.xmax + 0,
           row_top - channel_step + skip / 2.0);

  row_top -= channel_step;
  if (channel.type() == AnimationChannelType::SUMMARY) {
    /* TODO ignore if channel is collapsed. */
    SummaryChannel *summary = static_cast<SummaryChannel *>(&channel);
    for (AnimationChannel *child : summary->children()) {
      row_top = draw_key_area_background_recurse(C, *child, v2d, row_top, channel_step, pos);
    }
  }
  return row_top;
}

void draw_dopesheet_key_area(const bContext &C, SummaryChannel &root, View2D &v2d)
{
  /* TODO get step from tree view. Also get expanded collapsed state from tree??*/
  const float channel_step = ANIM_UI_get_channel_step();
  const float first_channel = ANIM_UI_get_first_channel_top(&v2d);

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_blend(GPU_BLEND_ALPHA);
  draw_key_area_background_recurse(C, root, v2d, first_channel, channel_step, pos);
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);

  draw_keys_recurse(C, root, v2d, first_channel, channel_step);
}

}  // namespace blender::ed::animfilter
