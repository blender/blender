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
#include "draw_manager_text.h"

#define BONE_VAR(eBone, pchan, var) ((eBone) ? (eBone->var) : (pchan->var))
#define BONE_FLAG(eBone, pchan) ((eBone) ? (eBone->flag) : (pchan->bone->flag))

/* For now just match 2.7x where possible. */
// #define USE_SOLID_COLOR

/* Reset for drawing each armature object */
static struct {
	/* Current armature object */
	Object *ob;
	/* Reset when changing current_armature */
	DRWShadingGroup *bone_octahedral_solid;
	DRWShadingGroup *bone_octahedral_wire;
	DRWShadingGroup *bone_box_solid;
	DRWShadingGroup *bone_box_wire;
	DRWShadingGroup *bone_wire_wire;
	DRWShadingGroup *bone_envelope_solid;
	DRWShadingGroup *bone_envelope_distance;
	DRWShadingGroup *bone_envelope_wire;
	DRWShadingGroup *bone_envelope_head_wire;
	DRWShadingGroup *bone_point_solid;
	DRWShadingGroup *bone_point_wire;
	DRWShadingGroup *bone_axes;
	DRWShadingGroup *relationship_lines;

	DRWPass *pass_bone_solid;
	DRWPass *pass_bone_wire;
	DRWPass *pass_bone_envelope;
} g_data = {NULL};

/* -------------------------------------------------------------------- */

/** \name Shader Groups (DRW_shgroup)
 * \{ */

/* Octahedral */
static void drw_shgroup_bone_octahedral_solid(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_octahedral_solid == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_octahedral_get();
		g_data.bone_octahedral_solid = shgroup_instance_solid(g_data.pass_bone_solid, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_octahedral_solid, final_bonemat, color);
}

static void drw_shgroup_bone_octahedral_wire(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_octahedral_wire == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_octahedral_wire_outline_get();
		g_data.bone_octahedral_wire = shgroup_instance_wire(g_data.pass_bone_wire, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_octahedral_wire, final_bonemat, color);
}

/* Box / B-Bone */
static void drw_shgroup_bone_box_solid(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_box_solid == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_box_get();
		g_data.bone_box_solid = shgroup_instance_solid(g_data.pass_bone_solid, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_box_solid, final_bonemat, color);
}

static void drw_shgroup_bone_box_wire(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_box_wire == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_box_wire_outline_get();
		g_data.bone_box_wire = shgroup_instance_wire(g_data.pass_bone_wire, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_box_wire, final_bonemat, color);
}

/* Wire */
static void drw_shgroup_bone_wire_wire(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_wire_wire == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_wire_wire_outline_get();
		g_data.bone_wire_wire = shgroup_instance_wire(g_data.pass_bone_wire, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_wire_wire, final_bonemat, color);
}

/* Envelope */
static void drw_shgroup_bone_envelope_distance(
        const float (*bone_mat)[4], const float color[4],
        const float *radius_head, const float *radius_tail, const float *distance)
{
	if (g_data.pass_bone_envelope != NULL) {
		if (g_data.bone_envelope_distance == NULL) {
			struct Gwn_Batch *geom = DRW_cache_bone_envelope_distance_outline_get();
			/* Note: bone_wire draw pass is not really working, think we need another one here? */
			g_data.bone_envelope_distance = shgroup_instance_bone_envelope_wire(g_data.pass_bone_envelope, geom);
		}
		float final_bonemat[4][4];
		mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
		DRW_shgroup_call_dynamic_add(g_data.bone_envelope_distance, final_bonemat, color, radius_head, radius_tail, distance);
	}
}

