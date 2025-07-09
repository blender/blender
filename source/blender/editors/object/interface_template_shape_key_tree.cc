/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"
#include "BKE_key.hh"

#include "BLI_listbase.h"
#include "BLT_translation.hh"

#include "UI_interface_layout.hh"
#include "UI_tree_view.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph.hh"
#include "DNA_key_types.h"
#include "WM_api.hh"

#include "ED_undo.hh"
#include "WM_types.hh"
#include <fmt/format.h>

namespace blender::ed::object::shapekey {

class ShapeKeyTreeView : public ui::AbstractTreeView {
 protected:
  Object &object_;

 public:
  ShapeKeyTreeView(Object &ob) : object_(ob)
  {
    is_flat_ = true;
  };

  void build_tree() override;
};

struct ShapeKey {
  Object *object;
  Key *key;
  KeyBlock *kb;
  int index;
};

class ShapeKeyDragController : public ui::AbstractViewItemDragController {
 private:
  ShapeKey drag_key_;

 public:
  ShapeKeyDragController(ShapeKeyTreeView &view, ShapeKey drag_key)
      : AbstractViewItemDragController(view), drag_key_(drag_key)
  {
  }

  eWM_DragDataType get_drag_type() const override
  {
    return WM_DRAG_SHAPE_KEY;
  }

  void *create_drag_data() const override
  {
    ShapeKey *drag_data = MEM_callocN<ShapeKey>(__func__);
    *drag_data = drag_key_;
    return drag_data;
  }
  void on_drag_start() override
  {
    drag_key_.object->shapenr = drag_key_.index + 1;
  }
};

class ShapeKeyDropTarget : public ui::TreeViewItemDropTarget {
 private:
  KeyBlock &drop_kb_;
  int drop_index_;

 public:
  ShapeKeyDropTarget(ui::AbstractTreeViewItem &item,
                     ui::DropBehavior behavior,
                     KeyBlock &drop_kb,
                     int index)
      : TreeViewItemDropTarget(item, behavior), drop_kb_(drop_kb), drop_index_(index)
  {
  }

  bool can_drop(const wmDrag &drag, const char ** /*r_disabled_hint*/) const override
  {
    if (drag.type != WM_DRAG_SHAPE_KEY) {
      return false;
    }
    const ShapeKey *drag_shapekey = static_cast<const ShapeKey *>(drag.poin);
    if (drag_shapekey->index == drop_index_) {
      return false;
    }
    return true;
  }

  std::string drop_tooltip(const ui::DragInfo &drag_info) const override
  {
    const ShapeKey *drag_shapekey = static_cast<const ShapeKey *>(drag_info.drag_data.poin);
    const StringRef drag_name = drag_shapekey->kb->name;
    const StringRef drop_name = drop_kb_.name;

    switch (drag_info.drop_location) {
      case ui::DropLocation::Into:
        BLI_assert_unreachable();
        break;
      case ui::DropLocation::Before:
        return fmt::format(fmt::runtime(TIP_("Move {} above {}")), drag_name, drop_name);
      case ui::DropLocation::After:
        return fmt::format(fmt::runtime(TIP_("Move {} below {}")), drag_name, drop_name);
      default:
        BLI_assert_unreachable();
        break;
    }

    return "";
  }

  bool on_drop(bContext *C, const ui::DragInfo &drag_info) const override
  {
    const ShapeKey *drag_shapekey = static_cast<const ShapeKey *>(drag_info.drag_data.poin);
    int drop_index = drop_index_;
    const int drag_index = drag_shapekey->index;

    switch (drag_info.drop_location) {
      case ui::DropLocation::Into:
        BLI_assert_unreachable();
        break;
      case ui::DropLocation::Before:
        drop_index -= int(drag_index < drop_index);
        break;
      case ui::DropLocation::After:
        drop_index += int(drag_index > drop_index);
        break;
    }
    Object *object = drag_shapekey->object;
    BKE_keyblock_move(object, drag_shapekey->index, drop_index);

    DEG_id_tag_update(static_cast<ID *>(object->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, object);
    ED_undo_push(C, "Drop Active Shape Key");

    return true;
  }
};

class ShapeKeyItem : public ui::AbstractTreeViewItem {
 private:
  ShapeKey shape_key_;

