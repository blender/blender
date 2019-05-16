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

#include "BLI_sys_types.h"
#include "BLI_math_base.h"
#include "BLI_math_rotation.h"

#include "BLT_translation.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "rna_internal.h"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "BKE_context.h"
#  include "BKE_main.h"
#  include "BKE_texture.h"

#  include "DEG_depsgraph.h"

#  include "ED_node.h"
#  include "WM_api.h"
#  include "WM_types.h"

static void rna_Light_buffer_size_set(PointerRNA *ptr, int value)
{
  Light *la = (Light *)ptr->data;

  CLAMP(value, 128, 10240);
  la->bufsize = value;
  la->bufsize &= (~15); /* round to multiple of 16 */
}

static StructRNA *rna_Light_refine(struct PointerRNA *ptr)
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

static void rna_Light_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Light *la = ptr->id.data;

  DEG_id_tag_update(&la->id, 0);
  WM_main_add_notifier(NC_LAMP | ND_LIGHTING, la);
}

static void rna_Light_draw_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Light *la = ptr->id.data;

  DEG_id_tag_update(&la->id, 0);
  WM_main_add_notifier(NC_LAMP | ND_LIGHTING_DRAW, la);
}

static void rna_Light_use_nodes_update(bContext *C, PointerRNA *ptr)
{
  Light *la = (Light *)ptr->data;

  if (la->use_nodes && la->nodetree == NULL)
    ED_node_shader_default(C, &la->id);

  rna_Light_update(CTX_data_main(C), CTX_data_scene(C), ptr);
}

#else
/* Don't define icons here, so they don't show up in the Light UI (properties Editor) - DingTo */
const EnumPropertyItem rna_enum_light_type_items[] = {
    {LA_LOCAL, "POINT", 0, "Point", "Omnidirectional point light source"},
    {LA_SUN, "SUN", 0, "Sun", "Constant direction parallel ray light source"},
    {LA_SPOT, "SPOT", 0, "Spot", "Directional cone light source"},
    {LA_AREA, "AREA", 0, "Area", "Directional area light source"},
    {0, NULL, 0, NULL, NULL},
};

static void rna_def_light(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  static float default_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  srna = RNA_def_struct(brna, "Light", "ID");
  RNA_def_struct_sdna(srna, "Light");
  RNA_def_struct_refine_func(srna, "rna_Light_refine");
  RNA_def_struct_ui_text(srna, "Light", "Light data-block for lighting a scene");
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_ID_LIGHT);
  RNA_def_struct_ui_icon(srna, ICON_LIGHT_DATA);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_light_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of Light");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_LIGHT);
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "dist");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_range(prop, 0, 1000, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Distance",
      "Falloff distance - the light is at half the original intensity at this point");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "r");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_color);
  RNA_def_property_ui_text(prop, "Color", "Light color");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "specular_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "spec_fac");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, 9999.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 2);
  RNA_def_property_ui_text(prop, "Specular Factor", "Specular reflection multiplier");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "use_custom_distance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_CUSTOM_ATTENUATION);
  RNA_def_property_ui_text(prop,
                           "Custom Attenuation",
                           "Use custom attenuation distance instead of global light threshold");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "cutoff_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "att_dist");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01f, 100.0f, 1.0, 2);
  RNA_def_property_ui_text(
      prop, "Cutoff Distance", "Distance at which the light influence will be set to 0");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  /* nodes */
  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node based lights");

  prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Use Nodes", "Use shader nodes to render the light");
  RNA_def_property_update(prop, 0, "rna_Light_use_nodes_update");

  /* common */
  rna_def_animdata_common(srna);
}

static void rna_def_light_energy(StructRNA *srna, bool distant)
{
  PropertyRNA *prop;

  if (distant) {
    /* Distant light strength has no unit defined, it's proportional to
     * Watt/m^2 and is not sensitive to scene unit scale. */
    prop = RNA_def_property(srna, "energy", PROP_FLOAT, PROP_NONE);
    RNA_def_property_float_default(prop, 10.0f);
    RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
    RNA_def_property_ui_text(prop, "Strength", "Amount of light emitted");
    RNA_def_property_update(prop, 0, "rna_Light_draw_update");
  }
  else {
    /* Lights with a location have power in Watt, which is sensitive to
     * scene unit scale. */
    prop = RNA_def_property(srna, "energy", PROP_FLOAT, PROP_POWER);
    RNA_def_property_float_default(prop, 10.0f);
    RNA_def_property_ui_range(prop, 0.0f, 1000000.0f, 10, 5);
    RNA_def_property_ui_text(prop, "Power", "Amount of light emitted");
    RNA_def_property_update(prop, 0, "rna_Light_draw_update");
  }
}

