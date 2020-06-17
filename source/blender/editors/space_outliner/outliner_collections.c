/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup spoutliner
 */

#include <string.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"

#include "outliner_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Utility API
 * \{ */

bool outliner_is_collection_tree_element(const TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (!tselem) {
    return false;
  }

  if (ELEM(tselem->type,
           TSE_LAYER_COLLECTION,
           TSE_SCENE_COLLECTION_BASE,
           TSE_VIEW_COLLECTION_BASE)) {
    return true;
  }
  else if (tselem->type == 0 && te->idcode == ID_GR) {
    return true;
  }

  return false;
}

Collection *outliner_collection_from_tree_element(const TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if (!tselem) {
    return NULL;
  }

  if (tselem->type == TSE_LAYER_COLLECTION) {
    LayerCollection *lc = te->directdata;
    return lc->collection;
  }
  else if (ELEM(tselem->type, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE)) {
    Scene *scene = (Scene *)tselem->id;
    return scene->master_collection;
  }
  else if (tselem->type == 0 && te->idcode == ID_GR) {
    return (Collection *)tselem->id;
  }

  return NULL;
}

TreeTraversalAction outliner_find_selected_collections(TreeElement *te, void *customdata)
{
  struct IDsSelectedData *data = customdata;
  TreeStoreElem *tselem = TREESTORE(te);

  if (outliner_is_collection_tree_element(te)) {
    BLI_addtail(&data->selected_array, BLI_genericNodeN(te));
    return TRAVERSE_CONTINUE;
  }

  if (tselem->type || (tselem->id && GS(tselem->id->name) != ID_GR)) {
    return TRAVERSE_SKIP_CHILDS;
  }

  return TRAVERSE_CONTINUE;
}

TreeTraversalAction outliner_find_selected_objects(TreeElement *te, void *customdata)
{
  struct IDsSelectedData *data = customdata;
  TreeStoreElem *tselem = TREESTORE(te);

  if (outliner_is_collection_tree_element(te)) {
    return TRAVERSE_CONTINUE;
  }

  if (tselem->type || (tselem->id == NULL) || (GS(tselem->id->name) != ID_OB)) {
    return TRAVERSE_SKIP_CHILDS;
  }

  BLI_addtail(&data->selected_array, BLI_genericNodeN(te));

  return TRAVERSE_CONTINUE;
}

/**
 * Populates the \param objects: ListBase with all the outliner selected objects
 * We store it as (Object *)LinkData->data
 * \param objects: expected to be empty
 */
void ED_outliner_selected_objects_get(const bContext *C, ListBase *objects)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  struct IDsSelectedData data = {{NULL}};
  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, outliner_find_selected_objects, &data);
  LISTBASE_FOREACH (LinkData *, link, &data.selected_array) {
    TreeElement *ten_selected = (TreeElement *)link->data;
    Object *ob = (Object *)TREESTORE(ten_selected)->id;
    BLI_addtail(objects, BLI_genericNodeN(ob));
  }
  BLI_freelistN(&data.selected_array);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool ED_outliner_collections_editor_poll(bContext *C)
{
  SpaceOutliner *so = CTX_wm_space_outliner(C);
  return (so != NULL) && ELEM(so->outlinevis, SO_VIEW_LAYER, SO_SCENES, SO_LIBRARIES);
}

static bool outliner_view_layer_collections_editor_poll(bContext *C)
{
  SpaceOutliner *so = CTX_wm_space_outliner(C);
  return (so != NULL) && (so->outlinevis == SO_VIEW_LAYER);
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
  struct CollectionNewData *data = customdata;
  Collection *collection = outliner_collection_from_tree_element(te);

  if (!collection) {
    return TRAVERSE_SKIP_CHILDS;
  }

  if (data->collection != NULL) {
    data->error = true;
    return TRAVERSE_BREAK;
  }

  data->collection = collection;
  return TRAVERSE_CONTINUE;
}

