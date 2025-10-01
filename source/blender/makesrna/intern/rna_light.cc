/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BLI_math_rotation.h"

#include "BLT_translation.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"
#include "rna_internal.hh"

#include "DNA_light_types.h"

#include "IMB_colormanagement.hh"

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "BLI_math_matrix_types.hh"

#  include "BKE_context.hh"
#  include "BKE_light.h"
#  include "BKE_main.hh"
#  include "BKE_texture.h"

#  include "DEG_depsgraph.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

#  include "ED_node.hh"

static StructRNA *rna_Light_refine(PointerRNA *ptr)
{
  Light *la = (Light *)ptr->data;

  switch (la->type) {
    case LA_LOCAL:
      return &RNA_PointLight;
    case LA_SUN:
      return &RNA_SunLight;
    case LA_SPOT:
      return &RNA_SpotLight;
    case LA_AREA:
      return &RNA_AreaLight;
    default:
      return &RNA_Light;
  }
}

static void rna_Light_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Light *la = (Light *)ptr->owner_id;

  DEG_id_tag_update(&la->id, 0);
  WM_main_add_notifier(NC_LAMP | ND_LIGHTING, la);
}

static void rna_Light_draw_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Light *la = (Light *)ptr->owner_id;

  DEG_id_tag_update(&la->id, 0);
  WM_main_add_notifier(NC_LAMP | ND_LIGHTING_DRAW, la);
}

static void rna_Light_use_nodes_update(bContext *C, PointerRNA *ptr)
{
  Light *la = (Light *)ptr->data;
  Main *bmain = CTX_data_main(C);

  if (la->use_nodes && la->nodetree == nullptr) {
    ED_node_shader_default(C, bmain, &la->id);
  }

  rna_Light_update(CTX_data_main(C), CTX_data_scene(C), ptr);
}

static void rna_Light_temperature_color_get(PointerRNA *ptr, float *color)
{
  Light *la = (Light *)ptr->data;

  if (la->mode & LA_USE_TEMPERATURE) {
    float rgb[4];
    IMB_colormanagement_blackbody_temperature_to_rgb(rgb, la->temperature);

    color[0] = rgb[0];
    color[1] = rgb[1];
    color[2] = rgb[2];
  }
  else {
    copy_v3_fl(color, 1.0f);
  }
}

static float rna_Light_area(Light *light, const float matrix_world[16])
{
  blender::float4x4 mat(matrix_world);
  return BKE_light_area(*light, mat);
}

#else

/* NOTE(@dingto): Don't define icons here,
 * so they don't show up in the Light UI (properties editor). */

