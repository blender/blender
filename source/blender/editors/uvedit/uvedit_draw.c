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

#include "BKE_DerivedMesh.h"
#include "BKE_editmesh.h"
#include "BKE_material.h"

#include "BKE_scene.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "UI_resources.h"
#include "UI_interface.h"
#include "UI_view2d.h"

#include "uvedit_intern.h"

/* use editmesh tessface */
#define USE_EDBM_LOOPTRIS

static void draw_uvs_lineloop_bmface(BMFace *efa, const int cd_loop_uv_offset);

void ED_image_draw_cursor(ARegion *ar, const float cursor[2])
{
	float zoom[2], x_fac, y_fac;

	UI_view2d_scale_get_inverse(&ar->v2d, &zoom[0], &zoom[1]);

	mul_v2_fl(zoom, 256.0f * UI_DPI_FAC);
	x_fac = zoom[0];
	y_fac = zoom[1];
	
	cpack(0xFFFFFF);
	glTranslatef(cursor[0], cursor[1], 0.0);
	fdrawline(-0.05f * x_fac, 0, 0, 0.05f * y_fac);
	fdrawline(0, 0.05f * y_fac, 0.05f * x_fac, 0.0f);
	fdrawline(0.05f * x_fac, 0.0f, 0.0f, -0.05f * y_fac);
	fdrawline(0.0f, -0.05f * y_fac, -0.05f * x_fac, 0.0f);

	setlinestyle(4);
	cpack(0xFF);
	fdrawline(-0.05f * x_fac, 0.0f, 0.0f, 0.05f * y_fac);
	fdrawline(0.0f, 0.05f * y_fac, 0.05f * x_fac, 0.0f);
	fdrawline(0.05f * x_fac, 0.0f, 0.0f, -0.05f * y_fac);
	fdrawline(0.0f, -0.05f * y_fac, -0.05f * x_fac, 0.0f);


	setlinestyle(0.0f);
	cpack(0x0);
	fdrawline(-0.020f * x_fac, 0.0f, -0.1f * x_fac, 0.0f);
	fdrawline(0.1f * x_fac, 0.0f, 0.020f * x_fac, 0.0f);
	fdrawline(0.0f, -0.020f * y_fac, 0.0f, -0.1f * y_fac);
	fdrawline(0.0f, 0.1f * y_fac, 0.0f, 0.020f * y_fac);

	setlinestyle(1);
	cpack(0xFFFFFF);
	fdrawline(-0.020f * x_fac, 0.0f, -0.1f * x_fac, 0.0f);
	fdrawline(0.1f * x_fac, 0.0f, 0.020f * x_fac, 0.0f);
	fdrawline(0.0f, -0.020f * y_fac, 0.0f, -0.1f * y_fac);
	fdrawline(0.0f, 0.1f * y_fac, 0.0f, 0.020f * y_fac);

	glTranslatef(-cursor[0], -cursor[1], 0.0);
	setlinestyle(0);
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
	BMFace *efa;
	BMIter iter;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	/* draws the mesh when painting */
	UI_ThemeColor(TH_UV_SHADOW);

	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		draw_uvs_lineloop_bmface(efa, cd_loop_uv_offset);
	}
}

static int draw_uvs_dm_shadow(DerivedMesh *dm)
{
	/* draw shadow mesh - this is the mesh with the modifier applied */

	if (dm && dm->drawUVEdges && CustomData_has_layer(&dm->loopData, CD_MLOOPUV)) {
		UI_ThemeColor(TH_UV_SHADOW);
		dm->drawUVEdges(dm);
		return 1;
	}

	return 0;
}

