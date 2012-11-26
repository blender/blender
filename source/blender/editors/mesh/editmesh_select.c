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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_select.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_array.h"
#include "BLI_smallhash.h"
#include "BLI_heap.h"

#include "BKE_context.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_report.h"
#include "BKE_paint.h"
#include "BKE_tessmesh.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_uvedit.h"
#include "ED_object.h"
#include "ED_view3d.h"

#include "BIF_gl.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "mesh_intern.h"

#include "UI_resources.h"

/* ****************************** MIRROR **************** */

void EDBM_select_mirrored(Object *UNUSED(obedit), BMEditMesh *em, int extend)
{
	BMVert *v1, *v2;
	BMIter iter;

	BM_ITER_MESH (v1, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (!BM_elem_flag_test(v1, BM_ELEM_SELECT) || BM_elem_flag_test(v1, BM_ELEM_HIDDEN)) {
			BM_elem_flag_disable(v1, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_enable(v1, BM_ELEM_TAG);
		}
	}

	EDBM_verts_mirror_cache_begin(em, TRUE);

	if (!extend)
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BM_ITER_MESH (v1, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (!BM_elem_flag_test(v1, BM_ELEM_TAG) || BM_elem_flag_test(v1, BM_ELEM_HIDDEN))
			continue;

		v2 = EDBM_verts_mirror_get(em, v1);
		if (v2 && !BM_elem_flag_test(v2, BM_ELEM_HIDDEN)) {
			BM_vert_select_set(em->bm, v2, TRUE);
		}
	}

	EDBM_verts_mirror_cache_end(em);
}

void EDBM_automerge(Scene *scene, Object *obedit, int update)
{
	BMEditMesh *em;
	
	if ((scene->toolsettings->automerge) &&
	    (obedit && obedit->type == OB_MESH))
	{
		em = BMEdit_FromObject(obedit);
		if (!em)
			return;

		BMO_op_callf(em->bm, BMO_FLAG_DEFAULTS,
		             "automerge verts=%hv dist=%f",
		             BM_ELEM_SELECT, scene->toolsettings->doublimit);
		if (update) {
			DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
			BMEdit_RecalcTessellation(em);
		}
	}
}

/* ****************************** SELECTION ROUTINES **************** */

unsigned int bm_solidoffs = 0, bm_wireoffs = 0, bm_vertoffs = 0;    /* set in drawobject.c ... for colorindices */

/* facilities for border select and circle select */
static char *selbuf = NULL;

/* opengl doesn't support concave... */
static void draw_triangulated(const int mcords[][2], const short tot)
{
	ListBase lb = {NULL, NULL};
	DispList *dl;
	float *fp;
	int a;
	
	/* make displist */
	dl = MEM_callocN(sizeof(DispList), "poly disp");
	dl->type = DL_POLY;
	dl->parts = 1;
	dl->nr = tot;
	dl->verts = fp = MEM_callocN(tot * 3 * sizeof(float), "poly verts");
	BLI_addtail(&lb, dl);
	
	for (a = 0; a < tot; a++, fp += 3) {
		fp[0] = (float)mcords[a][0];
		fp[1] = (float)mcords[a][1];
	}
	
	/* do the fill */
	BKE_displist_fill(&lb, &lb, 0);

	/* do the draw */
	dl = lb.first;  /* filldisplist adds in head of list */
	if (dl->type == DL_INDEX3) {
		int *index;
		
		a = dl->parts;
		fp = dl->verts;
		index = dl->index;
		glBegin(GL_TRIANGLES);
		while (a--) {
			glVertex3fv(fp + 3 * index[0]);
			glVertex3fv(fp + 3 * index[1]);
			glVertex3fv(fp + 3 * index[2]);
			index += 3;
		}
		glEnd();
	}
	
	BKE_displist_free(&lb);
}


/* reads rect, and builds selection array for quick lookup */
/* returns if all is OK */
int EDBM_backbuf_border_init(ViewContext *vc, short xmin, short ymin, short xmax, short ymax)
{
	struct ImBuf *buf;
	unsigned int *dr;
	int a;
	
	if (vc->obedit == NULL || vc->v3d->drawtype < OB_SOLID || (vc->v3d->flag & V3D_ZBUF_SELECT) == 0) {
		return 0;
	}
	
	buf = view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	if (buf == NULL) return 0;
	if (bm_vertoffs == 0) return 0;

	dr = buf->rect;
	
	/* build selection lookup */
	selbuf = MEM_callocN(bm_vertoffs + 1, "selbuf");
	
	a = (xmax - xmin + 1) * (ymax - ymin + 1);
	while (a--) {
		if (*dr > 0 && *dr <= bm_vertoffs)
			selbuf[*dr] = 1;
		dr++;
	}
	IMB_freeImBuf(buf);
	return 1;
}

int EDBM_backbuf_check(unsigned int index)
{
	if (selbuf == NULL) return 1;
	if (index > 0 && index <= bm_vertoffs)
		return selbuf[index];
	return 0;
}

void EDBM_backbuf_free(void)
{
	if (selbuf) MEM_freeN(selbuf);
	selbuf = NULL;
}

/* mcords is a polygon mask
 * - grab backbuffer,
 * - draw with black in backbuffer, 
 * - grab again and compare
 * returns 'OK' 
 */
int EDBM_backbuf_border_mask_init(ViewContext *vc, const int mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax)
{
	unsigned int *dr, *drm;
	struct ImBuf *buf, *bufmask;
	int a;
	
	/* method in use for face selecting too */
	if (vc->obedit == NULL) {
		if (!(paint_facesel_test(vc->obact) || paint_vertsel_test(vc->obact))) {
			return 0;
		}
	}
	else if (vc->v3d->drawtype < OB_SOLID || (vc->v3d->flag & V3D_ZBUF_SELECT) == 0) {
		return 0;
	}

	buf = view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	if (buf == NULL) return 0;
	if (bm_vertoffs == 0) return 0;

	dr = buf->rect;

	/* draw the mask */
	glDisable(GL_DEPTH_TEST);
	
	glColor3ub(0, 0, 0);
	
	/* yah, opengl doesn't do concave... tsk! */
	ED_region_pixelspace(vc->ar);
	draw_triangulated(mcords, tot);
	
	glBegin(GL_LINE_LOOP);  /* for zero sized masks, lines */
	for (a = 0; a < tot; a++) {
		glVertex2iv(mcords[a]);
	}
	glEnd();
	
	glFinish(); /* to be sure readpixels sees mask */
	
	/* grab mask */
	bufmask = view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);

	if (bufmask == NULL) {
		return 0; /* only when mem alloc fails, go crash somewhere else! */
	}
	else {
		drm = bufmask->rect;
	}

	/* build selection lookup */
	selbuf = MEM_callocN(bm_vertoffs + 1, "selbuf");
	
	a = (xmax - xmin + 1) * (ymax - ymin + 1);
	while (a--) {
		if (*dr > 0 && *dr <= bm_vertoffs && *drm == 0) selbuf[*dr] = 1;
		dr++; drm++;
	}
	IMB_freeImBuf(buf);
	IMB_freeImBuf(bufmask);

	return 1;
}

/* circle shaped sample area */
int EDBM_backbuf_circle_init(ViewContext *vc, short xs, short ys, short rads)
{
	struct ImBuf *buf;
	unsigned int *dr;
	short xmin, ymin, xmax, ymax, xc, yc;
	int radsq;
	
	/* method in use for face selecting too */
	if (vc->obedit == NULL) {
		if (!(paint_facesel_test(vc->obact) || paint_vertsel_test(vc->obact))) {
			return 0;
		}
	}
	else if (vc->v3d->drawtype < OB_SOLID || (vc->v3d->flag & V3D_ZBUF_SELECT) == 0) return 0;
	
	xmin = xs - rads; xmax = xs + rads;
	ymin = ys - rads; ymax = ys + rads;
	buf = view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	if (bm_vertoffs == 0) return 0;
	if (buf == NULL) return 0;

	dr = buf->rect;
	
	/* build selection lookup */
	selbuf = MEM_callocN(bm_vertoffs + 1, "selbuf");
	radsq = rads * rads;
	for (yc = -rads; yc <= rads; yc++) {
		for (xc = -rads; xc <= rads; xc++, dr++) {
			if (xc * xc + yc * yc < radsq) {
				if (*dr > 0 && *dr <= bm_vertoffs) selbuf[*dr] = 1;
			}
		}
	}

	IMB_freeImBuf(buf);
	return 1;
	
}

static void findnearestvert__doClosest(void *userData, BMVert *eve, const float screen_co[2], int index)
{
	struct { float mval_fl[2], pass, select, strict; float dist, lastIndex, closestIndex; BMVert *closest; } *data = userData;

	if (data->pass == 0) {
		if (index <= data->lastIndex)
			return;
	}
	else {
		if (index > data->lastIndex)
			return;
	}

	if (data->dist > 3) {
		float dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT) == data->select) {
			if (data->strict == 1) {
				return;
			}
			else {
				dist_test += 5;
			}
		}

		if (dist_test < data->dist) {
			data->dist = dist_test;
			data->closest = eve;
			data->closestIndex = index;
		}
	}
}




static unsigned int findnearestvert__backbufIndextest(void *handle, unsigned int index)
{
	BMEditMesh *em = (BMEditMesh *)handle;
	BMVert *eve = BM_vert_at_index(em->bm, index - 1);

	if (eve && BM_elem_flag_test(eve, BM_ELEM_SELECT)) return 0;
	return 1;
}
/**
 * findnearestvert
 * 
 * dist (in/out): minimal distance to the nearest and at the end, actual distance
 * sel: selection bias
 *      if SELECT, selected vertice are given a 5 pixel bias to make them further than unselect verts
 *      if 0, unselected vertice are given the bias
 * strict: if 1, the vertice corresponding to the sel parameter are ignored and not just biased 
 */
