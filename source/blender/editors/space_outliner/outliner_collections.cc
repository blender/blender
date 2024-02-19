/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_object_types.h"

#include "BKE_collection.h"
#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_report.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "outliner_intern.hh" /* own include */

namespace blender::ed::outliner {

/* -------------------------------------------------------------------- */
/** \name Utility API
 * \{ */

bool outliner_is_collection_tree_element(const TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (!tselem) {
    return false;
  }

  if (ELEM(
          tselem->type, TSE_LAYER_COLLECTION, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE))
  {
    return true;
  }
  if ((tselem->type == TSE_SOME_ID) && te->idcode == ID_GR) {
    return true;
  }

  return false;
}

Collection *outliner_collection_from_tree_element(const TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (!tselem) {
    return nullptr;
  }

  if (tselem->type == TSE_LAYER_COLLECTION) {
    LayerCollection *lc = static_cast<LayerCollection *>(te->directdata);
    return lc->collection;
  }
  if (ELEM(tselem->type, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE)) {
    Scene *scene = (Scene *)tselem->id;
    return scene->master_collection;
  }
  if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_GR)) {
    return (Collection *)tselem->id;
  }

  return nullptr;
}

TreeTraversalAction outliner_collect_selected_collections(TreeElement *te, void *customdata)
{
  IDsSelectedData *data = static_cast<IDsSelectedData *>(customdata);
  TreeStoreElem *tselem = TREESTORE(te);

  if (outliner_is_collection_tree_element(te)) {
    BLI_addtail(&data->selected_array, BLI_genericNodeN(te));
    return TRAVERSE_CONTINUE;
  }

  if ((tselem->type != TSE_SOME_ID) || (tselem->id && GS(tselem->id->name) != ID_GR)) {
    return TRAVERSE_SKIP_CHILDS;
  }

  return TRAVERSE_CONTINUE;
}

TreeTraversalAction outliner_collect_selected_objects(TreeElement *te, void *customdata)
{
  IDsSelectedData *data = static_cast<IDsSelectedData *>(customdata);
  TreeStoreElem *tselem = TREESTORE(te);

  if (outliner_is_collection_tree_element(te)) {
    return TRAVERSE_CONTINUE;
  }

  if ((tselem->type != TSE_SOME_ID) || (tselem->id == nullptr) || (GS(tselem->id->name) != ID_OB))
  {
    return TRAVERSE_SKIP_CHILDS;
  }

  BLI_addtail(&data->selected_array, BLI_genericNodeN(te));

  return TRAVERSE_CONTINUE;
}

}  // namespace blender::ed::outliner

void ED_outliner_selected_objects_get(const bContext *C, ListBase *objects)
{
  using namespace blender::ed::outliner;

  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  IDsSelectedData data = {{nullptr}};
  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         outliner_collect_selected_objects,
                         &data);
  LISTBASE_FOREACH (LinkData *, link, &data.selected_array) {
    TreeElement *ten_selected = (TreeElement *)link->data;
    Object *ob = (Object *)TREESTORE(ten_selected)->id;
    BLI_addtail(objects, BLI_genericNodeN(ob));
  }
  BLI_freelistN(&data.selected_array);
}

namespace blender::ed::outliner {

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

}  // namespace blender::ed::outliner

bool ED_outliner_collections_editor_poll(bContext *C)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  return (space_outliner != nullptr) &&
         ELEM(space_outliner->outlinevis, SO_VIEW_LAYER, SO_SCENES, SO_LIBRARIES);
}

namespace blender::ed::outliner {

static bool outliner_view_layer_collections_editor_poll(bContext *C)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  return (space_outliner != nullptr) && (space_outliner->outlinevis == SO_VIEW_LAYER);
}

