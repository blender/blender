/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BKE_file_handler.hh"

#include "DNA_collection_types.h"

#include "BLI_path_utils.hh"
#include "BLI_utildefines.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

const EnumPropertyItem rna_enum_collection_color_items[] = {
    {COLLECTION_COLOR_NONE, "NONE", ICON_X, "None", "Assign no color tag to the collection"},
    {COLLECTION_COLOR_01, "COLOR_01", ICON_COLLECTION_COLOR_01, "Color 01", ""},
    {COLLECTION_COLOR_02, "COLOR_02", ICON_COLLECTION_COLOR_02, "Color 02", ""},
    {COLLECTION_COLOR_03, "COLOR_03", ICON_COLLECTION_COLOR_03, "Color 03", ""},
    {COLLECTION_COLOR_04, "COLOR_04", ICON_COLLECTION_COLOR_04, "Color 04", ""},
    {COLLECTION_COLOR_05, "COLOR_05", ICON_COLLECTION_COLOR_05, "Color 05", ""},
    {COLLECTION_COLOR_06, "COLOR_06", ICON_COLLECTION_COLOR_06, "Color 06", ""},
    {COLLECTION_COLOR_07, "COLOR_07", ICON_COLLECTION_COLOR_07, "Color 07", ""},
    {COLLECTION_COLOR_08, "COLOR_08", ICON_COLLECTION_COLOR_08, "Color 08", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
/* Minus 1 for NONE & 1 for the nullptr sentinel. */
BLI_STATIC_ASSERT(ARRAY_SIZE(rna_enum_collection_color_items) - 2 == COLLECTION_COLOR_TOT,
                  "Collection color total is an invalid size");

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "DNA_object_types.h"
#  include "DNA_scene_types.h"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"
#  include "DEG_depsgraph_query.hh"

#  include "BKE_collection.hh"
#  include "BKE_global.hh"
#  include "BKE_idprop.hh"
#  include "BKE_layer.hh"
#  include "BKE_lib_id.hh"
#  include "BKE_library.hh"
#  include "BKE_report.hh"

#  include "BLT_translation.hh"

#  include "WM_api.hh"

#  include "RNA_access.hh"

static void rna_Collection_all_objects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  ListBase collection_objects = BKE_collection_object_cache_get(collection);
  rna_iterator_listbase_begin(iter, ptr, &collection_objects, nullptr);
}

static PointerRNA rna_Collection_all_objects_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we are actually iterating a ObjectBase list, so override get */
  Base *base = (Base *)internal->link;
  return RNA_id_pointer_create(&base->object->id);
}

static void rna_Collection_objects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &collection->gobject, nullptr);
}

static PointerRNA rna_Collection_objects_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we are actually iterating a ObjectBase list, so override get */
  CollectionObject *cob = (CollectionObject *)internal->link;
  return RNA_id_pointer_create(&cob->ob->id);
}

static bool rna_collection_objects_edit_check(Collection *collection,
                                              ReportList *reports,
                                              Object *object)
{
  if (!DEG_is_original(collection)) {
    BKE_reportf(
        reports, RPT_ERROR, "Collection '%s' is not an original ID", collection->id.name + 2);
    return false;
  }
  if (!DEG_is_original(object)) {
    BKE_reportf(reports, RPT_ERROR, "Collection '%s' is not an original ID", object->id.name + 2);
    return false;
  }
  /* Currently this should not be allowed (might be supported in the future though...). */
  if (ID_IS_OVERRIDE_LIBRARY(&collection->id)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not (un)link the object '%s' because the collection '%s' is overridden",
                object->id.name + 2,
                collection->id.name + 2);
    return false;
  }
  if (!ID_IS_EDITABLE(&collection->id)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not (un)link the object '%s' because the collection '%s' is linked",
                object->id.name + 2,
                collection->id.name + 2);
    return false;
  }
  return true;
}

static void rna_Collection_objects_link(Collection *collection,
                                        Main *bmain,
                                        ReportList *reports,
                                        Object *object)
{
  if (!rna_collection_objects_edit_check(collection, reports, object)) {
    return;
  }
  if (!BKE_collection_object_add(bmain, collection, object)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Object '%s' already in collection '%s'",
                object->id.name + 2,
                collection->id.name + 2);
    return;
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &object->id);
}