BMVert *EDBM_vert_find_nearest(ViewContext *vc, float *r_dist, const short sel, const short strict)
{
	if (vc->v3d->drawtype > OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		float distance;
		unsigned int index;
		BMVert *eve;
		
		if (strict) {
			index = view3d_sample_backbuf_rect(vc, vc->mval, 50, bm_wireoffs, 0xFFFFFF, &distance,
			                                   strict, vc->em, findnearestvert__backbufIndextest);
		}
		else {
			index = view3d_sample_backbuf_rect(vc, vc->mval, 50, bm_wireoffs, 0xFFFFFF, &distance,
			                                   0, NULL, NULL);
		}
		
		eve = BM_vert_at_index(vc->em->bm, index - 1);
		
		if (eve && distance < *r_dist) {
			*r_dist = distance;
			return eve;
		}
		else {
			return NULL;
		}
			
	}
	else {
		struct { float mval_fl[2], pass, select, strict; float dist, lastIndex, closestIndex; BMVert *closest; } data;
		static int lastSelectedIndex = 0;
		static BMVert *lastSelected = NULL;
		
		if (lastSelected && BM_vert_at_index(vc->em->bm, lastSelectedIndex) != lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval_fl[0] = vc->mval[0];
		data.mval_fl[1] = vc->mval[1];
		data.select = sel;
		data.dist = *r_dist;
		data.strict = strict;
		data.closest = NULL;
		data.closestIndex = 0;

		data.pass = 0;

		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

		mesh_foreachScreenVert(vc, findnearestvert__doClosest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

		if (data.dist > 3) {
			data.pass = 1;
			mesh_foreachScreenVert(vc, findnearestvert__doClosest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
		}

		*r_dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
}

/* returns labda for closest distance v1 to line-piece v2 - v3 */
float labda_PdistVL2Dfl(const float v1[2], const float v2[2], const float v3[2])
{
	float rc[2], len;
	
	rc[0] = v3[0] - v2[0];
	rc[1] = v3[1] - v2[1];
	len = rc[0] * rc[0] + rc[1] * rc[1];
	if (len == 0.0f)
		return 0.0f;
	
	return (rc[0] * (v1[0] - v2[0]) + rc[1] * (v1[1] - v2[1])) / len;
}

/* note; uses v3d, so needs active 3d window */
static void findnearestedge__doClosest(void *userData, BMEdge *eed, const float screen_co_a[2], const float screen_co_b[2], int UNUSED(index))
{
	struct { ViewContext vc; float mval_fl[2]; float dist; BMEdge *closest; } *data = userData;
	int distance;

	distance = dist_to_line_segment_v2(data->mval_fl, screen_co_a, screen_co_b);
		
	if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
		distance += 5;
	}

	if (distance < data->dist) {
		if (data->vc.rv3d->rflag & RV3D_CLIPPING) {
			float labda = labda_PdistVL2Dfl(data->mval_fl, screen_co_a, screen_co_b);
			float vec[3];

			vec[0] = eed->v1->co[0] + labda * (eed->v2->co[0] - eed->v1->co[0]);
			vec[1] = eed->v1->co[1] + labda * (eed->v2->co[1] - eed->v1->co[1]);
			vec[2] = eed->v1->co[2] + labda * (eed->v2->co[2] - eed->v1->co[2]);

			if (ED_view3d_clipping_test(data->vc.rv3d, vec, TRUE) == 0) {
				data->dist = distance;
				data->closest = eed;
			}
		}
		else {
			data->dist = distance;
			data->closest = eed;
		}
	}
}
BMEdge *EDBM_edge_find_nearest(ViewContext *vc, float *r_dist)
{

	if (vc->v3d->drawtype > OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		float distance;
		unsigned int index;
		BMEdge *eed;
		
		view3d_validate_backbuf(vc);
		
		index = view3d_sample_backbuf_rect(vc, vc->mval, 50, bm_solidoffs, bm_wireoffs, &distance, 0, NULL, NULL);
		eed = BM_edge_at_index(vc->em->bm, index - 1);
		
		if (eed && distance < *r_dist) {
			*r_dist = distance;
			return eed;
		}
		else {
			return NULL;
		}
	}
	else {
		struct { ViewContext vc; float mval_fl[2]; float dist; BMEdge *closest; } data;

		data.vc = *vc;
		data.mval_fl[0] = vc->mval[0];
		data.mval_fl[1] = vc->mval[1];
		data.dist = *r_dist;
		data.closest = NULL;
		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

		mesh_foreachScreenEdge(vc, findnearestedge__doClosest, &data, V3D_PROJ_TEST_CLIP_WIN);

		*r_dist = data.dist;
		return data.closest;
	}
}

static void findnearestface__getDistance(void *userData, BMFace *efa, const float screen_co[2], int UNUSED(index))
{
	struct { float mval_fl[2]; float dist; BMFace *toFace; } *data = userData;

	if (efa == data->toFace) {
		const float dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);

		if (dist_test < data->dist) {
			data->dist = dist_test;
		}
	}
}
static void findnearestface__doClosest(void *userData, BMFace *efa, const float screen_co[2], int index)
{
	struct { float mval_fl[2], pass; float dist, lastIndex, closestIndex; BMFace *closest; } *data = userData;

	if (data->pass == 0) {
		if (index <= data->lastIndex)
			return;
	}
	else {
		if (index > data->lastIndex)
			return;
	}

	if (data->dist > 3) {
		const float dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);

		if (dist_test < data->dist) {
			data->dist = dist_test;
			data->closest = efa;
			data->closestIndex = index;
		}
	}
}

BMFace *EDBM_face_find_nearest(ViewContext *vc, float *r_dist)
{

	if (vc->v3d->drawtype > OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		unsigned int index;
		BMFace *efa;

		view3d_validate_backbuf(vc);

		index = view3d_sample_backbuf(vc, vc->mval[0], vc->mval[1]);
		efa = BM_face_at_index(vc->em->bm, index - 1);
		
		if (efa) {
			struct { float mval_fl[2]; float dist; BMFace *toFace; } data;

			data.mval_fl[0] = vc->mval[0];
			data.mval_fl[1] = vc->mval[1];
			data.dist = 0x7FFF;     /* largest short */
			data.toFace = efa;

			mesh_foreachScreenFace(vc, findnearestface__getDistance, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

			if ((vc->em->selectmode == SCE_SELECT_FACE) || (data.dist < *r_dist)) {  /* only faces, no dist check */
				*r_dist = data.dist;
				return efa;
			}
		}
		
		return NULL;
	}
	else {
		struct { float mval_fl[2], pass; float dist, lastIndex, closestIndex; BMFace *closest; } data;
		static int lastSelectedIndex = 0;
		static BMFace *lastSelected = NULL;

		if (lastSelected && BM_face_at_index(vc->em->bm, lastSelectedIndex) != lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval_fl[0] = vc->mval[0];
		data.mval_fl[1] = vc->mval[1];
		data.dist = *r_dist;
		data.closest = NULL;
		data.closestIndex = 0;
		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

		data.pass = 0;
		mesh_foreachScreenFace(vc, findnearestface__doClosest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

		if (data.dist > 3.0f) {
			data.pass = 1;
			ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
			mesh_foreachScreenFace(vc, findnearestface__doClosest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
		}

		*r_dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
}

/* best distance based on screen coords. 
 * use em->selectmode to define how to use 
 * selected vertices and edges get disadvantage
 * return 1 if found one
 */
static int unified_findnearest(ViewContext *vc, BMVert **r_eve, BMEdge **r_eed, BMFace **r_efa)
{
	BMEditMesh *em = vc->em;
	float dist = 75.0f;
	
	*r_eve = NULL;
	*r_eed = NULL;
	*r_efa = NULL;
	
	/* no afterqueue (yet), so we check it now, otherwise the em_xxxofs indices are bad */
	view3d_validate_backbuf(vc);
	
	if (em->selectmode & SCE_SELECT_VERTEX)
		*r_eve = EDBM_vert_find_nearest(vc, &dist, BM_ELEM_SELECT, 0);
	if (em->selectmode & SCE_SELECT_FACE)
		*r_efa = EDBM_face_find_nearest(vc, &dist);

	dist -= 20; /* since edges select lines, we give dots advantage of 20 pix */
	if (em->selectmode & SCE_SELECT_EDGE)
		*r_eed = EDBM_edge_find_nearest(vc, &dist);

	/* return only one of 3 pointers, for frontbuffer redraws */
	if (*r_eed) {
		*r_efa = NULL; *r_eve = NULL;
	}
	else if (*r_efa) {
		*r_eve = NULL;
	}
	
	return (*r_eve || *r_eed || *r_efa);
}

/* ****************  SIMILAR "group" SELECTS. FACE, EDGE AND VERTEX ************** */
static EnumPropertyItem prop_similar_compare_types[] = {
	{SIM_CMP_EQ, "EQUAL", 0, "Equal", ""},
	{SIM_CMP_GT, "GREATER", 0, "Greater", ""},
	{SIM_CMP_LT, "LESS", 0, "Less", ""},

	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem prop_similar_types[] = {
	{SIMVERT_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMVERT_FACE, "FACE", 0, "Amount of Adjacent Faces", ""},
	{SIMVERT_VGROUP, "VGROUP", 0, "Vertex Groups", ""},
	{SIMVERT_EDGE, "EDGE", 0, "Amount of connecting edges", ""},

	{SIMEDGE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDGE_DIR, "DIR", 0, "Direction", ""},
	{SIMEDGE_FACE, "FACE", 0, "Amount of Faces Around an Edge", ""},
	{SIMEDGE_FACE_ANGLE, "FACE_ANGLE", 0, "Face Angles", ""},
	{SIMEDGE_CREASE, "CREASE", 0, "Crease", ""},
	{SIMEDGE_BEVEL, "BEVEL", 0, "Bevel", ""},
	{SIMEDGE_SEAM, "SEAM", 0, "Seam", ""},
	{SIMEDGE_SHARP, "SHARP", 0, "Sharpness", ""},

	{SIMFACE_MATERIAL, "MATERIAL", 0, "Material", ""},
	{SIMFACE_IMAGE, "IMAGE", 0, "Image", ""},
	{SIMFACE_AREA, "AREA", 0, "Area", ""},
	{SIMFACE_SIDES, "SIDES", 0, "Polygon Sides", ""},
	{SIMFACE_PERIMETER, "PERIMETER", 0, "Perimeter", ""},
	{SIMFACE_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMFACE_COPLANAR, "COPLANAR", 0, "Co-planar", ""},

	{0, NULL, 0, NULL, NULL}
};

/* selects new faces/edges/verts based on the existing selection */

static int similar_face_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(ob);
	BMOperator bmop;

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_faces faces=%hf type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_ALL, BM_ELEM_SELECT, TRUE);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(C, em, FALSE);

	/* we succeeded */
	return OPERATOR_FINISHED;
}	

/* ***************************************************** */

/* EDGE GROUP */

/* wrap the above function but do selection flushing edge to face */
static int similar_edge_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(ob);
	BMOperator bmop;

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_edges edges=%he type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "edges.out", BM_ALL, BM_ELEM_SELECT, TRUE);
	EDBM_selectmode_flush(em);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(C, em, FALSE);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

/* ********************************* */

/*
 * VERT GROUP
 * mode 1: same normal
 * mode 2: same number of face users
 * mode 3: same vertex groups
 */
static int similar_vert_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(ob);
	BMOperator bmop;
	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_verts verts=%hv type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_ALL, BM_ELEM_SELECT, TRUE);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_selectmode_flush(em);

	EDBM_update_generic(C, em, FALSE);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

static int edbm_select_similar_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "threshold");

	const int type = RNA_enum_get(op->ptr, "type");

	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set(op->ptr, prop, ts->select_thresh);
	}
	else {
		ts->select_thresh = RNA_property_float_get(op->ptr, prop);
	}

	if      (type < 100) return similar_vert_select_exec(C, op);
	else if (type < 200) return similar_edge_select_exec(C, op);
	else                 return similar_face_select_exec(C, op);
}