static int collection_new_exec(bContext *C, wmOperator *op)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  ARegion *region = CTX_wm_region(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  struct CollectionNewData data = {
      .error = false,
      .collection = NULL,
  };

  if (RNA_boolean_get(op->ptr, "nested")) {
    outliner_build_tree(bmain, scene, view_layer, soops, region);

    outliner_tree_traverse(
        soops, &soops->tree, 0, TSE_SELECTED, collection_find_selected_to_add, &data);

    if (data.error) {
      BKE_report(op->reports, RPT_ERROR, "More than one collection is selected");
      return OPERATOR_CANCELLED;
    }
  }

  if (data.collection == NULL || ID_IS_LINKED(data.collection)) {
    data.collection = scene->master_collection;
  }

  if (ID_IS_LINKED(scene)) {
    BKE_report(op->reports, RPT_ERROR, "Can't add a new collection to linked scene/collection");
    return OPERATOR_CANCELLED;
  }

  BKE_collection_add(bmain, data.collection, NULL);

  DEG_id_tag_update(&data.collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);

  outliner_cleanup_tree(soops);
  WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);
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
  ot->poll = ED_outliner_collections_editor_poll;

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
  SpaceOutliner *soops;
  GSet *collections_to_edit;
};

static TreeTraversalAction collection_find_data_to_edit(TreeElement *te, void *customdata)
{
  struct CollectionEditData *data = customdata;
  Collection *collection = outliner_collection_from_tree_element(te);

  if (!collection) {
    return TRAVERSE_SKIP_CHILDS;
  }

  if (collection->flag & COLLECTION_IS_MASTER) {
    /* skip - showing warning/error message might be misleading
     * when deleting multiple collections, so just do nothing */
  }
  else {
    /* Delete, duplicate and link don't edit children, those will come along
     * with the parents. */
    BLI_gset_add(data->collections_to_edit, collection);
    return TRAVERSE_SKIP_CHILDS;
  }

  return TRAVERSE_CONTINUE;
}

void outliner_collection_delete(
    bContext *C, Main *bmain, Scene *scene, ReportList *reports, bool hierarchy)
{
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

  struct CollectionEditData data = {
      .scene = scene,
      .soops = soops,
  };

  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  /* We first walk over and find the Collections we actually want to delete
   * (ignoring duplicates). */
  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_edit, &data);

  /* Effectively delete the collections. */
  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);

    /* Test in case collection got deleted as part of another one. */
    if (BLI_findindex(&bmain->collections, collection) != -1) {
      /* We cannot allow to delete collections that are indirectly linked,
       * or that are used by (linked to...) other linked scene/collection. */
      bool skip = false;
      if (ID_IS_LINKED(collection)) {
        if (collection->id.tag & LIB_TAG_INDIRECT) {
          skip = true;
        }
        else {
          LISTBASE_FOREACH (CollectionParent *, cparent, &collection->parents) {
            Collection *parent = cparent->collection;
            if (ID_IS_LINKED(parent)) {
              skip = true;
              break;
            }
            else if (parent->flag & COLLECTION_IS_MASTER) {
              Scene *parent_scene = BKE_collection_master_scene_search(bmain, parent);
              if (ID_IS_LINKED(parent_scene)) {
                skip = true;
                break;
              }
            }
          }
        }
      }

      if (!skip) {
        BKE_collection_delete(bmain, collection, hierarchy);
      }
      else {
        BKE_reportf(
            reports,
            RPT_WARNING,
            "Cannot delete linked collection '%s', it is used by other linked scenes/collections",
            collection->id.name + 2);
      }
    }
  }

  BLI_gset_free(data.collections_to_edit, NULL);
}

static int collection_hierarchy_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  const Base *basact_prev = BASACT(view_layer);

  outliner_collection_delete(C, bmain, scene, op->reports, true);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

  if (basact_prev != BASACT(view_layer)) {
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
  ot->poll = ED_outliner_collections_editor_poll;

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
  struct CollectionObjectsSelectData *data = customdata;
  TreeStoreElem *tselem = TREESTORE(te);

  switch (tselem->type) {
    case TSE_LAYER_COLLECTION:
      data->layer_collection = te->directdata;
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
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

  struct CollectionObjectsSelectData data = {
      .layer_collection = NULL,
  };

  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, outliner_find_first_selected_layer_collection, &data);
  return data.layer_collection;
}

static int collection_objects_select_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LayerCollection *layer_collection = outliner_active_layer_collection(C);
  bool deselect = STREQ(op->idname, "OUTLINER_OT_collection_objects_deselect");

  if (layer_collection == NULL) {
    return OPERATOR_CANCELLED;
  }

  BKE_layer_collection_objects_select(view_layer, layer_collection, deselect);

  Scene *scene = CTX_data_scene(C);
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
  struct CollectionDuplicateData *data = customdata;
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
  SpaceOutliner *soops = CTX_wm_space_outliner(C);

  struct CollectionDuplicateData data = {
      .te = NULL,
  };

  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, outliner_find_first_selected_collection, &data);
  return data.te;
}

