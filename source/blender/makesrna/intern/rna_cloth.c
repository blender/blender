/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_cloth.c
 *  \ingroup RNA
 */

#include <stdlib.h>
#include <limits.h>

#include "DNA_cloth_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "BKE_cloth.h"
#include "BKE_modifier.h"

#include "BPH_mass_spring.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"

static void rna_cloth_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
}

static void rna_cloth_pinning_changed(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
/*	ClothSimSettings *settings = (ClothSimSettings *)ptr->data; */
	ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);

	cloth_free_modifier(clmd);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
}

static void rna_cloth_reset(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

	settings->reset = 1;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
}


static void rna_ClothSettings_max_bend_set(struct PointerRNA *ptr, float value)
{
	ClothSimSettings *settings = (ClothSimSettings *)ptr->data;
	
	/* check for clipping */
	if (value < settings->bending)
		value = settings->bending;
	
	settings->max_bend = value;
}

static void rna_ClothSettings_max_struct_set(struct PointerRNA *ptr, float value)
{
	ClothSimSettings *settings = (ClothSimSettings *)ptr->data;
	
	/* check for clipping */
	if (value < settings->structural)
		value = settings->structural;
	
	settings->max_struct = value;
}

static void rna_ClothSettings_max_sewing_set(struct PointerRNA *ptr, float value)
{
	ClothSimSettings *settings = (ClothSimSettings *)ptr->data;

	/* check for clipping */
	if (value < 0.0f)
		value = 0.0f;

	settings->max_sewing = value;
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

static PointerRNA rna_ClothSettings_rest_shape_key_get(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	ClothSimSettings *sim = (ClothSimSettings *)ptr->data;

	return rna_object_shapekey_index_get(ob->data, sim->shapekey_rest);
}

static void rna_ClothSettings_rest_shape_key_set(PointerRNA *ptr, PointerRNA value)
{
	Object *ob = (Object *)ptr->id.data;
	ClothSimSettings *sim = (ClothSimSettings *)ptr->data;

	sim->shapekey_rest = rna_object_shapekey_index_set(ob->data, value, sim->shapekey_rest);
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

static char *rna_ClothSettings_path(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	ModifierData *md = modifiers_findByType(ob, eModifierType_Cloth);

	if (md) {
		char name_esc[sizeof(md->name) * 2];
		BLI_strescape(name_esc, md->name, sizeof(name_esc));
		return BLI_sprintfN("modifiers[\"%s\"].settings", name_esc);
	}
	else {
		return NULL;
	}
}

static char *rna_ClothCollisionSettings_path(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	ModifierData *md = modifiers_findByType(ob, eModifierType_Cloth);

	if (md) {
		char name_esc[sizeof(md->name) * 2];
		BLI_strescape(name_esc, md->name, sizeof(name_esc));
		return BLI_sprintfN("modifiers[\"%s\"].collision_settings", name_esc);
	}
	else {
		return NULL;
	}
}

#else

static void rna_def_cloth_solver_result(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem status_items[] = {
	    {BPH_SOLVER_SUCCESS, "SUCCESS", 0, "Success", "Computation was successful"},
	    {BPH_SOLVER_NUMERICAL_ISSUE, "NUMERICAL_ISSUE", 0, "Numerical Issue", "The provided data did not satisfy the prerequisites"},
	    {BPH_SOLVER_NO_CONVERGENCE, "NO_CONVERGENCE", 0, "No Convergence", "Iterative procedure did not converge"},
	    {BPH_SOLVER_INVALID_INPUT, "INVALID_INPUT", 0, "Invalid Input", "The inputs are invalid, or the algorithm has been improperly called"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "ClothSolverResult", NULL);
	RNA_def_struct_ui_text(srna, "Solver Result", "Result of cloth solver iteration");
	
	RNA_define_verify_sdna(0);
	
	prop = RNA_def_property(srna, "status", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, status_items);
	RNA_def_property_enum_sdna(prop, NULL, "status");
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Status", "Status of the solver iteration");
	
	prop = RNA_def_property(srna, "max_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_error");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Maximum Error", "Maximum error during substeps");
	
	prop = RNA_def_property(srna, "min_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min_error");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Minimum Error", "Minimum error during substeps");
	
	prop = RNA_def_property(srna, "avg_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "avg_error");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Average Error", "Average error during substeps");
	
	prop = RNA_def_property(srna, "max_iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "max_iterations");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Maximum Iterations", "Maximum iterations during substeps");
	
	prop = RNA_def_property(srna, "min_iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "min_iterations");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Minimum Iterations", "Minimum iterations during substeps");
	
	prop = RNA_def_property(srna, "avg_iterations", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "avg_iterations");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Average Iterations", "Average iterations during substeps");
	
	RNA_define_verify_sdna(1);
}

static void rna_def_cloth_sim_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ClothSettings", NULL);
	RNA_def_struct_ui_text(srna, "Cloth Settings", "Cloth simulation settings for an object");
	RNA_def_struct_sdna(srna, "ClothSimSettings");
	RNA_def_struct_path_func(srna, "rna_ClothSettings_path");
	
	/* goal */
	
	prop = RNA_def_property(srna, "goal_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mingoal");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Goal Minimum",
	                         "Goal minimum, vertex group weights are scaled to match this range");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "goal_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxgoal");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Goal Maximum",
	                         "Goal maximum, vertex group weights are scaled to match this range");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "goal_default", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "defgoal");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Goal Default",
	                         "Default Goal (vertex target position) value, when no Vertex Group used");
	RNA_def_property_update(prop, 0, "rna_cloth_update");
	
	prop = RNA_def_property(srna, "goal_spring", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "goalspring");
	RNA_def_property_range(prop, 0.0f, 0.999f);
	RNA_def_property_ui_text(prop, "Goal Stiffness", "Goal (vertex target position) spring stiffness");
	RNA_def_property_update(prop, 0, "rna_cloth_update");
	
	prop = RNA_def_property(srna, "goal_friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "goalfrict");
	RNA_def_property_range(prop, 0.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Goal Damping", "Goal (vertex target position) friction");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "internal_friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "velocity_smooth");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Internal Friction", "");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "collider_friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "collider_friction");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Collider Friction", "");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "density_target", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "density_target");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Target Density", "Maximum density of hair");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "density_strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "density_strength");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Target Density Strength", "Influence of target density on the simulation");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	/* mass */

	prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Mass", "Mass of cloth material");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "vertex_group_mass", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ClothSettings_mass_vgroup_get", "rna_ClothSettings_mass_vgroup_length",
	                              "rna_ClothSettings_mass_vgroup_set");
	RNA_def_property_ui_text(prop, "Mass Vertex Group", "Vertex Group for pinning of vertices");
	RNA_def_property_update(prop, 0, "rna_cloth_pinning_changed");
	
	prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_ACCELERATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -100.0, 100.0);
	RNA_def_property_float_funcs(prop, "rna_ClothSettings_gravity_get", "rna_ClothSettings_gravity_set", NULL);
	RNA_def_property_ui_text(prop, "Gravity", "Gravity or external force vector");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	/* various */

	prop = RNA_def_property(srna, "air_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "Cvi");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Air Damping", "Air has normally some thickness which slows falling things down");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "vel_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vel_damping");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Velocity Damping",
	                         "Damp velocity to help cloth reach the resting position faster "
	                         "(1.0 = no damping, 0.0 = fully dampened)");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "use_pin_cloth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", CLOTH_SIMSETTINGS_FLAG_GOAL);
	RNA_def_property_ui_text(prop, "Pin Cloth", "Enable pinning of cloth vertices to other objects/positions");
	RNA_def_property_update(prop, 0, "rna_cloth_pinning_changed");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

	prop = RNA_def_property(srna, "pin_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "goalspring");
	RNA_def_property_range(prop, 0.0f, 50.0);
	RNA_def_property_ui_text(prop, "Pin Stiffness", "Pin (vertex target position) spring stiffness");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "quality", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "stepsPerFrame");
	RNA_def_property_range(prop, 1, 80);
	RNA_def_property_ui_text(prop, "Quality",
	                         "Quality of the simulation in steps per frame (higher is better quality but slower)");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "vertex_group_shrink", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ClothSettings_shrink_vgroup_get", "rna_ClothSettings_shrink_vgroup_length",
	                              "rna_ClothSettings_shrink_vgroup_set");
	RNA_def_property_ui_text(prop, "Shrink Vertex Group", "Vertex Group for shrinking cloth");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "shrink_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shrink_min");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Shrink Factor Min", "Min amount to shrink cloth by");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "shrink_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shrink_max");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Shrink Factor Max", "Max amount to shrink cloth by");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "voxel_cell_size", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "voxel_cell_size");
	RNA_def_property_range(prop, 0.0001f, 10000.0f);
	RNA_def_property_float_default(prop, 0.1f);
	RNA_def_property_ui_text(prop, "Voxel Grid Cell Size", "Size of the voxel grid cells for interaction effects");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	/* springs */

	prop = RNA_def_property(srna, "use_stiffness_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", CLOTH_SIMSETTINGS_FLAG_SCALING);
	RNA_def_property_ui_text(prop, "Stiffness Scaling",
	                         "If enabled, stiffness can be scaled along a weight painted vertex group");
	RNA_def_property_update(prop, 0, "rna_cloth_update");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	
	prop = RNA_def_property(srna, "spring_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "Cdis");
	RNA_def_property_range(prop, 0.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Spring Damping",
	                         "Damping of cloth velocity (higher = more smooth, less jiggling)");
	RNA_def_property_update(prop, 0, "rna_cloth_update");
	
	prop = RNA_def_property(srna, "structural_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "structural");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Structural Stiffness", "Overall stiffness of structure");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "structural_stiffness_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_struct");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_ClothSettings_max_struct_set", NULL);
	RNA_def_property_ui_text(prop, "Structural Stiffness Maximum", "Maximum structural stiffness value");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "sewing_force_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_sewing");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_ClothSettings_max_sewing_set", NULL);
	RNA_def_property_ui_text(prop, "Sewing Force Max", "Maximum sewing force");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "vertex_group_structural_stiffness", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ClothSettings_struct_vgroup_get",
	                              "rna_ClothSettings_struct_vgroup_length",
	                              "rna_ClothSettings_struct_vgroup_set");
	RNA_def_property_ui_text(prop, "Structural Stiffness Vertex Group",
	                         "Vertex group for fine control over structural stiffness");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "bending_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bending");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Bending Stiffness",
	                         "Wrinkle coefficient (higher = less smaller but more big wrinkles)");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "bending_stiffness_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_bend");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_ClothSettings_max_bend_set", NULL);
	RNA_def_property_ui_text(prop, "Bending Stiffness Maximum", "Maximum bending stiffness value");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "bending_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bending_damping");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Bending Spring Damping",
	                         "Damping of bending motion");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "use_sewing_springs", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", CLOTH_SIMSETTINGS_FLAG_SEW);
	RNA_def_property_ui_text(prop, "Sew Cloth", "Pulls loose edges together");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

	prop = RNA_def_property(srna, "vertex_group_bending", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ClothSettings_bend_vgroup_get", "rna_ClothSettings_bend_vgroup_length",
	                              "rna_ClothSettings_bend_vgroup_set");
	RNA_def_property_ui_text(prop, "Bending Stiffness Vertex Group",
	                         "Vertex group for fine control over bending stiffness");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");

	prop = RNA_def_property(srna, "pre_roll", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "preroll");
	RNA_def_property_range(prop, 0, MAXFRAME);
	RNA_def_property_ui_text(prop, "Pre Roll", "Start simulation a number of frames earlier to let the cloth settle in");
	RNA_def_property_update(prop, 0, "rna_cloth_reset");

	prop = RNA_def_property(srna, "rest_shape_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_pointer_funcs(prop, "rna_ClothSettings_rest_shape_key_get",
	                               "rna_ClothSettings_rest_shape_key_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Rest Shape Key", "Shape key to use the rest spring lengths from");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	/* unused */

	/* unused still */
