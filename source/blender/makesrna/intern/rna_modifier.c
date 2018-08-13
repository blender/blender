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
 * Contributor(s): Blender Foundation (2008), Juho Veps‰l‰inen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_modifier.c
 *  \ingroup RNA
 */


#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_data_transfer.h"
#include "BKE_dynamicpaint.h"
#include "BKE_effect.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_multires.h"
#include "BKE_ocean.h"
#include "BKE_smoke.h" /* For smokeModifier_free & smokeModifier_createType */

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

const EnumPropertyItem rna_enum_object_modifier_type_items[] = {
	{0, "", 0, N_("Modify"), ""},
	{eModifierType_DataTransfer, "DATA_TRANSFER", ICON_MOD_DATA_TRANSFER, "Data Transfer", ""},
	{eModifierType_MeshCache, "MESH_CACHE", ICON_MOD_MESHDEFORM, "Mesh Cache", ""},
	{eModifierType_MeshSequenceCache, "MESH_SEQUENCE_CACHE", ICON_MOD_MESHDEFORM, "Mesh Sequence Cache", ""},
	{eModifierType_NormalEdit, "NORMAL_EDIT", ICON_MOD_NORMALEDIT, "Normal Edit", ""},
	{eModifierType_WeightedNormal, "WEIGHTED_NORMAL", ICON_MOD_NORMALEDIT, "Weighted Normal", ""},
	{eModifierType_UVProject, "UV_PROJECT", ICON_MOD_UVPROJECT, "UV Project", ""},
	{eModifierType_UVWarp, "UV_WARP", ICON_MOD_UVPROJECT, "UV Warp", ""},
	{eModifierType_WeightVGEdit, "VERTEX_WEIGHT_EDIT", ICON_MOD_VERTEX_WEIGHT, "Vertex Weight Edit", ""},
	{eModifierType_WeightVGMix, "VERTEX_WEIGHT_MIX", ICON_MOD_VERTEX_WEIGHT, "Vertex Weight Mix", ""},
	{eModifierType_WeightVGProximity, "VERTEX_WEIGHT_PROXIMITY", ICON_MOD_VERTEX_WEIGHT,
	                                  "Vertex Weight Proximity", ""},
	{0, "", 0, N_("Generate"), ""},
	{eModifierType_Array, "ARRAY", ICON_MOD_ARRAY, "Array", ""},
	{eModifierType_Bevel, "BEVEL", ICON_MOD_BEVEL, "Bevel", ""},
	{eModifierType_Boolean, "BOOLEAN", ICON_MOD_BOOLEAN, "Boolean", ""},
	{eModifierType_Build, "BUILD", ICON_MOD_BUILD, "Build", ""},
	{eModifierType_Decimate, "DECIMATE", ICON_MOD_DECIM, "Decimate", ""},
	{eModifierType_EdgeSplit, "EDGE_SPLIT", ICON_MOD_EDGESPLIT, "Edge Split", ""},
	{eModifierType_Mask, "MASK", ICON_MOD_MASK, "Mask", ""},
	{eModifierType_Mirror, "MIRROR", ICON_MOD_MIRROR, "Mirror", ""},
	{eModifierType_Multires, "MULTIRES", ICON_MOD_MULTIRES, "Multiresolution", ""},
	{eModifierType_Remesh, "REMESH", ICON_MOD_REMESH, "Remesh", ""},
	{eModifierType_Screw, "SCREW", ICON_MOD_SCREW, "Screw", ""},
	{eModifierType_Skin, "SKIN", ICON_MOD_SKIN, "Skin", ""},
	{eModifierType_Solidify, "SOLIDIFY", ICON_MOD_SOLIDIFY, "Solidify", ""},
	{eModifierType_Subsurf, "SUBSURF", ICON_MOD_SUBSURF, "Subdivision Surface", ""},
	{eModifierType_Triangulate, "TRIANGULATE", ICON_MOD_TRIANGULATE, "Triangulate", ""},
	{eModifierType_Wireframe, "WIREFRAME", ICON_MOD_WIREFRAME, "Wireframe", "Generate a wireframe on the edges of a mesh"},
	{0, "", 0, N_("Deform"), ""},
	{eModifierType_Armature, "ARMATURE", ICON_MOD_ARMATURE, "Armature", ""},
	{eModifierType_Cast, "CAST", ICON_MOD_CAST, "Cast", ""},
	{eModifierType_CorrectiveSmooth, "CORRECTIVE_SMOOTH", ICON_MOD_SMOOTH, "Corrective Smooth", ""},
	{eModifierType_Curve, "CURVE", ICON_MOD_CURVE, "Curve", ""},
	{eModifierType_Displace, "DISPLACE", ICON_MOD_DISPLACE, "Displace", ""},
	{eModifierType_Hook, "HOOK", ICON_HOOK, "Hook", ""},
	{eModifierType_LaplacianSmooth, "LAPLACIANSMOOTH", ICON_MOD_SMOOTH, "Laplacian Smooth", ""},
	{eModifierType_LaplacianDeform, "LAPLACIANDEFORM", ICON_MOD_MESHDEFORM, "Laplacian Deform", ""},
	{eModifierType_Lattice, "LATTICE", ICON_MOD_LATTICE, "Lattice", ""},
	{eModifierType_MeshDeform, "MESH_DEFORM", ICON_MOD_MESHDEFORM, "Mesh Deform", ""},
	{eModifierType_Shrinkwrap, "SHRINKWRAP", ICON_MOD_SHRINKWRAP, "Shrinkwrap", ""},
	{eModifierType_SimpleDeform, "SIMPLE_DEFORM", ICON_MOD_SIMPLEDEFORM, "Simple Deform", ""},
	{eModifierType_Smooth, "SMOOTH", ICON_MOD_SMOOTH, "Smooth", ""},
	{eModifierType_SurfaceDeform, "SURFACE_DEFORM", ICON_MOD_MESHDEFORM, "Surface Deform", ""},
	{eModifierType_Warp, "WARP", ICON_MOD_WARP, "Warp", ""},
	{eModifierType_Wave, "WAVE", ICON_MOD_WAVE, "Wave", ""},
	{0, "", 0, N_("Simulate"), ""},
	{eModifierType_Cloth, "CLOTH", ICON_MOD_CLOTH, "Cloth", ""},
	{eModifierType_Collision, "COLLISION", ICON_MOD_PHYSICS, "Collision", ""},
	{eModifierType_DynamicPaint, "DYNAMIC_PAINT", ICON_MOD_DYNAMICPAINT, "Dynamic Paint", ""},
	{eModifierType_Explode, "EXPLODE", ICON_MOD_EXPLODE, "Explode", ""},
	{eModifierType_Fluidsim, "FLUID_SIMULATION", ICON_MOD_FLUIDSIM, "Fluid Simulation", ""},
	{eModifierType_Ocean, "OCEAN", ICON_MOD_OCEAN, "Ocean", ""},
	{eModifierType_ParticleInstance, "PARTICLE_INSTANCE", ICON_MOD_PARTICLES, "Particle Instance", ""},
	{eModifierType_ParticleSystem, "PARTICLE_SYSTEM", ICON_MOD_PARTICLES, "Particle System", ""},
	{eModifierType_Smoke, "SMOKE", ICON_MOD_SMOKE, "Smoke", ""},
	{eModifierType_Softbody, "SOFT_BODY", ICON_MOD_SOFT, "Soft Body", ""},
	{eModifierType_Surface, "SURFACE", ICON_MOD_PHYSICS, "Surface", ""},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_modifier_triangulate_quad_method_items[] = {
	{MOD_TRIANGULATE_QUAD_BEAUTY, "BEAUTY", 0, "Beauty ", "Split the quads in nice triangles, slower method"},
	{MOD_TRIANGULATE_QUAD_FIXED, "FIXED", 0, "Fixed", "Split the quads on the first and third vertices"},
	{MOD_TRIANGULATE_QUAD_ALTERNATE, "FIXED_ALTERNATE", 0, "Fixed Alternate",
		                             "Split the quads on the 2nd and 4th vertices"},
	{MOD_TRIANGULATE_QUAD_SHORTEDGE, "SHORTEST_DIAGONAL", 0, "Shortest Diagonal",
		                             "Split the quads based on the distance between the vertices"},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_modifier_triangulate_ngon_method_items[] = {
	{MOD_TRIANGULATE_NGON_BEAUTY, "BEAUTY", 0, "Beauty", "Arrange the new triangles evenly (slow)"},
	{MOD_TRIANGULATE_NGON_EARCLIP, "CLIP", 0, "Clip", "Split the polygons with an ear clipping algorithm"},
	{0, NULL, 0, NULL, NULL}
};

#ifndef RNA_RUNTIME
/* use eWarp_Falloff_*** & eHook_Falloff_***, they're in sync */
static const EnumPropertyItem modifier_warp_falloff_items[] = {
	{eWarp_Falloff_None,    "NONE", 0, "No Falloff", ""},
	{eWarp_Falloff_Curve,   "CURVE", 0, "Curve", ""},
	{eWarp_Falloff_Smooth,  "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
	{eWarp_Falloff_Sphere,  "SPHERE", ICON_SPHERECURVE, "Sphere", ""},
	{eWarp_Falloff_Root,    "ROOT", ICON_ROOTCURVE, "Root", ""},
	{eWarp_Falloff_InvSquare, "INVERSE_SQUARE", ICON_ROOTCURVE, "Inverse Square", ""},
	{eWarp_Falloff_Sharp,   "SHARP", ICON_SHARPCURVE, "Sharp", ""},
	{eWarp_Falloff_Linear,  "LINEAR", ICON_LINCURVE, "Linear", ""},
	{eWarp_Falloff_Const,   "CONSTANT", ICON_NOCURVE, "Constant", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif

/* ***** Data Transfer ***** */

const EnumPropertyItem rna_enum_dt_method_vertex_items[] = {
	{MREMAP_MODE_TOPOLOGY, "TOPOLOGY", 0, "Topology",
	 "Copy from identical topology meshes"},
	{MREMAP_MODE_VERT_NEAREST, "NEAREST", 0, "Nearest vertex",
	 "Copy from closest vertex"},
	{MREMAP_MODE_VERT_EDGE_NEAREST, "EDGE_NEAREST", 0, "Nearest Edge Vertex",
	 "Copy from closest vertex of closest edge"},
	{MREMAP_MODE_VERT_EDGEINTERP_NEAREST, "EDGEINTERP_NEAREST", 0, "Nearest Edge Interpolated",
	 "Copy from interpolated values of vertices from closest point on closest edge"},
	{MREMAP_MODE_VERT_POLY_NEAREST, "POLY_NEAREST", 0, "Nearest Face Vertex",
	 "Copy from closest vertex of closest face"},
	{MREMAP_MODE_VERT_POLYINTERP_NEAREST, "POLYINTERP_NEAREST", 0, "Nearest Face Interpolated",
	 "Copy from interpolated values of vertices from closest point on closest face"},
	{MREMAP_MODE_VERT_POLYINTERP_VNORPROJ, "POLYINTERP_VNORPROJ", 0, "Projected Face Interpolated",
	 "Copy from interpolated values of vertices from point on closest face hit by normal-projection"},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_dt_method_edge_items[] = {
	{MREMAP_MODE_TOPOLOGY, "TOPOLOGY", 0, "Topology",
	 "Copy from identical topology meshes"},
	{MREMAP_MODE_EDGE_VERT_NEAREST, "VERT_NEAREST", 0, "Nearest Vertices",
	 "Copy from most similar edge (edge which vertices are the closest of destination edge's ones)"},
	{MREMAP_MODE_EDGE_NEAREST, "NEAREST", 0, "Nearest Edge",
	 "Copy from closest edge (using midpoints)"},
	{MREMAP_MODE_EDGE_POLY_NEAREST, "POLY_NEAREST", 0, "Nearest Face Edge",
	 "Copy from closest edge of closest face (using midpoints)"},
	{MREMAP_MODE_EDGE_EDGEINTERP_VNORPROJ, "EDGEINTERP_VNORPROJ", 0, "Projected Edge Interpolated",
	 "Interpolate all source edges hit by the projection of destination one along its own normal (from vertices)"},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_dt_method_loop_items[] = {
	{MREMAP_MODE_TOPOLOGY, "TOPOLOGY", 0, "Topology",
	 "Copy from identical topology meshes"},
	{MREMAP_MODE_LOOP_NEAREST_LOOPNOR, "NEAREST_NORMAL", 0, "Nearest Corner And Best Matching Normal",
	 "Copy from nearest corner which has the best matching normal"},
	{MREMAP_MODE_LOOP_NEAREST_POLYNOR, "NEAREST_POLYNOR", 0, "Nearest Corner And Best Matching Face Normal",
	 "Copy from nearest corner which has the face with the best matching normal to destination corner's face one"},
	{MREMAP_MODE_LOOP_POLY_NEAREST, "NEAREST_POLY", 0, "Nearest Corner Of Nearest Face",
	 "Copy from nearest corner of nearest polygon"},
	{MREMAP_MODE_LOOP_POLYINTERP_NEAREST, "POLYINTERP_NEAREST", 0, "Nearest Face Interpolated",
	 "Copy from interpolated corners of the nearest source polygon"},
	{MREMAP_MODE_LOOP_POLYINTERP_LNORPROJ, "POLYINTERP_LNORPROJ", 0, "Projected Face Interpolated",
	 "Copy from interpolated corners of the source polygon hit by corner normal projection"},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_dt_method_poly_items[] = {
	{MREMAP_MODE_TOPOLOGY, "TOPOLOGY", 0, "Topology",
	 "Copy from identical topology meshes"},
	{MREMAP_MODE_POLY_NEAREST, "NEAREST", 0, "Nearest Face",
	 "Copy from nearest polygon (using center points)"},
	{MREMAP_MODE_POLY_NOR, "NORMAL", 0, "Best Normal-Matching",
	 "Copy from source polygon which normal is the closest to destination one"},
	{MREMAP_MODE_POLY_POLYINTERP_PNORPROJ, "POLYINTERP_PNORPROJ", 0, "Projected Face Interpolated",
	 "Interpolate all source polygons intersected by the projection of destination one along its own normal"},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_dt_mix_mode_items[] = {
	{CDT_MIX_TRANSFER, "REPLACE", 0, "Replace",
	 "Overwrite all elements' data"},
	{CDT_MIX_REPLACE_ABOVE_THRESHOLD, "ABOVE_THRESHOLD", 0, "Above Threshold",
	 "Only replace destination elements where data is above given threshold (exact behavior depends on data type)"},
	{CDT_MIX_REPLACE_BELOW_THRESHOLD, "BELOW_THRESHOLD", 0, "Below Threshold",
	 "Only replace destination elements where data is below given threshold (exact behavior depends on data type)"},
	{CDT_MIX_MIX, "MIX", 0, "Mix",
	 "Mix source value into destination one, using given threshold as factor"},
	{CDT_MIX_ADD, "ADD", 0, "Add",
	 "Add source value to destination one, using given threshold as factor"},
	{CDT_MIX_SUB, "SUB", 0, "Subtract",
	 "Subtract source value to destination one, using given threshold as factor"},
	{CDT_MIX_MUL, "MUL", 0, "Multiply",
	 "Multiply source value to destination one, using given threshold as factor"},
	/* etc. etc. */
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_dt_layers_select_src_items[] = {
	{DT_LAYERS_ACTIVE_SRC, "ACTIVE", 0, "Active Layer",
	 "Only transfer active data layer"},
	{DT_LAYERS_ALL_SRC, "ALL", 0, "All Layers",
	 "Transfer all data layers"},
	{DT_LAYERS_VGROUP_SRC_BONE_SELECT, "BONE_SELECT", 0, "Selected Pose Bones",
	 "Transfer all vertex groups used by selected pose bones"},
	{DT_LAYERS_VGROUP_SRC_BONE_DEFORM, "BONE_DEFORM", 0, "Deform Pose Bones",
	 "Transfer all vertex groups used by deform bones"},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_dt_layers_select_dst_items[] = {
	{DT_LAYERS_ACTIVE_DST, "ACTIVE", 0, "Active Layer",
	 "Affect active data layer of all targets"},
	{DT_LAYERS_NAME_DST, "NAME", 0, "By Name",
	 "Match target data layers to affect by name"},
	{DT_LAYERS_INDEX_DST, "INDEX", 0, "By Order",
	 "Match target data layers to affect by order (indices)"},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_axis_xy_items[] = {
	{0, "X", 0, "X", ""},
	{1, "Y", 0, "Y", ""},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_axis_xyz_items[] = {
	{0, "X", 0, "X", ""},
	{1, "Y", 0, "Y", ""},
	{2, "Z", 0, "Z", ""},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_axis_flag_xyz_items[] = {
	{(1 << 0), "X", 0, "X", ""},
	{(1 << 1), "Y", 0, "Y", ""},
	{(1 << 2), "Z", 0, "Z", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "DNA_particle_types.h"
#include "DNA_curve_types.h"
#include "DNA_smoke_types.h"

#include "BKE_cachefile.h"
#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

static void rna_UVProject_projectors_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	UVProjectModifierData *uvp = (UVProjectModifierData *)ptr->data;
	rna_iterator_array_begin(iter, (void *)uvp->projectors, sizeof(Object *), uvp->num_projectors, 0, NULL);
}

static StructRNA *rna_Modifier_refine(struct PointerRNA *ptr)
{
	ModifierData *md = (ModifierData *)ptr->data;

	switch ((ModifierType)md->type) {
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
		case eModifierType_Solidify:
			return &RNA_SolidifyModifier;
		case eModifierType_Screw:
			return &RNA_ScrewModifier;
		case eModifierType_Ocean:
			return &RNA_OceanModifier;
		case eModifierType_Warp:
			return &RNA_WarpModifier;
		case eModifierType_WeightVGEdit:
			return &RNA_VertexWeightEditModifier;
		case eModifierType_WeightVGMix:
			return &RNA_VertexWeightMixModifier;
		case eModifierType_WeightVGProximity:
			return &RNA_VertexWeightProximityModifier;
		case eModifierType_DynamicPaint:
			return &RNA_DynamicPaintModifier;
		case eModifierType_Remesh:
			return &RNA_RemeshModifier;
		case eModifierType_Skin:
			return &RNA_SkinModifier;
		case eModifierType_LaplacianSmooth:
			return &RNA_LaplacianSmoothModifier;
		case eModifierType_Triangulate:
			return &RNA_TriangulateModifier;
		case eModifierType_UVWarp:
			return &RNA_UVWarpModifier;
		case eModifierType_MeshCache:
			return &RNA_MeshCacheModifier;
		case eModifierType_LaplacianDeform:
			return &RNA_LaplacianDeformModifier;
		case eModifierType_Wireframe:
			return &RNA_WireframeModifier;
		case eModifierType_DataTransfer:
			return &RNA_DataTransferModifier;
		case eModifierType_NormalEdit:
			return &RNA_NormalEditModifier;
		case eModifierType_CorrectiveSmooth:
			return &RNA_CorrectiveSmoothModifier;
		case eModifierType_MeshSequenceCache:
			return &RNA_MeshSequenceCacheModifier;
		case eModifierType_SurfaceDeform:
			return &RNA_SurfaceDeformModifier;
		case eModifierType_WeightedNormal:
			return &RNA_WeightedNormalModifier;
		/* Default */
		case eModifierType_None:
		case eModifierType_ShapeKey:
		case NUM_MODIFIER_TYPES:
			return &RNA_Modifier;
	}

	return &RNA_Modifier;
}

static void rna_Modifier_name_set(PointerRNA *ptr, const char *value)
{
	ModifierData *md = ptr->data;
	char oldname[sizeof(md->name)];

	/* make a copy of the old name first */
	BLI_strncpy(oldname, md->name, sizeof(md->name));

	/* copy the new name into the name slot */
	BLI_strncpy_utf8(md->name, value, sizeof(md->name));

	/* make sure the name is truly unique */
	if (ptr->id.data) {
		Object *ob = ptr->id.data;
		modifier_unique_name(&ob->modifiers, md);
	}

	/* fix all the animation data which may link to this */
	BKE_animdata_fix_paths_rename_all(NULL, "modifiers", oldname, md->name);
}

static char *rna_Modifier_path(PointerRNA *ptr)
{
	ModifierData *md = ptr->data;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"]", name_esc);
}

static void rna_Modifier_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	DEG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ptr->id.data);
}

static void rna_Modifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_Modifier_update(bmain, scene, ptr);
	DEG_relations_tag_update(bmain);
}

/* Vertex Groups */

#define RNA_MOD_VGROUP_NAME_SET(_type, _prop)                                               \
static void rna_##_type##Modifier_##_prop##_set(PointerRNA *ptr, const char *value)         \
{                                                                                           \
	_type##ModifierData *tmd = (_type##ModifierData *)ptr->data;                            \
	rna_object_vgroup_name_set(ptr, value, tmd->_prop, sizeof(tmd->_prop));                 \
}

RNA_MOD_VGROUP_NAME_SET(Armature, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Bevel, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Cast, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Curve, name);
RNA_MOD_VGROUP_NAME_SET(DataTransfer, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Decimate, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(CorrectiveSmooth, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Displace, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Hook, name);
RNA_MOD_VGROUP_NAME_SET(LaplacianDeform, anchor_grp_name);
RNA_MOD_VGROUP_NAME_SET(LaplacianSmooth, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Lattice, name);
RNA_MOD_VGROUP_NAME_SET(Mask, vgroup);
RNA_MOD_VGROUP_NAME_SET(MeshDeform, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(NormalEdit, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Shrinkwrap, vgroup_name);
RNA_MOD_VGROUP_NAME_SET(SimpleDeform, vgroup_name);
RNA_MOD_VGROUP_NAME_SET(Smooth, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Solidify, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(UVWarp, vgroup_name);
RNA_MOD_VGROUP_NAME_SET(Warp, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Wave, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGEdit, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGEdit, mask_defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGMix, defgrp_name_a);
RNA_MOD_VGROUP_NAME_SET(WeightVGMix, defgrp_name_b);
RNA_MOD_VGROUP_NAME_SET(WeightVGMix, mask_defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGProximity, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightVGProximity, mask_defgrp_name);
RNA_MOD_VGROUP_NAME_SET(WeightedNormal, defgrp_name);
RNA_MOD_VGROUP_NAME_SET(Wireframe, defgrp_name);

static void rna_ExplodeModifier_vgroup_get(PointerRNA *ptr, char *value)
{
	ExplodeModifierData *emd = (ExplodeModifierData *)ptr->data;
	rna_object_vgroup_name_index_get(ptr, value, emd->vgroup);
}

static int rna_ExplodeModifier_vgroup_length(PointerRNA *ptr)
{
	ExplodeModifierData *emd = (ExplodeModifierData *)ptr->data;
	return rna_object_vgroup_name_index_length(ptr, emd->vgroup);
}

static void rna_ExplodeModifier_vgroup_set(PointerRNA *ptr, const char *value)
{
	ExplodeModifierData *emd = (ExplodeModifierData *)ptr->data;
	rna_object_vgroup_name_index_set(ptr, value, &emd->vgroup);
}

#undef RNA_MOD_VGROUP_NAME_SET

/* UV layers */

#define RNA_MOD_UVLAYER_NAME_SET(_type, _prop)                                              \
static void rna_##_type##Modifier_##_prop##_set(PointerRNA *ptr, const char *value)         \
{                                                                                           \
	_type##ModifierData *tmd = (_type##ModifierData *)ptr->data;                            \
	rna_object_uvlayer_name_set(ptr, value, tmd->_prop, sizeof(tmd->_prop));                \
}

RNA_MOD_UVLAYER_NAME_SET(MappingInfo, uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(UVProject, uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(UVWarp, uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(WeightVGEdit, mask_tex_uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(WeightVGMix, mask_tex_uvlayer_name);
RNA_MOD_UVLAYER_NAME_SET(WeightVGProximity, mask_tex_uvlayer_name);

#undef RNA_MOD_UVLAYER_NAME_SET

/* Objects */

static void modifier_object_set(Object *self, Object **ob_p, int type, PointerRNA value)
{
	Object *ob = value.data;

	if (!self || ob != self) {
		if (!ob || type == OB_EMPTY || ob->type == type) {
			id_lib_extern((ID *)ob);
			*ob_p = ob;
		}
	}
}

#define RNA_MOD_OBJECT_SET(_type, _prop, _obtype)                                           \
static void rna_##_type##Modifier_##_prop##_set(PointerRNA *ptr, PointerRNA value)          \
{                                                                                           \
	_type##ModifierData *tmd = (_type##ModifierData *)ptr->data;                            \
	modifier_object_set(ptr->id.data, &tmd->_prop, _obtype, value);                         \
}

RNA_MOD_OBJECT_SET(Armature, object, OB_ARMATURE);
RNA_MOD_OBJECT_SET(Array, start_cap, OB_MESH);
RNA_MOD_OBJECT_SET(Array, end_cap, OB_MESH);
RNA_MOD_OBJECT_SET(Array, curve_ob, OB_CURVE);
RNA_MOD_OBJECT_SET(Boolean, object, OB_MESH);
RNA_MOD_OBJECT_SET(Cast, object, OB_EMPTY);
RNA_MOD_OBJECT_SET(Curve, object, OB_CURVE);
RNA_MOD_OBJECT_SET(DataTransfer, ob_source, OB_MESH);
RNA_MOD_OBJECT_SET(Lattice, object, OB_LATTICE);
RNA_MOD_OBJECT_SET(Mask, ob_arm, OB_ARMATURE);
RNA_MOD_OBJECT_SET(MeshDeform, object, OB_MESH);
RNA_MOD_OBJECT_SET(NormalEdit, target, OB_EMPTY);
RNA_MOD_OBJECT_SET(Shrinkwrap, target, OB_MESH);
RNA_MOD_OBJECT_SET(Shrinkwrap, auxTarget, OB_MESH);
RNA_MOD_OBJECT_SET(SurfaceDeform, target, OB_MESH);

static void rna_HookModifier_object_set(PointerRNA *ptr, PointerRNA value)
{
	HookModifierData *hmd = ptr->data;
	Object *ob = (Object *)value.data;

	hmd->object = ob;
	id_lib_extern((ID *)ob);
	BKE_object_modifier_hook_reset(ob, hmd);
}

static PointerRNA rna_UVProjector_object_get(PointerRNA *ptr)
{
	Object **ob = (Object **)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Object, *ob);
}

static void rna_UVProjector_object_set(PointerRNA *ptr, PointerRNA value)
{
	Object **ob_p = (Object **)ptr->data;
	Object *ob = (Object *)value.data;
	id_lib_extern((ID *)ob);
	*ob_p = ob;
}

#undef RNA_MOD_OBJECT_SET

/* Other rna callbacks */

static void rna_Smoke_set_type(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	SmokeModifierData *smd = (SmokeModifierData *)ptr->data;
	Object *ob = (Object *)ptr->id.data;

	/* nothing changed */
	if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain)
		return;

	smokeModifier_free(smd); /* XXX TODO: completely free all 3 pointers */
	smokeModifier_createType(smd); /* create regarding of selected type */

	switch (smd->type) {
		case MOD_SMOKE_TYPE_DOMAIN:
			ob->dt = OB_WIRE;
			break;
		case MOD_SMOKE_TYPE_FLOW:
		case MOD_SMOKE_TYPE_COLL:
		case 0:
		default:
			break;
	}

	/* update dependency since a domain - other type switch could have happened */
	rna_Modifier_dependency_update(bmain, scene, ptr);
}

static void rna_MultiresModifier_type_set(PointerRNA *ptr, int value)
{
	Object *ob = (Object *)ptr->id.data;
	MultiresModifierData *mmd = (MultiresModifierData *)ptr->data;

	multires_force_update(ob);
	mmd->simple = value;
}

static void rna_MultiresModifier_level_range(PointerRNA *ptr, int *min, int *max,
                                             int *UNUSED(softmin), int *UNUSED(softmax))
{
	MultiresModifierData *mmd = (MultiresModifierData *)ptr->data;

	*min = 0;
	*max = max_ii(0, mmd->totlvl);  /* intentionally _not_ -1 */
}

static bool rna_MultiresModifier_external_get(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	Mesh *me = ob->data;

	return CustomData_external_test(&me->ldata, CD_MDISPS);
}

static void rna_MultiresModifier_filepath_get(PointerRNA *ptr, char *value)
{
	Object *ob = (Object *)ptr->id.data;
	CustomDataExternal *external = ((Mesh *)ob->data)->ldata.external;

	BLI_strncpy(value, (external) ? external->filename : "", sizeof(external->filename));
}

static void rna_MultiresModifier_filepath_set(PointerRNA *ptr, const char *value)
{
	Object *ob = (Object *)ptr->id.data;
	CustomDataExternal *external = ((Mesh *)ob->data)->ldata.external;

	if (external && !STREQ(external->filename, value)) {
		BLI_strncpy(external->filename, value, sizeof(external->filename));
		multires_force_external_reload(ob);
	}
}

static int rna_MultiresModifier_filepath_length(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	CustomDataExternal *external = ((Mesh *)ob->data)->ldata.external;

	return strlen((external) ? external->filename : "");
}

static int rna_ShrinkwrapModifier_face_cull_get(PointerRNA *ptr)
{
	ShrinkwrapModifierData *swm = (ShrinkwrapModifierData *)ptr->data;
	return swm->shrinkOpts & (MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE | MOD_SHRINKWRAP_CULL_TARGET_BACKFACE);
}

static void rna_ShrinkwrapModifier_face_cull_set(struct PointerRNA *ptr, int value)
{
	ShrinkwrapModifierData *swm = (ShrinkwrapModifierData *)ptr->data;

	swm->shrinkOpts =
	    (swm->shrinkOpts & ~(MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE | MOD_SHRINKWRAP_CULL_TARGET_BACKFACE)) | value;
}

static bool rna_MeshDeformModifier_is_bound_get(PointerRNA *ptr)
{
	return (((MeshDeformModifierData *)ptr->data)->bindcagecos != NULL);
}

static PointerRNA rna_SoftBodyModifier_settings_get(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	return rna_pointer_inherit_refine(ptr, &RNA_SoftBodySettings, ob->soft);
}

static PointerRNA rna_SoftBodyModifier_point_cache_get(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	return rna_pointer_inherit_refine(ptr, &RNA_PointCache, ob->soft->shared->pointcache);
}

static PointerRNA rna_CollisionModifier_settings_get(PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	return rna_pointer_inherit_refine(ptr, &RNA_CollisionSettings, ob->pd);
}

static void rna_UVProjectModifier_num_projectors_set(PointerRNA *ptr, int value)
{
	UVProjectModifierData *md = (UVProjectModifierData *)ptr->data;
	int a;

	md->num_projectors = CLAMPIS(value, 1, MOD_UVPROJECT_MAXPROJECTORS);
	for (a = md->num_projectors; a < MOD_UVPROJECT_MAXPROJECTORS; a++)
		md->projectors[a] = NULL;
}

static void rna_OceanModifier_init_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	OceanModifierData *omd = (OceanModifierData *)ptr->data;

	BKE_ocean_free_modifier_cache(omd);
	rna_Modifier_update(bmain, scene, ptr);
}

static void rna_OceanModifier_ocean_chop_set(PointerRNA *ptr, float value)
{
	OceanModifierData *omd = (OceanModifierData *)ptr->data;
	float old_value = omd->chop_amount;

	omd->chop_amount = value;

	if ((old_value == 0.0f && value > 0.0f) ||
	    (old_value > 0.0f && value == 0.0f))
	{
		BKE_ocean_free_modifier_cache(omd);
	}
}

static bool rna_LaplacianDeformModifier_is_bind_get(PointerRNA *ptr)
{
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)ptr->data;
	return ((lmd->flag & MOD_LAPLACIANDEFORM_BIND) && (lmd->cache_system != NULL));
}

/* NOTE: Curve and array modifiers requires curve path to be evaluated,
 * dependency graph will make sure that curve eval would create such a path,
 * but if curve was already evaluated we might miss path.
 *
 * So what we do here is: if path was not calculated for target curve we
 * tag it for update.
 */

static void rna_CurveModifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	CurveModifierData *cmd = (CurveModifierData *)ptr->data;
	rna_Modifier_update(bmain, scene, ptr);
	DEG_relations_tag_update(bmain);
	if (cmd->object != NULL) {
		Curve *curve = cmd->object->data;
		if ((curve->flag & CU_PATH) == 0) {
			DEG_id_tag_update(&curve->id, OB_RECALC_DATA);
		}
	}
}

static void rna_ArrayModifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ArrayModifierData *amd = (ArrayModifierData *)ptr->data;
	rna_Modifier_update(bmain, scene, ptr);
	DEG_relations_tag_update(bmain);
	if (amd->curve_ob != NULL) {
		Curve *curve = amd->curve_ob->data;
		if ((curve->flag & CU_PATH) == 0) {
			DEG_id_tag_update(&curve->id, OB_RECALC_DATA);
		}
	}
}


static void rna_DataTransferModifier_use_data_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

	if (!(dtmd->flags & MOD_DATATRANSFER_USE_VERT)) {
		dtmd->data_types &= ~DT_TYPE_VERT_ALL;
	}
	if (!(dtmd->flags & MOD_DATATRANSFER_USE_EDGE)) {
		dtmd->data_types &= ~DT_TYPE_EDGE_ALL;
	}
	if (!(dtmd->flags & MOD_DATATRANSFER_USE_LOOP)) {
		dtmd->data_types &= ~DT_TYPE_LOOP_ALL;
	}
	if (!(dtmd->flags & MOD_DATATRANSFER_USE_POLY)) {
		dtmd->data_types &= ~DT_TYPE_POLY_ALL;
	}

	rna_Modifier_update(bmain, scene, ptr);
}

static void rna_DataTransferModifier_data_types_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;
	const int item_types = BKE_object_data_transfer_get_dttypes_item_types(dtmd->data_types);

	if (item_types & ME_VERT) {
		dtmd->flags |= MOD_DATATRANSFER_USE_VERT;
	}
	if (item_types & ME_EDGE) {
		dtmd->flags |= MOD_DATATRANSFER_USE_EDGE;
	}
	if (item_types & ME_LOOP) {
		dtmd->flags |= MOD_DATATRANSFER_USE_LOOP;
	}
	if (item_types & ME_POLY) {
		dtmd->flags |= MOD_DATATRANSFER_USE_POLY;
	}

	rna_Modifier_update(bmain, scene, ptr);
}

static void rna_DataTransferModifier_verts_data_types_set(struct PointerRNA *ptr, int value)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

	dtmd->data_types &= ~DT_TYPE_VERT_ALL;
	dtmd->data_types |= value;
}

static void rna_DataTransferModifier_edges_data_types_set(struct PointerRNA *ptr, int value)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

	dtmd->data_types &= ~DT_TYPE_EDGE_ALL;
	dtmd->data_types |= value;
}

static void rna_DataTransferModifier_loops_data_types_set(struct PointerRNA *ptr, int value)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

	dtmd->data_types &= ~DT_TYPE_LOOP_ALL;
	dtmd->data_types |= value;
}

static void rna_DataTransferModifier_polys_data_types_set(struct PointerRNA *ptr, int value)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;

	dtmd->data_types &= ~DT_TYPE_POLY_ALL;
	dtmd->data_types |= value;
}

static const EnumPropertyItem *rna_DataTransferModifier_layers_select_src_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *prop, bool *r_free)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;
	EnumPropertyItem *item = NULL, tmp_item = {0};
	int totitem = 0;

	if (!C) {  /* needed for docs and i18n tools */
		return rna_enum_dt_layers_select_src_items;
	}

	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = CTX_data_scene(C);

	/* No active here! */
	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_ALL_SRC);

	if (STREQ(RNA_property_identifier(prop), "layers_vgroup_select_src")) {
		Object *ob_src = dtmd->ob_source;

#if 0  /* XXX Don't think we want this in modifier version... */
		if (BKE_object_pose_armature_get(ob_src)) {
			RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_VGROUP_SRC_BONE_SELECT);
			RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_VGROUP_SRC_BONE_DEFORM);
		}
#endif

		if (ob_src) {
			bDeformGroup *dg;
			int i;

			RNA_enum_item_add_separator(&item, &totitem);

			for (i = 0, dg = ob_src->defbase.first; dg; i++, dg = dg->next) {
				tmp_item.value = i;
				tmp_item.identifier = tmp_item.name = dg->name;
				RNA_enum_item_add(&item, &totitem, &tmp_item);
			}
		}
	}
	else if (STREQ(RNA_property_identifier(prop), "layers_shapekey_select_src")) {
		/* TODO */
	}
	else if (STREQ(RNA_property_identifier(prop), "layers_uv_select_src")) {
		Object *ob_src = dtmd->ob_source;

		if (ob_src) {
			Mesh *me_eval;
			int num_data, i;

			me_eval = mesh_get_eval_final(depsgraph, scene, ob_src, CD_MASK_BAREMESH | CD_MLOOPUV);
			num_data = CustomData_number_of_layers(&me_eval->ldata, CD_MLOOPUV);

			RNA_enum_item_add_separator(&item, &totitem);

			for (i = 0; i < num_data; i++) {
				tmp_item.value = i;
				tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(&me_eval->ldata, CD_MLOOPUV, i);
				RNA_enum_item_add(&item, &totitem, &tmp_item);
			}
		}
	}
	else if (STREQ(RNA_property_identifier(prop), "layers_vcol_select_src")) {
		Object *ob_src = dtmd->ob_source;

		if (ob_src) {
			Mesh *me_eval;
			int num_data, i;

			me_eval = mesh_get_eval_final(depsgraph, scene, ob_src, CD_MASK_BAREMESH | CD_MLOOPCOL);
			num_data = CustomData_number_of_layers(&me_eval->ldata, CD_MLOOPCOL);

			RNA_enum_item_add_separator(&item, &totitem);

			for (i = 0; i < num_data; i++) {
				tmp_item.value = i;
				tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(&me_eval->ldata, CD_MLOOPCOL, i);
				RNA_enum_item_add(&item, &totitem, &tmp_item);
			}
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static const EnumPropertyItem *rna_DataTransferModifier_layers_select_dst_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *prop, bool *r_free)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;
	EnumPropertyItem *item = NULL, tmp_item = {0};
	int totitem = 0;

	if (!C) {  /* needed for docs and i18n tools */
		return rna_enum_dt_layers_select_dst_items;
	}

	/* No active here! */
	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_dst_items, DT_LAYERS_NAME_DST);
	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_dst_items, DT_LAYERS_INDEX_DST);

	if (STREQ(RNA_property_identifier(prop), "layers_vgroup_select_dst")) {
		/* Only list destination layers if we have a single source! */
		if (dtmd->layers_select_src[DT_MULTILAYER_INDEX_MDEFORMVERT] >= 0) {
			Object *ob_dst = CTX_data_active_object(C); /* XXX Is this OK? */

			if (ob_dst) {
				bDeformGroup *dg;
				int i;

				RNA_enum_item_add_separator(&item, &totitem);

				for (i = 0, dg = ob_dst->defbase.first; dg; i++, dg = dg->next) {
					tmp_item.value = i;
					tmp_item.identifier = tmp_item.name = dg->name;
					RNA_enum_item_add(&item, &totitem, &tmp_item);
				}
			}
		}
	}
	else if (STREQ(RNA_property_identifier(prop), "layers_shapekey_select_dst")) {
		/* TODO */
	}
	else if (STREQ(RNA_property_identifier(prop), "layers_uv_select_dst")) {
		/* Only list destination layers if we have a single source! */
		if (dtmd->layers_select_src[DT_MULTILAYER_INDEX_UV] >= 0) {
			Object *ob_dst = CTX_data_active_object(C); /* XXX Is this OK? */

			if (ob_dst && ob_dst->data) {
				Mesh *me_dst;
				CustomData *ldata;
				int num_data, i;

				me_dst = ob_dst->data;
				ldata = &me_dst->ldata;
				num_data = CustomData_number_of_layers(ldata, CD_MLOOPUV);

				RNA_enum_item_add_separator(&item, &totitem);

				for (i = 0; i < num_data; i++) {
					tmp_item.value = i;
					tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(ldata, CD_MLOOPUV, i);
					RNA_enum_item_add(&item, &totitem, &tmp_item);
				}
			}
		}
	}
	else if (STREQ(RNA_property_identifier(prop), "layers_vcol_select_dst")) {
		/* Only list destination layers if we have a single source! */
		if (dtmd->layers_select_src[DT_MULTILAYER_INDEX_VCOL] >= 0) {
			Object *ob_dst = CTX_data_active_object(C); /* XXX Is this OK? */

			if (ob_dst && ob_dst->data) {
				Mesh *me_dst;
				CustomData *ldata;
				int num_data, i;

				me_dst = ob_dst->data;
				ldata = &me_dst->ldata;
				num_data = CustomData_number_of_layers(ldata, CD_MLOOPCOL);

				RNA_enum_item_add_separator(&item, &totitem);

				for (i = 0; i < num_data; i++) {
					tmp_item.value = i;
					tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(ldata, CD_MLOOPCOL, i);
					RNA_enum_item_add(&item, &totitem, &tmp_item);
				}
			}
		}
	}


	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static const EnumPropertyItem *rna_DataTransferModifier_mix_mode_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *)ptr->data;
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	bool support_advanced_mixing, support_threshold;

	if (!C) {  /* needed for docs and i18n tools */
		return rna_enum_dt_mix_mode_items;
	}

	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_TRANSFER);

	BKE_object_data_transfer_get_dttypes_capacity(dtmd->data_types, &support_advanced_mixing, &support_threshold);

	if (support_threshold) {
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_REPLACE_ABOVE_THRESHOLD);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_REPLACE_BELOW_THRESHOLD);
	}

	if (support_advanced_mixing) {
		RNA_enum_item_add_separator(&item, &totitem);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_MIX);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_ADD);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_SUB);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_MUL);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static void rna_CorrectiveSmoothModifier_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)ptr->data;

	MEM_SAFE_FREE(csmd->delta_cache);

	rna_Modifier_update(bmain, scene, ptr);
}