static bool collection_edit_in_active_scene_poll(bContext *C)
{
  if (!ED_outliner_collections_editor_poll(C)) {
    return false;
  }
  Scene *scene = CTX_data_scene(C);
  if (ID_IS_LINKED(scene) || ID_IS_OVERRIDE_LIBRARY(scene)) {
    return false;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Collection
 * \{ */

struct CollectionNewData {
  bool error;
  Collection *collection;
};

static TreeTraversalAction collection_find_selected_to_add(TreeElement *te, void *customdata)
{
  CollectionNewData *data = static_cast<CollectionNewData *>(customdata);
  Collection *collection = outliner_collection_from_tree_element(te);

  if (!collection) {
    return TRAVERSE_SKIP_CHILDS;
  }

  if (data->collection != nullptr) {
    data->error = true;
    return TRAVERSE_BREAK;
  }

  data->collection = collection;
  return TRAVERSE_CONTINUE;
}

static int collection_new_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  CollectionNewData data{};

  if (RNA_boolean_get(op->ptr, "nested")) {
    outliner_build_tree(bmain, scene, view_layer, space_outliner, region);

    outliner_tree_traverse(space_outliner,
                           &space_outliner->tree,
                           0,
                           TSE_SELECTED,
                           collection_find_selected_to_add,
                           &data);

    if (data.error) {
      BKE_report(op->reports, RPT_ERROR, "More than one collection is selected");
      return OPERATOR_CANCELLED;
    }
  }

  if (data.collection == nullptr || ID_IS_LINKED(data.collection) ||
      ID_IS_OVERRIDE_LIBRARY(data.collection))
  {
    data.collection = scene->master_collection;
  }

  if (ID_IS_LINKED(scene) || ID_IS_OVERRIDE_LIBRARY(scene)) {
    BKE_report(op->reports, RPT_ERROR, "Can't add a new collection to linked/override scene");
    return OPERATOR_CANCELLED;
  }

  BKE_collection_add(bmain, data.collection, nullptr);

  DEG_id_tag_update(&data.collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);

  outliner_cleanup_tree(space_outliner);
  WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Collection";
  ot->idname = "OUTLINER_OT_collection_new";
  ot->description = "Add a new collection inside selected collection";

  /* api callbacks */
  ot->exec = collection_new_exec;
  ot->poll = collection_edit_in_active_scene_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "nested", true, "Nested", "Add as child of selected collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Collection
 * \{ */

struct CollectionEditData {
  Scene *scene;
  SpaceOutliner *space_outliner;
  GSet *collections_to_edit;

  /* Whether the processed operation should be allowed on liboverride collections, or not. */
  bool is_liboverride_allowed;
  /* Whether the processed operation should be allowed on hierarchy roots of liboverride
   * collections, or not. */
  bool is_liboverride_hierarchy_root_allowed;
};

static TreeTraversalAction collection_collect_data_to_edit(TreeElement *te, void *customdata)
{
  CollectionEditData *data = static_cast<CollectionEditData *>(customdata);
  Collection *collection = outliner_collection_from_tree_element(te);

  if (!collection) {
    return TRAVERSE_SKIP_CHILDS;
  }

  if (collection->flag & COLLECTION_IS_MASTER) {
    /* Skip - showing warning/error message might be misleading
     * when deleting multiple collections, so just do nothing. */
    return TRAVERSE_CONTINUE;
  }

  if (ID_IS_OVERRIDE_LIBRARY_REAL(collection)) {
    if (ID_IS_OVERRIDE_LIBRARY_HIERARCHY_ROOT(collection)) {
      if (!(data->is_liboverride_hierarchy_root_allowed || data->is_liboverride_allowed)) {
        return TRAVERSE_SKIP_CHILDS;
      }
    }
    else {
      if (!data->is_liboverride_allowed) {
        return TRAVERSE_SKIP_CHILDS;
      }
    }
  }

  /* Delete, duplicate and link don't edit children, those will come along
   * with the parents. */
  BLI_gset_add(data->collections_to_edit, collection);
  return TRAVERSE_SKIP_CHILDS;
}

void outliner_collection_delete(
    bContext *C, Main *bmain, Scene *scene, ReportList *reports, bool do_hierarchy)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  CollectionEditData data{};
  data.scene = scene;
  data.space_outliner = space_outliner;
  data.is_liboverride_allowed = false;
  data.is_liboverride_hierarchy_root_allowed = do_hierarchy;

  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  /* We first walk over and find the Collections we actually want to delete
   * (ignoring duplicates). */
  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         collection_collect_data_to_edit,
                         &data);

  /* Effectively delete the collections. */
  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    Collection *collection = static_cast<Collection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));

    /* Test in case collection got deleted as part of another one. */
    if (BLI_findindex(&bmain->collections, collection) != -1) {
      /* We cannot allow deleting collections that are indirectly linked,
       * or that are used by (linked to...) other linked scene/collection. */
      bool skip = false;
      if (ID_IS_LINKED(collection)) {
        if (collection->id.tag & LIB_TAG_INDIRECT) {
          skip = true;
        }
        else {
          LISTBASE_FOREACH (CollectionParent *, cparent, &collection->runtime.parents) {
            Collection *parent = cparent->collection;
            if (ID_IS_LINKED(parent) || ID_IS_OVERRIDE_LIBRARY(parent)) {
              skip = true;
              break;
            }
            if (parent->flag & COLLECTION_IS_MASTER) {
              BLI_assert(parent->id.flag & LIB_EMBEDDED_DATA);

              ID *scene_owner = BKE_id_owner_get(&parent->id);
              BLI_assert(scene_owner != nullptr);
              BLI_assert(GS(scene_owner->name) == ID_SCE);
              if (ID_IS_LINKED(scene_owner) || ID_IS_OVERRIDE_LIBRARY(scene_owner)) {
                skip = true;
                break;
              }
            }
          }
        }
      }

      if (!skip) {
        BKE_collection_delete(bmain, collection, do_hierarchy);
      }
      else {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Cannot delete collection '%s', it is either a linked one used by other "
                    "linked scenes/collections, or a library override one",
                    collection->id.name + 2);
      }
    }
  }

  BLI_gset_free(data.collections_to_edit, nullptr);
}