static void draw_uvs_stretch(SpaceImage *sima, Scene *scene, BMEditMesh *em, MTexPoly *activetf)
{
	BMesh *bm = em->bm;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	Image *ima = sima->image;
	float aspx, aspy, col[4];
	int i;

	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&bm->pdata, CD_MTEXPOLY);

	BLI_buffer_declare_static(vec2f, tf_uv_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_buffer_declare_static(vec2f, tf_uvorig_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);

	ED_space_image_get_uv_aspect(sima, &aspx, &aspy);
	
	switch (sima->dt_uvstretch) {
		case SI_UVDT_STRETCH_AREA:
		{
			float totarea = 0.0f, totuvarea = 0.0f, areadiff, uvarea, area;
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				const int efa_len = efa->len;
				float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_resize_data(&tf_uv_buf,     vec2f, efa_len);
				float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_resize_data(&tf_uvorig_buf, vec2f, efa_len);
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

				BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					copy_v2_v2(tf_uvorig[i], luv->uv);
				}

				uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

				totarea += BM_face_calc_area(efa);
				totuvarea += area_poly_v2((const float (*)[2])tf_uv, efa->len);
				
				if (uvedit_face_visible_test(scene, ima, efa, tf)) {
					BM_elem_flag_enable(efa, BM_ELEM_TAG);
				}
				else {
					if (tf == activetf)
						activetf = NULL;
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
				}
			}
			
			if (totarea < FLT_EPSILON || totuvarea < FLT_EPSILON) {
				col[0] = 1.0;
				col[1] = col[2] = 0.0;
				glColor3fv(col);
				BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
						glBegin(GL_POLYGON);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							glVertex2fv(luv->uv);
						}
						glEnd();
					}
				}
			}
			else {
				BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
						const int efa_len = efa->len;
						float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_resize_data(&tf_uv_buf,     vec2f, efa_len);
						float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_resize_data(&tf_uvorig_buf, vec2f, efa_len);

						area = BM_face_calc_area(efa) / totarea;

						BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							copy_v2_v2(tf_uvorig[i], luv->uv);
						}

						uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

						uvarea = area_poly_v2((const float (*)[2])tf_uv, efa->len) / totuvarea;
						
						if (area < FLT_EPSILON || uvarea < FLT_EPSILON)
							areadiff = 1.0f;
						else if (area > uvarea)
							areadiff = 1.0f - (uvarea / area);
						else
							areadiff = 1.0f - (area / uvarea);
						
						weight_to_rgb(col, areadiff);
						glColor3fv(col);
						
						/* TODO: USE_EDBM_LOOPTRIS */
						glBegin(GL_POLYGON);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							glVertex2fv(luv->uv);
						}
						glEnd();
					}
				}
			}
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
			
			glShadeModel(GL_SMOOTH);
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
				
				if (uvedit_face_visible_test(scene, ima, efa, tf)) {
					const int efa_len = efa->len;
					float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_resize_data(&tf_uv_buf,     vec2f, efa_len);
					float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_resize_data(&tf_uvorig_buf, vec2f, efa_len);
					float *uvang = BLI_buffer_resize_data(&uvang_buf, float, efa_len);
					float *ang   = BLI_buffer_resize_data(&ang_buf,   float, efa_len);
					float (*av)[3]  = (float (*)[3])BLI_buffer_resize_data(&av_buf, vec3f, efa_len);
					float (*auv)[2] = (float (*)[2])BLI_buffer_resize_data(&auv_buf, vec2f, efa_len);
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

					/* TODO: USE_EDBM_LOOPTRIS */
					glBegin(GL_POLYGON);
					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						a = fabsf(uvang[i] - ang[i]) / (float)M_PI;
						weight_to_rgb(col, 1.0f - pow2f(1.0f - a));
						glColor3fv(col);
						glVertex2fv(luv->uv);
					}
					glEnd();
				}
				else {
					if (tf == activetf)
						activetf = NULL;
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
				}
			}

			BLI_buffer_free(&uvang_buf);
			BLI_buffer_free(&ang_buf);
			BLI_buffer_free(&av_buf);
			BLI_buffer_free(&auv_buf);

			glShadeModel(GL_FLAT);

			break;
		}
	}

	BLI_buffer_free(&tf_uv_buf);
	BLI_buffer_free(&tf_uvorig_buf);
}

static void draw_uvs_lineloop_bmface(BMFace *efa, const int cd_loop_uv_offset)
{
	BMIter liter;
	BMLoop *l;
	MLoopUV *luv;

	glBegin(GL_LINE_LOOP);
	BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
		luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		glVertex2fv(luv->uv);
	}
	glEnd();
}

