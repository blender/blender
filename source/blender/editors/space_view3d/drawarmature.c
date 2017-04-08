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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawarmature.c
 *  \ingroup spview3d
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

#include "BIF_glutil.h"

#include "ED_armature.h"
#include "ED_keyframes_draw.h"

#include "GPU_basic_shader.h"
#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"

#include "UI_resources.h"

#include "view3d_intern.h"

#include "GPU_select.h"

/* *************** Armature Drawing - Coloring API ***************************** */

/* global here is reset before drawing each bone */
static ThemeWireColor *bcolor = NULL;
static float fcolor[4] = {0.0f};
static bool flat_color;

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
		bcolor = NULL;
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
		bcolor = &btheme->tarm[(color_index - 1)];
	}
	else if (color_index == -1) {
		/* use the group's own custom color set (grp is always != NULL here) */
		bcolor = &grp->cs;
	}
	else {
		bcolor = NULL;
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
static bool set_pchan_color(short colCode, int boneflag, short constflag)
{
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

static void set_ebone_color(const unsigned int boneflag)
{
	if ((boneflag & BONE_DRAW_ACTIVE) && (boneflag & BONE_SELECTED)) {
		UI_GetThemeColor4fv(TH_EDGE_SELECT, fcolor);
	}
	else if (boneflag & BONE_DRAW_ACTIVE) {
		UI_GetThemeColorBlendShade4fv(TH_WIRE_EDIT, TH_EDGE_SELECT, 0.15f, 0, fcolor);
	}
	else if (boneflag & BONE_SELECTED) {
		UI_GetThemeColorShade4fv(TH_EDGE_SELECT, -20, fcolor);
	}
	else {
		UI_GetThemeColor4fv(TH_WIRE_EDIT, fcolor);
	}
}

/* *************** Armature drawing, helper calls for parts ******************* */

static void add_solid_flat_triangle(VertexBuffer *vbo, unsigned int *vertex, unsigned int pos, unsigned int nor,
                                    const float p1[3], const float p2[3], const float p3[3], const float n[3])
{
	VertexBuffer_set_attrib(vbo, nor, *vertex, n);
	VertexBuffer_set_attrib(vbo, pos, (*vertex)++, p1);
	VertexBuffer_set_attrib(vbo, nor, *vertex, n);
	VertexBuffer_set_attrib(vbo, pos, (*vertex)++, p2);
	VertexBuffer_set_attrib(vbo, nor, *vertex, n);
	VertexBuffer_set_attrib(vbo, pos, (*vertex)++, p3);
}

/* half the cube, in Y */
static const float cube_vert[8][3] = {
	{-1.0,  0.0, -1.0},
	{-1.0,  0.0,  1.0},
	{-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0},
	{ 1.0,  0.0, -1.0},
	{ 1.0,  0.0,  1.0},
	{ 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0},
};

static const float cube_wire[24] = {
	0, 1, 1, 2, 2, 3, 3, 0,
	4, 5, 5, 6, 6, 7, 7, 4,
	0, 4, 1, 5, 2, 6, 3, 7,
};

static void drawsolidcube_size(float xsize, float ysize, float zsize)
{
	static VertexFormat format = {0};
	static VertexBuffer vbo = {{0}};
	static Batch batch = {{0}};
	const float light_vec[3] = {0.0f, 0.0f, 1.0f};

	if (format.attrib_ct == 0) {
		unsigned int i = 0;
		float n[3] = {0.0f};
		/* Vertex format */
		unsigned int pos = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
		unsigned int nor = VertexFormat_add_attrib(&format, "nor", COMP_F32, 3, KEEP_FLOAT);

		/* Vertices */
		VertexBuffer_init_with_format(&vbo, &format);
		VertexBuffer_allocate_data(&vbo, 36);

		n[0] = -1.0;
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[0], cube_vert[1], cube_vert[2], n);
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[2], cube_vert[3], cube_vert[0], n);
		n[0] = 0;
		n[1] = -1.0;
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[0], cube_vert[4], cube_vert[5], n);
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[5], cube_vert[1], cube_vert[0], n);
		n[1] = 0;
		n[0] = 1.0;
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[4], cube_vert[7], cube_vert[6], n);
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[6], cube_vert[5], cube_vert[4], n);
		n[0] = 0;
		n[1] = 1.0;
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[7], cube_vert[3], cube_vert[2], n);
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[2], cube_vert[6], cube_vert[7], n);
		n[1] = 0;
		n[2] = 1.0;
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[1], cube_vert[5], cube_vert[6], n);
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[6], cube_vert[2], cube_vert[1], n);
		n[2] = -1.0;
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[7], cube_vert[4], cube_vert[0], n);
		add_solid_flat_triangle(&vbo, &i, pos, nor, cube_vert[0], cube_vert[3], cube_vert[7], n);

		Batch_init(&batch, PRIM_TRIANGLES, &vbo, NULL);
	}

	gpuPushMatrix();
	gpuScale3f(xsize, ysize, zsize);

	if (flat_color) {
		Batch_set_builtin_program(&batch, GPU_SHADER_3D_UNIFORM_COLOR);
	}
	else {
		/* TODO replace with good default lighting shader ? */
		Batch_set_builtin_program(&batch, GPU_SHADER_SIMPLE_LIGHTING);
		Batch_Uniform3fv(&batch, "light", light_vec);
	}
	Batch_Uniform4fv(&batch, "color", fcolor);
	Batch_draw(&batch);

	gpuPopMatrix();
}

static void drawcube_size(float xsize, float ysize, float zsize)
{
	static VertexFormat format = {0};
	static VertexBuffer vbo = {{0}};
	static ElementListBuilder elb = {0};
	static ElementList el = {0};
	static Batch batch = {{0}};

	if (format.attrib_ct == 0) {
		/* Vertex format */
		unsigned int pos = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);

		/* Elements */
		ElementListBuilder_init(&elb, PRIM_LINES, 12, 8);
		for (int i = 0; i < 12; ++i) {
			add_line_vertices(&elb, cube_wire[i*2], cube_wire[i*2+1]);
		}
		ElementList_build_in_place(&elb, &el);

		/* Vertices */
		VertexBuffer_init_with_format(&vbo, &format);
		VertexBuffer_allocate_data(&vbo, 8);
		for (int i = 0; i < 8; ++i) {
			VertexBuffer_set_attrib(&vbo, pos, i, cube_vert[i]);
		}

		Batch_init(&batch, PRIM_LINES, &vbo, &el);
		Batch_set_builtin_program(&batch, GPU_SHADER_3D_UNIFORM_COLOR);
	}

	gpuPushMatrix();
	gpuScale3f(xsize, ysize, zsize);

	Batch_use_program(&batch);
	Batch_Uniform4fv(&batch, "color", fcolor);
	Batch_draw(&batch);

	gpuPopMatrix();
}


static void draw_bonevert(void)
{
	static VertexFormat format = {0};
	static VertexBuffer vbo = {{0}};
	static Batch batch = {{0}};

	if (format.attrib_ct == 0) {
		/* Vertex format */
		unsigned int pos = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);

		/* Vertices */
		VertexBuffer_init_with_format(&vbo, &format);
		VertexBuffer_allocate_data(&vbo, 96);
		for (int i = 0; i < 16; ++i) {
			float vert[3] = {0.f, 0.f, 0.f};
			const float r = 0.05f;

			vert[0] = r * cosf(2 * M_PI * i / 16.f);
			vert[1] = r * sinf(2 * M_PI * i / 16.f);
			VertexBuffer_set_attrib(&vbo, pos, i * 6 + 0, vert);
			vert[0] = r * cosf(2 * M_PI * (i + 1) / 16.f);
			vert[1] = r * sinf(2 * M_PI * (i + 1) / 16.f);
			VertexBuffer_set_attrib(&vbo, pos, i * 6 + 1, vert);

			vert[0] = 0.f;
			vert[1] = r * cosf(2 * M_PI * i / 16.f);
			vert[2] = r * sinf(2 * M_PI * i / 16.f);
			VertexBuffer_set_attrib(&vbo, pos, i * 6 + 2, vert);
			vert[1] = r * cosf(2 * M_PI * (i + 1) / 16.f);
			vert[2] = r * sinf(2 * M_PI * (i + 1) / 16.f);
			VertexBuffer_set_attrib(&vbo, pos, i * 6 + 3, vert);

			vert[1] = 0.f;
			vert[0] = r * cosf(2 * M_PI * i / 16.f);
			vert[2] = r * sinf(2 * M_PI * i / 16.f);
			VertexBuffer_set_attrib(&vbo, pos, i * 6 + 4, vert);
			vert[0] = r * cosf(2 * M_PI * (i + 1) / 16.f);
			vert[2] = r * sinf(2 * M_PI * (i + 1) / 16.f);
			VertexBuffer_set_attrib(&vbo, pos, i * 6 + 5, vert);
		}

		Batch_init(&batch, PRIM_LINES, &vbo, NULL);
		Batch_set_builtin_program(&batch, GPU_SHADER_3D_UNIFORM_COLOR);
	}

	Batch_use_program(&batch);
	Batch_Uniform4fv(&batch, "color", fcolor);
	Batch_draw(&batch);
}

static void draw_bonevert_solid(void)
{
	Batch *batch = Batch_get_sphere(0);
	const float light_vec[3] = {0.0f, 0.0f, 1.0f};

	gpuPushMatrix();
	gpuScaleUniform(0.05);

	if (flat_color) {
		Batch_set_builtin_program(batch, GPU_SHADER_3D_UNIFORM_COLOR);
	}
	else {
		/* TODO replace with good default lighting shader ? */
		Batch_set_builtin_program(batch, GPU_SHADER_SIMPLE_LIGHTING);
		Batch_Uniform3fv(batch, "light", light_vec);
	}
	Batch_Uniform4fv(batch, "color", fcolor);
	Batch_draw(batch);

	gpuPopMatrix();
}

static const float bone_octahedral_verts[6][3] = {
	{ 0.0f, 0.0f,  0.0f},
	{ 0.1f, 0.1f,  0.1f},
	{ 0.1f, 0.1f, -0.1f},
	{-0.1f, 0.1f, -0.1f},
	{-0.1f, 0.1f,  0.1f},
	{ 0.0f, 1.0f,  0.0f}
};

static const unsigned int bone_octahedral_wire[24] = {
	0, 1,  1, 5,  5, 3,  3, 0,
	0, 4,  4, 5,  5, 2,  2, 0,
	1, 2,  2, 3,  3, 4,  4, 1,
};

static const unsigned int bone_octahedral_solid_tris[8][3] = {
	{2, 1, 0}, /* bottom */
	{3, 2, 0},
	{4, 3, 0},
	{1, 4, 0},

	{5, 1, 2}, /* top */
	{5, 2, 3},
	{5, 3, 4},
	{5, 4, 1}
};

/* aligned with bone_octahedral_solid_tris */
static const float bone_octahedral_solid_normals[8][3] = {
	{ M_SQRT1_2,   -M_SQRT1_2,    0.00000000f},
	{-0.00000000f, -M_SQRT1_2,   -M_SQRT1_2},
	{-M_SQRT1_2,   -M_SQRT1_2,    0.00000000f},
	{ 0.00000000f, -M_SQRT1_2,    M_SQRT1_2},
	{ 0.99388373f,  0.11043154f, -0.00000000f},
	{ 0.00000000f,  0.11043154f, -0.99388373f},
	{-0.99388373f,  0.11043154f,  0.00000000f},
	{ 0.00000000f,  0.11043154f,  0.99388373f}
};

