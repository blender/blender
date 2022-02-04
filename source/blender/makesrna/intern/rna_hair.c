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

#include "rna_internal.h"

#include "DNA_hair_types.h"

#include "BLI_math_base.h"
#include "BLI_string.h"

#ifdef RNA_RUNTIME

#  include "BLI_math_vector.h"

#  include "BKE_attribute.h"
#  include "BKE_hair.h"

#  include "DEG_depsgraph.h"

#  include "WM_api.h"
#  include "WM_types.h"

static Hair *rna_hair(PointerRNA *ptr)
{
  return (Hair *)ptr->owner_id;
}

static int rna_Hair_curve_offset_data_length(PointerRNA *ptr)
{
  const Hair *curves = rna_hair(ptr);
  return curves->geometry.curve_size + 1;
}

static void rna_Hair_curve_offset_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  const Hair *curves = rna_hair(ptr);
  rna_iterator_array_begin(iter,
                           (void *)curves->geometry.offsets,
                           sizeof(int),
                           curves->geometry.curve_size + 1,
                           false,
                           NULL);
}

static int rna_CurvePoint_index_get(PointerRNA *ptr)
{
  const Hair *hair = rna_hair(ptr);
  const float(*co)[3] = ptr->data;
  return (int)(co - hair->geometry.position);
}

static void rna_CurvePoint_location_get(PointerRNA *ptr, float value[3])
{
  copy_v3_v3(value, (const float *)ptr->data);
}

static void rna_CurvePoint_location_set(PointerRNA *ptr, const float value[3])
{
  copy_v3_v3((float *)ptr->data, value);
}

static float rna_CurvePoint_radius_get(PointerRNA *ptr)
{
  const Hair *hair = rna_hair(ptr);
  if (hair->geometry.radius == NULL) {
    return 0.0f;
  }
  const float(*co)[3] = ptr->data;
  return hair->geometry.radius[co - hair->geometry.position];
}

static void rna_CurvePoint_radius_set(PointerRNA *ptr, float value)
{
  const Hair *hair = rna_hair(ptr);
  if (hair->geometry.radius == NULL) {
    return;
  }
  const float(*co)[3] = ptr->data;
  hair->geometry.radius[co - hair->geometry.position] = value;
}

static char *rna_CurvePoint_path(PointerRNA *ptr)
{
  return BLI_sprintfN("points[%d]", rna_CurvePoint_index_get(ptr));
}

static int rna_CurveSlice_index_get(PointerRNA *ptr)
{
  Hair *hair = rna_hair(ptr);
  return (int)((int *)ptr->data - hair->geometry.offsets);
}

static char *rna_CurveSlice_path(PointerRNA *ptr)
{
  return BLI_sprintfN("curves[%d]", rna_CurveSlice_index_get(ptr));
}

static void rna_CurveSlice_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Hair *hair = rna_hair(ptr);
  const int *offset_ptr = (int *)ptr->data;
  const int offset = *offset_ptr;
  const int size = *(offset_ptr + 1) - offset;
  float(*co)[3] = hair->geometry.position + *offset_ptr;
  rna_iterator_array_begin(iter, co, sizeof(float[3]), size, 0, NULL);
}

static int rna_CurveSlice_first_point_index_get(PointerRNA *ptr)
{
  const int *offset_ptr = (int *)ptr->data;
  return *offset_ptr;
}

static int rna_CurveSlice_points_length_get(PointerRNA *ptr)
{
  const int *offset_ptr = (int *)ptr->data;
  const int offset = *offset_ptr;
  return *(offset_ptr + 1) - offset;
}

static void rna_Hair_update_data(struct Main *UNUSED(bmain),
                                 struct Scene *UNUSED(scene),
                                 PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  /* cheating way for importers to avoid slow updates */
  if (id->us > 0) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

#else

static void rna_def_hair_point(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurvePoint", NULL);
  RNA_def_struct_ui_text(srna, "Curve Point", "Curve curve control point");
  RNA_def_struct_path_func(srna, "rna_CurvePoint_path");

  prop = RNA_def_property(srna, "position", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(
      prop, "rna_CurvePoint_location_get", "rna_CurvePoint_location_set", NULL);
  RNA_def_property_ui_text(prop, "Position", "");
  RNA_def_property_update(prop, 0, "rna_Hair_update_data");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(
      prop, "rna_CurvePoint_radius_get", "rna_CurvePoint_radius_set", NULL);
  RNA_def_property_ui_text(prop, "Radius", "");
  RNA_def_property_update(prop, 0, "rna_Hair_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurvePoint_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this points");
}

static void rna_def_hair_curve(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurveSlice", NULL);
  RNA_def_struct_ui_text(srna, "Curve Slice", "A single curve from a curves data-block");
  RNA_def_struct_path_func(srna, "rna_CurveSlice_path");

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurvePoint");
  RNA_def_property_ui_text(prop, "Points", "Control points of the curve");
  RNA_def_property_collection_funcs(prop,
                                    "rna_CurveSlice_points_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_CurveSlice_points_length_get",
                                    NULL,
                                    NULL,
                                    NULL);

  prop = RNA_def_property(srna, "first_point_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurveSlice_first_point_index_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "First Point Index", "The index of this curve's first control point");

  prop = RNA_def_property(srna, "points_length", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurveSlice_points_length_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Number of Points", "Number of control points in the curve");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurveSlice_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this curve");
}

static void rna_def_hair(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Hair", "ID");
  RNA_def_struct_ui_text(srna, "Hair", "Hair data-block for hair curves");
  RNA_def_struct_ui_icon(srna, ICON_HAIR_DATA);

  /* Point and Curve RNA API helpers. */

  prop = RNA_def_property(srna, "curves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "geometry.offsets", "geometry.curve_size");
  RNA_def_property_struct_type(prop, "CurveSlice");
  RNA_def_property_ui_text(prop, "Curves", "All hair curves");

  /* TODO: better solution for (*co)[3] parsing issue. */

  RNA_define_verify_sdna(0);
  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "geometry.position", "geometry.point_size");
  RNA_def_property_struct_type(prop, "CurvePoint");
  RNA_def_property_ui_text(prop, "Points", "Control points of all hair curves");
  RNA_define_verify_sdna(1);

  /* Direct access to built-in attributes. */

  RNA_define_verify_sdna(0);
  prop = RNA_def_property(srna, "position_data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "geometry.position", "geometry.point_size");
  RNA_def_property_struct_type(prop, "FloatVectorAttributeValue");
  RNA_def_property_update(prop, 0, "rna_Hair_update_data");
  RNA_define_verify_sdna(1);

  prop = RNA_def_property(srna, "curve_offset_data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "geometry.offsets", NULL);
  RNA_def_property_struct_type(prop, "IntAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Hair_curve_offset_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Hair_curve_offset_data_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_update(prop, 0, "rna_Hair_update_data");

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");

  /* attributes */
  rna_def_attributes_common(srna);

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_hair(BlenderRNA *brna)
{
  rna_def_hair_point(brna);
  rna_def_hair_curve(brna);
  rna_def_hair(brna);
}

#endif