static void rna_Collection_objects_unlink(Collection *collection,
                                          Main *bmain,
                                          ReportList *reports,
                                          Object *object)
{
  if (!rna_collection_objects_edit_check(collection, reports, object)) {
    return;
  }
  if (!BKE_collection_object_remove(bmain, collection, object, false)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Object '%s' not in collection '%s'",
                object->id.name + 2,
                collection->id.name + 2);
    return;
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &object->id);
}

static bool rna_Collection_objects_override_apply(Main *bmain,
                                                  RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PointerRNA *ptr_item_dst = &rnaapply_ctx.ptr_item_dst;
  PointerRNA *ptr_item_src = &rnaapply_ctx.ptr_item_src;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert_msg(opop->operation == LIBOVERRIDE_OP_REPLACE,
                 "Unsupported RNA override operation on collections' objects");
  UNUSED_VARS_NDEBUG(opop);

  Collection *coll_dst = (Collection *)ptr_dst->owner_id;

  if (ptr_item_dst->type == nullptr || ptr_item_src->type == nullptr) {
    // BLI_assert_msg(0, "invalid source or destination object.");
    return false;
  }

  Object *ob_dst = static_cast<Object *>(ptr_item_dst->data);
  Object *ob_src = static_cast<Object *>(ptr_item_src->data);

  if (ob_src == ob_dst) {
    return true;
  }

  if (!BKE_collection_object_replace(bmain, coll_dst, ob_dst, ob_src)) {
    BLI_assert_msg(0, "Could not find destination object in destination collection!");
    return false;
  }

  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

static void rna_Collection_children_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &collection->children, nullptr);
}

static PointerRNA rna_Collection_children_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we are actually iterating a CollectionChild list, so override get */
  CollectionChild *child = (CollectionChild *)internal->link;
  return RNA_id_pointer_create(&child->collection->id);
}

static bool rna_collection_children_edit_check(Collection *collection,
                                               ReportList *reports,
                                               Collection *child)
{
  if (!DEG_is_original(collection)) {
    BKE_reportf(
        reports, RPT_ERROR, "Collection '%s' is not an original ID", collection->id.name + 2);
    return false;
  }
  if (!DEG_is_original(child)) {
    BKE_reportf(reports, RPT_ERROR, "Collection '%s' is not an original ID", child->id.name + 2);
    return false;
  }
  /* Currently this should not be allowed (might be supported in the future though...). */
  if (ID_IS_OVERRIDE_LIBRARY(&collection->id)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not (un)link the collection '%s' because the collection '%s' is overridden",
                child->id.name + 2,
                collection->id.name + 2);
    return false;
  }
  if (!ID_IS_EDITABLE(&collection->id)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not (un)link the collection '%s' because the collection '%s' is linked",
                child->id.name + 2,
                collection->id.name + 2);
    return false;
  }
  return true;
}

static void rna_Collection_children_link(Collection *collection,
                                         Main *bmain,
                                         ReportList *reports,
                                         Collection *child)
{
  if (!rna_collection_children_edit_check(collection, reports, child)) {
    return;
  }
  if (!BKE_collection_child_add(bmain, collection, child)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Collection '%s' already in collection '%s'",
                child->id.name + 2,
                collection->id.name + 2);
    return;
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &child->id);
}

static void rna_Collection_children_unlink(Collection *collection,
                                           Main *bmain,
                                           ReportList *reports,
                                           Collection *child)
{
  if (!rna_collection_children_edit_check(collection, reports, child)) {
    return;
  }
  if (!BKE_collection_child_remove(bmain, collection, child)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Collection '%s' not in collection '%s'",
                child->id.name + 2,
                collection->id.name + 2);
    return;
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &child->id);
}