static int collection_duplicate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  TreeElement *te = outliner_active_collection(C);
  const bool linked = strstr(op->idname, "linked") != NULL;

  /* Can happen when calling from a key binding. */
  if (te == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active collection");
    return OPERATOR_CANCELLED;
  }

  Collection *collection = outliner_collection_from_tree_element(te);
  Collection *parent = (te->parent) ? outliner_collection_from_tree_element(te->parent) : NULL;

  /* We are allowed to duplicated linked collections (they will become local IDs then),
   * but we should not allow its parent to be a linked ID, ever.
   * This can happen when a whole scene is linked e.g. */
  if (parent != NULL && ID_IS_LINKED(parent)) {
    Scene *scene = CTX_data_scene(C);
    parent = ID_IS_LINKED(scene) ? NULL : scene->master_collection;
  }
  else if (parent != NULL && (parent->flag & COLLECTION_IS_MASTER) != 0) {
    Scene *scene = BKE_collection_master_scene_search(bmain, parent);
    BLI_assert(scene != NULL);
    if (ID_IS_LINKED(scene)) {
      scene = CTX_data_scene(C);
      parent = ID_IS_LINKED(scene) ? NULL : scene->master_collection;
    }
  }

  if (collection->flag & COLLECTION_IS_MASTER) {
    BKE_report(op->reports, RPT_ERROR, "Can't duplicate the master collection");
    return OPERATOR_CANCELLED;
  }

  if (parent == NULL) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Could not find a valid parent collection for the new duplicate, "
               "it won't be linked to any view layer");
  }

  const eDupli_ID_Flags dupli_flags = USER_DUP_OBJECT | (linked ? 0 : U.dupflag);
  BKE_collection_duplicate(bmain, parent, collection, dupli_flags, 0);

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
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  struct CollectionEditData data = {
      .scene = scene,
      .soops = soops,
  };

  if (ID_IS_LINKED(active_collection) ||
      ((active_collection->flag & COLLECTION_IS_MASTER) && ID_IS_LINKED(scene))) {
    BKE_report(op->reports, RPT_ERROR, "Cannot add a collection to a linked collection/scene");
    return OPERATOR_CANCELLED;
  }

  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  /* We first walk over and find the Collections we actually want to link (ignoring duplicates). */
  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_edit, &data);

  /* Effectively link the collections. */
  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);
    BKE_collection_child_add(bmain, active_collection, collection);
    id_fake_user_clear(&collection->id);
  }

  BLI_gset_free(data.collections_to_edit, NULL);

  DEG_id_tag_update(&active_collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

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
  ot->poll = ED_outliner_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Instance Collection
 * \{ */

static int collection_instance_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  struct CollectionEditData data = {
      .scene = scene,
      .soops = soops,
  };

  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  /* We first walk over and find the Collections we actually want to instance
   * (ignoring duplicates). */
  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_edit, &data);

  /* Find an active collection to add to, that doesn't give dependency cycles. */
  LayerCollection *active_lc = BKE_layer_collection_get_active(view_layer);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);

    while (BKE_collection_find_cycle(active_lc->collection, collection)) {
      active_lc = BKE_layer_collection_activate_parent(view_layer, active_lc);
    }
  }

  /* Effectively instance the collections. */
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);
    Object *ob = ED_object_add_type(
        C, OB_EMPTY, collection->id.name + 2, scene->cursor.location, NULL, false, 0);
    ob->instance_collection = collection;
    ob->transflag |= OB_DUPLICOLLECTION;
    id_lib_extern(&collection->id);
    id_us_plus(&collection->id);
  }

  BLI_gset_free(data.collections_to_edit, NULL);

  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

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
  ot->poll = ED_outliner_collections_editor_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exclude Collection
 * \{ */

static TreeTraversalAction layer_collection_find_data_to_edit(TreeElement *te, void *customdata)
{
  struct CollectionEditData *data = customdata;
  TreeStoreElem *tselem = TREESTORE(te);

  if (!(tselem && tselem->type == TSE_LAYER_COLLECTION)) {
    return TRAVERSE_CONTINUE;
  }

  LayerCollection *lc = te->directdata;

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
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  if (!(soops && soops->outlinevis == SO_VIEW_LAYER)) {
    return false;
  }

  Scene *scene = CTX_data_scene(C);
  struct CollectionEditData data = {
      .scene = scene,
      .soops = soops,
  };
  data.collections_to_edit = BLI_gset_ptr_new(__func__);
  bool result = false;

  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, layer_collection_find_data_to_edit, &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *lc = BLI_gsetIterator_getKey(&collections_to_edit_iter);

    if (clear && (lc->flag & flag)) {
      result = true;
    }
    else if (!clear && !(lc->flag & flag)) {
      result = true;
    }
  }

  BLI_gset_free(data.collections_to_edit, NULL);
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
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  struct CollectionEditData data = {
      .scene = scene,
      .soops = soops,
  };
  bool clear = strstr(op->idname, "clear") != NULL;
  int flag = strstr(op->idname, "holdout") ?
                 LAYER_COLLECTION_HOLDOUT :
                 strstr(op->idname, "indirect_only") ? LAYER_COLLECTION_INDIRECT_ONLY :
                                                       LAYER_COLLECTION_EXCLUDE;

  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, layer_collection_find_data_to_edit, &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *lc = BLI_gsetIterator_getKey(&collections_to_edit_iter);
    BKE_layer_collection_set_flag(lc, flag, !clear);
  }

  BLI_gset_free(data.collections_to_edit, NULL);

  BKE_layer_collection_sync(scene, view_layer);
  DEG_relations_tag_update(bmain);

  WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

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
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  struct CollectionEditData data = {
      .scene = scene,
      .soops = soops,
  };
  data.collections_to_edit = BLI_gset_ptr_new(__func__);
  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, layer_collection_find_data_to_edit, &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *layer_collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);

    if (extend) {
      BKE_layer_collection_isolate_global(scene, view_layer, layer_collection, true);
    }
    else {
      PointerRNA ptr;
      PropertyRNA *prop = RNA_struct_type_find_property(&RNA_LayerCollection, "hide_viewport");
      RNA_pointer_create(&scene->id, &RNA_LayerCollection, layer_collection, &ptr);

      /* We need to flip the value because the isolate flag routine was designed to work from the
       * outliner as a callback. That means the collection visibility was set before the callback
       * was called. */
      const bool value = !RNA_property_boolean_get(&ptr, prop);
      outliner_collection_isolate_flag(
          scene, view_layer, layer_collection, NULL, prop, "hide_viewport", value);
      break;
    }
  }
  BLI_gset_free(data.collections_to_edit, NULL);

  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
  return OPERATOR_FINISHED;
}

