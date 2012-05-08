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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_utils.c
 *  \ingroup edsculpt
 */

#include <math.h>
#include <stdlib.h>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_paint.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BIF_gl.h"
/* TODO: remove once projectf goes away */
#include "BIF_glutil.h"

#include "RE_shader_ext.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "BLO_sys_types.h"
#include "ED_mesh.h" /* for face mask functions */

#include "WM_api.h"
#include "WM_types.h"

#include "paint_intern.h"

/* Convert the object-space axis-aligned bounding box (expressed as
 * its minimum and maximum corners) into a screen-space rectangle,
 * returns zero if the result is empty */
int paint_convert_bb_to_rect(rcti *rect,
                             const float bb_min[3],
                             const float bb_max[3],
                             const ARegion *ar,
                             RegionView3D *rv3d,
                             Object *ob)
{
	float projection_mat[4][4];
	int i, j, k;

	rect->xmin = rect->ymin = INT_MAX;
	rect->xmax = rect->ymax = INT_MIN;

	/* return zero if the bounding box has non-positive volume */
	if (bb_min[0] > bb_max[0] || bb_min[1] > bb_max[1] || bb_min[2] > bb_max[2])
		return 0;

	ED_view3d_ob_project_mat_get(rv3d, ob, projection_mat);

	for (i = 0; i < 2; ++i) {
		for (j = 0; j < 2; ++j) {
			for (k = 0; k < 2; ++k) {
				float vec[3], proj[2];
				vec[0] = i ? bb_min[0] : bb_max[0];
				vec[1] = j ? bb_min[1] : bb_max[1];
				vec[2] = k ? bb_min[2] : bb_max[2];
				/* convert corner to screen space */
				ED_view3d_project_float_v2(ar, vec, proj, projection_mat);
				/* expand 2D rectangle */
				rect->xmin = MIN2(rect->xmin, proj[0]);
				rect->xmax = MAX2(rect->xmax, proj[0]);
				rect->ymin = MIN2(rect->ymin, proj[1]);
				rect->ymax = MAX2(rect->ymax, proj[1]);
			}
		}
	}

	/* return false if the rectangle has non-positive area */
	return rect->xmin < rect->xmax && rect->ymin < rect->ymax;
}

/* Get four planes in object-space that describe the projection of
 * screen_rect from screen into object-space (essentially converting a
 * 2D screens-space bounding box into four 3D planes) */
void paint_calc_redraw_planes(float planes[4][4],
                              const ARegion *ar,
                              RegionView3D *rv3d,
                              Object *ob,
                              const rcti *screen_rect)
{
	BoundBox bb;
	bglMats mats;
	rcti rect;

	memset(&bb, 0, sizeof(BoundBox));
	view3d_get_transformation(ar, rv3d, ob, &mats);

	/* use some extra space just in case */
	rect = *screen_rect;
	rect.xmin -= 2;
	rect.xmax += 2;
	rect.ymin -= 2;
	rect.ymax += 2;

	ED_view3d_calc_clipping(&bb, planes, &mats, &rect);
	mul_m4_fl(planes, -1.0f);
}

/* convert a point in model coordinates to 2D screen coordinates */
/* TODO: can be deleted once all calls are replaced with
 * view3d_project_float() */
void projectf(bglMats *mats, const float v[3], float p[2])
{
	double ux, uy, uz;

	gluProject(v[0], v[1], v[2], mats->modelview, mats->projection,
	           (GLint *)mats->viewport, &ux, &uy, &uz);
	p[0] = ux;
	p[1] = uy;
}

float paint_calc_object_space_radius(ViewContext *vc, const float center[3],
                                     float pixel_radius)
{
	Object *ob = vc->obact;
	float delta[3], scale, loc[3];
	float mval_f[2];

	mul_v3_m4v3(loc, ob->obmat, center);

	initgrabz(vc->rv3d, loc[0], loc[1], loc[2]);

	mval_f[0] = pixel_radius;
	mval_f[1] = 0.0f;
	ED_view3d_win_to_delta(vc->ar, mval_f, delta);

	scale = fabsf(mat4_to_scale(ob->obmat));
	scale = (scale == 0.0f) ? 1.0f : scale;

	return len_v3(delta) / scale;
}

