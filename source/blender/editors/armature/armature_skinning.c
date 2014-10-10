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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009 full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * API's for creating vertex groups from bones
 * - Interfaces with heat weighting in meshlaplacian
 */

/** \file blender/editors/armature/armature_skinning.c
 *  \ingroup edarmature
 */

#include "DNA_mesh_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_deform.h"
#include "BKE_report.h"
#include "BKE_subsurf.h"
#include "BKE_modifier.h"

#include "ED_armature.h"
#include "ED_mesh.h"


#include "armature_intern.h"
#include "meshlaplacian.h"

#if 0
#include "reeb.h"
#endif

/* ********************************** Bone Skinning *********************************************** */

static int bone_skinnable_cb(Object *ob, Bone *bone, void *datap)
{
	/* Bones that are deforming
	 * are regarded to be "skinnable" and are eligible for
	 * auto-skinning.
	 *
	 * This function performs 2 functions:
	 *
	 *   a) It returns 1 if the bone is skinnable.
	 *      If we loop over all bones with this 
	 *      function, we can count the number of
	 *      skinnable bones.
	 *   b) If the pointer data is non null,
	 *      it is treated like a handle to a
	 *      bone pointer -- the bone pointer
	 *      is set to point at this bone, and
	 *      the pointer the handle points to
	 *      is incremented to point to the
	 *      next member of an array of pointers
	 *      to bones. This way we can loop using
	 *      this function to construct an array of
	 *      pointers to bones that point to all
	 *      skinnable bones.
	 */
	Bone ***hbone;
	int a, segments;
	struct { Object *armob; void *list; int heat; } *data = datap;

	if (!(ob->mode & OB_MODE_WEIGHT_PAINT) || !(bone->flag & BONE_HIDDEN_P)) {
		if (!(bone->flag & BONE_NO_DEFORM)) {
			if (data->heat && data->armob->pose && BKE_pose_channel_find_name(data->armob->pose, bone->name))
				segments = bone->segments;
			else
				segments = 1;
			
			if (data->list != NULL) {
				hbone = (Bone ***) &data->list;
				
				for (a = 0; a < segments; a++) {
					**hbone = bone;
					++*hbone;
				}
			}
			return segments;
		}
	}
	return 0;
}

static int vgroup_add_unique_bone_cb(Object *ob, Bone *bone, void *UNUSED(ptr)) 
{
	/* This group creates a vertex group to ob that has the
	 * same name as bone (provided the bone is skinnable). 
	 * If such a vertex group already exist the routine exits.
	 */
	if (!(bone->flag & BONE_NO_DEFORM)) {
		if (!defgroup_find_name(ob, bone->name)) {
			ED_vgroup_add_name(ob, bone->name);
			return 1;
		}
	}
	return 0;
}

static int dgroup_skinnable_cb(Object *ob, Bone *bone, void *datap) 
{
	/* Bones that are deforming
	 * are regarded to be "skinnable" and are eligible for
	 * auto-skinning.
	 *
	 * This function performs 2 functions:
	 *
	 *   a) If the bone is skinnable, it creates 
	 *      a vertex group for ob that has
	 *      the name of the skinnable bone
	 *      (if one doesn't exist already).
	 *   b) If the pointer data is non null,
	 *      it is treated like a handle to a
	 *      bDeformGroup pointer -- the 
	 *      bDeformGroup pointer is set to point
	 *      to the deform group with the bone's
	 *      name, and the pointer the handle 
	 *      points to is incremented to point to the
	 *      next member of an array of pointers
	 *      to bDeformGroups. This way we can loop using
	 *      this function to construct an array of
	 *      pointers to bDeformGroups, all with names
	 *      of skinnable bones.
	 */
	bDeformGroup ***hgroup, *defgroup = NULL;
	int a, segments;
	struct { Object *armob; void *list; int heat; } *data = datap;
	int wpmode = (ob->mode & OB_MODE_WEIGHT_PAINT);
	bArmature *arm = data->armob->data;

	if (!wpmode || !(bone->flag & BONE_HIDDEN_P)) {
		if (!(bone->flag & BONE_NO_DEFORM)) {
			if (data->heat && data->armob->pose && BKE_pose_channel_find_name(data->armob->pose, bone->name))
				segments = bone->segments;
			else
				segments = 1;
			
			if (!wpmode || ((arm->layer & bone->layer) && (bone->flag & BONE_SELECTED)))
				if (!(defgroup = defgroup_find_name(ob, bone->name)))
					defgroup = ED_vgroup_add_name(ob, bone->name);
			
			if (data->list != NULL) {
				hgroup = (bDeformGroup ***) &data->list;
				
				for (a = 0; a < segments; a++) {
					**hgroup = defgroup;
					++*hgroup;
				}
			}
			return segments;
		}
	}
	return 0;
}

