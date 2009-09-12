/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_scene_types.h"
#include "DNA_boid_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "WM_api.h"
#include "WM_types.h"

EnumPropertyItem boidrule_type_items[] ={
	{eBoidRuleType_Goal, "GOAL", 0, "Goal", "Go to assigned object or loudest assigned signal source."},
	{eBoidRuleType_Avoid, "AVOID", 0, "Avoid", "Get away from assigned object or loudest assigned signal source."},
	{eBoidRuleType_AvoidCollision, "AVOID_COLLISION", 0, "Avoid Collision", "Monoeuver to avoid collisions with other boids and deflector objects in near future."},
	{eBoidRuleType_Separate, "SEPARATE", 0, "Separate", "Keep from going through other boids."},
	{eBoidRuleType_Flock, "FLOCK", 0, "Flock", "Move to center of neighbors and match their velocity."},
	{eBoidRuleType_FollowLeader, "FOLLOW_LEADER", 0, "Follow Leader", "Follow a boid or assigned object."},
	{eBoidRuleType_AverageSpeed, "AVERAGE_SPEED", 0, "Average Speed", "Maintain speed, flight level or wander."},
	{eBoidRuleType_Fight, "FIGHT", 0, "Fight", "Go to closest enemy and attack when in range."},
	//{eBoidRuleType_Protect, "PROTECT", 0, "Protect", "Go to enemy closest to target and attack when in range."},
	//{eBoidRuleType_Hide, "HIDE", 0, "Hide", "Find a deflector move to it's other side from closest enemy."},
	//{eBoidRuleType_FollowPath, "FOLLOW_PATH", 0, "Follow Path", "Move along a assigned curve or closest curve in a group."},
	//{eBoidRuleType_FollowWall, "FOLLOW_WALL", 0, "Follow Wall", "Move next to a deflector object's in direction of it's tangent."},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem boidruleset_type_items[] ={
	{eBoidRulesetType_Fuzzy, "FUZZY", 0, "Fuzzy", "Rules are gone through top to bottom. Only the first rule that effect above fuzziness threshold is evaluated."},
	{eBoidRulesetType_Random, "RANDOM", 0, "Random", "A random rule is selected for each boid."},
	{eBoidRulesetType_Average, "AVERAGE", 0, "Average", "All rules are averaged."},
	{0, NULL, 0, NULL, NULL}};


#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_particle.h"

static void rna_Boids_reset(bContext *C, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	ParticleSettings *part;

	if(ptr->type==&RNA_ParticleSystem) {
		ParticleSystem *psys = (ParticleSystem*)ptr->data;
		Object *ob = psys_find_object(scene, psys);
		
		psys->recalc = PSYS_RECALC_RESET;

		if(ob)
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	}
	else {
		part = ptr->id.data;
		psys_flush_particle_settings(scene, part, PSYS_RECALC_RESET);
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_DATA, NULL);
}
static void rna_Boids_reset_deps(bContext *C, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	ParticleSettings *part;

	if(ptr->type==&RNA_ParticleSystem) {
		ParticleSystem *psys = (ParticleSystem*)ptr->data;
		Object *ob = psys_find_object(scene, psys);
		
		psys->recalc = PSYS_RECALC_RESET;

		if(ob)
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	}
	else {
		part = ptr->id.data;
		psys_flush_particle_settings(scene, part, PSYS_RECALC_RESET);
		DAG_scene_sort(scene);
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_DATA, NULL);
}

static StructRNA* rna_BoidRule_refine(struct PointerRNA *ptr)
{
	BoidRule *rule= (BoidRule*)ptr->data;

	switch(rule->type) {
		case eBoidRuleType_Goal:
			return &RNA_BoidRuleGoal;
		case eBoidRuleType_Avoid:
			return &RNA_BoidRuleAvoid;
		case eBoidRuleType_AvoidCollision:
			return &RNA_BoidRuleAvoidCollision;
		case eBoidRuleType_FollowLeader:
			return &RNA_BoidRuleFollowLeader;
		case eBoidRuleType_AverageSpeed:
			return &RNA_BoidRuleAverageSpeed;
		case eBoidRuleType_Fight:
			return &RNA_BoidRuleFight;
		default:
			return &RNA_BoidRule;
	}
}

