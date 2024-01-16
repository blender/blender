/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"

#include "BLT_translation.h"

#include "ANIM_bone_collections.hh"

#include "UI_interface.hh"
#include "UI_tree_view.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "ED_undo.hh"

#include "WM_api.hh"

#include <fmt/format.h>

namespace blender::ui::bonecollections {

using namespace blender::animrig;

class BoneCollectionTreeView : public AbstractTreeView {
 protected:
  bArmature &armature_;

 public:
  explicit BoneCollectionTreeView(bArmature &armature);
  void build_tree() override;

 private:
  void build_tree_node_recursive(TreeViewItemContainer &parent, const int bcoll_index);
};

/**
 * Bone collection and the Armature that owns it.
 */
struct ArmatureBoneCollection {
  bArmature *armature;
  int bcoll_index;

  ArmatureBoneCollection() = default;
  ArmatureBoneCollection(bArmature *armature, int bcoll_index)
      : armature(armature), bcoll_index(bcoll_index)
  {
  }

  const BoneCollection &bcoll() const
  {
    return *armature->collection_array[bcoll_index];
  }
  BoneCollection &bcoll()
  {
    return *armature->collection_array[bcoll_index];
  }
};

class BoneCollectionDragController : public AbstractViewItemDragController {
 private:
  ArmatureBoneCollection drag_arm_bcoll_;

 public:
  BoneCollectionDragController(BoneCollectionTreeView &tree_view,
                               bArmature &armature,
                               const int bcoll_index);

  eWM_DragDataType get_drag_type() const override;
  void *create_drag_data() const override;
  void on_drag_start() override;
};

class BoneCollectionDropTarget : public TreeViewItemDropTarget {
 private:
  ArmatureBoneCollection drop_bonecoll_;

 public:
  BoneCollectionDropTarget(AbstractTreeViewItem &item,
                           DropBehavior behavior,
                           const ArmatureBoneCollection &drop_bonecoll)
      : TreeViewItemDropTarget(item, behavior), drop_bonecoll_(drop_bonecoll)
  {
  }

  bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override
  {
    const ArmatureBoneCollection *drag_arm_bcoll = static_cast<const ArmatureBoneCollection *>(
        drag.poin);

    /* Do not allow dropping onto another armature. */
    if (drag_arm_bcoll->armature != drop_bonecoll_.armature) {
      *r_disabled_hint = "Cannot drag & drop bone collections between Armatures.";
      return false;
    }

    /* Dragging onto itself doesn't do anything. */
    if (drag_arm_bcoll->bcoll_index == drop_bonecoll_.bcoll_index) {
      return false;
    }

    /* Do not allow dropping onto its own descendants. */
    if (armature_bonecoll_is_descendant_of(
            drag_arm_bcoll->armature, drag_arm_bcoll->bcoll_index, drop_bonecoll_.bcoll_index))
    {
      *r_disabled_hint = "Cannot drag a collection onto a descendent";
      return false;
    }

    return true;
  }

  std::string drop_tooltip(const DragInfo &drag_info) const override
  {
    const ArmatureBoneCollection *drag_bone_collection =
        static_cast<const ArmatureBoneCollection *>(drag_info.drag_data.poin);
    const BoneCollection &drag_bcoll = drag_bone_collection->bcoll();
    const BoneCollection &drop_bcoll = drop_bonecoll_.bcoll();

    std::string_view drag_name = drag_bcoll.name;
    std::string_view drop_name = drop_bcoll.name;

    switch (drag_info.drop_location) {
      case DropLocation::Into:
        return fmt::format(TIP_("Move {} into {}"), drag_name, drop_name);
      case DropLocation::Before:
        return fmt::format(TIP_("Move {} above {}"), drag_name, drop_name);
      case DropLocation::After:
        return fmt::format(TIP_("Move {} below {}"), drag_name, drop_name);
    }

    return "";
  }

