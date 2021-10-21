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

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_asset_types.h"
#include "DNA_defs.h"
#include "DNA_space_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#  include "BKE_asset.h"
#  include "BKE_asset_library.h"
#  include "BKE_context.h"
#  include "BKE_idprop.h"

#  include "BLI_listbase.h"
#  include "BLI_uuid.h"

#  include "ED_asset.h"
#  include "ED_fileselect.h"

#  include "RNA_access.h"

static bool rna_AssetMetaData_editable_from_owner_id(const ID *owner_id,
                                                     const AssetMetaData *asset_data,
                                                     const char **r_info)
{
  if (owner_id && asset_data && (owner_id->asset_data == asset_data)) {
    return true;
  }

  if (r_info) {
    *r_info =
        "Asset metadata from external asset libraries can't be edited, only assets stored in the "
        "current file can";
  }
  return false;
}

int rna_AssetMetaData_editable(PointerRNA *ptr, const char **r_info)
{
  AssetMetaData *asset_data = ptr->data;

  return rna_AssetMetaData_editable_from_owner_id(ptr->owner_id, asset_data, r_info) ?
             PROP_EDITABLE :
             0;
}

static int rna_AssetTag_editable(PointerRNA *ptr, const char **r_info)
{
  AssetTag *asset_tag = ptr->data;
  ID *owner_id = ptr->owner_id;
  if (owner_id && owner_id->asset_data) {
    BLI_assert_msg(BLI_findindex(&owner_id->asset_data->tags, asset_tag) != -1,
                   "The owner of the asset tag pointer is not the asset ID containing the tag");
    UNUSED_VARS_NDEBUG(asset_tag);
  }

  return rna_AssetMetaData_editable_from_owner_id(ptr->owner_id, owner_id->asset_data, r_info) ?
             PROP_EDITABLE :
             0;
}

static AssetTag *rna_AssetMetaData_tag_new(
    ID *id, AssetMetaData *asset_data, ReportList *reports, const char *name, bool skip_if_exists)
{
  const char *disabled_info = NULL;
  if (!rna_AssetMetaData_editable_from_owner_id(id, asset_data, &disabled_info)) {
    BKE_report(reports, RPT_WARNING, disabled_info);
    return NULL;
  }

  AssetTag *tag = NULL;

  if (skip_if_exists) {
    struct AssetTagEnsureResult result = BKE_asset_metadata_tag_ensure(asset_data, name);

    if (!result.is_new) {
      BKE_reportf(
          reports, RPT_WARNING, "Tag '%s' already present for given asset", result.tag->name);
      /* Report, but still return valid item. */
    }
    tag = result.tag;
  }
  else {
    tag = BKE_asset_metadata_tag_add(asset_data, name);
  }

  return tag;
}

static void rna_AssetMetaData_tag_remove(ID *id,
                                         AssetMetaData *asset_data,
                                         ReportList *reports,
                                         PointerRNA *tag_ptr)
{
  const char *disabled_info = NULL;
  if (!rna_AssetMetaData_editable_from_owner_id(id, asset_data, &disabled_info)) {
    BKE_report(reports, RPT_WARNING, disabled_info);
    return;
  }

  AssetTag *tag = tag_ptr->data;
  if (BLI_findindex(&asset_data->tags, tag) == -1) {
    BKE_reportf(reports, RPT_ERROR, "Tag '%s' not found in given asset", tag->name);
    return;
  }

  BKE_asset_metadata_tag_remove(asset_data, tag);
  RNA_POINTER_INVALIDATE(tag_ptr);
}

static IDProperty **rna_AssetMetaData_idprops(PointerRNA *ptr)
{
  AssetMetaData *asset_data = ptr->data;
  return &asset_data->properties;
}