static void draw_bone_octahedral(void)
{
	static VertexFormat format = {0};
	static VertexBuffer vbo = {{0}};
	static ElementListBuilder elb = {0};
	static ElementList el = {0};
	static Batch batch = {{0}};

	if (format.attrib_ct == 0) {
		/* Vertex format */
		unsigned int pos = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);

		/* Elements */
		ElementListBuilder_init(&elb, PRIM_LINES, 12, 6);
		for (int i = 0; i < 12; ++i) {
			add_line_vertices(&elb, bone_octahedral_wire[i*2], bone_octahedral_wire[i*2+1]);
		}
		ElementList_build_in_place(&elb, &el);

		/* Vertices */
		VertexBuffer_init_with_format(&vbo, &format);
		VertexBuffer_allocate_data(&vbo, 6);
		for (int i = 0; i < 6; ++i) {
			VertexBuffer_set_attrib(&vbo, pos, i, bone_octahedral_verts[i]);
		}

		Batch_init(&batch, PRIM_LINES, &vbo, &el);
		Batch_set_builtin_program(&batch, GPU_SHADER_3D_UNIFORM_COLOR);
	}

	Batch_use_program(&batch);
	Batch_Uniform4fv(&batch, "color", fcolor);
	Batch_draw(&batch);
}

static void draw_bone_solid_octahedral(void)
{
	static VertexFormat format = {0};
	static VertexBuffer vbo = {{0}};
	static Batch batch = {{0}};
	const float light_vec[3] = {0.0f, 0.0f, 1.0f};

	if (format.attrib_ct == 0) {
		unsigned int v_idx = 0;
		/* Vertex format */
		unsigned int pos = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
		unsigned int nor = VertexFormat_add_attrib(&format, "nor", COMP_F32, 3, KEEP_FLOAT);

		/* Vertices */
		VertexBuffer_init_with_format(&vbo, &format);
		VertexBuffer_allocate_data(&vbo, 24);

		for (int i = 0; i < 8; i++) {
			add_solid_flat_triangle(&vbo, &v_idx, pos, nor,
			                        bone_octahedral_verts[bone_octahedral_solid_tris[i][0]],
			                        bone_octahedral_verts[bone_octahedral_solid_tris[i][1]],
			                        bone_octahedral_verts[bone_octahedral_solid_tris[i][2]],
			                        bone_octahedral_solid_normals[i]);
		}

		Batch_init(&batch, PRIM_TRIANGLES, &vbo, NULL);
	}

	if (flat_color) {
		Batch_set_builtin_program(&batch, GPU_SHADER_3D_UNIFORM_COLOR);
	}
	else {
		/* TODO replace with good default lighting shader ? */
		Batch_set_builtin_program(&batch, GPU_SHADER_SIMPLE_LIGHTING);
		Batch_Uniform3fv(&batch, "light", light_vec);
	}
	Batch_Uniform4fv(&batch, "color", fcolor);
	Batch_draw(&batch);
}

/* *************** Armature drawing, bones ******************* */


static void draw_bone_points(const short dt, int armflag, unsigned int boneflag, int id)
{
	/*	Draw root point if we are not connected */
	if ((boneflag & BONE_CONNECTED) == 0) {
		if (id != -1)
			GPU_select_load_id(id | BONESEL_ROOT);
		
		if (dt <= OB_WIRE) {
			if (armflag & ARM_EDITMODE) {
				if (boneflag & BONE_ROOTSEL) {
					UI_GetThemeColor4fv(TH_VERTEX_SELECT, fcolor);
				}
				else {
					UI_GetThemeColor4fv(TH_VERTEX, fcolor);
				}
			}
		}
		else {
			if (armflag & ARM_POSEMODE) 
				set_pchan_color(PCHAN_COLOR_SOLID, boneflag, 0);
			else {
				UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
			}
		}
		
		if (dt > OB_WIRE) 
			draw_bonevert_solid();
		else 
			draw_bonevert();
	}
	
	/*	Draw tip point */
	if (id != -1)
		GPU_select_load_id(id | BONESEL_TIP);
	
	if (dt <= OB_WIRE) {
		if (armflag & ARM_EDITMODE) {
			if (boneflag & BONE_TIPSEL) {
				UI_GetThemeColor4fv(TH_VERTEX_SELECT, fcolor);
			}
			else {
				UI_GetThemeColor4fv(TH_VERTEX, fcolor);
			}
		}
	}
	else {
		if (armflag & ARM_POSEMODE) 
			set_pchan_color(PCHAN_COLOR_SOLID, boneflag, 0);
		else {
			UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
		}
	}
	
	gpuPushMatrix();
	gpuTranslate2f(0.0f, 1.0f);
	if (dt > OB_WIRE) 
		draw_bonevert_solid();
	else 
		draw_bonevert();
	gpuPopMatrix();
}

/* 16 values of sin function (still same result!) */
static const float si[16] = {
	0.00000000f,
	0.20129852f, 0.39435585f,
	0.57126821f, 0.72479278f,
	0.84864425f, 0.93775213f,
	0.98846832f, 0.99871650f,
	0.96807711f, 0.89780453f,
	0.79077573f, 0.65137248f,
	0.48530196f, 0.29936312f,
	0.10116832f
};
/* 16 values of cos function (still same result!) */
static const float co[16] = {
	1.00000000f,
	0.97952994f, 0.91895781f,
	0.82076344f, 0.68896691f,
	0.52896401f, 0.34730525f,
	0.15142777f, -0.05064916f,
	-0.25065253f, -0.44039415f,
	-0.61210598f, -0.75875812f,
	-0.87434661f, -0.95413925f,
	-0.99486932f
};



/* smat, imat = mat & imat to draw screenaligned */
static void draw_sphere_bone_dist(float smat[4][4], float imat[4][4], bPoseChannel *pchan, EditBone *ebone)
{
	float head, tail, dist /*, length*/;
	float *headvec, *tailvec, dirvec[3];
	
	/* figure out the sizes of spheres */
	if (ebone) {
		/* this routine doesn't call get_matrix_editbone() that calculates it */
		ebone->length = len_v3v3(ebone->head, ebone->tail);

		/*length = ebone->length;*/ /*UNUSED*/
		tail = ebone->rad_tail;
		dist = ebone->dist;
		if (ebone->parent && (ebone->flag & BONE_CONNECTED))
			head = ebone->parent->rad_tail;
		else
			head = ebone->rad_head;
		headvec = ebone->head;
		tailvec = ebone->tail;
	}
	else {
		/*length = pchan->bone->length;*/ /*UNUSED*/
		tail = pchan->bone->rad_tail;
		dist = pchan->bone->dist;
		if (pchan->parent && (pchan->bone->flag & BONE_CONNECTED))
			head = pchan->parent->bone->rad_tail;
		else
			head = pchan->bone->rad_head;
		headvec = pchan->pose_head;
		tailvec = pchan->pose_tail;
	}
	
	/* ***** draw it ***** */
	
	/* move vector to viewspace */
	sub_v3_v3v3(dirvec, tailvec, headvec);
	mul_mat3_m4_v3(smat, dirvec);
	/* clear zcomp */
	dirvec[2] = 0.0f;

	if (head != tail) {
		/* correction when viewing along the bones axis
		 * it pops in and out but better then artifacts, [#23841] */
		float view_dist = len_v2(dirvec);

		if (head - view_dist > tail) {
			tailvec = headvec;
			tail = head;
			zero_v3(dirvec);
			dirvec[0] = 0.00001;  /* XXX. weak but ok */
		}
		else if (tail - view_dist > head) {
			headvec = tailvec;
			head = tail;
			zero_v3(dirvec);
			dirvec[0] = 0.00001;  /* XXX. weak but ok */
		}
	}

	/* move vector back */
	mul_mat3_m4_v3(imat, dirvec);
	
	if (0.0f != normalize_v3(dirvec)) {
		float norvec[3], vec1[3], vec2[3], vec[3];
		int a;
		
		//mul_v3_fl(dirvec, head);
		cross_v3_v3v3(norvec, dirvec, imat[2]);

		VertexFormat *format = immVertexFormat();
		unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immBegin(PRIM_TRIANGLE_STRIP, 66);
		immUniformColor4ub(255, 255, 255, 50);
		
		for (a = 0; a < 16; a++) {
			vec[0] = -si[a] * dirvec[0] + co[a] * norvec[0];
			vec[1] = -si[a] * dirvec[1] + co[a] * norvec[1];
			vec[2] = -si[a] * dirvec[2] + co[a] * norvec[2];

			madd_v3_v3v3fl(vec1, headvec, vec, head);
			madd_v3_v3v3fl(vec2, headvec, vec, head + dist);
			
			immVertex3fv(pos, vec1);
			immVertex3fv(pos, vec2);
		}
		
		for (a = 15; a >= 0; a--) {
			vec[0] = si[a] * dirvec[0] + co[a] * norvec[0];
			vec[1] = si[a] * dirvec[1] + co[a] * norvec[1];
			vec[2] = si[a] * dirvec[2] + co[a] * norvec[2];

			madd_v3_v3v3fl(vec1, tailvec, vec, tail);
			madd_v3_v3v3fl(vec2, tailvec, vec, tail + dist);
			
			immVertex3fv(pos, vec1);
			immVertex3fv(pos, vec2);
		}
		/* make it cyclic... */
		
		vec[0] = -si[0] * dirvec[0] + co[0] * norvec[0];
		vec[1] = -si[0] * dirvec[1] + co[0] * norvec[1];
		vec[2] = -si[0] * dirvec[2] + co[0] * norvec[2];

		madd_v3_v3v3fl(vec1, headvec, vec, head);
		madd_v3_v3v3fl(vec2, headvec, vec, head + dist);

		immVertex3fv(pos, vec1);
		immVertex3fv(pos, vec2);
		
		immEnd();
		immUnbindProgram();
	}
}