static bool rna_Collection_children_override_apply(Main *bmain,
                                                   RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PointerRNA *ptr_item_dst = &rnaapply_ctx.ptr_item_dst;
  PointerRNA *ptr_item_src = &rnaapply_ctx.ptr_item_src;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert_msg(opop->operation == LIBOVERRIDE_OP_REPLACE,
                 "Unsupported RNA override operation on collections' children");
  UNUSED_VARS_NDEBUG(opop);

  Collection *coll_dst = (Collection *)ptr_dst->owner_id;

  if (ptr_item_dst->type == nullptr || ptr_item_src->type == nullptr) {
    /* This can happen when reference and overrides differ, just ignore then. */
    return false;
  }

  Collection *subcoll_dst = static_cast<Collection *>(ptr_item_dst->data);
  Collection *subcoll_src = static_cast<Collection *>(ptr_item_src->data);

  CollectionChild *collchild_dst = static_cast<CollectionChild *>(
      BLI_findptr(&coll_dst->children, subcoll_dst, offsetof(CollectionChild, collection)));

  if (collchild_dst == nullptr) {
    BLI_assert_msg(0, "Could not find destination sub-collection in destination collection!");
    return false;
  }

  /* XXX TODO: We most certainly rather want to have a 'swap object pointer in collection'
   * util in BKE_collection. This is only temp quick dirty test! */
  id_us_min(&collchild_dst->collection->id);
  collchild_dst->collection = subcoll_src;
  id_us_plus(&collchild_dst->collection->id);

  BKE_collection_object_cache_free(bmain, coll_dst, 0);
  BKE_main_collection_sync(bmain);

  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

static void rna_Collection_flag_set(PointerRNA *ptr, const bool value, const int flag)
{
  Collection *collection = (Collection *)ptr->data;

  if (collection->flag & COLLECTION_IS_MASTER) {
    return;
  }

  if (value) {
    collection->flag |= flag;
  }
  else {
    collection->flag &= ~flag;
  }
}

static void rna_Collection_hide_select_set(PointerRNA *ptr, bool value)
{
  rna_Collection_flag_set(ptr, value, COLLECTION_HIDE_SELECT);
}

static void rna_Collection_hide_viewport_set(PointerRNA *ptr, bool value)
{
  rna_Collection_flag_set(ptr, value, COLLECTION_HIDE_VIEWPORT);
}

static void rna_Collection_hide_render_set(PointerRNA *ptr, bool value)
{
  rna_Collection_flag_set(ptr, value, COLLECTION_HIDE_RENDER);
}

static void rna_Collection_flag_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  BKE_collection_object_cache_free(bmain, collection, 0);
  BKE_main_collection_sync(bmain);

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);
}

static int rna_Collection_color_tag_get(PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;

  return collection->color_tag;
}

static void rna_Collection_color_tag_set(PointerRNA *ptr, int value)
{
  Collection *collection = (Collection *)ptr->data;

  if (collection->flag & COLLECTION_IS_MASTER) {
    return;
  }

  collection->color_tag = value;
}

static void rna_Collection_color_tag_update(Main * /*bmain*/, Scene *scene, PointerRNA * /*ptr*/)
{
  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, scene);
}

static void rna_Collection_instance_offset_update(Main * /*bmain*/,
                                                  Scene * /*scene*/,
                                                  PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  DEG_id_tag_update(&collection->id, ID_RECALC_GEOMETRY);
}

static std::optional<std::string> rna_CollectionLightLinking_path(const PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->owner_id;
  CollectionLightLinking *collection_light_linking = (CollectionLightLinking *)ptr->data;

  int counter;

  counter = 0;
  LISTBASE_FOREACH (CollectionObject *, collection_object, &collection->gobject) {
    if (&collection_object->light_linking == collection_light_linking) {
      return fmt::format("collection_objects[{}].light_linking", counter);
    }
    ++counter;
  }

  counter = 0;
  LISTBASE_FOREACH (CollectionChild *, collection_child, &collection->children) {
    if (&collection_child->light_linking == collection_light_linking) {
      return fmt::format("collection_children[{}].light_linking", counter);
    }
    ++counter;
  }

  return "..";
}

static void rna_CollectionLightLinking_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  /* The light linking collection comes from the collection. It does not have shading component,
   * but is collected to objects via hierarchy component. Tagging its hierarchy for update will
   * lead the objects which use the collection to update its shading. */
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_HIERARCHY);

  /* Tag relations for update so that an updated state of light sets is calculated. */
  DEG_relations_tag_update(bmain);
}

static void rna_CollectionExport_name_set(PointerRNA *ptr, const char *value)
{
  CollectionExport *data = reinterpret_cast<CollectionExport *>(ptr->data);
  BKE_collection_exporter_name_set(nullptr, data, value);
}

static CollectionExport *rna_CollectionExport_new(Collection *collection,
                                                  ReportList *reports,
                                                  int type,
                                                  const char *name)
{
  blender::bke::FileHandlerType *fh = nullptr;
  blender::Span<std::unique_ptr<blender::bke::FileHandlerType>> types =
      blender::bke::file_handlers();
  if (types.index_range().contains(type)) {
    fh = types[type].get();
  }
  if (fh) {
    CollectionExport *exporter = BKE_collection_exporter_add(
        collection, fh->idname, name ? (char *)name : fh->label);

    WM_main_add_notifier(NC_SCENE, nullptr);
    return exporter;
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "File handler not found");
    return nullptr;
  }
}

