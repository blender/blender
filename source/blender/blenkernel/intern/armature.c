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
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/armature.c
 *  \ingroup bke
 */


#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_ghash.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_lattice_types.h"
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_build.h"

#include "BIK_api.h"

/* **************** Generic Functions, data level *************** */

bArmature *BKE_armature_add(Main *bmain, const char *name)
{
	bArmature *arm;

	arm = BKE_libblock_alloc(bmain, ID_AR, name, 0);
	arm->deformflag = ARM_DEF_VGROUP | ARM_DEF_ENVELOPE;
	arm->flag = ARM_COL_CUSTOM; /* custom bone-group colors */
	arm->layer = 1;
	arm->ghostsize = 1;
	return arm;
}

bArmature *BKE_armature_from_object(Object *ob)
{
	if (ob->type == OB_ARMATURE)
		return (bArmature *)ob->data;
	return NULL;
}

int BKE_armature_bonelist_count(ListBase *lb)
{
	int i = 0;
	for (Bone *bone = lb->first; bone; bone = bone->next) {
		i += 1 + BKE_armature_bonelist_count(&bone->childbase);
	}

	return i;
}

void BKE_armature_bonelist_free(ListBase *lb)
{
	Bone *bone;

	for (bone = lb->first; bone; bone = bone->next) {
		if (bone->prop) {
			IDP_FreeProperty(bone->prop);
			MEM_freeN(bone->prop);
		}
		BKE_armature_bonelist_free(&bone->childbase);
	}

	BLI_freelistN(lb);
}

/** Free (or release) any data used by this armature (does not free the armature itself). */
void BKE_armature_free(bArmature *arm)
{
	BKE_animdata_free(&arm->id, false);

	BKE_armature_bonelist_free(&arm->bonebase);

	/* free editmode data */
	if (arm->edbo) {
		BLI_freelistN(arm->edbo);

		MEM_freeN(arm->edbo);
		arm->edbo = NULL;
	}
}

void BKE_armature_make_local(Main *bmain, bArmature *arm, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &arm->id, true, lib_local);
}

static void copy_bonechildren(
        Bone *bone_dst, const Bone *bone_src, const Bone *bone_src_act, Bone **r_bone_dst_act, const int flag)
{
	Bone *bone_src_child, *bone_dst_child;

	if (bone_src == bone_src_act) {
		*r_bone_dst_act = bone_dst;
	}

	if (bone_src->prop) {
		bone_dst->prop = IDP_CopyProperty_ex(bone_src->prop, flag);
	}

	/* Copy this bone's list */
	BLI_duplicatelist(&bone_dst->childbase, &bone_src->childbase);

	/* For each child in the list, update it's children */
	for (bone_src_child = bone_src->childbase.first, bone_dst_child = bone_dst->childbase.first;
	     bone_src_child;
	     bone_src_child = bone_src_child->next, bone_dst_child = bone_dst_child->next)
	{
		bone_dst_child->parent = bone_dst;
		copy_bonechildren(bone_dst_child, bone_src_child, bone_src_act, r_bone_dst_act, flag);
	}
}

/**
 * Only copy internal data of Armature ID from source to already allocated/initialized destination.
 * You probably nerver want to use that directly, use id_copy or BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_armature_copy_data(Main *UNUSED(bmain), bArmature *arm_dst, const bArmature *arm_src, const int flag)
{
	Bone *bone_src, *bone_dst;
	Bone *bone_dst_act = NULL;

	/* We never handle usercount here for own data. */
	const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

	BLI_duplicatelist(&arm_dst->bonebase, &arm_src->bonebase);

	/* Duplicate the childrens' lists */
	bone_dst = arm_dst->bonebase.first;
	for (bone_src = arm_src->bonebase.first; bone_src; bone_src = bone_src->next) {
		bone_dst->parent = NULL;
		copy_bonechildren(bone_dst, bone_src, arm_src->act_bone, &bone_dst_act, flag_subdata);
		bone_dst = bone_dst->next;
	}

	arm_dst->act_bone = bone_dst_act;

	arm_dst->edbo = NULL;
	arm_dst->act_edbone = NULL;
}

bArmature *BKE_armature_copy(Main *bmain, const bArmature *arm)
{
	bArmature *arm_copy;
	BKE_id_copy_ex(bmain, &arm->id, (ID **)&arm_copy, 0, false);
	return arm_copy;
}

static Bone *get_named_bone_bonechildren(ListBase *lb, const char *name)
{
	Bone *curBone, *rbone;

	for (curBone = lb->first; curBone; curBone = curBone->next) {
		if (STREQ(curBone->name, name))
			return curBone;

		rbone = get_named_bone_bonechildren(&curBone->childbase, name);
		if (rbone)
			return rbone;
	}

	return NULL;
}


/**
 * Walk the list until the bone is found (slow!),
 * use #BKE_armature_bone_from_name_map for multiple lookups.
 */
Bone *BKE_armature_find_bone_name(bArmature *arm, const char *name)
{
	if (!arm)
		return NULL;

	return get_named_bone_bonechildren(&arm->bonebase, name);
}

static void armature_bone_from_name_insert_recursive(GHash *bone_hash, ListBase *lb)
{
	for (Bone *bone = lb->first; bone; bone = bone->next) {
		BLI_ghash_insert(bone_hash, bone->name, bone);
		armature_bone_from_name_insert_recursive(bone_hash, &bone->childbase);
	}
}

/**
 * Create a (name -> bone) map.
 *
 * \note typically #bPose.chanhash us used via #BKE_pose_channel_find_name
 * this is for the cases we can't use pose channels.
 */
GHash *BKE_armature_bone_from_name_map(bArmature *arm)
{
	const int bones_count = BKE_armature_bonelist_count(&arm->bonebase);
	GHash *bone_hash = BLI_ghash_str_new_ex(__func__, bones_count);
	armature_bone_from_name_insert_recursive(bone_hash, &arm->bonebase);
	return bone_hash;
}

bool BKE_armature_bone_flag_test_recursive(const Bone *bone, int flag)
{
	if (bone->flag & flag) {
		return true;
	}
	else if (bone->parent) {
		return BKE_armature_bone_flag_test_recursive(bone->parent, flag);
	}
	else {
		return false;
	}
}

/* Finds the best possible extension to the name on a particular axis. (For renaming, check for
 * unique names afterwards) strip_number: removes number extensions  (TODO: not used)
 * axis: the axis to name on
 * head/tail: the head/tail co-ordinate of the bone on the specified axis */
int bone_autoside_name(char name[MAXBONENAME], int UNUSED(strip_number), short axis, float head, float tail)
{
	unsigned int len;
	char basename[MAXBONENAME] = "";
	char extension[5] = "";

	len = strlen(name);
	if (len == 0)
		return 0;
	BLI_strncpy(basename, name, sizeof(basename));

	/* Figure out extension to append:
	 *	- The extension to append is based upon the axis that we are working on.
	 *	- If head happens to be on 0, then we must consider the tail position as well to decide
	 *	  which side the bone is on
	 *		-> If tail is 0, then it's bone is considered to be on axis, so no extension should be added
	 *		-> Otherwise, extension is added from perspective of object based on which side tail goes to
	 *	- If head is non-zero, extension is added from perspective of object based on side head is on
	 */
	if (axis == 2) {
		/* z-axis - vertical (top/bottom) */
		if (IS_EQF(head, 0.0f)) {
			if (tail < 0)
				strcpy(extension, "Bot");
			else if (tail > 0)
				strcpy(extension, "Top");
		}
		else {
			if (head < 0)
				strcpy(extension, "Bot");
			else
				strcpy(extension, "Top");
		}
	}
	else if (axis == 1) {
		/* y-axis - depth (front/back) */
		if (IS_EQF(head, 0.0f)) {
			if (tail < 0)
				strcpy(extension, "Fr");
			else if (tail > 0)
				strcpy(extension, "Bk");
		}
		else {
			if (head < 0)
				strcpy(extension, "Fr");
			else
				strcpy(extension, "Bk");
		}
	}
	else {
		/* x-axis - horizontal (left/right) */
		if (IS_EQF(head, 0.0f)) {
			if (tail < 0)
				strcpy(extension, "R");
			else if (tail > 0)
				strcpy(extension, "L");
		}
		else {
			if (head < 0)
				strcpy(extension, "R");
			/* XXX Shouldn't this be simple else, as for z and y axes? */
			else if (head > 0)
				strcpy(extension, "L");
		}
	}

	/* Simple name truncation
	 *	- truncate if there is an extension and it wouldn't be able to fit
	 *	- otherwise, just append to end
	 */
	if (extension[0]) {
		bool changed = true;

		while (changed) { /* remove extensions */
			changed = false;
			if (len > 2 && basename[len - 2] == '.') {
				if (basename[len - 1] == 'L' || basename[len - 1] == 'R') { /* L R */
					basename[len - 2] = '\0';
					len -= 2;
					changed = true;
				}
			}
			else if (len > 3 && basename[len - 3] == '.') {
				if ((basename[len - 2] == 'F' && basename[len - 1] == 'r') || /* Fr */
				    (basename[len - 2] == 'B' && basename[len - 1] == 'k')) /* Bk */
				{
					basename[len - 3] = '\0';
					len -= 3;
					changed = true;
				}
			}
			else if (len > 4 && basename[len - 4] == '.') {
				if ((basename[len - 3] == 'T' && basename[len - 2] == 'o' && basename[len - 1] == 'p') || /* Top */
				    (basename[len - 3] == 'B' && basename[len - 2] == 'o' && basename[len - 1] == 't')) /* Bot */
				{
					basename[len - 4] = '\0';
					len -= 4;
					changed = true;
				}
			}
		}

		if ((MAXBONENAME - len) < strlen(extension) + 1) { /* add 1 for the '.' */
			strncpy(name, basename, len - strlen(extension));
		}

		BLI_snprintf(name, MAXBONENAME, "%s.%s", basename, extension);

		return 1;
	}

	else
		return 0;
}

/* ************* B-Bone support ******************* */

/* data has MAX_BBONE_SUBDIV+1 interpolated points, will become desired amount with equal distances */
void equalize_bbone_bezier(float *data, int desired)
{
	float *fp, totdist, ddist, dist, fac1, fac2;
	float pdist[MAX_BBONE_SUBDIV + 1];
	float temp[MAX_BBONE_SUBDIV + 1][4];
	int a, nr;

	pdist[0] = 0.0f;
	for (a = 0, fp = data; a < MAX_BBONE_SUBDIV; a++, fp += 4) {
		copy_qt_qt(temp[a], fp);
		pdist[a + 1] = pdist[a] + len_v3v3(fp, fp + 4);
	}
	/* do last point */
	copy_qt_qt(temp[a], fp);
	totdist = pdist[a];

	/* go over distances and calculate new points */
	ddist = totdist / ((float)desired);
	nr = 1;
	for (a = 1, fp = data + 4; a < desired; a++, fp += 4) {
		dist = ((float)a) * ddist;

		/* we're looking for location (distance) 'dist' in the array */
		while ((nr < MAX_BBONE_SUBDIV) && (dist >= pdist[nr]))
			nr++;

		fac1 = pdist[nr] - pdist[nr - 1];
		fac2 = pdist[nr] - dist;
		fac1 = fac2 / fac1;
		fac2 = 1.0f - fac1;

		fp[0] = fac1 * temp[nr - 1][0] + fac2 * temp[nr][0];
		fp[1] = fac1 * temp[nr - 1][1] + fac2 * temp[nr][1];
		fp[2] = fac1 * temp[nr - 1][2] + fac2 * temp[nr][2];
		fp[3] = fac1 * temp[nr - 1][3] + fac2 * temp[nr][3];
	}
	/* set last point, needed for orientation calculus */
	copy_qt_qt(fp, temp[MAX_BBONE_SUBDIV]);
}

