/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_curves_types.h"

#include "BLI_math_base.h"
#include "BLI_string.h"

#include "WM_types.h"

const EnumPropertyItem rna_enum_curves_types[] = {
    {CURVE_TYPE_CATMULL_ROM, "CATMULL_ROM", 0, "Catmull Rom", ""},
    {CURVE_TYPE_POLY, "POLY", 0, "Poly", ""},
    {CURVE_TYPE_BEZIER, "BEZIER", 0, "Bezier", ""},
    {CURVE_TYPE_NURBS, "NURBS", 0, "NURBS", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_curve_normal_modes[] = {
    {NORMAL_MODE_MINIMUM_TWIST,
     "MINIMUM_TWIST",
     ICON_NONE,
     "Minimum Twist",
     "Calculate normals with the smallest twist around the curve tangent across the whole curve"},
    {NORMAL_MODE_Z_UP,
     "Z_UP",
     ICON_NONE,
     "Z Up",
     "Calculate normals perpendicular to the Z axis and the curve tangent. If a series of points "
     "is vertical, the X axis is used"},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BLI_math_vector.h"

#  include "BKE_attribute.h"
#  include "BKE_curves.h"

#  include "DEG_depsgraph.h"

#  include "WM_api.h"
#  include "WM_types.h"

static Curves *rna_curves(const PointerRNA *ptr)
{
  return (Curves *)ptr->owner_id;
}

static int rna_Curves_curve_offset_data_length(PointerRNA *ptr)
{
  const Curves *curves = rna_curves(ptr);
  return curves->geometry.curve_num + 1;
}

static void rna_Curves_curve_offset_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  const Curves *curves = rna_curves(ptr);
  rna_iterator_array_begin(iter,
                           (void *)curves->geometry.curve_offsets,
                           sizeof(int),
                           curves->geometry.curve_num + 1,
                           false,
                           NULL);
}

static float (*get_curves_positions(Curves *curves))[3]
{
  return (float(*)[3])CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT3, "position");
}

static const float (*get_curves_positions_const(const Curves *curves))[3]
{
  return (const float(*)[3])CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT3, "position");
}

static int rna_CurvePoint_index_get_const(const PointerRNA *ptr)
{
  const Curves *curves = rna_curves(ptr);
  const float(*co)[3] = ptr->data;
  const float(*positions)[3] = get_curves_positions_const(curves);
  return (int)(co - positions);
}

static int rna_Curves_position_data_length(PointerRNA *ptr)
{
  const Curves *curves = rna_curves(ptr);
  return curves->geometry.point_num;
}

int rna_Curves_position_data_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Curves *curves = rna_curves(ptr);
  if (index < 0 || index >= curves->geometry.point_num) {
    return false;
  }
  r_ptr->owner_id = &curves->id;
  r_ptr->type = &RNA_FloatVectorAttributeValue;
  r_ptr->data = &get_curves_positions(curves)[index];
  return true;
}

static void rna_Curves_position_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  rna_iterator_array_begin(iter,
                           get_curves_positions(curves),
                           sizeof(float[3]),
                           curves->geometry.point_num,
                           false,
                           NULL);
}

static int rna_CurvePoint_index_get(PointerRNA *ptr)
{
  return rna_CurvePoint_index_get_const(ptr);
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
  const Curves *curves = rna_curves(ptr);
  const float *radii = (const float *)CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT, "radius");
  if (radii == NULL) {
    return 0.0f;
  }
  return radii[rna_CurvePoint_index_get_const(ptr)];
}

static void rna_CurvePoint_radius_set(PointerRNA *ptr, float value)
{
  const Curves *curves = rna_curves(ptr);
  float *radii = (float *)CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT, "radius");
  if (radii == NULL) {
    return;
  }
  radii[rna_CurvePoint_index_get_const(ptr)] = value;
}

static char *rna_CurvePoint_path(const PointerRNA *ptr)
{
  return BLI_sprintfN("points[%d]", rna_CurvePoint_index_get_const(ptr));
}

int rna_Curves_points_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Curves *curves = rna_curves(ptr);
  if (index < 0 || index >= curves->geometry.point_num) {
    return false;
  }
  r_ptr->owner_id = &curves->id;
  r_ptr->type = &RNA_CurvePoint;
  r_ptr->data = &get_curves_positions(curves)[index];
  return true;
}

static int rna_CurveSlice_index_get_const(const PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  return (int)((int *)ptr->data - curves->geometry.curve_offsets);
}

