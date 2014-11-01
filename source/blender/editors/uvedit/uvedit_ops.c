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
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/uvedit/uvedit_ops.c
 *  \ingroup eduv
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_image_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_lasso.h"
#include "BLI_blenlib.h"
#include "BLI_array.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh_mapping.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_editmesh.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_uvedit.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "uvedit_intern.h"

static void uv_select_all_perform(Scene *scene, Image *ima, BMEditMesh *em, int action);
static void uv_select_flush_from_tag_face(SpaceImage *sima, Scene *scene, Object *obedit, const bool select);
static void uv_select_flush_from_tag_loop(SpaceImage *sima, Scene *scene, Object *obedit, const bool select);

/************************* state testing ************************/

bool ED_uvedit_test(Object *obedit)
{
	BMEditMesh *em;
	int ret;

	if (!obedit)
		return 0;
	
	if (obedit->type != OB_MESH)
		return 0;

	em = BKE_editmesh_from_object(obedit);
	ret = EDBM_mtexpoly_check(em);
	
	return ret;
}

static int ED_operator_uvedit_can_uv_sculpt(struct bContext *C)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ToolSettings *toolsettings = CTX_data_tool_settings(C);
	Object *obedit = CTX_data_edit_object(C);

	return ED_space_image_show_uvedit(sima, obedit) && !(toolsettings->use_uv_sculpt);
}

static int UNUSED_FUNCTION(ED_operator_uvmap_mesh) (bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (CustomData_get_layer(&me->fdata, CD_MTFACE) != NULL)
			return 1;
	}

	return 0;
}
/**************************** object active image *****************************/

static bool is_image_texture_node(bNode *node)
{
	return ELEM(node->type, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT);
}

bool ED_object_get_active_image(Object *ob, int mat_nr,
                                Image **r_ima, ImageUser **r_iuser, bNode **r_node, bNodeTree **r_ntree)
{
	Material *ma = give_current_material(ob, mat_nr);
	bNodeTree *ntree = (ma && ma->use_nodes) ? ma->nodetree : NULL;
	bNode *node = (ntree) ? nodeGetActiveTexture(ntree) : NULL;

	if (node && is_image_texture_node(node)) {
		if (r_ima) *r_ima = (Image *)node->id;
		if (r_iuser) *r_iuser = NULL;
		if (r_node) *r_node = node;
		if (r_ntree) *r_ntree = ntree;
		return true;
	}
	
	if (r_ima) *r_ima = NULL;
	if (r_iuser) *r_iuser = NULL;
	if (r_node) *r_node = node;
	if (r_ntree) *r_ntree = ntree;

	return false;
}

void ED_object_assign_active_image(Main *bmain, Object *ob, int mat_nr, Image *ima)
{
	Material *ma = give_current_material(ob, mat_nr);
	bNode *node = (ma && ma->use_nodes) ? nodeGetActiveTexture(ma->nodetree) : NULL;

	if (node && is_image_texture_node(node)) {
		node->id = &ima->id;
		ED_node_tag_update_nodetree(bmain, ma->nodetree);
	}
}

/************************* assign image ************************/

//#define USE_SWITCH_ASPECT

void ED_uvedit_assign_image(Main *UNUSED(bmain), Scene *scene, Object *obedit, Image *ima, Image *previma)
{
	BMEditMesh *em;
	BMIter iter;
	MTexPoly *tf;
	bool update = false;
	const bool selected = !(scene->toolsettings->uv_flag & UV_SYNC_SELECTION);
	
	/* skip assigning these procedural images... */
	if (ima && (ima->type == IMA_TYPE_R_RESULT || ima->type == IMA_TYPE_COMPOSITE))
		return;

	/* verify we have a mesh we can work with */
	if (!obedit || (obedit->type != OB_MESH))
		return;

	em = BKE_editmesh_from_object(obedit);
	if (!em || !em->bm->totface) {
		return;
	}

	if (BKE_scene_use_new_shading_nodes(scene)) {
		/* new shading system, do not assign anything */
	}
	else {
		BMFace *efa;

		int cd_loop_uv_offset;
		int cd_poly_tex_offset;

		/* old shading system, assign image to selected faces */
#ifdef USE_SWITCH_ASPECT
		float prev_aspect[2], fprev_aspect;
		float aspect[2], faspect;

		ED_image_get_uv_aspect(previma, prev_aspect, prev_aspect + 1);
		ED_image_get_uv_aspect(ima, aspect, aspect + 1);

		fprev_aspect = prev_aspect[0] / prev_aspect[1];
		faspect = aspect[0] / aspect[1];
#endif

		/* ensure we have a uv map */
		if (!CustomData_has_layer(&em->bm->pdata, CD_MTEXPOLY)) {
			BM_data_layer_add(em->bm, &em->bm->pdata, CD_MTEXPOLY);
			BM_data_layer_add(em->bm, &em->bm->ldata, CD_MLOOPUV);
			/* make UVs all nice 0-1 */
			ED_mesh_uv_loop_reset_ex(obedit->data, CustomData_get_active_layer_index(&em->bm->pdata, CD_MTEXPOLY));
			update = true;
		}

		cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
		cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

		/* now assign to all visible faces */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (uvedit_face_visible_test(scene, previma, efa, tf) &&
			    (selected == true || uvedit_face_select_test(scene, efa, cd_loop_uv_offset)))
			{
				if (ima) {
					tf->tpage = ima;
					
					if (ima->id.us == 0) id_us_plus(&ima->id);
					else id_lib_extern(&ima->id);

#ifdef USE_SWITCH_ASPECT
					/* we also need to correct the aspect of uvs */
					if (scene->toolsettings->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT) {
						/* do nothing */
					}
					else {
						BMIter liter;
						BMLoop *l;

						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

							luv->uv[0] *= fprev_aspect;
							luv->uv[0] /= faspect;
						}
					}
#endif
				}
				else {
					tf->tpage = NULL;
				}

				update = true;
			}
		}

		/* and update depdency graph */
		if (update) {
			DAG_id_tag_update(obedit->data, 0);
		}
	}

}

/* dotile - 1, set the tile flag (from the space image)
 *          2, set the tile index for the faces. */
static bool uvedit_set_tile(Object *obedit, Image *ima, int curtile)
{
	BMEditMesh *em;
	BMFace *efa;
	BMIter iter;
	MTexPoly *tf;
	int cd_poly_tex_offset;
	
	/* verify if we have something to do */
	if (!ima || !ED_uvedit_test(obedit))
		return false;

	if ((ima->tpageflag & IMA_TILES) == 0)
		return false;

	/* skip assigning these procedural images... */
	if (ima->type == IMA_TYPE_R_RESULT || ima->type == IMA_TYPE_COMPOSITE)
		return false;
	
	em = BKE_editmesh_from_object(obedit);

	cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

		if (BM_elem_flag_test(efa, BM_ELEM_SELECT))
			tf->tile = curtile;  /* set tile index */
	}

	DAG_id_tag_update(obedit->data, 0);

	return true;
}

/*********************** space conversion *********************/

static void uvedit_pixel_to_float(SpaceImage *sima, float *dist, float pixeldist)
{
	int width, height;

	if (sima) {
		ED_space_image_get_size(sima, &width, &height);
	}
	else {
		width =  IMG_SIZE_FALLBACK;
		height = IMG_SIZE_FALLBACK;
	}

	dist[0] = pixeldist / width;
	dist[1] = pixeldist / height;
}

/*************** visibility and selection utilities **************/

bool uvedit_face_visible_nolocal(Scene *scene, BMFace *efa)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION)
		return (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) == 0);
	else
		return (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) == 0 && BM_elem_flag_test(efa, BM_ELEM_SELECT));
}

bool uvedit_face_visible_test(Scene *scene, Image *ima, BMFace *efa, MTexPoly *tf)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SHOW_SAME_IMAGE)
		return (tf->tpage == ima) ? uvedit_face_visible_nolocal(scene, efa) : false;
	else
		return uvedit_face_visible_nolocal(scene, efa);
}

bool uvedit_face_select_test(Scene *scene, BMFace *efa,
                             const int cd_loop_uv_offset)
{
	ToolSettings *ts = scene->toolsettings;
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		return (BM_elem_flag_test(efa, BM_ELEM_SELECT));
	}
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			if (!(luv->flag & MLOOPUV_VERTSEL))
				return false;
		}

		return true;
	}
}

bool uvedit_face_select_set(struct Scene *scene, struct BMEditMesh *em, struct BMFace *efa, const bool select,
                            const bool do_history, const int cd_loop_uv_offset)
{
	if (select) {
		return uvedit_face_select_enable(scene, em, efa, do_history, cd_loop_uv_offset);
	}
	else {
		return uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
	}
}

bool uvedit_face_select_enable(Scene *scene, BMEditMesh *em, BMFace *efa, const bool do_history,
                               const int cd_loop_uv_offset)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		BM_face_select_set(em->bm, efa, true);
		if (do_history) {
			BM_select_history_store(em->bm, (BMElem *)efa);
		}
	}
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			luv->flag |= MLOOPUV_VERTSEL;
		}

		return true;
	}

	return false;
}

bool uvedit_face_select_disable(Scene *scene, BMEditMesh *em, BMFace *efa,
                                const int cd_loop_uv_offset)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		BM_face_select_set(em->bm, efa, false);
	}
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			luv->flag &= ~MLOOPUV_VERTSEL;
		}

		return true;
	}

	return false;
}

bool uvedit_edge_select_test(Scene *scene, BMLoop *l,
                             const int cd_loop_uv_offset)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE) {
			return BM_elem_flag_test(l->f, BM_ELEM_SELECT);
		}
		else if (ts->selectmode == SCE_SELECT_EDGE) {
			return BM_elem_flag_test(l->e, BM_ELEM_SELECT);
		}
		else {
			return BM_elem_flag_test(l->v, BM_ELEM_SELECT) && 
			       BM_elem_flag_test(l->next->v, BM_ELEM_SELECT);
		}
	}
	else {
		MLoopUV *luv1, *luv2;

		luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

		return (luv1->flag & MLOOPUV_VERTSEL) && (luv2->flag & MLOOPUV_VERTSEL);
	}
}

void uvedit_edge_select_set(BMEditMesh *em, Scene *scene, BMLoop *l, const bool select,
                            const bool do_history, const int cd_loop_uv_offset)

{
	if (select) {
		uvedit_edge_select_enable(em, scene, l, do_history, cd_loop_uv_offset);
	}
	else {
		uvedit_edge_select_disable(em, scene, l, cd_loop_uv_offset);
	}
}

void uvedit_edge_select_enable(BMEditMesh *em, Scene *scene, BMLoop *l, const bool do_history,
                               const int cd_loop_uv_offset)

{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			BM_face_select_set(em->bm, l->f, true);
		else if (ts->selectmode & SCE_SELECT_EDGE)
			BM_edge_select_set(em->bm, l->e, true);
		else {
			BM_vert_select_set(em->bm, l->e->v1, true);
			BM_vert_select_set(em->bm, l->e->v2, true);
		}

		if (do_history) {
			BM_select_history_store(em->bm, (BMElem *)l->e);
		}
	}
	else {
		MLoopUV *luv1, *luv2;

		luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
		
		luv1->flag |= MLOOPUV_VERTSEL;
		luv2->flag |= MLOOPUV_VERTSEL;
	}
}

void uvedit_edge_select_disable(BMEditMesh *em, Scene *scene, BMLoop *l,
                                const int cd_loop_uv_offset)

{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			BM_face_select_set(em->bm, l->f, false);
		else if (ts->selectmode & SCE_SELECT_EDGE)
			BM_edge_select_set(em->bm, l->e, false);
		else {
			BM_vert_select_set(em->bm, l->e->v1, false);
			BM_vert_select_set(em->bm, l->e->v2, false);
		}
	}
	else {
		MLoopUV *luv1, *luv2;

		luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
		
		luv1->flag &= ~MLOOPUV_VERTSEL;
		luv2->flag &= ~MLOOPUV_VERTSEL;
	}
}

bool uvedit_uv_select_test(Scene *scene, BMLoop *l,
                           const int cd_loop_uv_offset)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			return BM_elem_flag_test_bool(l->f, BM_ELEM_SELECT);
		else
			return BM_elem_flag_test_bool(l->v, BM_ELEM_SELECT);
	}
	else {
		MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		return (luv->flag & MLOOPUV_VERTSEL) != 0;
	}
}

