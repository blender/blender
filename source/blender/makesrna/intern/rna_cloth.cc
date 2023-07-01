/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <limits.h>
#include <stdlib.h>

#include "DNA_cloth_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "BLI_math.h"

#include "BKE_cloth.h"
#include "BKE_modifier.h"

#include "SIM_mass_spring.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#  include "BKE_context.h"
#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

static void rna_cloth_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
}

static void rna_cloth_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  DEG_relations_tag_update(bmain);
  rna_cloth_update(bmain, scene, ptr);
}

static void rna_cloth_pinning_changed(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  /*  ClothSimSettings *settings = (ClothSimSettings *)ptr->data; */
  ClothModifierData *clmd = (ClothModifierData *)BKE_modifiers_findby_type(ob,
                                                                           eModifierType_Cloth);

  cloth_free_modifier(clmd);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
}

static void rna_ClothSettings_bending_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  settings->bending = value;

  /* check for max clipping */
  if (value > settings->max_bend) {
    settings->max_bend = value;
  }
}

static void rna_ClothSettings_max_bend_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  /* check for clipping */
  if (value < settings->bending) {
    value = settings->bending;
  }

  settings->max_bend = value;
}

static void rna_ClothSettings_tension_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  settings->tension = value;

  /* check for max clipping */
  if (value > settings->max_tension) {
    settings->max_tension = value;
  }
}

static void rna_ClothSettings_max_tension_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  /* check for clipping */
  if (value < settings->tension) {
    value = settings->tension;
  }

  settings->max_tension = value;
}

static void rna_ClothSettings_compression_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  settings->compression = value;

  /* check for max clipping */
  if (value > settings->max_compression) {
    settings->max_compression = value;
  }
}

static void rna_ClothSettings_max_compression_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  /* check for clipping */
  if (value < settings->compression) {
    value = settings->compression;
  }

  settings->max_compression = value;
}

static void rna_ClothSettings_shear_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  settings->shear = value;

  /* check for max clipping */
  if (value > settings->max_shear) {
    settings->max_shear = value;
  }
}

static void rna_ClothSettings_max_shear_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  /* check for clipping */
  if (value < settings->shear) {
    value = settings->shear;
  }

  settings->max_shear = value;
}

static void rna_ClothSettings_max_sewing_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  /* check for clipping */
  if (value < 0.0f) {
    value = 0.0f;
  }

  settings->max_sewing = value;
}

static void rna_ClothSettings_shrink_min_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  settings->shrink_min = value;

  /* check for max clipping */
  if (value > settings->shrink_max) {
    settings->shrink_max = value;
  }
}

static void rna_ClothSettings_shrink_max_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  /* check for clipping */
  if (value < settings->shrink_min) {
    value = settings->shrink_min;
  }

  settings->shrink_max = value;
}

static void rna_ClothSettings_internal_tension_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  settings->internal_tension = value;

  /* check for max clipping */
  if (value > settings->max_internal_tension) {
    settings->max_internal_tension = value;
  }
}

static void rna_ClothSettings_max_internal_tension_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  /* check for clipping */
  if (value < settings->internal_tension) {
    value = settings->internal_tension;
  }

  settings->max_internal_tension = value;
}

static void rna_ClothSettings_internal_compression_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  settings->internal_compression = value;

  /* check for max clipping */
  if (value > settings->max_internal_compression) {
    settings->max_internal_compression = value;
  }
}

static void rna_ClothSettings_max_internal_compression_set(PointerRNA *ptr, float value)
{
  ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

  /* check for clipping */
  if (value < settings->internal_compression) {
    value = settings->internal_compression;
  }

  settings->max_internal_compression = value;
}

static void rna_ClothSettings_mass_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, sim->vgroup_mass);
}

static int rna_ClothSettings_mass_vgroup_length(PointerRNA *ptr)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, sim->vgroup_mass);
}

static void rna_ClothSettings_mass_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &sim->vgroup_mass);
}

static void rna_ClothSettings_shrink_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, sim->vgroup_shrink);
}

static int rna_ClothSettings_shrink_vgroup_length(PointerRNA *ptr)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, sim->vgroup_shrink);
}

