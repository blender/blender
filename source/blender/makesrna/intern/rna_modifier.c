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
 * Contributor(s): Blender Foundation (2008), Juho Veps‰l‰inen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"

#include "BKE_bmesh.h" /* For BevelModifierData */
#include "BKE_smoke.h" /* For smokeModifier_free & smokeModifier_createType */

#include "WM_api.h"
#include "WM_types.h"

EnumPropertyItem modifier_type_items[] ={
	{0, "", 0, "Generate", ""},
	{eModifierType_Array, "ARRAY", ICON_MOD_ARRAY, "Array", ""},
	{eModifierType_Bevel, "BEVEL", ICON_MOD_BEVEL, "Bevel", ""},
	{eModifierType_Boolean, "BOOLEAN", ICON_MOD_BOOLEAN, "Boolean", ""},
	{eModifierType_Build, "BUILD", ICON_MOD_BUILD, "Build", ""},
	{eModifierType_Decimate, "DECIMATE", ICON_MOD_DECIM, "Decimate", ""},
	{eModifierType_EdgeSplit, "EDGE_SPLIT", ICON_MOD_EDGESPLIT, "Edge Split", ""},
	{eModifierType_Mask, "MASK", ICON_MOD_MASK, "Mask", ""},
	{eModifierType_Mirror, "MIRROR", ICON_MOD_MIRROR, "Mirror", ""},
	{eModifierType_Multires, "MULTIRES", ICON_MOD_MULTIRES, "Multiresolution", ""},
	{eModifierType_Subsurf, "SUBSURF", ICON_MOD_SUBSURF, "Subdivision Surface", ""},
	{eModifierType_UVProject, "UV_PROJECT", ICON_MOD_UVPROJECT, "UV Project", ""},
	{0, "", 0, "Deform", ""},
	{eModifierType_Armature, "ARMATURE", ICON_MOD_ARMATURE, "Armature", ""},
	{eModifierType_Cast, "CAST", ICON_MOD_CAST, "Cast", ""},
	{eModifierType_Curve, "CURVE", ICON_MOD_CURVE, "Curve", ""},
	{eModifierType_Displace, "DISPLACE", ICON_MOD_DISPLACE, "Displace", ""},
	{eModifierType_Hook, "HOOK", ICON_HOOK, "Hook", ""},
	{eModifierType_Lattice, "LATTICE", ICON_MOD_LATTICE, "Lattice", ""},
	{eModifierType_MeshDeform, "MESH_DEFORM", ICON_MOD_MESHDEFORM, "Mesh Deform", ""},
	{eModifierType_Shrinkwrap, "SHRINKWRAP", ICON_MOD_SHRINKWRAP, "Shrinkwrap", ""},
	{eModifierType_SimpleDeform, "SIMPLE_DEFORM", ICON_MOD_SIMPLEDEFORM, "Simple Deform", ""},
	{eModifierType_Smooth, "SMOOTH", ICON_MOD_SMOOTH, "Smooth", ""},
	{eModifierType_Wave, "WAVE", ICON_MOD_WAVE, "Wave", ""},
	{0, "", 0, "Physics", ""},
	{eModifierType_Cloth, "CLOTH", ICON_MOD_CLOTH, "Cloth", ""},
	{eModifierType_Collision, "COLLISION", ICON_MOD_PHYSICS, "Collision", ""},
	{eModifierType_Explode, "EXPLODE", ICON_MOD_EXPLODE, "Explode", ""},
	{eModifierType_Fluidsim, "FLUID_SIMULATION", ICON_MOD_FLUIDSIM, "Fluid Simulation", ""},
	{eModifierType_ParticleInstance, "PARTICLE_INSTANCE", ICON_MOD_PARTICLES, "Particle Instance", ""},
	{eModifierType_ParticleSystem, "PARTICLE_SYSTEM", ICON_MOD_PARTICLES, "Particle System", ""},
	{eModifierType_Smoke, "SMOKE", ICON_MOD_SMOKE, "Smoke", ""},
	{eModifierType_Softbody, "SOFT_BODY", ICON_MOD_SOFT, "Soft Body", ""},
	{eModifierType_Surface, "SURFACE", ICON_MOD_PHYSICS, "Surface", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_library.h"

static void rna_UVProject_projectors_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	UVProjectModifierData *uvp= (UVProjectModifierData*)ptr->data;
	rna_iterator_array_begin(iter, (void*)uvp->projectors, sizeof(Object*), uvp->num_projectors, 0, NULL);
}

static StructRNA* rna_Modifier_refine(struct PointerRNA *ptr)
{
	ModifierData *md= (ModifierData*)ptr->data;

	switch(md->type) {
		case eModifierType_Subsurf:
			return &RNA_SubsurfModifier;
		case eModifierType_Lattice:
			return &RNA_LatticeModifier;
		case eModifierType_Curve:
			return &RNA_CurveModifier;
		case eModifierType_Build:
			return &RNA_BuildModifier;
		case eModifierType_Mirror:
			return &RNA_MirrorModifier;
		case eModifierType_Decimate:
			return &RNA_DecimateModifier;
		case eModifierType_Wave:
			return &RNA_WaveModifier;
		case eModifierType_Armature:
			return &RNA_ArmatureModifier;
		case eModifierType_Hook:
			return &RNA_HookModifier;
		case eModifierType_Softbody:
			return &RNA_SoftBodyModifier;
		case eModifierType_Boolean:
			return &RNA_BooleanModifier;
		case eModifierType_Array:
			return &RNA_ArrayModifier;
		case eModifierType_EdgeSplit:
			return &RNA_EdgeSplitModifier;
		case eModifierType_Displace:
			return &RNA_DisplaceModifier;
		case eModifierType_UVProject:
			return &RNA_UVProjectModifier;
		case eModifierType_Smooth:
			return &RNA_SmoothModifier;
		case eModifierType_Cast:
			return &RNA_CastModifier;
		case eModifierType_MeshDeform:
			return &RNA_MeshDeformModifier;
		case eModifierType_ParticleSystem:
			return &RNA_ParticleSystemModifier;
		case eModifierType_ParticleInstance:
			return &RNA_ParticleInstanceModifier;
		case eModifierType_Explode:
			return &RNA_ExplodeModifier;
		case eModifierType_Cloth:
			return &RNA_ClothModifier;
		case eModifierType_Collision:
			return &RNA_CollisionModifier;
		case eModifierType_Bevel:
			return &RNA_BevelModifier;
		case eModifierType_Shrinkwrap:
			return &RNA_ShrinkwrapModifier;
		case eModifierType_Fluidsim:
			return &RNA_FluidSimulationModifier;
		case eModifierType_Mask:
			return &RNA_MaskModifier;
		case eModifierType_SimpleDeform:
			return &RNA_SimpleDeformModifier;
		case eModifierType_Multires:
			return &RNA_MultiresModifier;
		case eModifierType_Surface:
			return &RNA_SurfaceModifier;
		case eModifierType_Smoke:
			return &RNA_SmokeModifier;
		default:
			return &RNA_Modifier;
	}
}

static char *rna_Modifier_path(PointerRNA *ptr)
{
	return BLI_sprintfN("modifiers[%s]", ((ModifierData*)ptr->data)->name);  // XXX not unique
}

static void rna_Modifier_update(bContext *C, PointerRNA *ptr)
{
	DAG_id_flush_update(ptr->id.data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ptr->id.data);
}

static void rna_Modifier_dependency_update(bContext *C, PointerRNA *ptr)
{
	rna_Modifier_update(C, ptr);
    DAG_scene_sort(CTX_data_scene(C));
}

static void rna_Smoke_set_type(bContext *C, PointerRNA *ptr)
{
	SmokeModifierData *smd= (SmokeModifierData *)ptr->data;
	Object *ob= (Object*)ptr->id.data;

	// nothing changed
	if((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain)
		return;
		
	smokeModifier_free(smd); // XXX TODO: completely free all 3 pointers
	smokeModifier_createType(smd); // create regarding of selected type

	if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
		ob->dt = OB_WIRE;
	
	// update dependancy since a domain - other type switch could have happened
	rna_Modifier_dependency_update(C, ptr);
}

static void rna_ExplodeModifier_vgroup_get(PointerRNA *ptr, char *value)
{
	ExplodeModifierData *emd= (ExplodeModifierData*)ptr->data;
	rna_object_vgroup_name_index_get(ptr, value, emd->vgroup);
}

static int rna_ExplodeModifier_vgroup_length(PointerRNA *ptr)
{
	ExplodeModifierData *emd= (ExplodeModifierData*)ptr->data;
	return rna_object_vgroup_name_index_length(ptr, emd->vgroup);
}

static void rna_ExplodeModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	ExplodeModifierData *emd= (ExplodeModifierData*)ptr->data;
	rna_object_vgroup_name_index_set(ptr, value, &emd->vgroup);
}

static void rna_SimpleDeformModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	SimpleDeformModifierData *smd= (SimpleDeformModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, smd->vgroup_name, sizeof(smd->vgroup_name));
}