static void rna_CorrectiveSmoothModifier_rest_source_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)ptr->data;

	if (csmd->rest_source != MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
		MEM_SAFE_FREE(csmd->bind_coords);
		csmd->bind_coords_num = 0;
	}

	rna_CorrectiveSmoothModifier_update(bmain, scene, ptr);
}

static bool rna_CorrectiveSmoothModifier_is_bind_get(PointerRNA *ptr)
{
	CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)ptr->data;
	return (csmd->bind_coords != NULL);
}

static bool rna_SurfaceDeformModifier_is_bound_get(PointerRNA *ptr)
{
	return (((SurfaceDeformModifierData *)ptr->data)->verts != NULL);
}

static void rna_MeshSequenceCache_object_path_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#ifdef WITH_ALEMBIC
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *)ptr->data;
	Object *ob = (Object *)ptr->id.data;

	mcmd->reader = CacheReader_open_alembic_object(mcmd->cache_file->handle,
	                                               mcmd->reader,
	                                               ob,
	                                               mcmd->object_path);
#endif

	rna_Modifier_update(bmain, scene, ptr);
}

static bool rna_ParticleInstanceModifier_particle_system_poll(PointerRNA *ptr, const PointerRNA value)
{
	ParticleInstanceModifierData *psmd = ptr->data;
	ParticleSystem *psys = value.data;

	if (!psmd->ob)
		return false;

	/* make sure psys is in the object */
	return BLI_findindex(&psmd->ob->particlesystem, psys) != -1;
}

