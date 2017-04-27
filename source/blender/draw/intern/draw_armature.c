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

#include "DRW_render.h"

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

#include "ED_armature.h"
#include "ED_keyframes_draw.h"

#include "GPU_select.h"

#include "UI_resources.h"

#include "draw_common.h"

#define BONE_VAR(eBone, pchan, var) ((eBone) ? (eBone->var) : (pchan->var))
#define BONE_FLAG(eBone, pchan) ((eBone) ? (eBone->flag) : (pchan->bone->flag))

/* Reset for drawing each armature object */
static struct {
	/* Current armature object */
	Object *ob;
	/* Reset when changing current_armature */
	DRWShadingGroup *bone_octahedral_solid;
	DRWShadingGroup *bone_octahedral_wire;
	DRWShadingGroup *bone_point_solid;
	DRWShadingGroup *bone_point_wire;
	DRWShadingGroup *bone_axes;
	DRWShadingGroup *relationship_lines;

	DRWPass *bone_solid;
	DRWPass *bone_wire;
} g_data = {NULL};

/* -------------------------------------------------------------------- */

/** \name Shader Groups (DRW_shgroup)
 * \{ */

/* Octahedral */
static void DRW_shgroup_bone_octahedral_solid(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_octahedral_solid == NULL) {
		struct Batch *geom = DRW_cache_bone_octahedral_get();
		g_data.bone_octahedral_solid = shgroup_instance_objspace_solid(g_data.bone_solid, geom, g_data.ob->obmat);
	}

	DRW_shgroup_call_dynamic_add(g_data.bone_octahedral_solid, bone_mat, color);
}

static void DRW_shgroup_bone_octahedral_wire(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_octahedral_wire == NULL) {
		struct Batch *geom = DRW_cache_bone_octahedral_wire_outline_get();
		g_data.bone_octahedral_wire = shgroup_instance_objspace_wire(g_data.bone_wire, geom, g_data.ob->obmat);
	}

	DRW_shgroup_call_dynamic_add(g_data.bone_octahedral_wire, bone_mat, color);
}

/* Head and tail sphere */
static void DRW_shgroup_bone_point_solid(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_point_solid == NULL) {
		struct Batch *geom = DRW_cache_bone_point_get();
		g_data.bone_point_solid = shgroup_instance_objspace_solid(g_data.bone_solid, geom, g_data.ob->obmat);
	}

	DRW_shgroup_call_dynamic_add(g_data.bone_point_solid, bone_mat, color);
}

static void DRW_shgroup_bone_point_wire(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_point_wire == NULL) {
		struct Batch *geom = DRW_cache_bone_point_wire_outline_get();
		g_data.bone_point_wire = shgroup_instance_objspace_wire(g_data.bone_wire, geom, g_data.ob->obmat);
	}

	DRW_shgroup_call_dynamic_add(g_data.bone_point_wire, bone_mat, color);
}

/* Axes */
static void DRW_shgroup_bone_axes(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_axes == NULL) {
		struct Batch *geom = DRW_cache_bone_arrows_get();
		g_data.bone_axes = shgroup_instance_objspace_wire(g_data.bone_wire, geom, g_data.ob->obmat);
	}

	DRW_shgroup_call_dynamic_add(g_data.bone_axes, bone_mat, color);
}

