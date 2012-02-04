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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_fluidsim.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_object_fluidsim.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_particle_types.h"

#include "BKE_depsgraph.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

static StructRNA* rna_FluidSettings_refine(struct PointerRNA *ptr)
{
	FluidsimSettings *fss= (FluidsimSettings*)ptr->data;

	switch(fss->type) {
		case OB_FLUIDSIM_DOMAIN:
			return &RNA_DomainFluidSettings;
		case OB_FLUIDSIM_FLUID:
			return &RNA_FluidFluidSettings;
		case OB_FLUIDSIM_OBSTACLE:
			return &RNA_ObstacleFluidSettings;
		case OB_FLUIDSIM_INFLOW:
			return &RNA_InflowFluidSettings;
		case OB_FLUIDSIM_OUTFLOW:
			return &RNA_OutflowFluidSettings;
		case OB_FLUIDSIM_PARTICLE:
			return &RNA_ParticleFluidSettings;
		case OB_FLUIDSIM_CONTROL:
			return &RNA_ControlFluidSettings;
		default:
			return &RNA_FluidSettings;
	}
}

static void rna_fluid_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Object *ob= ptr->id.data;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT|ND_MODIFIER, ob);
}

static int fluidsim_find_lastframe(Object *ob, FluidsimSettings *fss)
{
	char targetFileTest[FILE_MAX];
	char targetFile[FILE_MAX];
	int curFrame = 1;

	BLI_join_dirfile(targetFile, sizeof(targetFile), fss->surfdataPath, OB_FLUIDSIM_SURF_FINAL_OBJ_FNAME);
	BLI_path_abs(targetFile, modifier_path_relbase(ob));

	do {
		BLI_strncpy(targetFileTest, targetFile, sizeof(targetFileTest));
		BLI_path_frame(targetFileTest, curFrame++, 0);
	} while(BLI_exists(targetFileTest));

	return curFrame - 1;
}

static void rna_fluid_find_enframe(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob= ptr->id.data;
	FluidsimModifierData *fluidmd= (FluidsimModifierData*)modifiers_findByType(ob, eModifierType_Fluidsim);

	if(fluidmd->fss->flag & OB_FLUIDSIM_REVERSE) {
		fluidmd->fss->lastgoodframe = fluidsim_find_lastframe(ob, fluidmd->fss);
	}
	else {
		fluidmd->fss->lastgoodframe = -1;
	}
	rna_fluid_update(bmain, scene, ptr);
}

static void rna_FluidSettings_update_type(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob= (Object*)ptr->id.data;
	FluidsimModifierData *fluidmd;
	ParticleSystemModifierData *psmd;
	ParticleSystem *psys;
	ParticleSettings *part;
	
	fluidmd= (FluidsimModifierData*)modifiers_findByType(ob, eModifierType_Fluidsim);
	fluidmd->fss->flag &= ~OB_FLUIDSIM_REVERSE; // clear flag

	/* remove fluidsim particle system */
	if(fluidmd->fss->type & OB_FLUIDSIM_PARTICLE) {
		for(psys=ob->particlesystem.first; psys; psys=psys->next)
			if(psys->part->type == PART_FLUID)
				break;

		if(ob->type == OB_MESH && !psys) {
			/* add particle system */
			part= psys_new_settings("ParticleSettings", bmain);
			psys= MEM_callocN(sizeof(ParticleSystem), "particle_system");

			part->type= PART_FLUID;
			psys->part= part;
			psys->pointcache= BKE_ptcache_add(&psys->ptcaches);
			psys->flag |= PSYS_ENABLED;
			BLI_strncpy(psys->name, "FluidParticles", sizeof(psys->name));
			BLI_addtail(&ob->particlesystem,psys);

			/* add modifier */
			psmd= (ParticleSystemModifierData*)modifier_new(eModifierType_ParticleSystem);
			BLI_strncpy(psmd->modifier.name, "FluidParticleSystem", sizeof(psmd->modifier.name));
			psmd->psys= psys;
			BLI_addtail(&ob->modifiers, psmd);
			modifier_unique_name(&ob->modifiers, (ModifierData *)psmd);
		}
	}
	else {
		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			if(psys->part->type == PART_FLUID) {
				/* clear modifier */
				psmd= psys_get_modifier(ob, psys);
				BLI_remlink(&ob->modifiers, psmd);
				modifier_free((ModifierData *)psmd);

				/* clear particle system */
				BLI_remlink(&ob->particlesystem, psys);
				psys_free(ob, psys);
			}
		}
	}

	rna_fluid_update(bmain, scene, ptr);
}