static void rna_CollectionExport_remove(Collection *collection, CollectionExport *exporter)
{
  BKE_collection_exporter_remove(collection, exporter);
  WM_main_add_notifier(NC_SCENE, nullptr);
}

static void rna_CollectionExport_move(Collection *collection,
                                      ReportList *reports,
                                      const int from,
                                      const int to)
{
  if (!BKE_collection_exporter_move(collection, from, to)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not move collection exporter from index '%d' to '%d'",
                from,
                to);
    return;
  }

  WM_main_add_notifier(NC_SCENE, nullptr);
}

static const EnumPropertyItem *rna_CollectionExport_type_itemf(bContext * /*C*/,
                                                               PointerRNA * /*ptr*/,
                                                               PropertyRNA * /*prop*/,
                                                               bool *r_free)
{
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  blender::Span<std::unique_ptr<blender::bke::FileHandlerType>> types =
      blender::bke::file_handlers();

  for (const int i : types.index_range()) {
    blender::bke::FileHandlerType *fh = types[i].get();
    if (WM_operatortype_find(fh->export_operator, true)) {
      item_tmp.value = i;
      item_tmp.identifier = fh->idname;
      item_tmp.name = fh->label;

      RNA_enum_item_add(&item, &totitem, &item_tmp);
    }
  }

  if (totitem == 0) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static PointerRNA rna_CollectionExport_export_properties_get(PointerRNA *ptr)
{
  const CollectionExport *data = reinterpret_cast<CollectionExport *>(ptr->data);

  /* If the File Handler or Operator is missing, we allow the data to be accessible
   * as generic ID properties. */
  blender::bke::FileHandlerType *fh = blender::bke::file_handler_find(data->fh_idname);
  if (!fh) {
    return RNA_pointer_create_discrete(
        ptr->owner_id, &RNA_IDPropertyWrapPtr, data->export_properties);
  }

  wmOperatorType *ot = WM_operatortype_find(fh->export_operator, false);
  if (!ot) {
    return RNA_pointer_create_discrete(
        ptr->owner_id, &RNA_IDPropertyWrapPtr, data->export_properties);
  }

  return RNA_pointer_create_discrete(ptr->owner_id, ot->srna, data->export_properties);
}

static const char *rna_CollectionExport_filepath_value_from_idprop(CollectionExport *data)
{
  if (IDProperty *group = data->export_properties) {
    IDProperty *filepath_prop = IDP_GetPropertyFromGroup(group, "filepath");
    if (filepath_prop && filepath_prop->type == IDP_STRING) {
      return IDP_string_get(filepath_prop);
    }
  }
  return nullptr;
}

static void rna_CollectionExport_filepath_get(PointerRNA *ptr, char *value)
{
  CollectionExport *data = reinterpret_cast<CollectionExport *>(ptr->data);
  const char *value_src = rna_CollectionExport_filepath_value_from_idprop(data);
  strcpy(value, value_src ? value_src : "");
}
static int rna_CollectionExport_filepath_length(PointerRNA *ptr)
{
  CollectionExport *data = reinterpret_cast<CollectionExport *>(ptr->data);
  const char *value_src = rna_CollectionExport_filepath_value_from_idprop(data);
  return value_src ? strlen(value_src) : 0;
}
static void rna_CollectionExport_filepath_set(PointerRNA *ptr, const char *value)
{
  CollectionExport *data = reinterpret_cast<CollectionExport *>(ptr->data);
  if (!data->export_properties) {
    IDPropertyTemplate val{};
    data->export_properties = IDP_New(IDP_GROUP, &val, "export_properties");
  }
  IDProperty *group = data->export_properties;
  /* By convention all exporters are expected to have a `filepath` property.
   * See #WM_operator_properties_filesel. */
  const char *prop_id = "filepath";
  const size_t value_maxsize = FILE_MAX;
  IDProperty *prop = IDP_GetPropertyFromGroup(group, prop_id);
  if (prop && prop->type != IDP_STRING) {
    IDP_FreeFromGroup(group, prop);
    prop = nullptr;
  }
  if (prop == nullptr) {
    prop = IDP_NewStringMaxSize(value, value_maxsize, prop_id);
    IDP_AddToGroup(group, prop);
  }
  else {
    IDP_AssignStringMaxSize(prop, value, value_maxsize);
  }
}

#else

/* collection.objects */
static void rna_def_collection_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "CollectionObjects");
  srna = RNA_def_struct(brna, "CollectionObjects", nullptr);
  RNA_def_struct_sdna(srna, "Collection");
  RNA_def_struct_ui_text(srna, "Collection Objects", "Collection of collection objects");

  /* add object */
  func = RNA_def_function(srna, "link", "rna_Collection_objects_link");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add this object to a collection");
  parm = RNA_def_pointer(func, "object", "Object", "", "Object to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* remove object */
  func = RNA_def_function(srna, "unlink", "rna_Collection_objects_unlink");
  RNA_def_function_ui_description(func, "Remove this object from a collection");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "object", "Object", "", "Object to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

/* collection.children */
static void rna_def_collection_children(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "CollectionChildren");
  srna = RNA_def_struct(brna, "CollectionChildren", nullptr);
  RNA_def_struct_sdna(srna, "Collection");
  RNA_def_struct_ui_text(srna, "Collection Children", "Collection of child collections");

  /* add child */
  func = RNA_def_function(srna, "link", "rna_Collection_children_link");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add this collection as child of this collection");
  parm = RNA_def_pointer(func, "child", "Collection", "", "Collection to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* remove child */
  func = RNA_def_function(srna, "unlink", "rna_Collection_children_unlink");
  RNA_def_function_ui_description(func, "Remove this child collection from a collection");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "child", "Collection", "", "Collection to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_collection_exporters(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "CollectionExports");
  srna = RNA_def_struct(brna, "CollectionExports", nullptr);
  RNA_def_struct_sdna(srna, "Collection");
  RNA_def_struct_ui_text(srna, "Export Handlers", "Collection of export handlers");

  func = RNA_def_function(srna, "new", "rna_CollectionExport_new");
  RNA_def_function_ui_description(func, "Add an export handler to the collection");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_enum(
      func, "type", rna_enum_dummy_DEFAULT_items, 0, "Type", "The type of export handler to add");
  RNA_def_property_enum_funcs(parm, nullptr, nullptr, "rna_CollectionExport_type_itemf");
  RNA_def_parameter_flags(parm, PROP_ENUM_NO_CONTEXT, PARM_REQUIRED);
  RNA_def_string(func, "name", nullptr, 0, "Name", "Name of the new export handler");
  parm = RNA_def_pointer(func, "exporter", "CollectionExport", "", "Newly created export handler");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_CollectionExport_remove");
  RNA_def_function_ui_description(func, "Remove an export handler from the collection");
  parm = RNA_def_pointer(func, "exporter", "CollectionExport", "", "Export Handler to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "move", "rna_CollectionExport_move");
  RNA_def_function_ui_description(func, "Move an export handler");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "from_index", -1, INT_MIN, INT_MAX, "From Index", "Index to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "to_index", -1, INT_MIN, INT_MAX, "To Index", "Target index", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_collection_light_linking(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* TODO(sergey): Use proper icons. */
  static const EnumPropertyItem light_linking_state_items[] = {
      {COLLECTION_LIGHT_LINKING_STATE_INCLUDE, "INCLUDE", ICON_OUTLINER_OB_LIGHT, "Include", ""},
      {COLLECTION_LIGHT_LINKING_STATE_EXCLUDE, "EXCLUDE", ICON_LIGHT, "Exclude", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_define_lib_overridable(true);

  srna = RNA_def_struct(brna, "CollectionLightLinking", nullptr);
  RNA_def_struct_sdna(srna, "CollectionLightLinking");
  RNA_def_struct_ui_text(
      srna,
      "Collection Light Linking",
      "Light linking settings of objects and children collections of a collection");
  RNA_def_struct_path_func(srna, "rna_CollectionLightLinking_path");

  /* Light state. */
  prop = RNA_def_property(srna, "link_state", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, light_linking_state_items);
  RNA_def_property_ui_text(
      prop, "Link State", "Light or shadow receiving state of the object or collection");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_CollectionLightLinking_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_collection_object(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CollectionObject", nullptr);
  RNA_def_struct_sdna(srna, "CollectionObject");
  RNA_def_struct_ui_text(
      srna, "Collection Object", "Object of a collection with its collection related settings");

  RNA_define_lib_overridable(true);

  /* Light Linking. */
  prop = RNA_def_property(srna, "light_linking", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "CollectionLightLinking");
  RNA_def_property_ui_text(prop, "Light Linking", "Light linking settings of the collection");

  RNA_define_lib_overridable(false);
}

static void rna_def_collection_child(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CollectionChild", nullptr);
  RNA_def_struct_sdna(srna, "CollectionChild");
  RNA_def_struct_ui_text(
      srna, "Collection Child", "Child collection with its collection related settings");

  RNA_define_lib_overridable(true);

  /* Light Linking. */
  prop = RNA_def_property(srna, "light_linking", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "CollectionLightLinking");
  RNA_def_property_ui_text(
      prop, "Light Linking", "Light linking settings of the collection object");

  RNA_define_lib_overridable(false);
}

static void rna_def_collection_exporter_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CollectionExport", nullptr);
  RNA_def_struct_sdna(srna, "CollectionExport");
  RNA_def_struct_ui_text(srna, "Collection Export Data", "Exporter configured for the collection");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_ui_text(srna, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_CollectionExport_name_set");

  prop = RNA_def_property(srna, "is_open", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IO_HANDLER_PANEL_OPEN);
  RNA_def_property_ui_text(prop, "Is Open", "Whether the panel is expanded or closed");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_PROPERTIES, nullptr);

  prop = RNA_def_property(srna, "export_properties", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "PropertyGroup");
  RNA_def_property_ui_text(
      prop, "Export Properties", "Properties associated with the configured exporter");
  RNA_def_property_pointer_funcs(
      prop, "rna_CollectionExport_export_properties_get", nullptr, nullptr, nullptr);

  /* Wrap the operator property because exposing the operator property directly
   * causes problems, as the operator property typically wont support
   * #PROP_PATH_SUPPORTS_BLEND_RELATIVE, when the collection property does since
   * it's expanded before passing it to the operator, see #137856 & #137507. */
  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_string_funcs(prop,
                                "rna_CollectionExport_filepath_get",
                                "rna_CollectionExport_filepath_length",
                                "rna_CollectionExport_filepath_set");
  RNA_def_property_string_maxlength(prop, FILE_MAX);
  RNA_def_property_ui_text(prop, "File Path", "The file path used for exporting");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_PROPERTIES, nullptr);
}

void RNA_def_collections(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Collection", "ID");
  RNA_def_struct_ui_text(srna, "Collection", "Collection of Object data-blocks");
  RNA_def_struct_ui_icon(srna, ICON_GROUP);
  /* This is done on save/load in `readfile.cc`,
   * removed if no objects are in the collection and not in a scene. */
  RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "instance_offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_ui_text(
      prop, "Instance Offset", "Offset from the origin to use when instancing");
  RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Collection_instance_offset_update");

  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_Collection_objects_override_apply");
  RNA_def_property_ui_text(prop, "Objects", "Objects that are directly in this collection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Collection_objects_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Collection_objects_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  rna_def_collection_objects(brna, prop);

  prop = RNA_def_property(srna, "all_objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(
      prop, "All Objects", "Objects that are in this collection and its child collections");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Collection_all_objects_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Collection_all_objects_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_override_funcs(
      prop, nullptr, nullptr, "rna_Collection_children_override_apply");
  RNA_def_property_ui_text(
      prop, "Children", "Collections that are immediate children of this collection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Collection_children_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Collection_children_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  rna_def_collection_children(brna, prop);

  /* Collection objects. */
  prop = RNA_def_property(srna, "collection_objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "CollectionObject");
  RNA_def_property_collection_sdna(prop, nullptr, "gobject", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Collection Objects",
      "Objects of the collection with their parent-collection-specific settings");
  /* TODO(sergey): Functions to link and unlink objects. */

  /* Children collections. */
  prop = RNA_def_property(srna, "collection_children", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "CollectionChild");
  RNA_def_property_collection_sdna(prop, nullptr, "children", nullptr);
  RNA_def_property_ui_text(prop,
                           "Collection Children",
                           "Children collections with their parent-collection-specific settings");

  /* Export Handlers. */
  prop = RNA_def_property(srna, "exporters", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "CollectionExport");
  RNA_def_property_collection_sdna(prop, nullptr, "exporters", nullptr);
  RNA_def_property_ui_text(
      prop, "Collection Export Handlers", "Export Handlers configured for the collection");
  rna_def_collection_exporters(brna, prop);

  prop = RNA_def_property(srna, "active_exporter_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Active Collection Exporter Index", "Active index in the exporters list");

  /* TODO(sergey): Functions to link and unlink collections. */

  /* Flags */
  prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", COLLECTION_HIDE_SELECT);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Collection_hide_select_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, -1);
  RNA_def_property_ui_text(prop, "Disable Selection", "Disable selection in viewport");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_Collection_flag_update");

  prop = RNA_def_property(srna, "hide_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", COLLECTION_HIDE_VIEWPORT);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Collection_hide_viewport_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, -1);
  RNA_def_property_ui_text(prop, "Disable in Viewports", "Globally disable in viewports");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_Collection_flag_update");

  prop = RNA_def_property(srna, "hide_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", COLLECTION_HIDE_RENDER);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Collection_hide_render_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, -1);
  RNA_def_property_ui_text(prop, "Disable in Renders", "Globally disable in renders");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_Collection_flag_update");

  static const EnumPropertyItem rna_collection_lineart_usage[] = {
      {COLLECTION_LRT_INCLUDE,
       "INCLUDE",
       0,
       "Include",
       "Generate feature lines for this collection"},
      {COLLECTION_LRT_OCCLUSION_ONLY,
       "OCCLUSION_ONLY",
       0,
       "Occlusion Only",
       "Only use the collection to produce occlusion"},
      {COLLECTION_LRT_EXCLUDE, "EXCLUDE", 0, "Exclude", "Don't use this collection in Line Art"},
      {COLLECTION_LRT_INTERSECTION_ONLY,
       "INTERSECTION_ONLY",
       0,
       "Intersection Only",
       "Only generate intersection lines for this collection"},
      {COLLECTION_LRT_NO_INTERSECTION,
       "NO_INTERSECTION",
       0,
       "No Intersection",
       "Include this collection but do not generate intersection lines"},
      {COLLECTION_LRT_FORCE_INTERSECTION,
       "FORCE_INTERSECTION",
       0,
       "Force Intersection",
       "Generate intersection lines even with objects that disabled intersection"},
      {0, nullptr, 0, nullptr, nullptr}};

  prop = RNA_def_property(srna, "lineart_usage", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_collection_lineart_usage);
  RNA_def_property_ui_text(prop, "Usage", "How to use this collection in Line Art calculation");
  RNA_def_property_update(prop, NC_SCENE, nullptr);

  prop = RNA_def_property(srna, "lineart_use_intersection_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "lineart_flags", 1);
  RNA_def_property_ui_text(
      prop, "Use Intersection Masks", "Use custom intersection mask for faces in this collection");
  RNA_def_property_update(prop, NC_SCENE, nullptr);

  prop = RNA_def_property(srna, "lineart_intersection_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_bitset_array_sdna(
      prop, nullptr, "lineart_intersection_mask", 1 << 0, 8);
  RNA_def_property_ui_text(
      prop, "Masks", "Intersection generated by this collection will have this mask value");
  RNA_def_property_update(prop, NC_SCENE, nullptr);

  prop = RNA_def_property(srna, "lineart_intersection_priority", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Intersection Priority",
                           "The intersection line will be included into the object with the "
                           "higher intersection priority value");
  RNA_def_property_update(prop, NC_SCENE, nullptr);

  prop = RNA_def_property(srna, "use_lineart_intersection_priority", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "lineart_flags", COLLECTION_LRT_USE_INTERSECTION_PRIORITY);
  RNA_def_property_ui_text(
      prop, "Use Intersection Priority", "Assign intersection priority value for this collection");
  RNA_def_property_update(prop, NC_SCENE, nullptr);

  prop = RNA_def_property(srna, "color_tag", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "color_tag");
  RNA_def_property_enum_funcs(
      prop, "rna_Collection_color_tag_get", "rna_Collection_color_tag_set", nullptr);
  RNA_def_property_enum_items(prop, rna_enum_collection_color_items);
  RNA_def_property_ui_text(prop, "Collection Color", "Color tag for a collection");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_Collection_color_tag_update");

  RNA_define_lib_overridable(false);

  rna_def_collection_light_linking(brna);
  rna_def_collection_object(brna);
  rna_def_collection_child(brna);
  rna_def_collection_exporter_data(brna);
}

#endif