 public:
  ShapeKeyItem(Object *object, Key *key, KeyBlock *kb, int index)
  {
    label_ = kb->name;
    shape_key_.object = object;
    shape_key_.key = key;
    shape_key_.kb = kb;
    shape_key_.index = index;
  };

  void build_row(uiLayout &row) override
  {
    uiItemL_ex(&row, this->label_, ICON_SHAPEKEY_DATA, false, false);
    uiLayout *sub = &row.row(true);
    sub->use_property_decorate_set(false);
    PointerRNA shapekey_ptr = RNA_pointer_create_discrete(
        &shape_key_.key->id, &RNA_ShapeKey, shape_key_.kb);

    if (shape_key_.index > 0) {
      sub->prop(&shapekey_ptr, "value", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);
    }

    sub->prop(&shapekey_ptr, "mute", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);
    sub->prop(&shapekey_ptr, "lock_shape", UI_ITEM_R_ICON_ONLY, std::nullopt, ICON_NONE);
  }

  std::optional<bool> should_be_active() const override
  {
    return shape_key_.object->shapenr == shape_key_.index + 1;
  }

  void on_activate(bContext &C) override
  {
    PointerRNA object_ptr = RNA_pointer_create_discrete(
        &shape_key_.object->id, &RNA_Object, shape_key_.object);
    PropertyRNA *prop = RNA_struct_find_property(&object_ptr, "active_shape_key_index");
    RNA_property_int_set(&object_ptr, prop, shape_key_.index);
    RNA_property_update(&C, &object_ptr, prop);

    ED_undo_push(&C, "Set Active Shape Key");
  }

  std::optional<bool> should_be_selected() const override
  {
    return shape_key_.kb->flag & KEYBLOCK_SEL;
  }

  void set_selected(const bool select) override
  {
    AbstractViewItem::set_selected(select);
    SET_FLAG_FROM_TEST(shape_key_.kb->flag, select, KEYBLOCK_SEL);
  }

  bool supports_renaming() const override
  {
    return true;
  }

  bool rename(const bContext &C, StringRefNull new_name) override
  {
    PointerRNA shapekey_ptr = RNA_pointer_create_discrete(
        &shape_key_.key->id, &RNA_ShapeKey, shape_key_.kb);
    RNA_string_set(&shapekey_ptr, "name", new_name.c_str());
    ED_undo_push(const_cast<bContext *>(&C), "Rename shape key");
    return true;
  }

  StringRef get_rename_string() const override
  {
    return label_;
  }

  std::unique_ptr<ui::AbstractViewItemDragController> create_drag_controller() const override
  {
    return std::make_unique<ShapeKeyDragController>(
        static_cast<ShapeKeyTreeView &>(get_tree_view()), shape_key_);
  }

  std::unique_ptr<ui::TreeViewItemDropTarget> create_drop_target() override
  {
    return std::make_unique<ShapeKeyDropTarget>(
        *this, ui::DropBehavior::Reorder, *shape_key_.kb, shape_key_.index);
  }
};

void ShapeKeyTreeView::build_tree()
{
  Key *key = BKE_key_from_object(&object_);
  if (key == nullptr) {
    return;
  }
  int index = 1;
  LISTBASE_FOREACH_INDEX (KeyBlock *, kb, &key->block, index) {
    this->add_tree_item<ShapeKeyItem>(&object_, key, kb, index);
  }
}

void template_tree(uiLayout *layout, bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }

  uiBlock *block = layout->block();

  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "Shape Key Tree View",
      std::make_unique<ed::object::shapekey::ShapeKeyTreeView>(*ob));
  tree_view->set_context_menu_title("Shape Key");
  tree_view->set_default_rows(4);
  tree_view->allow_multiselect_items();

  ui::TreeViewBuilder::build_tree_view(*C, *tree_view, *layout);
}
}  // namespace blender::ed::object::shapekey
