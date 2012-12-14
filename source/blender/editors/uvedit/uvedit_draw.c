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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_tessmesh.h"

#include "BLI_array.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_util.h"
#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "UI_resources.h"
#include "UI_interface.h"
#include "UI_view2d.h"

#include "uvedit_intern.h"

void draw_image_cursor(SpaceImage *sima, ARegion *ar)
{
	float zoom[2], x_fac, y_fac;

	UI_view2d_getscale_inverse(&ar->v2d, &zoom[0], &zoom[1]);

	mul_v2_fl(zoom, 256.0f * UI_DPI_FAC);
	x_fac = zoom[0];
	y_fac = zoom[1];
	
	cpack(0xFFFFFF);
	glTranslatef(sima->cursor[0], sima->cursor[1], 0.0);
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

	glTranslatef(-sima->cursor[0], -sima->cursor[1], 0.0);
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
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMesh *bm = em->bm;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;

	/* draws the gray mesh when painting */
	glColor3ub(112, 112, 112);

	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		glBegin(GL_LINE_LOOP);
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

			glVertex2fv(luv->uv);
		}
		glEnd();
	}
}

static int draw_uvs_dm_shadow(DerivedMesh *dm)
{
	/* draw shadow mesh - this is the mesh with the modifier applied */

	if (dm && dm->drawUVEdges && CustomData_has_layer(&dm->loopData, CD_MLOOPUV)) {
		glColor3ub(112, 112, 112);
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

	ED_space_image_get_uv_aspect(sima, &aspx, &aspy);
	
	switch (sima->dt_uvstretch) {
		case SI_UVDT_STRETCH_AREA:
		{
			float totarea = 0.0f, totuvarea = 0.0f, areadiff, uvarea, area;
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				const int efa_len = efa->len;
				float (*tf_uv)[2] = BLI_array_alloca(tf_uv, efa_len);
				float (*tf_uvorig)[2] = BLI_array_alloca(tf_uvorig, efa_len);
				tf = CustomData_bmesh_get(&bm->pdata, efa->head.data, CD_MTEXPOLY);

				BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
					copy_v2_v2(tf_uvorig[i], luv->uv);
				}

				uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

				totarea += BM_face_calc_area(efa);
				//totuvarea += tf_area(tf, efa->v4!=0);
				totuvarea += uv_poly_area(tf_uv, efa->len);
				
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
							luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
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
						float (*tf_uv)[2] = BLI_array_alloca(tf_uv, efa_len);
						float (*tf_uvorig)[2] = BLI_array_alloca(tf_uvorig, efa_len);

						area = BM_face_calc_area(efa) / totarea;

						BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
							luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
							copy_v2_v2(tf_uvorig[i], luv->uv);
						}

						uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

						//uvarea = tf_area(tf, efa->v4!=0) / totuvarea;
						uvarea = uv_poly_area(tf_uv, efa->len) / totuvarea;
						
						if (area < FLT_EPSILON || uvarea < FLT_EPSILON)
							areadiff = 1.0f;
						else if (area > uvarea)
							areadiff = 1.0f - (uvarea / area);
						else
							areadiff = 1.0f - (area / uvarea);
						
						weight_to_rgb(col, areadiff);
						glColor3fv(col);
						
						glBegin(GL_POLYGON);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
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

			col[3] = 0.5f; /* hard coded alpha, not that nice */
			
			glShadeModel(GL_SMOOTH);
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				tf = CustomData_bmesh_get(&bm->pdata, efa->head.data, CD_MTEXPOLY);
				
				if (uvedit_face_visible_test(scene, ima, efa, tf)) {
					const int efa_len = efa->len;
					float (*tf_uv)[2] = BLI_array_alloca(tf_uv, efa_len);
					float (*tf_uvorig)[2] = BLI_array_alloca(tf_uvorig, efa_len);
					float *uvang = BLI_array_alloca(uvang, efa_len);
					float *ang = BLI_array_alloca(ang, efa_len);
					float (*av)[3] = BLI_array_alloca(av, efa_len);  /* use for 2d and 3d  angle vectors */
					float (*auv)[2] = BLI_array_alloca(auv, efa_len);
					int j;

					BM_elem_flag_enable(efa, BM_ELEM_TAG);

					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
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

					glBegin(GL_POLYGON);
					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
						a = fabsf(uvang[i] - ang[i]) / (float)M_PI;
						weight_to_rgb(col, 1.0f - powf((1.0f - a), 2.0f));
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

			glShadeModel(GL_FLAT);

			break;
		}
	}
}