float paint_get_tex_pixel(Brush *br, float u, float v)
{
	TexResult texres;
	float co[3];
	int hasrgb;

	co[0] = u;
	co[1] = v;
	co[2] = 0;

	memset(&texres, 0, sizeof(TexResult));
	hasrgb = multitex_ext(br->mtex.tex, co, NULL, NULL, 0, &texres);

	if (hasrgb & TEX_RGB)
		texres.tin = (0.35f * texres.tr + 0.45f * texres.tg + 0.2f * texres.tb) * texres.ta;

	return texres.tin;
}

/* 3D Paint */

static void imapaint_project(Object *ob, float model[][4], float proj[][4], const float co[3], float pco[4])
{
	copy_v3_v3(pco, co);
	pco[3] = 1.0f;

	mul_m4_v3(ob->obmat, pco);
	mul_m4_v3(model, pco);
	mul_m4_v4(proj, pco);
}

static void imapaint_tri_weights(Object *ob,
                                 const float v1[3], const float v2[3], const float v3[3],
                                 const float co[3], float w[3])
{
	float pv1[4], pv2[4], pv3[4], h[3], divw;
	float model[4][4], proj[4][4], wmat[3][3], invwmat[3][3];
	GLint view[4];

	/* compute barycentric coordinates */

	/* get the needed opengl matrices */
	glGetIntegerv(GL_VIEWPORT, view);
	glGetFloatv(GL_MODELVIEW_MATRIX,  (float *)model);
	glGetFloatv(GL_PROJECTION_MATRIX, (float *)proj);
	view[0] = view[1] = 0;

	/* project the verts */
	imapaint_project(ob, model, proj, v1, pv1);
	imapaint_project(ob, model, proj, v2, pv2);
	imapaint_project(ob, model, proj, v3, pv3);

	/* do inverse view mapping, see gluProject man page */
	h[0] = (co[0] - view[0]) * 2.0f / view[2] - 1;
	h[1] = (co[1] - view[1]) * 2.0f / view[3] - 1;
	h[2] = 1.0f;

	/* solve for (w1,w2,w3)/perspdiv in:
	 * h * perspdiv = Project * Model * (w1 * v1 + w2 * v2 + w3 * v3) */

	wmat[0][0] = pv1[0];  wmat[1][0] = pv2[0];  wmat[2][0] = pv3[0];
	wmat[0][1] = pv1[1];  wmat[1][1] = pv2[1];  wmat[2][1] = pv3[1];
	wmat[0][2] = pv1[3];  wmat[1][2] = pv2[3];  wmat[2][2] = pv3[3];

	invert_m3_m3(invwmat, wmat);
	mul_m3_v3(invwmat, h);

	copy_v3_v3(w, h);

	/* w is still divided by perspdiv, make it sum to one */
	divw = w[0] + w[1] + w[2];
	if (divw != 0.0f) {
		mul_v3_fl(w, 1.0f / divw);
	}
}