const EnumPropertyItem rna_enum_light_type_items[] = {
    {LA_LOCAL, "POINT", 0, "Point", "Omnidirectional point light source"},
    {LA_SUN, "SUN", 0, "Sun", "Constant direction parallel ray light source"},
    {LA_SPOT, "SPOT", 0, "Spot", "Directional cone light source"},
    {LA_AREA, "AREA", 0, "Area", "Directional area light source"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_light_api(StructRNA *srna)
{
  FunctionRNA *func = RNA_def_function(srna, "area", "rna_Light_area");
  RNA_def_function_ui_description(func,
                                  "Compute light area based on type and shape. The normalize "
                                  "option divides light intensity by this area");
  PropertyRNA *parm = RNA_def_property(func, "matrix_world", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "Object to world space transformation matrix");
  parm = RNA_def_property(func, "area", PROP_FLOAT, PROP_NONE);
  RNA_def_function_return(func, parm);
}

static void rna_def_light(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  static const float default_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  srna = RNA_def_struct(brna, "Light", "ID");
  RNA_def_struct_sdna(srna, "Light");
  RNA_def_struct_refine_func(srna, "rna_Light_refine");
  RNA_def_struct_ui_text(srna, "Light", "Light data-block for lighting a scene");
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_ID_LIGHT);
  RNA_def_struct_ui_icon(srna, ICON_LIGHT_DATA);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_light_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of light");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_LIGHT);
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "use_temperature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_USE_TEMPERATURE);
  RNA_def_property_ui_text(
      prop, "Use Temperature", "Use blackbody temperature to define a natural light color");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_LIGHT);
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "r");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_color);
  RNA_def_property_ui_text(prop, "Color", "Light color");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "temperature", PROP_FLOAT, PROP_COLOR_TEMPERATURE);
  RNA_def_property_float_sdna(prop, nullptr, "temperature");
  RNA_def_property_range(prop, 800.0f, 20000.0f);
  RNA_def_property_ui_range(prop, 800.0f, 20000.0f, 400.0f, 1);
  RNA_def_property_ui_text(prop, "Temperature", "Light color temperature in Kelvin");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "temperature_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_Light_temperature_color_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Temperature Color", "Color from Temperature");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "specular_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "spec_fac");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 2);
  RNA_def_property_ui_text(prop, "Specular Factor", "Specular reflection multiplier");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "diffuse_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "diff_fac");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 2);
  RNA_def_property_ui_text(prop, "Diffuse Factor", "Diffuse reflection multiplier");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "transmission_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "transmission_fac");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 2);
  RNA_def_property_ui_text(prop, "Transmission Factor", "Transmission light multiplier");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "volume_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "volume_fac");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 2);
  RNA_def_property_ui_text(prop, "Volume Factor", "Volume light multiplier");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "use_custom_distance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_CUSTOM_ATTENUATION);
  RNA_def_property_ui_text(prop,
                           "Custom Attenuation",
                           "Use custom attenuation distance instead of global light threshold");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "cutoff_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "att_dist");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01f, 100.0f, 1.0, 2);
  RNA_def_property_ui_text(
      prop, "Cutoff Distance", "Distance at which the light influence will be set to 0");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "use_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_SHADOW);
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "exposure", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -32.0f, 32.0f);
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Exposure",
      "Scales the power of the light exponentially, multiplying the intensity by 2^exposure");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "mode", LA_UNNORMALIZED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Normalize",
                           "Normalize intensity by light area, for consistent total light "
                           "output regardless of size and shape");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  /* nodes */
  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "nodetree");
  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node based lights");

  prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_nodes", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Use Nodes", "Use shader nodes to render the light");
  RNA_def_property_update(prop, 0, "rna_Light_use_nodes_update");

  /* common */
  rna_def_animdata_common(srna);
  rna_def_light_api(srna);
}

static void rna_def_light_energy(StructRNA *srna, const short light_type)
{
  PropertyRNA *prop;

  switch (light_type) {
    case LA_SUN: {
      /* Distant light strength has no unit defined,
       * it's proportional to 'watt/m^2' and is not sensitive to scene unit scale. */
      prop = RNA_def_property(srna, "energy", PROP_FLOAT, PROP_NONE);
      RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
      RNA_def_property_ui_text(
          prop, "Strength", "Sunlight strength in watts per meter squared (W/m²)");
      RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_LIGHT);
      RNA_def_property_update(prop, 0, "rna_Light_draw_update");
      break;
    }
    case LA_SPOT: {
      /* Lights with a location have radiometric power in Watts,
       * which is sensitive to scene unit scale. */
      prop = RNA_def_property(srna, "energy", PROP_FLOAT, PROP_NONE);
      RNA_def_property_ui_range(prop, 0.0f, 1000000.0f, 10, 3);
      RNA_def_property_ui_text(
          prop,
          "Power",
          "The energy this light would emit over its entire area "
          "if it wasn't limited by the spot angle, in units of radiant power (W)");
      RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_LIGHT);
      RNA_def_property_update(prop, 0, "rna_Light_draw_update");
      break;
    }
    default: {
      /* Lights with a location have radiometric power in Watts,
       * which is sensitive to scene unit scale. */
      prop = RNA_def_property(srna, "energy", PROP_FLOAT, PROP_NONE);
      RNA_def_property_ui_range(prop, 0.0f, 1000000.0f, 10, 3);
      RNA_def_property_ui_text(prop,
                               "Power",
                               "Light energy emitted over the entire area of the light in all "
                               "directions, in units of radiant power (W)");
      RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_LIGHT);
      RNA_def_property_update(prop, 0, "rna_Light_draw_update");
      break;
    }
  }
}

