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

#include "BLI_math.h"
#include "BLI_lasso.h"
#include "BLI_blenlib.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_tessmesh.h"

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

static void select_all_perform(Scene *scene, Image *ima, BMEditMesh *em, int action);

/************************* state testing ************************/

int ED_uvedit_test(Object *obedit)
{
	BMEditMesh *em;
	int ret;

	if (!obedit)
		return 0;
	
	if (obedit->type != OB_MESH)
		return 0;

	em = BMEdit_FromObject(obedit);
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

static int UNUSED_FUNCTION(ED_operator_uvmap_mesh) (bContext * C)
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

static int is_image_texture_node(bNode *node)
{
	return ELEM(node->type, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT);
}

int ED_object_get_active_image(Object *ob, int mat_nr, Image **ima, ImageUser **iuser, bNode **node_r)
{
	Material *ma = give_current_material(ob, mat_nr);
	bNode *node = (ma && ma->use_nodes) ? nodeGetActiveTexture(ma->nodetree) : NULL;

	if (node && is_image_texture_node(node)) {
		if (ima) *ima = (Image *)node->id;
		if (iuser) *iuser = NULL;
		if (node_r) *node_r = node;
		return TRUE;
	}
	
	if (ima) *ima = NULL;
	if (iuser) *iuser = NULL;
	if (node_r) *node_r = node;

	return FALSE;
}

void ED_object_assign_active_image(Main *bmain, Object *ob, int mat_nr, Image *ima)
{
	Material *ma = give_current_material(ob, mat_nr);
	bNode *node = (ma && ma->use_nodes) ? nodeGetActiveTexture(ma->nodetree) : NULL;

	if (node && is_image_texture_node(node)) {
		node->id = &ima->id;
		ED_node_generic_update(bmain, ma->nodetree, node);
	}
}

/************************* assign image ************************/

void ED_uvedit_assign_image(Main *bmain, Scene *scene, Object *obedit, Image *ima, Image *previma)
{
	BMEditMesh *em;
	BMFace *efa;
	BMIter iter;
	MTexPoly *tf;
	int update = 0;
	
	/* skip assigning these procedural images... */
	if (ima && (ima->type == IMA_TYPE_R_RESULT || ima->type == IMA_TYPE_COMPOSITE))
		return;

	/* verify we have a mesh we can work with */
	if (!obedit || (obedit->type != OB_MESH))
		return;

	em = BMEdit_FromObject(obedit);
	if (!em || !em->bm->totface) {
		return;
	}

	if (scene_use_new_shading_nodes(scene)) {
		/* new shading system, assign image in material */
		int sloppy = 1;
		BMFace *efa = BM_active_face_get(em->bm, sloppy);

		if (efa)
			ED_object_assign_active_image(bmain, obedit, efa->mat_nr + 1, ima);
	}
	else {
		/* old shading system, assign image to selected faces */
		
		/* ensure we have a uv map */
		if (!CustomData_has_layer(&em->bm->pdata, CD_MTEXPOLY)) {
			BM_data_layer_add(em->bm, &em->bm->pdata, CD_MTEXPOLY);
			BM_data_layer_add(em->bm, &em->bm->ldata, CD_MLOOPUV);
			update = 1;
		}

		/* now assign to all visible faces */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if (uvedit_face_visible_test(scene, previma, efa, tf)) {
				if (ima) {
					tf->tpage = ima;
					
					if (ima->id.us == 0) id_us_plus(&ima->id);
					else id_lib_extern(&ima->id);
				}
				else {
					tf->tpage = NULL;
				}

				update = 1;
			}
		}

		/* and update depdency graph */
		if (update)
			DAG_id_tag_update(obedit->data, 0);
	}

}

/* dotile - 1, set the tile flag (from the space image)
 *          2, set the tile index for the faces. */
static int uvedit_set_tile(Object *obedit, Image *ima, int curtile)
{
	BMEditMesh *em;
	BMFace *efa;
	BMIter iter;
	MTexPoly *tf;
	
	/* verify if we have something to do */
	if (!ima || !ED_uvedit_test(obedit))
		return 0;

	if ((ima->tpageflag & IMA_TILES) == 0)
		return 0;

	/* skip assigning these procedural images... */
	if (ima->type == IMA_TYPE_R_RESULT || ima->type == IMA_TYPE_COMPOSITE)
		return 0;
	
	em = BMEdit_FromObject(obedit);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

		if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && BM_elem_flag_test(efa, BM_ELEM_SELECT))
			tf->tile = curtile;  /* set tile index */
	}

	DAG_id_tag_update(obedit->data, 0);

	return 1;
}

/*********************** space conversion *********************/

static void uvedit_pixel_to_float(SpaceImage *sima, float *dist, float pixeldist)
{
	int width, height;

	if (sima) {
		ED_space_image_size(sima, &width, &height);
	}
	else {
		width = 256;
		height = 256;
	}

	dist[0] = pixeldist / width;
	dist[1] = pixeldist / height;
}

/*************** visibility and selection utilities **************/

int uvedit_face_visible_nolocal(Scene *scene, BMFace *efa)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION)
		return (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) == 0);
	else
		return (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) == 0 && BM_elem_flag_test(efa, BM_ELEM_SELECT));
}

int uvedit_face_visible_test(Scene *scene, Image *ima, BMFace *efa, MTexPoly *tf)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SHOW_SAME_IMAGE)
		return (tf->tpage == ima) ? uvedit_face_visible_nolocal(scene, efa) : 0;
	else
		return uvedit_face_visible_nolocal(scene, efa);
}

int uvedit_face_select_test(Scene *scene, BMEditMesh *em, BMFace *efa)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION)
		return (BM_elem_flag_test(efa, BM_ELEM_SELECT));
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			if (!(luv->flag & MLOOPUV_VERTSEL))
				return 0;
		}

		return 1;
	}
}

int uvedit_face_select_enable(Scene *scene, BMEditMesh *em, BMFace *efa, const short do_history)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		BM_face_select_set(em->bm, efa, TRUE);
		if (do_history) {
			BM_select_history_store(em->bm, (BMElem *)efa);
		}
	}
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			luv->flag |= MLOOPUV_VERTSEL;
		}

		return 1;
	}

	return 0;
}

int uvedit_face_select_disable(Scene *scene, BMEditMesh *em, BMFace *efa)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		BM_face_select_set(em->bm, efa, FALSE);
	}
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			luv->flag &= ~MLOOPUV_VERTSEL;
		}

		return 1;
	}

	return 0;
}

int uvedit_edge_select_test(BMEditMesh *em, Scene *scene, BMLoop *l)
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

		luv1 = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		luv2 = CustomData_bmesh_get(&em->bm->ldata, l->next->head.data, CD_MLOOPUV);

		return (luv1->flag & MLOOPUV_VERTSEL) && (luv2->flag & MLOOPUV_VERTSEL);
	}
}

void uvedit_edge_select_enable(BMEditMesh *em, Scene *scene, BMLoop *l, const short do_history)

{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			BM_face_select_set(em->bm, l->f, TRUE);
		else if (ts->selectmode & SCE_SELECT_EDGE)
			BM_edge_select_set(em->bm, l->e, TRUE);
		else {
			BM_vert_select_set(em->bm, l->e->v1, TRUE);
			BM_vert_select_set(em->bm, l->e->v2, TRUE);
		}

		if (do_history) {
			BM_select_history_store(em->bm, (BMElem *)l->e);
		}
	}
	else {
		MLoopUV *luv1, *luv2;

		luv1 = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		luv2 = CustomData_bmesh_get(&em->bm->ldata, l->next->head.data, CD_MLOOPUV);
		
		luv1->flag |= MLOOPUV_VERTSEL;
		luv2->flag |= MLOOPUV_VERTSEL;
	}
}

void uvedit_edge_select_disable(BMEditMesh *em, Scene *scene, BMLoop *l)

{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			BM_face_select_set(em->bm, l->f, FALSE);
		else if (ts->selectmode & SCE_SELECT_EDGE)
			BM_edge_select_set(em->bm, l->e, FALSE);
		else {
			BM_vert_select_set(em->bm, l->e->v1, FALSE);
			BM_vert_select_set(em->bm, l->e->v2, FALSE);
		}
	}
	else {
		MLoopUV *luv1, *luv2;

		luv1 = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		luv2 = CustomData_bmesh_get(&em->bm->ldata, l->next->head.data, CD_MLOOPUV);
		
		luv1->flag &= ~MLOOPUV_VERTSEL;
		luv2->flag &= ~MLOOPUV_VERTSEL;
	}
}