static void rna_ClothSettings_shrink_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &sim->vgroup_shrink);
}

static void rna_ClothSettings_struct_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, sim->vgroup_struct);
}

static int rna_ClothSettings_struct_vgroup_length(PointerRNA *ptr)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, sim->vgroup_struct);
}

static void rna_ClothSettings_struct_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &sim->vgroup_struct);
}

static void rna_ClothSettings_shear_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, sim->vgroup_shear);
}

static int rna_ClothSettings_shear_vgroup_length(PointerRNA *ptr)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, sim->vgroup_shear);
}

static void rna_ClothSettings_shear_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &sim->vgroup_shear);
}

static void rna_ClothSettings_bend_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, sim->vgroup_bend);
}

static int rna_ClothSettings_bend_vgroup_length(PointerRNA *ptr)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, sim->vgroup_bend);
}

static void rna_ClothSettings_bend_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &sim->vgroup_bend);
}

static void rna_ClothSettings_internal_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, sim->vgroup_intern);
}

static int rna_ClothSettings_internal_vgroup_length(PointerRNA *ptr)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, sim->vgroup_intern);
}

static void rna_ClothSettings_internal_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &sim->vgroup_intern);
}

static void rna_ClothSettings_pressure_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, sim->vgroup_pressure);
}

static int rna_ClothSettings_pressure_vgroup_length(PointerRNA *ptr)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, sim->vgroup_pressure);
}

static void rna_ClothSettings_pressure_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &sim->vgroup_pressure);
}

static void rna_CollSettings_selfcol_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothCollSettings *coll = (ClothCollSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, coll->vgroup_selfcol);
}

static int rna_CollSettings_selfcol_vgroup_length(PointerRNA *ptr)
{
  ClothCollSettings *coll = (ClothCollSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, coll->vgroup_selfcol);
}

static void rna_CollSettings_selfcol_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothCollSettings *coll = (ClothCollSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &coll->vgroup_selfcol);
}

static void rna_CollSettings_objcol_vgroup_get(PointerRNA *ptr, char *value)
{
  ClothCollSettings *coll = (ClothCollSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, coll->vgroup_objcol);
}

static int rna_CollSettings_objcol_vgroup_length(PointerRNA *ptr)
{
  ClothCollSettings *coll = (ClothCollSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, coll->vgroup_objcol);
}

static void rna_CollSettings_objcol_vgroup_set(PointerRNA *ptr, const char *value)
{
  ClothCollSettings *coll = (ClothCollSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &coll->vgroup_objcol);
}

static PointerRNA rna_ClothSettings_rest_shape_key_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;

  return rna_object_shapekey_index_get(static_cast<ID *>(ob->data), sim->shapekey_rest);
}

static void rna_ClothSettings_rest_shape_key_set(PointerRNA *ptr,
                                                 PointerRNA value,
                                                 ReportList * /*reports*/)
{
  Object *ob = (Object *)ptr->owner_id;
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;

  sim->shapekey_rest = rna_object_shapekey_index_set(
      static_cast<ID *>(ob->data), value, sim->shapekey_rest);
}

static void rna_ClothSettings_gravity_get(PointerRNA *ptr, float *values)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;

  values[0] = sim->gravity[0];
  values[1] = sim->gravity[1];
  values[2] = sim->gravity[2];
}

static void rna_ClothSettings_gravity_set(PointerRNA *ptr, const float *values)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;

  sim->gravity[0] = values[0];
  sim->gravity[1] = values[1];
  sim->gravity[2] = values[2];
}

static char *rna_ClothSettings_path(const PointerRNA *ptr)
{
  const Object *ob = (Object *)ptr->owner_id;
  const ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_Cloth);

  if (md) {
    char name_esc[sizeof(md->name) * 2];
    BLI_str_escape(name_esc, md->name, sizeof(name_esc));
    return BLI_sprintfN("modifiers[\"%s\"].settings", name_esc);
  }
  else {
    return nullptr;
  }
}

