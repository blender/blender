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

#include <float.h>
#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "BKE_context.h"
#  include "BKE_main.h"
#  include "BKE_texture.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

#  include "ED_node.h"

#  include "WM_api.h"

static PointerRNA rna_World_lighting_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_WorldLighting, ptr->id.data);
}

static PointerRNA rna_World_mist_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_WorldMistSettings, ptr->id.data);
}

static void rna_World_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  World *wo = ptr->id.data;

  DEG_id_tag_update(&wo->id, 0);
  WM_main_add_notifier(NC_WORLD | ND_WORLD, wo);
}

#  if 0
static void rna_World_draw_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  World *wo = ptr->id.data;

  DEG_id_tag_update(&wo->id, 0);
  WM_main_add_notifier(NC_WORLD | ND_WORLD_DRAW, wo);
}
#  endif

static void rna_World_draw_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  World *wo = ptr->id.data;

  DEG_id_tag_update(&wo->id, 0);
  WM_main_add_notifier(NC_WORLD | ND_WORLD_DRAW, wo);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, NULL);
}

static void rna_World_use_nodes_update(bContext *C, PointerRNA *ptr)
{
  World *wrld = (World *)ptr->data;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  if (wrld->use_nodes && wrld->nodetree == NULL) {
    ED_node_shader_default(C, &wrld->id);
  }

  DEG_relations_tag_update(bmain);
  rna_World_update(bmain, scene, ptr);
  rna_World_draw_update(bmain, scene, ptr);
}

#else

static void rna_def_lighting(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WorldLighting", NULL);
  RNA_def_struct_sdna(srna, "World");
  RNA_def_struct_nested(brna, srna, "World");
  RNA_def_struct_ui_text(srna, "Lighting", "Lighting for a World data-block");

  /* ambient occlusion */
  prop = RNA_def_property(srna, "use_ambient_occlusion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_AMB_OCC);
  RNA_def_property_ui_text(
      prop,
      "Use Ambient Occlusion",
      "Use Ambient Occlusion to add shadowing based on distance between objects");
  RNA_def_property_update(prop, 0, "rna_World_update");

  prop = RNA_def_property(srna, "ao_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "aoenergy");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 2);
  RNA_def_property_ui_text(prop, "Factor", "Factor for ambient occlusion blending");
  RNA_def_property_update(prop, 0, "rna_World_update");

  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "aodist");
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
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "WorldMistSettings", NULL);
  RNA_def_struct_sdna(srna, "World");
  RNA_def_struct_nested(brna, srna, "World");
  RNA_def_struct_ui_text(srna, "World Mist", "Mist settings for a World data-block");

  prop = RNA_def_property(srna, "use_mist", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_MIST);
  RNA_def_property_ui_text(
      prop, "Use Mist", "Occlude objects with the environment color as they are further away");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "misi");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Minimum", "Overall minimum intensity of the mist effect");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  prop = RNA_def_property(srna, "start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "miststa");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
  RNA_def_property_ui_text(
      prop, "Start", "Starting distance of the mist, measured from the camera");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  prop = RNA_def_property(srna, "depth", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "mistdist");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
  RNA_def_property_ui_text(prop, "Depth", "Distance over which the mist effect fades in");
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "misthi");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Height", "Control how much mist density decreases with height");
  RNA_def_property_update(prop, 0, "rna_World_update");

  prop = RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mistype");
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
  RNA_def_property_float_sdna(prop, NULL, "horr");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_world_color);
  RNA_def_property_ui_text(prop, "Color", "Color of the background");
  /* RNA_def_property_update(prop, 0, "rna_World_update"); */
  /* render-only uses this */
  RNA_def_property_update(prop, 0, "rna_World_draw_update");

  /* nested structs */
  prop = RNA_def_property(srna, "light_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "WorldLighting");
  RNA_def_property_pointer_funcs(prop, "rna_World_lighting_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Lighting", "World lighting settings");

  prop = RNA_def_property(srna, "mist_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "WorldMistSettings");
  RNA_def_property_pointer_funcs(prop, "rna_World_mist_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Mist", "World mist settings");

  /* nodes */
  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node based worlds");

  prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Use Nodes", "Use shader nodes to render the world");
  RNA_def_property_update(prop, 0, "rna_World_use_nodes_update");

  rna_def_lighting(brna);
  rna_def_world_mist(brna);
}

#endif