static void draw_uvs_lineloop_mpoly(Mesh *me, MPoly *mpoly)
{
	MLoopUV *mloopuv;
	int i;

	glBegin(GL_LINE_LOOP);
	mloopuv = &me->mloopuv[mpoly->loopstart];
	for (i = mpoly->totloop; i != 0; i--, mloopuv++) {
		glVertex2fv(mloopuv->uv);
	}
	glEnd();
}

static void draw_uvs_other_mesh_texface(Object *ob, const Image *curimage)
{
	Mesh *me = ob->data;
	MPoly *mpoly = me->mpoly;
	MTexPoly *mtpoly = me->mtpoly;
	int a;

	if (me->mloopuv == NULL) {
		return;
	}

	for (a = me->totpoly; a != 0; a--, mpoly++, mtpoly++) {
		if (mtpoly->tpage != curimage) {
			continue;
		}

		draw_uvs_lineloop_mpoly(me, mpoly);
	}
}
static void draw_uvs_other_mesh_new_shading(Object *ob, const Image *curimage)
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
		const int mat_nr = mpoly->mat_nr;
		if ((mat_nr >= totcol) ||
		    (BLI_BITMAP_TEST(mat_test_array, mat_nr)) == 0)
		{
			continue;
		}

		draw_uvs_lineloop_mpoly(me, mpoly);
	}
}
static void draw_uvs_other_mesh(Object *ob, const Image *curimage, const bool new_shading_nodes)
{
	if (new_shading_nodes) {
		draw_uvs_other_mesh_new_shading(ob, curimage);
	}
	else {
		draw_uvs_other_mesh_texface(ob, curimage);
	}
}

static void draw_uvs_other(Scene *scene, Object *obedit, const Image *curimage, const bool new_shading_nodes)
{
	Base *base;

	UI_ThemeColor(TH_UV_OTHERS);

	for (base = scene->base.first; base; base = base->next) {
		Object *ob = base->object;

		if (!(base->flag & SELECT)) continue;
		if (!(base->lay & scene->lay)) continue;
		if (ob->restrictflag & OB_RESTRICT_VIEW) continue;

		if ((ob->type == OB_MESH) && (ob != obedit) && ((Mesh *)ob->data)->mloopuv) {
			draw_uvs_other_mesh(ob, curimage, new_shading_nodes);
		}
	}
}

static void draw_uvs_texpaint(SpaceImage *sima, Scene *scene, Object *ob)
{
	const bool new_shading_nodes = BKE_scene_use_new_shading_nodes(scene);
	Image *curimage = ED_space_image(sima);
	Mesh *me = ob->data;
	Material *ma;

	if (sima->flag & SI_DRAW_OTHER) {
		draw_uvs_other(scene, ob, curimage, new_shading_nodes);
	}

	UI_ThemeColor(TH_UV_SHADOW);

	ma = give_current_material(ob, ob->actcol);

	if (me->mtpoly) {
		MPoly *mpoly = me->mpoly;
		MLoopUV *mloopuv, *mloopuv_base;
		int a, b;
		if (!(ma && ma->texpaintslot && ma->texpaintslot[ma->paint_active_slot].uvname &&
		      (mloopuv = CustomData_get_layer_named(&me->ldata, CD_MLOOPUV, ma->texpaintslot[ma->paint_active_slot].uvname))))
		{
			mloopuv = me->mloopuv;
		}

		mloopuv_base = mloopuv;

		for (a = me->totpoly; a > 0; a--, mpoly++) {
			if ((scene->toolsettings->uv_flag & UV_SHOW_SAME_IMAGE) && mpoly->mat_nr != ob->actcol - 1)
				continue;
			glBegin(GL_LINE_LOOP);

			mloopuv = mloopuv_base + mpoly->loopstart;
			for (b = 0; b < mpoly->totloop; b++, mloopuv++) {
				glVertex2fv(mloopuv->uv);
			}
			glEnd();
		}
	}
}