static int collection_isolate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "extend");
  if (!RNA_property_is_set(op->ptr, prop) && (event->shift)) {
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
  return outliner_active_layer_collection(C) != NULL;
}

static int collection_visibility_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  const bool is_inside = strstr(op->idname, "inside") != NULL;
  const bool show = strstr(op->idname, "show") != NULL;
  struct CollectionEditData data = {
      .scene = scene,
      .soops = soops,
  };
  data.collections_to_edit = BLI_gset_ptr_new(__func__);

  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, layer_collection_find_data_to_edit, &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *layer_collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);
    BKE_layer_collection_set_visible(view_layer, layer_collection, show, is_inside);
  }
  BLI_gset_free(data.collections_to_edit, NULL);

  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
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
  if (te == NULL) {
    return false;
  }

  Collection *collection = outliner_collection_from_tree_element(te);
  if (collection == NULL) {
    return false;
  }

  if (clear && (collection->flag & flag)) {
    return true;
  }
  else if (!clear && !(collection->flag & flag)) {
    return true;
  }

  return false;
}

static bool collection_enable_poll(bContext *C)
{
  return collection_flag_poll(C, true, COLLECTION_RESTRICT_VIEWPORT);
}

static bool collection_disable_poll(bContext *C)
{
  return collection_flag_poll(C, false, COLLECTION_RESTRICT_VIEWPORT);
}

static bool collection_enable_render_poll(bContext *C)
{
  return collection_flag_poll(C, true, COLLECTION_RESTRICT_RENDER);
}

static bool collection_disable_render_poll(bContext *C)
{
  return collection_flag_poll(C, false, COLLECTION_RESTRICT_RENDER);
}

