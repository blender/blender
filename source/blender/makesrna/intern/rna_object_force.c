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
 * Contributor(s): Blender Foundation (2008), Thomas Dinges
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_object_types.h"
#include "DNA_object_force.h"

#ifdef RNA_RUNTIME

#else

static void rna_def_pointcache(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "PointCache", NULL);
	RNA_def_struct_ui_text(srna, "Point Cache", "Point cache for physics simulations.");
	
	prop= RNA_def_property(srna, "start_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "startframe");
	RNA_def_property_range(prop, 1, 300000);
	RNA_def_property_ui_text(prop, "Start", "Frame on which the simulation starts.");
	
	prop= RNA_def_property(srna, "end_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "endframe");
	RNA_def_property_range(prop, 1, 300000);
	RNA_def_property_ui_text(prop, "End", "Frame on which the simulation stops.");
}

static void rna_def_collision(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "CollisionSettings", NULL);
	RNA_def_struct_sdna(srna, "PartDeflect");
	RNA_def_struct_ui_text(srna, "Collision Settings", "Collision settings for object in physics simulation.");
	
	prop= RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deflect", 1);
	RNA_def_property_ui_text(prop, "Enabled", "Enable this objects as a collider for physics systems");
	
	/* Particle Interaction */
	
	prop= RNA_def_property(srna, "damping_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_damp");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Damping Factor", "Amount of damping during particle collision");
	
	prop= RNA_def_property(srna, "random_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_rdamp");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Damping", "Random variation of damping");
	
	prop= RNA_def_property(srna, "friction_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_frict");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Friction Factor", "Amount of friction during particle collision");
	
	prop= RNA_def_property(srna, "random_friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_rfrict");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Random Friction", "Random variation of friction");
		
	prop= RNA_def_property(srna, "permeability", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_perm");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Permeability", "Chance that the particle will pass through the mesh");
	
	prop= RNA_def_property(srna, "kill_particles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PDEFLE_KILL_PART);
	RNA_def_property_ui_text(prop, "Kill Particles", "Kill collided particles");
	
	/* Soft Body and Cloth Interaction */
	
	prop= RNA_def_property(srna, "inner_thickness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_sbift");
	RNA_def_property_range(prop, 0.001f, 1.0f);
	RNA_def_property_ui_text(prop, "Inner Thickness", "Inner face thickness");
	
	prop= RNA_def_property(srna, "outer_thickness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_sboft");
	RNA_def_property_range(prop, 0.001f, 1.0f);
	RNA_def_property_ui_text(prop, "Outer Thickness", "Outer face thickness");
	
	prop= RNA_def_property(srna, "damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pdef_sbdamp");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Damping", "Amount of damping during collision");
	
	/* Does this belong here?
	prop= RNA_def_property(srna, "collision_stack", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "softflag", OB_SB_COLLFINAL);
	RNA_def_property_ui_text(prop, "Collision from Stack", "Pick collision object from modifier stack (softbody only)");
	*/
}

static void rna_def_field(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem field_type_items[] = {
		{PFIELD_FORCE, "FORCE", "None", ""},
		{PFIELD_VORTEX, "VORTEX", "Vortex", ""},
		{PFIELD_MAGNET, "MAGNET", "Magnetic", ""},
		{PFIELD_WIND, "WIND", "Wind", ""},
		{PFIELD_GUIDE, "GUIDE", "Spherical", ""},
		{PFIELD_TEXTURE, "TEXTURE", "Texture", ""},
		{PFIELD_HARMONIC, "HARMONIC", "Harmonic", ""},
		{PFIELD_CHARGE, "CHARGE", "Charge", ""},
		{PFIELD_LENNARDJ, "LENNARDJ", "Lennard-Jones", ""},
		{0, NULL, NULL, NULL}};
		
	static EnumPropertyItem falloff_items[] = {
		{PFIELD_FALL_SPHERE, "SPHERE", "Sphere", ""},
		{PFIELD_FALL_TUBE, "TUBE", "Tube", ""},
		{PFIELD_FALL_CONE, "CONE", "Cone", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "FieldSettings", NULL);
	RNA_def_struct_sdna(srna, "PartDeflect");
	RNA_def_struct_ui_text(srna, "Field Settings", "Field settings for an object in physics simulation.");
	
	/* Enums */
	
	prop= RNA_def_property(srna, "field_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "forcefield");
	RNA_def_property_enum_items(prop, field_type_items);
	RNA_def_property_ui_text(prop, "Field Type", "Choose Field Type");
	
	prop= RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "falloff");
	RNA_def_property_enum_items(prop, falloff_items);
	RNA_def_property_ui_text(prop, "Fall-Off", "Fall-Off Shape");
	
	/* Float */
	
	prop= RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_strength");
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Strength", "Strength of force field");
	
	prop= RNA_def_property(srna, "falloff_power", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_power");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Falloff Power", "Falloff power (real gravitational falloff = 2)");
	
	prop= RNA_def_property(srna, "harmonic_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_damp");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Harmonic Damping", "Damping of the harmonic force");
	
	prop= RNA_def_property(srna, "minimum_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mindist");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Minimum Distance", "Minimum distance for the field's fall-off");
	
	prop= RNA_def_property(srna, "maximum_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxdist");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Maximum Distance", "Maximum distance for the field to work");
	
	prop= RNA_def_property(srna, "radial_minimum", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "minrad");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Minimum Radial Distance", "Minimum radial distance for the field's fall-off");
	
	prop= RNA_def_property(srna, "radial_maximum", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxrad");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Maximum Radial Distance", "Maximum radial distance for the field to work");
	
	prop= RNA_def_property(srna, "radial_falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_power_r");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Radial Falloff Power", "Radial falloff power (real gravitational falloff = 2)");
}

static void rna_def_game_softbody(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "GameSoftBodySettings", NULL);
	RNA_def_struct_sdna(srna, "BulletSoftBody");
	RNA_def_struct_ui_text(srna, "Game Soft Body Settings", "Soft body simulation settings for an object in the game engine.");
}

static void rna_def_softbody(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "SoftBodySettings", NULL);
	RNA_def_struct_sdna(srna, "SoftBody");
	RNA_def_struct_ui_text(srna, "Soft Body Settings", "Soft body simulation settings for an object.");
}

void RNA_def_object_force(BlenderRNA *brna)
{
	rna_def_pointcache(brna);
	rna_def_collision(brna);
	rna_def_field(brna);
	rna_def_game_softbody(brna);
	rna_def_softbody(brna);
}

#endif