#if 0
	prop = RNA_def_property(srna, "shear_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shear");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Shear Stiffness", "Shear spring stiffness");
#endif
	/* unused still */
#if 0
	prop = RNA_def_property(srna, "shear_stiffness_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_shear");
	RNA_def_property_range(prop, 0.0f, upperLimitf);
	RNA_def_property_ui_text(prop, "Shear Stiffness Maximum", "Maximum shear scaling value");
#endif
	/* unused still */
#if 0
	prop = RNA_def_property(srna, "effector_force_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eff_force_scale");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Effector Force Scale", "");
#endif
	/* unused still */
#if 0
	prop = RNA_def_property(srna, "effector_wind_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eff_wind_scale");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Effector Wind Scale", "");
#endif
	/* unused still */
#if 0
	prop = RNA_def_property(srna, "tearing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", CLOTH_SIMSETTINGS_FLAG_TEARING);
	RNA_def_property_ui_text(prop, "Tearing", "");
#endif
	/* unused still */
#if 0
	prop = RNA_def_property(srna, "max_spring_extensions", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxspringlen");
	RNA_def_property_range(prop, 1.0, 1000.0);
	RNA_def_property_ui_text(prop, "Maximum Spring Extension", "Maximum extension before spring gets cut");
#endif
}

static void rna_def_cloth_collision_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ClothCollisionSettings", NULL);
	RNA_def_struct_ui_text(srna, "Cloth Collision Settings",
	                       "Cloth simulation settings for self collision and collision with other objects");
	RNA_def_struct_sdna(srna, "ClothCollSettings");
	RNA_def_struct_path_func(srna, "rna_ClothCollisionSettings_path");

	/* general collision */

	prop = RNA_def_property(srna, "use_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", CLOTH_COLLSETTINGS_FLAG_ENABLED);
	RNA_def_property_ui_text(prop, "Enable Collision", "Enable collisions with other objects");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "repel_force", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "repel_force");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Repulsion Force", "Repulsion force to apply on cloth when close to colliding");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "distance_repel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "distance_repel");
	RNA_def_property_range(prop, 0.001f, 10.0f);
	RNA_def_property_float_default(prop, 0.005f);
	RNA_def_property_ui_text(prop, "Repulsion Distance",
	                         "Maximum distance to apply repulsion force, must be greater than minimum distance");
	RNA_def_property_update(prop, 0, "rna_cloth_update");
	
	prop = RNA_def_property(srna, "distance_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "epsilon");
	RNA_def_property_range(prop, 0.001f, 1.0f);
	RNA_def_property_ui_text(prop, "Minimum Distance",
	                         "Minimum distance between collision objects before collision response takes in");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 80.0f);
	RNA_def_property_ui_text(prop, "Friction", "Friction force if a collision happened (higher = less movement)");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "damping");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Restitution", "Amount of velocity lost on collision");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "collision_quality", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "loop_count");
	RNA_def_property_range(prop, 1, 20);
	RNA_def_property_ui_text(prop, "Collision Quality",
	                         "How many collision iterations should be done. (higher is better quality but slower)");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	/* self collision */

	prop = RNA_def_property(srna, "use_self_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", CLOTH_COLLSETTINGS_FLAG_SELF);
	RNA_def_property_ui_text(prop, "Enable Self Collision", "Enable self collisions");
	RNA_def_property_update(prop, 0, "rna_cloth_update");
	
	prop = RNA_def_property(srna, "self_distance_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "selfepsilon");
	RNA_def_property_range(prop, 0.5f, 1.0f);
	RNA_def_property_ui_text(prop, "Self Minimum Distance", "0.5 means no distance at all, 1.0 is maximum distance");
	RNA_def_property_update(prop, 0, "rna_cloth_update");
	
	prop = RNA_def_property(srna, "self_friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 80.0f);
	RNA_def_property_ui_text(prop, "Self Friction", "Friction/damping with self contact");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "self_collision_quality", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "self_loop_count");
	RNA_def_property_range(prop, 1, 10);
	RNA_def_property_ui_text(prop, "Self Collision Quality",
	                         "How many self collision iterations should be done "
	                         "(higher is better quality but slower)");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Collision Group", "Limit colliders to this Group");
	RNA_def_property_update(prop, 0, "rna_cloth_update");

	prop = RNA_def_property(srna, "vertex_group_self_collisions", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_CollSettings_selfcol_vgroup_get", "rna_CollSettings_selfcol_vgroup_length",
	                              "rna_CollSettings_selfcol_vgroup_set");
	RNA_def_property_ui_text(prop, "Selfcollision Vertex Group",
	                         "Vertex group to define vertices which are not used during self collisions");
	RNA_def_property_update(prop, 0, "rna_cloth_update");
}

void RNA_def_cloth(BlenderRNA *brna)
{
	rna_def_cloth_solver_result(brna);
	rna_def_cloth_sim_settings(brna);
	rna_def_cloth_collision_settings(brna);
}

#endif