/* smat, imat = mat & imat to draw screenaligned */
static void draw_sphere_bone_wire(float smat[4][4], float imat[4][4],
                                  int armflag, int boneflag, short constflag, unsigned int id,
                                  bPoseChannel *pchan, EditBone *ebone)
{
	float head, tail /*, length*/;
	float *headvec, *tailvec, dirvec[3];

	VertexFormat *format = immVertexFormat();
	unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	
	/* figure out the sizes of spheres */
	if (ebone) {
		/* this routine doesn't call get_matrix_editbone() that calculates it */
		ebone->length = len_v3v3(ebone->head, ebone->tail);
		
		/*length = ebone->length;*/ /*UNUSED*/
		tail = ebone->rad_tail;
		if (ebone->parent && (boneflag & BONE_CONNECTED))
			head = ebone->parent->rad_tail;
		else
			head = ebone->rad_head;
		headvec = ebone->head;
		tailvec = ebone->tail;
	}
	else {
		/*length = pchan->bone->length;*/ /*UNUSED*/
		tail = pchan->bone->rad_tail;
		if ((pchan->parent) && (boneflag & BONE_CONNECTED))
			head = pchan->parent->bone->rad_tail;
		else
			head = pchan->bone->rad_head;
		headvec = pchan->pose_head;
		tailvec = pchan->pose_tail;
	}
	
	/* sphere root color */
	if (armflag & ARM_EDITMODE) {
		if (boneflag & BONE_ROOTSEL) {
			UI_GetThemeColor4fv(TH_VERTEX_SELECT, fcolor);
		}
		else {
			UI_GetThemeColor4fv(TH_VERTEX, fcolor);
		}
	}
	else if (armflag & ARM_POSEMODE)
		set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag);

	immUniformColor4fv(fcolor);

	/*	Draw root point if we are not connected */
	if ((boneflag & BONE_CONNECTED) == 0) {
		if (id != -1)
			GPU_select_load_id(id | BONESEL_ROOT);
		
		imm_drawcircball(headvec, head, imat, pos);
	}
	
	/*	Draw tip point */
	if (armflag & ARM_EDITMODE) {
		if (boneflag & BONE_TIPSEL) {
			UI_GetThemeColor4fv(TH_VERTEX_SELECT, fcolor);
		}
		else {
			UI_GetThemeColor4fv(TH_VERTEX, fcolor);
		}
	}
	
	if (id != -1)
		GPU_select_load_id(id | BONESEL_TIP);
	
	imm_drawcircball(tailvec, tail, imat, pos);
	
	/* base */
	if (armflag & ARM_EDITMODE) {
		if (boneflag & BONE_SELECTED){
			UI_GetThemeColor4fv(TH_SELECT, fcolor);
		}
		else {
			UI_GetThemeColor4fv(TH_WIRE_EDIT, fcolor);
		}
	}
	
	sub_v3_v3v3(dirvec, tailvec, headvec);
	
	/* move vector to viewspace */
	mul_mat3_m4_v3(smat, dirvec);
	/* clear zcomp */
	dirvec[2] = 0.0f;
	/* move vector back */
	mul_mat3_m4_v3(imat, dirvec);
	
	if (0.0f != normalize_v3(dirvec)) {
		float norvech[3], norvect[3], vec[3];
		
		copy_v3_v3(vec, dirvec);
		
		mul_v3_fl(dirvec, head);
		cross_v3_v3v3(norvech, dirvec, imat[2]);
		
		mul_v3_fl(vec, tail);
		cross_v3_v3v3(norvect, vec, imat[2]);
		
		if (id != -1)
			GPU_select_load_id(id | BONESEL_BONE);
		
		immBegin(PRIM_LINES, 4);

		add_v3_v3v3(vec, headvec, norvech);
		immVertex3fv(pos, vec);

		add_v3_v3v3(vec, tailvec, norvect);
		immVertex3fv(pos, vec);

		sub_v3_v3v3(vec, headvec, norvech);
		immVertex3fv(pos, vec);

		sub_v3_v3v3(vec, tailvec, norvect);
		immVertex3fv(pos, vec);
		
		immEnd();
	}

	immUnbindProgram();
}

/* does wire only for outline selecting */
static void draw_sphere_bone(const short dt, int armflag, int boneflag, short constflag, unsigned int id,
                             bPoseChannel *pchan, EditBone *ebone)
{
	Batch *sphere = Batch_get_sphere(1);
	float head, tail, length;
	float fac1, fac2, size1, size2;
	const float light_vec[3] = {0.0f, 0.0f, 1.0f};

	/* dt is always OB_SOlID */
	Batch_set_builtin_program(sphere, GPU_SHADER_SIMPLE_LIGHTING);
	Batch_Uniform3fv(sphere, "light", light_vec);

	gpuPushMatrix();

	/* figure out the sizes of spheres */
	if (ebone) {
		length = ebone->length;
		tail = ebone->rad_tail;
		if (ebone->parent && (boneflag & BONE_CONNECTED))
			head = ebone->parent->rad_tail;
		else
			head = ebone->rad_head;
	}
	else {
		length = pchan->bone->length;
		tail = pchan->bone->rad_tail;
		if (pchan->parent && (boneflag & BONE_CONNECTED))
			head = pchan->parent->bone->rad_tail;
		else
			head = pchan->bone->rad_head;
	}
	
	/* move to z-axis space */
	gpuRotateAxis(-90.0f, 'X');

	/* sphere root color */
	if (armflag & ARM_EDITMODE) {
		if (boneflag & BONE_ROOTSEL)
			UI_GetThemeColor4fv(TH_VERTEX_SELECT, fcolor);
		else
			UI_GetThemeColorShade4fv(TH_BONE_SOLID, -30, fcolor);
	}
	else if (armflag & ARM_POSEMODE)
		set_pchan_color(PCHAN_COLOR_SPHEREBONE_END, boneflag, constflag);
	else if (dt == OB_SOLID)
		UI_GetThemeColorShade4fv(TH_BONE_SOLID, -30, fcolor);
	
	/*	Draw root point if we are not connected */
	if ((boneflag & BONE_CONNECTED) == 0) {
		if (id != -1)
			GPU_select_load_id(id | BONESEL_ROOT);
		gpuPushMatrix();
		gpuScaleUniform(head);
		Batch_Uniform4fv(sphere, "color", fcolor);
		Batch_draw(sphere);
		gpuPopMatrix();
	}
	
	/*	Draw tip point */
	if (armflag & ARM_EDITMODE) {
		if (boneflag & BONE_TIPSEL) UI_GetThemeColor4fv(TH_VERTEX_SELECT, fcolor);
		else UI_GetThemeColorShade4fv(TH_BONE_SOLID, -30, fcolor);
	}

	if (id != -1)
		GPU_select_load_id(id | BONESEL_TIP);
	
	gpuTranslate3f(0.0f, 0.0f, length);

	gpuPushMatrix();
	gpuScaleUniform(tail);
	Batch_use_program(sphere); /* hack to make the following uniforms stick */
	Batch_Uniform4fv(sphere, "color", fcolor);
	Batch_draw(sphere);
	gpuPopMatrix();

	gpuTranslate3f(0.0f, 0.0f, -length);
	
	/* base */
	if (armflag & ARM_EDITMODE) {
		if (boneflag & BONE_SELECTED) UI_GetThemeColor4fv(TH_SELECT, fcolor);
		else UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
	}
	else if (armflag & ARM_POSEMODE)
		set_pchan_color(PCHAN_COLOR_SPHEREBONE_BASE, boneflag, constflag);
	else if (dt == OB_SOLID)
		UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);

	Batch_use_program(sphere); /* hack to make the following uniforms stick */
	Batch_Uniform4fv(sphere, "color", fcolor);
	
	fac1 = (length - head) / length;
	fac2 = (length - tail) / length;
	
	if (length > (head + tail)) {
		size1 = fac2 * tail + (1.0f - fac2) * head;
		size2 = fac1 * head + (1.0f - fac1) * tail;

		if (id != -1)
			GPU_select_load_id(id | BONESEL_BONE);
		
		/* draw sphere on extrema */
		gpuPushMatrix();
		gpuTranslate3f(0.0f, 0.0f, length - tail);
		gpuScaleUniform(size1);

		Batch_draw(sphere);
		gpuPopMatrix();

		gpuPushMatrix();
		gpuTranslate3f(0.0f, 0.0f, head);
		gpuScaleUniform(size2);

		Batch_draw(sphere);
		gpuPopMatrix();

		/* draw cynlinder between spheres */
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -1.0f);

		VertexFormat *format = immVertexFormat();
		unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);
		unsigned int nor = VertexFormat_add_attrib(format, "nor", COMP_F32, 3, KEEP_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_SIMPLE_LIGHTING);
		immUniformColor4fv(fcolor);
		immUniform3fv("light", light_vec);

		gpuTranslate3f(0.0f, 0.0f, head);
		imm_draw_cylinder_fill_normal_3d(pos, nor, size2, size1, length - head - tail, 16, 1);

		immUnbindProgram();
		
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
	else {
		size1 = fac1 * head + (1.0f - fac1) * tail;

		/* 1 sphere in center */
		gpuTranslate3f(0.0f, 0.0f, (head + length - tail) / 2.0f);

		gpuScaleUniform(size1);
		Batch_draw(sphere);
	}
	
	gpuPopMatrix();
}

static void draw_line_bone(int armflag, int boneflag, short constflag, unsigned int id,
                           bPoseChannel *pchan, EditBone *ebone)
{
	float length;
	
	if (pchan) 
		length = pchan->bone->length;
	else 
		length = ebone->length;
	
	VertexFormat *format = immVertexFormat();
	unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);

	gpuPushMatrix();
	gpuScaleUniform(length);
	
	/* this chunk not in object mode */
	if (armflag & (ARM_EDITMODE | ARM_POSEMODE)) {
		glLineWidth(4.0f);
		glPointSize(8.0f);

		if (armflag & ARM_POSEMODE)
			set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag);
		else if (armflag & ARM_EDITMODE) {
			UI_GetThemeColor4fv(TH_WIRE_EDIT, fcolor);
		}

		/* line */
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor4fv(fcolor);

		if (id != -1)
			GPU_select_load_id(id | BONESEL_BONE);

		immBegin(PRIM_LINES, 2);
		immVertex3f(pos, 0.0f, 1.0f, 0.0f);
		immVertex3f(pos, 0.0f, 0.0f, 0.0f);
		immEnd();

		immUnbindProgram();

		immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_UNIFORM_COLOR);
		immUniformColor4fv(fcolor);

		/*	Draw root point if we are not connected */
		if ((boneflag & BONE_CONNECTED) == 0) {
			if (G.f & G_PICKSEL)
				GPU_select_load_id(id | BONESEL_ROOT);

			immBegin(PRIM_POINTS, 1);
			immVertex3f(pos, 0.0f, 0.0f, 0.0f);
			immEnd();
		}

		/* tip */
		if (G.f & G_PICKSEL)
			GPU_select_load_id(id | BONESEL_TIP);

		immBegin(PRIM_POINTS, 1);
		immVertex3f(pos, 0.0f, 1.0f, 0.0f);
		immEnd();

		immUnbindProgram();


		/* further we send no names */
		if (id != -1)
			GPU_select_load_id(id & 0xFFFF);  /* object tag, for bordersel optim */
		
		if (armflag & ARM_POSEMODE)
			set_pchan_color(PCHAN_COLOR_LINEBONE, boneflag, constflag);
	}

	/* Now draw the inner color */
	glLineWidth(2.0f);
	glPointSize(5.0f);

	/* line */
	if (armflag & ARM_EDITMODE) {
		if (boneflag & BONE_SELECTED) UI_GetThemeColor4fv(TH_EDGE_SELECT, fcolor);
		else UI_GetThemeColorShade4fv(TH_BACK, -30, fcolor);
	}

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor4fv(fcolor);

	immBegin(PRIM_LINES, 2);
	immVertex3f(pos, 0.0f, 1.0f, 0.0f);
	immVertex3f(pos, 0.0f, 0.0f, 0.0f);
	immEnd();

	immUnbindProgram();

	immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_UNIFORM_COLOR);

	/*Draw root point if we are not connected */
	if ((boneflag & BONE_CONNECTED) == 0) {
		if (armflag & ARM_EDITMODE) {
			if (boneflag & BONE_ROOTSEL) UI_GetThemeColor4fv(TH_VERTEX_SELECT, fcolor);
			else UI_GetThemeColor4fv(TH_VERTEX, fcolor);
		}
		immUniformColor4fv(fcolor);
		immBegin(PRIM_POINTS, 1);
		immVertex3f(pos, 0.0f, 0.0f, 0.0f);
		immEnd();
	}

	/* tip */
	if ((G.f & G_PICKSEL) == 0) {
		/* no bitmap in selection mode, crashes 3d cards... */
		if (armflag & ARM_EDITMODE) {
			if (boneflag & BONE_TIPSEL) UI_GetThemeColor4fv(TH_VERTEX_SELECT, fcolor);
			else UI_GetThemeColor4fv(TH_VERTEX, fcolor);
		}
		immUniformColor4fv(fcolor);
		immBegin(PRIM_POINTS, 1);
		immVertex3f(pos, 0.0f, 1.0f, 0.0f);
		immEnd();
	}

	immUnbindProgram();

	gpuPopMatrix();
}

/* A partial copy of b_bone_spline_setup(), with just the parts for previewing editmode curve settings 
 *
 * This assumes that prev/next bones don't have any impact (since they should all still be in the "straight"
 * position here anyway), and that we can simply apply the bbone settings to get the desired effect...
 */