void uvedit_uv_select_set(BMEditMesh *em, Scene *scene, BMLoop *l, const bool select,
                          const bool do_history, const int cd_loop_uv_offset)
{
	if (select) {
		uvedit_uv_select_enable(em, scene, l, do_history, cd_loop_uv_offset);
	}
	else {
		uvedit_uv_select_disable(em, scene, l, cd_loop_uv_offset);
	}
}

void uvedit_uv_select_enable(BMEditMesh *em, Scene *scene, BMLoop *l,
                             const bool do_history, const int cd_loop_uv_offset)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			BM_face_select_set(em->bm, l->f, true);
		else
			BM_vert_select_set(em->bm, l->v, true);

		if (do_history) {
			BM_select_history_remove(em->bm, (BMElem *)l->v);
		}
	}
	else {
		MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		luv->flag |= MLOOPUV_VERTSEL;
	}
}

void uvedit_uv_select_disable(BMEditMesh *em, Scene *scene, BMLoop *l,
                              const int cd_loop_uv_offset)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			BM_face_select_set(em->bm, l->f, false);
		else
			BM_vert_select_set(em->bm, l->v, false);
	}
	else {
		MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		luv->flag &= ~MLOOPUV_VERTSEL;
	}
}

/*********************** live unwrap utilities ***********************/

void uvedit_live_unwrap_update(SpaceImage *sima, Scene *scene, Object *obedit)
{
	if (sima && (sima->flag & SI_LIVE_UNWRAP)) {
		ED_uvedit_live_unwrap_begin(scene, obedit);
		ED_uvedit_live_unwrap_re_solve();
		ED_uvedit_live_unwrap_end(0);
	}
}

/*********************** geometric utilities ***********************/
void uv_poly_center(BMFace *f, float r_cent[2], const int cd_loop_uv_offset)
{
	BMLoop *l;
	MLoopUV *luv;
	BMIter liter;

	zero_v2(r_cent);

	BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
		luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		add_v2_v2(r_cent, luv->uv);
	}

	mul_v2_fl(r_cent, 1.0f / (float)f->len);
}

void uv_poly_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		uv[i][0] = uv_orig[i][0] * aspx;
		uv[i][1] = uv_orig[i][1] * aspy;
	}
}

bool ED_uvedit_minmax(Scene *scene, Image *ima, Object *obedit, float r_min[2], float r_max[2])
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	bool changed = false;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	INIT_MINMAX2(r_min, r_max);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				minmax_v2v2_v2(r_min, r_max, luv->uv);
				changed = true;
			}
		}
	}

	return changed;
}

/* Be careful when using this, it bypasses all synchronization options */
void ED_uvedit_select_all(BMesh *bm)
{
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;

	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			luv->flag |= MLOOPUV_VERTSEL;
		}
	}
}

static bool ED_uvedit_median(Scene *scene, Image *ima, Object *obedit, float co[2])
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	unsigned int sel = 0;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	zero_v2(co);
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
				add_v2_v2(co, luv->uv);
				sel++;
			}
		}
	}

	mul_v2_fl(co, 1.0f / (float)sel);

	return (sel != 0);
}

static bool uvedit_center(Scene *scene, Image *ima, Object *obedit, float cent[2], char mode)
{
	bool changed = false;
	
	if (mode == V3D_CENTER) {  /* bounding box */
		float min[2], max[2];
		if (ED_uvedit_minmax(scene, ima, obedit, min, max)) {
			mid_v2_v2v2(cent, min, max);
			changed = true;
		}
	}
	else {
		if (ED_uvedit_median(scene, ima, obedit, cent)) {
			changed = true;
		}
	}

	return changed;
}

/************************** find nearest ****************************/

void uv_find_nearest_edge(Scene *scene, Image *ima, BMEditMesh *em, const float co[2], NearestHit *hit)
{
	MTexPoly *tf;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv, *luv_next;
	float mindist_squared, dist_squared;
	int i;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	mindist_squared = 1e10f;
	memset(hit, 0, sizeof(*hit));

	BM_mesh_elem_index_ensure(em->bm, BM_VERT);
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
			luv      = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

			dist_squared = dist_squared_to_line_segment_v2(co, luv->uv, luv_next->uv);

			if (dist_squared < mindist_squared) {
				hit->tf = tf;
				hit->efa = efa;
				
				hit->l = l;
				hit->luv = luv;
				hit->luv_next = luv_next;
				hit->lindex = i;

				mindist_squared = dist_squared;
			}
		}
	}
}

static void uv_find_nearest_face(Scene *scene, Image *ima, BMEditMesh *em, const float co[2], NearestHit *hit)
{
	MTexPoly *tf;
	BMFace *efa;
	BMIter iter;
	float mindist, dist, cent[2];

	const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	mindist = 1e10f;
	memset(hit, 0, sizeof(*hit));

	/*this will fill in hit.vert1 and hit.vert2*/
	uv_find_nearest_edge(scene, ima, em, co, hit);
	hit->l = NULL;
	hit->luv = hit->luv_next = NULL;

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;

		uv_poly_center(efa, cent, cd_loop_uv_offset);

		dist = len_manhattan_v2v2(co, cent);

		if (dist < mindist) {
			hit->tf = tf;
			hit->efa = efa;
			mindist = dist;
		}
	}
}

static bool uv_nearest_between(const BMLoop *l, const float co[2],
                               const int cd_loop_uv_offset)
{
	const float *uv_prev = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset))->uv;
	const float *uv_curr = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l,       cd_loop_uv_offset))->uv;
	const float *uv_next = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset))->uv;

	return ((line_point_side_v2(uv_prev, uv_curr, co) >  0.0f) &&
	        (line_point_side_v2(uv_next, uv_curr, co) <= 0.0f));
}

void uv_find_nearest_vert(Scene *scene, Image *ima, BMEditMesh *em,
                          float const co[2], const float penalty[2], NearestHit *hit)
{
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float mindist, dist;
	int i;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	/*this will fill in hit.vert1 and hit.vert2*/
	uv_find_nearest_edge(scene, ima, em, co, hit);
	hit->l = NULL;
	hit->luv = hit->luv_next = NULL;

	mindist = 1e10f;
	memset(hit, 0, sizeof(*hit));
	
	BM_mesh_elem_index_ensure(em->bm, BM_VERT);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;

		BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			if (penalty && uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
				dist = len_manhattan_v2v2(co, luv->uv) + len_manhattan_v2(penalty);
			else
				dist = len_manhattan_v2v2(co, luv->uv);

			if (dist <= mindist) {
				if (dist == mindist) {
					if (!uv_nearest_between(l, co, cd_loop_uv_offset)) {
						continue;
					}
				}

				mindist = dist;

				hit->l = l;
				hit->luv = luv;
				hit->luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
				hit->tf = tf;
				hit->efa = efa;
				hit->lindex = i;
			}
		}
	}
}

bool ED_uvedit_nearest_uv(Scene *scene, Object *obedit, Image *ima, const float co[2], float r_uv[2])
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float mindist, dist;
	bool found = false;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	mindist = 1e10f;
	copy_v2_v2(r_uv, co);
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			dist = len_manhattan_v2v2(co, luv->uv);

			if (dist <= mindist) {
				mindist = dist;

				copy_v2_v2(r_uv, luv->uv);
				found = true;
			}
		}
	}

	return found;
}

/*********************** loop select ***********************/

static void uv_select_edgeloop_vertex_loop_flag(UvMapVert *first)
{
	UvMapVert *iterv;
	int count = 0;

	for (iterv = first; iterv; iterv = iterv->next) {
		if (iterv->separate && iterv != first)
			break;

		count++;
	}
	
	if (count < 5)
		first->flag = 1;
}

static UvMapVert *uv_select_edgeloop_vertex_map_get(UvVertMap *vmap, BMFace *efa, BMLoop *l)
{
	UvMapVert *iterv, *first;
	first = BM_uv_vert_map_at_index(vmap,  BM_elem_index_get(l->v));

	for (iterv = first; iterv; iterv = iterv->next) {
		if (iterv->separate)
			first = iterv;
		if (iterv->f == BM_elem_index_get(efa))
			return first;
	}
	
	return NULL;
}

static bool uv_select_edgeloop_edge_tag_faces(BMEditMesh *em, UvMapVert *first1, UvMapVert *first2, int *totface)
{
	UvMapVert *iterv1, *iterv2;
	BMFace *efa;
	int tot = 0;

	/* count number of faces this edge has */
	for (iterv1 = first1; iterv1; iterv1 = iterv1->next) {
		if (iterv1->separate && iterv1 != first1)
			break;

		for (iterv2 = first2; iterv2; iterv2 = iterv2->next) {
			if (iterv2->separate && iterv2 != first2)
				break;

			if (iterv1->f == iterv2->f) {
				/* if face already tagged, don't do this edge */
				efa = BM_face_at_index(em->bm, iterv1->f);
				if (BM_elem_flag_test(efa, BM_ELEM_TAG))
					return false;

				tot++;
				break;
			}
		}
	}

	if (*totface == 0) /* start edge */
		*totface = tot;
	else if (tot != *totface) /* check for same number of faces as start edge */
		return false;

	/* tag the faces */
	for (iterv1 = first1; iterv1; iterv1 = iterv1->next) {
		if (iterv1->separate && iterv1 != first1)
			break;

		for (iterv2 = first2; iterv2; iterv2 = iterv2->next) {
			if (iterv2->separate && iterv2 != first2)
				break;

			if (iterv1->f == iterv2->f) {
				efa = BM_face_at_index(em->bm, iterv1->f);
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
				break;
			}
		}
	}

	return true;
}

static int uv_select_edgeloop(Scene *scene, Image *ima, BMEditMesh *em, NearestHit *hit,
                              const float limit[2], const bool extend)
{
	BMFace *efa;
	BMIter iter, liter;
	BMLoop *l;
	MTexPoly *tf;
	UvVertMap *vmap;
	UvMapVert *iterv_curr;
	UvMapVert *iterv_next;
	int starttotf;
	bool looking, select;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	/* setup */
	BM_mesh_elem_table_ensure(em->bm, BM_FACE);
	vmap = BM_uv_vert_map_create(em->bm, 0, limit);

	BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_FACE);

	if (!extend) {
		uv_select_all_perform(scene, ima, em, SEL_DESELECT);
	}

	BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);

	/* set flags for first face and verts */
	iterv_curr = uv_select_edgeloop_vertex_map_get(vmap, hit->efa, hit->l);
	iterv_next = uv_select_edgeloop_vertex_map_get(vmap, hit->efa, hit->l->next);
	uv_select_edgeloop_vertex_loop_flag(iterv_curr);
	uv_select_edgeloop_vertex_loop_flag(iterv_next);

	starttotf = 0;
	uv_select_edgeloop_edge_tag_faces(em, iterv_curr, iterv_next, &starttotf);

	/* sorry, first edge isn't even ok */
	looking = !(iterv_curr->flag == 0 && iterv_next->flag == 0);

	/* iterate */
	while (looking) {
		looking = false;

		/* find correct valence edges which are not tagged yet, but connect to tagged one */

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (!BM_elem_flag_test(efa, BM_ELEM_TAG) && uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					/* check face not hidden and not tagged */
					if (!(iterv_curr = uv_select_edgeloop_vertex_map_get(vmap, efa, l)))
						continue;
					if (!(iterv_next = uv_select_edgeloop_vertex_map_get(vmap, efa, l->next)))
						continue;

					/* check if vertex is tagged and has right valence */
					if (iterv_curr->flag || iterv_next->flag) {
						if (uv_select_edgeloop_edge_tag_faces(em, iterv_curr, iterv_next, &starttotf)) {
							looking = true;
							BM_elem_flag_enable(efa, BM_ELEM_TAG);

							uv_select_edgeloop_vertex_loop_flag(iterv_curr);
							uv_select_edgeloop_vertex_loop_flag(iterv_next);
							break;
						}
					}
				}
			}
		}
	}

	/* do the actual select/deselect */
	iterv_curr = uv_select_edgeloop_vertex_map_get(vmap, hit->efa, hit->l);
	iterv_next = uv_select_edgeloop_vertex_map_get(vmap, hit->efa, hit->l->next);
	iterv_curr->flag = 1;
	iterv_next->flag = 1;

	if (extend) {
		select = !(uvedit_uv_select_test(scene, hit->l, cd_loop_uv_offset));
	}
	else {
		select = true;
	}
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			iterv_curr = uv_select_edgeloop_vertex_map_get(vmap, efa, l);

			if (iterv_curr->flag) {
				uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
			}
		}
	}

	/* cleanup */
	BM_uv_vert_map_free(vmap);

	return (select) ? 1 : -1;
}