static char *rna_BoidRule_path(PointerRNA *ptr)
{
	return BLI_sprintfN("rules[%s]", ((BoidRule*)ptr->data)->name);  // XXX not unique
}

static PointerRNA rna_BoidState_active_boid_rule_get(PointerRNA *ptr)
{
	BoidState *state= (BoidState*)ptr->data;
	BoidRule *rule = (BoidRule*)state->rules.first;

	for(; rule; rule=rule->next) {
		if(rule->flag & BOIDRULE_CURRENT)
			return rna_pointer_inherit_refine(ptr, &RNA_BoidRule, rule);
	}
	return rna_pointer_inherit_refine(ptr, &RNA_BoidRule, NULL);
}
static void rna_BoidState_active_boid_rule_index_range(PointerRNA *ptr, int *min, int *max)
{
	BoidState *state= (BoidState*)ptr->data;
	*min= 0;
	*max= BLI_countlist(&state->rules)-1;
	*max= MAX2(0, *max);
}

static int rna_BoidState_active_boid_rule_index_get(PointerRNA *ptr)
{
	BoidState *state= (BoidState*)ptr->data;
	BoidRule *rule = (BoidRule*)state->rules.first;
	int i=0;

	for(; rule; rule=rule->next, i++) {
		if(rule->flag & BOIDRULE_CURRENT)
			return i;
	}
	return 0;
}

static void rna_BoidState_active_boid_rule_index_set(struct PointerRNA *ptr, int value)
{
	BoidState *state= (BoidState*)ptr->data;
	BoidRule *rule = (BoidRule*)state->rules.first;
	int i=0;

	for(; rule; rule=rule->next, i++) {
		if(i==value)
			rule->flag |= BOIDRULE_CURRENT;
		else
			rule->flag &= ~BOIDRULE_CURRENT;
	}
}

static PointerRNA rna_BoidSettings_active_boid_state_get(PointerRNA *ptr)
{
	BoidSettings *boids= (BoidSettings*)ptr->data;
	BoidState *state = (BoidState*)boids->states.first;

	for(; state; state=state->next) {
		if(state->flag & BOIDSTATE_CURRENT)
			return rna_pointer_inherit_refine(ptr, &RNA_BoidState, state);
	}
	return rna_pointer_inherit_refine(ptr, &RNA_BoidState, NULL);
}
static void rna_BoidSettings_active_boid_state_index_range(PointerRNA *ptr, int *min, int *max)
{
	BoidSettings *boids= (BoidSettings*)ptr->data;
	*min= 0;
	*max= BLI_countlist(&boids->states)-1;
	*max= MAX2(0, *max);
}

static int rna_BoidSettings_active_boid_state_index_get(PointerRNA *ptr)
{
	BoidSettings *boids= (BoidSettings*)ptr->data;
	BoidState *state = (BoidState*)boids->states.first;
	int i=0;

	for(; state; state=state->next, i++) {
		if(state->flag & BOIDSTATE_CURRENT)
			return i;
	}
	return 0;
}

static void rna_BoidSettings_active_boid_state_index_set(struct PointerRNA *ptr, int value)
{
	BoidSettings *boids= (BoidSettings*)ptr->data;
	BoidState *state = (BoidState*)boids->states.first;
	int i=0;

	for(; state; state=state->next, i++) {
		if(i==value)
			state->flag |= BOIDSTATE_CURRENT;
		else
			state->flag &= ~BOIDSTATE_CURRENT;
	}
}

#else

static void rna_def_boidrule_goal(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "BoidRuleGoal", "BoidRule");
	RNA_def_struct_ui_text(srna, "Goal", "");
	RNA_def_struct_sdna(srna, "BoidRuleGoalAvoid");

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Goal object.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset_deps");

	prop= RNA_def_property(srna, "predict", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "options", BRULE_GOAL_AVOID_PREDICT);
	RNA_def_property_ui_text(prop, "Predict", "Predict target movement.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
}

