/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_listbase.h"

#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_tree_view.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_grease_pencil.hh"
#include "ED_undo.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include <fmt/format.h>

namespace blender::ui::greasepencil {

using namespace blender::bke::greasepencil;

class LayerTreeView : public AbstractTreeView {
 public:
  explicit LayerTreeView(GreasePencil &grease_pencil) : grease_pencil_(grease_pencil) {}

  void build_tree() override;

 private:
  void build_tree_node_recursive(TreeViewOrItem &parent, TreeNode &node);
  GreasePencil &grease_pencil_;
};

class LayerNodeDropTarget : public TreeViewItemDropTarget {
  TreeNode &drop_tree_node_;

 public:
  LayerNodeDropTarget(AbstractTreeViewItem &item, TreeNode &drop_tree_node, DropBehavior behavior)
      : TreeViewItemDropTarget(item, behavior), drop_tree_node_(drop_tree_node)
  {
  }

  bool can_drop(const wmDrag &drag, const char ** /*r_disabled_hint*/) const override
  {
    if (!ELEM(drag.type, WM_DRAG_GREASE_PENCIL_LAYER, WM_DRAG_GREASE_PENCIL_GROUP)) {
      return false;
    }

    wmDragGreasePencilLayer *active_drag_node = static_cast<wmDragGreasePencilLayer *>(drag.poin);
    if (active_drag_node->node->wrap().is_layer()) {
      return true;
    }

    LayerGroup &group = active_drag_node->node->wrap().as_group();
    if (drop_tree_node_.is_child_of(group)) {
      /* Don't drop group node into its child node. */
      return false;
    }
    return true;
  }

  std::string drop_tooltip(const DragInfo &drag_info) const override
  {
    const wmDragGreasePencilLayer *drag_grease_pencil =
        static_cast<const wmDragGreasePencilLayer *>(drag_info.drag_data.poin);
    TreeNode &drag_node = drag_grease_pencil->node->wrap();

    const StringRef drag_name = drag_node.name();
    const StringRef drop_name = drop_tree_node_.name();
    const StringRef node_type = drag_node.is_layer() ? "layer" : "group";

    switch (drag_info.drop_location) {
      case DropLocation::Into:
        return fmt::format(
            fmt::runtime(TIP_("Move {} {} into {}")), node_type, drag_name, drop_name);
      case DropLocation::Before:
        return fmt::format(
            fmt::runtime(TIP_("Move {} {} above {}")), node_type, drag_name, drop_name);
      case DropLocation::After:
        return fmt::format(
            fmt::runtime(TIP_("Move {} {} below {}")), node_type, drag_name, drop_name);
      default:
        BLI_assert_unreachable();
        break;
    }

    return "";
  }

  bool on_drop(bContext *C, const DragInfo &drag_info) const override
  {
    const wmDragGreasePencilLayer *drag_grease_pencil =
        static_cast<const wmDragGreasePencilLayer *>(drag_info.drag_data.poin);
    GreasePencil &grease_pencil = *drag_grease_pencil->grease_pencil;
    TreeNode &drag_node = drag_grease_pencil->node->wrap();

    if (!drop_tree_node_.parent_group()) {
      /* Root node is not added to the tree view, so there should never be a drop target for this.
       */
      BLI_assert_unreachable();
      return false;
    }

    if (&drop_tree_node_ == &drag_node) {
      return false;
    }

    switch (drag_info.drop_location) {
      case DropLocation::Into: {
        BLI_assert_msg(drop_tree_node_.is_group(),
                       "Inserting should not be possible for layers, only for groups, because "
                       "only groups use DropBehavior::Reorder_and_Insert");
        LayerGroup &drop_group = drop_tree_node_.as_group();
        grease_pencil.move_node_into(drag_node, drop_group);
        break;
      }
      case DropLocation::Before: {
        /* Draw order is inverted, so inserting before (above) means inserting the node after. */
        grease_pencil.move_node_after(drag_node, drop_tree_node_);
        break;
      }
      case DropLocation::After: {
        /* Draw order is inverted, so inserting after (below) means inserting the node before. */
        grease_pencil.move_node_before(drag_node, drop_tree_node_);
        break;
      }
      default: {
        BLI_assert_unreachable();
        return false;
      }
    }

    if (drag_node.is_layer()) {
      WM_msg_publish_rna_prop(
          CTX_wm_message_bus(C), &grease_pencil.id, &grease_pencil, GreasePencilv3Layers, active);
      WM_msg_publish_rna_prop(
          CTX_wm_message_bus(C), &grease_pencil.id, &grease_pencil, GreasePencil, layers);
    }
    else if (drag_node.is_group()) {
      WM_msg_publish_rna_prop(CTX_wm_message_bus(C),
                              &grease_pencil.id,
                              &grease_pencil,
                              GreasePencilv3LayerGroup,
                              active);
      WM_msg_publish_rna_prop(
          CTX_wm_message_bus(C), &grease_pencil.id, &grease_pencil, GreasePencil, layer_groups);
    }

    ED_undo_push(C, "Reorder Layers");

    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
    return true;
  }
};

class LayerViewItemDragController : public AbstractViewItemDragController {
  GreasePencil &grease_pencil_;
  TreeNode &dragged_node_;