/* get "next" and "prev" bones - these are used for handle calculations */
void BKE_pchan_get_bbone_handles(bPoseChannel *pchan, bPoseChannel **r_prev, bPoseChannel **r_next)
{
	if (pchan->bboneflag & PCHAN_BBONE_CUSTOM_HANDLES) {
		/* use the provided bones as the next/prev - leave blank to eliminate this effect altogether */
		*r_prev = pchan->bbone_prev;
		*r_next = pchan->bbone_next;
	}
	else {
		/* evaluate next and prev bones */
		if (pchan->bone->flag & BONE_CONNECTED)
			*r_prev = pchan->parent;
		else
			*r_prev = NULL;

		*r_next = pchan->child;
	}
}

/* returns pointer to static array, filled with desired amount of bone->segments elements */
/* this calculation is done  within unit bone space */
void b_bone_spline_setup(bPoseChannel *pchan, int rest, Mat4 result_array[MAX_BBONE_SUBDIV])
{
	bPoseChannel *next, *prev;
	Bone *bone = pchan->bone;
	float h1[3], h2[3], scale[3], length, roll1 = 0.0f, roll2;
	float mat3[3][3], imat[4][4], posemat[4][4], scalemat[4][4], iscalemat[4][4];
	float data[MAX_BBONE_SUBDIV + 1][4], *fp;
	int a;
	bool do_scale = false;

	length = bone->length;

	if (!rest) {
		/* check if we need to take non-uniform bone scaling into account */
		mat4_to_size(scale, pchan->pose_mat);

		if (fabsf(scale[0] - scale[1]) > 1e-6f || fabsf(scale[1] - scale[2]) > 1e-6f) {
			size_to_mat4(scalemat, scale);
			invert_m4_m4(iscalemat, scalemat);

			length *= scale[1];
			do_scale = 1;
		}
	}

	BKE_pchan_get_bbone_handles(pchan, &prev, &next);

	/* find the handle points, since this is inside bone space, the
	 * first point = (0, 0, 0)
	 * last point =  (0, length, 0) */
	if (rest) {
		invert_m4_m4(imat, pchan->bone->arm_mat);
	}
	else if (do_scale) {
		copy_m4_m4(posemat, pchan->pose_mat);
		normalize_m4(posemat);
		invert_m4_m4(imat, posemat);
	}
	else
		invert_m4_m4(imat, pchan->pose_mat);

	if (prev) {
		float difmat[4][4], result[3][3], imat3[3][3];

		/* transform previous point inside this bone space */
		if ((pchan->bboneflag & PCHAN_BBONE_CUSTOM_HANDLES) &&
		    (pchan->bboneflag & PCHAN_BBONE_CUSTOM_START_REL))
		{
			/* Use delta movement (from restpose), and apply this relative to the current bone's head */
			if (rest) {
				/* in restpose, arm_head == pose_head */
				h1[0] = h1[1] = h1[2] = 0.0f;
			}
			else {
				float delta[3];
				sub_v3_v3v3(delta, prev->pose_head, prev->bone->arm_head);
				sub_v3_v3v3(h1, pchan->pose_head, delta);
			}
		}
		else {
			/* Use bone head as absolute position */
			if (rest)
				copy_v3_v3(h1, prev->bone->arm_head);
			else
				copy_v3_v3(h1, prev->pose_head);
		}
		mul_m4_v3(imat, h1);

		if (prev->bone->segments > 1) {
			/* if previous bone is B-bone too, use average handle direction */
			h1[1] -= length;
			roll1 = 0.0f;
		}

		normalize_v3(h1);
		negate_v3(h1);

		if (prev->bone->segments == 1) {
			/* find the previous roll to interpolate */
			if (rest)
				mul_m4_m4m4(difmat, imat, prev->bone->arm_mat);
			else
				mul_m4_m4m4(difmat, imat, prev->pose_mat);
			copy_m3_m4(result, difmat); /* the desired rotation at beginning of next bone */

			vec_roll_to_mat3(h1, 0.0f, mat3); /* the result of vec_roll without roll */

			invert_m3_m3(imat3, mat3);
			mul_m3_m3m3(mat3, result, imat3); /* the matrix transforming vec_roll to desired roll */

			roll1 = atan2f(mat3[2][0], mat3[2][2]);
		}
	}
	else {
		h1[0] = 0.0f; h1[1] = 1.0; h1[2] = 0.0f;
		roll1 = 0.0f;
	}
	if (next) {
		float difmat[4][4], result[3][3], imat3[3][3];

		/* transform next point inside this bone space */
		if ((pchan->bboneflag & PCHAN_BBONE_CUSTOM_HANDLES) &&
		    (pchan->bboneflag & PCHAN_BBONE_CUSTOM_END_REL))
		{
			/* Use delta movement (from restpose), and apply this relative to the current bone's tail */
			if (rest) {
				/* in restpose, arm_tail == pose_tail */
				h2[0] = h2[1] = h2[2] = 0.0f;
			}
			else {
				float delta[3];
				sub_v3_v3v3(delta, next->pose_tail, next->bone->arm_tail);
				add_v3_v3v3(h2, pchan->pose_tail, delta);
			}
		}
		else {
			/* Use bone tail as absolute position */
			if (rest)
				copy_v3_v3(h2, next->bone->arm_tail);
			else
				copy_v3_v3(h2, next->pose_tail);
		}
		mul_m4_v3(imat, h2);

		/* if next bone is B-bone too, use average handle direction */
		if (next->bone->segments > 1) {
			/* pass */
		}
		else {
			h2[1] -= length;
		}
		normalize_v3(h2);

		/* find the next roll to interpolate as well */
		if (rest)
			mul_m4_m4m4(difmat, imat, next->bone->arm_mat);
		else
			mul_m4_m4m4(difmat, imat, next->pose_mat);
		copy_m3_m4(result, difmat); /* the desired rotation at beginning of next bone */

		vec_roll_to_mat3(h2, 0.0f, mat3); /* the result of vec_roll without roll */

		invert_m3_m3(imat3, mat3);
		mul_m3_m3m3(mat3, imat3, result); /* the matrix transforming vec_roll to desired roll */

		roll2 = atan2f(mat3[2][0], mat3[2][2]);

	}
	else {
		h2[0] = 0.0f; h2[1] = 1.0f; h2[2] = 0.0f;
		roll2 = 0.0;
	}

	{
		const float circle_factor = length * (cubic_tangent_factor_circle_v3(h1, h2) / 0.75f);

		const float combined_ease1 = bone->ease1 + (!rest ? pchan->ease1 : 0.0f);
		const float combined_ease2 = bone->ease2 + (!rest ? pchan->ease2 : 0.0f);
		const float hlength1 = combined_ease1 * circle_factor;
		const float hlength2 = combined_ease2 * circle_factor;

		/* and only now negate h2 */
		mul_v3_fl(h1,  hlength1);
		mul_v3_fl(h2, -hlength2);
	}

	/* Add effects from bbone properties over the top
	 * - These properties allow users to hand-animate the
	 *   bone curve/shape, without having to resort to using
	 *   extra bones
	 * - The "bone" level offsets are for defining the restpose
	 *   shape of the bone (e.g. for curved eyebrows for example).
	 *   -> In the viewport, it's needed to define what the rest pose
	 *      looks like
	 *   -> For "rest == 0", we also still need to have it present
	 *      so that we can "cancel out" this restpose when it comes
	 *      time to deform some geometry, it won't cause double transforms.
	 * - The "pchan" level offsets are the ones that animators actually
	 *   end up animating
	 */
	{
		/* add extra rolls */
		roll1 += bone->roll1 + (!rest ? pchan->roll1 : 0.0f);
		roll2 += bone->roll2 + (!rest ? pchan->roll2 : 0.0f);

		if (bone->flag & BONE_ADD_PARENT_END_ROLL) {
			if (prev) {
				if (prev->bone)
					roll1 += prev->bone->roll2;

				if (!rest)
					roll1 += prev->roll2;
			}
		}

		/* extra curve x / y */
		/* NOTE: Scale correction factors here are to compensate for some random floating-point glitches
		 *       when scaling up the bone or it's parent by a factor of approximately 8.15/6, which results
		 *       in the bone length getting scaled up too (from 1 to 8), causing the curve to flatten out.
		 */
		const float xscale_correction = (do_scale) ? scale[0] : 1.0f;
		const float yscale_correction = (do_scale) ? scale[2] : 1.0f;

		h1[0] += (bone->curveInX + (!rest ? pchan->curveInX : 0.0f)) * xscale_correction;
		h1[2] += (bone->curveInY + (!rest ? pchan->curveInY : 0.0f)) * yscale_correction;

		h2[0] += (bone->curveOutX + (!rest ? pchan->curveOutX : 0.0f)) * xscale_correction;
		h2[2] += (bone->curveOutY + (!rest ? pchan->curveOutY : 0.0f)) * yscale_correction;
	}

	/* make curve */
	if (bone->segments > MAX_BBONE_SUBDIV)
		bone->segments = MAX_BBONE_SUBDIV;

	BKE_curve_forward_diff_bezier(0.0f,  h1[0],                               h2[0],                               0.0f,   data[0],     MAX_BBONE_SUBDIV, 4 * sizeof(float));
	BKE_curve_forward_diff_bezier(0.0f,  h1[1],                               length + h2[1],                      length, data[0] + 1, MAX_BBONE_SUBDIV, 4 * sizeof(float));
	BKE_curve_forward_diff_bezier(0.0f,  h1[2],                               h2[2],                               0.0f,   data[0] + 2, MAX_BBONE_SUBDIV, 4 * sizeof(float));
	BKE_curve_forward_diff_bezier(roll1, roll1 + 0.390464f * (roll2 - roll1), roll2 - 0.390464f * (roll2 - roll1), roll2,  data[0] + 3, MAX_BBONE_SUBDIV, 4 * sizeof(float));

	equalize_bbone_bezier(data[0], bone->segments); /* note: does stride 4! */

	/* make transformation matrices for the segments for drawing */
	for (a = 0, fp = data[0]; a < bone->segments; a++, fp += 4) {
		sub_v3_v3v3(h1, fp + 4, fp);
		vec_roll_to_mat3(h1, fp[3], mat3); /* fp[3] is roll */

		copy_m4_m3(result_array[a].mat, mat3);
		copy_v3_v3(result_array[a].mat[3], fp);

		if (do_scale) {
			/* correct for scaling when this matrix is used in scaled space */
			mul_m4_series(result_array[a].mat, iscalemat, result_array[a].mat, scalemat);
		}

		/* BBone scale... */
		{
			const int num_segments = bone->segments;

			const float scaleIn = bone->scaleIn * (!rest ? pchan->scaleIn : 1.0f);
			const float scaleFactorIn  = 1.0f + (scaleIn  - 1.0f) * ((float)(num_segments - a) / (float)num_segments);

			const float scaleOut = bone->scaleOut * (!rest ? pchan->scaleOut : 1.0f);
			const float scaleFactorOut = 1.0f + (scaleOut - 1.0f) * ((float)(a + 1)            / (float)num_segments);

			const float scalefac = scaleFactorIn * scaleFactorOut;
			float bscalemat[4][4], bscale[3];

			bscale[0] = scalefac;
			bscale[1] = 1.0f;
			bscale[2] = scalefac;

			size_to_mat4(bscalemat, bscale);

			/* Note: don't multiply by inverse scale mat here, as it causes problems with scaling shearing and breaking segment chains */
			/*mul_m4_series(result_array[a].mat, ibscalemat, result_array[a].mat, bscalemat);*/
			mul_m4_series(result_array[a].mat, result_array[a].mat, bscalemat);
		}

	}
}