static PointerRNA rna_ParticleInstanceModifier_particle_system_get(PointerRNA *ptr)
{
	ParticleInstanceModifierData *psmd = ptr->data;
	ParticleSystem *psys;
	PointerRNA rptr;

	if (!psmd->ob)
		return PointerRNA_NULL;

	psys = BLI_findlink(&psmd->ob->particlesystem, psmd->psys - 1);
	RNA_pointer_create((ID *)psmd->ob, &RNA_ParticleSystem, psys, &rptr);
	return rptr;
}

static void rna_ParticleInstanceModifier_particle_system_set(PointerRNA *ptr, const PointerRNA value)
{
	ParticleInstanceModifierData *psmd = ptr->data;

	if (!psmd->ob)
		return;

	psmd->psys = BLI_findindex(&psmd->ob->particlesystem, value.data) + 1;
	CLAMP_MIN(psmd->psys, 1);
}

#else

static PropertyRNA *rna_def_property_subdivision_common(StructRNA *srna, const char type[])
{
	static const EnumPropertyItem prop_subdivision_type_items[] = {
		{SUBSURF_TYPE_CATMULL_CLARK, "CATMULL_CLARK", 0, "Catmull-Clark", ""},
		{SUBSURF_TYPE_SIMPLE, "SIMPLE", 0, "Simple", ""},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop = RNA_def_property(srna, "subdivision_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, type);
	RNA_def_property_enum_items(prop, prop_subdivision_type_items);
	RNA_def_property_ui_text(prop, "Subdivision Type", "Select type of subdivision algorithm");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	return prop;
}

static void rna_def_modifier_subsurf(BlenderRNA *brna)
{
	static const EnumPropertyItem prop_uv_smooth_items[] = {
		{SUBSURF_UV_SMOOTH_NONE, "NONE", 0,
		 "Sharp", "UVs are not smoothed, boundaries are kept sharp"},
		{SUBSURF_UV_SMOOTH_PRESERVE_CORNERS, "PRESERVE_CORNERS", 0,
		 "Smooth, keep corners", "UVs are smoothed, corners on discontinuous boundary are kept sharp"},
#if 0
		{SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_AND_JUNCTIONS, "PRESERVE_CORNERS_AND_JUNCTIONS", 0,
		 "Smooth, keep corners+junctions", "UVs are smoothed, corners on discontinuous boundary and "
		 "junctions of 3 or more regions are kept sharp"},
		{SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE, "PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE", 0,
		 "Smooth, keep corners+junctions+concave", "UVs are smoothed, corners on discontinuous boundary, "
		 "junctions of 3 or more regions and darts and concave corners are kept sharp"},
		{SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES, "PRESERVE_BOUNDARIES", 0,
		 "Smooth, keep corners", "UVs are smoothed, boundaries are kept sharp"},
		{SUBSURF_UV_SMOOTH_ALL, "PRESERVE_BOUNDARIES", 0,
		 "Smooth all", "UVs and boundaries are smoothed"},
#endif
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SubsurfModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Subsurf Modifier", "Subdivision surface modifier");
	RNA_def_struct_sdna(srna, "SubsurfModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SUBSURF);

	rna_def_property_subdivision_common(srna, "subdivType");

	/* see CCGSUBSURF_LEVEL_MAX for max limit */
	prop = RNA_def_property(srna, "levels", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "levels");
	RNA_def_property_range(prop, 0, 11);
	RNA_def_property_ui_range(prop, 0, 6, 1, -1);
	RNA_def_property_ui_text(prop, "Levels", "Number of subdivisions to perform");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "render_levels", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "renderLevels");
	RNA_def_property_range(prop, 0, 11);
	RNA_def_property_ui_range(prop, 0, 6, 1, -1);
	RNA_def_property_ui_text(prop, "Render Levels", "Number of subdivisions to perform when rendering");

	prop = RNA_def_property(srna, "show_only_control_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", eSubsurfModifierFlag_ControlEdges);
	RNA_def_property_ui_text(prop, "Optimal Display", "Skip drawing/rendering of interior subdivided edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "uv_smooth", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "uv_smooth");
	RNA_def_property_enum_items(prop, prop_uv_smooth_items);
	RNA_def_property_ui_text(prop, "UV Smooth", "Controls how smoothing is applied to UVs");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_generic_map_info(StructRNA *srna)
{
	static const EnumPropertyItem prop_texture_coordinates_items[] = {
		{MOD_DISP_MAP_LOCAL, "LOCAL", 0, "Local", "Use the local coordinate system for the texture coordinates"},
		{MOD_DISP_MAP_GLOBAL, "GLOBAL", 0, "Global", "Use the global coordinate system for the texture coordinates"},
		{MOD_DISP_MAP_OBJECT, "OBJECT", 0, "Object",
		 "Use the linked object's local coordinate system for the texture coordinates"},
		{MOD_DISP_MAP_UV, "UV", 0, "UV", "Use UV coordinates for the texture coordinates"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Texture", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "texture_coords", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texmapping");
	RNA_def_property_enum_items(prop, prop_texture_coordinates_items);
	RNA_def_property_ui_text(prop, "Texture Coordinates", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Map", "UV map name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MappingInfoModifier_uvlayer_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "texture_coords_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "map_object");
	RNA_def_property_ui_text(prop, "Texture Coordinate Object", "Object to set the texture coordinates");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
}

static void rna_def_modifier_warp(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "WarpModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Warp Modifier", "Warp modifier");
	RNA_def_struct_sdna(srna, "WarpModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_WARP);

	prop = RNA_def_property(srna, "object_from", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "From", "Object to transform from");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "object_to", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "To", "Object to transform to");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100, 100, 10, 2);
	RNA_def_property_ui_text(prop, "Strength", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, modifier_warp_falloff_items);
	RNA_def_property_ui_text(prop, "Falloff Type", "");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_UNSIGNED | PROP_DISTANCE);
	RNA_def_property_ui_text(prop, "Radius", "Radius to apply");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curfalloff");
	RNA_def_property_ui_text(prop, "Falloff Curve", "Custom falloff curve");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_volume_preserve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WARP_VOLUME_PRESERVE);
	RNA_def_property_ui_text(prop, "Preserve Volume", "Preserve volume when rotations are used");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WarpModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	rna_def_modifier_generic_map_info(srna);
}

static void rna_def_modifier_multires(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MultiresModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Multires Modifier", "Multiresolution mesh modifier");
	RNA_def_struct_sdna(srna, "MultiresModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MULTIRES);

	prop = rna_def_property_subdivision_common(srna, "simple");
	RNA_def_property_enum_funcs(prop, NULL, "rna_MultiresModifier_type_set", NULL);

	prop = RNA_def_property(srna, "levels", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "lvl");
	RNA_def_property_ui_text(prop, "Levels", "Number of subdivisions to use in the viewport");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MultiresModifier_level_range");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "sculpt_levels", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "sculptlvl");
	RNA_def_property_ui_text(prop, "Sculpt Levels", "Number of subdivisions to use in sculpt mode");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MultiresModifier_level_range");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "render_levels", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "renderlvl");
	RNA_def_property_ui_text(prop, "Render Levels", "The subdivision level visible at render time");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MultiresModifier_level_range");

	prop = RNA_def_property(srna, "total_levels", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "totlvl");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Total Levels", "Number of subdivisions for which displacements are stored");

	prop = RNA_def_property(srna, "is_external", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_MultiresModifier_external_get", NULL);
	RNA_def_property_ui_text(prop, "External",
	                         "Store multires displacements outside the .blend file, to save memory");

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_funcs(prop, "rna_MultiresModifier_filepath_get", "rna_MultiresModifier_filepath_length",
	                              "rna_MultiresModifier_filepath_set");
	RNA_def_property_ui_text(prop, "File Path", "Path to external displacements file");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "show_only_control_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", eMultiresModifierFlag_ControlEdges);
	RNA_def_property_ui_text(prop, "Optimal Display", "Skip drawing/rendering of interior subdivided edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_subsurf_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flags", eMultiresModifierFlag_PlainUv);
	RNA_def_property_ui_text(prop, "Subdivide UVs", "Use subsurf to subdivide UVs");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_lattice(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LatticeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Lattice Modifier", "Lattice deformation modifier");
	RNA_def_struct_sdna(srna, "LatticeModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_LATTICE);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Lattice object to deform with");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_LatticeModifier_object_set", NULL, "rna_Lattice_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of Vertex Group which determines influence of modifier per point");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_LatticeModifier_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 10, 2);
	RNA_def_property_ui_text(prop, "Strength", "Strength of modifier effect");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_curve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_deform_axis_items[] = {
		{MOD_CURVE_POSX, "POS_X", 0, "X", ""},
		{MOD_CURVE_POSY, "POS_Y", 0, "Y", ""},
		{MOD_CURVE_POSZ, "POS_Z", 0, "Z", ""},
		{MOD_CURVE_NEGX, "NEG_X", 0, "-X", ""},
		{MOD_CURVE_NEGY, "NEG_Y", 0, "-Y", ""},
		{MOD_CURVE_NEGZ, "NEG_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "CurveModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Curve Modifier", "Curve deformation modifier");
	RNA_def_struct_sdna(srna, "CurveModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_CURVE);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Curve object to deform with");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_CurveModifier_object_set", NULL, "rna_Curve_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_CurveModifier_dependency_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of Vertex Group which determines influence of modifier per point");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CurveModifier_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "deform_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "defaxis");
	RNA_def_property_enum_items(prop, prop_deform_axis_items);
	RNA_def_property_ui_text(prop, "Deform Axis", "The axis that the curve deforms along");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_build(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BuildModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Build Modifier", "Build effect modifier");
	RNA_def_struct_sdna(srna, "BuildModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_BUILD);

	prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "start");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Start", "Start frame of the effect");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "frame_duration", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "length");
	RNA_def_property_range(prop, 1, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Length", "Total time the build effect requires");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_reverse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_BUILD_FLAG_REVERSE);
	RNA_def_property_ui_text(prop, "Reversed", "Deconstruct the mesh instead of building it");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_random_order", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_BUILD_FLAG_RANDOMIZE);
	RNA_def_property_ui_text(prop, "Randomize", "Randomize the faces or edges during build");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Seed", "Seed for random if used");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_mirror(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MirrorModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Mirror Modifier", "Mirroring modifier");
	RNA_def_struct_sdna(srna, "MirrorModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MIRROR);

	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_AXIS_X);
	RNA_def_property_ui_text(prop, "X", "Enable X axis mirror");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_AXIS_Y);
	RNA_def_property_ui_text(prop, "Y", "Enable Y axis mirror");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_AXIS_Z);
	RNA_def_property_ui_text(prop, "Z", "Enable Z axis mirror");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_clip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_CLIPPING);
	RNA_def_property_ui_text(prop, "Clip", "Prevent vertices from going through the mirror during transform");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_mirror_vertex_groups", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_VGROUP);
	RNA_def_property_ui_text(prop, "Mirror Vertex Groups", "Mirror vertex groups (e.g. .R->.L)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_mirror_merge", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", MOD_MIR_NO_MERGE);
	RNA_def_property_ui_text(prop, "Merge Vertices", "Merge vertices within the merge threshold");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_mirror_u", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_MIRROR_U);
	RNA_def_property_ui_text(prop, "Mirror U", "Mirror the U texture coordinate around the flip offset point");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_mirror_v", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MIR_MIRROR_V);
	RNA_def_property_ui_text(prop, "Mirror V", "Mirror the V texture coordinate around the flip offset point");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mirror_offset_u", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "uv_offset[0]");
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_range(prop, -1, 1, 2, 4);
	RNA_def_property_ui_text(prop, "Flip U Offset", "Amount to offset mirrored UVs flipping point from the 0.5 on the U axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mirror_offset_v", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "uv_offset[1]");
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_range(prop, -1, 1, 2, 4);
	RNA_def_property_ui_text(prop, "Flip V Offset", "Amount to offset mirrored UVs flipping point from the 0.5 point on the V axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "offset_u", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "uv_offset_copy[0]");
	RNA_def_property_range(prop, -10000.0f, 10000.0f);
	RNA_def_property_ui_range(prop, -1, 1, 2, 4);
	RNA_def_property_ui_text(prop, "U Offset", "Mirrored UV offset on the U axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "offset_v", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "uv_offset_copy[1]");
	RNA_def_property_range(prop, -10000.0f, 10000.0f);
	RNA_def_property_ui_range(prop, -1, 1, 2, 4);
	RNA_def_property_ui_text(prop, "V Offset", "Mirrored UV offset on the V axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "merge_threshold", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "tolerance");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 0.01, 6);
	RNA_def_property_ui_text(prop, "Merge Limit", "Distance within which mirrored vertices are merged");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mirror_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mirror_ob");
	RNA_def_property_ui_text(prop, "Mirror Object", "Object to use as mirror");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
}

static void rna_def_modifier_decimate(BlenderRNA *brna)
{
	static const EnumPropertyItem modifier_decim_mode_items[] = {
		{MOD_DECIM_MODE_COLLAPSE, "COLLAPSE", 0, "Collapse", "Use edge collapsing"},
		{MOD_DECIM_MODE_UNSUBDIV, "UNSUBDIV", 0, "Un-Subdivide", "Use un-subdivide face reduction"},
		{MOD_DECIM_MODE_DISSOLVE, "DISSOLVE", 0, "Planar", "Dissolve geometry to form planar polygons"},
		{0, NULL, 0, NULL, NULL}
	};

	/* Note, keep in sync with operator 'MESH_OT_decimate' */

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DecimateModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Decimate Modifier", "Decimation modifier");
	RNA_def_struct_sdna(srna, "DecimateModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_DECIM);

	prop = RNA_def_property(srna, "decimate_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, modifier_decim_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* (mode == MOD_DECIM_MODE_COLLAPSE) */
	prop = RNA_def_property(srna, "ratio", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "percent");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 1, 4);
	RNA_def_property_ui_text(prop, "Ratio", "Ratio of triangles to reduce to (collapse only)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* (mode == MOD_DECIM_MODE_UNSUBDIV) */
	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "iter");
	RNA_def_property_range(prop, 0, SHRT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 1, -1);
	RNA_def_property_ui_text(prop, "Iterations", "Number of times reduce the geometry (unsubdivide only)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* (mode == MOD_DECIM_MODE_DISSOLVE) */
	prop = RNA_def_property(srna, "angle_limit", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_range(prop, 0, DEG2RAD(180));
	RNA_def_property_ui_range(prop, 0, DEG2RAD(180), 10, 2);
	RNA_def_property_ui_text(prop, "Angle Limit", "Only dissolve angles below this (planar only)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* (mode == MOD_DECIM_MODE_COLLAPSE) */
	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name (collapse only)");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DecimateModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DECIM_FLAG_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence (collapse only)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_collapse_triangulate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DECIM_FLAG_TRIANGULATE);
	RNA_def_property_ui_text(prop, "Triangulate", "Keep triangulated faces resulting from decimation (collapse only)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_symmetry", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DECIM_FLAG_SYMMETRY);
	RNA_def_property_ui_text(prop, "Symmetry", "Maintain symmetry on an axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "symmetry_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "symmetry_axis");
	RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
	RNA_def_property_ui_text(prop, "Axis", "Axis of symmetry");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "defgrp_factor");
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_range(prop, 0, 10, 1, 4);
	RNA_def_property_ui_text(prop, "Factor", "Vertex group strength");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	/* end collapse-only option */

	/* (mode == MOD_DECIM_MODE_DISSOLVE) */
	prop = RNA_def_property(srna, "use_dissolve_boundaries", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS);
	RNA_def_property_ui_text(prop, "All Boundaries", "Dissolve all vertices inbetween face boundaries (planar only)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "delimit", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG); /* important to run before default set */
	RNA_def_property_enum_items(prop, rna_enum_mesh_delimit_mode_items);
	RNA_def_property_ui_text(prop, "Delimit", "Limit merging geometry");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* end dissolve-only option */



	/* all modes use this */
	prop = RNA_def_property(srna, "face_count", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Face Count", "The current number of faces in the decimated mesh");
}

static void rna_def_modifier_wave(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "WaveModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Wave Modifier", "Wave effect modifier");
	RNA_def_struct_sdna(srna, "WaveModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_WAVE);

	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_X);
	RNA_def_property_ui_text(prop, "X", "X axis motion");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_Y);
	RNA_def_property_ui_text(prop, "Y", "Y axis motion");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_CYCL);
	RNA_def_property_ui_text(prop, "Cyclic", "Cyclic wave effect");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM);
	RNA_def_property_ui_text(prop, "Normals", "Displace along normals");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normal_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_X);
	RNA_def_property_ui_text(prop, "X Normal", "Enable displacement along the X normal");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normal_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_Y);
	RNA_def_property_ui_text(prop, "Y Normal", "Enable displacement along the Y normal");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normal_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WAVE_NORM_Z);
	RNA_def_property_ui_text(prop, "Z Normal", "Enable displacement along the Z normal");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "time_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "timeoffs");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Time Offset",
	                         "Either the starting frame (for positive speed) or ending frame (for negative speed.)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "lifetime");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Lifetime",  "Lifetime of the wave in frames, zero means infinite");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "damping_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "damp");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Damping Time",  "Number of frames in which the wave damps out after it dies");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "falloff");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Falloff Radius",  "Distance after which it fades out");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "start_position_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "startx");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Start Position X",  "X coordinate of the start position");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "start_position_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "starty");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Start Position Y",  "Y coordinate of the start position");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "start_position_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "objectcenter");
	RNA_def_property_ui_text(prop, "Start Position Object", "Object which defines the wave center");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the wave");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WaveModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -1, 1, 10, 2);
	RNA_def_property_ui_text(prop, "Speed", "Speed of the wave, towards the starting point when negative");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -2, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Height", "Height of the wave");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 5, 10, 2);
	RNA_def_property_ui_text(prop, "Width", "Distance between the waves");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "narrowness", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "narrow");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 10, 10, 2);
	RNA_def_property_ui_text(prop, "Narrowness",
	                         "Distance between the top and the base of a wave, the higher the value, "
	                         "the more narrow the wave");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	rna_def_modifier_generic_map_info(srna);
}

