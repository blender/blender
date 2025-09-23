/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_file_handler.hh"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"

#include "object_intern.hh"

namespace blender::ed::object {

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

  ob = context_object(C);

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

static wmOperatorStatus objects_add_active_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int single_collection_index = RNA_enum_get(op->ptr, "collection");
  Collection *single_collection = collection_object_active_find_index(
      bmain, scene, ob, single_collection_index);
  bool is_cycle = false;
  bool changed_multi = false;

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

    bool changed = false;
    CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
      if (BKE_collection_has_object(collection, base->object)) {
        continue;
      }

      if (!BKE_collection_object_cyclic_check(bmain, base->object, collection)) {
        BKE_collection_object_add(bmain, collection, base->object);
        changed = true;
      }
      else {
        is_cycle = true;
      }
    }
    CTX_DATA_END;

    if (changed) {
      DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
      changed_multi = true;
    }
  }
  FOREACH_COLLECTION_END;

  if (is_cycle) {
    BKE_report(op->reports, RPT_WARNING, "Skipped some collections because of cycle detected");
  }

  if (!changed_multi) {
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
  ot->name = "Add Selected to Active Object's Collection";
  ot->description =
      "Add selected objects to one of the collections the active-object is part of. "
      "Optionally add to \"All Collections\" to ensure selected objects are included in "
      "the same collections as the active object";
  ot->idname = "COLLECTION_OT_objects_add_active";

  /* API callbacks. */
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

static wmOperatorStatus objects_remove_active_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  int single_collection_index = RNA_enum_get(op->ptr, "collection");
  Collection *single_collection = collection_object_active_find_index(
      bmain, scene, ob, single_collection_index);
  bool changed_multi = false;

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
      bool changed = false;
      CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
        BKE_collection_object_remove(bmain, collection, base->object, false);
        changed = true;
      }
      CTX_DATA_END;

      if (changed) {
        DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
        changed_multi = true;
      }
    }
  }
  FOREACH_COLLECTION_END;

  if (!changed_multi) {
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

  /* API callbacks. */
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

static wmOperatorStatus collection_objects_remove_all_exec(bContext *C, wmOperator * /*op*/)
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

  /* API callbacks. */
  ot->exec = collection_objects_remove_all_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus collection_objects_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int single_collection_index = RNA_enum_get(op->ptr, "collection");
  Collection *single_collection = collection_object_active_find_index(
      bmain, scene, ob, single_collection_index);
  bool changed_multi = false;

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
    bool changed = false;
    CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
      BKE_collection_object_remove(bmain, collection, base->object, false);
      changed = true;
    }
    CTX_DATA_END;

    if (changed) {
      DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
      changed_multi = true;
    }
  }
  FOREACH_COLLECTION_END;

  if (!changed_multi) {
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

  /* API callbacks. */
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

static wmOperatorStatus collection_create_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  char name[MAX_ID_NAME - 2]; /* id name */
  bool changed = false;

  RNA_string_get(op->ptr, "name", name);

  Collection *collection = BKE_collection_add(bmain, nullptr, name);
  id_fake_user_set(&collection->id);

  CTX_DATA_BEGIN (C, Base *, base, selected_bases) {
    BKE_collection_object_add(bmain, collection, base->object);
    changed = true;
  }
  CTX_DATA_END;

  if (changed) {
    DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  }

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

  /* API callbacks. */
  ot->exec = collection_create_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_string(ot->srna, "name", nullptr, MAX_ID_NAME - 2, "Name", "Name of the new collection");
}

static bool collection_exporter_common_check(const Collection *collection)
{
  return collection != nullptr &&
         !(ID_IS_LINKED(&collection->id) || ID_IS_OVERRIDE_LIBRARY(&collection->id));
}

static bool collection_exporter_poll(bContext *C)
{
  const Collection *collection = CTX_data_collection(C);
  return collection_exporter_common_check(collection);
}

static bool collection_exporter_remove_poll(bContext *C)
{
  const Collection *collection = CTX_data_collection(C);
  return collection_exporter_common_check(collection) &&
         !BLI_listbase_is_empty(&collection->exporters);
}

static bool collection_export_all_poll(bContext *C)
{
  return CTX_data_view_layer(C) != nullptr;
}

static wmOperatorStatus collection_exporter_add_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Collection *collection = CTX_data_collection(C);

  char name[MAX_ID_NAME - 2]; /* id name */
  RNA_string_get(op->ptr, "name", name);

  bke::FileHandlerType *fh = bke::file_handler_find(name);
  if (!fh) {
    BKE_reportf(op->reports, RPT_ERROR, "File handler '%s' not found", name);
    return OPERATOR_CANCELLED;
  }

  if (!WM_operatortype_find(fh->export_operator, true)) {
    BKE_reportf(
        op->reports, RPT_ERROR, "File handler operator '%s' not found", fh->export_operator);
    return OPERATOR_CANCELLED;
  }

  BKE_collection_exporter_add(collection, fh->idname, fh->label);

  BKE_view_layer_need_resync_tag(CTX_data_view_layer(C));
  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_PROPERTIES, nullptr);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  return OPERATOR_FINISHED;
}

