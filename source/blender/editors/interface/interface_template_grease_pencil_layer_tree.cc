/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "BLT_translation.h"

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
  LayerNodeDropTarget(AbstractTreeView &view, TreeNode &drop_tree_node, DropBehavior behavior)
      : TreeViewItemDropTarget(view, behavior), drop_tree_node_(drop_tree_node)
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

    std::string_view drag_name = drag_layer.name();
    std::string_view drop_name = drop_tree_node_.name();

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
    Layer &drag_layer = drag_grease_pencil->layer->wrap();

    LayerGroup &drag_parent = drag_layer.parent_group();
    LayerGroup *drop_parent_group = drop_tree_node_.parent_group();
    if (!drop_parent_group) {
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
        drag_parent.unlink_node(&drag_layer.as_node());
        drop_group.add_layer(&drag_layer);
        break;
      }
      case DropLocation::Before: {
        drag_parent.unlink_node(&drag_layer.as_node());
        /* Draw order is inverted, so inserting before means inserting below. */
        drop_parent_group->add_layer_after(&drag_layer, &drop_tree_node_);
        break;
      }
      case DropLocation::After: {
        drag_parent.unlink_node(&drag_layer.as_node());
        /* Draw order is inverted, so inserting after means inserting above. */
        drop_parent_group->add_layer_before(&drag_layer, &drop_tree_node_);
        break;
      }
      default: {
        BLI_assert_unreachable();
        return false;
      }
    }
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
    return false;
  }

  std::optional<bool> should_be_active() const override
  {
    if (this->grease_pencil_.has_active_layer()) {
      return reinterpret_cast<GreasePencilLayer *>(&layer_) == this->grease_pencil_.active_layer;
    }
    return {};
  }

  void on_activate(bContext &C) override
  {
    PointerRNA grease_pencil_ptr = RNA_pointer_create(
        &grease_pencil_.id, &RNA_GreasePencilv3Layers, nullptr);
    PointerRNA value_ptr = RNA_pointer_create(&grease_pencil_.id, &RNA_GreasePencilLayer, &layer_);

    PropertyRNA *prop = RNA_struct_find_property(&grease_pencil_ptr, "active");

    RNA_property_pointer_set(&grease_pencil_ptr, prop, value_ptr, nullptr);
    RNA_property_update(&C, &grease_pencil_ptr, prop);

    ED_undo_push(&C, "Active Grease Pencil Layer");
  }

  bool supports_renaming() const override
  {
    return true;
  }

  bool rename(const bContext & /*C*/, StringRefNull new_name) override
  {
    grease_pencil_.rename_node(layer_.as_node(), new_name);
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
    return std::make_unique<LayerNodeDropTarget>(
        get_tree_view(), layer_.as_node(), DropBehavior::Reorder);
  }

 private:
  GreasePencil &grease_pencil_;
  Layer &layer_;

  void build_layer_name(uiLayout &row)
  {
    uiBut *but = uiItemL_ex(
        &row, IFACE_(layer_.name().c_str()), ICON_OUTLINER_DATA_GP_LAYER, false, false);
    if (layer_.is_locked() || !layer_.parent_group().is_visible()) {
      UI_but_disable(but, "Layer is locked or not visible");
    }
  }

  void build_layer_buttons(uiLayout &row)
  {
    uiBut *but;
    PointerRNA layer_ptr = RNA_pointer_create(&grease_pencil_.id, &RNA_GreasePencilLayer, &layer_);

    uiBlock *block = uiLayoutGetBlock(&row);
    but = uiDefIconButR(block,
                        UI_BTYPE_ICON_TOGGLE,
                        0,
                        ICON_NONE,
                        0,
                        0,
                        UI_UNIT_X,
                        UI_UNIT_Y,
                        &layer_ptr,
                        "hide",
                        0,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        nullptr);
    if (!layer_.parent_group().is_visible()) {
      UI_but_flag_enable(but, UI_BUT_INACTIVE);
    }

    but = uiDefIconButR(block,
                        UI_BTYPE_ICON_TOGGLE,
                        0,
                        ICON_NONE,
                        0,
                        0,
                        UI_UNIT_X,
                        UI_UNIT_Y,
                        &layer_ptr,
                        "lock",
                        0,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        nullptr);
    if (layer_.parent_group().is_locked()) {
      UI_but_flag_enable(but, UI_BUT_INACTIVE);
    }
  }
};

class LayerGroupViewItem : public AbstractTreeViewItem {
 public:
  LayerGroupViewItem(GreasePencil &grease_pencil, LayerGroup &group)
      : grease_pencil_(grease_pencil), group_(group)
  {
    this->disable_activatable();
    this->label_ = group_.name();
  }

  void build_row(uiLayout &row) override
  {
    build_layer_group_name(row);

    uiLayout *sub = uiLayoutRow(&row, true);
    uiLayoutSetPropDecorate(sub, false);

    build_layer_group_buttons(*sub);
  }

  bool supports_renaming() const override
  {
    return true;
  }

  bool rename(const bContext & /*C*/, StringRefNull new_name) override
  {
    grease_pencil_.rename_node(group_.as_node(), new_name);
    return true;
  }

  StringRef get_rename_string() const override
  {
    return group_.name();
  }

  std::unique_ptr<TreeViewItemDropTarget> create_drop_target() override
  {
    return std::make_unique<LayerNodeDropTarget>(
        get_tree_view(), group_.as_node(), DropBehavior::ReorderAndInsert);
  }

 private:
  GreasePencil &grease_pencil_;
  LayerGroup &group_;

  void build_layer_group_name(uiLayout &row)
  {
    uiItemS_ex(&row, 0.8f);
    uiBut *but = uiItemL_ex(&row, IFACE_(group_.name().c_str()), ICON_FILE_FOLDER, false, false);
    if (group_.is_locked()) {
      UI_but_disable(but, "Layer Group is locked");
    }
  }

  void build_layer_group_buttons(uiLayout &row)
  {
    PointerRNA group_ptr = RNA_pointer_create(
        &grease_pencil_.id, &RNA_GreasePencilLayerGroup, &group_);

    uiItemR(&row, &group_ptr, "hide", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);
    uiItemR(&row, &group_ptr, "lock", UI_ITEM_R_ICON_ONLY, nullptr, ICON_NONE);
  }
};

void LayerTreeView::build_tree_node_recursive(TreeViewOrItem &parent, TreeNode &node)
{
  using namespace blender::bke::greasepencil;
  if (node.is_layer()) {
    LayerViewItem &item = parent.add_tree_item<LayerViewItem>(this->grease_pencil_,
                                                              node.as_layer());
    item.set_collapsed(false);
  }
  else if (node.is_group()) {
    LayerGroupViewItem &group_item = parent.add_tree_item<LayerGroupViewItem>(this->grease_pencil_,
                                                                              node.as_group());
    group_item.set_collapsed(false);
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