static void rna_def_modifier_armature(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ArmatureModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Armature Modifier", "Armature deformation modifier");
	RNA_def_struct_sdna(srna, "ArmatureModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_ARMATURE);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Armature object to deform with");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ArmatureModifier_object_set", NULL, "rna_Armature_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "use_bone_envelopes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_ENVELOPE);
	RNA_def_property_ui_text(prop, "Use Bone Envelopes", "Bind Bone envelopes to armature modifier");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_vertex_groups", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_VGROUP);
	RNA_def_property_ui_text(prop, "Use Vertex Groups", "Bind vertex groups to armature modifier");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_deform_preserve_volume", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_QUATERNION);
	RNA_def_property_ui_text(prop, "Preserve Volume", "Deform rotation interpolation with quaternions");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_multi_modifier", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "multi", 0);
	RNA_def_property_ui_text(prop, "Multi Modifier",
	                         "Use same input as previous modifier, and mix results using overall vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of Vertex Group which determines influence of modifier per point");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ArmatureModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_hook(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "HookModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Hook Modifier", "Hook modifier to modify the location of vertices");
	RNA_def_struct_sdna(srna, "HookModifierData");
	RNA_def_struct_ui_icon(srna, ICON_HOOK);

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "force");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Strength",  "Relative force of the hook");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, modifier_warp_falloff_items);  /* share the enum */
	RNA_def_property_ui_text(prop, "Falloff Type", "");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "falloff");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Radius",  "If not zero, the distance from the hook where influence ends");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curfalloff");
	RNA_def_property_ui_text(prop, "Falloff Curve", "Custom falloff curve");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cent");
	RNA_def_property_ui_text(prop, "Hook Center", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "matrix_inverse", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "parentinv");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Matrix", "Reverse the transformation between this object and its target");
	RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Modifier_update");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Parent Object for hook, also recalculates and clears offset");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_HookModifier_object_set", NULL, NULL);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target",
	                         "Name of Parent Bone for hook (if applicable), also recalculates and clears offset");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "use_falloff_uniform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_HOOK_UNIFORM_SPACE);
	RNA_def_property_ui_text(prop, "Uniform Falloff", "Compensate for non-uniform object scale");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of Vertex Group which determines influence of modifier per point");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_HookModifier_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_softbody(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SoftBodyModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Soft Body Modifier", "Soft body simulation modifier");
	RNA_def_struct_sdna(srna, "SoftbodyModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SOFT);

	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "SoftBodySettings");
	RNA_def_property_pointer_funcs(prop, "rna_SoftBodyModifier_settings_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Soft Body Settings", "");

	prop = RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "PointCache");
	RNA_def_property_pointer_funcs(prop, "rna_SoftBodyModifier_point_cache_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Soft Body Point Cache", "");
}