int uvedit_uv_select_test(BMEditMesh *em, Scene *scene, BMLoop *l)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			return BM_elem_flag_test(l->f, BM_ELEM_SELECT);
		else
			return BM_elem_flag_test(l->v, BM_ELEM_SELECT);
	}
	else {
		MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

		return luv->flag & MLOOPUV_VERTSEL;
	}
}

void uvedit_uv_select_enable(BMEditMesh *em, Scene *scene, BMLoop *l, const short do_history)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			BM_face_select_set(em->bm, l->f, TRUE);
		else
			BM_vert_select_set(em->bm, l->v, TRUE);

		if (do_history) {
			BM_select_history_remove(em->bm, (BMElem *)l->v);
		}
	}
	else {
		MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		
		luv->flag |= MLOOPUV_VERTSEL;
	}
}

void uvedit_uv_select_disable(BMEditMesh *em, Scene *scene, BMLoop *l)
{
	ToolSettings *ts = scene->toolsettings;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode & SCE_SELECT_FACE)
			BM_face_select_set(em->bm, l->f, FALSE);
		else
			BM_vert_select_set(em->bm, l->v, FALSE);
	}
	else {
		MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		
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
void uv_poly_center(BMEditMesh *em, BMFace *f, float r_cent[2])
{
	BMLoop *l;
	MLoopUV *luv;
	BMIter liter;

	zero_v2(r_cent);

	BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
		luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		add_v2_v2(r_cent, luv->uv);
	}

	mul_v2_fl(r_cent, 1.0f / (float)f->len);
}

float uv_poly_area(float uv[][2], int len)
{
	//BMESH_TODO: make this not suck
	//maybe use scanfill? I dunno.

	if (len >= 4)
		return area_tri_v2(uv[0], uv[1], uv[2]) + area_tri_v2(uv[0], uv[2], uv[3]); 
	else
		return area_tri_v2(uv[0], uv[1], uv[2]); 

	return 1.0;
}

void uv_poly_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		uv[i][0] = uv_orig[i][0] * aspx;
		uv[i][1] = uv_orig[i][1] * aspy;
	}
}

int ED_uvedit_minmax(Scene *scene, Image *ima, Object *obedit, float r_min[2], float r_max[2])
{
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	int sel;

	INIT_MINMAX2(r_min, r_max);

	sel = 0;
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_uv_select_test(em, scene, l)) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				DO_MINMAX2(luv->uv, r_min, r_max);
				sel = 1;
			}
		}
	}

	return sel;
}

static int ED_uvedit_median(Scene *scene, Image *ima, Object *obedit, float co[2])
{
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	unsigned int sel = 0;

	zero_v2(co);
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			if (uvedit_uv_select_test(em, scene, l)) {
				add_v2_v2(co, luv->uv);
				sel++;
			}
		}
	}

	mul_v2_fl(co, 1.0f / (float)sel);

	return (sel != 0);
}

static int uvedit_center(Scene *scene, Image *ima, Object *obedit, float cent[2], char mode)
{
	int change = FALSE;
	
	if (mode == V3D_CENTER) {  /* bounding box */
		float min[2], max[2];
		if (ED_uvedit_minmax(scene, ima, obedit, min, max)) {
			mid_v2_v2v2(cent, min, max);
			change = TRUE;
		}
	}
	else {
		if (ED_uvedit_median(scene, ima, obedit, cent)) {
			change = TRUE;
		}
	}

	return change;
}

/************************** find nearest ****************************/

void uv_find_nearest_edge(Scene *scene, Image *ima, BMEditMesh *em, const float co[2], NearestHit *hit)
{
	MTexPoly *tf;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv, *nextluv;
	float mindist, dist;
	int i;

	mindist = 1e10f;
	memset(hit, 0, sizeof(*hit));

	BM_mesh_elem_index_ensure(em->bm, BM_VERT);
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		i = 0;
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			nextluv = CustomData_bmesh_get(&em->bm->ldata, l->next->head.data, CD_MLOOPUV);

			dist = dist_to_line_segment_v2(co, luv->uv, nextluv->uv);

			if (dist < mindist) {
				hit->tf = tf;
				hit->efa = efa;
				
				hit->l = l;
				hit->nextl = l->next;
				hit->luv = luv;
				hit->nextluv = nextluv;
				hit->lindex = i;
				hit->vert1 = BM_elem_index_get(hit->l->v);
				hit->vert2 = BM_elem_index_get(hit->l->next->v);

				mindist = dist;
			}

			i++;
		}
	}
}

static void find_nearest_uv_face(Scene *scene, Image *ima, BMEditMesh *em, const float co[2], NearestHit *hit)
{
	MTexPoly *tf;
	BMFace *efa;
	BMIter iter;
	float mindist, dist, cent[2];

	mindist = 1e10f;
	memset(hit, 0, sizeof(*hit));

	/*this will fill in hit.vert1 and hit.vert2*/
	uv_find_nearest_edge(scene, ima, em, co, hit);
	hit->l = hit->nextl = NULL;
	hit->luv = hit->nextluv = NULL;

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;

		uv_poly_center(em, efa, cent);

		dist = fabs(co[0] - cent[0]) + fabs(co[1] - cent[1]);

		if (dist < mindist) {
			hit->tf = tf;
			hit->efa = efa;
			mindist = dist;
		}
	}
}

static int nearest_uv_between(BMEditMesh *em, BMFace *efa, int UNUSED(nverts), int id,
                              const float co[2], const float uv[2])
{
	BMLoop *l;
	MLoopUV *luv;
	BMIter iter;
	float m[3], v1[3], v2[3], c1, c2, *uv1, /* *uv2, */ /* UNUSED */ *uv3;
	int id1, id2, i;

	id1 = (id + efa->len - 1) % efa->len;
	id2 = (id + efa->len + 1) % efa->len;

	m[0] = co[0] - uv[0];
	m[1] = co[1] - uv[1];

	i = 0;
	BM_ITER_ELEM (l, &iter, efa, BM_LOOPS_OF_FACE) {
		luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		
		if (i == id1)
			uv1 = luv->uv;
		else if (i == id)
			;  /* uv2 = luv->uv; */ /* UNUSED */
		else if (i == id2)
			uv3 = luv->uv;

		i++;
	}

	sub_v3_v3v3(v1, uv1, uv);
	sub_v3_v3v3(v2, uv3, uv);

	/* m and v2 on same side of v-v1? */
	c1 = v1[0] * m[1] - v1[1] * m[0];
	c2 = v1[0] * v2[1] - v1[1] * v2[0];

	if (c1 * c2 < 0.0f)
		return 0;

	/* m and v1 on same side of v-v2? */
	c1 = v2[0] * m[1] - v2[1] * m[0];
	c2 = v2[0] * v1[1] - v2[1] * v1[0];

	return (c1 * c2 >= 0.0f);
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

	/*this will fill in hit.vert1 and hit.vert2*/
	uv_find_nearest_edge(scene, ima, em, co, hit);
	hit->l = hit->nextl = NULL;
	hit->luv = hit->nextluv = NULL;

	mindist = 1e10f;
	memset(hit, 0, sizeof(*hit));
	
	BM_mesh_elem_index_ensure(em->bm, BM_VERT);

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		i = 0;
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			if (penalty && uvedit_uv_select_test(em, scene, l))
				dist = fabs(co[0] - luv->uv[0]) + penalty[0] + fabs(co[1] - luv->uv[1]) + penalty[1];
			else
				dist = fabs(co[0] - luv->uv[0]) + fabs(co[1] - luv->uv[1]);

			if (dist <= mindist) {
				if (dist == mindist)
					if (!nearest_uv_between(em, efa, efa->len, i, co, luv->uv)) {
						i++;
						continue;
					}

				mindist = dist;

				hit->l = l;
				hit->nextl = l->next;
				hit->luv = luv;
				hit->nextluv = CustomData_bmesh_get(&em->bm->ldata, l->next->head.data, CD_MLOOPUV);
				hit->tf = tf;
				hit->efa = efa;
				hit->lindex = i;
				hit->vert1 = BM_elem_index_get(hit->l->v);
			}

			i++;
		}
	}
}

int ED_uvedit_nearest_uv(Scene *scene, Object *obedit, Image *ima, const float co[2], float r_uv[2])
{
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float mindist, dist;
	int found = FALSE;

	mindist = 1e10f;
	copy_v2_v2(r_uv, co);
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;
		
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			dist = fabs(co[0] - luv->uv[0]) + fabs(co[1] - luv->uv[1]);

			if (dist <= mindist) {
				mindist = dist;

				copy_v2_v2(r_uv, luv->uv);
				found = TRUE;
			}
		}
	}

	return found;
}