static void rna_AssetMetaData_description_get(PointerRNA *ptr, char *value)
{
  AssetMetaData *asset_data = ptr->data;

  if (asset_data->description) {
    strcpy(value, asset_data->description);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_AssetMetaData_description_length(PointerRNA *ptr)
{
  AssetMetaData *asset_data = ptr->data;
  return asset_data->description ? strlen(asset_data->description) : 0;
}

static void rna_AssetMetaData_description_set(PointerRNA *ptr, const char *value)
{
  AssetMetaData *asset_data = ptr->data;

  if (asset_data->description) {
    MEM_freeN(asset_data->description);
  }

  if (value[0]) {
    asset_data->description = BLI_strdup(value);
  }
  else {
    asset_data->description = NULL;
  }
}

static void rna_AssetMetaData_active_tag_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  const AssetMetaData *asset_data = ptr->data;
  *min = *softmin = 0;
  *max = *softmax = MAX2(asset_data->tot_tags - 1, 0);
}

static void rna_AssetMetaData_catalog_id_get(PointerRNA *ptr, char *value)
{
  const AssetMetaData *asset_data = ptr->data;
  BLI_uuid_format(value, asset_data->catalog_id);
}

static int rna_AssetMetaData_catalog_id_length(PointerRNA *UNUSED(ptr))
{
  return UUID_STRING_LEN - 1;
}

static void rna_AssetMetaData_catalog_id_set(PointerRNA *ptr, const char *value)
{
  AssetMetaData *asset_data = ptr->data;
  bUUID new_uuid;

  if (value[0] == '\0') {
    BKE_asset_metadata_catalog_id_clear(asset_data);
    return;
  }

  if (!BLI_uuid_parse_string(&new_uuid, value)) {
    // TODO(Sybren): raise ValueError exception once that's possible from an RNA setter.
    printf("UUID %s not formatted correctly, ignoring new value\n", value);
    return;
  }

  /* This just sets the new UUID and clears the catalog simple name. The actual
   * catalog simple name will be updated by some update function, as it
   * needs the asset library from the context. */
  /* TODO(Sybren): write that update function. */
  BKE_asset_metadata_catalog_id_set(asset_data, new_uuid, "");
}

void rna_AssetMetaData_catalog_id_update(struct bContext *C, struct PointerRNA *ptr)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  if (sfile == NULL) {
    /* Until there is a proper Asset Service available, it's only possible to get the asset library
     * from within the asset browser context. */
    return;
  }

  AssetLibrary *asset_library = ED_fileselect_active_asset_library_get(sfile);
  if (asset_library == NULL) {
    /* The SpaceFile may not be an asset browser but a regular file browser. */
    return;
  }

  AssetMetaData *asset_data = ptr->data;
  BKE_asset_library_refresh_catalog_simplename(asset_library, asset_data);
}

static PointerRNA rna_AssetHandle_file_data_get(PointerRNA *ptr)
{
  AssetHandle *asset_handle = ptr->data;
  /* Have to cast away const, but the file entry API doesn't allow modifications anyway. */
  return rna_pointer_inherit_refine(
      ptr, &RNA_FileSelectEntry, (FileDirEntry *)asset_handle->file_data);
}

static void rna_AssetHandle_file_data_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          struct ReportList *UNUSED(reports))
{
  AssetHandle *asset_handle = ptr->data;
  asset_handle->file_data = value.data;
}

static void rna_AssetHandle_get_full_library_path(
    // AssetHandle *asset,
    bContext *C,
    FileDirEntry *asset_file,
    AssetLibraryReference *library,
    char r_result[/*FILE_MAX_LIBEXTRA*/])
{
  AssetHandle asset = {.file_data = asset_file};
  ED_asset_handle_get_full_library_path(C, library, &asset, r_result);
}

static PointerRNA rna_AssetHandle_local_id_get(PointerRNA *ptr)
{
  const AssetHandle *asset = ptr->data;
  ID *id = ED_asset_handle_get_local_id(asset);
  return rna_pointer_inherit_refine(ptr, &RNA_ID, id);
}

const EnumPropertyItem *rna_asset_library_reference_itemf(bContext *UNUSED(C),
                                                          PointerRNA *UNUSED(ptr),
                                                          PropertyRNA *UNUSED(prop),
                                                          bool *r_free)
{
  const EnumPropertyItem *items = ED_asset_library_reference_to_rna_enum_itemf();
  if (!items) {
    *r_free = false;
  }

  *r_free = true;
  return items;
}

#else

static void rna_def_asset_tag(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AssetTag", NULL);
  RNA_def_struct_ui_text(srna, "Asset Tag", "User defined tag (name token)");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_editable_func(prop, "rna_AssetTag_editable");
  RNA_def_property_string_maxlength(prop, MAX_NAME);
  RNA_def_property_ui_text(prop, "Name", "The identifier that makes up this tag");
  RNA_def_struct_name_property(srna, prop);
}

