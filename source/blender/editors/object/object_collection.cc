/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "UI_interface_icons.hh"

#include "object_intern.h"

/********************* 3d view operators ***********************/

/* can be called with C == nullptr */
static const EnumPropertyItem *collection_object_active_itemf(bContext *C,
                                                              PointerRNA * /*ptr*/,
                                                              PropertyRNA * /*prop*/,
                                                              bool *r_free)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob;
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;

  if (C == nullptr) {
    return rna_enum_dummy_NULL_items;
  }

  ob = ED_object_context(C);

  /* check that the object exists */
  if (ob) {
    Collection *collection;
    int i = 0, count = 0;

    /* if 2 or more collections, add option to add to all collections */
    collection = nullptr;
    while ((collection = BKE_collection_object_find(bmain, scene, collection, ob))) {
      count++;
    }

    if (count >= 2) {
      item_tmp.identifier = item_tmp.name = "All Collections";
      item_tmp.value = INT_MAX; /* this will give nullptr on lookup */
      RNA_enum_item_add(&item, &totitem, &item_tmp);
      RNA_enum_item_add_separator(&item, &totitem);
    }

    /* add collections */
    collection = nullptr;
    while ((collection = BKE_collection_object_find(bmain, scene, collection, ob))) {
      item_tmp.identifier = item_tmp.name = collection->id.name + 2;
      item_tmp.icon = UI_icon_color_from_collection(collection);
      item_tmp.value = i;
      RNA_enum_item_add(&item, &totitem, &item_tmp);
      i++;
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

/* get the collection back from the enum index, quite awkward and UI specific */
static Collection *collection_object_active_find_index(Main *bmain,
                                                       Scene *scene,
                                                       Object *ob,
                                                       const int collection_object_index)
{
  Collection *collection = nullptr;
  int i = 0;
  while ((collection = BKE_collection_object_find(bmain, scene, collection, ob))) {
    if (i == collection_object_index) {
      break;
    }
    i++;
  }

  return collection;
}

static int objects_add_active_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int single_collection_index = RNA_enum_get(op->ptr, "collection");
  Collection *single_collection = collection_object_active_find_index(
      bmain, scene, ob, single_collection_index);
  bool is_cycle = false;
  bool updated = false;

  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* now add all selected objects to the collection(s) */
  FOREACH_COLLECTION_BEGIN (bmain, scene, Collection *, collection) {
    if (single_collection && collection != single_collection) {
      continue;
    }
    if (!BKE_collection_has_object(collection, ob)) {
      continue;
    }

    CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
      if (BKE_collection_has_object(collection, base->object)) {
        continue;
      }

      if (!BKE_collection_object_cyclic_check(bmain, base->object, collection)) {
        BKE_collection_object_add(bmain, collection, base->object);
        DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
        updated = true;
      }
      else {
        is_cycle = true;
      }
    }
    CTX_DATA_END;
  }
  FOREACH_COLLECTION_END;

  if (is_cycle) {
    BKE_report(op->reports, RPT_WARNING, "Skipped some collections because of cycle detected");
  }

  if (!updated) {
    return OPERATOR_CANCELLED;
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_objects_add_active(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Selected to Active Collection";
  ot->description = "Add the object to an object collection that contains the active object";
  ot->idname = "COLLECTION_OT_objects_add_active";

  /* api callbacks */
  ot->exec = objects_add_active_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna,
                      "collection",
                      rna_enum_dummy_NULL_items,
                      0,
                      "Collection",
                      "The collection to add other selected objects to");
  RNA_def_enum_funcs(prop, collection_object_active_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static int objects_remove_active_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  int single_collection_index = RNA_enum_get(op->ptr, "collection");
  Collection *single_collection = collection_object_active_find_index(
      bmain, scene, ob, single_collection_index);
  bool ok = false;

  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Linking to same collection requires its own loop so we can avoid
   * looking up the active objects collections each time. */
  FOREACH_COLLECTION_BEGIN (bmain, scene, Collection *, collection) {
    if (single_collection && collection != single_collection) {
      continue;
    }

    if (BKE_collection_has_object(collection, ob)) {
      /* Remove collections from selected objects */
      CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
        BKE_collection_object_remove(bmain, collection, base->object, false);
        DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
        ok = true;
      }
      CTX_DATA_END;
    }
  }
  FOREACH_COLLECTION_END;

  if (!ok) {
    BKE_report(op->reports, RPT_ERROR, "Active object contains no collections");
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_objects_remove_active(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Remove Selected from Active Collection";
  ot->description = "Remove the object from an object collection that contains the active object";
  ot->idname = "COLLECTION_OT_objects_remove_active";

  /* api callbacks */
  ot->exec = objects_remove_active_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna,
                      "collection",
                      rna_enum_dummy_NULL_items,
                      0,
                      "Collection",
                      "The collection to remove other selected objects from");
  RNA_def_enum_funcs(prop, collection_object_active_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static int collection_objects_remove_all_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    BKE_object_groups_clear(bmain, scene, base->object);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_objects_remove_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove from All Collections";
  ot->description = "Remove selected objects from all collections";
  ot->idname = "COLLECTION_OT_objects_remove_all";

  /* api callbacks */
  ot->exec = collection_objects_remove_all_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int collection_objects_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int single_collection_index = RNA_enum_get(op->ptr, "collection");
  Collection *single_collection = collection_object_active_find_index(
      bmain, scene, ob, single_collection_index);
  bool updated = false;

  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  FOREACH_COLLECTION_BEGIN (bmain, scene, Collection *, collection) {
    if (single_collection && collection != single_collection) {
      continue;
    }
    if (!BKE_collection_has_object(collection, ob)) {
      continue;
    }

    /* now remove all selected objects from the collection */
    CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
      BKE_collection_object_remove(bmain, collection, base->object, false);
      DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
      updated = true;
    }
    CTX_DATA_END;
  }
  FOREACH_COLLECTION_END;

  if (!updated) {
    return OPERATOR_CANCELLED;
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_objects_remove(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Remove from Collection";
  ot->description = "Remove selected objects from a collection";
  ot->idname = "COLLECTION_OT_objects_remove";

  /* api callbacks */
  ot->exec = collection_objects_remove_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna,
                      "collection",
                      rna_enum_dummy_NULL_items,
                      0,
                      "Collection",
                      "The collection to remove this object from");
  RNA_def_enum_funcs(prop, collection_object_active_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static int collection_create_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  char name[MAX_ID_NAME - 2]; /* id name */

  RNA_string_get(op->ptr, "name", name);

  Collection *collection = BKE_collection_add(bmain, nullptr, name);
  id_fake_user_set(&collection->id);

  CTX_DATA_BEGIN (C, Base *, base, selected_bases) {
    BKE_collection_object_add(bmain, collection, base->object);
    DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_create(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Create New Collection";
  ot->description = "Create an object collection from selected objects";
  ot->idname = "COLLECTION_OT_create";

  /* api callbacks */
  ot->exec = collection_create_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_string(
      ot->srna, "name", "Collection", MAX_ID_NAME - 2, "Name", "Name of the new collection");
}

/****************** properties window operators *********************/

static int collection_add_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = ED_object_context(C);
  Main *bmain = CTX_data_main(C);

  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  Collection *collection = BKE_collection_add(bmain, nullptr, "Collection");
  id_fake_user_set(&collection->id);
  BKE_collection_object_add(bmain, collection, ob);

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_collection_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add to Collection";
  ot->idname = "OBJECT_OT_collection_add";
  ot->description = "Add an object to a new collection";

  /* api callbacks */
  ot->exec = collection_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int collection_link_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_context(C);
  Collection *collection = static_cast<Collection *>(
      BLI_findlink(&bmain->collections, RNA_enum_get(op->ptr, "collection")));

  if (ELEM(nullptr, ob, collection)) {
    return OPERATOR_CANCELLED;
  }

  /* Early return check, if the object is already in collection
   * we could skip all the dependency check and just consider
   * operator is finished.
   */
  if (BKE_collection_has_object(collection, ob)) {
    return OPERATOR_FINISHED;
  }

  /* Currently this should not be allowed (might be supported in the future though...). */
  if (ID_IS_OVERRIDE_LIBRARY(&collection->id)) {
    BKE_report(op->reports, RPT_ERROR, "Could not add the collection because it is overridden");
    return OPERATOR_CANCELLED;
  }
  /* Linked collections are already checked for by using RNA_collection_local_itemf
   * but operator can be called without invoke */
  if (ID_IS_LINKED(&collection->id)) {
    BKE_report(op->reports, RPT_ERROR, "Could not add the collection because it is linked");
    return OPERATOR_CANCELLED;
  }

  /* Adding object to collection which is used as dupli-collection for self is bad idea.
   *
   * It is also bad idea to add object to collection which is in collection which
   * contains our current object.
   */
  if (BKE_collection_object_cyclic_check(bmain, ob, collection)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Could not add the collection because of dependency cycle detected");
    return OPERATOR_CANCELLED;
  }

  BKE_collection_object_add(bmain, collection, ob);

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_collection_link(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Link to Collection";
  ot->idname = "OBJECT_OT_collection_link";
  ot->description = "Add an object to an existing collection";

  /* api callbacks */
  ot->exec = collection_link_exec;
  ot->invoke = WM_enum_search_invoke;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "collection", rna_enum_dummy_NULL_items, 0, "Collection", "");
  RNA_def_enum_funcs(prop, RNA_collection_local_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static int collection_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_context(C);
  Collection *collection = static_cast<Collection *>(
      CTX_data_pointer_get_type(C, "collection", &RNA_Collection).data);

  if (!ob || !collection) {
    return OPERATOR_CANCELLED;
  }
  if (ID_IS_LINKED(collection) || ID_IS_OVERRIDE_LIBRARY(collection)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Cannot remove an object from a linked or library override collection");
    return OPERATOR_CANCELLED;
  }

  BKE_collection_object_remove(bmain, collection, ob, false);

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_collection_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Collection";
  ot->idname = "OBJECT_OT_collection_remove";
  ot->description = "Remove the active object from this collection";

  /* api callbacks */
  ot->exec = collection_remove_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int collection_unlink_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Collection *collection = static_cast<Collection *>(
      CTX_data_pointer_get_type(C, "collection", &RNA_Collection).data);

  if (!collection) {
    return OPERATOR_CANCELLED;
  }
  if (ID_IS_OVERRIDE_LIBRARY(collection) &&
      collection->id.override_library->hierarchy_root != &collection->id)
  {
    BKE_report(op->reports,
               RPT_ERROR,
               "Cannot unlink a library override collection which is not the root of its override "
               "hierarchy");
    return OPERATOR_CANCELLED;
  }

  BKE_id_delete(bmain, collection);

  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_collection_unlink(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unlink Collection";
  ot->idname = "OBJECT_OT_collection_unlink";
  ot->description = "Unlink the collection from all objects";

  /* api callbacks */
  ot->exec = collection_unlink_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Select objects in the same collection as the active */
static int select_grouped_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Collection *collection = static_cast<Collection *>(
      CTX_data_pointer_get_type(C, "collection", &RNA_Collection).data);

  if (!collection) {
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, Base *, base, visible_bases) {
    if (((base->flag & BASE_SELECTED) == 0) && ((base->flag & BASE_SELECTABLE) != 0)) {
      if (BKE_collection_has_object_recursive(collection, base->object)) {
        ED_object_base_select(base, BA_SELECT);
      }
    }
  }
  CTX_DATA_END;

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_collection_objects_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Objects in Collection";
  ot->idname = "OBJECT_OT_collection_objects_select";
  ot->description = "Select all objects in collection";

  /* api callbacks */
  ot->exec = select_grouped_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
