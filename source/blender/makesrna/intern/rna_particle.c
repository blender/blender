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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>

#include "limits.h"

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_particle_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "WM_types.h"
#include "WM_api.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_particle.h"

#include "BLI_arithb.h"

/* property update functions */
static void rna_Particle_redo(bContext *C, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	ParticleSettings *part;
	if(ptr->type==&RNA_ParticleSystem) {
		ParticleSystem *psys = (ParticleSystem*)ptr->data;
		Object *ob = psys_find_object(scene, psys);
		
		psys->recalc = PSYS_RECALC_REDO;

		if(ob)
			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	}
	else {
		part = ptr->id.data;
		psys_flush_particle_settings(scene, part, PSYS_RECALC_REDO);
	}
}

static void rna_Particle_reset(bContext *C, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	ParticleSettings *part;

	if(ptr->type==&RNA_ParticleSystem) {
		ParticleSystem *psys = (ParticleSystem*)ptr->data;
		Object *ob = psys_find_object(scene, psys);
		
		psys->recalc = PSYS_RECALC_RESET;

		if(ob) {
			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
			//WM_event_add_notifier(C, NC_SCENE|ND_CACHE_PHYSICS, scene);
		}
	}
	else {
		part = ptr->id.data;
		psys_flush_particle_settings(scene, part, PSYS_RECALC_RESET);
		//WM_event_add_notifier(C, NC_SCENE|ND_CACHE_PHYSICS, scene);
	}
}

static void rna_Particle_change_type(bContext *C, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	ParticleSettings *part;

	if(ptr->type==&RNA_ParticleSystem) {
		ParticleSystem *psys = (ParticleSystem*)ptr->data;
		Object *ob = psys_find_object(scene, psys);
		
		psys->recalc = PSYS_RECALC_RESET|PSYS_RECALC_TYPE;

		if(ob) {
			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
			//WM_event_add_notifier(C, NC_SCENE|ND_CACHE_PHYSICS, scene);
		}
	}
	else {
		part = ptr->id.data;
		psys_flush_particle_settings(scene, part, PSYS_RECALC_RESET|PSYS_RECALC_TYPE);
		//WM_event_add_notifier(C, NC_SCENE|ND_CACHE_PHYSICS, scene);
	}
}

static void rna_Particle_redo_child(bContext *C, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	ParticleSettings *part;

	if(ptr->type==&RNA_ParticleSystem) {
		ParticleSystem *psys = (ParticleSystem*)ptr->data;
		Object *ob = psys_find_object(scene, psys);
		
		psys->recalc = PSYS_RECALC_CHILD;

		if(ob)
			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	}
	else {
		part = ptr->id.data;

		psys_flush_particle_settings(scene, part, PSYS_RECALC_CHILD);
	}
}
static void rna_PartSettings_start_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;

	/* check for clipping */
	if(value > settings->end)
		value = settings->end;

	if(settings->type==PART_REACTOR && value < 1.0)
		value = 1.0;
	else if (value < -30000.0f) //TODO: replace 30000 with MAXFRAMEF when available in 2.5
		value = -30000.0f;

	settings->sta = value;
}

static void rna_PartSettings_end_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;

	/* check for clipping */
	if(value < settings->sta)
		value = settings->sta;

	settings->end = value;
}

static void rna_PartSetting_linelentail_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	settings->draw_line[0] = value;
}

static float rna_PartSetting_linelentail_get(struct PointerRNA *ptr)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	return settings->draw_line[0];
}

static void rna_PartSetting_linelenhead_set(struct PointerRNA *ptr, float value)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	settings->draw_line[1] = value;
}

static float rna_PartSetting_linelenhead_get(struct PointerRNA *ptr)
{
	ParticleSettings *settings = (ParticleSettings*)ptr->data;
	return settings->draw_line[1];
}

static int rna_ParticleSystem_name_length(PointerRNA *ptr)
{
	ParticleSystem *psys= ptr->data;

	if(psys->part)
		return strlen(psys->part->id.name+2);
	
	return 0;
}

static void rna_ParticleSystem_name_get(PointerRNA *ptr, char *str)
{
	ParticleSystem *psys= ptr->data;

	if(psys->part)
		strcpy(str, psys->part->id.name+2);
	else
		strcpy(str, "");
}