/* ************ Armature Deform ******************* */

typedef struct bPoseChanDeform {
	Mat4     *b_bone_mats;
	DualQuat *dual_quat;
	DualQuat *b_bone_dual_quats;
} bPoseChanDeform;

static void pchan_b_bone_defmats(bPoseChannel *pchan, bPoseChanDeform *pdef_info, const bool use_quaternion)
{
	Bone *bone = pchan->bone;
	Mat4 b_bone[MAX_BBONE_SUBDIV], b_bone_rest[MAX_BBONE_SUBDIV];
	Mat4 *b_bone_mats;
	DualQuat *b_bone_dual_quats = NULL;
	int a;

	b_bone_spline_setup(pchan, 0, b_bone);
	b_bone_spline_setup(pchan, 1, b_bone_rest);

	/* allocate b_bone matrices and dual quats */
	b_bone_mats = MEM_mallocN((1 + bone->segments) * sizeof(Mat4), "BBone defmats");
	pdef_info->b_bone_mats = b_bone_mats;

	if (use_quaternion) {
		b_bone_dual_quats = MEM_mallocN((bone->segments) * sizeof(DualQuat), "BBone dqs");
		pdef_info->b_bone_dual_quats = b_bone_dual_quats;
	}

	/* first matrix is the inverse arm_mat, to bring points in local bone space
	 * for finding out which segment it belongs to */
	invert_m4_m4(b_bone_mats[0].mat, bone->arm_mat);

	/* then we make the b_bone_mats:
	 * - first transform to local bone space
	 * - translate over the curve to the bbone mat space
	 * - transform with b_bone matrix
	 * - transform back into global space */

	for (a = 0; a < bone->segments; a++) {
		float tmat[4][4];

		invert_m4_m4(tmat, b_bone_rest[a].mat);
		mul_m4_series(b_bone_mats[a + 1].mat, pchan->chan_mat, bone->arm_mat, b_bone[a].mat, tmat, b_bone_mats[0].mat);

		if (use_quaternion)
			mat4_to_dquat(&b_bone_dual_quats[a], bone->arm_mat, b_bone_mats[a + 1].mat);
	}
}

static void b_bone_deform(bPoseChanDeform *pdef_info, Bone *bone, float co[3], DualQuat *dq, float defmat[3][3])
{
	Mat4 *b_bone = pdef_info->b_bone_mats;
	float (*mat)[4] = b_bone[0].mat;
	float segment, y;
	int a;

	/* need to transform co back to bonespace, only need y */
	y = mat[0][1] * co[0] + mat[1][1] * co[1] + mat[2][1] * co[2] + mat[3][1];

	/* now calculate which of the b_bones are deforming this */
	segment = bone->length / ((float)bone->segments);
	a = (int)(y / segment);

	/* note; by clamping it extends deform at endpoints, goes best with
	 * straight joints in restpos. */
	CLAMP(a, 0, bone->segments - 1);

	if (dq) {
		copy_dq_dq(dq, &(pdef_info->b_bone_dual_quats)[a]);
	}
	else {
		mul_m4_v3(b_bone[a + 1].mat, co);

		if (defmat) {
			copy_m3_m4(defmat, b_bone[a + 1].mat);
		}
	}
}

/* using vec with dist to bone b1 - b2 */
float distfactor_to_bone(const float vec[3], const float b1[3], const float b2[3], float rad1, float rad2, float rdist)
{
	float dist_sq;
	float bdelta[3];
	float pdelta[3];
	float hsqr, a, l, rad;

	sub_v3_v3v3(bdelta, b2, b1);
	l = normalize_v3(bdelta);

	sub_v3_v3v3(pdelta, vec, b1);

	a = dot_v3v3(bdelta, pdelta);
	hsqr = len_squared_v3(pdelta);

	if (a < 0.0f) {
		/* If we're past the end of the bone, do a spherical field attenuation thing */
		dist_sq = len_squared_v3v3(b1, vec);
		rad = rad1;
	}
	else if (a > l) {
		/* If we're past the end of the bone, do a spherical field attenuation thing */
		dist_sq = len_squared_v3v3(b2, vec);
		rad = rad2;
	}
	else {
		dist_sq = (hsqr - (a * a));

		if (l != 0.0f) {
			rad = a / l;
			rad = rad * rad2 + (1.0f - rad) * rad1;
		}
		else
			rad = rad1;
	}

	a = rad * rad;
	if (dist_sq < a)
		return 1.0f;
	else {
		l = rad + rdist;
		l *= l;
		if (rdist == 0.0f || dist_sq >= l)
			return 0.0f;
		else {
			a = sqrtf(dist_sq) - rad;
			return 1.0f - (a * a) / (rdist * rdist);
		}
	}
}

static void pchan_deform_mat_add(bPoseChannel *pchan, float weight, float bbonemat[3][3], float mat[3][3])
{
	float wmat[3][3];

	if (pchan->bone->segments > 1)
		copy_m3_m3(wmat, bbonemat);
	else
		copy_m3_m4(wmat, pchan->chan_mat);

	mul_m3_fl(wmat, weight);
	add_m3_m3m3(mat, mat, wmat);
}

static float dist_bone_deform(bPoseChannel *pchan, bPoseChanDeform *pdef_info, float vec[3], DualQuat *dq,
                              float mat[3][3], const float co[3])
{
	Bone *bone = pchan->bone;
	float fac, contrib = 0.0;
	float cop[3], bbonemat[3][3];
	DualQuat bbonedq;

	if (bone == NULL)
		return 0.0f;

	copy_v3_v3(cop, co);

	fac = distfactor_to_bone(cop, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);

	if (fac > 0.0f) {
		fac *= bone->weight;
		contrib = fac;
		if (contrib > 0.0f) {
			if (vec) {
				if (bone->segments > 1)
					/* applies on cop and bbonemat */
					b_bone_deform(pdef_info, bone, cop, NULL, (mat) ? bbonemat : NULL);
				else
					mul_m4_v3(pchan->chan_mat, cop);

				/* Make this a delta from the base position */
				sub_v3_v3(cop, co);
				madd_v3_v3fl(vec, cop, fac);

				if (mat)
					pchan_deform_mat_add(pchan, fac, bbonemat, mat);
			}
			else {
				if (bone->segments > 1) {
					b_bone_deform(pdef_info, bone, cop, &bbonedq, NULL);
					add_weighted_dq_dq(dq, &bbonedq, fac);
				}
				else
					add_weighted_dq_dq(dq, pdef_info->dual_quat, fac);
			}
		}
	}

	return contrib;
}

static void pchan_bone_deform(bPoseChannel *pchan, bPoseChanDeform *pdef_info, float weight, float vec[3], DualQuat *dq,
                              float mat[3][3], const float co[3], float *contrib)
{
	float cop[3], bbonemat[3][3];
	DualQuat bbonedq;

	if (!weight)
		return;

	copy_v3_v3(cop, co);

	if (vec) {
		if (pchan->bone->segments > 1)
			/* applies on cop and bbonemat */
			b_bone_deform(pdef_info, pchan->bone, cop, NULL, (mat) ? bbonemat : NULL);
		else
			mul_m4_v3(pchan->chan_mat, cop);

		vec[0] += (cop[0] - co[0]) * weight;
		vec[1] += (cop[1] - co[1]) * weight;
		vec[2] += (cop[2] - co[2]) * weight;

		if (mat)
			pchan_deform_mat_add(pchan, weight, bbonemat, mat);
	}
	else {
		if (pchan->bone->segments > 1) {
			b_bone_deform(pdef_info, pchan->bone, cop, &bbonedq, NULL);
			add_weighted_dq_dq(dq, &bbonedq, weight);
		}
		else
			add_weighted_dq_dq(dq, pdef_info->dual_quat, weight);
	}

	(*contrib) += weight;
}

typedef struct ArmatureBBoneDefmatsData {
	bPoseChanDeform *pdef_info_array;
	DualQuat *dualquats;
	bool use_quaternion;
} ArmatureBBoneDefmatsData;

static void armature_bbone_defmats_cb(void *userdata, Link *iter, int index)
{
	ArmatureBBoneDefmatsData *data = userdata;
	bPoseChannel *pchan = (bPoseChannel *)iter;

	if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
		bPoseChanDeform *pdef_info = &data->pdef_info_array[index];
		const bool use_quaternion = data->use_quaternion;

		if (pchan->bone->segments > 1) {
			pchan_b_bone_defmats(pchan, pdef_info, use_quaternion);
		}

		if (use_quaternion) {
			pdef_info->dual_quat = &data->dualquats[index];
			mat4_to_dquat(pdef_info->dual_quat, pchan->bone->arm_mat, pchan->chan_mat);
		}
	}
}