static void envelope_bone_weighting(Object *ob, Mesh *mesh, float (*verts)[3], int numbones, Bone **bonelist,
                                    bDeformGroup **dgrouplist, bDeformGroup **dgroupflip,
                                    float (*root)[3], float (*tip)[3], const int *selected, float scale)
{
	/* Create vertex group weights from envelopes */

	Bone *bone;
	bDeformGroup *dgroup;
	float distance;
	int i, iflip, j;
	bool use_topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;
	bool use_mask = false;

	if ((ob->mode & OB_MODE_WEIGHT_PAINT) &&
	    (mesh->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)))
	{
		use_mask = true;
	}

	/* for each vertex in the mesh */
	for (i = 0; i < mesh->totvert; i++) {

		if (use_mask && !(mesh->mvert[i].flag & SELECT)) {
			continue;
		}

		iflip = (dgroupflip) ? mesh_get_x_mirror_vert(ob, i, use_topology) : -1;
		
		/* for each skinnable bone */
		for (j = 0; j < numbones; ++j) {
			if (!selected[j])
				continue;
			
			bone = bonelist[j];
			dgroup = dgrouplist[j];
			
			/* store the distance-factor from the vertex to the bone */
			distance = distfactor_to_bone(verts[i], root[j], tip[j],
			                              bone->rad_head * scale, bone->rad_tail * scale, bone->dist * scale);
			
			/* add the vert to the deform group if (weight != 0.0) */
			if (distance != 0.0f)
				ED_vgroup_vert_add(ob, dgroup, i, distance, WEIGHT_REPLACE);
			else
				ED_vgroup_vert_remove(ob, dgroup, i);
			
			/* do same for mirror */
			if (dgroupflip && dgroupflip[j] && iflip != -1) {
				if (distance != 0.0f)
					ED_vgroup_vert_add(ob, dgroupflip[j], iflip, distance,
					                   WEIGHT_REPLACE);
				else
					ED_vgroup_vert_remove(ob, dgroupflip[j], iflip);
			}
		}
	}
}