static void rna_def_light_shadow(StructRNA *srna, bool sun)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "shadow_buffer_clip_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "clipsta");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(prop,
                           "Shadow Buffer Clip Start",
                           "Shadow map clip start, below which objects will not generate shadows");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "shadow_soft_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "radius");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Shadow Soft Size", "Light size for ray shadow sampling (Raytraced shadows)");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  /* Eevee */
  prop = RNA_def_property(srna, "shadow_filter_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 5.0f, 1.0f, 2);
  RNA_def_property_ui_text(
      prop, "Shadow Filter Radius", "Blur shadow aliasing using Percentage Closer Filtering");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_maximum_resolution", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0001f, 0.020f, 0.05f, 4);
  RNA_def_property_ui_text(prop,
                           "Shadows Resolution Limit",
                           "Minimum size of a shadow map pixel. Higher values use less memory at "
                           "the cost of shadow quality.");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "use_shadow_jitter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_SHADOW_JITTER);
  RNA_def_property_ui_text(
      prop,
      "Shadow Jitter",
      "Enable jittered soft shadows to increase shadow precision (disabled in viewport unless "
      "enabled in the render settings). Has a high performance impact.");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_jitter_overblur", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0.0f, 20.0f, 10.0f, 0);
  RNA_def_property_ui_text(
      prop,
      "Shadow Jitter Overblur",
      "Apply shadow tracing to each jittered sample to reduce under-sampling artifacts");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_Light_update");

  if (sun) {
    prop = RNA_def_property(srna, "shadow_cascade_max_distance", PROP_FLOAT, PROP_DISTANCE);
    RNA_def_property_float_sdna(prop, nullptr, "cascade_max_dist");
    RNA_def_property_range(prop, 0.0f, FLT_MAX);
    RNA_def_property_ui_text(prop,
                             "Cascade Max Distance",
                             "End distance of the cascaded shadow map (only in perspective view)");
    RNA_def_property_update(prop, 0, "rna_Light_update");

    prop = RNA_def_property(srna, "shadow_cascade_count", PROP_INT, PROP_NONE);
    RNA_def_property_int_sdna(prop, nullptr, "cascade_count");
    RNA_def_property_range(prop, 1, 4);
    RNA_def_property_ui_text(
        prop, "Cascade Count", "Number of texture used by the cascaded shadow map");
    RNA_def_property_update(prop, 0, "rna_Light_update");

    prop = RNA_def_property(srna, "shadow_cascade_exponent", PROP_FLOAT, PROP_FACTOR);
    RNA_def_property_float_sdna(prop, nullptr, "cascade_exponent");
    RNA_def_property_range(prop, 0.0f, 1.0f);
    RNA_def_property_ui_text(prop,
                             "Exponential Distribution",
                             "Higher value increase resolution towards the viewpoint");
    RNA_def_property_update(prop, 0, "rna_Light_update");

    prop = RNA_def_property(srna, "shadow_cascade_fade", PROP_FLOAT, PROP_FACTOR);
    RNA_def_property_float_sdna(prop, nullptr, "cascade_fade");
    RNA_def_property_range(prop, 0.0f, 1.0f);
    RNA_def_property_ui_text(
        prop, "Cascade Fade", "How smooth is the transition between each cascade");
    RNA_def_property_update(prop, 0, "rna_Light_update");
  }
  else {
    prop = RNA_def_property(srna, "use_absolute_resolution", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_SHAD_RES_ABSOLUTE);
    RNA_def_property_ui_text(prop,
                             "Absolute Resolution Limit",
                             "Limit the resolution at 1 unit from the light origin instead of "
                             "relative to the shadowed pixel");
    RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
    RNA_def_property_update(prop, 0, "rna_Light_update");
  }
}

