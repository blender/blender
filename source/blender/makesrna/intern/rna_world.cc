/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <float.h>
#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "BKE_context.h"
#  include "BKE_layer.h"
#  include "BKE_main.h"
#  include "BKE_texture.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

#  include "ED_node.h"

#  include "WM_api.h"

static PointerRNA rna_World_lighting_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_WorldLighting, ptr->owner_id);
}

static PointerRNA rna_World_mist_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_WorldMistSettings, ptr->owner_id);
}

static void rna_World_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  World *wo = (World *)ptr->owner_id;

  DEG_id_tag_update(&wo->id, 0);
  WM_main_add_notifier(NC_WORLD | ND_WORLD, wo);
}

#  if 0
static void rna_World_draw_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  World *wo = (World *)ptr->owner_id;

  DEG_id_tag_update(&wo->id, 0);
  WM_main_add_notifier(NC_WORLD | ND_WORLD_DRAW, wo);
}
#  endif

static void rna_World_draw_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  World *wo = (World *)ptr->owner_id;

  DEG_id_tag_update(&wo->id, 0);
  WM_main_add_notifier(NC_WORLD | ND_WORLD_DRAW, wo);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
}

static void rna_World_use_nodes_update(bContext *C, PointerRNA *ptr)
{
  World *wrld = (World *)ptr->data;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  if (wrld->use_nodes && wrld->nodetree == nullptr) {
    ED_node_shader_default(C, &wrld->id);
  }

  DEG_relations_tag_update(bmain);
  rna_World_update(bmain, scene, ptr);
  rna_World_draw_update(bmain, scene, ptr);
}

void rna_World_lightgroup_get(PointerRNA *ptr, char *value)
{
  LightgroupMembership *lgm = ((World *)ptr->owner_id)->lightgroup;
  char value_buf[sizeof(lgm->name)];
  int len = BKE_lightgroup_membership_get(lgm, value_buf);
  memcpy(value, value_buf, len + 1);
}

int rna_World_lightgroup_length(PointerRNA *ptr)
{
  LightgroupMembership *lgm = ((World *)ptr->owner_id)->lightgroup;
  return BKE_lightgroup_membership_length(lgm);
}

void rna_World_lightgroup_set(PointerRNA *ptr, const char *value)
{
  BKE_lightgroup_membership_set(&((World *)ptr->owner_id)->lightgroup, value);
}

#else

static const EnumPropertyItem world_probe_resolution_items[] = {
    {LIGHT_PROBE_RESOLUTION_64, "64", 0, "64", ""},
    {LIGHT_PROBE_RESOLUTION_128, "128", 0, "128", ""},
    {LIGHT_PROBE_RESOLUTION_256, "256", 0, "256", ""},
    {LIGHT_PROBE_RESOLUTION_512, "512", 0, "512", ""},
    {LIGHT_PROBE_RESOLUTION_1024, "1024", 0, "1024", ""},
    {LIGHT_PROBE_RESOLUTION_2048, "2048", 0, "2048", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_lighting(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WorldLighting", nullptr);
  RNA_def_struct_sdna(srna, "World");
  RNA_def_struct_nested(brna, srna, "World");
  RNA_def_struct_ui_text(srna, "Lighting", "Lighting for a World data-block");

  /* ambient occlusion */
  prop = RNA_def_property(srna, "ao_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "aoenergy");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 2);
  RNA_def_property_ui_text(prop, "Factor", "Factor for ambient occlusion blending");
  RNA_def_property_update(prop, 0, "rna_World_update");

  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "aodist");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Distance", "Length of rays, defines how far away other faces give occlusion effect");
  RNA_def_property_update(prop, 0, "rna_World_update");
}

static void rna_def_world_mist(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem falloff_items[] = {
      {WO_MIST_QUADRATIC, "QUADRATIC", 0, "Quadratic", "Use quadratic progression"},
      {WO_MIST_LINEAR, "LINEAR", 0, "Linear", "Use linear progression"},
      {WO_MIST_INVERSE_QUADRATIC,
       "INVERSE_QUADRATIC",
       0,
       "Inverse Quadratic",
       "Use inverse quadratic progression"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "WorldMistSettings", nullptr);
  RNA_def_struct_sdna(srna, "World");
  RNA_def_struct_nested(brna, srna, "World");
  RNA_def_struct_ui_text(srna, "World Mist", "Mist settings for a World data-block");

  prop = RNA_def_property(srna, "use_mist", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", WO_MIST);
  RNA_def_property_ui_text(
      prop, "Use Mist", "Occlude objects with the environment color as they are further away");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "misi");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Minimum", "Overall minimum intensity of the mist effect");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  prop = RNA_def_property(srna, "start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "miststa");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
  RNA_def_property_ui_text(
      prop, "Start", "Starting distance of the mist, measured from the camera");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  prop = RNA_def_property(srna, "depth", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "mistdist");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
  RNA_def_property_ui_text(prop, "Depth", "Distance over which the mist effect fades in");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "misthi");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Height", "Control how much mist density decreases with height");
  RNA_def_property_update(prop, 0, "rna_World_update");

  prop = RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mistype");
  RNA_def_property_enum_items(prop, falloff_items);
  RNA_def_property_ui_text(prop, "Falloff", "Type of transition used to fade mist");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");
}

void RNA_def_world(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static float default_world_color[] = {0.05f, 0.05f, 0.05f};

  srna = RNA_def_struct(brna, "World", "ID");
  RNA_def_struct_ui_text(
      srna,
      "World",
      "World data-block describing the environment and ambient lighting of a scene");
  RNA_def_struct_ui_icon(srna, ICON_WORLD_DATA);

  rna_def_animdata_common(srna);

  /* colors */
  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "horr");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_world_color);
  RNA_def_property_ui_text(prop, "Color", "Color of the background");
  // RNA_def_property_update(prop, 0, "rna_World_update");
  /* render-only uses this */
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  /* nested structs */
  prop = RNA_def_property(srna, "light_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "WorldLighting");
  RNA_def_property_pointer_funcs(prop, "rna_World_lighting_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Lighting", "World lighting settings");

  prop = RNA_def_property(srna, "mist_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "WorldMistSettings");
  RNA_def_property_pointer_funcs(prop, "rna_World_mist_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Mist", "World mist settings");

  /* nodes */
  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "nodetree");
  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node based worlds");

  prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_nodes", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Use Nodes", "Use shader nodes to render the world");
  RNA_def_property_update(prop, 0, "rna_World_use_nodes_update");

  /* Lightgroup Membership */
  prop = RNA_def_property(srna, "lightgroup", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_World_lightgroup_get", "rna_World_lightgroup_length", "rna_World_lightgroup_set");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Lightgroup", "Lightgroup that the world belongs to");

  /* Reflection Probe Baking. */
  prop = RNA_def_property(srna, "probe_resolution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "probe_resolution");
  RNA_def_property_enum_items(prop, world_probe_resolution_items);
  RNA_def_property_ui_text(prop, "Resolution", "Resolution when baked to a texture");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  rna_def_lighting(brna);
  rna_def_world_mist(brna);
}

#endif
