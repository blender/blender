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

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#  include "BKE_asset.h"
#  include "BKE_idprop.h"

#  include "BLI_listbase.h"

#  include "RNA_access.h"

static AssetTag *rna_AssetMetaData_tag_new(AssetMetaData *asset_data,
                                           ReportList *reports,
                                           const char *name,
                                           bool skip_if_exists)
{
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

static void rna_AssetMetaData_tag_remove(AssetMetaData *asset_data,
                                         ReportList *reports,
                                         PointerRNA *tag_ptr)
{
  AssetTag *tag = tag_ptr->data;
  if (BLI_findindex(&asset_data->tags, tag) == -1) {
    BKE_reportf(reports, RPT_ERROR, "Tag '%s' not found in given asset", tag->name);
    return;
  }

  BKE_asset_metadata_tag_remove(asset_data, tag);
  RNA_POINTER_INVALIDATE(tag_ptr);
}

static IDProperty *rna_AssetMetaData_idprops(PointerRNA *ptr, bool create)
{
  AssetMetaData *asset_data = ptr->data;

  if (create && !asset_data->properties) {
    IDPropertyTemplate val = {0};
    asset_data->properties = IDP_New(IDP_GROUP, &val, "RNA_AssetMetaData group");
  }

  return asset_data->properties;
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

#else

static void rna_def_asset_tag(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AssetTag", NULL);
  RNA_def_struct_ui_text(srna, "Asset Tag", "User defined tag (name token)");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
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
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
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
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
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
  //  RNA_def_struct_ui_icon(srna, ICON_ASSET); /* TODO: Icon doesn't exist!. */
  /* The struct has custom properties, but no pointer properties to other IDs! */
  RNA_def_struct_idprops_func(srna, "rna_AssetMetaData_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES); /* Mandatory! */

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_AssetMetaData_description_get",
                                "rna_AssetMetaData_description_length",
                                "rna_AssetMetaData_description_set");
  RNA_def_property_ui_text(
      prop, "Description", "A description of the asset to be displayed for the user");

  prop = RNA_def_property(srna, "tags", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "AssetTag");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Tags",
                           "Custom tags (name tokens) for the asset, used for filtering and "
                           "general asset management");
  rna_def_asset_tags_api(brna, prop);

  prop = RNA_def_property(srna, "active_tag", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_AssetMetaData_active_tag_range");
  RNA_def_property_ui_text(prop, "Active Tag", "Index of the tag set for editing");
}

void RNA_def_asset(BlenderRNA *brna)
{
  RNA_define_animate_sdna(false);

  rna_def_asset_tag(brna);
  rna_def_asset_data(brna);

  RNA_define_animate_sdna(true);
}

#endif