/* Relationship lines */
static void UNUSED_FUNCTION(DRW_shgroup_bone_relationship_lines)(const float head[3], const float tail[3])
{
	DRW_shgroup_call_dynamic_add(g_data.relationship_lines, head);
	DRW_shgroup_call_dynamic_add(g_data.relationship_lines, tail);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Drawing Color Helpers
 * \{ */

/**
 * Follow `TH_*` naming except for mixed colors.
 */
static struct {
	float select_color[4];
	float edge_select_color[4];
	float bone_select_color[4];  /* tint */
	float wire_color[4];
	float wire_edit_color[4];
	float bone_solid_color[4];
	float bone_active_unselect_color[4];  /* mix */
	float bone_pose_color[4];
	float bone_pose_active_color[4];
	float bone_pose_active_unselect_color[4];  /* mix */
	float text_hi_color[4];
	float text_color[4];
	float vertex_select_color[4];
	float vertex_color[4];

	/* not a theme, this is an override */
	const float *const_color;
} g_theme;

/** See: 'set_pchan_color'*/
static void update_color(const float const_color[4])
{
	g_theme.const_color = const_color;

#define NO_ALPHA(c) (((c)[3] = 1.0f), (c))

	UI_GetThemeColor3fv(TH_SELECT, NO_ALPHA(g_theme.select_color));
	UI_GetThemeColor3fv(TH_EDGE_SELECT, NO_ALPHA(g_theme.edge_select_color));
	UI_GetThemeColorShade3fv(TH_EDGE_SELECT, -20, NO_ALPHA(g_theme.bone_select_color));
	UI_GetThemeColor3fv(TH_WIRE, NO_ALPHA(g_theme.wire_color));
	UI_GetThemeColor3fv(TH_WIRE_EDIT, NO_ALPHA(g_theme.wire_edit_color));
	UI_GetThemeColor3fv(TH_BONE_SOLID, NO_ALPHA(g_theme.bone_solid_color));
	UI_GetThemeColorBlendShade3fv(TH_WIRE_EDIT, TH_EDGE_SELECT, 0.15f, 0, NO_ALPHA(g_theme.bone_active_unselect_color));
	UI_GetThemeColor3fv(TH_BONE_POSE, NO_ALPHA(g_theme.bone_pose_color));
	UI_GetThemeColor3fv(TH_BONE_POSE_ACTIVE, NO_ALPHA(g_theme.bone_pose_active_color));
	UI_GetThemeColorBlendShade3fv(TH_WIRE, TH_BONE_POSE, 0.15f, 0, NO_ALPHA(g_theme.bone_pose_active_unselect_color));
	UI_GetThemeColor3fv(TH_TEXT_HI, NO_ALPHA(g_theme.text_hi_color));
	UI_GetThemeColor3fv(TH_TEXT, NO_ALPHA(g_theme.text_color));
	UI_GetThemeColor3fv(TH_VERTEX_SELECT, NO_ALPHA(g_theme.vertex_select_color));
	UI_GetThemeColor3fv(TH_VERTEX, NO_ALPHA(g_theme.vertex_color));

#undef NO_ALPHA
}

static const float *get_bone_solid_color(const EditBone *eBone, const bPoseChannel *pchan, const bArmature *arm)
{
	if (g_theme.const_color)
		return g_theme.bone_solid_color;

	/* Edit Mode */
	if (eBone) {
		bool is_active = (arm->act_edbone == eBone);
		if (eBone->flag & BONE_SELECTED) {
			if (is_active) {
				return g_theme.edge_select_color;
			}
			else {
				return g_theme.bone_select_color;
			}
		}
	}
	else if (arm->flag & ARM_POSEMODE) {
		bool is_active = (arm->act_bone == pchan->bone);
		if (pchan->bone->flag & BONE_SELECTED) {
			if (is_active) {
				return g_theme.bone_pose_active_color;
			}
			else {
				return g_theme.bone_pose_color;
			}
		}
	}

	return g_theme.bone_solid_color;
}

static const float *get_bone_wire_color(const EditBone *eBone, const bPoseChannel *pchan, const bArmature *arm)
{
	if (g_theme.const_color)
		return g_theme.const_color;

	if (eBone) {
		bool is_active = (arm->act_edbone == eBone);
		if (eBone->flag & BONE_SELECTED) {
			if (is_active) {
				return g_theme.edge_select_color;
			}
			else {
				return g_theme.bone_select_color;
			}
		}
		else {
			if (is_active) {
				return g_theme.bone_active_unselect_color;
			}
			else {
				return g_theme.wire_edit_color;
			}
		}
	}
	else if (arm->flag & ARM_POSEMODE) {
		bool is_active = (arm->act_bone == pchan->bone);
		if (pchan->bone->flag & BONE_SELECTED) {
			if (is_active) {
				return g_theme.bone_pose_active_color;
			}
			else {
				return g_theme.bone_pose_color;
			}
		}
		else {
			if (is_active) {
				return g_theme.bone_pose_active_unselect_color;
			}
			else {
				return g_theme.wire_color;
			}
		}
	}

	return g_theme.vertex_color;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Helper Utils
 * \{ */

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
	const float *col = (g_theme.const_color) ? g_theme.const_color :
	                   (BONE_FLAG(eBone, pchan) & BONE_SELECTED) ? g_theme.text_hi_color : g_theme.text_color;

	DRW_shgroup_bone_axes(BONE_VAR(eBone, pchan, disp_tail_mat), col);
}

static void draw_points(
        const EditBone *eBone, const bPoseChannel *pchan, const bArmature *arm,
        const int select_id)
{
	const float *col_solid_root = g_theme.bone_solid_color;
	const float *col_solid_tail = g_theme.bone_solid_color;
	const float *col_wire_root = (g_theme.const_color) ? g_theme.const_color : g_theme.vertex_color;
	const float *col_wire_tail = (g_theme.const_color) ? g_theme.const_color : g_theme.vertex_color;

	/* Edit bone points can be selected */
	if (eBone) {
		if (eBone->flag & BONE_ROOTSEL) {
			col_solid_root = g_theme.vertex_select_color;
			col_wire_root = g_theme.vertex_select_color;
		}
		if (eBone->flag & BONE_TIPSEL) {
			col_solid_tail = g_theme.vertex_select_color;
			col_wire_tail = g_theme.vertex_select_color;
		}
	}
	else if (arm->flag & ARM_POSEMODE) {
		col_solid_root = col_solid_tail = get_bone_solid_color(eBone, pchan, arm);
		col_wire_root = col_wire_tail = get_bone_wire_color(eBone, pchan, arm);
	}

	/*	Draw root point if we are not connected and parent are not hidden */
	if ((BONE_FLAG(eBone, pchan) & BONE_CONNECTED) == 0) {
		if (select_id != -1) {
			DRW_select_load_id(select_id | BONESEL_ROOT);
		}

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
	if (select_id != -1) {
		DRW_select_load_id(select_id | BONESEL_TIP);
	}
	DRW_shgroup_bone_point_solid(BONE_VAR(eBone, pchan, disp_tail_mat), col_solid_tail);
	DRW_shgroup_bone_point_wire(BONE_VAR(eBone, pchan, disp_tail_mat), col_wire_tail);

	if (select_id != -1) {
		DRW_select_load_id(-1);
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Draw Bones
 * \{ */

static void draw_bone_custom_shape(
        EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm),
        const int UNUSED(select_id))
{
	/* work in progress  -- fclem */
}

static void draw_bone_envelope(
        EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm),
        const int UNUSED(select_id))
{
	/* work in progress  -- fclem */
}

static void draw_bone_line(
        EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm),
        const int UNUSED(select_id))
{
	/* work in progress  -- fclem */
}

static void draw_bone_wire(
        EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm),
        const int UNUSED(select_id))
{
	/* work in progress  -- fclem */
}

static void draw_bone_box(
        EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm),
        const int UNUSED(select_id))
{
	/* work in progress  -- fclem */
}

static void draw_bone_octahedral(
        EditBone *eBone, bPoseChannel *pchan, bArmature *arm,
        const int select_id)
{
	const float *col_solid = get_bone_solid_color(eBone, pchan, arm);
	const float *col_wire = get_bone_wire_color(eBone, pchan, arm);

	if (select_id != -1) {
		DRW_select_load_id(select_id | BONESEL_BONE);
	}

	DRW_shgroup_bone_octahedral_solid(BONE_VAR(eBone, pchan, disp_mat), col_solid);
	DRW_shgroup_bone_octahedral_wire(BONE_VAR(eBone, pchan, disp_mat), col_wire);

	if (select_id != -1) {
		DRW_select_load_id(-1);
	}

	draw_points(eBone, pchan, arm, select_id);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Main Draw Loops
 * \{ */

static void draw_armature_edit(Object *ob)
{
	EditBone *eBone;
	bArmature *arm = ob->data;
	int index;
	const bool is_select = DRW_state_is_select();

	update_color(NULL);

	for (eBone = arm->edbo->first, index = 0; eBone; eBone = eBone->next, index++) {
		if (eBone->layer & arm->layer) {
			if ((eBone->flag & BONE_HIDDEN_A) == 0) {
				const int select_id = is_select ? index : (unsigned int)-1;

				draw_bone_update_disp_matrix(eBone, NULL, arm->drawtype);

				if (arm->drawtype == ARM_ENVELOPE) {
					draw_bone_envelope(eBone, NULL, arm, select_id);
				}
				else if (arm->drawtype == ARM_LINE) {
					draw_bone_line(eBone, NULL, arm, select_id);
				}
				else if (arm->drawtype == ARM_WIRE) {
					draw_bone_wire(eBone, NULL, arm, select_id);
				}
				else if (arm->drawtype == ARM_B_BONE) {
					draw_bone_box(eBone, NULL, arm, select_id);
				}
				else {
					draw_bone_octahedral(eBone, NULL, arm, select_id);
				}

				/*	Draw additional axes */
				if (arm->flag & ARM_DRAWAXES) {
					draw_axes(eBone, NULL);
				}
			}
		}
	}
}

/* if const_color is NULL do pose mode coloring */
static void draw_armature_pose(Object *ob, const float const_color[4])
{
	bArmature *arm = ob->data;
	bPoseChannel *pchan;
	int index = -1;
	Bone *bone;

	update_color(const_color);

	/* We can't safely draw non-updated pose, might contain NULL bone pointers... */
	if (ob->pose->flag & POSE_RECALC) {
		BKE_pose_rebuild(ob, arm);
	}

	// if (!(base->flag & OB_FROMDUPLI)) // TODO
	{
		if (ob->mode & OB_MODE_POSE) {
			arm->flag |= ARM_POSEMODE;
		}

		if (arm->flag & ARM_POSEMODE) {
			index = ob->base_selection_color;
		}
	}

	bool is_pose_select = (arm->flag & ARM_POSEMODE) && DRW_state_is_select();

	/* being set below */
	arm->layer_used = 0;

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		bone = pchan->bone;
		arm->layer_used |= bone->layer;

		/* bone must be visible */
		if ((bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0) {
			if (bone->layer & arm->layer) {
				const int select_id = is_pose_select ? index : (unsigned int)-1;

				draw_bone_update_disp_matrix(NULL, pchan, arm->drawtype);

				if ((pchan->custom) && !(arm->flag & ARM_NO_CUSTOM)) {
					draw_bone_custom_shape(NULL, pchan, arm, select_id);
				}
				else if (arm->drawtype == ARM_ENVELOPE) {
					draw_bone_envelope(NULL, pchan, arm, select_id);
				}
				else if (arm->drawtype == ARM_LINE) {
					draw_bone_line(NULL, pchan, arm, select_id);
				}
				else if (arm->drawtype == ARM_WIRE) {
					draw_bone_wire(NULL, pchan, arm, select_id);
				}
				else if (arm->drawtype == ARM_B_BONE) {
					draw_bone_box(NULL, pchan, arm, select_id);
				}
				else {
					draw_bone_octahedral(NULL, pchan, arm, select_id);
				}

				/*	Draw additional axes */
				if (arm->flag & ARM_DRAWAXES)
					draw_axes(NULL, pchan);

				if (is_pose_select) {
					index += 0x10000;
				}
			}
		}
	}

	arm->flag &= ~ARM_POSEMODE;
}

/**
 * This function set the object space to use for all subsequent `DRW_shgroup_bone_*` calls.
 */
static void DRW_shgroup_armature(
        Object *ob, DRWPass *pass_bone_solid, DRWPass *pass_bone_wire,
        DRWShadingGroup *shgrp_relationship_lines)
{
	memset(&g_data, 0x0, sizeof(g_data));
	g_data.ob = ob;

	g_data.bone_solid = pass_bone_solid;
	g_data.bone_wire = pass_bone_wire;
	g_data.relationship_lines = shgrp_relationship_lines;
}

void DRW_shgroup_armature_object(
        Object *ob, SceneLayer *sl, DRWPass *pass_bone_solid, DRWPass *pass_bone_wire,
        DRWShadingGroup *shgrp_relationship_lines)
{
	float *color;
	DRW_object_wire_theme_get(ob, sl, &color);

	DRW_shgroup_armature(ob, pass_bone_solid, pass_bone_wire, shgrp_relationship_lines);
	draw_armature_pose(ob, color);
}

void DRW_shgroup_armature_pose(
        Object *ob, DRWPass *pass_bone_solid, DRWPass *pass_bone_wire,
        DRWShadingGroup *shgrp_relationship_lines)
{
	DRW_shgroup_armature(ob, pass_bone_solid, pass_bone_wire, shgrp_relationship_lines);
	draw_armature_pose(ob, NULL);
}

void DRW_shgroup_armature_edit(
        Object *ob, DRWPass *pass_bone_solid, DRWPass *pass_bone_wire,
        DRWShadingGroup *shgrp_relationship_lines)
{
	DRW_shgroup_armature(ob, pass_bone_solid, pass_bone_wire, shgrp_relationship_lines);
	draw_armature_edit(ob);
}

/** \} */