/* compute uv coordinates of mouse in face */
void imapaint_pick_uv(Scene *scene, Object *ob, unsigned int faceindex, const int xy[2], float uv[2])
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	const int *index = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	MTFace *tface = dm->getTessFaceDataArray(dm, CD_MTFACE), *tf;
	int numfaces = dm->getNumTessFaces(dm), a, findex;
	float p[2], w[3], absw, minabsw;
	MFace mf;
	MVert mv[4];

	minabsw = 1e10;
	uv[0] = uv[1] = 0.0;

	/* test all faces in the derivedmesh with the original index of the picked face */
	for (a = 0; a < numfaces; a++) {
		findex = index ? index[a] : a;

		if (findex == faceindex) {
			dm->getTessFace(dm, a, &mf);

			dm->getVert(dm, mf.v1, &mv[0]);
			dm->getVert(dm, mf.v2, &mv[1]);
			dm->getVert(dm, mf.v3, &mv[2]);
			if (mf.v4)
				dm->getVert(dm, mf.v4, &mv[3]);

			tf = &tface[a];

			p[0] = xy[0];
			p[1] = xy[1];

			if (mf.v4) {
				/* the triangle with the largest absolute values is the one
				 * with the most negative weights */
				imapaint_tri_weights(ob, mv[0].co, mv[1].co, mv[3].co, p, w);
				absw = fabs(w[0]) + fabs(w[1]) + fabs(w[2]);
				if (absw < minabsw) {
					uv[0] = tf->uv[0][0] * w[0] + tf->uv[1][0] * w[1] + tf->uv[3][0] * w[2];
					uv[1] = tf->uv[0][1] * w[0] + tf->uv[1][1] * w[1] + tf->uv[3][1] * w[2];
					minabsw = absw;
				}

				imapaint_tri_weights(ob, mv[1].co, mv[2].co, mv[3].co, p, w);
				absw = fabs(w[0]) + fabs(w[1]) + fabs(w[2]);
				if (absw < minabsw) {
					uv[0] = tf->uv[1][0] * w[0] + tf->uv[2][0] * w[1] + tf->uv[3][0] * w[2];
					uv[1] = tf->uv[1][1] * w[0] + tf->uv[2][1] * w[1] + tf->uv[3][1] * w[2];
					minabsw = absw;
				}
			}
			else {
				imapaint_tri_weights(ob, mv[0].co, mv[1].co, mv[2].co, p, w);
				absw = fabs(w[0]) + fabs(w[1]) + fabs(w[2]);
				if (absw < minabsw) {
					uv[0] = tf->uv[0][0] * w[0] + tf->uv[1][0] * w[1] + tf->uv[2][0] * w[2];
					uv[1] = tf->uv[0][1] * w[0] + tf->uv[1][1] * w[1] + tf->uv[2][1] * w[2];
					minabsw = absw;
				}
			}
		}
	}

	dm->release(dm);
}

/* returns 0 if not found, otherwise 1 */
int imapaint_pick_face(ViewContext *vc, const int mval[2], unsigned int *index, unsigned int totface)
{
	if (totface == 0)
		return 0;

	/* sample only on the exact position */
	*index = view3d_sample_backbuf(vc, mval[0], mval[1]);

	if ((*index) <= 0 || (*index) > (unsigned int)totface) {
		return 0;
	}

	(*index)--;
	
	return 1;
}

/* used for both 3d view and image window */
void paint_sample_color(Scene *scene, ARegion *ar, int x, int y)    /* frontbuf */
{
	Brush *br = paint_brush(paint_get_active(scene));
	unsigned int col;
	char *cp;

	CLAMP(x, 0, ar->winx);
	CLAMP(y, 0, ar->winy);
	
	glReadBuffer(GL_FRONT);
	glReadPixels(x + ar->winrct.xmin, y + ar->winrct.ymin, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &col);
	glReadBuffer(GL_BACK);

	cp = (char *)&col;
	
	if (br) {
		br->rgb[0] = cp[0] / 255.0f;
		br->rgb[1] = cp[1] / 255.0f;
		br->rgb[2] = cp[2] / 255.0f;
	}
}

static int brush_curve_preset_exec(bContext *C, wmOperator *op)
{
	Brush *br = paint_brush(paint_get_active(CTX_data_scene(C)));
	brush_curve_preset(br, RNA_enum_get(op->ptr, "shape"));

	return OPERATOR_FINISHED;
}

static int brush_curve_preset_poll(bContext *C)
{
	Brush *br = paint_brush(paint_get_active(CTX_data_scene(C)));

	return br && br->curve;
}