static void rna_def_light_falloff(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem prop_fallofftype_items[] = {
      {LA_FALLOFF_CONSTANT, "CONSTANT", 0, "Constant", ""},
      {LA_FALLOFF_INVLINEAR, "INVERSE_LINEAR", 0, "Inverse Linear", ""},
      {LA_FALLOFF_INVSQUARE, "INVERSE_SQUARE", 0, "Inverse Square", ""},
      {LA_FALLOFF_INVCOEFFICIENTS, "INVERSE_COEFFICIENTS", 0, "Inverse Coefficients", ""},
      {LA_FALLOFF_CURVE, "CUSTOM_CURVE", 0, "Custom Curve", ""},
      {LA_FALLOFF_SLIDERS, "LINEAR_QUADRATIC_WEIGHTED", 0, "Lin/Quad Weighted", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_fallofftype_items);
  RNA_def_property_ui_text(prop, "Falloff Type", "Intensity Decay with distance");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curfalloff");
  RNA_def_property_ui_text(prop, "Falloff Curve", "Custom light falloff curve");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "linear_attenuation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "att1");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Linear Attenuation", "Linear distance attenuation");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "quadratic_attenuation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "att2");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Quadratic Attenuation", "Quadratic distance attenuation");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "constant_coefficient", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "coeff_const");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Constant Coefficient", "Constant distance attenuation coefficient");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "linear_coefficient", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "coeff_lin");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Linear Coefficient", "Linear distance attenuation coefficient");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "quadratic_coefficient", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "coeff_quad");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Quadratic Coefficient", "Quadratic distance attenuation coefficient");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");
}