static EnumPropertyItem from_items[] = {
	{PART_FROM_VERT, "VERT", 0, "Vertexes", ""},
	{PART_FROM_FACE, "FACE", 0, "Faces", ""},
	{PART_FROM_VOLUME, "VOLUME", 0, "Volume", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem reactor_from_items[] = {
	{PART_FROM_VERT, "VERT", 0, "Vertexes", ""},
	{PART_FROM_FACE, "FACE", 0, "Faces", ""},
	{PART_FROM_VOLUME, "VOLUME", 0, "Volume", ""},
	{PART_FROM_PARTICLE, "PARTICLE", 0, "Particle", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem *rna_Particle_from_itemf(PointerRNA *ptr)
{
	ParticleSettings *part = ptr->id.data;

	if(part->type==PART_REACTOR)
		return reactor_from_items;
	else
		return from_items;
}

static EnumPropertyItem draw_as_items[] = {
	{PART_DRAW_NOT, "NONE", 0, "None", ""},
	{PART_DRAW_REND, "RENDER", 0, "Rendered", ""},
	{PART_DRAW_DOT, "DOT", 0, "Point", ""},
	{PART_DRAW_CIRC, "CIRC", 0, "Circle", ""},
	{PART_DRAW_CROSS, "CROSS", 0, "Cross", ""},
	{PART_DRAW_AXIS, "AXIS", 0, "Axis", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem hair_draw_as_items[] = {
	{PART_DRAW_NOT, "NONE", 0, "None", ""},
	{PART_DRAW_REND, "RENDER", 0, "Rendered", ""},
	{PART_DRAW_PATH, "PATH", 0, "Path", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem ren_as_items[] = {
	{PART_DRAW_NOT, "NONE", 0, "None", ""},
	{PART_DRAW_HALO, "HALO", 0, "Halo", ""},
	{PART_DRAW_LINE, "LINE", 0, "Line", ""},
	{PART_DRAW_PATH, "PATH", 0, "Path", ""},
	{PART_DRAW_OB, "OBJECT", 0, "Object", ""},
	{PART_DRAW_GR, "GROUP", 0, "Group", ""},
	{PART_DRAW_BB, "BILLBOARD", 0, "Billboard", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem hair_ren_as_items[] = {
	{PART_DRAW_NOT, "NONE", 0, "None", ""},
	{PART_DRAW_PATH, "PATH", 0, "Path", ""},
	{PART_DRAW_OB, "OBJECT", 0, "Object", ""},
	{PART_DRAW_GR, "GROUP", 0, "Group", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem *rna_Particle_draw_as_itemf(PointerRNA *ptr)
{
	ParticleSettings *part = ptr->id.data;

	if(part->type==PART_HAIR)
		return hair_draw_as_items;
	else
		return draw_as_items;
}

static EnumPropertyItem *rna_Particle_ren_as_itemf(PointerRNA *ptr)
{
	ParticleSettings *part = ptr->id.data;

	if(part->type==PART_HAIR)
		return hair_ren_as_items;
	else
		return ren_as_items;
}


#else

static void rna_def_particle_hair_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ParticleHairKey", NULL);
	RNA_def_struct_sdna(srna, "HairKey");
	RNA_def_struct_ui_text(srna, "Particle Hair Key", "Particle key for hair particle system.");

	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_ui_text(prop, "Location", "Key location.");

	prop= RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Time", "Relative time of key over hair length.");

	prop= RNA_def_property(srna, "weight", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Weight", "Weight for softbody simulation.");
}

static void rna_def_particle_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ParticleKey", NULL);
	RNA_def_struct_ui_text(srna, "Particle Key", "Key location for a particle over time.");

	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_ui_text(prop, "Location", "Key location.");

	prop= RNA_def_property(srna, "velocity", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "vel");
	RNA_def_property_ui_text(prop, "Velocity", "Key velocity");

	prop= RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ROTATION);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Rotation", "Key rotation quaterion.");

	prop= RNA_def_property(srna, "angular_velocity", PROP_FLOAT, PROP_ROTATION);
	RNA_def_property_float_sdna(prop, NULL, "ave");
	RNA_def_property_ui_text(prop, "Angular Velocity", "Key angular velocity.");

	prop= RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Time", "Time of key over the simulation.");
}

static void rna_def_child_particle(BlenderRNA *brna)
{
	StructRNA *srna;
	//PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ChildParticle", NULL);
	RNA_def_struct_ui_text(srna, "Child Particle", "Child particle interpolated from simulated or edited particles.");

//	int num, parent;	/* num is face index on the final derived mesh */

//	int pa[4];			/* nearest particles to the child, used for the interpolation */
//	float w[4];			/* interpolation weights for the above particles */
//	float fuv[4], foffset; /* face vertex weights and offset */
//	float rand[3];
}

static void rna_def_particle(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem alive_items[] = {
		{PARS_KILLED, "KILLED", 0, "Killed", ""},
		{PARS_DEAD, "DEAD", 0, "Dead", ""},
		{PARS_UNBORN, "UNBORN", 0, "Unborn", ""},
		{PARS_ALIVE, "ALIVE", 0, "Alive", ""},
		{PARS_DYING, "DYING", 0, "Dying", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Particle", NULL);
	RNA_def_struct_sdna(srna, "ParticleData");
	RNA_def_struct_ui_text(srna, "Particle", "Particle in a particle system.");

	prop= RNA_def_property(srna, "stick_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "stick_ob");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Stick Object", "Object that particle sticks to when dead");

//	ParticleKey state;		/* normally current global coordinates or	*/
//							/* in sticky object space if dead & sticky	*/
//
//	ParticleKey prev_state; /* previous state */

//	prop= RNA_def_property(srna, "hair", PROP_COLLECTION, PROP_NONE);
//	RNA_def_property_collection_sdna(prop, NULL, "hair", "???totalHair???"); //don't know what the hair array size is
//	RNA_def_property_struct_type(prop, "HairKey");
//	RNA_def_property_ui_text(prop, "Hair", "");

	prop= RNA_def_property(srna, "keys", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "keys", "totkey");
	RNA_def_property_struct_type(prop, "ParticleKey");
	RNA_def_property_ui_text(prop, "Keyed States", "");

	prop= RNA_def_property(srna, "random_rotation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "r_rot");
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Random Rotation", "");

	prop= RNA_def_property(srna, "random_a_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "r_ave");
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Random erm.. Velocity", "");//TODO: fix name

	prop= RNA_def_property(srna, "random_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "r_ve");//optional if prop names are the same
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Random Velocity", "");

//
//	float fuv[4], foffset;	/* coordinates on face/edge number "num" and depth along*/
//							/* face normal for volume emission						*/

	prop= RNA_def_property(srna, "time", PROP_FLOAT, PROP_NONE);
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Time", "");

	prop= RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_NONE);
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Lifetime", "");

	prop= RNA_def_property(srna, "die_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dietime");
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Die Time", "");

	prop= RNA_def_property(srna, "banking_angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bank");
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Banking Angle", "");

	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Size", "");

	prop= RNA_def_property(srna, "size_multiplier", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sizemul");
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Size Multiplier", "");

//
//	int num;				/* index to vert/edge/face */
//	int num_dmcache;		/* index to derived mesh data (face) to avoid slow lookups */
//	int pad;
//
//	int totkey;
//	int bpi;				/* softbody body point start index */

	/* flag */
	prop= RNA_def_property(srna, "unexist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PARS_UNEXIST);
	RNA_def_property_ui_text(prop, "unexist", "");

	prop= RNA_def_property(srna, "no_disp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PARS_NO_DISP);
	RNA_def_property_ui_text(prop, "no_disp", "");

	prop= RNA_def_property(srna, "sticky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PARS_STICKY);
	RNA_def_property_ui_text(prop, "sticky", "");

	prop= RNA_def_property(srna, "transform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PARS_TRANSFORM);
	RNA_def_property_ui_text(prop, "transform", "");

	prop= RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PARS_HIDE);
	RNA_def_property_ui_text(prop, "hide", "");

	prop= RNA_def_property(srna, "tag", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PARS_TAG);
	RNA_def_property_ui_text(prop, "tag", "");

	prop= RNA_def_property(srna, "rekey", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PARS_REKEY);
	RNA_def_property_ui_text(prop, "rekey", "");

	prop= RNA_def_property(srna, "edit_recalc", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PARS_EDIT_RECALC);
	RNA_def_property_ui_text(prop, "edit_recalc", "");


	prop= RNA_def_property(srna, "alive_state", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "alive");
	RNA_def_property_enum_items(prop, alive_items);
	RNA_def_property_ui_text(prop, "Alive State", "");

	prop= RNA_def_property(srna, "loop", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	//TODO: bounds
	RNA_def_property_ui_text(prop, "Loop", "How may times the particle life has looped");

//	short rt2;
}

static void rna_def_particle_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem type_items[] = {
		{PART_EMITTER, "EMITTER", 0, "Emitter", ""},
		{PART_REACTOR, "REACTOR", 0, "Reactor", ""},
		{PART_HAIR, "HAIR", 0, "Hair", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem dist_items[] = {
		{PART_DISTR_JIT, "JIT", 0, "Jittered", ""},
		{PART_DISTR_RAND, "RAND", 0, "Random", ""},
		{PART_DISTR_GRID, "GRID", 0, "Grid", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem phys_type_items[] = {
		{PART_PHYS_NO, "NO", 0, "No", ""},
		{PART_PHYS_NEWTON, "NEWTON", 0, "Newtonian", ""},
		{PART_PHYS_KEYED, "KEYED", 0, "Keyed", ""},
		{PART_PHYS_BOIDS, "BOIDS", 0, "Boids", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem rot_mode_items[] = {
		{0, "NONE", 0, "None", ""},
		{PART_ROT_NOR, "NOR", 0, "Normal", ""},
		{PART_ROT_VEL, "VEL", 0, "Velocity", ""},
		{PART_ROT_GLOB_X, "GLOB_X", 0, "Global X", ""},
		{PART_ROT_GLOB_Y, "GLOB_Y", 0, "Global Y", ""},
		{PART_ROT_GLOB_Z, "GLOB_Z", 0, "Global Z", ""},
		{PART_ROT_OB_X, "OB_X", 0, "Object X", ""},
		{PART_ROT_OB_Y, "OB_Y", 0, "Object Y", ""},
		{PART_ROT_OB_Z, "OB_Z", 0, "Object Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem ave_mode_items[] = {
		{0, "NONE", 0, "None", ""},
		{PART_AVE_SPIN, "SPIN", 0, "Spin", ""},
		{PART_AVE_RAND, "RAND", 0, "Random", ""} ,
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem react_event_items[] = {
		{PART_EVENT_DEATH, "DEATH", 0, "Death", ""},
		{PART_EVENT_COLLIDE, "COLLIDE", 0, "Collision", ""},
		{PART_EVENT_NEAR, "NEAR", 0, "Near", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem child_type_items[] = {
		{0, "NONE", 0, "None", ""},
		{PART_CHILD_PARTICLES, "PARTICLES", 0, "Particles", ""},
		{PART_CHILD_FACES, "FACES", 0, "Faces", ""},
		{0, NULL, 0, NULL, NULL}
	};

	//TODO: names, tooltips
	static EnumPropertyItem rot_from_items[] = {
		{PART_ROT_KEYS, "KEYS", 0, "keys", ""},
		{PART_ROT_ZINCR, "ZINCR", 0, "zincr", ""},
		{PART_ROT_IINCR, "IINCR", 0, "iincr", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem integrator_type_items[] = {
		{PART_INT_EULER, "EULER", 0, "Euler", ""},
		{PART_INT_MIDPOINT, "MIDPOINT", 0, "Midpoint", ""},
		{PART_INT_RK4, "RK4", 0, "RK4", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem kink_type_items[] = {
		{PART_KINK_NO, "NO", 0, "Nothing", ""},
		{PART_KINK_CURL, "CURL", 0, "Curl", ""},
		{PART_KINK_RADIAL, "RADIAL", 0, "Radial", ""},
		{PART_KINK_WAVE, "WAVE", 0, "Wave", ""},
		{PART_KINK_BRAID, "BRAID", 0, "Braid", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem kink_axis_items[] = {
		{0, "X", 0, "X", ""},
		{1, "Y", 0, "Y", ""},
		{2, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bb_align_items[] = {
		{PART_BB_X, "X", 0, "X", ""},
		{PART_BB_Y, "Y", 0, "Y", ""},
		{PART_BB_Z, "Z", 0, "Z", ""},
		{PART_BB_VIEW, "VIEW", 0, "View", ""},
		{PART_BB_VEL, "VEL", 0, "Velocity", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bb_anim_items[] = {
		{PART_BB_ANIM_NONE, "NONE", 0, "None", ""},
		{PART_BB_ANIM_TIME, "TIME", 0, "Time", ""},
		{PART_BB_ANIM_ANGLE, "ANGLE", 0, "Angle", ""},
		//{PART_BB_ANIM_OFF_TIME, "OFF_TIME", 0, "off_time", ""},
		//{PART_BB_ANIM_OFF_ANGLE, "OFF_ANGLE", 0, "off_angle", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bb_split_offset_items[] = {
		{PART_BB_OFF_NONE, "NONE", 0, "None", ""},
		{PART_BB_OFF_LINEAR, "LINEAR", 0, "Linear", ""},
		{PART_BB_OFF_RANDOM, "RANDOM", 0, "Random", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna= RNA_def_struct(brna, "ParticleSettings", "ID");
	RNA_def_struct_ui_text(srna, "Particle Settings", "Particle settings, reusable by multiple particle systems.");
	RNA_def_struct_ui_icon(srna, ICON_PARTICLE_DATA);

	/* flag */
	prop= RNA_def_property(srna, "react_start_end", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_REACT_STA_END);
	RNA_def_property_ui_text(prop, "Start/End", "Give birth to unreacted particles eventually.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "react_multiple", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_REACT_MULTIPLE);
	RNA_def_property_ui_text(prop, "Multi React", "React multiple times.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "loop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_LOOP);
	RNA_def_property_ui_text(prop, "Loop", "Loop particle lives.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* TODO: used somewhere? */
	prop= RNA_def_property(srna, "hair_geometry", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_HAIR_GEOMETRY);
	RNA_def_property_ui_text(prop, "Hair Geometry", "");//TODO: tooltip

	prop= RNA_def_property(srna, "unborn", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_UNBORN);
	RNA_def_property_ui_text(prop, "Unborn", "Show particles before they are emitted.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "died", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_DIED);
	RNA_def_property_ui_text(prop, "Died", "Show particles after they have died");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "trand", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_TRAND);
	RNA_def_property_ui_text(prop, "Random", "Emit in random order of elements");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "even_distribution", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_EDISTR);
	RNA_def_property_ui_text(prop, "Even Distribution", "Use even distribution from faces based on face areas or edge lengths.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "sticky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_STICKY);
	RNA_def_property_ui_text(prop, "Sticky", "Particles stick to collided objects if they die in the collision.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "die_on_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_DIE_ON_COL);
	RNA_def_property_ui_text(prop, "Die on hit", "Particles die when they collide with a deflector object.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "size_deflect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SIZE_DEFL);
	RNA_def_property_ui_text(prop, "Size Deflect", "Use particle's size in deflection.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "rotation_dynamic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ROT_DYN);
	RNA_def_property_ui_text(prop, "Dynamic", "Sets rotation to dynamic/constant");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "sizemass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SIZEMASS);
	RNA_def_property_ui_text(prop, "Mass from Size", "Multiply mass with particle size.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "abs_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ABS_LENGTH);
	RNA_def_property_ui_text(prop, "Abs Length", "Use maximum length for children");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "absolute_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ABS_TIME);
	RNA_def_property_ui_text(prop, "Absolute Time", "Set all ipos that work on particles to be calculated in absolute/relative time.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "global_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_GLOB_TIME);
	RNA_def_property_ui_text(prop, "Global Time", "Set all ipos that work on particles to be calculated in global/object time.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "boids_2d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_BOIDS_2D);
	RNA_def_property_ui_text(prop, "Boids 2D", "Constrain boids to a surface");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "branching", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_BRANCHING);
	RNA_def_property_ui_text(prop, "Branching", "Branch child paths from each other.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "animate_branching", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ANIM_BRANCHING);
	RNA_def_property_ui_text(prop, "Animated", "Animate branching");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "symmetric_branching", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SYMM_BRANCHING);
	RNA_def_property_ui_text(prop, "Symmetric", "Start and end points are the same.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "hair_bspline", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_HAIR_BSPLINE);
	RNA_def_property_ui_text(prop, "B-Spline", "Interpolate hair using B-Splines.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "grid_invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_GRID_INVERT);
	RNA_def_property_ui_text(prop, "Invert", "Invert what is considered object and what is not.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "child_effector", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_EFFECT);
	RNA_def_property_ui_text(prop, "Children", "Apply effectors to children.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "child_seams", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_SEAMS);
	RNA_def_property_ui_text(prop, "Use seams", "Use seams to determine parents");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	/* TODO: used somewhere? */
	prop= RNA_def_property(srna, "child_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_RENDER);
	RNA_def_property_ui_text(prop, "child_render", "");

	prop= RNA_def_property(srna, "child_guide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_GUIDE);
	RNA_def_property_ui_text(prop, "child_guide", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "self_effect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SELF_EFFECT);
	RNA_def_property_ui_text(prop, "Self Effect", "Particle effectors effect themselves.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");


	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_change_type");

	prop= RNA_def_property(srna, "emit_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "from");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Particle_from_itemf");
	RNA_def_property_ui_text(prop, "Emit From", "Where to emit particles from");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "distr");
	RNA_def_property_enum_items(prop, dist_items);
	RNA_def_property_ui_text(prop, "Distribution", "How to distribute particles on selected element");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* physics modes */
	prop= RNA_def_property(srna, "physics_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "phystype");
	RNA_def_property_enum_items(prop, phys_type_items);
	RNA_def_property_ui_text(prop, "Physics Type", "Particle physics type");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rotmode");
	RNA_def_property_enum_items(prop, rot_mode_items);
	RNA_def_property_ui_text(prop, "Rotation", "Particles initial rotation");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "angular_velocity_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "avemode");
	RNA_def_property_enum_items(prop, ave_mode_items);
	RNA_def_property_ui_text(prop, "Angular Velocity Mode", "Particle angular velocity mode.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "react_event", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "reactevent");
	RNA_def_property_enum_items(prop, react_event_items);
	RNA_def_property_ui_text(prop, "React On", "The event of target particles to react on.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/*draw flag*/
	prop= RNA_def_property(srna, "velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_VEL);
	RNA_def_property_ui_text(prop, "Velocity", "Show particle velocity");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	//prop= RNA_def_property(srna, "draw_path_length", PROP_BOOLEAN, PROP_NONE);
	//RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_PATH_LEN);
	//RNA_def_property_ui_text(prop, "Path length", "Draw path length");
	//RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "show_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_SIZE);
	RNA_def_property_ui_text(prop, "Size", "Show particle size");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "emitter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_EMITTER);
	RNA_def_property_ui_text(prop, "Emitter", "Render emitter Object also.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	//prop= RNA_def_property(srna, "draw_health", PROP_BOOLEAN, PROP_NONE);
	//RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_HEALTH);
	//RNA_def_property_ui_text(prop, "Health", "Draw boid health");
	//RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	//prop= RNA_def_property(srna, "timed_path", PROP_BOOLEAN, PROP_NONE);
	//RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_TIMED_PATH);
	//RNA_def_property_ui_text(prop, "Clip with time", "Clip path based on time");
	//RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	//prop= RNA_def_property(srna, "draw_cached_path", PROP_BOOLEAN, PROP_NONE);
	//RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_CACHED_PATH);
	//RNA_def_property_ui_text(prop, "Path", "Draw particle path if the path is baked");
	//RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "billboard_lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_BB_LOCK);
	RNA_def_property_ui_text(prop, "Lock Billboard", "Lock the billboards align axis");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "parent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_PARENT);
	RNA_def_property_ui_text(prop, "Parents", "Render parent particles.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "num", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_NUM);
	RNA_def_property_ui_text(prop, "Number", "Show particle number");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "rand_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_RAND_GR);
	RNA_def_property_ui_text(prop, "Pick Random", "Pick objects from group randomly");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "render_adaptive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_REN_ADAPT);
	RNA_def_property_ui_text(prop, "Adaptive render", "Draw steps of the particle path");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "velocity_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_VEL_LENGTH);
	RNA_def_property_ui_text(prop, "Speed", "Multiply line length by particle speed");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "material_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_MAT_COL);
	RNA_def_property_ui_text(prop, "Material Color", "Draw particles using material's diffuse color.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "whole_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_WHOLE_GR);
	RNA_def_property_ui_text(prop, "Whole Group", "Use whole group at once.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "render_strand", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_REN_STRAND);
	RNA_def_property_ui_text(prop, "Strand render", "Use the strand primitive for rendering");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "draw_as", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "draw_as");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Particle_draw_as_itemf");
	RNA_def_property_ui_text(prop, "Particle Drawing", "How particles are drawn in viewport");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "ren_as", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ren_as");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Particle_ren_as_itemf");
	RNA_def_property_ui_text(prop, "Particle Rendering", "How particles are rendered");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "draw_size", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Draw Size", "Size of particles on viewport in pixels (0=default)");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "child_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "childtype");
	RNA_def_property_enum_items(prop, child_type_items);
	RNA_def_property_ui_text(prop, "Children From", "Create child particles");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "draw_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 7);
	RNA_def_property_ui_text(prop, "Steps", "How many steps paths are drawn with (power of 2)");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "render_step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ren_step");
	RNA_def_property_range(prop, 0, 9);
	RNA_def_property_ui_text(prop, "Render", "How many steps paths are rendered with (power of 2)");

	prop= RNA_def_property(srna, "hair_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 2, 50);
	RNA_def_property_ui_text(prop, "Segments", "Number of hair segments");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");


	//TODO: not found in UI, readonly?
	prop= RNA_def_property(srna, "keys_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, INT_MAX);//TODO:min,max
	RNA_def_property_ui_text(prop, "Keys Step", "");

	/* adaptive path rendering */
	prop= RNA_def_property(srna, "adaptive_angle", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "adapt_angle");
	RNA_def_property_range(prop, 0, 45);
	RNA_def_property_ui_text(prop, "Degrees", "How many degrees path has to curve to make another render segment");

	prop= RNA_def_property(srna, "adaptive_pix", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "adapt_pix");
	RNA_def_property_range(prop, 0, 50);
	RNA_def_property_ui_text(prop, "Pixel", "How many pixels path has to cover to make another render segment");

	prop= RNA_def_property(srna, "display", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "disp");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Display", "Percentage of particles to display in 3d view");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "material", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "omat");
	RNA_def_property_range(prop, 1, 16);
	RNA_def_property_ui_text(prop, "Material", "Specify material used for the particles");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");


	//interpolation
	//TODO: can't find where interpolation is used

	//TODO: is this read only/internal?
	prop= RNA_def_property(srna, "rotate_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rotfrom");
	RNA_def_property_enum_items(prop, rot_from_items);
	RNA_def_property_ui_text(prop, "Rotate From", "");

	prop= RNA_def_property(srna, "integrator", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, integrator_type_items);
	RNA_def_property_ui_text(prop, "Integration", "Select physics integrator type");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "kink", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, kink_type_items);
	RNA_def_property_ui_text(prop, "Kink", "Type of periodic offset on the path");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "kink_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, kink_axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Which axis to use for offset");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	/* used?
	prop= RNA_def_property(srna, "inbetween", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "nbetween");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_text(prop, "Inbetween", "");
	*/

	prop= RNA_def_property(srna, "boid_neighbours", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "boidneighbours");
	RNA_def_property_range(prop, 1, 10);
	RNA_def_property_ui_text(prop, "Neighbours", "How many neighbours to consider for each boid");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* billboards */
	prop= RNA_def_property(srna, "billboard_align", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bb_align");
	RNA_def_property_enum_items(prop, bb_align_items);
	RNA_def_property_ui_text(prop, "Align to", "In respect to what the billboards are aligned");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "billboard_uv_split", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "bb_uv_split");
	RNA_def_property_range(prop, 1, 10);
	RNA_def_property_ui_text(prop, "UV Split", "Amount of rows/columns to split uv coordinates for billboards");

	prop= RNA_def_property(srna, "billboard_animation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bb_anim");
	RNA_def_property_enum_items(prop, bb_anim_items);
	RNA_def_property_ui_text(prop, "Animate", "How to animate billboard textures.");

	prop= RNA_def_property(srna, "billboard_split_offset", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bb_split_offset");
	RNA_def_property_enum_items(prop, bb_split_offset_items);
	RNA_def_property_ui_text(prop, "Offset", "How to offset billboard textures");

	prop= RNA_def_property(srna, "billboard_tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bb_tilt");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Tilt", "Tilt of the billboards");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "billboard_random_tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bb_rand_tilt");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Tilt", "Random tilt of the billboards");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "billboard_offset", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "bb_offset");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Billboard Offset", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	/* simplification */
	prop= RNA_def_property(srna, "enable_simplify", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "simplify_flag", PART_SIMPLIFY_ENABLE);
	RNA_def_property_ui_text(prop, "Child Simplification", "Remove child strands as the object becomes smaller on the screen.");

	prop= RNA_def_property(srna, "viewport", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "simplify_flag", PART_SIMPLIFY_VIEWPORT);
	RNA_def_property_ui_text(prop, "Viewport", "");

	prop= RNA_def_property(srna, "simplify_refsize", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "simplify_refsize");
	RNA_def_property_range(prop, 1, 32768);
	RNA_def_property_ui_text(prop, "Reference Size", "Reference size size in pixels, after which simplification begins.");

	prop= RNA_def_property(srna, "simplify_rate", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Rate", "Speed of simplification");

	prop= RNA_def_property(srna, "simplify_transition", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Transition", "Transition period for fading out strands.");

	prop= RNA_def_property(srna, "simplify_viewport", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 0.999f);
	RNA_def_property_ui_text(prop, "Rate", "Speed of Simplification");

	/* general values */
	prop= RNA_def_property(srna, "start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sta");//optional if prop names are the same
	RNA_def_property_range(prop, -30000.0f, 30000.0f); //TODO: replace 30000 with MAXFRAMEF when available in 2.5
	RNA_def_property_float_funcs(prop, NULL, "rna_PartSettings_start_set", NULL);
	RNA_def_property_ui_text(prop, "Start", "Frame # to start emitting particles.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -30000.0f, 30000.0f); //TODO: replace 30000 with MAXFRAMEF when available in 2.5
	RNA_def_property_float_funcs(prop, NULL, "rna_PartSettings_end_set", NULL);
	RNA_def_property_ui_text(prop, "End", "Frame # to stop emitting particles.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 1.0f, 30000.0f);
	RNA_def_property_ui_text(prop, "Lifetime", "Specify the life span of the particles");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "random_lifetime", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randlife");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random", "Give the particle life a random variation.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "time_tweak", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "timetweak");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Tweak", "A multiplier for physics timestep (1.0 means one frame = 1/25 seconds)");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "jitter_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "jitfac");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Amount", "Amount of jitter applied to the sampling.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "keyed_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Time", "Keyed key time relative to remaining particle life.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "effect_hair", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eff_hair");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Stiffnes", "Hair stiffness for effectors");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	//float rt; TODO:find where rt is used - can't find it in UI

	prop= RNA_def_property(srna, "amount", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "totpart");
	RNA_def_property_range(prop, 0, 100000);
	RNA_def_property_ui_text(prop, "Amount", "Total number of particles.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "userjit", PROP_INT, PROP_UNSIGNED);//TODO: can we get a better name for userjit?
	RNA_def_property_int_sdna(prop, NULL, "userjit");
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_text(prop, "P/F", "Emission locations / face (0 = automatic).");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "grid_resolution", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "grid_res");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Resolution", "The resolution of the particle grid.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* initial velocity factors */
	prop= RNA_def_property(srna, "normal_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "normfac");//optional if prop names are the same
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Normal", "Let the surface normal give the particle a starting speed.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "object_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "obfac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Object", "Let the object give the particle a starting speed");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "random_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randfac");//optional if prop names are the same
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Random", "Give the starting speed a random variation.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "particle_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "partfac");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Particle", "Let the target particle give the particle a starting speed.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "tangent_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tanfac");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Tangent", "Let the surface tangent give the particle a starting speed.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "tangent_phase", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tanphase");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Rot", "Rotate the surface tangent.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "reactor_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "reactfac");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Reactor", "Let the vector away from the target particles location give the particle a starting speed.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "angular_velocity_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "avefac");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Angular Velocity", "Angular velocity amount");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "phase_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "phasefac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Phase", "Initial rotation phase");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "random_rotation_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randrotfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Rotation", "Randomize rotation");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "random_phase_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randphasefac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Phase", "Randomize rotation phase");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* physical properties */
	prop= RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Mass", "Specify the mass of the particles");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "particle_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Size", "The size of the particles");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "random_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randsize");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Size", "Give the particle size a random variation");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "reaction_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "reactshape");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Shape", "Power of reaction strength dependence on distance to target.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");


	/* global physical properties */
	prop= RNA_def_property(srna, "acceleration", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "acc");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Accelaration", "Constant acceleration");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "drag_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dragfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Drag", "Specify the amount of air-drag.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "brownian_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "brownfac");
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Brownian", "Specify the amount of brownian motion");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "damp_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dampfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Damp", "Specify the amount of damping");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* length */
	//TODO: is this readonly?
	prop= RNA_def_property(srna, "length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "length");
//	RNA_def_property_range(prop, 0.0f, upperLimitf);//TODO: limits
	RNA_def_property_ui_text(prop, "Length", "");

	prop= RNA_def_property(srna, "absolute_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "abslength");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Max Length", "Absolute maximum path length for children, in blender units.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "random_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randlength");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Length", "Give path length a random variation.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	/* children */
	prop= RNA_def_property(srna, "child_nbr", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "child_nbr");//optional if prop names are the same
	RNA_def_property_range(prop, 0, MAX_PART_CHILDREN);
	RNA_def_property_ui_text(prop, "Children Per Parent", "Amount of children/parent");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "rendered_child_nbr", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ren_child_nbr");
	RNA_def_property_range(prop, 0, MAX_PART_CHILDREN);
	RNA_def_property_ui_text(prop, "Rendered Children", "Amount of children/parent for rendering.");

	prop= RNA_def_property(srna, "virtual_parents", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "parents");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Virtual Parents", "Relative amount of virtual parents.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "child_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childsize");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Child Size", "A multiplier for the child particle size.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "child_random_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childrandsize");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Child Size", "Random variation to the size of the child particles.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "child_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childrad");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Child Radius", "Radius of children around parent.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "child_roundness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childflat");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Child Roundness", "Roundness of children around parent.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	//TODO: is this readonly?
	prop= RNA_def_property(srna, "child_spread", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childspread");
//	RNA_def_property_range(prop, 0.0f, upperLimitf); TODO: limits
	RNA_def_property_ui_text(prop, "Child Spread", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	/* clumping */
	prop= RNA_def_property(srna, "clump_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clumpfac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Clump", "Amount of clumping");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "clumppow", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clumppow");
	RNA_def_property_range(prop, -0.999f, 0.999f);
	RNA_def_property_ui_text(prop, "Shape", "Shape of clumping");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");


	/* kink */
	prop= RNA_def_property(srna, "kink_amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_amp");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Amplitude", "The amplitude of the offset.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "kink_frequency", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_freq");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Frequency", "The frequency of the offset (1/total length)");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "kink_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -0.999f, 0.999f);
	RNA_def_property_ui_text(prop, "Shape", "Adjust the offset to the beginning/end");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");


	/* rough */
	prop= RNA_def_property(srna, "rough1", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Rough1", "Amount of location dependent rough.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "rough1_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01f, 10.0f);
	RNA_def_property_ui_text(prop, "Size1", "Size of location dependent rough.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "rough2", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Rough2", "Amount of random rough.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "rough2_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2_size");
	RNA_def_property_range(prop, 0.01f, 10.0f);
	RNA_def_property_ui_text(prop, "Size2", "Size of random rough.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "rough2_thres", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2_thres");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Amount of particles left untouched by random rough.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "rough_endpoint", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough_end");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Rough Endpoint", "Amount of end point rough.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "rough_end_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Shape", "Shape of end point rough");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	/* branching */
	prop= RNA_def_property(srna, "branch_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "branch_thres");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Threshold of branching.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	/* drawing stuff */
	prop= RNA_def_property(srna, "line_length_tail", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_PartSetting_linelentail_get", "rna_PartSetting_linelentail_set", NULL);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Back", "Length of the line's tail");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "line_length_head", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_PartSetting_linelenhead_get", "rna_PartSetting_linelenhead_set", NULL);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Head", "Length of the line's head");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	/* boids */
	prop= RNA_def_property(srna, "max_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_vel");
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Maximum Velocity", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "lateral_acceleration_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_lat_acc");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Lateral Acceleration", "Lateral acceleration % of max velocity");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "tangential_acceleration_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_tan_acc");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Tangential acceleration", "Tangential acceleration % of max velocity");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "average_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "average_vel");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Average Velocity", "The usual speed % of max velocity");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "banking", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Banking", "Banking of boids on turns (1.0==natural banking)");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "banking_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_bank");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Maximum Banking", "How much a boid can bank at a single step");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "ground_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "groundz");
	RNA_def_property_range(prop, -100.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Ground Z", "Default Z value");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/*TODO: not sure how to deal with this
	prop= RNA_def_property(srna, "boid_factor", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "boidfac");
	RNA_def_property_ui_text(prop, "Boid Factor", "");

	//char boidrule[8];
	*/

	prop= RNA_def_property(srna, "dupli_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "dup_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dupli Group", "Show Objects in this Group in place of particles");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "effector_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "eff_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Group", "Limit effectors to this Group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "dupli_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "dup_ob");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dupli Object", "Show this Object in place of particles.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "billboard_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "bb_ob");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Billboard Object", "Billboards face this object (default is active camera)");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

#if 0
	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "ipo");
	RNA_def_property_struct_type(prop, "Ipo");
	RNA_def_property_ui_text(prop, "Ipo", "");
#endif

//	struct PartDeflect *pd;
//	struct PartDeflect *pd2;
}

static void rna_def_particle_system(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ParticleSystem", NULL);
	RNA_def_struct_ui_text(srna, "Particle System", "Particle system in an object.");
	RNA_def_struct_ui_icon(srna, ICON_PARTICLE_DATA);

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ParticleSystem_name_get", "rna_ParticleSystem_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Particle system name.");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "settings", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "part");
	RNA_def_property_ui_text(prop, "Settings", "Particle system settings.");

	prop= RNA_def_property(srna, "particles", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "particles", "totpart");
	RNA_def_property_struct_type(prop, "Particle");
	RNA_def_property_ui_text(prop, "Particles", "Particles generated by the particle system.");

	prop= RNA_def_property(srna, "child_particles", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "child", "totchild");
	RNA_def_property_struct_type(prop, "ChildParticle");
	RNA_def_property_ui_text(prop, "Child Particles", "Child particles generated by the particle system.");

	prop= RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Seed", "Offset in the random number table, to get a different randomized result.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* hair */
	prop= RNA_def_property(srna, "softbody", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "soft");
	RNA_def_property_ui_text(prop, "Soft Body", "Soft body settings for hair physics simulation.");

	prop= RNA_def_property(srna, "use_softbody", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "softflag", OB_SB_ENABLE);
	RNA_def_property_ui_text(prop, "Use Soft Body", "Enable use of soft body for hair physics simulation.");

	prop= RNA_def_property(srna, "editable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PSYS_EDITED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* various checks needed */
	RNA_def_property_ui_text(prop, "Editable", "For hair particle systems, finalize the hair to enable editing.");

	/* reactor */
	prop= RNA_def_property(srna, "reactor_target_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target_ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Reactor Target Object", "For reactor systems, the object that has the target particle system (empty if same object).");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "reactor_target_particle_system", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "target_psys");
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_text(prop, "Reactor Target Particle System", "For reactor systems, index of particle system on the target object.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* boids */
	prop= RNA_def_property(srna, "boids_surface_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "keyed_ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Boids Surface Object", "For boids physics systems, constrain boids to this object's surface.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* keyed */
	prop= RNA_def_property(srna, "keyed_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "keyed_ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Keyed Object", "For keyed physics systems, the object that has the target particle system.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "keyed_particle_system", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "keyed_psys");
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_text(prop, "Keyed Particle System", "For keyed physics systems, index of particle system on the keyed object.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "keyed_first", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PSYS_FIRST_KEYED);
	RNA_def_property_ui_text(prop, "Keyed First", "Set the system to be the starting point of keyed particles");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "keyed_timed", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PSYS_KEYED_TIME);
	RNA_def_property_ui_text(prop, "Keyed Timed", "Use intermediate key times for keyed particles (setting for starting point only).");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* billboard */
	prop= RNA_def_property(srna, "billboard_normal_uv", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "bb_uvname[0]");
	RNA_def_property_string_maxlength(prop, 32);
	RNA_def_property_ui_text(prop, "Billboard Normal UV", "UV Layer to control billboard normals.");

	prop= RNA_def_property(srna, "billboard_time_index_uv", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "bb_uvname[1]");
	RNA_def_property_string_maxlength(prop, 32);
	RNA_def_property_ui_text(prop, "Billboard Time Index UV", "UV Layer to control billboard time index (X-Y).");

	prop= RNA_def_property(srna, "billboard_split_uv", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "bb_uvname[2]");
	RNA_def_property_string_maxlength(prop, 32);
	RNA_def_property_ui_text(prop, "Billboard Split UV", "UV Layer to control billboard splitting.");

	/* vertex groups */
	prop= RNA_def_property(srna, "vertex_group_density", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[0]");
	RNA_def_property_ui_text(prop, "Vertex Group Density", "Vertex group to control density.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_density_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_DENSITY));
	RNA_def_property_ui_text(prop, "Vertex Group Density Negate", "Negate the effect of the density vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_velocity", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[1]");
	RNA_def_property_ui_text(prop, "Vertex Group Velocity", "Vertex group to control velocity.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_velocity_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_VEL));
	RNA_def_property_ui_text(prop, "Vertex Group Velocity Negate", "Negate the effect of the velocity vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_length", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[2]");
	RNA_def_property_ui_text(prop, "Vertex Group Length", "Vertex group to control length.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "vertex_group_length_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_LENGTH));
	RNA_def_property_ui_text(prop, "Vertex Group Length Negate", "Negate the effect of the length vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo");

	prop= RNA_def_property(srna, "vertex_group_clump", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[3]");
	RNA_def_property_ui_text(prop, "Vertex Group Clump", "Vertex group to control clump.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_clump_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_CLUMP));
	RNA_def_property_ui_text(prop, "Vertex Group Clump Negate", "Negate the effect of the clump vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_kink", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[4]");
	RNA_def_property_ui_text(prop, "Vertex Group Kink", "Vertex group to control kink.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_kink_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_KINK));
	RNA_def_property_ui_text(prop, "Vertex Group Kink Negate", "Negate the effect of the kink vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_roughness1", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[5]");
	RNA_def_property_ui_text(prop, "Vertex Group Roughness 1", "Vertex group to control roughness 1.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_roughness1_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_ROUGH1));
	RNA_def_property_ui_text(prop, "Vertex Group Roughness 1 Negate", "Negate the effect of the roughness 1 vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_roughness2", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[6]");
	RNA_def_property_ui_text(prop, "Vertex Group Roughness 2", "Vertex group to control roughness 2.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_roughness2_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_ROUGH2));
	RNA_def_property_ui_text(prop, "Vertex Group Roughness 2 Negate", "Negate the effect of the roughness 2 vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_roughness_end", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[7]");
	RNA_def_property_ui_text(prop, "Vertex Group Roughness End", "Vertex group to control roughness end.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_roughness_end_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_ROUGHE));
	RNA_def_property_ui_text(prop, "Vertex Group Roughness End Negate", "Negate the effect of the roughness end vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_redo_child");

	prop= RNA_def_property(srna, "vertex_group_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[8]");
	RNA_def_property_ui_text(prop, "Vertex Group Size", "Vertex group to control size.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_size_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_SIZE));
	RNA_def_property_ui_text(prop, "Vertex Group Size Negate", "Negate the effect of the size vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_tangent", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[9]");
	RNA_def_property_ui_text(prop, "Vertex Group Tangent", "Vertex group to control tangent.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_tangent_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_TAN));
	RNA_def_property_ui_text(prop, "Vertex Group Tangent Negate", "Negate the effect of the tangent vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_rotation", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[10]");
	RNA_def_property_ui_text(prop, "Vertex Group Rotation", "Vertex group to control rotation.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_rotation_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_ROT));
	RNA_def_property_ui_text(prop, "Vertex Group Rotation Negate", "Negate the effect of the rotation vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_field", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vgroup[11]");
	RNA_def_property_ui_text(prop, "Vertex Group Field", "Vertex group to control field.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	prop= RNA_def_property(srna, "vertex_group_field_negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "vg_neg", (1 << PSYS_VG_EFFECTOR));
	RNA_def_property_ui_text(prop, "Vertex Group Field Negate", "Negate the effect of the field vertex group.");
	RNA_def_property_update(prop, NC_OBJECT|ND_PARTICLE, "rna_Particle_reset");

	/* pointcache */
	prop= RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "pointcache");
	RNA_def_property_struct_type(prop, "PointCache");
	RNA_def_property_ui_text(prop, "Point Cache", "");
}

void RNA_def_particle(BlenderRNA *brna)
{
	rna_def_particle_hair_key(brna);
	rna_def_particle_key(brna);
	rna_def_child_particle(brna);
	rna_def_particle(brna);
	rna_def_particle_system(brna);
	rna_def_particle_settings(brna);
}

#endif

