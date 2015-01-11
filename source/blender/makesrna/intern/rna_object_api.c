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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_object_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"

#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_depsgraph.h"

#include "rna_internal.h"  /* own include */

static EnumPropertyItem space_items[] = {
	{CONSTRAINT_SPACE_WORLD,    "WORLD", 0, "World Space",
	                            "The most gobal space in Blender"},
	{CONSTRAINT_SPACE_POSE,     "POSE", 0, "Pose Space",
	                            "The pose space of a bone (its armature's object space)"},
	{CONSTRAINT_SPACE_PARLOCAL, "LOCAL_WITH_PARENT", 0, "Local With Parent",
	                            "The local space of a bone's parent bone"},
	{CONSTRAINT_SPACE_LOCAL,    "LOCAL", 0, "Local Space",
	                            "The local space of an object/bone"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "BLI_math.h"

#include "BKE_anim.h"
#include "BKE_bvhutils.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "ED_object.h"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "MEM_guardedalloc.h"

/* Convert a given matrix from a space to another (using the object and/or a bone as reference). */
static void rna_Scene_mat_convert_space(Object *ob, ReportList *reports, bPoseChannel *pchan,
                                        float *mat, float *mat_ret, int from, int to)
{
	copy_m4_m4((float (*)[4])mat_ret, (float (*)[4])mat);

	/* Error in case of invalid from/to values when pchan is NULL */
	if (pchan == NULL) {
		if (ELEM(from, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_PARLOCAL)) {
			const char *identifier = NULL;
			RNA_enum_identifier(space_items, from, &identifier);
			BKE_reportf(reports, RPT_ERROR, "'from_space' '%s' is invalid when no pose bone is given!", identifier);
			return;
		}
		if (ELEM(to, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_PARLOCAL)) {
			const char *identifier = NULL;
			RNA_enum_identifier(space_items, to, &identifier);
			BKE_reportf(reports, RPT_ERROR, "'to_space' '%s' is invalid when no pose bone is given!", identifier);
			return;
		}
	}

	BKE_constraint_mat_convertspace(ob, pchan, (float (*)[4])mat_ret, from, to, false);
}

static void rna_Object_calc_matrix_camera(
        Object *ob, float mat_ret[16], int width, int height, float scalex, float scaley)
{
	CameraParams params;

	/* setup parameters */
	BKE_camera_params_init(&params);
	BKE_camera_params_from_object(&params, ob);

	/* compute matrix, viewplane, .. */
	BKE_camera_params_compute_viewplane(&params, width, height, scalex, scaley);
	BKE_camera_params_compute_matrix(&params);

	copy_m4_m4((float (*)[4])mat_ret, params.winmat);
}

static void rna_Object_camera_fit_coords(
        Object *ob, Scene *scene, int num_cos, float *cos, float co_ret[3], float *scale_ret)
{
	BKE_camera_view_frame_fit_to_coords(scene, (float (*)[3])cos, num_cos / 3, ob, co_ret, scale_ret);
}

/* copied from Mesh_getFromObject and adapted to RNA interface */
/* settings: 0 - preview, 1 - render */
static Mesh *rna_Object_to_mesh(
        Object *ob, ReportList *reports, Scene *sce,
        int apply_modifiers, int settings, int calc_tessface, int calc_undeformed)
{
	return rna_Main_meshes_new_from_object(G.main, reports, sce, ob, apply_modifiers, settings, calc_tessface, calc_undeformed);
}

/* mostly a copy from convertblender.c */
static void dupli_render_particle_set(Scene *scene, Object *ob, int level, int enable)
{
	/* ugly function, but we need to set particle systems to their render
	 * settings before calling object_duplilist, to get render level duplis */
	Group *group;
	GroupObject *go;
	ParticleSystem *psys;
	DerivedMesh *dm;
	float mat[4][4];

	unit_m4(mat);

	if (level >= MAX_DUPLI_RECUR)
		return;
	
	if (ob->transflag & OB_DUPLIPARTS) {
		for (psys = ob->particlesystem.first; psys; psys = psys->next) {
			if (ELEM(psys->part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
				if (enable)
					psys_render_set(ob, psys, mat, mat, 1, 1, 0.f);
				else
					psys_render_restore(ob, psys);
			}
		}

		if (enable) {
			/* this is to make sure we get render level duplis in groups:
			 * the derivedmesh must be created before init_render_mesh,
			 * since object_duplilist does dupliparticles before that */
			dm = mesh_create_derived_render(scene, ob, CD_MASK_BAREMESH | CD_MASK_MTFACE | CD_MASK_MCOL);
			dm->release(dm);

			for (psys = ob->particlesystem.first; psys; psys = psys->next)
				psys_get_modifier(ob, psys)->flag &= ~eParticleSystemFlag_psys_updated;
		}
	}

	if (ob->dup_group == NULL) return;
	group = ob->dup_group;

	for (go = group->gobject.first; go; go = go->next)
		dupli_render_particle_set(scene, go->ob, level + 1, enable);
}
/* When no longer needed, duplilist should be freed with Object.free_duplilist */
static void rna_Object_create_duplilist(Object *ob, ReportList *reports, Scene *sce, int settings)
{
	bool for_render = (settings == DAG_EVAL_RENDER);
	EvaluationContext eval_ctx = {0};
	eval_ctx.mode = settings;

	if (!(ob->transflag & OB_DUPLI)) {
		BKE_report(reports, RPT_ERROR, "Object does not have duplis");
		return;
	}

	/* free duplilist if a user forgets to */
	if (ob->duplilist) {
		BKE_report(reports, RPT_WARNING, "Object.dupli_list has not been freed");

		free_object_duplilist(ob->duplilist);
		ob->duplilist = NULL;
	}
	if (for_render)
		dupli_render_particle_set(sce, ob, 0, 1);
	ob->duplilist = object_duplilist(&eval_ctx, sce, ob);
	if (for_render)
		dupli_render_particle_set(sce, ob, 0, 0);
	/* ob->duplilist should now be freed with Object.free_duplilist */
}

static void rna_Object_free_duplilist(Object *ob)
{
	if (ob->duplilist) {
		free_object_duplilist(ob->duplilist);
		ob->duplilist = NULL;
	}
}

static PointerRNA rna_Object_shape_key_add(Object *ob, bContext *C, ReportList *reports,
                                           const char *name, int from_mix)
{
	KeyBlock *kb = NULL;

	if ((kb = BKE_object_insert_shape_key(ob, name, from_mix))) {
		PointerRNA keyptr;

		RNA_pointer_create((ID *)ob->data, &RNA_ShapeKey, kb, &keyptr);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
		
		return keyptr;
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' does not support shapes", ob->id.name + 2);
		return PointerRNA_NULL;
	}
}

static int rna_Object_is_visible(Object *ob, Scene *sce)
{
	return !(ob->restrictflag & OB_RESTRICT_VIEW) && (ob->lay & sce->lay);
}

#if 0
static void rna_Mesh_assign_verts_to_group(Object *ob, bDeformGroup *group, int *indices, int totindex,
                                           float weight, int assignmode)
{
	if (ob->type != OB_MESH) {
		BKE_report(reports, RPT_ERROR, "Object should be of mesh type");
		return;
	}

	Mesh *me = (Mesh *)ob->data;
	int group_index = BLI_findlink(&ob->defbase, group);
	if (group_index == -1) {
		BKE_report(reports, RPT_ERROR, "No vertex groups assigned to mesh");
		return;
	}

	if (assignmode != WEIGHT_REPLACE && assignmode != WEIGHT_ADD && assignmode != WEIGHT_SUBTRACT) {
		BKE_report(reports, RPT_ERROR, "Bad assignment mode");
		return;
	}

	/* makes a set of dVerts corresponding to the mVerts */
	if (!me->dvert)
		create_dverts(&me->id);

	/* loop list adding verts to group  */
	for (i = 0; i < totindex; i++) {
		if (i < 0 || i >= me->totvert) {
			BKE_report(reports, RPT_ERROR, "Bad vertex index in list");
			return;
		}

		add_vert_defnr(ob, group_index, i, weight, assignmode);
	}
}
#endif

/* don't call inside a loop */
static int dm_tessface_to_poly_index(DerivedMesh *dm, int tessface_index)
{
	if (tessface_index != ORIGINDEX_NONE) {
		/* double lookup */
		const int *index_mf_to_mpoly;
		if ((index_mf_to_mpoly = dm->getTessFaceDataArray(dm, CD_ORIGINDEX))) {
			const int *index_mp_to_orig = dm->getPolyDataArray(dm, CD_ORIGINDEX);
			return DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, tessface_index);
		}
	}

	return ORIGINDEX_NONE;
}

static void rna_Object_ray_cast(Object *ob, ReportList *reports, float ray_start[3], float ray_end[3],
                                float r_location[3], float r_normal[3], int *index)
{
	BVHTreeFromMesh treeData = {NULL};
	
	if (ob->derivedFinal == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' has no mesh data to be used for ray casting", ob->id.name + 2);
		return;
	}

	/* no need to managing allocation or freeing of the BVH data. this is generated and freed as needed */
	bvhtree_from_mesh_faces(&treeData, ob->derivedFinal, 0.0f, 4, 6);

	/* may fail if the mesh has no faces, in that case the ray-cast misses */
	if (treeData.tree != NULL) {
		BVHTreeRayHit hit;
		float ray_nor[3], dist;
		sub_v3_v3v3(ray_nor, ray_end, ray_start);

		dist = hit.dist = normalize_v3(ray_nor);
		hit.index = -1;
		
		if (BLI_bvhtree_ray_cast(treeData.tree, ray_start, ray_nor, 0.0f, &hit,
		                         treeData.raycast_callback, &treeData) != -1)
		{
			if (hit.dist <= dist) {
				copy_v3_v3(r_location, hit.co);
				copy_v3_v3(r_normal, hit.no);
				*index = dm_tessface_to_poly_index(ob->derivedFinal, hit.index);
				free_bvhtree_from_mesh(&treeData);
				return;
			}
		}
	}

	zero_v3(r_location);
	zero_v3(r_normal);
	*index = -1;
	free_bvhtree_from_mesh(&treeData);
}

static void rna_Object_closest_point_on_mesh(Object *ob, ReportList *reports, float point_co[3], float max_dist,
                                             float n_location[3], float n_normal[3], int *index)
{
	BVHTreeFromMesh treeData = {NULL};
	
	if (ob->derivedFinal == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' has no mesh data to be used for finding nearest point",
		            ob->id.name + 2);
		return;
	}

	/* no need to managing allocation or freeing of the BVH data. this is generated and freed as needed */
	bvhtree_from_mesh_faces(&treeData, ob->derivedFinal, 0.0f, 4, 6);

	if (treeData.tree == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' could not create internal data for finding nearest point",
		            ob->id.name + 2);
		return;
	}
	else {
		BVHTreeNearest nearest;

		nearest.index = -1;
		nearest.dist_sq = max_dist * max_dist;

		if (BLI_bvhtree_find_nearest(treeData.tree, point_co, &nearest, treeData.nearest_callback, &treeData) != -1) {
			copy_v3_v3(n_location, nearest.co);
			copy_v3_v3(n_normal, nearest.no);
			*index = dm_tessface_to_poly_index(ob->derivedFinal, nearest.index);
			free_bvhtree_from_mesh(&treeData);
			return;
		}
	}

	zero_v3(n_location);
	zero_v3(n_normal);
	*index = -1;
	free_bvhtree_from_mesh(&treeData);
}

