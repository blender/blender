/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file draw_armature.c
 *  \ingroup draw
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_dlrbTree.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_nla.h"
#include "BKE_curve.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_armature.h"
#include "ED_keyframes_draw.h"

#include "UI_resources.h"

#include "draw_mode_pass.h"

#define BONE_VAR(eBone, pchan, var) ((eBone) ? (eBone->var) : (pchan->var))
#define BONE_FLAG(eBone, pchan) ((eBone) ? (eBone->flag) : (pchan->bone->flag))

/* *************** Armature Drawing - Coloring API ***************************** */

static float colorBoneSolid[4];
static float colorTextHi[4];
static float colorText[4];
static float colorVertexSelect[4];
static float colorVertex[4];

static const float *constColor;

static void update_color(const float const_color[4])
{
	constColor = const_color;

	UI_GetThemeColor4fv(TH_BONE_SOLID, colorBoneSolid);
	UI_GetThemeColor4fv(TH_TEXT_HI, colorTextHi);
	UI_GetThemeColor4fv(TH_TEXT, colorText);
	UI_GetThemeColor4fv(TH_VERTEX_SELECT, colorVertexSelect);
	UI_GetThemeColor4fv(TH_VERTEX, colorVertex);
}

static const float *get_bone_solid_color(EditBone *eBone, bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm))
{
	if (constColor)
		return colorBoneSolid;

	/* Edit Mode */
	if (eBone) {
		if (eBone->flag & BONE_SELECTED)
			return colorVertexSelect;
	}

	return colorBoneSolid;
}

static const float *get_bone_wire_color(EditBone *eBone, bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm))
{
	if (constColor)
		return constColor;

	if (eBone) {
		if (eBone->flag & BONE_SELECTED)
			return colorVertexSelect;
	}

	return colorVertex;
}

/* *************** Armature drawing, helper calls for parts ******************* */

static void draw_bone_update_disp_matrix(EditBone *eBone, bPoseChannel *pchan, int drawtype)
{
	float s[4][4], ebmat[4][4];
	float length;
	float (*bone_mat)[4];
	float (*disp_mat)[4];
	float (*disp_tail_mat)[4];

	/* TODO : This should be moved to depsgraph or armature refresh
	 * and not be tight to the draw pass creation.
	 * This would refresh armature without invalidating the draw cache */
	if (pchan) {
		length = pchan->bone->length;
		bone_mat = pchan->pose_mat;
		disp_mat = pchan->disp_mat;
		disp_tail_mat = pchan->disp_tail_mat;
	}
	else {
		eBone->length = len_v3v3(eBone->tail, eBone->head);
		ED_armature_ebone_to_mat4(eBone, ebmat);

		length = eBone->length;
		bone_mat = ebmat;
		disp_mat = eBone->disp_mat;
		disp_tail_mat = eBone->disp_tail_mat;
	}

	if (pchan && pchan->custom) {
		/* TODO */
	}
	else if (drawtype == ARM_ENVELOPE) {
		/* TODO */
	}
	else if (drawtype == ARM_LINE) {
		/* TODO */
	}
	else if (drawtype == ARM_WIRE) {
		/* TODO */
	}
	else if (drawtype == ARM_B_BONE) {
		/* TODO */
	}
	else {
		scale_m4_fl(s, length);
		mul_m4_m4m4(disp_mat, bone_mat, s);
		copy_m4_m4(disp_tail_mat, disp_mat);
		translate_m4(disp_tail_mat, 0.0f, 1.0f, 0.0f);
	}
}

static void draw_axes(EditBone *eBone, bPoseChannel *pchan)
{
	const float *col = (constColor) ? constColor :
	                   (BONE_FLAG(eBone, pchan) & BONE_SELECTED) ? colorTextHi : colorText;

	DRW_shgroup_bone_axes(BONE_VAR(eBone, pchan, disp_tail_mat), col);
}