static void COLLECTION_OT_exporter_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Exporter";
  ot->description = "Add exporter to the exporter list";
  ot->idname = "COLLECTION_OT_exporter_add";

  /* API callbacks. */
  ot->exec = collection_exporter_add_exec;
  ot->poll = collection_exporter_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_string(ot->srna, "name", nullptr, MAX_ID_NAME - 2, "Name", "FileHandler idname");
}

static wmOperatorStatus collection_exporter_remove_exec(bContext *C, wmOperator *op)
{
  Collection *collection = CTX_data_collection(C);
  ListBase *exporters = &collection->exporters;

  int index = RNA_int_get(op->ptr, "index");
  CollectionExport *data = static_cast<CollectionExport *>(BLI_findlink(exporters, index));
  if (!data) {
    return OPERATOR_CANCELLED;
  }

  BKE_collection_exporter_remove(collection, data);

  BKE_view_layer_need_resync_tag(CTX_data_view_layer(C));
  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_PROPERTIES, nullptr);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus collection_exporter_remove_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent * /*event*/)
{
  return WM_operator_confirm_ex(
      C, op, IFACE_("Remove exporter?"), nullptr, IFACE_("Delete"), ALERT_ICON_NONE, false);
}

static void COLLECTION_OT_exporter_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Exporter";
  ot->description = "Remove exporter from the exporter list";
  ot->idname = "COLLECTION_OT_exporter_remove";

  /* API callbacks. */
  ot->invoke = collection_exporter_remove_invoke;
  ot->exec = collection_exporter_remove_exec;
  ot->poll = collection_exporter_remove_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "Exporter index", 0, INT_MAX);
}

static wmOperatorStatus collection_exporter_move_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Collection *collection = CTX_data_collection(C);
  const int dir = RNA_enum_get(op->ptr, "direction");
  const int from = collection->active_exporter_index;

  /* Move Up/down to index. */
  const int to = from + dir;

  if (!BKE_collection_exporter_move(collection, from, to)) {
    return OPERATOR_CANCELLED;
  }

  collection->active_exporter_index = to;
  return OPERATOR_FINISHED;
}

static void COLLECTION_OT_exporter_move(wmOperatorType *ot)
{
  static const EnumPropertyItem exporter_move[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Exporter";
  ot->description = "Move exporter up or down in the exporter list";
  ot->idname = "COLLECTION_OT_exporter_move";

  /* API callbacks. */
  ot->exec = collection_exporter_move_exec;
  ot->poll = collection_exporter_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "direction",
               exporter_move,
               0,
               "Direction",
               "Direction to move the active exporter");
}

static wmOperatorStatus collection_exporter_export(bContext *C,
                                                   wmOperator *op,
                                                   CollectionExport *data,
                                                   Collection *collection,
                                                   const bool report_success)
{
  using namespace blender;
  bke::FileHandlerType *fh = bke::file_handler_find(data->fh_idname);
  if (!fh) {
    BKE_reportf(op->reports, RPT_ERROR, "File handler '%s' not found", data->fh_idname);
    return OPERATOR_CANCELLED;
  }

  wmOperatorType *ot = WM_operatortype_find(fh->export_operator, false);
  if (!ot) {
    BKE_reportf(
        op->reports, RPT_ERROR, "File handler operator '%s' not found", fh->export_operator);
    return OPERATOR_CANCELLED;
  }

  /* Execute operator with our stored properties. */
  /* TODO: Cascade settings down from parent collections(?) */
  IDProperty *op_props = IDP_CopyProperty(data->export_properties);
  PointerRNA properties = RNA_pointer_create_discrete(nullptr, ot->srna, op_props);
  const char *collection_name = collection->id.name + 2;

  /* Ensure we have a valid filepath set. Create one if the user has not specified anything yet. */
  char filepath[FILE_MAX];
  RNA_string_get(&properties, "filepath", filepath);
  if (!filepath[0]) {
    BLI_path_join(
        filepath, sizeof(filepath), "//", fh->get_default_filename(collection_name).c_str());
  }
  else {
    const char *filename = BLI_path_basename(filepath);
    if (!filename[0] || !BLI_path_extension(filename)) {
      BKE_reportf(op->reports, RPT_ERROR, "File path '%s' is not a valid file", filepath);

      IDP_FreeProperty(op_props);
      return OPERATOR_CANCELLED;
    }
  }

  const Main *bmain = CTX_data_main(C);
  BLI_path_abs(filepath, BKE_main_blendfile_path(bmain));

  /* Ensure that any properties from when this operator was "last used" are cleared. Save them for
   * restoration later. Otherwise properties from a regular File->Export may contaminate this
   * collection export. */
  IDProperty *last_properties = ot->last_properties;
  ot->last_properties = nullptr;

  RNA_string_set(&properties, "filepath", filepath);
  RNA_string_set(&properties, "collection", collection_name);
  wmOperatorStatus op_result = WM_operator_name_call_ptr(
      C, ot, wm::OpCallContext::ExecDefault, &properties, nullptr);

  /* Free the "last used" properties that were just set from the collection export and restore the
   * original "last used" properties. */
  if (ot->last_properties) {
    IDP_FreeProperty(ot->last_properties);
  }
  ot->last_properties = last_properties;

  IDP_FreeProperty(op_props);

  if (report_success && op_result == OPERATOR_FINISHED) {
    BKE_reportf(op->reports, RPT_INFO, "Exported '%s'", filepath);
  }

  return op_result;
}

