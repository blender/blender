/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_rect.hh"
#include "BLI_string.hh"

#include "BKE_context.hh"
#include "BKE_library.hh"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_userdef_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "UI_interface_c.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_tree_view.hh"
#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"
#include "ED_keyframes_draw.hh"
#include "ED_undo.hh"

namespace blender::ed::animfilter {

/* Returns the char * and max length of the name string. */
static PropertyRNA *name_prop_from_rna(PointerRNA &ptr)
{
  if (!ptr.has_data() || !ID_IS_EDITABLE(ptr.owner_id)) {
    return nullptr;
  }

  return RNA_struct_find_property(&ptr, "name");
}

static uchar4 channel_color_from_rna_pointer(const PointerRNA &ptr)
{
  uchar4 color(0, 0, 0, 0);
  /* All those are queried by the legacy system via `get_backdrop_color`. */
  /*
  TH_ANIM_ACTIVE = Summary
  TH_HEADER = Summary collapsed?
  TH_DOPESHEET_CHANNELOB
  TH_DOPESHEET_CHANNELSUBOB
  TH_GROUP
  TH_GROUP_ACTIVE
  TH_CHANNEL
  */
  if (!ptr) {
    /* Summary color. Currently the summary is the only channel without a valid pointer. */
    blender::ui::theme::get_color_4ubv(TH_ANIM_ACTIVE, color);
  }
  else if (RNA_struct_is_ID(ptr.type)) {
    blender::ui::theme::get_color_4ubv(TH_DOPESHEET_CHANNELOB, color);
  }
  else if (ptr.type == RNA_ActionSlot || ptr.type == RNA_ActionLayer) {
    blender::ui::theme::get_color_4ubv(TH_DOPESHEET_CHANNELSUBOB, color);
  }
  else if (ptr.type == RNA_PoseBone || ptr.type == RNA_ActionGroup) {
    blender::ui::theme::get_color_4ubv(TH_GROUP, color);
  }
  return color;
}

class ChannelItem : public ui::AbstractTreeViewItem {
  PointerRNA source_;
  std::string label_;
  bool is_selected_;
  bool is_datachannel_;

 public:
  ChannelItem(AnimationChannel &channel)
      : source_(channel.source()),
        label_(channel.label()),
        is_selected_(channel.is_selected()),
        is_datachannel_(channel.type() == AnimationChannelType::DATA)
  {
  }

  void build_row(ui::Layout &layout) override
  {
    /* Only summary channels get a backdrop color. */
    if (!is_datachannel_) {
      /* TODO get this to draw below the label filling out the background of the layout. */
      ui::Block *block = layout.block();
      ui::block_emboss_set(block, ui::EmbossType::Emboss);
      ui::Button *bg = uiDefBut(block,
                                ui::ButtonType::Roundbox,
                                "",
                                0,
                                0,
                                UI_UNIT_X,
                                UI_UNIT_Y,
                                nullptr,
                                0.0f,
                                0.0f,
                                "");
      uchar4 color = channel_color_from_rna_pointer(this->source_);
      color[3] = 255;
      ui::button_color_set(bg, color);
    }

    layout.label(this->label_, this->is_selected_ ? ICON_BOIDS : ICON_NONE);

    if (is_datachannel_) {
      /* TODO make this work for GP. ideally we'd have access to the channel here still so we don't
       * need to if/else through RNA types. */
      if (source_.type == RNA_FCurve) {
        /* TODO those are not selectable for some reason. */
        layout.prop(
            &source_, "use_modifiers", ui::ITEM_R_ICON_ONLY, std::nullopt, ICON_MODIFIER_ON);
        layout.prop(&source_, "mute", ui::ITEM_R_ICON_ONLY, std::nullopt, ICON_CHECKBOX_HLT);
        layout.prop(&source_, "lock", ui::ITEM_R_ICON_ONLY, std::nullopt, ICON_UNLOCKED);
      }
    }
  }

 protected:
  bool matches(const AbstractViewItem &other) const override
  {
    const ChannelItem &other_channel_item = dynamic_cast<const ChannelItem &>(other);
    /* TODO: this matches all AnimationChannels without a source to each other. For now it works,
     * because the only channel without a source is the top-level summary, and there is only one of
     * those. */
    return this->source_.owner_id == other_channel_item.source_.owner_id &&
           this->source_.data == other_channel_item.source_.data;
  }

  bool supports_renaming() const override
  {
    return name_prop_from_rna(const_cast<PointerRNA &>(this->source_)) != nullptr;
  }

  StringRef get_rename_string() const override
  {
    return this->label_;
  }

