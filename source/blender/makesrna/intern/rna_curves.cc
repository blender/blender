/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "DNA_curves_types.h"

#include "BKE_attribute.h"

#include "BLT_translation.hh"

#include "WM_types.hh"

const EnumPropertyItem rna_enum_curves_type_items[] = {
    {CURVE_TYPE_CATMULL_ROM, "CATMULL_ROM", 0, "Catmull Rom", ""},
    {CURVE_TYPE_POLY, "POLY", 0, "Poly", ""},
    {CURVE_TYPE_BEZIER, "BEZIER", 0, "Bézier", ""},
    {CURVE_TYPE_NURBS, "NURBS", 0, "NURBS", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_curves_handle_type_items[] = {
    {BEZIER_HANDLE_FREE,
     "FREE",
     0,
     "Free",
     "The handle can be moved anywhere, and does not influence the point's other handle"},
    {BEZIER_HANDLE_AUTO,
     "AUTO",
     0,
     "Auto",
     "The location is automatically calculated to be smooth"},
    {BEZIER_HANDLE_VECTOR,
     "VECTOR",
     0,
     "Vector",
     "The location is calculated to point to the next/previous control point"},
    {BEZIER_HANDLE_ALIGN,
     "ALIGN",
     0,
     "Align",
     "The location is constrained to point in the opposite direction as the other handle"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_curve_normal_mode_items[] = {
    {NORMAL_MODE_MINIMUM_TWIST,
     "MINIMUM_TWIST",
     ICON_NONE,
     N_("Minimum Twist"),
     N_("Calculate normals with the smallest twist around the curve tangent across the whole "
        "curve")},
    {NORMAL_MODE_Z_UP,
     "Z_UP",
     ICON_NONE,
     N_("Z Up"),
     N_("Calculate normals perpendicular to the Z axis and the curve tangent. If a series of "
        "points is vertical, the X axis is used.")},
    {NORMAL_MODE_FREE,
     "FREE",
     ICON_NONE,
     N_("Free"),
     N_("Use the stored custom normal attribute as the final normals")},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLI_math_vector.h"
#  include "BLI_offset_indices.hh"

#  include "BKE_attribute.hh"
#  include "BKE_curves.hh"
#  include "BKE_report.hh"

#  include "DEG_depsgraph.hh"

#  include "ED_curves.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

static Curves *rna_curves(const PointerRNA *ptr)
{
  return reinterpret_cast<Curves *>(ptr->owner_id);
}

static int rna_Curves_curve_offset_data_length(PointerRNA *ptr)
{
  const Curves *curves = rna_curves(ptr);
  return curves->geometry.curve_num + 1;
}

static void rna_Curves_curve_offset_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  rna_iterator_array_begin(iter,
                           ptr,
                           curves->geometry.wrap().offsets_for_write().data(),
                           sizeof(int),
                           curves->geometry.curve_num + 1,
                           false,
                           nullptr);
}

static bool rna_Curves_curve_offset_data_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Curves *curves = rna_curves(ptr);
  if (index < 0 || index >= curves->geometry.curve_num + 1) {
    return false;
  }
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_IntAttributeValue, &curves->geometry.wrap().offsets_for_write()[index], *r_ptr);
  return true;
}

static float (*get_curves_positions_for_write(Curves &curves))[3]
{
  return reinterpret_cast<float (*)[3]>(curves.geometry.wrap().positions_for_write().data());
}

static const float (*get_curves_positions(const Curves &curves))[3]
{
  return reinterpret_cast<const float (*)[3]>(curves.geometry.wrap().positions().data());
}

static int rna_CurvePoint_index_get_const(const PointerRNA *ptr)
{
  const Curves *curves = rna_curves(ptr);
  const float (*co)[3] = static_cast<float (*)[3]>(ptr->data);
  const float (*positions)[3] = get_curves_positions(*curves);
  return int(co - positions);
}

static void rna_Curves_curves_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  rna_iterator_array_begin(iter,
                           ptr,
                           curves->geometry.wrap().offsets_for_write().data(),
                           sizeof(int),
                           curves->geometry.curve_num,
                           false,
                           nullptr);
}

static int rna_Curves_curves_length(PointerRNA *ptr)
{
  const Curves *curves = rna_curves(ptr);
  return curves->geometry.curve_num;
}

static bool rna_Curves_curves_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Curves *curves = rna_curves(ptr);
  if (index < 0 || index >= curves->geometry.curve_num) {
    return false;
  }
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_CurveSlice, &curves->geometry.wrap().offsets_for_write()[index], *r_ptr);
  return true;
}

static int rna_Curves_position_data_length(PointerRNA *ptr)
{
  const Curves *curves = rna_curves(ptr);
  return curves->geometry.point_num;
}