 public:
  LayerViewItemDragController(LayerTreeView &tree_view,
                              GreasePencil &grease_pencil,
                              GreasePencilLayerTreeNode &node)
      : AbstractViewItemDragController(tree_view),
        grease_pencil_(grease_pencil),
        dragged_node_(node.wrap())
  {
  }

  std::optional<eWM_DragDataType> get_drag_type() const override
  {
    if (dragged_node_.wrap().is_layer()) {
      return WM_DRAG_GREASE_PENCIL_LAYER;
    }
    return WM_DRAG_GREASE_PENCIL_GROUP;
  }

  void *create_drag_data() const override
  {
    wmDragGreasePencilLayer *drag_data = MEM_callocN<wmDragGreasePencilLayer>(__func__);
    drag_data->node = &dragged_node_;
    drag_data->grease_pencil = &grease_pencil_;
    return drag_data;
  }

  void on_drag_start(bContext & /*C*/) override
  {
    grease_pencil_.set_active_node(&dragged_node_);
  }
};

class LayerViewItem : public AbstractTreeViewItem {
 public:
  LayerViewItem(GreasePencil &grease_pencil, Layer &layer)
      : grease_pencil_(grease_pencil), layer_(layer)
  {
    this->label_ = layer.name();
  }

  void build_row(uiLayout &row) override
  {
    build_layer_name(row);

    uiLayout *sub = &row.row(true);
    sub->use_property_decorate_set(false);

    build_layer_buttons(*sub);
  }

  bool supports_collapsing() const override
  {
    /* This is a bit redundant since `LayerViewItem` can't have children.
     * But being explicit might catch errors. */
    return false;
  }

  std::optional<bool> should_be_active() const override
  {
    if (this->grease_pencil_.has_active_layer()) {
      return reinterpret_cast<GreasePencilLayer *>(&layer_) ==
             reinterpret_cast<GreasePencilLayer *>(this->grease_pencil_.get_active_layer());
    }
    return {};
  }

  void on_activate(bContext &C) override
  {
    PointerRNA layers_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilv3Layers, nullptr);
    PointerRNA value_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilLayer, &layer_);

    PropertyRNA *prop = RNA_struct_find_property(&layers_ptr, "active");

    if (grease_pencil_.has_active_group()) {
      WM_msg_publish_rna_prop(CTX_wm_message_bus(&C),
                              &grease_pencil_.id,
                              &grease_pencil_,
                              GreasePencilv3LayerGroup,
                              active);
    }

    RNA_property_pointer_set(&layers_ptr, prop, value_ptr, nullptr);
    RNA_property_update(&C, &layers_ptr, prop);