static void rna_def_boidrule_avoid(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "BoidRuleAvoid", "BoidRule");
	RNA_def_struct_ui_text(srna, "Avoid", "");
	RNA_def_struct_sdna(srna, "BoidRuleGoalAvoid");

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object to avoid.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset_deps");

	prop= RNA_def_property(srna, "predict", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "options", BRULE_GOAL_AVOID_PREDICT);
	RNA_def_property_ui_text(prop, "Predict", "Predict target movement.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "fear_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Fear factor", "Avoid object if danger from it is above this threshol.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
}

static void rna_def_boidrule_avoid_collision(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "BoidRuleAvoidCollision", "BoidRule");
	RNA_def_struct_ui_text(srna, "Avoid Collision", "");

	prop= RNA_def_property(srna, "boids", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "options", BRULE_ACOLL_WITH_BOIDS);
	RNA_def_property_ui_text(prop, "Boids", "Avoid collision with other boids.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "deflectors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "options", BRULE_ACOLL_WITH_DEFLECTORS);
	RNA_def_property_ui_text(prop, "Deflectors", "Avoid collision with deflector objects.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "look_ahead", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Look ahead", "Time to look ahead in seconds.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
}

static void rna_def_boidrule_follow_leader(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "BoidRuleFollowLeader", "BoidRule");
	RNA_def_struct_ui_text(srna, "Follow Leader", "");

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Follow this object instead of a boid.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset_deps");

	prop= RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Distance", "Distance behind leader to follow.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "queue_size", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Queue Size", "How many boids in a line.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "line", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "options", BRULE_LEADER_IN_LINE);
	RNA_def_property_ui_text(prop, "Line", "Follow leader in a line.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
}

static void rna_def_boidrule_average_speed(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "BoidRuleAverageSpeed", "BoidRule");
	RNA_def_struct_ui_text(srna, "Average Speed", "");

	prop= RNA_def_property(srna, "wander", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Wander", "How fast velocity's direction is randomized.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "level", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Level", "How much velocity's z-component is kept constant.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Speed", "Percentage of maximum speed.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
}

static void rna_def_boidrule_fight(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "BoidRuleFight", "BoidRule");
	RNA_def_struct_ui_text(srna, "Fight", "");

	prop= RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Fight Distance", "Attack boids at max this distance.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "flee_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Flee Distance", "Flee to this distance.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
}