static wmOperatorStatus collection_exporter_export_exec(bContext *C, wmOperator *op)
{
  Collection *collection = CTX_data_collection(C);
  ListBase *exporters = &collection->exporters;

  int index = RNA_int_get(op->ptr, "index");
  CollectionExport *data = static_cast<CollectionExport *>(BLI_findlink(exporters, index));
  if (!data) {
    return OPERATOR_CANCELLED;
  }

  return collection_exporter_export(C, op, data, collection, true);
}

static void COLLECTION_OT_exporter_export(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Export";
  ot->description = "Invoke the export operation";
  ot->idname = "COLLECTION_OT_exporter_export";

  /* API callbacks. */
  ot->exec = collection_exporter_export_exec;
  ot->poll = collection_exporter_poll;

  /* flags */
  ot->flag = 0;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "Exporter index", 0, INT_MAX);
}

struct CollectionExportStats {
  int successful_exports_num = 0;
  int collections_num = 0;
};

static wmOperatorStatus collection_export(bContext *C,
                                          wmOperator *op,
                                          Collection *collection,
                                          CollectionExportStats &stats)
{
  ListBase *exporters = &collection->exporters;
  int files_num = 0;

  LISTBASE_FOREACH (CollectionExport *, data, exporters) {
    if (collection_exporter_export(C, op, data, collection, false) != OPERATOR_FINISHED) {
      /* Do not continue calling exporters if we encounter one that fails. */
      return OPERATOR_CANCELLED;
    }
    files_num++;
  }

  if (files_num) {
    stats.successful_exports_num += files_num;
    stats.collections_num++;
  }
  return OPERATOR_FINISHED;
}

static wmOperatorStatus collection_io_export_all_exec(bContext *C, wmOperator *op)
{
  Collection *collection = CTX_data_collection(C);
  CollectionExportStats stats;
  wmOperatorStatus result = collection_export(C, op, collection, stats);

  /* Only report if nothing was cancelled along the way. We don't want this UI report to happen
   * over-top any reports from the actual failures. */
  if (result == OPERATOR_FINISHED && stats.successful_exports_num > 0) {
    BKE_reportf(op->reports,
                RPT_INFO,
                "Exported %d files from collection '%s'",
                stats.successful_exports_num,
                collection->id.name + 2);
  }

  return result;
}

static void COLLECTION_OT_export_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Export All";
  ot->description = "Invoke all configured exporters on this collection";
  ot->idname = "COLLECTION_OT_export_all";

  /* API callbacks. */
  ot->exec = collection_io_export_all_exec;
  ot->poll = collection_exporter_poll;

  /* flags */
  ot->flag = 0;
}