static void drw_shgroup_bone_envelope_solid(
        const float (*bone_mat)[4], const float color[4],
        const float *radius_head, const float *radius_tail)
{
	if (g_data.bone_envelope_solid == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_envelope_solid_get();
		g_data.bone_envelope_solid = shgroup_instance_bone_envelope_solid(g_data.pass_bone_solid, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_envelope_solid, final_bonemat, color, radius_head, radius_tail);
}

static void drw_shgroup_bone_envelope_wire(
        const float (*bone_mat)[4], const float color[4],
        const float *radius_head, const float *radius_tail, const float *distance)
{
	if (g_data.bone_envelope_wire == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_envelope_wire_outline_get();
		g_data.bone_envelope_wire = shgroup_instance_bone_envelope_wire(g_data.pass_bone_wire, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_envelope_wire, final_bonemat, color, radius_head, radius_tail, distance);
}

static void drw_shgroup_bone_envelope_head_wire(
        const float (*bone_mat)[4], const float color[4],
        const float *radius_head, const float *radius_tail, const float *distance)
{
	if (g_data.bone_envelope_head_wire == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_envelope_head_wire_outline_get();
		g_data.bone_envelope_head_wire = shgroup_instance_bone_envelope_wire(g_data.pass_bone_wire, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_envelope_head_wire, final_bonemat, color, radius_head, radius_tail, distance);
}

/* Custom (geometry) */

static void drw_shgroup_bone_custom_solid(const float (*bone_mat)[4], const float color[4], Object *custom)
{
	/* grr, not re-using instances! */
	struct Gwn_Batch *geom = DRW_cache_object_surface_get(custom);
	if (geom) {
		DRWShadingGroup *shgrp_geom_solid = shgroup_instance_solid(g_data.pass_bone_solid, geom);
		float final_bonemat[4][4];
		mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
		DRW_shgroup_call_dynamic_add(shgrp_geom_solid, final_bonemat, color);
	}
}

static void drw_shgroup_bone_custom_wire(const float (*bone_mat)[4], const float color[4], Object *custom)
{
	/* grr, not re-using instances! */
	struct Gwn_Batch *geom = DRW_cache_object_wire_outline_get(custom);
	if (geom) {
		DRWShadingGroup *shgrp_geom_wire = shgroup_instance_wire(g_data.pass_bone_wire, geom);
		float final_bonemat[4][4];
		mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
		DRW_shgroup_call_dynamic_add(shgrp_geom_wire, final_bonemat, color);
	}
}

/* Head and tail sphere */
static void drw_shgroup_bone_point_solid(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_point_solid == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_point_get();
		g_data.bone_point_solid = shgroup_instance_solid(g_data.pass_bone_solid, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_point_solid, final_bonemat, color);
}

static void drw_shgroup_bone_point_wire(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_point_wire == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_point_wire_outline_get();
		g_data.bone_point_wire = shgroup_instance_wire(g_data.pass_bone_wire, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_point_wire, final_bonemat, color);
}

/* Axes */
static void drw_shgroup_bone_axes(const float (*bone_mat)[4], const float color[4])
{
	if (g_data.bone_axes == NULL) {
		struct Gwn_Batch *geom = DRW_cache_bone_arrows_get();
		g_data.bone_axes = shgroup_instance_wire(g_data.pass_bone_wire, geom);
	}
	float final_bonemat[4][4];
	mul_m4_m4m4(final_bonemat, g_data.ob->obmat, bone_mat);
	DRW_shgroup_call_dynamic_add(g_data.bone_axes, final_bonemat, color);
}

/* Relationship lines */
static void UNUSED_FUNCTION(drw_shgroup_bone_relationship_lines)(const float head[3], const float tail[3])
{
	DRW_shgroup_call_dynamic_add(g_data.relationship_lines, head);
	DRW_shgroup_call_dynamic_add(g_data.relationship_lines, tail);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Drawing Theme Helpers
 *
 * Note, this section is duplicate of code in 'drawarmature.c'.
 *
 * \{ */

/* global here is reset before drawing each bone */
struct {
	const ThemeWireColor *bcolor;
} g_color;

/* values of colCode for set_pchan_color */
enum {
	PCHAN_COLOR_NORMAL  = 0,        /* normal drawing */
	PCHAN_COLOR_SOLID,              /* specific case where "solid" color is needed */
	PCHAN_COLOR_CONSTS,             /* "constraint" colors (which may/may-not be suppressed) */

	PCHAN_COLOR_SPHEREBONE_BASE,    /* for the 'stick' of sphere (envelope) bones */
	PCHAN_COLOR_SPHEREBONE_END,     /* for the ends of sphere (envelope) bones */
	PCHAN_COLOR_LINEBONE            /* for the middle of line-bones */
};

/* This function sets the color-set for coloring a certain bone */
static void set_pchan_colorset(Object *ob, bPoseChannel *pchan)
{
	bPose *pose = (ob) ? ob->pose : NULL;
	bArmature *arm = (ob) ? ob->data : NULL;
	bActionGroup *grp = NULL;
	short color_index = 0;

	/* sanity check */
	if (ELEM(NULL, ob, arm, pose, pchan)) {
		g_color.bcolor = NULL;
		return;
	}

	/* only try to set custom color if enabled for armature */
	if (arm->flag & ARM_COL_CUSTOM) {
		/* currently, a bone can only use a custom color set if it's group (if it has one),
		 * has been set to use one
		 */
		if (pchan->agrp_index) {
			grp = (bActionGroup *)BLI_findlink(&pose->agroups, (pchan->agrp_index - 1));
			if (grp)
				color_index = grp->customCol;
		}
	}

	/* bcolor is a pointer to the color set to use. If NULL, then the default
	 * color set (based on the theme colors for 3d-view) is used.
	 */
	if (color_index > 0) {
		bTheme *btheme = UI_GetTheme();
		g_color.bcolor = &btheme->tarm[(color_index - 1)];
	}
	else if (color_index == -1) {
		/* use the group's own custom color set (grp is always != NULL here) */
		g_color.bcolor = &grp->cs;
	}
	else {
		g_color.bcolor = NULL;
	}
}

/* This function is for brightening/darkening a given color (like UI_GetThemeColorShade3ubv()) */
static void cp_shade_color3ub(unsigned char cp[3], const int offset)
{
	int r, g, b;

	r = offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g = offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b = offset + (int) cp[2];
	CLAMP(b, 0, 255);

	cp[0] = r;
	cp[1] = g;
	cp[2] = b;
}

/* This function sets the gl-color for coloring a certain bone (based on bcolor) */
static bool set_pchan_color(short colCode, const int boneflag, const short constflag, float r_color[4])
{
	float *fcolor = r_color;
	const ThemeWireColor *bcolor = g_color.bcolor;

	switch (colCode) {
		case PCHAN_COLOR_NORMAL:
		{
			if (bcolor) {
				unsigned char cp[4] = {255};

				if (boneflag & BONE_DRAW_ACTIVE) {
					copy_v3_v3_char((char *)cp, bcolor->active);
					if (!(boneflag & BONE_SELECTED)) {
						cp_shade_color3ub(cp, -80);
					}
				}
				else if (boneflag & BONE_SELECTED) {
					copy_v3_v3_char((char *)cp, bcolor->select);
				}
				else {
					/* a bit darker than solid */
					copy_v3_v3_char((char *)cp, bcolor->solid);
					cp_shade_color3ub(cp, -50);
				}

				rgb_uchar_to_float(fcolor, cp);
			}
			else {
				if ((boneflag & BONE_DRAW_ACTIVE) && (boneflag & BONE_SELECTED)) {
					UI_GetThemeColor4fv(TH_BONE_POSE_ACTIVE, fcolor);
				}
				else if (boneflag & BONE_DRAW_ACTIVE) {
					UI_GetThemeColorBlendShade4fv(TH_WIRE, TH_BONE_POSE, 0.15f, 0, fcolor);
				}
				else if (boneflag & BONE_SELECTED) {
					UI_GetThemeColor4fv(TH_BONE_POSE, fcolor);
				}
				else {
					UI_GetThemeColor4fv(TH_WIRE, fcolor);
				}
			}

			return true;
		}
		case PCHAN_COLOR_SOLID:
		{
			if (bcolor) {
				rgb_uchar_to_float(fcolor, (unsigned char *)bcolor->solid);
			}
			else {
				UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
			}

			return true;
		}
		case PCHAN_COLOR_CONSTS:
		{
			if ((bcolor == NULL) || (bcolor->flag & TH_WIRECOLOR_CONSTCOLS)) {
				unsigned char cp[4];
				if (constflag & PCHAN_HAS_TARGET) rgba_char_args_set((char *)cp, 255, 150, 0, 80);
				else if (constflag & PCHAN_HAS_IK) rgba_char_args_set((char *)cp, 255, 255, 0, 80);
				else if (constflag & PCHAN_HAS_SPLINEIK) rgba_char_args_set((char *)cp, 200, 255, 0, 80);
				else if (constflag & PCHAN_HAS_CONST) rgba_char_args_set((char *)cp, 0, 255, 120, 80);

				rgba_uchar_to_float(fcolor, cp);

				return true;
			}
			return false;
		}
		case PCHAN_COLOR_SPHEREBONE_BASE:
		{
			if (bcolor) {
				unsigned char cp[4] = {255};

				if (boneflag & BONE_DRAW_ACTIVE) {
					copy_v3_v3_char((char *)cp, bcolor->active);
				}
				else if (boneflag & BONE_SELECTED) {
					copy_v3_v3_char((char *)cp, bcolor->select);
				}
				else {
					copy_v3_v3_char((char *)cp, bcolor->solid);
				}

				rgb_uchar_to_float(fcolor, cp);
			}
			else {
				if (boneflag & BONE_DRAW_ACTIVE) {
					UI_GetThemeColorShade4fv(TH_BONE_POSE, 40, fcolor);
				}
				else if (boneflag & BONE_SELECTED) {
					UI_GetThemeColor4fv(TH_BONE_POSE, fcolor);
				}
				else {
					UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
				}
			}

			return true;
		}
		case PCHAN_COLOR_SPHEREBONE_END:
		{
			if (bcolor) {
				unsigned char cp[4] = {255};

				if (boneflag & BONE_DRAW_ACTIVE) {
					copy_v3_v3_char((char *)cp, bcolor->active);
					cp_shade_color3ub(cp, 10);
				}
				else if (boneflag & BONE_SELECTED) {
					copy_v3_v3_char((char *)cp, bcolor->select);
					cp_shade_color3ub(cp, -30);
				}
				else {
					copy_v3_v3_char((char *)cp, bcolor->solid);
					cp_shade_color3ub(cp, -30);
				}

				rgb_uchar_to_float(fcolor, cp);
			}
			else {
				if (boneflag & BONE_DRAW_ACTIVE) {
					UI_GetThemeColorShade4fv(TH_BONE_POSE, 10, fcolor);
				}
				else if (boneflag & BONE_SELECTED) {
					UI_GetThemeColorShade4fv(TH_BONE_POSE, -30, fcolor);
				}
				else {
					UI_GetThemeColorShade4fv(TH_BONE_SOLID, -30, fcolor);
				}
			}
			break;
		}
		case PCHAN_COLOR_LINEBONE:
		{
			/* inner part in background color or constraint */
			if ((constflag) && ((bcolor == NULL) || (bcolor->flag & TH_WIRECOLOR_CONSTCOLS))) {
				unsigned char cp[4];
				if (constflag & PCHAN_HAS_TARGET) rgba_char_args_set((char *)cp, 255, 150, 0, 255);
				else if (constflag & PCHAN_HAS_IK) rgba_char_args_set((char *)cp, 255, 255, 0, 255);
				else if (constflag & PCHAN_HAS_SPLINEIK) rgba_char_args_set((char *)cp, 200, 255, 0, 255);
				else if (constflag & PCHAN_HAS_CONST) rgba_char_args_set((char *)cp, 0, 255, 120, 255);
				else if (constflag) UI_GetThemeColor4ubv(TH_BONE_POSE, cp);  /* PCHAN_HAS_ACTION */

				rgb_uchar_to_float(fcolor, cp);
			}
			else {
				if (bcolor) {
					const char *cp = bcolor->solid;
					rgb_uchar_to_float(fcolor, (unsigned char *)cp);
					fcolor[3] = 204.f / 255.f;
				}
				else {
					UI_GetThemeColorShade4fv(TH_BACK, -30, fcolor);
				}
			}

			return true;
		}
	}

	return false;
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

static const float *get_bone_solid_color(
        const EditBone *eBone, const bPoseChannel *pchan, const bArmature *arm,
        const int boneflag, const short constflag)
{
	if (g_theme.const_color)
		return g_theme.bone_solid_color;

#ifdef USE_SOLID_COLOR
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
#else
	if (arm->drawtype == ARM_ENVELOPE) {
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
	}
#endif

	if (arm->flag & ARM_POSEMODE) {
		float *disp_color = pchan->draw_data->solid_color;
		set_pchan_color(PCHAN_COLOR_SOLID, boneflag, constflag, disp_color);
		disp_color[3] = 1.0;
		return disp_color;
	}

	return g_theme.bone_solid_color;
}

static const float *get_bone_wire_color(
        const EditBone *eBone, const bPoseChannel *pchan, const bArmature *arm,
        const int boneflag, const short constflag)
{
	if (g_theme.const_color)
		return g_theme.const_color;

	if (eBone) {
		if (boneflag & BONE_SELECTED) {
			if (boneflag & BONE_DRAW_ACTIVE) {
				return g_theme.edge_select_color;
			}
			else {
				return g_theme.bone_select_color;
			}
		}
		else {
			if (boneflag & BONE_DRAW_ACTIVE) {
				return g_theme.bone_active_unselect_color;
			}
			else {
				return g_theme.wire_edit_color;
			}
		}
	}
	else if (arm->flag & ARM_POSEMODE) {
		float *disp_color = pchan->draw_data->wire_color;
		set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag, disp_color);
		disp_color[3] = 1.0;
		return disp_color;


#if 0
		if (boneflag & BONE_SELECTED) {
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
#endif
	}

	return g_theme.vertex_color;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Helper Utils
 * \{ */

static void pchan_draw_data_init(bPoseChannel *pchan)
{
	if (pchan->draw_data != NULL) {
		if (pchan->draw_data->bbone_matrix_len != pchan->bone->segments) {
			MEM_SAFE_FREE(pchan->draw_data);
		}
	}

	if (pchan->draw_data == NULL) {
		pchan->draw_data = MEM_mallocN(sizeof(*pchan->draw_data) + sizeof(Mat4) * pchan->bone->segments, __func__);
		pchan->draw_data->bbone_matrix_len = pchan->bone->segments;
	}
}

static void draw_bone_update_disp_matrix_default(EditBone *eBone, bPoseChannel *pchan)
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

	scale_m4_fl(s, length);
	mul_m4_m4m4(disp_mat, bone_mat, s);
	copy_m4_m4(disp_tail_mat, disp_mat);
	translate_m4(disp_tail_mat, 0.0f, 1.0f, 0.0f);
}

/* XXX Direct copy from drawarmature.c... This is ugly! */
/* A partial copy of b_bone_spline_setup(), with just the parts for previewing editmode curve settings
 *
 * This assumes that prev/next bones don't have any impact (since they should all still be in the "straight"
 * position here anyway), and that we can simply apply the bbone settings to get the desired effect...
 */
static void ebone_spline_preview(EditBone *ebone, float result_array[MAX_BBONE_SUBDIV][4][4])
{
	float h1[3], h2[3], length, hlength1, hlength2, roll1 = 0.0f, roll2 = 0.0f;
	float mat3[3][3];
	float data[MAX_BBONE_SUBDIV + 1][4], *fp;
	int a;

	length = ebone->length;

	hlength1 = ebone->ease1 * length * 0.390464f; /* 0.5f * sqrt(2) * kappa, the handle length for near-perfect circles */
	hlength2 = ebone->ease2 * length * 0.390464f;

	/* find the handle points, since this is inside bone space, the
	 * first point = (0, 0, 0)
	 * last point =  (0, length, 0)
	 *
	 * we also just apply all the "extra effects", since they're the whole reason we're doing this...
	 */
	h1[0] = ebone->curveInX;
	h1[1] = hlength1;
	h1[2] = ebone->curveInY;
	roll1 = ebone->roll1;

	h2[0] = ebone->curveOutX;
	h2[1] = -hlength2;
	h2[2] = ebone->curveOutY;
	roll2 = ebone->roll2;

	/* make curve */
	if (ebone->segments > MAX_BBONE_SUBDIV)
		ebone->segments = MAX_BBONE_SUBDIV;

	BKE_curve_forward_diff_bezier(0.0f,  h1[0],                               h2[0],                               0.0f,   data[0],     MAX_BBONE_SUBDIV, 4 * sizeof(float));
	BKE_curve_forward_diff_bezier(0.0f,  h1[1],                               length + h2[1],                      length, data[0] + 1, MAX_BBONE_SUBDIV, 4 * sizeof(float));
	BKE_curve_forward_diff_bezier(0.0f,  h1[2],                               h2[2],                               0.0f,   data[0] + 2, MAX_BBONE_SUBDIV, 4 * sizeof(float));
	BKE_curve_forward_diff_bezier(roll1, roll1 + 0.390464f * (roll2 - roll1), roll2 - 0.390464f * (roll2 - roll1), roll2,  data[0] + 3, MAX_BBONE_SUBDIV, 4 * sizeof(float));

	equalize_bbone_bezier(data[0], ebone->segments); /* note: does stride 4! */

	/* make transformation matrices for the segments for drawing */
	for (a = 0, fp = data[0]; a < ebone->segments; a++, fp += 4) {
		sub_v3_v3v3(h1, fp + 4, fp);
		vec_roll_to_mat3(h1, fp[3], mat3); /* fp[3] is roll */

		copy_m4_m3(result_array[a], mat3);
		copy_v3_v3(result_array[a][3], fp);

		/* "extra" scale facs... */
		{
			const int num_segments = ebone->segments;

			const float scaleFactorIn  = 1.0f + (ebone->scaleIn  - 1.0f) * ((float)(num_segments - a) / (float)num_segments);
			const float scaleFactorOut = 1.0f + (ebone->scaleOut - 1.0f) * ((float)(a + 1)            / (float)num_segments);

			const float scalefac = scaleFactorIn * scaleFactorOut;
			float bscalemat[4][4], bscale[3];

			bscale[0] = scalefac;
			bscale[1] = 1.0f;
			bscale[2] = scalefac;

			size_to_mat4(bscalemat, bscale);

			/* Note: don't multiply by inverse scale mat here, as it causes problems with scaling shearing and breaking segment chains */
			mul_m4_series(result_array[a], result_array[a], bscalemat);
		}
	}
}

static void draw_bone_update_disp_matrix_bbone(EditBone *eBone, bPoseChannel *pchan)
{
	float s[4][4], ebmat[4][4];
	float length, xwidth, zwidth;
	float (*bone_mat)[4];
	short bbone_segments;

	/* TODO : This should be moved to depsgraph or armature refresh
	 * and not be tight to the draw pass creation.
	 * This would refresh armature without invalidating the draw cache */
	if (pchan) {
		length = pchan->bone->length;
		xwidth = pchan->bone->xwidth;
		zwidth = pchan->bone->zwidth;
		bone_mat = pchan->pose_mat;
		bbone_segments = pchan->bone->segments;
	}
	else {
		eBone->length = len_v3v3(eBone->tail, eBone->head);
		ED_armature_ebone_to_mat4(eBone, ebmat);

		length = eBone->length;
		xwidth = eBone->xwidth;
		zwidth = eBone->zwidth;
		bone_mat = ebmat;
		bbone_segments = eBone->segments;
	}

	size_to_mat4(s, (const float[3]){xwidth, length / bbone_segments, zwidth});

	/* Compute BBones segment matrices... */
	/* Note that we need this even for one-segment bones, because box drawing need specific weirdo matrix for the box,
	 * that we cannot use to draw end points & co. */
	if (pchan) {
		Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
		if (bbone_segments > 1) {
			b_bone_spline_setup(pchan, 0, bbones_mat);

			for (int i = bbone_segments; i--; bbones_mat++) {
				mul_m4_m4m4(bbones_mat->mat, bbones_mat->mat, s);
				mul_m4_m4m4(bbones_mat->mat, bone_mat, bbones_mat->mat);
			}
		}
		else {
			mul_m4_m4m4(bbones_mat->mat, bone_mat, s);
		}
	}
	else {
		float (*bbones_mat)[4][4] = eBone->disp_bbone_mat;

		if (bbone_segments > 1) {
			ebone_spline_preview(eBone, bbones_mat);

			for (int i = bbone_segments; i--; bbones_mat++) {
				mul_m4_m4m4(*bbones_mat, *bbones_mat, s);
				mul_m4_m4m4(*bbones_mat, bone_mat, *bbones_mat);
			}
		}
		else {
			mul_m4_m4m4(*bbones_mat, bone_mat, s);
		}
	}

	/* Grrr... We need default display matrix to draw end points, axes, etc. :( */
	draw_bone_update_disp_matrix_default(eBone, pchan);
}

static void draw_bone_update_disp_matrix_custom(bPoseChannel *pchan)
{
	float s[4][4];
	float length;
	float (*bone_mat)[4];
	float (*disp_mat)[4];
	float (*disp_tail_mat)[4];

	/* See TODO above */
	length = PCHAN_CUSTOM_DRAW_SIZE(pchan);
	bone_mat = pchan->custom_tx ? pchan->custom_tx->pose_mat : pchan->pose_mat;
	disp_mat = pchan->disp_mat;
	disp_tail_mat = pchan->disp_tail_mat;

	scale_m4_fl(s, length);
	mul_m4_m4m4(disp_mat, bone_mat, s);
	copy_m4_m4(disp_tail_mat, disp_mat);
	translate_m4(disp_tail_mat, 0.0f, 1.0f, 0.0f);
}

static void draw_axes(EditBone *eBone, bPoseChannel *pchan)
{
	const float *col = (g_theme.const_color) ? g_theme.const_color :
	                   (BONE_FLAG(eBone, pchan) & BONE_SELECTED) ? g_theme.text_hi_color : g_theme.text_color;

	drw_shgroup_bone_axes(BONE_VAR(eBone, pchan, disp_mat), col);
}

static void draw_points(
        const EditBone *eBone, const bPoseChannel *pchan, const bArmature *arm,
        const int boneflag, const short constflag,
        const int select_id)
{
	const float *col_solid_root = g_theme.bone_solid_color;
	const float *col_solid_tail = g_theme.bone_solid_color;
	const float *col_wire_root = (g_theme.const_color) ? g_theme.const_color : g_theme.vertex_color;
	const float *col_wire_tail = (g_theme.const_color) ? g_theme.const_color : g_theme.vertex_color;

	const bool is_envelope_draw = (arm->drawtype == ARM_ENVELOPE);
	static const float envelope_ignore = -1.0f;

	/* Edit bone points can be selected */
	if (eBone) {
		if (eBone->flag & BONE_ROOTSEL) {
#ifdef USE_SOLID_COLOR
			col_solid_root = g_theme.vertex_select_color;
#else
			if (is_envelope_draw) {
				col_solid_root = g_theme.vertex_select_color;
			}
#endif
			col_wire_root = g_theme.vertex_select_color;
		}
		if (eBone->flag & BONE_TIPSEL) {
#ifdef USE_SOLID_COLOR
			col_solid_tail = g_theme.vertex_select_color;
#else
			if (is_envelope_draw) {
				col_solid_tail = g_theme.vertex_select_color;
			}
#endif
			col_wire_tail = g_theme.vertex_select_color;
		}
	}
	else if (arm->flag & ARM_POSEMODE) {
		col_solid_root = col_solid_tail = get_bone_solid_color(eBone, pchan, arm, boneflag, constflag);
		col_wire_root = col_wire_tail = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
	}

	/*	Draw root point if we are not connected and parent are not hidden */
	if ((BONE_FLAG(eBone, pchan) & BONE_CONNECTED) == 0) {
		if (select_id != -1) {
			DRW_select_load_id(select_id | BONESEL_ROOT);
		}

		if (eBone) {
			if (!((eBone->parent) && !EBONE_VISIBLE(arm, eBone->parent))) {
				if (is_envelope_draw) {
					drw_shgroup_bone_envelope_solid(eBone->disp_mat, col_solid_root,
					                                &eBone->rad_head, &envelope_ignore);
					drw_shgroup_bone_envelope_head_wire(eBone->disp_mat, col_wire_root,
					                                    &eBone->rad_head, &envelope_ignore, &envelope_ignore);
				}
				else {
					drw_shgroup_bone_point_solid(eBone->disp_mat, col_solid_root);
					drw_shgroup_bone_point_wire(eBone->disp_mat, col_wire_root);
				}
			}
		}
		else {
			Bone *bone = pchan->bone;
			if (!((bone->parent) && (bone->parent->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)))) {
				if (is_envelope_draw) {
					drw_shgroup_bone_envelope_solid(pchan->disp_mat, col_solid_root,
					                                &bone->rad_head, &envelope_ignore);
					drw_shgroup_bone_envelope_head_wire(pchan->disp_mat, col_wire_root,
					                                    &bone->rad_head, &envelope_ignore, &envelope_ignore);
				}
				else {
					drw_shgroup_bone_point_solid(pchan->disp_mat, col_solid_root);
					drw_shgroup_bone_point_wire(pchan->disp_mat, col_wire_root);
				}
			}
		}
	}

	/*	Draw tip point */
	if (select_id != -1) {
		DRW_select_load_id(select_id | BONESEL_TIP);
	}

	if (is_envelope_draw) {
		const float *rad_tail = eBone ? &eBone->rad_tail : &pchan->bone->rad_tail;
		drw_shgroup_bone_envelope_solid(
		            BONE_VAR(eBone, pchan, disp_mat), col_solid_tail, &envelope_ignore, rad_tail);
		drw_shgroup_bone_envelope_head_wire(
		            BONE_VAR(eBone, pchan, disp_mat), col_wire_tail, &envelope_ignore, rad_tail, &envelope_ignore);
	}
	else {
		drw_shgroup_bone_point_solid(BONE_VAR(eBone, pchan, disp_tail_mat), col_solid_tail);
		drw_shgroup_bone_point_wire(BONE_VAR(eBone, pchan, disp_tail_mat), col_wire_tail);
	}

	if (select_id != -1) {
		DRW_select_load_id(-1);
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Draw Bones
 * \{ */

static void draw_bone_custom_shape(
        EditBone *eBone, bPoseChannel *pchan, bArmature *arm,
        const int boneflag, const short constflag,
        const int select_id)
{
	const float *col_solid = get_bone_solid_color(eBone, pchan, arm, boneflag, constflag);
	const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);
	const float (*disp_mat)[4] = pchan->disp_mat;

	if (select_id != -1) {
		DRW_select_load_id(select_id | BONESEL_BONE);
	}

	drw_shgroup_bone_custom_solid(disp_mat, col_solid, pchan->custom);
	drw_shgroup_bone_custom_wire(disp_mat, col_wire, pchan->custom);

	if (select_id != -1) {
		DRW_select_load_id(-1);
	}
}

static void draw_bone_envelope(
        EditBone *eBone, bPoseChannel *pchan, bArmature *arm,
        const int boneflag, const short constflag,
        const int select_id)
{
	const float *col_solid = get_bone_solid_color(eBone, pchan, arm, boneflag, constflag);
	const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);

	static const float col_white[4] = {1.0f, 1.0f, 1.0f, 0.2f};

	float *rad_head, *rad_tail, *distance;
	if (eBone) {
		rad_tail = &eBone->rad_tail;
		distance = &eBone->dist;
		rad_head = (eBone->parent && (boneflag & BONE_CONNECTED)) ? &eBone->parent->rad_tail : &eBone->rad_head;
	}
	else {
		rad_tail = &pchan->bone->rad_tail;
		distance = &pchan->bone->dist;
		rad_head = (pchan->parent && (boneflag & BONE_CONNECTED)) ? &pchan->parent->bone->rad_tail : &pchan->bone->rad_head;
	}

	if ((boneflag & BONE_NO_DEFORM) == 0 &&
	    ((boneflag & BONE_SELECTED) || (eBone && (boneflag & (BONE_ROOTSEL | BONE_TIPSEL)))))
	{
		drw_shgroup_bone_envelope_distance(BONE_VAR(eBone, pchan, disp_mat), col_white, rad_head, rad_tail, distance);
	}

	if (select_id != -1) {
		DRW_select_load_id(select_id | BONESEL_BONE);
	}

	drw_shgroup_bone_envelope_solid(BONE_VAR(eBone, pchan, disp_mat), col_solid, rad_head, rad_tail);
	drw_shgroup_bone_envelope_wire(BONE_VAR(eBone, pchan, disp_mat), col_wire, rad_head, rad_tail, distance);

	if (select_id != -1) {
		DRW_select_load_id(-1);
	}

	draw_points(eBone, pchan, arm, boneflag, constflag, select_id);
}

static void draw_bone_line(
        EditBone *UNUSED(eBone), bPoseChannel *UNUSED(pchan), bArmature *UNUSED(arm),
        const int UNUSED(boneflag), const short UNUSED(constflag),
        const int UNUSED(select_id))
{
	/* work in progress  -- fclem */
}

static void draw_bone_wire(
        EditBone *eBone, bPoseChannel *pchan, bArmature *arm,
        const int boneflag, const short constflag,
        const int select_id)
{
	const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);

	if (select_id != -1) {
		DRW_select_load_id(select_id | BONESEL_BONE);
	}

	if (pchan) {
		Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
		BLI_assert(bbones_mat != NULL);

		for (int i = pchan->bone->segments; i--; bbones_mat++) {
			drw_shgroup_bone_wire_wire(bbones_mat->mat, col_wire);
		}
	}
	else if (eBone) {
		for (int i = 0; i < eBone->segments; i++) {
			drw_shgroup_bone_wire_wire(eBone->disp_bbone_mat[i], col_wire);
		}
	}

	if (select_id != -1) {
		DRW_select_load_id(-1);
	}

	if (eBone) {
		draw_points(eBone, pchan, arm, boneflag, constflag, select_id);
	}
}

static void draw_bone_box(
        EditBone *eBone, bPoseChannel *pchan, bArmature *arm,
        const int boneflag, const short constflag,
        const int select_id)
{
	const float *col_solid = get_bone_solid_color(eBone, pchan, arm, boneflag, constflag);
	const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);

	if (select_id != -1) {
		DRW_select_load_id(select_id | BONESEL_BONE);
	}

	if (pchan) {
		Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
		BLI_assert(bbones_mat != NULL);

		for (int i = pchan->bone->segments; i--; bbones_mat++) {
			drw_shgroup_bone_box_solid(bbones_mat->mat, col_solid);
			drw_shgroup_bone_box_wire(bbones_mat->mat, col_wire);
		}
	}
	else if (eBone) {
		for (int i = 0; i < eBone->segments; i++) {
			drw_shgroup_bone_box_solid(eBone->disp_bbone_mat[i], col_solid);
			drw_shgroup_bone_box_wire(eBone->disp_bbone_mat[i], col_wire);
		}
	}

	if (select_id != -1) {
		DRW_select_load_id(-1);
	}

	if (eBone) {
		draw_points(eBone, pchan, arm, boneflag, constflag, select_id);
	}
}

static void draw_bone_octahedral(
        EditBone *eBone, bPoseChannel *pchan, bArmature *arm,
        const int boneflag, const short constflag,
        const int select_id)
{
	const float *col_solid = get_bone_solid_color(eBone, pchan, arm, boneflag, constflag);
	const float *col_wire = get_bone_wire_color(eBone, pchan, arm, boneflag, constflag);

	if (select_id != -1) {
		DRW_select_load_id(select_id | BONESEL_BONE);
	}

	drw_shgroup_bone_octahedral_solid(BONE_VAR(eBone, pchan, disp_mat), col_solid);
	drw_shgroup_bone_octahedral_wire(BONE_VAR(eBone, pchan, disp_mat), col_wire);

	if (select_id != -1) {
		DRW_select_load_id(-1);
	}

	draw_points(eBone, pchan, arm, boneflag, constflag, select_id);
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

	const bool show_text = DRW_state_show_text();

	for (eBone = arm->edbo->first, index = 0; eBone; eBone = eBone->next, index++) {
		if (eBone->layer & arm->layer) {
			if ((eBone->flag & BONE_HIDDEN_A) == 0) {
				const int select_id = is_select ? index : (unsigned int)-1;

				const short constflag = 0;

				/* catch exception for bone with hidden parent */
				int boneflag = eBone->flag;
				if ((eBone->parent) && !EBONE_VISIBLE(arm, eBone->parent)) {
					boneflag &= ~BONE_CONNECTED;
				}

				/* set temporary flag for drawing bone as active, but only if selected */
				if (eBone == arm->act_edbone) {
					boneflag |= BONE_DRAW_ACTIVE;
				}

				if (arm->drawtype == ARM_ENVELOPE) {
					draw_bone_update_disp_matrix_default(eBone, NULL);
					draw_bone_envelope(eBone, NULL, arm, boneflag, constflag, select_id);
				}
				else if (arm->drawtype == ARM_LINE) {
					draw_bone_update_disp_matrix_default(eBone, NULL);
					draw_bone_line(eBone, NULL, arm, boneflag, constflag, select_id);
				}
				else if (arm->drawtype == ARM_WIRE) {
					draw_bone_update_disp_matrix_bbone(eBone, NULL);
					draw_bone_wire(eBone, NULL, arm, boneflag, constflag, select_id);
				}
				else if (arm->drawtype == ARM_B_BONE) {
					draw_bone_update_disp_matrix_bbone(eBone, NULL);
					draw_bone_box(eBone, NULL, arm, boneflag, constflag, select_id);
				}
				else {
					draw_bone_update_disp_matrix_default(eBone, NULL);
					draw_bone_octahedral(eBone, NULL, arm, boneflag, constflag, select_id);
				}

				/* Draw names of bone */
				if (show_text && (arm->flag & ARM_DRAWNAMES)) {
					unsigned char color[4];
					UI_GetThemeColor4ubv((eBone->flag & BONE_SELECTED) ? TH_TEXT_HI : TH_TEXT, color);

					float vec[3];
					mid_v3_v3v3(vec, eBone->head, eBone->tail);
					mul_m4_v3(ob->obmat, vec);

					struct DRWTextStore *dt = DRW_text_cache_ensure();
					DRW_text_cache_add(
					        dt, vec, eBone->name, strlen(eBone->name),
					        10, DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR, color);
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
			index = ob->select_color;
		}
	}

	const bool is_pose_select = (arm->flag & ARM_POSEMODE) && DRW_state_is_select();
	const bool show_text = DRW_state_show_text();

	/* being set below */
	arm->layer_used = 0;

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		bone = pchan->bone;
		arm->layer_used |= bone->layer;

		/* bone must be visible */
		if ((bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0) {
			if (bone->layer & arm->layer) {
				const int select_id = is_pose_select ? index : (unsigned int)-1;

				const short constflag = pchan->constflag;

				pchan_draw_data_init(pchan);

				if (const_color) {
					/* keep color */
				}
				else {
					/* set color-set to use */
					set_pchan_colorset(ob, pchan);
				}

				int boneflag = bone->flag;
				/* catch exception for bone with hidden parent */
				boneflag = bone->flag;
				if ((bone->parent) && (bone->parent->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
					boneflag &= ~BONE_CONNECTED;
				}

				/* set temporary flag for drawing bone as active, but only if selected */
				if (bone == arm->act_bone)
					boneflag |= BONE_DRAW_ACTIVE;


				if ((pchan->custom) && !(arm->flag & ARM_NO_CUSTOM)) {
					draw_bone_update_disp_matrix_custom(pchan);
					draw_bone_custom_shape(NULL, pchan, arm, boneflag, constflag, select_id);
				}
				else if (arm->drawtype == ARM_ENVELOPE) {
					draw_bone_update_disp_matrix_default(NULL, pchan);
					draw_bone_envelope(NULL, pchan, arm, boneflag, constflag, select_id);
				}
				else if (arm->drawtype == ARM_LINE) {
					draw_bone_update_disp_matrix_default(NULL, pchan);
					draw_bone_line(NULL, pchan, arm, boneflag, constflag, select_id);
				}
				else if (arm->drawtype == ARM_WIRE) {
					draw_bone_update_disp_matrix_bbone(NULL, pchan);
					draw_bone_wire(NULL, pchan, arm, boneflag, constflag, select_id);
				}
				else if (arm->drawtype == ARM_B_BONE) {
					draw_bone_update_disp_matrix_bbone(NULL, pchan);
					draw_bone_box(NULL, pchan, arm, boneflag, constflag, select_id);
				}
				else {
					draw_bone_update_disp_matrix_default(NULL, pchan);
					draw_bone_octahedral(NULL, pchan, arm, boneflag, constflag, select_id);
				}

				/* Draw names of bone */
				if (show_text && (arm->flag & ARM_DRAWNAMES)) {
					unsigned char color[4];
					UI_GetThemeColor4ubv((arm->flag & ARM_POSEMODE) &&
					                     (bone->flag & BONE_SELECTED) ? TH_TEXT_HI : TH_TEXT, color);
					float vec[3];
					mid_v3_v3v3(vec, pchan->pose_head, pchan->pose_tail);
					mul_m4_v3(ob->obmat, vec);

					struct DRWTextStore *dt = DRW_text_cache_ensure();
					DRW_text_cache_add(
					        dt, vec, pchan->name, strlen(pchan->name),
					        10, DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR, color);
				}

				/*	Draw additional axes */
				if (arm->flag & ARM_DRAWAXES) {
					draw_axes(NULL, pchan);
				}
			}
		}
		if (is_pose_select) {
			index += 0x10000;
		}
	}

	arm->flag &= ~ARM_POSEMODE;
}

/**
 * This function set the object space to use for all subsequent `DRW_shgroup_bone_*` calls.
 */
static void drw_shgroup_armature(
        Object *ob, DRWPass *pass_bone_solid, DRWPass *pass_bone_wire, DRWPass *pass_bone_envelope,
        DRWShadingGroup *shgrp_relationship_lines)
{
	memset(&g_data, 0x0, sizeof(g_data));
	g_data.ob = ob;

	g_data.pass_bone_solid = pass_bone_solid;
	g_data.pass_bone_wire = pass_bone_wire;
	g_data.pass_bone_envelope = pass_bone_envelope;
	g_data.relationship_lines = shgrp_relationship_lines;

	memset(&g_color, 0x0, sizeof(g_color));
}

void DRW_shgroup_armature_object(
        Object *ob, ViewLayer *view_layer, DRWPass *pass_bone_solid, DRWPass *pass_bone_wire, DRWPass *UNUSED(pass_bone_envelope),
        DRWShadingGroup *shgrp_relationship_lines)
{
	float *color;
	DRW_object_wire_theme_get(ob, view_layer, &color);

	drw_shgroup_armature(ob, pass_bone_solid, pass_bone_wire, NULL, shgrp_relationship_lines);
	draw_armature_pose(ob, color);
}

void DRW_shgroup_armature_pose(
        Object *ob, DRWPass *pass_bone_solid, DRWPass *pass_bone_wire, DRWPass *pass_bone_envelope,
        DRWShadingGroup *shgrp_relationship_lines)
{
	drw_shgroup_armature(ob, pass_bone_solid, pass_bone_wire, pass_bone_envelope, shgrp_relationship_lines);
	draw_armature_pose(ob, NULL);
}

void DRW_shgroup_armature_edit(
        Object *ob, DRWPass *pass_bone_solid, DRWPass *pass_bone_wire, DRWPass *pass_bone_envelope,
        DRWShadingGroup *shgrp_relationship_lines)
{
	drw_shgroup_armature(ob, pass_bone_solid, pass_bone_wire, pass_bone_envelope, shgrp_relationship_lines);
	draw_armature_edit(ob);
}

/** \} */