static void rna_def_boidrule(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	/* data */
	srna= RNA_def_struct(brna, "BoidRule", NULL);
	RNA_def_struct_ui_text(srna , "Boid Rule", "");
	RNA_def_struct_refine_func(srna, "rna_BoidRule_refine");
	RNA_def_struct_path_func(srna, "rna_BoidRule_path");
	
	/* strings */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Boid rule name.");
	RNA_def_struct_name_property(srna, prop);
	
	/* enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, boidrule_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	
	/* flags */
	prop= RNA_def_property(srna, "in_air", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BOIDRULE_IN_AIR);
	RNA_def_property_ui_text(prop, "In Air", "Use rule when boid is flying.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
	
	prop= RNA_def_property(srna, "on_land", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BOIDRULE_ON_LAND);
	RNA_def_property_ui_text(prop, "On Land", "Use rule when boid is on land.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
	
	//prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	//RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Expanded);
	//RNA_def_property_ui_text(prop, "Expanded", "Set modifier expanded in the user interface.");

	/* types */
	rna_def_boidrule_goal(brna);
	rna_def_boidrule_avoid(brna);
	rna_def_boidrule_avoid_collision(brna);
	rna_def_boidrule_follow_leader(brna);
	rna_def_boidrule_average_speed(brna);
	rna_def_boidrule_fight(brna);
}

static void rna_def_boidstate(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BoidState", NULL);
	RNA_def_struct_ui_text(srna, "Boid State", "Boid state for boid physics.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Boid state name.");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "ruleset_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, boidruleset_type_items);
	RNA_def_property_ui_text(prop, "Rule Evaluation", "How the rules in the list are evaluated.");

	prop= RNA_def_property(srna, "rules", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "BoidRule");
	RNA_def_property_ui_text(prop, "Boid Rules", "");

	prop= RNA_def_property(srna, "active_boid_rule", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BoidRule");
	RNA_def_property_pointer_funcs(prop, "rna_BoidState_active_boid_rule_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Boid Rule", "");

	prop= RNA_def_property(srna, "active_boid_rule_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_BoidState_active_boid_rule_index_get", "rna_BoidState_active_boid_rule_index_set", "rna_BoidState_active_boid_rule_index_range");
	RNA_def_property_ui_text(prop, "Active Boid Rule Index", "");

	prop= RNA_def_property(srna, "rule_fuzziness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Rule Fuzzines", "");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "volume", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Volume", "");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Falloff", "");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
}
static void rna_def_boid_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BoidSettings", NULL);
	RNA_def_struct_ui_text(srna, "Boid Settings", "Settings for boid physics.");

	prop= RNA_def_property(srna, "landing_smoothness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Landing Smoothness", "How smoothly the boids land.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "banking", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_text(prop, "Banking", "Amount of rotation around velocity vector on turns.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_text(prop, "Height", "Boid height relative to particle size.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	/* states */
	prop= RNA_def_property(srna, "states", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "BoidState");
	RNA_def_property_ui_text(prop, "Boid States", "");

	prop= RNA_def_property(srna, "active_boid_state", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BoidRule");
	RNA_def_property_pointer_funcs(prop, "rna_BoidSettings_active_boid_state_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Boid Rule", "");

	prop= RNA_def_property(srna, "active_boid_state_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_BoidSettings_active_boid_state_index_get", "rna_BoidSettings_active_boid_state_index_set", "rna_BoidSettings_active_boid_state_index_range");
	RNA_def_property_ui_text(prop, "Active Boid State Index", "");

	/* character properties */
	prop= RNA_def_property(srna, "health", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Health", "Initial boid health when born.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Strength", "Maximum caused damage on attack per second.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "aggression", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Aggression", "Boid will fight this times stronger enemy.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "accuracy", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Accuracy", "Accuracy of attack.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "range", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Range", "The maximum distance from which a boid can attack.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	/* physical properties */
	prop= RNA_def_property(srna, "air_min_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Min Air Speed", "Minimum speed in air (relative to maximum speed).");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "air_max_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Max Air Speed", "Maximum speed in air.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "air_max_acc", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Max Air Acceleration", "Maximum acceleration in air (relative to maximum speed).");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "air_max_ave", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Max Air Angular Velocity", "Maximum angular velocity in air (relative to 180 degrees).");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "air_personal_space", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Air Personal Space", "Radius of boids personal space in air (% of particle size).");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "land_jump_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Jump Speed", "Maximum speed for jumping.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "land_max_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Max Land Speed", "Maximum speed on land.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "land_max_acc", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Max Land Acceleration", "Maximum acceleration on land (relative to maximum speed).");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "land_max_ave", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Max Land Angular Velocity", "Maximum angular velocity on land (relative to 180 degrees).");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "land_personal_space", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Land Personal Space", "Radius of boids personal space on land (% of particle size).");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "land_stick_force", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Land Stick Force", "How strong a force must be to start effecting a boid on land.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	/* options */
	prop= RNA_def_property(srna, "allow_flight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "options", BOID_ALLOW_FLIGHT);
	RNA_def_property_ui_text(prop, "Allow Flight", "Allow boids to move in air.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "allow_land", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "options", BOID_ALLOW_LAND);
	RNA_def_property_ui_text(prop, "Allow Land", "Allow boids to move on land.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");

	prop= RNA_def_property(srna, "allow_climb", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "options", BOID_ALLOW_CLIMB);
	RNA_def_property_ui_text(prop, "Allow Climbing", "Allow boids to climb goal objects.");
	RNA_def_property_update(prop, 0, "rna_Boids_reset");
}

void RNA_def_boid(BlenderRNA *brna)
{
	rna_def_boidrule(brna);
	rna_def_boidstate(brna);
	rna_def_boid_settings(brna);
}

#endif
