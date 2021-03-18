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
 * \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_collection_types.h"

#include "DNA_lineart_types.h"

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

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
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "DNA_object_types.h"
#  include "DNA_scene_types.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

#  include "BKE_collection.h"
#  include "BKE_global.h"
#  include "BKE_layer.h"

#  include "WM_api.h"

#  include "RNA_access.h"

static void rna_Collection_all_objects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  ListBase collection_objects = BKE_collection_object_cache_get(collection);
  rna_iterator_listbase_begin(iter, &collection_objects, NULL);
}

static PointerRNA rna_Collection_all_objects_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we are actually iterating a ObjectBase list, so override get */
  Base *base = (Base *)internal->link;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, base->object);
}

static void rna_Collection_objects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  rna_iterator_listbase_begin(iter, &collection->gobject, NULL);
}

static PointerRNA rna_Collection_objects_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we are actually iterating a ObjectBase list, so override get */
  CollectionObject *cob = (CollectionObject *)internal->link;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, cob->ob);
}

static void rna_Collection_objects_link(Collection *collection,
                                        Main *bmain,
                                        ReportList *reports,
                                        Object *object)
{
  /* Currently this should not be allowed (might be supported in the future though...). */
  if (ID_IS_OVERRIDE_LIBRARY(&collection->id)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not link the object '%s' because the collection '%s' is overridden",
                object->id.name + 2,
                collection->id.name + 2);
    return;
  }
  if (ID_IS_LINKED(&collection->id)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not link the object '%s' because the collection '%s' is linked",
                object->id.name + 2,
                collection->id.name + 2);
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

  DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &object->id);
}

static void rna_Collection_objects_unlink(Collection *collection,
                                          Main *bmain,
                                          ReportList *reports,
                                          Object *object)
{
  if (!BKE_collection_object_remove(bmain, collection, object, false)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Object '%s' not in collection '%s'",
                object->id.name + 2,
                collection->id.name + 2);
    return;
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &object->id);
}

static bool rna_Collection_objects_override_apply(Main *bmain,
                                                  PointerRNA *ptr_dst,
                                                  PointerRNA *UNUSED(ptr_src),
                                                  PointerRNA *UNUSED(ptr_storage),
                                                  PropertyRNA *UNUSED(prop_dst),
                                                  PropertyRNA *UNUSED(prop_src),
                                                  PropertyRNA *UNUSED(prop_storage),
                                                  const int UNUSED(len_dst),
                                                  const int UNUSED(len_src),
                                                  const int UNUSED(len_storage),
                                                  PointerRNA *ptr_item_dst,
                                                  PointerRNA *ptr_item_src,
                                                  PointerRNA *UNUSED(ptr_item_storage),
                                                  IDOverrideLibraryPropertyOperation *opop)
{
  BLI_assert(opop->operation == IDOVERRIDE_LIBRARY_OP_REPLACE &&
             "Unsupported RNA override operation on collections' objects");
  UNUSED_VARS_NDEBUG(opop);

  Collection *coll_dst = (Collection *)ptr_dst->owner_id;

  if (ptr_item_dst->type == NULL || ptr_item_src->type == NULL) {
    //    BLI_assert(0 && "invalid source or destination object.");
    return false;
  }

  Object *ob_dst = ptr_item_dst->data;
  Object *ob_src = ptr_item_src->data;

  if (ob_src == ob_dst) {
    return true;
  }

  CollectionObject *cob_dst = BLI_findptr(
      &coll_dst->gobject, ob_dst, offsetof(CollectionObject, ob));

  if (cob_dst == NULL) {
    BLI_assert(0 && "Could not find destination object in destination collection!");
    return false;
  }

  /* XXX TODO We most certainly rather want to have a 'swap object pointer in collection'
   * util in BKE_collection. This is only temp quick dirty test! */
  id_us_min(&cob_dst->ob->id);
  cob_dst->ob = ob_src;
  id_us_plus(&cob_dst->ob->id);

  if (BKE_collection_is_in_scene(coll_dst)) {
    BKE_main_collection_sync(bmain);
  }

  return true;
}

static void rna_Collection_children_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  rna_iterator_listbase_begin(iter, &collection->children, NULL);
}

static PointerRNA rna_Collection_children_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we are actually iterating a CollectionChild list, so override get */
  CollectionChild *child = (CollectionChild *)internal->link;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_Collection, child->collection);
}

static void rna_Collection_children_link(Collection *collection,
                                         Main *bmain,
                                         ReportList *reports,
                                         Collection *child)
{
  if (!BKE_collection_child_add(bmain, collection, child)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Collection '%s' already in collection '%s'",
                child->id.name + 2,
                collection->id.name + 2);
    return;
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &child->id);
}

static void rna_Collection_children_unlink(Collection *collection,
                                           Main *bmain,
                                           ReportList *reports,
                                           Collection *child)
{
  if (!BKE_collection_child_remove(bmain, collection, child)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Collection '%s' not in collection '%s'",
                child->id.name + 2,
                collection->id.name + 2);
    return;
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &child->id);
}