/*********************** linked select ***********************/

static void uv_select_linked(Scene *scene, Image *ima, BMEditMesh *em, const float limit[2], NearestHit *hit, bool extend)
{
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	UvVertMap *vmap;
	UvMapVert *vlist, *iterv, *startv;
	int i, stacksize = 0, *stack;
	unsigned int a;
	char *flag;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	BM_mesh_elem_table_ensure(em->bm, BM_FACE); /* we can use this too */
	vmap = BM_uv_vert_map_create(em->bm, 1, limit);

	if (vmap == NULL)
		return;

	stack = MEM_mallocN(sizeof(*stack) * (em->bm->totface + 1), "UvLinkStack");
	flag = MEM_callocN(sizeof(*flag) * em->bm->totface, "UvLinkFlag");

	if (!hit) {
		BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

					if (luv->flag & MLOOPUV_VERTSEL) {
						stack[stacksize] = a;
						stacksize++;
						flag[a] = 1;

						break;
					}
				}
			}
		}
	}
	else {
		BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
			if (efa == hit->efa) {
				stack[stacksize] = a;
				stacksize++;
				flag[a] = 1;
				break;
			}
		}
	}

	while (stacksize > 0) {

		stacksize--;
		a = stack[stacksize];

		efa = BM_face_at_index(em->bm, a);

		BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {

			/* make_uv_vert_map_EM sets verts tmp.l to the indices */
			vlist = BM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));
			
			startv = vlist;

			for (iterv = vlist; iterv; iterv = iterv->next) {
				if (iterv->separate)
					startv = iterv;
				if (iterv->f == a)
					break;
			}

			for (iterv = startv; iterv; iterv = iterv->next) {
				if ((startv != iterv) && (iterv->separate))
					break;
				else if (!flag[iterv->f]) {
					flag[iterv->f] = 1;
					stack[stacksize] = iterv->f;
					stacksize++;
				}
			}
		}
	}

	if (!extend) {
		BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				
				if (flag[a])
					luv->flag |= MLOOPUV_VERTSEL;
				else
					luv->flag &= ~MLOOPUV_VERTSEL;
			}
		}
	}
	else {
		BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
			if (!flag[a]) {
				continue;
			}
			
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						
				if (luv->flag & MLOOPUV_VERTSEL) {
					break;
				}
			}
			
			if (l) {
				break;
			}
		}

		if (efa) {
			BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
				if (!flag[a]) {
					continue;
				}

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					
					luv->flag &= ~MLOOPUV_VERTSEL;
				}
			}
		}
		else {
			BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
				if (!flag[a]) {
					continue;
				}

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					
					luv->flag |= MLOOPUV_VERTSEL;
				}
			}
		}
	}
	
	MEM_freeN(stack);
	MEM_freeN(flag);
	BM_uv_vert_map_free(vmap);
}

/* WATCH IT: this returns first selected UV,
 * not ideal in many cases since there could be multiple */
static float *uv_sel_co_from_eve(Scene *scene, Image *ima, BMEditMesh *em, BMVert *eve)
{
	BMIter liter;
	BMLoop *l;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	BM_ITER_ELEM (l, &liter, eve, BM_LOOPS_OF_VERT) {
		MTexPoly *tf = BM_ELEM_CD_GET_VOID_P(l->f, cd_poly_tex_offset);

		if (!uvedit_face_visible_test(scene, ima, l->f, tf))
			continue;

		if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
			MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			return luv->uv;
		}
	}

	return NULL;
}

static int uv_select_more_less(bContext *C, const bool select)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	SpaceImage *sima = CTX_wm_space_image(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	ToolSettings *ts = scene->toolsettings;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (select) {
			EDBM_select_more(em);
		}
		else {
			EDBM_select_less(em);
		}

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		return OPERATOR_FINISHED;
	}

	if (ts->uv_selectmode == UV_SELECT_FACE) {

		/* clear tags */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_elem_flag_disable(efa, BM_ELEM_TAG);
		}

		/* mark loops to be selected */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			MTexPoly *tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (uvedit_face_visible_test(scene, ima, efa, tf)) {

#define IS_SEL   1
#define IS_UNSEL 2

				int sel_state = 0;

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					if (luv->flag & MLOOPUV_VERTSEL) {
						sel_state |= IS_SEL;
					}
					else {
						sel_state |= IS_UNSEL;
					}

					/* if we have a mixed selection, tag to grow it */
					if (sel_state == (IS_SEL | IS_UNSEL)) {
						BM_elem_flag_enable(efa, BM_ELEM_TAG);
						break;
					}
				}

#undef IS_SEL
#undef IS_UNSEL

			}
		}

		/* select tagged faces */
		uv_select_flush_from_tag_face(sima, scene, obedit, select);
	}
	else {

		/* clear tags */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				BM_elem_flag_disable(l, BM_ELEM_TAG);
			}
		}

		/* mark loops to be selected */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			MTexPoly *tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

					MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

					if (((luv->flag & MLOOPUV_VERTSEL) != 0) == select) {
						BM_elem_flag_enable(l->next, BM_ELEM_TAG);
						BM_elem_flag_enable(l->prev, BM_ELEM_TAG);
					}
				}
			}
		}

		/* select tagged loops */
		uv_select_flush_from_tag_loop(sima, scene, obedit, select);
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static int uv_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	return uv_select_more_less(C, true);
}

static void UV_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->description = "Select more UV vertices connected to initial selection";
	ot->idname = "UV_OT_select_more";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = uv_select_more_exec;
	ot->poll = ED_operator_uvedit_space_image;
}

static int uv_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	return uv_select_more_less(C, false);
}

static void UV_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->description = "Deselect UV vertices at the boundary of each selection region";
	ot->idname = "UV_OT_select_less";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = uv_select_less_exec;
	ot->poll = ED_operator_uvedit_space_image;
}

/* ******************** align operator **************** */

static void uv_weld_align(bContext *C, int tool)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	SpaceImage *sima;
	Scene *scene;
	Image *ima;
	MTexPoly *tf;
	float cent[2], min[2], max[2];

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	scene = CTX_data_scene(C);
	ima = CTX_data_edit_image(C);
	sima = CTX_wm_space_image(C);

	INIT_MINMAX2(min, max);

	if (tool == 'a') {
		BMIter iter, liter;
		BMFace *efa;
		BMLoop *l;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
					MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					minmax_v2v2_v2(min, max, luv->uv);
				}
			}
		}

		tool = (max[0] - min[0] >= max[1] - min[1]) ? 'y' : 'x';
	}

	uvedit_center(scene, ima, obedit, cent, 0);

	if (tool == 'x' || tool == 'w') {
		BMIter iter, liter;
		BMFace *efa;
		BMLoop *l;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
					MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					luv->uv[0] = cent[0];
				}

			}
		}
	}

	if (tool == 'y' || tool == 'w') {
		BMIter iter, liter;
		BMFace *efa;
		BMLoop *l;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
					MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					luv->uv[1] = cent[1];
				}

			}
		}
	}

	if (tool == 's' || tool == 't' || tool == 'u') {
		BMEdge *eed;
		BMLoop *l;
		BMVert *eve;
		BMVert *eve_start;
		BMIter iter, liter, eiter;

		/* clear tag */
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}

		/* tag verts with a selected UV */
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			BM_ITER_ELEM (l, &liter, eve, BM_LOOPS_OF_VERT) {
				tf = BM_ELEM_CD_GET_VOID_P(l->f, cd_poly_tex_offset);

				if (!uvedit_face_visible_test(scene, ima, l->f, tf))
					continue;

				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
					BM_elem_flag_enable(eve, BM_ELEM_TAG);
					break;
				}
			}
		}

		/* flush vertex tags to edges */
		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			BM_elem_flag_set(eed, BM_ELEM_TAG, (BM_elem_flag_test(eed->v1, BM_ELEM_TAG) &&
			                                    BM_elem_flag_test(eed->v2, BM_ELEM_TAG)));
		}

		/* find a vertex with only one tagged edge */
		eve_start = NULL;
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			int tot_eed_tag = 0;
			BM_ITER_ELEM (eed, &eiter, eve, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(eed, BM_ELEM_TAG)) {
					tot_eed_tag++;
				}
			}

			if (tot_eed_tag == 1) {
				eve_start = eve;
				break;
			}
		}

		if (eve_start) {
			BMVert **eve_line = NULL;
			BMVert *eve_next = NULL;
			BLI_array_declare(eve_line);
			int i;

			eve = eve_start;

			/* walk over edges, building an array of verts in a line */
			while (eve) {
				BLI_array_append(eve_line, eve);
				/* don't touch again */
				BM_elem_flag_disable(eve, BM_ELEM_TAG);

				eve_next = NULL;

				/* find next eve */
				BM_ITER_ELEM (eed, &eiter, eve, BM_EDGES_OF_VERT) {
					if (BM_elem_flag_test(eed, BM_ELEM_TAG)) {
						BMVert *eve_other = BM_edge_other_vert(eed, eve);
						if (BM_elem_flag_test(eve_other, BM_ELEM_TAG)) {
							/* this is a tagged vert we didnt walk over yet, step onto it */
							eve_next = eve_other;
							break;
						}
					}
				}

				eve = eve_next;
			}

			/* now we have all verts, make into a line */
			if (BLI_array_count(eve_line) > 2) {

				/* we know the returns from these must be valid */
				const float *uv_start = uv_sel_co_from_eve(scene, ima, em, eve_line[0]);
				const float *uv_end   = uv_sel_co_from_eve(scene, ima, em, eve_line[BLI_array_count(eve_line) - 1]);
				/* For t & u modes */
				float a = 0.0f;

				if (tool == 't') {
					if (uv_start[1] == uv_end[1])
						tool = 's';
					else
						a = (uv_end[0] - uv_start[0]) / (uv_end[1] - uv_start[1]);
				}
				else if (tool == 'u') {
					if (uv_start[0] == uv_end[0])
						tool = 's';
					else
						a = (uv_end[1] - uv_start[1]) / (uv_end[0] - uv_start[0]);
				}

				/* go over all verts except for endpoints */
				for (i = 0; i < BLI_array_count(eve_line); i++) {
					BM_ITER_ELEM (l, &liter, eve_line[i], BM_LOOPS_OF_VERT) {
						tf = BM_ELEM_CD_GET_VOID_P(l->f, cd_poly_tex_offset);

						if (!uvedit_face_visible_test(scene, ima, l->f, tf))
							continue;

						if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
							MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							/* Projection of point (x, y) over line (x1, y1, x2, y2) along X axis:
							 * new_y = (y2 - y1) / (x2 - x1) * (x - x1) + y1
							 * Maybe this should be a BLI func? Or is it already existing?
							 * Could use interp_v2_v2v2, but not sure it's worth it here...*/
							if (tool == 't')
								luv->uv[0] = a * (luv->uv[1] - uv_start[1]) + uv_start[0];
							else if (tool == 'u')
								luv->uv[1] = a * (luv->uv[0] - uv_start[0]) + uv_start[1];
							else
								closest_to_line_segment_v2(luv->uv, luv->uv, uv_start, uv_end);
						}
					}
				}
			}
			else {
				/* error - not a line, needs 3+ points  */
			}

			if (eve_line) {
				MEM_freeN(eve_line);
			}
		}
		else {
			/* error - cant find an endpoint */
		}
	}


	uvedit_live_unwrap_update(sima, scene, obedit);
	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
}

static int uv_align_exec(bContext *C, wmOperator *op)
{
	uv_weld_align(C, RNA_enum_get(op->ptr, "axis"));

	return OPERATOR_FINISHED;
}

