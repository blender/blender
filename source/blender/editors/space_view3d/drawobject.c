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
 * Contributor(s): Blender Foundation, full recode and added functions
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawobject.c
 *  \ingroup spview3d
 */

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_constraint_types.h"  /* for drawing constraint */
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_link_utils.h"
#include "BLI_string.h"
#include "BLI_math.h"
#include "BLI_memarena.h"

#include "BKE_anim.h"  /* for the where_on_path function */
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"  /* for the get_constraint_target function */
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_render.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_movieclip.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_unit.h"
#include "BKE_tracking.h"

#include "BKE_editmesh.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"
#include "GPU_select.h"
#include "GPU_basic_shader.h"
#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_batch.h"
#include "GPU_matrix.h"

#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_types.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "WM_api.h"
#include "BLF_api.h"

#include "view3d_intern.h"  /* bad level include */

/* prototypes */
static void imm_draw_box(const float vec[8][3], bool solid, unsigned pos);

/* Workaround for sequencer scene render mode.
 *
 * Strips doesn't use DAG to update objects or so, which
 * might lead to situations when object is drawing without
 * curve cache ready.
 *
 * Ideally we don't want to evaluate objects from drawing,
 * but it'll require some major sequencer re-design. So
 * for now just fallback to legacy behavior with calling
 * display ist creating from draw().
 */
#define SEQUENCER_DAG_WORKAROUND

typedef enum eWireDrawMode {
	OBDRAW_WIRE_OFF = 0,
	OBDRAW_WIRE_ON = 1,
	OBDRAW_WIRE_ON_DEPTH = 2
} eWireDrawMode;

typedef struct drawDMVerts_userData {
	BMesh *bm;

	BMVert *eve_act;
	char sel;
	unsigned int pos, color;

	/* cached theme values */
	unsigned char th_editmesh_active[4];
	unsigned char th_vertex_select[4];
	unsigned char th_vertex[4];
	unsigned char th_skin_root[4];

	/* for skin node drawing */
	int cd_vskin_offset;
	float imat[4][4];
} drawDMVerts_userData;

typedef struct drawDMEdgesSel_userData {
	BMesh *bm;

	unsigned char *baseCol, *selCol, *actCol;
	BMEdge *eed_act;
} drawDMEdgesSel_userData;

typedef struct drawDMEdgesSelInterp_userData {
	BMesh *bm;

	unsigned char *baseCol, *selCol;
	unsigned char *lastCol;
} drawDMEdgesSelInterp_userData;

typedef struct drawDMEdgesWeightInterp_userData {
	BMesh *bm;

	int cd_dvert_offset;
	int defgroup_tot;
	int vgroup_index;
	char weight_user;
	float alert_color[3];

} drawDMEdgesWeightInterp_userData;

typedef struct drawDMFacesSel_userData {
#ifdef WITH_FREESTYLE
	unsigned char *cols[4];
#else
	unsigned char *cols[3];
#endif

	DerivedMesh *dm;
	BMesh *bm;

	BMFace *efa_act;
	const int *orig_index_mp_to_orig;
} drawDMFacesSel_userData;

typedef struct drawDMNormal_userData {
	unsigned int pos;
	BMesh *bm;
	int uniform_scale;
	float normalsize;
	float tmat[3][3];
	float imat[3][3];
} drawDMNormal_userData;

typedef struct drawMVertOffset_userData {
	unsigned int pos, col;
	MVert *mvert;
	int offset;
} drawMVertOffset_userData;

typedef struct drawDMLayer_userData {
	BMesh *bm;
	int cd_layer_offset;
	unsigned int pos, col;
} drawDMLayer_userData;

typedef struct drawBMOffset_userData {
	unsigned int pos, col;
	BMesh *bm;
	int offset;
} drawBMOffset_userData;

typedef struct drawBMSelect_userData {
	BMesh *bm;
	bool select;
	unsigned int pos;
} drawBMSelect_userData;


static void drawcube_size(float size, unsigned pos);
static void drawcircle_size(float size, unsigned pos);
static void draw_empty_sphere(float size, unsigned pos);
static void draw_empty_cone(float size, unsigned pos);

static void ob_wire_color_blend_theme_id(const unsigned char ob_wire_col[4], const int theme_id, float fac, float r_col[3])
{
	float col_wire[3], col_bg[3];

	rgb_uchar_to_float(col_wire, ob_wire_col);

	UI_GetThemeColor3fv(theme_id, col_bg);
	interp_v3_v3v3(r_col, col_bg, col_wire, fac);
}

int view3d_effective_drawtype(const struct View3D *v3d)
{
	if (v3d->drawtype == OB_RENDER) {
		return v3d->prev_drawtype;
	}
	return v3d->drawtype;
}

/* this condition has been made more complex since editmode can draw textures */
bool check_object_draw_texture(Scene *scene, View3D *v3d, const char drawtype)
{
	const int v3d_drawtype = view3d_effective_drawtype(v3d);
	/* texture and material draw modes */
	if (ELEM(v3d_drawtype, OB_TEXTURE, OB_MATERIAL) && drawtype > OB_SOLID) {
		return true;
	}

	/* textured solid */
	if ((v3d_drawtype == OB_SOLID) &&
	    (v3d->flag2 & V3D_SOLID_TEX) &&
	    (BKE_scene_use_new_shading_nodes(scene) == false))
	{
		return true;
	}
	
	if (v3d->flag2 & V3D_SHOW_SOLID_MATCAP) {
		return true;
	}
	
	return false;
}

static bool check_object_draw_editweight(Mesh *me, DerivedMesh *finalDM)
{
	if (me->drawflag & ME_DRAWEIGHT) {
		/* editmesh handles its own weight drawing */
		if (finalDM->type != DM_TYPE_EDITBMESH) {
			return true;
		}
	}

	return false;
}

static bool check_ob_drawface_dot(Scene *sce, View3D *vd, char dt)
{
	if ((sce->toolsettings->selectmode & SCE_SELECT_FACE) == 0)
		return false;

	if (G.f & G_BACKBUFSEL)
		return false;

	if ((vd->flag & V3D_ZBUF_SELECT) == 0)
		return true;

	/* if its drawing textures with zbuf sel, then don't draw dots */
	if (dt == OB_TEXTURE && vd->drawtype == OB_TEXTURE)
		return false;

	if ((vd->drawtype >= OB_SOLID) && (vd->flag2 & V3D_SOLID_TEX))
		return false;

	return true;
}

/* ************************ */

/* check for glsl drawing */

bool draw_glsl_material(Scene *scene, SceneLayer *sl, Object *ob, View3D *v3d, const char dt)
{
	if (G.f & G_PICKSEL)
		return false;
	if (!check_object_draw_texture(scene, v3d, dt))
		return false;
	if (ob == OBACT_NEW && (ob && ob->mode & OB_MODE_WEIGHT_PAINT))
		return false;
	
	if (v3d->flag2 & V3D_SHOW_SOLID_MATCAP)
		return true;

	if (v3d->drawtype == OB_TEXTURE)
		return (scene->gm.matmode == GAME_MAT_GLSL && !BKE_scene_use_new_shading_nodes(scene));
	else if (v3d->drawtype == OB_MATERIAL && dt > OB_SOLID)
		return true;
	else
		return false;
}

static bool check_alpha_pass(Base *base)
{
	if (base->flag_legacy & OB_FROMDUPLI)
		return false;

	if (G.f & G_PICKSEL)
		return false;

	if (base->object->mode & OB_MODE_ALL_PAINT)
		return false;

	return (base->object->dtx & OB_DRAWTRANSP);
}

/***/
static const unsigned int colortab[] = {
	0x0, 0x403000, 0xFFFF88
};

/* ----------------- OpenGL Circle Drawing - Tables for Optimized Drawing Speed ------------------ */
/* 32 values of sin function (still same result!) */
#define CIRCLE_RESOL 32

static const float sinval[CIRCLE_RESOL] = {
	0.00000000,
	0.20129852,
	0.39435585,
	0.57126821,
	0.72479278,
	0.84864425,
	0.93775213,
	0.98846832,
	0.99871650,
	0.96807711,
	0.89780453,
	0.79077573,
	0.65137248,
	0.48530196,
	0.29936312,
	0.10116832,
	-0.10116832,
	-0.29936312,
	-0.48530196,
	-0.65137248,
	-0.79077573,
	-0.89780453,
	-0.96807711,
	-0.99871650,
	-0.98846832,
	-0.93775213,
	-0.84864425,
	-0.72479278,
	-0.57126821,
	-0.39435585,
	-0.20129852,
	0.00000000
};

/* 32 values of cos function (still same result!) */
static const float cosval[CIRCLE_RESOL] = {
	1.00000000,
	0.97952994,
	0.91895781,
	0.82076344,
	0.68896691,
	0.52896401,
	0.34730525,
	0.15142777,
	-0.05064916,
	-0.25065253,
	-0.44039415,
	-0.61210598,
	-0.75875812,
	-0.87434661,
	-0.95413925,
	-0.99486932,
	-0.99486932,
	-0.95413925,
	-0.87434661,
	-0.75875812,
	-0.61210598,
	-0.44039415,
	-0.25065253,
	-0.05064916,
	0.15142777,
	0.34730525,
	0.52896401,
	0.68896691,
	0.82076344,
	0.91895781,
	0.97952994,
	1.00000000
};

/**
 * \param viewmat_local_unit is typically the 'rv3d->viewmatob'
 * copied into a 3x3 matrix and normalized.
 */
static void draw_xyz_wire(const float viewmat_local_unit[3][3], const float c[3], float size, int axis, unsigned pos)
{
	int line_type;
	float buffer[4][3];
	int n = 0;

	float v1[3] = {0.0f, 0.0f, 0.0f}, v2[3] = {0.0f, 0.0f, 0.0f};
	float dim = size * 0.1f;
	float dx[3], dy[3];

	dx[0] = dim;  dx[1] = 0.0f; dx[2] = 0.0f;
	dy[0] = 0.0f; dy[1] = dim;  dy[2] = 0.0f;

	switch (axis) {
		case 0:     /* x axis */
			line_type = GL_LINES;

			/* bottom left to top right */
			negate_v3_v3(v1, dx);
			sub_v3_v3(v1, dy);
			copy_v3_v3(v2, dx);
			add_v3_v3(v2, dy);

			copy_v3_v3(buffer[n++], v1);
			copy_v3_v3(buffer[n++], v2);
			
			/* top left to bottom right */
			mul_v3_fl(dy, 2.0f);
			add_v3_v3(v1, dy);
			sub_v3_v3(v2, dy);
			
			copy_v3_v3(buffer[n++], v1);
			copy_v3_v3(buffer[n++], v2);

			break;
		case 1:     /* y axis */
			line_type = GL_LINES;
			
			/* bottom left to top right */
			mul_v3_fl(dx, 0.75f);
			negate_v3_v3(v1, dx);
			sub_v3_v3(v1, dy);
			copy_v3_v3(v2, dx);
			add_v3_v3(v2, dy);
			
			copy_v3_v3(buffer[n++], v1);
			copy_v3_v3(buffer[n++], v2);
			
			/* top left to center */
			mul_v3_fl(dy, 2.0f);
			add_v3_v3(v1, dy);
			zero_v3(v2);
			
			copy_v3_v3(buffer[n++], v1);
			copy_v3_v3(buffer[n++], v2);
			
			break;
		case 2:     /* z axis */
			line_type = GL_LINE_STRIP;
			
			/* start at top left */
			negate_v3_v3(v1, dx);
			add_v3_v3(v1, dy);
			
			copy_v3_v3(buffer[n++], v1);
			
			mul_v3_fl(dx, 2.0f);
			add_v3_v3(v1, dx);

			copy_v3_v3(buffer[n++], v1);
			
			mul_v3_fl(dy, 2.0f);
			sub_v3_v3(v1, dx);
			sub_v3_v3(v1, dy);
			
			copy_v3_v3(buffer[n++], v1);
			
			add_v3_v3(v1, dx);
		
			copy_v3_v3(buffer[n++], v1);
			
			break;
		default:
			BLI_assert(0);
			return;
	}

	immBegin(line_type, n);
	for (int i = 0; i < n; i++) {
		mul_transposed_m3_v3((float (*)[3])viewmat_local_unit, buffer[i]);
		add_v3_v3(buffer[i], c);
		immVertex3fv(pos, buffer[i]);
	}
	immEnd();

	/* TODO: recode this function for clarity once we're not in a hurry to modernize GL usage */

#if 0
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, buffer);
	glDrawArrays(line_type, 0, n);
	glDisableClientState(GL_VERTEX_ARRAY);
#endif
}

void drawaxes(const float viewmat_local[4][4], float size, char drawtype, const unsigned char color[4])
{
	int axis;
	float v1[3] = {0.0, 0.0, 0.0};
	float v2[3] = {0.0, 0.0, 0.0};
	float v3[3] = {0.0, 0.0, 0.0};

	glLineWidth(1.0f);

	unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
	if (color) {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor4ubv(color);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_DEPTH_ONLY);
	}

	switch (drawtype) {
		case OB_PLAINAXES:
			immBegin(GL_LINES, 6);
			for (axis = 0; axis < 3; axis++) {
				v1[axis] = size;
				v2[axis] = -size;
				immVertex3fv(pos, v1);
				immVertex3fv(pos, v2);

				/* reset v1 & v2 to zero */
				v1[axis] = v2[axis] = 0.0f;
			}
			immEnd();
			break;

		case OB_SINGLE_ARROW:
			immBegin(GL_LINES, 2);
			/* in positive z direction only */
			v1[2] = size;
			immVertex3fv(pos, v1);
			immVertex3fv(pos, v2);
			immEnd();

			/* square pyramid */
			immBegin(GL_TRIANGLES, 12);

			v2[0] = size * 0.035f; v2[1] = size * 0.035f;
			v3[0] = size * -0.035f; v3[1] = size * 0.035f;
			v2[2] = v3[2] = size * 0.75f;

			for (axis = 0; axis < 4; axis++) {
				if (axis % 2 == 1) {
					v2[0] = -v2[0];
					v3[1] = -v3[1];
				}
				else {
					v2[1] = -v2[1];
					v3[0] = -v3[0];
				}

				immVertex3fv(pos, v1);
				immVertex3fv(pos, v2);
				immVertex3fv(pos, v3);
			}
			immEnd();
			break;

		case OB_CUBE:
			drawcube_size(size, pos);
			break;

		case OB_CIRCLE:
			drawcircle_size(size, pos);
			break;

		case OB_EMPTY_SPHERE:
			draw_empty_sphere(size, pos);
			break;

		case OB_EMPTY_CONE:
			draw_empty_cone(size, pos);
			break;

		case OB_ARROWS:
		default:
		{
			float viewmat_local_unit[3][3];

			copy_m3_m4(viewmat_local_unit, (float (*)[4])viewmat_local);
			normalize_m3(viewmat_local_unit);

			for (axis = 0; axis < 3; axis++) {
				const int arrow_axis = (axis == 0) ? 1 : 0;

				immBegin(GL_LINES, 6);

				v2[axis] = size;
				immVertex3fv(pos, v1);
				immVertex3fv(pos, v2);

				v1[axis] = size * 0.85f;
				v1[arrow_axis] = -size * 0.08f;
				immVertex3fv(pos, v1);
				immVertex3fv(pos, v2);

				v1[arrow_axis] = size * 0.08f;
				immVertex3fv(pos, v1);
				immVertex3fv(pos, v2);

				immEnd();

				v2[axis] += size * 0.125f;

				draw_xyz_wire(viewmat_local_unit, v2, size, axis, pos);

				/* reset v1 & v2 to zero */
				v1[arrow_axis] = v1[axis] = v2[axis] = 0.0f;
			}
		}
	}

	immUnbindProgram();
}


/* Function to draw an Image on an empty Object */
static void draw_empty_image(Object *ob, const short dflag, const unsigned char ob_wire_col[4], StereoViews sview)
{
	Image *ima = ob->data;

	const float ob_alpha = ob->col[3];
	float width, height;

	int bindcode = 0;

	if (ima) {
		ImageUser iuser = *ob->iuser;

		/* Support multi-view */
		if (ima && (sview == STEREO_RIGHT_ID)) {
			iuser.multiview_eye = sview;
			iuser.flag |= IMA_SHOW_STEREO;
			BKE_image_multiview_index(ima, &iuser);
		}

		if (ob_alpha > 0.0f) {
			bindcode = GPU_verify_image(ima, &iuser, GL_TEXTURE_2D, 0, false, false, false);
			/* don't bother drawing the image if alpha = 0 */
		}

		int w, h;
		BKE_image_get_size(ima, &iuser, &w, &h);
		width = w;
		height = h;
	}
	else {
		/* if no image, make it a 1x1 empty square, honor scale & offset */
		width = height = 1.0f;
	}

	const float aspect = height / width;

	float left = ob->ima_ofs[0];
	float right = ob->ima_ofs[0] + ob->empty_drawsize;
	float top = ob->ima_ofs[1] + ob->empty_drawsize * aspect;
	float bottom = ob->ima_ofs[1];

	bool use_blend = false;

	if (bindcode) {
		use_blend = ob_alpha < 1.0f || BKE_image_has_alpha(ima);

		if (use_blend) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		VertexFormat *format = immVertexFormat();
		unsigned pos = add_attrib(format, "pos", GL_FLOAT, 2, KEEP_FLOAT);
		unsigned texCoord = add_attrib(format, "texCoord", GL_FLOAT, 2, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_MODULATE_ALPHA);
		immUniform1f("alpha", ob_alpha);
		immUniform1i("image", 0); /* default GL_TEXTURE0 unit */

		immBegin(GL_TRIANGLE_FAN, 4);
		immAttrib2f(texCoord, 0.0f, 0.0f);
		immVertex2f(pos, left, bottom);

		immAttrib2f(texCoord, 1.0f, 0.0f);
		immVertex2f(pos, right, bottom);

		immAttrib2f(texCoord, 1.0f, 1.0f);
		immVertex2f(pos, right, top);

		immAttrib2f(texCoord, 0.0f, 1.0f);
		immVertex2f(pos, left, top);
		immEnd();

		immUnbindProgram();

		glBindTexture(GL_TEXTURE_2D, 0); /* necessary? */
	}

	/* Draw the image outline */
	glLineWidth(1.5f);
	unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 2, KEEP_FLOAT);

	const bool picking = dflag & DRAW_CONSTCOLOR;
	if (picking) {
		/* TODO: deal with picking separately, use this function just to draw */
		immBindBuiltinProgram(GPU_SHADER_3D_DEPTH_ONLY);
		if (use_blend) {
			glDisable(GL_BLEND);
		}

		imm_draw_line_box(pos, left, bottom, right, top);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor3ubv(ob_wire_col);
		glEnable(GL_LINE_SMOOTH);

		if (!use_blend) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		imm_draw_line_box(pos, left, bottom, right, top);

		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}

	immUnbindProgram();
}

static void circball_array_fill(float verts[CIRCLE_RESOL][3], const float cent[3], float rad, const float tmat[4][4])
{
	float vx[3], vy[3];
	float *viter = (float *)verts;

	mul_v3_v3fl(vx, tmat[0], rad);
	mul_v3_v3fl(vy, tmat[1], rad);

	for (unsigned int a = 0; a < CIRCLE_RESOL; a++, viter += 3) {
		viter[0] = cent[0] + sinval[a] * vx[0] + cosval[a] * vy[0];
		viter[1] = cent[1] + sinval[a] * vx[1] + cosval[a] * vy[1];
		viter[2] = cent[2] + sinval[a] * vx[2] + cosval[a] * vy[2];
	}
}

void imm_drawcircball(const float cent[3], float rad, const float tmat[4][4], unsigned pos)
{
	float verts[CIRCLE_RESOL][3];

	circball_array_fill(verts, cent, rad, tmat);

	immBegin(GL_LINE_LOOP, CIRCLE_RESOL);
	for (int i = 0; i < CIRCLE_RESOL; ++i) {
		immVertex3fv(pos, verts[i]);
	}
	immEnd();
}

/* circle for object centers, special_color is for library or ob users */
static void drawcentercircle(View3D *v3d, RegionView3D *UNUSED(rv3d), const float co[3], int selstate, bool special_color)
{
	const float outlineWidth = 1.0f * U.pixelsize;
	const float size = U.obcenter_dia * U.pixelsize + outlineWidth;

 	if (v3d->zbuf) {
		glDisable(GL_DEPTH_TEST);
		/* TODO(merwin): fit things like this into plates/buffers design */
	}

	glEnable(GL_BLEND);
	GPU_enable_program_point_size();

	unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);
	immUniform1f("size", size);

	if (special_color) {
		if (selstate == ACTIVE || selstate == SELECT) immUniformColor4ub(0x88, 0xFF, 0xFF, 155);
		else immUniformColor4ub(0x55, 0xCC, 0xCC, 155);
	}
	else {
		if (selstate == ACTIVE) immUniformThemeColorShadeAlpha(TH_ACTIVE, 0, -80);
		else if (selstate == SELECT) immUniformThemeColorShadeAlpha(TH_SELECT, 0, -80);
		else if (selstate == DESELECT) immUniformThemeColorShadeAlpha(TH_TRANSFORM, 0, -80);
	}

	/* set up outline */
	float outlineColor[4];
	UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, outlineColor);
	immUniform4fv("outlineColor", outlineColor);
	immUniform1f("outlineWidth", outlineWidth);

	immBegin(GL_POINTS, 1);
	immVertex3fv(pos, co);
	immEnd();

	immUnbindProgram();

	GPU_disable_program_point_size();
	glDisable(GL_BLEND);

 	if (v3d->zbuf) {
		glEnable(GL_DEPTH_TEST);
	}
}

/* *********** text drawing for object/particles/armature ************* */

typedef struct ViewCachedString {
	struct ViewCachedString *next;
	float vec[3];
	union {
		unsigned char ub[4];
		int pack;
	} col;
	short sco[2];
	short xoffs;
	short flag;
	int str_len;

	/* str is allocated past the end */
	char str[0];
} ViewCachedString;

/* one arena for all 3 string lists */
static MemArena         *g_v3d_strings_arena = NULL;
static ViewCachedString *g_v3d_strings[3] = {NULL, NULL, NULL};
static int g_v3d_string_level = -1;

void view3d_cached_text_draw_begin(void)
{
	g_v3d_string_level++;

	BLI_assert(g_v3d_string_level >= 0);

	if (g_v3d_string_level == 0) {
		BLI_assert(g_v3d_strings_arena == NULL);
	}
}

void view3d_cached_text_draw_add(const float co[3],
                                 const char *str, const size_t str_len,
                                 short xoffs, short flag,
                                 const unsigned char col[4])
{
	int alloc_len = str_len + 1;
	ViewCachedString *vos;

	BLI_assert(str_len == strlen(str));

	if (g_v3d_strings_arena == NULL) {
		g_v3d_strings_arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 14), __func__);
	}

	vos = BLI_memarena_alloc(g_v3d_strings_arena, sizeof(ViewCachedString) + alloc_len);

	BLI_LINKS_PREPEND(g_v3d_strings[g_v3d_string_level], vos);

	copy_v3_v3(vos->vec, co);
	copy_v4_v4_uchar(vos->col.ub, col);
	vos->xoffs = xoffs;
	vos->flag = flag;
	vos->str_len = str_len;

	/* allocate past the end */
	memcpy(vos->str, str, alloc_len);
}

void view3d_cached_text_draw_end(View3D *v3d, ARegion *ar, bool depth_write, float mat[4][4])
{
	RegionView3D *rv3d = ar->regiondata;
	ViewCachedString *vos;
	int tot = 0;
	
	BLI_assert(g_v3d_string_level >= 0 && g_v3d_string_level <= 2);

	/* project first and test */
	for (vos = g_v3d_strings[g_v3d_string_level]; vos; vos = vos->next) {
		if (mat && !(vos->flag & V3D_CACHE_TEXT_WORLDSPACE))
			mul_m4_v3(mat, vos->vec);

		if (ED_view3d_project_short_ex(ar,
		                               (vos->flag & V3D_CACHE_TEXT_GLOBALSPACE) ? rv3d->persmat : rv3d->persmatob,
		                               (vos->flag & V3D_CACHE_TEXT_LOCALCLIP) != 0,
		                               vos->vec, vos->sco,
		                               V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
		{
			tot++;
		}
		else {
			vos->sco[0] = IS_CLIPPED;
		}
	}

	if (tot) {
		int col_pack_prev = 0;

		if (rv3d->rflag & RV3D_CLIPPING) {
			ED_view3d_clipping_disable();
		}

		float original_proj[4][4];
		gpuGetProjectionMatrix3D(original_proj);
		wmOrtho2_region_pixelspace(ar);

		gpuPushMatrix();
		gpuLoadIdentity();
		
		if (depth_write) {
			if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
		}
		else {
			glDepthMask(GL_FALSE);
		}
		
		for (vos = g_v3d_strings[g_v3d_string_level]; vos; vos = vos->next) {
			if (vos->sco[0] != IS_CLIPPED) {
				if (col_pack_prev != vos->col.pack) {
					//glColor3ubv(vos->col.ub);
					col_pack_prev = vos->col.pack;
				}

				((vos->flag & V3D_CACHE_TEXT_ASCII) ?
				 BLF_draw_default_ascii :
				 BLF_draw_default
				 )((float)(vos->sco[0] + vos->xoffs),
				   (float)(vos->sco[1]),
				   (depth_write) ? 0.0f : 2.0f,
				   vos->str,
				   vos->str_len);
			}
		}

		if (depth_write) {
			if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
		}
		else {
			glDepthMask(GL_TRUE);
		}
		
		gpuPopMatrix();
		gpuLoadProjectionMatrix3D(original_proj); /* TODO: make this more 2D friendly */

		if (rv3d->rflag & RV3D_CLIPPING) {
			ED_view3d_clipping_enable();
		}
	}

	g_v3d_strings[g_v3d_string_level] = NULL;

	if (g_v3d_string_level == 0) {
		if (g_v3d_strings_arena) {
			BLI_memarena_free(g_v3d_strings_arena);
			g_v3d_strings_arena = NULL;
		}
	}

	g_v3d_string_level--;
}

/* ******************** primitive drawing ******************* */

/* draws a cube given the scaling of the cube, assuming that
 * all required matrices have been set (used for drawing empties)
 */
static void drawcube_size(float size, unsigned pos)
{
	const GLfloat verts[8][3] = {
		{-size, -size, -size},
		{-size, -size,  size},
		{-size,  size, -size},
		{-size,  size,  size},
		{ size, -size, -size},
		{ size, -size,  size},
		{ size,  size, -size},
		{ size,  size,  size}
	};

	const GLubyte indices[24] = {0,1,1,3,3,2,2,0,0,4,4,5,5,7,7,6,6,4,1,5,3,7,2,6};

#if 0
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawRangeElements(GL_LINES, 0, 7, 24, GL_UNSIGNED_BYTE, indices);
	glDisableClientState(GL_VERTEX_ARRAY);
#else
	immBegin(GL_LINES, 24);
	for (int i = 0; i < 24; ++i) {
		immVertex3fv(pos, verts[indices[i]]);
	}
	immEnd();
#endif
}

static void drawshadbuflimits(const Lamp *la, const float mat[4][4], unsigned pos)
{
	float sta[3], end[3], lavec[3];

	negate_v3_v3(lavec, mat[2]);
	normalize_v3(lavec);

	madd_v3_v3v3fl(sta, mat[3], lavec, la->clipsta);
	madd_v3_v3v3fl(end, mat[3], lavec, la->clipend);

	immBegin(GL_LINES, 2);
	immVertex3fv(pos, sta);
	immVertex3fv(pos, end);
	immEnd();

	glPointSize(3.0f);
	immBegin(GL_POINTS, 2);
	immVertex3fv(pos, sta);
	immVertex3fv(pos, end);
	immEnd();
}

static void spotvolume(float lvec[3], float vvec[3], const float inp)
{
	/* camera is at 0,0,0 */
	float temp[3], plane[3], mat1[3][3], mat2[3][3], mat3[3][3], mat4[3][3], q[4], co, si, angle;

	normalize_v3(lvec);
	normalize_v3(vvec);             /* is this the correct vector ? */

	cross_v3_v3v3(temp, vvec, lvec);      /* equation for a plane through vvec and lvec */
	cross_v3_v3v3(plane, lvec, temp);     /* a plane perpendicular to this, parallel with lvec */

	/* vectors are exactly aligned, use the X axis, this is arbitrary */
	if (normalize_v3(plane) == 0.0f)
		plane[1] = 1.0f;

	/* now we've got two equations: one of a cone and one of a plane, but we have
	 * three unknowns. We remove one unknown by rotating the plane to z=0 (the plane normal) */

	/* rotate around cross product vector of (0,0,1) and plane normal, dot product degrees */
	/* according definition, we derive cross product is (plane[1],-plane[0],0), en cos = plane[2]);*/

	/* translating this comment to english didnt really help me understanding the math! :-) (ton) */
	
	q[1] =  plane[1];
	q[2] = -plane[0];
	q[3] =  0;
	normalize_v3(&q[1]);

	angle = saacos(plane[2]) / 2.0f;
	co = cosf(angle);
	si = sqrtf(1 - co * co);

	q[0] =  co;
	q[1] *= si;
	q[2] *= si;
	q[3] =  0;

	quat_to_mat3(mat1, q);

	/* rotate lamp vector now over acos(inp) degrees */
	copy_v3_v3(vvec, lvec);

	unit_m3(mat2);
	co = inp;
	si = sqrtf(1.0f - inp * inp);

	mat2[0][0] =  co;
	mat2[1][0] = -si;
	mat2[0][1] =  si;
	mat2[1][1] =  co;
	mul_m3_m3m3(mat3, mat2, mat1);

	mat2[1][0] =  si;
	mat2[0][1] = -si;
	mul_m3_m3m3(mat4, mat2, mat1);
	transpose_m3(mat1);

	mul_m3_m3m3(mat2, mat1, mat3);
	mul_m3_v3(mat2, lvec);
	mul_m3_m3m3(mat2, mat1, mat4);
	mul_m3_v3(mat2, vvec);
}

static void draw_spot_cone(Lamp *la, float x, float z, unsigned pos)
{
	z = fabsf(z);

	const bool square = (la->mode & LA_SQUARE);

	immBegin(GL_TRIANGLE_FAN, square ? 6 : 34);
	immVertex3f(pos, 0.0f, 0.0f, -x);

	if (square) {
		immVertex3f(pos, z, z, 0);
		immVertex3f(pos, -z, z, 0);
		immVertex3f(pos, -z, -z, 0);
		immVertex3f(pos, z, -z, 0);
		immVertex3f(pos, z, z, 0);
	}
	else {
		for (int a = 0; a < 33; a++) {
			float angle = a * M_PI * 2 / (33 - 1);
			immVertex3f(pos, z * cosf(angle), z * sinf(angle), 0.0f);
		}
	}

	immEnd();
}

static void draw_transp_spot_volume(Lamp *la, float x, float z, unsigned pos)
{
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);

	/* draw backside darkening */
	glCullFace(GL_FRONT);

	glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
	immUniformColor4f(0.0f, 0.0f, 0.0f, 0.4f);

	draw_spot_cone(la, x, z, pos);

	/* draw front side lighting */
	glCullFace(GL_BACK);

	glBlendFunc(GL_ONE, GL_ONE);
	immUniformColor4f(0.2f, 0.2f, 0.2f, 1.0f);

	draw_spot_cone(la, x, z, pos);

	/* restore state */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);
}

#ifdef WITH_GAMEENGINE
static void draw_transp_sun_volume(Lamp *la, unsigned pos)
{
	float box[8][3];

	/* construct box */
	box[0][0] = box[1][0] = box[2][0] = box[3][0] = -la->shadow_frustum_size;
	box[4][0] = box[5][0] = box[6][0] = box[7][0] = +la->shadow_frustum_size;
	box[0][1] = box[1][1] = box[4][1] = box[5][1] = -la->shadow_frustum_size;
	box[2][1] = box[3][1] = box[6][1] = box[7][1] = +la->shadow_frustum_size;
	box[0][2] = box[3][2] = box[4][2] = box[7][2] = -la->clipend;
	box[1][2] = box[2][2] = box[5][2] = box[6][2] = -la->clipsta;

	/* draw edges */
	imm_draw_box(box, false, pos);

	/* draw faces */
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);

	/* draw backside darkening */
	glCullFace(GL_FRONT);

	glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
	immUniformColor4f(0.0f, 0.0f, 0.0f, 0.4f);

	imm_draw_box(box, true, pos);

	/* draw front side lighting */
	glCullFace(GL_BACK);

	glBlendFunc(GL_ONE, GL_ONE);
	immUniformColor4f(0.2f, 0.2f, 0.2f, 1.0f);

	imm_draw_box(box, true, pos);

	/* restore state */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);
}
#endif

void drawlamp(View3D *v3d, RegionView3D *rv3d, Base *base,
              const char dt, const short dflag, const unsigned char ob_wire_col[4], const bool is_obact)
{
	Object *ob = base->object;
	const float pixsize = ED_view3d_pixel_size(rv3d, ob->obmat[3]);
	Lamp *la = ob->data;
	float vec[3], lvec[3], vvec[3], circrad;
	float imat[4][4];

	/* cone can't be drawn for duplicated lamps, because duplilist would be freed */
	/* the moment of view3d_draw_transp() call */
	const bool is_view = (rv3d->persp == RV3D_CAMOB && v3d->camera == base->object);
	const bool drawcone = ((dt > OB_WIRE) &&
	                       !(G.f & G_PICKSEL) &&
	                       (la->type == LA_SPOT) &&
	                       (la->mode & LA_SHOW_CONE) &&
	                       !(base->flag_legacy & OB_FROMDUPLI) &&
	                       !is_view);

#ifdef WITH_GAMEENGINE
	const bool drawshadowbox = (
	        (rv3d->rflag & RV3D_IS_GAME_ENGINE) &&
	        (dt > OB_WIRE) &&
	        !(G.f & G_PICKSEL) &&
	        (la->type == LA_SUN) &&
	        ((la->mode & LA_SHAD_BUF) || 
	        (la->mode & LA_SHAD_RAY)) &&
	        (la->mode & LA_SHOW_SHADOW_BOX) &&
	        !(base->flag_legacy & OB_FROMDUPLI) &&
	        !is_view);
#else
	const bool drawshadowbox = false;
#endif

	if ((drawcone || drawshadowbox) && !v3d->transp) {
		/* in this case we need to draw delayed */
		ED_view3d_after_add(&v3d->afterdraw_transp, base, dflag);
		return;
	}

	/* we first draw only the screen aligned & fixed scale stuff */
	gpuPushMatrix();
	gpuLoadMatrix3D(rv3d->viewmat);

	/* lets calculate the scale: */
	const float lampsize_px = U.obcenter_dia;
	const float lampsize = pixsize * lampsize_px * 0.5f;

	/* and view aligned matrix: */
	copy_m4_m4(imat, rv3d->viewinv);
	normalize_v3(imat[0]);
	normalize_v3(imat[1]);

	const unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

	/* lamp center */
	copy_v3_v3(vec, ob->obmat[3]);

	float curcol[4];
	if ((dflag & DRAW_CONSTCOLOR) == 0) {
		/* for AA effects */
		rgb_uchar_to_float(curcol, ob_wire_col);
		curcol[3] = 0.6f;
		/* TODO: pay attention to GL_BLEND */
	}

	glLineWidth(1.0f);
	setlinestyle(3);

	if (lampsize > 0.0f) {
		const float outlineWidth = 1.5f * U.pixelsize;
		const float lampdot_size = lampsize_px * U.pixelsize + outlineWidth;

		/* Inner Circle */
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			const float *color = curcol;
			if (ob->id.us > 1) {
				if (is_obact || ((base->flag & BASE_SELECTED) != 0)) {
					static const float active_color[4] = {0.533f, 1.0f, 1.0f, 1.0f};
					color = active_color;
				}
				else {
					static const float inactive_color[4] = {0.467f, 0.8f, 0.8f, 1.0f};
					color = inactive_color;
				}
			}

			GPU_enable_program_point_size();
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA);
			immUniform1f("size", lampdot_size);
			immUniform1f("outlineWidth", outlineWidth);
			immUniformColor3fvAlpha(color, 0.3f);
			immUniform4fv("outlineColor", color);

			immBegin(GL_POINTS, 1);
			immVertex3fv(pos, vec);
			immEnd();

			immUnbindProgram();

			glDisable(GL_BLEND);
			GPU_disable_program_point_size();
		}
		else {
			/* CONSTCOLOR in effect */
			/* TODO: separate picking from drawing */
			immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_UNIFORM_COLOR);
			/* color doesn't matter, so don't set */
			glPointSize(lampdot_size);

			immBegin(GL_POINTS, 1);
			immVertex3fv(pos, vec);
			immEnd();

			immUnbindProgram();
		}

		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		/* TODO(merwin): short term, use DEPTH_ONLY for picking
		 *               long term, separate picking from drawing
		 */

		/* restore */
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			immUniformColor4fv(curcol);
		}

		/* Outer circle */
		circrad = 3.0f * lampsize;

		imm_drawcircball(vec, circrad, imat, pos);

		/* draw dashed outer circle if shadow is on. remember some lamps can't have certain shadows! */
		if (la->type != LA_HEMI) {
			if ((la->mode & LA_SHAD_RAY) || ((la->mode & LA_SHAD_BUF) && (la->type == LA_SPOT))) {
				imm_drawcircball(vec, circrad + 3.0f * pixsize, imat, pos);
			}
		}
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			immUniformColor4fv(curcol);
		}
		circrad = 0.0f;
	}

	/* draw the pretty sun rays */
	if (la->type == LA_SUN) {
		float v1[3], v2[3], mat[3][3];
		short axis;

		/* setup a 45 degree rotation matrix */
		axis_angle_normalized_to_mat3_ex(mat, imat[2], M_SQRT1_2, M_SQRT1_2);

		/* vectors */
		mul_v3_v3fl(v1, imat[0], circrad * 1.2f);
		mul_v3_v3fl(v2, imat[0], circrad * 2.5f);

		/* center */
		gpuPushMatrix();
		gpuTranslate3fv(vec);

		setlinestyle(3);

		immBegin(GL_LINES, 16);
		for (axis = 0; axis < 8; axis++) {
			immVertex3fv(pos, v1);
			immVertex3fv(pos, v2);
			mul_m3_v3(mat, v1);
			mul_m3_v3(mat, v2);
		}
		immEnd();

		gpuPopMatrix();
	}

	if (la->type == LA_LOCAL) {
		if (la->mode & LA_SPHERE) {
			imm_drawcircball(vec, la->dist, imat, pos);
		}
	}

	gpuPopMatrix();  /* back in object space */
	zero_v3(vec);

	if (is_view) {
		/* skip drawing extra info */
	}
	else if (la->type == LA_SPOT) {
		float x, y, z, z_abs;
		copy_v3_fl3(lvec, 0.0f, 0.0f, 1.0f);
		copy_v3_fl3(vvec, rv3d->persmat[0][2], rv3d->persmat[1][2], rv3d->persmat[2][2]);
		mul_transposed_mat3_m4_v3(ob->obmat, vvec);

		x = -la->dist;
		y = cosf(la->spotsize * 0.5f);
		z = x * sqrtf(1.0f - y * y);

		spotvolume(lvec, vvec, y);
		mul_v3_fl(lvec, x);
		mul_v3_fl(vvec, x);

		x *= y;

		z_abs = fabsf(z);

		if (la->mode & LA_SQUARE) {
			/* draw pyramid */
			const float vertices[5][3] = {
			    /* 5 of vertex coords of pyramid */
			    {0.0f, 0.0f, 0.0f},
			    {z_abs, z_abs, x},
			    {-z_abs, -z_abs, x},
			    {z_abs, -z_abs, x},
			    {-z_abs, z_abs, x},
			};

			immBegin(GL_LINES, 16);
			for (int i = 1; i <= 4; ++i) {
				immVertex3fv(pos, vertices[0]); /* apex to corner */
				immVertex3fv(pos, vertices[i]);
				int next_i = (i == 4) ? 1 : (i + 1);
				immVertex3fv(pos, vertices[i]); /* corner to next corner */
				immVertex3fv(pos, vertices[next_i]);
			}
			immEnd();

			gpuTranslate3f(0.0f, 0.0f, x);

			/* draw the square representing spotbl */
			if (la->type == LA_SPOT) {
				float blend = z_abs * (1.0f - pow2f(la->spotblend));

				/* hide line if it is zero size or overlaps with outer border,
				 * previously it adjusted to always to show it but that seems
				 * confusing because it doesn't show the actual blend size */
				if (blend != 0.0f && blend != z_abs) {
					imm_draw_line_box_3D(pos, blend, -blend, -blend, blend);
				}
			}
		}
		else {
			/* draw the angled sides of the cone */
			immBegin(GL_LINE_STRIP, 3);
			immVertex3fv(pos, vvec);
			immVertex3fv(pos, vec);
			immVertex3fv(pos, lvec);
			immEnd();

			/* draw the circle at the end of the cone */
			gpuTranslate3f(0.0f, 0.0f, x);
			imm_draw_lined_circle_3D(pos, 0.0f, 0.0f, z_abs, 32);

			/* draw the circle representing spotbl */
			if (la->type == LA_SPOT) {
				float blend = z_abs * (1.0f - pow2f(la->spotblend));

				/* hide line if it is zero size or overlaps with outer border,
				 * previously it adjusted to always to show it but that seems
				 * confusing because it doesn't show the actual blend size */
				if (blend != 0.0f && blend != z_abs) {
					imm_draw_lined_circle_3D(pos, 0.0f, 0.0f, blend, 32);
				}
			}
		}

		if (drawcone)
			draw_transp_spot_volume(la, x, z, pos);

		/* draw clip start, useful for wide cones where its not obvious where the start is */
		gpuTranslate3f(0.0f, 0.0f, -x);  /* reverse translation above */
		immBegin(GL_LINES, 2);
		if (la->type == LA_SPOT && (la->mode & LA_SHAD_BUF)) {
			float lvec_clip[3];
			float vvec_clip[3];
			float clipsta_fac = la->clipsta / -x;

			interp_v3_v3v3(lvec_clip, vec, lvec, clipsta_fac);
			interp_v3_v3v3(vvec_clip, vec, vvec, clipsta_fac);

			immVertex3fv(pos, lvec_clip);
			immVertex3fv(pos, vvec_clip);
		}
		/* Else, draw spot direction (using distance as end limit, same as for Area lamp). */
		else {
			immVertex3f(pos, 0.0f, 0.0f, -circrad);
			immVertex3f(pos, 0.0f, 0.0f, -la->dist);
		}
		immEnd();
	}
	else if (ELEM(la->type, LA_HEMI, LA_SUN)) {
		/* draw the line from the circle along the dist */
		immBegin(GL_LINES, 2);
		vec[2] = -circrad;
		immVertex3fv(pos, vec);
		vec[2] = -la->dist;
		immVertex3fv(pos, vec);
		immEnd();

		if (la->type == LA_HEMI) {
			/* draw the hemisphere curves */
			short axis, steps, dir;
			float outdist, zdist, mul;
			zero_v3(vec);
			outdist = 0.14f; mul = 1.4f; dir = 1;

			setlinestyle(4);
			/* loop over the 4 compass points, and draw each arc as a LINE_STRIP */
			for (axis = 0; axis < 4; axis++) {
				float v[3] = {0.0f, 0.0f, 0.0f};
				zdist = 0.02f;

				immBegin(GL_LINE_STRIP, 6);

				for (steps = 0; steps < 6; steps++) {
					if (axis == 0 || axis == 1) {       /* x axis up, x axis down */
						/* make the arcs start at the edge of the energy circle */
						if (steps == 0) v[0] = dir * circrad;
						else v[0] = v[0] + dir * (steps * outdist);
					}
					else if (axis == 2 || axis == 3) {      /* y axis up, y axis down */
						/* make the arcs start at the edge of the energy circle */
						v[1] = (steps == 0) ? (dir * circrad) : (v[1] + dir * (steps * outdist));
					}

					v[2] = v[2] - steps * zdist;

					immVertex3fv(pos, v);

					zdist = zdist * mul;
				}

				immEnd();
				/* flip the direction */
				dir = -dir;
			}
		}

#ifdef WITH_GAMEENGINE
		if (drawshadowbox) {
			draw_transp_sun_volume(la, pos);
		}
#endif
	}
	else if (la->type == LA_AREA) {
		setlinestyle(3);
		if (la->area_shape == LA_AREA_SQUARE)
			imm_draw_line_box_3D(pos, -la->area_size * 0.5f, -la->area_size * 0.5f, la->area_size * 0.5f, la->area_size * 0.5f);
		else if (la->area_shape == LA_AREA_RECT)
			imm_draw_line_box_3D(pos, -la->area_size * 0.5f, -la->area_sizey * 0.5f, la->area_size * 0.5f, la->area_sizey * 0.5f);

		immBegin(GL_LINES, 2);
		immVertex3f(pos, 0.0f, 0.0f, -circrad);
		immVertex3f(pos, 0.0f, 0.0f, -la->dist);
		immEnd();
	}

	/* and back to viewspace */
	gpuPushMatrix();
	gpuLoadMatrix3D(rv3d->viewmat);
	copy_v3_v3(vec, ob->obmat[3]);

	setlinestyle(0);

	if ((la->type == LA_SPOT) && (la->mode & LA_SHAD_BUF) && (is_view == false)) {
		drawshadbuflimits(la, ob->obmat, pos);
	}

	if ((dflag & DRAW_CONSTCOLOR) == 0) {
		immUniformThemeColor(TH_LAMP);
	}

	glEnable(GL_BLEND);

	if (vec[2] > 0) vec[2] -= circrad;
	else vec[2] += circrad;

	immBegin(GL_LINES, 2);
	immVertex3fv(pos, vec);
	vec[2] = 0;
	immVertex3fv(pos, vec);
	immEnd();

	glPointSize(2.0f);
	immBegin(GL_POINTS, 1);
	immVertex3fv(pos, vec);
	immEnd();

	glDisable(GL_BLEND);

	immUnbindProgram();
	gpuPopMatrix();
}

static void draw_limit_line(float sta, float end, const short dflag, const unsigned char col[3], unsigned pos)
{
	immBegin(GL_LINES, 2);
	immVertex3f(pos, 0.0f, 0.0f, -sta);
	immVertex3f(pos, 0.0f, 0.0f, -end);
	immEnd();

	if (!(dflag & DRAW_PICKING)) {
		glPointSize(3.0f);
		/* would like smooth round points here, but that means binding another shader...
		 * if it's really desired, pull these points into their own function to be called after */
		immBegin(GL_POINTS, 2);
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			immUniformColor3ubv(col);
		}
		immVertex3f(pos, 0.0f, 0.0f, -sta);
		immVertex3f(pos, 0.0f, 0.0f, -end);
		immEnd();
	}
}


/* yafray: draw camera focus point (cross, similar to aqsis code in tuhopuu) */
/* qdn: now also enabled for Blender to set focus point for defocus composite node */
static void draw_focus_cross(float dist, float size, unsigned pos)
{
	immBegin(GL_LINES, 4);
	immVertex3f(pos, -size, 0.0f, -dist);
	immVertex3f(pos, size, 0.0f, -dist);
	immVertex3f(pos, 0.0f, -size, -dist);
	immVertex3f(pos, 0.0f, size, -dist);
	immEnd();
}

#ifdef VIEW3D_CAMERA_BORDER_HACK
unsigned char view3d_camera_border_hack_col[3];
bool view3d_camera_border_hack_test = false;
#endif

/* ****************** draw clip data *************** */

static void draw_viewport_object_reconstruction(
        Scene *scene, BaseLegacy *base, const View3D *v3d, const RegionView3D *rv3d,
        MovieClip *clip, MovieTrackingObject *tracking_object,
        const short dflag, const unsigned char ob_wire_col[4],
        int *global_track_index, bool draw_selected)
{
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	float mat[4][4], imat[4][4];
	unsigned char col_unsel[4], col_sel[4];
	int tracknr = *global_track_index;
	ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
	float camera_size[3];

	UI_GetThemeColor4ubv(TH_TEXT, col_unsel);
	UI_GetThemeColor4ubv(TH_SELECT, col_sel);

	BKE_tracking_get_camera_object_matrix(scene, base->object, mat);

	/* we're compensating camera size for bundles size,
	 * to make it so bundles are always displayed with the same size */
	copy_v3_v3(camera_size, base->object->size);
	if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0)
		mul_v3_fl(camera_size, tracking_object->scale);

	gpuPushMatrix();

	if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
		/* current ogl matrix is translated in camera space, bundles should
		 * be rendered in world space, so camera matrix should be "removed"
		 * from current ogl matrix */
		invert_m4_m4(imat, base->object->obmat);

		gpuMultMatrix3D(imat);
		gpuMultMatrix3D(mat);
	}
	else {
		float obmat[4][4];
		int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, scene->r.cfra);

		BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object, framenr, obmat);

		invert_m4_m4(imat, obmat);
		gpuMultMatrix3D(imat);
	}

	for (track = tracksbase->first; track; track = track->next) {
		bool selected = TRACK_SELECTED(track);

		if (draw_selected && !selected)
			continue;

		if ((track->flag & TRACK_HAS_BUNDLE) == 0)
			continue;

		if (dflag & DRAW_PICKING)
			GPU_select_load_id(base->selcol + (tracknr << 16));

		gpuPushMatrix();
		gpuTranslate3fv(track->bundle_pos);
		gpuScale3f(v3d->bundle_size / 0.05f / camera_size[0],
		           v3d->bundle_size / 0.05f / camera_size[1],
		           v3d->bundle_size / 0.05f / camera_size[2]);

		const int v3d_drawtype = view3d_effective_drawtype(v3d);
		if (v3d_drawtype == OB_WIRE) {
			unsigned char color[4];
			const unsigned char *color_ptr = NULL;
			if ((dflag & DRAW_CONSTCOLOR) == 0) {
				if (selected && (track->flag & TRACK_CUSTOMCOLOR) == 0) {
					color_ptr = ob_wire_col;
				}
				else {
					rgba_float_to_uchar(color, track->color);
					color_ptr = color;
				}
			}

			drawaxes(rv3d->viewmatob, 0.05f, v3d->bundle_drawtype, color_ptr);
		}
		else if (v3d_drawtype > OB_WIRE) {
			if (v3d->bundle_drawtype == OB_EMPTY_SPHERE) {
				Batch *batch;

				gpuScaleUniform(0.05f);

				/* selection outline */
				if (selected) {
					batch = Batch_get_sphere_wire(1);

					if ((dflag & DRAW_CONSTCOLOR) == 0) {
						Batch_set_builtin_program(batch, GPU_SHADER_3D_UNIFORM_COLOR);
						Batch_Uniform4f(batch, "color",
						                ob_wire_col[0]/255.f,
						                ob_wire_col[1]/255.f,
						                ob_wire_col[2]/255.f, 1.0f);
					}
					else {
						Batch_set_builtin_program(batch, GPU_SHADER_3D_DEPTH_ONLY);
					}
					glLineWidth(2.0f);

					Batch_draw(batch);
				}

				batch = Batch_get_sphere(0);

				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					const float light[3] = {0.0f, 0.0f, 1.0f};
					float col[3];
					Batch_set_builtin_program(batch, GPU_SHADER_SIMPLE_LIGHTING);
					Batch_Uniform3fv(batch, "light", light);

					if (track->flag & TRACK_CUSTOMCOLOR) copy_v3_v3(col, track->color);
					else UI_GetThemeColor3fv(TH_BUNDLE_SOLID, col);
					Batch_Uniform4f(batch, "color", col[0], col[1], col[2], 1.0f);
				}
				else {
					Batch_set_builtin_program(batch, GPU_SHADER_3D_DEPTH_ONLY);
				}

				Batch_draw(batch);
			}
			else {
				unsigned char color[4];
				const unsigned char *color_ptr = NULL;
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					if (selected) {
						color_ptr = ob_wire_col;
					}
					else {
						if (track->flag & TRACK_CUSTOMCOLOR) rgba_float_to_uchar(color, track->color);
						else UI_GetThemeColor4ubv(TH_WIRE, color);

						color_ptr = color;
					}
				}

				drawaxes(rv3d->viewmatob, 0.05f, v3d->bundle_drawtype, color_ptr);
			}
		}

		gpuPopMatrix();

		if ((dflag & DRAW_PICKING) == 0 && (v3d->flag2 & V3D_SHOW_BUNDLENAME)) {
			float pos[3];

			mul_v3_m4v3(pos, mat, track->bundle_pos);
			view3d_cached_text_draw_add(pos,
			                            track->name, strlen(track->name),
			                            10, V3D_CACHE_TEXT_GLOBALSPACE,
			                            selected ? col_sel : col_unsel);
		}

		tracknr++;
	}

	if ((dflag & DRAW_PICKING) == 0) {
		if ((v3d->flag2 & V3D_SHOW_CAMERAPATH) && (tracking_object->flag & TRACKING_OBJECT_CAMERA)) {
			MovieTrackingReconstruction *reconstruction;
			reconstruction = BKE_tracking_object_get_reconstruction(tracking, tracking_object);

			if (reconstruction->camnr >= 2) {
				MovieReconstructedCamera *camera = reconstruction->cameras;
				unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

				immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
				immUniformThemeColor(TH_CAMERA_PATH);

				glLineWidth(2.0f);

				immBegin(GL_LINE_STRIP, reconstruction->camnr);
				for (int a = 0; a < reconstruction->camnr; a++, camera++) {
					immVertex3fv(pos, camera->mat[3]);
				}
				immEnd();

				immUnbindProgram();
			}
		}
	}

	gpuPopMatrix();

	*global_track_index = tracknr;
}

static void draw_viewport_reconstruction(
        Scene *scene, BaseLegacy *base, const View3D *v3d, const RegionView3D *rv3d, MovieClip *clip,
        const short dflag, const unsigned char ob_wire_col[4],
        const bool draw_selected)
{
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *tracking_object;
	int global_track_index = 1;

	if ((v3d->flag2 & V3D_SHOW_RECONSTRUCTION) == 0)
		return;

	if (v3d->flag2 & V3D_RENDER_OVERRIDE)
		return;

	GPU_basic_shader_colors(NULL, NULL, 0, 1.0f);
	GPU_basic_shader_bind(GPU_SHADER_LIGHTING | GPU_SHADER_USE_COLOR);

	tracking_object = tracking->objects.first;
	while (tracking_object) {
		draw_viewport_object_reconstruction(
		        scene, base, v3d, rv3d, clip, tracking_object,
		        dflag, ob_wire_col, &global_track_index, draw_selected);

		tracking_object = tracking_object->next;
	}

	/* restore */
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);

	if (dflag & DRAW_PICKING)
		GPU_select_load_id(base->selcol);
}

/* camera frame */
static void drawcamera_frame(float vec[4][3], bool filled, unsigned pos)
{
	immBegin(filled ? GL_QUADS : GL_LINE_LOOP, 4);
	immVertex3fv(pos, vec[0]);
	immVertex3fv(pos, vec[1]);
	immVertex3fv(pos, vec[2]);
	immVertex3fv(pos, vec[3]);
	immEnd();
}

/* center point to camera frame */
static void drawcamera_framelines(float vec[4][3], float origin[3], unsigned pos)
{
	immBegin(GL_LINES, 8);
	immVertex3fv(pos, origin);
	immVertex3fv(pos, vec[0]);
	immVertex3fv(pos, origin);
	immVertex3fv(pos, vec[1]);
	immVertex3fv(pos, origin);
	immVertex3fv(pos, vec[2]);
	immVertex3fv(pos, origin);
	immVertex3fv(pos, vec[3]);
	immEnd();
}

static void drawcamera_volume(float near_plane[4][3], float far_plane[4][3], bool filled, unsigned pos)
{
	drawcamera_frame(near_plane, filled, pos);
	drawcamera_frame(far_plane, filled, pos);

	if (filled) {
		immBegin(GL_QUADS, 16); /* TODO(merwin): use GL_TRIANGLE_STRIP here */
		immVertex3fv(pos, near_plane[0]);
		immVertex3fv(pos, far_plane[0]);
		immVertex3fv(pos, far_plane[1]);
		immVertex3fv(pos, near_plane[1]);

		immVertex3fv(pos, near_plane[1]);
		immVertex3fv(pos, far_plane[1]);
		immVertex3fv(pos, far_plane[2]);
		immVertex3fv(pos, near_plane[2]);

		immVertex3fv(pos, near_plane[2]);
		immVertex3fv(pos, near_plane[1]);
		immVertex3fv(pos, far_plane[1]);
		immVertex3fv(pos, far_plane[2]);

		immVertex3fv(pos, far_plane[0]);
		immVertex3fv(pos, near_plane[0]);
		immVertex3fv(pos, near_plane[3]);
		immVertex3fv(pos, far_plane[3]);
		immEnd();
	}
	else {
		immBegin(GL_LINES, 8);
		for (int i = 0; i < 4; ++i) {
			immVertex3fv(pos, near_plane[i]);
			immVertex3fv(pos, far_plane[i]);
		}
		immEnd();
	}
}

static bool drawcamera_is_stereo3d(Scene *scene, View3D *v3d, Object *ob)
{
	return (ob == v3d->camera) &&
	        (scene->r.scemode & R_MULTIVIEW) != 0 &&
	        (v3d->stereo3d_flag);
}

static void drawcamera_stereo3d(
        Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob, const Camera *cam,
        float vec[4][3], float drawsize, const float scale[3], unsigned pos)
{
	float obmat[4][4];
	float vec_lr[2][4][3];
	const float fac = (cam->stereo.pivot == CAM_S3D_PIVOT_CENTER) ? 2.0f : 1.0f;
	float origin[2][3] = {{0}};
	float tvec[3];
	const Camera *cam_lr[2];
	const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

	const bool is_stereo3d_cameras = (v3d->stereo3d_flag & V3D_S3D_DISPCAMERAS) && (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D);
	const bool is_stereo3d_plane = (v3d->stereo3d_flag & V3D_S3D_DISPPLANE) && (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D);
	const bool is_stereo3d_volume = (v3d->stereo3d_flag & V3D_S3D_DISPVOLUME);

	zero_v3(tvec);

	/* caller bound GPU_SHADER_3D_UNIFORM_COLOR, passed in pos attribute ID */

	for (int i = 0; i < 2; i++) {
		ob = BKE_camera_multiview_render(scene, ob, names[i]);
		cam_lr[i] = ob->data;

		gpuLoadMatrix3D(rv3d->viewmat);
		BKE_camera_multiview_model_matrix(&scene->r, ob, names[i], obmat);
		gpuMultMatrix3D(obmat);

		copy_m3_m3(vec_lr[i], vec);
		copy_v3_v3(vec_lr[i][3], vec[3]);

		if (cam->stereo.convergence_mode == CAM_S3D_OFFAXIS) {
			const float shift_x =
			        ((BKE_camera_multiview_shift_x(&scene->r, ob, names[i]) - cam->shiftx) *
			         (drawsize * scale[0] * fac));

			for (int j = 0; j < 4; j++) {
				vec_lr[i][j][0] += shift_x;
			}
		}

		if (is_stereo3d_cameras) {
			/* camera frame */
			drawcamera_frame(vec_lr[i], false, pos);

			/* center point to camera frame */
			drawcamera_framelines(vec_lr[i], tvec, pos);
		}

		/* connecting line */
		mul_m4_v3(obmat, origin[i]);

		/* convergence plane */
		if (is_stereo3d_plane || is_stereo3d_volume) {
			for (int j = 0; j < 4; j++) {
				mul_m4_v3(obmat, vec_lr[i][j]);
			}
		}
	}

	/* the remaining drawing takes place in the view space */
	gpuLoadMatrix3D(rv3d->viewmat);

	if (is_stereo3d_cameras) {
		/* draw connecting lines */
		glPushAttrib(GL_ENABLE_BIT); /* TODO(merwin): new state tracking! */
		glLineStipple(2, 0xAAAA);
		glEnable(GL_LINE_STIPPLE);

		immBegin(GL_LINES, 2);
		immVertex3fv(pos, origin[0]);
		immVertex3fv(pos, origin[1]);
		immEnd();

		glPopAttrib();
	}

	/* draw convergence plane */
	if (is_stereo3d_plane) {
		float axis_center[3], screen_center[3];
		float world_plane[4][3];
		float local_plane[4][3];
		float offset;

		mid_v3_v3v3(axis_center, origin[0], origin[1]);

		for (int i = 0; i < 4; i++) {
			mid_v3_v3v3(world_plane[i], vec_lr[0][i], vec_lr[1][i]);
			sub_v3_v3v3(local_plane[i], world_plane[i], axis_center);
		}

		mid_v3_v3v3(screen_center, world_plane[0], world_plane[2]);
		offset = cam->stereo.convergence_distance / len_v3v3(screen_center, axis_center);

		for (int i = 0; i < 4; i++) {
			mul_v3_fl(local_plane[i], offset);
			add_v3_v3(local_plane[i], axis_center);
		}

		immUniformColor3f(0.0f, 0.0f, 0.0f);

		/* camera frame */
		drawcamera_frame(local_plane, false, pos);

		if (v3d->stereo3d_convergence_alpha > 0.0f) {
			glEnable(GL_BLEND);
			glDepthMask(GL_FALSE);  /* disable write in zbuffer, needed for nice transp */

			immUniformColor4f(0.0f, 0.0f, 0.0f, v3d->stereo3d_convergence_alpha);

			drawcamera_frame(local_plane, true, pos);

			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);  /* restore write in zbuffer */
		}
	}

	/* draw convergence plane */
	if (is_stereo3d_volume) {
		float screen_center[3];
		float near_plane[4][3], far_plane[4][3];

		for (int i = 0; i < 2; i++) {
			mid_v3_v3v3(screen_center, vec_lr[i][0], vec_lr[i][2]);

			float offset = len_v3v3(screen_center, origin[i]);

			for (int j = 0; j < 4; j++) {
				sub_v3_v3v3(near_plane[j], vec_lr[i][j], origin[i]);
				mul_v3_fl(near_plane[j], cam_lr[i]->clipsta / offset);
				add_v3_v3(near_plane[j], origin[i]);

				sub_v3_v3v3(far_plane[j], vec_lr[i][j], origin[i]);
				mul_v3_fl(far_plane[j], cam_lr[i]->clipend / offset);
				add_v3_v3(far_plane[j], origin[i]);
			}

			/* camera frame */
			immUniformColor3f(0.0f, 0.0f, 0.0f);

			drawcamera_volume(near_plane, far_plane, false, pos);

			if (v3d->stereo3d_volume_alpha > 0.0f) {
				glEnable(GL_BLEND);
				glDepthMask(GL_FALSE);  /* disable write in zbuffer, needed for nice transp */

				if (i == 0)
					immUniformColor4f(0.0f, 1.0f, 1.0f, v3d->stereo3d_volume_alpha);
				else
					immUniformColor4f(1.0f, 0.0f, 0.0f, v3d->stereo3d_volume_alpha);

				drawcamera_volume(near_plane, far_plane, true, pos);

				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);  /* restore write in zbuffer */
			}
		}
	}
}

/* flag similar to draw_object() */
void drawcamera(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base,
                const short dflag, const unsigned char ob_wire_col[4])
{
	/* a standing up pyramid with (0,0,0) as top */
	Camera *cam;
	Object *ob = base->object;
	float tvec[3];
	float vec[4][3], asp[2], shift[2], scale[3];
	MovieClip *clip = BKE_object_movieclip_get(scene, base->object, false);

	const bool is_active = (ob == v3d->camera);
	const bool is_view = (rv3d->persp == RV3D_CAMOB && is_active);
	const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;
	const bool is_stereo3d = drawcamera_is_stereo3d(scene, v3d, ob);
	const bool is_stereo3d_view = (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D);
	const bool is_stereo3d_cameras = (ob == scene->camera) &&
	                                 is_multiview &&
	                                 is_stereo3d_view &&
	                                 (v3d->stereo3d_flag & V3D_S3D_DISPCAMERAS);
	const bool is_selection_camera_stereo = (G.f & G_PICKSEL) &&
	                                        is_view && is_multiview &&
	                                        is_stereo3d_view;

	/* draw data for movie clip set as active for scene */
	if (clip) {
		draw_viewport_reconstruction(scene, base, v3d, rv3d, clip, dflag, ob_wire_col, false);
		draw_viewport_reconstruction(scene, base, v3d, rv3d, clip, dflag, ob_wire_col, true);
	}

#ifdef VIEW3D_CAMERA_BORDER_HACK
	if (is_view && !(G.f & G_PICKSEL)) {
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			view3d_camera_border_hack_col[0] = ob_wire_col[0];
			view3d_camera_border_hack_col[1] = ob_wire_col[1];
			view3d_camera_border_hack_col[2] = ob_wire_col[2];
		}
		else {
			float col[4];
			glGetFloatv(GL_CURRENT_COLOR, col);
			rgb_float_to_uchar(view3d_camera_border_hack_col, col);
		}
		view3d_camera_border_hack_test = true;
		return;
	}
#endif

	cam = ob->data;

	/* BKE_camera_multiview_model_matrix already accounts for scale, don't do it here */
	if (is_selection_camera_stereo) {
		scale[0] = 1.0f;
		scale[1] = 1.0f;
		scale[2] = 1.0f;
	}
	else {
		scale[0] = 1.0f / len_v3(ob->obmat[0]);
		scale[1] = 1.0f / len_v3(ob->obmat[1]);
		scale[2] = 1.0f / len_v3(ob->obmat[2]);
	}

	float drawsize;
	BKE_camera_view_frame_ex(scene, cam, cam->drawsize, is_view, scale,
	                         asp, shift, &drawsize, vec);

	unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	if (ob_wire_col) {
		immUniformColor3ubv(ob_wire_col);
	}
	glLineWidth(1.0f);

	/* camera frame */
	if (!is_stereo3d_cameras) {
		/* make sure selection uses the same matrix for camera as the one used while viewing */
		if (is_selection_camera_stereo) {
			float obmat[4][4];
			bool is_left = v3d->multiview_eye == STEREO_LEFT_ID;

			gpuPushMatrix();
			gpuLoadMatrix3D(rv3d->viewmat);
			BKE_camera_multiview_model_matrix(&scene->r, ob, is_left ? STEREO_LEFT_NAME : STEREO_RIGHT_NAME, obmat);
			gpuMultMatrix3D(obmat);

			drawcamera_frame(vec, false, pos);
			gpuPopMatrix();
		}
		else {
			drawcamera_frame(vec, false, pos);
		}
	}

	if (is_view) {
		immUnbindProgram();
		return;
	}

	zero_v3(tvec);

	/* center point to camera frame */
	if (!is_stereo3d_cameras)
		drawcamera_framelines(vec, tvec, pos);

	/* arrow on top */
	tvec[2] = vec[1][2]; /* copy the depth */

	/* draw an outline arrow for inactive cameras and filled
	 * for active cameras. We actually draw both outline+filled
	 * for active cameras so the wire can be seen side-on */
	for (int i = 0; i < 2; i++) {
		if (i == 0) immBegin(GL_LINE_LOOP, 3);
		else if (i == 1 && is_active) {
			glDisable(GL_CULL_FACE); /* TODO: declarative state tracking */
			immBegin(GL_TRIANGLES, 3);
		}
		else break;

		tvec[0] = shift[0] + ((-0.7f * drawsize) * scale[0]);
		tvec[1] = shift[1] + ((drawsize * (asp[1] + 0.1f)) * scale[1]);
		immVertex3fv(pos, tvec); /* left */
		
		tvec[0] = shift[0] + ((0.7f * drawsize) * scale[0]);
		immVertex3fv(pos, tvec); /* right */
		
		tvec[0] = shift[0];
		tvec[1] = shift[1] + ((1.1f * drawsize * (asp[1] + 0.7f)) * scale[1]);
		immVertex3fv(pos, tvec); /* top */

		immEnd();
	}

	if ((dflag & DRAW_SCENESET) == 0) {
		if (cam->flag & (CAM_SHOWLIMITS | CAM_SHOWMIST)) {
			float nobmat[4][4];

			/* draw in normalized object matrix space */
			copy_m4_m4(nobmat, ob->obmat);
			normalize_m4(nobmat);

			gpuLoadMatrix3D(rv3d->viewmat);
			gpuMultMatrix3D(nobmat);

			if (cam->flag & CAM_SHOWLIMITS) {
				const unsigned char col[3] = {128, 128, 60}, col_hi[3] = {255, 255, 120};

				draw_limit_line(cam->clipsta, cam->clipend, dflag, (is_active ? col_hi : col), pos);
				/* qdn: was yafray only, now also enabled for Blender to be used with defocus composite node */
				draw_focus_cross(BKE_camera_object_dof_distance(ob), cam->drawsize, pos);
			}

			if (cam->flag & CAM_SHOWMIST) {
				World *world = scene->world;
				const unsigned char col[3] = {128, 128, 128}, col_hi[3] = {255, 255, 255};

				if (world) {
					draw_limit_line(world->miststa, world->miststa + world->mistdist,
					                dflag, (is_active ? col_hi : col), pos);
				}
			}
		}
	}

	/* stereo cameras drawing */
	if (is_stereo3d) {
		drawcamera_stereo3d(scene, v3d, rv3d, ob, cam, vec, drawsize, scale, pos);
	}

	immUnbindProgram();
}

/* flag similar to draw_object() */
void drawspeaker(const unsigned char ob_wire_col[3])
{
	VertexFormat *format = immVertexFormat();
	unsigned int pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	if (ob_wire_col) {
		immUniformColor3ubv(ob_wire_col);
	}

	glLineWidth(1.0f);

	const int segments = 16;

	for (int j = 0; j < 3; j++) {
		float z = 0.25f * j - 0.125f;

		immBegin(GL_LINE_LOOP, segments);
		for (int i = 0; i < segments; i++) {
			float x = cosf((float)M_PI * i / 8.0f) * (j == 0 ? 0.5f : 0.25f);
			float y = sinf((float)M_PI * i / 8.0f) * (j == 0 ? 0.5f : 0.25f);
			immVertex3f(pos, x, y, z);
		}
		immEnd();
	}

	for (int j = 0; j < 4; j++) {
		float x = (((j + 1) % 2) * (j - 1)) * 0.5f;
		float y = ((j % 2) * (j - 2)) * 0.5f;
		immBegin(GL_LINE_STRIP, 3);
		for (int i = 0; i < 3; i++) {
			if (i == 1) {
				x *= 0.5f;
				y *= 0.5f;
			}

			float z = 0.25f * i - 0.125f;
			immVertex3f(pos, x, y, z);
		}
		immEnd();
	}

	immUnbindProgram();
}

static void lattice_draw_verts(Lattice *lt, DispList *dl, BPoint *actbp, short sel,
                               unsigned int pos, unsigned int color)
{
	BPoint *bp = lt->def;
	const float *co = dl ? dl->verts : NULL;
	float active_color[4], draw_color[4];

	UI_GetThemeColor4fv(sel ? TH_VERTEX_SELECT : TH_VERTEX, draw_color);
	UI_GetThemeColor4fv(TH_ACTIVE_VERT, active_color);

	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));
	immBeginAtMost(GL_POINTS, lt->pntsw * lt->pntsv * lt->pntsu);

	for (int w = 0; w < lt->pntsw; w++) {
		int wxt = (w == 0 || w == lt->pntsw - 1);
		for (int v = 0; v < lt->pntsv; v++) {
			int vxt = (v == 0 || v == lt->pntsv - 1);
			for (int u = 0; u < lt->pntsu; u++, bp++, co += 3) {
				int uxt = (u == 0 || u == lt->pntsu - 1);
				if (!(lt->flag & LT_OUTSIDE) || uxt || vxt || wxt) {
					if (bp->hide == 0) {
						/* check for active BPoint and ensure selected */
						if ((bp == actbp) && (bp->f1 & SELECT)) {
							immAttrib4fv(color, active_color);
							immVertex3fv(pos, dl ? co : bp->vec);
						}
						else if ((bp->f1 & SELECT) == sel) {
							immAttrib4fv(color, draw_color);
							immVertex3fv(pos, dl ? co : bp->vec);
						}
					}
				}
			}
		}
	}
	
	immEnd();
}

static void drawlattice__point(Lattice *lt, DispList *dl, int u, int v, int w, int actdef_wcol,
                               unsigned int pos, unsigned int color)
{
	int index = ((w * lt->pntsv + v) * lt->pntsu) + u;

	if (actdef_wcol) {
		float col[3];
		MDeformWeight *mdw = defvert_find_index(lt->dvert + index, actdef_wcol - 1);
		weight_to_rgb(col, mdw ? mdw->weight : 0.0f);
		immAttrib3fv(color, col);
	}
	
	if (dl) {
		immVertex3fv(pos, &dl->verts[index * 3]);
	}
	else {
		immVertex3fv(pos, lt->def[index].vec);
	}
}

#ifdef SEQUENCER_DAG_WORKAROUND
static void ensure_curve_cache(Scene *scene, Object *object)
{
	bool need_recalc = object->curve_cache == NULL;
	/* Render thread might have freed the curve cache if the
	 * object is not visible. If the object is also used for
	 * particles duplication, then render thread might have
	 * also created curve_cache with only bevel and path
	 * filled in.
	 *
	 * So check for curve_cache != NULL is not fully correct
	 * here, we also need to check whether display list is
	 * empty or not.
	 *
	 * The trick below tries to optimize calls to displist
	 * creation for cases curve is empty. Meaning, if the curve
	 * is empty (without splines) bevel list would also be empty.
	 * And the thing is, render thread always leaves bevel list
	 * in a proper state. So if bevel list is here and display
	 * list is not we need to make display list.
	 */
	if (need_recalc == false) {
		need_recalc = object->curve_cache->disp.first == NULL &&
		              object->curve_cache->bev.first != NULL;
	}
	if (need_recalc) {
		switch (object->type) {
			case OB_CURVE:
			case OB_SURF:
			case OB_FONT:
				BKE_displist_make_curveTypes(scene, object, false);
				break;
			case OB_MBALL:
				BKE_displist_make_mball(G.main->eval_ctx, scene, object);
				break;
			case OB_LATTICE:
				BKE_lattice_modifiers_calc(scene, object);
				break;
		}
	}
}
#endif

/* lattice color is hardcoded, now also shows weightgroup values in edit mode */
static void drawlattice(View3D *v3d, Object *ob, const short dflag, const unsigned char ob_wire_col[4])
{
	Lattice *lt = ob->data;
	DispList *dl;
	int u, v, w;
	int actdef_wcol = 0;
	const bool is_edit = (lt->editlatt != NULL);

	dl = BKE_displist_find(&ob->curve_cache->disp, DL_VERTS);
	
	if (is_edit) {
		lt = lt->editlatt->latt;

		if (ob->defbase.first && lt->dvert) {
			actdef_wcol = ob->actdef;
		}
	}

	VertexFormat *format = immVertexFormat();
	unsigned int color, pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);

	if (actdef_wcol) {
		color = add_attrib(format, "color", GL_FLOAT, 3, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

		if (is_edit) {
			immUniformThemeColor(TH_WIRE_EDIT);
		}
		else {
			if ((dflag & DRAW_CONSTCOLOR) == 0) {
				immUniformColor3ubv(ob_wire_col);
			}
			else {
				immUniformColor3f(0.0f, 0.0f, 0.0f);
			}
		}
	}

	glLineWidth(1.0f);
	immBeginAtMost(GL_LINES, lt->pntsw * lt->pntsv * lt->pntsu * 6);

	for (w = 0; w < lt->pntsw; w++) {
		int wxt = (w == 0 || w == lt->pntsw - 1);
		for (v = 0; v < lt->pntsv; v++) {
			int vxt = (v == 0 || v == lt->pntsv - 1);
			for (u = 0; u < lt->pntsu; u++) {
				int uxt = (u == 0 || u == lt->pntsu - 1);

				if (w && ((uxt || vxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u, v, w - 1, actdef_wcol, pos, color);
					drawlattice__point(lt, dl, u, v, w, actdef_wcol, pos, color);
				}
				if (v && ((uxt || wxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u, v - 1, w, actdef_wcol, pos, color);
					drawlattice__point(lt, dl, u, v, w, actdef_wcol, pos, color);
				}
				if (u && ((vxt || wxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u - 1, v, w, actdef_wcol, pos, color);
					drawlattice__point(lt, dl, u, v, w, actdef_wcol, pos, color);
				}
			}
		}
	}

	immEnd();
	immUnbindProgram();

	if (is_edit) {
		BPoint *actbp = BKE_lattice_active_point_get(lt);

		if (v3d->zbuf) glDisable(GL_DEPTH_TEST);

		VertexFormat *v_format = immVertexFormat();
		unsigned int v_pos = add_attrib(v_format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		unsigned int v_color = add_attrib(v_format, "color", GL_FLOAT, 4, KEEP_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR);

		lattice_draw_verts(lt, dl, actbp, 0, v_pos, v_color);
		lattice_draw_verts(lt, dl, actbp, 1, v_pos, v_color);

		immUnbindProgram();

		if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	}
}

/* ***************** ******************** */

/* draw callback */

typedef struct drawDMVertSel_userData {
	MVert *mvert;
	int active;
	unsigned char *col[3];  /* (base, sel, act) */
	char sel_prev;
	unsigned int pos, color;
} drawDMVertSel_userData;

static void drawSelectedVertices__mapFunc(void *userData, int index, const float co[3],
                                          const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	drawDMVertSel_userData *data = userData;
	MVert *mv = &data->mvert[index];

	if (!(mv->flag & ME_HIDE)) {
		const char sel = (index == data->active) ? 2 : (mv->flag & SELECT);
		if (sel != data->sel_prev) {
			immAttrib3ubv(data->color, data->col[sel]);
			data->sel_prev = sel;
		}

		immVertex3fv(data->pos, co);
	}
}

static void drawSelectedVertices(DerivedMesh *dm, Mesh *me)
{
	drawDMVertSel_userData data;
	VertexFormat *format = immVertexFormat();

	/* TODO define selected color */
	unsigned char base_col[3] = {0x0, 0x0, 0x0};
	unsigned char sel_col[3] = {0xd8, 0xb8, 0x0};
	unsigned char act_col[3] = {0xff, 0xff, 0xff};

	data.mvert = me->mvert;
	data.active = BKE_mesh_mselect_active_get(me, ME_VSEL);
	data.sel_prev = 0xff;

	data.col[0] = base_col;
	data.col[1] = sel_col;
	data.col[2] = act_col;

	data.color = add_attrib(format, "color", GL_UNSIGNED_BYTE, 3, NORMALIZE_INT_TO_FLOAT);
	data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);

	if (dm->getNumVerts(dm) == 0) return;

	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

	immBeginAtMost(GL_POINTS, dm->getNumVerts(dm));
	dm->foreachMappedVert(dm, drawSelectedVertices__mapFunc, &data, DM_FOREACH_NOP);
	immEnd();

	immUnbindProgram();
}

/* ************** DRAW MESH ****************** */

/* First section is all the "simple" draw routines,
 * ones that just pass some sort of primitive to GL,
 * with perhaps various options to control lighting,
 * color, etc.
 *
 * These routines should not have user interface related
 * logic!!!
 */

static void calcDrawDMNormalScale(Object *ob, drawDMNormal_userData *data)
{
	float obmat[3][3];

	copy_m3_m4(obmat, ob->obmat);

	data->uniform_scale = is_uniform_scaled_m3(obmat);

	if (!data->uniform_scale) {
		/* inverted matrix */
		invert_m3_m3(data->imat, obmat);

		/* transposed inverted matrix */
		transpose_m3_m3(data->tmat, data->imat);
	}
}

static void draw_dm_face_normals__mapFunc(void *userData, int index, const float cent[3], const float no[3])
{
	drawDMNormal_userData *data = userData;
	BMFace *efa = BM_face_at_index(data->bm, index);
	float n[3];

	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		if (!data->uniform_scale) {
			mul_v3_m3v3(n, data->tmat, no);
			normalize_v3(n);
			mul_m3_v3(data->imat, n);
		}
		else {
			copy_v3_v3(n, no);
		}

		immVertex3fv(data->pos, cent);
		immVertex3f(data->pos, cent[0] + n[0] * data->normalsize,
		                       cent[1] + n[1] * data->normalsize,
		                       cent[2] + n[2] * data->normalsize);
	}
}

static void draw_dm_face_normals(BMEditMesh *em, Scene *scene, Object *ob, DerivedMesh *dm, int theme_id)
{
	VertexFormat *format = immVertexFormat();
	drawDMNormal_userData data;

	data.bm = em->bm;
	data.normalsize = scene->toolsettings->normalsize;
	data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);

	calcDrawDMNormalScale(ob, &data);

	if (dm->getNumPolys(dm) == 0) return;

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformThemeColor(theme_id);

	immBeginAtMost(GL_LINES, dm->getNumPolys(dm) * 2);
	dm->foreachMappedFaceCenter(dm, draw_dm_face_normals__mapFunc, &data, DM_FOREACH_USE_NORMAL);
	immEnd();

	immUnbindProgram();
}

static void draw_dm_face_centers__mapFunc(void *userData, int index, const float cent[3], const float UNUSED(no[3]))
{
	drawBMSelect_userData *data = userData;
	BMFace *efa = BM_face_at_index(data->bm, index);
	
	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) &&
	    (BM_elem_flag_test(efa, BM_ELEM_SELECT) == data->select))
	{
		immVertex3fv(data->pos, cent);
	}
}
static void draw_dm_face_centers(BMEditMesh *em, DerivedMesh *dm, bool select, const unsigned char fcol[3])
{
	VertexFormat *format = immVertexFormat();

	drawBMSelect_userData data;
	data.bm = em->bm;
	data.select = select;
	data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);

	if (dm->getNumPolys(dm) == 0) return;

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3ubv(fcol);

	immBeginAtMost(GL_POINTS, dm->getNumPolys(dm));
	dm->foreachMappedFaceCenter(dm, draw_dm_face_centers__mapFunc, &data, DM_FOREACH_NOP);
	immEnd();

	immUnbindProgram();
}

static void draw_dm_vert_normals__mapFunc(void *userData, int index, const float co[3], const float no_f[3], const short no_s[3])
{
	drawDMNormal_userData *data = userData;
	BMVert *eve = BM_vert_at_index(data->bm, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
		float no[3], n[3];

		if (no_f) {
			copy_v3_v3(no, no_f);
		}
		else {
			normal_short_to_float_v3(no, no_s);
		}

		if (!data->uniform_scale) {
			mul_v3_m3v3(n, data->tmat, no);
			normalize_v3(n);
			mul_m3_v3(data->imat, n);
		}
		else {
			copy_v3_v3(n, no);
		}

		immVertex3fv(data->pos, co);
		immVertex3f(data->pos, co[0] + n[0] * data->normalsize,
		                 co[1] + n[1] * data->normalsize,
		                 co[2] + n[2] * data->normalsize);
	}
}

static void draw_dm_vert_normals(BMEditMesh *em, Scene *scene, Object *ob, DerivedMesh *dm, int theme_id)
{
	drawDMNormal_userData data;
	VertexFormat *format = immVertexFormat();

	data.bm = em->bm;
	data.normalsize = scene->toolsettings->normalsize;
	data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);

	calcDrawDMNormalScale(ob, &data);

	if (dm->getNumVerts(dm) == 0) return;

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformThemeColor(theme_id);

	immBeginAtMost(GL_LINES, dm->getNumVerts(dm) * 2);
	dm->foreachMappedVert(dm, draw_dm_vert_normals__mapFunc, &data, DM_FOREACH_USE_NORMAL);
	immEnd();

	immUnbindProgram();
}

static void draw_dm_verts_skin_root__mapFunc(void *userData, int index, const float co[3],
                                             const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	drawDMVerts_userData *data = userData;
	BMVert *eve = BM_vert_at_index(data->bm, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BM_elem_flag_test(eve, BM_ELEM_SELECT) == data->sel) {
		/* skin nodes: draw a red circle around the root node(s) */
		const MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(eve, data->cd_vskin_offset);
		if (vs->flag & MVERT_SKIN_ROOT) {
			float radius = (vs->radius[0] + vs->radius[1]) * 0.5f;
			imm_drawcircball(co, radius, data->imat, data->pos);
		}
	}
}

/* Draw verts with color set based on selection */
static void draw_dm_verts__mapFunc(void *userData, int index, const float co[3],
                                   const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	drawDMVerts_userData *data = userData;
	BMVert *eve = BM_vert_at_index(data->bm, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BM_elem_flag_test(eve, BM_ELEM_SELECT) == data->sel) {
		/* draw active in a different color - no need to stop/start point drawing for this :D */
		if (eve == data->eve_act) {
			immAttrib4ubv(data->color, data->th_editmesh_active);
			immVertex3fv(data->pos, co);
		}
		else {
			immAttrib4ubv(data->color, data->sel ? data->th_vertex_select : data->th_vertex);
			immVertex3fv(data->pos, co);
		}
	}
}

static void draw_dm_verts(BMEditMesh *em, DerivedMesh *dm, const char sel, BMVert *eve_act,
                          RegionView3D *rv3d, const unsigned char col[4])
{
	VertexFormat *format = immVertexFormat();

	drawDMVerts_userData data;
	data.sel = sel;
	data.eve_act = eve_act;
	data.bm = em->bm;
	data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	data.color = add_attrib(format, "color", GL_UNSIGNED_BYTE, 4, NORMALIZE_INT_TO_FLOAT);

	/* Cache theme values */
	UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, data.th_editmesh_active);
	UI_GetThemeColor4ubv(TH_VERTEX_SELECT, data.th_vertex_select);
	UI_GetThemeColor4ubv(TH_VERTEX, data.th_vertex);
	UI_GetThemeColor4ubv(TH_SKIN_ROOT, data.th_skin_root);

	/* Set correct alpha */
	data.th_editmesh_active[3] = data.th_vertex_select[3] = data.th_vertex[3] = data.th_skin_root[3] = col[3];

	/* view-aligned matrix */
	mul_m4_m4m4(data.imat, rv3d->viewmat, em->ob->obmat);
	invert_m4(data.imat);

	if (dm->getNumVerts(dm) == 0) return;

	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

	immBeginAtMost(GL_POINTS, dm->getNumVerts(dm));
	dm->foreachMappedVert(dm, draw_dm_verts__mapFunc, &data, DM_FOREACH_NOP);
	immEnd();

	immUnbindProgram();

	/* For skin root drawing */
	data.cd_vskin_offset = CustomData_get_offset(&em->bm->vdata, CD_MVERT_SKIN);

	if (data.cd_vskin_offset != -1) {
		data.pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor4ubv(data.th_skin_root);

		dm->foreachMappedVert(dm, draw_dm_verts_skin_root__mapFunc, &data, DM_FOREACH_NOP);

		immUnbindProgram();
	}
}

/* Draw edges with color set based on selection */
static DMDrawOption draw_dm_edges_sel__setDrawOptions(void *userData, int index)
{
	BMEdge *eed;
	drawDMEdgesSel_userData *data = userData;
	unsigned char *col;

	eed = BM_edge_at_index(data->bm, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
		if (eed == data->eed_act) {
			glColor4ubv(data->actCol);
		}
		else {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
				col = data->selCol;
			}
			else {
				col = data->baseCol;
			}
			/* no alpha, this is used so a transparent color can disable drawing unselected edges in editmode */
			if (col[3] == 0)
				return DM_DRAW_OPTION_SKIP;
			
			glColor4ubv(col);
		}
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

static void draw_dm_edges_sel(BMEditMesh *em, DerivedMesh *dm, unsigned char *baseCol,
                              unsigned char *selCol, unsigned char *actCol, BMEdge *eed_act)
{
	drawDMEdgesSel_userData data;
	
	data.baseCol = baseCol;
	data.selCol = selCol;
	data.actCol = actCol;
	data.bm = em->bm;
	data.eed_act = eed_act;
	dm->drawMappedEdges(dm, draw_dm_edges_sel__setDrawOptions, &data);
}

/* Draw edges */
static DMDrawOption draw_dm_edges__setDrawOptions(void *userData, int index)
{
	if (BM_elem_flag_test(BM_edge_at_index(userData, index), BM_ELEM_HIDDEN))
		return DM_DRAW_OPTION_SKIP;
	else
		return DM_DRAW_OPTION_NORMAL;
}

static void draw_dm_edges(BMEditMesh *em, DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges__setDrawOptions, em->bm);
}

/* Draw edges with color interpolated based on selection */
static DMDrawOption draw_dm_edges_sel_interp__setDrawOptions(void *userData, int index)
{
	drawDMEdgesSelInterp_userData *data = userData;
	if (BM_elem_flag_test(BM_edge_at_index(data->bm, index), BM_ELEM_HIDDEN))
		return DM_DRAW_OPTION_SKIP;
	else
		return DM_DRAW_OPTION_NORMAL;
}
static void draw_dm_edges_sel_interp__setDrawInterpOptions(void *userData, int index, float t)
{
	drawDMEdgesSelInterp_userData *data = userData;
	BMEdge *eed = BM_edge_at_index(data->bm, index);
	unsigned char **cols = userData;
	unsigned int col0_id = (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT)) ? 2 : 1;
	unsigned int col1_id = (BM_elem_flag_test(eed->v2, BM_ELEM_SELECT)) ? 2 : 1;
	unsigned char *col0 = cols[col0_id];
	unsigned char *col1 = cols[col1_id];
	unsigned char *col_pt;

	if (col0_id == col1_id) {
		col_pt = col0;
	}
	else if (t == 0.0f) {
		col_pt = col0;
	}
	else if (t == 1.0f) {
		col_pt = col1;
	}
	else {
		unsigned char  col_blend[4];
		interp_v4_v4v4_uchar(col_blend, col0, col1, t);
		glColor4ubv(col_blend);
		data->lastCol = NULL;
		return;
	}

	if (data->lastCol != col_pt) {
		data->lastCol = col_pt;
		glColor4ubv(col_pt);
	}
}

static void draw_dm_edges_sel_interp(BMEditMesh *em, DerivedMesh *dm, unsigned char *baseCol, unsigned char *selCol)
{
	drawDMEdgesSelInterp_userData data;
	data.bm = em->bm;
	data.baseCol = baseCol;
	data.selCol = selCol;
	data.lastCol = NULL;

	dm->drawMappedEdgesInterp(dm, draw_dm_edges_sel_interp__setDrawOptions, draw_dm_edges_sel_interp__setDrawInterpOptions, &data);
}

static void bm_color_from_weight(float col[3], BMVert *vert, drawDMEdgesWeightInterp_userData *data)
{
	MDeformVert *dvert = BM_ELEM_CD_GET_VOID_P(vert, data->cd_dvert_offset);
	float weight = defvert_find_weight(dvert, data->vgroup_index);

	if ((weight == 0.0f) &&
	    ((data->weight_user == OB_DRAW_GROUPUSER_ACTIVE) ||
	     ((data->weight_user == OB_DRAW_GROUPUSER_ALL) && defvert_is_weight_zero(dvert, data->defgroup_tot))))
	{
		copy_v3_v3(col, data->alert_color);
	}
	else {
		weight_to_rgb(col, weight);
	}
}

static void draw_dm_edges_nop_interp__setDrawInterpOptions(void *UNUSED(userData), int UNUSED(index), float UNUSED(t))
{
	/* pass */
}

static void draw_dm_edges_weight_interp__setDrawInterpOptions(void *userData, int index, float t)
{
	drawDMEdgesWeightInterp_userData *data = userData;
	BMEdge *eed = BM_edge_at_index(data->bm, index);
	float col[3];

	if (t == 0.0f) {
		bm_color_from_weight(col, eed->v1, data);
	}
	else if (t == 1.0f) {
		bm_color_from_weight(col, eed->v2, data);
	}
	else {
		float col_v1[3];
		float col_v2[3];

		bm_color_from_weight(col_v1, eed->v1, data);
		bm_color_from_weight(col_v2, eed->v2, data);
		interp_v3_v3v3(col, col_v1, col_v2, t);
	}

	glColor3fv(col);
}

static void draw_dm_edges_weight_interp(BMEditMesh *em, DerivedMesh *dm, const char weight_user)
{
	drawDMEdgesWeightInterp_userData data;
	Object *ob = em->ob;

	data.bm = em->bm;
	data.cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
	data.defgroup_tot = BLI_listbase_count(&ob->defbase);
	data.vgroup_index = ob->actdef - 1;
	data.weight_user = weight_user;
	UI_GetThemeColor3fv(TH_VERTEX_UNREFERENCED, data.alert_color);

	if ((data.vgroup_index != -1) && (data.cd_dvert_offset != -1)) {
		glEnable(GL_BLEND);
		dm->drawMappedEdgesInterp(
		        dm,
		        draw_dm_edges_sel_interp__setDrawOptions,
		        draw_dm_edges_weight_interp__setDrawInterpOptions,
		        &data);
		glDisable(GL_BLEND);
	}
	else {
		float col[3];

		if (data.weight_user == OB_DRAW_GROUPUSER_NONE) {
			weight_to_rgb(col, 0.0f);
		}
		else {
			copy_v3_v3(col, data.alert_color);
		}
		glColor3fv(col);

		dm->drawMappedEdgesInterp(
		        dm,
		        draw_dm_edges_sel_interp__setDrawOptions,
		        draw_dm_edges_nop_interp__setDrawInterpOptions,
		        &data);
	}

}

static bool draw_dm_edges_weight_check(Mesh *me, View3D *v3d)
{
	if (me->drawflag & ME_DRAWEIGHT) {
		if ((v3d->drawtype == OB_WIRE) ||
		    (v3d->flag2 & V3D_SOLID_MATCAP) ||
		    ((v3d->flag2 & V3D_OCCLUDE_WIRE) && (v3d->drawtype > OB_WIRE)))
		{
			return true;
		}
	}

	return false;
}

/* Draw only seam edges */
static DMDrawOption draw_dm_edges_seams__setDrawOptions(void *userData, int index)
{
	BMEdge *eed = BM_edge_at_index(userData, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && BM_elem_flag_test(eed, BM_ELEM_SEAM))
		return DM_DRAW_OPTION_NORMAL;
	else
		return DM_DRAW_OPTION_SKIP;
}

static void draw_dm_edges_seams(BMEditMesh *em, DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_seams__setDrawOptions, em->bm);
}

/* Draw only sharp edges */
static DMDrawOption draw_dm_edges_sharp__setDrawOptions(void *userData, int index)
{
	BMEdge *eed = BM_edge_at_index(userData, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && !BM_elem_flag_test(eed, BM_ELEM_SMOOTH))
		return DM_DRAW_OPTION_NORMAL;
	else
		return DM_DRAW_OPTION_SKIP;
}

static void draw_dm_edges_sharp(BMEditMesh *em, DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_sharp__setDrawOptions, em->bm);
}

#ifdef WITH_FREESTYLE

static bool draw_dm_test_freestyle_edge_mark(BMesh *bm, BMEdge *eed)
{
	FreestyleEdge *fed = CustomData_bmesh_get(&bm->edata, eed->head.data, CD_FREESTYLE_EDGE);
	if (!fed)
		return false;
	return (fed->flag & FREESTYLE_EDGE_MARK) != 0;
}

/* Draw only Freestyle feature edges */
static DMDrawOption draw_dm_edges_freestyle__setDrawOptions(void *userData, int index)
{
	BMEdge *eed = BM_edge_at_index(userData, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && draw_dm_test_freestyle_edge_mark(userData, eed))
		return DM_DRAW_OPTION_NORMAL;
	else
		return DM_DRAW_OPTION_SKIP;
}

static void draw_dm_edges_freestyle(BMEditMesh *em, DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_freestyle__setDrawOptions, em->bm);
}

static bool draw_dm_test_freestyle_face_mark(BMesh *bm, BMFace *efa)
{
	FreestyleFace *ffa = CustomData_bmesh_get(&bm->pdata, efa->head.data, CD_FREESTYLE_FACE);
	if (!ffa)
		return false;
	return (ffa->flag & FREESTYLE_FACE_MARK) != 0;
}

#endif

/* Draw loop normals. */
static void draw_dm_loop_normals__mapFunc(void *userData, int vertex_index, int face_index,
                                          const float co[3], const float no[3])
{
	if (no) {
		const drawDMNormal_userData *data = userData;
		const BMVert *eve = BM_vert_at_index(data->bm, vertex_index);
		const BMFace *efa = BM_face_at_index(data->bm, face_index);
		float vec[3];

		if (!(BM_elem_flag_test(eve, BM_ELEM_HIDDEN) || BM_elem_flag_test(efa, BM_ELEM_HIDDEN))) {
			if (!data->uniform_scale) {
				mul_v3_m3v3(vec, (float(*)[3])data->tmat, no);
				normalize_v3(vec);
				mul_m3_v3((float(*)[3])data->imat, vec);
			}
			else {
				copy_v3_v3(vec, no);
			}
			mul_v3_fl(vec, data->normalsize);
			add_v3_v3(vec, co);
			immVertex3fv(data->pos, co);
			immVertex3fv(data->pos, vec);
		}
	}
}

static void draw_dm_loop_normals(BMEditMesh *em, Scene *scene, Object *ob, DerivedMesh *dm, int theme_id)
{
	drawDMNormal_userData data;

	data.bm = em->bm;
	data.normalsize = scene->toolsettings->normalsize;
	data.pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

	if (dm->getNumLoops(dm) == 0) return;

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformThemeColor(theme_id);

	calcDrawDMNormalScale(ob, &data);

	immBeginAtMost(GL_LINES, dm->getNumLoops(dm) * 2);
	dm->foreachMappedLoop(dm, draw_dm_loop_normals__mapFunc, &data, DM_FOREACH_USE_NORMAL);
	immEnd();

	immUnbindProgram();
}

/* Draw faces with color set based on selection
 * return 2 for the active face so it renders with stipple enabled */
static DMDrawOption draw_dm_faces_sel__setDrawOptions(void *userData, int index)
{
	drawDMFacesSel_userData *data = userData;
	BMFace *efa = BM_face_at_index(data->bm, index);
	unsigned char *col;
	
	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		if (efa == data->efa_act) {
			glColor4ubv(data->cols[2]);
			return DM_DRAW_OPTION_STIPPLE;
		}
		else {
#ifdef WITH_FREESTYLE
			col = data->cols[BM_elem_flag_test(efa, BM_ELEM_SELECT) ? 1 : draw_dm_test_freestyle_face_mark(data->bm, efa) ? 3 : 0];
#else
			col = data->cols[BM_elem_flag_test(efa, BM_ELEM_SELECT) ? 1 : 0];
#endif
			if (col[3] == 0)
				return DM_DRAW_OPTION_SKIP;
			glColor4ubv(col);
			return DM_DRAW_OPTION_NORMAL;
		}
	}
	return DM_DRAW_OPTION_SKIP;
}

static int draw_dm_faces_sel__compareDrawOptions(void *userData, int index, int next_index)
{

	drawDMFacesSel_userData *data = userData;
	int i;
	BMFace *efa;
	BMFace *next_efa;

	unsigned char *col, *next_col;

	i = data->orig_index_mp_to_orig ? data->orig_index_mp_to_orig[index] : index;
	efa = (i != ORIGINDEX_NONE) ? BM_face_at_index(data->bm, i) : NULL;
	i = data->orig_index_mp_to_orig ? data->orig_index_mp_to_orig[next_index] : next_index;
	next_efa = (i != ORIGINDEX_NONE) ? BM_face_at_index(data->bm, i) : NULL;

	if (ELEM(NULL, efa, next_efa))
		return 0;

	if (efa == next_efa)
		return 1;

	if (efa == data->efa_act || next_efa == data->efa_act)
		return 0;

#ifdef WITH_FREESTYLE
	col = data->cols[BM_elem_flag_test(efa, BM_ELEM_SELECT) ? 1 : draw_dm_test_freestyle_face_mark(data->bm, efa) ? 3 : 0];
	next_col = data->cols[BM_elem_flag_test(next_efa, BM_ELEM_SELECT) ? 1 : draw_dm_test_freestyle_face_mark(data->bm, efa) ? 3 : 0];
#else
	col = data->cols[BM_elem_flag_test(efa, BM_ELEM_SELECT) ? 1 : 0];
	next_col = data->cols[BM_elem_flag_test(next_efa, BM_ELEM_SELECT) ? 1 : 0];
#endif

	if (col[3] == 0 || next_col[3] == 0)
		return 0;

	return col == next_col;
}

/* also draws the active face */
#ifdef WITH_FREESTYLE
static void draw_dm_faces_sel(BMEditMesh *em, DerivedMesh *dm, unsigned char *baseCol,
                              unsigned char *selCol, unsigned char *actCol, unsigned char *markCol, BMFace *efa_act)
#else
static void draw_dm_faces_sel(BMEditMesh *em, DerivedMesh *dm, unsigned char *baseCol,
                              unsigned char *selCol, unsigned char *actCol, BMFace *efa_act)
#endif
{
	drawDMFacesSel_userData data;
	data.dm = dm;
	data.cols[0] = baseCol;
	data.bm = em->bm;
	data.cols[1] = selCol;
	data.cols[2] = actCol;
#ifdef WITH_FREESTYLE
	data.cols[3] = markCol;
#endif
	data.efa_act = efa_act;
	/* double lookup */
	data.orig_index_mp_to_orig  = DM_get_poly_data_layer(dm, CD_ORIGINDEX);

	dm->drawMappedFaces(dm, draw_dm_faces_sel__setDrawOptions, NULL, draw_dm_faces_sel__compareDrawOptions, &data, DM_DRAW_SKIP_HIDDEN);
}

static DMDrawOption draw_dm_creases__setDrawOptions(void *userData, int index)
{
	drawDMLayer_userData *data = userData;
	BMesh *bm = data->bm;
	BMEdge *eed = BM_edge_at_index(bm, index);
	
	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
		const float crease = BM_ELEM_CD_GET_FLOAT(eed, data->cd_layer_offset);
		if (crease != 0.0f) {
			UI_ThemeColorBlend(TH_WIRE_EDIT, TH_EDGE_CREASE, crease);
			return DM_DRAW_OPTION_NORMAL;
		}
	}
	return DM_DRAW_OPTION_SKIP;
}
static void draw_dm_creases(BMEditMesh *em, DerivedMesh *dm)
{
	drawDMLayer_userData data;

	data.bm = em->bm;
	data.cd_layer_offset = CustomData_get_offset(&em->bm->edata, CD_CREASE);

	if (data.cd_layer_offset != -1) {
		glLineWidth(3.0f);
		dm->drawMappedEdges(dm, draw_dm_creases__setDrawOptions, &data);
	}
}

static DMDrawOption draw_dm_bweights__setDrawOptions(void *userData, int index)
{
	drawDMLayer_userData *data = userData;
	BMesh *bm = data->bm;
	BMEdge *eed = BM_edge_at_index(bm, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
		const float bweight = BM_ELEM_CD_GET_FLOAT(eed, data->cd_layer_offset);
		if (bweight != 0.0f) {
			UI_ThemeColorBlend(TH_WIRE_EDIT, TH_EDGE_BEVEL, bweight);
			return DM_DRAW_OPTION_NORMAL;
		}
	}
	return DM_DRAW_OPTION_SKIP;
}
static void draw_dm_bweights__mapFunc(void *userData, int index, const float co[3],
                                      const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	drawDMLayer_userData *data = userData;
	BMesh *bm = data->bm;
	BMVert *eve = BM_vert_at_index(bm, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
		const float bweight = BM_ELEM_CD_GET_FLOAT(eve, data->cd_layer_offset);
		if (bweight != 0.0f) {
			unsigned char col[3];
			UI_GetThemeColorBlend3ubv(TH_VERTEX, TH_VERTEX_BEVEL, bweight, col);
			immAttrib3ubv(data->col, col);
			immVertex3fv(data->pos, co);
		}
	}
}
static void draw_dm_bweights(BMEditMesh *em, Scene *scene, DerivedMesh *dm)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->selectmode & SCE_SELECT_VERTEX) {
		drawDMLayer_userData data;

		data.bm = em->bm;
		data.cd_layer_offset = CustomData_get_offset(&em->bm->vdata, CD_BWEIGHT);

		/* is that ever true? */
		if (data.cd_layer_offset != -1) {
			VertexFormat *format = immVertexFormat();
			data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			data.col = add_attrib(format, "color", GL_UNSIGNED_BYTE, 4, NORMALIZE_INT_TO_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

			glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE) + 2.0f);

			immBeginAtMost(GL_POINTS, dm->getNumVerts(dm));
			dm->foreachMappedVert(dm, draw_dm_bweights__mapFunc, &data, DM_FOREACH_NOP);
			immEnd();

			immUnbindProgram();
		}
	}
	else {
		drawDMLayer_userData data;

		data.bm = em->bm;
		data.cd_layer_offset = CustomData_get_offset(&em->bm->edata, CD_BWEIGHT);

		if (data.cd_layer_offset != -1) {
			glLineWidth(3.0f);
			dm->drawMappedEdges(dm, draw_dm_bweights__setDrawOptions, &data);
		}
	}
}

/* Second section of routines: Combine first sets to form fancy
 * drawing routines (for example rendering twice to get overlays).
 *
 * Also includes routines that are basic drawing but are too
 * specialized to be split out (like drawing creases or measurements).
 */

/* EditMesh drawing routines */

static void draw_em_fancy_verts(Scene *scene, View3D *v3d, Object *obedit,
                                BMEditMesh *em, DerivedMesh *cageDM, BMVert *eve_act,
                                RegionView3D *rv3d)
{
	ToolSettings *ts = scene->toolsettings;

	if (v3d->zbuf) glDepthMask(GL_FALSE);  /* disable write in zbuffer, zbuf select */

	for (int sel = 0; sel < 2; sel++) {
		unsigned char col[4], fcol[4];

		UI_GetThemeColor3ubv(sel ? TH_VERTEX_SELECT : TH_VERTEX, col);
		UI_GetThemeColor3ubv(sel ? TH_FACE_DOT : TH_WIRE_EDIT, fcol);

		for (int pass = 0; pass < 2; pass++) {
			float size = UI_GetThemeValuef(TH_VERTEX_SIZE);
			float fsize = UI_GetThemeValuef(TH_FACEDOT_SIZE);

			if (pass == 0) {
				if (v3d->zbuf && !(v3d->flag & V3D_ZBUF_SELECT)) {
					glDisable(GL_DEPTH_TEST);
					glEnable(GL_BLEND);
				}
				else {
					continue;
				}

				size = (size > 2.1f ? size / 2.0f : size);
				fsize = (fsize > 2.1f ? fsize / 2.0f : fsize);
				col[3] = fcol[3] = 100;
			}
			else {
				col[3] = fcol[3] = 255;
			}

			if (ts->selectmode & SCE_SELECT_VERTEX) {
				draw_dm_verts(em, cageDM, sel, eve_act, rv3d, col);
			}
			
			if (check_ob_drawface_dot(scene, v3d, obedit->dt)) {
				glPointSize(fsize);
				draw_dm_face_centers(em, cageDM, sel, fcol);
			}
			
			if (pass == 0) {
				glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);
			}
		}
	}

	if (v3d->zbuf) glDepthMask(GL_TRUE);
}

static void draw_em_fancy_edges(BMEditMesh *em, Scene *scene, View3D *v3d,
                                Mesh *me, DerivedMesh *cageDM, short sel_only,
                                BMEdge *eed_act)
{
	ToolSettings *ts = scene->toolsettings;
	unsigned char wireCol[4], selCol[4], actCol[4];

	/* since this function does transparent... */
	UI_GetThemeColor4ubv(TH_EDGE_SELECT, selCol);
	UI_GetThemeColor4ubv(TH_WIRE_EDIT, wireCol);
	UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, actCol);
	
	/* when sel only is used, don't render wire, only selected, this is used for
	 * textured draw mode when the 'edges' option is disabled */
	if (sel_only)
		wireCol[3] = 0;

	for (int pass = 0; pass < 2; pass++) {
		/* show wires in transparent when no zbuf clipping for select */
		if (pass == 0) {
			if (v3d->zbuf && (v3d->flag & V3D_ZBUF_SELECT) == 0) {
				glEnable(GL_BLEND);
				glDisable(GL_DEPTH_TEST);
				selCol[3] = 85;
				if (!sel_only) wireCol[3] = 85;
			}
			else {
				continue;
			}
		}
		else {
			selCol[3] = 255;
			if (!sel_only) wireCol[3] = 255;
		}

		if ((me->drawflag & ME_DRAWEDGES) || (ts->selectmode & SCE_SELECT_EDGE)) {
			if (cageDM->drawMappedEdgesInterp &&
			    ((ts->selectmode & SCE_SELECT_VERTEX) || (me->drawflag & ME_DRAWEIGHT)))
			{
				if (draw_dm_edges_weight_check(me, v3d)) {
					// Interpolate vertex weights
					draw_dm_edges_weight_interp(em, cageDM, ts->weightuser);
				}
				else if (ts->selectmode == SCE_SELECT_FACE) {
					draw_dm_edges_sel(em, cageDM, wireCol, selCol, actCol, eed_act);
				}
				else {
					// Interpolate vertex selection
					draw_dm_edges_sel_interp(em, cageDM, wireCol, selCol);
				}
			}
			else {
				draw_dm_edges_sel(em, cageDM, wireCol, selCol, actCol, eed_act);
			}
		}
		else {
			if (!sel_only) {
				glColor4ubv(wireCol);
				draw_dm_edges(em, cageDM);
			}
		}

		if (pass == 0) {
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
		}
	}
}

static void draw_em_measure_stats(ARegion *ar, View3D *v3d, Object *ob, BMEditMesh *em, UnitSettings *unit)
{
	/* Do not use ascii when using non-default unit system, some unit chars are utf8 (micro, square, etc.).
	 * See bug #36090.
	 */
	const short txt_flag = V3D_CACHE_TEXT_LOCALCLIP | (unit->system ? 0 : V3D_CACHE_TEXT_ASCII);
	Mesh *me = ob->data;
	float v1[3], v2[3], v3[3], vmid[3], fvec[3];
	char numstr[32]; /* Stores the measurement display text here */
	size_t numstr_len;
	const char *conv_float; /* Use a float conversion matching the grid size */
	unsigned char col[4] = {0, 0, 0, 255}; /* color of the text to draw */
	float area; /* area of the face */
	float grid = unit->system ? unit->scale_length : v3d->grid;
	const bool do_split = (unit->flag & USER_UNIT_OPT_SPLIT) != 0;
	const bool do_global = (v3d->flag & V3D_GLOBAL_STATS) != 0;
	const bool do_moving = (G.moving & G_TRANSFORM_EDIT) != 0;
	/* when 2 edge-info options are enabled, space apart */
	const bool do_edge_textpair = (me->drawflag & ME_DRAWEXTRA_EDGELEN) && (me->drawflag & ME_DRAWEXTRA_EDGEANG);
	const float edge_texpair_sep = 0.4f;
	float clip_planes[4][4];
	/* allow for displaying shape keys and deform mods */
	DerivedMesh *dm = EDBM_mesh_deform_dm_get(em);
	BMIter iter;

	/* make the precision of the display value proportionate to the gridsize */

	if (grid <= 0.01f) conv_float = "%.6g";
	else if (grid <= 0.1f) conv_float = "%.5g";
	else if (grid <= 1.0f) conv_float = "%.4g";
	else if (grid <= 10.0f) conv_float = "%.3g";
	else conv_float = "%.2g";

	if (me->drawflag & (ME_DRAWEXTRA_EDGELEN | ME_DRAWEXTRA_EDGEANG)) {
		BoundBox bb;
		const rcti rect = {0, ar->winx, 0, ar->winy};

		ED_view3d_clipping_calc(&bb, clip_planes, ar, em->ob, &rect);
	}

	if (me->drawflag & ME_DRAWEXTRA_EDGELEN) {
		BMEdge *eed;

		UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGELEN, col);

		if (dm) {
			BM_mesh_elem_index_ensure(em->bm, BM_VERT);
		}

		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			/* draw selected edges, or edges next to selected verts while dragging */
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT) ||
			    (do_moving && (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) ||
			                   BM_elem_flag_test(eed->v2, BM_ELEM_SELECT))))
			{
				float v1_clip[3], v2_clip[3];

				if (dm) {
					dm->getVertCo(dm, BM_elem_index_get(eed->v1), v1);
					dm->getVertCo(dm, BM_elem_index_get(eed->v2), v2);
				}
				else {
					copy_v3_v3(v1, eed->v1->co);
					copy_v3_v3(v2, eed->v2->co);
				}

				if (clip_segment_v3_plane_n(v1, v2, clip_planes, 4, v1_clip, v2_clip)) {

					if (do_edge_textpair) {
						interp_v3_v3v3(vmid, v1, v2, edge_texpair_sep);
					}
					else {
						mid_v3_v3v3(vmid, v1_clip, v2_clip);
					}

					if (do_global) {
						mul_mat3_m4_v3(ob->obmat, v1);
						mul_mat3_m4_v3(ob->obmat, v2);
					}

					if (unit->system) {
						numstr_len = bUnit_AsString(numstr, sizeof(numstr), len_v3v3(v1, v2) * unit->scale_length, 3,
						                            unit->system, B_UNIT_LENGTH, do_split, false);
					}
					else {
						numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), conv_float, len_v3v3(v1, v2));
					}

					view3d_cached_text_draw_add(vmid, numstr, numstr_len, 0, txt_flag, col);
				}
			}
		}
	}

	if (me->drawflag & ME_DRAWEXTRA_EDGEANG) {
		const bool is_rad = (unit->system_rotation == USER_UNIT_ROT_RADIANS);
		BMEdge *eed;

		UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGEANG, col);

		if (dm) {
			BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_FACE);
		}

		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			BMLoop *l_a, *l_b;
			if (BM_edge_loop_pair(eed, &l_a, &l_b)) {
				/* draw selected edges, or edges next to selected verts while dragging */
				if (BM_elem_flag_test(eed, BM_ELEM_SELECT) ||
				    (do_moving && (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) ||
				                   BM_elem_flag_test(eed->v2, BM_ELEM_SELECT) ||
				                   /* special case, this is useful to show when verts connected to
				                    * this edge via a face are being transformed */
				                   BM_elem_flag_test(l_a->next->next->v, BM_ELEM_SELECT) ||
				                   BM_elem_flag_test(l_a->prev->v, BM_ELEM_SELECT)       ||
				                   BM_elem_flag_test(l_b->next->next->v, BM_ELEM_SELECT) ||
				                   BM_elem_flag_test(l_b->prev->v, BM_ELEM_SELECT)
				                   )))
				{
					float v1_clip[3], v2_clip[3];

					if (dm) {
						dm->getVertCo(dm, BM_elem_index_get(eed->v1), v1);
						dm->getVertCo(dm, BM_elem_index_get(eed->v2), v2);
					}
					else {
						copy_v3_v3(v1, eed->v1->co);
						copy_v3_v3(v2, eed->v2->co);
					}

					if (clip_segment_v3_plane_n(v1, v2, clip_planes, 4, v1_clip, v2_clip)) {
						float no_a[3], no_b[3];
						float angle;

						if (do_edge_textpair) {
							interp_v3_v3v3(vmid, v2_clip, v1_clip, edge_texpair_sep);
						}
						else {
							mid_v3_v3v3(vmid, v1_clip, v2_clip);
						}

						if (dm) {
							dm->getPolyNo(dm, BM_elem_index_get(l_a->f), no_a);
							dm->getPolyNo(dm, BM_elem_index_get(l_b->f), no_b);
						}
						else {
							copy_v3_v3(no_a, l_a->f->no);
							copy_v3_v3(no_b, l_b->f->no);
						}

						if (do_global) {
							mul_mat3_m4_v3(ob->imat, no_a);
							mul_mat3_m4_v3(ob->imat, no_b);
							normalize_v3(no_a);
							normalize_v3(no_b);
						}

						angle = angle_normalized_v3v3(no_a, no_b);

						numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%.3f", is_rad ? angle : RAD2DEGF(angle));

						view3d_cached_text_draw_add(vmid, numstr, numstr_len, 0, txt_flag, col);
					}
				}
			}
		}
	}

	if (me->drawflag & ME_DRAWEXTRA_FACEAREA) {
		/* would be nice to use BM_face_calc_area, but that is for 2d faces
		 * so instead add up tessellation triangle areas */
		BMFace *f = NULL;

#define DRAW_EM_MEASURE_STATS_FACEAREA()                                                 \
	if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {                                          \
		mul_v3_fl(vmid, 1.0f / (float)n);                                                \
		if (unit->system) {                                                              \
			numstr_len = bUnit_AsString(                                                 \
			        numstr, sizeof(numstr),                                              \
			        (double)(area * unit->scale_length * unit->scale_length),            \
			        3, unit->system, B_UNIT_AREA, do_split, false);                      \
		}                                                                                \
		else {                                                                           \
			numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), conv_float, area);    \
		}                                                                                \
		view3d_cached_text_draw_add(vmid, numstr, numstr_len, 0, txt_flag, col);         \
	} (void)0

		UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEAREA, col);
		
		if (dm) {
			BM_mesh_elem_index_ensure(em->bm, BM_VERT);
		}

		area = 0.0;
		zero_v3(vmid);
		int n = 0;
		for (int i = 0; i < em->tottri; i++) {
			BMLoop **l = em->looptris[i];
			if (f && l[0]->f != f) {
				DRAW_EM_MEASURE_STATS_FACEAREA();
				zero_v3(vmid);
				area = 0.0;
				n = 0;
			}

			f = l[0]->f;

			if (dm) {
				dm->getVertCo(dm, BM_elem_index_get(l[0]->v), v1);
				dm->getVertCo(dm, BM_elem_index_get(l[1]->v), v2);
				dm->getVertCo(dm, BM_elem_index_get(l[2]->v), v3);
			}
			else {
				copy_v3_v3(v1, l[0]->v->co);
				copy_v3_v3(v2, l[1]->v->co);
				copy_v3_v3(v3, l[2]->v->co);
			}

			add_v3_v3(vmid, v1);
			add_v3_v3(vmid, v2);
			add_v3_v3(vmid, v3);
			n += 3;
			if (do_global) {
				mul_mat3_m4_v3(ob->obmat, v1);
				mul_mat3_m4_v3(ob->obmat, v2);
				mul_mat3_m4_v3(ob->obmat, v3);
			}
			area += area_tri_v3(v1, v2, v3);
		}

		if (f) {
			DRAW_EM_MEASURE_STATS_FACEAREA();
		}
#undef DRAW_EM_MEASURE_STATS_FACEAREA
	}

	if (me->drawflag & ME_DRAWEXTRA_FACEANG) {
		BMFace *efa;
		const bool is_rad = (unit->system_rotation == USER_UNIT_ROT_RADIANS);

		UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEANG, col);

		if (dm) {
			BM_mesh_elem_index_ensure(em->bm, BM_VERT);
		}

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			const bool is_face_sel = BM_elem_flag_test_bool(efa, BM_ELEM_SELECT);

			if (is_face_sel || do_moving) {
				BMIter liter;
				BMLoop *loop;
				bool is_first = true;

				BM_ITER_ELEM (loop, &liter, efa, BM_LOOPS_OF_FACE) {
					if (is_face_sel ||
					    (do_moving &&
					     (BM_elem_flag_test(loop->v, BM_ELEM_SELECT) ||
					      BM_elem_flag_test(loop->prev->v, BM_ELEM_SELECT) ||
					      BM_elem_flag_test(loop->next->v, BM_ELEM_SELECT))))
					{
						float v2_local[3];

						/* lazy init center calc */
						if (is_first) {
							if (dm) {
								BMLoop *l_iter, *l_first;
								float tvec[3];
								zero_v3(vmid);
								l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
								do {
									dm->getVertCo(dm, BM_elem_index_get(l_iter->v), tvec);
									add_v3_v3(vmid, tvec);
								} while ((l_iter = l_iter->next) != l_first);
								mul_v3_fl(vmid, 1.0f / (float)efa->len);
							}
							else {
								BM_face_calc_center_bounds(efa, vmid);
							}
							is_first = false;
						}

						if (dm) {
							dm->getVertCo(dm, BM_elem_index_get(loop->prev->v), v1);
							dm->getVertCo(dm, BM_elem_index_get(loop->v),       v2);
							dm->getVertCo(dm, BM_elem_index_get(loop->next->v), v3);
						}
						else {
							copy_v3_v3(v1, loop->prev->v->co);
							copy_v3_v3(v2, loop->v->co);
							copy_v3_v3(v3, loop->next->v->co);
						}

						copy_v3_v3(v2_local, v2);

						if (do_global) {
							mul_mat3_m4_v3(ob->obmat, v1);
							mul_mat3_m4_v3(ob->obmat, v2);
							mul_mat3_m4_v3(ob->obmat, v3);
						}

						float angle = angle_v3v3v3(v1, v2, v3);

						numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%.3f", is_rad ? angle : RAD2DEGF(angle));
						interp_v3_v3v3(fvec, vmid, v2_local, 0.8f);
						view3d_cached_text_draw_add(fvec, numstr, numstr_len, 0, txt_flag, col);
					}
				}
			}
		}
	}
}

static void draw_em_indices(BMEditMesh *em)
{
	const short txt_flag = V3D_CACHE_TEXT_ASCII | V3D_CACHE_TEXT_LOCALCLIP;
	BMEdge *e;
	BMFace *f;
	BMVert *v;
	char numstr[32];
	size_t numstr_len;
	float pos[3];
	unsigned char col[4];

	BMIter iter;
	BMesh *bm = em->bm;

	/* For now, reuse appropriate theme colors from stats text colors */
	int i = 0;
	if (em->selectmode & SCE_SELECT_VERTEX) {
		UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEANG, col);
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", i);
				view3d_cached_text_draw_add(v->co, numstr, numstr_len, 0, txt_flag, col);
			}
			i++;
		}
	}

	if (em->selectmode & SCE_SELECT_EDGE) {
		i = 0;
		UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGELEN, col);
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
				numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", i);
				mid_v3_v3v3(pos, e->v1->co, e->v2->co);
				view3d_cached_text_draw_add(pos, numstr, numstr_len, 0, txt_flag, col);
			}
			i++;
		}
	}

	if (em->selectmode & SCE_SELECT_FACE) {
		i = 0;
		UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEAREA, col);
		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
				BM_face_calc_center_mean(f, pos);
				numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", i);
				view3d_cached_text_draw_add(pos, numstr, numstr_len, 0, txt_flag, col);
			}
			i++;
		}
	}
}

static DMDrawOption draw_em_fancy__setFaceOpts(void *userData, int index)
{
	BMEditMesh *em = userData;

	if (UNLIKELY(index >= em->bm->totface))
		return DM_DRAW_OPTION_NORMAL;

	BMFace *efa = BM_face_at_index(em->bm, index);
	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

static DMDrawOption draw_em_fancy__setGLSLFaceOpts(void *userData, int index)
{
	BMEditMesh *em = userData;

	if (UNLIKELY(index >= em->bm->totface))
		return DM_DRAW_OPTION_NORMAL;

	BMFace *efa = BM_face_at_index(em->bm, index);

	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

static void draw_em_fancy(Scene *scene, SceneLayer *sl, ARegion *ar, View3D *v3d,
                          Object *ob, BMEditMesh *em, DerivedMesh *cageDM, DerivedMesh *finalDM, const char dt)

{
	RegionView3D *rv3d = ar->regiondata;
	Mesh *me = ob->data;
	const bool use_occlude_wire = (dt > OB_WIRE) && (v3d->flag2 & V3D_OCCLUDE_WIRE);
	bool use_depth_offset = false;
	
	glLineWidth(1.0f);
	
	BM_mesh_elem_table_ensure(em->bm, BM_VERT | BM_EDGE | BM_FACE);

	if (check_object_draw_editweight(me, finalDM)) {
		if (dt > OB_WIRE) {
			draw_mesh_paint_weight_faces(finalDM, true, draw_em_fancy__setFaceOpts, me->edit_btmesh);

			ED_view3d_polygon_offset(rv3d, 1.0);
			glDepthMask(GL_FALSE);
			use_depth_offset = true;
		}
		else {
			glEnable(GL_DEPTH_TEST);
			draw_mesh_paint_weight_faces(finalDM, false, draw_em_fancy__setFaceOpts, me->edit_btmesh);
			draw_mesh_paint_weight_edges(rv3d, finalDM, true, true, draw_dm_edges__setDrawOptions, me->edit_btmesh->bm);
			glDisable(GL_DEPTH_TEST);
		}
	}
	else if (dt > OB_WIRE) {
		if (use_occlude_wire) {
			/* use the cageDM since it always overlaps the editmesh faces */
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			cageDM->drawMappedFaces(cageDM, draw_em_fancy__setFaceOpts,
			                        GPU_object_material_bind, NULL, me->edit_btmesh, DM_DRAW_SKIP_HIDDEN | DM_DRAW_NEED_NORMALS);
			GPU_object_material_unbind();
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		else if (check_object_draw_texture(scene, v3d, dt)) {
			if (draw_glsl_material(scene, sl, ob, v3d, dt)) {
				glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);

				finalDM->drawMappedFacesGLSL(finalDM, GPU_object_material_bind,
				                             draw_em_fancy__setGLSLFaceOpts, em);
				GPU_object_material_unbind();

				glFrontFace(GL_CCW);
			}
			else {
				draw_mesh_textured(scene, sl, v3d, rv3d, ob, finalDM, 0);
			}
		}
		else {
			glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);
			finalDM->drawMappedFaces(finalDM, draw_em_fancy__setFaceOpts, GPU_object_material_bind, NULL, me->edit_btmesh, DM_DRAW_SKIP_HIDDEN | DM_DRAW_NEED_NORMALS);

			glFrontFace(GL_CCW);

			GPU_object_material_unbind();
		}

		/* Setup for drawing wire over, disable zbuffer
		 * write to show selected edge wires better */
		UI_ThemeColor(TH_WIRE_EDIT);

		ED_view3d_polygon_offset(rv3d, 1.0);
		glDepthMask(GL_FALSE);
		use_depth_offset = true;
	}
	else {
		if (cageDM != finalDM) {
			UI_ThemeColorBlend(TH_WIRE_EDIT, TH_BACK, 0.7);
			finalDM->drawEdges(finalDM, 1, 0);
		}
	}

	if ((dt > OB_WIRE) && (v3d->flag2 & V3D_RENDER_SHADOW)) {
		/* pass */
	}
	else {
		/* annoying but active faces is stored differently */
		BMFace *efa_act = BM_mesh_active_face_get(em->bm, false, true);
		BMEdge *eed_act = NULL;
		BMVert *eve_act = NULL;

		if (em->bm->selected.last) {
			BMEditSelection *ese = em->bm->selected.last;
			/* face is handled above */
#if 0
			if (ese->type == BM_FACE) {
				efa_act = (BMFace *)ese->data;
			}
			else
#endif
			if (ese->htype == BM_EDGE) {
				eed_act = (BMEdge *)ese->ele;
			}
			else if (ese->htype == BM_VERT) {
				eve_act = (BMVert *)ese->ele;
			}
		}

		if ((me->drawflag & ME_DRAWFACES) && (use_occlude_wire == false)) {  /* transp faces */
			unsigned char col1[4], col2[4], col3[4];
#ifdef WITH_FREESTYLE
			unsigned char col4[4];
#endif

			UI_GetThemeColor4ubv(TH_FACE, col1);
			UI_GetThemeColor4ubv(TH_FACE_SELECT, col2);
			UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, col3);
#ifdef WITH_FREESTYLE
			UI_GetThemeColor4ubv(TH_FREESTYLE_FACE_MARK, col4);
#endif

			glEnable(GL_BLEND);
			glDepthMask(GL_FALSE);  /* disable write in zbuffer, needed for nice transp */

			/* don't draw unselected faces, only selected, this is MUCH nicer when texturing */
			if (check_object_draw_texture(scene, v3d, dt))
				col1[3] = 0;

#ifdef WITH_FREESTYLE
			if (!(me->drawflag & ME_DRAW_FREESTYLE_FACE) || !CustomData_has_layer(&em->bm->pdata, CD_FREESTYLE_FACE))
				col4[3] = 0;

			draw_dm_faces_sel(em, cageDM, col1, col2, col3, col4, efa_act);
#else
			draw_dm_faces_sel(em, cageDM, col1, col2, col3, efa_act);
#endif

			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);  /* restore write in zbuffer */
		}
		else if (efa_act) {
			/* even if draw faces is off it would be nice to draw the stipple face
			 * Make all other faces zero alpha except for the active */
			unsigned char col1[4], col2[4], col3[4];
#ifdef WITH_FREESTYLE
			unsigned char col4[4];
			col4[3] = 0; /* don't draw */
#endif
			col1[3] = col2[3] = 0; /* don't draw */

			UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, col3);

			glEnable(GL_BLEND);
			glDepthMask(GL_FALSE);  /* disable write in zbuffer, needed for nice transp */

#ifdef WITH_FREESTYLE
			draw_dm_faces_sel(em, cageDM, col1, col2, col3, col4, efa_act);
#else
			draw_dm_faces_sel(em, cageDM, col1, col2, col3, efa_act);
#endif

			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);  /* restore write in zbuffer */
		}

		/* here starts all fancy draw-extra over */
		if ((me->drawflag & ME_DRAWEDGES) == 0 && check_object_draw_texture(scene, v3d, dt)) {
			/* we are drawing textures and 'ME_DRAWEDGES' is disabled, don't draw any edges */

			/* only draw selected edges otherwise there is no way of telling if a face is selected */
			draw_em_fancy_edges(em, scene, v3d, me, cageDM, 1, eed_act);

		}
		else {
			if (me->drawflag & ME_DRAWSEAMS) {
				UI_ThemeColor(TH_EDGE_SEAM);
				glLineWidth(2.0f);

				draw_dm_edges_seams(em, cageDM);

				glColor3ub(0, 0, 0);
			}

			if (me->drawflag & ME_DRAWSHARP) {
				UI_ThemeColor(TH_EDGE_SHARP);
				glLineWidth(2.0f);

				draw_dm_edges_sharp(em, cageDM);

				glColor3ub(0, 0, 0);
			}

#ifdef WITH_FREESTYLE
			if (me->drawflag & ME_DRAW_FREESTYLE_EDGE && CustomData_has_layer(&em->bm->edata, CD_FREESTYLE_EDGE)) {
				UI_ThemeColor(TH_FREESTYLE_EDGE_MARK);
				glLineWidth(2.0f);

				draw_dm_edges_freestyle(em, cageDM);

				glColor3ub(0, 0, 0);
			}
#endif

			if (me->drawflag & ME_DRAWCREASES) {
				draw_dm_creases(em, cageDM);
			}
			if (me->drawflag & ME_DRAWBWEIGHTS) {
				draw_dm_bweights(em, scene, cageDM);
			}

			glLineWidth(1.0f);
			draw_em_fancy_edges(em, scene, v3d, me, cageDM, 0, eed_act);
		}

		{
			draw_em_fancy_verts(scene, v3d, ob, em, cageDM, eve_act, rv3d);

			if (me->drawflag & ME_DRAWNORMALS) {
				draw_dm_face_normals(em, scene, ob, cageDM, TH_NORMAL);
			}
			if (me->drawflag & ME_DRAW_VNORMALS) {
				draw_dm_vert_normals(em, scene, ob, cageDM, TH_VNORMAL);
			}
			if (me->drawflag & ME_DRAW_LNORMALS) {
				draw_dm_loop_normals(em, scene, ob, cageDM, TH_LNORMAL);
			}

			if ((me->drawflag & (ME_DRAWEXTRA_EDGELEN |
			                     ME_DRAWEXTRA_FACEAREA |
			                     ME_DRAWEXTRA_FACEANG |
			                     ME_DRAWEXTRA_EDGEANG)) &&
			    !(v3d->flag2 & V3D_RENDER_OVERRIDE))
			{
				draw_em_measure_stats(ar, v3d, ob, em, &scene->unit);
			}

			if ((G.debug & G_DEBUG) && (me->drawflag & ME_DRAWEXTRA_INDICES) &&
			    !(v3d->flag2 & V3D_RENDER_OVERRIDE))
			{
				draw_em_indices(em);
			}
		}
	}

	if (use_depth_offset) {
		glDepthMask(GL_TRUE);
		ED_view3d_polygon_offset(rv3d, 0.0);
		GPU_object_material_unbind();
	}
#if 0  /* currently not needed */
	else if (use_occlude_wire) {
		ED_view3d_polygon_offset(rv3d, 0.0);
	}
#endif
}

static void draw_em_fancy_new(Scene *UNUSED(scene), ARegion *UNUSED(ar), View3D *UNUSED(v3d),
                              Object *UNUSED(ob), Mesh *me, BMEditMesh *UNUSED(em), DerivedMesh *UNUSED(cageDM), DerivedMesh *UNUSED(finalDM), const char UNUSED(dt))
{
	/* for now... something simple! */
	Batch *surface = BKE_mesh_batch_cache_get_all_triangles(me);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable(GL_BLEND);

	/* disable depth writes for transparent surface, so it doesn't interfere with itself */
	glDepthMask(GL_FALSE);

	Batch_set_builtin_program(surface, GPU_SHADER_3D_UNIFORM_COLOR);
	Batch_Uniform4f(surface, "color", 1.0f, 0.5f, 0.0f, 0.5f);
	Batch_draw(surface);

#if 0 /* until I understand finalDM better */
	if (finalDM != cageDM) {
		puts("finalDM != cageDM");
		Batch *finalSurface = MBC_get_all_triangles(finalDM);
		Batch_set_builtin_program(finalSurface, GPU_SHADER_3D_UNIFORM_COLOR);
		Batch_Uniform4f(finalSurface, "color", 0.0f, 0.0f, 0.0f, 0.05f);
		Batch_draw(finalSurface);
	}
#endif

	glDepthMask(GL_TRUE);

	/* now write surface depth so other objects won't poke through
	 * NOTE: does not help as much as desired
	 * TODO: draw edit object last to avoid this mess
	 */
	Batch_set_builtin_program(surface, GPU_SHADER_3D_DEPTH_ONLY);
	Batch_draw(surface);

	if (GLEW_VERSION_3_2) {
#if 0
		Batch *overlay = BKE_mesh_batch_cache_get_overlay_edges(me);
		Batch_set_builtin_program(overlay, GPU_SHADER_EDGES_OVERLAY);
		Batch_Uniform2f(overlay, "viewportSize", ar->winx, ar->winy);
		Batch_draw(overlay);
#endif

#if 0 /* TODO: use this SIMPLE variant for pure triangle meshes */
		Batch_set_builtin_program(surface, GPU_SHADER_EDGES_OVERLAY_SIMPLE);
		/* use these defaults:
		 * const float edgeColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		 * Batch_Uniform4f(surface, "fillColor", edgeColor[0], edgeColor[1], edgeColor[2], 0.0f);
		 * Batch_Uniform4fv(surface, "outlineColor", edgeColor);
		 * Batch_Uniform1f(surface, "outlineWidth", 1.0f);
		 */
		Batch_Uniform2f(surface, "viewportSize", ar->winx, ar->winy);
		Batch_draw(surface);
#endif
	}
	else {
		Batch *edges = BKE_mesh_batch_cache_get_all_edges(me);
		Batch_set_builtin_program(edges, GPU_SHADER_3D_UNIFORM_COLOR);
		Batch_Uniform4f(edges, "color", 0.0f, 0.0f, 0.0f, 1.0f);
		glEnable(GL_LINE_SMOOTH);
		glLineWidth(1.5f);
		Batch_draw(edges);
		glDisable(GL_LINE_SMOOTH);
	}

#if 0 /* looks good even without points */
	Batch *verts = MBC_get_all_verts(me);
	glEnable(GL_BLEND);

	Batch_set_builtin_program(verts, GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
	Batch_Uniform4f(verts, "color", 0.0f, 0.0f, 0.0f, 1.0f);
	Batch_Uniform1f(verts, "size", UI_GetThemeValuef(TH_VERTEX_SIZE) * 1.5f);
	Batch_draw(verts);

	glDisable(GL_BLEND);
#endif
}

/* Mesh drawing routines */

void draw_mesh_object_outline(View3D *v3d, Object *ob, DerivedMesh *dm, const unsigned char ob_wire_col[4]) /* LEGACY */
{
	if ((v3d->transp == false) &&  /* not when we draw the transparent pass */
	    (ob->mode & OB_MODE_ALL_PAINT) == false) /* not when painting (its distracting) - campbell */
	{
		glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
		glDepthMask(GL_FALSE);

		if (ob_wire_col) glColor4ubv(ob_wire_col);

		/* if transparent, we cannot draw the edges for solid select... edges
		 * have no material info. GPU_object_material_visible will skip the
		 * transparent faces */
		if (ob->dtx & OB_DRAWTRANSP) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			dm->drawFacesSolid(dm, NULL, 0, GPU_object_material_visible);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		else {
			dm->drawEdges(dm, 0, 1);
		}

		glDepthMask(GL_TRUE);
	}
}

static void draw_mesh_object_outline_new(View3D *v3d, RegionView3D *rv3d, Object *ob, Mesh *me, const bool is_active)
{
	if ((v3d->transp == false) &&  /* not when we draw the transparent pass */
	    (ob->mode & OB_MODE_ALL_PAINT) == false) /* not when painting (its distracting) - campbell */
	{
		glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
		glDepthMask(GL_FALSE);

		float outline_color[4];
		UI_GetThemeColor4fv((is_active ? TH_ACTIVE : TH_SELECT), outline_color);

#if 1 /* new version that draws only silhouette edges */
		Batch *fancy_edges = BKE_mesh_batch_cache_get_fancy_edges(me);

		if (rv3d->persp == RV3D_ORTHO) {
			Batch_set_builtin_program(fancy_edges, GPU_SHADER_EDGES_FRONT_BACK_ORTHO);
			/* set eye vector, transformed to object coords */
			float eye[3] = { 0.0f, 0.0f, 1.0f }; /* looking into the screen */
			mul_m3_v3((float (*)[3])gpuGetNormalMatrixInverse(NULL), eye);
			Batch_Uniform3fv(fancy_edges, "eye", eye);
		}
		else {
			Batch_set_builtin_program(fancy_edges, GPU_SHADER_EDGES_FRONT_BACK_PERSP);
		}

		Batch_Uniform1b(fancy_edges, "drawFront", false);
		Batch_Uniform1b(fancy_edges, "drawBack", false);
		Batch_Uniform1b(fancy_edges, "drawSilhouette", true);
		Batch_Uniform4fv(fancy_edges, "silhouetteColor", outline_color);

		Batch_draw(fancy_edges);
#else /* alternate version that matches look of old viewport (but more efficient) */
		Batch *batch = MBC_get_all_edges(dm);
		Batch_set_builtin_program(batch, GPU_SHADER_3D_UNIFORM_COLOR);
		Batch_Uniform4fv(batch, "color", outline_color);
		Batch_draw(batch);
#endif

		glDepthMask(GL_TRUE);
	}
}

static bool object_is_halo(Scene *scene, Object *ob)
{
	const Material *ma = give_current_material(ob, 1);
	return (ma && (ma->material_type == MA_TYPE_HALO) && !BKE_scene_use_new_shading_nodes(scene));
}

static void draw_mesh_fancy(Scene *scene, SceneLayer *sl, ARegion *ar, View3D *v3d, RegionView3D *rv3d, Base *base,
                            const char dt, const unsigned char ob_wire_col[4], const short dflag)
{
#ifdef WITH_GAMEENGINE
	Object *ob = (rv3d->rflag & RV3D_IS_GAME_ENGINE) ? BKE_object_lod_meshob_get(base->object, sl) : base->object;
#else
	Object *ob = base->object;
#endif
	Mesh *me = ob->data;
	eWireDrawMode draw_wire = OBDRAW_WIRE_OFF;
	bool /* no_verts,*/ no_edges, no_faces;
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, scene->customdata_mask);
	const bool is_obact = (ob == OBACT_NEW);
	int draw_flags = (is_obact && BKE_paint_select_face_test(ob)) ? DRAW_FACE_SELECT : 0;

	if (!dm)
		return;

	DM_update_materials(dm, ob);

	/* Check to draw dynamic paint colors (or weights from WeightVG modifiers).
	 * Note: Last "preview-active" modifier in stack will win! */
	if (DM_get_loop_data_layer(dm, CD_PREVIEW_MLOOPCOL) && modifiers_isPreview(ob))
		draw_flags |= DRAW_MODIFIERS_PREVIEW;

	/* Unwanted combination */
	if (draw_flags & DRAW_FACE_SELECT) {
		draw_wire = OBDRAW_WIRE_OFF;
	}
	else if (ob->dtx & OB_DRAWWIRE) {
		draw_wire = OBDRAW_WIRE_ON_DEPTH; /* draw wire after solid using zoffset and depth buffer adjusment */
	}
	
	/* check polys instead of tessfaces because of dyntopo where tessfaces don't exist */
	if (dm->type == DM_TYPE_CCGDM) {
		no_edges = !subsurf_has_edges(dm);
		no_faces = !subsurf_has_faces(dm);
	}
	else {
		no_edges = (dm->getNumEdges(dm) == 0);
		no_faces = (dm->getNumPolys(dm) == 0);
	}

	/* vertexpaint, faceselect wants this, but it doesnt work for shaded? */
	glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);

	if (dt == OB_BOUNDBOX) {
		if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_WIRE) == 0)
			draw_bounding_volume(ob, ob->boundtype, ob_wire_col);
	}
	else if ((no_faces && no_edges) ||
	         ((!is_obact || (ob->mode == OB_MODE_OBJECT)) && object_is_halo(scene, ob)))
	{
		glPointSize(1.5f);
		dm->drawVerts(dm);
	}
	else if ((dt == OB_WIRE) || no_faces) {
		draw_wire = OBDRAW_WIRE_ON; /* draw wire only, no depth buffer stuff */
	}
	else if (((is_obact && ob->mode & OB_MODE_TEXTURE_PAINT)) ||
	         check_object_draw_texture(scene, v3d, dt))
	{
		bool draw_loose = true;

		if ((v3d->flag & V3D_SELECT_OUTLINE) &&
		    ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
		    (base->flag & BASE_SELECTED) &&
		    !(G.f & G_PICKSEL || (draw_flags & DRAW_FACE_SELECT)) &&
		    (draw_wire == OBDRAW_WIRE_OFF))
		{
			draw_mesh_object_outline(v3d, ob, dm, ob_wire_col);
		}

		if (draw_glsl_material(scene, sl, ob, v3d, dt) && !(draw_flags & DRAW_MODIFIERS_PREVIEW)) {
			Paint *p;

			glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);

			if ((v3d->flag2 & V3D_SHOW_SOLID_MATCAP) && ob->sculpt && (p = BKE_paint_get_active(scene, sl))) {
				GPUVertexAttribs gattribs;
				float planes[4][4];
				float (*fpl)[4] = NULL;
				const bool fast = (p->flags & PAINT_FAST_NAVIGATE) && (rv3d->rflag & RV3D_NAVIGATING);

				if (ob->sculpt->partial_redraw) {
					if (ar->do_draw & RGN_DRAW_PARTIAL) {
						ED_sculpt_redraw_planes_get(planes, ar, ob);
						fpl = planes;
						ob->sculpt->partial_redraw = 0;
					}
				}

				GPU_object_material_bind(1, &gattribs);
				dm->drawFacesSolid(dm, fpl, fast, NULL);
				draw_loose = false;
			}
			else
				dm->drawFacesGLSL(dm, GPU_object_material_bind);

#if 0 /* XXX */
			if (BKE_bproperty_object_get(ob, "Text"))
				draw_mesh_text(ob, 1);
#endif
			GPU_object_material_unbind();

			glFrontFace(GL_CCW);

			if (draw_flags & DRAW_FACE_SELECT)
				draw_mesh_face_select(rv3d, me, dm, false);
		}
		else {
			draw_mesh_textured(scene, sl, v3d, rv3d, ob, dm, draw_flags);
		}

		if (draw_loose && !(draw_flags & DRAW_FACE_SELECT)) {
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					glColor3ubv(ob_wire_col);
				}
				glLineWidth(1.0f);
				dm->drawLooseEdges(dm);
			}
		}
	}
	else if (dt == OB_SOLID) {
		if (draw_flags & DRAW_MODIFIERS_PREVIEW) {
			/* for object selection draws no shade */
			if (dflag & (DRAW_PICKING | DRAW_CONSTCOLOR)) {
				dm->drawFacesSolid(dm, NULL, 0, GPU_object_material_bind);
				GPU_object_material_unbind();
			}
			else {
				const float specular[3] = {0.47f, 0.47f, 0.47f};

				/* draw outline */
				if ((v3d->flag & V3D_SELECT_OUTLINE) &&
				    ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
				    (base->flag & BASE_SELECTED) &&
				    (draw_wire == OBDRAW_WIRE_OFF) &&
				    (ob->sculpt == NULL))
				{
					draw_mesh_object_outline(v3d, ob, dm, ob_wire_col);
				}

				/* materials arent compatible with vertex colors */
				GPU_end_object_materials();

				/* set default specular */
				GPU_basic_shader_colors(NULL, specular, 35, 1.0f);
				GPU_basic_shader_bind(GPU_SHADER_LIGHTING | GPU_SHADER_USE_COLOR);

				dm->drawMappedFaces(dm, NULL, NULL, NULL, NULL, DM_DRAW_USE_COLORS | DM_DRAW_NEED_NORMALS);

				GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
			}
		}
		else {
			Paint *p;

			if ((v3d->flag & V3D_SELECT_OUTLINE) &&
			    ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
			    (base->flag & BASE_SELECTED) &&
			    (draw_wire == OBDRAW_WIRE_OFF) &&
			    (ob->sculpt == NULL))
			{
				draw_mesh_object_outline(v3d, ob, dm, ob_wire_col);
			}

			glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);

			if (ob->sculpt && (p = BKE_paint_get_active(scene, sl))) {
				float planes[4][4];
				float (*fpl)[4] = NULL;
				const bool fast = (p->flags & PAINT_FAST_NAVIGATE) && (rv3d->rflag & RV3D_NAVIGATING);

				if (ob->sculpt->partial_redraw) {
					if (ar->do_draw & RGN_DRAW_PARTIAL) {
						ED_sculpt_redraw_planes_get(planes, ar, ob);
						fpl = planes;
						ob->sculpt->partial_redraw = 0;
					}
				}

				dm->drawFacesSolid(dm, fpl, fast, GPU_object_material_bind);
			}
			else
				dm->drawFacesSolid(dm, NULL, 0, GPU_object_material_bind);

			glFrontFace(GL_CCW);

			GPU_object_material_unbind();

			if (!ob->sculpt && (v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					glColor3ubv(ob_wire_col);
				}
				glLineWidth(1.0f);
				dm->drawLooseEdges(dm);
			}
		}
	}
	else if (dt == OB_PAINT) {
		draw_mesh_paint(v3d, rv3d, ob, dm, draw_flags);

		/* since we already draw wire as wp guide, don't draw over the top */
		draw_wire = OBDRAW_WIRE_OFF;
	}

	if ((draw_wire != OBDRAW_WIRE_OFF) &&  /* draw extra wire */
	    /* when overriding with render only, don't bother */
	    (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_SOLID) == 0))
	{
		/* When using wireframe object draw in particle edit mode
		 * the mesh gets in the way of seeing the particles, fade the wire color
		 * with the background. */

		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			if (is_obact && (ob->mode & OB_MODE_PARTICLE_EDIT)) {
				float color[3];
				ob_wire_color_blend_theme_id(ob_wire_col, TH_BACK, 0.15f, color);
				glColor3fv(color);
			}
			else {
				glColor3ubv(ob_wire_col);
			}
		}

		/* If drawing wire and drawtype is not OB_WIRE then we are
		 * overlaying the wires.
		 *
		 * UPDATE bug #10290 - With this wire-only objects can draw
		 * behind other objects depending on their order in the scene. 2x if 0's below. undo'ing zr's commit: r4059
		 *
		 * if draw wire is 1 then just drawing wire, no need for depth buffer stuff,
		 * otherwise this wire is to overlay solid mode faces so do some depth buffer tricks.
		 */
		if (dt != OB_WIRE && (draw_wire == OBDRAW_WIRE_ON_DEPTH)) {
			ED_view3d_polygon_offset(rv3d, 1.0);
			glDepthMask(GL_FALSE);  /* disable write in zbuffer, selected edge wires show better */
		}
		
		glLineWidth(1.0f);
		dm->drawEdges(dm, ((dt == OB_WIRE) || no_faces), (ob->dtx & OB_DRAW_ALL_EDGES) != 0);

		if (dt != OB_WIRE && (draw_wire == OBDRAW_WIRE_ON_DEPTH)) {
			glDepthMask(GL_TRUE);
			ED_view3d_polygon_offset(rv3d, 0.0);
		}
	}
	
	if (is_obact && BKE_paint_select_vert_test(ob)) {
		const bool use_depth = (v3d->flag & V3D_ZBUF_SELECT) != 0;
		glColor3f(0.0f, 0.0f, 0.0f);
		glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

		if (!use_depth) glDisable(GL_DEPTH_TEST);
		else            ED_view3d_polygon_offset(rv3d, 1.0);
		drawSelectedVertices(dm, ob->data);
		if (!use_depth) glEnable(GL_DEPTH_TEST);
		else            ED_view3d_polygon_offset(rv3d, 0.0);
	}
	dm->release(dm);
}

/* returns true if nothing was drawn, for detecting to draw an object center */
static bool draw_mesh_object(Scene *scene, SceneLayer *sl, ARegion *ar, View3D *v3d, RegionView3D *rv3d, BaseLegacy *base,
                             const char dt, const unsigned char ob_wire_col[4], const short dflag)
{
	Object *ob = base->object;
	Object *obedit = scene->obedit;
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	bool do_alpha_after = false, drawlinked = false, retval = false;

	/* If we are drawing shadows and any of the materials don't cast a shadow,
	 * then don't draw the object */
	if (v3d->flag2 & V3D_RENDER_SHADOW) {
		for (int i = 0; i < ob->totcol; ++i) {
			Material *ma = give_current_material(ob, i);
			if (ma && !(ma->mode2 & MA_CASTSHADOW)) {
				return true;
			}
		}
	}
	
	if (obedit && ob != obedit && ob->data == obedit->data) {
		if (BKE_key_from_object(ob) || BKE_key_from_object(obedit)) {}
		else if (ob->modifiers.first || obedit->modifiers.first) {}
		else drawlinked = true;
	}

	/* backface culling */
	if (v3d->flag2 & V3D_BACKFACE_CULLING) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}

	if (ob == obedit || drawlinked) {
		DerivedMesh *finalDM, *cageDM;
		
		if (obedit != ob) {
			finalDM = cageDM = editbmesh_get_derived_base(
			        ob, em, scene->customdata_mask);
		}
		else {
			cageDM = editbmesh_get_derived_cage_and_final(
			        scene, ob, em, scene->customdata_mask,
			        &finalDM);
		}

		const bool use_material = ((me->drawflag & ME_DRAWEIGHT) == 0);

		DM_update_materials(finalDM, ob);
		if (cageDM != finalDM) {
			DM_update_materials(cageDM, ob);
		}

		if (use_material) {
			if (dt > OB_WIRE) {
				const bool glsl = draw_glsl_material(scene, sl, ob, v3d, dt);

				GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, glsl, NULL);
			}
		}

		draw_em_fancy(scene, sl, ar, v3d, ob, em, cageDM, finalDM, dt);

		if (use_material) {
			GPU_end_object_materials();
		}

		if (obedit != ob)
			finalDM->release(finalDM);
	}
	else {
		/* ob->bb was set by derived mesh system, do NULL check just to be sure */
		if (me->totpoly <= 4 || (!ob->bb || ED_view3d_boundbox_clip(rv3d, ob->bb))) {
			if (dt > OB_WIRE) {
				const bool glsl = draw_glsl_material(scene, sl, ob, v3d, dt);

				if (dt == OB_SOLID || glsl) {
					const bool check_alpha = check_alpha_pass(base);
					GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, glsl,
					                           (check_alpha) ? &do_alpha_after : NULL);
				}
			}

			draw_mesh_fancy(scene, sl, ar, v3d, rv3d, base, dt, ob_wire_col, dflag);

			GPU_end_object_materials();
			
			if (me->totvert == 0) retval = true;
		}
	}
	
	if ((dflag & DRAW_PICKING) == 0 && (base->flag_legacy & OB_FROMDUPLI) == 0 && (v3d->flag2 & V3D_RENDER_SHADOW) == 0) {
		/* GPU_begin_object_materials checked if this is needed */
		if (do_alpha_after) {
			if (ob->dtx & OB_DRAWXRAY) {
				ED_view3d_after_add(&v3d->afterdraw_xraytransp, base, dflag);
			}
			else {
				ED_view3d_after_add(&v3d->afterdraw_transp, base, dflag);
			}
		}
		else if (ob->dtx & OB_DRAWXRAY && ob->dtx & OB_DRAWTRANSP) {
			/* special case xray+transp when alpha is 1.0, without this the object vanishes */
			if (v3d->xray == 0 && v3d->transp == 0) {
				ED_view3d_after_add(&v3d->afterdraw_xray, base, dflag);
			}
		}
	}

	if (v3d->flag2 & V3D_BACKFACE_CULLING)
		glDisable(GL_CULL_FACE);
	
	return retval;
}

static void make_color_variations(const unsigned char base_ubyte[4], float low[4], float med[4], float high[4], const bool other_obedit)
{
	/* original idea: nice variations (lighter & darker shades) of base color
	 * current implementation uses input color as high; med & low get closer to background color
	 */

	float bg[3];
	UI_GetThemeColor3fv(TH_BACK, bg);

	float base[4];
	rgba_uchar_to_float(base, base_ubyte);

	if (other_obedit) {
		/* this object should fade away so user can focus on the object being edited */
		interp_v3_v3v3(low, bg, base, 0.1f);
		interp_v3_v3v3(med, bg, base, 0.2f);
		interp_v3_v3v3(high, bg, base, 0.25f);
	}
	else {
		interp_v3_v3v3(low, bg, base, 0.333f);
		interp_v3_v3v3(med, bg, base, 0.667f);
		copy_v3_v3(high, base);
	}

	/* use original alpha */
	low[3] = base[3];
	med[3] = base[3];
	high[3] = base[3];
}

static void draw_mesh_fancy_new(Scene *scene, SceneLayer *sl, ARegion *ar, View3D *v3d, RegionView3D *rv3d, Base *base,
                                const char dt, const unsigned char ob_wire_col[4], const short dflag, const bool other_obedit)
{
	if (dflag & (DRAW_PICKING | DRAW_CONSTCOLOR)) {
		/* too complicated! use existing methods */
		/* TODO: move this into a separate depth pre-pass */
		draw_mesh_fancy(scene, sl, ar, v3d, rv3d, base, dt, ob_wire_col, dflag);
		return;
	}

#ifdef WITH_GAMEENGINE
	Object *ob = (rv3d->rflag & RV3D_IS_GAME_ENGINE) ? BKE_object_lod_meshob_get(base->object, sl) : base->object;
#else
	Object *ob = base->object;
#endif
	Mesh *me = ob->data;
	eWireDrawMode draw_wire = OBDRAW_WIRE_OFF; /* could be bool draw_wire_overlay */
	bool no_edges, no_faces;
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, scene->customdata_mask);
	const bool is_obact = (ob == OBACT_NEW);
	int draw_flags = (is_obact && BKE_paint_select_face_test(ob)) ? DRAW_FACE_SELECT : 0;

	if (!dm)
		return;

	const bool solid = dt >= OB_SOLID;
	if (solid) {
		DM_update_materials(dm, ob);
	}

	/* Check to draw dynamic paint colors (or weights from WeightVG modifiers).
	 * Note: Last "preview-active" modifier in stack will win! */
	if (DM_get_loop_data_layer(dm, CD_PREVIEW_MLOOPCOL) && modifiers_isPreview(ob))
		draw_flags |= DRAW_MODIFIERS_PREVIEW;

	/* Unwanted combination */
	if (draw_flags & DRAW_FACE_SELECT) {
		draw_wire = OBDRAW_WIRE_OFF;
	}
	else if (ob->dtx & OB_DRAWWIRE) {
		draw_wire = OBDRAW_WIRE_ON;
	}

	/* check polys instead of tessfaces because of dyntopo where tessfaces don't exist */
	if (dm->type == DM_TYPE_CCGDM) {
		no_edges = !subsurf_has_edges(dm);
		no_faces = !subsurf_has_faces(dm);
	}
	else {
		no_edges = (dm->getNumEdges(dm) == 0);
		no_faces = (dm->getNumPolys(dm) == 0);
	}

	if (solid) {
		/* vertexpaint, faceselect wants this, but it doesnt work for shaded? */
		glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);
	}

	if (dt == OB_BOUNDBOX) {
		if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_WIRE) == 0)
			draw_bounding_volume(ob, ob->boundtype, ob_wire_col);
	}
	else if ((no_faces && no_edges) ||
	         ((!is_obact || (ob->mode == OB_MODE_OBJECT)) && object_is_halo(scene, ob)))
	{
		glPointSize(1.5f);
		// dm->drawVerts(dm);
		// TODO: draw smooth round points as a batch
	}
	else if ((dt == OB_WIRE) || no_faces) {
		draw_wire = OBDRAW_WIRE_ON;

		/* enable depth for wireframes */
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		glLineWidth(1.0f);

#if 1 /* fancy wireframes */

		Batch *fancy_edges = BKE_mesh_batch_cache_get_fancy_edges(me);

		if (rv3d->persp == RV3D_ORTHO) {
			Batch_set_builtin_program(fancy_edges, GPU_SHADER_EDGES_FRONT_BACK_ORTHO);
			/* set eye vector, transformed to object coords */
			float eye[3] = { 0.0f, 0.0f, 1.0f }; /* looking into the screen */
			mul_m3_v3((float (*)[3])gpuGetNormalMatrixInverse(NULL), eye);
			Batch_Uniform3fv(fancy_edges, "eye", eye);
		}
		else {
			Batch_set_builtin_program(fancy_edges, GPU_SHADER_EDGES_FRONT_BACK_PERSP);
		}

		float frontColor[4];
		float backColor[4];
		float outlineColor[4];
		make_color_variations(ob_wire_col, backColor, frontColor, outlineColor, other_obedit);

		Batch_Uniform4fv(fancy_edges, "frontColor", frontColor);
		Batch_Uniform4fv(fancy_edges, "backColor", backColor);
		Batch_Uniform1b(fancy_edges, "drawFront", true);
		Batch_Uniform1b(fancy_edges, "drawBack", true); /* false here = backface cull */
		Batch_Uniform1b(fancy_edges, "drawSilhouette", false);

		Batch_draw(fancy_edges);

		/* extra oomph for the silhouette contours */
		glLineWidth(2.0f);
		Batch_use_program(fancy_edges); /* hack to make the following uniforms stick */
		Batch_Uniform1b(fancy_edges, "drawFront", false);
		Batch_Uniform1b(fancy_edges, "drawBack", false);
		Batch_Uniform1b(fancy_edges, "drawSilhouette", true);
		Batch_Uniform4fv(fancy_edges, "silhouetteColor", outlineColor);

		Batch_draw(fancy_edges);

#else /* simple wireframes */

		Batch *batch = MBC_get_all_edges(dm);
		Batch_set_builtin_program(batch, GPU_SHADER_3D_UNIFORM_COLOR);

		float color[4];
		rgba_uchar_to_float(color, ob_wire_col);

		Batch_Uniform4fv(batch, "color", color);

		Batch_draw(batch);
#endif
	}
	else if (((is_obact && ob->mode & OB_MODE_TEXTURE_PAINT)) ||
	         check_object_draw_texture(scene, v3d, dt))
	{
		bool draw_loose = true;

		if ((v3d->flag & V3D_SELECT_OUTLINE) &&
		    ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
		    (base->flag & BASE_SELECTED) &&
		    !(G.f & G_PICKSEL || (draw_flags & DRAW_FACE_SELECT)) &&
		    (draw_wire == OBDRAW_WIRE_OFF))
		{
			draw_mesh_object_outline_new(v3d, rv3d, ob, me, (ob == OBACT_NEW));
		}

		if (draw_glsl_material(scene, sl, ob, v3d, dt) && !(draw_flags & DRAW_MODIFIERS_PREVIEW)) {
			Paint *p;

			glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);

			if ((v3d->flag2 & V3D_SHOW_SOLID_MATCAP) && ob->sculpt && (p = BKE_paint_get_active(scene, sl))) {
				GPUVertexAttribs gattribs;
				float planes[4][4];
				float (*fpl)[4] = NULL;
				const bool fast = (p->flags & PAINT_FAST_NAVIGATE) && (rv3d->rflag & RV3D_NAVIGATING);

				if (ob->sculpt->partial_redraw) {
					if (ar->do_draw & RGN_DRAW_PARTIAL) {
						ED_sculpt_redraw_planes_get(planes, ar, ob);
						fpl = planes;
						ob->sculpt->partial_redraw = 0;
					}
				}

				GPU_object_material_bind(1, &gattribs);
				dm->drawFacesSolid(dm, fpl, fast, NULL);
				draw_loose = false;
			}
			else
				dm->drawFacesGLSL(dm, GPU_object_material_bind);

			GPU_object_material_unbind();

			glFrontFace(GL_CCW);

			if (draw_flags & DRAW_FACE_SELECT)
				draw_mesh_face_select(rv3d, me, dm, false);
		}
		else {
			draw_mesh_textured(scene, sl, v3d, rv3d, ob, dm, draw_flags);
		}

		if (draw_loose && !(draw_flags & DRAW_FACE_SELECT)) {
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					glColor3ubv(ob_wire_col);
				}
				glLineWidth(1.0f);
				dm->drawLooseEdges(dm);
			}
		}
	}
	else if (dt == OB_SOLID) {
		if (draw_flags & DRAW_MODIFIERS_PREVIEW) {
			/* for object selection draws no shade */
			if (dflag & (DRAW_PICKING | DRAW_CONSTCOLOR)) {
				/* TODO: draw basic faces with GPU_SHADER_3D_DEPTH_ONLY */
			}
			else {
				const float specular[3] = {0.47f, 0.47f, 0.47f};

				/* draw outline */
				/* TODO: move this into a separate pass */
				if ((v3d->flag & V3D_SELECT_OUTLINE) &&
				    ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
				    (base->flag & BASE_SELECTED) &&
				    (draw_wire == OBDRAW_WIRE_OFF) &&
				    (ob->sculpt == NULL))
				{
					draw_mesh_object_outline_new(v3d, rv3d, ob, me, (ob == OBACT_NEW));
				}

				/* materials arent compatible with vertex colors */
				GPU_end_object_materials();

				/* set default specular */
				GPU_basic_shader_colors(NULL, specular, 35, 1.0f);
				GPU_basic_shader_bind(GPU_SHADER_LIGHTING | GPU_SHADER_USE_COLOR);

				dm->drawMappedFaces(dm, NULL, NULL, NULL, NULL, DM_DRAW_USE_COLORS | DM_DRAW_NEED_NORMALS);

				GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
			}
		}
		else {
			Paint *p;

			if ((v3d->flag & V3D_SELECT_OUTLINE) &&
			    ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
			    (base->flag & BASE_SELECTED) &&
			    (draw_wire == OBDRAW_WIRE_OFF) &&
			    (ob->sculpt == NULL))
			{
				/* TODO: move this into a separate pass */
				draw_mesh_object_outline_new(v3d, rv3d, ob, me, (ob == OBACT_NEW));
			}

			glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);

			if (ob->sculpt && (p = BKE_paint_get_active(scene, sl))) {
				float planes[4][4];
				float (*fpl)[4] = NULL;
				const bool fast = (p->flags & PAINT_FAST_NAVIGATE) && (rv3d->rflag & RV3D_NAVIGATING);

				if (ob->sculpt->partial_redraw) {
					if (ar->do_draw & RGN_DRAW_PARTIAL) {
						ED_sculpt_redraw_planes_get(planes, ar, ob);
						fpl = planes;
						ob->sculpt->partial_redraw = 0;
					}
				}

				dm->drawFacesSolid(dm, fpl, fast, GPU_object_material_bind);
			}
			else
				dm->drawFacesSolid(dm, NULL, 0, GPU_object_material_bind);

			glFrontFace(GL_CCW);

			GPU_object_material_unbind();

			if (!ob->sculpt && (v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					glColor3ubv(ob_wire_col);
				}
				glLineWidth(1.0f);
				dm->drawLooseEdges(dm);
			}
		}
	}
	else if (dt == OB_PAINT) {
		draw_mesh_paint(v3d, rv3d, ob, dm, draw_flags);

		/* since we already draw wire as wp guide, don't draw over the top */
		draw_wire = OBDRAW_WIRE_OFF;
	}

	if ((draw_wire != OBDRAW_WIRE_OFF) &&  /* draw extra wire */
	    /* when overriding with render only, don't bother */
	    (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_SOLID) == 0)) // <-- is this "== 0" in the right spot???
	{
		/* When using wireframe object draw in particle edit mode
		 * the mesh gets in the way of seeing the particles, fade the wire color
		 * with the background. */

		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			/* TODO:
			 * Batch_UniformColor4ubv(ob_wire_col);
			 */
		}

		/* If drawing wire and drawtype is not OB_WIRE then we are
		 * overlaying the wires.
		 *
		 * No need for polygon offset because new technique is AWESOME.
		 */
#if 0
		glLineWidth(1.0f);
		dm->drawEdges(dm, ((dt == OB_WIRE) || no_faces), (ob->dtx & OB_DRAW_ALL_EDGES) != 0);
#else
		/* something */
#endif
	}
	
#if 0 // (merwin) what is this for?
	if (is_obact && BKE_paint_select_vert_test(ob)) {
		const bool use_depth = (v3d->flag & V3D_ZBUF_SELECT) != 0;
		glColor3f(0.0f, 0.0f, 0.0f);
		glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

		if (!use_depth) glDisable(GL_DEPTH_TEST);
		else            ED_view3d_polygon_offset(rv3d, 1.0);
		drawSelectedVertices(dm, ob->data);
		if (!use_depth) glEnable(GL_DEPTH_TEST);
		else            ED_view3d_polygon_offset(rv3d, 0.0);
	}
#endif

	dm->release(dm);
}

static bool draw_mesh_object_new(Scene *scene, SceneLayer *sl, ARegion *ar, View3D *v3d, RegionView3D *rv3d, BaseLegacy *base,
                                 const char dt, const unsigned char ob_wire_col[4], const short dflag)
{
	Object *ob = base->object;
	Object *obedit = scene->obedit;
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	bool do_alpha_after = false, drawlinked = false, retval = false;

	if (v3d->flag2 & V3D_RENDER_SHADOW) {
		/* TODO: handle shadow pass separately */
		return true;
	}
	
	if (obedit && ob != obedit && ob->data == obedit->data) {
		if (BKE_key_from_object(ob) || BKE_key_from_object(obedit)) {}
		else if (ob->modifiers.first || obedit->modifiers.first) {}
		else drawlinked = true;
	}

	/* backface culling */
	const bool solid = dt > OB_WIRE;
	const bool cullBackface = solid && (v3d->flag2 & V3D_BACKFACE_CULLING);
	if (cullBackface) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}

	if (ob == obedit || drawlinked) {
		DerivedMesh *finalDM, *cageDM;

		if (obedit != ob) {
			/* linked to the edit object */
			finalDM = cageDM = editbmesh_get_derived_base(
			        ob, em, scene->customdata_mask);
		}
		else {
			cageDM = editbmesh_get_derived_cage_and_final(
			        scene, ob, em, scene->customdata_mask,
			        &finalDM);
		}

		const bool use_material = solid && ((me->drawflag & ME_DRAWEIGHT) == 0);

#if 0 // why update if not being used?
		DM_update_materials(finalDM, ob);
		if (cageDM != finalDM) {
			DM_update_materials(cageDM, ob);
		}
#endif // moved to below

		if (use_material) {
			DM_update_materials(finalDM, ob);
			if (cageDM != finalDM) {
				DM_update_materials(cageDM, ob);
			}

			const bool glsl = draw_glsl_material(scene, sl, ob, v3d, dt);

			GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, glsl, NULL);
		}

		draw_em_fancy_new(scene, ar, v3d, ob, me, em, cageDM, finalDM, dt);

		if (use_material) {
			GPU_end_object_materials();
		}

		if (obedit != ob)
			finalDM->release(finalDM);
	}
	else {
		/* ob->bb was set by derived mesh system, do NULL check just to be sure */
		if (me->totpoly <= 4 || (!ob->bb || ED_view3d_boundbox_clip(rv3d, ob->bb))) {
			if (solid) {
				const bool glsl = draw_glsl_material(scene, sl, ob, v3d, dt);

				if (dt == OB_SOLID || glsl) {
					const bool check_alpha = check_alpha_pass(base);
					GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, glsl,
					                           (check_alpha) ? &do_alpha_after : NULL);
				}
			}

			const bool other_obedit = obedit && (obedit != ob);

			draw_mesh_fancy_new(scene, sl, ar, v3d, rv3d, base, dt, ob_wire_col, dflag, other_obedit);

			GPU_end_object_materials();

			if (me->totvert == 0) retval = true;
		}
	}

	if (cullBackface)
		glDisable(GL_CULL_FACE);

	return retval;
}

/* ************** DRAW DISPLIST ****************** */

static void drawDispListVerts(int dt, const void *data, unsigned int vert_ct, const unsigned char wire_col[3])
{
	VertexFormat format = {0};
	unsigned int pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);

	VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
	VertexBuffer_allocate_data(vbo, vert_ct);

	fillAttrib(vbo, pos_id, data);

	Batch *batch = Batch_create(dt, vbo, NULL);
	Batch_set_builtin_program(batch, GPU_SHADER_3D_UNIFORM_COLOR);
	if (wire_col) {
		Batch_Uniform4f(batch, "color", wire_col[0]/255.0f, wire_col[1]/255.0f, wire_col[2]/255.0f, 1.0f);
	}
	Batch_draw(batch);
	Batch_discard_all(batch);
}

/* convert dispList with elem indices to batch, only support triangles and quads
 * XXX : This is a huge perf issue. We should cache the resulting batches inside the object instead.
 *       But new viewport will do it anyway
 * TODO implement flat drawing */
static void drawDispListElem(bool quads, bool UNUSED(smooth), const float *data, const float *ndata, unsigned int vert_ct,
                             const int *elem, unsigned int elem_ct, const unsigned char wire_col[3])
{
	VertexFormat format = {0};
	int i;
	const int *idx = elem;
	unsigned int pos_id, nor_id;

	pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	if (ndata) {
		nor_id = add_attrib(&format, "nor", GL_FLOAT, 3, KEEP_FLOAT);
	}

	ElementListBuilder elb;
	ElementListBuilder_init(&elb, GL_TRIANGLES, (quads) ? elem_ct * 2 : elem_ct, 0xffffffff);

	if (quads) {
		for (i = elem_ct; i; --i, idx += 4) {
			add_triangle_vertices(&elb, idx[0], idx[1], idx[2]);
			add_triangle_vertices(&elb, idx[0], idx[2], idx[3]);
		}
	}
	else {
		for (i = elem_ct; i; --i, idx += 3) {
			add_triangle_vertices(&elb, idx[0], idx[1], idx[2]);
		}
	}

	VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
	VertexBuffer_allocate_data(vbo, vert_ct);

	fillAttrib(vbo, pos_id, data);

	if (ndata) {
		fillAttrib(vbo, nor_id, ndata);
	}

	Batch *batch = Batch_create(GL_TRIANGLES, vbo, ElementList_build(&elb));
	Batch_set_builtin_program(batch, GPU_SHADER_SIMPLE_LIGHTING);
	if (wire_col) {
		Batch_Uniform4f(batch, "color", wire_col[0]/255.0f, wire_col[1]/255.0f, wire_col[2]/255.0f, 1.0f);
	}
	Batch_Uniform4f(batch, "color", 0.8f, 0.8f, 0.8f, 1.0f);
	Batch_Uniform3f(batch, "light", 0.0f, 0.0f, 1.0f);
	Batch_draw(batch);
	Batch_discard_all(batch);
}

/**
 * \param dl_type_mask Only draw types matching this mask.
 * \return true when nothing was drawn
 */
static bool drawDispListwire_ex(ListBase *dlbase, unsigned int dl_type_mask, const unsigned char wire_col[3])
{
	if (dlbase == NULL) return true;
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (DispList *dl = dlbase->first; dl; dl = dl->next) {
		if (dl->parts == 0 || dl->nr == 0) {
			continue;
		}

		if ((dl_type_mask & (1 << dl->type)) == 0) {
			continue;
		}

		const float *data = dl->verts;
		int parts;

		switch (dl->type) {
			case DL_SEGM:
				for (parts = 0; parts < dl->parts; parts++)
					drawDispListVerts(GL_LINE_STRIP, data + (parts * dl->nr * 3), dl->nr, wire_col);
				break;

			case DL_POLY:
				for (parts = 0; parts < dl->parts; parts++)
					drawDispListVerts(GL_LINE_LOOP, data + (parts * dl->nr * 3), dl->nr, wire_col);
				break;

			case DL_SURF:
				for (parts = 0; parts < dl->parts; parts++) {
					if (dl->flag & DL_CYCL_U)
						drawDispListVerts(GL_LINE_LOOP, data + (parts * dl->nr * 3), dl->nr, wire_col);
					else
						drawDispListVerts(GL_LINE_STRIP, data + (parts * dl->nr * 3), dl->nr, wire_col);
				}

				float *data_aligned = MEM_mallocN(sizeof(float) * 3 * dl->parts, "aligned data");
				for (int nr = 0; nr < dl->nr; nr++) {
					int ofs = 3 * dl->nr;
					int idx = 0;

					data = (dl->verts) + 3 * nr;
					parts = dl->parts;

					while (parts--) {
						copy_v3_v3(data_aligned + idx, data);
						data += ofs;
						idx += 3;
					}

					if (dl->flag & DL_CYCL_V)
						drawDispListVerts(GL_LINE_LOOP, data_aligned, dl->parts, wire_col);
					else
						drawDispListVerts(GL_LINE_STRIP, data_aligned, dl->parts, wire_col);
				}

				if (data_aligned)
					MEM_freeN(data_aligned);

				break;

			case DL_INDEX3:
				drawDispListElem(false, true, dl->verts, NULL, dl->nr, dl->index, dl->parts, wire_col);
				break;

			case DL_INDEX4:
				drawDispListElem(true, true, dl->verts, NULL, dl->nr, dl->index, dl->parts, wire_col);
				break;
		}
	}
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	return false;
}

static bool drawDispListwire(ListBase *dlbase, const short ob_type, const unsigned char wire_col[3])
{
	unsigned int dl_mask = 0xffffffff;

	/* skip fill-faces for curves & fonts */
	if (ELEM(ob_type, OB_FONT, OB_CURVE)) {
		dl_mask &= ~((1 << DL_INDEX3) | (1 << DL_INDEX4));
	}

	return drawDispListwire_ex(dlbase, dl_mask, wire_col);
}

static bool index3_nors_incr = true;

static void drawDispListsolid(ListBase *lb, Object *ob, const short UNUSED(dflag),
                              const unsigned char ob_wire_col[4], const bool use_glsl)
{
	GPUVertexAttribs gattribs;
	
	if (lb == NULL) return;

	/* track current material, -1 for none (needed for lines) */
	short col = -1;
	
	DispList *dl = lb->first;
	while (dl) {
		const float *data = dl->verts;
		//const float *ndata = dl->nors;

		switch (dl->type) {
			case DL_SEGM:
				if (ob->type == OB_SURF) {
					if (col != -1) {
						GPU_object_material_unbind();
						col = -1;
					}

					drawDispListVerts(GL_LINE_STRIP, data, dl->nr, ob_wire_col);
				}
				break;
			case DL_POLY:
				if (ob->type == OB_SURF) {
					if (col != -1) {
						GPU_object_material_unbind();
						col = -1;
					}

					drawDispListVerts(GL_LINE_LOOP, data, dl->nr, ob_wire_col);
				}
				break;
			case DL_SURF:

				if (dl->index) {
					if (col != dl->col) {
						GPU_object_material_bind(dl->col + 1, use_glsl ? &gattribs : NULL);
						col = dl->col;
					}

					drawDispListElem(true, (dl->rt & CU_SMOOTH), dl->verts, dl->nors, dl->nr * dl->parts, dl->index, dl->totindex, ob_wire_col);
				}
				break;

			case DL_INDEX3:
				if (col != dl->col) {
					GPU_object_material_bind(dl->col + 1, use_glsl ? &gattribs : NULL);
					col = dl->col;
				}

#if 0
				/* for polys only one normal needed */
				if (index3_nors_incr) {
					glEnableClientState(GL_NORMAL_ARRAY);
					glNormalPointer(GL_FLOAT, 0, dl->nors);
				}
				else
					glNormal3fv(ndata);
#endif
				drawDispListElem(false, (dl->rt & CU_SMOOTH), dl->verts, dl->nors, dl->nr, dl->index, dl->parts, ob_wire_col);

#if 0
				if (index3_nors_incr)
					glDisableClientState(GL_NORMAL_ARRAY);
#endif

				break;

			case DL_INDEX4:
				if (col != dl->col) {
					GPU_object_material_bind(dl->col + 1, use_glsl ? &gattribs : NULL);
					col = dl->col;
				}

				drawDispListElem(true, true, dl->verts, dl->nors, dl->nr, dl->index, dl->parts, ob_wire_col);

				break;
		}
		dl = dl->next;
	}

	glFrontFace(GL_CCW);

	if (col != -1) {
		GPU_object_material_unbind();
	}
}

static void drawCurveDMWired(Object *ob)
{
	DerivedMesh *dm = ob->derivedFinal;
	dm->drawEdges(dm, 1, 0);
}

/* return true when nothing was drawn */
static bool drawCurveDerivedMesh(Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d, BaseLegacy *base, const char dt)
{
	Object *ob = base->object;
	DerivedMesh *dm = ob->derivedFinal;

	if (!dm) {
		return true;
	}

	DM_update_materials(dm, ob);

	glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);

	if (dt > OB_WIRE && dm->getNumPolys(dm)) {
		bool glsl = draw_glsl_material(scene, sl, ob, v3d, dt);
		GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, glsl, NULL);

		if (!glsl)
			dm->drawFacesSolid(dm, NULL, 0, GPU_object_material_bind);
		else
			dm->drawFacesGLSL(dm, GPU_object_material_bind);

		GPU_end_object_materials();
	}
	else {
		if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_SOLID) == 0)
			drawCurveDMWired(ob);
	}

	return false;
}

/**
 * Only called by #drawDispList
 * \return true when nothing was drawn
 */
static bool drawDispList_nobackface(Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d, Base *base,
                                    const char dt, const short dflag, const unsigned char ob_wire_col[4])
{
	Object *ob = base->object;
	ListBase *lb = NULL;
	DispList *dl;
	Curve *cu;
	const bool render_only = (v3d->flag2 & V3D_RENDER_OVERRIDE) != 0;
	const bool solid = (dt > OB_WIRE);

	switch (ob->type) {
		case OB_FONT:
		case OB_CURVE:
			cu = ob->data;

			lb = &ob->curve_cache->disp;

			if (solid) {
				const bool has_faces = BKE_displist_has_faces(lb);
				dl = lb->first;
				if (dl == NULL) {
					return true;
				}

				if (dl->nors == NULL) BKE_displist_normals_add(lb);
				index3_nors_incr = false;

				if (!render_only) {
					/* when we have faces, only draw loose-wire */
					if (has_faces) {
						drawDispListwire_ex(lb, (1 << DL_SEGM), ob_wire_col);
					}
					else {
						drawDispListwire(lb, ob->type, ob_wire_col);
					}
				}

				if (has_faces == false) {
					/* pass */
				}
				else {
					if (draw_glsl_material(scene, sl, ob, v3d, dt)) {
						GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, 1, NULL);
						drawDispListsolid(lb, ob, dflag, ob_wire_col, true);
						GPU_end_object_materials();
					}
					else {
						GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, 0, NULL);
						drawDispListsolid(lb, ob, dflag, ob_wire_col, false);
						GPU_end_object_materials();
					}
					if (cu->editnurb && cu->bevobj == NULL && cu->taperobj == NULL && cu->ext1 == 0.0f && cu->ext2 == 0.0f) {
						unsigned char col[4] = {0, 0, 0, 0};
						drawDispListwire(lb, ob->type, col);
					}
				}
				index3_nors_incr = true;
			}
			else {
				if (!render_only || BKE_displist_has_faces(lb)) {
					return drawDispListwire(lb, ob->type, ob_wire_col);
				}
			}
			break;
		case OB_SURF:

			lb = &ob->curve_cache->disp;

			if (solid) {
				dl = lb->first;
				if (dl == NULL) {
					return true;
				}

				if (dl->nors == NULL) BKE_displist_normals_add(lb);

				if (draw_glsl_material(scene, sl, ob, v3d, dt)) {
					GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, 1, NULL);
					drawDispListsolid(lb, ob, dflag, ob_wire_col, true);
					GPU_end_object_materials();
				}
				else {
					GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, 0, NULL);
					drawDispListsolid(lb, ob, dflag, ob_wire_col, false);
					GPU_end_object_materials();
				}
			}
			else {
				return drawDispListwire(lb, ob->type, ob_wire_col);
			}
			break;
		case OB_MBALL:

			if (BKE_mball_is_basis(ob)) {
				lb = &ob->curve_cache->disp;
				if (BLI_listbase_is_empty(lb)) {
					return true;
				}

				if (solid) {

					if (draw_glsl_material(scene, sl, ob, v3d, dt)) {
						GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, 1, NULL);
						drawDispListsolid(lb, ob, dflag, ob_wire_col, true);
						GPU_end_object_materials();
					}
					else {
						GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, 0, NULL);
						drawDispListsolid(lb, ob, dflag, ob_wire_col, false);
						GPU_end_object_materials();
					}
				}
				else {
					return drawDispListwire(lb, ob->type, ob_wire_col);
				}
			}
			break;
	}

	return false;
}
static bool drawDispList(Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d, Base *base,
                         const char dt, const short dflag, const unsigned char ob_wire_col[4])
{
	bool retval;

	/* backface culling */
	if (v3d->flag2 & V3D_BACKFACE_CULLING) {
		/* not all displists use same in/out normal direction convention */
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}

#ifdef SEQUENCER_DAG_WORKAROUND
	ensure_curve_cache(scene, base->object);
#endif

	if (drawCurveDerivedMesh(scene, sl, v3d, rv3d, base, dt) == false) {
		retval = false;
	}
	else {
		Object *ob = base->object;
		GLenum mode;

		if (ob->type == OB_MBALL) {
			mode = (ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW;
		}
		else {
			mode = (ob->transflag & OB_NEG_SCALE) ? GL_CCW : GL_CW;
		}

		glFrontFace(mode);

		retval = drawDispList_nobackface(scene, sl, v3d, rv3d, base, dt, dflag, ob_wire_col);

		if (mode != GL_CCW) {
			glFrontFace(GL_CCW);
		}
	}

	if (v3d->flag2 & V3D_BACKFACE_CULLING) {
		glDisable(GL_CULL_FACE);
	}

	return retval;
}

/* *********** drawing for particles ************* */
/* stride :   offset size in bytes
 * col[4] :   the color to use when *color is NULL, can be also NULL */
static void draw_vertex_array(int dt, const float *vert, const float *nor, const float *color, int stride, int vert_ct, float col[4])
{
	VertexFormat format = {0};
	unsigned int pos_id, nor_id, col_id;
	pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	if (nor) nor_id = add_attrib(&format, "nor", GL_FLOAT, 3, KEEP_FLOAT);
	if (color) col_id = add_attrib(&format, "color", GL_FLOAT, 3, KEEP_FLOAT);

	VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
	VertexBuffer_allocate_data(vbo, vert_ct);

	if (stride == 0) {
		fillAttrib(vbo, pos_id, vert);
		if (nor) fillAttrib(vbo, nor_id, nor);
		if (color) fillAttrib(vbo, col_id, color);
	}
	else {
		fillAttribStride(vbo, pos_id, stride, vert);
		if (nor) fillAttribStride(vbo, nor_id, stride, nor);
		if (color) fillAttribStride(vbo, col_id, stride, color);
	}

	Batch *batch = Batch_create(dt, vbo, NULL);
	if (nor && color) {
		Batch_set_builtin_program(batch, GPU_SHADER_SIMPLE_LIGHTING_SMOOTH_COLOR);
		Batch_Uniform3f(batch, "light", 0.0f, 0.0f, 1.0f);
	}
	else if (nor) {
		Batch_set_builtin_program(batch, GPU_SHADER_SIMPLE_LIGHTING);
		Batch_Uniform3f(batch, "light", 0.0f, 0.0f, 1.0f);
		if (col) Batch_Uniform4fv(batch, "color", col);
	}
	else if (color) {
		Batch_set_builtin_program(batch, GPU_SHADER_3D_SMOOTH_COLOR);
	}
	else {
		Batch_set_builtin_program(batch, GPU_SHADER_3D_UNIFORM_COLOR);
		if (col) Batch_Uniform4fv(batch, "color", col);
	}
	Batch_draw(batch);
	Batch_discard_all(batch);
}
static void draw_particle_arrays_new(int draw_as, int ob_dt, int select,
                                     const float *vert, const float *nor, const float *color,
                                     int totpoint, float col[4])
{
	/* draw created data arrays */
	switch (draw_as) {
		case PART_DRAW_AXIS:
		case PART_DRAW_CROSS:
			draw_vertex_array(GL_LINES, vert, nor, color, 0, 6 * totpoint, col);
			break;
		case PART_DRAW_LINE:
			draw_vertex_array(GL_LINES, vert, nor, color, 0, 2 * totpoint, col);
			break;
		case PART_DRAW_BB:
			if (ob_dt <= OB_WIRE || select)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			else
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			draw_vertex_array(GL_QUADS, vert, nor, color, 0, 4 * totpoint, col);
			break;
		default:
			draw_vertex_array(GL_POINTS, vert, nor, color, 0, totpoint, col);
			break;
	}
}
static void draw_particle(ParticleKey *state, int draw_as, short draw, float pixsize,
                          float imat[4][4], const float draw_line[2], ParticleBillboardData *bb, ParticleDrawData *pdd,
                          unsigned int pos)
{
	float vec[3], vec2[3];
	float *vd = NULL;
	float *cd = NULL;
	float ma_col[3] = {0.0f, 0.0f, 0.0f};

	/* null only for PART_DRAW_CIRC */
	if (pdd) {
		vd = pdd->vd;
		cd = pdd->cd;

		if (pdd->ma_col) {
			copy_v3_v3(ma_col, pdd->ma_col);
		}
	}

	switch (draw_as) {
		case PART_DRAW_DOT:
		{
			if (vd) {
				copy_v3_v3(vd, state->co); pdd->vd += 3;
			}
			if (cd) {
				copy_v3_v3(cd, pdd->ma_col);
				pdd->cd += 3;
			}
			break;
		}
		case PART_DRAW_CROSS:
		case PART_DRAW_AXIS:
		{
			vec[0] = 2.0f * pixsize;
			vec[1] = vec[2] = 0.0;
			mul_qt_v3(state->rot, vec);
			if (draw_as == PART_DRAW_AXIS) {
				if (cd) {
					cd[1] = cd[2] = cd[4] = cd[5] = 0.0;
					cd[0] = cd[3] = 1.0;
					cd[6] = cd[8] = cd[9] = cd[11] = 0.0;
					cd[7] = cd[10] = 1.0;
					cd[13] = cd[12] = cd[15] = cd[16] = 0.0;
					cd[14] = cd[17] = 1.0;
					pdd->cd += 18;
				}

				copy_v3_v3(vec2, state->co);
			}
			else {
				if (cd) {
					cd[0] = cd[3] = cd[6] = cd[9] = cd[12] = cd[15] = ma_col[0];
					cd[1] = cd[4] = cd[7] = cd[10] = cd[13] = cd[16] = ma_col[1];
					cd[2] = cd[5] = cd[8] = cd[11] = cd[14] = cd[17] = ma_col[2];
					pdd->cd += 18;
				}
				sub_v3_v3v3(vec2, state->co, vec);
			}

			add_v3_v3(vec, state->co);
			copy_v3_v3(pdd->vd, vec); pdd->vd += 3;
			copy_v3_v3(pdd->vd, vec2); pdd->vd += 3;

			vec[1] = 2.0f * pixsize;
			vec[0] = vec[2] = 0.0;
			mul_qt_v3(state->rot, vec);
			if (draw_as == PART_DRAW_AXIS) {
				copy_v3_v3(vec2, state->co);
			}
			else {
				sub_v3_v3v3(vec2, state->co, vec);
			}

			add_v3_v3(vec, state->co);
			copy_v3_v3(pdd->vd, vec); pdd->vd += 3;
			copy_v3_v3(pdd->vd, vec2); pdd->vd += 3;

			vec[2] = 2.0f * pixsize;
			vec[0] = vec[1] = 0.0f;
			mul_qt_v3(state->rot, vec);
			if (draw_as == PART_DRAW_AXIS) {
				copy_v3_v3(vec2, state->co);
			}
			else {
				sub_v3_v3v3(vec2, state->co, vec);
			}

			add_v3_v3(vec, state->co);

			copy_v3_v3(pdd->vd, vec); pdd->vd += 3;
			copy_v3_v3(pdd->vd, vec2); pdd->vd += 3;
			break;
		}
		case PART_DRAW_LINE:
		{
			copy_v3_v3(vec, state->vel);
			normalize_v3(vec);
			if (draw & PART_DRAW_VEL_LENGTH)
				mul_v3_fl(vec, len_v3(state->vel));
			madd_v3_v3v3fl(pdd->vd, state->co, vec, -draw_line[0]); pdd->vd += 3;
			madd_v3_v3v3fl(pdd->vd, state->co, vec,  draw_line[1]); pdd->vd += 3;
			if (cd) {
				cd[0] = cd[3] = ma_col[0];
				cd[1] = cd[4] = ma_col[1];
				cd[2] = cd[5] = ma_col[2];
				pdd->cd += 6;
			}
			break;
		}
		case PART_DRAW_CIRC:
		{
			imm_drawcircball(state->co, pixsize, imat, pos);
			break;
		}
		case PART_DRAW_BB:
		{
			float xvec[3], yvec[3], zvec[3], bb_center[3];
			if (cd) {
				cd[0] = cd[3] = cd[6] = cd[9] = ma_col[0];
				cd[1] = cd[4] = cd[7] = cd[10] = ma_col[1];
				cd[2] = cd[5] = cd[8] = cd[11] = ma_col[2];
				pdd->cd += 12;
			}

			copy_v3_v3(bb->vec, state->co);
			copy_v3_v3(bb->vel, state->vel);

			psys_make_billboard(bb, xvec, yvec, zvec, bb_center);
			
			add_v3_v3v3(pdd->vd, bb_center, xvec);
			add_v3_v3(pdd->vd, yvec); pdd->vd += 3;

			sub_v3_v3v3(pdd->vd, bb_center, xvec);
			add_v3_v3(pdd->vd, yvec); pdd->vd += 3;

			sub_v3_v3v3(pdd->vd, bb_center, xvec);
			sub_v3_v3v3(pdd->vd, pdd->vd, yvec); pdd->vd += 3;

			add_v3_v3v3(pdd->vd, bb_center, xvec);
			sub_v3_v3v3(pdd->vd, pdd->vd, yvec); pdd->vd += 3;

			copy_v3_v3(pdd->nd, zvec); pdd->nd += 3;
			copy_v3_v3(pdd->nd, zvec); pdd->nd += 3;
			copy_v3_v3(pdd->nd, zvec); pdd->nd += 3;
			copy_v3_v3(pdd->nd, zvec); pdd->nd += 3;
			break;
		}
	}
}
static void draw_particle_data(ParticleSystem *psys, RegionView3D *rv3d,
                               ParticleKey *state, int draw_as,
                               float imat[4][4], ParticleBillboardData *bb, ParticleDrawData *pdd,
                               const float ct, const float pa_size, const float r_tilt, const float pixsize_scale,
                               unsigned int pos)
{
	ParticleSettings *part = psys->part;
	float pixsize;

	if (psys->parent)
		mul_m4_v3(psys->parent->obmat, state->co);

	/* create actual particle data */
	if (draw_as == PART_DRAW_BB) {
		bb->offset[0] = part->bb_offset[0];
		bb->offset[1] = part->bb_offset[1];
		bb->size[0] = part->bb_size[0] * pa_size;
		if (part->bb_align == PART_BB_VEL) {
			float pa_vel = len_v3(state->vel);
			float head = part->bb_vel_head * pa_vel;
			float tail = part->bb_vel_tail * pa_vel;
			bb->size[1] = part->bb_size[1] * pa_size + head + tail;
			/* use offset to adjust the particle center. this is relative to size, so need to divide! */
			if (bb->size[1] > 0.0f)
				bb->offset[1] += (head - tail) / bb->size[1];
		}
		else {
			bb->size[1] = part->bb_size[1] * pa_size;
		}
		bb->tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
		bb->time = ct;
	}

	pixsize = ED_view3d_pixel_size(rv3d, state->co) * pixsize_scale;

	draw_particle(state, draw_as, part->draw, pixsize, imat, part->draw_line, bb, pdd, pos);
}
/* unified drawing of all new particle systems draw types except dupli ob & group
 * mostly tries to use vertex arrays for speed
 *
 * 1. check that everything is ok & updated
 * 2. start initializing things
 * 3. initialize according to draw type
 * 4. allocate drawing data arrays
 * 5. start filling the arrays
 * 6. draw the arrays
 * 7. clean up
 */
static void draw_new_particle_system(Scene *scene, View3D *v3d, RegionView3D *rv3d,
                                     Base *base, ParticleSystem *psys,
                                     const char ob_dt, const short dflag)
{
	Object *ob = base->object;
	ParticleEditSettings *pset = PE_settings(scene);
	ParticleSettings *part = psys->part;
	ParticleData *pars = psys->particles;
	ParticleData *pa;
	ParticleKey state, *states = NULL;
	ParticleBillboardData bb;
	ParticleSimulationData sim = {NULL};
	ParticleDrawData *pdd = psys->pdd;
	Material *ma;
	float vel[3], imat[4][4];
	float timestep, pixsize_scale = 1.0f, pa_size, r_tilt, r_length;
	float pa_time, pa_birthtime, pa_dietime, pa_health, intensity;
	float cfra;
	float ma_col[3] = {0.0f, 0.0f, 0.0f};
	int a, totpart, totpoint = 0, totve = 0, drawn, draw_as, totchild = 0;
	bool select = (base->flag & BASE_SELECTED) != 0, create_cdata = false, need_v = false;
	GLint polygonmode[2];
	char numstr[32];
	unsigned char tcol[4] = {0, 0, 0, 255};
	unsigned int pos;

/* 1. */
	if (part == NULL || !psys_check_enabled(ob, psys, G.is_rendering))
		return;

	if (pars == NULL) return;

	/* don't draw normal paths in edit mode */
	if (psys_in_edit_mode(scene, psys) && (pset->flag & PE_DRAW_PART) == 0)
		return;

	if (part->draw_as == PART_DRAW_REND)
		draw_as = part->ren_as;
	else
		draw_as = part->draw_as;

	if (draw_as == PART_DRAW_NOT)
		return;

	/* prepare curvemapping tables */
	if ((psys->part->child_flag & PART_CHILD_USE_CLUMP_CURVE) && psys->part->clumpcurve)
		curvemapping_changed_all(psys->part->clumpcurve);
	if ((psys->part->child_flag & PART_CHILD_USE_ROUGH_CURVE) && psys->part->roughcurve)
		curvemapping_changed_all(psys->part->roughcurve);

/* 2. */
	sim.scene = scene;
	sim.ob = ob;
	sim.psys = psys;
	sim.psmd = psys_get_modifier(ob, psys);

	if (part->phystype == PART_PHYS_KEYED) {
		if (psys->flag & PSYS_KEYED) {
			psys_count_keyed_targets(&sim);
			if (psys->totkeyed == 0)
				return;
		}
	}

	if (select) {
		select = false;
		if (psys_get_current(ob) == psys)
			select = true;
	}

	psys->flag |= PSYS_DRAWING;

	if (part->type == PART_HAIR && !psys->childcache)
		totchild = 0;
	else
		totchild = psys->totchild * part->disp / 100;

	ma = give_current_material(ob, part->omat);

	if (v3d->zbuf) glDepthMask(1);

	if ((ma) && (part->draw_col == PART_DRAW_COL_MAT)) {
		rgb_float_to_uchar(tcol, &(ma->r));
		copy_v3_v3(ma_col, &ma->r);
	}

	timestep = psys_get_timestep(&sim);

	if ((ob->flag & OB_FROMGROUP) != 0) {
		float mat[4][4];
		mul_m4_m4m4(mat, ob->obmat, psys->imat);
		gpuMultMatrix3D(mat);
	}

	/* needed for text display */
	invert_m4_m4(ob->imat, ob->obmat);

	totpart = psys->totpart;

	cfra = BKE_scene_frame_get(scene);

	if (draw_as == PART_DRAW_PATH && psys->pathcache == NULL && psys->childcache == NULL)
		draw_as = PART_DRAW_DOT;

/* 3. */
	glLineWidth(1.0f);

	switch (draw_as) {
		case PART_DRAW_DOT:
			if (part->draw_size)
				glPointSize(part->draw_size);
			else
				glPointSize(2.0);  /* default dot size */
			break;
		case PART_DRAW_CIRC:
			/* calculate view aligned matrix: */
			copy_m4_m4(imat, rv3d->viewinv);
			normalize_v3(imat[0]);
			normalize_v3(imat[1]);
			/* fall-through */
		case PART_DRAW_CROSS:
		case PART_DRAW_AXIS:
			/* lets calculate the scale: */
			
			if (part->draw_size == 0.0)
				pixsize_scale = 2.0f;
			else
				pixsize_scale = part->draw_size;

			if (draw_as == PART_DRAW_AXIS)
				create_cdata = 1;
			break;
		case PART_DRAW_OB:
			if (part->dup_ob == NULL)
				draw_as = PART_DRAW_DOT;
			else
				draw_as = 0;
			break;
		case PART_DRAW_GR:
			if (part->dup_group == NULL)
				draw_as = PART_DRAW_DOT;
			else
				draw_as = 0;
			break;
		case PART_DRAW_BB:
			if (v3d->camera == NULL && part->bb_ob == NULL) {
				printf("Billboards need an active camera or a target object!\n");

				draw_as = part->draw_as = PART_DRAW_DOT;

				if (part->draw_size)
					glPointSize(part->draw_size);
				else
					glPointSize(2.0);  /* default dot size */
			}
			else if (part->bb_ob)
				bb.ob = part->bb_ob;
			else
				bb.ob = v3d->camera;

			bb.align = part->bb_align;
			bb.anim = part->bb_anim;
			bb.lock = part->draw & PART_DRAW_BB_LOCK;
			break;
		case PART_DRAW_PATH:
			break;
		case PART_DRAW_LINE:
			need_v = 1;
			break;
	}
	if (part->draw & PART_DRAW_SIZE && part->draw_as != PART_DRAW_CIRC) {
		copy_m4_m4(imat, rv3d->viewinv);
		normalize_v3(imat[0]);
		normalize_v3(imat[1]);
	}

	if (ELEM(draw_as, PART_DRAW_DOT, PART_DRAW_CROSS, PART_DRAW_LINE) &&
	    (part->draw_col > PART_DRAW_COL_MAT))
	{
		create_cdata = 1;
	}

	if (!create_cdata && pdd && pdd->cdata) {
		MEM_freeN(pdd->cdata);
		pdd->cdata = pdd->cd = NULL;
	}

/* 4. */
	if (draw_as && ELEM(draw_as, PART_DRAW_PATH, PART_DRAW_CIRC) == 0) {
		int tot_vec_size = (totpart + totchild) * 3 * sizeof(float);
		int create_ndata = 0;

		if (!pdd)
			pdd = psys->pdd = MEM_callocN(sizeof(ParticleDrawData), "ParticleDrawData");

		if (part->draw_as == PART_DRAW_REND && part->trail_count > 1) {
			tot_vec_size *= part->trail_count;
			psys_make_temp_pointcache(ob, psys);
		}

		switch (draw_as) {
			case PART_DRAW_AXIS:
			case PART_DRAW_CROSS:
				tot_vec_size *= 6;
				if (draw_as != PART_DRAW_CROSS)
					create_cdata = 1;
				break;
			case PART_DRAW_LINE:
				tot_vec_size *= 2;
				break;
			case PART_DRAW_BB:
				tot_vec_size *= 4;
				create_ndata = 1;
				break;
		}

		if (pdd->tot_vec_size != tot_vec_size)
			psys_free_pdd(psys);

		if (!pdd->vdata)
			pdd->vdata = MEM_callocN(tot_vec_size, "particle_vdata");
		if (create_cdata && !pdd->cdata)
			pdd->cdata = MEM_callocN(tot_vec_size, "particle_cdata");
		if (create_ndata && !pdd->ndata)
			pdd->ndata = MEM_callocN(tot_vec_size, "particle_ndata");

		if (part->draw & PART_DRAW_VEL && draw_as != PART_DRAW_LINE) {
			if (!pdd->vedata)
				pdd->vedata = MEM_callocN(2 * (totpart + totchild) * 3 * sizeof(float), "particle_vedata");

			need_v = 1;
		}
		else if (pdd->vedata) {
			/* velocity data not needed, so free it */
			MEM_freeN(pdd->vedata);
			pdd->vedata = NULL;
		}

		pdd->vd = pdd->vdata;
		pdd->ved = pdd->vedata;
		pdd->cd = pdd->cdata;
		pdd->nd = pdd->ndata;
		pdd->tot_vec_size = tot_vec_size;
	}
	else if (psys->pdd) {
		psys_free_pdd(psys);
		MEM_freeN(psys->pdd);
		pdd = psys->pdd = NULL;
	}

	if (pdd) {
		pdd->ma_col = ma_col;
	}

	psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

	/* circles don't use drawdata, so have to add a special case here */
	if ((pdd || draw_as == PART_DRAW_CIRC) && draw_as != PART_DRAW_PATH) {
		/* 5. */
		if (pdd && (pdd->flag & PARTICLE_DRAW_DATA_UPDATED) &&
		    (pdd->vedata || part->draw & (PART_DRAW_SIZE | PART_DRAW_NUM | PART_DRAW_HEALTH)) == 0)
		{
			totpoint = pdd->totpoint; /* draw data is up to date */
		}
		else {
			if ((draw_as == PART_DRAW_CIRC) || (part->draw & PART_DRAW_SIZE)) {
				pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
				immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
				imm_cpack(0xFFFFFF);
			}
			for (a = 0, pa = pars; a < totpart + totchild; a++, pa++) {
				/* setup per particle individual stuff */
				if (a < totpart) {
					if (totchild && (part->draw & PART_DRAW_PARENT) == 0) continue;
					if (pa->flag & PARS_NO_DISP || pa->flag & PARS_UNEXIST) continue;

					pa_time = (cfra - pa->time) / pa->lifetime;
					pa_birthtime = pa->time;
					pa_dietime = pa->dietime;
					pa_size = pa->size;
					if (part->phystype == PART_PHYS_BOIDS)
						pa_health = pa->boid->data.health;
					else
						pa_health = -1.0;

					r_tilt = 2.0f * (psys_frand(psys, a + 21) - 0.5f);
					r_length = psys_frand(psys, a + 22);

					if (part->draw_col > PART_DRAW_COL_MAT) {
						switch (part->draw_col) {
							case PART_DRAW_COL_VEL:
								intensity = len_v3(pa->state.vel) / part->color_vec_max;
								break;
							case PART_DRAW_COL_ACC:
								intensity = len_v3v3(pa->state.vel, pa->prev_state.vel) / ((pa->state.time - pa->prev_state.time) * part->color_vec_max);
								break;
							default:
								intensity = 1.0f; /* should never happen */
								BLI_assert(0);
								break;
						}
						CLAMP(intensity, 0.0f, 1.0f);
						weight_to_rgb(ma_col, intensity);
					}
				}
				else {
					ChildParticle *cpa = &psys->child[a - totpart];

					pa_time = psys_get_child_time(psys, cpa, cfra, &pa_birthtime, &pa_dietime);
					pa_size = psys_get_child_size(psys, cpa, cfra, NULL);

					pa_health = -1.0;

					r_tilt = 2.0f * (psys_frand(psys, a + 21) - 0.5f);
					r_length = psys_frand(psys, a + 22);
				}

				drawn = 0;
				if (part->draw_as == PART_DRAW_REND && part->trail_count > 1) {
					float length = part->path_end * (1.0f - part->randlength * r_length);
					int trail_count = part->trail_count * (1.0f - part->randlength * r_length);
					float ct = ((part->draw & PART_ABS_PATH_TIME) ? cfra : pa_time) - length;
					float dt = length / (trail_count ? (float)trail_count : 1.0f);
					int i = 0;

					ct += dt;
					for (i = 0; i < trail_count; i++, ct += dt) {

						if (part->draw & PART_ABS_PATH_TIME) {
							if (ct < pa_birthtime || ct > pa_dietime)
								continue;
						}
						else if (ct < 0.0f || ct > 1.0f)
							continue;

						state.time = (part->draw & PART_ABS_PATH_TIME) ? -ct : -(pa_birthtime + ct * (pa_dietime - pa_birthtime));
						psys_get_particle_on_path(&sim, a, &state, need_v);

						draw_particle_data(psys, rv3d,
						                   &state, draw_as, imat, &bb, psys->pdd,
						                   ct, pa_size, r_tilt, pixsize_scale, pos);

						totpoint++;
						drawn = 1;
					}
				}
				else {
					state.time = cfra;
					if (psys_get_particle_state(&sim, a, &state, 0)) {

						draw_particle_data(psys, rv3d,
						                   &state, draw_as, imat, &bb, psys->pdd,
						                   pa_time, pa_size, r_tilt, pixsize_scale, pos);

						totpoint++;
						drawn = 1;
					}
				}

				if (drawn) {
					/* additional things to draw for each particle
					 * (velocity, size and number) */
					if ((part->draw & PART_DRAW_VEL) && pdd && pdd->vedata) {
						copy_v3_v3(pdd->ved, state.co);
						pdd->ved += 3;
						mul_v3_v3fl(vel, state.vel, timestep);
						add_v3_v3v3(pdd->ved, state.co, vel);
						pdd->ved += 3;

						totve++;
					}

					if (part->draw & PART_DRAW_SIZE) {
						setlinestyle(3);
						imm_drawcircball(state.co, pa_size, imat, pos);
						setlinestyle(0);
					}


					if ((part->draw & PART_DRAW_NUM || part->draw & PART_DRAW_HEALTH) &&
					    (v3d->flag2 & V3D_RENDER_OVERRIDE) == 0)
					{
						size_t numstr_len;
						float vec_txt[3];
						char *val_pos = numstr;
						numstr[0] = '\0';

						if (part->draw & PART_DRAW_NUM) {
							if (a < totpart && (part->draw & PART_DRAW_HEALTH) && (part->phystype == PART_PHYS_BOIDS)) {
								numstr_len = BLI_snprintf_rlen(val_pos, sizeof(numstr), "%d:%.2f", a, pa_health);
							}
							else {
								numstr_len = BLI_snprintf_rlen(val_pos, sizeof(numstr), "%d", a);
							}
						}
						else {
							if (a < totpart && (part->draw & PART_DRAW_HEALTH) && (part->phystype == PART_PHYS_BOIDS)) {
								numstr_len = BLI_snprintf_rlen(val_pos, sizeof(numstr), "%.2f", pa_health);
							}
						}

						if (numstr[0]) {
							/* in path drawing state.co is the end point
							 * use worldspace because object matrix is already applied */
							mul_v3_m4v3(vec_txt, ob->imat, state.co);
							view3d_cached_text_draw_add(vec_txt, numstr, numstr_len,
							                            10, V3D_CACHE_TEXT_WORLDSPACE | V3D_CACHE_TEXT_ASCII, tcol);
						}
					}
				}
			}
			if ((draw_as == PART_DRAW_CIRC) || (part->draw & PART_DRAW_SIZE)) {
				immUnbindProgram();
			}
		}
	}
/* 6. */

	glGetIntegerv(GL_POLYGON_MODE, polygonmode);

	if (draw_as == PART_DRAW_PATH) {
		ParticleCacheKey **cache, *path;
		float *cdata2 = NULL;

		if (totchild && (part->draw & PART_DRAW_PARENT) == 0)
			totpart = 0;
		else if (psys->pathcache == NULL)
			totpart = 0;

		/* draw actual/parent particles */
		cache = psys->pathcache;
		for (a = 0, pa = psys->particles; a < totpart; a++, pa++) {
			path = cache[a];
			if (path->segments > 0) {
				if (((dflag & DRAW_CONSTCOLOR) == 0) && (part->draw_col == PART_DRAW_COL_MAT)) {
					draw_vertex_array(GL_LINE_STRIP, path->co, path->vel, path->col, sizeof(ParticleCacheKey), path->segments + 1, NULL);
				}
				else {
					float color[4];
					rgba_uchar_to_float(color, tcol);
					draw_vertex_array(GL_LINE_STRIP, path->co, path->vel, NULL, sizeof(ParticleCacheKey), path->segments + 1, color);
				}
			}
		}

		if (part->type ==  PART_HAIR) {
			if (part->draw & PART_DRAW_GUIDE_HAIRS) {
				DerivedMesh *hair_dm = psys->hair_out_dm;

				for (a = 0, pa = psys->particles; a < totpart; a++, pa++) {
					if (pa->totkey > 1) {
						HairKey *hkey = pa->hair;
						
						/* XXX use proper theme color here */
						float color[4] = {0.58f, 0.67f, 1.0f, 1.0f};
						draw_vertex_array(GL_LINE_STRIP, hkey->world_co, NULL, NULL, sizeof(HairKey), pa->totkey, color);
					}
				}
				
				if (hair_dm) {
					MVert *mvert = hair_dm->getVertArray(hair_dm);
					int i;
					
					unsigned int pos_id = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
					immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
					immUniformColor3f(0.9f, 0.4f, 0.4f);

					unsigned int count = 0;
					for (a = 0, pa = psys->particles; a < totpart; a++, pa++) {
						count += MAX2(pa->totkey - 1, 0);
					}

					if (count > 0) {
						immBegin(GL_LINES, count * 2);
						for (a = 0, pa = psys->particles; a < totpart; a++, pa++) {
							for (i = 1; i < pa->totkey; ++i) {
								float v1[3], v2[3];

								copy_v3_v3(v1, mvert[pa->hair_index + i - 1].co);
								copy_v3_v3(v2, mvert[pa->hair_index + i].co);

								mul_m4_v3(ob->obmat, v1);
								mul_m4_v3(ob->obmat, v2);

								immVertex3fv(pos_id, v1);
								immVertex3fv(pos_id, v2);
							}
						}
						immEnd();
					}

					immUnbindProgram();
				}
			}
			
			if (part->draw & PART_DRAW_HAIR_GRID) {
				ClothModifierData *clmd = psys->clmd;
				if (clmd) {
					float *gmin = clmd->hair_grid_min;
					float *gmax = clmd->hair_grid_max;
					int *res = clmd->hair_grid_res;
					int i;
					
					unsigned int pos_id = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
					immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
					if (select)
						immUniformThemeColor(TH_ACTIVE);
					else
						immUniformThemeColor(TH_WIRE);

					immBegin(GL_LINES, 24);
					immVertex3f(pos_id, gmin[0], gmin[1], gmin[2]); immVertex3f(pos_id, gmax[0], gmin[1], gmin[2]);
					immVertex3f(pos_id, gmax[0], gmin[1], gmin[2]); immVertex3f(pos_id, gmax[0], gmax[1], gmin[2]);
					immVertex3f(pos_id, gmax[0], gmax[1], gmin[2]); immVertex3f(pos_id, gmin[0], gmax[1], gmin[2]);
					immVertex3f(pos_id, gmin[0], gmax[1], gmin[2]); immVertex3f(pos_id, gmin[0], gmin[1], gmin[2]);
					
					immVertex3f(pos_id, gmin[0], gmin[1], gmax[2]); immVertex3f(pos_id, gmax[0], gmin[1], gmax[2]);
					immVertex3f(pos_id, gmax[0], gmin[1], gmax[2]); immVertex3f(pos_id, gmax[0], gmax[1], gmax[2]);
					immVertex3f(pos_id, gmax[0], gmax[1], gmax[2]); immVertex3f(pos_id, gmin[0], gmax[1], gmax[2]);
					immVertex3f(pos_id, gmin[0], gmax[1], gmax[2]); immVertex3f(pos_id, gmin[0], gmin[1], gmax[2]);
					
					immVertex3f(pos_id, gmin[0], gmin[1], gmin[2]); immVertex3f(pos_id, gmin[0], gmin[1], gmax[2]);
					immVertex3f(pos_id, gmax[0], gmin[1], gmin[2]); immVertex3f(pos_id, gmax[0], gmin[1], gmax[2]);
					immVertex3f(pos_id, gmin[0], gmax[1], gmin[2]); immVertex3f(pos_id, gmin[0], gmax[1], gmax[2]);
					immVertex3f(pos_id, gmax[0], gmax[1], gmin[2]); immVertex3f(pos_id, gmax[0], gmax[1], gmax[2]);
					immEnd();
					
					if (select)
						immUniformThemeColorShadeAlpha(TH_ACTIVE, 0, -100);
					else
						immUniformThemeColorShadeAlpha(TH_WIRE, 0, -100);

					int count = 0;
					count += MAX2(0, res[0] - 2) * 8;
					count += MAX2(0, res[1] - 2) * 8;
					count += MAX2(0, res[2] - 2) * 8;

					if (count >= 2) {
						glEnable(GL_BLEND);
						immBegin(GL_LINES, count);
						for (i = 1; i < res[0] - 1; ++i) {
							float f = interpf(gmax[0], gmin[0], (float)i / (float)(res[0] - 1));
							immVertex3f(pos_id, f, gmin[1], gmin[2]); immVertex3f(pos_id, f, gmax[1], gmin[2]);
							immVertex3f(pos_id, f, gmax[1], gmin[2]); immVertex3f(pos_id, f, gmax[1], gmax[2]);
							immVertex3f(pos_id, f, gmax[1], gmax[2]); immVertex3f(pos_id, f, gmin[1], gmax[2]);
							immVertex3f(pos_id, f, gmin[1], gmax[2]); immVertex3f(pos_id, f, gmin[1], gmin[2]);
						}
						for (i = 1; i < res[1] - 1; ++i) {
							float f = interpf(gmax[1], gmin[1], (float)i / (float)(res[1] - 1));
							immVertex3f(pos_id, gmin[0], f, gmin[2]); immVertex3f(pos_id, gmax[0], f, gmin[2]);
							immVertex3f(pos_id, gmax[0], f, gmin[2]); immVertex3f(pos_id, gmax[0], f, gmax[2]);
							immVertex3f(pos_id, gmax[0], f, gmax[2]); immVertex3f(pos_id, gmin[0], f, gmax[2]);
							immVertex3f(pos_id, gmin[0], f, gmax[2]); immVertex3f(pos_id, gmin[0], f, gmin[2]);
						}
						for (i = 1; i < res[2] - 1; ++i) {
							float f = interpf(gmax[2], gmin[2], (float)i / (float)(res[2] - 1));
							immVertex3f(pos_id, gmin[0], gmin[1], f); immVertex3f(pos_id, gmax[0], gmin[1], f);
							immVertex3f(pos_id, gmax[0], gmin[1], f); immVertex3f(pos_id, gmax[0], gmax[1], f);
							immVertex3f(pos_id, gmax[0], gmax[1], f); immVertex3f(pos_id, gmin[0], gmax[1], f);
							immVertex3f(pos_id, gmin[0], gmax[1], f); immVertex3f(pos_id, gmin[0], gmin[1], f);
						}
						immEnd();
						glDisable(GL_BLEND);
					}

					immUnbindProgram();
				}
			}
		}
		
		/* draw child particles */
		cache = psys->childcache;
		for (a = 0; a < totchild; a++) {
			path = cache[a];

			if (((dflag & DRAW_CONSTCOLOR) == 0) && (part->draw_col == PART_DRAW_COL_MAT)) {
				draw_vertex_array(GL_LINE_STRIP, path->co, path->vel, path->col, sizeof(ParticleCacheKey), path->segments + 1, NULL);
			}
			else {
				float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
				draw_vertex_array(GL_LINE_STRIP, path->co, path->vel, NULL, sizeof(ParticleCacheKey), path->segments + 1, color);
			}
		}

		/* restore & clean up */
		if (cdata2) {
			MEM_freeN(cdata2);
			cdata2 = NULL;
		}

		if ((part->draw & PART_DRAW_NUM) && (v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
			cache = psys->pathcache;

			for (a = 0, pa = psys->particles; a < totpart; a++, pa++) {
				float vec_txt[3];
				size_t numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%i", a);
				/* use worldspace because object matrix is already applied */
				mul_v3_m4v3(vec_txt, ob->imat, cache[a]->co);
				view3d_cached_text_draw_add(vec_txt, numstr, numstr_len,
				                            10, V3D_CACHE_TEXT_WORLDSPACE | V3D_CACHE_TEXT_ASCII, tcol);
			}
		}
	}
	else if (pdd && ELEM(draw_as, 0, PART_DRAW_CIRC) == 0) {

		if (pdd->vdata) {
			if (select) {
				float color[4];
				UI_GetThemeColor4fv(TH_ACTIVE, color);

				if (part->draw_size)
					glPointSize(part->draw_size + 2);
				else
					glPointSize(4.0);

				glLineWidth(3.0);

				draw_particle_arrays_new(draw_as, ob_dt, 1, pdd->vdata, NULL, NULL, totpoint, color);
			}

			glPointSize(part->draw_size ? part->draw_size : 2.0);
			glLineWidth(1.0);


#if 0
			/* enable other data arrays */
			/* billboards are drawn this way */
			if (pdd->ndata && ob_dt > OB_WIRE) {
				GPU_basic_shader_colors(NULL, NULL, 0.0f, 1.0f);
				GPU_basic_shader_bind(GPU_SHADER_LIGHTING | GPU_SHADER_USE_COLOR);
			}
			if ((dflag & DRAW_CONSTCOLOR) == 0) {
				if (pdd->cdata) {
					glEnableClientState(GL_COLOR_ARRAY);
					glColorPointer(3, GL_FLOAT, 0, pdd->cdata);
				}
			}
#endif

			draw_particle_arrays_new(draw_as, ob_dt, 0, pdd->vdata, pdd->ndata, pdd->cdata, totpoint, NULL);
		}


		pdd->flag |= PARTICLE_DRAW_DATA_UPDATED;
		pdd->totpoint = totpoint;
	}

	if (pdd && pdd->vedata) {
		float color[4] = {0.75f, 0.75f, 0.75f, 1.0f};
		draw_vertex_array(GL_LINES, pdd->vedata, NULL, NULL, 0, 2 * totve, color);
	}

	glPolygonMode(GL_FRONT, polygonmode[0]);
	glPolygonMode(GL_BACK, polygonmode[1]);

/* 7. */
	
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);

	if (states)
		MEM_freeN(states);

	psys->flag &= ~PSYS_DRAWING;

	/* draw data can't be saved for billboards as they must update to target changes */
	if (draw_as == PART_DRAW_BB) {
		psys_free_pdd(psys);
		pdd->flag &= ~PARTICLE_DRAW_DATA_UPDATED;
	}

	if (psys->lattice_deform_data) {
		end_latt_deform(psys->lattice_deform_data);
		psys->lattice_deform_data = NULL;
	}

	if (pdd) {
		/* drop references to stack memory */
		pdd->ma_col = NULL;
	}

	if ((ob->flag & OB_FROMGROUP) != 0) {
		gpuLoadMatrix3D(rv3d->viewmat);
	}
}

static void draw_update_ptcache_edit(Scene *scene, SceneLayer *sl, Object *ob, PTCacheEdit *edit)
{
	if (edit->psys && edit->psys->flag & PSYS_HAIR_UPDATED)
		PE_update_object(scene, sl, ob, 0);

	/* create path and child path cache if it doesn't exist already */
	if (edit->pathcache == NULL)
		psys_cache_edit_paths(scene, ob, edit, CFRA, G.is_rendering);
}

static void draw_ptcache_edit(Scene *scene, View3D *v3d, PTCacheEdit *edit)
{
	ParticleEditSettings *pset = PE_settings(scene);
	const int totpoint = edit->totpoint;
	const bool timed = (pset->flag & PE_FADE_TIME) ? pset->fade_frames : false;

	if (edit->pathcache == NULL)
		return;

	PE_hide_keys_time(scene, edit, CFRA);

	/* opengl setup */
	if ((v3d->flag & V3D_ZBUF_SELECT) == 0)
		glDisable(GL_DEPTH_TEST);

	/* get selection theme colors */
	float sel_col[3], nosel_col[3];
	UI_GetThemeColor3fv(TH_VERTEX_SELECT, sel_col);
	UI_GetThemeColor3fv(TH_VERTEX, nosel_col);

	/* draw paths */
	const int totkeys = (*edit->pathcache)->segments + 1;

	glEnable(GL_BLEND);
	float *pathcol = MEM_callocN(totkeys * 4 * sizeof(float), "particle path color data");

	if (pset->brushtype == PE_BRUSH_WEIGHT)
		glLineWidth(2.0f);

	ParticleCacheKey **cache = edit->pathcache;
	PTCacheEditPoint *point;
	int i;

	for (i = 0, point = edit->points; i < totpoint; i++, point++) {
		ParticleCacheKey *path = cache[i];

		VertexFormat format = {0};
		unsigned int pos_id, col_id, col_comp;

		col_comp = ((point->flag & PEP_HIDE) || timed) ? 4 : 3;

		pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		col_id = add_attrib(&format, "color", GL_FLOAT, col_comp, KEEP_FLOAT);

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, path->segments + 1);

		fillAttribStride(vbo, pos_id, sizeof(ParticleCacheKey), path->co);

		float *pcol = pathcol;

		if (point->flag & PEP_HIDE) {
			for (int k = 0; k < totkeys; k++, pcol += 4) {
				copy_v3_v3(pcol, path->col);
				pcol[3] = 0.25f;
			}

			fillAttrib(vbo, col_id, pathcol);
		}
		else if (timed) {
			ParticleCacheKey *pkey = path;
			for (int k = 0; k < totkeys; k++, pkey++, pcol += 4) {
				copy_v3_v3(pcol, pkey->col);
				pcol[3] = 1.0f - fabsf((float)(CFRA) -pkey->time) / (float)pset->fade_frames;
			}

			fillAttrib(vbo, col_id, pathcol);
		}
		else {
			/* FIXME: shader wants 4 color components but the cache only contains ParticleCacheKey
			 * So alpha is random */
			fillAttribStride(vbo, col_id, sizeof(ParticleCacheKey), path->col);
		}

		Batch *batch = Batch_create(GL_LINE_STRIP, vbo, NULL);
		Batch_set_builtin_program(batch, GPU_SHADER_3D_SMOOTH_COLOR);
		Batch_draw(batch);
		Batch_discard_all(batch);
	}

	if (pathcol) { MEM_freeN(pathcol); pathcol = NULL; }

	/* draw edit vertices */
	if (pset->selectmode != SCE_SELECT_PATH) {
		glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

		if (pset->selectmode == SCE_SELECT_POINT) {
			float *pd = NULL, *pdata = NULL;
			float *cd = NULL, *cdata = NULL;
			int totkeys_visible = 0;

			VertexFormat format = {0};
			unsigned int pos_id, col_id;
			pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			col_id = add_attrib(&format, "color", GL_FLOAT, (timed ? 4 : 3), KEEP_FLOAT);

			for (i = 0, point = edit->points; i < totpoint; i++, point++)
				if (!(point->flag & PEP_HIDE))
					totkeys_visible += point->totkey;

			if (totkeys_visible) {
				if (edit->points && !(edit->points->keys->flag & PEK_USE_WCO))
					pd = pdata = MEM_callocN(totkeys_visible * 3 * sizeof(float), "particle edit point data");
				cd = cdata = MEM_callocN(totkeys_visible * (timed ? 4 : 3) * sizeof(float), "particle edit color data");
			}

			for (i = 0, point = edit->points; i < totpoint; i++, point++) {
				if (point->flag & PEP_HIDE)
					continue;

				PTCacheEditKey *key = point->keys;
				for (int k = 0; k < point->totkey; k++, key++) {
					if (pd) {
						copy_v3_v3(pd, key->co);
						pd += 3;
					}

					if (key->flag & PEK_SELECT) {
						copy_v3_v3(cd, sel_col);
					}
					else {
						copy_v3_v3(cd, nosel_col);
					}

					if (timed)
						*(cd + 3) = 1.0f - fabsf((float)CFRA - *key->time) / (float)pset->fade_frames;

					cd += (timed ? 4 : 3);
				}
			}
			cd = cdata;
			pd = pdata;
			for (i = 0, point = edit->points; i < totpoint; i++, point++) {
				if (point->flag & PEP_HIDE || point->totkey == 0)
					continue;

				VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
				VertexBuffer_allocate_data(vbo, point->totkey);

				if (point->keys->flag & PEK_USE_WCO)
					fillAttribStride(vbo, pos_id, sizeof(PTCacheEditKey), point->keys->world_co);
				else
					fillAttrib(vbo, pos_id, pd);

				fillAttrib(vbo, col_id, cd);

				Batch *batch = Batch_create(GL_POINTS, vbo, NULL);
				Batch_set_builtin_program(batch, GPU_SHADER_3D_SMOOTH_COLOR);
				Batch_draw(batch);
				Batch_discard_all(batch);

				pd += pd ? 3 * point->totkey : 0;
				cd += (timed ? 4 : 3) * point->totkey;
			}
			if (pdata) { MEM_freeN(pdata); pd = pdata = NULL; }
			if (cdata) { MEM_freeN(cdata); cd = cdata = NULL; }
		}
		else if (pset->selectmode == SCE_SELECT_END) {
			VertexFormat *format = immVertexFormat();
			unsigned int pos_id = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			unsigned int col_id = add_attrib(format, "color", GL_FLOAT, 3, KEEP_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
			immBeginAtMost(GL_POINTS, totpoint);
			for (i = 0, point = edit->points; i < totpoint; i++, point++) {
				if ((point->flag & PEP_HIDE) == 0 && point->totkey) {
					PTCacheEditKey *key = point->keys + point->totkey - 1;
					if ((key->flag & PEK_SELECT) != 0) {
						immAttrib3fv(col_id, sel_col);
					}
					else {
						immAttrib3fv(col_id, nosel_col);
					}
					/* has to be like this.. otherwise selection won't work, have try glArrayElement later..*/
					immVertex3fv(pos_id, (key->flag & PEK_USE_WCO) ? key->world_co : key->co);
				}
			}
			immEnd();
			immUnbindProgram();
		}
	}

	glDisable(GL_BLEND);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
}

static void ob_draw_RE_motion(float com[3], float rotscale[3][3], float itw, float ith, float drw_size)
{
	float tr[3][3];
	float root[3], tip[3];
	/* take a copy for not spoiling original */
	copy_m3_m3(tr, rotscale);
	float tw = itw * drw_size;
	float th = ith * drw_size;

	VertexFormat *format = immVertexFormat();
	unsigned int pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	unsigned int col = add_attrib(format, "color", GL_UNSIGNED_BYTE, 4, NORMALIZE_INT_TO_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

	immBegin(GL_LINES, 30);

	immAttrib4ub(col, 0x7F, 0x00, 0x00, 155);
	root[1] = root[2] = 0.0f;
	root[0] = -drw_size;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	tip[1] = tip[2] = 0.0f;
	tip[0] = drw_size;
	mul_m3_v3(tr, tip);
	add_v3_v3(tip, com);
	immVertex3fv(pos, tip);

	root[1] = 0.0f; root[2] = tw;
	root[0] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[1] = 0.0f; root[2] = -tw;
	root[0] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[1] = tw; root[2] = 0.0f;
	root[0] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[1] = -tw; root[2] = 0.0f;
	root[0] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	immAttrib4ub(col, 0x00, 0x7F, 0x00, 155);

	root[0] = root[2] = 0.0f;
	root[1] = -drw_size;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	tip[0] = tip[2] = 0.0f;
	tip[1] = drw_size;
	mul_m3_v3(tr, tip);
	add_v3_v3(tip, com);
	immVertex3fv(pos, tip);

	root[0] = 0.0f; root[2] = tw;
	root[1] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[0] = 0.0f; root[2] = -tw;
	root[1] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[0] = tw; root[2] = 0.0f;
	root[1] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[0] = -tw; root[2] = 0.0f;
	root[1] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	immAttrib4ub(col, 0x00, 0x00, 0x7F, 155);
	root[0] = root[1] = 0.0f;
	root[2] = -drw_size;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	tip[0] = tip[1] = 0.0f;
	tip[2] = drw_size;
	mul_m3_v3(tr, tip);
	add_v3_v3(tip, com);
	immVertex3fv(pos, tip);

	root[0] = 0.0f; root[1] = tw;
	root[2] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[0] = 0.0f; root[1] = -tw;
	root[2] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[0] = tw; root[1] = 0.0f;
	root[2] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	root[0] = -tw; root[1] = 0.0f;
	root[2] = th;
	mul_m3_v3(tr, root);
	add_v3_v3(root, com);
	immVertex3fv(pos, root);
	immVertex3fv(pos, tip);

	immEnd();

	immUnbindProgram();
}

/* place to add drawers */

static void drawhandlesN(Nurb *nu, const char sel, const bool hide_handles)
{
	if (nu->hide || hide_handles) return;

	if (nu->type == CU_BEZIER && nu->pntsu > 0) {

		const float *fp;

#define TH_HANDLE_COL_TOT ((TH_HANDLE_SEL_FREE - TH_HANDLE_FREE) + 1)
		/* use MIN2 when indexing to ensure newer files don't read outside the array */
		unsigned char handle_cols[TH_HANDLE_COL_TOT][3];
		const int basecol = sel ? TH_HANDLE_SEL_FREE : TH_HANDLE_FREE;

		for (int a = 0; a < TH_HANDLE_COL_TOT; a++) {
			UI_GetThemeColor3ubv(basecol + a, handle_cols[a]);
		}

		VertexFormat *format = immVertexFormat();
		unsigned int pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		unsigned int col = add_attrib(format, "color", GL_UNSIGNED_BYTE, 3, NORMALIZE_INT_TO_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

		glLineWidth(1.0f);

		immBeginAtMost(GL_LINES, nu->pntsu * 4);

		BezTriple *bezt = nu->bezt;
		int a = nu->pntsu;
		while (a--) {
			if (bezt->hide == 0) {
				if ((bezt->f2 & SELECT) == sel) {
					fp = bezt->vec[0];

					immAttrib3ubv(col, handle_cols[MIN2(bezt->h1, TH_HANDLE_COL_TOT - 1)]);
					immVertex3fv(pos, fp);
					immVertex3fv(pos, fp + 3);

					immAttrib3ubv(col, handle_cols[MIN2(bezt->h2, TH_HANDLE_COL_TOT - 1)]);
					immVertex3fv(pos, fp + 3);
					immVertex3fv(pos, fp + 6);
				}
				else if ((bezt->f1 & SELECT) == sel) {
					fp = bezt->vec[0];

					immAttrib3ubv(col, handle_cols[MIN2(bezt->h1, TH_HANDLE_COL_TOT - 1)]);
					immVertex3fv(pos, fp);
					immVertex3fv(pos, fp + 3);
				}
				else if ((bezt->f3 & SELECT) == sel) {
					fp = bezt->vec[1];

					immAttrib3ubv(col, handle_cols[MIN2(bezt->h2, TH_HANDLE_COL_TOT - 1)]);
					immVertex3fv(pos, fp);
					immVertex3fv(pos, fp + 3);
				}
			}
			bezt++;
		}

		immEnd();

		immUnbindProgram();

#undef TH_HANDLE_COL_TOT

	}
}

static void drawhandlesN_active(Nurb *nu)
{
	if (nu->hide) return;

	if (nu->type == CU_BEZIER && nu->pntsu > 0) {
		unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformThemeColor(TH_ACTIVE_SPLINE);
		glLineWidth(2.0f);

		immBeginAtMost(GL_LINES, nu->pntsu * 4);
		BezTriple *bezt = nu->bezt;
		int a = nu->pntsu;
		while (a--) {
			if (bezt->hide == 0) {
				const float *fp = bezt->vec[0];

				immVertex3fv(pos, fp);
				immVertex3fv(pos, fp + 3);

				immVertex3fv(pos, fp + 3);
				immVertex3fv(pos, fp + 6);
			}
			bezt++;
		}
		immEnd();
		immUnbindProgram();
	}
}

static void drawvertsN(const Nurb *nurb, const bool hide_handles, const void *vert)
{
	const Nurb *nu;

	// just quick guesstimate of how many verts to draw
	int count = 0;
	for (nu = nurb; nu; nu = nu->next) {
		if (!nu->hide) {
			if (nu->type == CU_BEZIER) {
				count += nu->pntsu * 3;
			}
			else {
				count += nu->pntsu * nu->pntsv;
			}
		}
	}
	if (count == 0) return;

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	unsigned color = add_attrib(format, "color", GL_UNSIGNED_BYTE, 3, NORMALIZE_INT_TO_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

	unsigned char vert_color[3];
	unsigned char vert_color_select[3];
	unsigned char vert_color_active[3];
	UI_GetThemeColor3ubv(TH_VERTEX, vert_color);
	UI_GetThemeColor3ubv(TH_VERTEX_SELECT, vert_color_select);
	UI_GetThemeColor3ubv(TH_ACTIVE_VERT, vert_color_active);

	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

	immBeginAtMost(GL_POINTS, count);
	
	for (nu = nurb; nu; nu = nu->next) {

		if (nu->hide) continue;

		if (nu->type == CU_BEZIER) {

			const BezTriple *bezt = nu->bezt;
			int a = nu->pntsu;
			while (a--) {
				if (bezt->hide == 0) {
					if (bezt == vert) {
						immAttrib3ubv(color, bezt->f2 & SELECT ? vert_color_active : vert_color);
						immVertex3fv(pos, bezt->vec[1]);
						if (!hide_handles) {
							immAttrib3ubv(color, bezt->f1 & SELECT ? vert_color_active : vert_color);
							immVertex3fv(pos, bezt->vec[0]);
							immAttrib3ubv(color, bezt->f3 & SELECT ? vert_color_active : vert_color);
							immVertex3fv(pos, bezt->vec[2]);
						}
					}
					else {
						immAttrib3ubv(color, bezt->f2 & SELECT ? vert_color_select : vert_color);
						immVertex3fv(pos, bezt->vec[1]);
						if (!hide_handles) {
							immAttrib3ubv(color, bezt->f1 & SELECT ? vert_color_select : vert_color);
							immVertex3fv(pos, bezt->vec[0]);
							immAttrib3ubv(color, bezt->f3 & SELECT ? vert_color_select : vert_color);
							immVertex3fv(pos, bezt->vec[2]);
						}
					}
				}
				bezt++;
			}
		}
		else {
			const BPoint *bp = nu->bp;
			int a = nu->pntsu * nu->pntsv;
			while (a--) {
				if (bp->hide == 0) {
					if (bp == vert) {
						immAttrib3ubv(color, vert_color_active);
					}
					else {
						immAttrib3ubv(color, bp->f1 & SELECT ? vert_color_select : vert_color);
					}
					immVertex3fv(pos, bp->vec);
				}
				bp++;
			}
		}
	}

	immEnd();
	immUnbindProgram();
}

static void editnurb_draw_active_poly(Nurb *nu)
{
	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformThemeColor(TH_ACTIVE_SPLINE);

	glLineWidth(2.0f);

	BPoint *bp = nu->bp;
	for (int b = 0; b < nu->pntsv; b++) {
		if (nu->pntsu >= 2) {
			if (nu->flagu & 1) immBegin(GL_LINE_LOOP, nu->pntsu);
			else immBegin(GL_LINE_STRIP, nu->pntsu);

			for (int a = 0; a < nu->pntsu; a++, bp++) {
				immVertex3fv(pos, bp->vec);
			}

			immEnd();
		}
	}

	immUnbindProgram();
}

static void editnurb_draw_active_nurbs(Nurb *nu)
{
	if (nu->pntsv > 0) {
		VertexFormat *format = immVertexFormat();
		unsigned pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformThemeColor(TH_ACTIVE_SPLINE);

		glLineWidth(2.0f);
		// just quick guesstimate of how many verts to draw
		int count = (nu->pntsu - 1) * nu->pntsv * 2;
		if (nu->pntsv > 1) count += (nu->pntsv - 1) * nu->pntsu * 2;
		if (count < 2) return;

		immBeginAtMost(GL_LINES, count);
		BPoint *bp = nu->bp;
		for (int b = 0; b < nu->pntsv; b++) {
			BPoint *bp1 = bp;
			bp++;

			for (int a = nu->pntsu - 1; a > 0; a--, bp++) {
				if (bp->hide == 0 && bp1->hide == 0) {
					immVertex3fv(pos, bp->vec);
					immVertex3fv(pos, bp1->vec);
				}
				bp1 = bp;
			}
		}

		if (nu->pntsv > 1) {    /* surface */
			int ofs = nu->pntsu;
			for (int b = 0; b < nu->pntsu; b++) {
				BPoint *bp1 = nu->bp + b;
				bp = bp1 + ofs;
				for (int a = nu->pntsv - 1; a > 0; a--, bp += ofs) {
					if (bp->hide == 0 && bp1->hide == 0) {
						immVertex3fv(pos, bp->vec);
						immVertex3fv(pos, bp1->vec);
					}
					bp1 = bp;
				}
			}
		}

		immEnd();

		immUnbindProgram();
	}
}

static void draw_editnurb_splines(Object *ob, Nurb *nurb, const bool sel)
{
	BPoint *bp, *bp1;
	int a, b;
	Curve *cu = ob->data;
	VertexFormat *format;
	unsigned int pos, col;
	unsigned char color[3];

	int index = 0;
	Nurb *nu = nurb;
	while (nu) {
		if (nu->hide == 0) {
			switch (nu->type) {
				case CU_POLY:
				{
					if (!sel && index == cu->actnu) {
						/* we should draw active spline highlight below everything */
						editnurb_draw_active_poly(nu);
					}

					format = immVertexFormat();
					pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
					immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

					glLineWidth(1.0f);

					immUniformThemeColor(TH_NURB_ULINE);
					bp = nu->bp;
					for (b = 0; b < nu->pntsv; b++) {
						if (nu->pntsu >= 2) {
							if (nu->flagu & 1) immBegin(GL_LINE_LOOP, nu->pntsu);
							else immBegin(GL_LINE_STRIP, nu->pntsu);

							for (a = 0; a < nu->pntsu; a++, bp++) {
								immVertex3fv(pos, bp->vec);
							}

							immEnd();
						}
					}
					immUnbindProgram();
					break;
				}
				case CU_NURBS:
				{
					if (!sel && index == cu->actnu) {
						/* we should draw active spline highlight below everything */
						editnurb_draw_active_nurbs(nu);
					}

					format = immVertexFormat();
					pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
					col = add_attrib(format, "color", GL_UNSIGNED_BYTE, 3, NORMALIZE_INT_TO_FLOAT);
					immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

					// just quick guesstimate of how many verts to draw
					int count = (nu->pntsu - 1) * nu->pntsv * 2;
					if (nu->pntsv > 1) count += (nu->pntsv - 1) * nu->pntsu * 2;
					if (count < 2) return;

					glLineWidth(1.0f);

					immBeginAtMost(GL_LINES, count);

					bp = nu->bp;
					for (b = 0; b < nu->pntsv; b++) {
						bp1 = bp;
						bp++;
						for (a = nu->pntsu - 1; a > 0; a--, bp++) {
							if (bp->hide == 0 && bp1->hide == 0) {
								if (sel) {
									if ((bp->f1 & SELECT) && (bp1->f1 & SELECT)) {
										UI_GetThemeColor3ubv(TH_NURB_SEL_ULINE, color);
										immAttrib3ubv(col, color);
										immVertex3fv(pos, bp->vec);
										immVertex3fv(pos, bp1->vec);
									}
								}
								else {
									if ((bp->f1 & SELECT) && (bp1->f1 & SELECT)) {
										/* pass */
									}
									else {
										UI_GetThemeColor3ubv(TH_NURB_ULINE, color);
										immAttrib3ubv(col, color);
										immVertex3fv(pos, bp->vec);
										immVertex3fv(pos, bp1->vec);
									}
								}
							}
							bp1 = bp;
						}
					}

					if (nu->pntsv > 1) {    /* surface */
						int ofs = nu->pntsu;
						for (b = 0; b < nu->pntsu; b++) {
							bp1 = nu->bp + b;
							bp = bp1 + ofs;
							for (a = nu->pntsv - 1; a > 0; a--, bp += ofs) {
								if (bp->hide == 0 && bp1->hide == 0) {
									if (sel) {
										if ((bp->f1 & SELECT) && (bp1->f1 & SELECT)) {
											UI_GetThemeColor3ubv(TH_NURB_SEL_VLINE, color);
											immAttrib3ubv(col, color);
											immVertex3fv(pos, bp->vec);
											immVertex3fv(pos, bp1->vec);
										}
									}
									else {
										if ((bp->f1 & SELECT) && (bp1->f1 & SELECT)) {
											/* pass */
										}
										else {
											UI_GetThemeColor3ubv(TH_NURB_VLINE, color);
											immAttrib3ubv(col, color);
											immVertex3fv(pos, bp->vec);
											immVertex3fv(pos, bp1->vec);
										}
									}
								}
								bp1 = bp;
							}
						}
					}
					immEnd();
					immUnbindProgram();
					break;
				}
			}
		}

		index++;
		nu = nu->next;
	}
}

static void draw_editnurb(
        Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d, Base *base, Nurb *nurb,
        const char dt, const short dflag, const unsigned char UNUSED(ob_wire_col[4]))
{
	ToolSettings *ts = scene->toolsettings;
	Object *ob = base->object;
	Curve *cu = ob->data;
	Nurb *nu;
	const void *vert = BKE_curve_vert_active_get(cu);
	const bool hide_handles = (cu->drawflag & CU_HIDE_HANDLES) != 0;
	unsigned char wire_col[3];

	/* DispList */
	UI_GetThemeColor3ubv(TH_WIRE_EDIT, wire_col);

	drawDispList(scene, sl, v3d, rv3d, base, dt, dflag, wire_col);

	/* for shadows only show solid faces */
	if (v3d->flag2 & V3D_RENDER_SHADOW)
		return;

	if (v3d->zbuf) glDepthFunc(GL_ALWAYS);
	
	/* first non-selected and active handles */
	int index = 0;
	for (nu = nurb; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			if (index == cu->actnu && !hide_handles)
				drawhandlesN_active(nu);
			drawhandlesN(nu, 0, hide_handles);
		}
		index++;
	}
	draw_editnurb_splines(ob, nurb, false);
	draw_editnurb_splines(ob, nurb, true);
	/* selected handles */
	for (nu = nurb; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER && (cu->drawflag & CU_HIDE_HANDLES) == 0)
			drawhandlesN(nu, 1, hide_handles);
	}
	
	if (v3d->zbuf) glDepthFunc(GL_LEQUAL);

	/* direction vectors for 3d curve paths
	 * when at its lowest, don't render normals */
	if ((cu->flag & CU_3D) && (ts->normalsize > 0.0015f) && (cu->drawflag & CU_HIDE_NORMALS) == 0) {
		BevList *bl;

		VertexFormat *format = immVertexFormat();
		unsigned pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformThemeColor(TH_WIRE_EDIT);

		glLineWidth(1.0f);

		int count = 0;
		for (bl = ob->curve_cache->bev.first, nu = nurb; nu && bl; bl = bl->next, nu = nu->next) {
			int nr = bl->nr;
			int skip = nu->resolu / 16;
			
			while (nr-- > 0) { /* accounts for empty bevel lists */
				count += 4;
				nr -= skip;
			}
		}

		if (count > 2) {
			immBegin(GL_LINES, count);
			for (bl = ob->curve_cache->bev.first, nu = nurb; nu && bl; bl = bl->next, nu = nu->next) {
				BevPoint *bevp = bl->bevpoints;
				int nr = bl->nr;
				int skip = nu->resolu / 16;
				
				while (nr-- > 0) { /* accounts for empty bevel lists */
					const float fac = bevp->radius * ts->normalsize;
					float vec_a[3]; /* Offset perpendicular to the curve */
					float vec_b[3]; /* Delta along the curve */

					vec_a[0] = fac;
					vec_a[1] = 0.0f;
					vec_a[2] = 0.0f;

					vec_b[0] = -fac;
					vec_b[1] = 0.0f;
					vec_b[2] = 0.0f;

					mul_qt_v3(bevp->quat, vec_a);
					mul_qt_v3(bevp->quat, vec_b);
					add_v3_v3(vec_a, bevp->vec);
					add_v3_v3(vec_b, bevp->vec);

					madd_v3_v3fl(vec_a, bevp->dir, -fac);
					madd_v3_v3fl(vec_b, bevp->dir, -fac);

					immVertex3fv(pos, vec_a);
					immVertex3fv(pos, bevp->vec);
					immVertex3fv(pos, bevp->vec);
					immVertex3fv(pos, vec_b);

					bevp += skip + 1;
					nr -= skip;
				}
			}
			immEnd();
		}
		immUnbindProgram();
	}

	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);

	drawvertsN(nurb, hide_handles, vert);

	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
}

static void draw_editfont_textcurs(RegionView3D *rv3d, float textcurs[4][2])
{
	unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 2, KEEP_FLOAT);
	ED_view3d_polygon_offset(rv3d, -1.0);
	set_inverted_drawing(1);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	imm_cpack(0);
	immBegin(GL_TRIANGLE_FAN, 4);
	immVertex2fv(pos, textcurs[0]);
	immVertex2fv(pos, textcurs[1]);
	immVertex2fv(pos, textcurs[2]);
	immVertex2fv(pos, textcurs[3]);
	immEnd();
	set_inverted_drawing(0);
	ED_view3d_polygon_offset(rv3d, 0.0);
	immUnbindProgram();
}

static void draw_editfont(Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d, Base *base,
                          const char dt, const short dflag, const unsigned char ob_wire_col[4])
{
	Object *ob = base->object;
	Curve *cu = ob->data;
	EditFont *ef = cu->editfont;
	float vec1[3], vec2[3];
	int selstart, selend;

	draw_editfont_textcurs(rv3d, ef->textcurs);

	if (cu->flag & CU_FAST) {
		cpack(0xFFFFFF);
		set_inverted_drawing(1);
		drawDispList(scene, sl, v3d, rv3d, base, OB_WIRE, dflag, ob_wire_col);
		set_inverted_drawing(0);
	}
	else {
		drawDispList(scene, sl, v3d, rv3d, base, dt, dflag, ob_wire_col);
	}

	if (cu->linewidth != 0.0f) {
		unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 2, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
		immUniformThemeColor(TH_WIRE_EDIT);
		copy_v3_v3(vec1, ob->orig);
		copy_v3_v3(vec2, ob->orig);
		vec1[0] += cu->linewidth;
		vec2[0] += cu->linewidth;
		vec1[1] += cu->linedist * cu->fsize;
		vec2[1] -= cu->lines * cu->linedist * cu->fsize;
		setlinestyle(3);
		immBegin(GL_LINES, 2);
		immVertex2fv(pos, vec1);
		immVertex2fv(pos, vec2);
		immEnd();
		setlinestyle(0);
		immUnbindProgram();
	}

	setlinestyle(3);
	for (int i = 0; i < cu->totbox; i++) {
		if (cu->tb[i].w != 0.0f) {
			unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
			immUniformThemeColor(i == (cu->actbox - 1) ? TH_ACTIVE : TH_WIRE);
			vec1[0] = cu->xof + cu->tb[i].x;
			vec1[1] = cu->yof + cu->tb[i].y + cu->fsize;
			vec1[2] = 0.001;
			immBegin(GL_LINE_STRIP, 5);
			immVertex3fv(pos, vec1);
			vec1[0] += cu->tb[i].w;
			immVertex3fv(pos, vec1);
			vec1[1] -= cu->tb[i].h;
			immVertex3fv(pos, vec1);
			vec1[0] -= cu->tb[i].w;
			immVertex3fv(pos, vec1);
			vec1[1] += cu->tb[i].h;
			immVertex3fv(pos, vec1);
			immEnd();
			immUnbindProgram();
		}
	}
	setlinestyle(0);


	if (BKE_vfont_select_get(ob, &selstart, &selend) && ef->selboxes) {
		const int seltot = selend - selstart;
		float selboxw;

		unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		imm_cpack(0xffffff);
		set_inverted_drawing(1);
		for (int i = 0; i <= seltot; i++) {
			EditFontSelBox *sb = &ef->selboxes[i];
			float tvec[3];

			if (i != seltot) {
				if (ef->selboxes[i + 1].y == sb->y)
					selboxw = ef->selboxes[i + 1].x - sb->x;
				else
					selboxw = sb->w;
			}
			else {
				selboxw = sb->w;
			}

			/* fill in xy below */
			tvec[2] = 0.001;

			immBegin(GL_TRIANGLE_FAN, 4);

			if (sb->rot == 0.0f) {
				copy_v2_fl2(tvec, sb->x, sb->y);
				immVertex3fv(pos, tvec);

				copy_v2_fl2(tvec, sb->x + selboxw, sb->y);
				immVertex3fv(pos, tvec);

				copy_v2_fl2(tvec, sb->x + selboxw, sb->y + sb->h);
				immVertex3fv(pos, tvec);

				copy_v2_fl2(tvec, sb->x, sb->y + sb->h);
				immVertex3fv(pos, tvec);
			}
			else {
				float mat[2][2];

				angle_to_mat2(mat, sb->rot);

				copy_v2_fl2(tvec, sb->x, sb->y);
				immVertex3fv(pos, tvec);

				copy_v2_fl2(tvec, selboxw, 0.0f);
				mul_m2v2(mat, tvec);
				add_v2_v2(tvec, &sb->x);
				immVertex3fv(pos, tvec);

				copy_v2_fl2(tvec, selboxw, sb->h);
				mul_m2v2(mat, tvec);
				add_v2_v2(tvec, &sb->x);
				immVertex3fv(pos, tvec);

				copy_v2_fl2(tvec, 0.0f, sb->h);
				mul_m2v2(mat, tvec);
				add_v2_v2(tvec, &sb->x);
				immVertex3fv(pos, tvec);
			}

			immEnd();
		}
		set_inverted_drawing(0);
		immUnbindProgram();
	}
}

/* draw a sphere for use as an empty drawtype */
static void draw_empty_sphere(float size, unsigned pos)
{
#define NSEGMENTS 16
	/* a single ring of vertices */
	float p[NSEGMENTS][2];
	for (int i = 0; i < NSEGMENTS; ++i) {
		float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
		p[i][0] = size * cosf(angle);
		p[i][1] = size * sinf(angle);
	}

	immBegin(GL_LINE_LOOP, NSEGMENTS);
	for (int i = 0; i < NSEGMENTS; ++i)
		immVertex3f(pos, p[i][0], p[i][1], 0.0f);
	immEnd();
	immBegin(GL_LINE_LOOP, NSEGMENTS);
	for (int i = 0; i < NSEGMENTS; ++i)
		immVertex3f(pos, p[i][0], 0.0f, p[i][1]);
	immEnd();
	immBegin(GL_LINE_LOOP, NSEGMENTS);
	for (int i = 0; i < NSEGMENTS; ++i)
		immVertex3f(pos, 0.0f, p[i][0], p[i][1]);
	immEnd();
#undef NSEGMENTS
}

/* draw a cone for use as an empty drawtype */
static void draw_empty_cone(float size, unsigned pos)
{
#define NSEGMENTS 8
	/* a single ring of vertices */
	float p[NSEGMENTS][2];
	for (int i = 0; i < NSEGMENTS; ++i) {
		float angle = 2 * M_PI * ((float)i / (float)NSEGMENTS);
		p[i][0] = size * cosf(angle);
		p[i][1] = size * sinf(angle);
	}

	/* cone sides */
	immBegin(GL_LINES, NSEGMENTS * 2);
	for (int i = 0; i < NSEGMENTS; ++i) {
		immVertex3f(pos, 0.0f, 2.0f * size, 0.0f);
		immVertex3f(pos, p[i][0], 0.0f, p[i][1]);
	}
	immEnd();

	/* end ring */
	immBegin(GL_LINE_LOOP, NSEGMENTS);
	for (int i = 0; i < NSEGMENTS; ++i)
		immVertex3f(pos, p[i][0], 0.0f, p[i][1]);
	immEnd();
#undef NSEGMENTS
}

static void drawspiral(unsigned int pos, const float cent[3], float rad, float tmat[4][4], int start)
{
	float vec[3], vx[3], vy[3];
	const float tot_inv = 1.0f / (float)CIRCLE_RESOL;
	int a;
	bool inverse = false;
	float x, y, fac;

	if (start < 0) {
		inverse = true;
		start = -start;
	}

	mul_v3_v3fl(vx, tmat[0], rad);
	mul_v3_v3fl(vy, tmat[1], rad);

	immBegin(GL_LINE_STRIP, CIRCLE_RESOL + 1);

	if (inverse == 0) {
		copy_v3_v3(vec, cent);
		immVertex3fv(pos, vec);

		for (a = 0; a < CIRCLE_RESOL; a++) {
			if (a + start >= CIRCLE_RESOL)
				start = -a + 1;

			fac = (float)a * tot_inv;
			x = sinval[a + start] * fac;
			y = cosval[a + start] * fac;

			vec[0] = cent[0] + (x * vx[0] + y * vy[0]);
			vec[1] = cent[1] + (x * vx[1] + y * vy[1]);
			vec[2] = cent[2] + (x * vx[2] + y * vy[2]);

			immVertex3fv(pos, vec);
		}
	}
	else {
		fac = (float)(CIRCLE_RESOL - 1) * tot_inv;
		x = sinval[start] * fac;
		y = cosval[start] * fac;

		vec[0] = cent[0] + (x * vx[0] + y * vy[0]);
		vec[1] = cent[1] + (x * vx[1] + y * vy[1]);
		vec[2] = cent[2] + (x * vx[2] + y * vy[2]);

		immVertex3fv(pos, vec);

		for (a = 0; a < CIRCLE_RESOL; a++) {
			if (a + start >= CIRCLE_RESOL)
				start = -a + 1;

			fac = (float)(-a + (CIRCLE_RESOL - 1)) * tot_inv;
			x = sinval[a + start] * fac;
			y = cosval[a + start] * fac;

			vec[0] = cent[0] + (x * vx[0] + y * vy[0]);
			vec[1] = cent[1] + (x * vx[1] + y * vy[1]);
			vec[2] = cent[2] + (x * vx[2] + y * vy[2]);
			immVertex3fv(pos, vec);
		}
	}

	immEnd();
}

/* draws a circle on x-z plane given the scaling of the circle, assuming that
 * all required matrices have been set (used for drawing empties) */
static void drawcircle_size(float size, unsigned pos)
{
	immBegin(GL_LINE_LOOP, CIRCLE_RESOL);

	/* coordinates are: cos(degrees * 11.25) = x, sin(degrees * 11.25) = y, 0.0f = z */
	for (short degrees = 0; degrees < CIRCLE_RESOL; degrees++) {
		float x = cosval[degrees];
		float y = sinval[degrees];

		immVertex3f(pos, x * size, 0.0f, y * size);
	}

	immEnd();
}

/* needs fixing if non-identity matrix used */
static void imm_drawtube(const float vec[3], float radius, float height, float tmat[4][4], unsigned pos)
{
	float cur[3];
	imm_drawcircball(vec, radius, tmat, pos);

	copy_v3_v3(cur, vec);
	cur[2] += height;

	imm_drawcircball(cur, radius, tmat, pos);

	immBegin(GL_LINES, 8);
	immVertex3f(pos, vec[0] + radius, vec[1], vec[2]);
	immVertex3f(pos, cur[0] + radius, cur[1], cur[2]);
	immVertex3f(pos, vec[0] - radius, vec[1], vec[2]);
	immVertex3f(pos, cur[0] - radius, cur[1], cur[2]);
	immVertex3f(pos, vec[0], vec[1] + radius, vec[2]);
	immVertex3f(pos, cur[0], cur[1] + radius, cur[2]);
	immVertex3f(pos, vec[0], vec[1] - radius, vec[2]);
	immVertex3f(pos, cur[0], cur[1] - radius, cur[2]);
	immEnd();
}

/* needs fixing if non-identity matrix used */
static void imm_drawcone(const float vec[3], float radius, float height, float tmat[4][4], unsigned pos)
{
	float cur[3];

	copy_v3_v3(cur, vec);
	cur[2] += height;

	imm_drawcircball(cur, radius, tmat, pos);

	immBegin(GL_LINES, 8);
	immVertex3f(pos, vec[0], vec[1], vec[2]);
	immVertex3f(pos, cur[0] + radius, cur[1], cur[2]);
	immVertex3f(pos, vec[0], vec[1], vec[2]);
	immVertex3f(pos, cur[0] - radius, cur[1], cur[2]);
	immVertex3f(pos, vec[0], vec[1], vec[2]);
	immVertex3f(pos, cur[0], cur[1] + radius, cur[2]);
	immVertex3f(pos, vec[0], vec[1], vec[2]);
	immVertex3f(pos, cur[0], cur[1] - radius, cur[2]);
	immEnd();
}

/* return true if nothing was drawn */
static bool drawmball(Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d, Base *base,
                      const char dt, const short dflag, const unsigned char ob_wire_col[4])
{
	Object *ob = base->object;
	MetaElem *ml;
	float imat[4][4];
	int code = 1;
	
	MetaBall *mb = ob->data;

	if (mb->editelems) {
		if ((G.f & G_PICKSEL) == 0) {
			unsigned char wire_col[4];
			UI_GetThemeColor4ubv(TH_WIRE_EDIT, wire_col);
			drawDispList(scene, sl, v3d, rv3d, base, dt, dflag, wire_col);
		}
		ml = mb->editelems->first;
	}
	else {
		if ((base->flag_legacy & OB_FROMDUPLI) == 0) {
			drawDispList(scene, sl, v3d, rv3d, base, dt, dflag, ob_wire_col);
		}
		ml = mb->elems.first;
	}

	if (ml == NULL) {
		return true;
	}

	if (v3d->flag2 & V3D_RENDER_OVERRIDE) {
		return false;
	}

	invert_m4_m4(imat, rv3d->viewmatob);
	normalize_v3(imat[0]);
	normalize_v3(imat[1]);

#if 0 /* no purpose? */
	if (mb->editelems == NULL) {
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			glColor3ubv(ob_wire_col);
		}
	}
#endif

	glLineWidth(1.0f);

	const int unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	while (ml) {
		/* draw radius */
		if (mb->editelems) {
			if ((dflag & DRAW_CONSTCOLOR) == 0) {
				if ((ml->flag & SELECT) && (ml->flag & MB_SCALE_RAD)) imm_cpack(0xA0A0F0);
				else imm_cpack(0x3030A0);
			}
			
			if (G.f & G_PICKSEL) {
				ml->selcol1 = code;
				GPU_select_load_id(code++);
			}
		}
		imm_drawcircball(&(ml->x), ml->rad, imat, pos);

		/* draw stiffness */
		if (mb->editelems) {
			if ((dflag & DRAW_CONSTCOLOR) == 0) {
				if ((ml->flag & SELECT) && !(ml->flag & MB_SCALE_RAD)) imm_cpack(0xA0F0A0);
				else imm_cpack(0x30A030);
			}
			
			if (G.f & G_PICKSEL) {
				ml->selcol2 = code;
				GPU_select_load_id(code++);
			}
			imm_drawcircball(&(ml->x), ml->rad * atanf(ml->s) / (float)M_PI_2, imat, pos);
		}
		
		ml = ml->next;
	}

	immUnbindProgram();
	return false;
}

static void draw_forcefield(Object *ob, RegionView3D *rv3d,
                            const short dflag, const unsigned char ob_wire_col[4])
{
	PartDeflect *pd = ob->pd;
	float imat[4][4], tmat[4][4];
	float vec[3] = {0.0, 0.0, 0.0};
	float draw_color[3] = {0.0f, 0.0f, 0.0f};
	/* scale size of circle etc with the empty drawsize */
	const float size = (ob->type == OB_EMPTY) ? ob->empty_drawsize : 1.0f;
	
	/* calculus here, is reused in PFIELD_FORCE */
	invert_m4_m4(imat, rv3d->viewmatob);
#if 0
	normalize_v3(imat[0]);  /* we don't do this because field doesnt scale either... apart from wind! */
	normalize_v3(imat[1]);
#endif

	const int unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3fv(draw_color);
	
	if (pd->forcefield == PFIELD_WIND) {
		float force_val = pd->f_strength;

		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			ob_wire_color_blend_theme_id(ob_wire_col, TH_BACK, 0.5f, draw_color);
			immUniformColor3fv(draw_color);
		}

		unit_m4(tmat);
		force_val *= 0.1f;
		imm_drawcircball(vec, size, tmat, pos);
		vec[2] = 0.5f * force_val;
		imm_drawcircball(vec, size, tmat, pos);
		vec[2] = 1.0f * force_val;
		imm_drawcircball(vec, size, tmat, pos);
		vec[2] = 1.5f * force_val;
		imm_drawcircball(vec, size, tmat, pos);
		vec[2] = 0.0f; /* reset vec for max dist circle */
	}
	else if (pd->forcefield == PFIELD_FORCE) {
		float ffall_val = pd->f_power;

		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			ob_wire_color_blend_theme_id(ob_wire_col, TH_BACK, 0.5f, draw_color);
			immUniformColor3fv(draw_color);
		}

		imm_drawcircball(vec, size, imat, pos);

		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			ob_wire_color_blend_theme_id(ob_wire_col, TH_BACK, 0.9f - 0.4f / powf(1.5f, ffall_val), draw_color);
			immUniformColor3fv(draw_color);
		}

		imm_drawcircball(vec, size * 1.5f, imat, pos);

		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			ob_wire_color_blend_theme_id(ob_wire_col, TH_BACK, 0.9f - 0.4f / powf(2.0f, ffall_val), draw_color);
			immUniformColor3fv(draw_color);
		}

		imm_drawcircball(vec, size * 2.0f, imat, pos);
	}
	else if (pd->forcefield == PFIELD_VORTEX) {
		float force_val = pd->f_strength;

		unit_m4(tmat);

		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			ob_wire_color_blend_theme_id(ob_wire_col, TH_BACK, 0.7f, draw_color);
			immUniformColor3fv(draw_color);
		}

		if (force_val < 0) {
			drawspiral(pos, vec, size, tmat, 1);
			drawspiral(pos, vec, size, tmat, 16);
		}
		else {
			drawspiral(pos, vec, size, tmat, -1);
			drawspiral(pos, vec, size, tmat, -16);
		}
	}
	else if (pd->forcefield == PFIELD_GUIDE && ob->type == OB_CURVE) {
		Curve *cu = ob->data;
		if ((cu->flag & CU_PATH) && ob->curve_cache->path && ob->curve_cache->path->data) {
			float guidevec1[4], guidevec2[3];
			float mindist = pd->f_strength;

			if ((dflag & DRAW_CONSTCOLOR) == 0) {
				ob_wire_color_blend_theme_id(ob_wire_col, TH_BACK, 0.5f, draw_color);
				immUniformColor3fv(draw_color);
			}

			/* path end */
			setlinestyle(3);
			where_on_path(ob, 1.0f, guidevec1, guidevec2, NULL, NULL, NULL);
			imm_drawcircball(guidevec1, mindist, imat, pos);

			/* path beginning */
			setlinestyle(0);
			where_on_path(ob, 0.0f, guidevec1, guidevec2, NULL, NULL, NULL);
			imm_drawcircball(guidevec1, mindist, imat, pos);
			
			copy_v3_v3(vec, guidevec1); /* max center */
		}
	}

	setlinestyle(3);

	if ((dflag & DRAW_CONSTCOLOR) == 0) {
		ob_wire_color_blend_theme_id(ob_wire_col, TH_BACK, 0.5f, draw_color);
		immUniformColor3fv(draw_color);
	}

	if (pd->falloff == PFIELD_FALL_SPHERE) {
		/* as last, guide curve alters it */
		if ((pd->flag & PFIELD_USEMAX) != 0) {
			imm_drawcircball(vec, pd->maxdist, imat, pos);
		}

		if ((pd->flag & PFIELD_USEMIN) != 0) {
			imm_drawcircball(vec, pd->mindist, imat, pos);
		}
	}
	else if (pd->falloff == PFIELD_FALL_TUBE) {
		float radius, distance;

		unit_m4(tmat);

		vec[0] = vec[1] = 0.0f;
		radius = (pd->flag & PFIELD_USEMAXR) ? pd->maxrad : 1.0f;
		distance = (pd->flag & PFIELD_USEMAX) ? pd->maxdist : 0.0f;
		vec[2] = distance;
		distance = (pd->flag & PFIELD_POSZ) ? -distance : -2.0f * distance;

		if (pd->flag & (PFIELD_USEMAX | PFIELD_USEMAXR)) {
			imm_drawtube(vec, radius, distance, tmat, pos);
		}

		radius = (pd->flag & PFIELD_USEMINR) ? pd->minrad : 1.0f;
		distance = (pd->flag & PFIELD_USEMIN) ? pd->mindist : 0.0f;
		vec[2] = distance;
		distance = (pd->flag & PFIELD_POSZ) ? -distance : -2.0f * distance;

		if (pd->flag & (PFIELD_USEMIN | PFIELD_USEMINR)) {
			imm_drawtube(vec, radius, distance, tmat, pos);
		}
	}
	else if (pd->falloff == PFIELD_FALL_CONE) {
		float radius, distance;

		unit_m4(tmat);

		radius = DEG2RADF((pd->flag & PFIELD_USEMAXR) ? pd->maxrad : 1.0f);
		distance = (pd->flag & PFIELD_USEMAX) ? pd->maxdist : 0.0f;

		if (pd->flag & (PFIELD_USEMAX | PFIELD_USEMAXR)) {
			imm_drawcone(vec, distance * sinf(radius), distance * cosf(radius), tmat, pos);
			if ((pd->flag & PFIELD_POSZ) == 0)
				imm_drawcone(vec, distance * sinf(radius), -distance * cosf(radius), tmat, pos);
		}

		radius = DEG2RADF((pd->flag & PFIELD_USEMINR) ? pd->minrad : 1.0f);
		distance = (pd->flag & PFIELD_USEMIN) ? pd->mindist : 0.0f;

		if (pd->flag & (PFIELD_USEMIN | PFIELD_USEMINR)) {
			imm_drawcone(vec, distance * sinf(radius), distance * cosf(radius), tmat, pos);
			if ((pd->flag & PFIELD_POSZ) == 0)
				imm_drawcone(vec, distance * sinf(radius), -distance * cosf(radius), tmat, pos);
		}
	}
	setlinestyle(0);

	immUnbindProgram();
}

static void imm_draw_box(const float vec[8][3], bool solid, unsigned pos)
{
	static const GLubyte quad_indices[24] = {0,1,2,3,7,6,5,4,4,5,1,0,3,2,6,7,3,7,4,0,1,5,6,2};
	static const GLubyte line_indices[24] = {0,1,1,2,2,3,3,0,0,4,4,5,5,6,6,7,7,4,1,5,2,6,3,7};

	const GLubyte *indices;
	GLenum prim_type;

	if (solid) {
		indices = quad_indices;
		prim_type = GL_QUADS;
	}
	else {
		indices = line_indices;
		prim_type = GL_LINES;
	}

	immBegin(prim_type, 24);
	for (int i = 0; i < 24; ++i) {
		immVertex3fv(pos, vec[indices[i]]);
	}
	immEnd();
}

static void imm_draw_bb(BoundBox *bb, char type, bool around_origin, const unsigned char ob_wire_col[4])
{
	float size[3], cent[3];
	Batch *sphere = Batch_get_sphere_wire(0);
	Batch_set_builtin_program(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
	if (ob_wire_col) Batch_Uniform4f(sphere, "color", ob_wire_col[0]/255.0f, ob_wire_col[1]/255.0f, ob_wire_col[2]/255.0f, 1.0f);
	
	BKE_boundbox_calc_size_aabb(bb, size);

	if (around_origin) {
		zero_v3(cent);
	}
	else {
		BKE_boundbox_calc_center_aabb(bb, cent);
	}
	
	gpuPushMatrix();

	unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	if (ob_wire_col) immUniformColor3ubv(ob_wire_col);

	if (type == OB_BOUND_SPHERE) {
		float scale = MAX3(size[0], size[1], size[2]);
		gpuTranslate3fv(cent);
		gpuRotateAxis(90, 'X');
		gpuScaleUniform(scale);
		Batch_draw(sphere);
	}
	else if (type == OB_BOUND_CYLINDER) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		gpuTranslate3f(cent[0], cent[1], cent[2] - size[2]);
		gpuScale3f(radius, radius, 2.0f * size[2]);
		imm_cylinder_wire(pos, 1.0f, 1.0f, 1.0f, 8, 1);
	}
	else if (type == OB_BOUND_CONE) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		gpuTranslate3f(cent[0], cent[1], cent[2] - size[2]);
		gpuScale3f(radius, radius, 2.0f * size[2]);
		imm_cylinder_wire(pos, 1.0f, 0.0f, 1.0f, 8, 1);

	}
	else if (type == OB_BOUND_CAPSULE) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		float length = size[2] > radius ? 2.0f * (size[2] - radius) : 0.0f;
		gpuTranslate3f(cent[0], cent[1], cent[2] - length * 0.5f);
		imm_cylinder_wire(pos, radius, radius, length, 8, 1);

		gpuRotateAxis(90, 'X');
		gpuScaleUniform(radius);
		Batch_draw(sphere);

		gpuTranslate3f(0.0f, length / radius, 0.0f);
		Batch_draw(sphere);
	}

	gpuPopMatrix();
	immUnbindProgram();
}

void draw_bounding_volume(Object *ob, char type, const unsigned char ob_wire_col[4])
{
	BoundBox  bb_local;
	BoundBox *bb = NULL;
	
	if (ob->type == OB_MESH) {
		bb = BKE_mesh_boundbox_get(ob);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		bb = BKE_curve_boundbox_get(ob);
	}
	else if (ob->type == OB_MBALL) {
		if (BKE_mball_is_basis(ob)) {
			bb = ob->bb;
		}
	}
	else if (ob->type == OB_ARMATURE) {
		bb = BKE_armature_boundbox_get(ob);
	}
	else if (ob->type == OB_LATTICE) {
		bb = BKE_lattice_boundbox_get(ob);
	}
	else {
		const float min[3] = {-1.0f, -1.0f, -1.0f}, max[3] = {1.0f, 1.0f, 1.0f};
		bb = &bb_local;
		BKE_boundbox_init_from_minmax(bb, min, max);
	}
	
	if (bb == NULL)
		return;

	if (ob->gameflag & OB_BOUNDS) { /* bounds need to be drawn around origin for game engine */

		if (type == OB_BOUND_BOX) {
			float vec[8][3], size[3];

			unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
			if (ob_wire_col) immUniformColor3ubv(ob_wire_col);

			BKE_boundbox_calc_size_aabb(bb, size);
			
			vec[0][0] = vec[1][0] = vec[2][0] = vec[3][0] = -size[0];
			vec[4][0] = vec[5][0] = vec[6][0] = vec[7][0] = +size[0];
			vec[0][1] = vec[1][1] = vec[4][1] = vec[5][1] = -size[1];
			vec[2][1] = vec[3][1] = vec[6][1] = vec[7][1] = +size[1];
			vec[0][2] = vec[3][2] = vec[4][2] = vec[7][2] = -size[2];
			vec[1][2] = vec[2][2] = vec[5][2] = vec[6][2] = +size[2];
			
			imm_draw_box(vec, false, pos);

			immUnbindProgram();
		}
		else {
			imm_draw_bb(bb, type, true, ob_wire_col);
		}
	}
	else {
		if (type == OB_BOUND_BOX) {
			unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
			if (ob_wire_col) immUniformColor3ubv(ob_wire_col);

			imm_draw_box(bb->vec, false, pos);

			immUnbindProgram();
		}
		else
			imm_draw_bb(bb, type, false, ob_wire_col);
	}

}

static void drawtexspace(Object *ob, const unsigned char ob_wire_col[3])
{
	float vec[8][3], loc[3], size[3];
	
	if (ob->type == OB_MESH) {
		BKE_mesh_texspace_get(ob->data, loc, NULL, size);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		BKE_curve_texspace_get(ob->data, loc, NULL, size);
	}
	else if (ob->type == OB_MBALL) {
		MetaBall *mb = ob->data;
		copy_v3_v3(size, mb->size);
		copy_v3_v3(loc, mb->loc);
	}
	else {
		return;
	}

	vec[0][0] = vec[1][0] = vec[2][0] = vec[3][0] = loc[0] - size[0];
	vec[4][0] = vec[5][0] = vec[6][0] = vec[7][0] = loc[0] + size[0];
	
	vec[0][1] = vec[1][1] = vec[4][1] = vec[5][1] = loc[1] - size[1];
	vec[2][1] = vec[3][1] = vec[6][1] = vec[7][1] = loc[1] + size[1];

	vec[0][2] = vec[3][2] = vec[4][2] = vec[7][2] = loc[2] - size[2];
	vec[1][2] = vec[2][2] = vec[5][2] = vec[6][2] = loc[2] + size[2];
	
	setlinestyle(2);

	unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);

	if (ob_wire_col) {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor3ubv(ob_wire_col);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_DEPTH_ONLY);
	}

	imm_draw_box(vec, false, pos);

	immUnbindProgram();

	setlinestyle(0);
}

/* draws wire outline */
static void draw_object_selected_outline(
        Scene *scene, SceneLayer *sl, View3D *v3d, ARegion *ar, Base *base,
        const unsigned char ob_wire_col[4])
{
	RegionView3D *rv3d = ar->regiondata;
	Object *ob = base->object;
	
	glDepthMask(GL_FALSE);
	
	if (ELEM(ob->type, OB_FONT, OB_CURVE, OB_SURF)) {
		bool has_faces = false;

#ifdef SEQUENCER_DAG_WORKAROUND
		ensure_curve_cache(scene, ob);
#endif

		DerivedMesh *dm = ob->derivedFinal;
		if (dm) {
			DM_update_materials(dm, ob);
		}

		if (dm) {
			has_faces = (dm->getNumPolys(dm) != 0);
		}
		else {
			has_faces = BKE_displist_has_faces(&ob->curve_cache->disp);
		}

		if (has_faces && ED_view3d_boundbox_clip(rv3d, ob->bb)) {
			glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
			if (dm) {
				draw_mesh_object_outline(v3d, ob, dm, ob_wire_col);
			}
			else {
				/* only draw 'solid' parts of the display list as wire. */
				drawDispListwire_ex(&ob->curve_cache->disp, (DL_INDEX3 | DL_INDEX4 | DL_SURF), ob_wire_col);
			}
		}
	}
	else if (ob->type == OB_MBALL) {
		if (BKE_mball_is_basis(ob)) {
			if ((base->flag_legacy & OB_FROMDUPLI) == 0) {
				glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
				drawDispListwire(&ob->curve_cache->disp, ob->type, ob_wire_col);
			}
		}
	}
	else if (ob->type == OB_ARMATURE) {
		if (!(ob->mode & OB_MODE_POSE && base == sl->basact)) {
			glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
			draw_armature(scene, sl, v3d, ar, base, OB_WIRE, 0, ob_wire_col, true);
		}
	}

	glDepthMask(GL_TRUE);
}

static void draw_wire_extra(Scene *scene, RegionView3D *rv3d, Object *ob, const unsigned char ob_wire_col[4])
{
	if (ELEM(ob->type, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL)) {
		unsigned char wire_edit_col[4];
		UI_GetThemeColor4ubv(TH_WIRE_EDIT, wire_edit_col);

		ED_view3d_polygon_offset(rv3d, 1.0);
		glDepthMask(GL_FALSE);  /* disable write in zbuffer, selected edge wires show better */
		glLineWidth(1.0f);

		if (ELEM(ob->type, OB_FONT, OB_CURVE, OB_SURF)) {
			if (ED_view3d_boundbox_clip(rv3d, ob->bb)) {

				if (ob->derivedFinal) {
					drawCurveDMWired(ob);
				}
				else {
					drawDispListwire(&ob->curve_cache->disp, ob->type, (scene->obedit == ob) ? wire_edit_col : ob_wire_col);
				}
			}
		}
		else if (ob->type == OB_MBALL) {
			if (BKE_mball_is_basis(ob)) {
				drawDispListwire(&ob->curve_cache->disp, ob->type, (scene->obedit == ob) ? wire_edit_col : ob_wire_col);
			}
		}

		glDepthMask(GL_TRUE);
		ED_view3d_polygon_offset(rv3d, 0.0);
	}
}

/* should be called in view space */
static void draw_hooks(Object *ob, unsigned int pos)
{
	for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData *) md;
			float vec[3];

			mul_v3_m4v3(vec, ob->obmat, hmd->cent);

			if (hmd->object) {
				setlinestyle(3);
				immBegin(GL_LINES, 2);
				immVertex3fv(pos, hmd->object->obmat[3]);
				immVertex3fv(pos, vec);
				immEnd();
				setlinestyle(0);
			}

			glPointSize(3.0f);
			immBegin(GL_POINTS, 1);
			immVertex3fv(pos, vec);
			immEnd();
		}
	}
}

static void draw_rigid_body_pivot(bRigidBodyJointConstraint *data,
                                  const short dflag, const unsigned char ob_wire_col[4])
{
	const char *axis_str[3] = {"px", "py", "pz"};
	float mat[4][4];

	unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	if (ob_wire_col) immUniformColor3ubv(ob_wire_col);

	eul_to_mat4(mat, &data->axX);
	glLineWidth(4.0f);
	setlinestyle(2);

	immBegin(GL_LINES, 6);
	for (int axis = 0; axis < 3; axis++) {
		float dir[3] = {0, 0, 0};
		float v[3];

		copy_v3_v3(v, &data->pivX);

		dir[axis] = 1.0f;
		mul_m4_v3(mat, dir);
		add_v3_v3(v, dir);
		immVertex3fv(pos, &data->pivX);
		immVertex3fv(pos, v);

		/* when const color is set wirecolor is NULL - we could get the current color but
		 * with selection and group instancing its not needed to draw the text */
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			view3d_cached_text_draw_add(v, axis_str[axis], 2, 0, V3D_CACHE_TEXT_ASCII, ob_wire_col);
		}
	}
	immEnd();

	setlinestyle(0);
	glLineWidth(1.0f);

	immUnbindProgram();
}

void draw_object_wire_color(Scene *scene, SceneLayer *sl, Base *base, unsigned char r_ob_wire_col[4])
{
	Object *ob = base->object;
	int colindex = 0;
	const bool is_edit = (ob->mode & OB_MODE_EDIT) != 0;
	/* confusing logic here, there are 2 methods of setting the color
	 * 'colortab[colindex]' and 'theme_id', colindex overrides theme_id.
	 *
	 * note: no theme yet for 'colindex' */
	int theme_id = is_edit ? TH_WIRE_EDIT : TH_WIRE;
	int theme_shade = 0;

	if ((scene->obedit == NULL) &&
	    (G.moving & G_TRANSFORM_OBJ) &&
	    ((base->flag & BASE_SELECTED) || (base->flag_legacy & BA_WAS_SEL)))
	{
		theme_id = TH_TRANSFORM;
	}
	else {
		/* Sets the 'colindex' */
		if (ID_IS_LINKED_DATABLOCK(ob)) {
			colindex = ((base->flag & BASE_SELECTED) || (base->flag_legacy & BA_WAS_SEL)) ? 2 : 1;
		}
		/* Sets the 'theme_id' or fallback to wire */
		else {
			if ((ob->flag & OB_FROMGROUP) != 0) {
				if ((base->flag & BASE_SELECTED) || (base->flag_legacy & BA_WAS_SEL)) {
					/* uses darker active color for non-active + selected */
					theme_id = TH_GROUP_ACTIVE;

					if (scene->basact != base) {
						theme_shade = -16;
					}
				}
				else {
					theme_id = TH_GROUP;
				}
			}
			else {
				if ((base->flag & BASE_SELECTED) || (base->flag_legacy & BA_WAS_SEL)) {
					theme_id = sl->basact == base ? TH_ACTIVE : TH_SELECT;
				}
				else {
					if (ob->type == OB_LAMP) theme_id = TH_LAMP;
					else if (ob->type == OB_SPEAKER) theme_id = TH_SPEAKER;
					else if (ob->type == OB_CAMERA) theme_id = TH_CAMERA;
					else if (ob->type == OB_EMPTY) theme_id = TH_EMPTY;
					/* fallback to TH_WIRE */
				}
			}
		}
	}

	/* finally set the color */
	if (colindex == 0) {
		if (theme_shade == 0) UI_GetThemeColor3ubv(theme_id, r_ob_wire_col);
		else                  UI_GetThemeColorShade3ubv(theme_id, theme_shade, r_ob_wire_col);
	}
	else {
		cpack_cpy_3ub(r_ob_wire_col, colortab[colindex]);
	}

	/* no reason to use this but some functions take col[4] */
	r_ob_wire_col[3] = 255;
}

static void draw_object_matcap_check(View3D *v3d, Object *ob)
{
	/* fixed rule, active object draws as matcap */
	BLI_assert((ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)) == 0);
	(void)ob;

	if (v3d->defmaterial == NULL) {
		extern Material defmaterial;

		v3d->defmaterial = MEM_mallocN(sizeof(Material), "matcap material");
		*(v3d->defmaterial) = defmaterial;
		BLI_listbase_clear(&v3d->defmaterial->gpumaterial);
		v3d->defmaterial->preview = NULL;
	}
	/* first time users */
	if (v3d->matcap_icon < ICON_MATCAP_01 ||
	    v3d->matcap_icon > ICON_MATCAP_24)
	{
		v3d->matcap_icon = ICON_MATCAP_01;
	}

	if (v3d->defmaterial->preview == NULL)
		v3d->defmaterial->preview = UI_icon_to_preview(v3d->matcap_icon);

	/* signal to all material checks, gets cleared below */
	v3d->flag2 |= V3D_SHOW_SOLID_MATCAP;
}

void draw_rigidbody_shape(Object *ob, const unsigned char ob_wire_col[4])
{
	BoundBox *bb = NULL;
	float size[3], vec[8][3];
	unsigned int pos;

	if (ob->type == OB_MESH) {
		bb = BKE_mesh_boundbox_get(ob);
	}

	if (bb == NULL)
		return;

	switch (ob->rigidbody_object->shape) {
		case RB_SHAPE_BOX:
			BKE_boundbox_calc_size_aabb(bb, size);
			
			pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
			if (ob_wire_col) immUniformColor3ubv(ob_wire_col);

			vec[0][0] = vec[1][0] = vec[2][0] = vec[3][0] = -size[0];
			vec[4][0] = vec[5][0] = vec[6][0] = vec[7][0] = +size[0];
			vec[0][1] = vec[1][1] = vec[4][1] = vec[5][1] = -size[1];
			vec[2][1] = vec[3][1] = vec[6][1] = vec[7][1] = +size[1];
			vec[0][2] = vec[3][2] = vec[4][2] = vec[7][2] = -size[2];
			vec[1][2] = vec[2][2] = vec[5][2] = vec[6][2] = +size[2];
			
			imm_draw_box(vec, false, pos);
			immUnbindProgram();
			break;
		case RB_SHAPE_SPHERE:
			imm_draw_bb(bb, OB_BOUND_SPHERE, true, ob_wire_col);
			break;
		case RB_SHAPE_CONE:
			imm_draw_bb(bb, OB_BOUND_CONE, true, ob_wire_col);
			break;
		case RB_SHAPE_CYLINDER:
			imm_draw_bb(bb, OB_BOUND_CYLINDER, true, ob_wire_col);
			break;
		case RB_SHAPE_CAPSULE:
			imm_draw_bb(bb, OB_BOUND_CAPSULE, true, ob_wire_col);
			break;
	}
}

/**
 * main object drawing function, draws in selection
 * \param dflag (draw flag) can be DRAW_PICKING and/or DRAW_CONSTCOLOR, DRAW_SCENESET
 */
void draw_object(Scene *scene, SceneLayer *sl, ARegion *ar, View3D *v3d, Base *base, const short dflag)
{
	ModifierData *md = NULL;
	Object *ob = base->object;
	Curve *cu;
	RegionView3D *rv3d = ar->regiondata;
	unsigned int col = 0;
	unsigned char _ob_wire_col[4];            /* dont initialize this */
	const unsigned char *ob_wire_col = NULL;  /* dont initialize this, use NULL crashes as a way to find invalid use */
	bool zbufoff = false, is_paint = false, empty_object = false;
	const bool is_obact = (ob == OBACT_NEW);
	const bool render_override = (v3d->flag2 & V3D_RENDER_OVERRIDE) != 0;
	const bool is_picking = (G.f & G_PICKSEL) != 0;
	const bool has_particles = (ob->particlesystem.first != NULL);
	bool skip_object = false;  /* Draw particles but not their emitter object. */
	SmokeModifierData *smd = NULL;

	if (ob != scene->obedit) {
		if (ob->restrictflag & OB_RESTRICT_VIEW)
			return;
		
		if (render_override) {
			if (ob->restrictflag & OB_RESTRICT_RENDER)
				return;
			
			if (!has_particles && (ob->transflag & (OB_DUPLI & ~OB_DUPLIFRAMES)))
				return;
		}
	}

	if (has_particles) {
		/* XXX particles are not safe for simultaneous threaded render */
		if (G.is_rendering) {
			return;
		}

		if (ob->mode == OB_MODE_OBJECT) {
			ParticleSystem *psys;

			skip_object = render_override;
			for (psys = ob->particlesystem.first; psys; psys = psys->next) {
				/* Once we have found a psys which renders its emitter object, we are done. */
				if (psys->part->draw & PART_DRAW_EMITTER) {
					skip_object = false;
					break;
				}
			}
		}
	}

	if (((base->flag_legacy & OB_FROMDUPLI) == 0) &&
	    (md = modifiers_findByType(ob, eModifierType_Smoke)) &&
	    (modifier_isEnabled(scene, md, eModifierMode_Realtime)))
	{
		smd = (SmokeModifierData *)md;

		if (smd->domain) {
			if (!v3d->transp && (dflag & DRAW_PICKING) == 0) {
				if (!v3d->xray && !(ob->dtx & OB_DRAWXRAY)) {
					/* object has already been drawn so skip drawing it */
					ED_view3d_after_add(&v3d->afterdraw_transp, base, dflag);
					return;
				}
				else if (v3d->xray) {
					/* object has already been drawn so skip drawing it */
					ED_view3d_after_add(&v3d->afterdraw_xraytransp, base, dflag);
					return;
				}
			}
		}
	}


	/* xray delay? */
	if ((dflag & DRAW_PICKING) == 0 && (base->flag_legacy & OB_FROMDUPLI) == 0 && (v3d->flag2 & V3D_RENDER_SHADOW) == 0) {
		/* don't do xray in particle mode, need the z-buffer */
		if (!(ob->mode & OB_MODE_PARTICLE_EDIT)) {
			/* xray and transp are set when it is drawing the 2nd/3rd pass */
			if (!v3d->xray && !v3d->transp && (ob->dtx & OB_DRAWXRAY) && !(ob->dtx & OB_DRAWTRANSP)) {
				ED_view3d_after_add(&v3d->afterdraw_xray, base, dflag);
				return;
			}

			/* allow transp option for empty images */
			if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE) {
				if (!v3d->xray && !v3d->transp && !(ob->dtx & OB_DRAWXRAY) && (ob->dtx & OB_DRAWTRANSP)) {
					ED_view3d_after_add(&v3d->afterdraw_transp, base, dflag);
					return;
				}
			}
		}
	}


	/* -------------------------------------------------------------------- */
	/* no return after this point, otherwise leaks */

	/* only once set now, will be removed too, should become a global standard */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	/* reset here to avoid having to call all over */
	glLineWidth(1.0f);

	view3d_cached_text_draw_begin();
	
	/* draw motion paths (in view space) */
	if (ob->mpath && !render_override) {
		bAnimVizSettings *avs = &ob->avs;
		
		/* setup drawing environment for paths */
		draw_motion_paths_init(v3d, ar);
		
		/* draw motion path for object */
		draw_motion_path_instance(scene, ob, NULL, avs, ob->mpath);
		
		/* cleanup after drawing */
		draw_motion_paths_cleanup(v3d);
	}

	/* multiply view with object matrix.
	 * local viewmat and persmat, to calculate projections */
	ED_view3d_init_mats_rv3d_gl(ob, rv3d);

	/* which wire color */
	if ((dflag & DRAW_CONSTCOLOR) == 0) {

		ED_view3d_project_base(ar, base);

		draw_object_wire_color(scene, sl, base, _ob_wire_col);
		ob_wire_col = _ob_wire_col;

		//glColor3ubv(ob_wire_col);
	}

	/* maximum drawtype */
	char dt = v3d->drawtype;
	if (dt == OB_RENDER) dt = v3d->prev_drawtype;
	dt = MIN2(dt, ob->dt);
	if (v3d->zbuf == 0 && dt > OB_WIRE) dt = OB_WIRE;
	short dtx = 0;


	/* faceselect exception: also draw solid when (dt == wire), except in editmode */
	if (is_obact) {
		if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)) {
			if (ob->type == OB_MESH) {
				if (dt < OB_SOLID) {
					zbufoff = true;
					dt = OB_SOLID;
				}

				if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
					dt = OB_PAINT;
				}

				is_paint = true;
				glEnable(GL_DEPTH_TEST);
			}
		}
	}

	/* matcap check - only when not painting color */
	if ((v3d->flag2 & V3D_SOLID_MATCAP) &&
	    (dt == OB_SOLID) &&
	    (is_paint == false && is_picking == false) &&
	    ((v3d->flag2 & V3D_RENDER_SHADOW) == 0))
	{
		draw_object_matcap_check(v3d, ob);
	}

	/* draw-extra supported for boundbox drawmode too */
	if (dt >= OB_BOUNDBOX) {
		dtx = ob->dtx;
		if (ob->mode & OB_MODE_EDIT) {
			/* the only 2 extra drawtypes alowed in editmode */
			dtx = dtx & (OB_DRAWWIRE | OB_TEXSPACE);
		}
	}

	if (!skip_object) {
		/* draw outline for selected objects, mesh does itself */
		if ((v3d->flag & V3D_SELECT_OUTLINE) && !render_override && ob->type != OB_MESH) {
			if (dt > OB_WIRE && (ob->mode & OB_MODE_EDIT) == 0 && (dflag & DRAW_SCENESET) == 0) {
				if (!(ob->dtx & OB_DRAWWIRE) && (base->flag & BASE_SELECTED) && !(dflag & (DRAW_PICKING | DRAW_CONSTCOLOR))) {
					draw_object_selected_outline(scene, sl, v3d, ar, base, ob_wire_col);
				}
			}
		}

		/* TODO Viewport: draw only for selection */
		if (!IS_VIEWPORT_LEGACY(v3d)) {
			if ((dflag & DRAW_PICKING) == 0) {
				if ((dt == OB_BOUNDBOX) || ELEM(ob->type, OB_EMPTY, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
					goto afterdraw;
				}
			}
		}

		switch (ob->type) {
			case OB_MESH:
				if (IS_VIEWPORT_LEGACY(v3d)) {
					empty_object = draw_mesh_object(scene, sl, ar, v3d, rv3d, base, dt, ob_wire_col, dflag);
				}
				else {
					empty_object = draw_mesh_object_new(scene, sl, ar, v3d, rv3d, base, dt, ob_wire_col, dflag);
				}
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					/* mesh draws wire itself */
					dtx &= ~OB_DRAWWIRE;
				}

				break;
			case OB_FONT:
				cu = ob->data;
				if (cu->editfont) {
					draw_editfont(scene, sl, v3d, rv3d, base, dt, dflag, ob_wire_col);
				}
				else if (dt == OB_BOUNDBOX) {
					if ((render_override && v3d->drawtype >= OB_WIRE) == 0) {
#ifdef SEQUENCER_DAG_WORKAROUND
						ensure_curve_cache(scene, base->object);
#endif
						draw_bounding_volume(ob, ob->boundtype, ob_wire_col);
					}
				}
				else if (ED_view3d_boundbox_clip(rv3d, ob->bb)) {
					empty_object = drawDispList(scene, sl, v3d, rv3d, base, dt, dflag, ob_wire_col);
				}

				break;
			case OB_CURVE:
			case OB_SURF:
				cu = ob->data;

				if (cu->editnurb) {
					ListBase *nurbs = BKE_curve_editNurbs_get(cu);
					draw_editnurb(scene, sl, v3d, rv3d, base, nurbs->first, dt, dflag, ob_wire_col);
				}
				else if (dt == OB_BOUNDBOX) {
					if ((render_override && (v3d->drawtype >= OB_WIRE)) == 0) {
#ifdef SEQUENCER_DAG_WORKAROUND
						ensure_curve_cache(scene, base->object);
#endif
						draw_bounding_volume(ob, ob->boundtype, ob_wire_col);
					}
				}
				else if (ED_view3d_boundbox_clip(rv3d, ob->bb)) {
					empty_object = drawDispList(scene, sl, v3d, rv3d, base, dt, dflag, ob_wire_col);
				}
				break;
			case OB_MBALL:
			{
				MetaBall *mb = ob->data;
				
				if (mb->editelems)
					drawmball(scene, sl, v3d, rv3d, base, dt, dflag, ob_wire_col);
				else if (dt == OB_BOUNDBOX) {
					if ((render_override && (v3d->drawtype >= OB_WIRE)) == 0) {
#ifdef SEQUENCER_DAG_WORKAROUND
						ensure_curve_cache(scene, base->object);
#endif
						draw_bounding_volume(ob, ob->boundtype, ob_wire_col);
					}
				}
				else
					empty_object = drawmball(scene, sl, v3d, rv3d, base, dt, dflag, ob_wire_col);
				break;
			}
			case OB_EMPTY:
				if (!render_override) {
					if (ob->empty_drawtype == OB_EMPTY_IMAGE) {
						draw_empty_image(ob, dflag, ob_wire_col, v3d->multiview_eye);
					}
					else {
						drawaxes(rv3d->viewmatob, ob->empty_drawsize, ob->empty_drawtype, ob_wire_col);
					}
				}
				break;
			case OB_LAMP:
				if (!render_override) {
					drawlamp(v3d, rv3d, base, dt, dflag, ob_wire_col, is_obact);
				}
				break;
			case OB_CAMERA:
				if (!render_override ||
				    (rv3d->persp == RV3D_CAMOB && v3d->camera == ob)) /* special exception for active camera */
				{
					drawcamera(scene, v3d, rv3d, base, dflag, ob_wire_col);
				}
				break;
			case OB_SPEAKER:
				if (!render_override)
					drawspeaker(ob_wire_col);
				break;
			case OB_LATTICE:
				if (!render_override) {
					/* Do not allow boundbox in edit nor pose mode! */
					if ((dt == OB_BOUNDBOX) && (ob->mode & OB_MODE_EDIT))
						dt = OB_WIRE;
					if (dt == OB_BOUNDBOX) {
						draw_bounding_volume(ob, ob->boundtype, ob_wire_col);
					}
					else {
#ifdef SEQUENCER_DAG_WORKAROUND
						ensure_curve_cache(scene, ob);
#endif
						drawlattice(v3d, ob, dflag, ob_wire_col);
					}
				}
				break;
			case OB_ARMATURE:
				if (!render_override) {
					/* Do not allow boundbox in edit nor pose mode! */
					if ((dt == OB_BOUNDBOX) && (ob->mode & (OB_MODE_EDIT | OB_MODE_POSE)))
						dt = OB_WIRE;
					if (dt == OB_BOUNDBOX) {
						draw_bounding_volume(ob, ob->boundtype, ob_wire_col);
					}
					else {
						unsigned char arm_col[4];
						glLineWidth(1.0f);

						if (ob_wire_col == NULL) {
							float fcol[4] = {0.0f, 0.0f, 0.0f, 1.0f};
							rgba_float_to_uchar(arm_col, fcol);
						}
						else
							copy_v4_v4_uchar(arm_col, ob_wire_col);

						empty_object = draw_armature(scene, sl, v3d, ar, base, dt, dflag, arm_col, false);
					}
				}
				break;
			default:
				if (!render_override) {
					drawaxes(rv3d->viewmatob, 1.0, OB_ARROWS, ob_wire_col);
				}
				break;
		}

		/* TODO Viewport: some elements are being drawn for object selection only */
afterdraw:

		if (!render_override) {
			if (ob->soft /*&& dflag & OB_SBMOTION*/) {
				float mrt[3][3], msc[3][3], mtr[3][3];
				SoftBody *sb = NULL;
				float tipw = 0.5f, tiph = 0.5f, drawsize = 4.0f;
				if ((sb = ob->soft)) {
					if (sb->solverflags & SBSO_ESTIMATEIPO) {

						gpuLoadMatrix3D(rv3d->viewmat);
						copy_m3_m3(msc, sb->lscale);
						copy_m3_m3(mrt, sb->lrot);
						mul_m3_m3m3(mtr, mrt, msc);
						ob_draw_RE_motion(sb->lcom, mtr, tipw, tiph, drawsize);
						gpuMultMatrix3D(ob->obmat);
					}
				}
			}

			if (ob->pd && ob->pd->forcefield) {
				draw_forcefield(ob, rv3d, dflag, ob_wire_col);
			}
		}
	}

	/* code for new particle system */
	if ((ob->particlesystem.first) &&
	    (ob != scene->obedit))
	{
		ParticleSystem *psys;

		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			/* for visibility, also while wpaint */
			if (col || (base->flag & BASE_SELECTED)) {
				cpack(0xFFFFFF);
			}
		}
		//glDepthMask(GL_FALSE);

		gpuLoadMatrix3D(rv3d->viewmat);
		
		view3d_cached_text_draw_begin();

		for (psys = ob->particlesystem.first; psys; psys = psys->next) {
			/* run this so that possible child particles get cached */
			if (ob->mode & OB_MODE_PARTICLE_EDIT && is_obact) {
				PTCacheEdit *edit = PE_create_current(scene, ob);
				if (edit && edit->psys == psys)
					draw_update_ptcache_edit(scene, sl, ob, edit);
			}

			draw_new_particle_system(scene, v3d, rv3d, base, psys, dt, dflag);
		}
		invert_m4_m4(ob->imat, ob->obmat);
		view3d_cached_text_draw_end(v3d, ar, 0, NULL);

		gpuMultMatrix3D(ob->obmat);
		
		//glDepthMask(GL_TRUE);
		if (col) cpack(col);
	}

	/* draw edit particles last so that they can draw over child particles */
	if ((dflag & DRAW_PICKING) == 0 &&
	    (!scene->obedit))
	{

		if (ob->mode & OB_MODE_PARTICLE_EDIT && is_obact) {
			PTCacheEdit *edit = PE_create_current(scene, ob);
			if (edit) {
				gpuLoadMatrix3D(rv3d->viewmat);
				draw_update_ptcache_edit(scene, sl, ob, edit);
				draw_ptcache_edit(scene, v3d, edit);
				gpuMultMatrix3D(ob->obmat);
			}
		}
	}

	/* draw code for smoke, only draw domains */
	if (smd && smd->domain) {
		SmokeDomainSettings *sds = smd->domain;
		const bool show_smoke = (CFRA >= sds->point_cache[0]->startframe);
		float viewnormal[3];

		gpuLoadMatrix3D(rv3d->viewmat);
		gpuMultMatrix3D(ob->obmat);

		if (!render_override) {
			BoundBox bb;
			float p0[3], p1[3];
			unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
			if (ob_wire_col) immUniformColor3ubv(ob_wire_col);

			/* draw max domain bounds */
			if ((sds->flags & MOD_SMOKE_ADAPTIVE_DOMAIN)) {
				VECSUBFAC(p0, sds->p0, sds->cell_size, sds->adapt_res);
				VECADDFAC(p1, sds->p1, sds->cell_size, sds->adapt_res);
				BKE_boundbox_init_from_minmax(&bb, p0, p1);
				imm_draw_box(bb.vec, false, pos);
			}

			/* draw a single voxel to hint the user about the resolution of the fluid */
			copy_v3_v3(p0, sds->p0);

			if (sds->flags & MOD_SMOKE_HIGHRES) {
				madd_v3_v3v3fl(p1, p0, sds->cell_size, 1.0f / (sds->amplify + 1));
			}
			else {
				add_v3_v3v3(p1, p0, sds->cell_size);
			}

			BKE_boundbox_init_from_minmax(&bb, p0, p1);
			imm_draw_box(bb.vec, false, pos);

			immUnbindProgram();
		}

		/* don't show smoke before simulation starts, this could be made an option in the future */
		if (sds->fluid && show_smoke) {
			float p0[3], p1[3];

			/* get view vector */
			invert_m4_m4(ob->imat, ob->obmat);
			mul_v3_mat3_m4v3(viewnormal, ob->imat, rv3d->viewinv[2]);
			normalize_v3(viewnormal);

			/* set dynamic boundaries to draw the volume
			 * also scale cube to global space to equalize volume slicing on all axes
			 *  (it's scaled back before drawing) */
			p0[0] = (sds->p0[0] + sds->cell_size[0] * sds->res_min[0] + sds->obj_shift_f[0]) * fabsf(ob->size[0]);
			p0[1] = (sds->p0[1] + sds->cell_size[1] * sds->res_min[1] + sds->obj_shift_f[1]) * fabsf(ob->size[1]);
			p0[2] = (sds->p0[2] + sds->cell_size[2] * sds->res_min[2] + sds->obj_shift_f[2]) * fabsf(ob->size[2]);
			p1[0] = (sds->p0[0] + sds->cell_size[0] * sds->res_max[0] + sds->obj_shift_f[0]) * fabsf(ob->size[0]);
			p1[1] = (sds->p0[1] + sds->cell_size[1] * sds->res_max[1] + sds->obj_shift_f[1]) * fabsf(ob->size[1]);
			p1[2] = (sds->p0[2] + sds->cell_size[2] * sds->res_max[2] + sds->obj_shift_f[2]) * fabsf(ob->size[2]);

			if (!sds->wt || !(sds->viewsettings & MOD_SMOKE_VIEW_SHOWBIG)) {
				sds->tex = NULL;
				GPU_create_smoke(smd, 0);
				draw_smoke_volume(sds, ob, p0, p1, viewnormal);
				GPU_free_smoke(smd);
			}
			else if (sds->wt && (sds->viewsettings & MOD_SMOKE_VIEW_SHOWBIG)) {
				sds->tex = NULL;
				GPU_create_smoke(smd, 1);
				draw_smoke_volume(sds, ob, p0, p1, viewnormal);
				GPU_free_smoke(smd);
			}

			/* smoke debug render */
			if (!render_override && sds->draw_velocity) {
				draw_smoke_velocity(sds, viewnormal);
			}
		}
	}

	if (!render_override) {
		bConstraint *con;

		for (con = ob->constraints.first; con; con = con->next) {
			if (con->type == CONSTRAINT_TYPE_RIGIDBODYJOINT) {
				bRigidBodyJointConstraint *data = (bRigidBodyJointConstraint *)con->data;
				if (data->flag & CONSTRAINT_DRAW_PIVOT)
					draw_rigid_body_pivot(data, dflag, ob_wire_col);
			}
		}

		if ((ob->gameflag & OB_BOUNDS) && (ob->mode == OB_MODE_OBJECT)) {
			if (ob->boundtype != ob->collision_boundtype || (dtx & OB_DRAWBOUNDOX) == 0) {
				setlinestyle(2);
				draw_bounding_volume(ob, ob->collision_boundtype, ob_wire_col);
				setlinestyle(0);
			}
		}
		if (ob->rigidbody_object) {
			draw_rigidbody_shape(ob, ob_wire_col);
		}

		/* draw extra: after normal draw because of makeDispList */
		if (dtx && (G.f & G_RENDER_OGL) == 0) {

			if (dtx & OB_AXIS) {
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					/* prevent random colors being used */
					drawaxes(rv3d->viewmatob, 1.0f, OB_ARROWS, ob_wire_col);
				}
				else {
					drawaxes(rv3d->viewmatob, 1.0f, OB_ARROWS, NULL);
				}
			}
			if (dtx & OB_DRAWBOUNDOX) {
				draw_bounding_volume(ob, ob->boundtype, ob_wire_col);
			}
			if (dtx & OB_TEXSPACE) {
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					/* prevent random colors being used */
					drawtexspace(ob, ob_wire_col);
				}
				else {
					drawtexspace(ob, NULL);
				}
			}
			if (dtx & OB_DRAWNAME) {
				/* patch for several 3d cards (IBM mostly) that crash on GL_SELECT with text drawing */
				/* but, we also don't draw names for sets or duplicators */
				if (dflag == 0) {
					const float zero[3] = {0, 0, 0};
					view3d_cached_text_draw_add(zero, ob->id.name + 2, strlen(ob->id.name + 2), 10, 0, ob_wire_col);
				}
			}
			if ((dtx & OB_DRAWWIRE) && dt >= OB_SOLID) {
				if ((dflag & DRAW_CONSTCOLOR) == 0) {
					draw_wire_extra(scene, rv3d, ob, ob_wire_col);
				}
			}
		}
	}

	if ((dt <= OB_SOLID) && !render_override) {
		if (((ob->gameflag & OB_DYNAMIC) &&
		     ((ob->gameflag & OB_BOUNDS) == 0)) ||

		    ((ob->gameflag & OB_BOUNDS) &&
		     (ob->collision_boundtype == OB_BOUND_SPHERE)))
		{
			float imat[4][4], vec[3] = {0.0f, 0.0f, 0.0f};

			unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

			invert_m4_m4(imat, rv3d->viewmatob);

			if ((dflag & DRAW_CONSTCOLOR) == 0) {
				/* prevent random colors being used */
				immUniformColor3ubv(ob_wire_col);
			}

			setlinestyle(2);
			imm_drawcircball(vec, ob->inertia, imat, pos);
			setlinestyle(0);

			immUnbindProgram();
		}
	}
	
	/* return warning, this is cached text draw */
	invert_m4_m4(ob->imat, ob->obmat);
	view3d_cached_text_draw_end(v3d, ar, 1, NULL);
	/* return warning, clear temp flag */
	v3d->flag2 &= ~V3D_SHOW_SOLID_MATCAP;
	
	gpuLoadMatrix3D(rv3d->viewmat);

	if (zbufoff) {
		glDisable(GL_DEPTH_TEST);
	}

	if ((base->flag_legacy & OB_FROMDUPLI) || render_override) {
		ED_view3d_clear_mats_rv3d(rv3d);
		return;
	}

	/* object centers, need to be drawn in viewmat space for speed, but OK for picking select */
	if (!is_obact || !(ob->mode & OB_MODE_ALL_PAINT)) {
		int do_draw_center = -1; /* defines below are zero or positive... */

		if (render_override) {
			/* don't draw */
		}
		else if (is_obact)
			do_draw_center = ACTIVE;
		else if (base->flag & BASE_SELECTED)
			do_draw_center = SELECT;
		else if (empty_object || (v3d->flag & V3D_DRAW_CENTERS))
			do_draw_center = DESELECT;

		if (do_draw_center != -1) {
			if (dflag & DRAW_PICKING) {
				/* draw a single point for opengl selection */
				if ((base->sx != IS_CLIPPED) &&
				    (U.obcenter_dia != 0.0))
				{
					unsigned pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
					immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_UNIFORM_COLOR);
					/* TODO: short term, use DEPTH_ONLY shader or set appropriate color */
					/* TODO: long term, solve picking & selection problem better */
					glPointSize(U.obcenter_dia);
					immBegin(GL_POINTS, 1);
					immVertex3fv(pos, ob->obmat[3]);
					immEnd();
					immUnbindProgram();
				}
			}
			else if ((dflag & DRAW_CONSTCOLOR) == 0) {
				/* we don't draw centers for duplicators and sets */
				if ((base->sx != IS_CLIPPED) &&
				    (U.obcenter_dia != 0.0) &&
				    !(G.f & G_RENDER_OGL))
				{
					/* check > 0 otherwise grease pencil can draw into the circle select which is annoying. */
					drawcentercircle(v3d, rv3d, ob->obmat[3], do_draw_center, ID_IS_LINKED_DATABLOCK(ob) || ob->id.us > 1);
				}
			}
		}
	}

	/* not for sets, duplicators or picking */
	if (dflag == 0 && (v3d->flag & V3D_HIDE_HELPLINES) == 0 && !render_override) {
		ListBase *list;
		RigidBodyCon *rbc = ob->rigidbody_constraint;
		
		unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor3ubv(ob_wire_col);

		/* draw hook center and offset line */
		if (ob != scene->obedit)
			draw_hooks(ob, pos);

		/* help lines and so */
		if (ob != scene->obedit && ob->parent && (ob->parent->lay & v3d->lay)) {
			setlinestyle(3);
			immBegin(GL_LINES, 2);
			immVertex3fv(pos, ob->obmat[3]);
			immVertex3fv(pos, ob->orig);
			immEnd();
			setlinestyle(0);
		}

		/* Drawing the constraint lines */
		if (ob->constraints.first) {
			bConstraint *curcon;
			bConstraintOb *cob;
			unsigned char col1[4], col2[4];
			
			list = &ob->constraints;
			
			UI_GetThemeColor3ubv(TH_GRID, col1);
			UI_make_axis_color(col1, col2, 'Z');
			immUniformColor3ubv(col2);
			
			cob = BKE_constraints_make_evalob(scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
			
			for (curcon = list->first; curcon; curcon = curcon->next) {
				if (ELEM(curcon->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_OBJECTSOLVER)) {
					/* special case for object solver and follow track constraints because they don't fill
					 * constraint targets properly (design limitation -- scene is needed for their target
					 * but it can't be accessed from get_targets callback) */

					Object *camob = NULL;

					if (curcon->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
						bFollowTrackConstraint *data = (bFollowTrackConstraint *)curcon->data;

						camob = data->camera ? data->camera : scene->camera;
					}
					else if (curcon->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
						bObjectSolverConstraint *data = (bObjectSolverConstraint *)curcon->data;

						camob = data->camera ? data->camera : scene->camera;
					}

					if (camob) {
						setlinestyle(3);
						immBegin(GL_LINES, 2);
						immVertex3fv(pos, camob->obmat[3]);
						immVertex3fv(pos, ob->obmat[3]);
						immEnd();
						setlinestyle(0);
					}
				}
				else {
					const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(curcon);

					if ((cti && cti->get_constraint_targets) && (curcon->flag & CONSTRAINT_EXPAND)) {
						ListBase targets = {NULL, NULL};
						bConstraintTarget *ct;

						cti->get_constraint_targets(curcon, &targets);

						for (ct = targets.first; ct; ct = ct->next) {
							/* calculate target's matrix */
							if (cti->get_target_matrix)
								cti->get_target_matrix(curcon, cob, ct, BKE_scene_frame_get(scene));
							else
								unit_m4(ct->matrix);

							setlinestyle(3);
							immBegin(GL_LINES, 2);
							immVertex3fv(pos, ct->matrix[3]);
							immVertex3fv(pos, ob->obmat[3]);
							immEnd();
							setlinestyle(0);
						}

						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(curcon, &targets, 1);
					}
				}
			}
			
			BKE_constraints_clear_evalob(cob);
		}
		/* draw rigid body constraint lines */
		if (rbc && (rbc->ob1 || rbc->ob2)) {
			immUniformThemeColor(TH_WIRE);

			setlinestyle(3);
			immBegin(GL_LINES, ((int)((bool)rbc->ob1) + (int)((bool)rbc->ob2)) * 2);
			if (rbc->ob1) {
				immVertex3fv(pos, ob->obmat[3]);
				immVertex3fv(pos, rbc->ob1->obmat[3]);
			}
			if (rbc->ob2) {
				immVertex3fv(pos, ob->obmat[3]);
				immVertex3fv(pos, rbc->ob2->obmat[3]);
			}
			immEnd();
			setlinestyle(0);
		}

		immUnbindProgram();
	}

	ED_view3d_clear_mats_rv3d(rv3d);
}


/**
 * Drawing for selection picking,
 * caller must have called 'GPU_select_load_id(base->selcode)' first.
 */
void draw_object_select(Scene *scene, SceneLayer *sl, ARegion *ar, View3D *v3d, Base *base, const short dflag)
{
	BLI_assert(dflag & DRAW_PICKING && dflag & DRAW_CONSTCOLOR);
	draw_object(scene, sl, ar, v3d, base, dflag);

	/* we draw duplicators for selection too */
	if ((base->object->transflag & OB_DUPLI)) {
		ListBase *lb;
		DupliObject *dob;
		Base tbase;

		tbase.flag_legacy = OB_FROMDUPLI;
		lb = object_duplilist(G.main->eval_ctx, scene, base->object);

		for (dob = lb->first; dob; dob = dob->next) {
			float omat[4][4];

			tbase.object = dob->ob;
			copy_m4_m4(omat, dob->ob->obmat);
			copy_m4_m4(dob->ob->obmat, dob->mat);

			/* extra service: draw the duplicator in drawtype of parent */
			/* MIN2 for the drawtype to allow bounding box objects in groups for lods */
			char dt = tbase.object->dt;    tbase.object->dt = MIN2(tbase.object->dt, base->object->dt);
			short dtx = tbase.object->dtx; tbase.object->dtx = base->object->dtx;

			draw_object(scene, sl, ar, v3d, &tbase, dflag);

			tbase.object->dt = dt;
			tbase.object->dtx = dtx;

			copy_m4_m4(dob->ob->obmat, omat);
		}
		free_object_duplilist(lb);
	}
}

/* ***************** BACKBUF SEL (BBS) ********* */

static void bbs_obmode_mesh_verts__mapFunc(void *userData, int index, const float co[3],
                                           const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	drawMVertOffset_userData *data = userData;
	MVert *mv = &data->mvert[index];

	if (!(mv->flag & ME_HIDE)) {
		int selcol;
		GPU_select_index_get(data->offset + index, &selcol);
		immAttrib3ubv(data->col, (unsigned char *)&selcol);
		immVertex3fv(data->pos, co);
	}
}

static void bbs_obmode_mesh_verts(Object *ob, DerivedMesh *dm, int offset)
{
	drawMVertOffset_userData data;
	Mesh *me = ob->data;
	MVert *mvert = me->mvert;
	data.mvert = mvert;
	data.offset = offset;

	VertexFormat *format = immVertexFormat();

	if (dm->getNumVerts(dm) == 0) return;

	data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	data.col = add_attrib(format, "color", GL_UNSIGNED_BYTE, 3, NORMALIZE_INT_TO_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

	immBeginAtMost(GL_POINTS, dm->getNumVerts(dm));
	dm->foreachMappedVert(dm, bbs_obmode_mesh_verts__mapFunc, &data, DM_FOREACH_NOP);
	immEnd();

	immUnbindProgram();
}

static void bbs_mesh_verts__mapFunc(void *userData, int index, const float co[3],
                                    const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	drawBMOffset_userData *data = userData;
	BMVert *eve = BM_vert_at_index(data->bm, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
		int selcol;
		GPU_select_index_get(data->offset + index, &selcol);
		immAttrib3ubv(data->col, (unsigned char *)&selcol);
		immVertex3fv(data->pos, co);
	}
}
static void bbs_mesh_verts(BMEditMesh *em, DerivedMesh *dm, int offset)
{
	drawBMOffset_userData data;
	data.bm = em->bm;
	data.offset = offset;
	VertexFormat *format = immVertexFormat();
	data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	data.col = add_attrib(format, "color", GL_UNSIGNED_BYTE, 3, NORMALIZE_INT_TO_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

	immBeginAtMost(GL_POINTS, em->bm->totvert);
	dm->foreachMappedVert(dm, bbs_mesh_verts__mapFunc, &data, DM_FOREACH_NOP);
	immEnd();

	immUnbindProgram();
}

static DMDrawOption bbs_mesh_wire__setDrawOptions(void *userData, int index)
{
	drawBMOffset_userData *data = userData;
	BMEdge *eed = BM_edge_at_index(data->bm, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
		GPU_select_index_set(data->offset + index);
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}
static void bbs_mesh_wire(BMEditMesh *em, DerivedMesh *dm, int offset)
{
	drawBMOffset_userData data;
	data.bm = em->bm;
	data.offset = offset;

	dm->drawMappedEdges(dm, bbs_mesh_wire__setDrawOptions, &data);
}

/**
 * dont set #GPU_framebuffer_index_set. just use to mask other
 */
static DMDrawOption bbs_mesh_mask__setSolidDrawOptions(void *userData, int index)
{
	BMFace *efa = BM_face_at_index(userData, index);
	
	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

static DMDrawOption bbs_mesh_solid__setSolidDrawOptions(void *userData, int index)
{
	BMFace *efa = BM_face_at_index(userData, index);

	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		GPU_select_index_set(index + 1);
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

static void bbs_mesh_solid__drawCenter(void *userData, int index, const float cent[3], const float UNUSED(no[3]))
{
	drawBMOffset_userData *data = (drawBMOffset_userData *)userData;
	BMFace *efa = BM_face_at_index(userData, index);

	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		int selcol;
		GPU_select_index_get(index + 1, &selcol);
		immAttrib3ubv(data->col, (unsigned char *)&selcol);
		immVertex3fv(data->pos, cent);
	}
}

/* two options, facecolors or black */
static void bbs_mesh_solid_EM(BMEditMesh *em, Scene *scene, View3D *v3d,
                              Object *ob, DerivedMesh *dm, bool use_faceselect)
{
	cpack(0);

	if (use_faceselect) {
		dm->drawMappedFaces(dm, bbs_mesh_solid__setSolidDrawOptions, NULL, NULL, em->bm, DM_DRAW_SKIP_HIDDEN | DM_DRAW_SELECT_USE_EDITMODE);

		if (check_ob_drawface_dot(scene, v3d, ob->dt)) {
			drawBMOffset_userData data; /* don't use offset */
			data.bm = em->bm;
			VertexFormat *format = immVertexFormat();
			data.pos = add_attrib(format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
			data.col = add_attrib(format, "color", GL_UNSIGNED_BYTE, 3, NORMALIZE_INT_TO_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

			glPointSize(UI_GetThemeValuef(TH_FACEDOT_SIZE));

			immBeginAtMost(GL_POINTS, em->bm->totface);
			dm->foreachMappedFaceCenter(dm, bbs_mesh_solid__drawCenter, &data, DM_FOREACH_NOP);
			immEnd();

			immUnbindProgram();
		}

	}
	else {
		dm->drawMappedFaces(dm, bbs_mesh_mask__setSolidDrawOptions, NULL, NULL, em->bm, DM_DRAW_SKIP_SELECT | DM_DRAW_SKIP_HIDDEN | DM_DRAW_SELECT_USE_EDITMODE);
	}
}

static DMDrawOption bbs_mesh_solid__setDrawOpts(void *UNUSED(userData), int index)
{
	GPU_select_index_set(index + 1);
	return DM_DRAW_OPTION_NORMAL;
}

static DMDrawOption bbs_mesh_solid_hide__setDrawOpts(void *userData, int index)
{
	Mesh *me = userData;

	if (!(me->mpoly[index].flag & ME_HIDE)) {
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

/* must have called GPU_framebuffer_index_set beforehand */
static DMDrawOption bbs_mesh_solid_hide2__setDrawOpts(void *userData, int index)
{
	Mesh *me = userData;

	if (!(me->mpoly[index].flag & ME_HIDE)) {
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

static void bbs_mesh_solid_verts(Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, scene->customdata_mask);

	DM_update_materials(dm, ob);

	dm->drawMappedFaces(dm, bbs_mesh_solid_hide2__setDrawOpts, GPU_object_material_bind, NULL, me, DM_DRAW_SKIP_HIDDEN);

	GPU_object_material_unbind();

	bbs_obmode_mesh_verts(ob, dm, 1);
	bm_vertoffs = me->totvert + 1;
	dm->release(dm);
}

static void bbs_mesh_solid_faces(Scene *scene, Object *ob)
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, scene->customdata_mask);
	Mesh *me = ob->data;
	
	DM_update_materials(dm, ob);

	if ((me->editflag & ME_EDIT_PAINT_FACE_SEL))
		dm->drawMappedFaces(dm, bbs_mesh_solid_hide__setDrawOpts, NULL, NULL, me, DM_DRAW_SKIP_HIDDEN);
	else
		dm->drawMappedFaces(dm, bbs_mesh_solid__setDrawOpts, NULL, NULL, me, 0);

	dm->release(dm);
}

void draw_object_backbufsel(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob)
{
	ToolSettings *ts = scene->toolsettings;

	gpuMultMatrix3D(ob->obmat);

	glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	switch (ob->type) {
		case OB_MESH:
			if (ob->mode & OB_MODE_EDIT) {
				Mesh *me = ob->data;
				BMEditMesh *em = me->edit_btmesh;

				DerivedMesh *dm = editbmesh_get_derived_cage(scene, ob, em, CD_MASK_BAREMESH);

				BM_mesh_elem_table_ensure(em->bm, BM_VERT | BM_EDGE | BM_FACE);

				DM_update_materials(dm, ob);

				bbs_mesh_solid_EM(em, scene, v3d, ob, dm, (ts->selectmode & SCE_SELECT_FACE) != 0);
				if (ts->selectmode & SCE_SELECT_FACE)
					bm_solidoffs = 1 + em->bm->totface;
				else
					bm_solidoffs = 1;

				ED_view3d_polygon_offset(rv3d, 1.0);

				/* we draw edges always, for loop (select) tools */
				bbs_mesh_wire(em, dm, bm_solidoffs);
				bm_wireoffs = bm_solidoffs + em->bm->totedge;

				/* we draw verts if vert select mode or if in transform (for snap). */
				if ((ts->selectmode & SCE_SELECT_VERTEX) || (G.moving & G_TRANSFORM_EDIT)) {
					bbs_mesh_verts(em, dm, bm_wireoffs);
					bm_vertoffs = bm_wireoffs + em->bm->totvert;
				}
				else {
					bm_vertoffs = bm_wireoffs;
				}

				ED_view3d_polygon_offset(rv3d, 0.0);

				dm->release(dm);
			}
			else {
				Mesh *me = ob->data;
				if ((me->editflag & ME_EDIT_PAINT_VERT_SEL) &&
				    /* currently vertex select only supports weight paint */
				    (ob->mode & OB_MODE_WEIGHT_PAINT))
				{
					bbs_mesh_solid_verts(scene, ob);
				}
				else {
					bbs_mesh_solid_faces(scene, ob);
				}
			}
			break;
		case OB_CURVE:
		case OB_SURF:
			break;
	}

	gpuLoadMatrix3D(rv3d->viewmat);
}


/* ************* draw object instances for bones, for example ****************** */
/*               assumes all matrices/etc set OK */

/* helper function for drawing object instances - meshes */
static void draw_object_mesh_instance(Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d,
                                      Object *ob, const short dt, int outline, const unsigned char ob_wire_col[4])
{
	Mesh *me = ob->data;
	DerivedMesh *dm = NULL, *edm = NULL;
	
	if (ob->mode & OB_MODE_EDIT) {
		edm = editbmesh_get_derived_base(ob, me->edit_btmesh, CD_MASK_BAREMESH);
		DM_update_materials(edm, ob);
	}
	else {
		dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
		DM_update_materials(dm, ob);
	}

	if (dt <= OB_WIRE) {
		glColor4ubv(ob_wire_col);
		if (dm)
			dm->drawEdges(dm, 1, 0);
		else if (edm)
			edm->drawEdges(edm, 1, 0);
	}
	else {
		if (outline)
			draw_mesh_object_outline(v3d, ob, dm ? dm : edm, ob_wire_col);

		if (dm) {
			bool glsl = draw_glsl_material(scene, sl, ob, v3d, dt);
			GPU_begin_object_materials(v3d, rv3d, scene, sl, ob, glsl, NULL);
		}
		
		glFrontFace((ob->transflag & OB_NEG_SCALE) ? GL_CW : GL_CCW);
		
		if (dm) {
			dm->drawFacesSolid(dm, NULL, 0, GPU_object_material_bind);
			GPU_end_object_materials();
		}
		else if (edm)
			edm->drawMappedFaces(edm, NULL, GPU_object_material_bind, NULL, NULL, DM_DRAW_NEED_NORMALS);

		GPU_object_material_unbind();
	}

	if (edm) edm->release(edm);
	if (dm) dm->release(dm);
}

void draw_object_instance(Scene *scene, SceneLayer *sl, View3D *v3d, RegionView3D *rv3d, Object *ob, const char dt, int outline, const float wire_col[4])
{
	if (ob == NULL)
		return;

	unsigned char bcol[4];
	rgba_float_to_uchar(bcol, wire_col);

	switch (ob->type) {
		case OB_MESH:
			draw_object_mesh_instance(scene, sl, v3d, rv3d, ob, dt, outline, bcol);
			break;
		case OB_EMPTY:
			if (ob->empty_drawtype == OB_EMPTY_IMAGE) {
				draw_empty_image(ob, 0, bcol, v3d->multiview_eye);
			}
			else {
				drawaxes(rv3d->viewmatob, ob->empty_drawsize, ob->empty_drawtype, bcol);
			}
			break;
	}
}
