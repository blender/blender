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
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/uvedit/uvedit_draw.c
 *  \ingroup eduv
 */


#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_buffer.h"
#include "BLI_bitmap.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_material.h"
#include "BKE_layer.h"

#include "BKE_scene.h"

#include "BIF_glutil.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "UI_resources.h"
#include "UI_interface.h"
#include "UI_view2d.h"

#include "uvedit_intern.h"

static void draw_uvs_lineloop_bmfaces(BMesh *bm, const int cd_loop_uv_offset, const uint shdr_pos);

void ED_image_draw_cursor(ARegion *ar, const float cursor[2])
{
	float zoom[2], x_fac, y_fac;

	UI_view2d_scale_get_inverse(&ar->v2d, &zoom[0], &zoom[1]);

	mul_v2_fl(zoom, 256.0f * UI_DPI_FAC);
	x_fac = zoom[0];
	y_fac = zoom[1];

	GPU_line_width(1.0f);

	GPU_matrix_translate_2fv(cursor);

	const uint shdr_pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

	float viewport_size[4];
	GPU_viewport_size_get_f(viewport_size);
	immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

	immUniform1i("colors_len", 2);  /* "advanced" mode */
	immUniformArray4fv("colors", (float *)(float[][4]){{1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, 2);
	immUniform1f("dash_width", 8.0f);

	immBegin(GPU_PRIM_LINES, 8);

	immVertex2f(shdr_pos, -0.05f * x_fac, 0.0f);
	immVertex2f(shdr_pos, 0.0f, 0.05f * y_fac);

	immVertex2f(shdr_pos, 0.0f, 0.05f * y_fac);
	immVertex2f(shdr_pos, 0.05f * x_fac, 0.0f);

	immVertex2f(shdr_pos, 0.05f * x_fac, 0.0f);
	immVertex2f(shdr_pos, 0.0f, -0.05f * y_fac);

	immVertex2f(shdr_pos, 0.0f, -0.05f * y_fac);
	immVertex2f(shdr_pos, -0.05f * x_fac, 0.0f);

	immEnd();

	immUniformArray4fv("colors", (float *)(float[][4]){{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, 2);
	immUniform1f("dash_width", 2.0f);

	immBegin(GPU_PRIM_LINES, 8);

	immVertex2f(shdr_pos, -0.020f * x_fac, 0.0f);
	immVertex2f(shdr_pos, -0.1f * x_fac, 0.0f);

	immVertex2f(shdr_pos, 0.1f * x_fac, 0.0f);
	immVertex2f(shdr_pos, 0.020f * x_fac, 0.0f);

	immVertex2f(shdr_pos, 0.0f, -0.020f * y_fac);
	immVertex2f(shdr_pos, 0.0f, -0.1f * y_fac);

	immVertex2f(shdr_pos, 0.0f, 0.1f * y_fac);
	immVertex2f(shdr_pos, 0.0f, 0.020f * y_fac);

	immEnd();

	immUnbindProgram();

	GPU_matrix_translate_2f(-cursor[0], -cursor[1]);
}

static int draw_uvs_face_check(Scene *scene)
{
	ToolSettings *ts = scene->toolsettings;

	/* checks if we are selecting only faces */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode == SCE_SELECT_FACE)
			return 2;
		else if (ts->selectmode & SCE_SELECT_FACE)
			return 1;
		else
			return 0;
	}
	else
		return (ts->uv_selectmode == UV_SELECT_FACE);
}

static void draw_uvs_shadow(Object *obedit)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	/* draws the mesh when painting */
	immUniformThemeColor(TH_UV_SHADOW);

	draw_uvs_lineloop_bmfaces(bm, cd_loop_uv_offset, pos);

	immUnbindProgram();
}

static void draw_uvs_stretch(SpaceImage *sima, Scene *scene, Object *obedit, BMEditMesh *em, const BMFace *efa_act)
{
	BMesh *bm = em->bm;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	Image *ima = sima->image;
	float aspx, aspy, col[4];
	int i;

	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	BLI_buffer_declare_static(vec2f, tf_uv_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_buffer_declare_static(vec2f, tf_uvorig_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);

	ED_space_image_get_uv_aspect(sima, &aspx, &aspy);

	switch (sima->dt_uvstretch) {
		case SI_UVDT_STRETCH_AREA:
		{
			float totarea = 0.0f, totuvarea = 0.0f, areadiff, uvarea, area;

			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				const int efa_len = efa->len;
				float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_reinit_data(&tf_uv_buf,     vec2f, efa_len);
				float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_reinit_data(&tf_uvorig_buf, vec2f, efa_len);

				BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					copy_v2_v2(tf_uvorig[i], luv->uv);
				}

				uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

				totarea += BM_face_calc_area(efa);
				totuvarea += area_poly_v2(tf_uv, efa->len);

				if (uvedit_face_visible_test(scene, obedit, ima, efa)) {
					BM_elem_flag_enable(efa, BM_ELEM_TAG);
				}
				else {
					if (efa == efa_act) {
						efa_act = NULL;
					}
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
				}
			}

			uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

			if (totarea < FLT_EPSILON || totuvarea < FLT_EPSILON) {
				col[0] = 1.0;
				col[1] = col[2] = 0.0;

				immUniformColor3fv(col);

				BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
						immBegin(GPU_PRIM_TRI_FAN, efa->len);

						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							immVertex2fv(pos, luv->uv);
						}

						immEnd();
					}
				}
			}
			else {
				BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
						const int efa_len = efa->len;
						float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_reinit_data(&tf_uv_buf,     vec2f, efa_len);
						float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_reinit_data(&tf_uvorig_buf, vec2f, efa_len);

						area = BM_face_calc_area(efa) / totarea;

						BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							copy_v2_v2(tf_uvorig[i], luv->uv);
						}

						uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

						uvarea = area_poly_v2(tf_uv, efa->len) / totuvarea;

						if (area < FLT_EPSILON || uvarea < FLT_EPSILON)
							areadiff = 1.0f;
						else if (area > uvarea)
							areadiff = 1.0f - (uvarea / area);
						else
							areadiff = 1.0f - (area / uvarea);

						BKE_defvert_weight_to_rgb(col, areadiff);
						immUniformColor3fv(col);

						/* TODO: use editmesh tessface */
						immBegin(GPU_PRIM_TRI_FAN, efa->len);

						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							immVertex2fv(pos, luv->uv);
						}

						immEnd();
					}
				}
			}

			immUnbindProgram();

			break;
		}
		case SI_UVDT_STRETCH_ANGLE:
		{
			float a;

			BLI_buffer_declare_static(float, uvang_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
			BLI_buffer_declare_static(float, ang_buf,   BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
			BLI_buffer_declare_static(vec3f, av_buf,  BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
			BLI_buffer_declare_static(vec2f, auv_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);

			col[3] = 0.5f; /* hard coded alpha, not that nice */

			GPUVertFormat *format = immVertexFormat();
			uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (uvedit_face_visible_test(scene, obedit, ima, efa)) {
					const int efa_len = efa->len;
					float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_reinit_data(&tf_uv_buf,     vec2f, efa_len);
					float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_reinit_data(&tf_uvorig_buf, vec2f, efa_len);
					float *uvang = BLI_buffer_reinit_data(&uvang_buf, float, efa_len);
					float *ang   = BLI_buffer_reinit_data(&ang_buf,   float, efa_len);
					float (*av)[3]  = (float (*)[3])BLI_buffer_reinit_data(&av_buf, vec3f, efa_len);
					float (*auv)[2] = (float (*)[2])BLI_buffer_reinit_data(&auv_buf, vec2f, efa_len);
					int j;

					BM_elem_flag_enable(efa, BM_ELEM_TAG);

					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						copy_v2_v2(tf_uvorig[i], luv->uv);
					}

					uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa_len);

					j = efa_len - 1;
					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						sub_v2_v2v2(auv[i], tf_uv[j], tf_uv[i]); normalize_v2(auv[i]);
						sub_v3_v3v3(av[i], l->prev->v->co, l->v->co); normalize_v3(av[i]);
						j = i;
					}

					for (i = 0; i < efa_len; i++) {
#if 0
						/* Simple but slow, better reuse normalized vectors
						 * (Not ported to bmesh, copied for reference) */
						uvang1 = RAD2DEG(angle_v2v2v2(tf_uv[3], tf_uv[0], tf_uv[1]));
						ang1 = RAD2DEG(angle_v3v3v3(efa->v4->co, efa->v1->co, efa->v2->co));
#endif
						uvang[i] = angle_normalized_v2v2(auv[i], auv[(i + 1) % efa_len]);
						ang[i] = angle_normalized_v3v3(av[i], av[(i + 1) % efa_len]);
					}

					/* TODO: use editmesh tessface */
					immBegin(GPU_PRIM_TRI_FAN, efa->len);
					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						a = fabsf(uvang[i] - ang[i]) / (float)M_PI;
						BKE_defvert_weight_to_rgb(col, 1.0f - pow2f(1.0f - a));
						immAttrib3fv(color, col);
						immVertex2fv(pos, luv->uv);
					}
					immEnd();
				}
				else {
					if (efa == efa_act)
						efa_act = NULL;
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
				}
			}

			immUnbindProgram();

			BLI_buffer_free(&uvang_buf);
			BLI_buffer_free(&ang_buf);
			BLI_buffer_free(&av_buf);
			BLI_buffer_free(&auv_buf);

			break;
		}
	}

	BLI_buffer_free(&tf_uv_buf);
	BLI_buffer_free(&tf_uvorig_buf);
}

