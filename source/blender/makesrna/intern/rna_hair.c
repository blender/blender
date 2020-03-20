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

#  include "BKE_hair.h"

#  include "DEG_depsgraph.h"

#  include "WM_api.h"
#  include "WM_types.h"

static Hair *rna_hair(PointerRNA *ptr)
{
  return (Hair *)ptr->owner_id;
}

static int rna_HairPoint_index_get(PointerRNA *ptr)
{
  const Hair *hair = rna_hair(ptr);
  const float(*co)[3] = ptr->data;
  return (int)(co - hair->co);
}

static void rna_HairPoint_location_get(PointerRNA *ptr, float value[3])
{
  copy_v3_v3(value, (const float *)ptr->data);
}

static void rna_HairPoint_location_set(PointerRNA *ptr, const float value[3])
{
  copy_v3_v3((float *)ptr->data, value);
}

static float rna_HairPoint_radius_get(PointerRNA *ptr)
{
  const Hair *hair = rna_hair(ptr);
  if (hair->radius == NULL) {
    return 0.0f;
  }
  const float(*co)[3] = ptr->data;
  return hair->radius[co - hair->co];
}

static void rna_HairPoint_radius_set(PointerRNA *ptr, float value)
{
  const Hair *hair = rna_hair(ptr);
  if (hair->radius == NULL) {
    return;
  }
  const float(*co)[3] = ptr->data;
  hair->radius[co - hair->co] = value;
}

static char *rna_HairPoint_path(PointerRNA *ptr)
{
  return BLI_sprintfN("points[%d]", rna_HairPoint_index_get(ptr));
}

static int rna_HairCurve_index_get(PointerRNA *ptr)
{
  Hair *hair = rna_hair(ptr);
  return (int)((HairCurve *)ptr->data - hair->curves);
}

static char *rna_HairCurve_path(PointerRNA *ptr)
{
  return BLI_sprintfN("curves[%d]", rna_HairCurve_index_get(ptr));
}

static void rna_HairCurve_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Hair *hair = rna_hair(ptr);
  HairCurve *curve = ptr->data;
  float(*co)[3] = hair->co + curve->firstpoint;
  rna_iterator_array_begin(iter, co, sizeof(float[3]), curve->numpoints, 0, NULL);
}

static int rna_HairCurve_points_length(PointerRNA *ptr)
{
  HairCurve *curve = ptr->data;
  return curve->numpoints;
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

  srna = RNA_def_struct(brna, "HairPoint", NULL);
  RNA_def_struct_ui_text(srna, "Hair Point", "Hair curve control point");
  RNA_def_struct_path_func(srna, "rna_HairPoint_path");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(
      prop, "rna_HairPoint_location_get", "rna_HairPoint_location_set", NULL);
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_update(prop, 0, "rna_Hair_update_data");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(prop, "rna_HairPoint_radius_get", "rna_HairPoint_radius_set", NULL);
  RNA_def_property_ui_text(prop, "Radius", "");
  RNA_def_property_update(prop, 0, "rna_Hair_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_HairPoint_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this points");
}

static void rna_def_hair_curve(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "HairCurve", NULL);
  RNA_def_struct_ui_text(srna, "Hair Curve", "Hair curve");
  RNA_def_struct_path_func(srna, "rna_HairCurve_path");

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "HairPoint");
  RNA_def_property_ui_text(prop, "Points", "Control points of the curve");
  RNA_def_property_collection_funcs(prop,
                                    "rna_HairCurve_points_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_HairCurve_points_length",
                                    NULL,
                                    NULL,
                                    NULL);

  /* TODO: naming consistency, editable? */
  prop = RNA_def_property(srna, "first_point_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "firstpoint");
  RNA_def_property_ui_text(prop, "First Point Index", "Index of the first loop of this polygon");

  prop = RNA_def_property(srna, "num_points", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "numpoints");
  RNA_def_property_ui_text(prop, "Number of Points", "Number of loops used by this polygon");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_HairCurve_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this curve");
}

static void rna_def_hair(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Hair", "ID");
  RNA_def_struct_ui_text(srna, "Hair", "Hair data-block for hair curves");
  RNA_def_struct_ui_icon(srna, ICON_HAIR_DATA);

  /* geometry */
  prop = RNA_def_property(srna, "curves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "curves", "totcurve");
  RNA_def_property_struct_type(prop, "HairCurve");
  RNA_def_property_ui_text(prop, "Curves", "All hair curves");

  /* TODO: better solution for (*co)[3] parsing issue. */
  RNA_define_verify_sdna(0);
  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "co", "totpoint");
  RNA_def_property_struct_type(prop, "HairPoint");
  RNA_def_property_ui_text(prop, "Points", "Control points of all hair curves");
  RNA_define_verify_sdna(1);

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");

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
