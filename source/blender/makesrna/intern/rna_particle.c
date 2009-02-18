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

#include <stdlib.h>

#include "limits.h"

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_particle_types.h"

#ifdef RNA_RUNTIME

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
#else

static void rna_def_hair_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "HairKey", NULL);
	RNA_def_struct_ui_text(srna, "Hair Key", "DOC_BROKEN");

	prop= RNA_def_property(srna, "hair_vertex_location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_array(prop, 3);
	//TODO:bounds
	RNA_def_property_ui_text(prop, "Hair Vertex Location", "");

	prop= RNA_def_property(srna, "time", PROP_FLOAT, PROP_NONE);
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Time", "Time along hair");

	prop= RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Softbody Weight", "");

//	short editflag;	/* saved particled edit mode flags */

}

static void rna_def_particle_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ParticleKey", NULL);
	RNA_def_struct_ui_text(srna, "Particle Key", "DOC_BROKEN");

	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_array(prop, 3);
	//TODO:bounds
	RNA_def_property_ui_text(prop, "Location", "");

	prop= RNA_def_property(srna, "velocity", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "vel");
	RNA_def_property_array(prop, 3);
	//TODO:bounds
	RNA_def_property_ui_text(prop, "Velocity", "");

	prop= RNA_def_property(srna, "rotation_quaternion", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_array(prop, 4);
	//TODO:bounds
	RNA_def_property_ui_text(prop, "Rotation Quaternion", "");

	prop= RNA_def_property(srna, "angular_velocity", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "ave");
	RNA_def_property_array(prop, 3);
	//TODO:bounds
	RNA_def_property_ui_text(prop, "Angular Velocity", "");

	prop= RNA_def_property(srna, "time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "time");//optional if prop names are the same
//	RNA_def_property_range(prop, lowerLimitf, upperLimitf);
	RNA_def_property_ui_text(prop, "Time", "Time along hair");
}

static void rna_def_child_particle(BlenderRNA *brna)
{
	StructRNA *srna;
	//PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ChildParticle", NULL);
	RNA_def_struct_ui_text(srna, "Child Particle", "DOC_BROKEN");

//	int num, parent;	/* num is face index on the final derived mesh */

//	int pa[4];			/* nearest particles to the child, used for the interpolation */
//	float w[4];			/* interpolation weights for the above particles */
//	float fuv[4], foffset; /* face vertex weights and offset */
//	float rand[3];
}

static void rna_def_particle_data(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem alive_items[] = {
		{PARS_KILLED, "KILLED", "Killed", ""},
		{PARS_DEAD, "DEAD", "Dead", ""},
		{PARS_UNBORN, "UNBORN", "Unborn", ""},
		{PARS_ALIVE, "ALIVE", "Alive", ""},
		{PARS_DYING, "DYING", "Dying", ""},
		{0, NULL, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Particle", NULL);
	RNA_def_struct_sdna(srna, "ParticleData");
	RNA_def_struct_ui_text(srna, "Particle", "Particle in a particle system.");

	prop= RNA_def_property(srna, "stick_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "stick_ob");
	RNA_def_property_struct_type(prop, "Object");
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
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	//TODO: bounds
	RNA_def_property_ui_text(prop, "Loop", "How may times the particle life has looped");

//	short rt2;
}

static void rna_def_particlesettings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem type_items[] = {
		{PART_EMITTER, "EMITTER", "Emitter", ""},
		{PART_REACTOR, "REACTOR", "Reactor", ""},
		{PART_HAIR, "HAIR", "Hair", ""},
		{PART_FLUID, "FLUID", "Fluid", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem from_items[] = {
		{PART_FROM_VERT, "VERT", "Vertexes", ""},
		{PART_FROM_FACE, "FACE", "Faces", ""},
		{PART_FROM_VOLUME, "VOLUME", "Volume", ""},
		{PART_FROM_PARTICLE, "PARTICLE", "Particle", ""},
		{PART_FROM_CHILD, "CHILD", "Child", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem dist_items[] = {
		{PART_DISTR_JIT, "JIT", "Jittered", ""},
		{PART_DISTR_RAND, "RAND", "Random", ""},
		{PART_DISTR_GRID, "GRID", "Grid", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem phys_type_items[] = {
		{PART_PHYS_NO, "NO", "no", ""},
		{PART_PHYS_NEWTON, "NEWTON", "Newtonian", ""},
		{PART_PHYS_KEYED, "KEYED", "Keyed", ""},
		{PART_PHYS_BOIDS, "BOIDS", "Boids", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem rot_mode_items[] = {
		{0, "NONE", "None", ""},
		{PART_ROT_NOR, "NOR", "Normal", ""},
		{PART_ROT_VEL, "VEL", "Velocity", ""},
		{PART_ROT_GLOB_X, "GLOB_X", "Global X", ""},
		{PART_ROT_GLOB_Y, "GLOB_Y", "Global Y", ""},
		{PART_ROT_GLOB_Z, "GLOB_Z", "Global Z", ""},
		{PART_ROT_OB_X, "OB_X", "Object X", ""},
		{PART_ROT_OB_Y, "OB_Y", "Object Y", ""},
		{PART_ROT_OB_Z, "OB_Z", "Object Z", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem ave_mode_items[] = {
		{0, "NONE", "None", ""},
		{PART_AVE_SPIN, "SPIN", "Spin", ""},
		{PART_AVE_RAND, "RAND", "Random", ""} ,
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem react_event_items[] = {
		{PART_EVENT_DEATH, "DEATH", "Death", ""},
		{PART_EVENT_COLLIDE, "COLLIDE", "Collision", ""},
		{PART_EVENT_NEAR, "NEAR", "Near", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem draw_as_items[] = {
		{PART_DRAW_NOT, "NONE", "None", ""},
		{PART_DRAW_DOT, "DOT", "Point", ""},
		{PART_DRAW_CIRC, "CIRC", "Circle", ""},
		{PART_DRAW_CROSS, "CROSS", "Cross", ""},
		{PART_DRAW_AXIS, "AXIS", "Axis", ""},
		{PART_DRAW_LINE, "LINE", "Line", ""},
		{PART_DRAW_PATH, "PATH", "Path", ""},
		{PART_DRAW_OB, "OBJECT", "Object", ""},
		{PART_DRAW_GR, "GROUP", "Group", ""},
		{PART_DRAW_BB, "BILLBOARD", "Billboard", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem child_type_items[] = {
		{0, "NONE", "None", ""},
		{PART_CHILD_PARTICLES, "PARTICLES", "Particles", ""},
		{PART_CHILD_FACES, "FACES", "Faces", ""},
		{0, NULL, NULL, NULL}
	};

	//TODO: names, tooltips
	static EnumPropertyItem rot_from_items[] = {
		{PART_ROT_KEYS, "KEYS", "keys", ""},
		{PART_ROT_ZINCR, "ZINCR", "zincr", ""},
		{PART_ROT_IINCR, "IINCR", "iincr", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem integrator_type_items[] = {
		{PART_INT_EULER, "EULER", "Euler", ""},
		{PART_INT_MIDPOINT, "MIDPOINT", "Midpoint", ""},
		{PART_INT_RK4, "RK4", "RK4", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem kink_type_items[] = {
		{PART_KINK_NO, "NO", "Nothing", ""},
		{PART_KINK_CURL, "CURL", "Curl", ""},
		{PART_KINK_RADIAL, "RADIAL", "Radial", ""},
		{PART_KINK_WAVE, "WAVE", "Wave", ""},
		{PART_KINK_BRAID, "BRAID", "Braid", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem kink_axis_items[] = {
		{0, "X", "X", ""},
		{1, "Y", "Y", ""},
		{2, "Z", "Z", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem bb_align_items[] = {
		{PART_BB_X, "X", "X", ""},
		{PART_BB_Y, "Y", "Y", ""},
		{PART_BB_Z, "Z", "Z", ""},
		{PART_BB_VIEW, "VIEW", "View", ""},
		{PART_BB_VEL, "VEL", "Velocity", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem bb_anim_items[] = {
		{PART_BB_ANIM_NONE, "NONE", "None", ""},
		{PART_BB_ANIM_TIME, "TIME", "Time", ""},
		{PART_BB_ANIM_ANGLE, "ANGLE", "Angle", ""},
		//{PART_BB_ANIM_OFF_TIME, "OFF_TIME", "off_time", ""},
		//{PART_BB_ANIM_OFF_ANGLE, "OFF_ANGLE", "off_angle", ""},
		{0, NULL, NULL, NULL}
	};

	static EnumPropertyItem bb_split_offset_items[] = {
		{PART_BB_OFF_NONE, "NONE", "None", ""},
		{PART_BB_OFF_LINEAR, "LINEAR", "Linear", ""},
		{PART_BB_OFF_RANDOM, "RANDOM", "Random", ""},
		{0, NULL, NULL, NULL}
	};

	srna= RNA_def_struct(brna, "ParticleSettings", "ID");
	RNA_def_struct_ui_text(srna, "Particle Settings", "Particle settings, reusable by multiple particle systems.");

	/* flag */
	prop= RNA_def_property(srna, "react_start_end", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_REACT_STA_END);
	RNA_def_property_ui_text(prop, "Start/End", "Give birth to unreacted particles eventually.");

	prop= RNA_def_property(srna, "react_multiple", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_REACT_MULTIPLE);
	RNA_def_property_ui_text(prop, "Multi React", "React multiple times.");

	prop= RNA_def_property(srna, "loop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_LOOP);
	RNA_def_property_ui_text(prop, "Loop", "Loop particle lives.");

	prop= RNA_def_property(srna, "hair_geometry", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_HAIR_GEOMETRY);
	RNA_def_property_ui_text(prop, "Hair Geometry", "");//TODO: tooltip

	prop= RNA_def_property(srna, "unborn", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_UNBORN);
	RNA_def_property_ui_text(prop, "Unborn", "Show particles before they are emitted.");

	prop= RNA_def_property(srna, "died", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_DIED);
	RNA_def_property_ui_text(prop, "Died", "Show particles after they have died");

	prop= RNA_def_property(srna, "trand", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_TRAND);
	RNA_def_property_ui_text(prop, "Random", "Emit in random order of elements");

	prop= RNA_def_property(srna, "even_distribution", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_EDISTR);
	RNA_def_property_ui_text(prop, "Even Distribution", "Use even distribution from faces based on face areas or edge lengths.");

	prop= RNA_def_property(srna, "sticky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_STICKY);
	RNA_def_property_ui_text(prop, "Sticky", "Particles stick to collided objects if they die in the collision.");

	prop= RNA_def_property(srna, "die_on_collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_DIE_ON_COL);
	RNA_def_property_ui_text(prop, "Die on hit", "Particles die when they collide with a deflector object.");

	prop= RNA_def_property(srna, "size_deflect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SIZE_DEFL);
	RNA_def_property_ui_text(prop, "Size Deflect", "Use particle's size in deflection.");

	prop= RNA_def_property(srna, "rotation_dynamic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ROT_DYN);
	RNA_def_property_ui_text(prop, "Dynamic", "Sets rotation to dynamic/constant");

	prop= RNA_def_property(srna, "sizemass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SIZEMASS);
	RNA_def_property_ui_text(prop, "Mass from Size", "Multiply mass with particle size.");

	prop= RNA_def_property(srna, "abs_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ABS_LENGTH);
	RNA_def_property_ui_text(prop, "Abs Length", "Use maximum length for children");

	prop= RNA_def_property(srna, "absolute_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ABS_TIME);
	RNA_def_property_ui_text(prop, "Absolute Time", "Set all ipos that work on particles to be calculated in absolute/relative time.");

	prop= RNA_def_property(srna, "global_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_GLOB_TIME);
	RNA_def_property_ui_text(prop, "Global Time", "Set all ipos that work on particles to be calculated in global/object time.");

	prop= RNA_def_property(srna, "boids_2d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_BOIDS_2D);
	RNA_def_property_ui_text(prop, "Boids 2D", "Constrain boids to a surface");

	prop= RNA_def_property(srna, "branching", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_BRANCHING);
	RNA_def_property_ui_text(prop, "Branching", "Branch child paths from eachother.");

	prop= RNA_def_property(srna, "animate_branching", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_ANIM_BRANCHING);
	RNA_def_property_ui_text(prop, "Animated", "Animate branching");

	prop= RNA_def_property(srna, "symmetric_branching", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SYMM_BRANCHING);
	RNA_def_property_ui_text(prop, "Symmetric", "Start and end points are the same.");

	prop= RNA_def_property(srna, "hair_bspline", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_HAIR_BSPLINE);
	RNA_def_property_ui_text(prop, "B-Spline", "Interpolate hair using B-Splines.");

	prop= RNA_def_property(srna, "grid_invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_GRID_INVERT);
	RNA_def_property_ui_text(prop, "Invert", "Invert what is considered object and what is not.");

	prop= RNA_def_property(srna, "child_effector", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_EFFECT);
	RNA_def_property_ui_text(prop, "Children", "Apply effectors to children.");

	prop= RNA_def_property(srna, "child_seams", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_SEAMS);
	RNA_def_property_ui_text(prop, "Use seams", "Use seams to determine parents");

	prop= RNA_def_property(srna, "child_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_RENDER);
	RNA_def_property_ui_text(prop, "child_render", "");

	prop= RNA_def_property(srna, "child_guide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_CHILD_GUIDE);
	RNA_def_property_ui_text(prop, "child_guide", "");

	prop= RNA_def_property(srna, "self_effect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PART_SELF_EFFECT);
	RNA_def_property_ui_text(prop, "Self Effect", "Particle effectors effect themselves.");


	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	prop= RNA_def_property(srna, "emit_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "from");
	RNA_def_property_enum_items(prop, from_items);
	RNA_def_property_ui_text(prop, "Emit From", "Where to emit particles from");

	prop= RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "distr");
	RNA_def_property_enum_items(prop, dist_items);
	RNA_def_property_ui_text(prop, "Distribution", "How to distribute particles on selected element");

	/* physics modes */
	prop= RNA_def_property(srna, "physics_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "phystype");
	RNA_def_property_enum_items(prop, phys_type_items);
	RNA_def_property_ui_text(prop, "Physics Type", "Particle physics type");

	prop= RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rotmode");
	RNA_def_property_enum_items(prop, rot_mode_items);
	RNA_def_property_ui_text(prop, "Rotation", "Particles initial rotation");

	prop= RNA_def_property(srna, "angular_velocity_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "avemode");
	RNA_def_property_enum_items(prop, ave_mode_items);
	RNA_def_property_ui_text(prop, "Angular Velocity Mode", "Particle angular velocity mode.");

	prop= RNA_def_property(srna, "react_event", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "reactevent");
	RNA_def_property_enum_items(prop, react_event_items);
	RNA_def_property_ui_text(prop, "React On", "The event of target particles to react on.");

	/*draw flag*/
	prop= RNA_def_property(srna, "velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_VEL);
	RNA_def_property_ui_text(prop, "Velocity", "Show particle velocity");

	/* used?
	prop= RNA_def_property(srna, "angle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_ANG);
	RNA_def_property_ui_text(prop, "Angle", "");
	*/

	prop= RNA_def_property(srna, "show_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_SIZE);
	RNA_def_property_ui_text(prop, "Size", "Show particle size");

	prop= RNA_def_property(srna, "emitter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_EMITTER);
	RNA_def_property_ui_text(prop, "Emitter", "Render emitter Object also.");

	//could not find this one in the UI - should this be read only?
	prop= RNA_def_property(srna, "keys", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_KEYS);
	RNA_def_property_ui_text(prop, "Keys", "");

	/* used?
	prop= RNA_def_property(srna, "adapt", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_ADAPT);
	RNA_def_property_ui_text(prop, "adapt", "");

	prop= RNA_def_property(srna, "cos", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_COS);
	RNA_def_property_ui_text(prop, "cos", "");
	*/

	prop= RNA_def_property(srna, "billboard_lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_BB_LOCK);
	RNA_def_property_ui_text(prop, "Lock Billboard", "Lock the billboards align axis");

	prop= RNA_def_property(srna, "parent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_PARENT);
	RNA_def_property_ui_text(prop, "Parents", "Render parent particles.");

	prop= RNA_def_property(srna, "num", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_NUM);
	RNA_def_property_ui_text(prop, "Number", "Show particle number");

	prop= RNA_def_property(srna, "rand_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_RAND_GR);
	RNA_def_property_ui_text(prop, "Pick Random", "Pick objects from group randomly");

	prop= RNA_def_property(srna, "render_adaptive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_REN_ADAPT);
	RNA_def_property_ui_text(prop, "Adaptive render", "Draw steps of the particle path");

	prop= RNA_def_property(srna, "velocity_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_VEL_LENGTH);
	RNA_def_property_ui_text(prop, "Speed", "Multiply line length by particle speed");

	prop= RNA_def_property(srna, "material_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_MAT_COL);
	RNA_def_property_ui_text(prop, "Material Color", "Draw particles using material's diffuse color.");

	prop= RNA_def_property(srna, "whole_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_WHOLE_GR);
	RNA_def_property_ui_text(prop, "Dupli Group", "Use whole group at once.");

	prop= RNA_def_property(srna, "render_strand", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "draw", PART_DRAW_REN_STRAND);
	RNA_def_property_ui_text(prop, "Strand render", "Use the strand primitive for rendering");


	prop= RNA_def_property(srna, "draw_as", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, draw_as_items);
	RNA_def_property_ui_text(prop, "Particle Visualization", "How particles are visualized");

	prop= RNA_def_property(srna, "draw_size", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Draw Size", "Size of particles on viewport in pixels (0=default)");

	prop= RNA_def_property(srna, "child_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "childtype");
	RNA_def_property_enum_items(prop, child_type_items);
	RNA_def_property_ui_text(prop, "Children From", "Create child particles");

	prop= RNA_def_property(srna, "draw_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 7);
	RNA_def_property_ui_text(prop, "Steps", "How many steps paths are drawn with (power of 2)");

	prop= RNA_def_property(srna, "render_step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ren_step");
	RNA_def_property_range(prop, 0, 9);
	RNA_def_property_ui_text(prop, "Render", "How many steps paths are rendered with (power of 2)");

	prop= RNA_def_property(srna, "hair_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 2, 50);
	RNA_def_property_ui_text(prop, "Segments", "Amount of hair segments");

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

	prop= RNA_def_property(srna, "material", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "omat");
	RNA_def_property_range(prop, 1, 16);
	RNA_def_property_ui_text(prop, "Material", "Specify material used for the particles");


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

	prop= RNA_def_property(srna, "kink", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, kink_type_items);
	RNA_def_property_ui_text(prop, "Kink", "Type of periodic offset on the path");

	prop= RNA_def_property(srna, "kink_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, kink_axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Which axis to use for offset");

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

	/* billboards */
	prop= RNA_def_property(srna, "billboard_align", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bb_align");
	RNA_def_property_enum_items(prop, bb_align_items);
	RNA_def_property_ui_text(prop, "Align to", "In respect to what the billboards are aligned");

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

	prop= RNA_def_property(srna, "billboard_random_tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bb_rand_tilt");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Tilt", "Random tilt of the billboards");

	prop= RNA_def_property(srna, "billboard_offset", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "bb_offset");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Billboard Offset", "");

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

	prop= RNA_def_property(srna, "end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -30000.0f, 30000.0f); //TODO: replace 30000 with MAXFRAMEF when available in 2.5
	RNA_def_property_float_funcs(prop, NULL, "rna_PartSettings_end_set", NULL);
	RNA_def_property_ui_text(prop, "End", "Frame # to stop emitting particles.");

	prop= RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 1.0f, 30000.0f);
	RNA_def_property_ui_text(prop, "Lifetime", "Specify the life span of the particles");

	prop= RNA_def_property(srna, "random_lifetime", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randlife");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Random", "Give the particle life a random variation.");

	prop= RNA_def_property(srna, "time_tweak", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "timetweak");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Tweak", "A multiplier for physics timestep (1.0 means one frame = 1/25 seconds)");

	prop= RNA_def_property(srna, "jitter_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "jitfac");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Amount", "Amount of jitter applied to the sampling.");

	prop= RNA_def_property(srna, "keyed_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Time", "Keyed key time relative to remaining particle life.");

	prop= RNA_def_property(srna, "effect_hair", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eff_hair");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Stiffnes", "Hair stiffness for effectors");

	//float rt; TODO:find where rt is used - can't find it in UI

	prop= RNA_def_property(srna, "total_particles", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "totpart");
	RNA_def_property_range(prop, 0, 100000);
	RNA_def_property_ui_text(prop, "Particle Amount", "The total number of particles.");

	prop= RNA_def_property(srna, "userjit", PROP_INT, PROP_UNSIGNED);//TODO: can we get a better name for userjit?
	RNA_def_property_int_sdna(prop, NULL, "userjit");
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_text(prop, "P/F", "Emission locations / face (0 = automatic).");

	prop= RNA_def_property(srna, "grid_resolution", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "grid_res");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Resolution", "The resolution of the particle grid.");

	/* initial velocity factors */
	prop= RNA_def_property(srna, "normal_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "normfac");//optional if prop names are the same
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Normal", "Let the surface normal give the particle a starting speed.");

	prop= RNA_def_property(srna, "object_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "obfac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Object", "Let the object give the particle a starting speed");

	prop= RNA_def_property(srna, "random_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randfac");//optional if prop names are the same
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Random", "Give the starting speed a random variation.");

	prop= RNA_def_property(srna, "particle_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "partfac");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Particle", "Let the target particle give the particle a starting speed.");

	prop= RNA_def_property(srna, "tangent_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tanfac");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Tangent", "Let the surface tangent give the particle a starting speed.");

	prop= RNA_def_property(srna, "tangent_phase", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tanphase");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Rot", "Rotate the surface tangent.");

	prop= RNA_def_property(srna, "reactor_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "reactfac");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Reactor", "Let the vector away from the target particles location give the particle a starting speed.");

	prop= RNA_def_property(srna, "angular_velocity_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "avefac");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Angular Velocity", "Angular velocity amount");

	prop= RNA_def_property(srna, "phase_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "phasefac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Phase", "Initial rotation phase");

	prop= RNA_def_property(srna, "random_rotation_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randrotfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Rotation", "Randomize rotation");

	prop= RNA_def_property(srna, "random_phase_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randphasefac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Phase", "Randomize rotation phase");

	/* physical properties */
	prop= RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Mass", "Specify the mass of the particles");

	prop= RNA_def_property(srna, "particle_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Size", "The size of the particles");

	prop= RNA_def_property(srna, "random_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randsize");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Size", "Give the particle size a random variation");

	prop= RNA_def_property(srna, "reaction_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "reactshape");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Shape", "Power of reaction strength dependence on distance to target.");


	/* global physical properties */
	prop= RNA_def_property(srna, "acceleration", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "acc");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Accelaration", "Constant acceleration");

	prop= RNA_def_property(srna, "drag_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dragfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Drag", "Specify the amount of air-drag.");

	prop= RNA_def_property(srna, "brownian_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "brownfac");
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Brownian", "Specify the amount of brownian motion");

	prop= RNA_def_property(srna, "damp_factorq", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dampfac");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Damp", "Specify the amount of damping");

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

	prop= RNA_def_property(srna, "random_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "randlength");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Length", "Give path length a random variation.");

	/* children */
	prop= RNA_def_property(srna, "child_nbr", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "child_nbr");//optional if prop names are the same
	RNA_def_property_range(prop, 0.0f, MAX_PART_CHILDREN);
	RNA_def_property_ui_text(prop, "Children Per Parent", "Amount of children/parent");

	prop= RNA_def_property(srna, "rendered_child_nbr", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ren_child_nbr");
	RNA_def_property_range(prop, 0.0f, MAX_PART_CHILDREN);
	RNA_def_property_ui_text(prop, "Rendered Children", "Amount of children/parent for rendering.");

	prop= RNA_def_property(srna, "virtual_parents", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "parents");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Virtual Parents", "Relative amount of virtual parents.");

	prop= RNA_def_property(srna, "child_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childsize");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Child Size", "A multiplier for the child particle size.");

	prop= RNA_def_property(srna, "child_random_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childrandsize");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Child Size", "Random variation to the size of the child particles.");

	prop= RNA_def_property(srna, "child_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childrad");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Child Radius", "Radius of children around parent.");

	prop= RNA_def_property(srna, "child_roundness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childflat");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Child Roundness", "Roundness of children around parent.");

	//TODO: is this readonly?
	prop= RNA_def_property(srna, "child_spread", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "childspread");
//	RNA_def_property_range(prop, 0.0f, upperLimitf); TODO: limits
	RNA_def_property_ui_text(prop, "Child Spread", "");

	/* clumping */
	prop= RNA_def_property(srna, "clump_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clumpfac");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Clump", "Amount of clumpimg");

	prop= RNA_def_property(srna, "clumppow", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clumppow");
	RNA_def_property_range(prop, -0.999f, 0.999f);
	RNA_def_property_ui_text(prop, "Shape", "Shape of clumpimg");


	/* kink */
	prop= RNA_def_property(srna, "kink_amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_amp");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Amplitude", "The amplitude of the offset.");

	prop= RNA_def_property(srna, "kink_frequency", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "kink_freq");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Frequency", "The frequency of the offset (1/total length)");

	prop= RNA_def_property(srna, "kink_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -0.999f, 0.999f);
	RNA_def_property_ui_text(prop, "Shape", "Adjust the offset to the beginning/end");


	/* rough */
	prop= RNA_def_property(srna, "rough1", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Rough1", "Amount of location dependant rough.");

	prop= RNA_def_property(srna, "rough1_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01f, 10.0f);
	RNA_def_property_ui_text(prop, "Size1", "Size of location dependant rough.");

	prop= RNA_def_property(srna, "rough2", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Rough2", "Amount of random rough.");

	prop= RNA_def_property(srna, "rough2_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2_size");
	RNA_def_property_range(prop, 0.01f, 10.0f);
	RNA_def_property_ui_text(prop, "Size2", "Size of random rough.");

	prop= RNA_def_property(srna, "rough2_thres", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough2_thres");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Amount of particles left untouched by random rough.");

	prop= RNA_def_property(srna, "rough_endpoint", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rough_end");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Rough Endpoint", "Amount of end point rough.");

	prop= RNA_def_property(srna, "rough_end_shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Shape", "Shape of end point rough");

	/* branching */
	prop= RNA_def_property(srna, "branch_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "branch_thres");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Threshold of branching.");

	/* drawing stuff */
	prop= RNA_def_property(srna, "line_length_tail", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_PartSetting_linelentail_get", "rna_PartSetting_linelentail_set", NULL);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Back", "Length of the line's tail");

	prop= RNA_def_property(srna, "line_length_head", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_PartSetting_linelenhead_get", "rna_PartSetting_linelenhead_set", NULL);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Head", "Length of the line's head");

	/* boids */
	prop= RNA_def_property(srna, "max_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_vel");
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Maximum Velocity", "");

	prop= RNA_def_property(srna, "lateral_acceleration_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_lat_acc");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Lateral Acceleration", "Lateral acceleration % of max velocity");

	prop= RNA_def_property(srna, "tangential_acceleration_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_tan_acc");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Tangential acceleration", "Tangential acceleration % of max velocity");

	prop= RNA_def_property(srna, "average_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "average_vel");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Average Velocity", "The usual speed % of max velocity");

	prop= RNA_def_property(srna, "banking", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Banking", "Banking of boids on turns (1.0==natural banking)");

	prop= RNA_def_property(srna, "banking_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_bank");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Maximum Banking", "How much a boid can bank at a single step");

	prop= RNA_def_property(srna, "ground_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "groundz");
	RNA_def_property_range(prop, -100.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Ground Z", "Default Z value");

	/*TODO: not sure how to deal with this
	prop= RNA_def_property(srna, "boid_factor", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "boidfac");
	RNA_def_property_ui_text(prop, "Boid Factor", "");

	//char boidrule[8];
	*/

	prop= RNA_def_property(srna, "dupli_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "dup_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_ui_text(prop, "Dupli Group", "Show Objects in this Group in place of particles");

	prop= RNA_def_property(srna, "effector_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "eff_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_ui_text(prop, "Effector Group", "Limit effectors to this Group.");

	prop= RNA_def_property(srna, "dupli_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "dup_ob");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Dupli Object", "Show this Object in place of particles.");

	prop= RNA_def_property(srna, "billboard_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "bb_ob");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Billboard Object", "Billboards face this object (default is active camera)");

#if 0
	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "ipo");
	RNA_def_property_struct_type(prop, "Ipo");
	RNA_def_property_ui_text(prop, "Ipo", "");
#endif

//	struct PartDeflect *pd;
//	struct PartDeflect *pd2;
}

static void rna_def_particlesystem(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ParticleSystem", NULL);
	RNA_def_struct_ui_text(srna, "Particle System", "Particle system in an object.");

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

	prop= RNA_def_property(srna, "softbody", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "soft");
	RNA_def_property_ui_text(prop, "Soft Body", "Soft body settings for hair physics simulation.");

	prop= RNA_def_property(srna, "reactor_target_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target_ob");
	RNA_def_property_ui_text(prop, "Reactor Target Object", "For reactor systems, the object that has the target particle system (empty if same object).");

	prop= RNA_def_property(srna, "reactor_target_particle_system", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "target_psys");
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_text(prop, "Reactor Target Particle System", "For reactor systems, index of particle system on the target object.");

	prop= RNA_def_property(srna, "boids_surface_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "keyed_ob");
	RNA_def_property_ui_text(prop, "Boids Surface Object", "For boids physics systems, constrain boids to this object's surface.");

	prop= RNA_def_property(srna, "keyed_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "keyed_ob");
	RNA_def_property_ui_text(prop, "Keyed Object", "For keyed physics systems, the object that has the target particle system.");

	prop= RNA_def_property(srna, "keyed_particle_system", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "keyed_psys");
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_text(prop, "Keyed Particle System", "For keyed physics systems, index of particle system on the keyed object.");

	prop= RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Seed", "Offset in the random number table, to get a different randomized result.");

	//	int seed;
	//	int flag, rt;
	//	short recalc, totkeyed, softflag, bakespace;
	//
	//	char bb_uvname[3][32];					/* billboard uv name */
	//
	//	/* if you change these remember to update array lengths to PSYS_TOT_VG! */
	//	short vgroup[12], vg_neg, rt3;			/* vertex groups */

	prop= RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "pointcache");
	RNA_def_property_struct_type(prop, "PointCache");
	RNA_def_property_ui_text(prop, "Point Cache", "");
}

void RNA_def_particle(BlenderRNA *brna)
{
	rna_def_hair_key(brna);
	rna_def_particle_key(brna);
	rna_def_child_particle(brna);
	rna_def_particle_data(brna);
	rna_def_particlesystem(brna);
	rna_def_particlesettings(brna);
}

#endif

