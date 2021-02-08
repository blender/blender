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

#include "DNA_pointcloud_types.h"

#include "BLI_math_base.h"
#include "BLI_string.h"

#ifdef RNA_RUNTIME

#  include "BLI_math_vector.h"

#  include "BKE_pointcloud.h"

#  include "DEG_depsgraph.h"

#  include "WM_api.h"
#  include "WM_types.h"

static PointCloud *rna_pointcloud(PointerRNA *ptr)
{
  return (PointCloud *)ptr->owner_id;
}

static int rna_Point_index_get(PointerRNA *ptr)
{
  const PointCloud *pointcloud = rna_pointcloud(ptr);
  const float(*co)[3] = ptr->data;
  return (int)(co - pointcloud->co);
}

static void rna_Point_location_get(PointerRNA *ptr, float value[3])
{
  copy_v3_v3(value, (const float *)ptr->data);
}

static void rna_Point_location_set(PointerRNA *ptr, const float value[3])
{
  copy_v3_v3((float *)ptr->data, value);
}

static float rna_Point_radius_get(PointerRNA *ptr)
{
  const PointCloud *pointcloud = rna_pointcloud(ptr);
  if (pointcloud->radius == NULL) {
    return 0.0f;
  }
  const float(*co)[3] = ptr->data;
  return pointcloud->radius[co - pointcloud->co];
}

static void rna_Point_radius_set(PointerRNA *ptr, float value)
{
  const PointCloud *pointcloud = rna_pointcloud(ptr);
  if (pointcloud->radius == NULL) {
    return;
  }
  const float(*co)[3] = ptr->data;
  pointcloud->radius[co - pointcloud->co] = value;
}

static char *rna_Point_path(PointerRNA *ptr)
{
  return BLI_sprintfN("points[%d]", rna_Point_index_get(ptr));
}

static void rna_PointCloud_update_data(struct Main *UNUSED(bmain),
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

static void rna_def_point(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Point", NULL);
  RNA_def_struct_ui_text(srna, "Point", "Point in a point cloud");
  RNA_def_struct_path_func(srna, "rna_Point_path");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop, "rna_Point_location_get", "rna_Point_location_set", NULL);
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_update(prop, 0, "rna_PointCloud_update_data");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(prop, "rna_Point_radius_get", "rna_Point_radius_set", NULL);
  RNA_def_property_ui_text(prop, "Radius", "");
  RNA_def_property_update(prop, 0, "rna_PointCloud_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_Point_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this points");
}

static void rna_def_pointcloud(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "PointCloud", "ID");
  RNA_def_struct_ui_text(srna, "Point Cloud", "Point cloud data-block");
  RNA_def_struct_ui_icon(srna, ICON_POINTCLOUD_DATA);

  /* geometry */
  /* TODO: better solution for (*co)[3] parsing issue. */
  RNA_define_verify_sdna(0);
  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "co", "totpoint");
  RNA_def_property_struct_type(prop, "Point");
  RNA_def_property_ui_text(prop, "Points", "");
  RNA_define_verify_sdna(1);

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");

  rna_def_attributes_common(srna);

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_pointcloud(BlenderRNA *brna)
{
  rna_def_point(brna);
  rna_def_pointcloud(brna);
}

#endif
