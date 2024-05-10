/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"

#include "UI_interface.hh"
#include "UI_tree_view.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "ED_undo.hh"

#include "WM_api.hh"

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
    return drag.type == WM_DRAG_GREASE_PENCIL_LAYER;
  }

  std::string drop_tooltip(const DragInfo &drag_info) const override
  {
    const wmDragGreasePencilLayer *drag_grease_pencil =
        static_cast<const wmDragGreasePencilLayer *>(drag_info.drag_data.poin);
    Layer &drag_layer = drag_grease_pencil->layer->wrap();

    const StringRef drag_name = drag_layer.name();
    const StringRef drop_name = drop_tree_node_.name();

    switch (drag_info.drop_location) {
      case DropLocation::Into:
        return fmt::format(TIP_("Move layer {} into {}"), drag_name, drop_name);
      case DropLocation::Before:
        return fmt::format(TIP_("Move layer {} above {}"), drag_name, drop_name);
      case DropLocation::After:
        return fmt::format(TIP_("Move layer {} below {}"), drag_name, drop_name);
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
    Layer &drag_layer = drag_grease_pencil->layer->wrap();

    if (!drop_tree_node_.parent_group()) {
      /* Root node is not added to the tree view, so there should never be a drop target for this.
       */
      BLI_assert_unreachable();
      return false;
    }

    if (&drop_tree_node_ == &drag_layer.as_node()) {
      return false;
    }

    switch (drag_info.drop_location) {
      case DropLocation::Into: {
        BLI_assert_msg(drop_tree_node_.is_group(),
                       "Inserting should not be possible for layers, only for groups, because "
                       "only groups use DropBehavior::Reorder_and_Insert");
        LayerGroup &drop_group = drop_tree_node_.as_group();
        grease_pencil.move_node_into(drag_layer.as_node(), drop_group);
        break;
      }
      case DropLocation::Before: {
        /* Draw order is inverted, so inserting before (above) means inserting the node after. */
        grease_pencil.move_node_after(drag_layer.as_node(), drop_tree_node_);
        break;
      }
      case DropLocation::After: {
        /* Draw order is inverted, so inserting after (below) means inserting the node before. */
        grease_pencil.move_node_before(drag_layer.as_node(), drop_tree_node_);
        break;
      }
      default: {
        BLI_assert_unreachable();
        return false;
      }
    }

    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
    return true;
  }
};

class LayerViewItemDragController : public AbstractViewItemDragController {
  GreasePencil &grease_pencil_;
  Layer &dragged_layer_;

 public:
  LayerViewItemDragController(LayerTreeView &tree_view, GreasePencil &grease_pencil, Layer &layer)
      : AbstractViewItemDragController(tree_view),
        grease_pencil_(grease_pencil),
        dragged_layer_(layer)
  {
  }

  eWM_DragDataType get_drag_type() const override
  {
    return WM_DRAG_GREASE_PENCIL_LAYER;
  }

  void *create_drag_data() const override
  {
    wmDragGreasePencilLayer *drag_data = MEM_new<wmDragGreasePencilLayer>(__func__);
    drag_data->layer = &dragged_layer_;
    drag_data->grease_pencil = &grease_pencil_;
    return drag_data;
  }

  void on_drag_start() override
  {
    grease_pencil_.set_active_layer(&dragged_layer_);
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

    uiLayout *sub = uiLayoutRow(&row, true);
    uiLayoutSetPropDecorate(sub, false);

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
    PointerRNA grease_pencil_ptr = RNA_pointer_create(
        &grease_pencil_.id, &RNA_GreasePencilv3Layers, nullptr);
    PointerRNA value_ptr = RNA_pointer_create(&grease_pencil_.id, &RNA_GreasePencilLayer, &layer_);

    PropertyRNA *prop = RNA_struct_find_property(&grease_pencil_ptr, "active_layer");

    RNA_property_pointer_set(&grease_pencil_ptr, prop, value_ptr, nullptr);
    RNA_property_update(&C, &grease_pencil_ptr, prop);

    ED_undo_push(&C, "Active Grease Pencil Layer");
  }

  bool supports_renaming() const override
  {
    return true;
  }