static char *rna_ClothCollisionSettings_path(const PointerRNA *ptr)
{
  const Object *ob = (Object *)ptr->owner_id;
  const ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_Cloth);

  if (md) {
    char name_esc[sizeof(md->name) * 2];
    BLI_str_escape(name_esc, md->name, sizeof(name_esc));
    return BLI_sprintfN("modifiers[\"%s\"].collision_settings", name_esc);
  }
  else {
    return nullptr;
  }
}

static int rna_ClothSettings_internal_editable(PointerRNA *ptr, const char **r_info)
{
  ClothSimSettings *sim = (ClothSimSettings *)ptr->data;

  if (sim && (sim->bending_model == CLOTH_BENDING_LINEAR)) {
    *r_info = "Only available with angular bending springs.";
    return 0;
  }

  return sim ? PROP_EDITABLE : 0;
}

#else

static void rna_def_cloth_solver_result(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem status_items[] = {
      {SIM_SOLVER_SUCCESS, "SUCCESS", 0, "Success", "Computation was successful"},
      {SIM_SOLVER_NUMERICAL_ISSUE,
       "NUMERICAL_ISSUE",
       0,
       "Numerical Issue",
       "The provided data did not satisfy the prerequisites"},
      {SIM_SOLVER_NO_CONVERGENCE,
       "NO_CONVERGENCE",
       0,
       "No Convergence",
       "Iterative procedure did not converge"},
      {SIM_SOLVER_INVALID_INPUT,
       "INVALID_INPUT",
       0,
       "Invalid Input",
       "The inputs are invalid, or the algorithm has been improperly called"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ClothSolverResult", nullptr);
  RNA_def_struct_ui_text(srna, "Solver Result", "Result of cloth solver iteration");

  RNA_define_verify_sdna(0);

  prop = RNA_def_property(srna, "status", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, status_items);
  RNA_def_property_enum_sdna(prop, nullptr, "status");
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Status", "Status of the solver iteration");

  prop = RNA_def_property(srna, "max_error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max_error");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Maximum Error", "Maximum error during substeps");

  prop = RNA_def_property(srna, "min_error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "min_error");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Minimum Error", "Minimum error during substeps");

  prop = RNA_def_property(srna, "avg_error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "avg_error");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Average Error", "Average error during substeps");

  prop = RNA_def_property(srna, "max_iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "max_iterations");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Maximum Iterations", "Maximum iterations during substeps");

  prop = RNA_def_property(srna, "min_iterations", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "min_iterations");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Minimum Iterations", "Minimum iterations during substeps");

  prop = RNA_def_property(srna, "avg_iterations", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "avg_iterations");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Average Iterations", "Average iterations during substeps");

  RNA_define_verify_sdna(1);
}

static void rna_def_cloth_sim_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_bending_model_items[] = {
      {CLOTH_BENDING_ANGULAR, "ANGULAR", 0, "Angular", "Cloth model with angular bending springs"},
      {CLOTH_BENDING_LINEAR,
       "LINEAR",
       0,
       "Linear",
       "Cloth model with linear bending springs (legacy)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ClothSettings", nullptr);
  RNA_def_struct_ui_text(srna, "Cloth Settings", "Cloth simulation settings for an object");
  RNA_def_struct_sdna(srna, "ClothSimSettings");
  RNA_def_struct_path_func(srna, "rna_ClothSettings_path");

  RNA_define_lib_overridable(true);

  /* goal */

  prop = RNA_def_property(srna, "goal_min", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "mingoal");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Goal Minimum", "Goal minimum, vertex group weights are scaled to match this range");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "goal_max", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "maxgoal");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Goal Maximum", "Goal maximum, vertex group weights are scaled to match this range");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "goal_default", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "defgoal");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Goal Default",
      "Default Goal (vertex target position) value, when no Vertex Group used");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "goal_spring", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "goalspring");
  RNA_def_property_range(prop, 0.0f, 0.999f);
  RNA_def_property_ui_text(
      prop, "Goal Stiffness", "Goal (vertex target position) spring stiffness");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "goal_friction", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "goalfrict");
  RNA_def_property_range(prop, 0.0f, 50.0f);
  RNA_def_property_ui_text(prop, "Goal Damping", "Goal (vertex target position) friction");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "internal_friction", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "velocity_smooth");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Internal Friction", "");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "collider_friction", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "collider_friction");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Collider Friction", "");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "density_target", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "density_target");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Target Density", "Maximum density of hair");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "density_strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "density_strength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Target Density Strength", "Influence of target density on the simulation");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  /* mass */

  prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_UNIT_MASS);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Vertex Mass", "The mass of each vertex on the cloth material");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "vertex_group_mass", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ClothSettings_mass_vgroup_get",
                                "rna_ClothSettings_mass_vgroup_length",
                                "rna_ClothSettings_mass_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Pin Vertex Group", "Vertex Group for pinning of vertices");
  RNA_def_property_update(prop, 0, "rna_cloth_pinning_changed");

  prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_ACCELERATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -100.0, 100.0);
  RNA_def_property_float_funcs(
      prop, "rna_ClothSettings_gravity_get", "rna_ClothSettings_gravity_set", nullptr);
  RNA_def_property_ui_text(prop, "Gravity", "Gravity or external force vector");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  /* various */

  prop = RNA_def_property(srna, "air_damping", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "Cvi");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(
      prop, "Air Damping", "Air has normally some thickness which slows falling things down");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "pin_stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "goalspring");
  RNA_def_property_range(prop, 0.0f, 50.0);
  RNA_def_property_ui_text(prop, "Pin Stiffness", "Pin (vertex target position) spring stiffness");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "stepsPerFrame");
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_ui_range(prop, 1, 80, 1, -1);
  RNA_def_property_ui_text(
      prop,
      "Quality",
      "Quality of the simulation in steps per frame (higher is better quality but slower)");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "time_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "time_scale");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Speed", "Cloth speed is multiplied by this value");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "vertex_group_shrink", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ClothSettings_shrink_vgroup_get",
                                "rna_ClothSettings_shrink_vgroup_length",
                                "rna_ClothSettings_shrink_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Shrink Vertex Group", "Vertex Group for shrinking cloth");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "shrink_min", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "shrink_min");
  RNA_def_property_range(prop, -FLT_MAX, 1.0f);
  RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.05f, 3);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_shrink_min_set", nullptr);
  RNA_def_property_ui_text(prop, "Shrink Factor", "Factor by which to shrink cloth");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "shrink_max", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "shrink_max");
  RNA_def_property_range(prop, -FLT_MAX, 1.0f);
  RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.05f, 3);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_shrink_max_set", nullptr);
  RNA_def_property_ui_text(prop, "Shrink Factor Max", "Max amount to shrink cloth by");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "voxel_cell_size", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "voxel_cell_size");
  RNA_def_property_range(prop, 0.0001f, 10000.0f);
  RNA_def_property_ui_text(
      prop, "Voxel Grid Cell Size", "Size of the voxel grid cells for interaction effects");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  /* springs */
  prop = RNA_def_property(srna, "tension_damping", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "tension_damp");
  RNA_def_property_range(prop, 0.0f, 50.0f);
  RNA_def_property_ui_text(
      prop, "Tension Spring Damping", "Amount of damping in stretching behavior");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "compression_damping", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "compression_damp");
  RNA_def_property_range(prop, 0.0f, 50.0f);
  RNA_def_property_ui_text(
      prop, "Compression Spring Damping", "Amount of damping in compression behavior");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "shear_damping", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "shear_damp");
  RNA_def_property_range(prop, 0.0f, 50.0f);
  RNA_def_property_ui_text(prop, "Shear Spring Damping", "Amount of damping in shearing behavior");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "tension_stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "tension");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_tension_set", nullptr);
  RNA_def_property_ui_text(prop, "Tension Stiffness", "How much the material resists stretching");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "tension_stiffness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max_tension");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_max_tension_set", nullptr);
  RNA_def_property_ui_text(prop, "Tension Stiffness Maximum", "Maximum tension stiffness value");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "compression_stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "compression");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_compression_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Compression Stiffness", "How much the material resists compression");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "compression_stiffness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max_compression");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_max_compression_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Compression Stiffness Maximum", "Maximum compression stiffness value");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "shear_stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "shear");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_shear_set", nullptr);
  RNA_def_property_ui_text(prop, "Shear Stiffness", "How much the material resists shearing");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "shear_stiffness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max_shear");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_max_shear_set", nullptr);
  RNA_def_property_ui_text(prop, "Shear Stiffness Maximum", "Maximum shear scaling value");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "sewing_force_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max_sewing");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_max_sewing_set", nullptr);
  RNA_def_property_ui_text(prop, "Sewing Force Max", "Maximum sewing force");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "vertex_group_structural_stiffness", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ClothSettings_struct_vgroup_get",
                                "rna_ClothSettings_struct_vgroup_length",
                                "rna_ClothSettings_struct_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop,
                           "Structural Stiffness Vertex Group",
                           "Vertex group for fine control over structural stiffness");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "vertex_group_shear_stiffness", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ClothSettings_shear_vgroup_get",
                                "rna_ClothSettings_shear_vgroup_length",
                                "rna_ClothSettings_shear_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Shear Stiffness Vertex Group", "Vertex group for fine control over shear stiffness");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "bending_stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "bending");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_bending_set", nullptr);
  RNA_def_property_ui_text(prop, "Bending Stiffness", "How much the material resists bending");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "bending_stiffness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max_bend");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_max_bend_set", nullptr);
  RNA_def_property_ui_text(prop, "Bending Stiffness Maximum", "Maximum bending stiffness value");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "bending_damping", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "bending_damping");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_text(
      prop, "Bending Spring Damping", "Amount of damping in bending behavior");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "use_sewing_springs", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", CLOTH_SIMSETTINGS_FLAG_SEW);
  RNA_def_property_ui_text(prop, "Sew Cloth", "Pulls loose edges together");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "vertex_group_bending", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ClothSettings_bend_vgroup_get",
                                "rna_ClothSettings_bend_vgroup_length",
                                "rna_ClothSettings_bend_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop,
                           "Bending Stiffness Vertex Group",
                           "Vertex group for fine control over bending stiffness");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "EffectorWeights");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Effector Weights", "");

  prop = RNA_def_property(srna, "rest_shape_key", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "ShapeKey");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_ClothSettings_rest_shape_key_get",
                                 "rna_ClothSettings_rest_shape_key_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Rest Shape Key", "Shape key to use the rest spring lengths from");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "use_dynamic_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", CLOTH_SIMSETTINGS_FLAG_DYNAMIC_BASEMESH);
  RNA_def_property_ui_text(
      prop, "Dynamic Base Mesh", "Make simulation respect deformations in the base mesh");
  RNA_def_property_update(prop, 0, "rna_cloth_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "bending_model", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bending_model");
  RNA_def_property_enum_items(prop, prop_bending_model_items);
  RNA_def_property_ui_text(prop, "Bending Model", "Physical model for simulating bending forces");
  RNA_def_property_update(prop, 0, "rna_cloth_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_internal_springs", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", CLOTH_SIMSETTINGS_FLAG_INTERNAL_SPRINGS);
  RNA_def_property_ui_text(prop,
                           "Create Internal Springs",
                           "Simulate an internal volume structure by creating springs connecting "
                           "the opposite sides of the mesh");
  RNA_def_property_update(prop, 0, "rna_cloth_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "internal_spring_normal_check", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flags", CLOTH_SIMSETTINGS_FLAG_INTERNAL_SPRINGS_NORMAL);
  RNA_def_property_ui_text(prop,
                           "Check Internal Spring Normals",
                           "Require the points the internal springs connect to have opposite "
                           "normal directions");
  RNA_def_property_editable_func(prop, "rna_ClothSettings_internal_editable");
  RNA_def_property_update(prop, 0, "rna_cloth_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "internal_spring_max_length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "internal_spring_max_length");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_text(
      prop,
      "Internal Spring Max Length",
      "The maximum length an internal spring can have during creation. If the distance between "
      "internal points is greater than this, no internal spring will be created between these "
      "points. "
      "A length of zero means that there is no length limit");
  RNA_def_property_editable_func(prop, "rna_ClothSettings_internal_editable");
  RNA_def_property_update(prop, 0, "rna_cloth_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "internal_spring_max_diversion", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "internal_spring_max_diversion");
  RNA_def_property_range(prop, 0.0f, M_PI_4);
  RNA_def_property_ui_text(prop,
                           "Internal Spring Max Diversion",
                           "How much the rays used to connect the internal points can diverge "
                           "from the vertex normal");
  RNA_def_property_editable_func(prop, "rna_ClothSettings_internal_editable");
  RNA_def_property_update(prop, 0, "rna_cloth_update");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "internal_tension_stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "internal_tension");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(prop, nullptr, "rna_ClothSettings_internal_tension_set", nullptr);
  RNA_def_property_ui_text(prop, "Tension Stiffness", "How much the material resists stretching");
  RNA_def_property_editable_func(prop, "rna_ClothSettings_internal_editable");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "internal_tension_stiffness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max_internal_tension");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_ClothSettings_max_internal_tension_set", nullptr);
  RNA_def_property_ui_text(prop, "Tension Stiffness Maximum", "Maximum tension stiffness value");
  RNA_def_property_editable_func(prop, "rna_ClothSettings_internal_editable");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "internal_compression_stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "internal_compression");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_ClothSettings_internal_compression_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Compression Stiffness", "How much the material resists compression");
  RNA_def_property_editable_func(prop, "rna_ClothSettings_internal_editable");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "internal_compression_stiffness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max_internal_compression");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_ClothSettings_max_internal_compression_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Compression Stiffness Maximum", "Maximum compression stiffness value");
  RNA_def_property_editable_func(prop, "rna_ClothSettings_internal_editable");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "vertex_group_intern", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ClothSettings_internal_vgroup_get",
                                "rna_ClothSettings_internal_vgroup_length",
                                "rna_ClothSettings_internal_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop,
                           "Internal Springs Vertex Group",
                           "Vertex group for fine control over the internal spring stiffness");
  RNA_def_property_editable_func(prop, "rna_ClothSettings_internal_editable");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  /* Pressure */

  prop = RNA_def_property(srna, "use_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", CLOTH_SIMSETTINGS_FLAG_PRESSURE);
  RNA_def_property_ui_text(prop, "Use Pressure", "Simulate pressure inside a closed cloth mesh");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "use_pressure_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", CLOTH_SIMSETTINGS_FLAG_PRESSURE_VOL);
  RNA_def_property_ui_text(prop,
                           "Use Custom Volume",
                           "Use the Target Volume parameter as the initial volume, instead "
                           "of calculating it from the mesh itself");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "uniform_pressure_force", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "uniform_pressure_force");
  RNA_def_property_range(prop, -10000.0f, 10000.0f);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop,
                           "Pressure",
                           "The uniform pressure that is constantly applied to the mesh, in units "
                           "of Pressure Scale. Can be negative");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "target_volume", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "target_volume");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop,
                           "Target Volume",
                           "The mesh volume where the inner/outer pressure will be the same. If "
                           "set to zero the change in volume will not affect pressure");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "pressure_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "pressure_factor");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop,
                           "Pressure Scale",
                           "Ambient pressure (kPa) that balances out between the inside and "
                           "outside of the object when it has the target volume");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "fluid_density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fluid_density");
  RNA_def_property_ui_range(prop, -2.0f, 2.0f, 0.05f, 4);
  RNA_def_property_ui_text(
      prop,
      "Fluid Density",
      "Density (kg/l) of the fluid contained inside the object, used to create "
      "a hydrostatic pressure gradient simulating the weight of the internal fluid, "
      "or buoyancy from the surrounding fluid if negative");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "vertex_group_pressure", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ClothSettings_pressure_vgroup_get",
                                "rna_ClothSettings_pressure_vgroup_length",
                                "rna_ClothSettings_pressure_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop,
      "Pressure Vertex Group",
      "Vertex Group for where to apply pressure. Zero weight means no "
      "pressure while a weight of one means full pressure. Faces with a vertex "
      "that has zero weight will be excluded from the volume calculation");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  /* unused */

  /* unused still */