UvElement *ED_uv_element_get(UvElementMap *map, BMFace *efa, BMLoop *l)
{
	UvElement *element;

	element = map->vert[BM_elem_index_get(l->v)];

	for (; element; element = element->next)
		if (element->face == efa)
			return element;

	return NULL;
}

/*********************** loop select ***********************/

static void select_edgeloop_uv_vertex_loop_flag(UvMapVert *first)
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

static UvMapVert *select_edgeloop_uv_vertex_map_get(UvVertMap *vmap, BMFace *efa, int a)
{
	UvMapVert *iterv, *first;
	BMLoop *l;

	l = BM_iter_at_index(NULL, BM_LOOPS_OF_FACE, efa, a);
	first = EDBM_uv_vert_map_at_index(vmap,  BM_elem_index_get(l->v));

	for (iterv = first; iterv; iterv = iterv->next) {
		if (iterv->separate)
			first = iterv;
		if (iterv->f == BM_elem_index_get(efa))
			return first;
	}
	
	return NULL;
}

static int select_edgeloop_uv_edge_tag_faces(BMEditMesh *em, UvMapVert *first1, UvMapVert *first2, int *totface)
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
				efa = EDBM_face_at_index(em, iterv1->f);
				if (BM_elem_flag_test(efa, BM_ELEM_TAG))
					return 0;

				tot++;
				break;
			}
		}
	}

	if (*totface == 0) /* start edge */
		*totface = tot;
	else if (tot != *totface) /* check for same number of faces as start edge */
		return 0;

	/* tag the faces */
	for (iterv1 = first1; iterv1; iterv1 = iterv1->next) {
		if (iterv1->separate && iterv1 != first1)
			break;

		for (iterv2 = first2; iterv2; iterv2 = iterv2->next) {
			if (iterv2->separate && iterv2 != first2)
				break;

			if (iterv1->f == iterv2->f) {
				efa = EDBM_face_at_index(em, iterv1->f);
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
				break;
			}
		}
	}

	return 1;
}

static int select_edgeloop(Scene *scene, Image *ima, BMEditMesh *em, NearestHit *hit,
                           float limit[2], const short extend)
{
	BMFace *efa;
	BMIter iter, liter;
	BMLoop *l;
	MTexPoly *tf;
	UvVertMap *vmap;
	UvMapVert *iterv1, *iterv2;
	int a, looking, nverts, starttotf, select;

	/* setup */
	EDBM_index_arrays_init(em, 0, 0, 1);
	vmap = EDBM_uv_vert_map_create(em, 0, 0, limit);

	BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_FACE);

	if (!extend) {
		select_all_perform(scene, ima, em, SEL_DESELECT);
	}

	BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, FALSE);

	/* set flags for first face and verts */
	nverts = hit->efa->len;
	iterv1 = select_edgeloop_uv_vertex_map_get(vmap, hit->efa, hit->lindex);
	iterv2 = select_edgeloop_uv_vertex_map_get(vmap, hit->efa, (hit->lindex + 1) % nverts);
	select_edgeloop_uv_vertex_loop_flag(iterv1);
	select_edgeloop_uv_vertex_loop_flag(iterv2);

	starttotf = 0;
	select_edgeloop_uv_edge_tag_faces(em, iterv1, iterv2, &starttotf);

	/* sorry, first edge isn't even ok */
	if (iterv1->flag == 0 && iterv2->flag == 0) looking = 0;
	else looking = 1;

	/* iterate */
	while (looking) {
		looking = 0;

		/* find correct valence edges which are not tagged yet, but connect to tagged one */

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if (!BM_elem_flag_test(efa, BM_ELEM_TAG) && uvedit_face_visible_test(scene, ima, efa, tf)) {
				nverts = efa->len;
				for (a = 0; a < nverts; a++) {
					/* check face not hidden and not tagged */
					iterv1 = select_edgeloop_uv_vertex_map_get(vmap, efa, a);
					iterv2 = select_edgeloop_uv_vertex_map_get(vmap, efa, (a + 1) % nverts);
					
					if (!iterv1 || !iterv2)
						continue;

					/* check if vertex is tagged and has right valence */
					if (iterv1->flag || iterv2->flag) {
						if (select_edgeloop_uv_edge_tag_faces(em, iterv1, iterv2, &starttotf)) {
							looking = 1;
							BM_elem_flag_enable(efa, BM_ELEM_TAG);

							select_edgeloop_uv_vertex_loop_flag(iterv1);
							select_edgeloop_uv_vertex_loop_flag(iterv2);
							break;
						}
					}
				}
			}
		}
	}

	/* do the actual select/deselect */
	nverts = hit->efa->len;
	iterv1 = select_edgeloop_uv_vertex_map_get(vmap, hit->efa, hit->lindex);
	iterv2 = select_edgeloop_uv_vertex_map_get(vmap, hit->efa, (hit->lindex + 1) % nverts);
	iterv1->flag = 1;
	iterv2->flag = 1;

	if (extend) {
		if (uvedit_uv_select_test(em, scene, hit->l))
			select = 0;
		else
			select = 1;
	}
	else
		select = 1;
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		a = 0;
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			iterv1 = select_edgeloop_uv_vertex_map_get(vmap, efa, a);

			if (iterv1->flag) {
				if (select) uvedit_uv_select_enable(em, scene, l, FALSE);
				else uvedit_uv_select_disable(em, scene, l);
			}

			a++;
		}
	}

	/* cleanup */
	EDBM_uv_vert_map_free(vmap);
	EDBM_index_arrays_free(em);

	return (select) ? 1 : -1;
}

/*********************** linked select ***********************/

static void select_linked(Scene *scene, Image *ima, BMEditMesh *em, const float limit[2], NearestHit *hit, int extend)
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

	EDBM_index_arrays_init(em, 0, 0, 1); /* we can use this too */
	vmap = EDBM_uv_vert_map_create(em, 1, 1, limit);

	if (vmap == NULL)
		return;

	stack = MEM_mallocN(sizeof(*stack) * (em->bm->totface + 1), "UvLinkStack");
	flag = MEM_callocN(sizeof(*flag) * em->bm->totface, "UvLinkFlag");

	if (!hit) {
		BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

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
		a = 0;
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (efa == hit->efa) {
				stack[stacksize] = a;
				stacksize++;
				flag[a] = 1;
				break;
			}

			a++;
		}
	}

	while (stacksize > 0) {
		int j;

		stacksize--;
		a = stack[stacksize];
		
		j = 0;
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (j == a)
				break;

			j++;
		}

		i = 0;
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

			/* make_uv_vert_map_EM sets verts tmp.l to the indices */
			vlist = EDBM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));
			
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

			i++;
		}
	}

	if (!extend) {
		a = 0;
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				
				if (flag[a])
					luv->flag |= MLOOPUV_VERTSEL;
				else
					luv->flag &= ~MLOOPUV_VERTSEL;
			}
			a++;
		}
	}
	else {
		a = 0;
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (!flag[a]) {
				a++;
				continue;
			}
			
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						
				if (luv->flag & MLOOPUV_VERTSEL)
					break;
			}
			
			if (l)
				break;
			
			a++;
		}

		if (efa) {
			a = 0;
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				if (!flag[a]) {
					a++;
					continue;
				}

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					
					luv->flag &= ~MLOOPUV_VERTSEL;
				}

				a++;
			}
		}
		else {
			a = 0;
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				if (!flag[a]) {
					a++;
					continue;
				}

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					
					luv->flag |= MLOOPUV_VERTSEL;
				}

				a++;
			}
		}
	}
	
	MEM_freeN(stack);
	MEM_freeN(flag);
	EDBM_uv_vert_map_free(vmap);
	EDBM_index_arrays_free(em);
}

/* WATCH IT: this returns first selected UV,
 * not ideal in many cases since there could be multiple */
static float *uv_sel_co_from_eve(Scene *scene, Image *ima, BMEditMesh *em, BMVert *eve)
{
	BMIter liter;
	BMLoop *l;

	BM_ITER_ELEM (l, &liter, eve, BM_LOOPS_OF_VERT) {
		MTexPoly *tf = CustomData_bmesh_get(&em->bm->pdata, l->f->head.data, CD_MTEXPOLY);

		if (!uvedit_face_visible_test(scene, ima, l->f, tf))
			continue;

		if (uvedit_uv_select_test(em, scene, l)) {
			MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			return luv->uv;
		}
	}

	return NULL;
}

/* ******************** align operator **************** */