static void rna_ShrinkwrapModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	ShrinkwrapModifierData *smd= (ShrinkwrapModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, smd->vgroup_name, sizeof(smd->vgroup_name));
}

static void rna_LatticeModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	LatticeModifierData *lmd= (LatticeModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->name, sizeof(lmd->name));
}

static void rna_ArmatureModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	ArmatureModifierData *lmd= (ArmatureModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->defgrp_name, sizeof(lmd->defgrp_name));
}

static void rna_CurveModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	CurveModifierData *lmd= (CurveModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->name, sizeof(lmd->name));
}

static void rna_DisplaceModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	DisplaceModifierData *lmd= (DisplaceModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->defgrp_name, sizeof(lmd->defgrp_name));
}

static void rna_HookModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	HookModifierData *lmd= (HookModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->name, sizeof(lmd->name));
}

static void rna_MaskModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	MaskModifierData *lmd= (MaskModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->vgroup, sizeof(lmd->vgroup));
}

static void rna_MeshDeformModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	MeshDeformModifierData *lmd= (MeshDeformModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->defgrp_name, sizeof(lmd->defgrp_name));
}

static void rna_SmoothModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	SmoothModifierData *lmd= (SmoothModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->defgrp_name, sizeof(lmd->defgrp_name));
}

static void rna_WaveModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	WaveModifierData *lmd= (WaveModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->defgrp_name, sizeof(lmd->defgrp_name));
}

static void rna_CastModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	CastModifierData *lmd= (CastModifierData*)ptr->data;
	rna_object_vgroup_name_set(ptr, value, lmd->defgrp_name, sizeof(lmd->defgrp_name));
}

static void rna_DisplaceModifier_uvlayer_set(PointerRNA *ptr, const char *value)
{
	DisplaceModifierData *smd= (DisplaceModifierData*)ptr->data;
	rna_object_uvlayer_name_set(ptr, value, smd->uvlayer_name, sizeof(smd->uvlayer_name));
}

static void rna_UVProjectModifier_uvlayer_set(PointerRNA *ptr, const char *value)
{
	UVProjectModifierData *umd= (UVProjectModifierData*)ptr->data;
	rna_object_uvlayer_name_set(ptr, value, umd->uvlayer_name, sizeof(umd->uvlayer_name));
}

static void rna_WaveModifier_uvlayer_set(PointerRNA *ptr, const char *value)
{
	WaveModifierData *wmd= (WaveModifierData*)ptr->data;
	rna_object_uvlayer_name_set(ptr, value, wmd->uvlayer_name, sizeof(wmd->uvlayer_name));
}

static void rna_MultiresModifier_level_range(PointerRNA *ptr, int *min, int *max)
{
	MultiresModifierData *mmd = (MultiresModifierData*)ptr->data;

	*min = 1;
	*max = mmd->totlvl;
}

static void modifier_object_set(Object **ob_p, int type, PointerRNA value)
{
	Object *ob= value.data;

	if(!ob || ob->type == type)
		*ob_p= ob;
}

static void rna_LatticeModifier_object_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((LatticeModifierData*)ptr->data)->object, OB_LATTICE, value);
}

static void rna_BooleanModifier_object_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((BooleanModifierData*)ptr->data)->object, OB_MESH, value);
}

static void rna_CurveModifier_object_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((CurveModifierData*)ptr->data)->object, OB_CURVE, value);
}

static void rna_CastModifier_object_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((CastModifierData*)ptr->data)->object, OB_MESH, value);
}

static void rna_ArmatureModifier_object_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((ArmatureModifierData*)ptr->data)->object, OB_ARMATURE, value);
}

static void rna_MaskModifier_armature_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((MaskModifierData*)ptr->data)->ob_arm, OB_ARMATURE, value);
}

static void rna_ShrinkwrapModifier_auxiliary_target_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((ShrinkwrapModifierData*)ptr->data)->auxTarget, OB_MESH, value);
}

