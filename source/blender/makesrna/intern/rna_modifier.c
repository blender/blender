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
 * Contributor(s): Blender Foundation (2008), Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_modifier_types.h"

#ifdef RNA_RUNTIME

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
			return &RNA_SoftbodyModifier;
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
		default:
			return &RNA_Modifier;
	}
}

#else

static void rna_def_modifier_subsurf(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "SubsurfModifier", "Modifier");
	RNA_def_struct_ui_text(srna , "Subsurf Modifier", "Subsurf Modifier.");
	RNA_def_struct_sdna(srna, "SubsurfModifierData");
}

static void rna_def_modifier_lattice(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "LatticeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Lattice Modifier", "Lattice Modifier.");
	RNA_def_struct_sdna(srna, "LatticeModifierData");
}

static void rna_def_modifier_curve(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "CurveModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Curve Modifier", "Curve Modifier.");
	RNA_def_struct_sdna(srna, "CurveModifierData");
}

static void rna_def_modifier_build(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "BuildModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Build Modifier", "Build Modifier.");
	RNA_def_struct_sdna(srna, "BuildModifierData");
}

static void rna_def_modifier_mirror(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "MirrorModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Mirror Modifier", "Mirror Modifier.");
	RNA_def_struct_sdna(srna, "MirrorModifierData");
}

static void rna_def_modifier_decimate(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "DecimateModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Decimate Modifier", "Decimate Modifier.");
	RNA_def_struct_sdna(srna, "DecimateModifierData");
}

static void rna_def_modifier_wave(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "WaveModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Wave Modifier", "Wave Modifier.");
	RNA_def_struct_sdna(srna, "WaveModifierData");
}

static void rna_def_modifier_armature(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ArmatureModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Armature Modifier", "Armature Modifier.");
	RNA_def_struct_sdna(srna, "ArmatureModifierData");
}

static void rna_def_modifier_hook(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "HookModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Hook Modifier", "Hook Modifier.");
	RNA_def_struct_sdna(srna, "HookModifierData");
}

static void rna_def_modifier_softbody(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "SoftbodyModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Softbody Modifier", "Softbody Modifier.");
	RNA_def_struct_sdna(srna, "SoftbodyModifierData");
}

static void rna_def_modifier_boolean(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "BooleanModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Boolean Modifier", "Boolean Modifier.");
	RNA_def_struct_sdna(srna, "BooleanModifierData");
}

static void rna_def_modifier_array(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ArrayModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Array Modifier", "Array Modifier.");
	RNA_def_struct_sdna(srna, "ArrayModifierData");
}

static void rna_def_modifier_edgesplit(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "EdgeSplitModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "EdgeSplit Modifier", "EdgeSplit Modifier.");
	RNA_def_struct_sdna(srna, "EdgeSplitModifierData");
}

static void rna_def_modifier_displace(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "DisplaceModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Displace Modifier", "Displace Modifier.");
	RNA_def_struct_sdna(srna, "DisplaceModifierData");
}

static void rna_def_modifier_uvproject(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "UVProjectModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "UVProject Modifier", "UVProject Modifier.");
	RNA_def_struct_sdna(srna, "UVProjectModifierData");
}

static void rna_def_modifier_smooth(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "SmoothModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Smooth Modifier", "Smooth Modifier.");
	RNA_def_struct_sdna(srna, "SmoothModifierData");
}

static void rna_def_modifier_cast(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "CastModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Cast Modifier", "Cast Modifier.");
	RNA_def_struct_sdna(srna, "CastModifierData");
}

static void rna_def_modifier_meshdeform(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "MeshDeformModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "MeshDeform Modifier", "MeshDeform Modifier.");
	RNA_def_struct_sdna(srna, "MeshDeformModifierData");
}

static void rna_def_modifier_particlesystem(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ParticleSystemModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "ParticleSystem Modifier", "ParticleSystem Modifier.");
	RNA_def_struct_sdna(srna, "ParticleSystemModifierData");
}

static void rna_def_modifier_particleinstance(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ParticleInstanceModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "ParticleInstance Modifier", "ParticleInstance Modifier.");
	RNA_def_struct_sdna(srna, "ParticleInstanceModifierData");
}

static void rna_def_modifier_explode(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ExplodeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Explode Modifier", "Explode Modifier.");
	RNA_def_struct_sdna(srna, "ExplodeModifierData");
}

static void rna_def_modifier_cloth(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ClothModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Cloth Modifier", "Cloth Modifier.");
	RNA_def_struct_sdna(srna, "ClothModifierData");
}

static void rna_def_modifier_collision(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "CollisionModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Collision Modifier", "Collision Modifier.");
	RNA_def_struct_sdna(srna, "CollisionModifierData");
}