static void UV_OT_align(wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[] = {
		{'s', "ALIGN_S", 0, "Straighten", "Align UVs along the line defined by the endpoints"},
		{'t', "ALIGN_T", 0, "Straighten X", "Align UVs along the line defined by the endpoints along the X axis"},
		{'u', "ALIGN_U", 0, "Straighten Y", "Align UVs along the line defined by the endpoints along the Y axis"},
		{'a', "ALIGN_AUTO", 0, "Align Auto", "Automatically choose the axis on which there is most alignment already"},
		{'x', "ALIGN_X", 0, "Align X", "Align UVs on X axis"},
		{'y', "ALIGN_Y", 0, "Align Y", "Align UVs on Y axis"},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Align";
	ot->description = "Align selected UV vertices to an axis";
	ot->idname = "UV_OT_align";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_align_exec;
	ot->poll = ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "axis", axis_items, 'a', "Axis", "Axis to align UV locations on");
}
/* ******************** weld near operator **************** */

typedef struct UVvert {
	MLoopUV *uv_loop;
	bool weld;
} UVvert;

static int uv_remove_doubles_exec(bContext *C, wmOperator *op)
{
	const float threshold = RNA_float_get(op->ptr, "threshold");
	const bool use_unselected = RNA_boolean_get(op->ptr, "use_unselected");

	SpaceImage *sima;
	Scene *scene;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	Image *ima;
	MTexPoly *tf;
	int uv_a_index;
	int uv_b_index;
	float *uv_a;
	const float *uv_b;

	BMIter iter, liter;
	BMFace *efa;
	BMLoop *l;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	sima = CTX_wm_space_image(C);
	scene = CTX_data_scene(C);
	ima = CTX_data_edit_image(C);

	if (use_unselected == false) {
		UVvert *vert_arr = NULL;
		BLI_array_declare(vert_arr);
		MLoopUV **loop_arr = NULL;
		BLI_array_declare(loop_arr);

		/* TODO, use qsort as with MESH_OT_remove_doubles, this isn't optimal */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
					MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					UVvert vert;
					vert.uv_loop = luv;
					vert.weld = false;
					BLI_array_append(vert_arr, vert);
				}

			}
		}

		for (uv_a_index = 0; uv_a_index < BLI_array_count(vert_arr); uv_a_index++) {
			if (vert_arr[uv_a_index].weld == false) {
				float uv_min[2];
				float uv_max[2];

				BLI_array_empty(loop_arr);
				BLI_array_append(loop_arr, vert_arr[uv_a_index].uv_loop);

				uv_a = vert_arr[uv_a_index].uv_loop->uv;

				copy_v2_v2(uv_max, uv_a);
				copy_v2_v2(uv_min, uv_a);

				vert_arr[uv_a_index].weld = true;
				for (uv_b_index = uv_a_index + 1; uv_b_index < BLI_array_count(vert_arr); uv_b_index++) {
					uv_b = vert_arr[uv_b_index].uv_loop->uv;
					if ((vert_arr[uv_b_index].weld == false) &&
					    (len_manhattan_v2v2(uv_a, uv_b) < threshold))
					{
						minmax_v2v2_v2(uv_min, uv_max, uv_b);
						BLI_array_append(loop_arr, vert_arr[uv_b_index].uv_loop);
						vert_arr[uv_b_index].weld = true;
					}
				}
				if (BLI_array_count(loop_arr)) {
					float uv_mid[2];
					mid_v2_v2v2(uv_mid, uv_min, uv_max);
					for (uv_b_index = 0; uv_b_index < BLI_array_count(loop_arr); uv_b_index++) {
						copy_v2_v2(loop_arr[uv_b_index]->uv, uv_mid);
					}
				}
			}
		}

		BLI_array_free(vert_arr);
		BLI_array_free(loop_arr);
	}
	else {
		/* selected -> unselected
		 *
		 * No need to use 'UVvert' here */
		MLoopUV **loop_arr = NULL;
		BLI_array_declare(loop_arr);
		MLoopUV **loop_arr_unselected = NULL;
		BLI_array_declare(loop_arr_unselected);

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
					BLI_array_append(loop_arr, luv);
				}
				else {
					BLI_array_append(loop_arr_unselected, luv);
				}
			}
		}

		for (uv_a_index = 0; uv_a_index < BLI_array_count(loop_arr); uv_a_index++) {
			float dist_best = FLT_MAX, dist;
			const float *uv_best = NULL;

			uv_a = loop_arr[uv_a_index]->uv;
			for (uv_b_index = 0; uv_b_index < BLI_array_count(loop_arr_unselected); uv_b_index++) {
				uv_b = loop_arr_unselected[uv_b_index]->uv;
				dist = len_manhattan_v2v2(uv_a, uv_b);
				if ((dist < threshold) && (dist < dist_best)) {
					uv_best = uv_b;
					dist_best = dist;
				}
			}
			if (uv_best) {
				copy_v2_v2(uv_a, uv_best);
			}
		}

		BLI_array_free(loop_arr);
		BLI_array_free(loop_arr_unselected);
	}

	uvedit_live_unwrap_update(sima, scene, obedit);
	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

static void UV_OT_remove_doubles(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Doubles UV";
	ot->description = "Selected UV vertices that are within a radius of each other are welded together";
	ot->idname = "UV_OT_remove_doubles";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = uv_remove_doubles_exec;
	ot->poll = ED_operator_uvedit;

	RNA_def_float(ot->srna, "threshold", 0.02f, 0.0f, 10.0f,
	              "Merge Distance", "Maximum distance between welded vertices", 0.0f, 1.0f);
	RNA_def_boolean(ot->srna, "use_unselected", 0, "Unselected", "Merge selected to other unselected vertices");
}
/* ******************** weld operator **************** */

static int uv_weld_exec(bContext *C, wmOperator *UNUSED(op))
{
	uv_weld_align(C, 'w');

	return OPERATOR_FINISHED;
}

static void UV_OT_weld(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Weld";
	ot->description = "Weld selected UV vertices together";
	ot->idname = "UV_OT_weld";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_weld_exec;
	ot->poll = ED_operator_uvedit;
}


/* ******************** (de)select all operator **************** */

static void uv_select_all_perform(Scene *scene, Image *ima, BMEditMesh *em, int action)
{
	ToolSettings *ts = scene->toolsettings;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	if (ts->uv_flag & UV_SYNC_SELECTION) {

		switch (action) {
			case SEL_TOGGLE:
				EDBM_select_toggle_all(em);
				break;
			case SEL_SELECT:
				EDBM_flag_enable_all(em, BM_ELEM_SELECT);
				break;
			case SEL_DESELECT:
				EDBM_flag_disable_all(em, BM_ELEM_SELECT);
				break;
			case SEL_INVERT:
				EDBM_select_swap(em);
				EDBM_selectmode_flush(em);
				break;
		}
	}
	else {
		if (action == SEL_TOGGLE) {
			action = SEL_SELECT;
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
	
				if (!uvedit_face_visible_test(scene, ima, efa, tf))
					continue;

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

					if (luv->flag & MLOOPUV_VERTSEL) {
						action = SEL_DESELECT;
						break;
					}
				}
			}
		}
	
		
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				switch (action) {
					case SEL_SELECT:
						luv->flag |= MLOOPUV_VERTSEL;
						break;
					case SEL_DESELECT:
						luv->flag &= ~MLOOPUV_VERTSEL;
						break;
					case SEL_INVERT:
						luv->flag ^= MLOOPUV_VERTSEL;
						break;
				}
			}
		}
	}
}

static int uv_select_all_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	int action = RNA_enum_get(op->ptr, "action");

	uv_select_all_perform(scene, ima, em, action);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static void UV_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->description = "Change selection of all UV vertices";
	ot->idname = "UV_OT_select_all";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_select_all_exec;
	ot->poll = ED_operator_uvedit;

	WM_operator_properties_select_all(ot);
}

/* ******************** mouse select operator **************** */

static bool uv_sticky_select(float *limit, int hitv[4], int v, float *hituv[4], float *uv, int sticky, int hitlen)
{
	int i;

	/* this function test if some vertex needs to selected
	 * in addition to the existing ones due to sticky select */
	if (sticky == SI_STICKY_DISABLE)
		return false;

	for (i = 0; i < hitlen; i++) {
		if (hitv[i] == v) {
			if (sticky == SI_STICKY_LOC) {
				if (fabsf(hituv[i][0] - uv[0]) < limit[0] && fabsf(hituv[i][1] - uv[1]) < limit[1])
					return true;
			}
			else if (sticky == SI_STICKY_VERTEX)
				return true;
		}
	}

	return false;
}

