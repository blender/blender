/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_cachefile_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

const EnumPropertyItem rna_enum_velocity_unit_items[] = {
    {CACHEFILE_VELOCITY_UNIT_SECOND, "SECOND", 0, "Second", ""},
    {CACHEFILE_VELOCITY_UNIT_FRAME, "FRAME", 0, "Frame", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "BLI_math_base.h"

#  include "BKE_cachefile.hh"
#  include "BKE_context.hh"
#  include "BKE_report.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

#  ifdef WITH_ALEMBIC
#    include "ABC_alembic.h"
#  endif

static void rna_CacheFile_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  CacheFile *cache_file = (CacheFile *)ptr->data;

  DEG_id_tag_update(&cache_file->id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
}

static void rna_CacheFileLayer_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  CacheFile *cache_file = (CacheFile *)ptr->owner_id;

  DEG_id_tag_update(&cache_file->id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
}

static void rna_CacheFile_object_paths_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  CacheFile *cache_file = (CacheFile *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &cache_file->object_paths, nullptr);
}

static PointerRNA rna_CacheFile_active_layer_get(PointerRNA *ptr)
{
  CacheFile *cache_file = (CacheFile *)ptr->owner_id;
  return RNA_pointer_create_with_parent(
      *ptr, &RNA_CacheFileLayer, BKE_cachefile_get_active_layer(cache_file));
}

static void rna_CacheFile_active_layer_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  CacheFile *cache_file = (CacheFile *)ptr->owner_id;
  int index = BLI_findindex(&cache_file->layers, value.data);
  if (index == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Layer '%s' not found in object '%s'",
                ((CacheFileLayer *)value.data)->filepath,
                cache_file->id.name + 2);
    return;
  }

  cache_file->active_layer = index + 1;
}

static int rna_CacheFile_active_layer_index_get(PointerRNA *ptr)
{
  CacheFile *cache_file = (CacheFile *)ptr->owner_id;
  return cache_file->active_layer - 1;
}

static void rna_CacheFile_active_layer_index_set(PointerRNA *ptr, int value)
{
  CacheFile *cache_file = (CacheFile *)ptr->owner_id;
  cache_file->active_layer = value + 1;
}

static void rna_CacheFile_active_layer_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  CacheFile *cache_file = (CacheFile *)ptr->owner_id;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&cache_file->layers) - 1);
}

static void rna_CacheFileLayer_hidden_flag_set(PointerRNA *ptr, const bool value)
{
  CacheFileLayer *layer = (CacheFileLayer *)ptr->data;

  if (value) {
    layer->flag |= CACHEFILE_LAYER_HIDDEN;
  }
  else {
    layer->flag &= ~CACHEFILE_LAYER_HIDDEN;
  }
}

static CacheFileLayer *rna_CacheFile_layer_new(CacheFile *cache_file,
                                               bContext *C,
                                               ReportList *reports,
                                               const char *filepath)
{
  CacheFileLayer *layer = BKE_cachefile_add_layer(cache_file, filepath);
  if (layer == nullptr) {
    BKE_reportf(
        reports, RPT_ERROR, "Cannot add a layer to CacheFile '%s'", cache_file->id.name + 2);
    return nullptr;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_cachefile_reload(depsgraph, cache_file);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
  return layer;
}

static void rna_CacheFile_layer_remove(CacheFile *cache_file, bContext *C, PointerRNA *layer_ptr)
{
  CacheFileLayer *layer = static_cast<CacheFileLayer *>(layer_ptr->data);
  BKE_cachefile_remove_layer(cache_file, layer);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_cachefile_reload(depsgraph, cache_file);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
}

#else

/* cachefile.object_paths */
static void rna_def_alembic_object_path(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "CacheObjectPath", nullptr);
  RNA_def_struct_sdna(srna, "CacheObjectPath");
  RNA_def_struct_ui_text(srna, "Object Path", "Path of an object inside of an Alembic archive");
  RNA_def_struct_ui_icon(srna, ICON_NONE);

  RNA_define_lib_overridable(true);

  PropertyRNA *prop = RNA_def_property(srna, "path", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Path", "Object path");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_FILEBROWSER);
  RNA_def_struct_name_property(srna, prop);

  RNA_define_lib_overridable(false);
}

/* cachefile.object_paths */
static void rna_def_cachefile_object_paths(BlenderRNA *brna, PropertyRNA *cprop)
{
  RNA_def_property_srna(cprop, "CacheObjectPaths");
  StructRNA *srna = RNA_def_struct(brna, "CacheObjectPaths", nullptr);
  RNA_def_struct_sdna(srna, "CacheFile");
  RNA_def_struct_ui_text(srna, "Object Paths", "Collection of object paths");
}

static void rna_def_cachefile_layer(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "CacheFileLayer", nullptr);
  RNA_def_struct_sdna(srna, "CacheFileLayer");
  RNA_def_struct_ui_text(
      srna,
      "Cache Layer",
      "Layer of the cache, used to load or override data from the first the first layer");

  PropertyRNA *prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File Path", "Path to the archive");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, 0, "rna_CacheFileLayer_update");

  prop = RNA_def_property(srna, "hide_layer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CACHEFILE_LAYER_HIDDEN);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_CacheFileLayer_hidden_flag_set");
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide Layer", "Do not load data from this layer");
  RNA_def_property_update(prop, 0, "rna_CacheFileLayer_update");
}