static int collection_flag_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  const bool is_render = strstr(op->idname, "render");
  const bool clear = strstr(op->idname, "show") || strstr(op->idname, "enable");
  int flag = is_render ? COLLECTION_RESTRICT_RENDER : COLLECTION_RESTRICT_VIEWPORT;
  struct CollectionEditData data = {
      .scene = scene,
      .soops = soops,
  };
  data.collections_to_edit = BLI_gset_ptr_new(__func__);
  const bool has_layer_collection = soops->outlinevis == SO_VIEW_LAYER;

  if (has_layer_collection) {
    outliner_tree_traverse(
        soops, &soops->tree, 0, TSE_SELECTED, layer_collection_find_data_to_edit, &data);
    GSetIterator collections_to_edit_iter;
    GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
      LayerCollection *layer_collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);
      Collection *collection = layer_collection->collection;
      if (ID_IS_LINKED(collection)) {
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
    BLI_gset_free(data.collections_to_edit, NULL);
  }
  else {
    outliner_tree_traverse(
        soops, &soops->tree, 0, TSE_SELECTED, collection_find_data_to_edit, &data);
    GSetIterator collections_to_edit_iter;
    GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
      Collection *collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);

      if (clear) {
        collection->flag &= ~flag;
      }
      else {
        collection->flag |= flag;
      }
    }
    BLI_gset_free(data.collections_to_edit, NULL);
  }

  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  if (!is_render) {
    DEG_relations_tag_update(CTX_data_main(C));
  }

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
  return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_enable(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Enable Collection";
  ot->idname = "OUTLINER_OT_collection_enable";
  ot->description = "Enable viewport drawing in the view layers";

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
  ot->description = "Disable viewport drawing in the view layers";

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
  SpaceOutliner *soops;
  GSet *collections_to_edit;
  GSet *bases_to_edit;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visibility for Collection & Object Operators
 * \{ */

static TreeTraversalAction outliner_hide_find_data_to_edit(TreeElement *te, void *customdata)
{
  struct OutlinerHideEditData *data = customdata;
  TreeStoreElem *tselem = TREESTORE(te);

  if (tselem == NULL) {
    return TRAVERSE_CONTINUE;
  }

  if (tselem->type == TSE_LAYER_COLLECTION) {
    LayerCollection *lc = te->directdata;

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
  else if (tselem->type == 0 && te->idcode == ID_OB) {
    Object *ob = (Object *)tselem->id;
    Base *base = BKE_view_layer_base_find(data->view_layer, ob);
    BLI_gset_add(data->bases_to_edit, base);
  }

  return TRAVERSE_CONTINUE;
}

static int outliner_hide_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceOutliner *soops = CTX_wm_space_outliner(C);
  struct OutlinerHideEditData data = {
      .scene = scene,
      .view_layer = view_layer,
      .soops = soops,
  };
  data.collections_to_edit = BLI_gset_ptr_new("outliner_hide_exec__collections_to_edit");
  data.bases_to_edit = BLI_gset_ptr_new("outliner_hide_exec__bases_to_edit");

  outliner_tree_traverse(
      soops, &soops->tree, 0, TSE_SELECTED, outliner_hide_find_data_to_edit, &data);

  GSetIterator collections_to_edit_iter;
  GSET_ITER (collections_to_edit_iter, data.collections_to_edit) {
    LayerCollection *layer_collection = BLI_gsetIterator_getKey(&collections_to_edit_iter);
    BKE_layer_collection_set_visible(view_layer, layer_collection, false, false);
  }
  BLI_gset_free(data.collections_to_edit, NULL);

  GSetIterator bases_to_edit_iter;
  GSET_ITER (bases_to_edit_iter, data.bases_to_edit) {
    Base *base = BLI_gsetIterator_getKey(&bases_to_edit_iter);
    base->flag |= BASE_HIDDEN;
  }
  BLI_gset_free(data.bases_to_edit, NULL);

  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
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

static int outliner_unhide_all_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* Unhide all the collections. */
  LayerCollection *lc_master = view_layer->layer_collections.first;
  LISTBASE_FOREACH (LayerCollection *, lc_iter, &lc_master->layer_collections) {
    BKE_layer_collection_set_flag(lc_iter, LAYER_COLLECTION_HIDE, false);
  }

  /* Unhide all objects. */
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    base->flag &= ~BASE_HIDDEN;
  }

  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
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