bool rna_Curves_position_data_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Curves *curves = rna_curves(ptr);
  if (index < 0 || index >= curves->geometry.point_num) {
    return false;
  }
  rna_pointer_create_with_ancestors(*ptr,
                                    &RNA_FloatVectorAttributeValue,
                                    &get_curves_positions_for_write(*curves)[index],
                                    *r_ptr);
  return true;
}

static void rna_Curves_position_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  rna_iterator_array_begin(iter,
                           ptr,
                           get_curves_positions_for_write(*curves),
                           sizeof(float[3]),
                           curves->geometry.point_num,
                           false,
                           nullptr);
}

static int rna_CurvePoint_index_get(PointerRNA *ptr)
{
  return rna_CurvePoint_index_get_const(ptr);
}

static void rna_CurvePoint_location_get(PointerRNA *ptr, float value[3])
{
  copy_v3_v3(value, static_cast<const float *>(ptr->data));
}

static void rna_CurvePoint_location_set(PointerRNA *ptr, const float value[3])
{
  copy_v3_v3(static_cast<float *>(ptr->data), value);
}

static float rna_CurvePoint_radius_get(PointerRNA *ptr)
{
  using namespace blender;
  const Curves *curves = rna_curves(ptr);
  const bke::AttributeAccessor attributes = curves->geometry.wrap().attributes();
  const VArray radii = *attributes.lookup_or_default<float>(
      "radius", bke::AttrDomain::Point, 0.0f);
  return radii[rna_CurvePoint_index_get_const(ptr)];
}

static void rna_CurvePoint_radius_set(PointerRNA *ptr, float value)
{
  using namespace blender;
  Curves *curves = rna_curves(ptr);
  bke::MutableAttributeAccessor attributes = curves->geometry.wrap().attributes_for_write();
  bke::AttributeWriter radii = attributes.lookup_or_add_for_write<float>("radius",
                                                                         bke::AttrDomain::Point);
  if (!radii) {
    return;
  }
  radii.varray.set(rna_CurvePoint_index_get_const(ptr), value);
}

static std::optional<std::string> rna_CurvePoint_path(const PointerRNA *ptr)
{
  return fmt::format("points[{}]", rna_CurvePoint_index_get_const(ptr));
}

bool rna_Curves_points_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Curves *curves = rna_curves(ptr);
  if (index < 0 || index >= curves->geometry.point_num) {
    return false;
  }
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_CurvePoint, &get_curves_positions_for_write(*curves)[index], *r_ptr);
  return true;
}

static int rna_CurveSlice_index_get_const(const PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  return int(static_cast<int *>(ptr->data) - curves->geometry.curve_offsets);
}

static int rna_CurveSlice_index_get(PointerRNA *ptr)
{
  return rna_CurveSlice_index_get_const(ptr);
}

static std::optional<std::string> rna_CurveSlice_path(const PointerRNA *ptr)
{
  return fmt::format("curves[{}]", rna_CurveSlice_index_get_const(ptr));
}

static int rna_CurveSlice_first_point_index_get(PointerRNA *ptr)
{
  const int *offset_ptr = static_cast<int *>(ptr->data);
  return *offset_ptr;
}

static int rna_CurveSlice_points_length_get(PointerRNA *ptr)
{
  const int *offset_ptr = static_cast<int *>(ptr->data);
  const int offset = *offset_ptr;
  return *(offset_ptr + 1) - offset;
}

static void rna_CurveSlice_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  const int offset = rna_CurveSlice_first_point_index_get(ptr);
  const int size = rna_CurveSlice_points_length_get(ptr);
  float (*positions)[3] = get_curves_positions_for_write(*curves);
  float (*co)[3] = positions + offset;
  rna_iterator_array_begin(iter, ptr, co, sizeof(float[3]), size, 0, nullptr);
}

static void rna_Curves_normals_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Curves *curves = rna_curves(ptr);
  float (*positions)[3] = blender::ed::curves::point_normals_array_create(curves);
  const int size = curves->geometry.point_num;
  rna_iterator_array_begin(iter, ptr, positions, sizeof(float[3]), size, true, nullptr);
}