void armature_deform_verts(Object *armOb, Object *target, const Mesh * mesh, float (*vertexCos)[3],
                           float (*defMats)[3][3], int numVerts, int deformflag,
                           float (*prevCos)[3], const char *defgrp_name)
{
	bPoseChanDeform *pdef_info_array;
	bPoseChanDeform *pdef_info = NULL;
	bArmature *arm = armOb->data;
	bPoseChannel *pchan, **defnrToPC = NULL;
	int *defnrToPCIndex = NULL;
	MDeformVert *dverts = NULL;
	bDeformGroup *dg;
	DualQuat *dualquats = NULL;
	float obinv[4][4], premat[4][4], postmat[4][4];
	const bool use_envelope   = (deformflag & ARM_DEF_ENVELOPE) != 0;
	const bool use_quaternion = (deformflag & ARM_DEF_QUATERNION) != 0;
	const bool invert_vgroup  = (deformflag & ARM_DEF_INVERT_VGROUP) != 0;
	int defbase_tot = 0;       /* safety for vertexgroup index overflow */
	int i, target_totvert = 0; /* safety for vertexgroup overflow */
	bool use_dverts = false;
	int armature_def_nr;
	int totchan;

	/* in editmode, or not an armature */
	if (arm->edbo || (armOb->pose == NULL)) {
		return;
	}

	if ((armOb->pose->flag & POSE_RECALC) != 0) {
		printf("ERROR! Trying to evaluate influence of armature '%s' which needs Pose recalc!", armOb->id.name);
		BLI_assert(0);
	}

	invert_m4_m4(obinv, target->obmat);
	copy_m4_m4(premat, target->obmat);
	mul_m4_m4m4(postmat, obinv, armOb->obmat);
	invert_m4_m4(premat, postmat);

	/* bone defmats are already in the channels, chan_mat */

	/* initialize B_bone matrices and dual quaternions */
	totchan = BLI_listbase_count(&armOb->pose->chanbase);

	if (use_quaternion) {
		dualquats = MEM_callocN(sizeof(DualQuat) * totchan, "dualquats");
	}

	pdef_info_array = MEM_callocN(sizeof(bPoseChanDeform) * totchan, "bPoseChanDeform");

	ArmatureBBoneDefmatsData data = {
	    .pdef_info_array = pdef_info_array, .dualquats = dualquats, .use_quaternion = use_quaternion
	};
	BLI_task_parallel_listbase(&armOb->pose->chanbase, &data, armature_bbone_defmats_cb, totchan > 512);

	/* get the def_nr for the overall armature vertex group if present */
	armature_def_nr = defgroup_name_index(target, defgrp_name);

	if (ELEM(target->type, OB_MESH, OB_LATTICE)) {
		defbase_tot = BLI_listbase_count(&target->defbase);

		if (target->type == OB_MESH) {
			Mesh *me = target->data;
			dverts = me->dvert;
			if (dverts)
				target_totvert = me->totvert;
		}
		else {
			Lattice *lt = target->data;
			dverts = lt->dvert;
			if (dverts)
				target_totvert = lt->pntsu * lt->pntsv * lt->pntsw;
		}
	}

	/* get a vertex-deform-index to posechannel array */
	if (deformflag & ARM_DEF_VGROUP) {
		if (ELEM(target->type, OB_MESH, OB_LATTICE)) {
			/* if we have a Mesh, only use dverts if it has them */
			if (mesh) {
				use_dverts = (mesh->dvert != NULL);
			}
			else if (dverts) {
				use_dverts = true;
			}

			if (use_dverts) {
				defnrToPC = MEM_callocN(sizeof(*defnrToPC) * defbase_tot, "defnrToBone");
				defnrToPCIndex = MEM_callocN(sizeof(*defnrToPCIndex) * defbase_tot, "defnrToIndex");
				/* TODO(sergey): Some considerations here:
				 *
				 * - Make it more generic function, maybe even keep together with chanhash.
				 * - Check whether keeping this consistent across frames gives speedup.
				 * - Don't use hash for small armatures.
				 */
				GHash *idx_hash = BLI_ghash_ptr_new("pose channel index by name");
				int pchan_index = 0;
				for (pchan = armOb->pose->chanbase.first; pchan != NULL; pchan = pchan->next, ++pchan_index) {
					BLI_ghash_insert(idx_hash, pchan, SET_INT_IN_POINTER(pchan_index));
				}
				for (i = 0, dg = target->defbase.first; dg; i++, dg = dg->next) {
					defnrToPC[i] = BKE_pose_channel_find_name(armOb->pose, dg->name);
					/* exclude non-deforming bones */
					if (defnrToPC[i]) {
						if (defnrToPC[i]->bone->flag & BONE_NO_DEFORM) {
							defnrToPC[i] = NULL;
						}
						else {
							defnrToPCIndex[i] = GET_INT_FROM_POINTER(BLI_ghash_lookup(idx_hash, defnrToPC[i]));
						}
					}
				}
				BLI_ghash_free(idx_hash, NULL, NULL);
			}
		}
	}

	for (i = 0; i < numVerts; i++) {
		MDeformVert *dvert;
		DualQuat sumdq, *dq = NULL;
		float *co, dco[3];
		float sumvec[3], summat[3][3];
		float *vec = NULL, (*smat)[3] = NULL;
		float contrib = 0.0f;
		float armature_weight = 1.0f; /* default to 1 if no overall def group */
		float prevco_weight = 1.0f;   /* weight for optional cached vertexcos */

		if (use_quaternion) {
			memset(&sumdq, 0, sizeof(DualQuat));
			dq = &sumdq;
		}
		else {
			sumvec[0] = sumvec[1] = sumvec[2] = 0.0f;
			vec = sumvec;

			if (defMats) {
				zero_m3(summat);
				smat = summat;
			}
		}

		if (use_dverts || armature_def_nr != -1) {
			if (mesh) {
				BLI_assert(i < mesh->totvert);
				dvert = mesh->dvert + i;
			}
			else if (dverts && i < target_totvert)
				dvert = dverts + i;
			else
				dvert = NULL;
		}
		else
			dvert = NULL;

		if (armature_def_nr != -1 && dvert) {
			armature_weight = defvert_find_weight(dvert, armature_def_nr);

			if (invert_vgroup)
				armature_weight = 1.0f - armature_weight;

			/* hackish: the blending factor can be used for blending with prevCos too */
			if (prevCos) {
				prevco_weight = armature_weight;
				armature_weight = 1.0f;
			}
		}

		/* check if there's any  point in calculating for this vert */
		if (armature_weight == 0.0f)
			continue;

		/* get the coord we work on */
		co = prevCos ? prevCos[i] : vertexCos[i];

		/* Apply the object's matrix */
		mul_m4_v3(premat, co);

		if (use_dverts && dvert && dvert->totweight) { /* use weight groups ? */
			MDeformWeight *dw = dvert->dw;
			int deformed = 0;
			unsigned int j;

			for (j = dvert->totweight; j != 0; j--, dw++) {
				const int index = dw->def_nr;
				if (index >= 0 && index < defbase_tot && (pchan = defnrToPC[index])) {
					float weight = dw->weight;
					Bone *bone = pchan->bone;
					pdef_info = pdef_info_array + defnrToPCIndex[index];

					deformed = 1;

					if (bone && bone->flag & BONE_MULT_VG_ENV) {
						weight *= distfactor_to_bone(co, bone->arm_head, bone->arm_tail,
						                             bone->rad_head, bone->rad_tail, bone->dist);
					}
					pchan_bone_deform(pchan, pdef_info, weight, vec, dq, smat, co, &contrib);
				}
			}
			/* if there are vertexgroups but not groups with bones
			 * (like for softbody groups) */
			if (deformed == 0 && use_envelope) {
				pdef_info = pdef_info_array;
				for (pchan = armOb->pose->chanbase.first; pchan; pchan = pchan->next, pdef_info++) {
					if (!(pchan->bone->flag & BONE_NO_DEFORM))
						contrib += dist_bone_deform(pchan, pdef_info, vec, dq, smat, co);
				}
			}
		}
		else if (use_envelope) {
			pdef_info = pdef_info_array;
			for (pchan = armOb->pose->chanbase.first; pchan; pchan = pchan->next, pdef_info++) {
				if (!(pchan->bone->flag & BONE_NO_DEFORM))
					contrib += dist_bone_deform(pchan, pdef_info, vec, dq, smat, co);
			}
		}

		/* actually should be EPSILON? weight values and contrib can be like 10e-39 small */
		if (contrib > 0.0001f) {
			if (use_quaternion) {
				normalize_dq(dq, contrib);

				if (armature_weight != 1.0f) {
					copy_v3_v3(dco, co);
					mul_v3m3_dq(dco, (defMats) ? summat : NULL, dq);
					sub_v3_v3(dco, co);
					mul_v3_fl(dco, armature_weight);
					add_v3_v3(co, dco);
				}
				else
					mul_v3m3_dq(co, (defMats) ? summat : NULL, dq);

				smat = summat;
			}
			else {
				mul_v3_fl(vec, armature_weight / contrib);
				add_v3_v3v3(co, vec, co);
			}

			if (defMats) {
				float pre[3][3], post[3][3], tmpmat[3][3];

				copy_m3_m4(pre, premat);
				copy_m3_m4(post, postmat);
				copy_m3_m3(tmpmat, defMats[i]);

				if (!use_quaternion) /* quaternion already is scale corrected */
					mul_m3_fl(smat, armature_weight / contrib);

				mul_m3_series(defMats[i], post, smat, pre, tmpmat);
			}
		}

		/* always, check above code */
		mul_m4_v3(postmat, co);

		/* interpolate with previous modifier position using weight group */
		if (prevCos) {
			float mw = 1.0f - prevco_weight;
			vertexCos[i][0] = prevco_weight * vertexCos[i][0] + mw * co[0];
			vertexCos[i][1] = prevco_weight * vertexCos[i][1] + mw * co[1];
			vertexCos[i][2] = prevco_weight * vertexCos[i][2] + mw * co[2];
		}
	}

	if (dualquats)
		MEM_freeN(dualquats);
	if (defnrToPC)
		MEM_freeN(defnrToPC);
	if (defnrToPCIndex)
		MEM_freeN(defnrToPCIndex);

	/* free B_bone matrices */
	pdef_info = pdef_info_array;
	for (pchan = armOb->pose->chanbase.first; pchan; pchan = pchan->next, pdef_info++) {
		if (pdef_info->b_bone_mats)
			MEM_freeN(pdef_info->b_bone_mats);
		if (pdef_info->b_bone_dual_quats)
			MEM_freeN(pdef_info->b_bone_dual_quats);
	}

	MEM_freeN(pdef_info_array);
}

/* ************ END Armature Deform ******************* */

void get_objectspace_bone_matrix(struct Bone *bone, float M_accumulatedMatrix[4][4], int UNUSED(root),
                                 int UNUSED(posed))
{
	copy_m4_m4(M_accumulatedMatrix, bone->arm_mat);
}

/* **************** Space to Space API ****************** */

/* Convert World-Space Matrix to Pose-Space Matrix */
void BKE_armature_mat_world_to_pose(Object *ob, float inmat[4][4], float outmat[4][4])
{
	float obmat[4][4];

	/* prevent crashes */
	if (ob == NULL)
		return;

	/* get inverse of (armature) object's matrix  */
	invert_m4_m4(obmat, ob->obmat);

	/* multiply given matrix by object's-inverse to find pose-space matrix */
	mul_m4_m4m4(outmat, inmat, obmat);
}

/* Convert World-Space Location to Pose-Space Location
 * NOTE: this cannot be used to convert to pose-space location of the supplied
 *       pose-channel into its local space (i.e. 'visual'-keyframing) */
void BKE_armature_loc_world_to_pose(Object *ob, const float inloc[3], float outloc[3])
{
	float xLocMat[4][4];
	float nLocMat[4][4];

	/* build matrix for location */
	unit_m4(xLocMat);
	copy_v3_v3(xLocMat[3], inloc);

	/* get bone-space cursor matrix and extract location */
	BKE_armature_mat_world_to_pose(ob, xLocMat, nLocMat);
	copy_v3_v3(outloc, nLocMat[3]);
}

/* Simple helper, computes the offset bone matrix.
 *     offs_bone = yoffs(b-1) + root(b) + bonemat(b).
 * Not exported, as it is only used in this file currently... */
static void get_offset_bone_mat(Bone *bone, float offs_bone[4][4])
{
	BLI_assert(bone->parent != NULL);

	/* Bone transform itself. */
	copy_m4_m3(offs_bone, bone->bone_mat);

	/* The bone's root offset (is in the parent's coordinate system). */
	copy_v3_v3(offs_bone[3], bone->head);

	/* Get the length translation of parent (length along y axis). */
	offs_bone[3][1] += bone->parent->length;
}