static void draw_uvs_lineloop_bmfaces(BMesh *bm, const int cd_loop_uv_offset, const uint shdr_pos)
{
	BMIter iter, liter;
	BMFace *efa;
	BMLoop *l;
	MLoopUV *luv;

	/* For more efficiency first transfer the entire buffer to vram. */
	GPUBatch *loop_batch = immBeginBatchAtMost(GPU_PRIM_LINE_LOOP, bm->totloop);

	BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
		if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
			continue;

		BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			immVertex2fv(shdr_pos, luv->uv);
		}
	}
	immEnd();

	/* Then draw each face contour separately. */
	GPU_batch_program_use_begin(loop_batch);
	unsigned int index = 0;
	BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
		if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
			continue;

		GPU_batch_draw_range_ex(loop_batch, index, efa->len, false);
		index += efa->len;
	}
	GPU_batch_program_use_end(loop_batch);
	GPU_batch_discard(loop_batch);
}

static void draw_uvs_lineloop_mpoly(Mesh *me, MPoly *mpoly, unsigned int pos)
{
	MLoopUV *mloopuv;
	int i;

	immBegin(GPU_PRIM_LINE_LOOP, mpoly->totloop);

	mloopuv = &me->mloopuv[mpoly->loopstart];
	for (i = mpoly->totloop; i != 0; i--, mloopuv++) {
		immVertex2fv(pos, mloopuv->uv);
	}

	immEnd();
}