static EnumPropertyItem *select_similar_type_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop),
                                                   int *free)
{
	Object *obedit;

	if (!C) /* needed for docs and i18n tools */
		return prop_similar_types;

	obedit = CTX_data_edit_object(C);

	if (obedit && obedit->type == OB_MESH) {
		EnumPropertyItem *item = NULL;
		int a, totitem = 0;
		BMEditMesh *em = BMEdit_FromObject(obedit);

		if (em->selectmode & SCE_SELECT_VERTEX) {
			for (a = SIMVERT_NORMAL; a < SIMEDGE_LENGTH; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		else if (em->selectmode & SCE_SELECT_EDGE) {
			for (a = SIMEDGE_LENGTH; a < SIMFACE_MATERIAL; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		else if (em->selectmode & SCE_SELECT_FACE) {
			for (a = SIMFACE_MATERIAL; a <= SIMFACE_COPLANAR; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		RNA_enum_item_end(&item, &totitem);

		*free = 1;

		return item;
	}

	return NULL;
}

void MESH_OT_select_similar(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Similar";
	ot->idname = "MESH_OT_select_similar";
	ot->description = "Select similar vertices, edges or faces by property types";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = edbm_select_similar_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	prop = ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, SIMVERT_NORMAL, "Type", "");
	RNA_def_enum_funcs(prop, select_similar_type_itemf);

	RNA_def_enum(ot->srna, "compare", prop_similar_compare_types, SIM_CMP_EQ, "Compare", "");

	RNA_def_float(ot->srna, "threshold", 0.0, 0.0, 1.0, "Threshold", "", 0.0, 1.0);
}


/* ****************  Mode Select *************** */

static int edbm_select_mode_exec(bContext *C, wmOperator *op)
{
	const int type       = RNA_enum_get(op->ptr,    "type");
	const int action     = RNA_enum_get(op->ptr,    "action");
	const int use_extend = RNA_boolean_get(op->ptr, "use_extend");
	const int use_expand = RNA_boolean_get(op->ptr, "use_expand");

	if (EDBM_selectmode_toggle(C, type, action, use_extend, use_expand)) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int edbm_select_mode_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	// RNA_enum_set(op->ptr, "type");  /* type must be set already */
	RNA_boolean_set(op->ptr, "use_extend", event->shift);
	RNA_boolean_set(op->ptr, "use_expand", event->ctrl);
	return edbm_select_mode_exec(C, op);
}

void MESH_OT_select_mode(wmOperatorType *ot)
{
	PropertyRNA *prop;

	static EnumPropertyItem elem_items[] = {
		{SCE_SELECT_VERTEX, "VERT", ICON_VERTEXSEL, "Vertices", ""},
		{SCE_SELECT_EDGE,   "EDGE", ICON_EDGESEL, "Edges", ""},
		{SCE_SELECT_FACE,   "FACE", ICON_FACESEL, "Faces", ""},
		{0, NULL, 0, NULL, NULL},
	};

	static EnumPropertyItem actions_items[] = {
		{0, "DISABLE", 0, "Disable", "Disable selected markers"},
		{1, "ENABLE", 0, "Enable", "Enable selected markers"},
		{2, "TOGGLE", 0, "Toggle", "Toggle disabled flag for selected markers"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select Mode";
	ot->idname = "MESH_OT_select_mode";
	ot->description = "Change selection mode";

	/* api callbacks */
	ot->invoke = edbm_select_mode_invoke;
	ot->exec = edbm_select_mode_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_boolean(ot->srna, "use_extend", FALSE, "Extend", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "use_expand", FALSE, "Expand", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	ot->prop = prop = RNA_def_enum(ot->srna, "type", elem_items, 0, "Type", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	RNA_def_enum(ot->srna, "action", actions_items, 2, "Action", "Selection action to execute");
}

/* ***************************************************** */

/* ****************  LOOP SELECTS *************** */

static void walker_select(BMEditMesh *em, int walkercode, void *start, int select)
{
	BMesh *bm = em->bm;
	BMElem *ele;
	BMWalker walker;

	BMW_init(&walker, bm, walkercode,
	         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
	         BMW_FLAG_TEST_HIDDEN,
	         BMW_NIL_LAY);
	ele = BMW_begin(&walker, start);
	for (; ele; ele = BMW_step(&walker)) {
		if (!select) {
			BM_select_history_remove(bm, ele);
		}
		BM_elem_select_set(bm, ele, select);
	}
	BMW_end(&walker);
}

static int edbm_loop_multiselect_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMEdge *eed;
	BMEdge **edarray;
	int edindex;
	int looptype = RNA_boolean_get(op->ptr, "ring");
	
	BMIter iter;
	int totedgesel = 0;

	BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
			totedgesel++;
		}
	}
	
	edarray = MEM_mallocN(sizeof(BMEdge *) * totedgesel, "edge array");
	edindex = 0;
	
	BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
			edarray[edindex] = eed;
			edindex++;
		}
	}
	
	if (looptype) {
		for (edindex = 0; edindex < totedgesel; edindex += 1) {
			eed = edarray[edindex];
			walker_select(em, BMW_EDGERING, eed, TRUE);
		}
		EDBM_selectmode_flush(em);
	}
	else {
		for (edindex = 0; edindex < totedgesel; edindex += 1) {
			eed = edarray[edindex];
			walker_select(em, BMW_LOOP, eed, TRUE);
		}
		EDBM_selectmode_flush(em);
	}
	MEM_freeN(edarray);
//	if (EM_texFaceCheck())
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_loop_multi_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Multi Select Loops";
	ot->idname = "MESH_OT_loop_multi_select";
	ot->description = "Select a loop of connected edges by connection type";
	
	/* api callbacks */
	ot->exec = edbm_loop_multiselect_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "ring", 0, "Ring", "");
}

		
/* ***************** MAIN MOUSE SELECTION ************** */


/* ***************** loop select (non modal) ************** */

static void mouse_mesh_loop(bContext *C, int mval[2], short extend, short deselect, short toggle, short ring)
{
	ViewContext vc;
	BMEditMesh *em;
	BMEdge *eed;
	int select = TRUE;
	float dist = 50.0f;
	float mvalf[2];

	em_setup_viewcontext(C, &vc);
	mvalf[0] = (float)(vc.mval[0] = mval[0]);
	mvalf[1] = (float)(vc.mval[1] = mval[1]);
	em = vc.em;

	/* no afterqueue (yet), so we check it now, otherwise the bm_xxxofs indices are bad */
	view3d_validate_backbuf(&vc);

	eed = EDBM_edge_find_nearest(&vc, &dist);
	if (eed) {
		if (extend == 0 && deselect == 0 && toggle == 0) {
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		}
	
		if (extend) {
			select = TRUE;
		}
		else if (deselect) {
			select = FALSE;
		}
		else if (BM_elem_flag_test(eed, BM_ELEM_SELECT) == 0) {
			select = TRUE;
		}
		else if (toggle) {
			select = FALSE;
		}

		if (em->selectmode & SCE_SELECT_FACE) {
			walker_select(em, BMW_FACELOOP, eed, select);
		}
		else if (em->selectmode & SCE_SELECT_EDGE) {
			if (ring)
				walker_select(em, BMW_EDGERING, eed, select);
			else
				walker_select(em, BMW_LOOP, eed, select);
		}
		else if (em->selectmode & SCE_SELECT_VERTEX) {
			if (ring)
				walker_select(em, BMW_EDGERING, eed, select);

			else
				walker_select(em, BMW_LOOP, eed, select);
		}

		EDBM_selectmode_flush(em);

		/* sets as active, useful for other tools */
		if (select) {
			if (em->selectmode & SCE_SELECT_VERTEX) {
				/* Find nearest vert from mouse
				 * (initialize to large values incase only one vertex can be projected) */
				float v1_co[2], v2_co[2];
				float length_1 = FLT_MAX;
				float length_2 = FLT_MAX;

				/* We can't be sure this has already been set... */
				ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

				if (ED_view3d_project_float_object(vc.ar, eed->v1->co, v1_co, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
					length_1 = len_squared_v2v2(mvalf, v1_co);
				}

				if (ED_view3d_project_float_object(vc.ar, eed->v2->co, v2_co, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
					length_2 = len_squared_v2v2(mvalf, v2_co);
				}
#if 0
				printf("mouse to v1: %f\nmouse to v2: %f\n", len_squared_v2v2(mvalf, v1_co),
				       len_squared_v2v2(mvalf, v2_co));
#endif
				BM_select_history_store(em->bm, (length_1 < length_2) ? eed->v1 : eed->v2);
			}
			else if (em->selectmode & SCE_SELECT_EDGE) {
				BM_select_history_store(em->bm, eed);
			}
			else if (em->selectmode & SCE_SELECT_FACE) {
				/* Select the face of eed which is the nearest of mouse. */
				BMFace *f, *efa = NULL;
				BMIter iterf;
				float best_dist = MAXFLOAT;

				/* We can't be sure this has already been set... */
				ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

				BM_ITER_ELEM(f, &iterf, eed, BM_FACES_OF_EDGE) {
					if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
						float cent[3];
						float co[2], tdist;

						BM_face_calc_center_mean(f, cent);
						if (ED_view3d_project_float_object(vc.ar, cent, co, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
							tdist = len_squared_v2v2(mvalf, co);
							if (tdist < best_dist) {
/*								printf("Best face: %p (%f)\n", f, tdist);*/
								best_dist = tdist;
								efa = f;
							}
						}
					}
				}
				if (efa) {
					BM_active_face_set(em->bm, efa);
					BM_select_history_store(em->bm, efa);
				}
			}
		}

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit);
	}
}

static int edbm_select_loop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	view3d_operator_needs_opengl(C);
	
	mouse_mesh_loop(C, event->mval, RNA_boolean_get(op->ptr, "extend"),
	                RNA_boolean_get(op->ptr, "deselect"),
	                RNA_boolean_get(op->ptr, "toggle"),
	                RNA_boolean_get(op->ptr, "ring"));
	
	/* cannot do tweaks for as long this keymap is after transform map */
	return OPERATOR_FINISHED;
}

void MESH_OT_loop_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Loop Select";
	ot->idname = "MESH_OT_loop_select";
	ot->description = "Select a loop of connected edges";
	
	/* api callbacks */
	ot->invoke = edbm_select_loop_invoke;
	ot->poll = ED_operator_editmesh_region_view3d;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "Extend the selection");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Remove from the selection");
	RNA_def_boolean(ot->srna, "toggle", 0, "Toggle Select", "Toggle the selection");
	RNA_def_boolean(ot->srna, "ring", 0, "Select Ring", "Select ring");
}

void MESH_OT_edgering_select(wmOperatorType *ot)
{
	/* description */
	ot->name = "Edge Ring Select";
	ot->idname = "MESH_OT_edgering_select";
	ot->description = "Select an edge ring";
	
	/* callbacks */
	ot->invoke = edbm_select_loop_invoke;
	ot->poll = ED_operator_editmesh_region_view3d;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Remove from the selection");
	RNA_def_boolean(ot->srna, "toggle", 0, "Toggle Select", "Toggle the selection");
	RNA_def_boolean(ot->srna, "ring", 1, "Select Ring", "Select ring");
}

/* ******************* generic tag_shortest_path and helpers ****************** */

static float step_cost_3_v3(const float v1[3], const float v2[3], const float v3[3])
{
	float cost, d1[3], d2[3];


	/* The cost is based on the simple sum of the length of the two edgees... */
	sub_v3_v3v3(d1, v2, v1);
	sub_v3_v3v3(d2, v3, v2);
	cost = len_v3(d1) + len_v3(d2);

	/* but is biased to give higher values to sharp turns, so that it will take
	 * paths with fewer "turns" when selecting between equal-weighted paths between
	 * the two edges */
	cost = cost + 0.5f * cost * (2.0f - sqrtf(fabsf(dot_v3v3(d1, d2))));

	return cost;
}

/* ******************* edgetag_shortest_path and helpers ****************** */

static float edgetag_cut_cost(BMEdge *e1, BMEdge *e2, BMVert *v)
{
	BMVert *v1 = BM_edge_other_vert(e1, v);
	BMVert *v2 = BM_edge_other_vert(e2, v);
	return step_cost_3_v3(v1->co, v->co, v2->co);
}

static void edgetag_add_adjacent(Heap *heap, BMEdge *e1, BMEdge **edges_prev, float *cost)
{
	BMIter viter;
	BMVert *v;

	BMIter eiter;
	BMEdge *e2;

	const int e1_index = BM_elem_index_get(e1);

	BM_ITER_ELEM (v, &viter, e1, BM_VERTS_OF_EDGE) {
		BM_ITER_ELEM (e2, &eiter, v, BM_EDGES_OF_VERT) {
			if (!BM_elem_flag_test(e2, BM_ELEM_TAG)) {
				/* we know 'e2' is not visited, check it out! */
				const int e2_index = BM_elem_index_get(e2);
				const float cost_cut = edgetag_cut_cost(e1, e2, v);
				const float cost_new = cost[e1_index] + cost_cut;

				if (cost[e2_index] > cost_new) {
					cost[e2_index] = cost_new;
					edges_prev[e2_index] = e1;
					BLI_heap_insert(heap, cost_new, e2);
				}
			}
		}
	}
}

static void edgetag_context_set(BMesh *bm, Scene *scene, BMEdge *e, int val)
{
	
	switch (scene->toolsettings->edge_mode) {
		case EDGE_MODE_SELECT:
			BM_edge_select_set(bm, e, val);
			break;
		case EDGE_MODE_TAG_SEAM:
			BM_elem_flag_set(e, BM_ELEM_SEAM, val);
			break;
		case EDGE_MODE_TAG_SHARP:
			BM_elem_flag_set(e, BM_ELEM_SMOOTH, !val);
			break;
		case EDGE_MODE_TAG_CREASE:
			BM_elem_float_data_set(&bm->edata, e, CD_CREASE, (val) ? 1.0f : 0.0f);
			break;
		case EDGE_MODE_TAG_BEVEL:
			BM_elem_float_data_set(&bm->edata, e, CD_BWEIGHT, (val) ? 1.0f : 0.0f);
			break;
	}
}

static int edgetag_context_check(Scene *scene, BMesh *bm, BMEdge *e)
{
	switch (scene->toolsettings->edge_mode) {
		case EDGE_MODE_SELECT:
			return BM_elem_flag_test(e, BM_ELEM_SELECT) ? TRUE : FALSE;
		case EDGE_MODE_TAG_SEAM:
			return BM_elem_flag_test(e, BM_ELEM_SEAM);
		case EDGE_MODE_TAG_SHARP:
			return !BM_elem_flag_test(e, BM_ELEM_SMOOTH);
		case EDGE_MODE_TAG_CREASE:
			return BM_elem_float_data_get(&bm->edata, e, CD_CREASE) ? TRUE : FALSE;
		case EDGE_MODE_TAG_BEVEL:
			return BM_elem_float_data_get(&bm->edata, e, CD_BWEIGHT) ? TRUE : FALSE;
	}
	return 0;
}

static int edgetag_shortest_path(Scene *scene, BMesh *bm, BMEdge *e_src, BMEdge *e_dst)
{
	/* BM_ELEM_TAG flag is used to store visited edges */
	BMEdge *e;
	BMIter eiter;
	Heap *heap;
	float *cost;
	BMEdge **edges_prev;
	int i, totedge;

	/* note, would pass BM_EDGE except we are looping over all edges anyway */
	BM_mesh_elem_index_ensure(bm, BM_VERT /* | BM_EDGE */);

	BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, i) {
		if (BM_elem_flag_test(e, BM_ELEM_HIDDEN) == FALSE) {
			BM_elem_flag_disable(e, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_enable(e, BM_ELEM_TAG);
		}

		BM_elem_index_set(e, i); /* set_inline */
	}
	bm->elem_index_dirty &= ~BM_EDGE;

	/* alloc */
	totedge = bm->totedge;
	edges_prev = MEM_callocN(sizeof(*edges_prev) * totedge, "SeamPathPrevious");
	cost = MEM_mallocN(sizeof(*cost) * totedge, "SeamPathCost");

	fill_vn_fl(cost, totedge, 1e20f);

	/*
	 * Arrays are now filled as follows:
	 *
	 * As the search continues, prevedge[n] will be the previous edge on the shortest
	 * path found so far to edge n. The visitedhash will of course contain entries
	 * for edges that have been visited, cost[n] will contain the length of the shortest
	 * path to edge n found so far, Finally, heap is a priority heap which is built on the
	 * the same data as the cost array, but inverted: it is a worklist of edges prioritized
	 * by the shortest path found so far to the edge.
	 */

	/* regular dijkstra shortest path, but over edges instead of vertices */
	heap = BLI_heap_new();
	BLI_heap_insert(heap, 0.0f, e_src);
	cost[BM_elem_index_get(e_src)] = 0.0f;

	e = NULL;

	while (!BLI_heap_is_empty(heap)) {
		e = BLI_heap_popmin(heap);

		if (e == e_dst)
			break;

		if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
			BM_elem_flag_enable(e, BM_ELEM_TAG);
			edgetag_add_adjacent(heap, e, edges_prev, cost);
		}
	}
	
	if (e == e_dst) {
		short all_set = TRUE;

		/* Check whether the path is already completely tagged.
		 * if it is, the tags will be cleared instead of set. */
		e = e_dst;
		do {
			if (!edgetag_context_check(scene, bm, e)) {
				all_set = FALSE;
				break;
			}
		} while ((e = edges_prev[BM_elem_index_get(e)]));

		/* Follow path back and source and add or remove tags */
		e = e_dst;
		do {
			edgetag_context_set(bm, scene, e, !all_set);
		} while ((e = edges_prev[BM_elem_index_get(e)]));
	}

	MEM_freeN(edges_prev);
	MEM_freeN(cost);
	BLI_heap_free(heap, NULL);

	return 1;
}