static void draw_uvs_other(Scene *scene, Object *obedit, Image *curimage)
{
	Base *base;

	glColor3ub(96, 96, 96);

	for (base = scene->base.first; base; base = base->next) {
		Object *ob = base->object;

		if (!(base->flag & SELECT)) continue;
		if (!(base->lay & scene->lay)) continue;
		if (ob->restrictflag & OB_RESTRICT_VIEW) continue;

		if ((ob->type == OB_MESH) && (ob != obedit)) {
			Mesh *me = ob->data;

			if (me->mtpoly) {
				MPoly *mpoly = me->mpoly;
				MTexPoly *mtpoly = me->mtpoly;
				MLoopUV *mloopuv;
				int a, b;

				for (a = me->totpoly; a > 0; a--, mtpoly++, mpoly++) {
					if (mtpoly->tpage == curimage) {
						glBegin(GL_LINE_LOOP);

						mloopuv = me->mloopuv + mpoly->loopstart;
						for (b = 0; b < mpoly->totloop; b++, mloopuv++) {
							glVertex2fv(mloopuv->uv);
						}
						glEnd();
					}
				}
			}
		}
	}
}

static void draw_uvs_texpaint(SpaceImage *sima, Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	Image *curimage = ED_space_image(sima);

	if (sima->flag & SI_DRAW_OTHER)
		draw_uvs_other(scene, ob, curimage);

	glColor3ub(112, 112, 112);

	if (me->mtpoly) {
		MPoly *mpoly = me->mpoly;
		MTexPoly *tface = me->mtpoly;
		MLoopUV *mloopuv;
		int a, b;

		for (a = me->totpoly; a > 0; a--, tface++, mpoly++) {
			if (tface->tpage == curimage) {
				glBegin(GL_LINE_LOOP);

				mloopuv = me->mloopuv + mpoly->loopstart;
				for (b = 0; b < mpoly->totloop; b++, mloopuv++) {
					glVertex2fv(mloopuv->uv);
				}
				glEnd();
			}
		}
	}
}

/* draws uv's in the image space */
static void draw_uvs(SpaceImage *sima, Scene *scene, Object *obedit)
{
	ToolSettings *ts;
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMFace *efa, *efa_act, *activef;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf, *activetf = NULL;
	MLoopUV *luv;
	DerivedMesh *finaldm, *cagedm;
	unsigned char col1[4], col2[4];
	float pointsize;
	int drawfaces, interpedges;
	Image *ima = sima->image;

	activetf = EDBM_mtexpoly_active_get(em, &efa_act, FALSE, FALSE); /* will be set to NULL if hidden */
	activef = BM_active_face_get(bm, FALSE, FALSE);
	ts = scene->toolsettings;

	drawfaces = draw_uvs_face_check(scene);
	if (ts->uv_flag & UV_SYNC_SELECTION)
		interpedges = (ts->selectmode & SCE_SELECT_VERTEX);
	else
		interpedges = (ts->uv_selectmode == UV_SELECT_VERTEX);
	
	/* draw other uvs */
	if (sima->flag & SI_DRAW_OTHER) {
		Image *curimage = (activetf) ? activetf->tpage : ima;

		draw_uvs_other(scene, obedit, curimage);
	}

	/* 1. draw shadow mesh */
	
	if (sima->flag & SI_DRAWSHADOW) {
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
		
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&bm->pdata, efa->head.data, CD_MTEXPOLY);
			
			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
				if (tf == activetf) continue;  /* important the temp boolean is set above */

				if (uvedit_face_select_test(scene, em, efa))
					glColor4ubv((GLubyte *)col2);
				else
					glColor4ubv((GLubyte *)col1);
				
				glBegin(GL_POLYGON);
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
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
		glDisable(GL_BLEND);
	}
	else {
		/* would be nice to do this within a draw loop but most below are optional, so it would involve too many checks */
		
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&bm->pdata, efa->head.data, CD_MTEXPOLY);

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

	if (activef) {
		tf = CustomData_bmesh_get(&bm->pdata, activef->head.data, CD_MTEXPOLY);
		if (uvedit_face_visible_test(scene, ima, activef, tf)) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			UI_ThemeColor4(TH_EDITMESH_ACTIVE);

			glEnable(GL_POLYGON_STIPPLE);
			glPolygonStipple(stipple_quarttone);

			glBegin(GL_POLYGON);
			BM_ITER_ELEM (l, &liter, activef, BM_LOOPS_OF_FACE) {
				luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
				glVertex2fv(luv->uv);
			}
			glEnd();

			glDisable(GL_POLYGON_STIPPLE);
			glDisable(GL_BLEND);
		}
	}
	
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
				tf = CustomData_bmesh_get(&bm->pdata, efa->head.data, CD_MTEXPOLY);

				if (tf) {
					cpack(0x111111);

					glBegin(GL_LINE_LOOP);
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
						glVertex2fv(luv->uv);
					}
					glEnd();

					setlinestyle(2);
					cpack(0x909090);

					glBegin(GL_LINE_LOOP);
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
						glVertex2fv(luv->uv);
					}
					glEnd();