static void add_verts_to_dgroups(ReportList *reports, Scene *scene, Object *ob, Object *par,
                                 int heat, const bool mirror)
{
	/* This functions implements the automatic computation of vertex group
	 * weights, either through envelopes or using a heat equilibrium.
	 *
	 * This function can be called both when parenting a mesh to an armature,
	 * or in weightpaint + posemode. In the latter case selection is taken
	 * into account and vertex weights can be mirrored.
	 *
	 * The mesh vertex positions used are either the final deformed coords
	 * from the derivedmesh in weightpaint mode, the final subsurf coords
	 * when parenting, or simply the original mesh coords.
	 */

	bArmature *arm = par->data;
	Bone **bonelist, *bone;
	bDeformGroup **dgrouplist, **dgroupflip;
	bDeformGroup *dgroup;
	bPoseChannel *pchan;
	Mesh *mesh;
	Mat4 bbone_array[MAX_BBONE_SUBDIV], *bbone = NULL;
	float (*root)[3], (*tip)[3], (*verts)[3];
	int *selected;
	int numbones, vertsfilled = 0, i, j, segments = 0;
	int wpmode = (ob->mode & OB_MODE_WEIGHT_PAINT);
	struct { Object *armob; void *list; int heat; } looper_data;

	looper_data.armob = par;
	looper_data.heat = heat;
	looper_data.list = NULL;

	/* count the number of skinnable bones */
	numbones = bone_looper(ob, arm->bonebase.first, &looper_data, bone_skinnable_cb);
	
	if (numbones == 0)
		return;
	
	if (ED_vgroup_data_create(ob->data) == false)
		return;

	/* create an array of pointer to bones that are skinnable
	 * and fill it with all of the skinnable bones */
	bonelist = MEM_callocN(numbones * sizeof(Bone *), "bonelist");
	looper_data.list = bonelist;
	bone_looper(ob, arm->bonebase.first, &looper_data, bone_skinnable_cb);

	/* create an array of pointers to the deform groups that
	 * correspond to the skinnable bones (creating them
	 * as necessary. */
	dgrouplist = MEM_callocN(numbones * sizeof(bDeformGroup *), "dgrouplist");
	dgroupflip = MEM_callocN(numbones * sizeof(bDeformGroup *), "dgroupflip");

	looper_data.list = dgrouplist;
	bone_looper(ob, arm->bonebase.first, &looper_data, dgroup_skinnable_cb);

	/* create an array of root and tip positions transformed into
	 * global coords */
	root = MEM_callocN(numbones * sizeof(float) * 3, "root");
	tip = MEM_callocN(numbones * sizeof(float) * 3, "tip");
	selected = MEM_callocN(numbones * sizeof(int), "selected");

	for (j = 0; j < numbones; ++j) {
		bone = bonelist[j];
		dgroup = dgrouplist[j];
		
		/* handle bbone */
		if (heat) {
			if (segments == 0) {
				segments = 1;
				bbone = NULL;
				
				if ((par->pose) && (pchan = BKE_pose_channel_find_name(par->pose, bone->name))) {
					if (bone->segments > 1) {
						segments = bone->segments;
						b_bone_spline_setup(pchan, 1, bbone_array);
						bbone = bbone_array;
					}
				}
			}
			
			segments--;
		}
		
		/* compute root and tip */
		if (bbone) {
			mul_v3_m4v3(root[j], bone->arm_mat, bbone[segments].mat[3]);
			if ((segments + 1) < bone->segments) {
				mul_v3_m4v3(tip[j], bone->arm_mat, bbone[segments + 1].mat[3]);
			}
			else {
				copy_v3_v3(tip[j], bone->arm_tail);
			}
		}
		else {
			copy_v3_v3(root[j], bone->arm_head);
			copy_v3_v3(tip[j], bone->arm_tail);
		}
		
		mul_m4_v3(par->obmat, root[j]);
		mul_m4_v3(par->obmat, tip[j]);
		
		/* set selected */
		if (wpmode) {
			if ((arm->layer & bone->layer) && (bone->flag & BONE_SELECTED))
				selected[j] = 1;
		}
		else
			selected[j] = 1;
		
		/* find flipped group */
		if (dgroup && mirror) {
			char name_flip[MAXBONENAME];

			BKE_deform_flip_side_name(name_flip, dgroup->name, false);
			dgroupflip[j] = defgroup_find_name(ob, name_flip);
		}
	}

	/* create verts */
	mesh = (Mesh *)ob->data;
	verts = MEM_callocN(mesh->totvert * sizeof(*verts), "closestboneverts");

	if (wpmode) {
		/* if in weight paint mode, use final verts from derivedmesh */
		DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
		
		if (dm->foreachMappedVert) {
			mesh_get_mapped_verts_coords(dm, verts, mesh->totvert);
			vertsfilled = 1;
		}
		
		dm->release(dm);
	}
	else if (modifiers_findByType(ob, eModifierType_Subsurf)) {
		/* is subsurf on? Lets use the verts on the limit surface then.
		 * = same amount of vertices as mesh, but vertices  moved to the
		 * subsurfed position, like for 'optimal'. */
		subsurf_calculate_limit_positions(mesh, verts);
		vertsfilled = 1;
	}

	/* transform verts to global space */
	for (i = 0; i < mesh->totvert; i++) {
		if (!vertsfilled)
			copy_v3_v3(verts[i], mesh->mvert[i].co);
		mul_m4_v3(ob->obmat, verts[i]);
	}

	/* compute the weights based on gathered vertices and bones */
	if (heat) {
		const char *error = NULL;

#ifdef WITH_OPENNL
		heat_bone_weighting(ob, mesh, verts, numbones, dgrouplist, dgroupflip,
		                    root, tip, selected, &error);
#else
		error = "Built without OpenNL";
#endif
		if (error) {
			BKE_report(reports, RPT_WARNING, error);
		}
	}
	else {
		envelope_bone_weighting(ob, mesh, verts, numbones, bonelist, dgrouplist,
		                        dgroupflip, root, tip, selected, mat4_to_scale(par->obmat));
	}

	/* only generated in some cases but can call anyway */
	ED_mesh_mirror_spatial_table(ob, NULL, NULL, 'e');

	/* free the memory allocated */
	MEM_freeN(bonelist);
	MEM_freeN(dgrouplist);
	MEM_freeN(dgroupflip);
	MEM_freeN(root);
	MEM_freeN(tip);
	MEM_freeN(selected);
	MEM_freeN(verts);
}

void create_vgroups_from_armature(ReportList *reports, Scene *scene, Object *ob, Object *par,
                                  const int mode, const bool mirror)
{
	/* Lets try to create some vertex groups 
	 * based on the bones of the parent armature.
	 */
	bArmature *arm = par->data;

	if (mode == ARM_GROUPS_NAME) {
		const int defbase_tot = BLI_countlist(&ob->defbase);
		int defbase_add;
		/* Traverse the bone list, trying to create empty vertex 
		 * groups corresponding to the bone.
		 */
		defbase_add = bone_looper(ob, arm->bonebase.first, NULL, vgroup_add_unique_bone_cb);

		if (defbase_add) {
			/* its possible there are DWeight's outside the range of the current
			 * objects deform groups, in this case the new groups wont be empty [#33889] */
			ED_vgroup_data_clamp_range(ob->data, defbase_tot);
		}
	}
	else if (ELEM(mode, ARM_GROUPS_ENVELOPE, ARM_GROUPS_AUTO)) {
		/* Traverse the bone list, trying to create vertex groups 
		 * that are populated with the vertices for which the
		 * bone is closest.
		 */
		add_verts_to_dgroups(reports, scene, ob, par, (mode == ARM_GROUPS_AUTO), mirror);
	}
}