/* ******************* mesh shortest path select, uses prev-selected edge ****************** */

/* since you want to create paths with multiple selects, it doesn't have extend option */
static int mouse_mesh_shortest_path_edge(bContext *C, ViewContext *vc)
{
	BMEditMesh *em = vc->em;
	BMEdge *e_dst;
	float dist = 75.0f;
	
	e_dst = EDBM_edge_find_nearest(vc, &dist);
	if (e_dst) {
		Mesh *me = vc->obedit->data;
		int path = 0;
		
		if (em->bm->selected.last) {
			BMEditSelection *ese = em->bm->selected.last;
			
			if (ese && ese->htype == BM_EDGE) {
				BMEdge *e_act;
				e_act = (BMEdge *)ese->ele;
				if (e_act != e_dst) {
					if (edgetag_shortest_path(vc->scene, em->bm, e_act, e_dst)) {
						BM_select_history_remove(em->bm, e_act);
						path = 1;
					}
				}
			}
		}
		if (path == 0) {
			int act = (edgetag_context_check(vc->scene, em->bm, e_dst) == 0);
			edgetag_context_set(em->bm, vc->scene, e_dst, act); /* switch the edge option */
		}
		
		EDBM_selectmode_flush(em);

		/* even if this is selected it may not be in the selection list */
		if (edgetag_context_check(vc->scene, em->bm, e_dst) == 0)
			BM_select_history_remove(em->bm, e_dst);
		else
			BM_select_history_store(em->bm, e_dst);
	
		/* force drawmode for mesh */
		switch (CTX_data_tool_settings(C)->edge_mode) {
			
			case EDGE_MODE_TAG_SEAM:
				me->drawflag |= ME_DRAWSEAMS;
				ED_uvedit_live_unwrap(vc->scene, vc->obedit);
				break;
			case EDGE_MODE_TAG_SHARP:
				me->drawflag |= ME_DRAWSHARP;
				break;
			case EDGE_MODE_TAG_CREASE:
				me->drawflag |= ME_DRAWCREASES;
				break;
			case EDGE_MODE_TAG_BEVEL:
				me->drawflag |= ME_DRAWBWEIGHTS;
				break;
		}
		
		EDBM_update_generic(C, em, FALSE);

		return TRUE;
	}
	else {
		return FALSE;
	}
}


/* ******************* facetag_shortest_path and helpers ****************** */


static float facetag_cut_cost(BMFace *f1, BMFace *f2, BMEdge *e)
{
	float f1_cent[3];
	float f2_cent[3];
	float e_cent[3];

	BM_face_calc_center_mean(f1, f1_cent);
	BM_face_calc_center_mean(f2, f2_cent);
	mid_v3_v3v3(e_cent, e->v1->co, e->v2->co);

	return step_cost_3_v3(f1_cent, e_cent, f2_cent);
}