  bool on_drop(bContext *C, const DragInfo &drag_info) const override
  {
    const ArmatureBoneCollection *drag_arm_bcoll = static_cast<const ArmatureBoneCollection *>(
        drag_info.drag_data.poin);
    bArmature *arm = drop_bonecoll_.armature;

    const int from_bcoll_index = drag_arm_bcoll->bcoll_index;
    const int to_bcoll_index = drop_bonecoll_.bcoll_index;

    int new_bcoll_index = -1;
    switch (drag_info.drop_location) {
      case DropLocation::Before:
        new_bcoll_index = ANIM_armature_bonecoll_move_before_after_index(
            arm, from_bcoll_index, to_bcoll_index, MoveLocation::Before);
        break;

      case DropLocation::Into: {
        if (!ANIM_armature_bonecoll_is_editable(arm, &drop_bonecoll_.bcoll())) {
          return false;
        }

        const int from_parent_index = armature_bonecoll_find_parent_index(arm, from_bcoll_index);
        /* The bone collection becomes the last child of the new parent, as
         * that's consistent with the drag & drop of scene collections in the
         * outliner. */
        new_bcoll_index = armature_bonecoll_move_to_parent(
            arm, from_bcoll_index, -1, from_parent_index, to_bcoll_index);
        break;
      }
      case DropLocation::After:
        new_bcoll_index = ANIM_armature_bonecoll_move_before_after_index(
            arm, from_bcoll_index, to_bcoll_index, MoveLocation::After);
        break;
    }

    if (new_bcoll_index < 0) {
      return false;
    }

    ANIM_armature_bonecoll_active_index_set(arm, new_bcoll_index);
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_COLLECTION, &arm->id);

    ED_undo_push(C, "Reorder Armature Bone Collections");
    return true;
  }
};

class BoneCollectionItem : public AbstractTreeViewItem {
 private:
  bArmature &armature_;
  int bcoll_index_;
  BoneCollection &bone_collection_;

 public:
  BoneCollectionItem(bArmature &armature, const int bcoll_index)
      : armature_(armature),
        bcoll_index_(bcoll_index),
        bone_collection_(*armature.collection_array[bcoll_index])
  {
    this->label_ = bone_collection_.name;
  }

  void build_row(uiLayout &row) override
  {
    uiLayout *sub = uiLayoutRow(&row, true);

    uiBut *name_label = uiItemL_ex(sub, bone_collection_.name, ICON_NONE, false, false);
    if (!ANIM_armature_bonecoll_is_editable(&armature_, &bone_collection_)) {
      UI_but_flag_enable(name_label, UI_BUT_INACTIVE);
    }

    /* Contains Active Bone icon. */
    /* Performance note: this check potentially loops over all bone collections the active bone is
     * assigned to. And this happens for each redraw of each bone collection in the armature. */
    {
      const bool contains_active_bone = ANIM_armature_bonecoll_contains_active_bone(
          &armature_, &bone_collection_);
      const int icon = contains_active_bone ? ICON_DOT : ICON_BLANK1;
      uiItemL(sub, "", icon);
    }

    /* Visibility eye icon. */
    {
      uiLayout *visibility_sub = uiLayoutRow(sub, true);
      uiLayoutSetActive(visibility_sub, bone_collection_.is_visible_ancestors());

      const int icon = bone_collection_.is_visible() ? ICON_HIDE_OFF : ICON_HIDE_ON;
      PointerRNA bcoll_ptr = rna_pointer();
      uiItemR(visibility_sub, &bcoll_ptr, "is_visible", UI_ITEM_R_ICON_ONLY, "", icon);
    }
  }

  void build_context_menu(bContext &C, uiLayout &column) const override
  {
    MenuType *mt = WM_menutype_find("ARMATURE_MT_collection_tree_context_menu", true);
    if (!mt) {
      return;
    }
    UI_menutype_draw(&C, mt, &column);
  }

  std::optional<bool> should_be_active() const override
  {
    return armature_.runtime.active_collection_index == bcoll_index_;
  }

  void on_activate(bContext &C) override
  {
    /* Let RNA handle the property change. This makes sure all the notifiers and DEG
     * update calls are properly called. */
    PointerRNA bcolls_ptr = RNA_pointer_create(&armature_.id, &RNA_BoneCollections, &armature_);
    PropertyRNA *prop = RNA_struct_find_property(&bcolls_ptr, "active_index");

    RNA_property_int_set(&bcolls_ptr, prop, bcoll_index_);
    RNA_property_update(&const_cast<bContext &>(C), &bcolls_ptr, prop);

    ED_undo_push(&const_cast<bContext &>(C), "Change Armature's Active Bone Collection");
  }

  bool supports_renaming() const override
  {
    return ANIM_armature_bonecoll_is_editable(&armature_, &bone_collection_);
  }