static void rna_def_light_shadow(StructRNA *srna, bool sun)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_SHADOW);
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "shadow_buffer_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "bufsize");
  RNA_def_property_range(prop, 128, 10240);
  RNA_def_property_ui_text(prop,
                           "Shadow Buffer Size",
                           "Resolution of the shadow buffer, higher values give crisper shadows "
                           "but use more memory");
  RNA_def_property_int_funcs(prop, NULL, "rna_Light_buffer_size_set", NULL);
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_buffer_clip_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "clipsta");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 9999.0f);
  RNA_def_property_ui_text(prop,
                           "Shadow Buffer Clip Start",
                           "Shadow map clip start, below which objects will not generate shadows");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "shadow_buffer_clip_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "clipend");
  RNA_def_property_float_default(prop, 40.0f);
  RNA_def_property_range(prop, 0.0f, 9999.0f);
  RNA_def_property_ui_text(prop,
                           "Shadow Buffer Clip End",
                           "Shadow map clip end, beyond which objects will not generate shadows");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "shadow_buffer_bias", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "bias");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.001f, 9999.0f);
  RNA_def_property_ui_range(prop, 0.001f, 5.0f, 1.0, 3);
  RNA_def_property_ui_text(prop, "Shadow Buffer Bias", "Bias for reducing self shadowing");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_buffer_bleed_bias", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "bleedbias");
  RNA_def_property_range(prop, 0.f, 1.f);
  RNA_def_property_ui_text(
      prop, "Shadow Buffer Bleed Bias", "Bias for reducing light-bleed on variance shadow maps");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_buffer_exp", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "bleedexp");
  RNA_def_property_float_default(prop, 2.5f);
  RNA_def_property_range(prop, 1.0f, 9999.0f);
  RNA_def_property_ui_text(
      prop, "Shadow Buffer Exponent", "Bias for reducing light-bleed on exponential shadow maps");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_buffer_soft", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "soft");
  RNA_def_property_float_default(prop, 3.0f);
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Shadow Buffer Soft", "Size of shadow buffer sampling area");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_buffer_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "samp");
  RNA_def_property_range(prop, 1, 16);
  RNA_def_property_ui_text(prop, "Samples", "Number of shadow buffer samples");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "shdwr");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Shadow Color", "Color of shadows cast by the light");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "shadow_soft_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "area_size");
  RNA_def_property_float_default(prop, 0.25f);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Shadow Soft Size", "Light size for ray shadow sampling (Raytraced shadows)");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  /* Eevee */
  prop = RNA_def_property(srna, "use_contact_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_SHAD_CONTACT);
  RNA_def_property_ui_text(prop,
                           "Contact Shadow",
                           "Use screen space raytracing to have correct shadowing "
                           "near occluder, or for small features that does not appear "
                           "in shadow maps");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "contact_shadow_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "contact_dist");
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_range(prop, 0.0f, 9999.0f);
  RNA_def_property_ui_text(prop,
                           "Contact Shadow Distance",
                           "World space distance in which to search for "
                           "screen space occluder");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "contact_shadow_bias", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "contact_bias");
  RNA_def_property_float_default(prop, 0.03f);
  RNA_def_property_range(prop, 0.001f, 9999.0f);
  RNA_def_property_ui_range(prop, 0.001f, 5.0f, 1.0, 3);
  RNA_def_property_ui_text(prop, "Contact Shadow Bias", "Bias to avoid self shadowing");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "contact_shadow_soft_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "contact_spread");
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_range(prop, 0.0f, 9999.0f);
  RNA_def_property_ui_text(
      prop, "Contact Shadow Soft", "Control how soft the contact shadows will be");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  prop = RNA_def_property(srna, "contact_shadow_thickness", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "contact_thickness");
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_range(prop, 0.0f, 9999.0f);
  RNA_def_property_ui_range(prop, 0, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Contact Shadow Thickness", "Pixel thickness used to detect occlusion");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  if (sun) {
    prop = RNA_def_property(srna, "shadow_cascade_max_distance", PROP_FLOAT, PROP_DISTANCE);
    RNA_def_property_float_sdna(prop, NULL, "cascade_max_dist");
    RNA_def_property_float_default(prop, 200.0f);
    RNA_def_property_range(prop, 0.0f, FLT_MAX);
    RNA_def_property_ui_text(prop,
                             "Cascade Max Distance",
                             "End distance of the cascaded shadow map (only in perspective view)");
    RNA_def_property_update(prop, 0, "rna_Light_update");

    prop = RNA_def_property(srna, "shadow_cascade_count", PROP_INT, PROP_NONE);
    RNA_def_property_int_sdna(prop, NULL, "cascade_count");
    RNA_def_property_int_default(prop, 4);
    RNA_def_property_range(prop, 1, 4);
    RNA_def_property_ui_text(
        prop, "Cascade Count", "Number of texture used by the cascaded shadow map");
    RNA_def_property_update(prop, 0, "rna_Light_update");

    prop = RNA_def_property(srna, "shadow_cascade_exponent", PROP_FLOAT, PROP_FACTOR);
    RNA_def_property_float_sdna(prop, NULL, "cascade_exponent");
    RNA_def_property_float_default(prop, 0.8f);
    RNA_def_property_range(prop, 0.0f, 1.0f);
    RNA_def_property_ui_text(prop,
                             "Exponential Distribution",
                             "Higher value increase resolution towards the viewpoint");
    RNA_def_property_update(prop, 0, "rna_Light_update");

    prop = RNA_def_property(srna, "shadow_cascade_fade", PROP_FLOAT, PROP_FACTOR);
    RNA_def_property_float_sdna(prop, NULL, "cascade_fade");
    RNA_def_property_float_default(prop, 0.1f);
    RNA_def_property_range(prop, 0.0f, 1.0f);
    RNA_def_property_ui_text(
        prop, "Cascade Fade", "How smooth is the transition between each cascade");
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

  rna_def_light_energy(srna, false);
  rna_def_light_falloff(srna);
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
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "AreaLight", "Light");
  RNA_def_struct_sdna(srna, "Light");
  RNA_def_struct_ui_text(srna, "Area Light", "Directional area Light");
  RNA_def_struct_ui_icon(srna, ICON_LIGHT_AREA);

  rna_def_light_energy(srna, false);
  rna_def_light_shadow(srna, false);
  rna_def_light_falloff(srna);

  prop = RNA_def_property(srna, "shape", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "area_shape");
  RNA_def_property_enum_items(prop, prop_areashape_items);
  RNA_def_property_ui_text(prop, "Shape", "Shape of the area Light");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "area_size");
  RNA_def_property_float_default(prop, 0.25f);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Size", "Size of the area of the area light, X direction size for rectangle shapes");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "size_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "area_sizey");
  RNA_def_property_float_default(prop, 0.25f);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 0.1, 3);
  RNA_def_property_ui_text(
      prop,
      "Size Y",
      "Size of the area of the area light in the Y direction for rectangle shapes");
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

  rna_def_light_energy(srna, false);
  rna_def_light_falloff(srna);
  rna_def_light_shadow(srna, false);

  prop = RNA_def_property(srna, "use_square", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_SQUARE);
  RNA_def_property_ui_text(prop, "Square", "Cast a square spot light shape");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "spot_blend", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "spotblend");
  RNA_def_property_float_default(prop, 0.15f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Spot Blend", "The softness of the spotlight edge");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "spot_size", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "spotsize");
  RNA_def_property_float_default(prop, DEG2RADF(45.0f));
  RNA_def_property_range(prop, DEG2RADF(1.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop, "Spot Size", "Angle of the spotlight beam");
  RNA_def_property_update(prop, 0, "rna_Light_draw_update");

  prop = RNA_def_property(srna, "show_cone", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_SHOW_CONE);
  RNA_def_property_ui_text(
      prop,
      "Show Cone",
      "Draw transparent cone in 3D view to visualize which objects are contained in it");
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
  RNA_def_property_float_sdna(prop, NULL, "sun_angle");
  RNA_def_property_float_default(prop, DEG2RADF(0.526f));
  RNA_def_property_range(prop, DEG2RADF(0.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop, "Angle", "Angular diameter of the Sun as seen from the Earth");
  RNA_def_property_update(prop, 0, "rna_Light_update");

  rna_def_light_energy(srna, true);
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
