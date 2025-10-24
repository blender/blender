/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"
#include "BKE_library.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_tree_interface.hh"

#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_node_tree_interface_types.h"

#include "ED_node.hh"
#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_tree_view.hh"

#include "WM_api.hh"

namespace node_interface = blender::bke::node_interface;

namespace blender::ui::nodes {

namespace {

using node_interface::bNodeTreeInterfaceItemReference;

class NodePanelViewItem;
class NodeSocketViewItem;
class NodeTreeInterfaceView;

class NodeTreeInterfaceDragController : public AbstractViewItemDragController {
 private:
  bNodeTreeInterfaceItem &item_;
  bNodeTree &tree_;

 public:
  explicit NodeTreeInterfaceDragController(NodeTreeInterfaceView &view,
                                           bNodeTreeInterfaceItem &item,
                                           bNodeTree &tree);
  ~NodeTreeInterfaceDragController() override = default;

  std::optional<eWM_DragDataType> get_drag_type() const override;

  void *create_drag_data() const override;
};

class NodeSocketDropTarget : public TreeViewItemDropTarget {
 private:
  bNodeTreeInterfaceSocket &socket_;

 public:
  explicit NodeSocketDropTarget(NodeSocketViewItem &item, bNodeTreeInterfaceSocket &socket);

  bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override;
  std::string drop_tooltip(const DragInfo &drag_info) const override;
  bool on_drop(bContext * /*C*/, const DragInfo &drag_info) const override;
};

class NodePanelDropTarget : public TreeViewItemDropTarget {
 private:
  bNodeTreeInterfacePanel &panel_;

 public:
  explicit NodePanelDropTarget(NodePanelViewItem &item, bNodeTreeInterfacePanel &panel);

  bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override;
  std::string drop_tooltip(const DragInfo &drag_info) const override;
  bool on_drop(bContext *C, const DragInfo &drag_info) const override;
};

class NodeSocketViewItem : public BasicTreeViewItem {
 private:
  bNodeTree &nodetree_;
  bNodeTreeInterfaceSocket &socket_;

 public:
  NodeSocketViewItem(bNodeTree &nodetree,
                     bNodeTreeInterface &interface,
                     bNodeTreeInterfaceSocket &socket)
      : BasicTreeViewItem(socket.name, ICON_NONE), nodetree_(nodetree), socket_(socket)
  {
    set_is_active_fn([interface, &socket]() { return interface.active_item() == &socket.item; });
    set_on_activate_fn([&interface](bContext & /*C*/, BasicTreeViewItem &new_active) {
      NodeSocketViewItem &self = static_cast<NodeSocketViewItem &>(new_active);
      interface.active_item_set(&self.socket_.item);
    });
  }

  void build_row(uiLayout &row) override
  {
    if (ID_IS_LINKED(&nodetree_)) {
      row.enabled_set(false);
    }

    row.use_property_decorate_set(false);

    uiLayout *input_socket_layout = &row.row(true);
    if (socket_.flag & NODE_INTERFACE_SOCKET_INPUT) {
      /* Context is not used by the template function. */
      uiTemplateNodeSocket(input_socket_layout, /*C*/ nullptr, socket_.socket_color());
    }
    else {
      /* Blank item to align output socket labels with inputs. */
      input_socket_layout->label("", ICON_BLANK1);
    }

    this->add_label(row, IFACE_(label_.c_str()));

    uiLayout *output_socket_layout = &row.row(true);
    if (socket_.flag & NODE_INTERFACE_SOCKET_OUTPUT) {
      /* Context is not used by the template function. */
      uiTemplateNodeSocket(output_socket_layout, /*C*/ nullptr, socket_.socket_color());
    }
    else {
      /* Blank item to align input socket labels with outputs. */
      output_socket_layout->label("", ICON_BLANK1);
    }
  }

 protected:
  bool matches(const AbstractViewItem &other) const override
  {
    const NodeSocketViewItem *other_item = dynamic_cast<const NodeSocketViewItem *>(&other);
    if (other_item == nullptr) {
      return false;
    }

    return &socket_ == &other_item->socket_;
  }