  bool rename(const bContext &C, StringRefNull new_name) override
  {
    /* Let RNA handle the renaming. This makes sure all the notifiers and DEG
     * update calls are properly called. */
    PointerRNA bcoll_ptr = rna_pointer();
    PropertyRNA *prop = RNA_struct_find_property(&bcoll_ptr, "name");

    RNA_property_string_set(&bcoll_ptr, prop, new_name.c_str());
    RNA_property_update(&const_cast<bContext &>(C), &bcoll_ptr, prop);

    ED_undo_push(&const_cast<bContext &>(C), "Rename Armature Bone Collection");
    return true;
  }

  StringRef get_rename_string() const override
  {
    return bone_collection_.name;
  }

  std::unique_ptr<AbstractViewItemDragController> create_drag_controller() const override
  {
    /* Reject dragging linked (or otherwise uneditable) bone collections. */
    if (!ANIM_armature_bonecoll_is_editable(&armature_, &bone_collection_)) {
      return {};
    }

    BoneCollectionTreeView &tree_view = static_cast<BoneCollectionTreeView &>(get_tree_view());
    return std::make_unique<BoneCollectionDragController>(tree_view, armature_, bcoll_index_);
  }

  std::unique_ptr<TreeViewItemDropTarget> create_drop_target() override
  {
    ArmatureBoneCollection drop_bonecoll(&armature_, bcoll_index_);
    /* For now, only support DropBehavior::Insert until there's code for actually reordering
     * siblings. Currently only 'move to another parent' is implemented. */
    return std::make_unique<BoneCollectionDropTarget>(
        *this, DropBehavior::ReorderAndInsert, drop_bonecoll);
  }

 protected:
  /** RNA pointer to the BoneCollection. */
  PointerRNA rna_pointer()
  {
    return RNA_pointer_create(&armature_.id, &RNA_BoneCollection, &bone_collection_);
  }
};

BoneCollectionTreeView::BoneCollectionTreeView(bArmature &armature) : armature_(armature) {}

void BoneCollectionTreeView::build_tree()
{
  for (int bcoll_index = 0; bcoll_index < armature_.collection_root_count; bcoll_index++) {
    build_tree_node_recursive(*this, bcoll_index);
  }
}

void BoneCollectionTreeView::build_tree_node_recursive(TreeViewItemContainer &parent,
                                                       const int bcoll_index)
{
  BoneCollection *bcoll = armature_.collection_array[bcoll_index];
  BoneCollectionItem &bcoll_tree_item = parent.add_tree_item<BoneCollectionItem>(armature_,
                                                                                 bcoll_index);
  bcoll_tree_item.set_collapsed(false);

  for (int child_index = bcoll->child_index; child_index < bcoll->child_index + bcoll->child_count;
       child_index++)
  {
    build_tree_node_recursive(bcoll_tree_item, child_index);
  }
}

BoneCollectionDragController::BoneCollectionDragController(BoneCollectionTreeView &tree_view,
                                                           bArmature &armature,
                                                           const int bcoll_index)
    : AbstractViewItemDragController(tree_view), drag_arm_bcoll_(&armature, bcoll_index)
{
}

eWM_DragDataType BoneCollectionDragController::get_drag_type() const
{
  return WM_DRAG_BONE_COLLECTION;
}

void *BoneCollectionDragController::create_drag_data() const
{
  ArmatureBoneCollection *drag_data = MEM_new<ArmatureBoneCollection>(__func__);
  *drag_data = drag_arm_bcoll_;
  return drag_data;
}

void BoneCollectionDragController::on_drag_start()
{
  ANIM_armature_bonecoll_active_index_set(drag_arm_bcoll_.armature, drag_arm_bcoll_.bcoll_index);
}

}  // namespace blender::ui::bonecollections

void uiTemplateBoneCollectionTree(uiLayout *layout, bContext *C)
{
  using namespace blender;

  Object *object = CTX_data_active_object(C);
  if (!object || object->type != OB_ARMATURE) {
    return;
  }

  bArmature *arm = static_cast<bArmature *>(object->data);
  BLI_assert(GS(arm->id.name) == ID_AR);

  uiBlock *block = uiLayoutGetBlock(layout);

  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "Bone Collection Tree View",
      std::make_unique<blender::ui::bonecollections::BoneCollectionTreeView>(*arm));
  tree_view->set_min_rows(3);

  ui::TreeViewBuilder::build_tree_view(*tree_view, *layout);
}