  bool rename(const bContext &C, StringRefNull new_name) override
  {
    PropertyRNA *prop = name_prop_from_rna(const_cast<PointerRNA &>(this->source_));
    RNA_property_string_set(&this->source_, prop, new_name.c_str());
    RNA_property_update(&const_cast<bContext &>(C), &this->source_, prop);

    ED_undo_push(&const_cast<bContext &>(C), "Rename Animation Channel");

    label_ = new_name;
    return true;
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
  ChannelTreeView(SummaryChannel &root) : root_(root) {}

  void build_tree() override
  {
    ChannelItem &root_item = add_tree_item<ChannelItem>(root_);
    add_channel_children(root_item, root_.children());
    root_item.uncollapse_by_default();
  }
};

void draw_channel_tree(const bContext &C, SummaryChannel &root, View2D & /* v2d */)
{
  ARegion *region = CTX_wm_region(&C);
  ui::Block *block = ui::block_begin(&C, region, __func__, ui::EmbossType::None);
  ui::Layout &layout = ui::block_layout(block,
                                        ui::LayoutDirection::Vertical,
                                        ui::LayoutType::Panel,
                                        0,
                                        region->winy - UI_TIME_SCRUB_MARGIN_Y -
                                            ANIM_UI_get_channel_skip(),
                                        region->winx,
                                        0,
                                        0,
                                        ui::style_get());

  ui::AbstractTreeView *tree_view = ui::block_add_view(
      *block, "channel_tree", std::make_unique<ChannelTreeView>(root));
  /* Just making it explicit that this is the row height we use as a base. */
  tree_view->set_row_height(UI_UNIT_Y);
  tree_view->set_row_scale_y(ANIM_UI_get_channel_scale_factor());
  tree_view->set_row_padding(ANIM_UI_get_channel_skip());
  ui::TreeViewBuilder::build_tree_view(C, *tree_view, layout, /*add_box=*/false);

  ui::block_layout_resolve(block);
  ui::block_align_end(block);
  ui::block_end(&C, block);
  ui::block_draw(&C, block);
}

static float get_key_height()
{
  return UI_UNIT_Y * 0.5;
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
  const float key_size = get_key_height();
  for (const KeyPoint &point : datapoints) {
    draw_keyframe_shape(point.frame,
                        y,
                        key_size,
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

/* Draw extras that are not keyframes like lines between keys. */
static void draw_channel_decorations(AnimationChannel &channel, const float y)
{
  Span<KeyPoint> datapoints = channel.datapoints();
  if (datapoints.size() < 2) {
    return;
  }

  float4 color_sel;
  float4 color_unsel;
  float4 ipo_color_linear;
  float4 ipo_color_constant;
  float4 ipo_color_other;
  float4 ipo_color_mix;

  ui::theme::get_color_4fv(TH_LONGKEY_SELECT, color_sel);
  ui::theme::get_color_4fv(TH_LONGKEY, color_unsel);
  ui::theme::get_color_4fv(TH_DOPESHEET_IPOLINE, ipo_color_linear);
  ui::theme::get_color_4fv(TH_DOPESHEET_IPOCONST, ipo_color_constant);
  ui::theme::get_color_4fv(TH_DOPESHEET_IPOOTHER, ipo_color_other);
  ui::theme::get_color_4fv(TH_KEYTYPE_KEYFRAME, ipo_color_mix);
  /* Logic copied from `channel_ui_data_init`. */
  ipo_color_mix[3] *= 0.5;

  /* See `channel_ui_data_init` for reference. */
  const int motion_line_height = get_key_height();
  const int interpolation_line_height = 0.1f * UI_UNIT_Y;
  IndexRange indices = datapoints.index_range();
  /* Decorations are always drawn to the next point, so we can ignore the last point. */
  indices = indices.drop_back(1);
  for (const int i : indices) {
    const KeyPoint &point = datapoints[i];
    rctf box;
    box.xmin = point.frame;
    box.xmax = datapoints[i + 1].frame;

    if (!point.has_motion) {
      box.ymin = y - motion_line_height / 2;
      box.ymax = y + motion_line_height / 2;
      ui::draw_roundbox_4fv(&box, true, 3.0f, point.is_selected ? color_sel : color_unsel);
    }

    const float *interpolation_color = nullptr;

    switch (point.interpolation) {
      case InterpolationSummary::LINEAR:
        interpolation_color = ipo_color_linear;
        break;
      case InterpolationSummary::CONSTANT:
        interpolation_color = ipo_color_constant;
        break;
      case InterpolationSummary::MIXED:
        interpolation_color = ipo_color_mix;
        break;
      default:
        break;
    }
    if (interpolation_color) {
      box.ymin = y - interpolation_line_height / 2;
      box.ymax = y + interpolation_line_height / 2;
      ui::draw_roundbox_4fv(&box, true, 3.0f, interpolation_color);
    }
  }
}

static float draw_keys_recurse(const bContext &C,
                               AnimationChannel &channel,
                               View2D &v2d,
                               float row_top,
                               const float channel_step)
{
  const float channel_y = row_top - channel_step / 2.0;
  draw_channel_decorations(channel, channel_y);
  draw_channel_keys(channel, channel_y, v2d);
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
  uchar4 color = channel_color_from_rna_pointer(channel.source());
  if (channel.type() == AnimationChannelType::DATA) {
    blender::ui::theme::get_color_4ubv(TH_CHANNEL, color);
  }
  immUniformColor4ubv(color);
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