static void ebone_spline_preview(EditBone *ebone, Mat4 result_array[MAX_BBONE_SUBDIV])
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
		
		copy_m4_m3(result_array[a].mat, mat3);
		copy_v3_v3(result_array[a].mat[3], fp);
		
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
			mul_m4_series(result_array[a].mat, result_array[a].mat, bscalemat);
		}
	}
}

static void draw_b_bone_boxes(const short dt, bPoseChannel *pchan, EditBone *ebone, float xwidth, float length, float zwidth)
{
	int segments = 0;
	
	if (pchan) 
		segments = pchan->bone->segments;
	else if (ebone)
		segments = ebone->segments;
	
	if (segments > 1) {
		float dlen = length / (float)segments;
		Mat4 bbone[MAX_BBONE_SUBDIV];
		int a;
		
		if (pchan) {
			b_bone_spline_setup(pchan, 0, bbone);
		}
		else if (ebone) {
			ebone_spline_preview(ebone, bbone);
		}
		
		for (a = 0; a < segments; a++) {
			gpuPushMatrix();
			gpuMultMatrix3D(bbone[a].mat);
			if (dt == OB_SOLID) drawsolidcube_size(xwidth, dlen, zwidth);
			else drawcube_size(xwidth, dlen, zwidth);
			gpuPopMatrix();
		}
	}
	else {
		if (dt == OB_SOLID) drawsolidcube_size(xwidth, length, zwidth);
		else drawcube_size(xwidth, length, zwidth);
	}
}

static void draw_b_bone(const short dt, int armflag, int boneflag, short constflag, unsigned int id,
                        bPoseChannel *pchan, EditBone *ebone)
{
	float xwidth, length, zwidth;
	
	if (pchan) {
		xwidth = pchan->bone->xwidth;
		length = pchan->bone->length;
		zwidth = pchan->bone->zwidth;
	}
	else {
		xwidth = ebone->xwidth;
		length = ebone->length;
		zwidth = ebone->zwidth;
	}
	
	/* draw points only if... */
	if (armflag & ARM_EDITMODE) {
		/* move to unitspace */
		gpuPushMatrix();
		gpuScaleUniform(length);
		draw_bone_points(dt, armflag, boneflag, id);
		gpuPopMatrix();
		length *= 0.95f;  /* make vertices visible */
	}

	/* colors for modes */
	if (armflag & ARM_POSEMODE) {
		if (dt <= OB_WIRE)
			set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag);
		else 
			set_pchan_color(PCHAN_COLOR_SOLID, boneflag, constflag);
	}
	else if (armflag & ARM_EDITMODE) {
		if (dt == OB_WIRE) {
			set_ebone_color(boneflag);
		}
		else {
			UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
		}
	}
	
	if (id != -1) {
		GPU_select_load_id((GLuint) id | BONESEL_BONE);
	}
	
	/* set up solid drawing */
	if (dt > OB_WIRE) {
		if (armflag & ARM_POSEMODE)
			set_pchan_color(PCHAN_COLOR_SOLID, boneflag, constflag);
		else {
			UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
		}
		
		flat_color = false;
		draw_b_bone_boxes(OB_SOLID, pchan, ebone, xwidth, length, zwidth);
	}
	else {
		/* wire */
		if (armflag & ARM_POSEMODE) {
			if (constflag && ((G.f & G_PICKSEL) == 0)) {
				/* set constraint colors */
				if (set_pchan_color(PCHAN_COLOR_CONSTS, boneflag, constflag)) {
					glEnable(GL_BLEND);
					
					flat_color = true;
					draw_b_bone_boxes(OB_SOLID, pchan, ebone, xwidth, length, zwidth);
					
					glDisable(GL_BLEND);
				}
				
				/* restore colors */
				set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag);
			}
		}
		
		draw_b_bone_boxes(OB_WIRE, pchan, ebone, xwidth, length, zwidth);
	}
}

static void draw_wire_bone_segments(bPoseChannel *pchan, Mat4 *bbones, float length, int segments)
{
	VertexFormat *format = immVertexFormat();
	unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor4fv(fcolor);

	if ((segments > 1) && (pchan)) {
		float dlen = length / (float)segments;
		Mat4 *bbone = bbones;
		int a;

		for (a = 0; a < segments; a++, bbone++) {
			gpuPushMatrix();
			gpuMultMatrix3D(bbone->mat);

			immBegin(PRIM_LINES, 2);
			immVertex3f(pos, 0.0f, 0.0f, 0.0f);
			immVertex3f(pos, 0.0f, dlen, 0.0f);
			immEnd();

			gpuPopMatrix();
		}
	}
	else {
		gpuPushMatrix();

		immBegin(PRIM_LINES, 2);
		immVertex3f(pos, 0.0f, 0.0f, 0.0f);
		immVertex3f(pos, 0.0f, length, 0.0f);
		immEnd();

		gpuPopMatrix();
	}

	immUnbindProgram();
}

static void draw_wire_bone(const short dt, int armflag, int boneflag, short constflag, unsigned int id,
                           bPoseChannel *pchan, EditBone *ebone)
{
	Mat4 bbones_array[MAX_BBONE_SUBDIV];
	Mat4 *bbones = NULL;
	int segments = 0;
	float length;
	
	if (pchan) {
		segments = pchan->bone->segments;
		length = pchan->bone->length;
		
		if (segments > 1) {
			b_bone_spline_setup(pchan, 0, bbones_array);
			bbones = bbones_array;
		}
	}
	else 
		length = ebone->length;
	
	/* draw points only if... */
	if (armflag & ARM_EDITMODE) {
		/* move to unitspace */
		gpuPushMatrix();
		gpuScaleUniform(length);
		flat_color = true;
		draw_bone_points(dt, armflag, boneflag, id);
		gpuPopMatrix();
		length *= 0.95f;  /* make vertices visible */
	}
	
	/* this chunk not in object mode */
	if (armflag & (ARM_EDITMODE | ARM_POSEMODE)) {
		if (id != -1)
			GPU_select_load_id((GLuint) id | BONESEL_BONE);
		
		draw_wire_bone_segments(pchan, bbones, length, segments);
		
		/* further we send no names */
		if (id != -1)
			GPU_select_load_id(id & 0xFFFF);    /* object tag, for bordersel optim */
	}
	
	/* colors for modes */
	if (armflag & ARM_POSEMODE) {
		set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag);
	}
	else if (armflag & ARM_EDITMODE) {
		set_ebone_color(boneflag);
	}
	
	/* draw normal */
	draw_wire_bone_segments(pchan, bbones, length, segments);
}

static void draw_bone(const short dt, int armflag, int boneflag, short constflag, unsigned int id, float length)
{
	
	/* Draw a 3d octahedral bone, we use normalized space based on length */
	gpuScaleUniform(length);

	/* set up solid drawing */
	if (dt > OB_WIRE) {
		UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);
		flat_color = false;
	}
	else
		flat_color = true;
	
	/* colors for posemode */
	if (armflag & ARM_POSEMODE) {
		if (dt <= OB_WIRE)
			set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag);
		else 
			set_pchan_color(PCHAN_COLOR_SOLID, boneflag, constflag);
	}
	
	
	draw_bone_points(dt, armflag, boneflag, id);
	
	/* now draw the bone itself */
	if (id != -1) {
		GPU_select_load_id((GLuint) id | BONESEL_BONE);
	}
	
	/* wire? */
	if (dt <= OB_WIRE) {
		/* colors */
		if (armflag & ARM_EDITMODE) {
			set_ebone_color(boneflag);
		}
		else if (armflag & ARM_POSEMODE) {
			if (constflag && ((G.f & G_PICKSEL) == 0)) {
				/* draw constraint colors */
				if (set_pchan_color(PCHAN_COLOR_CONSTS, boneflag, constflag)) {
					glEnable(GL_BLEND);
					
					draw_bone_solid_octahedral();
					
					glDisable(GL_BLEND);
				}
				
				/* restore colors */
				set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, constflag);
			}
		}
		draw_bone_octahedral();
	}
	else {
		/* solid */
		if (armflag & ARM_POSEMODE)
			set_pchan_color(PCHAN_COLOR_SOLID, boneflag, constflag);
		else
			UI_GetThemeColor4fv(TH_BONE_SOLID, fcolor);

		draw_bone_solid_octahedral();
	}
}

static void draw_custom_bone(Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d, Object *ob,
                             const short dt, int armflag, int boneflag, unsigned int id, float length)
{
	if (ob == NULL) return;
	
	gpuScaleUniform(length);
	
	/* colors for posemode */
	if (armflag & ARM_POSEMODE) {
		set_pchan_color(PCHAN_COLOR_NORMAL, boneflag, 0);
	}
	
	if (id != -1) {
		GPU_select_load_id((GLuint) id | BONESEL_BONE);
	}

	draw_object_instance(scene, sl, v3d, rv3d, ob, dt, armflag & ARM_POSEMODE, fcolor);
}


static void pchan_draw_IK_root_lines(bPoseChannel *pchan, short only_temp)
{
	bConstraint *con;
	bPoseChannel *parchan;

	VertexFormat *format = immVertexFormat();
	unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor4fv(fcolor);

	setlinestyle(3);
	for (con = pchan->constraints.first; con; con = con->next) {
		if (con->enforce == 0.0f)
			continue;
		
		switch (con->type) {
			case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data = (bKinematicConstraint *)con->data;
				int segcount = 0;
				float ik_tip[3];
				
				/* if only_temp, only draw if it is a temporary ik-chain */
				if ((only_temp) && !(data->flag & CONSTRAINT_IK_TEMP))
					continue;

				/* exclude tip from chain? */
				if ((data->flag & CONSTRAINT_IK_TIP) == 0)
					parchan = pchan->parent;
				else
					parchan = pchan;
				
				copy_v3_v3(ik_tip, parchan->pose_tail);
				
				/* Find the chain's root */
				while (parchan->parent) {
					segcount++;
					/* FIXME: revise the breaking conditions */
					if (segcount == data->rootbone || segcount > 255) break;  /* 255 is weak */
					parchan = parchan->parent;
				}

				if (parchan) {
					immBegin(PRIM_LINES, 2);
					immVertex3fv(pos, ik_tip);
					immVertex3fv(pos, parchan->pose_head);
					immEnd();
				}
				
				break;
			}
			case CONSTRAINT_TYPE_SPLINEIK: 
			{
				bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
				int segcount = 0;
				float ik_tip[3];
				
				parchan = pchan;
				copy_v3_v3(ik_tip, parchan->pose_tail);
				
				/* Find the chain's root */
				while (parchan->parent) {
					segcount++;
					/* FIXME: revise the breaking conditions */
					if (segcount == data->chainlen || segcount > 255) break;  /* 255 is weak */
					parchan = parchan->parent;
				}
				/* Only draw line in case our chain is more than one bone long! */
				if (parchan != pchan) { /* XXX revise the breaking conditions to only stop at the tail? */
					immBegin(PRIM_LINES, 2);
					immVertex3fv(pos, ik_tip);
					immVertex3fv(pos, parchan->pose_head);
					immEnd();
				}
				break;
			}
		}
	}
	setlinestyle(0);
	immUnbindProgram();
}

static void imm_sphere_project(unsigned int pos, float ax, float az)
{
	float dir[3], sine, q3;

	sine = 1.0f - ax * ax - az * az;
	q3 = (sine < 0.0f) ? 0.0f : (2.0f * sqrtf(sine));

	dir[0] = -az * q3;
	dir[1] = 1.0f - 2.0f * sine;
	dir[2] = ax * q3;

	immVertex3fv(pos, dir);
}