static int collection_hierarchy_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Base *basact_prev = BKE_view_layer_active_base_get(view_layer);

  outliner_collection_delete(C, bmain, scene, op->reports, true);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);

  BKE_view_layer_synced_ensure(scene, view_layer);
  if (basact_prev != BKE_view_layer_active_base_get(view_layer)) {
    WM_msg_publish_rna_prop(mbus, &scene->id, view_layer, LayerObjects, active);
  }

  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_hierarchy_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Hierarchy";
  ot->idname = "OUTLINER_OT_collection_hierarchy_delete";
  ot->description = "Delete selected collection hierarchies";

  /* api callbacks */
  ot->exec = collection_hierarchy_delete_exec;
  ot->poll = collection_edit_in_active_scene_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select/Deselect Collection Objects
 * \{ */

struct CollectionObjectsSelectData {
  bool error;
  LayerCollection *layer_collection;
};

static TreeTraversalAction outliner_find_first_selected_layer_collection(TreeElement *te,
                                                                         void *customdata)
{
  CollectionObjectsSelectData *data = static_cast<CollectionObjectsSelectData *>(customdata);
  TreeStoreElem *tselem = TREESTORE(te);

  switch (tselem->type) {
    case TSE_LAYER_COLLECTION:
      data->layer_collection = static_cast<LayerCollection *>(te->directdata);
      return TRAVERSE_BREAK;
    case TSE_R_LAYER:
    case TSE_SCENE_COLLECTION_BASE:
    case TSE_VIEW_COLLECTION_BASE:
      return TRAVERSE_CONTINUE;
    default:
      return TRAVERSE_SKIP_CHILDS;
  }
}

static LayerCollection *outliner_active_layer_collection(bContext *C)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  CollectionObjectsSelectData data{};

  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         outliner_find_first_selected_layer_collection,
                         &data);
  return data.layer_collection;
}

static int collection_objects_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  bool deselect = STREQ(op->idname, "OUTLINER_OT_collection_objects_deselect");

  IDsSelectedData selected_collections{};
  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         outliner_collect_selected_collections,
                         &selected_collections);

  if (selected_collections.selected_array.first == nullptr) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (LinkData *, link, &selected_collections.selected_array) {
    TreeElement *te = static_cast<TreeElement *>(link->data);
    if (te->store_elem->type == TSE_LAYER_COLLECTION) {
      LayerCollection *layer_collection = static_cast<LayerCollection *>(te->directdata);
      BKE_layer_collection_objects_select(scene, view_layer, layer_collection, deselect);
    }
  }

  BLI_freelistN(&selected_collections.selected_array);
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_objects_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Objects";
  ot->idname = "OUTLINER_OT_collection_objects_select";
  ot->description = "Select objects in collection";

  /* api callbacks */
  ot->exec = collection_objects_select_exec;
  ot->poll = ED_outliner_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_objects_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Objects";
  ot->idname = "OUTLINER_OT_collection_objects_deselect";
  ot->description = "Deselect objects in collection";

  /* api callbacks */
  ot->exec = collection_objects_select_exec;
  ot->poll = ED_outliner_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Collection
 * \{ */

struct CollectionDuplicateData {
  TreeElement *te;
};

static TreeTraversalAction outliner_find_first_selected_collection(TreeElement *te,
                                                                   void *customdata)
{
  CollectionDuplicateData *data = static_cast<CollectionDuplicateData *>(customdata);
  TreeStoreElem *tselem = TREESTORE(te);

  switch (tselem->type) {
    case TSE_LAYER_COLLECTION:
      data->te = te;
      return TRAVERSE_BREAK;
    case TSE_R_LAYER:
    case TSE_SCENE_COLLECTION_BASE:
    case TSE_VIEW_COLLECTION_BASE:
    default:
      return TRAVERSE_CONTINUE;
  }
}

static TreeElement *outliner_active_collection(bContext *C)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  CollectionDuplicateData data = {};

  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         outliner_find_first_selected_collection,
                         &data);
  return data.te;
}

