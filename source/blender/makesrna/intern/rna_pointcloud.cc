/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "BKE_attribute.h"

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLI_math_vector.h"
#  include "BLI_virtual_array.hh"

#  include "BKE_customdata.hh"
#  include "BKE_pointcloud.hh"

#  include "DEG_depsgraph.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

using blender::float3;

static PointCloud *rna_pointcloud(const PointerRNA *ptr)
{
  return (PointCloud *)ptr->owner_id;
}

static float3 *get_pointcloud_positions(PointCloud *pointcloud)
{
  return pointcloud->positions_for_write().data();
}

static const float3 *get_pointcloud_positions_const(const PointCloud *pointcloud)
{
  return pointcloud->positions().data();
}

static int rna_Point_index_get_const(const PointerRNA *ptr)
{
  const PointCloud *pointcloud = rna_pointcloud(ptr);
  const float3 *co = static_cast<const float3 *>(ptr->data);
  const float3 *positions = get_pointcloud_positions_const(pointcloud);
  return int(co - positions);
}

static int rna_Point_index_get(PointerRNA *ptr)
{
  return rna_Point_index_get_const(ptr);
}

static int rna_PointCloud_points_length(PointerRNA *ptr)
{
  const PointCloud *pointcloud = rna_pointcloud(ptr);
  return pointcloud->totpoint;
}

static void rna_PointCloud_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  PointCloud *pointcloud = rna_pointcloud(ptr);
  rna_iterator_array_begin(iter,
                           ptr,
                           get_pointcloud_positions(pointcloud),
                           sizeof(float3),
                           pointcloud->totpoint,
                           false,
                           nullptr);
}

bool rna_PointCloud_points_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  PointCloud *pointcloud = rna_pointcloud(ptr);
  if (index < 0 || index >= pointcloud->totpoint) {
    return false;
  }
  rna_pointer_create_with_ancestors(
      *ptr, &RNA_Point, &get_pointcloud_positions(pointcloud)[index], *r_ptr);
  return true;
}

static void rna_Point_location_get(PointerRNA *ptr, float value[3])
{
  copy_v3_v3(value, *static_cast<const float3 *>(ptr->data));
}

static void rna_Point_location_set(PointerRNA *ptr, const float value[3])
{
  *static_cast<float3 *>(ptr->data) = float3(value);
}

static float rna_Point_radius_get(PointerRNA *ptr)
{
  const PointCloud *pointcloud = rna_pointcloud(ptr);
  return pointcloud->radius().get(rna_Point_index_get_const(ptr));
}

static void rna_Point_radius_set(PointerRNA *ptr, float value)
{
  PointCloud *pointcloud = rna_pointcloud(ptr);
  pointcloud->radius_for_write()[rna_Point_index_get_const(ptr)] = value;
}

static std::optional<std::string> rna_Point_path(const PointerRNA *ptr)
{
  return fmt::format("points[{}]", rna_Point_index_get_const(ptr));
}

static void rna_PointCloud_update_data(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  /* cheating way for importers to avoid slow updates */
  if (id->us > 0) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

#else

static void rna_def_point(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Point", nullptr);
  RNA_def_struct_ui_text(srna, "Point", "Point in a point cloud");
  RNA_def_struct_path_func(srna, "rna_Point_path");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop, "rna_Point_location_get", "rna_Point_location_set", nullptr);
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_update(prop, 0, "rna_PointCloud_update_data");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(prop, "rna_Point_radius_get", "rna_Point_radius_set", nullptr);
  RNA_def_property_ui_text(prop, "Radius", "");
  RNA_def_property_update(prop, 0, "rna_PointCloud_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_Point_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Index", "Index of this point");
}

static void rna_def_pointcloud(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "PointCloud", "ID");
  RNA_def_struct_ui_text(srna, "Point Cloud", "Point cloud data-block");
  RNA_def_struct_ui_icon(srna, ICON_POINTCLOUD_DATA);

  /* geometry */
  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Point");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_PointCloud_points_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_PointCloud_points_length",
                                    "rna_PointCloud_points_lookup_int",
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Points", "");

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

  rna_def_attributes_common(srna, AttributeOwnerType::PointCloud);

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_pointcloud(BlenderRNA *brna)
{
  rna_def_point(brna);
  rna_def_pointcloud(brna);
}

#endif
