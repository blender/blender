/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_rect.hh"
#include "BLI_string.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_library.hh"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_action_types.h"
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
#include "ED_space_action.hh"
#include "ED_space_graph.hh"
#include "ED_undo.hh"

#include "WM_api.hh"

namespace blender::ed::animfilter {

/* Returns the char * and max length of the name string. */
static PropertyRNA *name_prop_from_rna(PointerRNA &ptr)
{
  if (!ptr.has_data() || !ID_IS_EDITABLE(ptr.owner_id)) {
    return nullptr;
  }

  return RNA_struct_name_property(ptr.type);
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
  else if (ptr.type == RNA_PoseBone || ptr.type == RNA_ActionGroup ||
           ptr.type == RNA_GreasePencilLayerGroup)
  {
    blender::ui::theme::get_color_4ubv(TH_GROUP, color);
  }
  return color;
}

static int icon_from_rna_pointer(const PointerRNA &ptr)
{
  /* Special cases for which we don't want icons. */
  if (ptr.type == RNA_FCurve) {
    return ICON_NONE;
  }
  const int icon = RNA_struct_ui_icon(ptr.type);
  /* The above returns ICON_DOT as a default. */
  if (icon != ICON_DOT) {
    return icon;
  }
  return ICON_NONE;
}

class ChannelItem : public ui::AbstractTreeViewItem {
  /* Keeping a reference to the short-lived AnimationChannel is fine, because the ChannelItem is
   * short-lived as well. Only specific properties are carried over between draw calls (see
   * #AbstractTreeViewItem::update_from_old().
   *
   * The correctness does depend on the channels being stored on the runtime data of the space
   * until the next redraw. */
  AnimationChannel &channel_;

  /* Cached locally for convenience and for access after the above channel has been freed. */
  PointerRNA source_;
  ChannelIdentifier chan_ident_;
  std::unique_ptr<AbstractChannelAPI> channel_api_;

 public:
  ChannelItem(AnimationChannel &channel)
      : channel_(channel), source_(channel.source()), chan_ident_(channel.identifier())
  {
    this->channel_api_ = create_channel_api(channel);
  }

  void build_row(ui::Layout &layout) override;

  void on_collapse_change(bContext &C, const bool is_collapsed) override
  {
    SpaceAction *saction = CTX_wm_space_action(&C);
    SpaceGraph *space_graph = CTX_wm_space_graph(&C);
    if (saction) {
      AnimationChannelUIState &channel_state =
          saction->runtime->animfilter_runtime.channel_state_for_update(channel_.identifier());
      /* Store the expanded/collapsed state. */
      channel_state.is_collapsed = is_collapsed;
    }
    else if (space_graph) {
      AnimationChannelUIState &channel_state =
          space_graph->runtime->animfilter_runtime.channel_state_for_update(channel_.identifier());
      /* Store the expanded/collapsed state. */
      channel_state.is_collapsed = is_collapsed;
    }
    else {
      /* TODO find a better way to support different editors. */
      BLI_assert_unreachable();
      return;
    }

    /* Trigger redrawing of the keys. */
    WM_event_add_notifier(&C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
  }

  std::optional<bool> should_be_collapsed() const override
  {
    if (channel_.type() != AnimationChannelType::SUMMARY) {
      return {}; /* Only summary channels have a way to collapse/expand them. */
    }
    const SummaryChannel &summary = dynamic_cast<const SummaryChannel &>(channel_);
    return summary.is_collapsed();
  }

  std::optional<bool> should_be_selected() const override
  {
    return this->channel_api_->is_selected();
  }

  void set_selected(const bool select) override;

 protected:
  bool matches(const AbstractViewItem &other) const override
  {
    const ChannelItem &other_channel_item = dynamic_cast<const ChannelItem &>(other);
    return this->chan_ident_ == other_channel_item.chan_ident_;
  }

  bool supports_renaming() const override
  {
    PropertyRNA *prop = name_prop_from_rna(const_cast<PointerRNA &>(this->source_));
    if (!prop) {
      return false;
    }
    /* TODO: call RNA_property_editable_info() and show the 'disabled' message somewhere. */
    return RNA_property_editable(&this->source_, prop);
  }

  StringRef get_rename_string() const override
  {
    return this->channel_.label();
  }

  bool rename(const bContext &C, StringRefNull new_name) override
  {
    PropertyRNA *prop = name_prop_from_rna(this->source_);
    RNA_property_string_set(&this->source_, prop, new_name.c_str());
    RNA_property_update(&const_cast<bContext &>(C), &this->source_, prop);

    ED_undo_push(&const_cast<bContext &>(C), "Rename Animation Channel");

    label_ = new_name;
    return true;
  }
};

class ChannelTreeView : public ui::AbstractTreeView {
  SummaryChannel &root_;
  const ChannelTreeSettings tree_settings_;