static int collection_duplicate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  TreeElement *te = outliner_active_collection(C);
  const bool linked = strstr(op->idname, "linked") != nullptr;

  /* Can happen when calling from a key binding. */
  if (te == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active collection");
    return OPERATOR_CANCELLED;
  }

  Collection *collection = outliner_collection_from_tree_element(te);
  Collection *parent = (te->parent) ? outliner_collection_from_tree_element(te->parent) : nullptr;

  /* We are allowed to duplicated linked collections (they will become local IDs then),
   * but we should not allow its parent to be a linked ID, ever.
   * This can happen when a whole scene is linked e.g. */
  if (parent != nullptr && (ID_IS_LINKED(parent) || ID_IS_OVERRIDE_LIBRARY(parent))) {
    Scene *scene = CTX_data_scene(C);
    parent = (ID_IS_LINKED(scene) || ID_IS_OVERRIDE_LIBRARY(scene)) ? nullptr :
                                                                      scene->master_collection;
  }
  else if (parent != nullptr && (parent->flag & COLLECTION_IS_MASTER) != 0) {
    BLI_assert(parent->id.flag & LIB_EMBEDDED_DATA);

    Scene *scene_owner = reinterpret_cast<Scene *>(BKE_id_owner_get(&parent->id));
    BLI_assert(scene_owner != nullptr);
    BLI_assert(GS(scene_owner->id.name) == ID_SCE);

    if (ID_IS_LINKED(scene_owner) || ID_IS_OVERRIDE_LIBRARY(scene_owner)) {
      scene_owner = CTX_data_scene(C);
      parent = (ID_IS_LINKED(scene_owner) || ID_IS_OVERRIDE_LIBRARY(scene_owner)) ?
                   nullptr :
                   scene_owner->master_collection;
    }
  }

  if (collection->flag & COLLECTION_IS_MASTER) {
    BKE_report(op->reports, RPT_ERROR, "Can't duplicate the master collection");
    return OPERATOR_CANCELLED;
  }

  if (parent == nullptr) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Could not find a valid parent collection for the new duplicate, "
               "it won't be linked to any view layer");
  }

  const eDupli_ID_Flags dupli_flags = (eDupli_ID_Flags)(USER_DUP_OBJECT |
                                                        (linked ? 0 : U.dupflag));
  BKE_collection_duplicate(bmain, parent, collection, dupli_flags, LIB_ID_DUPLICATE_IS_ROOT_ID);

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_LAYER, CTX_data_scene(C));
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_duplicate_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Linked Collection";
  ot->idname = "OUTLINER_OT_collection_duplicate_linked";
  ot->description =
      "Recursively duplicate the collection, all its children and objects, with linked object "
      "data";

  /* api callbacks */
  ot->exec = collection_duplicate_exec;
  ot->poll = ED_outliner_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Collection";
  ot->idname = "OUTLINER_OT_collection_duplicate";
  ot->description =
      "Recursively duplicate the collection, all its children, objects and object data";

  /* api callbacks */
  ot->exec = collection_duplicate_exec;
  ot->poll = ED_outliner_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Link Collection
 * \{ */

static int collection_link_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Collection *active_collection = CTX_data_layer_collection(C)->collection;
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  CollectionEditData data{};
  data.scene = scene;
  data.space_outliner = space_outliner;
  data.is_liboverride_allowed = false; /* No linking of non-root collections. */
  data.is_liboverride_hierarchy_root_allowed = true;

  if ((ID_IS_LINKED(active_collection) || ID_IS_OVERRIDE_LIBRARY(active_collection)) ||
      ((active_collection->flag & COLLECTION_IS_MASTER) &&
       (ID_IS_LINKED(scene) || ID_IS_OVERRIDE_LIBRARY(scene))))
  {
    BKE_report(
        op->reports, RPT_ERROR, "Cannot add a collection to a linked/override collection/scene");
    return OPERATOR_CANCELLED;
  }

  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  /* We first walk over and find the Collections we actually want to link (ignoring duplicates). */
  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         collection_collect_data_to_edit,
                         &data);

  /* Effectively link the collections. */
  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    Collection *collection = static_cast<Collection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));
    BKE_collection_child_add(bmain, active_collection, collection);
    id_fake_user_clear(&collection->id);
  }

  BLI_gset_free(data.collections_to_edit, nullptr);

  DEG_id_tag_update(&active_collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_link(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Link Collection";
  ot->idname = "OUTLINER_OT_collection_link";
  ot->description = "Link selected collections to active scene";

  /* api callbacks */
  ot->exec = collection_link_exec;
  ot->poll = collection_edit_in_active_scene_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Instance Collection
 * \{ */

static int collection_instance_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  CollectionEditData data{};
  data.scene = scene;
  data.space_outliner = space_outliner;
  data.is_liboverride_allowed = false; /* No instancing of non-root collections. */
  data.is_liboverride_hierarchy_root_allowed = true;

  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  /* We first walk over and find the Collections we actually want to instance
   * (ignoring duplicates). */
  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         collection_collect_data_to_edit,
                         &data);

  /* Find an active collection to add to, that doesn't give dependency cycles. */
  LayerCollection *active_lc = BKE_layer_collection_get_active(view_layer);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    Collection *collection = static_cast<Collection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));

    while (BKE_collection_cycle_find(active_lc->collection, collection)) {
      active_lc = BKE_layer_collection_activate_parent(view_layer, active_lc);
    }
  }

  /* Effectively instance the collections. */
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    Collection *collection = static_cast<Collection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));
    Object *ob = ED_object_add_type(
        C, OB_EMPTY, collection->id.name + 2, scene->cursor.location, nullptr, false, 0);
    ob->instance_collection = collection;
    ob->transflag |= OB_DUPLICOLLECTION;
    id_us_plus(&collection->id);
  }

  BLI_gset_free(data.collections_to_edit, nullptr);

  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_instance(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Instance Collection";
  ot->idname = "OUTLINER_OT_collection_instance";
  ot->description = "Instance selected collections to active scene";

  /* api callbacks */
  ot->exec = collection_instance_exec;
  ot->poll = collection_edit_in_active_scene_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exclude Collection
 * \{ */

static TreeTraversalAction layer_collection_collect_data_to_edit(TreeElement *te, void *customdata)
{
  CollectionEditData *data = static_cast<CollectionEditData *>(customdata);
  TreeStoreElem *tselem = TREESTORE(te);

  if (!(tselem && tselem->type == TSE_LAYER_COLLECTION)) {
    return TRAVERSE_CONTINUE;
  }

  LayerCollection *lc = static_cast<LayerCollection *>(te->directdata);

  if (lc->collection->flag & COLLECTION_IS_MASTER) {
    /* skip - showing warning/error message might be misleading
     * when deleting multiple collections, so just do nothing */
  }
  else {
    /* Delete, duplicate and link don't edit children, those will come along
     * with the parents. */
    BLI_gset_add(data->collections_to_edit, lc);
  }

  return TRAVERSE_CONTINUE;
}

static bool collections_view_layer_poll(bContext *C, bool clear, int flag)
{
  /* Poll function so the right click menu show current state of selected collections. */
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  if (!(space_outliner && space_outliner->outlinevis == SO_VIEW_LAYER)) {
    return false;
  }

  Scene *scene = CTX_data_scene(C);
  CollectionEditData data{};
  data.scene = scene;
  data.space_outliner = space_outliner;
  data.is_liboverride_allowed = true;
  data.is_liboverride_hierarchy_root_allowed = true;
  data.collections_to_edit = BLI_gset_ptr_new(__func__);
  bool result = false;

  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         layer_collection_collect_data_to_edit,
                         &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *lc = static_cast<LayerCollection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));

    if (clear && (lc->flag & flag)) {
      result = true;
    }
    else if (!clear && !(lc->flag & flag)) {
      result = true;
    }
  }

  BLI_gset_free(data.collections_to_edit, nullptr);
  return result;
}