static void rna_DomainFluidSettings_memory_estimate_get(PointerRNA *ptr, char *value)
{
#ifndef WITH_MOD_FLUID
	(void)ptr;
	value[0]= '\0';
#else
	Object *ob= (Object*)ptr->id.data;
	FluidsimSettings *fss= (FluidsimSettings*)ptr->data;

	fluid_estimate_memory(ob, fss, value);
#endif
}

static int rna_DomainFluidSettings_memory_estimate_length(PointerRNA *UNUSED(ptr))
{
#ifndef WITH_MOD_FLUID
	return 0;
#else
	return 31;
#endif
}

static char *rna_FluidSettings_path(PointerRNA *ptr)
{
	FluidsimSettings *fss = (FluidsimSettings*)ptr->data;
	ModifierData *md= (ModifierData *)fss->fmd;

	return BLI_sprintfN("modifiers[\"%s\"].settings", md->name);
}

static void rna_FluidMeshVertex_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	FluidsimSettings *fss = (FluidsimSettings*)ptr->data;
	rna_iterator_array_begin(iter, fss->meshVelocities, sizeof(float)*3, fss->totvert, 0, NULL);
}

static int rna_FluidMeshVertex_data_length(PointerRNA *ptr)
{
	FluidsimSettings *fss = (FluidsimSettings*)ptr->data;
	return fss->totvert;
}

#else