static void draw_dof_ellipse(unsigned int pos, float ax, float az)
{
	const int n = 16;
	const int tri = n*n - 2*n + 1; /* Yay fancy math ! */
	const float staticSine[16] = {
		0.0f, 0.104528463268f, 0.207911690818f, 0.309016994375f,
		0.406736643076f, 0.5f, 0.587785252292f, 0.669130606359f,
		0.743144825477f, 0.809016994375f, 0.866025403784f,
		0.913545457643f, 0.951056516295f, 0.978147600734f,
		0.994521895368f, 1.0f
	};

	int i, j;
	float x, z, px, pz;

	glEnable(GL_BLEND);
	glDepthMask(0);

	immUniformColor4ub(70, 70, 70, 50);

	immBegin(PRIM_TRIANGLES, tri*3);
	pz = 0.0f;
	for (i = 1; i < n; i++) {
		z = staticSine[i];
		
		px = 0.0f;
		for (j = 1; j <= (n - i); j++) {
			x = staticSine[j];
			
			if (j == n - i) {
				imm_sphere_project(pos, ax * px, az * z);
				imm_sphere_project(pos, ax * px, az * pz);
				imm_sphere_project(pos, ax * x, az * pz);
			}
			else {
				imm_sphere_project(pos, ax * x, az * z);
				imm_sphere_project(pos, ax * x, az * pz);
				imm_sphere_project(pos, ax * px, az * pz);

				imm_sphere_project(pos, ax * px, az * pz);
				imm_sphere_project(pos, ax * px, az * z);
				imm_sphere_project(pos, ax * x, az * z);
			}
			
			px = x;
		}
		pz = z;
	}
	immEnd();

	glDisable(GL_BLEND);
	glDepthMask(1);

	immUniformColor3ub(0, 0, 0);

	immBegin(PRIM_LINE_STRIP, n);
	for (i = 0; i < n; i++)
		imm_sphere_project(pos, staticSine[n - i - 1] * ax, staticSine[i] * az);
	immEnd();
}

static void draw_pose_dofs(Object *ob)
{
	bArmature *arm = ob->data;
	bPoseChannel *pchan;
	Bone *bone;

	VertexFormat *format = immVertexFormat();
	unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		bone = pchan->bone;
		
		if ((bone != NULL) && !(bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
			if (bone->flag & BONE_SELECTED) {
				if (bone->layer & arm->layer) {
					if (pchan->ikflag & (BONE_IK_XLIMIT | BONE_IK_ZLIMIT)) {
						if (BKE_pose_channel_in_IK_chain(ob, pchan)) {
							float corner[4][3], posetrans[3], mat[4][4];
							float phi = 0.0f, theta = 0.0f, scale;
							int a, i;
							
							/* in parent-bone pose, but own restspace */
							gpuPushMatrix();
							
							copy_v3_v3(posetrans, pchan->pose_mat[3]);
							gpuTranslate3fv(posetrans);
							
							if (pchan->parent) {
								copy_m4_m4(mat, pchan->parent->pose_mat);
								mat[3][0] = mat[3][1] = mat[3][2] = 0.0f;
								gpuMultMatrix3D(mat);
							}
							
							copy_m4_m3(mat, pchan->bone->bone_mat);
							gpuMultMatrix3D(mat);
							
							scale = bone->length * pchan->size[1];
							gpuScaleUniform(scale);
							
							if (((pchan->ikflag & BONE_IK_XLIMIT) != 0) &&
							    ((pchan->ikflag & BONE_IK_ZLIMIT) != 0))
							{
								float amin[3], amax[3];

								for (i = 0; i < 3; i++) {
									/* *0.5f here comes from M_PI/360.0f when rotations were still in degrees */
									amin[i] = sinf(pchan->limitmin[i] * 0.5f);
									amax[i] = sinf(pchan->limitmax[i] * 0.5f);
								}

								gpuScale3f(1.0f, -1.0f, 1.0f);
								if ((amin[0] != 0.0f) && (amin[2] != 0.0f))
									draw_dof_ellipse(pos, amin[0], amin[2]);
								if ((amin[0] != 0.0f) && (amax[2] != 0.0f))
									draw_dof_ellipse(pos, amin[0], amax[2]);
								if ((amax[0] != 0.0f) && (amin[2] != 0.0f))
									draw_dof_ellipse(pos, amax[0], amin[2]);
								if ((amax[0] != 0.0f) && (amax[2] != 0.0f))
									draw_dof_ellipse(pos, amax[0], amax[2]);
								gpuScale3f(1.0f, -1.0f, 1.0f); /* XXX same as above, is this intentional? */
							}
							
							/* arcs */
							if (pchan->ikflag & BONE_IK_ZLIMIT) {
								/* OpenGL requires rotations in degrees; so we're taking the average angle here */
								theta = RAD2DEGF(0.5f * (pchan->limitmin[2] + pchan->limitmax[2]));
								gpuPushMatrix();
								gpuRotateAxis(theta, 'Z');
								
								immUniformColor3ub(50, 50, 255);  /* blue, Z axis limit */
								immBegin(PRIM_LINE_STRIP, 33);
								for (a = -16; a <= 16; a++) {
									/* *0.5f here comes from M_PI/360.0f when rotations were still in degrees */
									float fac = ((float)a) / 16.0f * 0.5f;
									
									phi = fac * (pchan->limitmax[2] - pchan->limitmin[2]);
									
									i = (a == -16) ? 0 : 1;
									corner[i][0] = sinf(phi);
									corner[i][1] = cosf(phi);
									corner[i][2] = 0.0f;
									immVertex3fv(pos, corner[i]);
								}
								immEnd();
								
								gpuPopMatrix();
							}
							
							if (pchan->ikflag & BONE_IK_XLIMIT) {
								/* OpenGL requires rotations in degrees; so we're taking the average angle here */
								theta = RAD2DEGF(0.5f * (pchan->limitmin[0] + pchan->limitmax[0]));
								gpuPushMatrix();
								gpuRotateAxis(theta, 'X');
								
								immUniformColor3ub(255, 50, 50);  /* Red, X axis limit */
								immBegin(PRIM_LINE_STRIP, 33);
								for (a = -16; a <= 16; a++) {
									/* *0.5f here comes from M_PI/360.0f when rotations were still in degrees */
									float fac = ((float)a) / 16.0f * 0.5f;
									phi = (float)M_PI_2 + fac * (pchan->limitmax[0] - pchan->limitmin[0]);
									
									i = (a == -16) ? 2 : 3;
									corner[i][0] = 0.0f;
									corner[i][1] = sinf(phi);
									corner[i][2] = cosf(phi);
									immVertex3fv(pos, corner[i]);
								}
								immEnd();
								
								gpuPopMatrix();
							}
							
							/* out of cone, out of bone */
							gpuPopMatrix();
						}
					}
				}
			}
		}
	}

	immUnbindProgram();
}

static void bone_matrix_translate_y(float mat[4][4], float y)
{
	float trans[3];

	copy_v3_v3(trans, mat[1]);
	mul_v3_fl(trans, y);
	add_v3_v3(mat[3], trans);
}