static void facetag_add_adjacent(Heap *heap, BMFace *f1, BMFace **faces_prev, float *cost)
{
	BMIter liter;
	BMLoop *l2;
	BMFace *f2;

	const int f1_index = BM_elem_index_get(f1);

	/* loop over faces of face, but do so by first looping over loops */
	BM_ITER_ELEM (l2, &liter, f1, BM_LOOPS_OF_FACE) {
		BMLoop *l_first;
		BMLoop *l_iter;

		l_iter = l_first = l2;
		do {
			f2 = l_iter->f;
			if (!BM_elem_flag_test(f2, BM_ELEM_TAG)) {
				/* we know 'f2' is not visited, check it out! */
				const int f2_index = BM_elem_index_get(f2);
				const float cost_cut = facetag_cut_cost(f1, f2, l_iter->e);
				const float cost_new = cost[f1_index] + cost_cut;

				if (cost[f2_index] > cost_new) {
					cost[f2_index] = cost_new;
					faces_prev[f2_index] = f1;
					BLI_heap_insert(heap, cost_new, f2);
				}
			}
		} while ((l_iter = l_iter->radial_next) != l_first);
	}
}

static void facetag_context_set(BMesh *bm, Scene *UNUSED(scene), BMFace *f, int val)
{
	BM_face_select_set(bm, f, val);
}

static int facetag_context_check(Scene *UNUSED(scene), BMesh *UNUSED(bm), BMFace *f)
{
	return BM_elem_flag_test(f, BM_ELEM_SELECT) ? 1 : 0;
}

static int facetag_shortest_path(Scene *scene, BMesh *bm, BMFace *f_src, BMFace *f_dst)
{
	/* BM_ELEM_TAG flag is used to store visited edges */
	BMFace *f;
	BMIter fiter;
	Heap *heap;
	float *cost;
	BMFace **faces_prev;
	int i, totface;

	/* note, would pass BM_EDGE except we are looping over all faces anyway */
	// BM_mesh_elem_index_ensure(bm, BM_VERT /* | BM_EDGE */); // NOT NEEDED FOR FACETAG

	BM_ITER_MESH_INDEX (f, &fiter, bm, BM_FACES_OF_MESH, i) {
		if (BM_elem_flag_test(f, BM_ELEM_HIDDEN) == FALSE) {
			BM_elem_flag_disable(f, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_enable(f, BM_ELEM_TAG);
		}

		BM_elem_index_set(f, i); /* set_inline */
	}
	bm->elem_index_dirty &= ~BM_FACE;

	/* alloc */
	totface = bm->totface;
	faces_prev = MEM_callocN(sizeof(*faces_prev) * totface, "SeamPathPrevious");
	cost = MEM_mallocN(sizeof(*cost) * totface, "SeamPathCost");

	fill_vn_fl(cost, totface, 1e20f);

	/*
	 * Arrays are now filled as follows:
	 *
	 * As the search continues, faces_prev[n] will be the previous face on the shortest
	 * path found so far to face n. The visitedhash will of course contain entries
	 * for faces that have been visited, cost[n] will contain the length of the shortest
	 * path to face n found so far, Finally, heap is a priority heap which is built on the
	 * the same data as the cost array, but inverted: it is a worklist of faces prioritized
	 * by the shortest path found so far to the face.
	 */

	/* regular dijkstra shortest path, but over faces instead of vertices */
	heap = BLI_heap_new();
	BLI_heap_insert(heap, 0.0f, f_src);
	cost[BM_elem_index_get(f_src)] = 0.0f;

	f = NULL;

	while (!BLI_heap_is_empty(heap)) {
		f = BLI_heap_popmin(heap);

		if (f == f_dst)
			break;

		if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
			BM_elem_flag_enable(f, BM_ELEM_TAG);
			facetag_add_adjacent(heap, f, faces_prev, cost);
		}
	}

	if (f == f_dst) {
		short all_set = TRUE;

		/* Check whether the path is already completely tagged.
		 * if it is, the tags will be cleared instead of set. */
		f = f_dst;
		do {
			if (!facetag_context_check(scene, bm, f)) {
				all_set = FALSE;
				break;
			}
		} while ((f = faces_prev[BM_elem_index_get(f)]));

		/* Follow path back and source and add or remove tags */
		f = f_dst;
		do {
			facetag_context_set(bm, scene, f, !all_set);
		} while ((f = faces_prev[BM_elem_index_get(f)]));
	}

	MEM_freeN(faces_prev);
	MEM_freeN(cost);
	BLI_heap_free(heap, NULL);

	return 1;
}

static int mouse_mesh_shortest_path_face(bContext *C, ViewContext *vc)
{
	BMEditMesh *em = vc->em;
	BMFace *f_dst;
	float dist = 75.0f;

	f_dst = EDBM_face_find_nearest(vc, &dist);
	if (f_dst) {
		int path = 0;
		BMFace *f_act = BM_active_face_get(em->bm, FALSE, TRUE);

		if (f_act) {
			if (f_act != f_dst) {
				if (facetag_shortest_path(vc->scene, em->bm, f_act, f_dst)) {
					BM_select_history_remove(em->bm, f_act);
					path = 1;
				}
			}
		}
		if (path == 0) {
			int act = (facetag_context_check(vc->scene, em->bm, f_dst) == 0);
			facetag_context_set(em->bm, vc->scene, f_dst, act); /* switch the face option */
		}

		EDBM_selectmode_flush(em);

		/* even if this is selected it may not be in the selection list */
		if (facetag_context_check(vc->scene, em->bm, f_dst) == 0)
			BM_select_history_remove(em->bm, f_dst);
		else
			BM_select_history_store(em->bm, f_dst);

		BM_active_face_set(em->bm, f_dst);

		EDBM_update_generic(C, em, FALSE);

		return TRUE;
	}
	else {
		return FALSE;
	}
}


/* ******************* operator for edge and face tag ****************** */

static int edbm_shortest_path_select_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	ViewContext vc;
	BMEditMesh *em;

	view3d_operator_needs_opengl(C);

	em_setup_viewcontext(C, &vc);
	vc.mval[0] = event->mval[0];
	vc.mval[1] = event->mval[1];
	em = vc.em;

	if (em->selectmode & SCE_SELECT_EDGE) {
		if (mouse_mesh_shortest_path_edge(C, &vc)) {
			return OPERATOR_FINISHED;
		}
		else {
			return OPERATOR_PASS_THROUGH;
		}
	}
	else if (em->selectmode & SCE_SELECT_FACE) {
		if (mouse_mesh_shortest_path_face(C, &vc)) {
			return OPERATOR_FINISHED;
		}
		else {
			return OPERATOR_PASS_THROUGH;
		}
	}

	return OPERATOR_PASS_THROUGH;
}

static int edbm_shortest_path_select_poll(bContext *C)
{
	if (ED_operator_editmesh_region_view3d(C)) {
		Object *obedit = CTX_data_edit_object(C);
		BMEditMesh *em = BMEdit_FromObject(obedit);
		return (em->selectmode & (SCE_SELECT_EDGE | SCE_SELECT_FACE)) != 0;
	}
	return 0;
}

void MESH_OT_select_shortest_path(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shortest Path Select";
	ot->idname = "MESH_OT_select_shortest_path";
	ot->description = "Select shortest path between two selections";
	
	/* api callbacks */
	ot->invoke = edbm_shortest_path_select_invoke;
	ot->poll = edbm_shortest_path_select_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "");
}

/* ************************************************** */
/* here actual select happens */
/* gets called via generic mouse select operator */
int EDBM_select_pick(bContext *C, const int mval[2], short extend, short deselect, short toggle)
{
	ViewContext vc;
	BMVert *eve = NULL;
	BMEdge *eed = NULL;
	BMFace *efa = NULL;

	/* setup view context for argument to callbacks */
	em_setup_viewcontext(C, &vc);
	vc.mval[0] = mval[0];
	vc.mval[1] = mval[1];

	if (unified_findnearest(&vc, &eve, &eed, &efa)) {

		/* Deselect everything */
		if (extend == 0 && deselect == 0 && toggle == 0)
			EDBM_flag_disable_all(vc.em, BM_ELEM_SELECT);

		if (efa) {
			if (extend) {
				/* set the last selected face */
				BM_active_face_set(vc.em->bm, efa);

				/* Work-around: deselect first, so we can guarantee it will */
				/* be active even if it was already selected */
				BM_select_history_remove(vc.em->bm, efa);
				BM_face_select_set(vc.em->bm, efa, FALSE);
				BM_select_history_store(vc.em->bm, efa);
				BM_face_select_set(vc.em->bm, efa, TRUE);
			}
			else if (deselect) {
				BM_select_history_remove(vc.em->bm, efa);
				BM_face_select_set(vc.em->bm, efa, FALSE);
			}
			else {
				/* set the last selected face */
				BM_active_face_set(vc.em->bm, efa);

				if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
					BM_select_history_store(vc.em->bm, efa);
					BM_face_select_set(vc.em->bm, efa, TRUE);
				}
				else if (toggle) {
					BM_select_history_remove(vc.em->bm, efa);
					BM_face_select_set(vc.em->bm, efa, FALSE);
				}
			}
		}
		else if (eed) {
			if (extend) {
				/* Work-around: deselect first, so we can guarantee it will */
				/* be active even if it was already selected */
				BM_select_history_remove(vc.em->bm, eed);
				BM_edge_select_set(vc.em->bm, eed, FALSE);
				BM_select_history_store(vc.em->bm, eed);
				BM_edge_select_set(vc.em->bm, eed, TRUE);
			}
			else if (deselect) {
				BM_select_history_remove(vc.em->bm, eed);
				BM_edge_select_set(vc.em->bm, eed, FALSE);
			}
			else {
				if (!BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
					BM_select_history_store(vc.em->bm, eed);
					BM_edge_select_set(vc.em->bm, eed, TRUE);
				}
				else if (toggle) {
					BM_select_history_remove(vc.em->bm, eed);
					BM_edge_select_set(vc.em->bm, eed, FALSE);
				}
			}
		}
		else if (eve) {
			if (extend) {
				/* Work-around: deselect first, so we can guarantee it will */
				/* be active even if it was already selected */
				BM_select_history_remove(vc.em->bm, eve);
				BM_vert_select_set(vc.em->bm, eve, FALSE);
				BM_select_history_store(vc.em->bm, eve);
				BM_vert_select_set(vc.em->bm, eve, TRUE);
			}
			else if (deselect) {
				BM_select_history_remove(vc.em->bm, eve);
				BM_vert_select_set(vc.em->bm, eve, FALSE);
			}
			else {
				if (!BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
					BM_select_history_store(vc.em->bm, eve);
					BM_vert_select_set(vc.em->bm, eve, TRUE);
				}
				else if (toggle) {
					BM_select_history_remove(vc.em->bm, eve);
					BM_vert_select_set(vc.em->bm, eve, FALSE);
				}
			}
		}

		EDBM_selectmode_flush(vc.em);

		/* change active material on object */
		if (efa && efa->mat_nr != vc.obedit->actcol - 1) {
			vc.obedit->actcol = efa->mat_nr + 1;
			vc.em->mat_nr = efa->mat_nr;

			WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, NULL);

		}

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit);
		return 1;
	}

	return 0;
}