static void weld_align_uv(bContext *C, int tool)
{
	SpaceImage *sima;
	Scene *scene;
	Object *obedit;
	Image *ima;
	BMEditMesh *em;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float cent[2], min[2], max[2];
	
	scene = CTX_data_scene(C);
	obedit = CTX_data_edit_object(C);
	em = BMEdit_FromObject(obedit);
	ima = CTX_data_edit_image(C);
	sima = CTX_wm_space_image(C);

	INIT_MINMAX2(min, max);

	if (tool == 'a') {
		BMFace *efa;
		BMLoop *l;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					DO_MINMAX2(luv->uv, min, max);
				}
			}
		}

		tool = (max[0] - min[0] >= max[1] - min[1]) ? 'y' : 'x';
	}

	uvedit_center(scene, ima, obedit, cent, 0);

	if (tool == 'x' || tool == 'w') {
		BMFace *efa;
		BMLoop *l;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					luv->uv[0] = cent[0];
				}

			}
		}
	}

	if (tool == 'y' || tool == 'w') {
		BMFace *efa;
		BMLoop *l;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
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
				tf = CustomData_bmesh_get(&em->bm->pdata, l->f->head.data, CD_MTEXPOLY);

				if (!uvedit_face_visible_test(scene, ima, l->f, tf))
					continue;

				if (uvedit_uv_select_test(em, scene, l)) {
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
				float *uv_start = uv_sel_co_from_eve(scene, ima, em, eve_line[0]);
				float *uv_end   = uv_sel_co_from_eve(scene, ima, em, eve_line[BLI_array_count(eve_line) - 1]);
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
						tf = CustomData_bmesh_get(&em->bm->pdata, l->f->head.data, CD_MTEXPOLY);

						if (!uvedit_face_visible_test(scene, ima, l->f, tf))
							continue;

						if (uvedit_uv_select_test(em, scene, l)) {
							MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
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

static int align_exec(bContext *C, wmOperator *op)
{
	weld_align_uv(C, RNA_enum_get(op->ptr, "axis"));

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
	ot->exec = align_exec;
	ot->poll = ED_operator_image_active; /* requires space image */;

	/* properties */
	RNA_def_enum(ot->srna, "axis", axis_items, 'a', "Axis", "Axis to align UV locations on");
}

/* ******************** weld operator **************** */

static int weld_exec(bContext *C, wmOperator *UNUSED(op))
{
	weld_align_uv(C, 'w');

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
	ot->exec = weld_exec;
	ot->poll = ED_operator_uvedit;
}


/* ******************** (de)select all operator **************** */

static void select_all_perform(Scene *scene, Image *ima, BMEditMesh *em, int action)
{
	ToolSettings *ts = scene->toolsettings;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;

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
				tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
	
				if (!uvedit_face_visible_test(scene, ima, efa, tf))
					continue;

				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

					if (luv->flag & MLOOPUV_VERTSEL) {
						action = SEL_DESELECT;
						break;
					}
				}
			}
		}
	
		
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

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

static int select_all_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);

	int action = RNA_enum_get(op->ptr, "action");

	select_all_perform(scene, ima, em, action);

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
	ot->exec = select_all_exec;
	ot->poll = ED_operator_uvedit;

	WM_operator_properties_select_all(ot);
}

/* ******************** mouse select operator **************** */

static int sticky_select(float *limit, int hitv[4], int v, float *hituv[4], float *uv, int sticky, int hitlen)
{
	int i;

	/* this function test if some vertex needs to selected
	 * in addition to the existing ones due to sticky select */
	if (sticky == SI_STICKY_DISABLE)
		return 0;

	for (i = 0; i < hitlen; i++) {
		if (hitv[i] == v) {
			if (sticky == SI_STICKY_LOC) {
				if (fabsf(hituv[i][0] - uv[0]) < limit[0] && fabsf(hituv[i][1] - uv[1]) < limit[1])
					return 1;
			}
			else if (sticky == SI_STICKY_VERTEX)
				return 1;
		}
	}

	return 0;
}

static int mouse_select(bContext *C, const float co[2], int extend, int loop)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	NearestHit hit;
	int i, select = 1, selectmode, sticky, sync, *hitv = NULL;
	BLI_array_declare(hitv);
	int flush = 0, hitlen = 0; /* 0 == don't flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
	float limit[2], **hituv = NULL;
	BLI_array_declare(hituv);
	float penalty[2];

	/* notice 'limit' is the same no matter the zoom level, since this is like
	 * remove doubles and could annoying if it joined points when zoomed out.
	 * 'penalty' is in screen pixel space otherwise zooming in on a uv-vert and
	 * shift-selecting can consider an adjacent point close enough to add to
	 * the selection rather than de-selecting the closest. */

	uvedit_pixel_to_float(sima, limit, 0.05f);
	uvedit_pixel_to_float(sima, penalty, 5.0f / sima->zoom);

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
			BLI_array_free(hitv);
			BLI_array_free(hituv);
			return OPERATOR_CANCELLED;
		}

		hitlen = 0;
	}
	else if (selectmode == UV_SELECT_VERTEX) {
		/* find vertex */
		uv_find_nearest_vert(scene, ima, em, co, penalty, &hit);
		if (hit.efa == NULL) {
			BLI_array_free(hitv);
			BLI_array_free(hituv);
			return OPERATOR_CANCELLED;
		}

		/* mark 1 vertex as being hit */
		BLI_array_grow_items(hitv, hit.efa->len);
		BLI_array_grow_items(hituv, hit.efa->len);
		for (i = 0; i < hit.efa->len; i++) {
			hitv[i] = 0xFFFFFFFF;
		}

		hitv[hit.lindex] = hit.vert1;
		hituv[hit.lindex] = hit.luv->uv;

		hitlen = hit.efa->len;
	}
	else if (selectmode == UV_SELECT_EDGE) {
		/* find edge */
		uv_find_nearest_edge(scene, ima, em, co, &hit);
		if (hit.efa == NULL) {
			BLI_array_free(hitv);
			BLI_array_free(hituv);
			return OPERATOR_CANCELLED;
		}

		/* mark 2 edge vertices as being hit */
		BLI_array_grow_items(hitv,  hit.efa->len);
		BLI_array_grow_items(hituv, hit.efa->len);
		fill_vn_i(hitv, hit.efa->len, 0xFFFFFFFF);

		hitv[hit.lindex] = hit.vert1;
		hitv[(hit.lindex + 1) % hit.efa->len] = hit.vert2;
		hituv[hit.lindex] = hit.luv->uv;
		hituv[(hit.lindex + 1) % hit.efa->len] = hit.nextluv->uv;

		hitlen = hit.efa->len;
	}
	else if (selectmode == UV_SELECT_FACE) {
		/* find face */
		find_nearest_uv_face(scene, ima, em, co, &hit);
		if (hit.efa == NULL) {
			BLI_array_free(hitv);
			BLI_array_free(hituv);
			return OPERATOR_CANCELLED;
		}
		
		/* make active */
		BM_active_face_set(em->bm, hit.efa);

		/* mark all face vertices as being hit */

		BLI_array_grow_items(hitv,  hit.efa->len);
		BLI_array_grow_items(hituv, hit.efa->len);
		i = 0;
		BM_ITER_ELEM (l, &liter, hit.efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			hituv[i] = luv->uv;
			hitv[i] = BM_elem_index_get(l->v);
			i++;
		}
		
		hitlen = hit.efa->len;
	}
	else if (selectmode == UV_SELECT_ISLAND) {
		uv_find_nearest_vert(scene, ima, em, co, NULL, &hit);

		if (hit.efa == NULL) {
			BLI_array_free(hitv);
			BLI_array_free(hituv);
			return OPERATOR_CANCELLED;
		}

		hitlen = 0;
	}
	else {
		hitlen = 0;
		BLI_array_free(hitv);
		BLI_array_free(hituv);
		return OPERATOR_CANCELLED;
	}

	/* do selection */
	if (loop) {
		flush = select_edgeloop(scene, ima, em, &hit, limit, extend);
	}
	else if (selectmode == UV_SELECT_ISLAND) {
		select_linked(scene, ima, em, limit, &hit, extend);
	}
	else if (extend) {
		if (selectmode == UV_SELECT_VERTEX) {
			/* (de)select uv vertex */
			if (uvedit_uv_select_test(em, scene, hit.l)) {
				uvedit_uv_select_disable(em, scene, hit.l);
				select = 0;
			}
			else {
				uvedit_uv_select_enable(em, scene, hit.l, TRUE);
				select = 1;
			}
			flush = 1;
		}
		else if (selectmode == UV_SELECT_EDGE) {
			/* (de)select edge */
			if (uvedit_edge_select_test(em, scene, hit.l)) {
				uvedit_edge_select_disable(em, scene, hit.l);
				select = 0;
			}
			else {
				uvedit_edge_select_enable(em, scene, hit.l, TRUE);
				select = 1;
			}
			flush = 1;
		}
		else if (selectmode == UV_SELECT_FACE) {
			/* (de)select face */
			if (uvedit_face_select_test(scene, em, hit.efa)) {
				uvedit_face_select_disable(scene, em, hit.efa);
				select = 0;
			}
			else {
				uvedit_face_select_enable(scene, em, hit.efa, TRUE);
				select = 1;
			}
			flush = -1;
		}

		/* de-selecting an edge may deselect a face too - validate */
		if (sync) {
			if (select == FALSE) {
				BM_select_history_validate(em->bm);
			}
		}

		/* (de)select sticky uv nodes */
		if (sticky != SI_STICKY_DISABLE) {

			BM_mesh_elem_index_ensure(em->bm, BM_VERT);

			/* deselect */
			if (select == 0) {
				BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
					tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
					if (!uvedit_face_visible_test(scene, ima, efa, tf))
						continue;

					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						if (sticky_select(limit, hitv, BM_elem_index_get(l->v), hituv, luv->uv, sticky, hitlen))
							uvedit_uv_select_disable(em, scene, l);
					}
				}
				flush = -1;
			}
			/* select */
			else {
				BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
					tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
					if (!uvedit_face_visible_test(scene, ima, efa, tf))
						continue;

					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						if (sticky_select(limit, hitv, BM_elem_index_get(l->v), hituv, luv->uv, sticky, hitlen))
							uvedit_uv_select_enable(em, scene, l, FALSE);
					}
				}

				flush = 1;
			}			
		}
	}
	else {
		/* deselect all */
		select_all_perform(scene, ima, em, SEL_DESELECT);

		if (selectmode == UV_SELECT_VERTEX) {
			/* select vertex */
			uvedit_uv_select_enable(em, scene, hit.l, TRUE);
			flush = 1;
		}
		else if (selectmode == UV_SELECT_EDGE) {
			/* select edge */
			uvedit_edge_select_enable(em, scene, hit.l, TRUE);
			flush = 1;
		}
		else if (selectmode == UV_SELECT_FACE) {
			/* select face */
			uvedit_face_select_enable(scene, em, hit.efa, TRUE);
		}

		/* select sticky uvs */
		if (sticky != SI_STICKY_DISABLE) {
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
				if (!uvedit_face_visible_test(scene, ima, efa, tf))
					continue;
				
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					if (sticky == SI_STICKY_DISABLE) continue;
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

					if (sticky_select(limit, hitv, BM_elem_index_get(l->v), hituv, luv->uv, sticky, hitlen))
						uvedit_uv_select_enable(em, scene, l, FALSE);

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

	BLI_array_free(hitv);
	BLI_array_free(hituv);

	return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
}

static int select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend, loop;

	RNA_float_get_array(op->ptr, "location", co);
	extend = RNA_boolean_get(op->ptr, "extend");
	loop = 0;

	return mouse_select(C, co, extend, loop);
}