static bool collections_exclude_set_poll(bContext *C)
{
  return collections_view_layer_poll(C, false, LAYER_COLLECTION_EXCLUDE);
}

static bool collections_exclude_clear_poll(bContext *C)
{
  return collections_view_layer_poll(C, true, LAYER_COLLECTION_EXCLUDE);
}

static bool collections_holdout_set_poll(bContext *C)
{
  return collections_view_layer_poll(C, false, LAYER_COLLECTION_HOLDOUT);
}

static bool collections_holdout_clear_poll(bContext *C)
{
  return collections_view_layer_poll(C, true, LAYER_COLLECTION_HOLDOUT);
}

static bool collections_indirect_only_set_poll(bContext *C)
{
  return collections_view_layer_poll(C, false, LAYER_COLLECTION_INDIRECT_ONLY);
}

static bool collections_indirect_only_clear_poll(bContext *C)
{
  return collections_view_layer_poll(C, true, LAYER_COLLECTION_INDIRECT_ONLY);
}

static int collection_view_layer_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  CollectionEditData data{};
  data.scene = scene;
  data.space_outliner = space_outliner;
  data.is_liboverride_allowed = true;
  data.is_liboverride_hierarchy_root_allowed = true;
  bool clear = strstr(op->idname, "clear") != nullptr;
  int flag = strstr(op->idname, "holdout")       ? LAYER_COLLECTION_HOLDOUT :
             strstr(op->idname, "indirect_only") ? LAYER_COLLECTION_INDIRECT_ONLY :
                                                   LAYER_COLLECTION_EXCLUDE;

  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         layer_collection_collect_data_to_edit,
                         &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *lc = static_cast<LayerCollection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));
    BKE_layer_collection_set_flag(lc, flag, !clear);
  }

  BLI_gset_free(data.collections_to_edit, nullptr);

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_exclude_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Disable from View Layer";
  ot->idname = "OUTLINER_OT_collection_exclude_set";
  ot->description = "Exclude collection from the active view layer";

  /* api callbacks */
  ot->exec = collection_view_layer_exec;
  ot->poll = collections_exclude_set_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_exclude_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Enable in View Layer";
  ot->idname = "OUTLINER_OT_collection_exclude_clear";
  ot->description = "Include collection in the active view layer";

  /* api callbacks */
  ot->exec = collection_view_layer_exec;
  ot->poll = collections_exclude_clear_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_holdout_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Holdout";
  ot->idname = "OUTLINER_OT_collection_holdout_set";
  ot->description = "Mask collection in the active view layer";

  /* api callbacks */
  ot->exec = collection_view_layer_exec;
  ot->poll = collections_holdout_set_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_holdout_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Holdout";
  ot->idname = "OUTLINER_OT_collection_holdout_clear";
  ot->description = "Clear masking of collection in the active view layer";

  /* api callbacks */
  ot->exec = collection_view_layer_exec;
  ot->poll = collections_holdout_clear_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_indirect_only_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Indirect Only";
  ot->idname = "OUTLINER_OT_collection_indirect_only_set";
  ot->description =
      "Set collection to only contribute indirectly (through shadows and reflections) in the view "
      "layer";

  /* api callbacks */
  ot->exec = collection_view_layer_exec;
  ot->poll = collections_indirect_only_set_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_indirect_only_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Indirect Only";
  ot->idname = "OUTLINER_OT_collection_indirect_only_clear";
  ot->description = "Clear collection contributing only indirectly in the view layer";

  /* api callbacks */
  ot->exec = collection_view_layer_exec;
  ot->poll = collections_indirect_only_clear_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visibility for Collection Operators
 * \{ */

static int collection_isolate_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  CollectionEditData data{};
  data.scene = scene;
  data.space_outliner = space_outliner;
  data.is_liboverride_allowed = true;
  data.is_liboverride_hierarchy_root_allowed = true;
  data.collections_to_edit = BLI_gset_ptr_new(__func__);
  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         layer_collection_collect_data_to_edit,
                         &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *layer_collection = static_cast<LayerCollection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));

    if (extend) {
      BKE_layer_collection_isolate_global(scene, view_layer, layer_collection, true);
    }
    else {
      PropertyRNA *prop = RNA_struct_type_find_property(&RNA_LayerCollection, "hide_viewport");
      PointerRNA ptr = RNA_pointer_create(&scene->id, &RNA_LayerCollection, layer_collection);

      /* We need to flip the value because the isolate flag routine was designed to work from the
       * outliner as a callback. That means the collection visibility was set before the callback
       * was called. */
      const bool value = !RNA_property_boolean_get(&ptr, prop);
      outliner_collection_isolate_flag(
          scene, view_layer, layer_collection, nullptr, prop, "hide_viewport", value);
      break;
    }
  }
  BLI_gset_free(data.collections_to_edit, nullptr);

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, nullptr);
  return OPERATOR_FINISHED;
}