#ifdef USE_EDBM_LOOPTRIS
static void draw_uvs_looptri(BMEditMesh *em, unsigned int *r_loop_index, const int cd_loop_uv_offset)
{
	unsigned int i = *r_loop_index;
	BMFace *f = em->looptris[i][0]->f;
	do {
		unsigned int j;
		for (j = 0; j < 3; j++) {
			MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(em->looptris[i][j], cd_loop_uv_offset);
			glVertex2fv(luv->uv);
		}
		i++;
	} while (i != em->tottri && (f == em->looptris[i][0]->f));
	*r_loop_index = i - 1;
}
#endif

/* draws uv's in the image space */
static void draw_uvs(SpaceImage *sima, Scene *scene, Object *obedit)
{
	const bool new_shading_nodes = BKE_scene_use_new_shading_nodes(scene);
	ToolSettings *ts;
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMFace *efa, *efa_act;
#ifndef USE_EDBM_LOOPTRIS
	BMFace *activef;
#endif
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf, *activetf = NULL;
	MLoopUV *luv;
	DerivedMesh *finaldm, *cagedm;
	unsigned char col1[4], col2[4];
	float pointsize;
	int drawfaces, interpedges;
	Image *ima = sima->image;

	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&bm->pdata, CD_MTEXPOLY);

	activetf = EDBM_mtexpoly_active_get(em, &efa_act, false, false); /* will be set to NULL if hidden */
#ifndef USE_EDBM_LOOPTRIS
	activef = BM_mesh_active_face_get(bm, false, false);
#endif
	ts = scene->toolsettings;

	drawfaces = draw_uvs_face_check(scene);
	if (ts->uv_flag & UV_SYNC_SELECTION)
		interpedges = (ts->selectmode & SCE_SELECT_VERTEX);
	else
		interpedges = (ts->uv_selectmode == UV_SELECT_VERTEX);
	
	/* draw other uvs */
	if (sima->flag & SI_DRAW_OTHER) {
		Image *curimage;

		if (new_shading_nodes) {
			if (efa_act) {
				ED_object_get_active_image(obedit, efa_act->mat_nr + 1, &curimage, NULL, NULL, NULL);
			}
			else {
				curimage = ima;
			}
		}
		else {
			curimage = (activetf) ? activetf->tpage : ima;
		}

		draw_uvs_other(scene, obedit, curimage, new_shading_nodes);
	}

	/* 1. draw shadow mesh */
	
	if (sima->flag & SI_DRAWSHADOW) {
		DM_update_materials(em->derivedFinal, obedit);
		/* first try existing derivedmesh */
		if (!draw_uvs_dm_shadow(em->derivedFinal)) {
			/* create one if it does not exist */
			cagedm = editbmesh_get_derived_cage_and_final(scene, obedit, me->edit_btmesh, &finaldm, CD_MASK_BAREMESH | CD_MASK_MTFACE);

			/* when sync selection is enabled, all faces are drawn (except for hidden)
			 * so if cage is the same as the final, theres no point in drawing this */
			if (!((ts->uv_flag & UV_SYNC_SELECTION) && (cagedm == finaldm)))
				draw_uvs_dm_shadow(finaldm);
			
			/* release derivedmesh again */
			if (cagedm != finaldm) cagedm->release(cagedm);
			finaldm->release(finaldm);
		}
	}
	
	/* 2. draw colored faces */
	
	if (sima->flag & SI_DRAW_STRETCH) {
		draw_uvs_stretch(sima, scene, em, activetf);
	}
	else if (!(sima->flag & SI_NO_DRAWFACES)) {
		/* draw transparent faces */
		UI_GetThemeColor4ubv(TH_FACE, col1);
		UI_GetThemeColor4ubv(TH_FACE_SELECT, col2);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		
#ifdef USE_EDBM_LOOPTRIS
		{
			unsigned int i;
			for (i = 0; i < em->tottri; i++) {
				efa = em->looptris[i][0]->f;
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
				if (uvedit_face_visible_test(scene, ima, efa, tf)) {
					const bool is_select = uvedit_face_select_test(scene, efa, cd_loop_uv_offset);
					BM_elem_flag_enable(efa, BM_ELEM_TAG);

					if (tf == activetf) {
						/* only once */
						glEnable(GL_POLYGON_STIPPLE);
						glPolygonStipple(stipple_quarttone);
						UI_ThemeColor4(TH_EDITMESH_ACTIVE);
					}
					else {
						glColor4ubv((GLubyte *)(is_select ? col2 : col1));
					}

					glBegin(GL_TRIANGLES);
					draw_uvs_looptri(em, &i, cd_loop_uv_offset);
					glEnd();

					if (tf == activetf) {
						glDisable(GL_POLYGON_STIPPLE);
					}
				}
				else {
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
				}
			}
		}
#else
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
				if (tf == activetf) continue;  /* important the temp boolean is set above */

				if (uvedit_face_select_test(scene, efa, cd_loop_uv_offset))
					glColor4ubv((GLubyte *)col2);
				else
					glColor4ubv((GLubyte *)col1);
				
				glBegin(GL_POLYGON);
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					glVertex2fv(luv->uv);
				}
				glEnd();
			}
			else {
				if (tf == activetf)
					activetf = NULL;
				BM_elem_flag_disable(efa, BM_ELEM_TAG);
			}
		}