static int select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float co[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return select_exec(C, op);
}

static void UV_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->description = "Select UV vertices";
	ot->idname = "UV_OT_select";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = select_exec;
	ot->invoke = select_invoke;
	ot->poll = ED_operator_image_active; /* requires space image */;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds", -100.0f, 100.0f);
}

/* ******************** loop select operator **************** */

static int select_loop_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend, loop;

	RNA_float_get_array(op->ptr, "location", co);
	extend = RNA_boolean_get(op->ptr, "extend");
	loop = 1;

	return mouse_select(C, co, extend, loop);
}

static int select_loop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float co[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return select_loop_exec(C, op);
}

static void UV_OT_select_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Loop Select";
	ot->description = "Select a loop of connected UV vertices";
	ot->idname = "UV_OT_select_loop";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = select_loop_exec;
	ot->invoke = select_loop_invoke;
	ot->poll = ED_operator_image_active; /* requires space image */;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds", -100.0f, 100.0f);
}

/* ******************** linked select operator **************** */

static int select_linked_internal(bContext *C, wmOperator *op, wmEvent *event, int pick)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	float limit[2];
	int extend;

	NearestHit hit, *hit_p = NULL;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		BKE_report(op->reports, RPT_ERROR, "Can't select linked when sync selection is enabled");
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

		uv_find_nearest_vert(scene, ima, em, co, NULL, &hit);
		hit_p = &hit;
	}

	select_linked(scene, ima, em, limit, hit_p, extend);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static int select_linked_exec(bContext *C, wmOperator *op)
{
	return select_linked_internal(C, op, NULL, 0);
}

static void UV_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->description = "Select all UV vertices linked to the active UV map";
	ot->idname = "UV_OT_select_linked";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = select_linked_exec;
	ot->poll = ED_operator_image_active;    /* requires space image */

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");
}

static int select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return select_linked_internal(C, op, event, 1);
}

static int select_linked_pick_exec(bContext *C, wmOperator *op)
{
	return select_linked_internal(C, op, NULL, 1);
}

static void UV_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked Pick";
	ot->description = "Select all UV vertices linked under the mouse";
	ot->idname = "UV_OT_select_linked_pick";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke = select_linked_pick_invoke;
	ot->exec = select_linked_pick_exec;
	ot->poll = ED_operator_image_active; /* requires space image */;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");

	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds", -100.0f, 100.0f);
}

/* ******************** unlink selection operator **************** */

static int unlink_selection_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		BKE_report(op->reports, RPT_ERROR, "Can't unlink selection when sync selection is enabled");
		return OPERATOR_CANCELLED;
	}
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		int desel = 0;

		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tf))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			
			if (!(luv->flag & MLOOPUV_VERTSEL)) {
				desel = 1;
				break;
			}
		}

		if (desel) {
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				luv->flag &= ~MLOOPUV_VERTSEL;
			}
		}
	}
	
	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static void UV_OT_unlink_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlink Selection";
	ot->description = "Unlink selected UV vertices from active UV map";
	ot->idname = "UV_OT_unlink_selected";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = unlink_selection_exec;
	ot->poll = ED_operator_uvedit;
}

static void uv_select_sync_flush(ToolSettings *ts, BMEditMesh *em, const short select)
{
	/* bmesh API handles flushing but not on de-select */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode != SCE_SELECT_FACE) {
			if (select == FALSE) {
				EDBM_deselect_flush(em);
			}
			else {
				EDBM_select_flush(em);
			}
		}

		if (select == FALSE) {
			BM_select_history_validate(em->bm);
		}
	}
}

/* ******************** border select operator **************** */

/* This function sets the selection on tagged faces, need because settings the
 * selection a face is done in a number of places but it also needs to respect
 * the sticky modes for the UV verts, so dealing with the sticky modes is best
 * done in a separate function.
 * 
 * De-selects faces that have been tagged on efa->tmp.l.  */

static void uv_faces_do_sticky(SpaceImage *sima, Scene *scene, Object *obedit, short select)
{
	/* Selecting UV Faces with some modes requires us to change 
	 * the selection in other faces (depending on the sticky mode).
	 * 
	 * This only needs to be done when the Mesh is not used for
	 * selection (so for sticky modes, vertex or location based). */
	
	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	/* MTexPoly *tf; */
	
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
			/* tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY); */ /* UNUSED */

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
					if (select)
						uvedit_uv_select_enable(em, scene, l, FALSE);
					else
						uvedit_uv_select_disable(em, scene, l);
				}
			}
		}
	}
	else if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_LOC) {
		BMFace *efa_vlist;
		/* MTexPoly *tf_vlist; */ /* UNUSED */
		UvMapVert *start_vlist = NULL, *vlist_iter;
		struct UvVertMap *vmap;
		float limit[2];
		unsigned int efa_index;
		//BMVert *eve; /* removed vert counting for now */ 
		//int a;
		
		uvedit_pixel_to_float(sima, limit, 0.05);
		
		EDBM_index_arrays_init(em, 0, 0, 1);
		vmap = EDBM_uv_vert_map_create(em, 0, 0, limit);
		
		/* verts are numbered above in make_uv_vert_map_EM, make sure this stays true! */
		if (vmap == NULL) {
			return;
		}
		
		efa = BM_iter_new(&iter, em->bm, BM_FACES_OF_MESH, NULL);
		for (efa_index = 0; efa; efa = BM_iter_step(&iter), efa_index++) {
			if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
				/* tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY); */ /* UNUSED */
				
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					if (select)
						uvedit_uv_select_enable(em, scene, l, FALSE);
					else
						uvedit_uv_select_disable(em, scene, l);
					
					vlist_iter = EDBM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));
					
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
							efa_vlist = EDBM_face_at_index(em, vlist_iter->f);
							/* tf_vlist = CustomData_bmesh_get(&em->bm->pdata, efa_vlist->head.data, CD_MTEXPOLY); */ /* UNUSED */
							
							if (select)
								uvedit_uv_select_enable(em, scene, BM_iter_at_index(em->bm, BM_LOOPS_OF_FACE, efa_vlist, vlist_iter->tfindex), FALSE);
							else
								uvedit_uv_select_disable(em, scene, BM_iter_at_index(em->bm, BM_LOOPS_OF_FACE, efa_vlist, vlist_iter->tfindex));
						}
						vlist_iter = vlist_iter->next;
					}
				}
			}
		}
		EDBM_index_arrays_free(em);
		EDBM_uv_vert_map_free(vmap);
		
	}
	else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
				if (select)
					uvedit_face_select_enable(scene, em, efa, FALSE);
				else
					uvedit_face_select_disable(scene, em, efa);
			}
		}
	}
}