/* Construct the matrices (rot/scale and loc) to apply the PoseChannels into the armature (object) space.
 * I.e. (roughly) the "pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b)" in the
 *     pose_mat(b)= pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b)
 * ...function.
 *
 * This allows to get the transformations of a bone in its object space, *before* constraints (and IK)
 * get applied (used by pose evaluation code).
 * And reverse: to find pchan transformations needed to place a bone at a given loc/rot/scale
 * in object space (used by interactive transform, and snapping code).
 *
 * Note that, with the HINGE/NO_SCALE/NO_LOCAL_LOCATION options, the location matrix
 * will differ from the rotation/scale matrix...
 *
 * NOTE: This cannot be used to convert to pose-space transforms of the supplied
 *       pose-channel into its local space (i.e. 'visual'-keyframing).
 *       (note: I don't understand that, so I keep it :p --mont29).
 */
void BKE_pchan_to_pose_mat(bPoseChannel *pchan, float rotscale_mat[4][4], float loc_mat[4][4])
{
	Bone *bone, *parbone;
	bPoseChannel *parchan;

	/* set up variables for quicker access below */
	bone = pchan->bone;
	parbone = bone->parent;
	parchan = pchan->parent;

	if (parchan) {
		float offs_bone[4][4];
		/* yoffs(b-1) + root(b) + bonemat(b). */
		get_offset_bone_mat(bone, offs_bone);

		/* Compose the rotscale matrix for this bone. */
		if ((bone->flag & BONE_HINGE) && (bone->flag & BONE_NO_SCALE)) {
			/* Parent rest rotation and scale. */
			mul_m4_m4m4(rotscale_mat, parbone->arm_mat, offs_bone);
		}
		else if (bone->flag & BONE_HINGE) {
			/* Parent rest rotation and pose scale. */
			float tmat[4][4], tscale[3];

			/* Extract the scale of the parent pose matrix. */
			mat4_to_size(tscale, parchan->pose_mat);
			size_to_mat4(tmat, tscale);

			/* Applies the parent pose scale to the rest matrix. */
			mul_m4_m4m4(tmat, tmat, parbone->arm_mat);

			mul_m4_m4m4(rotscale_mat, tmat, offs_bone);
		}
		else if (bone->flag & BONE_NO_SCALE) {
			/* Parent pose rotation and rest scale (i.e. no scaling). */
			float tmat[4][4];
			copy_m4_m4(tmat, parchan->pose_mat);
			normalize_m4(tmat);
			mul_m4_m4m4(rotscale_mat, tmat, offs_bone);
		}
		else
			mul_m4_m4m4(rotscale_mat, parchan->pose_mat, offs_bone);

		/* Compose the loc matrix for this bone. */
		/* NOTE: That version does not modify bone's loc when HINGE/NO_SCALE options are set. */

		/* In this case, use the object's space *orientation*. */
		if (bone->flag & BONE_NO_LOCAL_LOCATION) {
			/* XXX I'm sure that code can be simplified! */
			float bone_loc[4][4], bone_rotscale[3][3], tmat4[4][4], tmat3[3][3];
			unit_m4(bone_loc);
			unit_m4(loc_mat);
			unit_m4(tmat4);

			mul_v3_m4v3(bone_loc[3], parchan->pose_mat, offs_bone[3]);

			unit_m3(bone_rotscale);
			copy_m3_m4(tmat3, parchan->pose_mat);
			mul_m3_m3m3(bone_rotscale, tmat3, bone_rotscale);

			copy_m4_m3(tmat4, bone_rotscale);
			mul_m4_m4m4(loc_mat, bone_loc, tmat4);
		}
		/* Those flags do not affect position, use plain parent transform space! */
		else if (bone->flag & (BONE_HINGE | BONE_NO_SCALE)) {
			mul_m4_m4m4(loc_mat, parchan->pose_mat, offs_bone);
		}
		/* Else (i.e. default, usual case), just use the same matrix for rotation/scaling, and location. */
		else
			copy_m4_m4(loc_mat, rotscale_mat);
	}
	/* Root bones. */
	else {
		/* Rotation/scaling. */
		copy_m4_m4(rotscale_mat, pchan->bone->arm_mat);
		/* Translation. */
		if (pchan->bone->flag & BONE_NO_LOCAL_LOCATION) {
			/* Translation of arm_mat, without the rotation. */
			unit_m4(loc_mat);
			copy_v3_v3(loc_mat[3], pchan->bone->arm_mat[3]);
		}
		else
			copy_m4_m4(loc_mat, rotscale_mat);
	}
}

/* Convert Pose-Space Matrix to Bone-Space Matrix.
 * NOTE: this cannot be used to convert to pose-space transforms of the supplied
 *       pose-channel into its local space (i.e. 'visual'-keyframing) */
void BKE_armature_mat_pose_to_bone(bPoseChannel *pchan, float inmat[4][4], float outmat[4][4])
{
	float rotscale_mat[4][4], loc_mat[4][4], inmat_[4][4];

	/* Security, this allows to call with inmat == outmat! */
	copy_m4_m4(inmat_, inmat);

	BKE_pchan_to_pose_mat(pchan, rotscale_mat, loc_mat);
	invert_m4(rotscale_mat);
	invert_m4(loc_mat);

	mul_m4_m4m4(outmat, rotscale_mat, inmat_);
	mul_v3_m4v3(outmat[3], loc_mat, inmat_[3]);
}

/* Convert Bone-Space Matrix to Pose-Space Matrix. */
void BKE_armature_mat_bone_to_pose(bPoseChannel *pchan, float inmat[4][4], float outmat[4][4])
{
	float rotscale_mat[4][4], loc_mat[4][4], inmat_[4][4];

	/* Security, this allows to call with inmat == outmat! */
	copy_m4_m4(inmat_, inmat);

	BKE_pchan_to_pose_mat(pchan, rotscale_mat, loc_mat);

	mul_m4_m4m4(outmat, rotscale_mat, inmat_);
	mul_v3_m4v3(outmat[3], loc_mat, inmat_[3]);
}

/* Convert Pose-Space Location to Bone-Space Location
 * NOTE: this cannot be used to convert to pose-space location of the supplied
 *       pose-channel into its local space (i.e. 'visual'-keyframing) */
void BKE_armature_loc_pose_to_bone(bPoseChannel *pchan, const float inloc[3], float outloc[3])
{
	float xLocMat[4][4];
	float nLocMat[4][4];

	/* build matrix for location */
	unit_m4(xLocMat);
	copy_v3_v3(xLocMat[3], inloc);

	/* get bone-space cursor matrix and extract location */
	BKE_armature_mat_pose_to_bone(pchan, xLocMat, nLocMat);
	copy_v3_v3(outloc, nLocMat[3]);
}

void BKE_armature_mat_pose_to_bone_ex(struct Depsgraph *depsgraph, Object *ob, bPoseChannel *pchan, float inmat[4][4], float outmat[4][4])
{
	bPoseChannel work_pchan = *pchan;

	/* recalculate pose matrix with only parent transformations,
	 * bone loc/sca/rot is ignored, scene and frame are not used. */
	BKE_pose_where_is_bone(depsgraph, NULL, ob, &work_pchan, 0.0f, false);

	/* find the matrix, need to remove the bone transforms first so this is
	 * calculated as a matrix to set rather then a difference ontop of whats
	 * already there. */
	unit_m4(outmat);
	BKE_pchan_apply_mat4(&work_pchan, outmat, false);

	BKE_armature_mat_pose_to_bone(&work_pchan, inmat, outmat);
}

/* same as BKE_object_mat3_to_rot() */
void BKE_pchan_mat3_to_rot(bPoseChannel *pchan, float mat[3][3], bool use_compat)
{
	BLI_ASSERT_UNIT_M3(mat);

	switch (pchan->rotmode) {
		case ROT_MODE_QUAT:
			mat3_normalized_to_quat(pchan->quat, mat);
			break;
		case ROT_MODE_AXISANGLE:
			mat3_normalized_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, mat);
			break;
		default: /* euler */
			if (use_compat)
				mat3_normalized_to_compatible_eulO(pchan->eul, pchan->eul, pchan->rotmode, mat);
			else
				mat3_normalized_to_eulO(pchan->eul, pchan->rotmode, mat);
			break;
	}
}

/* Apply a 4x4 matrix to the pose bone,
 * similar to BKE_object_apply_mat4() */
void BKE_pchan_apply_mat4(bPoseChannel *pchan, float mat[4][4], bool use_compat)
{
	float rot[3][3];
	mat4_to_loc_rot_size(pchan->loc, rot, pchan->size, mat);
	BKE_pchan_mat3_to_rot(pchan, rot, use_compat);
}

/* Remove rest-position effects from pose-transform for obtaining
 * 'visual' transformation of pose-channel.
 * (used by the Visual-Keyframing stuff) */
void BKE_armature_mat_pose_to_delta(float delta_mat[4][4], float pose_mat[4][4], float arm_mat[4][4])
{
	float imat[4][4];

	invert_m4_m4(imat, arm_mat);
	mul_m4_m4m4(delta_mat, imat, pose_mat);
}

/* **************** Rotation Mode Conversions ****************************** */
/* Used for Objects and Pose Channels, since both can have multiple rotation representations */

/* Called from RNA when rotation mode changes
 * - the result should be that the rotations given in the provided pointers have had conversions
 *   applied (as appropriate), such that the rotation of the element hasn't 'visually' changed  */
void BKE_rotMode_change_values(float quat[4], float eul[3], float axis[3], float *angle, short oldMode, short newMode)
{
	/* check if any change - if so, need to convert data */
	if (newMode > 0) { /* to euler */
		if (oldMode == ROT_MODE_AXISANGLE) {
			/* axis-angle to euler */
			axis_angle_to_eulO(eul, newMode, axis, *angle);
		}
		else if (oldMode == ROT_MODE_QUAT) {
			/* quat to euler */
			normalize_qt(quat);
			quat_to_eulO(eul, newMode, quat);
		}
		/* else { no conversion needed } */
	}
	else if (newMode == ROT_MODE_QUAT) { /* to quat */
		if (oldMode == ROT_MODE_AXISANGLE) {
			/* axis angle to quat */
			axis_angle_to_quat(quat, axis, *angle);
		}
		else if (oldMode > 0) {
			/* euler to quat */
			eulO_to_quat(quat, eul, oldMode);
		}
		/* else { no conversion needed } */
	}
	else if (newMode == ROT_MODE_AXISANGLE) { /* to axis-angle */
		if (oldMode > 0) {
			/* euler to axis angle */
			eulO_to_axis_angle(axis, angle, eul, oldMode);
		}
		else if (oldMode == ROT_MODE_QUAT) {
			/* quat to axis angle */
			normalize_qt(quat);
			quat_to_axis_angle(axis, angle, quat);
		}

		/* when converting to axis-angle, we need a special exception for the case when there is no axis */
		if (IS_EQF(axis[0], axis[1]) && IS_EQF(axis[1], axis[2])) {
			/* for now, rotate around y-axis then (so that it simply becomes the roll) */
			axis[1] = 1.0f;
		}
	}
}

/* **************** The new & simple (but OK!) armature evaluation ********* */

/* ****************** And how it works! ****************************************
 *
 * This is the bone transformation trick; they're hierarchical so each bone(b)
 * is in the coord system of bone(b-1):
 *
 * arm_mat(b)= arm_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b)
 *
 * -> yoffs is just the y axis translation in parent's coord system
 * -> d_root is the translation of the bone root, also in parent's coord system
 *
 * pose_mat(b)= pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b)
 *
 * we then - in init deform - store the deform in chan_mat, such that:
 *
 * pose_mat(b)= arm_mat(b) * chan_mat(b)
 *
 * *************************************************************************** */