static void edbm_strip_selections(BMEditMesh *em)
{
	BMEditSelection *ese, *nextese;

	if (!(em->selectmode & SCE_SELECT_VERTEX)) {
		ese = em->bm->selected.first;
		while (ese) {
			nextese = ese->next;
			if (ese->htype == BM_VERT) BLI_freelinkN(&(em->bm->selected), ese);
			ese = nextese;
		}
	}
	if (!(em->selectmode & SCE_SELECT_EDGE)) {
		ese = em->bm->selected.first;
		while (ese) {
			nextese = ese->next;
			if (ese->htype == BM_EDGE) BLI_freelinkN(&(em->bm->selected), ese);
			ese = nextese;
		}
	}
	if (!(em->selectmode & SCE_SELECT_FACE)) {
		ese = em->bm->selected.first;
		while (ese) {
			nextese = ese->next;
			if (ese->htype == BM_FACE) BLI_freelinkN(&(em->bm->selected), ese);
			ese = nextese;
		}
	}
}

/* when switching select mode, makes sure selection is consistent for editing */
/* also for paranoia checks to make sure edge or face mode works */
void EDBM_selectmode_set(BMEditMesh *em)
{
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;
	
	em->bm->selectmode = em->selectmode;

	edbm_strip_selections(em); /* strip BMEditSelections from em->selected that are not relevant to new mode */
	
	if (em->selectmode & SCE_SELECT_VERTEX) {
		EDBM_select_flush(em);
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		/* deselect vertices, and select again based on edge select */
		eve = BM_iter_new(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
		for (; eve; eve = BM_iter_step(&iter)) BM_vert_select_set(em->bm, eve, FALSE);
		
		eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for (; eed; eed = BM_iter_step(&iter)) {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
				BM_edge_select_set(em->bm, eed, TRUE);
			}
		}
		
		/* selects faces based on edge status */
		EDBM_selectmode_flush(em);
	}
	else if (em->selectmode & SCE_SELECT_FACE) {
		/* deselect eges, and select again based on face select */
		eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for (; eed; eed = BM_iter_step(&iter)) BM_edge_select_set(em->bm, eed, FALSE);
		
		efa = BM_iter_new(&iter, em->bm, BM_FACES_OF_MESH, NULL);
		for (; efa; efa = BM_iter_step(&iter)) {
			if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
				BM_face_select_set(em->bm, efa, TRUE);
			}
		}
	}
}

void EDBM_selectmode_convert(BMEditMesh *em, const short selectmode_old, const short selectmode_new)
{
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;

	/* first tag-to-select, then select --- this avoids a feedback loop */

	/* have to find out what the selectionmode was previously */
	if (selectmode_old == SCE_SELECT_VERTEX) {
		if (selectmode_new == SCE_SELECT_EDGE) {
			/* select all edges associated with every selected vert */
			BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
				BM_elem_flag_set(eed, BM_ELEM_TAG, BM_edge_is_any_vert_flag_test(eed, BM_ELEM_SELECT));
			}

			BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test(eed, BM_ELEM_TAG)) {
					BM_edge_select_set(em->bm, eed, TRUE);
				}
			}
		}
		else if (selectmode_new == SCE_SELECT_FACE) {
			/* select all faces associated with every selected vert */
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				BM_elem_flag_set(efa, BM_ELEM_TAG, BM_face_is_any_vert_flag_test(efa, BM_ELEM_SELECT));
			}

			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
					BM_face_select_set(em->bm, efa, TRUE);
				}
			}
		}
	}
	else if (selectmode_old == SCE_SELECT_EDGE) {
		if (selectmode_new == SCE_SELECT_FACE) {
			/* select all faces associated with every selected edge */
			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				BM_elem_flag_set(efa, BM_ELEM_TAG, BM_face_is_any_edge_flag_test(efa, BM_ELEM_SELECT));
			}

			BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
				if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
					BM_face_select_set(em->bm, efa, TRUE);
				}
			}
		}
	}
}

/* user facing function, does notification and undo push */
int EDBM_selectmode_toggle(bContext *C, const short selectmode_new,
                           const int action, const int use_extend, const int use_expand)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = NULL;
	int ret = FALSE;

	if (obedit && obedit->type == OB_MESH) {
		em = BMEdit_FromObject(obedit);
	}

	if (em == NULL) {
		return ret;
	}

	switch (action) {
		case -1:
			/* already set */
			break;
		case 0:  /* disable */
			/* check we have something to do */
			if ((em->selectmode & selectmode_new) == 0) {
				return FALSE;
			}
			em->selectmode &= ~selectmode_new;
			break;
		case 1:  /* enable */
			/* check we have something to do */
			if ((em->selectmode & selectmode_new) != 0) {
				return FALSE;
			}
			em->selectmode |= selectmode_new;
			break;
		case 2:  /* toggle */
			/* can't disable this flag if its the only one set */
			if (em->selectmode == selectmode_new) {
				return FALSE;
			}
			em->selectmode ^= selectmode_new;
			break;
		default:
			BLI_assert(0);
	}

	switch (selectmode_new) {
		case SCE_SELECT_VERTEX:
			if (use_extend == 0 || em->selectmode == 0)
				em->selectmode = SCE_SELECT_VERTEX;
			ts->selectmode = em->selectmode;
			EDBM_selectmode_set(em);
			ret = TRUE;
			break;
		case SCE_SELECT_EDGE:
			if (use_extend == 0 || em->selectmode == 0) {
				if (use_expand) {
					const short selmode_max = highest_order_bit_s(ts->selectmode);
					if (selmode_max == SCE_SELECT_VERTEX) {
						EDBM_selectmode_convert(em, selmode_max, SCE_SELECT_EDGE);
					}
				}
				em->selectmode = SCE_SELECT_EDGE;
			}
			ts->selectmode = em->selectmode;
			EDBM_selectmode_set(em);
			ret = TRUE;
			break;
		case SCE_SELECT_FACE:
			if (use_extend == 0 || em->selectmode == 0) {
				if (use_expand) {
					const short selmode_max = highest_order_bit_s(ts->selectmode);
					if (ELEM(selmode_max, SCE_SELECT_VERTEX, SCE_SELECT_EDGE)) {
						EDBM_selectmode_convert(em, selmode_max, SCE_SELECT_FACE);
					}
				}

				em->selectmode = SCE_SELECT_FACE;
			}
			ts->selectmode = em->selectmode;
			EDBM_selectmode_set(em);
			ret = TRUE;
			break;
		default:
			BLI_assert(0);
			break;
	}

	if (ret == TRUE) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);
	}

	return ret;
}

void EDBM_deselect_by_material(BMEditMesh *em, const short index, const short select)
{
	BMIter iter;
	BMFace *efa;

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
			continue;
		if (efa->mat_nr == index) {
			BM_face_select_set(em->bm, efa, select);
		}
	}
}

void EDBM_select_toggle_all(BMEditMesh *em) /* exported for UV */
{
	if (em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel)
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	else
		EDBM_flag_enable_all(em, BM_ELEM_SELECT);
}

void EDBM_select_swap(BMEditMesh *em) /* exported for UV */
{
	BMIter iter;
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	
	if (em->bm->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN))
				continue;
			BM_vert_select_set(em->bm, eve, !BM_elem_flag_test(eve, BM_ELEM_SELECT));
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			BM_edge_select_set(em->bm, eed, !BM_elem_flag_test(eed, BM_ELEM_SELECT));
		}
	}
	else {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
				continue;
			BM_face_select_set(em->bm, efa, !BM_elem_flag_test(efa, BM_ELEM_SELECT));
		}

	}
//	if (EM_texFaceCheck())
}

int EDBM_select_interior_faces(BMEditMesh *em)
{
	BMesh *bm = em->bm;
	BMIter iter;
	BMIter eiter;
	BMFace *efa;
	BMEdge *eed;
	int ok;
	int change = FALSE;

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
			continue;


		ok = TRUE;
		BM_ITER_ELEM (eed, &eiter, efa, BM_EDGES_OF_FACE) {
			if (BM_edge_face_count(eed) < 3) {
				ok = FALSE;
				break;
			}
		}

		if (ok) {
			BM_face_select_set(bm, efa, TRUE);
			change = TRUE;
		}
	}

	return change;
}

static void linked_limit_default(bContext *C, wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "limit")) {
		Object *obedit = CTX_data_edit_object(C);
		BMEditMesh *em = BMEdit_FromObject(obedit);
		if (em->selectmode == SCE_SELECT_FACE)
			RNA_boolean_set(op->ptr, "limit", TRUE);
		else
			RNA_boolean_set(op->ptr, "limit", FALSE);
	}
}