/* ObjectBase */

static void rna_ObjectBase_layers_from_view(Base *base, View3D *v3d)
{
	base->lay = base->object->lay = v3d->lay;
}

static int rna_Object_is_modified(Object *ob, Scene *scene, int settings)
{
	return BKE_object_is_modified(scene, ob) & settings;
}

static int rna_Object_is_deform_modified(Object *ob, Scene *scene, int settings)
{
	return BKE_object_is_deform_modified(scene, ob) & settings;
}

#ifndef NDEBUG
void rna_Object_dm_info(struct Object *ob, int type, char *result)
{
	DerivedMesh *dm = NULL;
	bool dm_release = false;
	char *ret = NULL;

	result[0] = '\0';

	switch (type) {
		case 0:
			if (ob->type == OB_MESH) {
				dm = CDDM_from_mesh(ob->data);
				ret = DM_debug_info(dm);
				dm_release = true;
			}
			break;
		case 1:
			dm = ob->derivedDeform;
			break;
		case 2:
			dm = ob->derivedFinal;
			break;
	}

	if (dm) {
		ret = DM_debug_info(dm);
		if (dm_release) {
			dm->release(dm);
		}
		if (ret) {
			strcpy(result, ret);
			MEM_freeN(ret);
		}
	}
}
#endif /* NDEBUG */