/* Computes vector and roll based on a rotation.
 * "mat" must contain only a rotation, and no scaling. */
void mat3_to_vec_roll(float mat[3][3], float r_vec[3], float *r_roll)
{
	if (r_vec) {
		copy_v3_v3(r_vec, mat[1]);
	}

	if (r_roll) {
		float vecmat[3][3], vecmatinv[3][3], rollmat[3][3];

		vec_roll_to_mat3(mat[1], 0.0f, vecmat);
		invert_m3_m3(vecmatinv, vecmat);
		mul_m3_m3m3(rollmat, vecmatinv, mat);

		*r_roll = atan2f(rollmat[2][0], rollmat[2][2]);
	}
}

/* Calculates the rest matrix of a bone based on its vector and a roll around that vector. */
/* Given v = (v.x, v.y, v.z) our (normalized) bone vector, we want the rotation matrix M
 * from the Y axis (so that M * (0, 1, 0) = v).
 *   -> The rotation axis a lays on XZ plane, and it is orthonormal to v, hence to the projection of v onto XZ plane.
 *   -> a = (v.z, 0, -v.x)
 * We know a is eigenvector of M (so M * a = a).
 * Finally, we have w, such that M * w = (0, 1, 0) (i.e. the vector that will be aligned with Y axis once transformed).
 * We know w is symmetric to v by the Y axis.
 *   -> w = (-v.x, v.y, -v.z)
 *
 * Solving this, we get (x, y and z being the components of v):
 *     ┌ (x^2 * y + z^2) / (x^2 + z^2),   x,   x * z * (y - 1) / (x^2 + z^2) ┐
 * M = │  x * (y^2 - 1)  / (x^2 + z^2),   y,    z * (y^2 - 1)  / (x^2 + z^2) │
 *     └ x * z * (y - 1) / (x^2 + z^2),   z,   (x^2 + z^2 * y) / (x^2 + z^2) ┘
 *
 * This is stable as long as v (the bone) is not too much aligned with +/-Y (i.e. x and z components
 * are not too close to 0).
 *
 * Since v is normalized, we have x^2 + y^2 + z^2 = 1, hence x^2 + z^2 = 1 - y^2 = (1 - y)(1 + y).
 * This allows to simplifies M like this:
 *     ┌ 1 - x^2 / (1 + y),   x,     -x * z / (1 + y) ┐
 * M = │                -x,   y,                   -z │
 *     └  -x * z / (1 + y),   z,    1 - z^2 / (1 + y) ┘
 *
 * Written this way, we see the case v = +Y is no more a singularity. The only one remaining is the bone being
 * aligned with -Y.
 *
 * Let's handle the asymptotic behavior when bone vector is reaching the limit of y = -1. Each of the four corner
 * elements can vary from -1 to 1, depending on the axis a chosen for doing the rotation. And the "rotation" here
 * is in fact established by mirroring XZ plane by that given axis, then inversing the Y-axis.
 * For sufficiently small x and z, and with y approaching -1, all elements but the four corner ones of M
 * will degenerate. So let's now focus on these corner elements.
 *
 * We rewrite M so that it only contains its four corner elements, and combine the 1 / (1 + y) factor:
 *                    ┌ 1 + y - x^2,        -x * z ┐
 * M* = 1 / (1 + y) * │                            │
 *                    └      -x * z,   1 + y - z^2 ┘
 *
 * When y is close to -1, computing 1 / (1 + y) will cause severe numerical instability, so we ignore it and
 * normalize M instead. We know y^2 = 1 - (x^2 + z^2), and y < 0, hence y = -sqrt(1 - (x^2 + z^2)).
 * Since x and z are both close to 0, we apply the binomial expansion to the first order:
 * y = -sqrt(1 - (x^2 + z^2)) = -1 + (x^2 + z^2) / 2. Which gives:
 *                        ┌  z^2 - x^2,  -2 * x * z ┐
 * M* = 1 / (x^2 + z^2) * │                         │
 *                        └ -2 * x * z,   x^2 - z^2 ┘
 */
void vec_roll_to_mat3_normalized(const float nor[3], const float roll, float mat[3][3])
{
#define THETA_THRESHOLD_NEGY 1.0e-9f
#define THETA_THRESHOLD_NEGY_CLOSE 1.0e-5f

	float theta;
	float rMatrix[3][3], bMatrix[3][3];

	BLI_ASSERT_UNIT_V3(nor);

	theta = 1.0f + nor[1];

	/* With old algo, 1.0e-13f caused T23954 and T31333, 1.0e-6f caused T27675 and T30438,
	 * so using 1.0e-9f as best compromise.
	 *
	 * New algo is supposed much more precise, since less complex computations are performed,
	 * but it uses two different threshold values...
	 *
	 * Note: When theta is close to zero, we have to check we do have non-null X/Z components as well
	 *       (due to float precision errors, we can have nor = (0.0, 0.99999994, 0.0)...).
	 */
	if (theta > THETA_THRESHOLD_NEGY_CLOSE || ((nor[0] || nor[2]) && theta > THETA_THRESHOLD_NEGY)) {
		/* nor is *not* -Y.
		 * We got these values for free... so be happy with it... ;)
		 */
		bMatrix[0][1] = -nor[0];
		bMatrix[1][0] = nor[0];
		bMatrix[1][1] = nor[1];
		bMatrix[1][2] = nor[2];
		bMatrix[2][1] = -nor[2];
		if (theta > THETA_THRESHOLD_NEGY_CLOSE) {
			/* If nor is far enough from -Y, apply the general case. */
			bMatrix[0][0] = 1 - nor[0] * nor[0] / theta;
			bMatrix[2][2] = 1 - nor[2] * nor[2] / theta;
			bMatrix[2][0] = bMatrix[0][2] = -nor[0] * nor[2] / theta;
		}
		else {
			/* If nor is too close to -Y, apply the special case. */
			theta = nor[0] * nor[0] + nor[2] * nor[2];
			bMatrix[0][0] = (nor[0] + nor[2]) * (nor[0] - nor[2]) / -theta;
			bMatrix[2][2] = -bMatrix[0][0];
			bMatrix[2][0] = bMatrix[0][2] = 2.0f * nor[0] * nor[2] / theta;
		}
	}
	else {
		/* If nor is -Y, simple symmetry by Z axis. */
		unit_m3(bMatrix);
		bMatrix[0][0] = bMatrix[1][1] = -1.0;
	}

	/* Make Roll matrix */
	axis_angle_normalized_to_mat3(rMatrix, nor, roll);

	/* Combine and output result */
	mul_m3_m3m3(mat, rMatrix, bMatrix);

#undef THETA_THRESHOLD_NEGY
#undef THETA_THRESHOLD_NEGY_CLOSE
}

void vec_roll_to_mat3(const float vec[3], const float roll, float mat[3][3])
{
	float nor[3];

	normalize_v3_v3(nor, vec);
	vec_roll_to_mat3_normalized(nor, roll, mat);
}

/* recursive part, calculates restposition of entire tree of children */
/* used by exiting editmode too */
void BKE_armature_where_is_bone(Bone *bone, Bone *prevbone, const bool use_recursion)
{
	float vec[3];

	/* Bone Space */
	sub_v3_v3v3(vec, bone->tail, bone->head);
	bone->length = len_v3(vec);
	vec_roll_to_mat3(vec, bone->roll, bone->bone_mat);

	/* this is called on old file reading too... */
	if (bone->xwidth == 0.0f) {
		bone->xwidth = 0.1f;
		bone->zwidth = 0.1f;
		bone->segments = 1;
	}

	if (prevbone) {
		float offs_bone[4][4];
		/* yoffs(b-1) + root(b) + bonemat(b) */
		get_offset_bone_mat(bone, offs_bone);

		/* Compose the matrix for this bone  */
		mul_m4_m4m4(bone->arm_mat, prevbone->arm_mat, offs_bone);
	}
	else {
		copy_m4_m3(bone->arm_mat, bone->bone_mat);
		copy_v3_v3(bone->arm_mat[3], bone->head);
	}

	/* and the kiddies */
	if (use_recursion) {
		prevbone = bone;
		for (bone = bone->childbase.first; bone; bone = bone->next) {
			BKE_armature_where_is_bone(bone, prevbone, use_recursion);
		}
	}
}

/* updates vectors and matrices on rest-position level, only needed
 * after editing armature itself, now only on reading file */
void BKE_armature_where_is(bArmature *arm)
{
	Bone *bone;

	/* hierarchical from root to children */
	for (bone = arm->bonebase.first; bone; bone = bone->next) {
		BKE_armature_where_is_bone(bone, NULL, true);
	}
}

/* if bone layer is protected, copy the data from from->pose
 * when used with linked libraries this copies from the linked pose into the local pose */
static void pose_proxy_synchronize(Object *ob, Object *from, int layer_protected)
{
	bPose *pose = ob->pose, *frompose = from->pose;
	bPoseChannel *pchan, *pchanp;
	bConstraint *con;
	int error = 0;

	if (frompose == NULL)
		return;

	/* in some cases when rigs change, we cant synchronize
	 * to avoid crashing check for possible errors here */
	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		if (pchan->bone->layer & layer_protected) {
			if (BKE_pose_channel_find_name(frompose, pchan->name) == NULL) {
				printf("failed to sync proxy armature because '%s' is missing pose channel '%s'\n",
				       from->id.name, pchan->name);
				error = 1;
			}
		}
	}

	if (error)
		return;

	/* clear all transformation values from library */
	BKE_pose_rest(frompose);

	/* copy over all of the proxy's bone groups */
	/* TODO for later
	 * - implement 'local' bone groups as for constraints
	 * Note: this isn't trivial, as bones reference groups by index not by pointer,
	 *       so syncing things correctly needs careful attention */
	BLI_freelistN(&pose->agroups);
	BLI_duplicatelist(&pose->agroups, &frompose->agroups);
	pose->active_group = frompose->active_group;

	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		pchanp = BKE_pose_channel_find_name(frompose, pchan->name);

		if (UNLIKELY(pchanp == NULL)) {
			/* happens for proxies that become invalid because of a missing link
			 * for regular cases it shouldn't happen at all */
		}
		else if (pchan->bone->layer & layer_protected) {
			ListBase proxylocal_constraints = {NULL, NULL};
			bPoseChannel pchanw;

			/* copy posechannel to temp, but restore important pointers */
			pchanw = *pchanp;
			pchanw.bone = pchan->bone;
			pchanw.prev = pchan->prev;
			pchanw.next = pchan->next;
			pchanw.parent = pchan->parent;
			pchanw.child = pchan->child;
			pchanw.custom_tx = pchan->custom_tx;

			pchanw.mpath = pchan->mpath;
			pchan->mpath = NULL;

			/* this is freed so copy a copy, else undo crashes */
			if (pchanw.prop) {
				pchanw.prop = IDP_CopyProperty(pchanw.prop);

				/* use the values from the existing props */
				if (pchan->prop) {
					IDP_SyncGroupValues(pchanw.prop, pchan->prop);
				}
			}

			/* constraints - proxy constraints are flushed... local ones are added after
			 *     1. extract constraints not from proxy (CONSTRAINT_PROXY_LOCAL) from pchan's constraints
			 *     2. copy proxy-pchan's constraints on-to new
			 *     3. add extracted local constraints back on top
			 *
			 * Note for BKE_constraints_copy: when copying constraints, disable 'do_extern' otherwise
			 *                                we get the libs direct linked in this blend.
			 */
			BKE_constraints_proxylocal_extract(&proxylocal_constraints, &pchan->constraints);
			BKE_constraints_copy(&pchanw.constraints, &pchanp->constraints, false);
			BLI_movelisttolist(&pchanw.constraints, &proxylocal_constraints);

			/* constraints - set target ob pointer to own object */
			for (con = pchanw.constraints.first; con; con = con->next) {
				const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;

				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);

					for (ct = targets.first; ct; ct = ct->next) {
						if (ct->tar == from)
							ct->tar = ob;
					}

					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(con, &targets, 0);
				}
			}

			/* free stuff from current channel */
			BKE_pose_channel_free(pchan);

			/* copy data in temp back over to the cleaned-out (but still allocated) original channel */
			*pchan = pchanw;
			if (pchan->custom) {
				id_us_plus(&pchan->custom->id);
			}
		}
		else {
			/* always copy custom shape */
			pchan->custom = pchanp->custom;
			if (pchan->custom) {
				id_us_plus(&pchan->custom->id);
			}
			if (pchanp->custom_tx)
				pchan->custom_tx = BKE_pose_channel_find_name(pose, pchanp->custom_tx->name);

			/* ID-Property Syncing */
			{
				IDProperty *prop_orig = pchan->prop;
				if (pchanp->prop) {
					pchan->prop = IDP_CopyProperty(pchanp->prop);
					if (prop_orig) {
						/* copy existing values across when types match */
						IDP_SyncGroupValues(pchan->prop, prop_orig);
					}
				}
				else {
					pchan->prop = NULL;
				}
				if (prop_orig) {
					IDP_FreeProperty(prop_orig);
					MEM_freeN(prop_orig);
				}
			}
		}
	}
}