static void rna_def_asset_tags_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "AssetTags");
  srna = RNA_def_struct(brna, "AssetTags", NULL);
  RNA_def_struct_sdna(srna, "AssetMetaData");
  RNA_def_struct_ui_text(srna, "Asset Tags", "Collection of custom asset tags");

  /* Tag collection */
  func = RNA_def_function(srna, "new", "rna_AssetMetaData_tag_new");
  RNA_def_function_ui_description(func, "Add a new tag to this asset");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_boolean(func,
                         "skip_if_exists",
                         false,
                         "Skip if Exists",
                         "Do not add a new tag if one of the same type already exists");
  /* return type */
  parm = RNA_def_pointer(func, "tag", "AssetTag", "", "New tag");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_AssetMetaData_tag_remove");
  RNA_def_function_ui_description(func, "Remove an existing tag from this asset");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  /* tag to remove */
  parm = RNA_def_pointer(func, "tag", "AssetTag", "", "Removed tag");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_asset_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AssetMetaData", NULL);
  RNA_def_struct_ui_text(srna, "Asset Data", "Additional data stored for an asset data-block");
  //  RNA_def_struct_ui_icon(srna, ICON_ASSET); /* TODO: Icon doesn't exist! */
  /* The struct has custom properties, but no pointer properties to other IDs! */
  RNA_def_struct_idprops_func(srna, "rna_AssetMetaData_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES); /* Mandatory! */

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_editable_func(prop, "rna_AssetMetaData_editable");
  RNA_def_property_string_funcs(prop,
                                "rna_AssetMetaData_description_get",
                                "rna_AssetMetaData_description_length",
                                "rna_AssetMetaData_description_set");
  RNA_def_property_ui_text(
      prop, "Description", "A description of the asset to be displayed for the user");

  prop = RNA_def_property(srna, "tags", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "AssetTag");
  RNA_def_property_editable_func(prop, "rna_AssetMetaData_editable");
  RNA_def_property_ui_text(prop,
                           "Tags",
                           "Custom tags (name tokens) for the asset, used for filtering and "
                           "general asset management");
  rna_def_asset_tags_api(brna, prop);

  prop = RNA_def_property(srna, "active_tag", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_AssetMetaData_active_tag_range");
  RNA_def_property_ui_text(prop, "Active Tag", "Index of the tag set for editing");

  prop = RNA_def_property(srna, "catalog_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_AssetMetaData_catalog_id_get",
                                "rna_AssetMetaData_catalog_id_length",
                                "rna_AssetMetaData_catalog_id_set");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_AssetMetaData_catalog_id_update");
  RNA_def_property_ui_text(prop,
                           "Catalog UUID",
                           "Identifier for the asset's catalog, used by Blender to look up the "
                           "asset's catalog path. Must be a UUID according to RFC4122");

  prop = RNA_def_property(srna, "catalog_simple_name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Catalog Simple Name",
                           "Simple name of the asset's catalog, for debugging and "
                           "data recovery purposes");
}

static void rna_def_asset_handle_api(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "get_full_library_path", "rna_AssetHandle_get_full_library_path");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  /* TODO temporarily static function, for until .py can receive the asset handle from context
   * properly. `asset_file_handle` should go away too then. */
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "asset_file_handle", "FileSelectEntry", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func,
                         "asset_library_ref",
                         "AssetLibraryReference",
                         "",
                         "The asset library containing the given asset, only valid if the asset "
                         "library is external (i.e. not the \"Current File\" one");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_string(func, "result", NULL, FILE_MAX_LIBEXTRA, "result", "");
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0);
  RNA_def_function_output(func, parm);
}

static void rna_def_asset_handle(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AssetHandle", "PropertyGroup");
  RNA_def_struct_ui_text(srna, "Asset Handle", "Reference to some asset");

  /* TODO It is super ugly to expose the file data here. We have to do it though so the asset view
   * template can populate a RNA collection with asset-handles, which are just file entries
   * currently. A proper design is being worked on. */
  prop = RNA_def_property(srna, "file_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "FileSelectEntry");
  RNA_def_property_pointer_funcs(
      prop, "rna_AssetHandle_file_data_get", "rna_AssetHandle_file_data_set", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "File Entry", "TEMPORARY, DO NOT USE - File data used to refer to the asset");

  prop = RNA_def_property(srna, "local_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_pointer_funcs(prop, "rna_AssetHandle_local_id_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop,
                           "",
                           "The local data-block this asset represents; only valid if that is a "
                           "data-block in this file");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  rna_def_asset_handle_api(srna);
}

static void rna_def_asset_library_reference(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "AssetLibraryReference", NULL);
  RNA_def_struct_ui_text(
      srna, "Asset Library Reference", "Identifier to refer to the asset library");
}

/**
 * \note the UI text and updating has to be set by the caller.
 */
PropertyRNA *rna_def_asset_library_reference_common(struct StructRNA *srna,
                                                    const char *get,
                                                    const char *set)
{
  PropertyRNA *prop = RNA_def_property(srna, "asset_library_ref", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, DummyRNA_NULL_items);
  RNA_def_property_enum_funcs(prop, get, set, "rna_asset_library_reference_itemf");

  return prop;
}

void RNA_def_asset(BlenderRNA *brna)
{
  RNA_define_animate_sdna(false);

  rna_def_asset_tag(brna);
  rna_def_asset_data(brna);
  rna_def_asset_library_reference(brna);
  rna_def_asset_handle(brna);

  RNA_define_animate_sdna(true);
}

#endif