static bool rna_Collection_children_override_apply(Main *bmain,
                                                   PointerRNA *ptr_dst,
                                                   PointerRNA *UNUSED(ptr_src),
                                                   PointerRNA *UNUSED(ptr_storage),
                                                   PropertyRNA *UNUSED(prop_dst),
                                                   PropertyRNA *UNUSED(prop_src),
                                                   PropertyRNA *UNUSED(prop_storage),
                                                   const int UNUSED(len_dst),
                                                   const int UNUSED(len_src),
                                                   const int UNUSED(len_storage),
                                                   PointerRNA *ptr_item_dst,
                                                   PointerRNA *ptr_item_src,
                                                   PointerRNA *UNUSED(ptr_item_storage),
                                                   IDOverrideLibraryPropertyOperation *opop)
{
  BLI_assert(opop->operation == IDOVERRIDE_LIBRARY_OP_REPLACE &&
             "Unsupported RNA override operation on collections' children");
  UNUSED_VARS_NDEBUG(opop);

  Collection *coll_dst = (Collection *)ptr_dst->owner_id;

  if (ptr_item_dst->type == NULL || ptr_item_src->type == NULL) {
    /* This can happen when reference and overrides differ, just ignore then. */
    return false;
  }

  Collection *subcoll_dst = ptr_item_dst->data;
  Collection *subcoll_src = ptr_item_src->data;

  CollectionChild *collchild_dst = BLI_findptr(
      &coll_dst->children, subcoll_dst, offsetof(CollectionChild, collection));

  if (collchild_dst == NULL) {
    BLI_assert(0 && "Could not find destination sub-collection in destination collection!");
    return false;
  }

  /* XXX TODO We most certainly rather want to have a 'swap object pointer in collection'
   * util in BKE_collection. This is only temp quick dirty test! */
  id_us_min(&collchild_dst->collection->id);
  collchild_dst->collection = subcoll_src;
  id_us_plus(&collchild_dst->collection->id);

  BKE_collection_object_cache_free(coll_dst);
  BKE_main_collection_sync(bmain);

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
  rna_Collection_flag_set(ptr, value, COLLECTION_RESTRICT_SELECT);
}

static void rna_Collection_hide_viewport_set(PointerRNA *ptr, bool value)
{
  rna_Collection_flag_set(ptr, value, COLLECTION_RESTRICT_VIEWPORT);
}

static void rna_Collection_hide_render_set(PointerRNA *ptr, bool value)
{
  rna_Collection_flag_set(ptr, value, COLLECTION_RESTRICT_RENDER);
}

static void rna_Collection_flag_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;
  BKE_collection_object_cache_free(collection);
  BKE_main_collection_sync(bmain);

  DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);
}

static int rna_Collection_color_tag_get(struct PointerRNA *ptr)
{
  Collection *collection = (Collection *)ptr->data;

  return collection->color_tag;
}

static void rna_Collection_color_tag_set(struct PointerRNA *ptr, int value)
{
  Collection *collection = (Collection *)ptr->data;

  if (collection->flag & COLLECTION_IS_MASTER) {
    return;
  }

  collection->color_tag = value;
}

static void rna_Collection_color_tag_update(Main *UNUSED(bmain),
                                            Scene *scene,
                                            PointerRNA *UNUSED(ptr))
{
  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, scene);
}

#else

/* collection.objects */
static void rna_def_collection_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "CollectionObjects");
  srna = RNA_def_struct(brna, "CollectionObjects", NULL);
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
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

/* collection.children */
static void rna_def_collection_children(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "CollectionChildren");
  srna = RNA_def_struct(brna, "CollectionChildren", NULL);
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
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

void RNA_def_collections(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Collection", "ID");
  RNA_def_struct_ui_text(srna, "Collection", "Collection of Object data-blocks");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_COLLECTION);
  /* This is done on save/load in readfile.c,
   * removed if no objects are in the collection and not in a scene. */
  RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "instance_offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_ui_text(
      prop, "Instance Offset", "Offset from the origin to use when instancing");
  RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_override_funcs(prop, NULL, NULL, "rna_Collection_objects_override_apply");
  RNA_def_property_ui_text(prop, "Objects", "Objects that are directly in this collection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Collection_objects_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Collection_objects_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
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
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);

  prop = RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_override_funcs(prop, NULL, NULL, "rna_Collection_children_override_apply");
  RNA_def_property_ui_text(
      prop, "Children", "Collections that are immediate children of this collection");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Collection_children_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Collection_children_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  rna_def_collection_children(brna, prop);

  /* Flags */
  prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", COLLECTION_RESTRICT_SELECT);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Collection_hide_select_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, -1);
  RNA_def_property_ui_text(prop, "Disable Selection", "Disable selection in viewport");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_Collection_flag_update");

  prop = RNA_def_property(srna, "hide_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", COLLECTION_RESTRICT_VIEWPORT);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Collection_hide_viewport_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, -1);
  RNA_def_property_ui_text(prop, "Disable in Viewports", "Globally disable in viewports");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_Collection_flag_update");

  prop = RNA_def_property(srna, "hide_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", COLLECTION_RESTRICT_RENDER);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Collection_hide_render_set");
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
      {COLLECTION_LRT_EXCLUDE, "EXCLUDE", 0, "Exclude", "Don't use this collection in line art"},
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
      {0, NULL, 0, NULL, NULL}};

  prop = RNA_def_property(srna, "lineart_usage", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_collection_lineart_usage);
  RNA_def_property_ui_text(prop, "Usage", "How to use this collection in line art");
  RNA_def_property_update(prop, NC_SCENE, NULL);

  prop = RNA_def_property(srna, "color_tag", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "color_tag");
  RNA_def_property_enum_funcs(
      prop, "rna_Collection_color_tag_get", "rna_Collection_color_tag_set", NULL);
  RNA_def_property_enum_items(prop, rna_enum_collection_color_items);
  RNA_def_property_ui_text(prop, "Collection Color", "Color tag for a collection");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_Collection_color_tag_update");

  RNA_define_lib_overridable(false);
}

#endif
