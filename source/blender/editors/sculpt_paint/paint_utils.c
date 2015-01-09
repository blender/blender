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
#include "DNA_material_types.h"

#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BLF_translation.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RE_render_ext.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "BLI_sys_types.h"
#include "ED_mesh.h" /* for face mask functions */

#include "WM_api.h"
#include "WM_types.h"

#include "paint_intern.h"

/* Convert the object-space axis-aligned bounding box (expressed as
 * its minimum and maximum corners) into a screen-space rectangle,
 * returns zero if the result is empty */
bool paint_convert_bb_to_rect(rcti *rect,
                              const float bb_min[3],
                              const float bb_max[3],
                              const ARegion *ar,
                              RegionView3D *rv3d,
                              Object *ob)
{
	float projection_mat[4][4];
	int i, j, k;

	BLI_rcti_init_minmax(rect);

	/* return zero if the bounding box has non-positive volume */
	if (bb_min[0] > bb_max[0] || bb_min[1] > bb_max[1] || bb_min[2] > bb_max[2])
		return 0;

	ED_view3d_ob_project_mat_get(rv3d, ob, projection_mat);

	for (i = 0; i < 2; ++i) {
		for (j = 0; j < 2; ++j) {
			for (k = 0; k < 2; ++k) {
				float vec[3], proj[2];
				int proj_i[2];
				vec[0] = i ? bb_min[0] : bb_max[0];
				vec[1] = j ? bb_min[1] : bb_max[1];
				vec[2] = k ? bb_min[2] : bb_max[2];
				/* convert corner to screen space */
				ED_view3d_project_float_v2_m4(ar, vec, proj, projection_mat);
				/* expand 2D rectangle */

				/* we could project directly to int? */
				proj_i[0] = proj[0];
				proj_i[1] = proj[1];

				BLI_rcti_do_minmax_v(rect, proj_i);
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

	ED_view3d_clipping_calc(&bb, planes, &mats, &rect);
	negate_m4(planes);
}

float paint_calc_object_space_radius(ViewContext *vc, const float center[3],
                                     float pixel_radius)
{
	Object *ob = vc->obact;
	float delta[3], scale, loc[3];
	const float mval_f[2] = {pixel_radius, 0.0f};
	float zfac;

	mul_v3_m4v3(loc, ob->obmat, center);

	zfac = ED_view3d_calc_zfac(vc->rv3d, loc, NULL);
	ED_view3d_win_to_delta(vc->ar, mval_f, delta, zfac);

	scale = fabsf(mat4_to_scale(ob->obmat));
	scale = (scale == 0.0f) ? 1.0f : scale;

	return len_v3(delta) / scale;
}

float paint_get_tex_pixel(MTex *mtex, float u, float v, struct ImagePool *pool, int thread)
{
	float intensity, rgba[4];
	float co[3] = {u, v, 0.0f};

	externtex(mtex, co, &intensity,
	          rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool);

	return intensity;
}

void paint_get_tex_pixel_col(MTex *mtex, float u, float v, float rgba[4], struct ImagePool *pool, int thread, bool convert_to_linear, struct ColorSpace *colorspace)
{
	float co[3] = {u, v, 0.0f};
	int hasrgb;
	float intensity;

	hasrgb = externtex(mtex, co, &intensity,
	                   rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool);
	if (!hasrgb) {
		rgba[0] = intensity;
		rgba[1] = intensity;
		rgba[2] = intensity;
		rgba[3] = 1.0f;
	}

	if (convert_to_linear)
		IMB_colormanagement_colorspace_to_scene_linear_v3(rgba, colorspace);

	linearrgb_to_srgb_v3_v3(rgba, rgba);

	CLAMP(rgba[0], 0.0f, 1.0f);
	CLAMP(rgba[1], 0.0f, 1.0f);
	CLAMP(rgba[2], 0.0f, 1.0f);
	CLAMP(rgba[3], 0.0f, 1.0f);
}

void paint_stroke_operator_properties(wmOperatorType *ot)
{
	static EnumPropertyItem stroke_mode_items[] = {
		{BRUSH_STROKE_NORMAL, "NORMAL", 0, "Normal", "Apply brush normally"},
		{BRUSH_STROKE_INVERT, "INVERT", 0, "Invert", "Invert action of brush for duration of stroke"},
		{BRUSH_STROKE_SMOOTH, "SMOOTH", 0, "Smooth", "Switch brush to smooth mode for duration of stroke"},
		{0}
	};

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");

	RNA_def_enum(ot->srna, "mode", stroke_mode_items, BRUSH_STROKE_NORMAL, 
	             "Stroke Mode",
	             "Action taken when a paint stroke is made");
	
}

/* 3D Paint */

static void imapaint_project(float matrix[4][4], const float co[3], float pco[4])
{
	copy_v3_v3(pco, co);
	pco[3] = 1.0f;

	mul_m4_v4(matrix, pco);
}

static void imapaint_tri_weights(float matrix[4][4], GLint view[4],
                                 const float v1[3], const float v2[3], const float v3[3],
                                 const float co[2], float w[3])
{
	float pv1[4], pv2[4], pv3[4], h[3], divw;
	float wmat[3][3], invwmat[3][3];

	/* compute barycentric coordinates */

	/* project the verts */
	imapaint_project(matrix, v1, pv1);
	imapaint_project(matrix, v2, pv2);
	imapaint_project(matrix, v3, pv3);

	/* do inverse view mapping, see gluProject man page */
	h[0] = (co[0] - view[0]) * 2.0f / view[2] - 1.0f;
	h[1] = (co[1] - view[1]) * 2.0f / view[3] - 1.0f;
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
static void imapaint_pick_uv(Scene *scene, Object *ob, unsigned int faceindex, const int xy[2], float uv[2])
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	MTFace *tf_base, *tf;
	Material *ma;
	TexPaintSlot *slot;
	int numfaces = dm->getNumTessFaces(dm), a, findex;
	float p[2], w[3], absw, minabsw;
	MFace mf;
	MVert mv[4];
	float matrix[4][4], proj[4][4];
	GLint view[4];
	ImagePaintMode mode = scene->toolsettings->imapaint.mode;

	/* compute barycentric coordinates */

	/* double lookup */
	const int *index_mf_to_mpoly = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	const int *index_mp_to_orig  = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	if (index_mf_to_mpoly == NULL) {
		index_mp_to_orig = NULL;
	}

	/* get the needed opengl matrices */
	glGetIntegerv(GL_VIEWPORT, view);
	glGetFloatv(GL_MODELVIEW_MATRIX,  (float *)matrix);
	glGetFloatv(GL_PROJECTION_MATRIX, (float *)proj);
	view[0] = view[1] = 0;
	mul_m4_m4m4(matrix, matrix, ob->obmat);
	mul_m4_m4m4(matrix, proj, matrix);

	minabsw = 1e10;
	uv[0] = uv[1] = 0.0;

	/* test all faces in the derivedmesh with the original index of the picked face */
	for (a = 0; a < numfaces; a++) {
		findex = index_mf_to_mpoly ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, a) : a;

		if (findex == faceindex) {
			dm->getTessFace(dm, a, &mf);

			dm->getVert(dm, mf.v1, &mv[0]);
			dm->getVert(dm, mf.v2, &mv[1]);
			dm->getVert(dm, mf.v3, &mv[2]);
			if (mf.v4)
				dm->getVert(dm, mf.v4, &mv[3]);

			if (mode == IMAGEPAINT_MODE_MATERIAL) {
				ma = dm->mat[mf.mat_nr];
				slot = &ma->texpaintslot[ma->paint_active_slot];

				if (!(slot && slot->uvname && (tf_base = CustomData_get_layer_named(&dm->faceData, CD_MTFACE, slot->uvname))))
					tf_base = CustomData_get_layer(&dm->faceData, CD_MTFACE);

				tf = &tf_base[a];
			}
			else {
				tf_base = CustomData_get_layer(&dm->faceData, CD_MTFACE);
				tf = &tf_base[a];
			}

			p[0] = xy[0];
			p[1] = xy[1];

			if (mf.v4) {
				/* the triangle with the largest absolute values is the one
				 * with the most negative weights */
				imapaint_tri_weights(matrix, view, mv[0].co, mv[1].co, mv[3].co, p, w);
				absw = fabsf(w[0]) + fabsf(w[1]) + fabsf(w[2]);
				if (absw < minabsw) {
					uv[0] = tf->uv[0][0] * w[0] + tf->uv[1][0] * w[1] + tf->uv[3][0] * w[2];
					uv[1] = tf->uv[0][1] * w[0] + tf->uv[1][1] * w[1] + tf->uv[3][1] * w[2];
					minabsw = absw;
				}

				imapaint_tri_weights(matrix, view, mv[1].co, mv[2].co, mv[3].co, p, w);
				absw = fabsf(w[0]) + fabsf(w[1]) + fabsf(w[2]);
				if (absw < minabsw) {
					uv[0] = tf->uv[1][0] * w[0] + tf->uv[2][0] * w[1] + tf->uv[3][0] * w[2];
					uv[1] = tf->uv[1][1] * w[0] + tf->uv[2][1] * w[1] + tf->uv[3][1] * w[2];
					minabsw = absw;
				}
			}
			else {
				imapaint_tri_weights(matrix, view, mv[0].co, mv[1].co, mv[2].co, p, w);
				absw = fabsf(w[0]) + fabsf(w[1]) + fabsf(w[2]);
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
static int imapaint_pick_face(ViewContext *vc, const int mval[2], unsigned int *r_index, unsigned int totface)
{
	if (totface == 0)
		return 0;

	/* sample only on the exact position */
	*r_index = view3d_sample_backbuf(vc, mval[0], mval[1]);

	if ((*r_index) == 0 || (*r_index) > (unsigned int)totface) {
		return 0;
	}

	(*r_index)--;

	return 1;
}


static Image *imapaint_face_image(Object *ob, Mesh *me, int face_index)
{
	Image *ima;
	MPoly *mp = me->mpoly + face_index;
	Material *ma = give_current_material(ob, mp->mat_nr + 1);;
	ima = ma && ma->texpaintslot ? ma->texpaintslot[ma->paint_active_slot].ima : NULL;

	return ima;
}

/* Uses symm to selectively flip any axis of a coordinate. */
void flip_v3_v3(float out[3], const float in[3], const char symm)
{
	if (symm & PAINT_SYMM_X)
		out[0] = -in[0];
	else
		out[0] = in[0];
	if (symm & PAINT_SYMM_Y)
		out[1] = -in[1];
	else
		out[1] = in[1];
	if (symm & PAINT_SYMM_Z)
		out[2] = -in[2];
	else
		out[2] = in[2];
}

/* used for both 3d view and image window */
void paint_sample_color(bContext *C, ARegion *ar, int x, int y, bool texpaint_proj, bool use_palette)
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = BKE_paint_get_active_from_context(C);
	Palette *palette = BKE_paint_palette(paint);
	PaletteColor *color;
	Brush *br = BKE_paint_brush(BKE_paint_get_active_from_context(C));
	unsigned int col;
	const unsigned char *cp;

	CLAMP(x, 0, ar->winx);
	CLAMP(y, 0, ar->winy);
	
	if (use_palette) {
		if (!palette) {
			palette = BKE_palette_add(CTX_data_main(C), "Palette");
			BKE_paint_palette_set(paint, palette);
		}

		color = BKE_palette_color_add(palette);
	}


	if (CTX_wm_view3d(C) && texpaint_proj) {
		/* first try getting a colour directly from the mesh faces if possible */
		Object *ob = OBACT;
		bool sample_success = false;
		ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;
		bool use_material = (imapaint->mode == IMAGEPAINT_MODE_MATERIAL);

		if (ob) {
			Mesh *me = (Mesh *)ob->data;
			DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);

			ViewContext vc;
			const int mval[2] = {x, y};
			unsigned int faceindex;
			unsigned int totface = me->totface;
			MTFace *dm_mtface = dm->getTessFaceDataArray(dm, CD_MTFACE);

			if (dm_mtface) {
				view3d_set_viewcontext(C, &vc);

				view3d_operator_needs_opengl(C);

				if (imapaint_pick_face(&vc, mval, &faceindex, totface)) {
					Image *image;
					
					if (use_material) 
						image = imapaint_face_image(ob, me, faceindex);
					else
						image = imapaint->canvas;
					
					if (image) {
						ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);
						if (ibuf && ibuf->rect) {
							float uv[2];
							float u, v;
							imapaint_pick_uv(scene, ob, faceindex, mval, uv);
							sample_success = true;
							
							u = fmodf(uv[0], 1.0f);
							v = fmodf(uv[1], 1.0f);
							
							if (u < 0.0f) u += 1.0f;
							if (v < 0.0f) v += 1.0f;
							
							u = u * ibuf->x - 0.5f;
							v = v * ibuf->y - 0.5f;
							
							if (ibuf->rect_float) {
								float rgba_f[4];
								bilinear_interpolation_color_wrap(ibuf, NULL, rgba_f, u, v);
								straight_to_premul_v4(rgba_f);
								if (use_palette) {
									linearrgb_to_srgb_v3_v3(color->rgb, rgba_f);
								}
								else {
									linearrgb_to_srgb_v3_v3(rgba_f, rgba_f);
									BKE_brush_color_set(scene, br, rgba_f);
								}
							}
							else {
								unsigned char rgba[4];
								bilinear_interpolation_color_wrap(ibuf, rgba, NULL, u, v);
								if (use_palette) {
									rgb_uchar_to_float(color->rgb, rgba);
								}
								else {
									float rgba_f[3];
									rgb_uchar_to_float(rgba_f, rgba);
									BKE_brush_color_set(scene, br, rgba_f);
								}
							}
						}
					
						BKE_image_release_ibuf(image, ibuf, NULL);
					}
				}
			}
			dm->release(dm);
		}

		if (!sample_success) {
			glReadBuffer(GL_FRONT);
			glReadPixels(x + ar->winrct.xmin, y + ar->winrct.ymin, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &col);
			glReadBuffer(GL_BACK);
		}
		else
			return;
	}
	else {
		glReadBuffer(GL_FRONT);
		glReadPixels(x + ar->winrct.xmin, y + ar->winrct.ymin, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &col);
		glReadBuffer(GL_BACK);
	}
	cp = (unsigned char *)&col;
	
	if (use_palette) {
		rgb_uchar_to_float(color->rgb, cp);
	}
	else {
		float rgba_f[3];
		rgb_uchar_to_float(rgba_f, cp);
		BKE_brush_color_set(scene, br, rgba_f);
	}
}

static int brush_curve_preset_exec(bContext *C, wmOperator *op)
{
	Brush *br = BKE_paint_brush(BKE_paint_get_active_from_context(C));

	if (br) {
		Scene *scene = CTX_data_scene(C);
		BKE_brush_curve_preset(br, RNA_enum_get(op->ptr, "shape"));
		BKE_paint_invalidate_cursor_overlay(scene, br->curve);
	}

	return OPERATOR_FINISHED;
}

static int brush_curve_preset_poll(bContext *C)
{
	Brush *br = BKE_paint_brush(BKE_paint_get_active_from_context(C));

	return br && br->curve;
}

void BRUSH_OT_curve_preset(wmOperatorType *ot)
{
	PropertyRNA *prop;
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

	prop = RNA_def_enum(ot->srna, "shape", prop_shape_items, CURVE_PRESET_SMOOTH, "Mode", "");
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
}


/* face-select ops */
static int paint_select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	paintface_select_linked(C, CTX_data_active_object(C), NULL, true);
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

static int paint_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool select = !RNA_boolean_get(op->ptr, "deselect");
	view3d_operator_needs_opengl(C);
	paintface_select_linked(C, CTX_data_active_object(C), event->mval, select);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked_pick(wmOperatorType *ot)
{
	ot->name = "Select Linked Pick";
	ot->description = "Select linked faces under the cursor";
	ot->idname = "PAINT_OT_face_select_linked_pick";

	ot->invoke = paint_select_linked_pick_invoke;
	ot->poll = facemask_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items");
}


static int face_select_all_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	paintface_deselect_all_visible(ob, RNA_enum_get(op->ptr, "action"), true);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}


void PAINT_OT_face_select_all(wmOperatorType *ot)
{
	ot->name = "(De)select All";
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
	paintvert_deselect_all_visible(ob, RNA_enum_get(op->ptr, "action"), true);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}


void PAINT_OT_vert_select_all(wmOperatorType *ot)
{
	ot->name = "(De)select All";
	ot->description = "Change selection for all vertices";
	ot->idname = "PAINT_OT_vert_select_all";

	ot->exec = vert_select_all_exec;
	ot->poll = vert_paint_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}


static int vert_select_ungrouped_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;

	if (BLI_listbase_is_empty(&ob->defbase) || (me->dvert == NULL)) {
		BKE_report(op->reports, RPT_ERROR, "No weights/vertex groups on object");
		return OPERATOR_CANCELLED;
	}

	paintvert_select_ungrouped(ob, RNA_boolean_get(op->ptr, "extend"), true);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_ungrouped(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Ungrouped";
	ot->idname = "PAINT_OT_vert_select_ungrouped";
	ot->description = "Select vertices without a group";

	/* api callbacks */
	ot->exec = vert_select_ungrouped_exec;
	ot->poll = vert_paint_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

static int face_select_hide_exec(bContext *C, wmOperator *op)
{
	const bool unselected = RNA_boolean_get(op->ptr, "unselected");
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