    ED_undo_push(&C, "Active Grease Pencil Layer");
  }

  bool supports_renaming() const override
  {
    return true;
  }

  bool rename(const bContext &C, StringRefNull new_name) override
  {
    PointerRNA layer_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilLayer, &layer_);
    PropertyRNA *prop = RNA_struct_find_property(&layer_ptr, "name");

    RNA_property_string_set(&layer_ptr, prop, new_name.c_str());
    RNA_property_update(&const_cast<bContext &>(C), &layer_ptr, prop);

    ED_undo_push(&const_cast<bContext &>(C), "Rename Grease Pencil Layer");
    return true;
  }

  StringRef get_rename_string() const override
  {
    return layer_.name();
  }

  void delete_item(bContext *C) override
  {
    grease_pencil_.remove_layer(layer_);
    DEG_id_tag_update(&grease_pencil_.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);
    ED_undo_push(C, "Delete Grease Pencil Layer");
  }

  std::unique_ptr<AbstractViewItemDragController> create_drag_controller() const override
  {
    return std::make_unique<LayerViewItemDragController>(
        static_cast<LayerTreeView &>(get_tree_view()), grease_pencil_, layer_.base);
  }

  std::unique_ptr<TreeViewItemDropTarget> create_drop_target() override
  {
    return std::make_unique<LayerNodeDropTarget>(*this, layer_.as_node(), DropBehavior::Reorder);
  }

 private:
  GreasePencil &grease_pencil_;
  Layer &layer_;

  void build_layer_name(uiLayout &row)
  {
    uiBut *but = uiItemL_ex(
        &row, layer_.name().c_str(), ICON_OUTLINER_DATA_GP_LAYER, false, false);

    if (ID_IS_LINKED(&grease_pencil_)) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
    else if (!layer_.is_editable()) {
      UI_but_disable(but, "Layer is locked or not visible");
    }
  }

  void build_layer_buttons(uiLayout &row)
  {
    uiLayout *sub;
    PointerRNA layer_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilLayer, &layer_);

    sub = &row.row(true);
    sub->active_set(layer_.parent_group().use_masks());
    sub->prop(&layer_ptr, "use_masks", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);

    sub = &row.row(true);
    sub->active_set(layer_.parent_group().use_onion_skinning());
    sub->prop(&layer_ptr, "use_onion_skinning", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);

    sub = &row.row(true);
    sub->active_set(layer_.parent_group().is_visible());
    sub->prop(&layer_ptr, "hide", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);

    sub = &row.row(true);
    sub->active_set(!layer_.parent_group().is_locked());
    sub->prop(&layer_ptr, "lock", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);
  }
};

class LayerGroupViewItem : public AbstractTreeViewItem {
 public:
  LayerGroupViewItem(GreasePencil &grease_pencil, LayerGroup &group)
      : grease_pencil_(grease_pencil), group_(group)
  {
    this->label_ = group_.name();
  }

  std::optional<bool> should_be_collapsed() const override
  {
    const bool is_collapsed = !group_.is_expanded();
    return is_collapsed;
  }

  bool set_collapsed(const bool collapsed) override
  {
    if (!AbstractTreeViewItem::set_collapsed(collapsed)) {
      return false;
    }
    group_.set_expanded(!collapsed);
    return true;
  }

  void on_collapse_change(bContext &C, const bool is_collapsed) override
  {
    const bool is_expanded = !is_collapsed;

    /* Let RNA handle the property change. This makes sure all the notifiers and DEG
     * update calls are properly called. */
    PointerRNA group_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilLayerGroup, &group_);
    PropertyRNA *prop = RNA_struct_find_property(&group_ptr, "is_expanded");