static int rna_Object_update_from_editmode(Object *ob)
{
	if (ob->mode & OB_MODE_EDIT) {
		return ED_object_editmode_load(ob);
	}
	return false;
}
#else /* RNA_RUNTIME */

void RNA_api_object(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem mesh_type_items[] = {
		{eModifierMode_Realtime, "PREVIEW", 0, "Preview", "Apply modifier preview settings"},
		{eModifierMode_Render, "RENDER", 0, "Render", "Apply modifier render settings"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem dupli_eval_mode_items[] = {
		{DAG_EVAL_VIEWPORT, "VIEWPORT", 0, "Viewport", "Generate duplis using viewport settings"},
		{DAG_EVAL_PREVIEW, "PREVIEW", 0, "Preview", "Generate duplis using preview settings"},
		{DAG_EVAL_RENDER, "RENDER", 0, "Render", "Generate duplis using render settings"},
		{0, NULL, 0, NULL, NULL}
	};

#ifndef NDEBUG
	static EnumPropertyItem mesh_dm_info_items[] = {
		{0, "SOURCE", 0, "Source", "Source mesh"},
		{1, "DEFORM", 0, "Deform", "Objects deform mesh"},
		{2, "FINAL", 0, "Final", "Objects final mesh"},
		{0, NULL, 0, NULL, NULL}
	};
#endif

	/* Matrix space conversion */
	func = RNA_def_function(srna, "convert_space", "rna_Scene_mat_convert_space");
	RNA_def_function_ui_description(func, "Convert (transform) the given matrix from one space to another");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "pose_bone", "PoseBone", "",
	                       "Bone to use to define spaces (may be None, in which case only the two 'WORLD' and "
	                       "'LOCAL' spaces are usable)");
	parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(parm, "", "The matrix to transform");
	parm = RNA_def_property(func, "matrix_return", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(parm, "", "The transformed matrix");
	RNA_def_function_output(func, parm);
	parm = RNA_def_enum(func, "from_space", space_items, CONSTRAINT_SPACE_WORLD, "",
	                    "The space in which 'matrix' is currently");
	parm = RNA_def_enum(func, "to_space", space_items, CONSTRAINT_SPACE_WORLD, "",
	                    "The space to which you want to transform 'matrix'");

	/* Camera-related operations */
	func = RNA_def_function(srna, "calc_matrix_camera", "rna_Object_calc_matrix_camera");
	RNA_def_function_ui_description(func, "Generate the camera projection matrix of this object "
	                                      "(mostly useful for Camera and Lamp types)");
	parm = RNA_def_property(func, "result", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(parm, "", "The camera projection matrix");
	RNA_def_function_output(func, parm);
	parm = RNA_def_int(func, "x", 1, 0, INT_MAX, "", "Width of the render area", 0, 10000);
	parm = RNA_def_int(func, "y", 1, 0, INT_MAX, "", "Height of the render area", 0, 10000);
	parm = RNA_def_float(func, "scale_x", 1.0f, 1.0e-6f, FLT_MAX, "", "Width scaling factor", 1.0e-2f, 100.0f);
	parm = RNA_def_float(func, "scale_y", 1.0f, 1.0e-6f, FLT_MAX, "", "height scaling factor", 1.0e-2f, 100.0f);

	func = RNA_def_function(srna, "camera_fit_coords", "rna_Object_camera_fit_coords");
	RNA_def_function_ui_description(func, "Compute the coordinate (and scale for ortho cameras) "
	                                      "given object should be to 'see' all given coordinates");
	parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene to get render size information from, if available");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_float_array(func, "coordinates", 1, NULL, -FLT_MAX, FLT_MAX, "", "Coordinates to fit in",
	                           -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_DYNAMIC);
	parm = RNA_def_property(func, "co_return", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(parm, 3);
	RNA_def_property_ui_text(parm, "", "The location to aim to be able to see all given points");
	RNA_def_property_flag(parm, PROP_OUTPUT);
	parm = RNA_def_property(func, "scale_return", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(parm, "", "The ortho scale to aim to be able to see all given points (if relevant)");
	RNA_def_property_flag(parm, PROP_OUTPUT);

	/* mesh */
	func = RNA_def_function(srna, "to_mesh", "rna_Object_to_mesh");
	RNA_def_function_ui_description(func, "Create a Mesh datablock with modifiers applied");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene within which to evaluate modifiers");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_boolean(func, "apply_modifiers", 0, "", "Apply modifiers");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_enum(func, "settings", mesh_type_items, 0, "", "Modifier settings to apply");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "calc_tessface", true, "Calculate Tessellation", "Calculate tessellation faces");
	RNA_def_boolean(func, "calc_undeformed", false, "Calculate Undeformed", "Calculate undeformed vertex coordinates");
	parm = RNA_def_pointer(func, "mesh", "Mesh", "",
	                       "Mesh created from object, remove it if it is only used for export");
	RNA_def_function_return(func, parm);

	/* duplis */
	func = RNA_def_function(srna, "dupli_list_create", "rna_Object_create_duplilist");
	RNA_def_function_ui_description(func, "Create a list of dupli objects for this object, needs to "
	                                "be freed manually with free_dupli_list to restore the "
	                                "objects real matrix and layers");
	parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene within which to evaluate duplis");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	RNA_def_enum(func, "settings", dupli_eval_mode_items, 0, "", "Generate texture coordinates for rendering");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	func = RNA_def_function(srna, "dupli_list_clear", "rna_Object_free_duplilist");
	RNA_def_function_ui_description(func, "Free the list of dupli objects");

	/* Armature */
	func = RNA_def_function(srna, "find_armature", "modifiers_isDeformedByArmature");
	RNA_def_function_ui_description(func, "Find armature influencing this object as a parent or via a modifier");
	parm = RNA_def_pointer(func, "ob_arm", "Object", "", "Armature object influencing this object or NULL");
	RNA_def_function_return(func, parm);

	/* Shape key */
	func = RNA_def_function(srna, "shape_key_add", "rna_Object_shape_key_add");
	RNA_def_function_ui_description(func, "Add shape key to an object");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	RNA_def_string(func, "name", "Key", 0, "", "Unique name for the new keylock"); /* optional */
	RNA_def_boolean(func, "from_mix", 1, "", "Create new shape from existing mix of shapes");
	parm = RNA_def_pointer(func, "key", "ShapeKey", "", "New shape keyblock");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);

	/* Ray Cast */
	func = RNA_def_function(srna, "ray_cast", "rna_Object_ray_cast");
	RNA_def_function_ui_description(func, "Cast a ray onto in object space");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	
	/* ray start and end */
	parm = RNA_def_float_vector(func, "start", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_float_vector(func, "end", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* return location and normal */
	parm = RNA_def_float_vector(func, "location", 3, NULL, -FLT_MAX, FLT_MAX, "Location",
	                            "The hit location of this ray cast", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
	parm = RNA_def_float_vector(func, "normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal",
	                            "The face normal at the ray cast hit location", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
	
	parm = RNA_def_int(func, "index", 0, 0, 0, "", "The face index, -1 when no intersection is found", 0, 0);
	RNA_def_function_output(func, parm);

	/* Nearest Point */
	func = RNA_def_function(srna, "closest_point_on_mesh", "rna_Object_closest_point_on_mesh");
	RNA_def_function_ui_description(func, "Find the nearest point on the object");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	/* location of point for test and max distance */
	parm = RNA_def_float_vector(func, "point", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* default is sqrt(FLT_MAX) */
	RNA_def_float(func, "max_dist", 1.844674352395373e+19, 0.0, FLT_MAX, "", "", 0.0, FLT_MAX);

	/* return location and normal */
	parm = RNA_def_float_vector(func, "location", 3, NULL, -FLT_MAX, FLT_MAX, "Location",
	                            "The location on the object closest to the point", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
	parm = RNA_def_float_vector(func, "normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal",
	                            "The face normal at the closest point", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);

	parm = RNA_def_int(func, "index", 0, 0, 0, "", "The face index, -1 when no closest point is found", 0, 0);
	RNA_def_function_output(func, parm);

	/* View */
	func = RNA_def_function(srna, "is_visible", "rna_Object_is_visible");
	RNA_def_function_ui_description(func, "Determine if object is visible in a given scene");
	parm = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_boolean(func, "result", 0, "", "Object visibility");
	RNA_def_function_return(func, parm);

	/* utility function for checking if the object is modified */
	func = RNA_def_function(srna, "is_modified", "rna_Object_is_modified");
	RNA_def_function_ui_description(func, "Determine if this object is modified from the base mesh data");
	parm = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_enum(func, "settings", mesh_type_items, 0, "", "Modifier settings to apply");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_boolean(func, "result", 0, "", "Object visibility");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "is_deform_modified", "rna_Object_is_deform_modified");
	RNA_def_function_ui_description(func, "Determine if this object is modified by a deformation from the base mesh data");
	parm = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_enum(func, "settings", mesh_type_items, 0, "", "Modifier settings to apply");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_boolean(func, "result", 0, "", "Object visibility");
	RNA_def_function_return(func, parm);

#ifndef NDEBUG
	/* mesh */
	func = RNA_def_function(srna, "dm_info", "rna_Object_dm_info");
	RNA_def_function_ui_description(func, "Returns a string for derived mesh data");

	parm = RNA_def_enum(func, "type", mesh_dm_info_items, 0, "", "Modifier settings to apply");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* weak!, no way to return dynamic string type */
	parm = RNA_def_string(func, "result", NULL, 16384, "result", "");
	RNA_def_property_flag(parm, PROP_THICK_WRAP); /* needed for string return value */
	RNA_def_function_output(func, parm);
#endif /* NDEBUG */

	func = RNA_def_function(srna, "update_from_editmode", "rna_Object_update_from_editmode");
	RNA_def_function_ui_description(func, "Load the objects edit-mode data intp the object data");
	parm = RNA_def_boolean(func, "result", 0, "", "Success");
	RNA_def_function_return(func, parm);
}


void RNA_api_object_base(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "layers_from_view", "rna_ObjectBase_layers_from_view");
	RNA_def_function_ui_description(func,
	                                "Sets the object layers from a 3D View (use when adding an object in local view)");
	parm = RNA_def_pointer(func, "view", "SpaceView3D", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
}

#endif /* RNA_RUNTIME */