void BRUSH_OT_curve_preset(wmOperatorType *ot)
{
	static EnumPropertyItem prop_shape_items[] = {
		{CURVE_PRESET_SHARP, "SHARP", 0, "Sharp", ""},
		{CURVE_PRESET_SMOOTH, "SMOOTH", 0, "Smooth", ""},
		{CURVE_PRESET_MAX, "MAX", 0, "Max", ""},
		{CURVE_PRESET_LINE, "LINE", 0, "Line", ""},
		{CURVE_PRESET_ROUND, "ROUND", 0, "Round", ""},
		{CURVE_PRESET_ROOT, "ROOT", 0, "Root", ""},
		{0, NULL, 0, NULL, NULL}};

	ot->name = "Preset";
	ot->description = "Set brush shape";
	ot->idname = "BRUSH_OT_curve_preset";

	ot->exec = brush_curve_preset_exec;
	ot->poll = brush_curve_preset_poll;

	RNA_def_enum(ot->srna, "shape", prop_shape_items, CURVE_PRESET_SMOOTH, "Mode", "");
}


/* face-select ops */
static int paint_select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	paintface_select_linked(C, CTX_data_active_object(C), NULL, 2);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked(wmOperatorType *ot)
{
	ot->name = "Select Linked";
	ot->description = "Select linked faces";
	ot->idname = "PAINT_OT_face_select_linked";

	ot->exec = paint_select_linked_exec;
	ot->poll = facemask_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int paint_select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	int mode = RNA_boolean_get(op->ptr, "extend") ? 1 : 0;
	paintface_select_linked(C, CTX_data_active_object(C), event->mval, mode);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked_pick(wmOperatorType *ot)
{
	ot->name = "Select Linked Pick";
	ot->description = "Select linked faces";
	ot->idname = "PAINT_OT_face_select_linked_pick";

	ot->invoke = paint_select_linked_pick_invoke;
	ot->poll = facemask_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the existing selection");
}


static int face_select_all_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	paintface_deselect_all_visible(ob, RNA_enum_get(op->ptr, "action"), TRUE);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}


void PAINT_OT_face_select_all(wmOperatorType *ot)
{
	ot->name = "Face Selection";
	ot->description = "Change selection for all faces";
	ot->idname = "PAINT_OT_face_select_all";

	ot->exec = face_select_all_exec;
	ot->poll = facemask_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}


static int vert_select_all_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	paintvert_deselect_all_visible(ob, RNA_enum_get(op->ptr, "action"), TRUE);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}


void PAINT_OT_vert_select_all(wmOperatorType *ot)
{
	ot->name = "Vertex Selection";
	ot->description = "Change selection for all vertices";
	ot->idname = "PAINT_OT_vert_select_all";

	ot->exec = vert_select_all_exec;
	ot->poll = vert_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

static int vert_select_inverse_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);
	paintvert_deselect_all_visible(ob, SEL_INVERT, TRUE);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_inverse(wmOperatorType *ot)
{
	ot->name = "Vertex Select Invert";
	ot->description = "Invert selection of vertices";
	ot->idname = "PAINT_OT_vert_select_inverse";

	ot->exec = vert_select_inverse_exec;
	ot->poll = vert_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
static int face_select_inverse_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);
	paintface_deselect_all_visible(ob, SEL_INVERT, TRUE);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}


void PAINT_OT_face_select_inverse(wmOperatorType *ot)
{
	ot->name = "Face Select Invert";
	ot->description = "Invert selection of faces";
	ot->idname = "PAINT_OT_face_select_inverse";

	ot->exec = face_select_inverse_exec;
	ot->poll = facemask_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_select_hide_exec(bContext *C, wmOperator *op)
{
	const int unselected = RNA_boolean_get(op->ptr, "unselected");
	Object *ob = CTX_data_active_object(C);
	paintface_hide(ob, unselected);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_hide(wmOperatorType *ot)
{
	ot->name = "Face Select Hide";
	ot->description = "Hide selected faces";
	ot->idname = "PAINT_OT_face_select_hide";

	ot->exec = face_select_hide_exec;
	ot->poll = facemask_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected objects");
}

static int face_select_reveal_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);
	paintface_reveal(ob);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_reveal(wmOperatorType *ot)
{
	ot->name = "Face Select Reveal";
	ot->description = "Reveal hidden faces";
	ot->idname = "PAINT_OT_face_select_reveal";

	ot->exec = face_select_reveal_exec;
	ot->poll = facemask_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected objects");
}