static int rebuild_pose_bone(bPose *pose, Bone *bone, bPoseChannel *parchan, int counter)
{
	bPoseChannel *pchan = BKE_pose_channel_verify(pose, bone->name); /* verify checks and/or adds */

	pchan->bone = bone;
	pchan->parent = parchan;

	counter++;

	for (bone = bone->childbase.first; bone; bone = bone->next) {
		counter = rebuild_pose_bone(pose, bone, pchan, counter);
		/* for quick detecting of next bone in chain, only b-bone uses it now */
		if (bone->flag & BONE_CONNECTED)
			pchan->child = BKE_pose_channel_find_name(pose, bone->name);
	}

	return counter;
}

/**
 * Clear pointers of object's pose (needed in remap case, since we cannot always wait for a complete pose rebuild).
 */
void BKE_pose_clear_pointers(bPose *pose)
{
	for (bPoseChannel *pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		pchan->bone = NULL;
		pchan->child = NULL;
	}
}

void BKE_pose_remap_bone_pointers(bArmature *armature, bPose *pose)
{
	GHash *bone_hash = BKE_armature_bone_from_name_map(armature);
	for (bPoseChannel *pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		pchan->bone = BLI_ghash_lookup(bone_hash, pchan->name);
	}
	BLI_ghash_free(bone_hash, NULL, NULL);
}

/**
 * Only after leave editmode, duplicating, validating older files, library syncing.
 *
 * \note pose->flag is set for it.
 *
 * \param bmain May be NULL, only used to tag depsgraph as being dirty...
 */
void BKE_pose_rebuild(Main *bmain, Object *ob, bArmature *arm, const bool do_id_user)
{
	Bone *bone;
	bPose *pose;
	bPoseChannel *pchan, *next;
	int counter = 0;

	/* only done here */
	if (ob->pose == NULL) {
		/* create new pose */
		ob->pose = MEM_callocN(sizeof(bPose), "new pose");

		/* set default settings for animviz */
		animviz_settings_init(&ob->pose->avs);
	}
	pose = ob->pose;

	/* clear */
	BKE_pose_clear_pointers(pose);

	/* first step, check if all channels are there */
	for (bone = arm->bonebase.first; bone; bone = bone->next) {
		counter = rebuild_pose_bone(pose, bone, NULL, counter);
	}

	/* and a check for garbage */
	for (pchan = pose->chanbase.first; pchan; pchan = next) {
		next = pchan->next;
		if (pchan->bone == NULL) {
			BKE_pose_channel_free_ex(pchan, do_id_user);
			BKE_pose_channels_hash_free(pose);
			BLI_freelinkN(&pose->chanbase, pchan);
		}
	}
	/* printf("rebuild pose %s, %d bones\n", ob->id.name, counter); */

	/* synchronize protected layers with proxy */
	/* HACK! To preserve 2.7x behavior that you always can pose even locked bones,
	 * do not do any restauration if this is a COW temp copy! */
	/* Switched back to just NO_MAIN tag, for some reasons (c) using COW tag was working this morning, but not anymore... */
	if (ob->proxy != NULL && (ob->id.tag & LIB_TAG_NO_MAIN) == 0) {
		BKE_object_copy_proxy_drivers(ob, ob->proxy);
		pose_proxy_synchronize(ob, ob->proxy, arm->layer_protected);
	}

	BKE_pose_update_constraint_flags(pose); /* for IK detection for example */

	pose->flag &= ~POSE_RECALC;
	pose->flag |= POSE_WAS_REBUILT;

	BKE_pose_channels_hash_make(pose);

	/* Rebuilding poses forces us to also rebuild the dependency graph, since there is one node per pose/bone... */
	if (bmain != NULL) {
		DEG_relations_tag_update(bmain);
	}
}

/* ********************** THE POSE SOLVER ******************* */

/* loc/rot/size to given mat4 */
void BKE_pchan_to_mat4(bPoseChannel *pchan, float chan_mat[4][4])
{
	float smat[3][3];
	float rmat[3][3];
	float tmat[3][3];

	/* get scaling matrix */
	size_to_mat3(smat, pchan->size);

	/* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
	if (pchan->rotmode > 0) {
		/* euler rotations (will cause gimble lock, but this can be alleviated a bit with rotation orders) */
		eulO_to_mat3(rmat, pchan->eul, pchan->rotmode);
	}
	else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
		/* axis-angle - not really that great for 3D-changing orientations */
		axis_angle_to_mat3(rmat, pchan->rotAxis, pchan->rotAngle);
	}
	else {
		/* quats are normalized before use to eliminate scaling issues */
		float quat[4];

		/* NOTE: we now don't normalize the stored values anymore, since this was kindof evil in some cases
		 * but if this proves to be too problematic, switch back to the old system of operating directly on
		 * the stored copy
		 */
		normalize_qt_qt(quat, pchan->quat);
		quat_to_mat3(rmat, quat);
	}

	/* calculate matrix of bone (as 3x3 matrix, but then copy the 4x4) */
	mul_m3_m3m3(tmat, rmat, smat);
	copy_m4_m3(chan_mat, tmat);

	/* prevent action channels breaking chains */
	/* need to check for bone here, CONSTRAINT_TYPE_ACTION uses this call */
	if ((pchan->bone == NULL) || !(pchan->bone->flag & BONE_CONNECTED)) {
		copy_v3_v3(chan_mat[3], pchan->loc);
	}
}

/* loc/rot/size to mat4 */
/* used in constraint.c too */
void BKE_pchan_calc_mat(bPoseChannel *pchan)
{
	/* this is just a wrapper around the copy of this function which calculates the matrix
	 * and stores the result in any given channel
	 */
	BKE_pchan_to_mat4(pchan, pchan->chan_mat);
}

#if 0 /* XXX OLD ANIMSYS, NLASTRIPS ARE NO LONGER USED */

/* NLA strip modifiers */
static void do_strip_modifiers(Scene *scene, Object *armob, Bone *bone, bPoseChannel *pchan)
{
	bActionModifier *amod;
	bActionStrip *strip, *strip2;
	float scene_cfra = BKE_scene_frame_get(scene);
	int do_modif;

	for (strip = armob->nlastrips.first; strip; strip = strip->next) {
		do_modif = false;

		if (scene_cfra >= strip->start && scene_cfra <= strip->end)
			do_modif = true;

		if ((scene_cfra > strip->end) && (strip->flag & ACTSTRIP_HOLDLASTFRAME)) {
			do_modif = true;

			/* if there are any other strips active, ignore modifiers for this strip -
			 * 'hold' option should only hold action modifiers if there are
			 * no other active strips */
			for (strip2 = strip->next; strip2; strip2 = strip2->next) {
				if (strip2 == strip) continue;

				if (scene_cfra >= strip2->start && scene_cfra <= strip2->end) {
					if (!(strip2->flag & ACTSTRIP_MUTE))
						do_modif = false;
				}
			}

			/* if there are any later, activated, strips with 'hold' set, they take precedence,
			 * so ignore modifiers for this strip */
			for (strip2 = strip->next; strip2; strip2 = strip2->next) {
				if (scene_cfra < strip2->start) continue;
				if ((strip2->flag & ACTSTRIP_HOLDLASTFRAME) && !(strip2->flag & ACTSTRIP_MUTE)) {
					do_modif = false;
				}
			}
		}

		if (do_modif) {
			/* temporal solution to prevent 2 strips accumulating */
			if (scene_cfra == strip->end && strip->next && strip->next->start == scene_cfra)
				continue;

			for (amod = strip->modifiers.first; amod; amod = amod->next) {
				switch (amod->type) {
					case ACTSTRIP_MOD_DEFORM:
					{
						/* validate first */
						if (amod->ob && amod->ob->type == OB_CURVE && amod->channel[0]) {

							if (STREQ(pchan->name, amod->channel)) {
								float mat4[4][4], mat3[3][3];

								curve_deform_vector(amod->ob, armob, bone->arm_mat[3], pchan->pose_mat[3], mat3, amod->no_rot_axis);
								copy_m4_m4(mat4, pchan->pose_mat);
								mul_m4_m3m4(pchan->pose_mat, mat3, mat4);

							}
						}
					}
					break;
					case ACTSTRIP_MOD_NOISE:
					{
						if (STREQ(pchan->name, amod->channel)) {
							float nor[3], loc[3], ofs;
							float eul[3], size[3], eulo[3], sizeo[3];

							/* calculate turbulance */
							ofs = amod->turbul / 200.0f;

							/* make a copy of starting conditions */
							copy_v3_v3(loc, pchan->pose_mat[3]);
							mat4_to_eul(eul, pchan->pose_mat);
							mat4_to_size(size, pchan->pose_mat);
							copy_v3_v3(eulo, eul);
							copy_v3_v3(sizeo, size);

							/* apply noise to each set of channels */
							if (amod->channels & 4) {
								/* for scaling */
								nor[0] = BLI_gNoise(amod->noisesize, size[0] + ofs, size[1], size[2], 0, 0) - ofs;
								nor[1] = BLI_gNoise(amod->noisesize, size[0], size[1] + ofs, size[2], 0, 0) - ofs;
								nor[2] = BLI_gNoise(amod->noisesize, size[0], size[1], size[2] + ofs, 0, 0) - ofs;
								add_v3_v3(size, nor);

								if (sizeo[0] != 0)
									mul_v3_fl(pchan->pose_mat[0], size[0] / sizeo[0]);
								if (sizeo[1] != 0)
									mul_v3_fl(pchan->pose_mat[1], size[1] / sizeo[1]);
								if (sizeo[2] != 0)
									mul_v3_fl(pchan->pose_mat[2], size[2] / sizeo[2]);
							}
							if (amod->channels & 2) {
								/* for rotation */
								nor[0] = BLI_gNoise(amod->noisesize, eul[0] + ofs, eul[1], eul[2], 0, 0) - ofs;
								nor[1] = BLI_gNoise(amod->noisesize, eul[0], eul[1] + ofs, eul[2], 0, 0) - ofs;
								nor[2] = BLI_gNoise(amod->noisesize, eul[0], eul[1], eul[2] + ofs, 0, 0) - ofs;

								compatible_eul(nor, eulo);
								add_v3_v3(eul, nor);
								compatible_eul(eul, eulo);

								loc_eul_size_to_mat4(pchan->pose_mat, loc, eul, size);
							}
							if (amod->channels & 1) {
								/* for location */
								nor[0] = BLI_gNoise(amod->noisesize, loc[0] + ofs, loc[1], loc[2], 0, 0) - ofs;
								nor[1] = BLI_gNoise(amod->noisesize, loc[0], loc[1] + ofs, loc[2], 0, 0) - ofs;
								nor[2] = BLI_gNoise(amod->noisesize, loc[0], loc[1], loc[2] + ofs, 0, 0) - ofs;

								add_v3_v3v3(pchan->pose_mat[3], loc, nor);
							}
						}
					}
					break;
				}
			}
		}
	}
}