static int uv_mouse_select(bContext *C, const float co[2], bool extend, bool loop)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	NearestHit hit;
	int i, selectmode, sticky, sync, *hitv = NULL;
	bool select = true;
	int flush = 0, hitlen = 0; /* 0 == don't flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
	float limit[2], **hituv = NULL;
	float penalty[2];

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	/* notice 'limit' is the same no matter the zoom level, since this is like
	 * remove doubles and could annoying if it joined points when zoomed out.
	 * 'penalty' is in screen pixel space otherwise zooming in on a uv-vert and
	 * shift-selecting can consider an adjacent point close enough to add to
	 * the selection rather than de-selecting the closest. */

	uvedit_pixel_to_float(sima, limit, 0.05f);
	uvedit_pixel_to_float(sima, penalty, 5.0f / (sima ? sima->zoom : 1.0f));

	/* retrieve operation mode */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		sync = 1;

		if (ts->selectmode & SCE_SELECT_FACE)
			selectmode = UV_SELECT_FACE;
		else if (ts->selectmode & SCE_SELECT_EDGE)
			selectmode = UV_SELECT_EDGE;
		else
			selectmode = UV_SELECT_VERTEX;

		sticky = SI_STICKY_DISABLE;
	}
	else {
		sync = 0;
		selectmode = ts->uv_selectmode;
		sticky = (sima) ? sima->sticky : 1;
	}

	/* find nearest element */
	if (loop) {
		/* find edge */
		uv_find_nearest_edge(scene, ima, em, co, &hit);
		if (hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}

		hitlen = 0;
	}
	else if (selectmode == UV_SELECT_VERTEX) {
		/* find vertex */
		uv_find_nearest_vert(scene, ima, em, co, penalty, &hit);
		if (hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}

		/* mark 1 vertex as being hit */
		hitv  = BLI_array_alloca(hitv,  hit.efa->len);
		hituv = BLI_array_alloca(hituv, hit.efa->len);
		fill_vn_i(hitv, hit.efa->len, 0xFFFFFFFF);

		hitv[hit.lindex] = BM_elem_index_get(hit.l->v);
		hituv[hit.lindex] = hit.luv->uv;

		hitlen = hit.efa->len;
	}
	else if (selectmode == UV_SELECT_EDGE) {
		/* find edge */
		uv_find_nearest_edge(scene, ima, em, co, &hit);
		if (hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}

		/* mark 2 edge vertices as being hit */
		hitv  = BLI_array_alloca(hitv,  hit.efa->len);
		hituv = BLI_array_alloca(hituv, hit.efa->len);
		fill_vn_i(hitv, hit.efa->len, 0xFFFFFFFF);

		hitv[hit.lindex] = BM_elem_index_get(hit.l->v);
		hitv[(hit.lindex + 1) % hit.efa->len] = BM_elem_index_get(hit.l->next->v);
		hituv[hit.lindex] = hit.luv->uv;
		hituv[(hit.lindex + 1) % hit.efa->len] = hit.luv_next->uv;

		hitlen = hit.efa->len;
	}
	else if (selectmode == UV_SELECT_FACE) {
		/* find face */
		uv_find_nearest_face(scene, ima, em, co, &hit);
		if (hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}
		
		/* make active */
		BM_mesh_active_face_set(em->bm, hit.efa);

		/* mark all face vertices as being hit */

		hitv  = BLI_array_alloca(hitv,  hit.efa->len);
		hituv = BLI_array_alloca(hituv, hit.efa->len);
		BM_ITER_ELEM_INDEX (l, &liter, hit.efa, BM_LOOPS_OF_FACE, i) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			hituv[i] = luv->uv;
			hitv[i] = BM_elem_index_get(l->v);
		}
		
		hitlen = hit.efa->len;
	}
	else if (selectmode == UV_SELECT_ISLAND) {
		uv_find_nearest_edge(scene, ima, em, co, &hit);

		if (hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}

		hitlen = 0;
	}
	else {
		hitlen = 0;
		return OPERATOR_CANCELLED;
	}

	/* do selection */
	if (loop) {
		flush = uv_select_edgeloop(scene, ima, em, &hit, limit, extend);
	}
	else if (selectmode == UV_SELECT_ISLAND) {
		uv_select_linked(scene, ima, em, limit, &hit, extend);
	}
	else if (extend) {
		if (selectmode == UV_SELECT_VERTEX) {
			/* (de)select uv vertex */
			select = !uvedit_uv_select_test(scene, hit.l, cd_loop_uv_offset);
			uvedit_uv_select_set(em, scene, hit.l, select, true, cd_loop_uv_offset);
			flush = 1;
		}
		else if (selectmode == UV_SELECT_EDGE) {
			/* (de)select edge */
			select = !(uvedit_edge_select_test(scene, hit.l, cd_loop_uv_offset));
			uvedit_edge_select_set(em, scene, hit.l, select, true, cd_loop_uv_offset);
			flush = 1;
		}
		else if (selectmode == UV_SELECT_FACE) {
			/* (de)select face */
			select = !(uvedit_face_select_test(scene, hit.efa, cd_loop_uv_offset));
			uvedit_face_select_set(scene, em, hit.efa, select, true, cd_loop_uv_offset);
			flush = -1;
		}

		/* de-selecting an edge may deselect a face too - validate */
		if (sync) {
			if (select == false) {
				BM_select_history_validate(em->bm);
			}
		}

		/* (de)select sticky uv nodes */
		if (sticky != SI_STICKY_DISABLE) {

			BM_mesh_elem_index_ensure(em->bm, BM_VERT);

			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
				if (!uvedit_face_visible_test(scene, ima, efa, tf))
					continue;

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					if (uv_sticky_select(limit, hitv, BM_elem_index_get(l->v), hituv, luv->uv, sticky, hitlen))
						uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
				}
			}

			flush = select ? 1 : -1;
		}
	}
	else {
		/* deselect all */
		uv_select_all_perform(scene, ima, em, SEL_DESELECT);

		if (selectmode == UV_SELECT_VERTEX) {
			/* select vertex */
			uvedit_uv_select_enable(em, scene, hit.l, true, cd_loop_uv_offset);
			flush = 1;
		}
		else if (selectmode == UV_SELECT_EDGE) {
			/* select edge */
			uvedit_edge_select_enable(em, scene, hit.l, true, cd_loop_uv_offset);
			flush = 1;
		}
		else if (selectmode == UV_SELECT_FACE) {
			/* select face */
			uvedit_face_select_enable(scene, em, hit.efa, true, cd_loop_uv_offset);
		}

		/* select sticky uvs */
		if (sticky != SI_STICKY_DISABLE) {
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
				if (!uvedit_face_visible_test(scene, ima, efa, tf))
					continue;
				
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					if (sticky == SI_STICKY_DISABLE) continue;
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

					if (uv_sticky_select(limit, hitv, BM_elem_index_get(l->v), hituv, luv->uv, sticky, hitlen))
						uvedit_uv_select_enable(em, scene, l, false, cd_loop_uv_offset);

					flush = 1;
				}
			}
		}
	}

	if (sync) {
		/* flush for mesh selection */

		/* before bmesh */
#if 0
		if (ts->selectmode != SCE_SELECT_FACE) {
			if (flush == 1) EDBM_select_flush(em);
			else if (flush == -1) EDBM_deselect_flush(em);
		}
#else
		if (flush != 0) {
			if (loop) {
				/* push vertex -> edge selection */
				if (select) {
					EDBM_select_flush(em);
				}
				else {
					EDBM_deselect_flush(em);
				}
			}
			else {
				EDBM_selectmode_flush(em);
			}
		}
#endif
	}

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
}

static int uv_select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	bool extend, loop;

	RNA_float_get_array(op->ptr, "location", co);
	extend = RNA_boolean_get(op->ptr, "extend");
	loop = false;

	return uv_mouse_select(C, co, extend, loop);
}

static int uv_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float co[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return uv_select_exec(C, op);
}

static void UV_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->description = "Select UV vertices";
	ot->idname = "UV_OT_select";
	ot->flag = OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_select_exec;
	ot->invoke = uv_select_invoke;
	ot->poll = ED_operator_uvedit; /* requires space image */;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds", -100.0f, 100.0f);
}

/* ******************** loop select operator **************** */

static int uv_select_loop_exec(bContext *C, wmOperator *op)
{
	float co[2];
	bool extend, loop;

	RNA_float_get_array(op->ptr, "location", co);
	extend = RNA_boolean_get(op->ptr, "extend");
	loop = true;

	return uv_mouse_select(C, co, extend, loop);
}

static int uv_select_loop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float co[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return uv_select_loop_exec(C, op);
}

static void UV_OT_select_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Loop Select";
	ot->description = "Select a loop of connected UV vertices";
	ot->idname = "UV_OT_select_loop";
	ot->flag = OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_select_loop_exec;
	ot->invoke = uv_select_loop_invoke;
	ot->poll = ED_operator_uvedit; /* requires space image */;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds", -100.0f, 100.0f);
}

/* ******************** linked select operator **************** */

static int uv_select_linked_internal(bContext *C, wmOperator *op, const wmEvent *event, int pick)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	float limit[2];
	int extend;

	NearestHit hit, *hit_p = NULL;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		BKE_report(op->reports, RPT_ERROR, "Cannot select linked when sync selection is enabled");
		return OPERATOR_CANCELLED;
	}

	extend = RNA_boolean_get(op->ptr, "extend");
	uvedit_pixel_to_float(sima, limit, 0.05f);

	if (pick) {
		float co[2];

		if (event) {
			/* invoke */
			ARegion *ar = CTX_wm_region(C);

			UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
			RNA_float_set_array(op->ptr, "location", co);
		}
		else {
			/* exec */
			RNA_float_get_array(op->ptr, "location", co);
		}

		uv_find_nearest_edge(scene, ima, em, co, &hit);
		hit_p = &hit;
	}

	uv_select_linked(scene, ima, em, limit, hit_p, extend);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static int uv_select_linked_exec(bContext *C, wmOperator *op)
{
	return uv_select_linked_internal(C, op, NULL, 0);
}

static void UV_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->description = "Select all UV vertices linked to the active UV map";
	ot->idname = "UV_OT_select_linked";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_select_linked_exec;
	ot->poll = ED_operator_uvedit;    /* requires space image */

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");
}

static int uv_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	return uv_select_linked_internal(C, op, event, 1);
}

static int uv_select_linked_pick_exec(bContext *C, wmOperator *op)
{
	return uv_select_linked_internal(C, op, NULL, 1);
}

static void UV_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked Pick";
	ot->description = "Select all UV vertices linked under the mouse";
	ot->idname = "UV_OT_select_linked_pick";
	ot->flag = OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke = uv_select_linked_pick_invoke;
	ot->exec = uv_select_linked_pick_exec;
	ot->poll = ED_operator_uvedit; /* requires space image */;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");

	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds", -100.0f, 100.0f);
}

/* note: this is based on similar use case to MESH_OT_split(), which has a similar effect
 * but in this case they are not joined to begin with (only having the behavior of being joined)
 * so its best to call this uv_select_split() instead of just split(), but assigned to the same key
 * as MESH_OT_split - Campbell */
static int uv_select_split_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Image *ima = CTX_data_edit_image(C);
	Object *obedit = CTX_data_edit_object(C);
	BMesh *bm = BKE_editmesh_from_object(obedit)->bm;

	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	bool changed = false;

	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&bm->pdata, CD_MTEXPOLY);

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		BKE_report(op->reports, RPT_ERROR, "Cannot split selection when sync selection is enabled");
		return OPERATOR_CANCELLED;
	}



	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		bool is_sel = false;
		bool is_unsel = false;
		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;

		/* are we all selected? */
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

			if (luv->flag & MLOOPUV_VERTSEL) {
				is_sel = true;
			}
			else {
				is_unsel = true;
			}

			/* we have mixed selection, bail out */
			if (is_sel && is_unsel) {
				break;
			}
		}

		if (is_sel && is_unsel) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				luv->flag &= ~MLOOPUV_VERTSEL;
			}

			changed = true;
		}
	}

	if (changed) {
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}


static void UV_OT_select_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Split";
	ot->description = "Select only entirely selected faces";
	ot->idname = "UV_OT_select_split";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = uv_select_split_exec;
	ot->poll = ED_operator_uvedit; /* requires space image */;
}

static void uv_select_sync_flush(ToolSettings *ts, BMEditMesh *em, const short select)
{
	/* bmesh API handles flushing but not on de-select */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode != SCE_SELECT_FACE) {
			if (select == false) {
				EDBM_deselect_flush(em);
			}
			else {
				EDBM_select_flush(em);
			}
		}

		if (select == false) {
			BM_select_history_validate(em->bm);
		}
	}
}



/* -------------------------------------------------------------------- */
/* Utility functions to flush the uv-selection from tags */

/**
 * helper function for #uv_select_flush_from_tag_loop and uv_select_flush_from_tag_face
 */
static void uv_select_flush_from_tag_sticky_loc_internal(Scene *scene, BMEditMesh *em, UvVertMap *vmap,
                                                         const unsigned int efa_index, BMLoop *l,
                                                         const bool select, const int cd_loop_uv_offset)
{
	UvMapVert *start_vlist = NULL, *vlist_iter;
	BMFace *efa_vlist;

	uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);

	vlist_iter = BM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));

	while (vlist_iter) {
		if (vlist_iter->separate)
			start_vlist = vlist_iter;

		if (efa_index == vlist_iter->f)
			break;

		vlist_iter = vlist_iter->next;
	}

	vlist_iter = start_vlist;
	while (vlist_iter) {

		if (vlist_iter != start_vlist && vlist_iter->separate)
			break;

		if (efa_index != vlist_iter->f) {
			BMLoop *l_other;
			efa_vlist = BM_face_at_index(em->bm, vlist_iter->f);
			/* tf_vlist = BM_ELEM_CD_GET_VOID_P(efa_vlist, cd_poly_tex_offset); */ /* UNUSED */

			l_other = BM_iter_at_index(em->bm, BM_LOOPS_OF_FACE, efa_vlist, vlist_iter->tfindex);

			uvedit_uv_select_set(em, scene, l_other, select, false, cd_loop_uv_offset);
		}
		vlist_iter = vlist_iter->next;
	}
}

/**
 * Flush the selection from face tags based on sticky and selection modes.
 *
 * needed because settings the selection a face is done in a number of places but it also needs to respect
 * the sticky modes for the UV verts, so dealing with the sticky modes is best done in a separate function.
 *
 * \note! This function is very similar to #uv_select_flush_from_tag_loop, be sure to update both upon changing.
 */
static void uv_select_flush_from_tag_face(SpaceImage *sima, Scene *scene, Object *obedit, const bool select)
{
	/* Selecting UV Faces with some modes requires us to change 
	 * the selection in other faces (depending on the sticky mode).
	 * 
	 * This only needs to be done when the Mesh is not used for
	 * selection (so for sticky modes, vertex or location based). */
	
	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	/* MTexPoly *tf; */
	const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	
	if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_VERTEX) {
		/* Tag all verts as untouched, then touch the ones that have a face center
		 * in the loop and select all MLoopUV's that use a touched vert. */
		BMVert *eve;
		
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
		
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					BM_elem_flag_enable(l->v, BM_ELEM_TAG);
				}
			}
		}

		/* now select tagged verts */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			/* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
					uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
				}
			}
		}
	}
	else if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_LOC) {
		struct UvVertMap *vmap;
		float limit[2];
		unsigned int efa_index;
		
		uvedit_pixel_to_float(sima, limit, 0.05);
		
		BM_mesh_elem_table_ensure(em->bm, BM_FACE);
		vmap = BM_uv_vert_map_create(em->bm, 0, limit);
		if (vmap == NULL) {
			return;
		}
		
		BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
			if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
				/* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */
				
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					uv_select_flush_from_tag_sticky_loc_internal(scene, em, vmap, efa_index, l,
					                                             select, cd_loop_uv_offset);
				}
			}
		}
		BM_uv_vert_map_free(vmap);
		
	}
	else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
				uvedit_face_select_set(scene, em, efa, select, false, cd_loop_uv_offset);
			}
		}
	}
}