static int collection_isolate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "extend");
  if (!RNA_property_is_set(op->ptr, prop) && (event->modifier & KM_SHIFT)) {
    RNA_property_boolean_set(op->ptr, prop, true);
  }
  return collection_isolate_exec(C, op);
}

void OUTLINER_OT_collection_isolate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Isolate Collection";
  ot->idname = "OUTLINER_OT_collection_isolate";
  ot->description = "Hide all but this collection and its parents";

  /* api callbacks */
  ot->exec = collection_isolate_exec;
  ot->invoke = collection_isolate_invoke;
  ot->poll = ED_outliner_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "extend", false, "Extend", "Extend current visible collections");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static bool collection_show_poll(bContext *C)
{
  return collections_view_layer_poll(C, true, LAYER_COLLECTION_HIDE);
}

static bool collection_hide_poll(bContext *C)
{
  return collections_view_layer_poll(C, false, LAYER_COLLECTION_HIDE);
}

static bool collection_inside_poll(bContext *C)
{
  if (!ED_outliner_collections_editor_poll(C)) {
    return false;
  }
  return outliner_active_layer_collection(C) != nullptr;
}

static int collection_visibility_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  const bool is_inside = strstr(op->idname, "inside") != nullptr;
  const bool show = strstr(op->idname, "show") != nullptr;
  CollectionEditData data{};
  data.scene = scene;
  data.space_outliner = space_outliner;
  data.is_liboverride_allowed = true;
  data.is_liboverride_hierarchy_root_allowed = true;
  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         layer_collection_collect_data_to_edit,
                         &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *layer_collection = static_cast<LayerCollection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));
    BKE_layer_collection_set_visible(scene, view_layer, layer_collection, show, is_inside);
  }
  BLI_gset_free(data.collections_to_edit, nullptr);

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, nullptr);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_show(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Collection";
  ot->idname = "OUTLINER_OT_collection_show";
  ot->description = "Show the collection in this view layer";

  /* api callbacks */
  ot->exec = collection_visibility_exec;
  ot->poll = collection_show_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Collection";
  ot->idname = "OUTLINER_OT_collection_hide";
  ot->description = "Hide the collection in this view layer";

  /* api callbacks */
  ot->exec = collection_visibility_exec;
  ot->poll = collection_hide_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_show_inside(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Inside Collection";
  ot->idname = "OUTLINER_OT_collection_show_inside";
  ot->description = "Show all the objects and collections inside the collection";

  /* api callbacks */
  ot->exec = collection_visibility_exec;
  ot->poll = collection_inside_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_hide_inside(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Inside Collection";
  ot->idname = "OUTLINER_OT_collection_hide_inside";
  ot->description = "Hide all the objects and collections inside the collection";

  /* api callbacks */
  ot->exec = collection_visibility_exec;
  ot->poll = collection_inside_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enable/Disable Collection Operators
 * \{ */

static bool collection_flag_poll(bContext *C, bool clear, int flag)
{
  if (!ED_outliner_collections_editor_poll(C)) {
    return false;
  }

  TreeElement *te = outliner_active_collection(C);
  if (te == nullptr) {
    return false;
  }

  Collection *collection = outliner_collection_from_tree_element(te);
  if (collection == nullptr) {
    return false;
  }

  if (clear && (collection->flag & flag)) {
    return true;
  }
  if (!clear && !(collection->flag & flag)) {
    return true;
  }

  return false;
}

static bool collection_enable_poll(bContext *C)
{
  return collection_flag_poll(C, true, COLLECTION_HIDE_VIEWPORT);
}

static bool collection_disable_poll(bContext *C)
{
  return collection_flag_poll(C, false, COLLECTION_HIDE_VIEWPORT);
}

static bool collection_enable_render_poll(bContext *C)
{
  return collection_flag_poll(C, true, COLLECTION_HIDE_RENDER);
}

static bool collection_disable_render_poll(bContext *C)
{
  return collection_flag_poll(C, false, COLLECTION_HIDE_RENDER);
}

static int collection_flag_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  const bool is_render = strstr(op->idname, "render");
  const bool clear = strstr(op->idname, "show") || strstr(op->idname, "enable");
  int flag = is_render ? COLLECTION_HIDE_RENDER : COLLECTION_HIDE_VIEWPORT;
  CollectionEditData data{};
  data.scene = scene;
  data.space_outliner = space_outliner;
  data.is_liboverride_allowed = true;
  data.is_liboverride_hierarchy_root_allowed = true;
  data.collections_to_edit = BLI_gset_ptr_new(__func__);
  const bool has_layer_collection = space_outliner->outlinevis == SO_VIEW_LAYER;

  if (has_layer_collection) {
    outliner_tree_traverse(space_outliner,
                           &space_outliner->tree,
                           0,
                           TSE_SELECTED,
                           layer_collection_collect_data_to_edit,
                           &data);
    GSetIterator collections_to_edit_iter;
    GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
      LayerCollection *layer_collection = static_cast<LayerCollection *>(
          BLI_gsetIterator_getKey(&collections_to_edit_iter));
      Collection *collection = layer_collection->collection;
      if (!BKE_id_is_editable(bmain, &collection->id)) {
        continue;
      }
      if (clear) {
        collection->flag &= ~flag;
      }
      else {
        collection->flag |= flag;
      }

      /* Make sure (at least for this view layer) the collection is visible. */
      if (clear && !is_render) {
        layer_collection->flag &= ~LAYER_COLLECTION_HIDE;
      }
    }
    BLI_gset_free(data.collections_to_edit, nullptr);
  }
  else {
    outliner_tree_traverse(space_outliner,
                           &space_outliner->tree,
                           0,
                           TSE_SELECTED,
                           collection_collect_data_to_edit,
                           &data);
    GSetIterator collections_to_edit_iter;
    GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
      Collection *collection = static_cast<Collection *>(
          BLI_gsetIterator_getKey(&collections_to_edit_iter));
      if (!BKE_id_is_editable(bmain, &collection->id)) {
        continue;
      }

      if (clear) {
        collection->flag &= ~flag;
      }
      else {
        collection->flag |= flag;
      }
    }
    BLI_gset_free(data.collections_to_edit, nullptr);
  }

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  if (!is_render) {
    DEG_relations_tag_update(CTX_data_main(C));
  }

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, nullptr);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_enable(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Enable Collection";
  ot->idname = "OUTLINER_OT_collection_enable";
  ot->description = "Enable viewport display in the view layers";

  /* api callbacks */
  ot->exec = collection_flag_exec;
  ot->poll = collection_enable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_disable(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Disable Collection";
  ot->idname = "OUTLINER_OT_collection_disable";
  ot->description = "Disable viewport display in the view layers";

  /* api callbacks */
  ot->exec = collection_flag_exec;
  ot->poll = collection_disable_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_enable_render(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Enable Collection in Render";
  ot->idname = "OUTLINER_OT_collection_enable_render";
  ot->description = "Render the collection";

  /* api callbacks */
  ot->exec = collection_flag_exec;
  ot->poll = collection_enable_render_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OUTLINER_OT_collection_disable_render(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Disable Collection in Render";
  ot->idname = "OUTLINER_OT_collection_disable_render";
  ot->description = "Do not render this collection";

  /* api callbacks */
  ot->exec = collection_flag_exec;
  ot->poll = collection_disable_render_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

struct OutlinerHideEditData {
  Scene *scene;
  ViewLayer *view_layer;
  SpaceOutliner *space_outliner;
  GSet *collections_to_edit;
  GSet *bases_to_edit;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visibility for Collection & Object Operators
 * \{ */

static TreeTraversalAction outliner_hide_collect_data_to_edit(TreeElement *te, void *customdata)
{
  OutlinerHideEditData *data = static_cast<OutlinerHideEditData *>(customdata);
  TreeStoreElem *tselem = TREESTORE(te);

  if (tselem == nullptr) {
    return TRAVERSE_CONTINUE;
  }

  if (tselem->type == TSE_LAYER_COLLECTION) {
    LayerCollection *lc = static_cast<LayerCollection *>(te->directdata);

    if (lc->collection->flag & COLLECTION_IS_MASTER) {
      /* Skip - showing warning/error message might be misleading
       * when deleting multiple collections, so just do nothing. */
    }
    else {
      /* Delete, duplicate and link don't edit children,
       * those will come along with the parents. */
      BLI_gset_add(data->collections_to_edit, lc);
    }
  }
  else if ((tselem->type == TSE_SOME_ID) && (te->idcode == ID_OB)) {
    Object *ob = (Object *)tselem->id;
    BKE_view_layer_synced_ensure(data->scene, data->view_layer);
    Base *base = BKE_view_layer_base_find(data->view_layer, ob);
    BLI_gset_add(data->bases_to_edit, base);
  }

  return TRAVERSE_CONTINUE;
}

static int outliner_hide_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  OutlinerHideEditData data{};
  data.scene = scene;
  data.view_layer = view_layer;
  data.space_outliner = space_outliner;
  data.collections_to_edit = BLI_gset_ptr_new("outliner_hide_exec__collections_to_edit");
  data.bases_to_edit = BLI_gset_ptr_new("outliner_hide_exec__bases_to_edit");

  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         outliner_hide_collect_data_to_edit,
                         &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *layer_collection = static_cast<LayerCollection *>(
        BLI_gsetIterator_getKey(&collections_to_edit_iter));
    BKE_layer_collection_set_visible(scene, view_layer, layer_collection, false, false);
  }
  BLI_gset_free(data.collections_to_edit, nullptr);

  GSetIterator bases_to_edit_iter;
  GSET_ITER (bases_to_edit_iter, data.bases_to_edit) {
    Base *base = static_cast<Base *>(BLI_gsetIterator_getKey(&bases_to_edit_iter));
    base->flag |= BASE_HIDDEN;
  }
  BLI_gset_free(data.bases_to_edit, nullptr);

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, nullptr);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide";
  ot->idname = "OUTLINER_OT_hide";
  ot->description = "Hide selected objects and collections";

  /* api callbacks */
  ot->exec = outliner_hide_exec;
  ot->poll = outliner_view_layer_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int outliner_unhide_all_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* Unhide all the collections. */
  LayerCollection *lc_master = static_cast<LayerCollection *>(view_layer->layer_collections.first);
  LISTBASE_FOREACH (LayerCollection *, lc_iter, &lc_master->layer_collections) {
    BKE_layer_collection_set_flag(lc_iter, LAYER_COLLECTION_HIDE, false);
  }

  /* Unhide all objects. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    base->flag &= ~BASE_HIDDEN;
  }

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, nullptr);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_unhide_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unhide All";
  ot->idname = "OUTLINER_OT_unhide_all";
  ot->description = "Unhide all objects and collections";

  /* api callbacks */
  ot->exec = outliner_unhide_all_exec;
  ot->poll = outliner_view_layer_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Color Tags
 * \{ */

static int outliner_color_tag_set_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);
  const short color_tag = RNA_enum_get(op->ptr, "color");

  IDsSelectedData selected{};

  outliner_tree_traverse(space_outliner,
                         &space_outliner->tree,
                         0,
                         TSE_SELECTED,
                         outliner_collect_selected_collections,
                         &selected);

  LISTBASE_FOREACH (LinkData *, link, &selected.selected_array) {
    TreeElement *te_selected = (TreeElement *)link->data;

    Collection *collection = outliner_collection_from_tree_element(te_selected);
    if (collection == scene->master_collection) {
      continue;
    }
    if (!BKE_id_is_editable(CTX_data_main(C), &collection->id)) {
      BKE_report(op->reports, RPT_WARNING, "Can't add a color tag to a linked collection");
      continue;
    }

    collection->color_tag = color_tag;
  };

  BLI_freelistN(&selected.selected_array);

  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, nullptr);

  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_color_tag_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Color Tag";
  ot->idname = "OUTLINER_OT_collection_color_tag_set";
  ot->description = "Set a color tag for the selected collections";

  /* api callbacks */
  ot->exec = outliner_color_tag_set_exec;
  ot->poll = ED_outliner_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(
      ot->srna, "color", rna_enum_collection_color_items, COLLECTION_COLOR_NONE, "Color Tag", "");
}

/** \} */

}  // namespace blender::ed::outliner