  bool supports_renaming() const override
  {
    return !ID_IS_LINKED(&nodetree_);
  }
  bool rename(const bContext &C, StringRefNull new_name) override
  {
    MEM_SAFE_FREE(socket_.name);

    socket_.name = BLI_strdup(new_name.c_str());
    nodetree_.tree_interface.tag_item_property_changed();
    BKE_main_ensure_invariants(*CTX_data_main(&C), nodetree_.id);
    ED_undo_push(&const_cast<bContext &>(C), new_name.c_str());
    return true;
  }
  StringRef get_rename_string() const override
  {
    return socket_.name;
  }

  void delete_item(bContext *C) override
  {
    Main *bmain = CTX_data_main(C);
    nodetree_.tree_interface.remove_item(socket_.item);
    BKE_main_ensure_invariants(*bmain, nodetree_.id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, &nodetree_);
    ED_undo_push(C, "Delete Node Interface Socket");
  }

  std::unique_ptr<AbstractViewItemDragController> create_drag_controller() const override;
  std::unique_ptr<TreeViewItemDropTarget> create_drop_target() override;
};

class NodePanelViewItem : public BasicTreeViewItem {
 private:
  bNodeTree &nodetree_;
  bNodeTreeInterfacePanel &panel_;
  const bNodeTreeInterfaceSocket *toggle_ = nullptr;

 public:
  NodePanelViewItem(bNodeTree &nodetree,
                    bNodeTreeInterface &interface,
                    bNodeTreeInterfacePanel &panel)
      : BasicTreeViewItem(panel.name, ICON_NONE), nodetree_(nodetree), panel_(panel)
  {
    set_is_active_fn([interface, &panel]() { return interface.active_item() == &panel.item; });
    set_on_activate_fn([&interface](bContext & /*C*/, BasicTreeViewItem &new_active) {
      NodePanelViewItem &self = static_cast<NodePanelViewItem &>(new_active);
      interface.active_item_set(&self.panel_.item);
    });
    toggle_ = panel.header_toggle_socket();
    is_always_collapsible_ = true;
  }

  void build_row(uiLayout &row) override
  {
    if (ID_IS_LINKED(&nodetree_)) {
      row.enabled_set(false);
    }
    /* Add boolean socket if panel has a toggle. */
    if (toggle_ != nullptr) {
      uiLayout *toggle_layout = &row.row(true);
      /* Context is not used by the template function. */
      uiTemplateNodeSocket(toggle_layout, /*C*/ nullptr, toggle_->socket_color());
    }

    this->add_label(row, IFACE_(label_.c_str()));

    uiLayout *sub = &row.row(true);
    sub->use_property_decorate_set(false);
  }

 protected:
  bool matches(const AbstractViewItem &other) const override
  {
    const NodePanelViewItem *other_item = dynamic_cast<const NodePanelViewItem *>(&other);
    if (other_item == nullptr) {
      return false;
    }

    return &panel_ == &other_item->panel_;
  }

  std::optional<bool> should_be_collapsed() const override
  {
    return panel_.flag & NODE_INTERFACE_PANEL_IS_COLLAPSED;
  }

  bool set_collapsed(const bool collapsed) override
  {
    if (!AbstractTreeViewItem::set_collapsed(collapsed)) {
      return false;
    }
    SET_FLAG_FROM_TEST(panel_.flag, collapsed, NODE_INTERFACE_PANEL_IS_COLLAPSED);
    return true;
  }

  bool supports_renaming() const override
  {
    return !ID_IS_LINKED(&nodetree_);
  }
  bool rename(const bContext &C, StringRefNull new_name) override
  {
    PointerRNA panel_ptr = RNA_pointer_create_discrete(
        &nodetree_.id, &RNA_NodeTreeInterfacePanel, &panel_);
    PropertyRNA *name_prop = RNA_struct_find_property(&panel_ptr, "name");
    RNA_property_string_set(&panel_ptr, name_prop, new_name.c_str());
    RNA_property_update(const_cast<bContext *>(&C), &panel_ptr, name_prop);
    return true;
  }
  StringRef get_rename_string() const override
  {
    return panel_.name;
  }