/* assumes object is Armature with pose */
static void draw_pose_bones(Scene *scene, SceneLayer *sl, View3D *v3d, ARegion *ar, Base *base,
                            const short dt, const unsigned char ob_wire_col[4],
                            const bool do_const_color, const bool is_outline)
{
	RegionView3D *rv3d = ar->regiondata;
	Object *ob = base->object;
	bArmature *arm = ob->data;
	bPoseChannel *pchan;
	Bone *bone;
	float smat[4][4], imat[4][4], bmat[4][4];
	int index = -1;
	const enum {
		DASH_RELATIONSHIP_LINES = 1,
		DASH_HELP_LINES = 2,
	} do_dashed = (
	        (is_outline ? 0 : DASH_RELATIONSHIP_LINES) |
	        ((v3d->flag & V3D_HIDE_HELPLINES) ? 0 : DASH_HELP_LINES));
	bool draw_wire = false;
	int flag;
	bool is_cull_enabled;
	
	/* being set below */
	arm->layer_used = 0;

	rgba_uchar_to_float(fcolor, ob_wire_col);

	/* precalc inverse matrix for drawing screen aligned */
	if (arm->drawtype == ARM_ENVELOPE) {
		/* precalc inverse matrix for drawing screen aligned */
		copy_m4_m4(smat, rv3d->viewmatob);
		mul_mat3_m4_fl(smat, 1.0f / len_v3(ob->obmat[0]));
		invert_m4_m4(imat, smat);
		
		/* and draw blended distances */
		if (arm->flag & ARM_POSEMODE) {
			glEnable(GL_BLEND);
			
			if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
			
			for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				bone = pchan->bone;
				if (bone) {
					/* 1) bone must be visible, 2) for OpenGL select-drawing cannot have unselectable [#27194] 
					 * NOTE: this is the only case with (NO_DEFORM == 0) flag, as this is for envelope influence drawing
					 */
					if (((bone->flag & (BONE_HIDDEN_P | BONE_NO_DEFORM | BONE_HIDDEN_PG)) == 0) &&
					    ((G.f & G_PICKSEL) == 0 || (bone->flag & BONE_UNSELECTABLE) == 0))
					{
						if (bone->flag & (BONE_SELECTED)) {
							if (bone->layer & arm->layer)
								draw_sphere_bone_dist(smat, imat, pchan, NULL);
						}
					}
				}
			}
			
			if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
		}
	}
	
	/* little speedup, also make sure transparent only draws once */
	glCullFace(GL_BACK);
	if (v3d->flag2 & V3D_BACKFACE_CULLING) {
		glEnable(GL_CULL_FACE);
		is_cull_enabled = true;
	}
	else {
		is_cull_enabled = false;
	}

	/* if solid we draw that first, with selection codes, but without names, axes etc */
	if (dt > OB_WIRE) {
		if (arm->flag & ARM_POSEMODE) 
			index = base->selcol;
		
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			bone = pchan->bone;
			arm->layer_used |= bone->layer;
			
			/* 1) bone must be visible, 2) for OpenGL select-drawing cannot have unselectable [#27194] */
			if (((bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0) &&
			    ((G.f & G_PICKSEL) == 0 || (bone->flag & BONE_UNSELECTABLE) == 0))
			{
				if (bone->layer & arm->layer) {
					const bool use_custom = (pchan->custom) && !(arm->flag & ARM_NO_CUSTOM);
					gpuPushMatrix();
					
					if (use_custom && pchan->custom_tx) {
						gpuMultMatrix3D(pchan->custom_tx->pose_mat);
					}
					else {
						gpuMultMatrix3D(pchan->pose_mat);
					}
					
					/* catch exception for bone with hidden parent */
					flag = bone->flag;
					if ((bone->parent) && (bone->parent->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
						flag &= ~BONE_CONNECTED;
					}
					
					/* set temporary flag for drawing bone as active, but only if selected */
					if (bone == arm->act_bone)
						flag |= BONE_DRAW_ACTIVE;
					
					if (do_const_color) {
						/* keep color */
					}
					else {
						/* set color-set to use */
						set_pchan_colorset(ob, pchan);
					}

					/* may be 2x width from custom bone's outline option */
					glLineWidth(1.0f);

					if (use_custom) {
						/* if drawwire, don't try to draw in solid */
						if (pchan->bone->flag & BONE_DRAWWIRE) {
							draw_wire = true;
						}
						else {
							if (is_cull_enabled && (v3d->flag2 & V3D_BACKFACE_CULLING) == 0) {
								is_cull_enabled = false;
								glDisable(GL_CULL_FACE);
							}

							draw_custom_bone(scene, sl, v3d, rv3d, pchan->custom,
							                 OB_SOLID, arm->flag, flag, index, PCHAN_CUSTOM_DRAW_SIZE(pchan));
						}
					}
					else {
						if (is_cull_enabled == false) {
							is_cull_enabled = true;
							glEnable(GL_CULL_FACE);
						}

						if (arm->drawtype == ARM_LINE) {
							/* nothing in solid */
						}
						else if (arm->drawtype == ARM_WIRE) {
							/* nothing in solid */
						}
						else if (arm->drawtype == ARM_ENVELOPE) {
							draw_sphere_bone(OB_SOLID, arm->flag, flag, 0, index, pchan, NULL);
						}
						else if (arm->drawtype == ARM_B_BONE) {
							draw_b_bone(OB_SOLID, arm->flag, flag, 0, index, pchan, NULL);
						}
						else {
							draw_bone(OB_SOLID, arm->flag, flag, 0, index, bone->length);
						}
					}

					gpuPopMatrix();
				}
			}
			
			if (index != -1)
				index += 0x10000;  /* pose bones count in higher 2 bytes only */
		}
		
		/* very very confusing... but in object mode, solid draw, we cannot do GPU_select_load_id yet,
		 * stick bones and/or wire custom-shapes are drawn in next loop 
		 */
		if (ELEM(arm->drawtype, ARM_LINE, ARM_WIRE) == 0 && (draw_wire == false) && index != -1) {
			/* object tag, for bordersel optim */
			GPU_select_load_id(index & 0xFFFF);
			index = -1;
		}
	}
	
	/* draw custom bone shapes as wireframes */
	if (!(arm->flag & ARM_NO_CUSTOM) &&
	    (draw_wire || (dt <= OB_WIRE)) )
	{
		if (arm->flag & ARM_POSEMODE)
			index = base->selcol;
			
		/* only draw custom bone shapes that need to be drawn as wires */
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			bone = pchan->bone;
			
			/* 1) bone must be visible, 2) for OpenGL select-drawing cannot have unselectable [#27194] */
			if (((bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0) &&
			    ((G.f & G_PICKSEL) == 0 || (bone->flag & BONE_UNSELECTABLE) == 0) )
			{
				if (bone->layer & arm->layer) {
					if (pchan->custom) {
						if ((dt < OB_SOLID) || (bone->flag & BONE_DRAWWIRE)) {
							gpuPushMatrix();
							
							if (pchan->custom_tx) {
								gpuMultMatrix3D(pchan->custom_tx->pose_mat);
							}
							else {
								gpuMultMatrix3D(pchan->pose_mat);
							}
							
							/* prepare colors */
							if (do_const_color) {
								/* 13 October 2009, Disabled this to make ghosting show the right colors (Aligorith) */
							}
							else if (arm->flag & ARM_POSEMODE)
								set_pchan_colorset(ob, pchan);
							else {
								rgba_uchar_to_float(fcolor, ob_wire_col);
							}
								
							/* catch exception for bone with hidden parent */
							flag = bone->flag;
							if ((bone->parent) && (bone->parent->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)))
								flag &= ~BONE_CONNECTED;
								
							/* set temporary flag for drawing bone as active, but only if selected */
							if (bone == arm->act_bone)
								flag |= BONE_DRAW_ACTIVE;
							
							draw_custom_bone(scene, sl, v3d, rv3d, pchan->custom,
							                 OB_WIRE, arm->flag, flag, index, PCHAN_CUSTOM_DRAW_SIZE(pchan));
							
							gpuPopMatrix();
						}
					}
				}
			}
			
			if (index != -1) 
				index += 0x10000;  /* pose bones count in higher 2 bytes only */
		}
		/* stick or wire bones have not been drawn yet so don't clear object selection in this case */
		if (ELEM(arm->drawtype, ARM_LINE, ARM_WIRE) == 0 && draw_wire && index != -1) {
			/* object tag, for bordersel optim */
			GPU_select_load_id(index & 0xFFFF);
			index = -1;
		}
	}
	
	/* custom bone may draw outline double-width */
	if (arm->flag & ARM_POSEMODE) {
		glLineWidth(1.0f);
	}

	/* wire draw over solid only in posemode */
	if ((dt <= OB_WIRE) || (arm->flag & ARM_POSEMODE) || ELEM(arm->drawtype, ARM_LINE, ARM_WIRE)) {
		/* draw line check first. we do selection indices */
		if (ELEM(arm->drawtype, ARM_LINE, ARM_WIRE)) {
			if (arm->flag & ARM_POSEMODE) 
				index = base->selcol;
		}
		/* if solid && posemode, we draw again with polygonoffset */
		else if ((dt > OB_WIRE) && (arm->flag & ARM_POSEMODE)) {
			ED_view3d_polygon_offset(rv3d, 1.0);
		}
		else {
			/* and we use selection indices if not done yet */
			if (arm->flag & ARM_POSEMODE) 
				index = base->selcol;
		}

		if (is_cull_enabled == false) {
			is_cull_enabled = true;
			glEnable(GL_CULL_FACE);
		}

		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			bone = pchan->bone;
			arm->layer_used |= bone->layer;
			
			/* 1) bone must be visible, 2) for OpenGL select-drawing cannot have unselectable [#27194] */
			if (((bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0) &&
			    ((G.f & G_PICKSEL) == 0 || (bone->flag & BONE_UNSELECTABLE) == 0))
			{
				if (bone->layer & arm->layer) {
					const short constflag = pchan->constflag;
					if ((do_dashed & DASH_RELATIONSHIP_LINES) && (pchan->parent)) {
						/* Draw a line from our root to the parent's tip 
						 * - only if V3D_HIDE_HELPLINES is enabled...
						 */
						if ((do_dashed & DASH_HELP_LINES) && ((bone->flag & BONE_CONNECTED) == 0)) {
							VertexFormat *format = immVertexFormat();
							unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);

							immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

							if (arm->flag & ARM_POSEMODE) {
								GPU_select_load_id(index & 0xFFFF);  /* object tag, for bordersel optim */
								UI_GetThemeColor4fv(TH_WIRE, fcolor);
								immUniformColor4fv(fcolor);
							}

							setlinestyle(3);
							immBegin(PRIM_LINES, 2);
							immVertex3fv(pos, pchan->pose_head);
							immVertex3fv(pos, pchan->parent->pose_tail);
							immEnd();
							setlinestyle(0);

							immUnbindProgram();
						}
						
						/* Draw a line to IK root bone 
						 *  - only if temporary chain (i.e. "autoik")
						 */
						if (arm->flag & ARM_POSEMODE) {
							if (constflag & PCHAN_HAS_IK) {
								if (bone->flag & BONE_SELECTED) {
									if (constflag & PCHAN_HAS_TARGET) rgba_float_args_set(fcolor, 200.f/255.f, 120.f/255.f, 0.f/255.f, 1.0f);
									else rgba_float_args_set(fcolor, 200.f/255.f, 200.f/255.f, 50.f/255.f, 1.0f);  /* add theme! */

									GPU_select_load_id(index & 0xFFFF);
									pchan_draw_IK_root_lines(pchan, !(do_dashed & DASH_HELP_LINES));
								}
							}
							else if (constflag & PCHAN_HAS_SPLINEIK) {
								if (bone->flag & BONE_SELECTED) {
									rgba_float_args_set(fcolor, 150.f/255.f, 200.f/255.f, 50.f/255.f, 1.0f);  /* add theme! */
									
									GPU_select_load_id(index & 0xFFFF);
									pchan_draw_IK_root_lines(pchan, !(do_dashed & DASH_HELP_LINES));
								}
							}
						}
					}
					
					gpuPushMatrix();
					if (arm->drawtype != ARM_ENVELOPE)
						gpuMultMatrix3D(pchan->pose_mat);
					
					/* catch exception for bone with hidden parent */
					flag = bone->flag;
					if ((bone->parent) && (bone->parent->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)))
						flag &= ~BONE_CONNECTED;
					
					/* set temporary flag for drawing bone as active, but only if selected */
					if (bone == arm->act_bone)
						flag |= BONE_DRAW_ACTIVE;
					
					/* extra draw service for pose mode */

					/* set color-set to use */
					if (do_const_color) {
						/* keep color */
					}
					else {
						set_pchan_colorset(ob, pchan);
					}
					
					if ((pchan->custom) && !(arm->flag & ARM_NO_CUSTOM)) {
						/* custom bone shapes should not be drawn here! */
					}
					else if (arm->drawtype == ARM_ENVELOPE) {
						if (dt < OB_SOLID)
							draw_sphere_bone_wire(smat, imat, arm->flag, flag, constflag, index, pchan, NULL);
					}
					else if (arm->drawtype == ARM_LINE)
						draw_line_bone(arm->flag, flag, constflag, index, pchan, NULL);
					else if (arm->drawtype == ARM_WIRE)
						draw_wire_bone(dt, arm->flag, flag, constflag, index, pchan, NULL);
					else if (arm->drawtype == ARM_B_BONE)
						draw_b_bone(OB_WIRE, arm->flag, flag, constflag, index, pchan, NULL);
					else
						draw_bone(OB_WIRE, arm->flag, flag, constflag, index, bone->length);
					
					gpuPopMatrix();
				}
			}
			
			/* pose bones count in higher 2 bytes only */
			if (index != -1) 
				index += 0x10000;
		}
		/* restore things */
		if (!ELEM(arm->drawtype, ARM_WIRE, ARM_LINE) && (dt > OB_WIRE) && (arm->flag & ARM_POSEMODE))
			ED_view3d_polygon_offset(rv3d, 0.0);
	}
	
	/* restore */
	if (is_cull_enabled) {
		glDisable(GL_CULL_FACE);
	}
	
	/* draw DoFs */
	if (arm->flag & ARM_POSEMODE) {
		if (((base->flag_legacy & OB_FROMDUPLI) == 0) && ((v3d->flag & V3D_HIDE_HELPLINES) == 0)) {
			draw_pose_dofs(ob);
		}
	}

	/* finally names and axes */
	if ((arm->flag & (ARM_DRAWNAMES | ARM_DRAWAXES)) &&
	    (is_outline == 0) &&
	    ((base->flag_legacy & OB_FROMDUPLI) == 0))
	{
		/* patch for several 3d cards (IBM mostly) that crash on GL_SELECT with text drawing */
		if ((G.f & G_PICKSEL) == 0) {
			float vec[3];

			unsigned char col[4];
			col[0] = ob_wire_col[0];
			col[1] = ob_wire_col[1];
			col[2] = ob_wire_col[2];
			col[3] = 255;
			
			if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
			
			for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				if ((pchan->bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0) {
					if (pchan->bone->layer & arm->layer) {
						if (arm->flag & (ARM_EDITMODE | ARM_POSEMODE)) {
							bone = pchan->bone;
							UI_GetThemeColor3ubv((bone->flag & BONE_SELECTED) ? TH_TEXT_HI : TH_TEXT, col);
						}
						else if (dt > OB_WIRE) {
							UI_GetThemeColor3ubv(TH_TEXT, col);
						}
						
						/*  Draw names of bone  */
						if (arm->flag & ARM_DRAWNAMES) {
							mid_v3_v3v3(vec, pchan->pose_head, pchan->pose_tail);
							view3d_cached_text_draw_add(vec, pchan->name, strlen(pchan->name), 10, 0, col);
						}
						
						/*	Draw additional axes on the bone tail  */
						if ((arm->flag & ARM_DRAWAXES) && (arm->flag & ARM_POSEMODE)) {
							gpuPushMatrix();
							copy_m4_m4(bmat, pchan->pose_mat);
							bone_matrix_translate_y(bmat, pchan->bone->length);
							gpuMultMatrix3D(bmat);
							
							float viewmat_pchan[4][4];
							mul_m4_m4m4(viewmat_pchan, rv3d->viewmatob, bmat);
							drawaxes(viewmat_pchan, pchan->bone->length * 0.25f, OB_ARROWS, col);
							
							gpuPopMatrix();
						}
					}
				}
			}
			
			if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
		}
	}

	if (index != -1) {
		GPU_select_load_id(-1);
	}
}