static void rna_def_point_light(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "PointLight", "Light");
  RNA_def_struct_sdna(srna, "Light");
  RNA_def_struct_ui_text(srna, "Point Light", "Omnidirectional point Light");
  RNA_def_struct_ui_icon(srna, ICON_LIGHT_POINT);

  PropertyRNA *prop;
  prop = RNA_def_property(srna, "use_soft_falloff", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_USE_SOFT_FALLOFF);
  RNA_def_property_ui_text(
      prop,
      "Soft Falloff",
      "Apply falloff to avoid sharp edges when the light geometry intersects with other objects");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  rna_def_light_energy(srna, LA_LOCAL);
  rna_def_light_shadow(srna, false);
}

static void rna_def_area_light(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_areashape_items[] = {
      {LA_AREA_SQUARE, "SQUARE", 0, "Square", ""},
      {LA_AREA_RECT, "RECTANGLE", 0, "Rectangle", ""},
      {LA_AREA_DISK, "DISK", 0, "Disk", ""},
      {LA_AREA_ELLIPSE, "ELLIPSE", 0, "Ellipse", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "AreaLight", "Light");
  RNA_def_struct_sdna(srna, "Light");
  RNA_def_struct_ui_text(srna, "Area Light", "Directional area Light");
  RNA_def_struct_ui_icon(srna, ICON_LIGHT_AREA);

  rna_def_light_energy(srna, LA_AREA);
  rna_def_light_shadow(srna, false);

  prop = RNA_def_property(srna, "shape", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "area_shape");
  RNA_def_property_enum_items(prop, prop_areashape_items);
  RNA_def_property_ui_text(prop, "Shape", "Shape of the area Light");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "area_size");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Size", "Size of the area of the area light, X direction size for rectangle shapes");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "size_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "area_sizey");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop,
      "Size Y",
      "Size of the area of the area light in the Y direction for rectangle shapes");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "spread", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "area_spread");
  RNA_def_property_range(prop, DEG2RADF(0.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_text(
      prop,
      "Spread",
      "How widely the emitted light fans out, as in the case of a gridded softbox");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");
}

static void rna_def_spot_light(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpotLight", "Light");
  RNA_def_struct_sdna(srna, "Light");
  RNA_def_struct_ui_text(srna, "Spot Light", "Directional cone Light");
  RNA_def_struct_ui_icon(srna, ICON_LIGHT_SPOT);

  rna_def_light_energy(srna, LA_SPOT);
  rna_def_light_shadow(srna, false);

  prop = RNA_def_property(srna, "use_square", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_SQUARE);
  RNA_def_property_ui_text(prop, "Square", "Cast a square spot light shape");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "spot_blend", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "spotblend");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Spot Blend", "The softness of the spotlight edge");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "spot_size", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "spotsize");
  RNA_def_property_range(prop, DEG2RADF(1.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop, "Beam Angle", "Angular diameter of the spotlight beam");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "show_cone", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_SHOW_CONE);
  RNA_def_property_ui_text(
      prop,
      "Show Cone",
      "Display transparent cone in 3D view to visualize which objects are contained in it");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "use_soft_falloff", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", LA_USE_SOFT_FALLOFF);
  RNA_def_property_ui_text(
      prop,
      "Soft Falloff",
      "Apply falloff to avoid sharp edges when the light geometry intersects with other objects");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");
}

static void rna_def_sun_light(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SunLight", "Light");
  RNA_def_struct_sdna(srna, "Light");
  RNA_def_struct_ui_text(srna, "Sun Light", "Constant direction parallel ray Light");
  RNA_def_struct_ui_icon(srna, ICON_LIGHT_SUN);

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "sun_angle");
  RNA_def_property_range(prop, DEG2RADF(0.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop, "Angle", "Angular diameter of the Sun as seen from the Earth");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  rna_def_light_energy(srna, LA_SUN);
  rna_def_light_shadow(srna, true);
}

void RNA_def_light(BlenderRNA *brna)
{
  rna_def_light(brna);
  rna_def_point_light(brna);
  rna_def_area_light(brna);
  rna_def_spot_light(brna);
  rna_def_sun_light(brna);
}

#endif