#endif

/* calculate tail of posechannel */
void BKE_pose_where_is_bone_tail(bPoseChannel *pchan)
{
	float vec[3];

	copy_v3_v3(vec, pchan->pose_mat[1]);
	mul_v3_fl(vec, pchan->bone->length);
	add_v3_v3v3(pchan->pose_tail, pchan->pose_head, vec);
}

/* The main armature solver, does all constraints excluding IK */
/* pchan is validated, as having bone and parent pointer
 * 'do_extra': when zero skips loc/size/rot, constraints and strip modifiers.
 */
void BKE_pose_where_is_bone(
        struct Depsgraph *depsgraph, Scene *scene,
        Object *ob, bPoseChannel *pchan, float ctime, bool do_extra)
{
	/* This gives a chan_mat with actions (ipos) results. */
	if (do_extra)
		BKE_pchan_calc_mat(pchan);
	else
		unit_m4(pchan->chan_mat);

	/* Construct the posemat based on PoseChannels, that we do before applying constraints. */
	/* pose_mat(b) = pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b) */
	BKE_armature_mat_bone_to_pose(pchan, pchan->chan_mat, pchan->pose_mat);

	/* Only rootbones get the cyclic offset (unless user doesn't want that). */
	/* XXX That could be a problem for snapping and other "reverse transform" features... */
	if (!pchan->parent) {
		if ((pchan->bone->flag & BONE_NO_CYCLICOFFSET) == 0)
			add_v3_v3(pchan->pose_mat[3], ob->pose->cyclic_offset);
	}

	if (do_extra) {
#if 0   /* XXX OLD ANIMSYS, NLASTRIPS ARE NO LONGER USED */
		/* do NLA strip modifiers - i.e. curve follow */
		do_strip_modifiers(scene, ob, bone, pchan);
#endif

		/* Do constraints */
		if (pchan->constraints.first) {
			bConstraintOb *cob;
			float vec[3];

			/* make a copy of location of PoseChannel for later */
			copy_v3_v3(vec, pchan->pose_mat[3]);

			/* prepare PoseChannel for Constraint solving
			 * - makes a copy of matrix, and creates temporary struct to use
			 */
			cob = BKE_constraints_make_evalob(depsgraph, scene, ob, pchan, CONSTRAINT_OBTYPE_BONE);

			/* Solve PoseChannel's Constraints */
			BKE_constraints_solve(depsgraph, &pchan->constraints, cob, ctime); /* ctime doesnt alter objects */

			/* cleanup after Constraint Solving
			 * - applies matrix back to pchan, and frees temporary struct used
			 */
			BKE_constraints_clear_evalob(cob);

			/* prevent constraints breaking a chain */
			if (pchan->bone->flag & BONE_CONNECTED) {
				copy_v3_v3(pchan->pose_mat[3], vec);
			}
		}
	}

	/* calculate head */
	copy_v3_v3(pchan->pose_head, pchan->pose_mat[3]);
	/* calculate tail */
	BKE_pose_where_is_bone_tail(pchan);
}

/* This only reads anim data from channels, and writes to channels */
/* This is the only function adding poses */
void BKE_pose_where_is(struct Depsgraph *depsgraph, Scene *scene, Object *ob)
{
	bArmature *arm;
	Bone *bone;
	bPoseChannel *pchan;
	float imat[4][4];
	float ctime;

	if (ob->type != OB_ARMATURE)
		return;
	arm = ob->data;

	if (ELEM(NULL, arm, scene))
		return;
	if ((ob->pose == NULL) || (ob->pose->flag & POSE_RECALC)) {
		/* WARNING! passing NULL bmain here means we won't tag depsgraph's as dirty - hopefully this is OK. */
		BKE_pose_rebuild(NULL, ob, arm, true);
	}

	ctime = BKE_scene_frame_get(scene); /* not accurate... */

	/* In editmode or restposition we read the data from the bones */
	if (arm->edbo || (arm->flag & ARM_RESTPOS)) {
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			bone = pchan->bone;
			if (bone) {
				copy_m4_m4(pchan->pose_mat, bone->arm_mat);
				copy_v3_v3(pchan->pose_head, bone->arm_head);
				copy_v3_v3(pchan->pose_tail, bone->arm_tail);
			}
		}
	}
	else {
		invert_m4_m4(ob->imat, ob->obmat); /* imat is needed */

		/* 1. clear flags */
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			pchan->flag &= ~(POSE_DONE | POSE_CHAIN | POSE_IKTREE | POSE_IKSPLINE);
		}

		/* 2a. construct the IK tree (standard IK) */
		BIK_initialize_tree(depsgraph, scene, ob, ctime);

		/* 2b. construct the Spline IK trees
		 *  - this is not integrated as an IK plugin, since it should be able
		 *	  to function in conjunction with standard IK
		 */
		BKE_pose_splineik_init_tree(scene, ob, ctime);

		/* 3. the main loop, channels are already hierarchical sorted from root to children */
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			/* 4a. if we find an IK root, we handle it separated */
			if (pchan->flag & POSE_IKTREE) {
				BIK_execute_tree(depsgraph, scene, ob, pchan, ctime);
			}
			/* 4b. if we find a Spline IK root, we handle it separated too */
			else if (pchan->flag & POSE_IKSPLINE) {
				BKE_splineik_execute_tree(depsgraph, scene, ob, pchan, ctime);
			}
			/* 5. otherwise just call the normal solver */
			else if (!(pchan->flag & POSE_DONE)) {
				BKE_pose_where_is_bone(depsgraph, scene, ob, pchan, ctime, 1);
			}
		}
		/* 6. release the IK tree */
		BIK_release_tree(scene, ob, ctime);
	}

	/* calculating deform matrices */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		if (pchan->bone) {
			invert_m4_m4(imat, pchan->bone->arm_mat);
			mul_m4_m4m4(pchan->chan_mat, pchan->pose_mat, imat);
		}
	}
}

/************** Bounding box ********************/
static int minmax_armature(Object *ob, float r_min[3], float r_max[3])
{
	bPoseChannel *pchan;

	/* For now, we assume BKE_pose_where_is has already been called (hence we have valid data in pachan). */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		minmax_v3v3_v3(r_min, r_max, pchan->pose_head);
		minmax_v3v3_v3(r_min, r_max, pchan->pose_tail);
	}

	return (BLI_listbase_is_empty(&ob->pose->chanbase) == false);
}

static void boundbox_armature(Object *ob)
{
	BoundBox *bb;
	float min[3], max[3];

	if (ob->bb == NULL) {
		ob->bb = MEM_callocN(sizeof(BoundBox), "Armature boundbox");
	}
	bb = ob->bb;

	INIT_MINMAX(min, max);
	if (!minmax_armature(ob, min, max)) {
		min[0] = min[1] = min[2] = -1.0f;
		max[0] = max[1] = max[2] = 1.0f;
	}

	BKE_boundbox_init_from_minmax(bb, min, max);

	bb->flag &= ~BOUNDBOX_DIRTY;
}

BoundBox *BKE_armature_boundbox_get(Object *ob)
{
	boundbox_armature(ob);

	return ob->bb;
}

bool BKE_pose_minmax(Object *ob, float r_min[3], float r_max[3], bool use_hidden, bool use_select)
{
	bool changed = false;

	if (ob->pose) {
		bArmature *arm = ob->data;
		bPoseChannel *pchan;

		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			/* XXX pchan->bone may be NULL for duplicated bones, see duplicateEditBoneObjects() comment
			 *     (editarmature.c:2592)... Skip in this case too! */
			if (pchan->bone &&
			    (!((use_hidden == false) && (PBONE_VISIBLE(arm, pchan->bone) == false)) &&
			     !((use_select == true)  && ((pchan->bone->flag & BONE_SELECTED) == 0))))
			{
				bPoseChannel *pchan_tx = (pchan->custom && pchan->custom_tx) ? pchan->custom_tx : pchan;
				BoundBox *bb_custom = ((pchan->custom) && !(arm->flag & ARM_NO_CUSTOM)) ?
				                      BKE_object_boundbox_get(pchan->custom) : NULL;
				if (bb_custom) {
					float mat[4][4], smat[4][4];
					scale_m4_fl(smat, PCHAN_CUSTOM_DRAW_SIZE(pchan));
					mul_m4_series(mat, ob->obmat, pchan_tx->pose_mat, smat);
					BKE_boundbox_minmax(bb_custom, mat, r_min, r_max);
				}
				else {
					float vec[3];
					mul_v3_m4v3(vec, ob->obmat, pchan_tx->pose_head);
					minmax_v3v3_v3(r_min, r_max, vec);
					mul_v3_m4v3(vec, ob->obmat, pchan_tx->pose_tail);
					minmax_v3v3_v3(r_min, r_max, vec);
				}

				changed = true;
			}
		}
	}

	return changed;
}

/************** Graph evaluation ********************/

bPoseChannel *BKE_armature_ik_solver_find_root(
        bPoseChannel *pchan,
        bKinematicConstraint *data)
{
	bPoseChannel *rootchan = pchan;
	if (!(data->flag & CONSTRAINT_IK_TIP)) {
		/* Exclude tip from chain. */
		rootchan = rootchan->parent;
	}
	if (rootchan != NULL) {
		int segcount = 0;
		while (rootchan->parent) {
			/* Continue up chain, until we reach target number of items. */
			segcount++;
			if (segcount == data->rootbone) {
				break;
			}
			rootchan = rootchan->parent;
		}
	}
	return rootchan;
}

bPoseChannel *BKE_armature_splineik_solver_find_root(
        bPoseChannel *pchan,
        bSplineIKConstraint *data)
{
	bPoseChannel *rootchan = pchan;
	int segcount = 0;
	BLI_assert(rootchan != NULL);
	while (rootchan->parent) {
		/* Continue up chain, until we reach target number of items. */
		segcount++;
		if (segcount == data->chainlen) {
			break;
		}
		rootchan = rootchan->parent;
	}
	return rootchan;
}
