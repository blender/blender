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

#include "DNA_lightprobe_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "BKE_main.h"
#  include "DEG_depsgraph.h"

#  include "DNA_collection_types.h"
#  include "DNA_object_types.h"

#  include "WM_api.h"

static void rna_LightProbe_recalc(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->id.data, ID_RECALC_GEOMETRY);
}

#else

static EnumPropertyItem parallax_type_items[] = {
    {LIGHTPROBE_SHAPE_ELIPSOID, "ELIPSOID", ICON_NONE, "Sphere", ""},
    {LIGHTPROBE_SHAPE_BOX, "BOX", ICON_NONE, "Box", ""},
    {0, NULL, 0, NULL, NULL},
};

static EnumPropertyItem lightprobe_type_items[] = {
    {LIGHTPROBE_TYPE_CUBE,
     "CUBEMAP",
     ICON_LIGHTPROBE_CUBEMAP,
     "Reflection Cubemap",
     "Capture reflections"},
    {LIGHTPROBE_TYPE_PLANAR, "PLANAR", ICON_LIGHTPROBE_PLANAR, "Reflection Plane", ""},
    {LIGHTPROBE_TYPE_GRID,
     "GRID",
     ICON_LIGHTPROBE_GRID,
     "Irradiance Volume",
     "Volume used for precomputing indirect lighting"},
    {0, NULL, 0, NULL, NULL},
};

static void rna_def_lightprobe(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LightProbe", "ID");
  RNA_def_struct_ui_text(
      srna, "LightProbe", "Light Probe data-block for lighting capture objects");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_LIGHTPROBE);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, lightprobe_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of light probe");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "clipsta");
  RNA_def_property_float_default(prop, 0.8f);
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(
      prop, "Clip Start", "Probe clip start, below which objects will not appear in reflections");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "clipend");
  RNA_def_property_float_default(prop, 40.0f);
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(
      prop, "Clip End", "Probe clip end, beyond which objects will not appear in reflections");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "show_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LIGHTPROBE_FLAG_SHOW_CLIP_DIST);
  RNA_def_property_ui_text(prop, "Clipping", "Show the clipping distances in the 3D view");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "influence_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "attenuation_type");
  RNA_def_property_enum_items(prop, parallax_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of influence volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "show_influence", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LIGHTPROBE_FLAG_SHOW_INFLUENCE);
  RNA_def_property_ui_text(prop, "Influence", "Show the influence volume in the 3D view");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "influence_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "distinf");
  RNA_def_property_float_default(prop, 2.5f);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Influence Distance", "Influence distance of the probe");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Falloff", "Control how fast the probe influence decreases");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "use_custom_parallax", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LIGHTPROBE_FLAG_CUSTOM_PARALLAX);
  RNA_def_property_ui_text(
      prop, "Use Custom Parallax", "Enable custom settings for the parallax correction volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "show_parallax", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LIGHTPROBE_FLAG_SHOW_PARALLAX);
  RNA_def_property_ui_text(prop, "Parallax", "Show the parallax correction volume in the 3D view");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "parallax_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, parallax_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of parallax volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "parallax_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "distpar");
  RNA_def_property_float_default(prop, 2.5f);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Parallax Radius", "Lowest corner of the parallax bounding box");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  /* irradiance grid */
  prop = RNA_def_property(srna, "grid_resolution_x", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_int_default(prop, 4);
  RNA_def_property_ui_text(
      prop, "Resolution X", "Number of sample along the x axis of the volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_resolution_y", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_int_default(prop, 4);
  RNA_def_property_ui_text(
      prop, "Resolution Y", "Number of sample along the y axis of the volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_resolution_z", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_int_default(prop, 4);
  RNA_def_property_ui_text(
      prop, "Resolution Z", "Number of sample along the z axis of the volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "visibility_buffer_bias", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "vis_bias");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.001f, 9999.0f);
  RNA_def_property_ui_range(prop, 0.001f, 5.0f, 1.0, 3);
  RNA_def_property_ui_text(prop, "Visibility Bias", "Bias for reducing self shadowing");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "visibility_bleed_bias", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "vis_bleedbias");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Visibility Bleed Bias", "Bias for reducing light-bleed on variance shadow maps");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  prop = RNA_def_property(srna, "visibility_blur", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "vis_blur");
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Visibility Blur", "Filter size of the visibility blur");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "intensity");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 3.0f, 1.0, 3);
  RNA_def_property_ui_text(
      prop, "Intensity", "Modify the intensity of the lighting captured by this probe");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "visibility_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_pointer_sdna(prop, NULL, "visibility_grp");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Visibility Collection", "Restrict objects visible for this probe");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "invert_visibility_collection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LIGHTPROBE_FLAG_INVERT_GROUP);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Invert Collection", "Invert visibility collection");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  /* Data preview */
  prop = RNA_def_property(srna, "show_data", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LIGHTPROBE_FLAG_SHOW_DATA);
  RNA_def_property_ui_text(prop,
                           "Show Preview Plane",
                           "Show captured lighting data into the 3D view for debugging purpose");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_lightprobe(BlenderRNA *brna)
{
  rna_def_lightprobe(brna);
}

#endif