static void draw_uvs_other_mesh(Object *ob, const Image *curimage,
                                const int other_uv_filter, unsigned int pos)
{
	Mesh *me = ob->data;
	MPoly *mpoly = me->mpoly;
	int a;
	BLI_bitmap *mat_test_array;
	bool ok = false;
	int totcol = 0;

	if (me->mloopuv == NULL) {
		return;
	}

	if (curimage && ob->totcol == 0) {
		return;
	}

	totcol = max_ii(ob->totcol, 1);
	mat_test_array = BLI_BITMAP_NEW_ALLOCA(totcol);

	for (a = 0; a < totcol; a++) {
		Image *image;

		/* if no materials, assume a default material with no image */
		if (ob->totcol)
			ED_object_get_active_image(ob, a + 1, &image, NULL, NULL, NULL);
		else
			image = NULL;

		if (image == curimage) {
			BLI_BITMAP_ENABLE(mat_test_array, a);
			ok = true;
		}
	}

	if (ok == false) {
		return;
	}

	for (a = me->totpoly; a != 0; a--, mpoly++) {
		if (other_uv_filter == SI_FILTER_ALL) {
			/* Nothing to compare, all UV faces are visible. */
		}
		else if (other_uv_filter == SI_FILTER_SAME_IMAGE) {
			const int mat_nr = mpoly->mat_nr;
			if ((mat_nr >= totcol) ||
			    (BLI_BITMAP_TEST(mat_test_array, mat_nr)) == 0)
			{
				continue;
			}
		}

		draw_uvs_lineloop_mpoly(me, mpoly, pos);
	}
}