static void rna_ShrinkwrapModifier_target_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((ShrinkwrapModifierData*)ptr->data)->target, OB_MESH, value);
}

static void rna_MeshDeformModifier_object_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((MeshDeformModifierData*)ptr->data)->object, OB_MESH, value);
}

static void rna_ArrayModifier_end_cap_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((ArrayModifierData*)ptr->data)->end_cap, OB_MESH, value);
}

static void rna_ArrayModifier_start_cap_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((ArrayModifierData*)ptr->data)->start_cap, OB_MESH, value);
}

static void rna_ArrayModifier_curve_set(PointerRNA *ptr, PointerRNA value)
{
	modifier_object_set(&((ArrayModifierData*)ptr->data)->curve_ob, OB_CURVE, value);
}

static int rna_MeshDeformModifier_is_bound_get(PointerRNA *ptr)
{
	return (((MeshDeformModifierData*)ptr->data)->bindcos != NULL);
}

static PointerRNA rna_SoftBodyModifier_settings_get(PointerRNA *ptr)
{
	Object *ob= (Object*)ptr->id.data;
	return rna_pointer_inherit_refine(ptr, &RNA_SoftBodySettings, ob->soft);
}

static PointerRNA rna_SoftBodyModifier_point_cache_get(PointerRNA *ptr)
{
	Object *ob= (Object*)ptr->id.data;
	return rna_pointer_inherit_refine(ptr, &RNA_PointCache, ob->soft->pointcache);
}

static PointerRNA rna_CollisionModifier_settings_get(PointerRNA *ptr)
{
	Object *ob= (Object*)ptr->id.data;
	return rna_pointer_inherit_refine(ptr, &RNA_CollisionSettings, ob->pd);
}

static PointerRNA rna_UVProjector_object_get(PointerRNA *ptr)
{
	Object **ob= (Object**)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Object, *ob);
}

static void rna_UVProjector_object_set(PointerRNA *ptr, PointerRNA value)
{
	Object **ob= (Object**)ptr->data;

    if(*ob)
		id_us_min((ID*)*ob);
	if(value.data)
		id_us_plus((ID*)value.data);

	*ob= value.data;
}

static void rna_UVProjectModifier_num_projectors_set(PointerRNA *ptr, int value)
{
	UVProjectModifierData *md= (UVProjectModifierData*)ptr->data;
	int a;

	md->num_projectors= CLAMPIS(value, 1, MOD_UVPROJECT_MAX);
	for(a=md->num_projectors; a<MOD_UVPROJECT_MAX; a++)
		md->projectors[a]= NULL;
}

#else