#endif
		glDisable(GL_BLEND);
	}
	else {
		/* would be nice to do this within a draw loop but most below are optional, so it would involve too many checks */
		
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
			}
			else {
				if (tf == activetf)
					activetf = NULL;
				BM_elem_flag_disable(efa, BM_ELEM_TAG);
			}
		}
		
	}

	/* 3. draw active face stippled */
#ifndef USE_EDBM_LOOPTRIS
	if (activef) {
		tf = BM_ELEM_CD_GET_VOID_P(activef, cd_poly_tex_offset);
		if (uvedit_face_visible_test(scene, ima, activef, tf)) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			UI_ThemeColor4(TH_EDITMESH_ACTIVE);

			glEnable(GL_POLYGON_STIPPLE);
			glPolygonStipple(stipple_quarttone);

			glBegin(GL_POLYGON);
			BM_ITER_ELEM (l, &liter, activef, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				glVertex2fv(luv->uv);
			}
			glEnd();

			glDisable(GL_POLYGON_STIPPLE);
			glDisable(GL_BLEND);
		}
	}
#endif
	
	/* 4. draw edges */

	if (sima->flag & SI_SMOOTH_UV) {
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	
	switch (sima->dt_uv) {
		case SI_UVDT_DASH:
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
					continue;
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

				if (tf) {
					cpack(0x111111);

					draw_uvs_lineloop_bmface(efa, cd_loop_uv_offset);

					setlinestyle(2);
					cpack(0x909090);

					draw_uvs_lineloop_bmface(efa, cd_loop_uv_offset);

					setlinestyle(0);
				}
			}
			break;
		case SI_UVDT_BLACK: /* black/white */
		case SI_UVDT_WHITE: 
			if (sima->dt_uv == SI_UVDT_WHITE) glColor3f(1.0f, 1.0f, 1.0f);
			else glColor3f(0.0f, 0.0f, 0.0f);

			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
					continue;

				draw_uvs_lineloop_bmface(efa, cd_loop_uv_offset);
			}
			break;
		case SI_UVDT_OUTLINE:
			glLineWidth(3);
			cpack(0x0);
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
					continue;

				draw_uvs_lineloop_bmface(efa, cd_loop_uv_offset);
			}
			
			glLineWidth(1);
			UI_GetThemeColor4ubv(TH_WIRE_EDIT, col2);
			glColor4ubv((unsigned char *)col2);

			if (me->drawflag & ME_DRAWEDGES) {
				int sel, lastsel = -1;
				UI_GetThemeColor4ubv(TH_EDGE_SELECT, col1);

				if (interpedges) {
					glShadeModel(GL_SMOOTH);

					BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
						if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
							continue;

						glBegin(GL_LINE_LOOP);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							sel = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
							glColor4ubv(sel ? (GLubyte *)col1 : (GLubyte *)col2);

							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							glVertex2fv(luv->uv);
						}
						glEnd();
					}

					glShadeModel(GL_FLAT);
				}
				else {
					BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
						if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
							continue;

						glBegin(GL_LINES);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							sel = uvedit_edge_select_test(scene, l, cd_loop_uv_offset);
							if (sel != lastsel) {
								glColor4ubv(sel ? (GLubyte *)col1 : (GLubyte *)col2);
								lastsel = sel;
							}
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							glVertex2fv(luv->uv);
							luv = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
							glVertex2fv(luv->uv);
						}
						glEnd();
					}
				}
			}
			else {
				/* no nice edges */
				BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
					if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
						continue;
				
					draw_uvs_lineloop_bmface(efa, cd_loop_uv_offset);
				}
			}
			
			break;
	}

	if (sima->flag & SI_SMOOTH_UV) {
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}

	/* 5. draw face centers */

	if (drawfaces) {
		float cent[2];
		
		pointsize = UI_GetThemeValuef(TH_FACEDOT_SIZE);
		glPointSize(pointsize); // TODO - drawobject.c changes this value after - Investigate!
		
		/* unselected faces */
		UI_ThemeColor(TH_WIRE);

		bglBegin(GL_POINTS);
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			if (!uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
				uv_poly_center(efa, cent, cd_loop_uv_offset);
				bglVertex2fv(cent);
			}
		}
		bglEnd();

		/* selected faces */
		UI_ThemeColor(TH_FACE_DOT);

		bglBegin(GL_POINTS);
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			if (uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
				uv_poly_center(efa, cent, cd_loop_uv_offset);
				bglVertex2fv(cent);
			}
		}
		bglEnd();
	}

	/* 6. draw uv vertices */
	
	if (drawfaces != 2) { /* 2 means Mesh Face Mode */
		/* unselected uvs */
		UI_ThemeColor(TH_VERTEX);
		pointsize = UI_GetThemeValuef(TH_VERTEX_SIZE);
		glPointSize(pointsize);
	
		bglBegin(GL_POINTS);
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				if (!uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
					bglVertex2fv(luv->uv);
			}
		}
		bglEnd();
	
		/* pinned uvs */
		/* give odd pointsizes odd pin pointsizes */
		glPointSize(pointsize * 2 + (((int)pointsize % 2) ? (-1) : 0));
		cpack(0xFF);
	
		bglBegin(GL_POINTS);
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				if (luv->flag & MLOOPUV_PINNED)
					bglVertex2fv(luv->uv);
			}
		}
		bglEnd();
	
		/* selected uvs */
		UI_ThemeColor(TH_VERTEX_SELECT);
		glPointSize(pointsize);
	
		bglBegin(GL_POINTS);
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
					bglVertex2fv(luv->uv);
			}
		}
		bglEnd();
	}

	glPointSize(1.0);
}