/* in editmode, we don't store the bone matrix... */
static void get_matrix_editbone(EditBone *ebone, float bmat[4][4])
{
	ebone->length = len_v3v3(ebone->tail, ebone->head);
	ED_armature_ebone_to_mat4(ebone, bmat);
}

static void draw_ebones(View3D *v3d, ARegion *ar, Object *ob, const short dt)
{
	RegionView3D *rv3d = ar->regiondata;
	EditBone *eBone;
	bArmature *arm = ob->data;
	float smat[4][4], imat[4][4], bmat[4][4];
	unsigned int index;
	int flag;
	
	/* being set in code below */
	arm->layer_used = 0;

	ED_view3d_check_mats_rv3d(rv3d);

	/* envelope (deform distance) */
	if (arm->drawtype == ARM_ENVELOPE) {
		/* precalc inverse matrix for drawing screen aligned */
		copy_m4_m4(smat, rv3d->viewmatob);
		mul_mat3_m4_fl(smat, 1.0f / len_v3(ob->obmat[0]));
		invert_m4_m4(imat, smat);
		
		/* and draw blended distances */
		glEnable(GL_BLEND);
		
		if (v3d->zbuf) glDisable(GL_DEPTH_TEST);

		for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
			if (eBone->layer & arm->layer) {
				if ((eBone->flag & (BONE_HIDDEN_A | BONE_NO_DEFORM)) == 0) {
					if (eBone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL))
						draw_sphere_bone_dist(smat, imat, NULL, eBone);
				}
			}
		}
		
		if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
	}
	
	/* if solid we draw it first */
	if ((dt > OB_WIRE) && (arm->drawtype != ARM_LINE)) {
		for (eBone = arm->edbo->first, index = 0; eBone; eBone = eBone->next, index++) {
			if (eBone->layer & arm->layer) {
				if ((eBone->flag & BONE_HIDDEN_A) == 0) {
					gpuPushMatrix();
					get_matrix_editbone(eBone, bmat);
					gpuMultMatrix3D(bmat);
					
					/* catch exception for bone with hidden parent */
					flag = eBone->flag;
					if ((eBone->parent) && !EBONE_VISIBLE(arm, eBone->parent)) {
						flag &= ~BONE_CONNECTED;
					}
						
					/* set temporary flag for drawing bone as active, but only if selected */
					if (eBone == arm->act_edbone)
						flag |= BONE_DRAW_ACTIVE;
					
					if (arm->drawtype == ARM_ENVELOPE)
						draw_sphere_bone(OB_SOLID, arm->flag, flag, 0, index, NULL, eBone);
					else if (arm->drawtype == ARM_B_BONE)
						draw_b_bone(OB_SOLID, arm->flag, flag, 0, index, NULL, eBone);
					else if (arm->drawtype == ARM_WIRE)
						draw_wire_bone(OB_SOLID, arm->flag, flag, 0, index, NULL, eBone);
					else {
						draw_bone(OB_SOLID, arm->flag, flag, 0, index, eBone->length);
					}
					
					gpuPopMatrix();
				}
			}
		}
	}
	
	/* if wire over solid, set offset */
	index = -1;
	GPU_select_load_id(-1);
	if (ELEM(arm->drawtype, ARM_LINE, ARM_WIRE)) {
		if (G.f & G_PICKSEL)
			index = 0;
	}
	else if (dt > OB_WIRE) 
		ED_view3d_polygon_offset(rv3d, 1.0);
	else if (arm->flag & ARM_EDITMODE) 
		index = 0;  /* do selection codes */
	
	for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
		arm->layer_used |= eBone->layer;
		if (eBone->layer & arm->layer) {
			if ((eBone->flag & BONE_HIDDEN_A) == 0) {
				
				/* catch exception for bone with hidden parent */
				flag = eBone->flag;
				if ((eBone->parent) && !EBONE_VISIBLE(arm, eBone->parent)) {
					flag &= ~BONE_CONNECTED;
				}
					
				/* set temporary flag for drawing bone as active, but only if selected */
				if (eBone == arm->act_edbone)
					flag |= BONE_DRAW_ACTIVE;
				
				if (arm->drawtype == ARM_ENVELOPE) {
					if (dt < OB_SOLID)
						draw_sphere_bone_wire(smat, imat, arm->flag, flag, 0, index, NULL, eBone);
				}
				else {
					gpuPushMatrix();
					get_matrix_editbone(eBone, bmat);
					gpuMultMatrix3D(bmat);
					
					if (arm->drawtype == ARM_LINE) 
						draw_line_bone(arm->flag, flag, 0, index, NULL, eBone);
					else if (arm->drawtype == ARM_WIRE)
						draw_wire_bone(OB_WIRE, arm->flag, flag, 0, index, NULL, eBone);
					else if (arm->drawtype == ARM_B_BONE)
						draw_b_bone(OB_WIRE, arm->flag, flag, 0, index, NULL, eBone);
					else
						draw_bone(OB_WIRE, arm->flag, flag, 0, index, eBone->length);
					
					gpuPopMatrix();
				}
				
				/* offset to parent */
				if (eBone->parent) {
					VertexFormat *format = immVertexFormat();
					unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);

					GPU_select_load_id(-1);  /* -1 here is OK! */

					immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
					UI_GetThemeColor4fv(TH_WIRE_EDIT, fcolor);
					immUniformColor4fv(fcolor);
					
					setlinestyle(3);
					immBegin(PRIM_LINES, 2);
					immVertex3fv(pos, eBone->head);
					immVertex3fv(pos, eBone->parent->tail);
					immEnd();
					setlinestyle(0);

					immUnbindProgram();
				}
			}
		}
		if (index != -1) index++;
	}
	
	/* restore */
	if (index != -1) {
		GPU_select_load_id(-1);
	}

	if (ELEM(arm->drawtype, ARM_LINE, ARM_WIRE)) {
		/* pass */
	}
	else if (dt > OB_WIRE) {
		ED_view3d_polygon_offset(rv3d, 0.0);
	}
	
	/* finally names and axes */
	if (arm->flag & (ARM_DRAWNAMES | ARM_DRAWAXES)) {
		/* patch for several 3d cards (IBM mostly) that crash on GL_SELECT with text drawing */
		if ((G.f & G_PICKSEL) == 0) {
			float vec[3];
			unsigned char col[4];
			col[3] = 255;
			
			if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
			
			for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
				if (eBone->layer & arm->layer) {
					if ((eBone->flag & BONE_HIDDEN_A) == 0) {

						UI_GetThemeColor3ubv((eBone->flag & BONE_SELECTED) ? TH_TEXT_HI : TH_TEXT, col);

						/*	Draw name */
						if (arm->flag & ARM_DRAWNAMES) {
							mid_v3_v3v3(vec, eBone->head, eBone->tail);
							view3d_cached_text_draw_add(vec, eBone->name, strlen(eBone->name), 10, 0, col);
						}
						/*	Draw additional axes */
						if (arm->flag & ARM_DRAWAXES) {
							gpuPushMatrix();
							get_matrix_editbone(eBone, bmat);
							bone_matrix_translate_y(bmat, eBone->length);
							gpuMultMatrix3D(bmat);

							float viewmat_ebone[4][4];
							mul_m4_m4m4(viewmat_ebone, rv3d->viewmatob, bmat);
							drawaxes(viewmat_ebone, eBone->length * 0.25f, OB_ARROWS, col);
							
							gpuPopMatrix();
						}
						
					}
				}
			}
			
			if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
		}
	}
}

/* ****************************** Armature Visualization ******************************** */

/* ---------- Paths --------- */

/* draw bone paths
 *	- in view space 
 */
static void draw_pose_paths(Scene *scene, View3D *v3d, ARegion *ar, Object *ob)
{
	bAnimVizSettings *avs = &ob->pose->avs;
	bArmature *arm = ob->data;
	bPoseChannel *pchan;
	
	/* setup drawing environment for paths */
	draw_motion_paths_init(v3d, ar);
	
	/* draw paths where they exist and they releated bone is visible */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		if ((pchan->bone->layer & arm->layer) && (pchan->mpath))
			draw_motion_path_instance(scene, ob, pchan, avs, pchan->mpath);
	}
	
	/* cleanup after drawing */
	draw_motion_paths_cleanup(v3d);
}


/* ---------- Ghosts --------- */

/* helper function for ghost drawing - sets/removes flags for temporarily 
 * hiding unselected bones while drawing ghosts
 */
static void ghost_poses_tag_unselected(Object *ob, short unset)
{
	bArmature *arm = ob->data;
	bPose *pose = ob->pose;
	bPoseChannel *pchan;
	
	/* don't do anything if no hiding any bones */
	if ((arm->flag & ARM_GHOST_ONLYSEL) == 0)
		return;
		
	/* loop over all pchans, adding/removing tags as appropriate */
	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		if ((pchan->bone) && (arm->layer & pchan->bone->layer)) {
			if (unset) {
				/* remove tags from all pchans if cleaning up */
				pchan->bone->flag &= ~BONE_HIDDEN_PG;
			}
			else {
				/* set tags on unselected pchans only */
				if ((pchan->bone->flag & BONE_SELECTED) == 0)
					pchan->bone->flag |= BONE_HIDDEN_PG;
			}
		}
	}
}

/* draw ghosts that occur within a frame range 
 *  note: object should be in posemode
 */
static void draw_ghost_poses_range(Scene *scene, SceneLayer *sl, View3D *v3d, ARegion *ar, Base *base)
{
	Object *ob = base->object;
	AnimData *adt = BKE_animdata_from_id(&ob->id);
	bArmature *arm = ob->data;
	bPose *posen, *poseo;
	float start, end, stepsize, range, colfac;
	int cfrao, flago;
	unsigned char col[4];
	
	start = (float)arm->ghostsf;
	end = (float)arm->ghostef;
	if (end <= start)
		return;
	
	/* prevent infinite loops if this is set to 0 - T49527 */
	if (arm->ghostsize < 1)
		arm->ghostsize = 1;
	
	stepsize = (float)(arm->ghostsize);
	range = (float)(end - start);
	
	/* store values */
	ob->mode &= ~OB_MODE_POSE;
	cfrao = CFRA;
	flago = arm->flag;
	arm->flag &= ~(ARM_DRAWNAMES | ARM_DRAWAXES);
	
	/* copy the pose */
	poseo = ob->pose;
	BKE_pose_copy_data(&posen, ob->pose, 1);
	ob->pose = posen;
	BKE_pose_rebuild(ob, ob->data);    /* child pointers for IK */
	ghost_poses_tag_unselected(ob, 0);      /* hide unselected bones if need be */
	
	glEnable(GL_BLEND);
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* draw from first frame of range to last */
	for (CFRA = (int)start; CFRA <= end; CFRA += (int)stepsize) {
		colfac = (end - (float)CFRA) / range;
		UI_GetThemeColorShadeAlpha4ubv(TH_WIRE, 0, -128 - (int)(120.0f * sqrtf(colfac)), col);
		
		BKE_animsys_evaluate_animdata(scene, &ob->id, adt, (float)CFRA, ADT_RECALC_ALL);
		BKE_pose_where_is(scene, ob);
		draw_pose_bones(scene, sl, v3d, ar, base, OB_WIRE, col, true, false);
	}
	glDisable(GL_BLEND);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	
	/* before disposing of temp pose, use it to restore object to a sane state */
	BKE_animsys_evaluate_animdata(scene, &ob->id, adt, (float)cfrao, ADT_RECALC_ALL);
	
	/* clean up temporary pose */
	ghost_poses_tag_unselected(ob, 1);      /* unhide unselected bones if need be */
	BKE_pose_free(posen);
	
	/* restore */
	CFRA = cfrao;
	ob->pose = poseo;
	arm->flag = flago;
	ob->mode |= OB_MODE_POSE;
}