  void add_channel_recursive(TreeViewItemContainer &container, AnimationChannel &channel)
  {
    /* Create the tree item for the channel. */
    const ChannelIdentifier chan_ident = channel.identifier();
    ChannelItem &item = container.add_tree_item<ChannelItem>(channel);
    /* TODO: maybe use the channelitem's UI state here already? */
    item.uncollapse_by_default();
    item.select_on_click_set();

    /* Set the color based on the channel type. */
    if (channel.type() == AnimationChannelType::SUMMARY) {
      uchar4 color = channel_color_from_rna_pointer(channel.source());
      /* This makes the tree view colors brighter than the key 'swimlanes'.
       * Skipping this will break the background color of the active tree item, making it appear
       * completely transparent */
      color[3] = 255;
      item.set_custom_color(color);
    }

    /* Recurse into children. */
    if (channel.type() == AnimationChannelType::SUMMARY) {
      SummaryChannel &summary = static_cast<SummaryChannel &>(channel);
      for (AnimationChannel *child : summary.children()) {
        add_channel_recursive(item, *child);
      }
    }
  }

 public:
  ChannelTreeView(SummaryChannel &root, const ChannelTreeSettings &tree_settings)
      : root_(root), tree_settings_(tree_settings)
  {
  }

  void build_tree() override
  {
    if (tree_settings_.show_summary) {
      this->add_channel_recursive(*this, root_);
    }
    else {
      for (AnimationChannel *child : root_.children()) {
        this->add_channel_recursive(*this, *child);
      }
    }
  }

  bool do_selection_syncing() const
  {
    return tree_settings_.do_selection_syncing;
  }
  bool show_visibility_property() const
  {
    return tree_settings_.show_visbility_property;
  }
};

/* A few functions need to be defined after the ChannelTreeView class. */
void ChannelItem::set_selected(const bool select)
{
  AbstractTreeViewItem::set_selected(select);

  if (this->channel_api_->is_selected() == select) {
    /* No change to selection state. This can easily happen, as set_selected() is called on every
     * tree item on rebuild of the tree (i.e. every redraw of the region). */
    return;
  }

  /* This flushes the selection state to the data, which is then read in the constructor. */
  const ChannelTreeView &tree_view = static_cast<ChannelTreeView &>(this->get_tree_view());
  this->channel_api_->set_selected(select, tree_view.do_selection_syncing());
}

void ChannelItem::build_row(ui::Layout &layout)
{
  const ChannelTreeView &tree_view = static_cast<ChannelTreeView &>(this->get_tree_view());

  /* TODO the layout creation code could move into the channel API. */
  /* Having the visibility icon before the channel name is just how the legacy system behaved.
   * Investigate if that is something to change. By putting it on the right side the icons could
   * be aligned vertically. This would also make it consistent with Grease Pencil channels in the
   * dope sheet where the hide icon is on the right. */
  if (channel_.type() == AnimationChannelType::DATA) {
    DataChannel &data_channel = static_cast<DataChannel &>(channel_);
    switch (data_channel.source_type()) {
      case DataChannelType::FCURVE:
        if (tree_view.show_visibility_property()) {
          layout.prop(&source_, "hide", ui::ITEM_R_ICON_ONLY, std::nullopt, ICON_HIDE_OFF);
        }
        break;
      case DataChannelType::GREASE_PENCIL:
        break;
    }
  }

  layout.label(channel_.label(), icon_from_rna_pointer(channel_.source()));

  if (channel_.type() == AnimationChannelType::DATA) {
    DataChannel &data_channel = static_cast<DataChannel &>(channel_);
    switch (data_channel.source_type()) {
      case DataChannelType::FCURVE: {
        layout.prop(
            &source_, "use_modifiers", ui::ITEM_R_ICON_ONLY, std::nullopt, ICON_MODIFIER_ON);
        layout.prop(&source_, "mute", ui::ITEM_R_ICON_ONLY, std::nullopt, ICON_CHECKBOX_HLT);
        layout.prop(&source_, "lock", ui::ITEM_R_ICON_ONLY, std::nullopt, ICON_UNLOCKED);
        break;
      }

      case DataChannelType::GREASE_PENCIL:
        break;
    }
  }
}

std::unique_ptr<ui::AbstractTreeView> build_channel_tree_view(
    ChannelArena &arena, const ChannelTreeSettings &tree_settings)
{
  return std::make_unique<ChannelTreeView>(*arena.root(), tree_settings);
}

void draw_channel_tree(const bContext &C, std::unique_ptr<ui::AbstractTreeView> channel_tree_view)
{
  ARegion *region = CTX_wm_region(&C);
  ui::Block *block = ui::block_begin(&C, region, __func__, ui::EmbossType::None);
  ui::Layout &layout = ui::block_layout(block,
                                        ui::LayoutDirection::Vertical,
                                        ui::LayoutType::Panel,
                                        0,
                                        -UI_TIME_SCRUB_MARGIN_Y - ANIM_UI_get_channel_skip(),
                                        region->winx,
                                        0,
                                        0,
                                        ui::style_get());

  ui::AbstractTreeView *tree_view = ui::block_add_view(
      *block, "channel_tree", std::move(channel_tree_view));

  /* Just making it explicit that this is the row height we use as a base. */
  tree_view->set_row_height(UI_UNIT_Y);
  tree_view->set_row_scale_y(ANIM_UI_get_channel_scale_factor());
  tree_view->set_row_padding(ANIM_UI_get_channel_skip());
  tree_view->set_indent_scale(0.5);
  tree_view->allow_multiselect_items();
  ui::TreeViewBuilder::build_tree_view(C, *tree_view, layout, /*add_box=*/false);

  ui::block_layout_resolve(block);
  ui::block_align_end(block);
  ui::block_end(&C, block);
  ui::block_draw(&C, block);
}

struct DrawSettings {
  /* Sizes. */
  /* See `channel_ui_data_init` for reference. */
  int motion_line_height = ANIM_UI_get_key_size();
  int interpolation_line_height = 0.1f * UI_UNIT_Y;
  /* Colors. */
  float4 sel;
  float4 unsel;
  float4 ipo_linear;
  float4 ipo_constant;
  float4 ipo_other;
  float4 ipo_mix;

