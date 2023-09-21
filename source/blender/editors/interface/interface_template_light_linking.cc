/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "UI_interface.hh"

#include <cstdio>
#include <memory>

#include "BLT_translation.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_light_linking.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_tree_view.hh"

#include "WM_api.hh"

#include "ED_undo.hh"

namespace blender::ui::light_linking {

namespace {

class CollectionDropTarget : public DropTargetInterface {
  Collection &collection_;

 public:
  CollectionDropTarget(Collection &collection) : collection_(collection) {}

  bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const override
  {
    if (drag.type != WM_DRAG_ID) {
      return false;
    }

    const wmDragID *drag_id = static_cast<wmDragID *>(drag.ids.first);
    if (!drag_id) {
      return false;
    }

    /* The dragged IDs are guaranteed to be the same type, so only check the type of the first one.
     */
    const ID_Type id_type = GS(drag_id->id->name);
    if (!ELEM(id_type, ID_OB, ID_GR)) {
      *r_disabled_hint = "Can only add objects and collections to the light linking collection";
      return false;
    }

    return true;
  }

  std::string drop_tooltip(const DragInfo & /*drag*/) const override
  {
    return TIP_("Add to light linking collection");
  }

  bool on_drop(bContext *C, const DragInfo &drag) const override
  {
    Main *bmain = CTX_data_main(C);
    Scene *scene = CTX_data_scene(C);

    LISTBASE_FOREACH (wmDragID *, drag_id, &drag.drag_data.ids) {
      BKE_light_linking_add_receiver_to_collection(
          bmain, &collection_, drag_id->id, COLLECTION_LIGHT_LINKING_STATE_INCLUDE);
    }

    /* It is possible that the light linking collection is also used by the view layer.
     * For this case send a notifier so that the UI is updated for the changes in the collection
     * content. */
    WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

    ED_undo_push(C, "Add to light linking collection");

    return true;
  }
};

class CollectionViewItem : public BasicTreeViewItem {
  Collection &collection_;

  ID *id_ = nullptr;
  CollectionLightLinking &collection_light_linking_;

 public:
  CollectionViewItem(Collection &collection,
                     ID &id,
                     CollectionLightLinking &collection_light_linking,
                     const BIFIconID icon)
      : BasicTreeViewItem(id.name + 2, icon),
        collection_(collection),
        id_(&id),
        collection_light_linking_(collection_light_linking)
  {
  }

  void build_row(uiLayout &row) override
  {
    add_label(row);

    uiLayout *sub = uiLayoutRow(&row, true);
    uiLayoutSetPropDecorate(sub, false);

    build_state_button(*sub);
    build_remove_button(*sub);
  }

 private:
  int get_state_icon() const
  {
    /* TODO(sergey): Use proper icons. */
    switch (collection_light_linking_.link_state) {
      case COLLECTION_LIGHT_LINKING_STATE_INCLUDE:
        return ICON_OUTLINER_OB_LIGHT;
      case COLLECTION_LIGHT_LINKING_STATE_EXCLUDE:
        return ICON_LIGHT;
    }
    BLI_assert_unreachable();
    return ICON_NONE;
  }

  static void link_state_toggle(CollectionLightLinking &collection_light_linking)
  {
    switch (collection_light_linking.link_state) {
      case COLLECTION_LIGHT_LINKING_STATE_INCLUDE:
        collection_light_linking.link_state = COLLECTION_LIGHT_LINKING_STATE_EXCLUDE;
        return;
      case COLLECTION_LIGHT_LINKING_STATE_EXCLUDE:
        collection_light_linking.link_state = COLLECTION_LIGHT_LINKING_STATE_INCLUDE;
        return;
    }

    BLI_assert_unreachable();
  }

  void build_state_button(uiLayout &row)
  {
    uiBlock *block = uiLayoutGetBlock(&row);
    const int icon = get_state_icon();

    PointerRNA collection_light_linking_ptr = RNA_pointer_create(
        &collection_.id, &RNA_CollectionLightLinking, &collection_light_linking_);

    uiBut *button = uiDefIconButR(block,
                                  UI_BTYPE_BUT,
                                  0,
                                  icon,
                                  0,
                                  0,
                                  UI_UNIT_X,
                                  UI_UNIT_Y,
                                  &collection_light_linking_ptr,
                                  "link_state",
                                  0,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  0.0f,
                                  nullptr);

    UI_but_func_set(button, [&collection_light_linking = collection_light_linking_](bContext &) {
      link_state_toggle(collection_light_linking);
    });
  }

  void build_remove_button(uiLayout &row)
  {
    PointerRNA id_ptr = RNA_id_pointer_create(id_);
    PointerRNA collection_ptr = RNA_id_pointer_create(&collection_.id);

    uiLayoutSetContextPointer(&row, "id", &id_ptr);
    uiLayoutSetContextPointer(&row, "collection", &collection_ptr);

    uiItemO(&row, "", ICON_X, "OBJECT_OT_light_linking_unlink_from_collection");
  }
};

class CollectionView : public AbstractTreeView {
  Collection &collection_;

 public:
  explicit CollectionView(Collection &collection) : collection_(collection) {}

  void build_tree() override
  {
    LISTBASE_FOREACH (CollectionChild *, collection_child, &collection_.children) {
      Collection *child_collection = collection_child->collection;
      add_tree_item<CollectionViewItem>(collection_,
                                        child_collection->id,
                                        collection_child->light_linking,
                                        ICON_OUTLINER_COLLECTION);
    }

    LISTBASE_FOREACH (CollectionObject *, collection_object, &collection_.gobject) {
      Object *child_object = collection_object->ob;
      add_tree_item<CollectionViewItem>(
          collection_, child_object->id, collection_object->light_linking, ICON_OBJECT_DATA);
    }
  }

  std::unique_ptr<DropTargetInterface> create_drop_target() override
  {
    return std::make_unique<CollectionDropTarget>(collection_);
  }
};

}  // namespace

}  // namespace blender::ui::light_linking

void uiTemplateLightLinkingCollection(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
  if (!ptr->data) {
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  if (!prop) {
    printf(
        "%s: property not found: %s.%s\n", __func__, RNA_struct_identifier(ptr->type), propname);
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname);
    return;
  }

  const PointerRNA collection_ptr = RNA_property_pointer_get(ptr, prop);
  if (!collection_ptr.data) {
    return;
  }
  if (collection_ptr.type != &RNA_Collection) {
    printf("%s: expected collection pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname);
    return;
  }

  Collection *collection = static_cast<Collection *>(collection_ptr.data);

  uiBlock *block = uiLayoutGetBlock(layout);

  blender::ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "Light Linking Collection Tree View",
      std::make_unique<blender::ui::light_linking::CollectionView>(*collection));
  tree_view->set_min_rows(3);

  blender::ui::TreeViewBuilder::build_tree_view(*tree_view, *layout);
}