static int edbm_select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	ViewContext vc;
	BMesh *bm;
	BMWalker walker;
	BMEditMesh *em;
	BMVert *eve;
	BMEdge *e, *eed;
	BMFace *efa;
	int sel = !RNA_boolean_get(op->ptr, "deselect");

	int limit;

	linked_limit_default(C, op);

	limit = RNA_boolean_get(op->ptr, "limit");

	/* unified_finednearest needs ogl */
	view3d_operator_needs_opengl(C);
	
	/* setup view context for argument to callbacks */
	em_setup_viewcontext(C, &vc);
	em = vc.em;

	if (em->bm->totedge == 0)
		return OPERATOR_CANCELLED;
	
	bm = em->bm;

	vc.mval[0] = event->mval[0];
	vc.mval[1] = event->mval[1];
	
	/* return warning! */
	
	if (unified_findnearest(&vc, &eve, &eed, &efa) == 0) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);
	
		return OPERATOR_CANCELLED;
	}
	
	if (em->selectmode == SCE_SELECT_FACE) {
		BMIter iter;

		if (efa == NULL)
			return OPERATOR_CANCELLED;

		if (limit) {
			/* grr, shouldn't need to alloc BMO flags here */
			BM_mesh_elem_toolflags_ensure(bm);
			/* hflag no-seam --> bmo-tag */
			BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
				/* BMESH_TODO, don't use 'BM_ELEM_SELECT' here, its a HFLAG only! */
				BMO_elem_flag_set(bm, e, BM_ELEM_SELECT, !BM_elem_flag_test(e, BM_ELEM_SEAM));
			}
		}

		/* walk */
		BMW_init(&walker, bm, BMW_ISLAND,
		         BMW_MASK_NOP, limit ? BM_ELEM_SELECT : BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		e = BMW_begin(&walker, efa);
		for (; efa; efa = BMW_step(&walker)) {
			BM_face_select_set(bm, efa, sel);
		}
		BMW_end(&walker);
	}
	else {
		if (efa) {
			eed = BM_FACE_FIRST_LOOP(efa)->e;
		}
		else if (!eed) {
			if (!eve || !eve->e)
				return OPERATOR_CANCELLED;

			eed = eve->e;
		}

		BMW_init(&walker, bm, BMW_SHELL,
		         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		e = BMW_begin(&walker, eed->v1);
		for (; e; e = BMW_step(&walker)) {
			BM_edge_select_set(bm, e, sel);
		}
		BMW_end(&walker);

		EDBM_selectmode_flush(em);
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "MESH_OT_select_linked_pick";
	ot->description = "(De)select all vertices linked to the edge under the mouse cursor";
	
	/* api callbacks */
	ot->invoke = edbm_select_linked_pick_invoke;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
	RNA_def_boolean(ot->srna, "limit", 0, "Limit by Seams", "");
}


static int edbm_select_linked_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMesh *bm = em->bm;
	BMIter iter;
	BMVert *v;
	BMEdge *e;
	BMWalker walker;

	int limit;

	linked_limit_default(C, op);

	limit = RNA_boolean_get(op->ptr, "limit");

	if (em->selectmode == SCE_SELECT_FACE) {
		BMFace *efa;

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			BM_elem_flag_set(efa, BM_ELEM_TAG, (BM_elem_flag_test(efa, BM_ELEM_SELECT) &&
			                                    !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)));
		}

		if (limit) {
			/* grr, shouldn't need to alloc BMO flags here */
			BM_mesh_elem_toolflags_ensure(bm);
			BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
				/* BMESH_TODO, don't use 'BM_ELEM_SELECT' here, its a HFLAG only! */
				BMO_elem_flag_set(bm, e, BM_ELEM_SELECT, !BM_elem_flag_test(e, BM_ELEM_SEAM));
			}
		}

		BMW_init(&walker, bm, BMW_ISLAND,
		         BMW_MASK_NOP, limit ? BM_ELEM_SELECT : BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
				e = BMW_begin(&walker, efa);
				for (; efa; efa = BMW_step(&walker)) {
					BM_face_select_set(bm, efa, TRUE);
				}
			}
		}
		BMW_end(&walker);

		if (limit) {
			BM_mesh_elem_toolflags_clear(bm);
		}
	}
	else {
		BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				BM_elem_flag_enable(v, BM_ELEM_TAG);
			}
			else {
				BM_elem_flag_disable(v, BM_ELEM_TAG);
			}
		}

		BMW_init(&walker, em->bm, BMW_SHELL,
		         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_TEST_HIDDEN,
		         BMW_NIL_LAY);

		BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
				e = BMW_begin(&walker, v);
				for (; e; e = BMW_step(&walker)) {
					BM_vert_select_set(em->bm, e->v1, TRUE);
					BM_vert_select_set(em->bm, e->v2, TRUE);
				}
			}
		}
		BMW_end(&walker);
	}
	EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked All";
	ot->idname = "MESH_OT_select_linked";
	ot->description = "Select all vertices linked to the active mesh";
	
	/* api callbacks */
	ot->exec = edbm_select_linked_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "limit", 0, "Limit by Seams", "");
}

/* ******************** **************** */

static int edbm_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);

	EDBM_select_more(em);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "MESH_OT_select_more";
	ot->description = "Select more vertices, edges or faces connected to initial selection";

	/* api callbacks */
	ot->exec = edbm_select_more_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int edbm_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);

	EDBM_select_less(em);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "MESH_OT_select_less";
	ot->description = "Deselect vertices, edges or faces at the boundary of each selection region";

	/* api callbacks */
	ot->exec = edbm_select_less_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Walk all reachable elements of the same type as h_act in breadth-first
 * order, starting from h_act. Deselects elements if the depth when they
 * are reached is not a multiple of "nth". */
static void walker_deselect_nth(BMEditMesh *em, int nth, int offset, BMHeader *h_act)
{
	BMElem *ele;
	BMesh *bm = em->bm;
	BMWalker walker;
	BMIter iter;
	int walktype = 0, itertype = 0, flushtype = 0;
	short mask_vert = 0, mask_edge = 0, mask_face = 0;

	/* No active element from which to start - nothing to do */
	if (h_act == NULL) {
		return;
	}

	/* Determine which type of iter, walker, and select flush to use
	 * based on type of the elements being deselected */
	switch (h_act->htype) {
		case BM_VERT:
			itertype = BM_VERTS_OF_MESH;
			walktype = BMW_CONNECTED_VERTEX;
			flushtype = SCE_SELECT_VERTEX;
			mask_vert = BM_ELEM_SELECT;
			break;
		case BM_EDGE:
			itertype = BM_EDGES_OF_MESH;
			walktype = BMW_SHELL;
			flushtype = SCE_SELECT_EDGE;
			mask_edge = BM_ELEM_SELECT;
			break;
		case BM_FACE:
			itertype = BM_FACES_OF_MESH;
			walktype = BMW_ISLAND;
			flushtype = SCE_SELECT_FACE;
			mask_face = BM_ELEM_SELECT;
			break;
	}

	/* grr, shouldn't need to alloc BMO flags here */
	BM_mesh_elem_toolflags_ensure(bm);

	/* Walker restrictions uses BMO flags, not header flags,
	 * so transfer BM_ELEM_SELECT from HFlags onto a BMO flag layer. */
	BMO_push(bm, NULL);
	BM_ITER_MESH (ele, &iter, bm, itertype) {
		if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
			/* BMESH_TODO, don't use 'BM_ELEM_SELECT' here, its a HFLAG only! */
			BMO_elem_flag_enable(bm, (BMElemF *)ele, BM_ELEM_SELECT);
		}
	}

	/* Walk over selected elements starting at active */
	BMW_init(&walker, bm, walktype,
	         mask_vert, mask_edge, mask_face,
	         BMW_FLAG_NOP, /* don't use BMW_FLAG_TEST_HIDDEN here since we want to desel all */
	         BMW_NIL_LAY);

	/* use tag to avoid touching the same verts twice */
	BM_ITER_MESH (ele, &iter, bm, itertype) {
		BM_elem_flag_disable(ele, BM_ELEM_TAG);
	}

	BLI_assert(walker.order == BMW_BREADTH_FIRST);
	for (ele = BMW_begin(&walker, h_act); ele != NULL; ele = BMW_step(&walker)) {
		if (!BM_elem_flag_test(ele, BM_ELEM_TAG)) {
			/* Deselect elements that aren't at "nth" depth from active */
			if ((offset + BMW_current_depth(&walker)) % nth) {
				BM_elem_select_set(bm, ele, FALSE);
			}
			BM_elem_flag_enable(ele, BM_ELEM_TAG);
		}
	}
	BMW_end(&walker);

	BMO_pop(bm);

	/* Flush selection up */
	EDBM_selectmode_flush_ex(em, flushtype);
}

static void deselect_nth_active(BMEditMesh *em, BMVert **r_eve, BMEdge **r_eed, BMFace **r_efa)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	BMIter iter;
	BMEditSelection *ese;

	*r_eve = NULL;
	*r_eed = NULL;
	*r_efa = NULL;

	EDBM_selectmode_flush(em);
	ese = (BMEditSelection *)em->bm->selected.last;

	if (ese) {
		switch (ese->htype) {
			case BM_VERT:
				*r_eve = (BMVert *)ese->ele;
				return;
			case BM_EDGE:
				*r_eed = (BMEdge *)ese->ele;
				return;
			case BM_FACE:
				*r_efa = (BMFace *)ese->ele;
				return;
		}
	}

	if (em->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				*r_eve = v;
				return;
			}
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
				*r_eed = e;
				return;
			}
		}
	}
	else if (em->selectmode & SCE_SELECT_FACE) {
		f = BM_active_face_get(em->bm, TRUE, FALSE);
		if (f) {
			*r_efa = f;
			return;
		}
	}
}

static int edbm_deselect_nth(BMEditMesh *em, int nth, int offset)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	deselect_nth_active(em, &v, &e, &f);

	if (v) {
		walker_deselect_nth(em, nth, offset, &v->head);
		return 1;
	}
	else if (e) {
		walker_deselect_nth(em, nth, offset, &e->head);
		return 1;
	}
	else if (f) {
		walker_deselect_nth(em, nth, offset, &f->head);
		return 1;
	}

	return 0;
}

static int edbm_select_nth_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	int nth = RNA_int_get(op->ptr, "nth");
	int offset = RNA_int_get(op->ptr, "offset");

	offset = MIN2(nth, offset);

	if (edbm_deselect_nth(em, nth, offset) == 0) {
		BKE_report(op->reports, RPT_ERROR, "Mesh has no active vert/edge/face");
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(C, em, FALSE);

	return OPERATOR_FINISHED;
}


void MESH_OT_select_nth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Checker Deselect";
	ot->idname = "MESH_OT_select_nth";
	ot->description = "Deselect every Nth element starting from a selected vertex, edge or face";

	/* api callbacks */
	ot->exec = edbm_select_nth_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "nth", 2, 2, INT_MAX, "Nth Selection", "", 2, 100);
	RNA_def_int(ot->srna, "offset", 0, 0, INT_MAX, "Offset", "", 0, 100);
}

void em_setup_viewcontext(bContext *C, ViewContext *vc)
{
	view3d_set_viewcontext(C, vc);
	
	if (vc->obedit) {
		vc->em = BMEdit_FromObject(vc->obedit);
	}
}

/* poll call for mesh operators requiring a view3d context */
int EM_view3d_poll(bContext *C)
{
	if (ED_operator_editmesh(C) && ED_operator_view3d_active(C))
		return 1;

	return 0;
}