static void draw_uvs_other(ViewLayer *view_layer, Object *obedit, const Image *curimage,
                           const int other_uv_filter)
{
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	immUniformThemeColor(TH_UV_OTHERS);

	for (Base *base = view_layer->object_bases.first; base; base = base->next) {
		if (((base->flag & BASE_SELECTED) != 0) &&
		    ((base->flag & BASE_VISIBLE) != 0))
		{
			Object *ob = base->object;
			if ((ob->type == OB_MESH) && (ob != obedit) && ((Mesh *)ob->data)->mloopuv) {
				draw_uvs_other_mesh(ob, curimage, other_uv_filter, pos);
			}
		}
	}
	immUnbindProgram();
}

static void draw_uvs_texpaint(SpaceImage *sima, Scene *scene, ViewLayer *view_layer, Object *ob)
{
	Image *curimage = ED_space_image(sima);
	Mesh *me = ob->data;
	Material *ma;

	if (sima->flag & SI_DRAW_OTHER) {
		draw_uvs_other(view_layer, ob, curimage, sima->other_uv_filter);
	}

	ma = give_current_material(ob, ob->actcol);

	if (me->mloopuv) {
		MPoly *mpoly = me->mpoly;
		MLoopUV *mloopuv, *mloopuv_base;
		int a, b;
		if (!(ma && ma->texpaintslot && ma->texpaintslot[ma->paint_active_slot].uvname &&
		      (mloopuv = CustomData_get_layer_named(&me->ldata, CD_MLOOPUV, ma->texpaintslot[ma->paint_active_slot].uvname))))
		{
			mloopuv = me->mloopuv;
		}

		uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		immUniformThemeColor(TH_UV_SHADOW);

		mloopuv_base = mloopuv;

		for (a = me->totpoly; a > 0; a--, mpoly++) {
			if ((scene->toolsettings->uv_flag & UV_SHOW_SAME_IMAGE) && mpoly->mat_nr != ob->actcol - 1)
				continue;

			immBegin(GPU_PRIM_LINE_LOOP, mpoly->totloop);

			mloopuv = mloopuv_base + mpoly->loopstart;
			for (b = 0; b < mpoly->totloop; b++, mloopuv++) {
				immVertex2fv(pos, mloopuv->uv);
			}

			immEnd();
		}

		immUnbindProgram();
	}
}

static void draw_uvs_looptri(BMEditMesh *em, unsigned int *r_loop_index, const int cd_loop_uv_offset, unsigned int pos)
{
	unsigned int i = *r_loop_index;
	BMFace *f = em->looptris[i][0]->f;
	do {
		unsigned int j;
		for (j = 0; j < 3; j++) {
			MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(em->looptris[i][j], cd_loop_uv_offset);
			immVertex2fv(pos, luv->uv);
		}
		i++;
	} while (i != em->tottri && (f == em->looptris[i][0]->f));
	*r_loop_index = i - 1;
}