#  if 0
  prop = RNA_def_property(srna, "effector_force_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "eff_force_scale");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Effector Force Scale", "");
#  endif
  /* unused still */
#  if 0
  prop = RNA_def_property(srna, "effector_wind_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "eff_wind_scale");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Effector Wind Scale", "");
#  endif
  /* unused still */
#  if 0
  prop = RNA_def_property(srna, "tearing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", CLOTH_SIMSETTINGS_FLAG_TEARING);
  RNA_def_property_ui_text(prop, "Tearing", "");
#  endif
  /* unused still */
#  if 0
  prop = RNA_def_property(srna, "max_spring_extensions", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "maxspringlen");
  RNA_def_property_range(prop, 1.0, 1000.0);
  RNA_def_property_ui_text(
      prop, "Maximum Spring Extension", "Maximum extension before spring gets cut");
#  endif

  RNA_define_lib_overridable(false);
}

static void rna_def_cloth_collision_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ClothCollisionSettings", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Cloth Collision Settings",
      "Cloth simulation settings for self collision and collision with other objects");
  RNA_def_struct_sdna(srna, "ClothCollSettings");
  RNA_def_struct_path_func(srna, "rna_ClothCollisionSettings_path");

  RNA_define_lib_overridable(true);

  /* general collision */

  prop = RNA_def_property(srna, "use_collision", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", CLOTH_COLLSETTINGS_FLAG_ENABLED);
  RNA_def_property_ui_text(prop, "Enable Collision", "Enable collisions with other objects");
  RNA_def_property_update(prop, 0, "rna_cloth_dependency_update");

  prop = RNA_def_property(srna, "distance_min", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "epsilon");
  RNA_def_property_range(prop, 0.001f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Minimum Distance",
      "Minimum distance between collision objects before collision response takes effect");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "friction", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 80.0f);
  RNA_def_property_ui_text(
      prop, "Friction", "Friction force if a collision happened (higher = less movement)");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "damping");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Restitution", "Amount of velocity lost on collision");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "collision_quality", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "loop_count");
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_range(prop, 1, 20, 1, -1);
  RNA_def_property_ui_text(
      prop,
      "Collision Quality",
      "How many collision iterations should be done. (higher is better quality but slower)");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "impulse_clamp", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clamp");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(
      prop,
      "Impulse Clamping",
      "Clamp collision impulses to avoid instability (0.0 to disable clamping)");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  /* self collision */

  prop = RNA_def_property(srna, "use_self_collision", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", CLOTH_COLLSETTINGS_FLAG_SELF);
  RNA_def_property_ui_text(prop, "Enable Self Collision", "Enable self collisions");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "self_distance_min", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "selfepsilon");
  RNA_def_property_range(prop, 0.001f, 0.1f);
  RNA_def_property_ui_text(
      prop,
      "Self Minimum Distance",
      "Minimum distance between cloth faces before collision response takes effect");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "self_friction", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 80.0f);
  RNA_def_property_ui_text(prop, "Self Friction", "Friction with self contact");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "group");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Collision Collection", "Limit colliders to this Collection");
  RNA_def_property_update(prop, 0, "rna_cloth_dependency_update");

  prop = RNA_def_property(srna, "vertex_group_self_collisions", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_CollSettings_selfcol_vgroup_get",
                                "rna_CollSettings_selfcol_vgroup_length",
                                "rna_CollSettings_selfcol_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop,
      "Selfcollision Vertex Group",
      "Triangles with all vertices in this group are not used during self collisions");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "vertex_group_object_collisions", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_CollSettings_objcol_vgroup_get",
                                "rna_CollSettings_objcol_vgroup_length",
                                "rna_CollSettings_objcol_vgroup_set");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop,
      "Collision Vertex Group",
      "Triangles with all vertices in this group are not used during object collisions");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  prop = RNA_def_property(srna, "self_impulse_clamp", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "self_clamp");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(
      prop,
      "Impulse Clamping",
      "Clamp collision impulses to avoid instability (0.0 to disable clamping)");
  RNA_def_property_update(prop, 0, "rna_cloth_update");

  RNA_define_lib_overridable(false);
}

void RNA_def_cloth(BlenderRNA *brna)
{
  rna_def_cloth_solver_result(brna);
  rna_def_cloth_sim_settings(brna);
  rna_def_cloth_collision_settings(brna);
}

#endif
