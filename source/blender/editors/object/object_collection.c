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
 *
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup edobj
 */

#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_screen.h"
#include "ED_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "object_intern.h"

/********************* 3d view operators ***********************/

/* can be called with C == NULL */
static const EnumPropertyItem *collection_object_active_itemf(bContext *C,
                                                              PointerRNA *UNUSED(ptr),
                                                              PropertyRNA *UNUSED(prop),
                                                              bool *r_free)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob;
  EnumPropertyItem *item = NULL, item_tmp = {0};
  int totitem = 0;

  if (C == NULL) {
    return DummyRNA_NULL_items;
  }

  ob = ED_object_context(C);

  /* check that the object exists */
  if (ob) {
    Collection *collection;
    int i = 0, count = 0;

    /* if 2 or more collections, add option to add to all collections */
    collection = NULL;
    while ((collection = BKE_collection_object_find(bmain, scene, collection, ob))) {
      count++;
    }

    if (count >= 2) {
      item_tmp.identifier = item_tmp.name = "All Collections";
      item_tmp.value = INT_MAX; /* this will give NULL on lookup */
      RNA_enum_item_add(&item, &totitem, &item_tmp);
      RNA_enum_item_add_separator(&item, &totitem);
    }

    /* add collections */
    collection = NULL;
    while ((collection = BKE_collection_object_find(bmain, scene, collection, ob))) {
      item_tmp.identifier = item_tmp.name = collection->id.name + 2;
      /* item_tmp.icon = ICON_ARMATURE_DATA; */
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
  Collection *collection = NULL;
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

  if (ob == NULL) {
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
        DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
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
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_objects_add_active(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Selected To Active Collection";
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
                      DummyRNA_NULL_items,
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
  Object *ob = OBACT(view_layer);
  int single_collection_index = RNA_enum_get(op->ptr, "collection");
  Collection *single_collection = collection_object_active_find_index(
      bmain, scene, ob, single_collection_index);
  bool ok = false;

  if (ob == NULL) {
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
        DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
        ok = 1;
      }
      CTX_DATA_END;
    }
  }
  FOREACH_COLLECTION_END;

  if (!ok) {
    BKE_report(op->reports, RPT_ERROR, "Active object contains no collections");
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_objects_remove_active(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Remove Selected From Active Collection";
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
                      DummyRNA_NULL_items,
                      0,
                      "Collection",
                      "The collection to remove other selected objects from");
  RNA_def_enum_funcs(prop, collection_object_active_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static int collection_objects_remove_all_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    BKE_object_groups_clear(bmain, scene, base->object);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_objects_remove_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove From All Unlinked Collections";
  ot->description = "Remove selected objects from all collections not used in a scene";
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

  if (ob == NULL) {
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
      DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
      updated = true;
    }
    CTX_DATA_END;
  }
  FOREACH_COLLECTION_END;

  if (!updated) {
    return OPERATOR_CANCELLED;
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void COLLECTION_OT_objects_remove(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Remove From Collection";
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
                      DummyRNA_NULL_items,
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

  Collection *collection = BKE_collection_add(bmain, NULL, name);
  id_fake_user_set(&collection->id);

  CTX_DATA_BEGIN (C, Base *, base, selected_bases) {
    BKE_collection_object_add(bmain, collection, base->object);
    DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_GROUP | NA_EDITED, NULL);

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

static int collection_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = ED_object_context(C);
  Main *bmain = CTX_data_main(C);

  if (ob == NULL) {
    return OPERATOR_CANCELLED;
  }

  Collection *collection = BKE_collection_add(bmain, NULL, "Collection");
  id_fake_user_set(&collection->id);
  BKE_collection_object_add(bmain, collection, ob);

  DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
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
  Collection *collection = BLI_findlink(&bmain->collections, RNA_enum_get(op->ptr, "collection"));

  if (ELEM(NULL, ob, collection)) {
    return OPERATOR_CANCELLED;
  }

  /* Early return check, if the object is already in collection
   * we could skip all the dependency check and just consider
   * operator is finished.
   */
  if (BKE_collection_has_object(collection, ob)) {
    return OPERATOR_FINISHED;
  }

  /* Adding object to collection which is used as dupli-collection for self is bad idea.
   *
   * It is also  bad idea to add object to collection which is in collection which
   * contains our current object.
   */
  if (BKE_collection_object_cyclic_check(bmain, ob, collection)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Could not add the collection because of dependency cycle detected");
    return OPERATOR_CANCELLED;
  }

  BKE_collection_object_add(bmain, collection, ob);

  DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
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
  prop = RNA_def_enum(ot->srna, "collection", DummyRNA_NULL_items, 0, "Collection", "");
  RNA_def_enum_funcs(prop, RNA_collection_local_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static int collection_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_context(C);
  Collection *collection = CTX_data_pointer_get_type(C, "collection", &RNA_Collection).data;

  if (!ob || !collection) {
    return OPERATOR_CANCELLED;
  }

  BKE_collection_object_remove(bmain, collection, ob, false);

  DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
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

static int collection_unlink_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Collection *collection = CTX_data_pointer_get_type(C, "collection", &RNA_Collection).data;

  if (!collection) {
    return OPERATOR_CANCELLED;
  }

  BKE_id_delete(bmain, collection);

  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

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
static int select_grouped_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Collection *collection = CTX_data_pointer_get_type(C, "collection", &RNA_Collection).data;

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