/* draws uv's in the image space */
static void draw_uvs(SpaceImage *sima, Scene *scene, ViewLayer *view_layer, Object *obedit, Depsgraph *depsgraph)
{
	ToolSettings *ts;
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMFace *efa, *efa_act;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	float col1[4], col2[4];
	float pointsize;
	int drawfaces, interpedges;
	Image *ima = sima->image;

	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	unsigned int pos, color;

	efa_act = EDBM_uv_active_face_get(em, false, false); /* will be set to NULL if hidden */
	ts = scene->toolsettings;

	drawfaces = draw_uvs_face_check(scene);
	if (ts->uv_flag & UV_SYNC_SELECTION)
		interpedges = (ts->selectmode & SCE_SELECT_VERTEX);
	else
		interpedges = (ts->uv_selectmode == UV_SELECT_VERTEX);

	/* draw other uvs */
	if (sima->flag & SI_DRAW_OTHER) {
		Image *curimage;

		if (efa_act) {
			ED_object_get_active_image(obedit, efa_act->mat_nr + 1, &curimage, NULL, NULL, NULL);
		}
		else {
			curimage = ima;
		}

		draw_uvs_other(view_layer, obedit, curimage, sima->other_uv_filter);
	}

	/* 1. draw shadow mesh */

	if (sima->flag & SI_DRAWSHADOW) {
		Object *ob_cage_eval = DEG_get_evaluated_object(depsgraph, obedit);
		/* XXX TODO: Need to check if shadow mesh is different than original mesh. */
		bool is_cage_like_final_meshes = (ob_cage_eval == obedit);

		/* When sync selection is enabled, all faces are drawn (except for hidden)
		 * so if cage is the same as the final, there is no point in drawing this. */
		if (((ts->uv_flag & UV_SYNC_SELECTION) == 0) || is_cage_like_final_meshes) {
			draw_uvs_shadow(ob_cage_eval);
		}
	}

	/* 2. draw colored faces */

	if (sima->flag & SI_DRAW_STRETCH) {
		draw_uvs_stretch(sima, scene, obedit, em, efa_act);
	}
	else {
		unsigned int tri_count = 0;
		BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
			if (uvedit_face_visible_test(scene, obedit, ima, efa)) {
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
				tri_count += efa->len - 2;
			}
			else {
				BM_elem_flag_disable(efa, BM_ELEM_TAG);
			}
		}

		if (tri_count && !(sima->flag & SI_NO_DRAWFACES)) {
			/* draw transparent faces */
			UI_GetThemeColor4fv(TH_FACE, col1);
			UI_GetThemeColor4fv(TH_FACE_SELECT, col2);
			GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
			GPU_blend(true);

			GPUVertFormat *format = immVertexFormat();
			pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

			GPUBatch *face_batch = immBeginBatch(GPU_PRIM_TRIS, tri_count * 3);
			for (unsigned int i = 0; i < em->tottri; i++) {
				efa = em->looptris[i][0]->f;
				if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
					const bool is_select = uvedit_face_select_test(scene, efa, cd_loop_uv_offset);

					if (efa == efa_act) {
						/* only once */
						float tmp_col[4];
						UI_GetThemeColor4fv(TH_EDITMESH_ACTIVE, tmp_col);
						immAttrib4fv(color, tmp_col);
					}
					else {
						immAttrib4fv(color, is_select ? col2 : col1);
					}

					draw_uvs_looptri(em, &i, cd_loop_uv_offset, pos);
				}
			}
			immEnd();

			/* XXX performance: we should not create and throw away result. */
			GPU_batch_draw(face_batch);
			GPU_batch_program_use_end(face_batch);
			GPU_batch_discard(face_batch);

			immUnbindProgram();

			GPU_blend(false);
		}
		else {
			if (efa_act && !uvedit_face_visible_test(scene, obedit, ima, efa_act)) {
				efa_act = NULL;
			}
		}
	}

	/* 3. draw active face stippled */
	/* (removed during OpenGL upgrade, reimplement if needed) */

	/* 4. draw edges */

	if (sima->flag & SI_SMOOTH_UV) {
		GPU_line_smooth(true);
		GPU_blend(true);
		GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
	}

	pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	switch (sima->dt_uv) {
		case SI_UVDT_DASH:
		{
			immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

			float viewport_size[4];
			GPU_viewport_size_get_f(viewport_size);
			immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

			immUniform1i("colors_len", 2);  /* "advanced" mode */
			immUniformArray4fv("colors", (float *)(float[][4]){{0.56f, 0.56f, 0.56f, 1.0f}, {0.07f, 0.07f, 0.07f, 1.0f}}, 2);
			immUniform1f("dash_width", 4.0f);
			GPU_line_width(1.0f);

			break;
		}
		case SI_UVDT_BLACK: /* black/white */
		case SI_UVDT_WHITE:
			immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
			if (sima->dt_uv == SI_UVDT_WHITE) {
				immUniformColor3f(1.0f, 1.0f, 1.0f);
			}
			else {
				immUniformColor3f(0.0f, 0.0f, 0.0f);
			}
			GPU_line_width(1.0f);

			break;
		case SI_UVDT_OUTLINE:
			immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
			imm_cpack(0x0);
			GPU_line_width(3.0f);

			break;
	}

	/* For more efficiency first transfer the entire buffer to vram. */
	GPUBatch *loop_batch = immBeginBatchAtMost(GPU_PRIM_LINE_LOOP, bm->totloop);
	GPUVertBuf *loop_vbo = loop_batch->verts[0];
	BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
		if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
			continue;

		BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			immVertex2fv(pos, luv->uv);
		}
	}
	immEnd();

	/* Then draw each face contour separately. */
	if (loop_vbo->vertex_len != 0) {
		GPU_batch_program_use_begin(loop_batch);
		unsigned int index = 0, loop_vbo_count;
		BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			GPU_batch_draw_range_ex(loop_batch, index, efa->len, false);
			index += efa->len;
		}
		loop_vbo_count = index;
		GPU_batch_program_use_end(loop_batch);
		immUnbindProgram();


		if (sima->dt_uv == SI_UVDT_OUTLINE) {
			GPU_line_width(1.0f);
			UI_GetThemeColor4fv(TH_WIRE_EDIT, col2);

			if (me->drawflag & ME_DRAWEDGES) {
				int sel;
				UI_GetThemeColor4fv(TH_EDGE_SELECT, col1);

				if (interpedges) {
					/* Create a color buffer. */
					static GPUVertFormat format = { 0 };
					static uint shdr_col;
					if (format.attr_len == 0) {
						shdr_col = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
					}

					GPUVertBuf *vbo_col = GPU_vertbuf_create_with_format(&format);
					GPU_vertbuf_data_alloc(vbo_col, loop_vbo_count);

					index = 0;
					BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
						if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
							continue;

						BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE) {
							sel = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
							GPU_vertbuf_attr_set(vbo_col, shdr_col, index++, sel ? col1 : col2);
						}
					}
					/* Reuse the UV buffer and add the color buffer. */
					GPU_batch_vertbuf_add_ex(loop_batch, vbo_col, true);

					/* Now draw each face contour separately with another builtin program. */
					GPU_batch_program_set_builtin(loop_batch, GPU_SHADER_2D_SMOOTH_COLOR);
					GPU_matrix_bind(loop_batch->interface);

					GPU_batch_program_use_begin(loop_batch);
					index = 0;
					BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
						if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
							continue;

						GPU_batch_draw_range_ex(loop_batch, index, efa->len, false);
						index += efa->len;
					}
					GPU_batch_program_use_end(loop_batch);
				}
				else {
					GPUVertFormat *format = immVertexFormat();
					pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
					color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

					immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

					/* Use batch here to avoid problems with `IMM_BUFFER_SIZE`. */
					GPUBatch *flat_edges_batch = immBeginBatchAtMost(GPU_PRIM_LINES, loop_vbo_count * 2);
					BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
						if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
							continue;

						BM_ITER_ELEM(l, &liter, efa, BM_LOOPS_OF_FACE) {
							sel = uvedit_edge_select_test(scene, l, cd_loop_uv_offset);
							immAttrib4fv(color, sel ? col1 : col2);

							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							immVertex2fv(pos, luv->uv);
							luv = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
							immVertex2fv(pos, luv->uv);
						}
					}
					immEnd();

					GPU_batch_draw(flat_edges_batch);
					GPU_batch_discard(flat_edges_batch);

					immUnbindProgram();
				}
			}
			else {
				GPU_batch_uniform_4fv(loop_batch, "color", col2);
				immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

				/* no nice edges */
				GPU_batch_program_use_begin(loop_batch);
				index = 0;
				BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
					if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
						continue;

					GPU_batch_draw_range_ex(loop_batch, index, efa->len, false);
					index += efa->len;
				}
				GPU_batch_program_use_end(loop_batch);
				immUnbindProgram();
			}
		}
	}
	else {
		immUnbindProgram();
	}

	GPU_batch_discard(loop_batch);

	if (sima->flag & SI_SMOOTH_UV) {
		GPU_line_smooth(false);
		GPU_blend(false);
	}

	/* 5. draw face centers */

	if (drawfaces) {
		float cent[2];
		bool col_set = false;

		GPUVertFormat *format = immVertexFormat();
		pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

		pointsize = UI_GetThemeValuef(TH_FACEDOT_SIZE);
		GPU_point_size(pointsize);

		immBeginAtMost(GPU_PRIM_POINTS, bm->totface);

		/* unselected faces */

		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			if (!uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
				/* Only set color for the first face */
				if (!col_set) {
					UI_GetThemeColor3fv(TH_WIRE, col1);
					immAttrib3fv(color, col1);

					col_set = true;
				}

				uv_poly_center(efa, cent, cd_loop_uv_offset);
				immVertex2fv(pos, cent);
			}
		}

		col_set = false;

		/* selected faces */

		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			if (uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
				/* Only set color for the first face */
				if (!col_set) {
					UI_GetThemeColor3fv(TH_FACE_DOT, col1);
					immAttrib3fv(color, col1);

					col_set = true;
				}

				uv_poly_center(efa, cent, cd_loop_uv_offset);
				immVertex2fv(pos, cent);
			}
		}

		immEnd();

		immUnbindProgram();
	}

	/* 6. draw uv vertices */

	if (drawfaces != 2) { /* 2 means Mesh Face Mode */
		pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		/* unselected uvs */
		immUniformThemeColor(TH_VERTEX);
		pointsize = UI_GetThemeValuef(TH_VERTEX_SIZE);
		GPU_point_size(pointsize);

		immBeginAtMost(GPU_PRIM_POINTS, bm->totloop);

		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				if (!uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
					immVertex2fv(pos, luv->uv);
			}
		}

		immEnd();

		/* pinned uvs */
		/* give odd pointsizes odd pin pointsizes */
		GPU_point_size(pointsize * 2 + (((int)pointsize % 2) ? (-1) : 0));
		imm_cpack(0xFF);

		immBeginAtMost(GPU_PRIM_POINTS, bm->totloop);

		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				if (luv->flag & MLOOPUV_PINNED)
					immVertex2fv(pos, luv->uv);
			}
		}

		immEnd();

		/* selected uvs */
		immUniformThemeColor(TH_VERTEX_SELECT);
		GPU_point_size(pointsize);

		immBeginAtMost(GPU_PRIM_POINTS, bm->totloop);

		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
					immVertex2fv(pos, luv->uv);
			}
		}

		immEnd();

		immUnbindProgram();
	}
}