  bool rename(const bContext &C, StringRefNull new_name) override
  {
    PointerRNA layer_ptr = RNA_pointer_create(&grease_pencil_.id, &RNA_GreasePencilLayer, &layer_);
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

  std::unique_ptr<AbstractViewItemDragController> create_drag_controller() const override
  {
    return std::make_unique<LayerViewItemDragController>(
        static_cast<LayerTreeView &>(get_tree_view()), grease_pencil_, layer_);
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
    if (!layer_.is_editable()) {
      UI_but_disable(but, "Layer is locked or not visible");
    }
  }

  void build_layer_buttons(uiLayout &row)
  {
    uiLayout *sub;
    PointerRNA layer_ptr = RNA_pointer_create(&grease_pencil_.id, &RNA_GreasePencilLayer, &layer_);

    sub = uiLayoutRow(&row, true);
    uiLayoutSetActive(sub, layer_.parent_group().use_masks());
    uiItemR(sub, &layer_ptr, "use_masks", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);

    sub = uiLayoutRow(&row, true);
    uiLayoutSetActive(sub, layer_.parent_group().use_onion_skinning());
    uiItemR(sub, &layer_ptr, "use_onion_skinning", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);

    sub = uiLayoutRow(&row, true);
    uiLayoutSetActive(sub, layer_.parent_group().is_visible());
    uiItemR(sub, &layer_ptr, "hide", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);

    sub = uiLayoutRow(&row, true);
    uiLayoutSetActive(sub, !layer_.parent_group().is_locked());
    uiItemR(sub, &layer_ptr, "lock", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);
  }
};

class LayerGroupViewItem : public AbstractTreeViewItem {
 public:
  LayerGroupViewItem(GreasePencil &grease_pencil, LayerGroup &group)
      : grease_pencil_(grease_pencil), group_(group)
  {
    this->label_ = group_.name();
  }

  void build_row(uiLayout &row) override
  {
    build_layer_group_name(row);

    uiLayout *sub = uiLayoutRow(&row, true);
    uiLayoutSetPropDecorate(sub, false);

    build_layer_group_buttons(*sub);
  }

  std::optional<bool> should_be_active() const override
  {
    if (this->grease_pencil_.has_active_group()) {
      return &group_ == this->grease_pencil_.get_active_group();
    }
    return {};
  }

  void on_activate(bContext &C) override
  {
    PointerRNA grease_pencil_ptr = RNA_pointer_create(
        &grease_pencil_.id, &RNA_GreasePencilv3LayerGroup, nullptr);
    PointerRNA value_ptr = RNA_pointer_create(
        &grease_pencil_.id, &RNA_GreasePencilLayerGroup, &group_);

    PropertyRNA *prop = RNA_struct_find_property(&grease_pencil_ptr, "active_group");

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
    PointerRNA group_ptr = RNA_pointer_create(
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
    uiItemS_ex(&row, 0.8f);
    uiBut *but = uiItemL_ex(&row, group_.name().c_str(), ICON_FILE_FOLDER, false, false);
    if (!group_.is_editable()) {
      UI_but_disable(but, "Layer Group is locked or not visible");
    }
  }

  void build_layer_group_buttons(uiLayout &row)
  {
    uiLayout *sub;
    PointerRNA group_ptr = RNA_pointer_create(
        &grease_pencil_.id, &RNA_GreasePencilLayerGroup, &group_);

    sub = uiLayoutRow(&row, true);
    if (group_.as_node().parent_group()) {
      uiLayoutSetActive(sub, group_.as_node().parent_group()->use_masks());
    }
    const int icon_mask = (group_.base.flag & GP_LAYER_TREE_NODE_HIDE_MASKS) == 0 ?
                              ICON_CLIPUV_DEHLT :
                              ICON_CLIPUV_HLT;
    uiItemR(sub, &group_ptr, "use_masks", UI_ITEM_R_ICON_ONLY, nullptr, icon_mask);

    sub = uiLayoutRow(&row, true);
    if (group_.as_node().parent_group()) {
      uiLayoutSetActive(sub, group_.as_node().parent_group()->use_onion_skinning());
    }
    uiItemR(sub, &group_ptr, "use_onion_skinning", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);

    sub = uiLayoutRow(&row, true);
    if (group_.as_node().parent_group()) {
      uiLayoutSetActive(sub, group_.as_node().parent_group()->is_visible());
    }
    uiItemR(sub, &group_ptr, "hide", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);

    sub = uiLayoutRow(&row, true);
    if (group_.as_node().parent_group()) {
      uiLayoutSetActive(sub, !group_.as_node().parent_group()->is_locked());
    }
    uiItemR(sub, &group_ptr, "lock", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);
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
    group_item.uncollapse_by_default();
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

  Object *object = CTX_data_active_object(C);
  if (!object || object->type != OB_GREASE_PENCIL) {
    return;
  }
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  uiBlock *block = uiLayoutGetBlock(layout);

  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "Grease Pencil Layer Tree View",
      std::make_unique<blender::ui::greasepencil::LayerTreeView>(grease_pencil));
  tree_view->set_min_rows(3);

  ui::TreeViewBuilder::build_tree_view(*tree_view, *layout);
}