static void rna_def_property_subdivision_common(StructRNA *srna, const char type[])
{
	static EnumPropertyItem prop_subdivision_type_items[] = {
		{0, "CATMULL_CLARK", 0, "Catmull-Clark", ""},
		{1, "SIMPLE", 0, "Simple", ""},
		{0, NULL, 0, NULL, NULL}};

	PropertyRNA *prop= RNA_def_property(srna, "subdivision_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, type);
	RNA_def_property_enum_items(prop, prop_subdivision_type_items);
	RNA_def_property_ui_text(prop, "Subdivision Type", "Selects type of subdivision algorithm.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_subsurf(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SubsurfModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Subsurf Modifier", "Subdivision surface modifier.");
	RNA_def_struct_sdna(srna, "SubsurfModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SUBSURF);

	rna_def_property_subdivision_common(srna, "subdivType");

	prop= RNA_def_property(srna, "levels", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "levels");
	RNA_def_property_range(prop, 1, 6);
	RNA_def_property_ui_range(prop, 1, 6, 1, 0);
	RNA_def_property_ui_text(prop, "Levels", "Number of subdivisions to perform.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "render_levels", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "renderLevels");
	RNA_def_property_range(prop, 1, 6);
	RNA_def_property_ui_range(prop, 1, 6, 1, 0);
	RNA_def_property_ui_text(prop, "Render Levels", "Number of subdivisions to perform when rendering.");

	prop= RNA_def_property(srna, "optimal_draw", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", eSubsurfModifierFlag_ControlEdges);
	RNA_def_property_ui_text(prop, "Optimal Draw", "Skip drawing/rendering of interior subdivided edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "subsurf_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", eSubsurfModifierFlag_SubsurfUv);
	RNA_def_property_ui_text(prop, "Subsurf UV", "Use subsurf to subdivide UVs.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_multires(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MultiresModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Multires Modifier", "Multiresolution mesh modifier.");
	RNA_def_struct_sdna(srna, "MultiresModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MULTIRES);

	rna_def_property_subdivision_common(srna, "simple");

	prop= RNA_def_property(srna, "level", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "lvl");
	RNA_def_property_ui_text(prop, "Level", "");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MultiresModifier_level_range");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_lattice(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "LatticeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Lattice Modifier", "Lattice deformation modifier.");
	RNA_def_struct_sdna(srna, "LatticeModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_LATTICE);

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Lattice object to deform with.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_LatticeModifier_object_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_LatticeModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_curve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_deform_axis_items[] = {
		{MOD_CURVE_POSX, "POS_X", 0, "X", ""},
		{MOD_CURVE_POSY, "POS_Y", 0, "Y", ""},
		{MOD_CURVE_POSZ, "POS_Z", 0, "Z", ""},
		{MOD_CURVE_NEGX, "NEG_X", 0, "-X", ""},
		{MOD_CURVE_NEGY, "NEG_Y", 0, "-Y", ""},
		{MOD_CURVE_NEGZ, "NEG_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "CurveModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Curve Modifier", "Curve deformation modifier.");
	RNA_def_struct_sdna(srna, "CurveModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_CURVE);

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Curve object to deform with.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_CurveModifier_object_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CurveModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "deform_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "defaxis");
	RNA_def_property_enum_items(prop, prop_deform_axis_items);
	RNA_def_property_ui_text(prop, "Deform Axis", "The axis that the curve deforms along.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_build(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "BuildModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Build Modifier", "Build effect modifier.");
	RNA_def_struct_sdna(srna, "BuildModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_BUILD);

	prop= RNA_def_property(srna, "start", PROP_FLOAT, PROP_TIME);
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Start", "Specify the start frame of the effect.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "length", PROP_FLOAT, PROP_TIME);
	RNA_def_property_range(prop, 1, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Length", "Specify the total time the build effect requires");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "randomize", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Randomize", "Randomize the faces or edges during build.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Seed", "Specify the seed for random if used.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_mirror(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MirrorModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Mirror Modifier", "Mirroring modifier.");
	RNA_def_struct_sdna(srna, "MirrorModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MIRROR);

	prop= RNA_def_property(srna, "x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_AXIS_X);
	RNA_def_property_ui_text(prop, "X", "Enable X axis mirror.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_AXIS_Y);
	RNA_def_property_ui_text(prop, "Y", "Enable Y axis mirror.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_AXIS_Z);
	RNA_def_property_ui_text(prop, "Z", "Enable Z axis mirror.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "clip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_CLIPPING);
	RNA_def_property_ui_text(prop, "Clip", "Prevents vertices from going through the mirror during transform.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "mirror_vertex_groups", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_VGROUP);
	RNA_def_property_ui_text(prop, "Mirror Vertex Groups", "Mirror vertex groups (e.g. .R->.L).");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "mirror_u", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_MIRROR_U);
	RNA_def_property_ui_text(prop, "Mirror U", "Mirror the U texture coordinate around the 0.5 point.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "mirror_v", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_MIRROR_V);
	RNA_def_property_ui_text(prop, "Mirror V", "Mirror the V texture coordinate around the 0.5 point.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "merge_limit", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tolerance");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 10, 3); 
	RNA_def_property_ui_text(prop, "Merge Limit", "Distance from axis within which mirrored vertices are merged.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "mirror_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mirror_ob");
	RNA_def_property_ui_text(prop, "Mirror Object", "Object to use as mirror.");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
}

static void rna_def_modifier_decimate(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "DecimateModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Decimate Modifier", "Decimation modifier.");
	RNA_def_struct_sdna(srna, "DecimateModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_DECIM);

	prop= RNA_def_property(srna, "ratio", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "percent");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Ratio", "Defines the ratio of triangles to reduce to.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "face_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "faceCount");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Face Count", "The current number of faces in the decimated mesh.");
}

static void rna_def_modifier_wave(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_texture_coordinates_items[] = {
		{MOD_WAV_MAP_LOCAL, "LOCAL", 0, "Local", ""},
		{MOD_WAV_MAP_GLOBAL, "GLOBAL", 0, "Global", ""},
		{MOD_WAV_MAP_OBJECT, "OBJECT", 0, "Object", ""},
		{MOD_WAV_MAP_UV, "MAP_UV", 0, "UV", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "WaveModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Wave Modifier", "Wave effect modifier.");
	RNA_def_struct_sdna(srna, "WaveModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_WAVE);

	prop= RNA_def_property(srna, "x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_X);
	RNA_def_property_ui_text(prop, "X", "X axis motion.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_Y);
	RNA_def_property_ui_text(prop, "Y", "Y axis motion.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "cyclic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_CYCL);
	RNA_def_property_ui_text(prop, "Cyclic", "Cyclic wave effect.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "normals", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM);
	RNA_def_property_ui_text(prop, "Normals", "Dispace along normals.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "x_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_X);
	RNA_def_property_ui_text(prop, "X Normal", "Enable displacement along the X normal");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "y_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_Y);
	RNA_def_property_ui_text(prop, "Y Normal", "Enable displacement along the Y normal");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "z_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_Z);
	RNA_def_property_ui_text(prop, "Z Normal", "Enable displacement along the Z normal");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "time_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "timeoffs");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Time Offset", "Either the starting frame (for positive speed) or ending frame (for negative speed.)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_TIME);
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Lifetime",  "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "damping_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "damp");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Damping Time",  "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "falloff");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Falloff Radius",  "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "start_position_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "startx");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Start Position X",  "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "start_position_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "starty");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Start Position Y",  "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "start_position_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "objectcenter");
	RNA_def_property_ui_text(prop, "Start Position Object", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the wave.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WaveModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Texture", "Texture for modulating the wave.");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "texture_coordinates", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texmapping");
	RNA_def_property_enum_items(prop, prop_texture_coordinates_items);
	RNA_def_property_ui_text(prop, "Texture Coordinates", "Texture coordinates used for modulating input.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Layer", "UV layer name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WaveModifier_uvlayer_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "texture_coordinates_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "map_object");
	RNA_def_property_ui_text(prop, "Texture Coordinates Object", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -1, 1, 10, 2);
	RNA_def_property_ui_text(prop, "Speed", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -2, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Height", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 5, 10, 2);
	RNA_def_property_ui_text(prop, "Width", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "narrowness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "narrow");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 10, 10, 2);
	RNA_def_property_ui_text(prop, "Narrowness", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_armature(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ArmatureModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Armature Modifier", "Armature deformation modifier.");
	RNA_def_struct_sdna(srna, "ArmatureModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_ARMATURE);

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Armature object to deform with.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ArmatureModifier_object_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ArmatureModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "use_vertex_groups", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_VGROUP);
	RNA_def_property_ui_text(prop, "Use Vertex Groups", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "use_bone_envelopes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_ENVELOPE);
	RNA_def_property_ui_text(prop, "Use Bone Envelopes", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "quaternion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_QUATERNION);
	RNA_def_property_ui_text(prop, "Quaternion", "Deform rotation interpolation with quaternions.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "b_bone_rest", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_B_BONE_REST);
	RNA_def_property_ui_text(prop, "B-Bone Rest",  "Make B-Bones deform already in rest position");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "multi_modifier", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "multi", 0);
	RNA_def_property_ui_text(prop, "Multi Modifier",  "Use same input as previous modifier, and mix results using overall vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_hook(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "HookModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Hook Modifier", "Hook modifier to modify the location of vertices.");
	RNA_def_struct_sdna(srna, "HookModifierData");
	RNA_def_struct_ui_icon(srna, ICON_HOOK);

	prop= RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Falloff",  "If not zero, the distance from the hook where influence ends.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "force", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Force",  "Relative force of the hook.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Parent Object for hook, also recalculates and clears offset");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
	
	prop= RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target", "Name of Parent Bone for hook (if applicable), also recalculates and clears offset");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_HookModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_softbody(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SoftBodyModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Soft Body Modifier", "Soft body simulation modifier.");
	RNA_def_struct_sdna(srna, "SoftbodyModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SOFT);

	prop= RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "SoftBodySettings");
	RNA_def_property_pointer_funcs(prop, "rna_SoftBodyModifier_settings_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Soft Body Settings", "");

	prop= RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "PointCache");
	RNA_def_property_pointer_funcs(prop, "rna_SoftBodyModifier_point_cache_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Soft Body Point Cache", "");
}

static void rna_def_modifier_boolean(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_operation_items[] = {
		{eBooleanModifierOp_Intersect, "INTERSECT", 0, "Intersect", ""},
		{eBooleanModifierOp_Union, "UNION", 0, "Union", ""},
		{eBooleanModifierOp_Difference, "DIFFERENCE", 0, "Difference", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "BooleanModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Boolean Modifier", "Boolean operations modifier.");
	RNA_def_struct_sdna(srna, "BooleanModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_BOOLEAN);

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Mesh object to use for boolean operation.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_BooleanModifier_object_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_operation_items);
	RNA_def_property_ui_text(prop, "Operation", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_array(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_fit_type_items[] = {
		{MOD_ARR_FIXEDCOUNT, "FIXED_COUNT", 0, "Fixed Count", ""},
		{MOD_ARR_FITLENGTH, "FIT_LENGTH", 0, "Fit Length", ""},
		{MOD_ARR_FITCURVE, "FIT_CURVE", 0, "Fit Curve", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "ArrayModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Array Modifier", "Array duplication modifier.");
	RNA_def_struct_sdna(srna, "ArrayModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_ARRAY);

	/* Length parameters */
	prop= RNA_def_property(srna, "fit_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_fit_type_items);
	RNA_def_property_ui_text(prop, "Fit Type", "Array length calculation method.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "count", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_range(prop, 1, 1000, 1, 0);
	RNA_def_property_ui_text(prop, "Count",  "Number of duplicates to make.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "length", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
	RNA_def_property_ui_text(prop, "Length", "Length to fit array within.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curve_ob");
	RNA_def_property_ui_text(prop, "Curve", "Curve object to fit array length to.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ArrayModifier_curve_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	/* Offset parameters */
	prop= RNA_def_property(srna, "constant_offset", PROP_BOOLEAN, PROP_TRANSLATION);
	RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_CONST);
	RNA_def_property_ui_text(prop, "Constant Offset", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "constant_offset_displacement", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_ui_text(prop, "Constant Offset Displacement", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "relative_offset", PROP_BOOLEAN, PROP_TRANSLATION);
	RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_RELATIVE);
	RNA_def_property_ui_text(prop, "Relative Offset", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "relative_offset_displacement", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "scale");
	RNA_def_property_ui_text(prop, "Relative Offset Displacement", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Vertex merging parameters */
	prop= RNA_def_property(srna, "merge_adjacent_vertices", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_ARR_MERGE);
	RNA_def_property_ui_text(prop, "Merge Vertices", "Merge vertices in adjacent duplicates.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "merge_end_vertices", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_ARR_MERGEFINAL);
	RNA_def_property_ui_text(prop, "Merge Vertices", "Merge vertices in first and last duplicates.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "merge_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "merge_dist");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 1, 4);
	RNA_def_property_ui_text(prop, "Merge Distance", "Limit below which to merge vertices.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Offset object */
	prop= RNA_def_property(srna, "add_offset_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_OBJ);
	RNA_def_property_ui_text(prop, "Add Offset Object", "Add an object transformation to the total offset.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "offset_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "offset_ob");
	RNA_def_property_ui_text(prop, "Offset Object", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
	
	/* Caps */
	prop= RNA_def_property(srna, "start_cap", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Start Cap", "Mesh object to use as a start cap.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ArrayModifier_start_cap_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "end_cap", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "End Cap", "Mesh object to use as an end cap.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ArrayModifier_end_cap_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
}

static void rna_def_modifier_edgesplit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "EdgeSplitModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "EdgeSplit Modifier", "Edge splitting modifier to create sharp edges.");
	RNA_def_struct_sdna(srna, "EdgeSplitModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_EDGESPLIT);

	prop= RNA_def_property(srna, "split_angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, 180);
	RNA_def_property_ui_range(prop, 0, 180, 100, 2);
	RNA_def_property_ui_text(prop, "Split Angle", "Angle above which to split edges.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "use_edge_angle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_EDGESPLIT_FROMANGLE);
	RNA_def_property_ui_text(prop, "Use Edge Angle", "Split edges with high angle between faces.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "use_sharp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_EDGESPLIT_FROMFLAG);
	RNA_def_property_ui_text(prop, "Use Sharp Edges", "Split edges that are marked as sharp.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_displace(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_direction_items[] = {
		{MOD_DISP_DIR_X, "X", 0, "X", ""},
		{MOD_DISP_DIR_Y, "Y", 0, "Y", ""},
		{MOD_DISP_DIR_Z, "Z", 0, "Z", ""},
		{MOD_DISP_DIR_NOR, "NORMAL", 0, "Normal", ""},
		{MOD_DISP_DIR_RGB_XYZ, "RGB_TO_XYZ", 0, "RGB to XYZ", ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_texture_coordinates_items[] = {
		{MOD_DISP_MAP_LOCAL, "LOCAL", 0, "Map", ""},
		{MOD_DISP_MAP_GLOBAL, "GLOBAL", 0, "Global", ""},
		{MOD_DISP_MAP_OBJECT, "OBJECT", 0, "Object", ""},
		{MOD_DISP_MAP_UV, "UV", 0, "UV", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "DisplaceModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Displace Modifier", "Displacement modifier.");
	RNA_def_struct_sdna(srna, "DisplaceModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_DISPLACE);

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DisplaceModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Texture", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "midlevel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Midlevel", "Material value that gives no displacement.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100, 100, 10, 2);
	RNA_def_property_ui_text(prop, "Strength", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_direction_items);
	RNA_def_property_ui_text(prop, "Direction", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "texture_coordinates", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texmapping");
	RNA_def_property_enum_items(prop, prop_texture_coordinates_items);
	RNA_def_property_ui_text(prop, "Texture Coordinates", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Layer", "UV layer name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DisplaceModifier_uvlayer_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "texture_coordinate_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "map_object");
	RNA_def_property_ui_text(prop, "Texture Coordinate Object", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
}

static void rna_def_modifier_uvproject(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "UVProjectModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "UV Project Modifier", "UV projection modifier to sets UVs from a projector.");
	RNA_def_struct_sdna(srna, "UVProjectModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_UVPROJECT);

	prop= RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Layer", "UV layer name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_UVProjectModifier_uvlayer_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "num_projectors", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Number of Projectors", "Number of projectors to use.");
	RNA_def_property_int_funcs(prop, NULL, "rna_UVProjectModifier_num_projectors_set", NULL);
	RNA_def_property_range(prop, 1, MOD_UVPROJECT_MAX);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "projectors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "UVProjector");
	RNA_def_property_collection_funcs(prop, "rna_UVProject_projectors_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, 0, 0, 0, 0);
	RNA_def_property_ui_text(prop, "Projectors", "");

	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "horizontal_aspect_ratio", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aspectx");
	RNA_def_property_range(prop, 1, FLT_MAX);
	RNA_def_property_ui_range(prop, 1, 1000, 100, 2);
	RNA_def_property_ui_text(prop, "Horizontal Aspect Ratio", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "vertical_aspect_ratio", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aspecty");
	RNA_def_property_range(prop, 1, FLT_MAX);
	RNA_def_property_ui_range(prop, 1, 1000, 100, 2);
	RNA_def_property_ui_text(prop, "Vertical Aspect Ratio", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "override_image", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_UVPROJECT_OVERRIDEIMAGE);
	RNA_def_property_ui_text(prop, "Override Image", "Override faces' current images with the given image.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	srna= RNA_def_struct(brna, "UVProjector", NULL);
	RNA_def_struct_ui_text(srna, "UVProjector", "UV projector used by the UV project modifier.");

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, "rna_UVProjector_object_get", "rna_UVProjector_object_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object to use as projector transform.");
}

static void rna_def_modifier_smooth(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SmoothModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Smooth Modifier", "Smoothing effect modifier.");
	RNA_def_struct_sdna(srna, "SmoothModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

	prop= RNA_def_property(srna, "x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_X);
	RNA_def_property_ui_text(prop, "X", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_Y);
	RNA_def_property_ui_text(prop, "Y", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_Z);
	RNA_def_property_ui_text(prop, "Z", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10, 10, 0.5, 2);
	RNA_def_property_ui_text(prop, "Factor", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "repeat", PROP_INT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 30, 1, 0);
	RNA_def_property_ui_text(prop, "Repeat", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SmoothModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_cast(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_cast_type_items[] = {
		{MOD_CAST_TYPE_SPHERE, "SPHERE", 0, "Sphere", ""},
		{MOD_CAST_TYPE_CYLINDER, "CYLINDER", 0, "Cylinder", ""},
		{MOD_CAST_TYPE_CUBOID, "CUBOID", 0, "Cuboid", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "CastModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Cast Modifier", "Cast modifier to cast to other shapes.");
	RNA_def_struct_sdna(srna, "CastModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_CAST);

	prop= RNA_def_property(srna, "cast_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_cast_type_items);
	RNA_def_property_ui_text(prop, "Cast Type", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Control object: if available, its location determines the center of the effect");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_CastModifier_object_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_X);
	RNA_def_property_ui_text(prop, "X", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_Y);
	RNA_def_property_ui_text(prop, "Y", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_Z);
	RNA_def_property_ui_text(prop, "Z", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "from_radius", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_SIZE_FROM_RADIUS);
	RNA_def_property_ui_text(prop, "From Radius", "Use radius as size of projection shape (0 = auto)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "use_transform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_USE_OB_TRANSFORM);
	RNA_def_property_ui_text(prop, "Use transform", "Use object transform to control projection shape");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10, 10, 5, 2);
	RNA_def_property_ui_text(prop, "Factor", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 10, 2);
	RNA_def_property_ui_text(prop, "Radius", "Only deform vertices within this distance from the center of the effect (leave as 0 for infinite.)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 10, 2);
	RNA_def_property_ui_text(prop, "Size", "Size of projection shape (leave as 0 for auto.)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CastModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_meshdeform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MeshDeformModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "MeshDeform Modifier", "Mesh deformation modifier to deform with other meshes.");
	RNA_def_struct_sdna(srna, "MeshDeformModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Mesh object to deform with.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_MeshDeformModifier_object_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
	
	prop= RNA_def_property(srna, "is_bound", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MeshDeformModifier_is_bound_get", NULL);
	RNA_def_property_ui_text(prop, "Bound", "Whether geometry has been bound to control cage.");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	prop= RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MDEF_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshDeformModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "precision", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gridsize");
	RNA_def_property_range(prop, 2, 10);
	RNA_def_property_ui_text(prop, "Precision", "The grid size for binding.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "dynamic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MDEF_DYNAMIC_BIND);
	RNA_def_property_ui_text(prop, "Dynamic", "Recompute binding dynamically on top of other deformers (slower and more memory consuming.)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

}

static void rna_def_modifier_particlesystem(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ParticleSystemModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "ParticleSystem Modifier", "Particle system simulation modifier.");
	RNA_def_struct_sdna(srna, "ParticleSystemModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_PARTICLES);
}

static void rna_def_modifier_particleinstance(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem particleinstance_axis[] = {
		{0, "X", 0, "X", ""},
		{1, "Y", 0, "Y", ""},
		{2, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna= RNA_def_struct(brna, "ParticleInstanceModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "ParticleInstance Modifier", "Particle system instancing modifier.");
	RNA_def_struct_sdna(srna, "ParticleInstanceModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_PARTICLES);

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_ui_text(prop, "Object", "Object that has the particle system.");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "particle_system_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "psys");
	RNA_def_property_range(prop, 1, 10);
	RNA_def_property_ui_text(prop, "Particle System Number", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axis");
	RNA_def_property_enum_items(prop, particleinstance_axis);
	RNA_def_property_ui_text(prop, "Axis", "Pole axis for rotation");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Parents);
	RNA_def_property_ui_text(prop, "Normal", "Create instances from normal particles.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Children);
	RNA_def_property_ui_text(prop, "Children", "Create instances from child particles.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "path", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Path);
	RNA_def_property_ui_text(prop, "Path", "Create instances along particle paths.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "unborn", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Unborn);
	RNA_def_property_ui_text(prop, "Unborn", "Show instances when particles are unborn.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "alive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Alive);
	RNA_def_property_ui_text(prop, "Alive", "Show instances when particles are alive.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "dead", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Dead);
	RNA_def_property_ui_text(prop, "Dead", "Show instances when particles are dead.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "keep_shape", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_KeepShape);
	RNA_def_property_ui_text(prop, "Keep Shape", "Don't stretch the object.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_UseSize);
	RNA_def_property_ui_text(prop, "Size", "Use particle size to scale the instances.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "position", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "position");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Position", "Position along path.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "random_position", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "random_position");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Random Position", "Randomize position along path.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_explode(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ExplodeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Explode Modifier", "Explosion effect modifier based on a particle system.");
	RNA_def_struct_sdna(srna, "ExplodeModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_EXPLODE);

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ExplodeModifier_vgroup_get", "rna_ExplodeModifier_vgroup_length", "rna_ExplodeModifier_vgroup_set");
	RNA_def_property_ui_text(prop, "Vertex Group", "");

	prop= RNA_def_property(srna, "protect", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Protect", "Clean vertex group edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "split_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_EdgeSplit);
	RNA_def_property_ui_text(prop, "Split Edges", "Split face edges for nicer shrapnel.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "unborn", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Unborn);
	RNA_def_property_ui_text(prop, "Unborn", "Show mesh when particles are unborn.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "alive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Alive);
	RNA_def_property_ui_text(prop, "Alive", "Show mesh when particles are alive.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "dead", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Dead);
	RNA_def_property_ui_text(prop, "Dead", "Show mesh when particles are dead.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_cloth(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ClothModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Cloth Modifier", "Cloth simulation modifier.");
	RNA_def_struct_sdna(srna, "ClothModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_CLOTH);
	
	prop= RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "sim_parms");
	RNA_def_property_ui_text(prop, "Cloth Settings", "");
	
	prop= RNA_def_property(srna, "collision_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "coll_parms");
	RNA_def_property_ui_text(prop, "Cloth Collision Settings", "");
	
	prop= RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Point Cache", "");
}

static void rna_def_modifier_smoke(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_smoke_type_items[] = {
			{0, "NONE", 0, "None", ""},
			{MOD_SMOKE_TYPE_DOMAIN, "TYPE_DOMAIN", 0, "Domain", ""},
			{MOD_SMOKE_TYPE_FLOW, "TYPE_FLOW", 0, "Flow", "Inflow/Outflow"},
			{MOD_SMOKE_TYPE_COLL, "TYPE_COLL", 0, "Collision", ""},
			{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "SmokeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Smoke Modifier", "Smoke simulation modifier.");
	RNA_def_struct_sdna(srna, "SmokeModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SMOKE);
	
	prop= RNA_def_property(srna, "domain_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "domain");
	RNA_def_property_ui_text(prop, "Domain Settings", "");
	
	prop= RNA_def_property(srna, "flow_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "flow");
	RNA_def_property_ui_text(prop, "Flow Settings", "");
	
	prop= RNA_def_property(srna, "coll_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "coll");
	RNA_def_property_ui_text(prop, "Collision Settings", "");
	
	prop= RNA_def_property(srna, "smoke_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_smoke_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, 0, "rna_Smoke_set_type");
}

static void rna_def_modifier_collision(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "CollisionModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Collision Modifier", "Collision modifier defining modifier stack position used for collision.");
	RNA_def_struct_sdna(srna, "CollisionModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_PHYSICS);

	prop= RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "CollisionSettings");
	RNA_def_property_pointer_funcs(prop, "rna_CollisionModifier_settings_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Settings", "");
	
	prop= RNA_def_property(srna, "absorption", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "absorption");
	RNA_def_property_ui_range(prop, 0, 100, 1, 2);
	RNA_def_property_ui_text(prop, "Absorption %", "How much of effector force gets lost during collision with this object (in percent).");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_bevel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_limit_method_items[] = {
		{0, "NONE", 0, "None", "Bevel the entire mesh by a constant amount."},
		{BME_BEVEL_ANGLE, "ANGLE", 0, "Angle", "Only bevel edges with sharp enough angles between faces."},
		{BME_BEVEL_WEIGHT, "WEIGHT", 0, "Weight", "Use bevel weights to determine how much bevel is applied; apply them separately in vert/edge select mode."},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_edge_weight_method_items[] = {
		{0, "AVERAGE", 0, "Average", ""},
		{BME_BEVEL_EMIN, "SHARPEST", 0, "Sharpest", ""},
		{BME_BEVEL_EMAX, "LARGEST", 0, "Largest", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "BevelModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Bevel Modifier", "Bevel modifier to make edges and vertices more rounded.");
	RNA_def_struct_sdna(srna, "BevelModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_BEVEL);

	prop= RNA_def_property(srna, "width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_range(prop, 0, 0.5);
	RNA_def_property_ui_text(prop, "Width", "Bevel value/amount.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "only_vertices", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", BME_BEVEL_VERT);
	RNA_def_property_ui_text(prop, "Only Vertices", "Bevel verts/corners, not edges.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "limit_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "lim_flags");
	RNA_def_property_enum_items(prop, prop_limit_method_items);
	RNA_def_property_ui_text(prop, "Limit Method", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "edge_weight_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "e_flags");
	RNA_def_property_enum_items(prop, prop_edge_weight_method_items);
	RNA_def_property_ui_text(prop, "Edge Weight Method", "What edge weight to use for weighting a vertex.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bevel_angle");
	RNA_def_property_range(prop, 0, 180);
	RNA_def_property_ui_range(prop, 0, 180, 100, 2);
	RNA_def_property_ui_text(prop, "Angle", "Angle above which to bevel edges.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_shrinkwrap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_mode_items[] = {
		{MOD_SHRINKWRAP_NEAREST_SURFACE, "NEAREST_SURFACEPOINT", 0, "Nearest Surface Point", ""},
		{MOD_SHRINKWRAP_PROJECT, "PROJECT", 0, "Project", ""},
		{MOD_SHRINKWRAP_NEAREST_VERTEX, "NEAREST_VERTEX", 0, "Nearest Vertex", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "ShrinkwrapModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Shrinkwrap Modifier", "Shrink wrapping modifier to shrink wrap and object to a target.");
	RNA_def_struct_sdna(srna, "ShrinkwrapModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SHRINKWRAP);

	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "shrinkType");
	RNA_def_property_enum_items(prop, prop_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Target", "Mesh target to shrink to.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ShrinkwrapModifier_target_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "auxiliary_target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "auxTarget");
	RNA_def_property_ui_text(prop, "Auxiliary Target", "Additional mesh target to shrink to.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ShrinkwrapModifier_auxiliary_target_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ShrinkwrapModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "keepDist");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 1, 2);
	RNA_def_property_ui_text(prop, "Offset", "Distance to keep from the target.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS);
	RNA_def_property_ui_text(prop, "X", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS);
	RNA_def_property_ui_text(prop, "Y", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS);
	RNA_def_property_ui_text(prop, "Z", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "subsurf_levels", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "subsurfLevels");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_range(prop, 0, 6, 1, 0);
	RNA_def_property_ui_text(prop, "Subsurf Levels", "Number of subdivisions that must be performed before extracting vertices' positions and normals.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "negative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR);
	RNA_def_property_ui_text(prop, "Negative", "Allow vertices to move in the negative direction of axis.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "positive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR);
	RNA_def_property_ui_text(prop, "Positive", "Allow vertices to move in the positive direction of axis.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "cull_front_faces", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE);
	RNA_def_property_ui_text(prop, "Cull Front Faces", "Stop vertices from projecting to a front face on the target.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "cull_back_faces", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_CULL_TARGET_BACKFACE);
	RNA_def_property_ui_text(prop, "Cull Back Faces", "Stop vertices from projecting to a back face on the target.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "keep_above_surface", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE);
	RNA_def_property_ui_text(prop, "Keep Above Surface", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_fluidsim(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "FluidSimulationModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Fluid Simulation Modifier", "Fluid simulation modifier.");
	RNA_def_struct_sdna(srna, "FluidsimModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_FLUIDSIM);

	prop= RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "fss");
	RNA_def_property_ui_text(prop, "Settings", "Settings for how this object is used in the fluid simulation.");
}

static void rna_def_modifier_mask(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_mode_items[] = {
		{MOD_MASK_MODE_VGROUP, "VERTEX_GROUP", 0, "Vertex Group", ""},
		{MOD_MASK_MODE_ARM, "ARMATURE", 0, "Armature", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "MaskModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Mask Modifier", "Mask modifier to hide parts of the mesh.");
	RNA_def_struct_sdna(srna, "MaskModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MASK);

	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "armature", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob_arm");
	RNA_def_property_ui_text(prop, "Armature", "Armature to use as source of bones to mask.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_MaskModifier_armature_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MaskModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "inverse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MASK_INV);
	RNA_def_property_ui_text(prop, "Inverse", "Use vertices that are not part of region defined.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_simpledeform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_mode_items[] = {
		{MOD_SIMPLEDEFORM_MODE_TWIST, "TWIST", 0, "Twist", ""},
		{MOD_SIMPLEDEFORM_MODE_BEND, "BEND", 0, "Bend", ""},
		{MOD_SIMPLEDEFORM_MODE_TAPER, "TAPER", 0, "Taper", ""},
		{MOD_SIMPLEDEFORM_MODE_STRETCH, "STRETCH", 0, "Stretch", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "SimpleDeformModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "SimpleDeform Modifier", "Simple deformation modifier to apply effects such as twisting and bending.");
	RNA_def_struct_sdna(srna, "SimpleDeformModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SIMPLEDEFORM);

	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name.");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SimpleDeformModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "origin", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Origin", "Origin of modifier space coordinates.");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop= RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "originOpts", MOD_SIMPLEDEFORM_ORIGIN_LOCAL);
	RNA_def_property_ui_text(prop, "Relative", "Sets the origin of deform space to be relative to the object.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10, 10, 0.5, 2);
	RNA_def_property_ui_text(prop, "Factor", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "limits", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limit");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 5, 2);
	RNA_def_property_ui_text(prop, "Limits", "Lower/Upper limits for deform.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "lock_x_axis", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "axis", MOD_SIMPLEDEFORM_LOCK_AXIS_X);
	RNA_def_property_ui_text(prop, "Lock X Axis", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop= RNA_def_property(srna, "lock_y_axis", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "axis", MOD_SIMPLEDEFORM_LOCK_AXIS_Y);
	RNA_def_property_ui_text(prop, "Lock Y Axis", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_surface(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "SurfaceModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Surface Modifier", "Surface modifier defining modifier stack position used for surface fields.");
	RNA_def_struct_sdna(srna, "SurfaceModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_PHYSICS);
}
void RNA_def_modifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	/* data */
	srna= RNA_def_struct(brna, "Modifier", NULL);
	RNA_def_struct_ui_text(srna , "Modifier", "Modifier affecting the geometry data of an object.");
	RNA_def_struct_refine_func(srna, "rna_Modifier_refine");
	RNA_def_struct_path_func(srna, "rna_Modifier_path");
	RNA_def_struct_sdna(srna, "ModifierData");
	
	/* strings */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Modifier name.");
	RNA_def_struct_name_property(srna, prop);
	
	/* enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, modifier_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	
	/* flags */
	prop= RNA_def_property(srna, "realtime", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Realtime);
	RNA_def_property_ui_text(prop, "Realtime", "Realtime display of a modifier.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 0);
	
	prop= RNA_def_property(srna, "render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Render);
	RNA_def_property_ui_text(prop, "Render", "Use modifier during rendering.");
	RNA_def_property_ui_icon(prop, ICON_SCENE, 0);
	
	prop= RNA_def_property(srna, "editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Editmode);
	RNA_def_property_ui_text(prop, "Editmode", "Use modifier while in the edit mode.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);
	
	prop= RNA_def_property(srna, "on_cage", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_OnCage);
	RNA_def_property_ui_text(prop, "On Cage", "Enable direct editing of modifier control cage.");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	
	prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Expanded);
	RNA_def_property_ui_text(prop, "Expanded", "Set modifier expanded in the user interface.");


	/* types */
	rna_def_modifier_subsurf(brna);
	rna_def_modifier_lattice(brna);
	rna_def_modifier_curve(brna);
	rna_def_modifier_build(brna);
	rna_def_modifier_mirror(brna);
	rna_def_modifier_decimate(brna);
	rna_def_modifier_wave(brna);
	rna_def_modifier_armature(brna);
	rna_def_modifier_hook(brna);
	rna_def_modifier_softbody(brna);
	rna_def_modifier_boolean(brna);
	rna_def_modifier_array(brna);
	rna_def_modifier_edgesplit(brna);
	rna_def_modifier_displace(brna);
	rna_def_modifier_uvproject(brna);
	rna_def_modifier_smooth(brna);
	rna_def_modifier_cast(brna);
	rna_def_modifier_meshdeform(brna);
	rna_def_modifier_particlesystem(brna);
	rna_def_modifier_particleinstance(brna);
	rna_def_modifier_explode(brna);
	rna_def_modifier_cloth(brna);
	rna_def_modifier_collision(brna);
	rna_def_modifier_bevel(brna);
	rna_def_modifier_shrinkwrap(brna);
	rna_def_modifier_fluidsim(brna);
	rna_def_modifier_mask(brna);
	rna_def_modifier_simpledeform(brna);
	rna_def_modifier_multires(brna);
	rna_def_modifier_surface(brna);
	rna_def_modifier_smoke(brna);
}

#endif