static int edbm_select_sharp_edges_exec(bContext *C, wmOperator *op)
{
	/* Find edges that have exactly two neighboring faces,
	 * check the angle between those faces, and if angle is
	 * small enough, select the edge
	 */
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMIter iter;
	BMEdge *e;
	BMLoop *l1, *l2;
	const float sharp = RNA_float_get(op->ptr, "sharpness");

	BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_HIDDEN) == FALSE &&
		    BM_edge_loop_pair(e, &l1, &l2))
		{
			/* edge has exactly two neighboring faces, check angle */
			const float angle = angle_normalized_v3v3(l1->f->no, l2->f->no);

			if (fabsf(angle) > sharp) {
				BM_edge_select_set(em->bm, e, TRUE);
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_edges_select_sharp(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Sharp Edges";
	ot->description = "Select all sharp-enough edges";
	ot->idname = "MESH_OT_edges_select_sharp";
	
	/* api callbacks */
	ot->exec = edbm_select_sharp_edges_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	prop = RNA_def_float_rotation(ot->srna, "sharpness", 0, NULL, DEG2RADF(0.01f), DEG2RADF(180.0f),
	                              "Sharpness", "", DEG2RADF(1.0f), DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(30.0f));
}

static int edbm_select_linked_flat_faces_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMIter iter, liter, liter2;
	BMFace *f, **stack = NULL;
	BLI_array_declare(stack);
	BMLoop *l, *l2;
	float sharp = RNA_float_get(op->ptr, "sharpness");
	int i;

	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		BM_elem_flag_disable(f, BM_ELEM_TAG);
	}

	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_HIDDEN) || !BM_elem_flag_test(f, BM_ELEM_SELECT) || BM_elem_flag_test(f, BM_ELEM_TAG))
			continue;

		BLI_array_empty(stack);
		i = 1;

		BLI_array_grow_one(stack);
		stack[i - 1] = f;

		while (i) {
			f = stack[i - 1];
			i--;

			BM_face_select_set(em->bm, f, TRUE);

			BM_elem_flag_enable(f, BM_ELEM_TAG);

			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				BM_ITER_ELEM (l2, &liter2, l, BM_LOOPS_OF_LOOP) {
					float angle;

					if (BM_elem_flag_test(l2->f, BM_ELEM_TAG) || BM_elem_flag_test(l2->f, BM_ELEM_HIDDEN))
						continue;

					/* edge has exactly two neighboring faces, check angle */
					angle = angle_normalized_v3v3(f->no, l2->f->no);

					/* invalidate: edge too sharp */
					if (angle < sharp) {
						BLI_array_grow_one(stack);
						stack[i] = l2->f;
						i++;
					}
				}
			}
		}
	}

	BLI_array_free(stack);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_faces_select_linked_flat(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Linked Flat Faces";
	ot->description = "Select linked faces by angle";
	ot->idname = "MESH_OT_faces_select_linked_flat";
	
	/* api callbacks */
	ot->exec = edbm_select_linked_flat_faces_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	prop = RNA_def_float_rotation(ot->srna, "sharpness", 0, NULL, DEG2RADF(0.01f), DEG2RADF(180.0f),
	                              "Sharpness", "", DEG2RADF(1.0f), DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(1.0f));
}

static int edbm_select_non_manifold_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMVert *v;
	BMEdge *e;
	BMIter iter;

	/* Selects isolated verts, and edges that do not have 2 neighboring
	 * faces
	 */
	
	if (em->selectmode == SCE_SELECT_FACE) {
		BKE_report(op->reports, RPT_ERROR, "Does not work in face selection mode");
		return OPERATOR_CANCELLED;
	}
	
	BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN) && !BM_vert_is_manifold(v)) {
			BM_vert_select_set(em->bm, v, TRUE);
		}
	}
	
	BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN) && !BM_edge_is_manifold(e)) {
			BM_edge_select_set(em->bm, e, TRUE);
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_non_manifold(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Non Manifold";
	ot->description = "Select all non-manifold vertices or edges";
	ot->idname = "MESH_OT_select_non_manifold";
	
	/* api callbacks */
	ot->exec = edbm_select_non_manifold_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int edbm_select_random_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;
	float randfac =  RNA_float_get(op->ptr, "percent") / 100.0f;

	BLI_srand(BLI_rand()); /* random seed */
	
	if (!RNA_boolean_get(op->ptr, "extend"))
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	if (em->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BLI_frand() < randfac) {
				BM_vert_select_set(em->bm, eve, TRUE);
			}
		}
		EDBM_selectmode_flush(em);
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && BLI_frand() < randfac) {
				BM_edge_select_set(em->bm, eed, TRUE);
			}
		}
		EDBM_selectmode_flush(em);
	}
	else {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && BLI_frand() < randfac) {
				BM_face_select_set(em->bm, efa, TRUE);
			}
		}
		EDBM_selectmode_flush(em);
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Random";
	ot->description = "Randomly select vertices";
	ot->idname = "MESH_OT_select_random";

	/* api callbacks */
	ot->exec = edbm_select_random_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f,
	                         "Percent", "Percentage of elements to select randomly", 0.f, 100.0f);
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend Selection", "Extend selection instead of deselecting everything first");
}

static int edbm_select_next_loop_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *f;
	BMVert *v;
	BMIter iter;
	
	BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_disable(v, BM_ELEM_TAG);
	}
	
	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		BMLoop *l;
		BMIter liter;
		
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			if (BM_elem_flag_test(l->v, BM_ELEM_SELECT)) {
				BM_elem_flag_enable(l->next->v, BM_ELEM_TAG);
				BM_vert_select_set(em->bm, l->v, FALSE);
			}
		}
	}

	BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
			BM_vert_select_set(em->bm, v, TRUE);
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_next_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Next Loop";
	ot->idname = "MESH_OT_select_next_loop";
	ot->description = "Select next edge loop adjacent to a selected loop";

	/* api callbacks */
	ot->exec = edbm_select_next_loop_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int edbm_region_to_loop_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMFace *f;
	BMEdge *e;
	BMIter iter;

	BM_mesh_elem_hflag_disable_all(em->bm, BM_EDGE, BM_ELEM_TAG, FALSE);

	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		BMLoop *l1, *l2;
		BMIter liter1, liter2;
		
		BM_ITER_ELEM (l1, &liter1, f, BM_LOOPS_OF_FACE) {
			int tot = 0, totsel = 0;
			
			BM_ITER_ELEM (l2, &liter2, l1->e, BM_LOOPS_OF_EDGE) {
				tot++;
				totsel += BM_elem_flag_test(l2->f, BM_ELEM_SELECT) != 0;
			}
			
			if ((tot != totsel && totsel > 0) || (totsel == 1 && tot == 1))
				BM_elem_flag_enable(l1->e, BM_ELEM_TAG);
		}
	}

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	
	BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			BM_edge_select_set(em->bm, e, TRUE);
		}
	}

	/* If in face-only select mode, switch to edge select mode so that
	 * an edge-only selection is not inconsistent state */
	if (em->selectmode == SCE_SELECT_FACE) {
		em->selectmode = SCE_SELECT_EDGE;
		EDBM_selectmode_set(em);
		EDBM_selectmode_to_scene(C);
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_region_to_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Boundary Loop";
	ot->idname = "MESH_OT_region_to_loop";
	ot->description = "Select boundary edges around the selected faces";

	/* api callbacks */
	ot->exec = edbm_region_to_loop_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int loop_find_region(BMLoop *l, int flag,
                            SmallHash *fhash, BMFace ***region_out)
{
	BLI_array_declare(region);
	BLI_array_declare(stack);
	BMFace **region = NULL;
	BMFace **stack = NULL;
	BMFace *f;
	
	BLI_array_append(stack, l->f);
	BLI_smallhash_insert(fhash, (uintptr_t)l->f, NULL);
	
	while (BLI_array_count(stack) > 0) {
		BMIter liter1, liter2;
		BMLoop *l1, *l2;
		
		f = BLI_array_pop(stack);
		BLI_array_append(region, f);
		
		BM_ITER_ELEM (l1, &liter1, f, BM_LOOPS_OF_FACE) {
			if (BM_elem_flag_test(l1->e, flag))
				continue;
			
			BM_ITER_ELEM (l2, &liter2, l1->e, BM_LOOPS_OF_EDGE) {
				if (BLI_smallhash_haskey(fhash, (uintptr_t)l2->f))
					continue;
				
				BLI_array_append(stack, l2->f);
				BLI_smallhash_insert(fhash, (uintptr_t)l2->f, NULL);
			}
		}
	}
	
	BLI_array_free(stack);
	
	*region_out = region;
	return BLI_array_count(region);
}

static int verg_radial(const void *va, const void *vb)
{
	BMEdge *e1 = *((void **)va);
	BMEdge *e2 = *((void **)vb);
	int a, b;
	
	a = BM_edge_face_count(e1);
	b = BM_edge_face_count(e2);
	
	if (a > b)  return -1;
	if (a == b) return  0;
	if (a < b)  return  1;
	
	return -1;
}

static int loop_find_regions(BMEditMesh *em, int selbigger)
{
	SmallHash visithash;
	BMIter iter;
	BMEdge *e, **edges = NULL;
	BLI_array_declare(edges);
	BMFace *f;
	int count = 0, i;
	
	BLI_smallhash_init(&visithash);
	
	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		BM_elem_flag_disable(f, BM_ELEM_TAG);
	}

	BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			BLI_array_append(edges, e);
			BM_elem_flag_enable(e, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_disable(e, BM_ELEM_TAG);
		}
	}
	
	/* sort edges by radial cycle length */
	qsort(edges,  BLI_array_count(edges), sizeof(void *), verg_radial);
	
	for (i = 0; i < BLI_array_count(edges); i++) {
		BMIter liter;
		BMLoop *l;
		BMFace **region = NULL, **region_out;
		int c, tot = 0;
		
		e = edges[i];
		
		if (!BM_elem_flag_test(e, BM_ELEM_TAG))
			continue;
		
		BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
			if (BLI_smallhash_haskey(&visithash, (uintptr_t)l->f))
				continue;
						
			c = loop_find_region(l, BM_ELEM_SELECT, &visithash, &region_out);

			if (!region || (selbigger ? c >= tot : c < tot)) {
				/* this region is the best seen so far */
				tot = c;
				if (region) {
					/* free the previous best */
					MEM_freeN(region);
				}
				/* track the current region as the new best */
				region = region_out;
			}
			else {
				/* this region is not as good as best so far, just free it */
				MEM_freeN(region_out);
			}
		}
		
		if (region) {
			int j;
			
			for (j = 0; j < tot; j++) {
				BM_elem_flag_enable(region[j], BM_ELEM_TAG);
				BM_ITER_ELEM (l, &liter, region[j], BM_LOOPS_OF_FACE) {
					BM_elem_flag_disable(l->e, BM_ELEM_TAG);
				}
			}
			
			count += tot;
			
			MEM_freeN(region);
		}
	}
	
	BLI_array_free(edges);
	BLI_smallhash_release(&visithash);
	
	return count;
}

static int edbm_loop_to_region_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMIter iter;
	BMFace *f;
	int selbigger = RNA_boolean_get(op->ptr, "select_bigger");
	int a, b;

	/* find the set of regions with smallest number of total faces */
	a = loop_find_regions(em, selbigger);
	b = loop_find_regions(em, !selbigger);
	
	if ((a <= b) ^ selbigger) {
		loop_find_regions(em, selbigger);
	}
	
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	
	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_TAG) && !BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
			BM_face_select_set(em->bm, f, TRUE);
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;
}

void MESH_OT_loop_to_region(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Loop Inner-Region";
	ot->idname = "MESH_OT_loop_to_region";
	ot->description = "Select region of faces inside of a selected loop of edges";

	/* api callbacks */
	ot->exec = edbm_loop_to_region_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "select_bigger", 0, "Select Bigger", "Select bigger regions instead of smaller ones");
}