static void draw_uv_shadows_get(
        SpaceImage *sima, Object *ob, Object *obedit,
        bool *show_shadow, bool *show_texpaint)
{
	*show_shadow = *show_texpaint = false;

	if (ED_space_image_show_render(sima) || (sima->flag & SI_NO_DRAW_TEXPAINT))
		return;

	if ((sima->mode == SI_MODE_PAINT) && obedit && obedit->type == OB_MESH) {
		struct BMEditMesh *em = BKE_editmesh_from_object(obedit);

		*show_shadow = EDBM_uv_check(em);
	}

	*show_texpaint = (ob && ob->type == OB_MESH && ob->mode == OB_MODE_TEXTURE_PAINT);
}

void ED_uvedit_draw_main(
        SpaceImage *sima,
        ARegion *ar, Scene *scene, ViewLayer *view_layer, Object *obedit, Object *obact, Depsgraph *depsgraph)
{
	ToolSettings *toolsettings = scene->toolsettings;
	bool show_uvedit, show_uvshadow, show_texpaint_uvshadow;

	show_uvedit = ED_space_image_show_uvedit(sima, obedit);
	draw_uv_shadows_get(sima, obact, obedit, &show_uvshadow, &show_texpaint_uvshadow);

	if (show_uvedit || show_uvshadow || show_texpaint_uvshadow) {
		if (show_uvshadow) {
			draw_uvs_shadow(obedit);
		}
		else if (show_uvedit) {
			uint objects_len = 0;
			Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(view_layer, &objects_len);
			for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
				Object *ob_iter = objects[ob_index];
				draw_uvs(sima, scene, view_layer, ob_iter, depsgraph);
			}
			MEM_freeN(objects);
		}
		else {
			draw_uvs_texpaint(sima, scene, view_layer, obact);
		}

		if (show_uvedit && !(toolsettings->use_uv_sculpt))
			ED_image_draw_cursor(ar, sima->cursor);
	}
}