static void draw_uv_shadows_get(SpaceImage *sima, Object *ob, Object *obedit, bool *show_shadow, bool *show_texpaint)
{
	*show_shadow = *show_texpaint = false;

	if (ED_space_image_show_render(sima) || (sima->flag & SI_NO_DRAW_TEXPAINT))
		return;

	if ((sima->mode == SI_MODE_PAINT) && obedit && obedit->type == OB_MESH) {
		struct BMEditMesh *em = BKE_editmesh_from_object(obedit);
		
		*show_shadow = EDBM_mtexpoly_check(em);
	}
	
	*show_texpaint = (ob && ob->type == OB_MESH && ob->mode == OB_MODE_TEXTURE_PAINT);
}

void ED_uvedit_draw_main(SpaceImage *sima, ARegion *ar, Scene *scene, Object *obedit, Object *obact)
{
	ToolSettings *toolsettings = scene->toolsettings;
	bool show_uvedit, show_uvshadow, show_texpaint_uvshadow;

	show_uvedit = ED_space_image_show_uvedit(sima, obedit);
	draw_uv_shadows_get(sima, obact, obedit, &show_uvshadow, &show_texpaint_uvshadow);

	if (show_uvedit || show_uvshadow || show_texpaint_uvshadow) {
		if (show_uvshadow)
			draw_uvs_shadow(obedit);
		else if (show_uvedit)
			draw_uvs(sima, scene, obedit);
		else
			draw_uvs_texpaint(sima, scene, obact);

		if (show_uvedit && !(toolsettings->use_uv_sculpt))
			ED_image_draw_cursor(ar, sima->cursor);
	}
}