static int border_select_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	ARegion *ar = CTX_wm_region(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	rcti rect;
	rctf rectf;
	int change, pinned, select, faces, extend;

	/* get rectangle from operator */
	rect.xmin = RNA_int_get(op->ptr, "xmin");
	rect.ymin = RNA_int_get(op->ptr, "ymin");
	rect.xmax = RNA_int_get(op->ptr, "xmax");
	rect.ymax = RNA_int_get(op->ptr, "ymax");
		
	UI_view2d_region_to_view(&ar->v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(&ar->v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

	/* figure out what to select/deselect */
	select = (RNA_int_get(op->ptr, "gesture_mode") == GESTURE_MODAL_SELECT);
	pinned = RNA_boolean_get(op->ptr, "pinned");
	extend = RNA_boolean_get(op->ptr, "extend");

	if (!extend)
		select_all_perform(scene, ima, em, SEL_DESELECT);
	
	if (ts->uv_flag & UV_SYNC_SELECTION)
		faces = (ts->selectmode == SCE_SELECT_FACE);
	else
		faces = (ts->uv_selectmode == UV_SELECT_FACE);

	/* do actual selection */
	if (faces && !pinned) {
		/* handle face selection mode */
		float cent[2];

		change = 0;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			/* assume not touched */
			BM_elem_flag_disable(efa, BM_ELEM_TAG);

			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				uv_poly_center(em, efa, cent);
				if (BLI_in_rctf(&rectf, cent[0], cent[1])) {
					BM_elem_flag_enable(efa, BM_ELEM_TAG);
					change = 1;
				}
			}
		}

		/* (de)selects all tagged faces and deals with sticky modes */
		if (change)
			uv_faces_do_sticky(sima, scene, obedit, select);
	}
	else {
		/* other selection modes */
		change = 1;
		
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if (!uvedit_face_visible_test(scene, ima, efa, tf))
				continue;
			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

				if (!pinned || (ts->uv_flag & UV_SYNC_SELECTION) ) {

					/* UV_SYNC_SELECTION - can't do pinned selection */
					if (BLI_in_rctf(&rectf, luv->uv[0], luv->uv[1])) {
						if (select) uvedit_uv_select_enable(em, scene, l, FALSE);
						else uvedit_uv_select_disable(em, scene, l);
					}
				}
				else if (pinned) {
					if ((luv->flag & MLOOPUV_PINNED) && 
					    BLI_in_rctf(&rectf, luv->uv[0], luv->uv[1])) {
						if (select) uvedit_uv_select_enable(em, scene, l, FALSE);
						else uvedit_uv_select_disable(em, scene, l);
					}
				}
			}
		}
	}

	if (change) {
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
	ot->exec = border_select_exec;
	ot->modal = WM_border_select_modal;
	ot->poll = ED_operator_image_active; /* requires space image */;
	ot->cancel = WM_border_select_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "pinned", 0, "Pinned", "Border select pinned UVs only");

	WM_operator_properties_gesture_border(ot, TRUE);
}

/* ******************** circle select operator **************** */

static int select_uv_inside_ellipse(BMEditMesh *em, SpaceImage *UNUSED(sima), Scene *scene, int select,
                                    float *offset, float *ell, BMLoop *l, MLoopUV *luv)
{
	/* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
	float x, y, r2, *uv;

	uv = luv->uv;

	x = (uv[0] - offset[0]) * ell[0];
	y = (uv[1] - offset[1]) * ell[1];

	r2 = x * x + y * y;
	if (r2 < 1.0f) {
		if (select) uvedit_uv_select_enable(em, scene, l, FALSE);
		else        uvedit_uv_select_disable(em, scene, l);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

static int circle_select_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	ARegion *ar = CTX_wm_region(C);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	int x, y, radius, width, height, select;
	float zoomx, zoomy, offset[2], ellipse[2];
	int gesture_mode = RNA_int_get(op->ptr, "gesture_mode");
	int change = FALSE;

	/* get operator properties */
	select = (gesture_mode == GESTURE_MODAL_SELECT);
	x = RNA_int_get(op->ptr, "x");
	y = RNA_int_get(op->ptr, "y");
	radius = RNA_int_get(op->ptr, "radius");

	/* compute ellipse size and location, not a circle since we deal
	 * with non square image. ellipse is normalized, r = 1.0. */
	ED_space_image_size(sima, &width, &height);
	ED_space_image_zoom(sima, ar, &zoomx, &zoomy);

	ellipse[0] = width * zoomx / radius;
	ellipse[1] = height * zoomy / radius;

	UI_view2d_region_to_view(&ar->v2d, x, y, &offset[0], &offset[1]);
	
	/* do selection */
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			change |= select_uv_inside_ellipse(em, sima, scene, select, offset, ellipse, l, luv);
		}
	}

	if (change) {
		uv_select_sync_flush(ts, em, select);

		if (ts->uv_flag & UV_SYNC_SELECTION) {
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		}
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
	ot->exec = circle_select_exec;
	ot->poll = ED_operator_image_active; /* requires space image */;
	ot->cancel = WM_gesture_circle_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 0, INT_MIN, INT_MAX, "Radius", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
}


/* ******************** lasso select operator **************** */

static void do_lasso_select_mesh_uv(bContext *C, int mcords[][2], short moves, short select)
{
	Image *ima = CTX_data_edit_image(C);
	ARegion *ar = CTX_wm_region(C);
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BMEdit_FromObject(obedit);

	BMIter iter, liter;

	BMFace *efa;
	BMLoop *l;
	MTexPoly *tf;
	int screen_uv[2], change = TRUE;
	rcti rect;

	BLI_lasso_boundbox(&rect, mcords, moves);

	if (ts->uv_selectmode == UV_SELECT_FACE) { /* Face Center Sel */
		change = FALSE;
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			/* assume not touched */
			if ((select) != (uvedit_face_select_test(scene, em, efa))) {
				float cent[2];
				uv_poly_center(em, efa, cent);
				UI_view2d_view_to_region(&ar->v2d, cent[0], cent[1], &screen_uv[0], &screen_uv[1]);
				if (BLI_in_rcti(&rect, screen_uv[0], screen_uv[1]) &&
				    BLI_lasso_is_point_inside(mcords, moves, screen_uv[0], screen_uv[1], V2D_IS_CLIPPED))
				{
					uvedit_face_select_enable(scene, em, efa, FALSE);
					change = TRUE;
				}
			}
		}
	}
	else { /* Vert Sel */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					if ((select) != (uvedit_uv_select_test(em, scene, l))) {
						MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						UI_view2d_view_to_region(&ar->v2d, luv->uv[0], luv->uv[1], &screen_uv[0], &screen_uv[1]);
						if (BLI_in_rcti(&rect, screen_uv[0], screen_uv[1]) &&
						    BLI_lasso_is_point_inside(mcords, moves, screen_uv[0], screen_uv[1], V2D_IS_CLIPPED))
						{
							if (select) {
								uvedit_uv_select_enable(em, scene, l, FALSE);
							}
							else {
								uvedit_uv_select_disable(em, scene, l);
							}
						}
					}
				}
			}
		}
	}
	if (change) {
		uv_select_sync_flush(scene->toolsettings, em, select);

		if (ts->uv_flag & UV_SYNC_SELECTION) {
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		}
	}
}