/**
 * Flush the selection from loop tags based on sticky and selection modes.
 *
 * needed because settings the selection a face is done in a number of places but it also needs to respect
 * the sticky modes for the UV verts, so dealing with the sticky modes is best done in a separate function.
 *
 * \note! This function is very similar to #uv_select_flush_from_tag_loop, be sure to update both upon changing.
 */
static void uv_select_flush_from_tag_loop(SpaceImage *sima, Scene *scene, Object *obedit, const bool select)
{
	/* Selecting UV Loops with some modes requires us to change
	 * the selection in other faces (depending on the sticky mode).
	 *
	 * This only needs to be done when the Mesh is not used for
	 * selection (so for sticky modes, vertex or location based). */

	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	/* MTexPoly *tf; */

	const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);


	if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_VERTEX) {
		/* Tag all verts as untouched, then touch the ones that have a face center
		 * in the loop and select all MLoopUV's that use a touched vert. */
		BMVert *eve;

		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
					BM_elem_flag_enable(l->v, BM_ELEM_TAG);
				}
			}
		}

		/* now select tagged verts */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			/* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
					uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
				}
			}
		}
	}
	else if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_LOC) {
		struct UvVertMap *vmap;
		float limit[2];
		unsigned int efa_index;

		uvedit_pixel_to_float(sima, limit, 0.05);

		BM_mesh_elem_table_ensure(em->bm, BM_FACE);
		vmap = BM_uv_vert_map_create(em->bm, 0, limit);
		if (vmap == NULL) {
			return;
		}

		BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
			/* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
					uv_select_flush_from_tag_sticky_loc_internal(scene, em, vmap, efa_index, l,
					                                             select, cd_loop_uv_offset);
				}
			}
		}
		BM_uv_vert_map_free(vmap);

	}
	else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
					uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
				}
			}
		}
	}
}

/* ******************** border select operator **************** */

static int uv_border_select_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	ARegion *ar = CTX_wm_region(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	rctf rectf;
	bool changed, pinned, select, extend;
	const bool use_face_center = (ts->uv_flag & UV_SYNC_SELECTION) ?
	                            (ts->selectmode == SCE_SELECT_FACE) :
	                            (ts->uv_selectmode == UV_SELECT_FACE);

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	/* get rectangle from operator */
	WM_operator_properties_border_to_rctf(op, &rectf);
	UI_view2d_region_to_view_rctf(&ar->v2d, &rectf, &rectf);

	/* figure out what to select/deselect */
	select = (RNA_int_get(op->ptr, "gesture_mode") == GESTURE_MODAL_SELECT);
	pinned = RNA_boolean_get(op->ptr, "pinned");
	extend = RNA_boolean_get(op->ptr, "extend");

	if (!extend)
		uv_select_all_perform(scene, ima, em, SEL_DESELECT);

	/* do actual selection */
	if (use_face_center && !pinned) {
		/* handle face selection mode */
		float cent[2];

		changed = false;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			/* assume not touched */
			BM_elem_flag_disable(efa, BM_ELEM_TAG);

			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				uv_poly_center(efa, cent, cd_loop_uv_offset);
				if (BLI_rctf_isect_pt_v(&rectf, cent)) {
					BM_elem_flag_enable(efa, BM_ELEM_TAG);
					changed = true;
				}
			}
		}

		/* (de)selects all tagged faces and deals with sticky modes */
		if (changed) {
			uv_select_flush_from_tag_face(sima, scene, obedit, select);
		}
	}
	else {
		/* other selection modes */
		changed = true;
		
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				if (!pinned || (ts->uv_flag & UV_SYNC_SELECTION)) {

					/* UV_SYNC_SELECTION - can't do pinned selection */
					if (BLI_rctf_isect_pt_v(&rectf, luv->uv)) {
						uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
					}
				}
				else if (pinned) {
					if ((luv->flag & MLOOPUV_PINNED) && BLI_rctf_isect_pt_v(&rectf, luv->uv)) {
						uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
					}
				}
			}
		}
	}

	if (changed) {
		uv_select_sync_flush(ts, em, select);

		if (ts->uv_flag & UV_SYNC_SELECTION) {
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		}
		
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
} 

static void UV_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->description = "Select UV vertices using border selection";
	ot->idname = "UV_OT_select_border";
	
	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = uv_border_select_exec;
	ot->modal = WM_border_select_modal;
	ot->poll = ED_operator_uvedit_space_image; /* requires space image */;
	ot->cancel = WM_border_select_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "pinned", 0, "Pinned", "Border select pinned UVs only");

	WM_operator_properties_gesture_border(ot, true);
}

/* ******************** circle select operator **************** */

static int uv_inside_circle(const float uv[2], const float offset[2], const float ellipse[2])
{
	/* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
	float x, y;
	x = (uv[0] - offset[0]) * ellipse[0];
	y = (uv[1] - offset[1]) * ellipse[1];
	return ((x * x + y * y) < 1.0f);
}

static bool uv_select_inside_ellipse(BMEditMesh *em, Scene *scene, const bool select,
                                     const float offset[2], const float ellipse[2], BMLoop *l, MLoopUV *luv,
                                     const int cd_loop_uv_offset)
{
	if (uv_inside_circle(luv->uv, offset, ellipse)) {
		uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
		return true;
	}
	else {
		return false;
	}
}

static int uv_circle_select_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	ARegion *ar = CTX_wm_region(C);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	int x, y, radius, width, height;
	float zoomx, zoomy, offset[2], ellipse[2];
	int gesture_mode = RNA_int_get(op->ptr, "gesture_mode");
	const bool select = (gesture_mode == GESTURE_MODAL_SELECT);
	bool changed = false;
	const bool use_face_center = (ts->uv_flag & UV_SYNC_SELECTION) ?
	                             (ts->selectmode == SCE_SELECT_FACE) :
	                             (ts->uv_selectmode == UV_SELECT_FACE);

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

	/* get operator properties */
	x = RNA_int_get(op->ptr, "x");
	y = RNA_int_get(op->ptr, "y");
	radius = RNA_int_get(op->ptr, "radius");

	/* compute ellipse size and location, not a circle since we deal
	 * with non square image. ellipse is normalized, r = 1.0. */
	ED_space_image_get_size(sima, &width, &height);
	ED_space_image_get_zoom(sima, ar, &zoomx, &zoomy);

	ellipse[0] = width * zoomx / radius;
	ellipse[1] = height * zoomy / radius;

	UI_view2d_region_to_view(&ar->v2d, x, y, &offset[0], &offset[1]);
	
	/* do selection */
	if (use_face_center) {
		changed = false;
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_elem_flag_disable(efa, BM_ELEM_TAG);
			/* assume not touched */
			if (select != uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
				float cent[2];
				uv_poly_center(efa, cent, cd_loop_uv_offset);
				if (uv_inside_circle(cent, offset, ellipse)) {
					BM_elem_flag_enable(efa, BM_ELEM_TAG);
					changed = true;
				}
			}
		}

		/* (de)selects all tagged faces and deals with sticky modes */
		if (changed) {
			uv_select_flush_from_tag_face(sima, scene, obedit, select);
		}
	}
	else {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				changed |= uv_select_inside_ellipse(em, scene, select, offset, ellipse, l, luv, cd_loop_uv_offset);
			}
		}
	}

	if (changed) {
		uv_select_sync_flush(ts, em, select);

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	}

	return OPERATOR_FINISHED;
}

static void UV_OT_circle_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Circle Select";
	ot->description = "Select UV vertices using circle selection";
	ot->idname = "UV_OT_circle_select";
	
	/* api callbacks */
	ot->invoke = WM_gesture_circle_invoke;
	ot->modal = WM_gesture_circle_modal;
	ot->exec = uv_circle_select_exec;
	ot->poll = ED_operator_uvedit_space_image; /* requires space image */;
	ot->cancel = WM_gesture_circle_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 1, 1, INT_MAX, "Radius", "", 1, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
}


/* ******************** lasso select operator **************** */

static bool do_lasso_select_mesh_uv(bContext *C, const int mcords[][2], short moves,
                                    const bool select, const bool extend)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Image *ima = CTX_data_edit_image(C);
	ARegion *ar = CTX_wm_region(C);
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int use_face_center = (ts->uv_flag & UV_SYNC_SELECTION) ?
	                            (ts->selectmode == SCE_SELECT_FACE) :
	                            (ts->uv_selectmode == UV_SELECT_FACE);

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	BMIter iter, liter;

	BMFace *efa;
	BMLoop *l;
	MTexPoly *tf;
	int screen_uv[2];
	bool changed = false;
	rcti rect;

	BLI_lasso_boundbox(&rect, mcords, moves);

	if (!extend && select) {
		uv_select_all_perform(scene, ima, em, SEL_DESELECT);
	}

	if (use_face_center) { /* Face Center Sel */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_elem_flag_disable(efa, BM_ELEM_TAG);
			/* assume not touched */
			if (select != uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
				float cent[2];
				uv_poly_center(efa, cent, cd_loop_uv_offset);

				if (UI_view2d_view_to_region_clip(&ar->v2d, cent[0], cent[1], &screen_uv[0], &screen_uv[1]) &&
				    BLI_rcti_isect_pt_v(&rect, screen_uv) &&
				    BLI_lasso_is_point_inside(mcords, moves, screen_uv[0], screen_uv[1], V2D_IS_CLIPPED))
				{
					BM_elem_flag_enable(efa, BM_ELEM_TAG);
					changed = true;
				}
			}
		}

		/* (de)selects all tagged faces and deals with sticky modes */
		if (changed) {
			uv_select_flush_from_tag_face(sima, scene, obedit, select);
		}
	}
	else { /* Vert Sel */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					if ((select) != (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))) {
						MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						if (UI_view2d_view_to_region_clip(&ar->v2d,
						                                  luv->uv[0], luv->uv[1],
						                                  &screen_uv[0], &screen_uv[1]) &&
						    BLI_rcti_isect_pt_v(&rect, screen_uv) &&
						    BLI_lasso_is_point_inside(mcords, moves, screen_uv[0], screen_uv[1], V2D_IS_CLIPPED))
						{
							uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
							changed = true;
						}
					}
				}
			}
		}
	}

	if (changed) {
		uv_select_sync_flush(scene->toolsettings, em, select);

		if (ts->uv_flag & UV_SYNC_SELECTION) {
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		}
	}

	return changed;
}

static int uv_lasso_select_exec(bContext *C, wmOperator *op)
{
	int mcords_tot;
	const int (*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

	if (mcords) {
		bool select, extend;
		bool changed;

		select = !RNA_boolean_get(op->ptr, "deselect");
		extend = RNA_boolean_get(op->ptr, "extend");
		changed = do_lasso_select_mesh_uv(C, mcords, mcords_tot, select, extend);

		MEM_freeN((void *)mcords);

		return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
	}

	return OPERATOR_PASS_THROUGH;
}

static void UV_OT_select_lasso(wmOperatorType *ot)
{
	ot->name = "Lasso Select UV";
	ot->description = "Select UVs using lasso selection";
	ot->idname = "UV_OT_select_lasso";

	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = uv_lasso_select_exec;
	ot->poll = ED_operator_uvedit_space_image;
	ot->cancel = WM_gesture_lasso_cancel;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items");
	RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection instead of deselecting everything first");
}



/* ******************** snap cursor operator **************** */

static void uv_snap_to_pixel(float uvco[2], float w, float h)
{
	uvco[0] = ((float)((int)((uvco[0] * w) + 0.5f))) / w;
	uvco[1] = ((float)((int)((uvco[1] * h) + 0.5f))) / h;
}

static void uv_snap_cursor_to_pixels(SpaceImage *sima)
{
	int width = 0, height = 0;

	ED_space_image_get_size(sima, &width, &height);
	uv_snap_to_pixel(sima->cursor, width, height);
}

static bool uv_snap_cursor_to_selection(Scene *scene, Image *ima, Object *obedit, SpaceImage *sima)
{
	return uvedit_center(scene, ima, obedit, sima->cursor, sima->around);
}

static int uv_snap_cursor_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	bool changed = false;

	switch (RNA_enum_get(op->ptr, "target")) {
		case 0:
			uv_snap_cursor_to_pixels(sima);
			changed = true;
			break;
		case 1:
			changed = uv_snap_cursor_to_selection(scene, ima, obedit, sima);
			break;
	}

	if (!changed)
		return OPERATOR_CANCELLED;
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, sima);

	return OPERATOR_FINISHED;
}