static void rna_def_cachefile_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  RNA_def_property_srna(cprop, "CacheFileLayers");
  StructRNA *srna = RNA_def_struct(brna, "CacheFileLayers", nullptr);
  RNA_def_struct_sdna(srna, "CacheFile");
  RNA_def_struct_ui_text(srna, "Cache Layers", "Collection of cache layers");

  PropertyRNA *prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "CacheFileLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_CacheFile_active_layer_get", "rna_CacheFile_active_layer_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Layer", "Active layer of the CacheFile");

  /* Add a layer. */
  FunctionRNA *func = RNA_def_function(srna, "new", "rna_CacheFile_layer_new");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Add a new layer");
  PropertyRNA *parm = RNA_def_string(
      func, "filepath", "File Path", 0, "", "File path to the archive used as a layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* Return type. */
  parm = RNA_def_pointer(func, "layer", "CacheFileLayer", "", "Newly created layer");
  RNA_def_function_return(func, parm);

  /* Remove a layer. */
  func = RNA_def_function(srna, "remove", "rna_CacheFile_layer_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove an existing layer from the cache file");
  parm = RNA_def_pointer(func, "layer", "CacheFileLayer", "", "Layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_cachefile(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "CacheFile", "ID");
  RNA_def_struct_sdna(srna, "CacheFile");
  RNA_def_struct_ui_text(srna, "CacheFile", "");
  RNA_def_struct_ui_icon(srna, ICON_FILE);

  RNA_define_lib_overridable(true);

  PropertyRNA *prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File Path", "Path to external displacements file");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");

  prop = RNA_def_property(srna, "is_sequence", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Sequence", "Whether the cache is separated in a series of files");
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");

  /* ----------------- For Scene time ------------------- */

  prop = RNA_def_property(srna, "override_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Override Frame",
                           "Whether to use a custom frame for looking up data in the cache file,"
                           " instead of using the current scene frame");
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");

  prop = RNA_def_property(srna, "frame", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "frame");
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop,
                           "Frame",
                           "The time to use for looking up the data in the cache file,"
                           " or to determine which file to use in a file sequence");
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");

  prop = RNA_def_property(srna, "frame_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "frame_offset");
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop,
                           "Frame Offset",
                           "Subtracted from the current frame to use for "
                           "looking up the data in the cache file, or to "
                           "determine which file to use in a file sequence");
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");

  /* ----------------- Axis Conversion ----------------- */

  prop = RNA_def_property(srna, "forward_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "forward_axis");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(prop, "Forward", "");
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");

  prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "up_axis");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(prop, "Up", "");
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_range(prop, 0.0001f, 1000.0f);
  RNA_def_property_ui_text(
      prop,
      "Scale",
      "Value by which to enlarge or shrink the object with respect to the world's origin"
      " (only applicable through a Transform Cache constraint)");
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");

  /* object paths */
  prop = RNA_def_property(srna, "object_paths", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "object_paths", nullptr);
  RNA_def_property_collection_funcs(prop,
                                    "rna_CacheFile_object_paths_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "CacheObjectPath");
  RNA_def_property_srna(prop, "CacheObjectPaths");
  RNA_def_property_ui_text(
      prop, "Object Paths", "Paths of the objects inside the Alembic archive");

  /* ----------------- Alembic Velocity Attribute ----------------- */

  prop = RNA_def_property(srna, "velocity_name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Velocity Attribute",
                           "Name of the Alembic attribute used for generating motion blur data");
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "velocity_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "velocity_unit");
  RNA_def_property_enum_items(prop, rna_enum_velocity_unit_items);
  RNA_def_property_ui_text(
      prop,
      "Velocity Unit",
      "Define how the velocity vectors are interpreted with regard to time, 'frame' means "
      "the delta time is 1 frame, 'second' means the delta time is 1 / FPS");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, 0, "rna_CacheFile_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  /* ----------------- Alembic Layers ----------------- */

  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "layers", nullptr);
  RNA_def_property_struct_type(prop, "CacheFileLayer");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Cache Layers", "Layers of the cache");
  rna_def_cachefile_layers(brna, prop);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "active_layer");
  RNA_def_property_int_funcs(prop,
                             "rna_CacheFile_active_layer_index_get",
                             "rna_CacheFile_active_layer_index_set",
                             "rna_CacheFile_active_layer_index_range");

  RNA_define_lib_overridable(false);

  rna_def_cachefile_object_paths(brna, prop);

  rna_def_animdata_common(srna);
}

void RNA_def_cachefile(BlenderRNA *brna)
{
  rna_def_cachefile(brna);
  rna_def_alembic_object_path(brna);
  rna_def_cachefile_layer(brna);
}

#endif