static void rna_def_modifier_bevel(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "BevelModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Bevel Modifier", "Bevel Modifier.");
	RNA_def_struct_sdna(srna, "BevelModifierData");
}

static void rna_def_modifier_shrinkwrap(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ShrinkwrapModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Shrinkwrap Modifier", "Shrinkwrap Modifier.");
	RNA_def_struct_sdna(srna, "ShrinkwrapModifierData");
}

static void rna_def_modifier_fluidsim(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "FluidSimulationModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Fluid Simulation Modifier", "Fluid Simulation Modifier.");
	RNA_def_struct_sdna(srna, "FluidsimModifierData");

	prop= RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "fss");
	RNA_def_property_struct_type(prop, "FluidSettings");
	RNA_def_property_ui_text(prop, "Settings", "Settings for how this object is used in the fluid simulation.");
}

static void rna_def_modifier_mask(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "MaskModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Mask Modifier", "Mask Modifier.");
	RNA_def_struct_sdna(srna, "MaskModifierData");
}

static void rna_def_modifier_simpledeform(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "SimpleDeformModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "SimpleDeform Modifier", "SimpleDeform Modifier.");
	RNA_def_struct_sdna(srna, "SimpleDeformModifierData");
}

void RNA_def_modifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem type_items[] ={
		{eModifierType_Subsurf, "SUBSURF", "Subsurf", ""},
		{eModifierType_Lattice, "LATTICE", "Lattice", ""},
		{eModifierType_Curve, "CURVE", "Curve", ""},
		{eModifierType_Build, "BUILD", "Build", ""},
		{eModifierType_Mirror, "MIRROR", "Mirror", ""},
		{eModifierType_Decimate, "DECIMATE", "Decimate", ""},
		{eModifierType_Wave, "WAVE", "Wave", ""},
		{eModifierType_Armature, "ARMATURE", "Armature", ""},
		{eModifierType_Hook, "HOOK", "Hook", ""},
		{eModifierType_Softbody, "SOFTBODY", "Softbody", ""},
		{eModifierType_Boolean, "BOOLEAN", "Boolean", ""},
		{eModifierType_Array, "ARRAY", "Array", ""},
		{eModifierType_EdgeSplit, "EDGESPLIT", "Edge Split", ""},
		{eModifierType_Displace, "DISPLACE", "Displace", ""},
		{eModifierType_UVProject, "UVPROJECT", "UV Project", ""},
		{eModifierType_Smooth, "SMOOTH", "Smooth", ""},
		{eModifierType_Cast, "CAST", "Cast", ""},
		{eModifierType_MeshDeform, "MESHDEFORM", "Mesh Deform", ""},
		{eModifierType_ParticleSystem, "PARTICLESYSTEM", "Particle System", ""},
		{eModifierType_ParticleInstance, "PARTICLEINSTANCE", "Particle Instance", ""},
		{eModifierType_Explode, "EXPLODE", "Explode", ""},
		{eModifierType_Cloth, "CLOTH", "Cloth", ""},
		{eModifierType_Collision, "COLLISION", "Collision", ""},
		{eModifierType_Bevel, "BEVEL", "Bevel", ""},
		{eModifierType_Shrinkwrap, "SHRINKWRAP", "Shrinkwrap", ""},
		{eModifierType_Fluidsim, "FLUIDSIMULATION", "Fluid Simulation", ""},
		{eModifierType_Mask, "MASK", "Mask", ""},
		{eModifierType_SimpleDeform, "SIMPLEDEFORM", "Simple Deform", ""},
		{0, NULL, NULL, NULL}};
	
	/* data */
	srna= RNA_def_struct(brna, "Modifier", NULL);
	RNA_def_struct_ui_text(srna , "Object Modifier", "DOC_BROKEN");
	RNA_def_struct_refine_func(srna, "rna_Modifier_refine");
	RNA_def_struct_sdna(srna, "ModifierData");
	
	/* strings */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	/* enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	
	/* flags */
	prop= RNA_def_property(srna, "realtime", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Realtime);
	RNA_def_property_ui_text(prop, "Realtime", "Realtime display of a modifier.");
	
	prop= RNA_def_property(srna, "render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Render);
	RNA_def_property_ui_text(prop, "Render", "Use modifier during rendering.");
	
	prop= RNA_def_property(srna, "editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Editmode);
	RNA_def_property_ui_text(prop, "Editmode", "Use modifier while in the edit mode.");
	
	prop= RNA_def_property(srna, "on_cage", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_OnCage);
	RNA_def_property_ui_text(prop, "On Cage", "Enable direct editing of modifier control cage.");
	
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
}

#endif