static void rna_def_modifier_boolean(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_operation_items[] = {
		{eBooleanModifierOp_Intersect, "INTERSECT", 0, "Intersect",
		                               "Keep the part of the mesh that intersects with the other selected object"},
		{eBooleanModifierOp_Union, "UNION", 0, "Union", "Combine two meshes in an additive way"},
		{eBooleanModifierOp_Difference, "DIFFERENCE", 0, "Difference", "Combine two meshes in a subtractive way"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "BooleanModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Boolean Modifier", "Boolean operations modifier");
	RNA_def_struct_sdna(srna, "BooleanModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_BOOLEAN);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Mesh object to use for Boolean operation");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_BooleanModifier_object_set", NULL, "rna_Mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_operation_items);
	RNA_def_property_ui_text(prop, "Operation", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "double_threshold", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "double_threshold");
	RNA_def_property_range(prop, 0, 1.0f);
	RNA_def_property_ui_range(prop, 0, 1, 0.0001, 6);
	RNA_def_property_ui_text(prop, "Overlap Threshold",  "Threshold for checking overlapping geometry");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* BMesh debugging options, only used when G_DEBUG is set */

	/* BMesh intersection options */
	static const EnumPropertyItem debug_items[] = {
		{eBooleanModifierBMeshFlag_BMesh_Separate, "SEPARATE", 0, "Separate", ""},
		{eBooleanModifierBMeshFlag_BMesh_NoDissolve, "NO_DISSOLVE", 0, "No Dissolve", ""},
		{eBooleanModifierBMeshFlag_BMesh_NoConnectRegions, "NO_CONNECT_REGIONS", 0, "No Connect Regions", ""},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "debug_options", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, debug_items);
	RNA_def_property_enum_sdna(prop, NULL, "bm_flag");
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Debug", "Debugging options, only when started with '-d'");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_array(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_fit_type_items[] = {
		{MOD_ARR_FIXEDCOUNT, "FIXED_COUNT", 0, "Fixed Count", "Duplicate the object a certain number of times"},
		{MOD_ARR_FITLENGTH, "FIT_LENGTH", 0, "Fit Length",
		                    "Duplicate the object as many times as fits in a certain length"},
		{MOD_ARR_FITCURVE, "FIT_CURVE", 0, "Fit Curve", "Fit the duplicated objects to a curve"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ArrayModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Array Modifier", "Array duplication modifier");
	RNA_def_struct_sdna(srna, "ArrayModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_ARRAY);

	/* Length parameters */
	prop = RNA_def_property(srna, "fit_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_fit_type_items);
	RNA_def_property_ui_text(prop, "Fit Type", "Array length calculation method");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "count", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_range(prop, 1, 1000, 1, -1);
	RNA_def_property_ui_text(prop, "Count",  "Number of duplicates to make");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "fit_length", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "length");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
	RNA_def_property_ui_text(prop, "Length", "Length to fit array within");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curve_ob");
	RNA_def_property_ui_text(prop, "Curve", "Curve object to fit array length to");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ArrayModifier_curve_ob_set", NULL, "rna_Curve_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_ArrayModifier_dependency_update");

	/* Offset parameters */
	prop = RNA_def_property(srna, "use_constant_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_CONST);
	RNA_def_property_ui_text(prop, "Constant Offset", "Add a constant offset");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "constant_offset_displace", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_ui_text(prop, "Constant Offset Displacement", "Value for the distance between arrayed items");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_relative_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_RELATIVE);
	RNA_def_property_ui_text(prop, "Relative Offset", "Add an offset relative to the object's bounding box");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* PROP_TRANSLATION causes units to be used which we don't want */
	prop = RNA_def_property(srna, "relative_offset_displace", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scale");
	RNA_def_property_ui_text(prop, "Relative Offset Displacement",
	                         "The size of the geometry will determine the distance between arrayed items");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Vertex merging parameters */
	prop = RNA_def_property(srna, "use_merge_vertices", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_ARR_MERGE);
	RNA_def_property_ui_text(prop, "Merge Vertices", "Merge vertices in adjacent duplicates");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_merge_vertices_cap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_ARR_MERGEFINAL);
	RNA_def_property_ui_text(prop, "Merge Vertices", "Merge vertices in first and last duplicates");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "merge_threshold", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "merge_dist");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 1, 4);
	RNA_def_property_ui_text(prop, "Merge Distance", "Limit below which to merge vertices");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Offset object */
	prop = RNA_def_property(srna, "use_object_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "offset_type", MOD_ARR_OFF_OBJ);
	RNA_def_property_ui_text(prop, "Object Offset", "Add another object's transformation to the total offset");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "offset_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "offset_ob");
	RNA_def_property_ui_text(prop, "Object Offset",
	                         "Use the location and rotation of another object to determine the distance and "
	                         "rotational change between arrayed items");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	/* Caps */
	prop = RNA_def_property(srna, "start_cap", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Start Cap", "Mesh object to use as a start cap");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ArrayModifier_start_cap_set", NULL, "rna_Mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "end_cap", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "End Cap", "Mesh object to use as an end cap");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ArrayModifier_end_cap_set", NULL, "rna_Mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "offset_u", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "uv_offset[0]");
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_range(prop, -1, 1, 2, 4);
	RNA_def_property_ui_text(prop, "U Offset", "Amount to offset array UVs on the U axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "offset_v", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "uv_offset[1]");
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_range(prop, -1, 1, 2, 4);
	RNA_def_property_ui_text(prop, "V Offset", "Amount to offset array UVs on the V axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_edgesplit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "EdgeSplitModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "EdgeSplit Modifier", "Edge splitting modifier to create sharp edges");
	RNA_def_struct_sdna(srna, "EdgeSplitModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_EDGESPLIT);

	prop = RNA_def_property(srna, "split_angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
	RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 2);
	RNA_def_property_ui_text(prop, "Split Angle", "Angle above which to split edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_edge_angle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_EDGESPLIT_FROMANGLE);
	RNA_def_property_ui_text(prop, "Use Edge Angle", "Split edges with high angle between faces");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_edge_sharp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_EDGESPLIT_FROMFLAG);
	RNA_def_property_ui_text(prop, "Use Sharp Edges", "Split edges that are marked as sharp");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_displace(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_direction_items[] = {
		{MOD_DISP_DIR_X, "X", 0, "X", "Use the texture's intensity value to displace in the X direction"},
		{MOD_DISP_DIR_Y, "Y", 0, "Y", "Use the texture's intensity value to displace in the Y direction"},
		{MOD_DISP_DIR_Z, "Z", 0, "Z", "Use the texture's intensity value to displace in the Z direction"},
		{MOD_DISP_DIR_NOR, "NORMAL", 0, "Normal",
		 "Use the texture's intensity value to displace along the vertex normal"},
		{MOD_DISP_DIR_CLNOR, "CUSTOM_NORMAL", 0, "Custom Normal",
		 "Use the texture's intensity value to displace along the (averaged) custom normal (falls back to vertex)"},
		{MOD_DISP_DIR_RGB_XYZ, "RGB_TO_XYZ", 0, "RGB to XYZ",
		 "Use the texture's RGB values to displace the mesh in the XYZ direction"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_space_items[] = {
		{MOD_DISP_SPACE_LOCAL, "LOCAL", 0, "Local", "Direction is defined in local coordinates"},
		{MOD_DISP_SPACE_GLOBAL, "GLOBAL", 0, "Global", "Direction is defined in global coordinates"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "DisplaceModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Displace Modifier", "Displacement modifier");
	RNA_def_struct_sdna(srna, "DisplaceModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_DISPLACE);

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of Vertex Group which determines influence of modifier per point");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DisplaceModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mid_level", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "midlevel");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Midlevel", "Material value that gives no displacement");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100, 100, 10, 3);
	RNA_def_property_ui_text(prop, "Strength", "Amount to displace geometry");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_direction_items);
	RNA_def_property_ui_text(prop, "Direction", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_space_items);
	RNA_def_property_ui_text(prop, "Space", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	rna_def_modifier_generic_map_info(srna);
}

static void rna_def_modifier_uvproject(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "UVProjectModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "UV Project Modifier", "UV projection modifier to set UVs from a projector");
	RNA_def_struct_sdna(srna, "UVProjectModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_UVPROJECT);

	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Map", "UV map name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_UVProjectModifier_uvlayer_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "projector_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "num_projectors");
	RNA_def_property_ui_text(prop, "Number of Projectors", "Number of projectors to use");
	RNA_def_property_int_funcs(prop, NULL, "rna_UVProjectModifier_num_projectors_set", NULL);
	RNA_def_property_range(prop, 1, MOD_UVPROJECT_MAXPROJECTORS);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "projectors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "UVProjector");
	RNA_def_property_collection_funcs(prop, "rna_UVProject_projectors_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Projectors", "");

	prop = RNA_def_property(srna, "aspect_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aspectx");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_range(prop, 1, FLT_MAX);
	RNA_def_property_ui_range(prop, 1, 1000, 1, 3);
	RNA_def_property_ui_text(prop, "Horizontal Aspect Ratio", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "aspect_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aspecty");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_range(prop, 1, FLT_MAX);
	RNA_def_property_ui_range(prop, 1, 1000, 1, 3);
	RNA_def_property_ui_text(prop, "Vertical Aspect Ratio", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "scale_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scalex");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1000, 1, 3);
	RNA_def_property_ui_text(prop, "Horizontal Scale", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "scale_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scaley");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1000, 1, 3);
	RNA_def_property_ui_text(prop, "Vertical Scale", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	srna = RNA_def_struct(brna, "UVProjector", NULL);
	RNA_def_struct_ui_text(srna, "UVProjector", "UV projector used by the UV project modifier");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, "rna_UVProjector_object_get", "rna_UVProjector_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_ui_text(prop, "Object", "Object to use as projector transform");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
}

static void rna_def_modifier_smooth(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SmoothModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Smooth Modifier", "Smoothing effect modifier");
	RNA_def_struct_sdna(srna, "SmoothModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_X);
	RNA_def_property_ui_text(prop, "X", "Smooth object along X axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_Y);
	RNA_def_property_ui_text(prop, "Y", "Smooth object along Y axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOOTH_Z);
	RNA_def_property_ui_text(prop, "Z", "Smooth object along Z axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10, 10, 1, 3);
	RNA_def_property_ui_text(prop, "Factor", "Strength of modifier effect");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "repeat");
	RNA_def_property_ui_range(prop, 0, 30, 1, -1);
	RNA_def_property_ui_text(prop, "Repeat", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of Vertex Group which determines influence of modifier per point");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SmoothModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}


static void rna_def_modifier_correctivesmooth(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem modifier_smooth_type_items[] = {
		{MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE, "SIMPLE", 0, "Simple",
		 "Use the average of adjacent edge-vertices"},
		{MOD_CORRECTIVESMOOTH_SMOOTH_LENGTH_WEIGHT, "LENGTH_WEIGHTED", 0, "Length Weight",
		 "Use the average of adjacent edge-vertices weighted by their length"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem modifier_rest_source_items[] = {
		{MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO, "ORCO", 0, "Original Coords",
		 "Use base mesh vert coords as the rest position"},
		{MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND, "BIND", 0, "Bind Coords",
		 "Use bind vert coords for rest position"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "CorrectiveSmoothModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Corrective Smooth Modifier", "Correct distortion caused by deformation");
	RNA_def_struct_sdna(srna, "CorrectiveSmoothModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lambda");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 3);
	RNA_def_property_ui_text(prop, "Lambda Factor", "Smooth factor effect");
	RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "repeat");
	RNA_def_property_ui_range(prop, 0, 200, 1, -1);
	RNA_def_property_ui_text(prop, "Repeat", "");
	RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

	prop = RNA_def_property(srna, "rest_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rest_source");
	RNA_def_property_enum_items(prop, modifier_rest_source_items);
	RNA_def_property_ui_text(prop, "Rest Source", "Select the source of rest positions");
	RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_rest_source_update");

	prop = RNA_def_property(srna, "smooth_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "smooth_type");
	RNA_def_property_enum_items(prop, modifier_smooth_type_items);
	RNA_def_property_ui_text(prop, "Smooth Type", "Method used for smoothing");
	RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CORRECTIVESMOOTH_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
	RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of Vertex Group which determines influence of modifier per point");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CorrectiveSmoothModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");

	prop = RNA_def_property(srna, "is_bind", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Bind current shape", "");
	RNA_def_property_boolean_funcs(prop, "rna_CorrectiveSmoothModifier_is_bind_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_only_smooth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CORRECTIVESMOOTH_ONLY_SMOOTH);
	RNA_def_property_ui_text(prop, "Only Smooth",
	                         "Apply smoothing without reconstructing the surface");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_pin_boundary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CORRECTIVESMOOTH_PIN_BOUNDARY);
	RNA_def_property_ui_text(prop, "Pin Boundaries",
	                         "Excludes boundary vertices from being smoothed");
	RNA_def_property_update(prop, 0, "rna_CorrectiveSmoothModifier_update");
}


static void rna_def_modifier_laplaciansmooth(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LaplacianSmoothModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Laplacian Smooth Modifier", "Smoothing effect modifier");
	RNA_def_struct_sdna(srna, "LaplacianSmoothModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_X);
	RNA_def_property_ui_text(prop, "X", "Smooth object along X axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_Y);
	RNA_def_property_ui_text(prop, "Y", "Smooth object along Y axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_Z);
	RNA_def_property_ui_text(prop, "Z", "Smooth object along Z axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_volume_preserve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME);
	RNA_def_property_ui_text(prop, "Preserve Volume", "Apply volume preservation after smooth");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normalized", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_LAPLACIANSMOOTH_NORMALIZED);
	RNA_def_property_ui_text(prop, "Normalized", "Improve and stabilize the enhanced shape");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "lambda_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lambda");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -1000.0, 1000.0, 5, 3);
	RNA_def_property_ui_text(prop, "Lambda Factor", "Smooth factor effect");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "lambda_border", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lambda_border");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -1000.0, 1000.0, 5, 3);
	RNA_def_property_ui_text(prop, "Lambda Border", "Lambda factor in border");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "repeat");
	RNA_def_property_ui_range(prop, 0, 200, 1, -1);
	RNA_def_property_ui_text(prop, "Repeat", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of Vertex Group which determines influence of modifier per point");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_LaplacianSmoothModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_cast(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_cast_type_items[] = {
		{MOD_CAST_TYPE_SPHERE, "SPHERE", 0, "Sphere", ""},
		{MOD_CAST_TYPE_CYLINDER, "CYLINDER", 0, "Cylinder", ""},
		{MOD_CAST_TYPE_CUBOID, "CUBOID", 0, "Cuboid", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "CastModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Cast Modifier", "Modifier to cast to other shapes");
	RNA_def_struct_sdna(srna, "CastModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_CAST);

	prop = RNA_def_property(srna, "cast_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_cast_type_items);
	RNA_def_property_ui_text(prop, "Cast Type", "Target object shape");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object",
	                         "Control object: if available, its location determines the center of the effect");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_CastModifier_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_X);
	RNA_def_property_ui_text(prop, "X", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_Y);
	RNA_def_property_ui_text(prop, "Y", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_Z);
	RNA_def_property_ui_text(prop, "Z", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_radius_as_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_SIZE_FROM_RADIUS);
	RNA_def_property_ui_text(prop, "From Radius", "Use radius as size of projection shape (0 = auto)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_transform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_CAST_USE_OB_TRANSFORM);
	RNA_def_property_ui_text(prop, "Use transform", "Use object transform to control projection shape");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10, 10, 5, 2);
	RNA_def_property_ui_text(prop, "Factor", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 5, 2);
	RNA_def_property_ui_text(prop, "Radius",
	                         "Only deform vertices within this distance from the center of the effect "
	                         "(leave as 0 for infinite.)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 5, 2);
	RNA_def_property_ui_text(prop, "Size", "Size of projection shape (leave as 0 for auto)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CastModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_meshdeform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
#if 0
	static const EnumPropertyItem prop_mode_items[] = {
		{0, "VOLUME", 0, "Volume", "Bind to volume inside cage mesh"},
		{1, "SURFACE", 0, "Surface", "Bind to surface of cage mesh"},
		{0, NULL, 0, NULL, NULL}
	};
#endif

	srna = RNA_def_struct(brna, "MeshDeformModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "MeshDeform Modifier", "Mesh deformation modifier to deform with other meshes");
	RNA_def_struct_sdna(srna, "MeshDeformModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Mesh object to deform with");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_MeshDeformModifier_object_set", NULL, "rna_Mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "is_bound", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MeshDeformModifier_is_bound_get", NULL);
	RNA_def_property_ui_text(prop, "Bound", "Whether geometry has been bound to control cage");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MDEF_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshDeformModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "precision", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gridsize");
	RNA_def_property_range(prop, 2, 10);
	RNA_def_property_ui_text(prop, "Precision", "The grid size for binding");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_dynamic_bind", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MDEF_DYNAMIC_BIND);
	RNA_def_property_ui_text(prop, "Dynamic",
	                         "Recompute binding dynamically on top of other deformers "
	                         "(slower and more memory consuming)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

#if 0
	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "Method of binding vertices are bound to cage mesh");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
#endif
}

static void rna_def_modifier_particlesystem(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ParticleSystemModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "ParticleSystem Modifier", "Particle system simulation modifier");
	RNA_def_struct_sdna(srna, "ParticleSystemModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_PARTICLES);

	prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "psys");
	RNA_def_property_ui_text(prop, "Particle System", "Particle System that this modifier controls");
}

static void rna_def_modifier_particleinstance(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem particleinstance_space[] = {
		{eParticleInstanceSpace_Local, "LOCAL", 0, "Local", "Use offset from the particle object in the instance object"},
		{eParticleInstanceSpace_World, "WORLD", 0, "World", "Use world space offset in the instance object"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ParticleInstanceModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "ParticleInstance Modifier", "Particle system instancing modifier");
	RNA_def_struct_sdna(srna, "ParticleInstanceModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_PARTICLES);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Mesh_object_poll");
	RNA_def_property_ui_text(prop, "Object", "Object that has the particle system");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "particle_system_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "psys");
	RNA_def_property_range(prop, 1, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Particle System Number", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_pointer_funcs(prop, "rna_ParticleInstanceModifier_particle_system_get", "rna_ParticleInstanceModifier_particle_system_set",
	                               NULL, "rna_ParticleInstanceModifier_particle_system_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Particle System", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axis");
	RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
	RNA_def_property_ui_text(prop, "Axis", "Pole axis for rotation");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "space");
	RNA_def_property_enum_items(prop, particleinstance_space);
	RNA_def_property_ui_text(prop, "Space", "Space to use for copying mesh data");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Parents);
	RNA_def_property_ui_text(prop, "Normal", "Create instances from normal particles");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Children);
	RNA_def_property_ui_text(prop, "Children", "Create instances from child particles");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_path", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Path);
	RNA_def_property_ui_text(prop, "Path", "Create instances along particle paths");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "show_unborn", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Unborn);
	RNA_def_property_ui_text(prop, "Unborn", "Show instances when particles are unborn");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "show_alive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Alive);
	RNA_def_property_ui_text(prop, "Alive", "Show instances when particles are alive");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "show_dead", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_Dead);
	RNA_def_property_ui_text(prop, "Dead", "Show instances when particles are dead");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_preserve_shape", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_KeepShape);
	RNA_def_property_ui_text(prop, "Keep Shape", "Don't stretch the object");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eParticleInstanceFlag_UseSize);
	RNA_def_property_ui_text(prop, "Size", "Use particle size to scale the instances");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "position", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "position");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Position", "Position along path");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "random_position", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "random_position");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Random Position", "Randomize position along path");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "rotation");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Rotation", "Rotation around path");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "random_rotation", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "random_rotation");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Random Rotation", "Randomize rotation around path");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "particle_amount", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Particle Amount", "Amount of particles to use for instancing");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "particle_offset", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Particle Offset", "Relative offset of particles to use for instancing, to avoid overlap of multiple instances");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "index_layer_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "index_layer_name");
	RNA_def_property_ui_text(prop, "Index Layer Name", "Custom data layer name for the index");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "value_layer_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "value_layer_name");
	RNA_def_property_ui_text(prop, "Value Layer Name", "Custom data layer name for the randomized value");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_explode(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ExplodeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Explode Modifier", "Explosion effect modifier based on a particle system");
	RNA_def_struct_sdna(srna, "ExplodeModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_EXPLODE);

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ExplodeModifier_vgroup_get", "rna_ExplodeModifier_vgroup_length",
	                              "rna_ExplodeModifier_vgroup_set");
	RNA_def_property_ui_text(prop, "Vertex Group", "");

	prop = RNA_def_property(srna, "protect", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Protect", "Clean vertex group edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_edge_cut", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_EdgeCut);
	RNA_def_property_ui_text(prop, "Cut Edges", "Cut face edges for nicer shrapnel");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "show_unborn", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Unborn);
	RNA_def_property_ui_text(prop, "Unborn", "Show mesh when particles are unborn");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "show_alive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Alive);
	RNA_def_property_ui_text(prop, "Alive", "Show mesh when particles are alive");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "show_dead", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_Dead);
	RNA_def_property_ui_text(prop, "Dead", "Show mesh when particles are dead");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eExplodeFlag_PaSize);
	RNA_def_property_ui_text(prop, "Size", "Use particle size for the shrapnel");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "particle_uv", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvname");
	RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME);
	RNA_def_property_ui_text(prop, "Particle UV", "UV map to change with particle age");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_cloth(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ClothModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Cloth Modifier", "Cloth simulation modifier");
	RNA_def_struct_sdna(srna, "ClothModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_CLOTH);

	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "sim_parms");
	RNA_def_property_ui_text(prop, "Cloth Settings", "");

	prop = RNA_def_property(srna, "collision_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "coll_parms");
	RNA_def_property_ui_text(prop, "Cloth Collision Settings", "");

	prop = RNA_def_property(srna, "solver_result", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ClothSolverResult");
	RNA_def_property_pointer_sdna(prop, NULL, "solver_result");
	RNA_def_property_ui_text(prop, "Solver Result", "");

	prop = RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Point Cache", "");

	prop = RNA_def_property(srna, "hair_grid_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "hair_grid_min");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair Grid Minimum", "");

	prop = RNA_def_property(srna, "hair_grid_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "hair_grid_max");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair Grid Maximum", "");

	prop = RNA_def_property(srna, "hair_grid_resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hair_grid_res");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair Grid Resolution", "");
}

static void rna_def_modifier_smoke(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_smoke_type_items[] = {
		{0, "NONE", 0, "None", ""},
		{MOD_SMOKE_TYPE_DOMAIN, "DOMAIN", 0, "Domain", ""},
		{MOD_SMOKE_TYPE_FLOW, "FLOW", 0, "Flow", "Inflow/Outflow"},
		{MOD_SMOKE_TYPE_COLL, "COLLISION", 0, "Collision", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "SmokeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Smoke Modifier", "Smoke simulation modifier");
	RNA_def_struct_sdna(srna, "SmokeModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SMOKE);

	prop = RNA_def_property(srna, "domain_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "domain");
	RNA_def_property_ui_text(prop, "Domain Settings", "");

	prop = RNA_def_property(srna, "flow_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "flow");
	RNA_def_property_ui_text(prop, "Flow Settings", "");

	prop = RNA_def_property(srna, "coll_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "coll");
	RNA_def_property_ui_text(prop, "Collision Settings", "");

	prop = RNA_def_property(srna, "smoke_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_smoke_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Smoke_set_type");
}

static void rna_def_modifier_dynamic_paint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DynamicPaintModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Dynamic Paint Modifier", "Dynamic Paint modifier");
	RNA_def_struct_sdna(srna, "DynamicPaintModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_DYNAMICPAINT);

	prop = RNA_def_property(srna, "canvas_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "canvas");
	RNA_def_property_ui_text(prop, "Canvas Settings", "");

	prop = RNA_def_property(srna, "brush_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "brush");
	RNA_def_property_ui_text(prop, "Brush Settings", "");

	prop = RNA_def_property(srna, "ui_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rna_enum_prop_dynamicpaint_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
}

static void rna_def_modifier_collision(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CollisionModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Collision Modifier",
	                       "Collision modifier defining modifier stack position used for collision");
	RNA_def_struct_sdna(srna, "CollisionModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_PHYSICS);

	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "CollisionSettings");
	RNA_def_property_pointer_funcs(prop, "rna_CollisionModifier_settings_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Settings", "");
}

static void rna_def_modifier_bevel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_limit_method_items[] = {
		{0, "NONE", 0, "None", "Bevel the entire mesh by a constant amount"},
		{MOD_BEVEL_ANGLE, "ANGLE", 0, "Angle", "Only bevel edges with sharp enough angles between faces"},
		{MOD_BEVEL_WEIGHT, "WEIGHT", 0, "Weight",
		                   "Use bevel weights to determine how much bevel is applied in edge mode"},
		{MOD_BEVEL_VGROUP, "VGROUP", 0, "Vertex Group",
		                   "Use vertex group weights to select whether vertex or edge is beveled"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_val_type_items[] = {
		{MOD_BEVEL_AMT_OFFSET, "OFFSET", 0, "Offset", "Amount is offset of new edges from original"},
		{MOD_BEVEL_AMT_WIDTH, "WIDTH", 0, "Width", "Amount is width of new face"},
		{MOD_BEVEL_AMT_DEPTH, "DEPTH", 0, "Depth", "Amount is perpendicular distance from original edge to bevel face"},
		{MOD_BEVEL_AMT_PERCENT, "PERCENT", 0, "Percent", "Amount is percent of adjacent edge length"},
		{0, NULL, 0, NULL, NULL}
	};

	/* TO BE DEPRECATED */
	static const EnumPropertyItem prop_edge_weight_method_items[] = {
		{0, "AVERAGE", 0, "Average", ""},
		{MOD_BEVEL_EMIN, "SHARPEST", 0, "Sharpest", ""},
		{MOD_BEVEL_EMAX, "LARGEST", 0, "Largest", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_harden_normals_items[] = {
		{ MOD_BEVEL_HN_NONE, "HN_NONE", 0, "Off", "Do not use Harden Normals" },
		{ MOD_BEVEL_HN_FACE, "HN_FACE", 0, "Face Area", "Use faces as weight" },
		{ MOD_BEVEL_HN_ADJ, "HN_ADJ", 0, "Vertex average", "Use adjacent vertices as weight" },
		{ MOD_BEVEL_FIX_SHA, "FIX_SHA", 0, "Fix shading", "Fix normal shading continuity" },
		{ 0, NULL, 0, NULL, NULL },
	};

	srna = RNA_def_struct(brna, "BevelModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Bevel Modifier", "Bevel modifier to make edges and vertices more rounded");
	RNA_def_struct_sdna(srna, "BevelModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_BEVEL);

	prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 0.1, 4);
	RNA_def_property_ui_text(prop, "Width", "Bevel value/amount");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "segments", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "res");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Segments", "Number of segments for round edges/verts");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_only_vertices", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_BEVEL_VERT);
	RNA_def_property_ui_text(prop, "Only Vertices", "Bevel verts/corners, not edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "limit_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "lim_flags");
	RNA_def_property_enum_items(prop, prop_limit_method_items);
	RNA_def_property_ui_text(prop, "Limit Method", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* TO BE DEPRECATED */
	prop = RNA_def_property(srna, "edge_weight_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "e_flags");
	RNA_def_property_enum_items(prop, prop_edge_weight_method_items);
	RNA_def_property_ui_text(prop, "Edge Weight Method", "What edge weight to use for weighting a vertex");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "angle_limit", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "bevel_angle");
	RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
	RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 2);
	RNA_def_property_ui_text(prop, "Angle", "Angle above which to bevel edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_BevelModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_clamp_overlap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flags", MOD_BEVEL_OVERLAP_OK);
	RNA_def_property_ui_text(prop, "Clamp Overlap", "Clamp the width to avoid overlap");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "offset_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "val_flags");
	RNA_def_property_enum_items(prop, prop_val_type_items);
	RNA_def_property_ui_text(prop, "Amount Type", "What distance Width measures");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "profile", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.05, 2);
	RNA_def_property_ui_text(prop, "Profile", "The profile shape (0.5 = round)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "material", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mat");
	RNA_def_property_range(prop, -1, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Material", "Material index of generated faces, -1 for automatic");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "loop_slide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flags", MOD_BEVEL_EVEN_WIDTHS);
	RNA_def_property_ui_text(prop, "Loop Slide", "Prefer sliding along edges to having even widths");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mark_seam", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_flags", MOD_BEVEL_MARK_SEAM);
	RNA_def_property_ui_text(prop, "Mark Seams", "Mark Seams along beveled edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mark_sharp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_flags", MOD_BEVEL_MARK_SHARP);
	RNA_def_property_ui_text(prop, "Mark Sharp", "Mark beveled edges as sharp");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "hnmode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_harden_normals_items);
	RNA_def_property_ui_text(prop, "Normal Mode", "Weighting mode for Harden Normals");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "hn_strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 1, 2);
	RNA_def_property_ui_text(prop, "Normal Strength", "Strength of calculated normal");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "set_wn_strength", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_BEVEL_SET_WN_STR);
	RNA_def_property_ui_text(prop, "Face Strength", "Set face strength of beveled faces for use in WN Modifier");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_shrinkwrap(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem shrink_type_items[] = {
		{MOD_SHRINKWRAP_NEAREST_SURFACE, "NEAREST_SURFACEPOINT", 0, "Nearest Surface Point",
		                                 "Shrink the mesh to the nearest target surface"},
		{MOD_SHRINKWRAP_PROJECT, "PROJECT", 0, "Project",
		                         "Shrink the mesh to the nearest target surface along a given axis"},
		{MOD_SHRINKWRAP_NEAREST_VERTEX, "NEAREST_VERTEX", 0, "Nearest Vertex",
		                                "Shrink the mesh to the nearest target vertex"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem shrink_face_cull_items[] = {
		{0, "OFF", 0, "Off", "No culling"},
		{MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE, "FRONT", 0, "Front", "No projection when in front of the face"},
		{MOD_SHRINKWRAP_CULL_TARGET_BACKFACE, "BACK", 0, "Back", "No projection when behind the face"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ShrinkwrapModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Shrinkwrap Modifier",
	                       "Shrink wrapping modifier to shrink wrap and object to a target");
	RNA_def_struct_sdna(srna, "ShrinkwrapModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SHRINKWRAP);

	prop = RNA_def_property(srna, "wrap_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "shrinkType");
	RNA_def_property_enum_items(prop, shrink_type_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "cull_face", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "shrinkOpts");
	RNA_def_property_enum_items(prop, shrink_face_cull_items);
	RNA_def_property_enum_funcs(prop, "rna_ShrinkwrapModifier_face_cull_get",
	                            "rna_ShrinkwrapModifier_face_cull_set", NULL);
	RNA_def_property_ui_text(prop, "Face Cull",
	                         "Stop vertices from projecting to a face on the target when facing towards/away");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Target", "Mesh target to shrink to");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ShrinkwrapModifier_target_set", NULL, "rna_Mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "auxiliary_target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "auxTarget");
	RNA_def_property_ui_text(prop, "Auxiliary Target", "Additional mesh target to shrink to");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_ShrinkwrapModifier_auxTarget_set", NULL, "rna_Mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ShrinkwrapModifier_vgroup_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "keepDist");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100, 100, 1, 2);
	RNA_def_property_ui_text(prop, "Offset", "Distance to keep from the target");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "project_limit", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "projLimit");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 1, 2);
	RNA_def_property_ui_text(prop, "Project Limit", "Limit the distance used for projection (zero disables)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_project_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS);
	RNA_def_property_ui_text(prop, "X", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_project_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS);
	RNA_def_property_ui_text(prop, "Y", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_project_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "projAxis", MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS);
	RNA_def_property_ui_text(prop, "Z", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "subsurf_levels", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "subsurfLevels");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_range(prop, 0, 6, 1, -1);
	RNA_def_property_ui_text(prop, "Subsurf Levels",
	                         "Number of subdivisions that must be performed before extracting vertices' "
	                         "positions and normals");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_negative_direction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR);
	RNA_def_property_ui_text(prop, "Negative", "Allow vertices to move in the negative direction of axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_positive_direction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR);
	RNA_def_property_ui_text(prop, "Positive", "Allow vertices to move in the positive direction of axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_keep_above_surface", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE);
	RNA_def_property_ui_text(prop, "Keep Above Surface", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shrinkOpts", MOD_SHRINKWRAP_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_fluidsim(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "FluidSimulationModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Fluid Simulation Modifier", "Fluid simulation modifier");
	RNA_def_struct_sdna(srna, "FluidsimModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_FLUIDSIM);

	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "fss");
	RNA_def_property_ui_text(prop, "Settings", "Settings for how this object is used in the fluid simulation");
}

static void rna_def_modifier_mask(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem modifier_mask_mode_items[] = {
		{MOD_MASK_MODE_VGROUP, "VERTEX_GROUP", 0, "Vertex Group", ""},
		{MOD_MASK_MODE_ARM, "ARMATURE", 0, "Armature", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "MaskModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Mask Modifier", "Mask modifier to hide parts of the mesh");
	RNA_def_struct_sdna(srna, "MaskModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MASK);

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, modifier_mask_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "armature", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob_arm");
	RNA_def_property_ui_text(prop, "Armature", "Armature to use as source of bones to mask");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_MaskModifier_ob_arm_set", NULL, "rna_Armature_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MaskModifier_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_MASK_INV);
	RNA_def_property_ui_text(prop, "Invert", "Use vertices that are not part of region defined");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_simpledeform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem simple_deform_mode_items[] = {
		{MOD_SIMPLEDEFORM_MODE_TWIST, "TWIST", 0, "Twist", "Rotate around the Z axis of the modifier space"},
		{MOD_SIMPLEDEFORM_MODE_BEND, "BEND", 0, "Bend", "Bend the mesh over the Z axis of the modifier space"},
		{MOD_SIMPLEDEFORM_MODE_TAPER, "TAPER", 0, "Taper", "Linearly scale along Z axis of the modifier space"},
		{MOD_SIMPLEDEFORM_MODE_STRETCH, "STRETCH", 0, "Stretch",
		                                "Stretch the object along the Z axis of the modifier space"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "SimpleDeformModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "SimpleDeform Modifier",
	                       "Simple deformation modifier to apply effects such as twisting and bending");
	RNA_def_struct_sdna(srna, "SimpleDeformModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SIMPLEDEFORM);

	prop = RNA_def_property(srna, "deform_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, simple_deform_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SimpleDeformModifier_vgroup_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "deform_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
	RNA_def_property_ui_text(prop, "Axis", "Deform around local axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "origin", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Origin", "Offset the origin and orientation of the deformation");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1.0, 3);
	RNA_def_property_ui_text(prop, "Factor", "Amount to deform object");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "factor");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_float_default(prop, DEG2RADF(45.0f));
	RNA_def_property_ui_range(prop, DEG2RAD(-360.0), DEG2RAD(360.0), 10.0, 3);
	RNA_def_property_ui_text(prop, "Angle", "Angle of deformation");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "limits", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limit");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 5, 2);
	RNA_def_property_ui_text(prop, "Limits", "Lower/Upper limits for deform");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "lock_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "axis", MOD_SIMPLEDEFORM_LOCK_AXIS_X);
	RNA_def_property_ui_text(prop, "X", "Do not allow deformation along the X axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "lock_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "axis", MOD_SIMPLEDEFORM_LOCK_AXIS_Y);
	RNA_def_property_ui_text(prop, "Y", "Do not allow deformation along the Y axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "lock_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "axis", MOD_SIMPLEDEFORM_LOCK_AXIS_Z);
	RNA_def_property_ui_text(prop, "Z", "Do not allow deformation along the Z axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SIMPLEDEFORM_FLAG_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_surface(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "SurfaceModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Surface Modifier",
	                       "Surface modifier defining modifier stack position used for surface fields");
	RNA_def_struct_sdna(srna, "SurfaceModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_PHYSICS);
}

static void rna_def_modifier_solidify(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SolidifyModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Solidify Modifier",
	                       "Create a solid skin by extruding, compensating for sharp angles");
	RNA_def_struct_sdna(srna, "SolidifyModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SOLIDIFY);

	prop = RNA_def_property(srna, "thickness", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10, 10, 0.1, 4);
	RNA_def_property_ui_text(prop, "Thickness", "Thickness of the shell");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "thickness_clamp", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "offset_clamp");
	RNA_def_property_range(prop, 0, 100.0);
	RNA_def_property_ui_range(prop, 0, 2.0, 0.1, 4);
	RNA_def_property_ui_text(prop, "Clamp", "Offset clamp based on geometry scale");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "thickness_vertex_group", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "offset_fac_vg");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
	RNA_def_property_ui_text(prop, "Vertex Group Factor",
	                         "Thickness factor to use for zero vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "offset_fac");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -1, 1, 0.1, 4);
	RNA_def_property_ui_text(prop, "Offset", "Offset the thickness from the center");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "edge_crease_inner", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "crease_inner");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
	RNA_def_property_ui_text(prop, "Inner Crease", "Assign a crease to inner edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "edge_crease_outer", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "crease_outer");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
	RNA_def_property_ui_text(prop, "Outer Crease", "Assign a crease to outer edges");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "edge_crease_rim", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "crease_rim");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
	RNA_def_property_ui_text(prop, "Rim Crease", "Assign a crease to the edges making up the rim");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "material_offset", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mat_ofs");
	RNA_def_property_range(prop, SHRT_MIN, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Material Offset", "Offset material index of generated faces");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "material_offset_rim", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mat_ofs_rim");
	RNA_def_property_range(prop, SHRT_MIN, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Rim Material Offset", "Offset material index of generated rim faces");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SolidifyModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_rim", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_RIM);
	RNA_def_property_ui_text(prop, "Fill Rim",
	                         "Create edge loops between the inner and outer surfaces on face edges "
	                         "(slow, disable when not needed)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_even_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_EVEN);
	RNA_def_property_ui_text(prop, "Even Thickness",
	                         "Maintain thickness by adjusting for sharp corners (slow, disable when not needed)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_quality_normals", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_NORMAL_CALC);
	RNA_def_property_ui_text(prop, "High Quality Normals",
	                         "Calculate normals which result in more even thickness (slow, disable when not needed)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_VGROUP_INV);
	RNA_def_property_ui_text(prop, "Vertex Group Invert", "Invert the vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_flip_normals", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_FLIP);
	RNA_def_property_ui_text(prop, "Flip Normals", "Invert the face direction");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_rim_only", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SOLIDIFY_NOSHELL);
	RNA_def_property_ui_text(prop, "Only Rim", "Only add the rim to the original data");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_screw(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ScrewModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Screw Modifier", "Revolve edges");
	RNA_def_struct_sdna(srna, "ScrewModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SCREW);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob_axis");
	RNA_def_property_ui_text(prop, "Object", "Object to define the screw axis");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "steps", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_range(prop, 2, 10000);
	RNA_def_property_ui_range(prop, 3, 512, 1, -1);
	RNA_def_property_ui_text(prop, "Steps", "Number of steps in the revolution");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "render_steps", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_range(prop, 2, 10000);
	RNA_def_property_ui_range(prop, 2, 512, 1, -1);
	RNA_def_property_ui_text(prop, "Render Steps", "Number of steps in the revolution");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "iter");
	RNA_def_property_range(prop, 1, 10000);
	RNA_def_property_ui_range(prop, 1, 100, 1, -1);
	RNA_def_property_ui_text(prop, "Iterations", "Number of times to apply the screw operation");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
	RNA_def_property_ui_text(prop, "Axis", "Screw axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_ui_range(prop, -M_PI * 2, M_PI * 2, 10, -1);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Angle", "Angle of revolution");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "screw_offset", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "screw_ofs");
	RNA_def_property_ui_text(prop, "Screw", "Offset the revolution along its axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "merge_threshold", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "merge_dist");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 1, 4);
	RNA_def_property_ui_text(prop, "Merge Distance", "Limit below which to merge vertices");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normal_flip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_NORMAL_FLIP);
	RNA_def_property_ui_text(prop, "Flip", "Flip normals of lathed faces");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normal_calculate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_NORMAL_CALC);
	RNA_def_property_ui_text(prop, "Calc Order", "Calculate the order of edges (needed for meshes, but not curves)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_object_screw_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_OBJECT_OFFSET);
	RNA_def_property_ui_text(prop, "Object Screw", "Use the distance between the objects to make a screw");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Vertex merging parameters */
	prop = RNA_def_property(srna, "use_merge_vertices", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_MERGE);
	RNA_def_property_ui_text(prop, "Merge Vertices", "Merge adjacent vertices (screw offset must be zero)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_smooth_shade", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_SMOOTH_SHADING);
	RNA_def_property_ui_text(prop, "Smooth Shading", "Output faces with smooth shading rather than flat shaded");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_stretch_u", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_UV_STRETCH_U);
	RNA_def_property_ui_text(prop, "Stretch U", "Stretch the U coordinates between 0-1 when UV's are present");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_stretch_v", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_UV_STRETCH_V);
	RNA_def_property_ui_text(prop, "Stretch V", "Stretch the V coordinates between 0-1 when UV's are present");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

#if 0
	prop = RNA_def_property(srna, "use_angle_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SCREW_OBJECT_ANGLE);
	RNA_def_property_ui_text(prop, "Object Angle", "Use the angle between the objects rather than the fixed angle");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
#endif
}

static void rna_def_modifier_uvwarp(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "UVWarpModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "UVWarp Modifier", "Add target position to uv coordinates");
	RNA_def_struct_sdna(srna, "UVWarpModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_UVPROJECT);

	prop = RNA_def_property(srna, "axis_u", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axis_u");
	RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
	RNA_def_property_ui_text(prop, "U-Axis", "Pole axis for rotation");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "axis_v", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axis_v");
	RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
	RNA_def_property_ui_text(prop, "V-Axis", "Pole axis for rotation");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "center");
	RNA_def_property_ui_text(prop, "UV Center", "Center point for rotate/scale");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "object_from", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object_src");
	RNA_def_property_ui_text(prop, "Object From", "Object defining offset");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "bone_from", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "bone_src");
	RNA_def_property_ui_text(prop, "Bone From", "Bone defining offset");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "object_to", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object_dst");
	RNA_def_property_ui_text(prop, "Object To", "Object defining offset");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "bone_to", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "bone_dst");
	RNA_def_property_ui_text(prop, "Bone To", "Bone defining offset");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_UVWarpModifier_vgroup_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Layer", "UV Layer name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_UVWarpModifier_uvlayer_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_weightvg_mask(BlenderRNA *UNUSED(brna), StructRNA *srna,
                                           const char *mask_vgroup_setter, const char *mask_uvlayer_setter)
{
	static const EnumPropertyItem weightvg_mask_tex_map_items[] = {
		{MOD_DISP_MAP_LOCAL, "LOCAL", 0, "Local", "Use local generated coordinates"},
		{MOD_DISP_MAP_GLOBAL, "GLOBAL", 0, "Global", "Use global coordinates"},
		{MOD_DISP_MAP_OBJECT, "OBJECT", 0, "Object", "Use local generated coordinates of another object"},
		{MOD_DISP_MAP_UV, "UV", 0, "UV", "Use coordinates from an UV layer"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem weightvg_mask_tex_used_items[] = {
		{MOD_WVG_MASK_TEX_USE_INT, "INT", 0, "Intensity", ""},
		{MOD_WVG_MASK_TEX_USE_RED, "RED", 0, "Red", ""},
		{MOD_WVG_MASK_TEX_USE_GREEN, "GREEN", 0, "Green", ""},
		{MOD_WVG_MASK_TEX_USE_BLUE, "BLUE", 0, "Blue", ""},
		{MOD_WVG_MASK_TEX_USE_HUE, "HUE", 0, "Hue", ""},
		{MOD_WVG_MASK_TEX_USE_SAT, "SAT", 0, "Saturation", ""},
		{MOD_WVG_MASK_TEX_USE_VAL, "VAL", 0, "Value", ""},
		{MOD_WVG_MASK_TEX_USE_ALPHA, "ALPHA", 0, "Alpha", ""},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	prop = RNA_def_property(srna, "mask_constant", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
	RNA_def_property_ui_text(prop, "Influence", "Global influence of current modifications on vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mask_vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "mask_defgrp_name");
	RNA_def_property_ui_text(prop, "Mask VGroup", "Masking vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, mask_vgroup_setter);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mask_texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Masking Tex", "Masking texture");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mask_tex_use_channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, weightvg_mask_tex_used_items);
	RNA_def_property_ui_text(prop, "Use Channel", "Which texture channel to use for masking");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mask_tex_mapping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, weightvg_mask_tex_map_items);
	RNA_def_property_ui_text(prop, "Texture Coordinates", "Which texture coordinates "
	                         "to use for mapping");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "mask_tex_uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "mask_tex_uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Map", "UV map name");
	RNA_def_property_string_funcs(prop, NULL, NULL, mask_uvlayer_setter);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mask_tex_map_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mask_tex_map_obj");
	RNA_def_property_ui_text(prop, "Texture Coordinate Object", "Which object to take texture "
	                         "coordinates from");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");
}

static void rna_def_modifier_weightvgedit(BlenderRNA *brna)
{
	static const EnumPropertyItem weightvg_edit_falloff_type_items[] = {
		{MOD_WVG_MAPPING_NONE, "LINEAR", ICON_LINCURVE, "Linear", "Null action"},
		{MOD_WVG_MAPPING_CURVE, "CURVE", ICON_RNDCURVE, "Custom Curve", ""},
		{MOD_WVG_MAPPING_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", ""},
		{MOD_WVG_MAPPING_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
		{MOD_WVG_MAPPING_ROOT, "ROOT", ICON_ROOTCURVE, "Root", ""},
		{MOD_WVG_MAPPING_SPHERE, "ICON_SPHERECURVE", ICON_SPHERECURVE, "Sphere", ""},
		{MOD_WVG_MAPPING_RANDOM, "RANDOM", ICON_RNDCURVE, "Random", ""},
		{MOD_WVG_MAPPING_STEP, "STEP", ICON_NOCURVE /* Would need a better icon... */, "Median Step",
		                       "Map all values below 0.5 to 0.0, and all others to 1.0"},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "VertexWeightEditModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "WeightVG Edit Modifier",
	                       "Edit the weights of vertices in a group");
	RNA_def_struct_sdna(srna, "WeightVGEditModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_VERTEX_WEIGHT);

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightVGEditModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, weightvg_edit_falloff_type_items);
	RNA_def_property_ui_text(prop, "Falloff Type", "How weights are mapped to their new values");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_add", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edit_flags", MOD_WVG_EDIT_ADD2VG);
	RNA_def_property_ui_text(prop, "Group Add", "Add vertices with weight over threshold "
	                         "to vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_remove", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edit_flags", MOD_WVG_EDIT_REMFVG);
	RNA_def_property_ui_text(prop, "Group Remove", "Remove vertices with weight below threshold "
	                         "from vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "default_weight", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
	RNA_def_property_ui_text(prop, "Default Weight", "Default weight a vertex will have if "
	                         "it is not in the vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "map_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "cmap_curve");
	RNA_def_property_ui_text(prop, "Mapping Curve", "Custom mapping curve");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "add_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "add_threshold");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
	RNA_def_property_ui_text(prop, "Add Threshold", "Lower bound for a vertex's weight "
	                         "to be added to the vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "remove_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rem_threshold");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
	RNA_def_property_ui_text(prop, "Remove Threshold", "Upper bound for a vertex's weight "
	                         "to be removed from the vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Common masking properties. */
	rna_def_modifier_weightvg_mask(brna, srna, "rna_WeightVGEditModifier_mask_defgrp_name_set",
	                               "rna_WeightVGEditModifier_mask_tex_uvlayer_name_set");
}

static void rna_def_modifier_weightvgmix(BlenderRNA *brna)
{
	static const EnumPropertyItem weightvg_mix_modes_items[] = {
		{MOD_WVG_MIX_SET, "SET", 0, "Replace", "Replace VGroup A's weights by VGroup B's ones"},
		{MOD_WVG_MIX_ADD, "ADD", 0, "Add", "Add VGroup B's weights to VGroup A's ones"},
		{MOD_WVG_MIX_SUB, "SUB", 0, "Subtract", "Subtract VGroup B's weights from VGroup A's ones"},
		{MOD_WVG_MIX_MUL, "MUL", 0, "Multiply", "Multiply VGroup A's weights by VGroup B's ones"},
		{MOD_WVG_MIX_DIV, "DIV", 0, "Divide", "Divide VGroup A's weights by VGroup B's ones"},
		{MOD_WVG_MIX_DIF, "DIF", 0, "Difference", "Difference between VGroup A's and VGroup B's weights"},
		{MOD_WVG_MIX_AVG, "AVG", 0, "Average", "Average value of VGroup A's and VGroup B's weights"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem weightvg_mix_set_items[] = {
		{MOD_WVG_SET_ALL, "ALL", 0, "All", "Affect all vertices (might add some to VGroup A)"},
		{MOD_WVG_SET_A,   "A",   0, "VGroup A", "Affect vertices in VGroup A"},
		{MOD_WVG_SET_B,   "B",   0, "VGroup B", "Affect vertices in VGroup B (might add some to VGroup A)"},
		{MOD_WVG_SET_OR,  "OR",  0, "VGroup A or B",
		                  "Affect vertices in at least one of both VGroups (might add some to VGroup A)"},
		{MOD_WVG_SET_AND, "AND", 0, "VGroup A and B", "Affect vertices in both groups"},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "VertexWeightMixModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "WeightVG Mix Modifier",
	                       "Mix the weights of two vertex groups");
	RNA_def_struct_sdna(srna, "WeightVGMixModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_VERTEX_WEIGHT);

	prop = RNA_def_property(srna, "vertex_group_a", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name_a");
	RNA_def_property_ui_text(prop, "Vertex Group A", "First vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightVGMixModifier_defgrp_name_a_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group_b", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name_b");
	RNA_def_property_ui_text(prop, "Vertex Group B", "Second vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightVGMixModifier_defgrp_name_b_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "default_weight_a", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
	RNA_def_property_ui_text(prop, "Default Weight A", "Default weight a vertex will have if "
	                         "it is not in the first A vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "default_weight_b", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
	RNA_def_property_ui_text(prop, "Default Weight B", "Default weight a vertex will have if "
	                         "it is not in the second B vgroup");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, weightvg_mix_modes_items);
	RNA_def_property_ui_text(prop, "Mix Mode", "How weights from vgroup B affect weights "
	                         "of vgroup A");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mix_set", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, weightvg_mix_set_items);
	RNA_def_property_ui_text(prop, "Vertex Set", "Which vertices should be affected");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Common masking properties. */
	rna_def_modifier_weightvg_mask(brna, srna, "rna_WeightVGMixModifier_mask_defgrp_name_set",
	                               "rna_WeightVGMixModifier_mask_tex_uvlayer_name_set");
}

static void rna_def_modifier_weightvgproximity(BlenderRNA *brna)
{
	static const EnumPropertyItem weightvg_proximity_modes_items[] = {
		{MOD_WVG_PROXIMITY_OBJECT, "OBJECT", 0, "Object",
		                           "Use distance between affected and target objects"},
		{MOD_WVG_PROXIMITY_GEOMETRY, "GEOMETRY", 0, "Geometry",
		                             "Use distance between affected object's vertices and target "
		                             "object, or target object's geometry"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem proximity_geometry_items[] = {
		{MOD_WVG_PROXIMITY_GEOM_VERTS, "VERTEX", 0, "Vertex", "Compute distance to nearest vertex"},
		{MOD_WVG_PROXIMITY_GEOM_EDGES, "EDGE", 0, "Edge", "Compute distance to nearest edge"},
		{MOD_WVG_PROXIMITY_GEOM_FACES, "FACE", 0, "Face", "Compute distance to nearest face"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem weightvg_proximity_falloff_type_items[] = {
		{MOD_WVG_MAPPING_NONE, "LINEAR", ICON_LINCURVE, "Linear", "Null action"},
		/* No curve mapping here! */
		{MOD_WVG_MAPPING_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", ""},
		{MOD_WVG_MAPPING_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
		{MOD_WVG_MAPPING_ROOT, "ROOT", ICON_ROOTCURVE, "Root", ""},
		{MOD_WVG_MAPPING_SPHERE, "ICON_SPHERECURVE", ICON_SPHERECURVE, "Sphere", ""},
		{MOD_WVG_MAPPING_RANDOM, "RANDOM", ICON_RNDCURVE, "Random", ""},
		{MOD_WVG_MAPPING_STEP, "STEP", ICON_NOCURVE /* Would need a better icon... */, "Median Step",
		                       "Map all values below 0.5 to 0.0, and all others to 1.0"},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "VertexWeightProximityModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "WeightVG Proximity Modifier",
	                       "Set the weights of vertices in a group from a target object's "
	                       "distance");
	RNA_def_struct_sdna(srna, "WeightVGProximityModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_VERTEX_WEIGHT);

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightVGProximityModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "proximity_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, weightvg_proximity_modes_items);
	RNA_def_property_enum_default(prop, MOD_WVG_PROXIMITY_GEOMETRY);
	RNA_def_property_ui_text(prop, "Proximity Mode", "Which distances to target object to use");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "proximity_geometry", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "proximity_flags");
	RNA_def_property_enum_items(prop, proximity_geometry_items);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG); /* important to run before default set */
	RNA_def_property_enum_default(prop, MOD_WVG_PROXIMITY_GEOM_FACES);
	RNA_def_property_ui_text(prop, "Proximity Geometry",
	                         "Use the shortest computed distance to target object's geometry "
	                         "as weight");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "proximity_ob_target");
	RNA_def_property_ui_text(prop, "Target Object", "Object to calculate vertices distances from");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "min_dist", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0, 1000.0, 10, -1);
	RNA_def_property_ui_text(prop, "Lowest", "Distance mapping to weight 0.0");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "max_dist", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0, 1000.0, 10, -1);
	RNA_def_property_ui_text(prop, "Highest", "Distance mapping to weight 1.0");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, weightvg_proximity_falloff_type_items);
	RNA_def_property_ui_text(prop, "Falloff Type", "How weights are mapped to their new values");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Common masking properties. */
	rna_def_modifier_weightvg_mask(brna, srna, "rna_WeightVGProximityModifier_mask_defgrp_name_set",
	                               "rna_WeightVGProximityModifier_mask_tex_uvlayer_name_set");
}

static void rna_def_modifier_remesh(BlenderRNA *brna)
{
	static const EnumPropertyItem mode_items[] = {
		{MOD_REMESH_CENTROID, "BLOCKS", 0, "Blocks", "Output a blocky surface with no smoothing"},
		{MOD_REMESH_MASS_POINT, "SMOOTH", 0, "Smooth", "Output a smooth surface with no sharp-features detection"},
		{MOD_REMESH_SHARP_FEATURES, "SHARP", 0, "Sharp",
		                            "Output a surface that reproduces sharp edges and corners from the input mesh"},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "RemeshModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Remesh Modifier",
	                       "Generate a new surface with regular topology that follows the shape of the input mesh");
	RNA_def_struct_sdna(srna, "RemeshModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_REMESH);

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 0.99, 0.01, 3);
	RNA_def_property_range(prop, 0, 0.99);
	RNA_def_property_ui_text(prop, "Scale",
	                         "The ratio of the largest dimension of the model over the size of the grid");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Threshold",
	                         "If removing disconnected pieces, minimum size of components to preserve as a ratio "
	                         "of the number of polygons in the largest component");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "octree_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "depth");
	RNA_def_property_range(prop, 1, 12);
	RNA_def_property_ui_text(prop, "Octree Depth", "Resolution of the octree; higher values give finer details");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "sharpness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "hermite_num");
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Sharpness",
	                         "Tolerance for outliers; lower values filter noise while higher values will reproduce "
	                         "edges closer to the input");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_remove_disconnected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_REMESH_FLOOD_FILL);
	RNA_def_property_ui_text(prop, "Remove Disconnected Pieces", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_smooth_shade", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_REMESH_SMOOTH_SHADING);
	RNA_def_property_ui_text(prop, "Smooth Shading", "Output faces with smooth shading rather than flat shaded");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_ocean(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem geometry_items[] = {
		{MOD_OCEAN_GEOM_GENERATE, "GENERATE", 0, "Generate",
		                          "Generate ocean surface geometry at the specified resolution"},
		{MOD_OCEAN_GEOM_DISPLACE, "DISPLACE", 0, "Displace", "Displace existing geometry according to simulation"},
#if 0
		{MOD_OCEAN_GEOM_SIM_ONLY, "SIM_ONLY", 0, "Sim Only",
		                          "Leaves geometry unchanged, but still runs simulation (to be used from texture)"},
#endif
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "OceanModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Ocean Modifier", "Simulate an ocean surface");
	RNA_def_struct_sdna(srna, "OceanModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_OCEAN);

	prop = RNA_def_property(srna, "geometry_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "geometry_mode");
	RNA_def_property_enum_items(prop, geometry_items);
	RNA_def_property_ui_text(prop, "Geometry", "Method of modifying geometry");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Size", "Surface scale factor (does not affect the height of the waves)");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, -1);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "repeat_x", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "repeat_x");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_range(prop, 1, 100, 1, -1);
	RNA_def_property_ui_text(prop, "Repeat X", "Repetitions of the generated surface in X");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "repeat_y", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "repeat_y");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_range(prop, 1, 100, 1, -1);
	RNA_def_property_ui_text(prop, "Repeat Y", "Repetitions of the generated surface in Y");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_normals", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_OCEAN_GENERATE_NORMALS);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Generate Normals",
	                         "Output normals for bump mapping - disabling can speed up performance if its not needed");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "use_foam", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_OCEAN_GENERATE_FOAM);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Generate Foam", "Generate foam mask as a vertex color channel");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "resolution", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "resolution");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_range(prop, 1, 32, 1, -1);
	RNA_def_property_ui_text(prop, "Resolution", "Resolution of the generated surface");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "spatial_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "spatial_size");
	RNA_def_property_ui_range(prop, 1, 512, 2, -1);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Spatial Size",
	                         "Size of the simulation domain (in meters), and of the generated geometry (in BU)");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "wind_velocity", PROP_FLOAT, PROP_VELOCITY);
	RNA_def_property_float_sdna(prop, NULL, "wind_velocity");
	RNA_def_property_ui_text(prop, "Wind Velocity", "Wind speed");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "damp");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Damping", "Damp reflected waves going in opposite direction to the wind");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "wave_scale_min", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "smallest_wave");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Smallest Wave", "Shortest allowed wavelength");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "wave_alignment", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "wave_alignment");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_text(prop, "Wave Alignment", "How much the waves are aligned to each other");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "wave_direction", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "wave_direction");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Wave Direction", "Main direction of the waves when they are (partially) aligned");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "wave_scale", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "wave_scale");
	RNA_def_property_ui_text(prop, "Wave Scale", "Scale of the displacement effect");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "depth", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "depth");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Depth", "Depth of the solid ground below the water surface");
	RNA_def_property_ui_range(prop, 0, 250, 1, -1);
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "foam_coverage", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "foam_coverage");
	RNA_def_property_ui_text(prop, "Foam Coverage", "Amount of generated foam");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "bake_foam_fade", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "foam_fade");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Foam Fade", "How much foam accumulates over time (baked ocean only)");
	RNA_def_property_ui_range(prop, 0.0, 10.0, 1, -1);
	RNA_def_property_update(prop, 0, NULL);

	prop = RNA_def_property(srna, "foam_layer_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "foamlayername");
	RNA_def_property_ui_text(prop, "Foam Layer Name", "Name of the vertex color layer used for foam");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "choppiness", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "chop_amount");
	RNA_def_property_ui_text(prop, "Choppiness",
	                         "Choppiness of the wave's crest (adds some horizontal component to the displacement)");
	RNA_def_property_ui_range(prop, 0.0, 4.0, 3, -1);
	RNA_def_property_float_funcs(prop, NULL, "rna_OceanModifier_ocean_chop_set", NULL);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "time");
	RNA_def_property_ui_text(prop, "Time", "Current time of the simulation");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, -1);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "random_seed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "seed");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Random Seed", "Seed of the random generator");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "bakestart");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Bake Start", "Start frame of the ocean baking");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "bakeend");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Bake End", "End frame of the ocean baking");
	RNA_def_property_update(prop, 0, "rna_OceanModifier_init_update");

	prop = RNA_def_property(srna, "is_cached", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cached", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Ocean is Cached", "Whether the ocean is using cached data or simulating");

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "cachepath");
	RNA_def_property_ui_text(prop, "Cache Path", "Path to a folder to store external baked images");
	/*RNA_def_property_update(prop, 0, "rna_Modifier_update"); */
	/* XXX how to update? */
}

static void rna_def_modifier_skin(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SkinModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Skin Modifier", "Generate Skin");
	RNA_def_struct_sdna(srna, "SkinModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SKIN);

	prop = RNA_def_property(srna, "branch_smoothing", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_ui_text(prop, "Branch Smoothing", "Smooth complex geometry around branches");
	RNA_def_property_ui_range(prop, 0, 1, 1, -1);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_smooth_shade", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SKIN_SMOOTH_SHADING);
	RNA_def_property_ui_text(prop, "Smooth Shading", "Output faces with smooth shading rather than flat shaded");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_x_symmetry", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "symmetry_axes", MOD_SKIN_SYMM_X);
	RNA_def_property_ui_text(prop, "X", "Avoid making unsymmetrical quads across the X axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_y_symmetry", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "symmetry_axes", MOD_SKIN_SYMM_Y);
	RNA_def_property_ui_text(prop, "Y", "Avoid making unsymmetrical quads across the Y axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_z_symmetry", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "symmetry_axes", MOD_SKIN_SYMM_Z);
	RNA_def_property_ui_text(prop, "Z", "Avoid making unsymmetrical quads across the Z axis");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_triangulate(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "TriangulateModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Triangulate Modifier", "Triangulate Mesh");
	RNA_def_struct_sdna(srna, "TriangulateModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_TRIANGULATE);

	prop = RNA_def_property(srna, "quad_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "quad_method");
	RNA_def_property_enum_items(prop, rna_enum_modifier_triangulate_quad_method_items);
	RNA_def_property_ui_text(prop, "Quad Method", "Method for splitting the quads into triangles");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "ngon_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ngon_method");
	RNA_def_property_enum_items(prop, rna_enum_modifier_triangulate_ngon_method_items);
	RNA_def_property_ui_text(prop, "Polygon Method", "Method for splitting the polygons into triangles");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_meshcache(BlenderRNA *brna)
{
	static const EnumPropertyItem prop_format_type_items[] = {
		{MOD_MESHCACHE_TYPE_MDD, "MDD", 0, "MDD ", ""},
		{MOD_MESHCACHE_TYPE_PC2, "PC2", 0, "PC2", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_deform_mode_items[] = {
		{MOD_MESHCACHE_DEFORM_OVERWRITE, "OVERWRITE", 0, "Overwrite",
		 "Replace vertex coords with cached values"},
		{MOD_MESHCACHE_DEFORM_INTEGRATE, "INTEGRATE", 0, "Integrate",
		 "Integrate deformation from this modifiers input with the mesh-cache coords (useful for shape keys)"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_interpolation_type_items[] = {
		{MOD_MESHCACHE_INTERP_NONE, "NONE", 0, "None ", ""},
		{MOD_MESHCACHE_INTERP_LINEAR, "LINEAR", 0, "Linear", ""},
		/* for cardinal we'd need to read 4x cache's */
		// {MOD_MESHCACHE_INTERP_CARDINAL, "CARDINAL", 0, "Cardinal", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_time_type_items[] = {
		/* use 'eval_frame' */
		{MOD_MESHCACHE_TIME_FRAME,   "FRAME",   0, "Frame",  "Control playback using a frame-number "
		                                                   "(ignoring time FPS and start frame from the file)"},
		/* use 'eval_time' */
		{MOD_MESHCACHE_TIME_SECONDS, "TIME",    0, "Time",   "Control playback using time in seconds"},
		/* use 'eval_factor' */
		{MOD_MESHCACHE_TIME_FACTOR,  "FACTOR",  0, "Factor", "Control playback using a value between [0, 1]"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_time_play_items[] = {
		{MOD_MESHCACHE_PLAY_CFEA, "SCENE", 0, "Scene", "Use the time from the scene"},
		{MOD_MESHCACHE_PLAY_EVAL, "CUSTOM", 0, "Custom", "Use the modifier's own time evaluation"},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshCacheModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Cache Modifier", "Cache Mesh");
	RNA_def_struct_sdna(srna, "MeshCacheModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);  /* XXX, needs own icon */

	prop = RNA_def_property(srna, "cache_format", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_format_type_items);
	RNA_def_property_ui_text(prop, "Format", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "interp");
	RNA_def_property_enum_items(prop, prop_interpolation_type_items);
	RNA_def_property_ui_text(prop, "Interpolation", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "time_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "time_mode");
	RNA_def_property_enum_items(prop, prop_time_type_items);
	RNA_def_property_ui_text(prop, "Time Mode", "Method to control playback time");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "play_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "play_mode");
	RNA_def_property_enum_items(prop, prop_time_play_items);
	RNA_def_property_ui_text(prop, "Time Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "deform_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "deform_mode");
	RNA_def_property_enum_items(prop, prop_deform_mode_items);
	RNA_def_property_ui_text(prop, "Deform Mode", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "File Path", "Path to external displacements file");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "factor");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Influence", "Influence of the deformation");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* -------------------------------------------------------------------- */
	/* Axis Conversion */
	prop = RNA_def_property(srna, "forward_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "forward_axis");
	RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
	RNA_def_property_ui_text(prop, "Forward", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "up_axis");
	RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
	RNA_def_property_ui_text(prop, "Up", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "flip_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "flip_axis");
	RNA_def_property_enum_items(prop, rna_enum_axis_flag_xyz_items);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Flip Axis",  "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* -------------------------------------------------------------------- */
	/* For Scene time */
	prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "frame_start");
	RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Frame Start", "Add this to the start frame");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "frame_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "frame_scale");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Frame Scale", "Evaluation time in seconds");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* -------------------------------------------------------------------- */
	/* eval values depend on 'time_mode' */
	prop = RNA_def_property(srna, "eval_frame", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eval_frame");
	RNA_def_property_range(prop, MINFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Evaluation Frame", "The frame to evaluate (starting at 0)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "eval_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eval_time");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Evaluation Time", "Evaluation time in seconds");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "eval_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eval_factor");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Evaluation Factor", "Evaluation time in seconds");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_meshseqcache(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshSequenceCacheModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Cache Modifier", "Cache Mesh");
	RNA_def_struct_sdna(srna, "MeshSeqCacheModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);  /* XXX, needs own icon */

	prop = RNA_def_property(srna, "cache_file", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "cache_file");
	RNA_def_property_struct_type(prop, "CacheFile");
	RNA_def_property_ui_text(prop, "Cache File", "");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "object_path", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object Path", "Path to the object in the Alembic archive used to lookup geometric data");
	RNA_def_property_update(prop, 0, "rna_MeshSequenceCache_object_path_update");

	static const EnumPropertyItem read_flag_items[] = {
		{MOD_MESHSEQ_READ_VERT,  "VERT", 0, "Vertex", ""},
		{MOD_MESHSEQ_READ_POLY,  "POLY", 0, "Faces", ""},
		{MOD_MESHSEQ_READ_UV,    "UV", 0, "UV", ""},
		{MOD_MESHSEQ_READ_COLOR, "COLOR", 0, "Color", ""},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "read_data", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_enum_sdna(prop, NULL, "read_flag");
	RNA_def_property_enum_items(prop, read_flag_items);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_laplaciandeform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LaplacianDeformModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Laplacian Deform Modifier", "Mesh deform modifier");
	RNA_def_struct_sdna(srna, "LaplacianDeformModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "anchor_grp_name");
	RNA_def_property_ui_text(prop, "Vertex Group for Anchors",
	                         "Name of Vertex Group which determines Anchors");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_LaplacianDeformModifier_anchor_grp_name_set");

	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "repeat");
	RNA_def_property_ui_range(prop, 1, 50, 1, -1);
	RNA_def_property_ui_text(prop, "Repeat", "");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "is_bind", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_LaplacianDeformModifier_is_bind_get", NULL);
	RNA_def_property_ui_text(prop, "Bound", "Whether geometry has been bound to anchors");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_wireframe(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "WireframeModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Wireframe Modifier", "Wireframe effect modifier");
	RNA_def_struct_sdna(srna, "WireframeModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_WIREFRAME);


	prop = RNA_def_property(srna, "thickness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 4);
	RNA_def_property_ui_text(prop, "Thickness", "Thickness factor");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "thickness_vertex_group", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "offset_fac_vg");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
	RNA_def_property_ui_text(prop, "Vertex Group Factor",
	                         "Thickness factor to use for zero vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "offset_fac");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -1, 1, 0.1, 4);
	RNA_def_property_ui_text(prop, "Offset", "Offset the thickness from the center");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_replace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_REPLACE);
	RNA_def_property_ui_text(prop, "Replace", "Remove original geometry");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_boundary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_BOUNDARY);
	RNA_def_property_ui_text(prop, "Boundary", "Support face boundaries");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_even_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_OFS_EVEN);
	RNA_def_property_ui_text(prop, "Offset Even", "Scale the offset to give more even thickness");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_relative_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_OFS_RELATIVE);
	RNA_def_property_ui_text(prop, "Offset Relative", "Scale the offset by surrounding geometry");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "use_crease", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_CREASE);
	RNA_def_property_ui_text(prop, "Offset Relative", "Crease hub edges for improved subsurf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "crease_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "crease_weight");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 1);
	RNA_def_property_ui_text(prop, "Weight", "Crease weight (if active)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "material_offset", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mat_ofs");
	RNA_def_property_range(prop, SHRT_MIN, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Material Offset", "Offset material index of generated faces");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");


	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for selecting the affected areas");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WireframeModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WIREFRAME_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_datatransfer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem DT_layer_vert_items[] = {
		{DT_TYPE_MDEFORMVERT, "VGROUP_WEIGHTS", 0, "Vertex Group(s)", "Transfer active or all vertex groups"},
#if 0  /* TODO */
		{DT_TYPE_SHAPEKEY, "SHAPEKEYS", 0, "Shapekey(s)", "Transfer active or all shape keys"},
#endif
#if 0  /* XXX When SkinModifier is enabled, it seems to erase its own CD_MVERT_SKIN layer from final DM :( */
		{DT_TYPE_SKIN, "SKIN", 0, "Skin Weight", "Transfer skin weights"},
#endif
		{DT_TYPE_BWEIGHT_VERT, "BEVEL_WEIGHT_VERT", 0, "Bevel Weight", "Transfer bevel weights"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem DT_layer_edge_items[] = {
		{DT_TYPE_SHARP_EDGE, "SHARP_EDGE", 0, "Sharp", "Transfer sharp mark"},
		{DT_TYPE_SEAM, "SEAM", 0, "UV Seam", "Transfer UV seam mark"},
		{DT_TYPE_CREASE, "CREASE", 0, "Subsurf Crease", "Transfer crease values"},
		{DT_TYPE_BWEIGHT_EDGE, "BEVEL_WEIGHT_EDGE", 0, "Bevel Weight", "Transfer bevel weights"},
		{DT_TYPE_FREESTYLE_EDGE, "FREESTYLE_EDGE", 0, "Freestyle Mark", "Transfer Freestyle edge mark"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem DT_layer_loop_items[] = {
		{DT_TYPE_LNOR, "CUSTOM_NORMAL", 0, "Custom Normals", "Transfer custom normals"},
		{DT_TYPE_VCOL, "VCOL", 0, "VCol", "Vertex (face corners) colors"},
		{DT_TYPE_UV, "UV", 0, "UVs", "Transfer UV layers"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem DT_layer_poly_items[] = {
		{DT_TYPE_SHARP_FACE, "SMOOTH", 0, "Smooth", "Transfer flat/smooth mark"},
		{DT_TYPE_FREESTYLE_FACE, "FREESTYLE_FACE", 0, "Freestyle Mark", "Transfer Freestyle face mark"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "DataTransferModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Data Transfer Modifier", "Modifier transferring some data from a source mesh");
	RNA_def_struct_sdna(srna, "DataTransferModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_DATA_TRANSFER);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob_source");
	RNA_def_property_ui_text(prop, "Source Object", "Object to transfer data from");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_DataTransferModifier_ob_source_set", NULL, "rna_Mesh_object_poll");
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_boolean(srna, "use_object_transform", true, "Object Transform",
	                       "Evaluate source and destination meshes in global space");
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_OBSRC_TRANSFORM);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Generic, UI-only data types toggles. */
	prop = RNA_def_boolean(srna, "use_vert_data", false, "Vertex Data", "Enable vertex data transfer");
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_USE_VERT);
	RNA_def_property_update(prop, 0, "rna_DataTransferModifier_use_data_update");

	prop = RNA_def_boolean(srna, "use_edge_data", false, "Edge Data", "Enable edge data transfer");
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_USE_EDGE);
	RNA_def_property_update(prop, 0, "rna_DataTransferModifier_use_data_update");

	prop = RNA_def_boolean(srna, "use_loop_data", false, "Face Corner Data", "Enable face corner data transfer");
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_USE_LOOP);
	RNA_def_property_update(prop, 0, "rna_DataTransferModifier_use_data_update");

	prop = RNA_def_boolean(srna, "use_poly_data", false, "Face Data", "Enable face data transfer");
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_USE_POLY);
	RNA_def_property_update(prop, 0, "rna_DataTransferModifier_use_data_update");

	/* Actual data types selection. */
	prop = RNA_def_enum(srna, "data_types_verts", DT_layer_vert_items, 0, "Vertex Data Types",
	                    "Which vertex data layers to transfer");
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_enum_sdna(prop, NULL, "data_types");
	RNA_def_property_enum_funcs(prop, NULL, "rna_DataTransferModifier_verts_data_types_set", NULL);
	RNA_def_property_update(prop, 0, "rna_DataTransferModifier_data_types_update");

	prop = RNA_def_enum(srna, "data_types_edges", DT_layer_edge_items, 0, "Edge Data Types",
	                    "Which edge data layers to transfer");
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_enum_sdna(prop, NULL, "data_types");
	RNA_def_property_enum_funcs(prop, NULL, "rna_DataTransferModifier_edges_data_types_set", NULL);
	RNA_def_property_update(prop, 0, "rna_DataTransferModifier_data_types_update");

	prop = RNA_def_enum(srna, "data_types_loops", DT_layer_loop_items, 0, "Face Corner Data Types",
	                    "Which face corner data layers to transfer");
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_enum_sdna(prop, NULL, "data_types");
	RNA_def_property_enum_funcs(prop, NULL, "rna_DataTransferModifier_loops_data_types_set", NULL);
	RNA_def_property_update(prop, 0, "rna_DataTransferModifier_data_types_update");

	prop = RNA_def_enum(srna, "data_types_polys", DT_layer_poly_items, 0, "Poly Data Types",
	                    "Which poly data layers to transfer");
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_enum_sdna(prop, NULL, "data_types");
	RNA_def_property_enum_funcs(prop, NULL, "rna_DataTransferModifier_polys_data_types_set", NULL);
	RNA_def_property_update(prop, 0, "rna_DataTransferModifier_data_types_update");

	/* Mapping methods. */
	prop = RNA_def_enum(srna, "vert_mapping", rna_enum_dt_method_vertex_items, MREMAP_MODE_VERT_NEAREST, "Vertex Mapping",
	                    "Method used to map source vertices to destination ones");
	RNA_def_property_enum_sdna(prop, NULL, "vmap_mode");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_enum(srna, "edge_mapping", rna_enum_dt_method_edge_items, MREMAP_MODE_EDGE_NEAREST, "Edge Mapping",
	                    "Method used to map source edges to destination ones");
	RNA_def_property_enum_sdna(prop, NULL, "emap_mode");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_enum(srna, "loop_mapping", rna_enum_dt_method_loop_items, MREMAP_MODE_LOOP_NEAREST_POLYNOR,
	                    "Face Corner Mapping", "Method used to map source faces' corners to destination ones");
	RNA_def_property_enum_sdna(prop, NULL, "lmap_mode");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_enum(srna, "poly_mapping", rna_enum_dt_method_poly_items, MREMAP_MODE_POLY_NEAREST, "Face Mapping",
	                    "Method used to map source faces to destination ones");
	RNA_def_property_enum_sdna(prop, NULL, "pmap_mode");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Mapping options and filtering. */
	prop = RNA_def_boolean(srna, "use_max_distance", false, "Only Neighbor Geometry",
	                       "Source elements must be closer than given distance from destination one");
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_MAP_MAXDIST);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_float(srna, "max_distance", 1.0f, 0.0f, FLT_MAX, "Max Distance",
	                     "Maximum allowed distance between source and destination element, for non-topology mappings",
	                     0.0f, 100.0f);
	RNA_def_property_float_sdna(prop, NULL, "map_max_distance");
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_float(srna, "ray_radius", 0.0f, 0.0f, FLT_MAX, "Ray Radius",
	                     "'Width' of rays (especially useful when raycasting against vertices or edges)", 0.0f, 10.0f);
	RNA_def_property_float_sdna(prop, NULL, "map_ray_radius");
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_float(srna, "islands_precision", 0.0f, 0.0f, 1.0f, "Islands Handling Refinement",
	                     "Factor controlling precision of islands handling "
	                     "(typically, 0.1 should be enough, higher values can make things really slow)", 0.0f, 1.0f);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* How to handle multi-layers types of data. */
	prop = RNA_def_enum(srna, "layers_vgroup_select_src", rna_enum_dt_layers_select_src_items, DT_LAYERS_ALL_SRC,
	                    "Source Layers Selection", "Which layers to transfer, in case of multi-layers types");
	RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_MDEFORMVERT]");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

#if 0
	prop = RNA_def_enum(srna, "layers_shapekey_select_src", rna_enum_dt_layers_select_src_items, DT_LAYERS_ALL_SRC,
	                    "Source Layers Selection", "Which layers to transfer, in case of multi-layers types");
	RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_SHAPEKEY]");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
#endif

	prop = RNA_def_enum(srna, "layers_vcol_select_src", rna_enum_dt_layers_select_src_items, DT_LAYERS_ALL_SRC,
	                    "Source Layers Selection", "Which layers to transfer, in case of multi-layers types");
	RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_VCOL]");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_enum(srna, "layers_uv_select_src", rna_enum_dt_layers_select_src_items, DT_LAYERS_ALL_SRC,
	                    "Source Layers Selection", "Which layers to transfer, in case of multi-layers types");
	RNA_def_property_enum_sdna(prop, NULL, "layers_select_src[DT_MULTILAYER_INDEX_UV]");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_layers_select_src_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_enum(srna, "layers_vgroup_select_dst", rna_enum_dt_layers_select_dst_items, DT_LAYERS_NAME_DST,
	                    "Destination Layers Matching", "How to match source and destination layers");
	RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_MDEFORMVERT]");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

#if 0
	prop = RNA_def_enum(srna, "layers_shapekey_select_dst", rna_enum_dt_layers_select_dst_items, DT_LAYERS_NAME_DST,
	                    "Destination Layers Matching", "How to match source and destination layers");
	RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_SHAPEKEY]");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
#endif

	prop = RNA_def_enum(srna, "layers_vcol_select_dst", rna_enum_dt_layers_select_dst_items, DT_LAYERS_NAME_DST,
	                    "Destination Layers Matching", "How to match source and destination layers");
	RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_VCOL]");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_enum(srna, "layers_uv_select_dst", rna_enum_dt_layers_select_dst_items, DT_LAYERS_NAME_DST,
	                    "Destination Layers Matching", "How to match source and destination layers");
	RNA_def_property_enum_sdna(prop, NULL, "layers_select_dst[DT_MULTILAYER_INDEX_UV]");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_layers_select_dst_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	/* Mix stuff */
	prop = RNA_def_enum(srna, "mix_mode", rna_enum_dt_mix_mode_items, CDT_MIX_TRANSFER, "Mix Mode",
	                   "How to affect destination elements with source values");
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DataTransferModifier_mix_mode_itemf");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_float(srna, "mix_factor", 1.0f, 0.0f, 1.0f, "Mix Factor",
	                     "Factor to use when applying data to destination (exact behavior depends on mix mode)",
	                     0.0f, 1.0f);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_string(srna, "vertex_group", NULL, MAX_VGROUP_NAME, "Vertex Group",
	                      "Vertex group name for selecting the affected areas");
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DataTransferModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_boolean(srna, "invert_vertex_group", false, "Invert", "Invert vertex group influence");
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DATATRANSFER_INVERT_VGROUP);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_normaledit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_mode_items[] = {
		{MOD_NORMALEDIT_MODE_RADIAL, "RADIAL", 0, "Radial",
		        "From an ellipsoid (shape defined by the boundbox's dimensions, target is optional)"},
		{MOD_NORMALEDIT_MODE_DIRECTIONAL, "DIRECTIONAL", 0, "Directional",
		        "Normals 'track' (point to) the target object"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_mix_mode_items[] = {
		{MOD_NORMALEDIT_MIX_COPY, "COPY", 0, "Copy", "Copy new normals (overwrite existing)"},
		{MOD_NORMALEDIT_MIX_ADD, "ADD", 0, "Add", "Copy sum of new and old normals"},
		{MOD_NORMALEDIT_MIX_SUB, "SUB", 0, "Subtract", "Copy new normals minus old normals"},
		{MOD_NORMALEDIT_MIX_MUL, "MUL", 0, "Multiply", "Copy product of old and new normals (*not* cross product)"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "NormalEditModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "Normal Edit Modifier", "Modifier affecting/generating custom normals");
	RNA_def_struct_sdna(srna, "NormalEditModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_NORMALEDIT);

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "How to affect (generate) normals");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_float_array(srna, "offset", 3, NULL, -FLT_MAX, FLT_MAX, "Offset",
	                           "Offset from object's center", -100.0f, 100.0f);
	RNA_def_property_subtype(prop, PROP_COORDS);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mix_mode_items);
	RNA_def_property_ui_text(prop, "Mix Mode", "How to mix generated normals with existing ones");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_float(srna, "mix_factor", 1.0f, 0.0f, 1.0f, "Mix Factor",
	                     "How much of generated normals to mix with exiting ones", 0.0f, 1.0f);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_float(srna, "mix_limit", 1.0f, 0.0f, DEG2RADF(180.0f), "Max Angle",
	                     "Maximum angle between old and new normals", 0.0f, DEG2RADF(180.0f));
	RNA_def_property_subtype(prop, PROP_ANGLE);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "no_polynors_fix", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_NORMALEDIT_NO_POLYNORS_FIX);
	RNA_def_property_boolean_default(prop, false);
	RNA_def_property_ui_text(prop, "Lock Polygon Normals",
	                         "Do not flip polygons when their normals are not consistent "
	                         "with their newly computed custom vertex normals");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for selecting/weighting the affected areas");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_NormalEditModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_NORMALEDIT_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Target", "Target object used to affect normals");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_NormalEditModifier_target_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "use_direction_parallel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_NORMALEDIT_USE_DIRECTION_PARALLEL);
	RNA_def_property_boolean_default(prop, true);
	RNA_def_property_ui_text(prop, "Parallel Normals",
	                         "Use same direction for all normals, from origin to target's center "
	                         "(Directional mode only)");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

static void rna_def_modifier_surfacedeform(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SurfaceDeformModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "SurfaceDeform Modifier", "");
	RNA_def_struct_sdna(srna, "SurfaceDeformModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MESHDEFORM);

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Target", "Mesh object to deform with");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_SurfaceDeformModifier_target_set", NULL, "rna_Mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_Modifier_dependency_update");

	prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 2.0f, 16.0f);
	RNA_def_property_ui_text(prop, "Interpolation falloff", "Controls how much nearby polygons influence deformation");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "is_bound", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SurfaceDeformModifier_is_bound_get", NULL);
	RNA_def_property_ui_text(prop, "Bound", "Whether geometry has been bound to target mesh");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_modifier_weightednormal(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_weighting_mode_items[] = {
		{MOD_WEIGHTEDNORMAL_MODE_FACE, "FACE_AREA", 0, "Face Area", "Generate face area weighted normals"},
		{MOD_WEIGHTEDNORMAL_MODE_ANGLE, "CORNER_ANGLE", 0, "Corner Angle", "Generate corner angle weighted normals"},
		{MOD_WEIGHTEDNORMAL_MODE_FACE_ANGLE, "FACE_AREA_WITH_ANGLE", 0, "Face Area And Angle",
		                                     "Generated normals weighted by both face area and angle"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "WeightedNormalModifier", "Modifier");
	RNA_def_struct_ui_text(srna, "WeightedNormal Modifier", "");
	RNA_def_struct_sdna(srna, "WeightedNormalModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_NORMALEDIT);

	prop = RNA_def_property(srna, "weight", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_range(prop, 1, 100, 1, -1);
	RNA_def_property_ui_text(prop, "Weight",
	                         "Corrective factor applied to faces' weights, 50 is neutral, "
	                         "lower values increase weight of weak faces, "
	                         "higher values increase weight of strong faces");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_weighting_mode_items);
	RNA_def_property_ui_text(prop, "Weighting Mode", "Weighted vertex normal mode to use");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "thresh", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_range(prop, 0, 10, 1, 2);
	RNA_def_property_ui_text(prop, "Threshold", "Threshold value for different weights to be considered equal");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "keep_sharp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WEIGHTEDNORMAL_KEEP_SHARP);
	RNA_def_property_ui_text(prop, "Keep Sharp",
	                         "Keep sharp edges as computed for default split normals, "
	                         "instead of setting a single weighted normal for each vertex");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "defgrp_name");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modifying the selected areas");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_WeightedNormalModifier_defgrp_name_set");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WEIGHTEDNORMAL_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "face_influence", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_WEIGHTEDNORMAL_FACE_INFLUENCE);
	RNA_def_property_ui_text(prop, "Face Influence", "Use influence of face for weighting");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
}

void RNA_def_modifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* data */
	srna = RNA_def_struct(brna, "Modifier", NULL);
	RNA_def_struct_ui_text(srna, "Modifier", "Modifier affecting the geometry data of an object");
	RNA_def_struct_refine_func(srna, "rna_Modifier_refine");
	RNA_def_struct_path_func(srna, "rna_Modifier_path");
	RNA_def_struct_sdna(srna, "ModifierData");

	/* strings */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Modifier_name_set");
	RNA_def_property_ui_text(prop, "Name", "Modifier name");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER | NA_RENAME, NULL);
	RNA_def_struct_name_property(srna, prop);

	/* enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rna_enum_object_modifier_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	/* flags */
	prop = RNA_def_property(srna, "show_viewport", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Realtime);
	RNA_def_property_ui_text(prop, "Realtime", "Display modifier in viewport");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 0);

	prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Render);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_ui_text(prop, "Render", "Use modifier during render");
	RNA_def_property_ui_icon(prop, ICON_SCENE, 0);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	prop = RNA_def_property(srna, "show_in_editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Editmode);
	RNA_def_property_ui_text(prop, "Edit Mode", "Display modifier in Edit mode");
	RNA_def_property_update(prop, 0, "rna_Modifier_update");
	RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);

	prop = RNA_def_property(srna, "show_on_cage", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_OnCage);
	RNA_def_property_ui_text(prop, "On Cage", "Adjust edit cage to modifier result");
	RNA_def_property_ui_icon(prop, ICON_MESH_DATA, 0);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Expanded);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_ui_text(prop, "Expanded", "Set modifier expanded in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);

	prop = RNA_def_property(srna, "use_apply_on_spline", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_ApplyOnSpline);
	RNA_def_property_ui_text(prop, "Apply on spline",
	                         "Apply this and all preceding deformation modifiers on splines' points rather than "
	                         "on filled curve/surface");
	RNA_def_property_ui_icon(prop, ICON_SURFACE_DATA, 0);
	RNA_def_property_update(prop, 0, "rna_Modifier_update");

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
	rna_def_modifier_correctivesmooth(brna);
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
	rna_def_modifier_warp(brna);
	rna_def_modifier_multires(brna);
	rna_def_modifier_surface(brna);
	rna_def_modifier_smoke(brna);
	rna_def_modifier_solidify(brna);
	rna_def_modifier_screw(brna);
	rna_def_modifier_uvwarp(brna);
	rna_def_modifier_weightvgedit(brna);
	rna_def_modifier_weightvgmix(brna);
	rna_def_modifier_weightvgproximity(brna);
	rna_def_modifier_dynamic_paint(brna);
	rna_def_modifier_ocean(brna);
	rna_def_modifier_remesh(brna);
	rna_def_modifier_skin(brna);
	rna_def_modifier_laplaciansmooth(brna);
	rna_def_modifier_triangulate(brna);
	rna_def_modifier_meshcache(brna);
	rna_def_modifier_laplaciandeform(brna);
	rna_def_modifier_wireframe(brna);
	rna_def_modifier_datatransfer(brna);
	rna_def_modifier_normaledit(brna);
	rna_def_modifier_meshseqcache(brna);
	rna_def_modifier_surfacedeform(brna);
	rna_def_modifier_weightednormal(brna);
}

#endif