static void UV_OT_snap_cursor(wmOperatorType *ot)
{
	static EnumPropertyItem target_items[] = {
		{0, "PIXELS", 0, "Pixels", ""},
		{1, "SELECTED", 0, "Selected", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Snap Cursor";
	ot->description = "Snap cursor to target type";
	ot->idname = "UV_OT_snap_cursor";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_snap_cursor_exec;
	ot->poll = ED_operator_uvedit_space_image; /* requires space image */;

	/* properties */
	RNA_def_enum(ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UVs to");
}

/* ******************** snap selection operator **************** */

static bool uv_snap_uvs_to_cursor(Scene *scene, Image *ima, Object *obedit, const float cursor[2])
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	bool changed = false;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				copy_v2_v2(luv->uv, cursor);
				changed = true;
			}
		}
	}

	return changed;
}

static bool uv_snap_uvs_offset(Scene *scene, Image *ima, Object *obedit, const float offset[2])
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	bool changed = false;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				add_v2_v2(luv->uv, offset);
				changed = true;
			}
		}
	}

	return changed;
}

static bool uv_snap_uvs_to_adjacent_unselected(Scene *scene, Image *ima, Object *obedit)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMFace *f;
	BMLoop *l, *lsub;
	BMIter iter, liter, lsubiter;
	MTexPoly *tface;
	MLoopUV *luv;
	bool changed = false;
	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&bm->pdata, CD_MTEXPOLY);
	
	/* index every vert that has a selected UV using it, but only once so as to
	 * get unique indices and to count how much to malloc */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		tface = BM_ELEM_CD_GET_VOID_P(f, cd_poly_tex_offset);
		if (uvedit_face_visible_test(scene, ima, f, tface)) {
			BM_elem_flag_enable(f, BM_ELEM_TAG);
			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				BM_elem_flag_set(l, BM_ELEM_TAG, uvedit_uv_select_test(scene, l, cd_loop_uv_offset));
			}
		}
		else {
			BM_elem_flag_disable(f, BM_ELEM_TAG);
		}
	}

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_TAG)) {           /* face: visible */
			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				if (BM_elem_flag_test(l, BM_ELEM_TAG)) {   /* loop: selected*/
					float uv[2] = {0.0f, 0.0f};
					int uv_tot = 0;

					BM_ITER_ELEM (lsub, &lsubiter, l->v, BM_LOOPS_OF_VERT) {
						if (BM_elem_flag_test(lsub->f, BM_ELEM_TAG) && /* face: visible */
						    !BM_elem_flag_test(lsub, BM_ELEM_TAG))     /* loop: unselected  */
						{
							luv = BM_ELEM_CD_GET_VOID_P(lsub, cd_loop_uv_offset);
							add_v2_v2(uv, luv->uv);
							uv_tot++;
						}
					}

					if (uv_tot) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						mul_v2_v2fl(luv->uv, uv, 1.0f / (float)uv_tot);
						changed = true;
					}
				}
			}
		}
	}

	return changed;
}

static bool uv_snap_uvs_to_pixels(SpaceImage *sima, Scene *scene, Object *obedit)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	Image *ima = sima->image;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	int width = 0, height = 0;
	float w, h;
	bool changed = false;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	ED_space_image_get_size(sima, &width, &height);
	w = (float)width;
	h = (float)height;
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				uv_snap_to_pixel(luv->uv, w, h);
			}
		}

		changed = true;
	}

	return changed;
}

static int uv_snap_selection_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	bool changed = false;

	switch (RNA_enum_get(op->ptr, "target")) {
		case 0:
			changed = uv_snap_uvs_to_pixels(sima, scene, obedit);
			break;
		case 1:
			changed = uv_snap_uvs_to_cursor(scene, ima, obedit, sima->cursor);
			break;
		case 2:
		{
			float center[2];
			if (uvedit_center(scene, ima, obedit, center, sima->around)) {
				float offset[2];
				sub_v2_v2v2(offset, sima->cursor, center);
				changed = uv_snap_uvs_offset(scene, ima, obedit, offset);
			}
			break;
		}
		case 3:
			changed = uv_snap_uvs_to_adjacent_unselected(scene, ima, obedit);
			break;
	}

	if (!changed)
		return OPERATOR_CANCELLED;

	uvedit_live_unwrap_update(sima, scene, obedit);
	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

static void UV_OT_snap_selected(wmOperatorType *ot)
{
	static EnumPropertyItem target_items[] = {
		{0, "PIXELS", 0, "Pixels", ""},
		{1, "CURSOR", 0, "Cursor", ""},
		{2, "CURSOR_OFFSET", 0, "Cursor (Offset)", ""},
		{3, "ADJACENT_UNSELECTED", 0, "Adjacent Unselected", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Snap Selection";
	ot->description = "Snap selected UV vertices to target type";
	ot->idname = "UV_OT_snap_selected";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_snap_selection_exec;
	ot->poll = ED_operator_uvedit_space_image;

	/* properties */
	RNA_def_enum(ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UVs to");
}

/* ******************** pin operator **************** */

static int uv_pin_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	const bool clear = RNA_boolean_get(op->ptr, "clear");
	
	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			
			if (!clear) {
				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
					luv->flag |= MLOOPUV_PINNED;
			}
			else {
				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
					luv->flag &= ~MLOOPUV_PINNED;
			}
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

static void UV_OT_pin(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Pin";
	ot->description = "Set/clear selected UV vertices as anchored between multiple unwrap operations";
	ot->idname = "UV_OT_pin";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_pin_exec;
	ot->poll = ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "Clear pinning for the selection instead of setting it");
}

/******************* select pinned operator ***************/

static int uv_select_pinned_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	
	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			
			if (luv->flag & MLOOPUV_PINNED)
				uvedit_uv_select_enable(em, scene, l, false, cd_loop_uv_offset);
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static void UV_OT_select_pinned(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Selected Pinned";
	ot->description = "Select all pinned UV vertices";
	ot->idname = "UV_OT_select_pinned";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_select_pinned_exec;
	ot->poll = ED_operator_uvedit;
}

/********************** hide operator *********************/

/* check if we are selected or unselected based on 'bool_test' arg,
 * needed for select swap support */
#define UV_SEL_TEST(luv, bool_test) ((((luv)->flag & MLOOPUV_VERTSEL) == MLOOPUV_VERTSEL) == bool_test)

/* is every UV vert selected or unselected depending on bool_test */
static bool bm_face_is_all_uv_sel(BMFace *f, bool select_test,
                                  const int cd_loop_uv_offset)
{
	BMLoop *l_iter;
	BMLoop *l_first;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
		if (!UV_SEL_TEST(luv, select_test)) {
			return false;
		}
	} while ((l_iter = l_iter->next) != l_first);

	return true;
}

static int uv_hide_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	MTexPoly *tf;
	const bool swap = RNA_boolean_get(op->ptr, "unselected");
	Image *ima = sima ? sima->image : NULL;
	const int use_face_center = (ts->uv_selectmode == UV_SELECT_FACE);

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&em->bm->pdata, CD_MTEXPOLY);

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		EDBM_mesh_hide(em, swap);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

		return OPERATOR_FINISHED;
	}

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		int hide = 0;

		tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

		if (!uvedit_face_visible_test(scene, ima, efa, tf)) {
			continue;
		}

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

			if (UV_SEL_TEST(luv, !swap)) {
				hide = 1;
				break;
			}
		}

		if (hide) {
			/* note, a special case for edges could be used,
			 * for now edges act like verts and get flushed */
			if (use_face_center) {
				if (em->selectmode == SCE_SELECT_FACE) {
					/* check that every UV is selected */
					if (bm_face_is_all_uv_sel(efa, true, cd_loop_uv_offset) == !swap) {
						BM_face_select_set(em->bm, efa, false);
					}
					uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
				}
				else {
					if (bm_face_is_all_uv_sel(efa, true, cd_loop_uv_offset) == !swap) {
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							if (UV_SEL_TEST(luv, !swap)) {
								BM_vert_select_set(em->bm, l->v, false);
							}
						}
					}
					if (!swap) uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
				}
			}
			else if (em->selectmode == SCE_SELECT_FACE) {
				/* check if a UV is de-selected */
				if (bm_face_is_all_uv_sel(efa, false, cd_loop_uv_offset) != !swap) {
					BM_face_select_set(em->bm, efa, false);
					uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
				}
			}
			else {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					if (UV_SEL_TEST(luv, !swap)) {
						BM_vert_select_set(em->bm, l->v, false);
						if (!swap) luv->flag &= ~MLOOPUV_VERTSEL;
					}
				}
			}
		}
	}

	/* flush vertex selection changes */
	if (em->selectmode != SCE_SELECT_FACE)
		EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX | SCE_SELECT_EDGE);
	
	BM_select_history_validate(em->bm);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

#undef UV_SEL_TEST

static void UV_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Selected";
	ot->description = "Hide (un)selected UV vertices";
	ot->idname = "UV_OT_hide";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_hide_exec;
	ot->poll = ED_operator_uvedit;

	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}

/****************** reveal operator ******************/

static int uv_reveal_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	const int use_face_center = (ts->uv_selectmode == UV_SELECT_FACE);
	const int stickymode = sima ? (sima->sticky != SI_STICKY_DISABLE) : 1;

	const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

	/* note on tagging, selecting faces needs to be delayed so it doesn't select the verts and
	 * confuse our checks on selected verts. */

	/* call the mesh function if we are in mesh sync sel */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		EDBM_mesh_reveal(em);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

		return OPERATOR_FINISHED;
	}
	if (use_face_center) {
		if (em->selectmode == SCE_SELECT_FACE) {
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				BM_elem_flag_disable(efa, BM_ELEM_TAG);
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						luv->flag |= MLOOPUV_VERTSEL;
					}
					/* BM_face_select_set(em->bm, efa, true); */
					BM_elem_flag_enable(efa, BM_ELEM_TAG);
				}
			}
		}
		else {
			/* enable adjacent faces to have disconnected UV selections if sticky is disabled */
			if (!stickymode) {
				BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
					if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
						int totsel = 0;
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							totsel += BM_elem_flag_test(l->v, BM_ELEM_SELECT);
						}
						
						if (!totsel) {
							BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
								luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
								luv->flag |= MLOOPUV_VERTSEL;
							}
							/* BM_face_select_set(em->bm, efa, true); */
							BM_elem_flag_enable(efa, BM_ELEM_TAG);
						}
					}
				}
			}
			else {
				BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
					if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							if (BM_elem_flag_test(l->v, BM_ELEM_SELECT) == 0) {
								luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
								luv->flag |= MLOOPUV_VERTSEL;
							}
						}
						/* BM_face_select_set(em->bm, efa, true); */
						BM_elem_flag_enable(efa, BM_ELEM_TAG);
					}
				}
			}
		}
	}
	else if (em->selectmode == SCE_SELECT_FACE) {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_elem_flag_disable(efa, BM_ELEM_TAG);
			if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					luv->flag |= MLOOPUV_VERTSEL;
				}
				/* BM_face_select_set(em->bm, efa, true); */
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
			}
		}
	}
	else {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_elem_flag_disable(efa, BM_ELEM_TAG);
			if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					if (BM_elem_flag_test(l->v, BM_ELEM_SELECT) == 0) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						luv->flag |= MLOOPUV_VERTSEL;
					}
				}
				/* BM_face_select_set(em->bm, efa, true); */
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
			}
		}
	}
	
	/* re-select tagged faces */
	BM_mesh_elem_hflag_enable_test(em->bm, BM_FACE, BM_ELEM_SELECT, true, false, BM_ELEM_TAG);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static void UV_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reveal Hidden";
	ot->description = "Reveal all hidden UV vertices";
	ot->idname = "UV_OT_reveal";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = uv_reveal_exec;
	ot->poll = ED_operator_uvedit;
}