#if 0
					glBegin(GL_LINE_STRIP);
					luv = CustomData_bmesh_get(&bm->ldata, efa->lbase->head.data, CD_MLOOPUV);
					glVertex2fv(luv->uv);
					luv = CustomData_bmesh_get(&bm->ldata, efa->lbase->next->head.data, CD_MLOOPUV);
					glVertex2fv(luv->uv);
					glEnd();
#endif

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

				glBegin(GL_LINE_LOOP);
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
					glVertex2fv(luv->uv);
				}
				glEnd();
			}
			break;
		case SI_UVDT_OUTLINE:
			glLineWidth(3);
			cpack(0x0);
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
					continue;

				glBegin(GL_LINE_LOOP);
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
					glVertex2fv(luv->uv);
				}
				glEnd();
			}
			
			glLineWidth(1);
			col2[0] = col2[1] = col2[2] = 192; col2[3] = 255;
			glColor4ubv((unsigned char *)col2); 
			
			if (me->drawflag & ME_DRAWEDGES) {
				int sel, lastsel = -1;
				UI_GetThemeColor4ubv(TH_VERTEX_SELECT, col1);

				if (interpedges) {
					glShadeModel(GL_SMOOTH);

					BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
						if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
							continue;

						glBegin(GL_LINE_LOOP);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							sel = (uvedit_uv_select_test(em, scene, l) ? 1 : 0);
							glColor4ubv(sel ? (GLubyte *)col1 : (GLubyte *)col2);

							luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
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
							sel = (uvedit_edge_select_test(em, scene, l) ? 1 : 0);
							if (sel != lastsel) {
								glColor4ubv(sel ? (GLubyte *)col1 : (GLubyte *)col2);
								lastsel = sel;
							}
							luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
							glVertex2fv(luv->uv);
							luv = CustomData_bmesh_get(&bm->ldata, l->next->head.data, CD_MLOOPUV);
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
				
					glBegin(GL_LINE_LOOP);
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
						glVertex2fv(luv->uv);
					}
					glEnd();
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

			if (!uvedit_face_select_test(scene, em, efa)) {
				uv_poly_center(em, efa, cent);
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

			if (uvedit_face_select_test(scene, em, efa)) {
				uv_poly_center(em, efa, cent);
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
				luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
				if (!uvedit_uv_select_test(em, scene, l))
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
				luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

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
				luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);

				if (uvedit_uv_select_test(em, scene, l))
					bglVertex2fv(luv->uv);
			}
		}
		bglEnd();
	}

	glPointSize(1.0);
}

void draw_uvedit_main(SpaceImage *sima, ARegion *ar, Scene *scene, Object *obedit, Object *obact)
{
	ToolSettings *toolsettings = scene->toolsettings;
	int show_uvedit, show_uvshadow, show_texpaint_uvshadow;

	show_texpaint_uvshadow = (obact && obact->type == OB_MESH && obact->mode == OB_MODE_TEXTURE_PAINT);
	show_uvedit = ED_space_image_show_uvedit(sima, obedit);
	show_uvshadow = ED_space_image_show_uvshadow(sima, obedit);

	if (show_uvedit || show_uvshadow || show_texpaint_uvshadow) {
		if (show_uvshadow)
			draw_uvs_shadow(obedit);
		else if (show_uvedit)
			draw_uvs(sima, scene, obedit);
		else
			draw_uvs_texpaint(sima, scene, obact);

		if (show_uvedit && !(toolsettings->use_uv_sculpt))
			draw_image_cursor(sima, ar);
	}
}