static void rna_def_fluidsim_slip(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem slip_items[] = {
		{OB_FSBND_NOSLIP, "NOSLIP", 0, "No Slip", "Obstacle causes zero normal and tangential velocity (=sticky), default for all (only option for moving objects)"},
		{OB_FSBND_PARTSLIP, "PARTIALSLIP", 0, "Partial Slip", "Mix between no-slip and free-slip (non moving objects only!)"},
		{OB_FSBND_FREESLIP, "FREESLIP", 0, "Free Slip", "Obstacle only causes zero normal velocity (=not sticky, non moving objects only!)"},
		{0, NULL, 0, NULL, NULL}};

	prop= RNA_def_property(srna, "slip_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "typeFlags");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, slip_items);
	RNA_def_property_ui_text(prop, "Slip Type", "");

	prop= RNA_def_property(srna, "partial_slip_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "partSlipValue");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Partial Slip Amount", "Amount of mixing between no- and free-slip, 0 is no slip and 1 is free slip");
}

static void rna_def_fluid_mesh_vertices(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "FluidMeshVertex", NULL);
	RNA_def_struct_sdna(srna, "FluidVertexVelocity");
	RNA_def_struct_ui_text(srna, "Fluid Mesh Vertex", "Vertex of a simulated fluid mesh");
	RNA_def_struct_ui_icon(srna, ICON_VERTEXSEL);

	prop= RNA_def_property(srna, "velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_sdna(prop, NULL, "vel");
	RNA_def_property_ui_text(prop, "Velocity", "");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}


static void rna_def_fluidsim_domain(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem quality_items[] = {
		{OB_FSDOM_GEOM, "GEOMETRY", 0, "Geometry", "Display geometry"},
		{OB_FSDOM_PREVIEW, "PREVIEW", 0, "Preview", "Display preview quality results"},
		{OB_FSDOM_FINAL, "FINAL", 0, "Final", "Display final quality results"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem viscosity_items[] = {
		{1, "MANUAL", 0, "Manual", "Manual viscosity settings"},
		{2, "WATER", 0, "Water", "Viscosity of 1.0 * 10^-6"},
		{3, "OIL", 0, "Oil", "Viscosity of 5.0 * 10^-5"},
		{4, "HONEY", 0, "Honey", "Viscosity of 2.0 * 10^-3"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "DomainFluidSettings", "FluidSettings");
	RNA_def_struct_sdna(srna, "FluidsimSettings");
	RNA_def_struct_ui_text(srna, "Domain Fluid Simulation Settings", "Fluid simulation settings for the domain of a fluid simulation");

	/* standard settings */

	prop= RNA_def_property(srna, "resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolutionxyz");
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_text(prop, "Resolution", "Domain resolution in X,Y and Z direction");

	prop= RNA_def_property(srna, "preview_resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "previewresxyz");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Preview Resolution", "Preview resolution in X,Y and Z direction");

	prop= RNA_def_property(srna, "viewport_display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "guiDisplayMode");
	RNA_def_property_enum_items(prop, quality_items);
	RNA_def_property_ui_text(prop, "Viewport Display Mode", "How to display the mesh in the viewport");
	RNA_def_property_update(prop, 0, "rna_fluid_update");

	prop= RNA_def_property(srna, "render_display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "renderDisplayMode");
	RNA_def_property_enum_items(prop, quality_items);
	RNA_def_property_ui_text(prop, "Render Display Mode", "How to display the mesh for rendering");

	prop= RNA_def_property(srna, "use_reverse_frames", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", OB_FLUIDSIM_REVERSE);
	RNA_def_property_ui_text(prop, "Reverse Frames", "Reverse fluid frames");
	RNA_def_property_update(prop, 0, "rna_fluid_find_enframe");

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_maxlength(prop, FILE_MAX);
	RNA_def_property_string_sdna(prop, NULL, "surfdataPath");
	RNA_def_property_ui_text(prop, "Path", "Directory (and/or filename prefix) to store baked fluid simulation files in");
	RNA_def_property_update(prop, 0, "rna_fluid_update");

	prop= RNA_def_property(srna, "memory_estimate", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_DomainFluidSettings_memory_estimate_get", "rna_DomainFluidSettings_memory_estimate_length", NULL);
	RNA_def_property_ui_text(prop, "Memory Estimate", "Estimated amount of memory needed for baking the domain");

	/* advanced settings */
	prop= RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_ACCELERATION);
	RNA_def_property_float_sdna(prop, NULL, "grav");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1000.1, 1000.1);
	RNA_def_property_ui_text(prop, "Gravity", "Gravity in X, Y and Z direction");
	
	prop= RNA_def_property(srna, "use_time_override", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", OB_FLUIDSIM_OVERRIDE_TIME);
	RNA_def_property_ui_text(prop, "Override Time", "Use a custom start and end time (in seconds) instead of the scene's timeline");
	
	prop= RNA_def_property(srna, "start_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "animStart");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Start Time", "Simulation time of the first blender frame (in seconds)");
	
	prop= RNA_def_property(srna, "end_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "animEnd");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "End Time", "Simulation time of the last blender frame (in seconds)");
	
	prop= RNA_def_property(srna, "frame_offset", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "frameOffset");
	RNA_def_property_ui_text(prop, "Cache Offset", "Offset when reading baked cache");
	RNA_def_property_update(prop, NC_OBJECT, "rna_fluid_update");
	
	prop= RNA_def_property(srna, "simulation_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "realsize");
	RNA_def_property_range(prop, 0.001, 10);
	RNA_def_property_ui_text(prop, "Real World Size", "Size of the simulation domain in metres");
	
	prop= RNA_def_property(srna, "simulation_rate", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "animRate");
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Simulation Speed", "Fluid motion rate (0 = stationary, 1 = normal speed)");
	
	prop= RNA_def_property(srna, "viscosity_preset", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "viscosityMode");
	RNA_def_property_enum_items(prop, viscosity_items);
	RNA_def_property_ui_text(prop, "Viscosity Preset", "Set viscosity of the fluid to a preset value, or use manual input");

	prop= RNA_def_property(srna, "viscosity_base", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "viscosityValue");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Viscosity Base", "Viscosity setting: value that is multiplied by 10 to the power of (exponent*-1)");

	prop= RNA_def_property(srna, "viscosity_exponent", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "viscosityExponent");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Viscosity Exponent", "Negative exponent for the viscosity value (to simplify entering small values e.g. 5*10^-6)");

	prop= RNA_def_property(srna, "grid_levels", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxRefine");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, -1, 4);
	RNA_def_property_ui_text(prop, "Grid Levels", "Number of coarsened grids to use (-1 for automatic)");

	prop= RNA_def_property(srna, "compressibility", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gstar");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_text(prop, "Compressibility", "Allowed compressibility due to gravitational force for standing fluid (directly affects simulation step size)");

	/* domain boundary settings */

	rna_def_fluidsim_slip(srna);

	prop= RNA_def_property(srna, "surface_smooth", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "surfaceSmoothing");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0, 5.0);
	RNA_def_property_ui_text(prop, "Surface Smoothing", "Amount of surface smoothing (a value of 0 is off, 1 is normal smoothing and more than 1 is extra smoothing)");

	prop= RNA_def_property(srna, "surface_subdivisions", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "surfaceSubdivs");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, 5);
	RNA_def_property_ui_text(prop, "Surface Subdivisions", "Number of isosurface subdivisions (this is necessary for the inclusion of particles into the surface generation - WARNING: can lead to longer computation times !)");

	prop= RNA_def_property(srna, "use_speed_vectors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "domainNovecgen", 0);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Generate Speed Vectors", "Generate speed vectors for vector blur");

	/* no collision object surface */
	prop= RNA_def_property(srna, "surface_noobs", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "typeFlags", OB_FSSG_NOOBS);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Hide fluid surface", "");

	/* particles */

	prop= RNA_def_property(srna, "tracer_particles", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "generateTracers");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, 10000);
	RNA_def_property_ui_text(prop, "Tracer Particles", "Number of tracer particles to generate");

	prop= RNA_def_property(srna, "generate_particles", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "generateParticles");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Generate Particles", "Amount of particles to generate (0=off, 1=normal, >1=more)");
	
	/* simulated fluid mesh data */
	prop= RNA_def_property(srna, "fluid_mesh_vertices", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "FluidMeshVertex");
	RNA_def_property_ui_text(prop, "Fluid Mesh Vertices", "Vertices of the fluid mesh generated by simulation");
	RNA_def_property_collection_funcs(prop, "rna_FluidMeshVertex_data_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", "rna_FluidMeshVertex_data_length", NULL, NULL, NULL);
	rna_def_fluid_mesh_vertices(brna);
}

static void rna_def_fluidsim_volume(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem volume_type_items[] = {
		{1, "VOLUME", 0, "Volume", "Use only the inner volume of the mesh"},
		{2, "SHELL", 0, "Shell", "Use only the outer shell of the mesh"},
		{3, "BOTH", 0, "Both", "Use both the inner volume and the outer shell of the mesh"},
		{0, NULL, 0, NULL, NULL}};

	prop= RNA_def_property(srna, "volume_initialization", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "volumeInitType");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, volume_type_items);
	RNA_def_property_ui_text(prop, "Volume Initialization", "Volume initialization type");

	prop= RNA_def_property(srna, "use_animated_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "domainNovecgen", 0);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Export Animated Mesh", "Export this mesh as an animated one (slower, only use if really necessary [e.g. armatures or parented objects], animated pos/rot/scale F-Curves do not require it)");
}

static void rna_def_fluidsim_active(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop= RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", OB_FLUIDSIM_ACTIVE);
	RNA_def_property_ui_text(prop, "Enabled", "Object contributes to the fluid simulation");
}

static void rna_def_fluidsim_fluid(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "FluidFluidSettings", "FluidSettings");
	RNA_def_struct_sdna(srna, "FluidsimSettings");
	RNA_def_struct_ui_text(srna, "Fluid Fluid Simulation Settings", "Fluid simulation settings for the fluid in the simulation");

	rna_def_fluidsim_active(srna);
	rna_def_fluidsim_volume(srna);
	
	prop= RNA_def_property(srna, "initial_velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "iniVelx");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1000.1, 1000.1);
	RNA_def_property_ui_text(prop, "Initial Velocity", "Initial velocity of fluid");
}

static void rna_def_fluidsim_obstacle(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ObstacleFluidSettings", "FluidSettings");
	RNA_def_struct_sdna(srna, "FluidsimSettings");
	RNA_def_struct_ui_text(srna, "Obstacle Fluid Simulation Settings", "Fluid simulation settings for obstacles in the simulation");

	rna_def_fluidsim_active(srna);
	rna_def_fluidsim_volume(srna);
	rna_def_fluidsim_slip(srna);

	prop= RNA_def_property(srna, "impact_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "surfaceSmoothing");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, -2.0, 10.0);
	RNA_def_property_ui_text(prop, "Impact Factor", "This is an unphysical value for moving objects - it controls the impact an obstacle has on the fluid, =0 behaves a bit like outflow (deleting fluid), =1 is default, while >1 results in high forces (can be used to tweak total mass)");
}

static void rna_def_fluidsim_inflow(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "InflowFluidSettings", "FluidSettings");
	RNA_def_struct_sdna(srna, "FluidsimSettings");
	RNA_def_struct_ui_text(srna, "Inflow Fluid Simulation Settings", "Fluid simulation settings for objects adding fluids in the simulation");

	rna_def_fluidsim_active(srna);
	rna_def_fluidsim_volume(srna);

	prop= RNA_def_property(srna, "inflow_velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "iniVelx");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1000.1, 1000.1);
	RNA_def_property_ui_text(prop, "Inflow Velocity", "Initial velocity of fluid");

	prop= RNA_def_property(srna, "use_local_coords", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "typeFlags", OB_FSINFLOW_LOCALCOORD);
	RNA_def_property_ui_text(prop, "Local Coordinates", "Use local coordinates for inflow (e.g. for rotating objects)");
}

static void rna_def_fluidsim_outflow(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "OutflowFluidSettings", "FluidSettings");
	RNA_def_struct_sdna(srna, "FluidsimSettings");
	RNA_def_struct_ui_text(srna, "Outflow Fluid Simulation Settings", "Fluid simulation settings for objects removing fluids from the simulation");

	rna_def_fluidsim_active(srna);
	rna_def_fluidsim_volume(srna);
}

static void rna_def_fluidsim_particle(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ParticleFluidSettings", "FluidSettings");
	RNA_def_struct_sdna(srna, "FluidsimSettings");
	RNA_def_struct_ui_text(srna, "Particle Fluid Simulation Settings", "Fluid simulation settings for objects storing fluid particles generated by the simulation");

	prop= RNA_def_property(srna, "use_drops", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "typeFlags", OB_FSPART_DROP);
	RNA_def_property_ui_text(prop, "Drops", "Show drop particles");

	prop= RNA_def_property(srna, "use_floats", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "typeFlags", OB_FSPART_FLOAT);
	RNA_def_property_ui_text(prop, "Floats", "Show floating foam particles");

	prop= RNA_def_property(srna, "show_tracer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "typeFlags", OB_FSPART_TRACER);
	RNA_def_property_ui_text(prop, "Tracer", "Show tracer particles");

	prop= RNA_def_property(srna, "particle_influence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "particleInfSize");
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_text(prop, "Particle Influence", "Amount of particle size scaling: 0=off (all same size), 1=full (range 0.2-2.0), >1=stronger");

	prop= RNA_def_property(srna, "alpha_influence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "particleInfAlpha");
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_text(prop, "Alpha Influence", "Amount of particle alpha change, inverse of size influence: 0=off (all same alpha), 1=full (large particles get lower alphas, smaller ones higher values)");

	prop= RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_maxlength(prop, FILE_MAX);
	RNA_def_property_string_sdna(prop, NULL, "surfdataPath");
	RNA_def_property_ui_text(prop, "Path", "Directory (and/or filename prefix) to store and load particles from");
	RNA_def_property_update(prop, 0, "rna_fluid_update");
}

static void rna_def_fluidsim_control(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ControlFluidSettings", "FluidSettings");
	RNA_def_struct_sdna(srna, "FluidsimSettings");
	RNA_def_struct_ui_text(srna, "Control Fluid Simulation Settings", "Fluid simulation settings for objects controlling the motion of fluid in the simulation");

	rna_def_fluidsim_active(srna);
	
	prop= RNA_def_property(srna, "start_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "cpsTimeStart");
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "Start Time", "Time when the control particles are activated");
	
	prop= RNA_def_property(srna, "end_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "cpsTimeEnd");
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_text(prop, "End Time", "Time when the control particles are deactivated");

	prop= RNA_def_property(srna, "attraction_strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "attractforceStrength");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_text(prop, "Attraction Strength", "Force strength for directional attraction towards the control object");

	prop= RNA_def_property(srna, "attraction_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "attractforceRadius");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Attraction Radius", "Force field radius around the control object");
	
	prop= RNA_def_property(srna, "velocity_strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "velocityforceStrength");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Velocity Strength", "Force strength of how much of the control object's velocity is influencing the fluid velocity");

	prop= RNA_def_property(srna, "velocity_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "velocityforceRadius");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Velocity Radius", "Force field radius around the control object");

	prop= RNA_def_property(srna, "quality", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cpsQuality");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 5.0, 100.0);
	RNA_def_property_ui_text(prop, "Quality", "Quality which is used for object sampling (higher = better but slower)");

	prop= RNA_def_property(srna, "use_reverse_frames", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", OB_FLUIDSIM_REVERSE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Reverse Frames", "Reverse control object movement");
	RNA_def_property_update(prop, 0, "rna_fluid_find_enframe");
}

void RNA_def_fluidsim(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_fluid_type_items[] = {
		{OB_FLUIDSIM_ENABLE, "NONE", 0, "None", ""},
		{OB_FLUIDSIM_DOMAIN, "DOMAIN", 0, "Domain", "Bounding box of this object represents the computational domain of the fluid simulation"},
		{OB_FLUIDSIM_FLUID, "FLUID", 0, "Fluid", "Object represents a volume of fluid in the simulation"},
		{OB_FLUIDSIM_OBSTACLE, "OBSTACLE", 0, "Obstacle", "Object is a fixed obstacle"},
		{OB_FLUIDSIM_INFLOW, "INFLOW", 0, "Inflow", "Object adds fluid to the simulation"},
		{OB_FLUIDSIM_OUTFLOW, "OUTFLOW", 0, "Outflow", "Object removes fluid from the simulation"},
		{OB_FLUIDSIM_PARTICLE, "PARTICLE", 0, "Particle", "Object is made a particle system to display particles generated by a fluidsim domain object"},
		{OB_FLUIDSIM_CONTROL, "CONTROL", 0, "Control", "Object is made a fluid control mesh, which influences the fluid"},
		{0, NULL, 0, NULL, NULL}};


	srna= RNA_def_struct(brna, "FluidSettings", NULL);
	RNA_def_struct_sdna(srna, "FluidsimSettings");
	RNA_def_struct_refine_func(srna, "rna_FluidSettings_refine");
	RNA_def_struct_path_func(srna, "rna_FluidSettings_path");
	RNA_def_struct_ui_text(srna, "Fluid Simulation Settings", "Fluid simulation settings for an object taking part in the simulation");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_fluid_type_items);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Type", "Type of participation in the fluid simulation");
	RNA_def_property_update(prop, 0, "rna_FluidSettings_update_type");

	//prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	//RNA_def_property_ui_text(prop, "IPO Curves", "IPO curves used by fluid simulation settings");

	/* types */

	rna_def_fluidsim_domain(brna);
	rna_def_fluidsim_fluid(brna);
	rna_def_fluidsim_obstacle(brna);
	rna_def_fluidsim_inflow(brna);
	rna_def_fluidsim_outflow(brna);
	rna_def_fluidsim_particle(brna);
	rna_def_fluidsim_control(brna);
}


#endif