/******************** set 3d cursor operator ********************/

static int uv_set_2d_cursor_poll(bContext *C)
{
	return ED_operator_uvedit_space_image(C) ||
	       ED_space_image_maskedit_poll(C) ||
	       ED_space_image_paint_curve(C);
}

static int uv_set_2d_cursor_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);

	if (!sima)
		return OPERATOR_CANCELLED;

	RNA_float_get_array(op->ptr, "location", sima->cursor);
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, NULL);
	
	return OPERATOR_FINISHED;
}

static int uv_set_2d_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float location[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return uv_set_2d_cursor_exec(C, op);
}

static void UV_OT_cursor_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set 2D Cursor";
	ot->description = "Set 2D cursor location";
	ot->idname = "UV_OT_cursor_set";
	
	/* api callbacks */
	ot->exec = uv_set_2d_cursor_exec;
	ot->invoke = uv_set_2d_cursor_invoke;
	ot->poll = uv_set_2d_cursor_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location",
	                     "Cursor location in normalized (0.0-1.0) coordinates", -10.0f, 10.0f);
}

/********************** set tile operator **********************/

static int set_tile_exec(bContext *C, wmOperator *op)
{
	Image *ima = CTX_data_edit_image(C);
	int tile[2];
	Object *obedit = CTX_data_edit_object(C);

	RNA_int_get_array(op->ptr, "tile", tile);

	if (uvedit_set_tile(obedit, ima, tile[0] + ima->xrep * tile[1])) {
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, NULL);

		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED;
}

static int set_tile_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Image *ima = CTX_data_edit_image(C);
	ARegion *ar = CTX_wm_region(C);
	float fx, fy;
	int tile[2];

	if (!ima || !(ima->tpageflag & IMA_TILES))
		return OPERATOR_CANCELLED;

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fx, &fy);

	if (fx >= 0.0f && fy >= 0.0f && fx < 1.0f && fy < 1.0f) {
		fx = fx * ima->xrep;
		fy = fy * ima->yrep;
		
		tile[0] = fx;
		tile[1] = fy;
		
		sima->curtile = tile[1] * ima->xrep + tile[0];
		RNA_int_set_array(op->ptr, "tile", tile);
	}

	return set_tile_exec(C, op);
}

static void UV_OT_tile_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Tile";
	ot->description = "Set UV image tile coordinates";
	ot->idname = "UV_OT_tile_set";
	
	/* api callbacks */
	ot->exec = set_tile_exec;
	ot->invoke = set_tile_invoke;
	ot->poll = ED_operator_image_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int_vector(ot->srna, "tile", 2, NULL, 0, INT_MAX, "Tile", "Tile coordinate", 0, 10);
}


static int uv_seams_from_islands_exec(bContext *C, wmOperator *op)
{
	UvVertMap *vmap;
	Object *ob = CTX_data_edit_object(C);
	Mesh *me = (Mesh *)ob->data;
	BMEditMesh *em;
	BMEdge *editedge;
	float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};
	const bool mark_seams = RNA_boolean_get(op->ptr, "mark_seams");
	const bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");

	BMesh *bm;
	BMIter iter;

	em = me->edit_btmesh;
	bm = em->bm;

	if (!EDBM_mtexpoly_check(em)) {
		return OPERATOR_CANCELLED;
	}

	/* This code sets editvert->tmp.l to the index. This will be useful later on. */
	BM_mesh_elem_table_ensure(bm, BM_FACE);
	vmap = BM_uv_vert_map_create(bm, 0, limit);

	BM_ITER_MESH (editedge, &iter, bm, BM_EDGES_OF_MESH) {
		/* flags to determine if we uv is separated from first editface match */
		char separated1 = 0, separated2;
		/* set to denote edge must be flagged as seam */
		char faces_separated = 0;
		/* flag to keep track if uv1 is disconnected from first editface match */
		char v1coincident = 1;
		/* For use with v1coincident. v1coincident will change only if we've had commonFaces */
		int commonFaces = 0;

		BMFace *efa1, *efa2;

		UvMapVert *mv1, *mvinit1, *mv2, *mvinit2, *mviter;
		/* mv2cache stores the first of the list of coincident uv's for later comparison
		 * mv2sep holds the last separator and is copied to mv2cache when a hit is first found */
		UvMapVert *mv2cache = NULL, *mv2sep = NULL;

		mvinit1 = vmap->vert[BM_elem_index_get(editedge->v1)];
		if (mark_seams)
			BM_elem_flag_disable(editedge, BM_ELEM_SEAM);

		for (mv1 = mvinit1; mv1 && !faces_separated; mv1 = mv1->next) {
			if (mv1->separate && commonFaces)
				v1coincident = 0;

			separated2 = 0;
			efa1 = BM_face_at_index(bm, mv1->f);
			mvinit2 = vmap->vert[BM_elem_index_get(editedge->v2)];

			for (mv2 = mvinit2; mv2; mv2 = mv2->next) {
				if (mv2->separate)
					mv2sep = mv2;

				efa2 = BM_face_at_index(bm, mv2->f);
				if (efa1 == efa2) {
					/* if v1 is not coincident no point in comparing */
					if (v1coincident) {
						/* have we found previously anything? */
						if (mv2cache) {
							/* flag seam unless proved to be coincident with previous hit */
							separated2 = 1;
							for (mviter = mv2cache; mviter; mviter = mviter->next) {
								if (mviter->separate && mviter != mv2cache)
									break;
								/* coincident with previous hit, do not flag seam */
								if (mviter == mv2)
									separated2 = 0;
							}
						}
						/* First hit case, store the hit in the cache */
						else {
							mv2cache = mv2sep;
							commonFaces = 1;
						}
					}
					else
						separated1 = 1;

					if (separated1 || separated2) {
						faces_separated = 1;
						break;
					}
				}
			}
		}

		if (faces_separated) {
			if (mark_seams)
				BM_elem_flag_enable(editedge, BM_ELEM_SEAM);
			if (mark_sharp)
				BM_elem_flag_disable(editedge, BM_ELEM_SMOOTH);
		}
	}

	me->drawflag |= ME_DRAWSEAMS;

	BM_uv_vert_map_free(vmap);

	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return OPERATOR_FINISHED;
}


static void UV_OT_seams_from_islands(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Seams From Islands";
	ot->description = "Set mesh seams according to island setup in the UV editor";
	ot->idname = "UV_OT_seams_from_islands";

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = uv_seams_from_islands_exec;
	ot->poll = ED_operator_uvedit;

	RNA_def_boolean(ot->srna, "mark_seams", 1, "Mark Seams", "Mark boundary edges as seams");
	RNA_def_boolean(ot->srna, "mark_sharp", 0, "Mark Sharp", "Mark boundary edges as sharp");
}

static int uv_mark_seam_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	Mesh *me = (Mesh *)ob->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMFace *efa;
	BMLoop *loop;
	BMIter iter, liter;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM (loop, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_edge_select_test(scene, loop, cd_loop_uv_offset)) {
				BM_elem_flag_enable(loop->e, BM_ELEM_SEAM);
			}
		}
	}

	me->drawflag |= ME_DRAWSEAMS;

	if (scene->toolsettings->edge_mode_live_unwrap)
		ED_unwrap_lscm(scene, ob, false);

	DAG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return OPERATOR_FINISHED;
}

static void UV_OT_mark_seam(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mark Seams";
	ot->description = "Mark selected UV edges as seams";
	ot->idname = "UV_OT_mark_seam";

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->exec = uv_mark_seam_exec;
	ot->poll = ED_operator_uvedit;
}


/* ************************** registration **********************************/

void ED_operatortypes_uvedit(void)
{
	WM_operatortype_append(UV_OT_select_all);
	WM_operatortype_append(UV_OT_select);
	WM_operatortype_append(UV_OT_select_loop);
	WM_operatortype_append(UV_OT_select_linked);
	WM_operatortype_append(UV_OT_select_linked_pick);
	WM_operatortype_append(UV_OT_select_split);
	WM_operatortype_append(UV_OT_select_pinned);
	WM_operatortype_append(UV_OT_select_border);
	WM_operatortype_append(UV_OT_select_lasso);
	WM_operatortype_append(UV_OT_circle_select);
	WM_operatortype_append(UV_OT_select_more);
	WM_operatortype_append(UV_OT_select_less);

	WM_operatortype_append(UV_OT_snap_cursor);
	WM_operatortype_append(UV_OT_snap_selected);

	WM_operatortype_append(UV_OT_align);

	WM_operatortype_append(UV_OT_stitch);

	WM_operatortype_append(UV_OT_seams_from_islands);
	WM_operatortype_append(UV_OT_mark_seam);
	WM_operatortype_append(UV_OT_weld);
	WM_operatortype_append(UV_OT_remove_doubles);
	WM_operatortype_append(UV_OT_pin);

	WM_operatortype_append(UV_OT_average_islands_scale);
	WM_operatortype_append(UV_OT_cube_project);
	WM_operatortype_append(UV_OT_cylinder_project);
	WM_operatortype_append(UV_OT_project_from_view);
	WM_operatortype_append(UV_OT_minimize_stretch);
	WM_operatortype_append(UV_OT_pack_islands);
	WM_operatortype_append(UV_OT_reset);
	WM_operatortype_append(UV_OT_sphere_project);
	WM_operatortype_append(UV_OT_unwrap);

	WM_operatortype_append(UV_OT_reveal);
	WM_operatortype_append(UV_OT_hide);

	WM_operatortype_append(UV_OT_cursor_set);
	WM_operatortype_append(UV_OT_tile_set);
}

void ED_keymap_uvedit(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	keymap = WM_keymap_find(keyconf, "UV Editor", 0, 0);
	keymap->poll = ED_operator_uvedit_can_uv_sculpt;

	/* Uv sculpt toggle */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", QKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.use_uv_sculpt");

	/* Mark edge seam */
	WM_keymap_add_item(keymap, "UV_OT_mark_seam", EKEY, KM_PRESS, KM_CTRL, 0);
	
	/* pick selection */
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select", SELECTMOUSE, KM_PRESS, 0, 0)->ptr, "extend", false);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", true);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_loop", SELECTMOUSE, KM_PRESS, KM_ALT, 0)->ptr, "extend", false);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_loop", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0)->ptr, "extend", true);
	WM_keymap_add_item(keymap, "UV_OT_select_split", YKEY, KM_PRESS, 0, 0);

	/* border/circle selection */
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "pinned", false);
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_border", BKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "pinned", true);

	WM_keymap_add_item(keymap, "UV_OT_circle_select", CKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "UV_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);

	/* selection manipulation */
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0)->ptr, "extend", false);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0)->ptr, "extend", false);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked", LKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0)->ptr, "extend", true);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", true);

	/* select more/less */
	WM_keymap_add_item(keymap, "UV_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "UV_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "UV_OT_select_pinned", PKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_menu(keymap, "IMAGE_MT_uvs_weldalign", WKEY, KM_PRESS, 0, 0);

	/* uv operations */
	WM_keymap_add_item(keymap, "UV_OT_stitch", VKEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "UV_OT_pin", PKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "clear", false);
	kmi = WM_keymap_add_item(keymap, "UV_OT_pin", PKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "clear", true);

	/* unwrap */
	WM_keymap_add_item(keymap, "UV_OT_unwrap", EKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_minimize_stretch", VKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_pack_islands", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_average_islands_scale", AKEY, KM_PRESS, KM_CTRL, 0);

	/* hide */
	kmi = WM_keymap_add_item(keymap, "UV_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", false);
	kmi = WM_keymap_add_item(keymap, "UV_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", true);

	WM_keymap_add_item(keymap, "UV_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);

	/* cursor */
	WM_keymap_add_item(keymap, "UV_OT_cursor_set", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_tile_set", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);
	
	/* menus */
	WM_keymap_add_menu(keymap, "IMAGE_MT_uvs_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_menu(keymap, "IMAGE_MT_uvs_select_mode", TABKEY, KM_PRESS, KM_CTRL, 0);

	ED_keymap_proportional_cycle(keyconf, keymap);
	ED_keymap_proportional_editmode(keyconf, keymap, false);

	transform_keymap_for_space(keyconf, keymap, SPACE_IMAGE);
}

