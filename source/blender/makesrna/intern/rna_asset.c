/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#  include "AS_asset_library.h"

#  include "BKE_asset.h"
#  include "BKE_asset_library_custom.h"
#  include "BKE_context.h"
#  include "BKE_idprop.h"

#  include "BLI_listbase.h"
#  include "BLI_uuid.h"

#  include "ED_asset.h"
#  include "ED_fileselect.h"

#  include "WM_api.h"
#  include "WM_types.h"

#  include "RNA_access.h"

static char *rna_AssetMetaData_path(const PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("asset_data");
}

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

static char *rna_AssetTag_path(const PointerRNA *ptr)
{
  const AssetTag *asset_tag = ptr->data;
  char asset_tag_name_esc[sizeof(asset_tag->name) * 2];
  BLI_str_escape(asset_tag_name_esc, asset_tag->name, sizeof(asset_tag_name_esc));
  return BLI_sprintfN("asset_data.tags[\"%s\"]", asset_tag_name_esc);
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

static void rna_AssetMetaData_author_get(PointerRNA *ptr, char *value)
{
  AssetMetaData *asset_data = ptr->data;

  if (asset_data->author) {
    strcpy(value, asset_data->author);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_AssetMetaData_author_length(PointerRNA *ptr)
{
  AssetMetaData *asset_data = ptr->data;
  return asset_data->author ? strlen(asset_data->author) : 0;
}

static void rna_AssetMetaData_author_set(PointerRNA *ptr, const char *value)
{
  AssetMetaData *asset_data = ptr->data;

  if (asset_data->author) {
    MEM_freeN(asset_data->author);
  }

  if (value[0]) {
    asset_data->author = BLI_strdup(value);
  }
  else {
    asset_data->author = NULL;
  }
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

static void rna_AssetMetaData_copyright_get(PointerRNA *ptr, char *value)
{
  AssetMetaData *asset_data = ptr->data;

  if (asset_data->copyright) {
    strcpy(value, asset_data->copyright);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_AssetMetaData_copyright_length(PointerRNA *ptr)
{
  AssetMetaData *asset_data = ptr->data;
  return asset_data->copyright ? strlen(asset_data->copyright) : 0;
}

static void rna_AssetMetaData_copyright_set(PointerRNA *ptr, const char *value)
{
  AssetMetaData *asset_data = ptr->data;

  if (asset_data->copyright) {
    MEM_freeN(asset_data->copyright);
  }

  if (value[0]) {
    asset_data->copyright = BLI_strdup(value);
  }
  else {
    asset_data->copyright = NULL;
  }
}

static void rna_AssetMetaData_license_get(PointerRNA *ptr, char *value)
{
  AssetMetaData *asset_data = ptr->data;

  if (asset_data->license) {
    strcpy(value, asset_data->license);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_AssetMetaData_license_length(PointerRNA *ptr)
{
  AssetMetaData *asset_data = ptr->data;
  return asset_data->license ? strlen(asset_data->license) : 0;
}

static void rna_AssetMetaData_license_set(PointerRNA *ptr, const char *value)
{
  AssetMetaData *asset_data = ptr->data;

  if (asset_data->license) {
    MEM_freeN(asset_data->license);
  }

  if (value[0]) {
    asset_data->license = BLI_strdup(value);
  }
  else {
    asset_data->license = NULL;
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
    /* TODO(@sybren): raise ValueError exception once that's possible from an RNA setter. */
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
  AS_asset_library_refresh_catalog_simplename(asset_library, asset_data);
}

static void rna_CustomAssetLibraryDefinition_name_set(PointerRNA *ptr, const char *value)
{
  CustomAssetLibraryDefinition *library = (CustomAssetLibraryDefinition *)ptr->data;

  /* We can't cleanly access the owning listbase here, but reconstructing the list from the link is
   * fine. */
  ListBase asset_libraries = BLI_listbase_from_link((Link *)library);
  BKE_asset_library_custom_name_set(&asset_libraries, library, value);
}

static void rna_CustomAssetLibraryDefinition_path_set(PointerRNA *ptr, const char *value)
{
  CustomAssetLibraryDefinition *library = (CustomAssetLibraryDefinition *)ptr->data;

  char dirpath[FILE_MAX];
  BLI_strncpy(dirpath, value, sizeof(dirpath));
  if (BLI_is_file(dirpath)) {
    BLI_path_parent_dir(dirpath);
  }
  BKE_asset_library_custom_path_set(library, dirpath);
}

void rna_AssetLibrary_settings_update(Main *UNUSED(bmain),
                                      Scene *UNUSED(scene),
                                      PointerRNA *UNUSED(ptr))
{
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIBRARY, NULL);
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
    FileDirEntry *asset_file,
    char r_result[/*FILE_MAX_LIBEXTRA*/])
{
  AssetHandle asset = {.file_data = asset_file};
  ED_asset_handle_get_full_library_path(&asset, r_result);
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
  const EnumPropertyItem *items = ED_asset_library_reference_to_rna_enum_itemf(true);
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
  RNA_def_struct_path_func(srna, "rna_AssetTag_path");
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
  RNA_def_struct_path_func(srna, "rna_AssetMetaData_path");
  RNA_def_struct_ui_text(srna, "Asset Data", "Additional data stored for an asset data-block");
  //  RNA_def_struct_ui_icon(srna, ICON_ASSET); /* TODO: Icon doesn't exist! */
  /* The struct has custom properties, but no pointer properties to other IDs! */
  RNA_def_struct_idprops_func(srna, "rna_AssetMetaData_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES); /* Mandatory! */

  prop = RNA_def_property(srna, "author", PROP_STRING, PROP_NONE);
  RNA_def_property_editable_func(prop, "rna_AssetMetaData_editable");
  RNA_def_property_string_funcs(prop,
                                "rna_AssetMetaData_author_get",
                                "rna_AssetMetaData_author_length",
                                "rna_AssetMetaData_author_set");
  RNA_def_property_ui_text(prop, "Author", "Name of the creator of the asset");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_editable_func(prop, "rna_AssetMetaData_editable");
  RNA_def_property_string_funcs(prop,
                                "rna_AssetMetaData_description_get",
                                "rna_AssetMetaData_description_length",
                                "rna_AssetMetaData_description_set");
  RNA_def_property_ui_text(
      prop, "Description", "A description of the asset to be displayed for the user");

  prop = RNA_def_property(srna, "copyright", PROP_STRING, PROP_NONE);
  RNA_def_property_editable_func(prop, "rna_AssetMetaData_editable");
  RNA_def_property_string_funcs(prop,
                                "rna_AssetMetaData_copyright_get",
                                "rna_AssetMetaData_copyright_length",
                                "rna_AssetMetaData_copyright_set");
  RNA_def_property_ui_text(
      prop,
      "Copyright",
      "Copyright notice for this asset. An empty copyright notice does not necessarily indicate "
      "that this is copyright-free. Contact the author if any clarification is needed");

  prop = RNA_def_property(srna, "license", PROP_STRING, PROP_NONE);
  RNA_def_property_editable_func(prop, "rna_AssetMetaData_editable");
  RNA_def_property_string_funcs(prop,
                                "rna_AssetMetaData_license_get",
                                "rna_AssetMetaData_license_length",
                                "rna_AssetMetaData_license_set");
  RNA_def_property_ui_text(prop,
                           "License",
                           "The type of license this asset is distributed under. An empty license "
                           "name does not necessarily indicate that this is free of licensing "
                           "terms. Contact the author if any clarification is needed");

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
  /* TODO temporarily static function, for until .py can receive the asset handle from context
   * properly. `asset_file_handle` should go away too then. */
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_pointer(func, "asset_file_handle", "FileSelectEntry", "", "");
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

static void rna_def_asset_representation(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "AssetRepresentation", NULL);
  RNA_def_struct_ui_text(srna,
                         "Asset Representation",
                         "Information about an entity that makes it possible for the asset system "
                         "to deal with the entity as asset");
}

static void rna_def_asset_catalog_path(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "AssetCatalogPath", NULL);
  RNA_def_struct_ui_text(srna, "Catalog Path", "");
}

static void rna_def_asset_library_reference(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "AssetLibraryReference", NULL);
  RNA_def_struct_ui_text(
      srna, "Asset Library Reference", "Identifier to refer to the asset library");
}

static void rna_def_asset_library_reference_custom(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CustomAssetLibraryDefinition", NULL);
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(
      srna, "Asset Library", "Settings to define a reusable library for Asset Browsers to use");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Name", "Identifier (not necessarily unique) for the asset library");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CustomAssetLibraryDefinition_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, 0, "rna_AssetLibrary_settings_update");

  prop = RNA_def_property(srna, "path", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_ui_text(
      prop, "Path", "Path to a directory with .blend files to use as an asset library");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CustomAssetLibraryDefinition_path_set");
  RNA_def_property_update(prop, 0, "rna_AssetLibrary_settings_update");

  static const EnumPropertyItem import_method_items[] = {
      {ASSET_IMPORT_LINK, "LINK", 0, "Link", "Import the assets as linked data-block"},
      {ASSET_IMPORT_APPEND,
       "APPEND",
       0,
       "Append",
       "Import the assets as copied data-block, with no link to the original asset data-block"},
      {ASSET_IMPORT_APPEND_REUSE,
       "APPEND_REUSE",
       0,
       "Append (Reuse Data)",
       "Import the assets as copied data-block while avoiding multiple copies of nested, "
       "typically heavy data. For example the textures of a material asset, or the mesh of an "
       "object asset, don't have to be copied every time this asset is imported. The instances of "
       "the asset share the data instead"},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(srna, "import_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, import_method_items);
  RNA_def_property_ui_text(
      prop,
      "Default Import Method",
      "Determine how the asset will be imported, unless overridden by the Asset Browser");
  RNA_def_property_update(prop, 0, "rna_AssetLibrary_settings_update");
}

PropertyRNA *rna_def_asset_library_reference_common(StructRNA *srna,
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
  rna_def_asset_library_reference_custom(brna);
  rna_def_asset_handle(brna);
  rna_def_asset_representation(brna);
  rna_def_asset_catalog_path(brna);

  RNA_define_animate_sdna(true);
}

#endif