static void draw_points(EditBone *eBone, bPoseChannel *pchan, bArmature *arm)
{
	const float *col_solid_root = colorBoneSolid;
	const float *col_solid_tail = colorBoneSolid;
	const float *col_wire_root = (constColor) ? constColor : colorVertex;
	const float *col_wire_tail = (constColor) ? constColor : colorVertex;

	/* Edit bone points can be selected */
	if (eBone) {
		if (eBone->flag & BONE_ROOTSEL) {
			col_solid_root = colorVertexSelect;
			col_wire_root = colorVertexSelect;
		}
		if (eBone->flag & BONE_TIPSEL) {
			col_solid_tail = colorVertexSelect;
			col_wire_tail = colorVertexSelect;
		}
	}

	/*	Draw root point if we are not connected and parent are not hidden */
	if ((BONE_FLAG(eBone, pchan) & BONE_CONNECTED) == 0) {
		if (eBone) {
			if (!((eBone->parent) && !EBONE_VISIBLE(arm, eBone->parent))) {
				DRW_shgroup_bone_point_solid(eBone->disp_mat, col_solid_root);
				DRW_shgroup_bone_point_wire(eBone->disp_mat, col_wire_root);
			}
		}
		else {
			Bone *bone = pchan->bone;
			if (!((bone->parent) && (bone->parent->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)))) {
				DRW_shgroup_bone_point_solid(pchan->disp_mat, col_solid_root);
				DRW_shgroup_bone_point_wire(pchan->disp_mat, col_wire_root);
			}
		}
	}

	/*	Draw tip point */
	DRW_shgroup_bone_point_solid(BONE_VAR(eBone, pchan, disp_tail_mat), col_solid_tail);
	DRW_shgroup_bone_point_wire(BONE_VAR(eBone, pchan, disp_tail_mat), col_wire_tail);
}

static void draw_bone_custom_shape(EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm))
{
	/* work in progress  -- fclem */
}

static void draw_bone_envelope(EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm))
{
	/* work in progress  -- fclem */
}

static void draw_bone_line(EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm))
{
	/* work in progress  -- fclem */
}

static void draw_bone_wire(EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm))
{
	/* work in progress  -- fclem */
}

static void draw_bone_box(EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm))
{
	/* work in progress  -- fclem */
}

static void draw_bone_octahedral(EditBone *eBone, bPoseChannel *pchan, bArmature *arm)
{
	const float *col_solid = get_bone_solid_color(eBone, pchan, arm);
	const float *col_wire = get_bone_wire_color(eBone, pchan, arm);

	DRW_shgroup_bone_octahedral_solid(BONE_VAR(eBone, pchan, disp_mat), col_solid);
	DRW_shgroup_bone_octahedral_wire(BONE_VAR(eBone, pchan, disp_mat), col_wire);

	draw_points(eBone, pchan, arm);
}

void draw_armature_edit(Object *ob)
{
	EditBone *eBone;
	bArmature *arm = ob->data;
	unsigned int index;

	update_color(NULL);

	for (eBone = arm->edbo->first, index = 0; eBone; eBone = eBone->next, index++) {
		if (eBone->layer & arm->layer) {
			if ((eBone->flag & BONE_HIDDEN_A) == 0) {

				draw_bone_update_disp_matrix(eBone, NULL, arm->drawtype);

				if (arm->drawtype == ARM_ENVELOPE)
					draw_bone_envelope(eBone, NULL, arm);
				else if (arm->drawtype == ARM_LINE)
					draw_bone_line(eBone, NULL, arm);
				else if (arm->drawtype == ARM_WIRE)
					draw_bone_wire(eBone, NULL, arm);
				else if (arm->drawtype == ARM_B_BONE)
					draw_bone_box(eBone, NULL, arm);
				else
					draw_bone_octahedral(eBone, NULL, arm);

				/*	Draw additional axes */
				if (arm->flag & ARM_DRAWAXES)
					draw_axes(eBone, NULL);
			}
		}
	}
}

/* if const_color is NULL do pose mode coloring */
void draw_armature_pose(Object *ob, const float const_color[4])
{
	bArmature *arm = ob->data;
	bPoseChannel *pchan;
	Bone *bone;

	update_color(const_color);

	/* We can't safely draw non-updated pose, might contain NULL bone pointers... */
	if (ob->pose->flag & POSE_RECALC) {
		BKE_pose_rebuild(ob, arm);
	}

	/* being set below */
	arm->layer_used = 0;

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		bone = pchan->bone;
		arm->layer_used |= bone->layer;

		/* bone must be visible */
		if ((bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0) {
			if (bone->layer & arm->layer) {

				draw_bone_update_disp_matrix(NULL, pchan, arm->drawtype);

				if ((pchan->custom) && !(arm->flag & ARM_NO_CUSTOM))
					draw_bone_custom_shape(NULL, pchan, arm);
				else if (arm->drawtype == ARM_ENVELOPE)
					draw_bone_envelope(NULL, pchan, arm);
				else if (arm->drawtype == ARM_LINE)
					draw_bone_line(NULL, pchan, arm);
				else if (arm->drawtype == ARM_WIRE)
					draw_bone_wire(NULL, pchan, arm);
				else if (arm->drawtype == ARM_B_BONE)
					draw_bone_box(NULL, pchan, arm);
				else
					draw_bone_octahedral(NULL, pchan, arm);

				/*	Draw additional axes */
				if (arm->flag & ARM_DRAWAXES)
					draw_axes(NULL, pchan);
			}
		}
	}
}