    RNA_property_boolean_set(&group_ptr, prop, is_expanded);
    RNA_property_update(&C, &group_ptr, prop);
  }

  void build_row(uiLayout &row) override
  {
    build_layer_group_name(row);

    uiLayout *sub = &row.row(true);
    sub->use_property_decorate_set(false);

    build_layer_group_buttons(*sub);
  }

  std::optional<bool> should_be_active() const override
  {
    if (this->grease_pencil_.has_active_group()) {
      return &group_ == this->grease_pencil_.get_active_group();
    }
    return {};
  }

  void build_context_menu(bContext &C, uiLayout &layout) const override
  {
    MenuType *mt = WM_menutype_find("GREASE_PENCIL_MT_group_context_menu", true);
    if (!mt) {
      return;
    }
    UI_menutype_draw(&C, mt, &layout);
  }

  void on_activate(bContext &C) override
  {
    PointerRNA grease_pencil_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilv3LayerGroup, nullptr);
    PointerRNA value_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilLayerGroup, &group_);

    PropertyRNA *prop = RNA_struct_find_property(&grease_pencil_ptr, "active");

    if (grease_pencil_.has_active_layer()) {
      WM_msg_publish_rna_prop(CTX_wm_message_bus(&C),
                              &grease_pencil_.id,
                              &grease_pencil_,
                              GreasePencilv3Layers,
                              active);
    }

    RNA_property_pointer_set(&grease_pencil_ptr, prop, value_ptr, nullptr);
    RNA_property_update(&C, &grease_pencil_ptr, prop);

    ED_undo_push(&C, "Active Grease Pencil Group");
  }

  bool supports_renaming() const override
  {
    return true;
  }

  bool rename(const bContext &C, StringRefNull new_name) override
  {
    PointerRNA group_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilLayerGroup, &group_);
    PropertyRNA *prop = RNA_struct_find_property(&group_ptr, "name");

    RNA_property_string_set(&group_ptr, prop, new_name.c_str());
    RNA_property_update(&const_cast<bContext &>(C), &group_ptr, prop);

    ED_undo_push(&const_cast<bContext &>(C), "Rename Grease Pencil Layer Group");
    return true;
  }

  StringRef get_rename_string() const override
  {
    return group_.name();
  }

  void delete_item(bContext *C) override
  {
    grease_pencil_.remove_group(group_);
    DEG_id_tag_update(&grease_pencil_.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);
    ED_undo_push(C, "Delete Grease Pencil Group");
  }

  std::unique_ptr<AbstractViewItemDragController> create_drag_controller() const override
  {
    return std::make_unique<LayerViewItemDragController>(
        static_cast<LayerTreeView &>(get_tree_view()), grease_pencil_, group_.base);
  }

  std::unique_ptr<TreeViewItemDropTarget> create_drop_target() override
  {
    return std::make_unique<LayerNodeDropTarget>(
        *this, group_.as_node(), DropBehavior::ReorderAndInsert);
  }

 private:
  GreasePencil &grease_pencil_;
  LayerGroup &group_;

  void build_layer_group_name(uiLayout &row)
  {
    short icon = ICON_GREASEPENCIL_LAYER_GROUP;
    if (group_.color_tag != LAYERGROUP_COLOR_NONE) {
      icon = ICON_LAYERGROUP_COLOR_01 + group_.color_tag;
    }

    uiBut *but = uiItemL_ex(&row, group_.name(), icon, false, false);
    if (ID_IS_LINKED(&grease_pencil_)) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
    else if (!group_.is_editable()) {
      UI_but_disable(but, "Layer Group is locked or not visible");
    }
  }

  void build_layer_group_buttons(uiLayout &row)
  {
    uiLayout *sub;
    PointerRNA group_ptr = RNA_pointer_create_discrete(
        &grease_pencil_.id, &RNA_GreasePencilLayerGroup, &group_);

    sub = &row.row(true);
    if (group_.as_node().parent_group()) {
      sub->active_set(group_.as_node().parent_group()->use_masks());
    }
    sub->prop(&group_ptr, "use_masks", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);

    sub = &row.row(true);
    if (group_.as_node().parent_group()) {
      sub->active_set(group_.as_node().parent_group()->use_onion_skinning());
    }
    sub->prop(&group_ptr, "use_onion_skinning", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);

    sub = &row.row(true);
    if (group_.as_node().parent_group()) {
      sub->active_set(group_.as_node().parent_group()->is_visible());
    }
    sub->prop(&group_ptr, "hide", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);

    sub = &row.row(true);
    if (group_.as_node().parent_group()) {
      sub->active_set(!group_.as_node().parent_group()->is_locked());
    }
    sub->prop(&group_ptr, "lock", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);
  }
};

void LayerTreeView::build_tree_node_recursive(TreeViewOrItem &parent, TreeNode &node)
{
  using namespace blender::bke::greasepencil;
  if (node.is_layer()) {
    parent.add_tree_item<LayerViewItem>(this->grease_pencil_, node.as_layer());
  }
  else if (node.is_group()) {
    LayerGroupViewItem &group_item = parent.add_tree_item<LayerGroupViewItem>(this->grease_pencil_,
                                                                              node.as_group());
    LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, node_, &node.as_group().children) {
      build_tree_node_recursive(group_item, node_->wrap());
    }
  }
}

void LayerTreeView::build_tree()
{
  using namespace blender::bke::greasepencil;
  LISTBASE_FOREACH_BACKWARD (
      GreasePencilLayerTreeNode *, node, &this->grease_pencil_.root_group_ptr->children)
  {
    this->build_tree_node_recursive(*this, node->wrap());
  }
}

}  // namespace blender::ui::greasepencil

void uiTemplateGreasePencilLayerTree(uiLayout *layout, bContext *C)
{
  using namespace blender;

  GreasePencil *grease_pencil = blender::ed::greasepencil::from_context(*C);

  if (grease_pencil == nullptr) {
    return;
  }

  uiBlock *block = layout->block();

  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "Grease Pencil Layer Tree View",
      std::make_unique<blender::ui::greasepencil::LayerTreeView>(*grease_pencil));
  tree_view->set_context_menu_title("Grease Pencil Layer");
  tree_view->set_default_rows(6);

  ui::TreeViewBuilder::build_tree_view(*C, *tree_view, *layout);
}