static int uv_lasso_select_exec(bContext *C, wmOperator *op)
{
	int i = 0;
	int mcords[1024][2];

	RNA_BEGIN (op->ptr, itemptr, "path") {
		float loc[2];

		RNA_float_get_array(&itemptr, "loc", loc);
		mcords[i][0] = (int)loc[0];
		mcords[i][1] = (int)loc[1];
		i++;
		if (i >= 1024) break;
	}
	RNA_END;

	if (i > 1) {
		short select;

		select = !RNA_boolean_get(op->ptr, "deselect");
		do_lasso_select_mesh_uv(C, mcords, i, select);

		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void UV_OT_select_lasso(wmOperatorType *ot)
{
	ot->name = "Lasso Select UV";
	ot->description = "Select UVs using lasso selection";
	ot->idname = "UV_OT_select_lasso";

	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = uv_lasso_select_exec;
	ot->poll = ED_operator_image_active;
	ot->cancel = WM_gesture_lasso_cancel;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items");
	RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection instead of deselecting everything first");
}



/* ******************** snap cursor operator **************** */

static void snap_uv_to_pixel(float uvco[2], float w, float h)
{
	uvco[0] = ((float)((int)((uvco[0] * w) + 0.5f))) / w;
	uvco[1] = ((float)((int)((uvco[1] * h) + 0.5f))) / h;
}

static void snap_cursor_to_pixels(SpaceImage *sima)
{
	int width = 0, height = 0;

	ED_space_image_size(sima, &width, &height);
	snap_uv_to_pixel(sima->cursor, width, height);
}

static int snap_cursor_to_selection(Scene *scene, Image *ima, Object *obedit, SpaceImage *sima)
{
	return uvedit_center(scene, ima, obedit, sima->cursor, sima->around);
}

static int snap_cursor_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	int change = 0;

	switch (RNA_enum_get(op->ptr, "target")) {
		case 0:
			snap_cursor_to_pixels(sima);
			change = 1;
			break;
		case 1:
			change = snap_cursor_to_selection(scene, ima, obedit, sima);
			break;
	}

	if (!change)
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
	ot->exec = snap_cursor_exec;
	ot->poll = ED_operator_image_active; /* requires space image */;

	/* properties */
	RNA_def_enum(ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UVs to");
}

/* ******************** snap selection operator **************** */

static int snap_uvs_to_cursor(Scene *scene, Image *ima, Object *obedit, SpaceImage *sima)
{
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	short change = 0;

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_uv_select_test(em, scene, l)) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				copy_v2_v2(luv->uv, sima->cursor);
				change = 1;
			}
		}
	}

	return change;
}

static int snap_uvs_to_adjacent_unselected(Scene *scene, Image *ima, Object *obedit)
{
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMesh *bm = em->bm;
	BMFace *f;
	BMLoop *l, *lsub;
	BMIter iter, liter, lsubiter;
	MTexPoly *tface;
	MLoopUV *luv;
	short change = FALSE;
	
	/* index every vert that has a selected UV using it, but only once so as to
	 * get unique indices and to count how much to malloc */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		tface = CustomData_bmesh_get(&bm->pdata, f->head.data, CD_MTEXPOLY);
		if (uvedit_face_visible_test(scene, ima, f, tface)) {
			BM_elem_flag_enable(f, BM_ELEM_TAG);
			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				BM_elem_flag_set(l, BM_ELEM_TAG, uvedit_uv_select_test(em, scene, l));
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

							luv = CustomData_bmesh_get(&bm->ldata, lsub->head.data, CD_MLOOPUV);
							add_v2_v2(uv, luv->uv);
							uv_tot++;
						}
					}

					if (uv_tot) {
						luv = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MLOOPUV);
						mul_v2_v2fl(luv->uv, uv, 1.0f / (float)uv_tot);
						change = TRUE;
					}
				}
			}
		}
	}

	return change;
}

static int snap_uvs_to_pixels(SpaceImage *sima, Scene *scene, Object *obedit)
{
	BMEditMesh *em = BMEdit_FromObject(obedit);
	Image *ima = sima->image;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	int width = 0, height = 0;
	float w, h;
	short change = 0;

	ED_space_image_size(sima, &width, &height);
	w = (float)width;
	h = (float)height;
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_uv_select_test(em, scene, l)) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				snap_uv_to_pixel(luv->uv, w, h);
			}
		}

		change = 1;
	}

	return change;
}

static int snap_selection_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	int change = 0;

	switch (RNA_enum_get(op->ptr, "target")) {
		case 0:
			change = snap_uvs_to_pixels(sima, scene, obedit);
			break;
		case 1:
			change = snap_uvs_to_cursor(scene, ima, obedit, sima);
			break;
		case 2:
			change = snap_uvs_to_adjacent_unselected(scene, ima, obedit);
			break;
	}

	if (!change)
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
		{2, "ADJACENT_UNSELECTED", 0, "Adjacent Unselected", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Snap Selection";
	ot->description = "Snap selected UV vertices to target type";
	ot->idname = "UV_OT_snap_selected";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec = snap_selection_exec;
	ot->poll = ED_operator_image_active; /* requires space image */;

	/* properties */
	RNA_def_enum(ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UVs to");
}

/* ******************** pin operator **************** */

static int pin_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	int clear = RNA_boolean_get(op->ptr, "clear");
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			
			if (!clear) {
				if (uvedit_uv_select_test(em, scene, l))
					luv->flag |= MLOOPUV_PINNED;
			}
			else {
				if (uvedit_uv_select_test(em, scene, l))
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
	ot->exec = pin_exec;
	ot->poll = ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "Clear pinning for the selection instead of setting it");
}

/******************* select pinned operator ***************/

static int select_pinned_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = CTX_data_edit_image(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		tface = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, efa, tface))
			continue;

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			
			if (luv->flag & MLOOPUV_PINNED)
				uvedit_uv_select_enable(em, scene, l, FALSE);
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
	ot->exec = select_pinned_exec;
	ot->poll = ED_operator_uvedit;
}

/********************** hide operator *********************/

/* check if we are selected or unselected based on 'bool_test' arg,
 * needed for select swap support */
#define UV_SEL_TEST(luv, bool_test) ((((luv)->flag & MLOOPUV_VERTSEL) == MLOOPUV_VERTSEL) == bool_test)

/* is every UV vert selected or unselected depending on bool_test */
static int bm_face_is_all_uv_sel(BMesh *bm, BMFace *f, int bool_test)
{
	BMLoop *l_iter;
	BMLoop *l_first;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		MLoopUV *luv = CustomData_bmesh_get(&bm->ldata, l_iter->head.data, CD_MLOOPUV);
		if (!UV_SEL_TEST(luv, bool_test)) {
			return FALSE;
		}
	} while ((l_iter = l_iter->next) != l_first);

	return TRUE;
}

static int hide_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	MTexPoly *tf;
	int swap = RNA_boolean_get(op->ptr, "unselected");
	Image *ima = sima ? sima->image : NULL;
	int facemode = (ts->uv_selectmode == UV_SELECT_FACE);

	if (ts->uv_flag & UV_SYNC_SELECTION) {
		EDBM_mesh_hide(em, swap);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

		return OPERATOR_FINISHED;
	}

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		int hide = 0;

		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

		if (!uvedit_face_visible_test(scene, ima, efa, tf)) {
			continue;
		}

		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			if (UV_SEL_TEST(luv, !swap)) {
				hide = 1;
				break;
			}
		}

		if (hide) {
			/* note, a special case for edges could be used,
			 * for now edges act like verts and get flushed */
			if (facemode) {
				if (em->selectmode == SCE_SELECT_FACE) {
					/* check that every UV is selected */
					if (bm_face_is_all_uv_sel(em->bm, efa, TRUE) == !swap) {
						BM_face_select_set(em->bm, efa, FALSE);
					}
					uvedit_face_select_disable(scene, em, efa);
				}
				else {
					if (bm_face_is_all_uv_sel(em->bm, efa, TRUE) == !swap) {
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
							if (UV_SEL_TEST(luv, !swap)) {
								BM_vert_select_set(em->bm, l->v, FALSE);
							}
						}
					}
					if (!swap) uvedit_face_select_disable(scene, em, efa);


				}
			}
			else if (em->selectmode == SCE_SELECT_FACE) {
				/* check if a UV is de-selected */
				if (bm_face_is_all_uv_sel(em->bm, efa, FALSE) != !swap) {
					BM_face_select_set(em->bm, efa, FALSE);
					uvedit_face_select_disable(scene, em, efa);
				}
			}
			else {
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					if (UV_SEL_TEST(luv, !swap)) {
						BM_vert_select_set(em->bm, l->v, FALSE);
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
	ot->exec = hide_exec;
	ot->poll = ED_operator_uvedit;

	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}

/****************** reveal operator ******************/

static int reveal_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	int facemode = (ts->uv_selectmode == UV_SELECT_FACE);
	int stickymode = sima ? (sima->sticky != SI_STICKY_DISABLE) : 1;

	/* note on tagging, selecting faces needs to be delayed so it doesn't select the verts and
	 * confuse our checks on selected verts. */

	/* call the mesh function if we are in mesh sync sel */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		EDBM_mesh_reveal(em);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

		return OPERATOR_FINISHED;
	}
	if (facemode) {
		if (em->selectmode == SCE_SELECT_FACE) {
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				BM_elem_flag_disable(efa, BM_ELEM_TAG);
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						luv->flag |= MLOOPUV_VERTSEL;
					}
					/* BM_face_select_set(em->bm, efa, TRUE); */
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
								luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
								luv->flag |= MLOOPUV_VERTSEL;
							}
							/* BM_face_select_set(em->bm, efa, TRUE); */
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
								luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
								luv->flag |= MLOOPUV_VERTSEL;
							}
						}
						/* BM_face_select_set(em->bm, efa, TRUE); */
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
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					luv->flag |= MLOOPUV_VERTSEL;
				}
				/* BM_face_select_set(em->bm, efa, TRUE); */
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
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						luv->flag |= MLOOPUV_VERTSEL;
					}
				}
				/* BM_face_select_set(em->bm, efa, TRUE); */
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
			}
		}
	}
	
	/* re-select tagged faces */
	BM_mesh_elem_hflag_enable_test(em->bm, BM_FACE, BM_ELEM_SELECT, TRUE, BM_ELEM_TAG);

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
	ot->exec = reveal_exec;
	ot->poll = ED_operator_uvedit;
}

