/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_tree_view.hh"

namespace blender::ui::greasepencil {

using namespace blender::bke::greasepencil;

class LayerViewItem : public AbstractTreeViewItem {
 public:
  LayerViewItem(GreasePencil &grease_pencil, Layer &layer)
      : grease_pencil_(grease_pencil), layer_(layer)
  {
    this->label_ = layer.name();
  }

  void build_row(uiLayout &row) override
  {
    uiLayout *sub = uiLayoutRow(&row, true);
    uiItemL(sub, IFACE_(layer_.name().c_str()), ICON_GREASEPENCIL);
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

  void on_activate() override
  {
    this->grease_pencil_.set_active_layer(&layer_);
  }

  bool supports_renaming() const override
  {
    return true;
  }

  bool rename(StringRefNull new_name) override
  {
    grease_pencil_.rename_layer(layer_, new_name);
    return true;
  }

  StringRef get_rename_string() const override
  {
    return layer_.name();
  }

 private:
  GreasePencil &grease_pencil_;
  Layer &layer_;
};

class LayerGroupViewItem : public AbstractTreeViewItem {
 public:
  LayerGroupViewItem(/*GreasePencil &grease_pencil, */ LayerGroup &group)
      : /*grease_pencil_(grease_pencil),*/ group_(group)
  {
    this->label_ = group_.name();
  }

  void build_row(uiLayout &row) override
  {
    uiLayout *sub = uiLayoutRow(&row, true);
    uiItemL(sub, IFACE_(group_.name().c_str()), ICON_FILE_FOLDER);
  }

 private:
  /* GreasePencil &grease_pencil_; */
  LayerGroup &group_;
};

class LayerTreeView : public AbstractTreeView {
 public:
  explicit LayerTreeView(GreasePencil &grease_pencil) : grease_pencil_(grease_pencil) {}

  void build_tree() override;

 private:
  void build_tree_node_recursive(TreeNode &node);
  GreasePencil &grease_pencil_;
};

void LayerTreeView::build_tree_node_recursive(TreeNode &node)
{
  using namespace blender::bke::greasepencil;
  if (node.is_layer()) {
    add_tree_item<LayerViewItem>(this->grease_pencil_, node.as_layer_for_write());
  }
  else if (node.is_group()) {
    add_tree_item<LayerGroupViewItem>(/*this->grease_pencil_, */ node.as_group_for_write());
    LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, node_, &node.as_group().children) {
      build_tree_node_recursive(node_->wrap());
    }
  }
}

void LayerTreeView::build_tree()
{
  using namespace blender::bke::greasepencil;
  LISTBASE_FOREACH_BACKWARD (
      GreasePencilLayerTreeNode *, node, &this->grease_pencil_.root_group.children)
  {
    this->build_tree_node_recursive(node->wrap());
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