  DrawSettings()
  {
    ui::theme::get_color_4fv(TH_LONGKEY_SELECT, sel);
    ui::theme::get_color_4fv(TH_LONGKEY, unsel);
    ui::theme::get_color_4fv(TH_DOPESHEET_IPOLINE, ipo_linear);
    ui::theme::get_color_4fv(TH_DOPESHEET_IPOCONST, ipo_constant);
    ui::theme::get_color_4fv(TH_DOPESHEET_IPOOTHER, ipo_other);
    ui::theme::get_color_4fv(TH_KEYTYPE_KEYFRAME, ipo_mix);

    /* Logic copied from `channel_ui_data_init`. */
    ipo_mix[3] *= 0.5;
  }
};

static bool point_less_than(const KeyPoint &p, const float frame)
{
  return p.frame < frame;
}

static bool point_larger_than(const float frame, const KeyPoint &p)
{
  return frame < p.frame;
}

static eKeyframeHandleDrawOpts handle_summary_as_draw_type(KeyframeHandleSummary summary)
{
  /* enum values match perfectly with eKeyframeHandleDrawOpts. */
  return eKeyframeHandleDrawOpts(summary);
}

static eKeyframeExtremeDrawOpts extreme_summary_as_draw_type(KeyframeExtremeSummary summary)
{
  switch (summary) {
    case KeyframeExtremeSummary::MIXED:
      return KEYFRAME_EXTREME_NONE;
    case KeyframeExtremeSummary::MAXIMUM:
      return KEYFRAME_EXTREME_MAX;
    case KeyframeExtremeSummary::MINIMUM:
      return KEYFRAME_EXTREME_MIN;
  }

  return KEYFRAME_EXTREME_NONE;
}

static void draw_channel_keys(AnimationChannel &channel,
                              const float y,
                              const View2D &v2d,
                              const KeyDrawSettings &key_draw_settings)
{
  Span<KeyPoint> datapoints = channel.datapoints();
  if (datapoints.is_empty()) {
    return;
  }
  const float key_size = ANIM_UI_get_key_size();
  /* Using binary search to find the bounds to draw. */
  const KeyPoint *start = std::lower_bound(
      datapoints.begin(), datapoints.end(), v2d.cur.xmin - key_size, point_less_than);
  if (start == datapoints.end()) {
    return;
  }
  const KeyPoint *end = std::upper_bound(
      start, datapoints.end(), v2d.cur.xmax + key_size, point_larger_than);
  const int key_count = std::distance(start, end);
  if (key_count == 0) {
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
  immBegin(GPU_PRIM_POINTS, key_count);

  for (const KeyPoint *point = start; point != end; point++) {
    draw_keyframe_shape(
        point->frame,
        y,
        key_size,
        point->is_selected,
        point->key_type,
        KEYFRAME_SHAPE_BOTH,
        1.0,
        &sh_bindings,
        key_draw_settings.draw_handle_types ? handle_summary_as_draw_type(point->handle_type) :
                                              KEYFRAME_HANDLE_NONE,
        key_draw_settings.draw_extremes ? extreme_summary_as_draw_type(point->extreme_type) :
                                          KEYFRAME_EXTREME_NONE);
  }

  immEnd();
  GPU_program_point_size(false);
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

/* Draw extras that are not keyframes like lines between keys. */
static void draw_channel_decorations(AnimationChannel &channel,
                                     const float y,
                                     const DrawSettings &settings,
                                     const View2D &v2d)
{
  Span<KeyPoint> datapoints = channel.datapoints();
  if (datapoints.size() < 2) {
    return;
  }

  IndexRange indices = datapoints.index_range();
  /* Decorations are always drawn to the next point, so we can ignore the last point. */
  indices = indices.drop_back(1);
  for (const int i : indices) {
    const KeyPoint &point = datapoints[i];
    rctf box;
    box.xmin = point.frame;
    box.xmax = datapoints[i + 1].frame;
    if (box.xmax < v2d.cur.xmin || box.xmin > v2d.cur.xmax) {
      continue;
    }

    if (!point.has_motion) {
      box.ymin = y - settings.motion_line_height / 2;
      box.ymax = y + settings.motion_line_height / 2;
      ui::draw_roundbox_4fv(&box, true, 3.0f, point.is_selected ? settings.sel : settings.unsel);
    }

    const float *interpolation_color = nullptr;

    switch (point.interpolation) {
      case InterpolationSummary::LINEAR:
        interpolation_color = settings.ipo_linear;
        break;
      case InterpolationSummary::CONSTANT:
        interpolation_color = settings.ipo_constant;
        break;
      case InterpolationSummary::MIXED:
        interpolation_color = settings.ipo_mix;
        break;
      default:
        break;
    }
    if (interpolation_color) {
      box.ymin = y - settings.interpolation_line_height / 2;
      box.ymax = y + settings.interpolation_line_height / 2;
      ui::draw_roundbox_4fv(&box, true, 3.0f, interpolation_color);
    }
  }
}

static bool is_channel_visible(const View2D &v2d, const float row_top, const float channel_step)
{
  return row_top > v2d.cur.ymin && row_top - channel_step < v2d.cur.ymax;
}

void draw_dopesheet_key_area(SummaryChannel &root,
                             View2D &v2d,
                             const KeyDrawSettings &key_draw_settings)
{
  const float channel_step = ANIM_UI_get_channel_step();
  const float skip = ANIM_UI_get_channel_skip();
  const float first_channel = ANIM_UI_get_first_channel_top(&v2d);

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_blend(GPU_BLEND_ALPHA);
  /* Draw backdrops first for each expanded channel. */
  foreach_expanded(root, [&](AnimationChannel &channel, const int row_index) {
    const float row_top = first_channel - row_index * channel_step;
    if (!is_channel_visible(v2d, row_top, channel_step)) {
      return;
    }
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
  });
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);

  DrawSettings settings;
  foreach_expanded(root, [&](AnimationChannel &channel, const int row_index) {
    const float row_top = first_channel - row_index * channel_step;
    if (is_channel_visible(v2d, row_top, channel_step)) {
      const float row_center = row_top - channel_step / 2.0;
      draw_channel_decorations(channel, row_center, settings, v2d);
      draw_channel_keys(channel, row_center, v2d, key_draw_settings);
    }
  });
}

}  // namespace blender::ed::animfilter