/******************** set 3d cursor operator ********************/

static int set_2d_cursor_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	float location[2];

	if (!sima)
		return OPERATOR_CANCELLED;

	RNA_float_get_array(op->ptr, "location", location);
	sima->cursor[0] = location[0];
	sima->cursor[1] = location[1];
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, NULL);
	
	return OPERATOR_FINISHED;
}

static int set_2d_cursor_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float location[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return set_2d_cursor_exec(C, op);
}

static void UV_OT_cursor_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set 2D Cursor";
	ot->description = "Set 2D cursor location";
	ot->idname = "UV_OT_cursor_set";
	
	/* api callbacks */
	ot->exec = set_2d_cursor_exec;
	ot->invoke = set_2d_cursor_invoke;
	ot->poll = ED_operator_image_active; /* requires space image */;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location", "Cursor location in normalised (0.0-1.0) coordinates", -10.0f, 10.0f);
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

static int set_tile_invoke(bContext *C, wmOperator *op, wmEvent *event)
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
	ot->poll = ED_operator_image_active; /* requires space image */;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int_vector(ot->srna, "tile", 2, NULL, 0, INT_MAX, "Tile", "Tile coordinate", 0, 10);
}


static int seams_from_islands_exec(bContext *C, wmOperator *op)
{
	UvVertMap *vmap;
	Object *ob = CTX_data_edit_object(C);
	Mesh *me = (Mesh *)ob->data;
	BMEditMesh *em;
	BMEdge *editedge;
	float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};
	char mark_seams = RNA_boolean_get(op->ptr, "mark_seams");
	char mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");

	BMesh *bm;
	BMIter iter;

	em = me->edit_btmesh;
	bm = em->bm;

	if (!EDBM_mtexpoly_check(em)) {
		return OPERATOR_CANCELLED;
	}

	/* This code sets editvert->tmp.l to the index. This will be useful later on. */
	EDBM_index_arrays_init(em, 0, 0, 1);
	vmap = EDBM_uv_vert_map_create(em, 0, 0, limit);

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
			efa1 = EDBM_face_at_index(em, mv1->f);
			mvinit2 = vmap->vert[BM_elem_index_get(editedge->v2)];

			for (mv2 = mvinit2; mv2; mv2 = mv2->next) {
				if (mv2->separate)
					mv2sep = mv2;

				efa2 = EDBM_face_at_index(em, mv2->f);
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

	EDBM_uv_vert_map_free(vmap);
	EDBM_index_arrays_free(em);

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
	ot->exec = seams_from_islands_exec;
	ot->poll = ED_operator_uvedit;

	RNA_def_boolean(ot->srna, "mark_seams", 1, "Mark Seams", "Mark boundary edges as seams");
	RNA_def_boolean(ot->srna, "mark_sharp", 0, "Mark Sharp", "Mark boundary edges as sharp");
}

static int mark_seam_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	Mesh *me = (Mesh *)ob->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMFace *efa;
	BMLoop *loop;

	BMIter iter, liter;

	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM (loop, &liter, efa, BM_LOOPS_OF_FACE) {
			if (uvedit_edge_select_test(em, scene, loop)) {
				BM_elem_flag_enable(loop->e, BM_ELEM_SEAM);
			}
		}
	}

	me->drawflag |= ME_DRAWSEAMS;

	if (scene->toolsettings->edge_mode_live_unwrap)
		ED_unwrap_lscm(scene, ob, FALSE);

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
	ot->exec = mark_seam_exec;
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
	WM_operatortype_append(UV_OT_unlink_selected);
	WM_operatortype_append(UV_OT_select_pinned);
	WM_operatortype_append(UV_OT_select_border);
	WM_operatortype_append(UV_OT_select_lasso);
	WM_operatortype_append(UV_OT_circle_select);

	WM_operatortype_append(UV_OT_snap_cursor);
	WM_operatortype_append(UV_OT_snap_selected);

	WM_operatortype_append(UV_OT_align);

	WM_operatortype_append(UV_OT_stitch);

	WM_operatortype_append(UV_OT_seams_from_islands);
	WM_operatortype_append(UV_OT_mark_seam);
	WM_operatortype_append(UV_OT_weld);
	WM_operatortype_append(UV_OT_pin);

	WM_operatortype_append(UV_OT_average_islands_scale);
	WM_operatortype_append(UV_OT_cube_project);
	WM_operatortype_append(UV_OT_cylinder_project);
	WM_operatortype_append(UV_OT_from_view);
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
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select", SELECTMOUSE, KM_PRESS, 0, 0)->ptr, "extend", FALSE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", TRUE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_loop", SELECTMOUSE, KM_PRESS, KM_ALT, 0)->ptr, "extend", FALSE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_loop", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0)->ptr, "extend", TRUE);

	/* border/circle selection */
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "pinned", FALSE);
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_border", BKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "pinned", TRUE);

	WM_keymap_add_item(keymap, "UV_OT_circle_select", CKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "UV_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "deselect", FALSE);
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", TRUE);

	/* selection manipulation */
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0)->ptr, "extend", FALSE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0)->ptr, "extend", FALSE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked", LKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0)->ptr, "extend", TRUE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", TRUE);

	WM_keymap_add_item(keymap, "UV_OT_unlink_selected", LKEY, KM_PRESS, KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "UV_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "UV_OT_select_pinned", PKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_menu(keymap, "IMAGE_MT_uvs_weldalign", WKEY, KM_PRESS, 0, 0);

	/* uv operations */
	WM_keymap_add_item(keymap, "UV_OT_stitch", VKEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "UV_OT_pin", PKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "clear", FALSE);
	kmi = WM_keymap_add_item(keymap, "UV_OT_pin", PKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "clear", TRUE);

	/* unwrap */
	WM_keymap_add_item(keymap, "UV_OT_unwrap", EKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_minimize_stretch", VKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_pack_islands", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_average_islands_scale", AKEY, KM_PRESS, KM_CTRL, 0);

	/* hide */
	kmi = WM_keymap_add_item(keymap, "UV_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", FALSE);
	kmi = WM_keymap_add_item(keymap, "UV_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", TRUE);

	WM_keymap_add_item(keymap, "UV_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);

	/* cursor */
	WM_keymap_add_item(keymap, "UV_OT_cursor_set", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_tile_set", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);
	
	/* menus */
	WM_keymap_add_menu(keymap, "IMAGE_MT_uvs_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_menu(keymap, "IMAGE_MT_uvs_select_mode", TABKEY, KM_PRESS, KM_CTRL, 0);

	/* pivot */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", COMMAKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.uv_editor.pivot_point");
	RNA_string_set(kmi->ptr, "value", "CENTER");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", COMMAKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.uv_editor.pivot_point");
	RNA_string_set(kmi->ptr, "value", "MEDIAN");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", PERIODKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.uv_editor.pivot_point");
	RNA_string_set(kmi->ptr, "value", "CURSOR");

	ED_object_generic_keymap(keyconf, keymap, 2);

	transform_keymap_for_space(keyconf, keymap, SPACE_IMAGE);
}