static void rna_Curves_update_data(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  /* Avoid updates for importers creating curves. */
  if (id->us > 0) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

void rna_Curves_update_draw(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
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

  srna = RNA_def_struct(brna, "CurvePoint", nullptr);
  RNA_def_struct_ui_text(srna, "Curve Point", "Curve control point");
  RNA_def_struct_path_func(srna, "rna_CurvePoint_path");

  prop = RNA_def_property(srna, "position", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(
      prop, "rna_CurvePoint_location_get", "rna_CurvePoint_location_set", nullptr);
  RNA_def_property_ui_text(prop, "Position", "");
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(
      prop, "rna_CurvePoint_radius_get", "rna_CurvePoint_radius_set", nullptr);
  RNA_def_property_ui_text(prop, "Radius", "");
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurvePoint_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Index", "Index of this point");
}

/* Defines a read-only vector type since normals can not be modified manually. */
static void rna_def_read_only_float_vector(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "FloatVectorValueReadOnly", nullptr);
  RNA_def_struct_sdna(srna, "vec3f");
  RNA_def_struct_ui_text(srna, "Read-Only Vector", "");

  PropertyRNA *prop = RNA_def_property(srna, "vector", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_ui_text(prop, "Vector", "3D vector");
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_curves_curve(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurveSlice", nullptr);
  RNA_def_struct_ui_text(srna, "Curve Slice", "A single curve from a curves data-block");
  RNA_def_struct_path_func(srna, "rna_CurveSlice_path");

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurvePoint");
  RNA_def_property_ui_text(prop, "Points", "Control points of the curve");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_CurveSlice_points_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_CurveSlice_points_length_get",
                                    nullptr,
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "first_point_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurveSlice_first_point_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "First Point Index", "The index of this curve's first control point");

  prop = RNA_def_property(srna, "points_length", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurveSlice_points_length_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Number of Points", "Number of control points in the curve");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_CurveSlice_index_get", nullptr, nullptr);
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Curves_curves_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Curves_curves_length",
                                    "rna_Curves_curves_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "CurveSlice");
  RNA_def_property_ui_text(prop, "Curves", "All curves in the data-block");

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurvePoint");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Curves_position_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Curves_position_data_length",
                                    "rna_Curves_points_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Points", "Control points of all curves");

  /* Direct access to built-in attributes. */

  prop = RNA_def_property(srna, "position_data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Curves_position_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Curves_position_data_length",
                                    "rna_Curves_position_data_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "FloatVectorAttributeValue");
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  prop = RNA_def_property(srna, "curve_offset_data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "IntAttributeValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Curves_curve_offset_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Curves_curve_offset_data_length",
                                    "rna_Curves_curve_offset_data_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  rna_def_read_only_float_vector(brna);

  prop = RNA_def_property(srna, "normals", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_struct_type(prop, "FloatVectorValueReadOnly");
  /* `lookup_int` isn't provided since the entire normals array is allocated and calculated when
   * it's accessed. */
  RNA_def_property_collection_funcs(prop,
                                    "rna_Curves_normals_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Curves_position_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(
      prop, "Normals", "The curve normal value at each of the curve's control points");

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.cc */
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_IDMaterials_assign_int");

  prop = RNA_def_property(srna, "surface", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Mesh_object_poll");
  RNA_def_property_ui_text(prop, "Surface", "Mesh object that the curves can be attached to");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "surface_uv_map", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "surface_uv_map");
  RNA_def_property_ui_text(prop,
                           "Surface UV Map",
                           "The name of the attribute on the surface mesh used to define the "
                           "attachment of each curve");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  /* Symmetry. */
  prop = RNA_def_property(srna, "use_mirror_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry", CURVES_SYMMETRY_X);
  RNA_def_property_ui_text(prop, "X", "Enable symmetry in the X axis");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  prop = RNA_def_property(srna, "use_mirror_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry", CURVES_SYMMETRY_Y);
  RNA_def_property_ui_text(prop, "Y", "Enable symmetry in the Y axis");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  prop = RNA_def_property(srna, "use_mirror_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry", CURVES_SYMMETRY_Z);
  RNA_def_property_ui_text(prop, "Z", "Enable symmetry in the Z axis");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  prop = RNA_def_property(srna, "selection_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_curves_domain_items);
  RNA_def_property_ui_text(prop, "Selection Domain", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Curves_update_data");

  prop = RNA_def_property(srna, "use_sculpt_collision", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CV_SCULPT_COLLISION_ENABLED);
  RNA_def_property_ui_text(
      prop, "Use Sculpt Collision", "Enable collision with the surface while sculpting");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  prop = RNA_def_property(srna, "surface_collision_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "surface_collision_distance");
  RNA_def_property_range(prop, FLT_EPSILON, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 10.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Collision distance", "Distance to keep the curves away from the surface");
  RNA_def_property_update(prop, 0, "rna_Curves_update_draw");

  /* attributes */
  rna_def_attributes_common(srna, AttributeOwnerType::Curves);

  /* common */
  rna_def_animdata_common(srna);

  RNA_api_curves(srna);
}

void RNA_def_curves(BlenderRNA *brna)
{
  rna_def_curves_point(brna);
  rna_def_curves_curve(brna);
  rna_def_curves(brna);
}

#endif