  void delete_item(bContext *C) override
  {
    Main *bmain = CTX_data_main(C);
    nodetree_.tree_interface.remove_item(panel_.item);
    BKE_main_ensure_invariants(*bmain, nodetree_.id);
    WM_main_add_notifier(NC_NODE | NA_EDITED, &nodetree_);
    ED_undo_push(C, "Delete Node Interface Panel");
  }

  std::unique_ptr<AbstractViewItemDragController> create_drag_controller() const override;
  std::unique_ptr<TreeViewItemDropTarget> create_drop_target() override;
};

class NodeTreeInterfaceView : public AbstractTreeView {
 private:
  bNodeTree &nodetree_;
  bNodeTreeInterface &interface_;

 public:
  explicit NodeTreeInterfaceView(bNodeTree &nodetree, bNodeTreeInterface &interface)
      : nodetree_(nodetree), interface_(interface)
  {
  }

  bNodeTree &nodetree()
  {
    return nodetree_;
  }

  bNodeTreeInterface &interface()
  {
    return interface_;
  }

  void build_tree() override
  {
    /* Draw root items */
    this->add_items_for_panel_recursive(interface_.root_panel, *this);
  }

 protected:
  void add_items_for_panel_recursive(bNodeTreeInterfacePanel &parent,
                                     ui::TreeViewOrItem &parent_item,
                                     const bNodeTreeInterfaceItem *skip_item = nullptr)
  {
    for (bNodeTreeInterfaceItem *item : parent.items()) {
      if (item == skip_item) {
        continue;
      }
      switch (NodeTreeInterfaceItemType(item->item_type)) {
        case NODE_INTERFACE_SOCKET: {
          bNodeTreeInterfaceSocket *socket = node_interface::get_item_as<bNodeTreeInterfaceSocket>(
              item);
          NodeSocketViewItem &socket_item = parent_item.add_tree_item<NodeSocketViewItem>(
              nodetree_, interface_, *socket);
          socket_item.uncollapse_by_default();
          break;
        }
        case NODE_INTERFACE_PANEL: {
          bNodeTreeInterfacePanel *panel = node_interface::get_item_as<bNodeTreeInterfacePanel>(
              item);
          NodePanelViewItem &panel_item = parent_item.add_tree_item<NodePanelViewItem>(
              nodetree_, interface_, *panel);
          panel_item.uncollapse_by_default();
          /* Skip over sockets which are a panel toggle. */
          const bNodeTreeInterfaceSocket *skip_item = panel->header_toggle_socket();
          add_items_for_panel_recursive(
              *panel, panel_item, reinterpret_cast<const bNodeTreeInterfaceItem *>(skip_item));
          break;
        }
      }
    }
  }
};

std::unique_ptr<AbstractViewItemDragController> NodeSocketViewItem::create_drag_controller() const
{
  if (!ID_IS_EDITABLE(&nodetree_.id)) {
    return nullptr;
  }
  return std::make_unique<NodeTreeInterfaceDragController>(
      static_cast<NodeTreeInterfaceView &>(this->get_tree_view()), socket_.item, nodetree_);
}

std::unique_ptr<TreeViewItemDropTarget> NodeSocketViewItem::create_drop_target()
{
  return std::make_unique<NodeSocketDropTarget>(*this, socket_);
}

std::unique_ptr<AbstractViewItemDragController> NodePanelViewItem::create_drag_controller() const
{
  if (!ID_IS_EDITABLE(&nodetree_.id)) {
    return nullptr;
  }
  return std::make_unique<NodeTreeInterfaceDragController>(
      static_cast<NodeTreeInterfaceView &>(this->get_tree_view()), panel_.item, nodetree_);
}

std::unique_ptr<TreeViewItemDropTarget> NodePanelViewItem::create_drop_target()
{
  return std::make_unique<NodePanelDropTarget>(*this, panel_);
}

NodeTreeInterfaceDragController::NodeTreeInterfaceDragController(NodeTreeInterfaceView &view,
                                                                 bNodeTreeInterfaceItem &item,
                                                                 bNodeTree &tree)
    : AbstractViewItemDragController(view), item_(item), tree_(tree)
{
}

std::optional<eWM_DragDataType> NodeTreeInterfaceDragController::get_drag_type() const
{
  return WM_DRAG_NODE_TREE_INTERFACE;
}

void *NodeTreeInterfaceDragController::create_drag_data() const
{
  bNodeTreeInterfaceItemReference *drag_data = MEM_callocN<bNodeTreeInterfaceItemReference>(
      __func__);
  drag_data->item = &item_;
  drag_data->tree = &tree_;
  return drag_data;
}

bNodeTreeInterfaceItemReference *get_drag_node_tree_declaration(const wmDrag &drag)
{
  BLI_assert(drag.type == WM_DRAG_NODE_TREE_INTERFACE);
  return static_cast<bNodeTreeInterfaceItemReference *>(drag.poin);
}

bool is_dragging_parent_panel(const wmDrag &drag, const bNodeTreeInterfaceItem &drop_target_item)
{
  if (drag.type != WM_DRAG_NODE_TREE_INTERFACE) {
    return false;
  }
  bNodeTreeInterfaceItemReference *drag_data = get_drag_node_tree_declaration(drag);
  if (const bNodeTreeInterfacePanel *panel = node_interface::get_item_as<bNodeTreeInterfacePanel>(
          drag_data->item))
  {
    if (panel->contains(drop_target_item)) {
      return true;
    }
  }
  return false;
}

NodeSocketDropTarget::NodeSocketDropTarget(NodeSocketViewItem &item,
                                           bNodeTreeInterfaceSocket &socket)
    : TreeViewItemDropTarget(item, DropBehavior::Reorder), socket_(socket)
{
}

bool NodeSocketDropTarget::can_drop(const wmDrag &drag, const char ** /*r_disabled_hint*/) const
{
  if (drag.type != WM_DRAG_NODE_TREE_INTERFACE) {
    return false;
  }
  if (is_dragging_parent_panel(drag, socket_.item)) {
    return false;
  }
  return true;
}

std::string NodeSocketDropTarget::drop_tooltip(const DragInfo &drag_info) const
{
  switch (drag_info.drop_location) {
    case DropLocation::Into:
      return "";
    case DropLocation::Before:
      return TIP_("Insert before socket");
    case DropLocation::After:
      return TIP_("Insert after socket");
  }
  return "";
}

bool on_drop_flat_item(bContext *C,
                       const DragInfo &drag_info,
                       bNodeTree &ntree,
                       bNodeTreeInterfaceItem &drop_target_item)
{
  bNodeTreeInterfaceItemReference *drag_data = get_drag_node_tree_declaration(drag_info.drag_data);
  BLI_assert(drag_data != nullptr);
  bNodeTreeInterfaceItem *drag_item = drag_data->item;
  BLI_assert(drag_item != nullptr);

  bNodeTreeInterface &interface = ntree.tree_interface;

  bNodeTreeInterfacePanel *parent = interface.find_item_parent(drop_target_item, true);
  int index = -1;

  /* Insert into same panel as the target. */
  BLI_assert(parent != nullptr);
  switch (drag_info.drop_location) {
    case DropLocation::Before:
      index = parent->items().as_span().first_index_try(&drop_target_item);
      break;
    case DropLocation::After:
      index = parent->items().as_span().first_index_try(&drop_target_item) + 1;
      break;
    default:
      /* All valid cases should be handled above. */
      BLI_assert_unreachable();
      break;
  }
  if (parent == nullptr || index < 0) {
    return false;
  }

  interface.move_item_to_parent(*drag_item, parent, index);

  /* General update */
  BKE_main_ensure_invariants(*CTX_data_main(C), ntree.id);
  ED_undo_push(C, "Insert node group item");
  return true;
}

bool NodeSocketDropTarget::on_drop(bContext *C, const DragInfo &drag_info) const
{
  bNodeTree &nodetree = this->get_view<NodeTreeInterfaceView>().nodetree();
  return on_drop_flat_item(C, drag_info, nodetree, socket_.item);
}

NodePanelDropTarget::NodePanelDropTarget(NodePanelViewItem &item, bNodeTreeInterfacePanel &panel)
    : TreeViewItemDropTarget(item, DropBehavior::ReorderAndInsert), panel_(panel)
{
}

bool NodePanelDropTarget::can_drop(const wmDrag &drag, const char ** /*r_disabled_hint*/) const
{
  if (drag.type != WM_DRAG_NODE_TREE_INTERFACE) {
    return false;
  }
  bNodeTreeInterfaceItemReference *drag_data = get_drag_node_tree_declaration(drag);

  /* Can't drop an item onto its children. */
  if (const bNodeTreeInterfacePanel *panel = node_interface::get_item_as<bNodeTreeInterfacePanel>(
          drag_data->item))
  {
    if (panel->contains(panel_.item)) {
      return false;
    }
  }

  return true;
}

std::string NodePanelDropTarget::drop_tooltip(const DragInfo &drag_info) const
{
  switch (drag_info.drop_location) {
    case DropLocation::Into:
      return TIP_("Insert into panel");
    case DropLocation::Before:
      return TIP_("Insert before panel");
    case DropLocation::After:
      return TIP_("Insert after panel");
  }
  return "";
}

bool NodePanelDropTarget::on_drop(bContext *C, const DragInfo &drag_info) const
{
  bNodeTreeInterfaceItemReference *drag_data = get_drag_node_tree_declaration(drag_info.drag_data);
  BLI_assert(drag_data != nullptr);
  bNodeTreeInterfaceItem *drag_item = drag_data->item;
  BLI_assert(drag_item != nullptr);

  bNodeTree &nodetree = get_view<NodeTreeInterfaceView>().nodetree();
  bNodeTreeInterface &interface = get_view<NodeTreeInterfaceView>().interface();

  bNodeTreeInterfacePanel *parent = nullptr;
  int index = -1;
  switch (drag_info.drop_location) {
    case DropLocation::Into: {
      /* Insert into target */
      parent = &panel_;
      const bool has_toggle_socket = panel_.header_toggle_socket() != nullptr;
      index = has_toggle_socket ? 1 : 0;
      break;
    }
    case DropLocation::Before: {
      /* Insert into same panel as the target. */
      parent = interface.find_item_parent(panel_.item, true);
      BLI_assert(parent != nullptr);
      index = parent->items().as_span().first_index_try(&panel_.item);
      break;
    }
    case DropLocation::After: {
      /* Insert into same panel as the target. */
      parent = interface.find_item_parent(panel_.item, true);
      BLI_assert(parent != nullptr);
      index = parent->items().as_span().first_index_try(&panel_.item) + 1;
      break;
    }
  }
  if (parent == nullptr || index < 0) {
    return false;
  }

  interface.move_item_to_parent(*drag_item, parent, index);

  /* General update */
  BKE_main_ensure_invariants(*CTX_data_main(C), nodetree.id);
  ED_undo_push(C, "Insert node group item");
  return true;
}

}  // namespace

}  // namespace blender::ui::nodes

void uiTemplateNodeTreeInterface(uiLayout *layout, const bContext *C, PointerRNA *ptr)
{
  if (!ptr->data) {
    return;
  }
  if (!RNA_struct_is_a(ptr->type, &RNA_NodeTreeInterface)) {
    return;
  }
  bNodeTree &nodetree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNodeTreeInterface &interface = *static_cast<bNodeTreeInterface *>(ptr->data);

  uiBlock *block = layout->block();

  blender::ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "Node Tree Declaration Tree View",
      std::make_unique<blender::ui::nodes::NodeTreeInterfaceView>(nodetree, interface));
  tree_view->set_context_menu_title("Node Tree Interface");
  tree_view->set_default_rows(5);

  blender::ui::TreeViewBuilder::build_tree_view(*C, *tree_view, *layout);
}
