/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.h"

#include "DNA_lightprobe_types.h"

#include "WM_types.hh"

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "BKE_main.h"
#  include "DEG_depsgraph.h"

#  include "DNA_collection_types.h"
#  include "DNA_object_types.h"

#  include "WM_api.hh"

static void rna_LightProbe_recalc(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
}

#else

static EnumPropertyItem parallax_type_items[] = {
    {LIGHTPROBE_SHAPE_ELIPSOID, "ELIPSOID", ICON_NONE, "Sphere", ""},
    {LIGHTPROBE_SHAPE_BOX, "BOX", ICON_NONE, "Box", ""},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem lightprobe_resolution_items[] = {
    {LIGHT_PROBE_RESOLUTION_64, "64", 0, "64", ""},
    {LIGHT_PROBE_RESOLUTION_128, "128", 0, "128", ""},
    {LIGHT_PROBE_RESOLUTION_256, "256", 0, "256", ""},
    {LIGHT_PROBE_RESOLUTION_512, "512", 0, "512", ""},
    {LIGHT_PROBE_RESOLUTION_1024, "1024", 0, "1024", ""},
    {LIGHT_PROBE_RESOLUTION_2048, "2048", 0, "2048", ""},
    {0, nullptr, 0, nullptr, nullptr},
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
  RNA_def_property_float_sdna(prop, nullptr, "clipsta");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(
      prop, "Clip Start", "Probe clip start, below which objects will not appear in reflections");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "clipend");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(
      prop, "Clip End", "Probe clip end, beyond which objects will not appear in reflections");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "show_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIGHTPROBE_FLAG_SHOW_CLIP_DIST);
  RNA_def_property_ui_text(prop, "Clipping", "Show the clipping distances in the 3D view");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "influence_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "attenuation_type");
  RNA_def_property_enum_items(prop, parallax_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of influence volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_influence", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIGHTPROBE_FLAG_SHOW_INFLUENCE);
  RNA_def_property_ui_text(prop, "Influence", "Show the influence volume in the 3D view");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "influence_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "distinf");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Influence Distance", "Influence distance of the probe");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Falloff", "Control how fast the probe influence decreases");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "use_custom_parallax", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIGHTPROBE_FLAG_CUSTOM_PARALLAX);
  RNA_def_property_ui_text(
      prop, "Use Custom Parallax", "Enable custom settings for the parallax correction volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_parallax", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIGHTPROBE_FLAG_SHOW_PARALLAX);
  RNA_def_property_ui_text(prop, "Parallax", "Show the parallax correction volume in the 3D view");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "parallax_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, parallax_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of parallax volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "parallax_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "distpar");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Parallax Radius", "Lowest corner of the parallax bounding box");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  /* irradiance grid */
  prop = RNA_def_property(srna, "grid_resolution_x", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_ui_text(
      prop, "Resolution X", "Number of samples along the x axis of the volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_resolution_y", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_ui_text(
      prop, "Resolution Y", "Number of samples along the y axis of the volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_resolution_z", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_ui_text(
      prop, "Resolution Z", "Number of samples along the z axis of the volume");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_normal_bias", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop,
                           "Normal Bias",
                           "Offset sampling of the irradiance grid in "
                           "the surface normal direction to reduce light bleeding");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_view_bias", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop,
                           "View Bias",
                           "Offset sampling of the irradiance grid in "
                           "the viewing direction to reduce light bleeding");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_irradiance_smoothing", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "grid_facing_bias");
  RNA_def_property_ui_text(
      prop, "Facing Bias", "Smoother irradiance interpolation but introduce light bleeding");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_bake_samples", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Bake Samples", "Number of ray directions to evaluate when baking");
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_surface_bias", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop,
                           "Capture Surface Bias",
                           "Moves capture points position away from surfaces to avoid artifacts");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_escape_bias", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Capture Escape Bias", "Moves capture points outside objects");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "surfel_density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop,
                           "Surfel Density",
                           "Number of surfels per unit distance (higher values improve quality)");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_validity_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop,
                           "Validity Threshold",
                           "Ratio of front-facing surface hits under which a grid sample will "
                           "not be considered for lighting");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_dilation_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop,
                           "Dilation Threshold",
                           "Ratio of front-facing surface hits under which a grid sample will "
                           "reuse neighbors grid sample lighting");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_dilation_radius", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop,
      "Dilation Radius",
      "Radius in grid sample to search valid grid samples to copy into invalid grid samples");
  RNA_def_property_range(prop, 1.0f, 5.0f);
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "grid_capture_world", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "grid_flag", LIGHTPROBE_GRID_CAPTURE_WORLD);
  RNA_def_property_ui_text(
      prop,
      "Capture World",
      "Bake incoming light from the world, instead of just the visibility, "
      "for more accurate lighting, but loose correct blending to surrounding irradiance volumes");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "grid_capture_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "grid_flag", LIGHTPROBE_GRID_CAPTURE_INDIRECT);
  RNA_def_property_ui_text(prop,
                           "Capture Indirect",
                           "Bake light bounces from light sources for more accurate lighting");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "grid_capture_emission", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "grid_flag", LIGHTPROBE_GRID_CAPTURE_EMISSION);
  RNA_def_property_ui_text(
      prop, "Capture Emission", "Bake emissive surfaces for more accurate lighting");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "visibility_buffer_bias", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "vis_bias");
  RNA_def_property_range(prop, 0.001f, 9999.0f);
  RNA_def_property_ui_range(prop, 0.001f, 5.0f, 1.0, 3);
  RNA_def_property_ui_text(prop, "Visibility Bias", "Bias for reducing self shadowing");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "visibility_bleed_bias", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "vis_bleedbias");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Visibility Bleed Bias", "Bias for reducing light-bleed on variance shadow maps");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  prop = RNA_def_property(srna, "visibility_blur", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "vis_blur");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Visibility Blur", "Filter size of the visibility blur");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "resolution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "resolution");
  RNA_def_property_enum_items(prop, lightprobe_resolution_items);
  RNA_def_property_ui_text(prop, "Resolution", "Resolution when baked to a texture");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "intensity");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 3.0f, 1.0, 3);
  RNA_def_property_ui_text(
      prop, "Intensity", "Modify the intensity of the lighting captured by this probe");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "visibility_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_pointer_sdna(prop, nullptr, "visibility_grp");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Visibility Collection", "Restrict objects visible for this probe");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  prop = RNA_def_property(srna, "invert_visibility_collection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIGHTPROBE_FLAG_INVERT_GROUP);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Invert Collection", "Invert visibility collection");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, "rna_LightProbe_recalc");

  /* Data preview */
  prop = RNA_def_property(srna, "show_data", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIGHTPROBE_FLAG_SHOW_DATA);
  RNA_def_property_ui_text(prop,
                           "Show Preview Plane",
                           "Show captured lighting data into the 3D view for debugging purpose");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, nullptr);

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_lightprobe(BlenderRNA *brna)
{
  rna_def_lightprobe(brna);
}

#endif