static wmOperatorStatus collection_export_recursive(bContext *C,
                                                    wmOperator *op,
                                                    LayerCollection *layer_collection,
                                                    CollectionExportStats &stats)
{
  /* Skip collections which have been Excluded in the View Layer. */
  if (layer_collection->flag & LAYER_COLLECTION_EXCLUDE) {
    return OPERATOR_FINISHED;
  }

  if (!collection_exporter_common_check(layer_collection->collection)) {
    return OPERATOR_FINISHED;
  }

  if (collection_export(C, op, layer_collection->collection, stats) != OPERATOR_FINISHED) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (LayerCollection *, child, &layer_collection->layer_collections) {
    if (collection_export_recursive(C, op, child, stats) != OPERATOR_FINISHED) {
      return OPERATOR_CANCELLED;
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus wm_collection_export_all_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);

  CollectionExportStats stats;
  LISTBASE_FOREACH (LayerCollection *, layer_collection, &view_layer->layer_collections) {
    if (collection_export_recursive(C, op, layer_collection, stats) != OPERATOR_FINISHED) {
      return OPERATOR_CANCELLED;
    }
  }

  /* Only report if nothing was cancelled along the way. We don't want this UI report to happen
   * over-top any reports from the actual failures. */
  if (stats.successful_exports_num > 0) {
    BKE_reportf(op->reports,
                RPT_INFO,
                "Exported %d files from %d collections",
                stats.successful_exports_num,
                stats.collections_num);
  }

  return OPERATOR_FINISHED;
}

static void WM_OT_collection_export_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Export All Collections";
  ot->description = "Invoke all configured exporters for all collections";
  ot->idname = "WM_OT_collection_export_all";

  /* API callbacks. */
  ot->exec = wm_collection_export_all_exec;
  ot->poll = collection_export_all_poll;

  /* flags */
  ot->flag = 0;
}

static void collection_exporter_menu_draw(const bContext * /*C*/, Menu *menu)
{
  using namespace blender;
  uiLayout *layout = menu->layout;

  /* Add all file handlers capable of being exported to the menu. */
  bool at_least_one = false;
  for (const auto &fh : bke::file_handlers()) {
    if (WM_operatortype_find(fh->export_operator, true)) {
      PointerRNA op_ptr = layout->op("COLLECTION_OT_exporter_add", fh->label, ICON_NONE);
      RNA_string_set(&op_ptr, "name", fh->idname);
      at_least_one = true;
    }
  }

  if (!at_least_one) {
    layout->label(IFACE_("No file handlers available"), ICON_NONE);
  }
}

void collection_exporter_register()
{
  MenuType *mt = MEM_callocN<MenuType>(__func__);
  STRNCPY_UTF8(mt->idname, "COLLECTION_MT_exporter_add");
  STRNCPY_UTF8(mt->label, N_("Add Exporter"));
  mt->draw = collection_exporter_menu_draw;

  WM_menutype_add(mt);
  WM_operatortype_append(COLLECTION_OT_exporter_add);
  WM_operatortype_append(COLLECTION_OT_exporter_remove);
  WM_operatortype_append(COLLECTION_OT_exporter_move);
  WM_operatortype_append(COLLECTION_OT_exporter_export);
  WM_operatortype_append(COLLECTION_OT_export_all);
  WM_operatortype_append(WM_OT_collection_export_all);
}

/****************** properties window operators *********************/

static wmOperatorStatus collection_add_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);
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

  /* API callbacks. */
  ot->exec = collection_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus collection_link_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_object(C);
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
  if (!ID_IS_EDITABLE(&collection->id)) {
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

  /* API callbacks. */
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

static wmOperatorStatus collection_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_object(C);
  Collection *collection = static_cast<Collection *>(
      CTX_data_pointer_get_type(C, "collection", &RNA_Collection).data);

  if (!ob || !collection) {
    return OPERATOR_CANCELLED;
  }
  if (!ID_IS_EDITABLE(collection) || ID_IS_OVERRIDE_LIBRARY(collection)) {
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

  /* API callbacks. */
  ot->exec = collection_remove_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus collection_unlink_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Collection *collection = CTX_data_collection(C);

  if (!collection) {
    return OPERATOR_CANCELLED;
  }
  if (collection->flag & COLLECTION_IS_MASTER) {
    return OPERATOR_CANCELLED;
  }
  BLI_assert((collection->id.flag & ID_FLAG_EMBEDDED_DATA) == 0);
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

static bool collection_unlink_poll(bContext *C)
{
  Collection *collection = CTX_data_collection(C);

  if (!collection) {
    return false;
  }
  if (collection->flag & COLLECTION_IS_MASTER) {
    return false;
  }
  BLI_assert((collection->id.flag & ID_FLAG_EMBEDDED_DATA) == 0);
  if (ID_IS_OVERRIDE_LIBRARY(collection) &&
      collection->id.override_library->hierarchy_root != &collection->id)
  {
    return false;
  }

  return ED_operator_objectmode(C);
}

void OBJECT_OT_collection_unlink(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unlink Collection";
  ot->idname = "OBJECT_OT_collection_unlink";
  ot->description = "Unlink the collection from all objects";

  /* API callbacks. */
  ot->exec = collection_unlink_exec;
  ot->poll = collection_unlink_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Select objects in the same collection as the active */
static wmOperatorStatus select_grouped_exec(bContext *C, wmOperator * /*op*/)
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
        base_select(base, BA_SELECT);
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

  /* API callbacks. */
  ot->exec = select_grouped_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::object