static int rna_CurveSlice_index_get(PointerRNA *ptr)
{
  return rna_CurveSlice_index_get_const(ptr);
}

static char *rna_CurveSlice_path(const PointerRNA *ptr)
{
  return BLI_sprintfN("curves[%d]", rna_CurveSlice_index_get_const(ptr));
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

static void rna_CurveSlice_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  const int offset = rna_CurveSlice_first_point_index_get(ptr);
  const int size = rna_CurveSlice_points_length_get(ptr);
  float(*positions)[3] = (float(*)[3])CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT3, "position");
  float(*co)[3] = positions + offset;
  rna_iterator_array_begin(iter, co, sizeof(float[3]), size, 0, NULL);
}

static void rna_Curves_update_data(struct Main *UNUSED(bmain),
                                   struct Scene *UNUSED(scene),
                                   PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  /* Avoid updates for importers creating curves. */
  if (id->us > 0) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

void rna_Curves_update_draw(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  /* Avoid updates for importers creating curves. */
  if (id->us > 0) {
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

#else

static void rna_def_curves_point(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurvePoint", NULL);
  RNA_def_struct_ui_text(srna, "Curve Point", "Curve control point");
  RNA_def_struct_path_func(srna, "rna_CurvePoint_path");

  prop = RNA_def_property(srna, "position", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(
      prop, "rna_CurvePoint_location_get", "rna_CurvePoint_location_set", NULL);
  RNA_def_property_ui_text(prop, "Position", "");
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(
      prop, "rna_CurvePoint_radius_get", "rna_CurvePoint_radius_set", NULL);
  RNA_def_property_ui_text(prop, "Radius", "");
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurvePoint_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this points");
}

static void rna_def_curves_curve(BlenderRNA *brna)
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

static void rna_def_curves(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Curves", "ID");
  RNA_def_struct_ui_text(srna, "Hair Curves", "Hair data-block for hair curves");
  RNA_def_struct_ui_icon(srna, ICON_CURVES_DATA);

  /* Point and Curve RNA API helpers. */

  prop = RNA_def_property(srna, "curves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "geometry.curve_offsets", "geometry.curve_num");
  RNA_def_property_struct_type(prop, "CurveSlice");
  RNA_def_property_ui_text(prop, "Curves", "All curves in the data-block");

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurvePoint");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Curves_position_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Curves_position_data_length",
                                    "rna_Curves_points_lookup_int",
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop, "Points", "Control points of all curves");

  /* Direct access to built-in attributes. */

  RNA_define_verify_sdna(0);
  prop = RNA_def_property(srna, "position_data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Curves_position_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Curves_position_data_length",
                                    "rna_Curves_position_data_lookup_int",
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "FloatVectorAttributeValue");
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");
  RNA_define_verify_sdna(1);

  prop = RNA_def_property(srna, "curve_offset_data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "geometry.curve_offsets", NULL);
  RNA_def_property_struct_type(prop, "IntAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Curves_curve_offset_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Curves_curve_offset_data_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");

  prop = RNA_def_property(srna, "surface", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Mesh_object_poll");
  RNA_def_property_ui_text(prop, "Surface", "Mesh object that the curves can be attached to");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "surface_uv_map", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "surface_uv_map");
  RNA_def_property_ui_text(prop,
                           "Surface UV Map",
                           "The name of the attribute on the surface mesh used to define the "
                           "attachment of each curve");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  /* Symmetry. */
  prop = RNA_def_property(srna, "use_mirror_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry", CURVES_SYMMETRY_X);
  RNA_def_property_ui_text(prop, "X", "Enable symmetry in the X axis");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  prop = RNA_def_property(srna, "use_mirror_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry", CURVES_SYMMETRY_Y);
  RNA_def_property_ui_text(prop, "Y", "Enable symmetry in the Y axis");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  prop = RNA_def_property(srna, "use_mirror_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry", CURVES_SYMMETRY_Z);
  RNA_def_property_ui_text(prop, "Z", "Enable symmetry in the Z axis");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  prop = RNA_def_property(srna, "selection_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_curves_domain_items);
  RNA_def_property_ui_text(prop, "Selection Domain", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  /* attributes */
  rna_def_attributes_common(srna);

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_curves(BlenderRNA *brna)
{
  rna_def_curves_point(brna);
  rna_def_curves_curve(brna);
  rna_def_curves(brna);
}

#endif
