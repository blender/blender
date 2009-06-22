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

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_pointcache.h"

#include "BLI_blenlib.h"

static void rna_Cache_toggle_disk_cache(bContext *C, PointerRNA *ptr)
{
	Object *ob = CTX_data_active_object(C);
	PointCache *cache = (PointCache*)ptr->data;
	PTCacheID *pid = NULL;
	ListBase pidlist;

	if(!ob)
		return;

	BKE_ptcache_ids_from_object(&pidlist, ob);

	for(pid=pidlist.first; pid; pid=pid->next) {
		if(pid->cache==cache)
			break;
	}

	if(pid)
		BKE_ptcache_toggle_disk_cache(pid);

	BLI_freelistN(&pidlist);
}

static void rna_Cache_idname_change(bContext *C, PointerRNA *ptr)
{
	Object *ob = CTX_data_active_object(C);
	PointCache *cache = (PointCache*)ptr->data;
	PTCacheID *pid = NULL, *pid2;
	ListBase pidlist;
	int new_name = 1;
	char name[80];

	if(!ob)
		return;

	/* TODO: check for proper characters */

	BKE_ptcache_ids_from_object(&pidlist, ob);

	for(pid=pidlist.first; pid; pid=pid->next) {
		if(pid->cache==cache)
			pid2 = pid;
		else if(strcmp(cache->name, "") && strcmp(cache->name,pid->cache->name)==0) {
			/*TODO: report "name exists" to user */
			strcpy(cache->name, cache->prev_name);
			new_name = 0;
		}
	}

	if(new_name) {
		if(pid2 && cache->flag & PTCACHE_DISK_CACHE) {
			strcpy(name, cache->name);
			strcpy(cache->name, cache->prev_name);

			cache->flag &= ~PTCACHE_DISK_CACHE;

			BKE_ptcache_toggle_disk_cache(pid2);

			strcpy(cache->name, name);

			cache->flag |= PTCACHE_DISK_CACHE;

			BKE_ptcache_toggle_disk_cache(pid2);
		}

		strcpy(cache->prev_name, cache->name);
	}

	BLI_freelistN(&pidlist);
}
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

	/* flags */
	prop= RNA_def_property(srna, "baked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_BAKED);

	prop= RNA_def_property(srna, "baking", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_BAKING);

	prop= RNA_def_property(srna, "disk_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_DISK_CACHE);
	RNA_def_property_ui_text(prop, "Disk Cache", "Save cache files to disk");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_toggle_disk_cache");

	prop= RNA_def_property(srna, "outdated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_OUTDATED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Cache is outdated", "");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Cache name");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_idname_change");

	prop= RNA_def_property(srna, "autocache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PTCACHE_AUTOCACHE);
	RNA_def_property_ui_text(prop, "Auto Cache", "Cache changes automatically");
	//RNA_def_property_update(prop, NC_OBJECT, "rna_Cache_toggle_autocache");

	prop= RNA_def_property(srna, "info", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "info");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Cache Info", "Info on current cache status.");

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
		{0, "NONE", 0, "None", ""},
		{PFIELD_FORCE, "SPHERICAL", 0, "Spherical", ""},
		{PFIELD_VORTEX, "VORTEX", 0, "Vortex", ""},
		{PFIELD_MAGNET, "MAGNET", 0, "Magnetic", ""},
		{PFIELD_WIND, "WIND", 0, "Wind", ""},
		{PFIELD_GUIDE, "GUIDE", 0, "Curve Guide", ""},
		{PFIELD_TEXTURE, "TEXTURE", 0, "Texture", ""},
		{PFIELD_HARMONIC, "HARMONIC", 0, "Harmonic", ""},
		{PFIELD_CHARGE, "CHARGE", 0, "Charge", ""},
		{PFIELD_LENNARDJ, "LENNARDJ", 0, "Lennard-Jones", ""},
		{0, NULL, 0, NULL, NULL}};
		
	static EnumPropertyItem falloff_items[] = {
		{PFIELD_FALL_SPHERE, "SPHERE", 0, "Sphere", ""},
		{PFIELD_FALL_TUBE, "TUBE", 0, "Tube", ""},
		{PFIELD_FALL_CONE, "CONE", 0, "Cone", ""},
		{0, NULL, 0, NULL, NULL}};
		
	static EnumPropertyItem texture_items[] = {
		{PFIELD_TEX_RGB, "RGB", 0, "RGB", ""},
		{PFIELD_TEX_GRAD, "GRADIENT", 0, "Gradient", ""},
		{PFIELD_TEX_CURL, "CURL", 0, "Curl", ""},
		{0, NULL, 0, NULL, NULL}};

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
	
	prop= RNA_def_property(srna, "texture_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tex_mode");
	RNA_def_property_enum_items(prop, texture_items);
	RNA_def_property_ui_text(prop, "Texture Mode", "How the texture effect is calculated (RGB & Curl need a RGB texture else Gradient will be used instead)");
	
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

	prop= RNA_def_property(srna, "texture_nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tex_nabla");
	RNA_def_property_range(prop, 0.0001f, 1.0f);
	RNA_def_property_ui_text(prop, "Nabla", "Defines size of derivative offset used for calculating gradient and curl");
	
	prop= RNA_def_property(srna, "noise", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f_noise");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Noise", "Noise of the wind force");

	prop= RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_range(prop, 1, 128);
	RNA_def_property_ui_text(prop, "Seed", "Seed of the wind noise");

	/* Boolean */
	
	prop= RNA_def_property(srna, "use_min_distance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_USEMIN);
	RNA_def_property_ui_text(prop, "Use Min", "Use a minimum distance for the field's fall-off");
	
	prop= RNA_def_property(srna, "use_max_distance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_USEMAX);
	RNA_def_property_ui_text(prop, "Use Max", "Use a maximum distance for the field to work");
	
	prop= RNA_def_property(srna, "use_radial_min", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_USEMINR);
	RNA_def_property_ui_text(prop, "Use Min", "Use a minimum radial distance for the field's fall-off");
	// "Use a minimum angle for the field's fall-off"
	
	prop= RNA_def_property(srna, "use_radial_max", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_USEMAXR);
	RNA_def_property_ui_text(prop, "Use Max", "Use a maximum radial distance for the field to work");
	// "Use a maximum angle for the field to work"
	
	prop= RNA_def_property(srna, "guide_path_add", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_GUIDE_PATH_ADD);
	RNA_def_property_ui_text(prop, "Additive", "Based on distance/falloff it adds a portion of the entire path");
	
	prop= RNA_def_property(srna, "planar", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_PLANAR);
	RNA_def_property_ui_text(prop, "Planar", "Create planar field");
	
	prop= RNA_def_property(srna, "surface", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_SURFACE);
	RNA_def_property_ui_text(prop, "Surface", "Use closest point on surface");
	
	prop= RNA_def_property(srna, "positive_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_POSZ);
	RNA_def_property_ui_text(prop, "Positive", "Effect only in direction of positive Z axis");
	
	prop= RNA_def_property(srna, "use_coordinates", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_TEX_OBJECT);
	RNA_def_property_ui_text(prop, "Use Coordinates", "Use object/global coordinates for texture");
	
	prop= RNA_def_property(srna, "force_2d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_TEX_2D);
	RNA_def_property_ui_text(prop, "2D", "Apply force only in 2d");
	
	prop= RNA_def_property(srna, "root_coordinates", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PFIELD_TEX_ROOTCO);
	RNA_def_property_ui_text(prop, "Root Texture Coordinates", "Texture coordinates from root particle locations");
	
	/* Pointer */
	
	prop= RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tex");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture", "Texture to use as force");
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