/* draw ghosts on keyframes in action within range 
 *	- object should be in posemode 
 */
static void draw_ghost_poses_keys(Scene *scene, SceneLayer *sl, View3D *v3d, ARegion *ar, BaseLegacy *base)
{
	Object *ob = base->object;
	AnimData *adt = BKE_animdata_from_id(&ob->id);
	bAction *act = (adt) ? adt->action : NULL;
	bArmature *arm = ob->data;
	bPose *posen, *poseo;
	DLRBT_Tree keys;
	ActKeyColumn *ak, *akn;
	float start, end, range, colfac, i;
	int cfrao, flago;
	unsigned char col[4];
	
	start = (float)arm->ghostsf;
	end = (float)arm->ghostef;
	if (end <= start)
		return;
	
	/* get keyframes - then clip to only within range */
	BLI_dlrbTree_init(&keys);
	action_to_keylist(adt, act, &keys, NULL);
	BLI_dlrbTree_linkedlist_sync(&keys);
	
	range = 0;
	for (ak = keys.first; ak; ak = akn) {
		akn = ak->next;
		
		if ((ak->cfra < start) || (ak->cfra > end))
			BLI_freelinkN((ListBase *)&keys, ak);
		else
			range++;
	}
	if (range == 0) return;
	
	/* store values */
	ob->mode &= ~OB_MODE_POSE;
	cfrao = CFRA;
	flago = arm->flag;
	arm->flag &= ~(ARM_DRAWNAMES | ARM_DRAWAXES);

	/* copy the pose */
	poseo = ob->pose;
	BKE_pose_copy_data(&posen, ob->pose, 1);
	ob->pose = posen;
	BKE_pose_rebuild(ob, ob->data);  /* child pointers for IK */
	ghost_poses_tag_unselected(ob, 0);    /* hide unselected bones if need be */
	
	glEnable(GL_BLEND);
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* draw from first frame of range to last */
	for (ak = keys.first, i = 0; ak; ak = ak->next, i++) {
		colfac = i / range;
		UI_GetThemeColorShadeAlpha4ubv(TH_WIRE, 0, -128 - (int)(120.0f * sqrtf(colfac)), col);
		
		CFRA = (int)ak->cfra;
		
		BKE_animsys_evaluate_animdata(scene, &ob->id, adt, (float)CFRA, ADT_RECALC_ALL);
		BKE_pose_where_is(scene, ob);
		draw_pose_bones(scene, sl, v3d, ar, base, OB_WIRE, col, true, false);
	}
	glDisable(GL_BLEND);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	
	/* before disposing of temp pose, use it to restore object to a sane state */
	BKE_animsys_evaluate_animdata(scene, &ob->id, adt, (float)cfrao, ADT_RECALC_ALL);
	
	/* clean up temporary pose */
	ghost_poses_tag_unselected(ob, 1);  /* unhide unselected bones if need be */
	BLI_dlrbTree_free(&keys);
	BKE_pose_free(posen);
	
	/* restore */
	CFRA = cfrao;
	ob->pose = poseo;
	arm->flag = flago;
	ob->mode |= OB_MODE_POSE;
}

/* draw ghosts around current frame
 *  - object is supposed to be armature in posemode
 */
static void draw_ghost_poses(Scene *scene, SceneLayer *sl, View3D *v3d, ARegion *ar, Base *base)
{
	Object *ob = base->object;
	AnimData *adt = BKE_animdata_from_id(&ob->id);
	bArmature *arm = ob->data;
	bPose *posen, *poseo;
	float cur, start, end, stepsize, range, colfac, actframe, ctime;
	int cfrao, flago;
	unsigned char col[4];
	
	/* pre conditions, get an action with sufficient frames */
	if (ELEM(NULL, adt, adt->action))
		return;

	calc_action_range(adt->action, &start, &end, 0);
	if (start == end)
		return;
	
	/* prevent infinite loops if this is set to 0 - T49527 */
	if (arm->ghostsize < 1)
		arm->ghostsize = 1;
	
	stepsize = (float)(arm->ghostsize);
	range = (float)(arm->ghostep) * stepsize + 0.5f;   /* plus half to make the for loop end correct */
	
	/* store values */
	ob->mode &= ~OB_MODE_POSE;
	cfrao = CFRA;
	actframe = BKE_nla_tweakedit_remap(adt, (float)CFRA, 0);
	flago = arm->flag;
	arm->flag &= ~(ARM_DRAWNAMES | ARM_DRAWAXES);
	
	/* copy the pose */
	poseo = ob->pose;
	BKE_pose_copy_data(&posen, ob->pose, 1);
	ob->pose = posen;
	BKE_pose_rebuild(ob, ob->data);    /* child pointers for IK */
	ghost_poses_tag_unselected(ob, 0);      /* hide unselected bones if need be */
	
	glEnable(GL_BLEND);
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* draw from darkest blend to lowest */
	for (cur = stepsize; cur < range; cur += stepsize) {
		ctime = cur - (float)fmod(cfrao, stepsize);  /* ensures consistent stepping */
		colfac = ctime / range;
		UI_GetThemeColorShadeAlpha4ubv(TH_WIRE, 0, -128 - (int)(120.0f * sqrtf(colfac)), col);
		
		/* only within action range */
		if (actframe + ctime >= start && actframe + ctime <= end) {
			CFRA = (int)BKE_nla_tweakedit_remap(adt, actframe + ctime, NLATIME_CONVERT_MAP);
			
			if (CFRA != cfrao) {
				BKE_animsys_evaluate_animdata(scene, &ob->id, adt, (float)CFRA, ADT_RECALC_ALL);
				BKE_pose_where_is(scene, ob);
				draw_pose_bones(scene, sl, v3d, ar, base, OB_WIRE, col, true, false);
			}
		}
		
		ctime = cur + (float)fmod((float)cfrao, stepsize) - stepsize + 1.0f;   /* ensures consistent stepping */
		colfac = ctime / range;
		UI_GetThemeColorShadeAlpha4ubv(TH_WIRE, 0, -128 - (int)(120.0f * sqrtf(colfac)), col);
		
		/* only within action range */
		if ((actframe - ctime >= start) && (actframe - ctime <= end)) {
			CFRA = (int)BKE_nla_tweakedit_remap(adt, actframe - ctime, NLATIME_CONVERT_MAP);
			
			if (CFRA != cfrao) {
				BKE_animsys_evaluate_animdata(scene, &ob->id, adt, (float)CFRA, ADT_RECALC_ALL);
				BKE_pose_where_is(scene, ob);
				draw_pose_bones(scene, sl, v3d, ar, base, OB_WIRE, col, true, false);
			}
		}
	}
	glDisable(GL_BLEND);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	
	/* before disposing of temp pose, use it to restore object to a sane state */
	BKE_animsys_evaluate_animdata(scene, &ob->id, adt, (float)cfrao, ADT_RECALC_ALL);
	
	/* clean up temporary pose */
	ghost_poses_tag_unselected(ob, 1);      /* unhide unselected bones if need be */
	BKE_pose_free(posen);
	
	/* restore */
	CFRA = cfrao;
	ob->pose = poseo;
	arm->flag = flago;
	ob->mode |= OB_MODE_POSE;
}

/* ********************************** Armature Drawing - Main ************************* */

/* called from drawobject.c, return true if nothing was drawn
 * (ob_wire_col == NULL) when drawing ghost */
bool draw_armature(Scene *scene, SceneLayer *sl, View3D *v3d, ARegion *ar, Base *base,
                   const short dt, const short dflag, const unsigned char ob_wire_col[4],
                   const bool is_outline)
{
	Object *ob = base->object;
	bArmature *arm = ob->data;
	bool retval = false;

	if (v3d->flag2 & V3D_RENDER_OVERRIDE)
		return true;

#if 0 /* Not used until lighting is properly reimplemented */
	if (dt > OB_WIRE) {
		/* we use color for solid lighting */
		if (ELEM(arm->drawtype, ARM_LINE, ARM_WIRE)) {
			const float diffuse[3] = {0.64f, 0.64f, 0.64f};
			const float specular[3] = {0.5f, 0.5f, 0.5f};
			GPU_basic_shader_colors(diffuse, specular, 35, 1.0f);
		}
		else {
			const float diffuse[3] = {1.0f, 1.0f, 1.0f};
			const float specular[3] = {1.0f, 1.0f, 1.0f};
			GPU_basic_shader_colors(diffuse, specular, 35, 1.0f);
			glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);  /* only for lighting... */
		}
	}
#endif

	/* arm->flag is being used to detect mode... */
	/* editmode? */
	if (arm->edbo) {
		arm->flag |= ARM_EDITMODE;
		draw_ebones(v3d, ar, ob, dt);
		arm->flag &= ~ARM_EDITMODE;
	}
	else {
		/*	Draw Pose */
		if (ob->pose && ob->pose->chanbase.first) {
			/* We can't safely draw non-updated pose, might contain NULL bone pointers... */
			if (ob->pose->flag & POSE_RECALC) {
				BKE_pose_rebuild(ob, arm);
			}

			/* drawing posemode selection indices or colors only in these cases */
			if (!(base->flag_legacy & OB_FROMDUPLI)) {
				if (G.f & G_PICKSEL) {
#if 0
					/* nifty but actually confusing to allow bone selection out of posemode */
					if (OBACT && (OBACT->mode & OB_MODE_WEIGHT_PAINT)) {
						if (ob == modifiers_isDeformedByArmature(OBACT))
							arm->flag |= ARM_POSEMODE;
					}
					else
#endif
					if (ob->mode & OB_MODE_POSE) {
						arm->flag |= ARM_POSEMODE;
					}
				}
				else if (ob->mode & OB_MODE_POSE) {
					if (arm->ghosttype == ARM_GHOST_RANGE) {
						draw_ghost_poses_range(scene, sl, v3d, ar, base);
					}
					else if (arm->ghosttype == ARM_GHOST_KEYS) {
						draw_ghost_poses_keys(scene, sl, v3d, ar, base);
					}
					else if (arm->ghosttype == ARM_GHOST_CUR) {
						if (arm->ghostep)
							draw_ghost_poses(scene, sl, v3d, ar, base);
					}
					if ((dflag & DRAW_SCENESET) == 0) {
						if (ob == OBACT_NEW)
							arm->flag |= ARM_POSEMODE;
						else if (OBACT_NEW && (OBACT_NEW->mode & OB_MODE_WEIGHT_PAINT)) {
							if (ob == modifiers_isDeformedByArmature(OBACT_NEW))
								arm->flag |= ARM_POSEMODE;
						}
						draw_pose_paths(scene, v3d, ar, ob);
					}
				}
			}
			draw_pose_bones(scene, sl, v3d, ar, base, dt, ob_wire_col, (dflag & DRAW_CONSTCOLOR), is_outline);
			arm->flag &= ~ARM_POSEMODE; 
		}
		else {
			retval = true;
		}
	}
	/* restore */
	glFrontFace(GL_CCW);

	return retval;
}
