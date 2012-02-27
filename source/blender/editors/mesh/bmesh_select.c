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
#include "ED_view3d.h"

#include "BIF_gl.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "mesh_intern.h"


/* ****************************** MIRROR **************** */

void EDBM_select_mirrored(Object *UNUSED(obedit), BMEditMesh *em, int extend)
{
	BMVert *v1, *v2;
	BMIter iter;

	BM_ITER(v1, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (!BM_elem_flag_test(v1, BM_ELEM_SELECT) || BM_elem_flag_test(v1, BM_ELEM_HIDDEN)) {
			BM_elem_flag_disable(v1, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_enable(v1, BM_ELEM_TAG);
		}
	}

	EDBM_CacheMirrorVerts(em, TRUE);

	if (!extend)
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BM_ITER(v1, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (!BM_elem_flag_test(v1, BM_ELEM_TAG) || BM_elem_flag_test(v1, BM_ELEM_HIDDEN))
			continue;

		v2 = EDBM_GetMirrorVert(em, v1);
		if (v2 && !BM_elem_flag_test(v2, BM_ELEM_HIDDEN)) {
			BM_elem_select_set(em->bm, v2, TRUE);
		}
	}

	EDBM_EndMirrorCache(em);
}

void EDBM_automerge(Scene *scene, Object *obedit, int update)
{
	BMEditMesh *em;
	
	if ((scene->toolsettings->automerge) &&
	    (obedit && obedit->type == OB_MESH))
	{
		em = ((Mesh *)obedit->data)->edit_btmesh;
		if (!em)
			return;

		BMO_op_callf(em->bm, "automerge verts=%hv dist=%f", BM_ELEM_SELECT, scene->toolsettings->doublimit);
		if (update) {
			DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
		}
	}
}

/* ****************************** SELECTION ROUTINES **************** */

unsigned int bm_solidoffs = 0, bm_wireoffs = 0, bm_vertoffs = 0;	/* set in drawobject.c ... for colorindices */

/* facilities for border select and circle select */
static char *selbuf = NULL;

/* opengl doesn't support concave... */
static void draw_triangulated(int mcords[][2], short tot)
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
	filldisplist(&lb, &lb, 0);

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
	
	freedisplist(&lb);
}


/* reads rect, and builds selection array for quick lookup */
/* returns if all is OK */
int EDBM_init_backbuf_border(ViewContext *vc, short xmin, short ymin, short xmax, short ymax)
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
	
	a = (xmax - xmin + 1) * (ymax-ymin + 1);
	while (a--) {
		if (*dr > 0 && *dr <= bm_vertoffs)
			selbuf[*dr] = 1;
		dr++;
	}
	IMB_freeImBuf(buf);
	return 1;
}

int EDBM_check_backbuf(unsigned int index)
{
	if (selbuf == NULL) return 1;
	if (index > 0 && index <= bm_vertoffs)
		return selbuf[index];
	return 0;
}

void EDBM_free_backbuf(void)
{
	if (selbuf) MEM_freeN(selbuf);
	selbuf = NULL;
}

/* mcords is a polygon mask
   - grab backbuffer,
   - draw with black in backbuffer, 
   - grab again and compare
   returns 'OK' 
*/
int EDBM_mask_init_backbuf_border(ViewContext *vc, int mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax)
{
	unsigned int *dr, *drm;
	struct ImBuf *buf, *bufmask;
	int a;
	
	/* method in use for face selecting too */
	if (vc->obedit == NULL) {
		if (paint_facesel_test(vc->obact));
		else if (paint_vertsel_test(vc->obact));
		else return 0;
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
	
	glFinish();	/* to be sure readpixels sees mask */
	
	/* grab mask */
	bufmask = view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	drm = bufmask->rect;
	if (bufmask == NULL) {
		return 0; /* only when mem alloc fails, go crash somewhere else! */
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
int EDBM_init_backbuf_circle(ViewContext *vc, short xs, short ys, short rads)
{
	struct ImBuf *buf;
	unsigned int *dr;
	short xmin, ymin, xmax, ymax, xc, yc;
	int radsq;
	
	/* method in use for face selecting too */
	if (vc->obedit == NULL) {
		if (paint_facesel_test(vc->obact));
		else if (paint_vertsel_test(vc->obact));
		else return 0;
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

static void findnearestvert__doClosest(void *userData, BMVert *eve, int x, int y, int index)
{
	struct { short mval[2], pass, select, strict; int dist, lastIndex, closestIndex; BMVert *closest; } *data = userData;

	if (data->pass == 0) {
		if (index <= data->lastIndex)
			return;
	}
	else {
		if (index > data->lastIndex)
			return;
	}

	if (data->dist > 3) {
		int temp = abs(data->mval[0] - x) + abs(data->mval[1]- y);
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT) == data->select) {
			if (data->strict == 1)
				return;
			else
				temp += 5;
		}

		if (temp < data->dist) {
			data->dist = temp;
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
 * 		if SELECT, selected vertice are given a 5 pixel bias to make them farter than unselect verts
 * 		if 0, unselected vertice are given the bias
 * strict: if 1, the vertice corresponding to the sel parameter are ignored and not just biased 
 */
BMVert *EDBM_findnearestvert(ViewContext *vc, int *dist, short sel, short strict)
{
	if (vc->v3d->drawtype > OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		int distance;
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
		
		if (eve && distance < *dist) {
			*dist = distance;
			return eve;
		}
		else {
			return NULL;
		}
			
	}
	else {
		struct { short mval[2], pass, select, strict; int dist, lastIndex, closestIndex; BMVert *closest; } data;
		static int lastSelectedIndex = 0;
		static BMVert *lastSelected = NULL;
		
		if (lastSelected && BM_vert_at_index(vc->em->bm, lastSelectedIndex) != lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval[0] = vc->mval[0];
		data.mval[1] = vc->mval[1];
		data.select = sel;
		data.dist = *dist;
		data.strict = strict;
		data.closest = NULL;
		data.closestIndex = 0;

		data.pass = 0;

		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

		mesh_foreachScreenVert(vc, findnearestvert__doClosest, &data, V3D_CLIP_TEST_RV3D_CLIPPING);

		if (data.dist > 3) {
			data.pass = 1;
			mesh_foreachScreenVert(vc, findnearestvert__doClosest, &data, V3D_CLIP_TEST_RV3D_CLIPPING);
		}

		*dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
}

/* returns labda for closest distance v1 to line-piece v2 - v3 */
float labda_PdistVL2Dfl(const float v1[3], const float v2[3], const float v3[3])
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
static void findnearestedge__doClosest(void *userData, BMEdge *eed, int x0, int y0, int x1, int y1, int UNUSED(index))
{
	struct { ViewContext vc; float mval[2]; int dist; BMEdge *closest; } *data = userData;
	float v1[2], v2[2];
	int distance;
		
	v1[0] = x0;
	v1[1] = y0;
	v2[0] = x1;
	v2[1] = y1;
		
	distance = dist_to_line_segment_v2(data->mval, v1, v2);
		
	if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
		distance += 5;
	}

	if (distance < data->dist) {
		if (data->vc.rv3d->rflag & RV3D_CLIPPING) {
			float labda = labda_PdistVL2Dfl(data->mval, v1, v2);
			float vec[3];

			vec[0] = eed->v1->co[0] + labda * (eed->v2->co[0] - eed->v1->co[0]);
			vec[1] = eed->v1->co[1] + labda * (eed->v2->co[1] - eed->v1->co[1]);
			vec[2] = eed->v1->co[2] + labda * (eed->v2->co[2] - eed->v1->co[2]);
			mul_m4_v3(data->vc.obedit->obmat, vec);

			if (ED_view3d_test_clipping(data->vc.rv3d, vec, 1) == 0) {
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
BMEdge *EDBM_findnearestedge(ViewContext *vc, int *dist)
{

	if (vc->v3d->drawtype > OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		int distance;
		unsigned int index;
		BMEdge *eed;
		
		view3d_validate_backbuf(vc);
		
		index = view3d_sample_backbuf_rect(vc, vc->mval, 50, bm_solidoffs, bm_wireoffs, &distance,0, NULL, NULL);
		eed = BM_edge_at_index(vc->em->bm, index - 1);
		
		if (eed && distance < *dist) {
			*dist = distance;
			return eed;
		}
		else {
			return NULL;
		}
	}
	else {
		struct { ViewContext vc; float mval[2]; int dist; BMEdge *closest; } data;

		data.vc = *vc;
		data.mval[0] = vc->mval[0];
		data.mval[1] = vc->mval[1];
		data.dist = *dist;
		data.closest = NULL;
		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

		mesh_foreachScreenEdge(vc, findnearestedge__doClosest, &data, 2);

		*dist = data.dist;
		return data.closest;
	}
}

static void findnearestface__getDistance(void *userData, BMFace *efa, int x, int y, int UNUSED(index))
{
	struct { short mval[2]; int dist; BMFace *toFace; } *data = userData;

	if (efa == data->toFace) {
		int temp = abs(data->mval[0] - x) + abs(data->mval[1] - y);

		if (temp < data->dist)
			data->dist = temp;
	}
}
static void findnearestface__doClosest(void *userData, BMFace *efa, int x, int y, int index)
{
	struct { short mval[2], pass; int dist, lastIndex, closestIndex; BMFace *closest; } *data = userData;

	if (data->pass == 0) {
		if (index <= data->lastIndex)
			return;
	}
	else {
		if (index > data->lastIndex)
			return;
	}

	if (data->dist > 3) {
		int temp = abs(data->mval[0] - x) + abs(data->mval[1] - y);

		if (temp < data->dist) {
			data->dist = temp;
			data->closest = efa;
			data->closestIndex = index;
		}
	}
}

BMFace *EDBM_findnearestface(ViewContext *vc, int *dist)
{

	if (vc->v3d->drawtype > OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		unsigned int index;
		BMFace *efa;

		view3d_validate_backbuf(vc);

		index = view3d_sample_backbuf(vc, vc->mval[0], vc->mval[1]);
		efa = BM_face_at_index(vc->em->bm, index - 1);
		
		if (efa) {
			struct { short mval[2]; int dist; BMFace *toFace; } data;

			data.mval[0] = vc->mval[0];
			data.mval[1] = vc->mval[1];
			data.dist = 0x7FFF;		/* largest short */
			data.toFace = efa;

			mesh_foreachScreenFace(vc, findnearestface__getDistance, &data);

			if (vc->em->selectmode == SCE_SELECT_FACE || data.dist < *dist) {	/* only faces, no dist check */
				*dist = data.dist;
				return efa;
			}
		}
		
		return NULL;
	}
	else {
		struct { short mval[2], pass; int dist, lastIndex, closestIndex; BMFace *closest; } data;
		static int lastSelectedIndex = 0;
		static BMFace *lastSelected = NULL;

		if (lastSelected && BM_face_at_index(vc->em->bm, lastSelectedIndex) != lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval[0] = vc->mval[0];
		data.mval[1] = vc->mval[1];
		data.dist = *dist;
		data.closest = NULL;
		data.closestIndex = 0;
		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

		data.pass = 0;
		mesh_foreachScreenFace(vc, findnearestface__doClosest, &data);

		if (data.dist > 3) {
			data.pass = 1;
			ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
			mesh_foreachScreenFace(vc, findnearestface__doClosest, &data);
		}

		*dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
}

/* best distance based on screen coords. 
   use em->selectmode to define how to use 
   selected vertices and edges get disadvantage
   return 1 if found one
*/
static int unified_findnearest(ViewContext *vc, BMVert **eve, BMEdge **eed, BMFace **efa) 
{
	BMEditMesh *em = vc->em;
	int dist = 75;
	
	*eve = NULL;
	*eed = NULL;
	*efa = NULL;
	
	/* no afterqueue (yet), so we check it now, otherwise the em_xxxofs indices are bad */
	view3d_validate_backbuf(vc);
	
	if (em->selectmode & SCE_SELECT_VERTEX)
		*eve = EDBM_findnearestvert(vc, &dist, BM_ELEM_SELECT, 0);
	if (em->selectmode & SCE_SELECT_FACE)
		*efa = EDBM_findnearestface(vc, &dist);

	dist-= 20;	/* since edges select lines, we give dots advantage of 20 pix */
	if (em->selectmode & SCE_SELECT_EDGE)
		*eed = EDBM_findnearestedge(vc, &dist);

	/* return only one of 3 pointers, for frontbuffer redraws */
	if (*eed) {
		*efa = NULL; *eve = NULL;
	}
	else if (*efa) {
		*eve = NULL;
	}
	
	return (*eve || *eed || *efa);
}

/* ****************  SIMILAR "group" SELECTS. FACE, EDGE AND VERTEX ************** */

static EnumPropertyItem prop_similar_types[] = {
	{SIMVERT_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMVERT_FACE, "FACE", 0, "Amount of Adjacent Faces", ""},
	{SIMVERT_VGROUP, "VGROUP", 0, "Vertex Groups", ""},

	{SIMEDGE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDGE_DIR, "DIR", 0, "Direction", ""},
	{SIMEDGE_FACE, "FACE", 0, "Amount of Faces Around an Edge", ""},
	{SIMEDGE_FACE_ANGLE, "FACE_ANGLE", 0, "Face Angles", ""},
	{SIMEDGE_CREASE, "CREASE", 0, "Crease", ""},
	{SIMEDGE_SEAM, "SEAM", 0, "Seam", ""},
	{SIMEDGE_SHARP, "SHARP", 0, "Sharpness", ""},

	{SIMFACE_MATERIAL, "MATERIAL", 0, "Material", ""},
	{SIMFACE_IMAGE, "IMAGE", 0, "Image", ""},
	{SIMFACE_AREA, "AREA", 0, "Area", ""},
	{SIMFACE_PERIMETER, "PERIMETER", 0, "Perimeter", ""},
	{SIMFACE_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMFACE_COPLANAR, "COPLANAR", 0, "Co-planar", ""},

	{0, NULL, 0, NULL, NULL}
};

/* selects new faces/edges/verts based on the existing selection */

static int similar_face_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;

	/* get the type from RNA */
	int type = RNA_enum_get(op->ptr, "type");

	float thresh = CTX_data_tool_settings(C)->select_thresh;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "similarfaces faces=%hf type=%i thresh=%f", BM_ELEM_SELECT, type, thresh);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "faceout", BM_ELEM_SELECT, BM_ALL, TRUE);

	/* finish the operator */
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	/* dependencies graph and notification stuff */
	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);

	/* we succeeded */
	return OPERATOR_FINISHED;
}	

/* ***************************************************** */

/* EDGE GROUP */

/* wrap the above function but do selection flushing edge to face */
static int similar_edge_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;

	/* get the type from RNA */
	int type = RNA_enum_get(op->ptr, "type");

	float thresh = CTX_data_tool_settings(C)->select_thresh;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "similaredges edges=%he type=%i thresh=%f", BM_ELEM_SELECT, type, thresh);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "edgeout", BM_ELEM_SELECT, BM_ALL, TRUE);
	EDBM_selectmode_flush(em);

	/* finish the operator */
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	/* dependencies graph and notification stuff */
	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

/* ********************************* */

/*
VERT GROUP
 mode 1: same normal
 mode 2: same number of face users
 mode 3: same vertex groups
*/


static int similar_vert_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;
	/* get the type from RNA */
	int type = RNA_enum_get(op->ptr, "type");
	float thresh = CTX_data_tool_settings(C)->select_thresh;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "similarverts verts=%hv type=%i thresh=%f", BM_ELEM_SELECT, type, thresh);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "vertout", BM_ELEM_SELECT, BM_ALL, TRUE);

	/* finish the operator */
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_selectmode_flush(em);

	/* dependencies graph and notification stuff */
	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

static int select_similar_exec(bContext *C, wmOperator *op)
{
	int type = RNA_enum_get(op->ptr, "type");

	if (type < 100)
		return similar_vert_select_exec(C, op);
	else if (type < 200)
		return similar_edge_select_exec(C, op);
	else
		return similar_face_select_exec(C, op);
}

static EnumPropertyItem *select_similar_type_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop),
                                                   int *free)
{
	Object *obedit = CTX_data_edit_object(C);

	if (obedit && obedit->type == OB_MESH) {
		EnumPropertyItem *item = NULL;
		int a, totitem = 0;
		BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;

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
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = select_similar_exec;
	ot->poll = ED_operator_editmesh;
	ot->description = "Select similar vertices, edges or faces by property types";
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop = ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, SIMVERT_NORMAL, "Type", "");
	RNA_def_enum_funcs(prop, select_similar_type_itemf);
}

/* ***************************************************** */

/* ****************  LOOP SELECTS *************** */

static void walker_select(BMEditMesh *em, int walkercode, void *start, int select)
{
	BMesh *bm = em->bm;
	BMElem *ele;
	BMWalker walker;

	BMW_init(&walker, bm, walkercode,
	         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
	         BMW_NIL_LAY);
	ele = BMW_begin(&walker, start);
	for ( ; ele; ele = BMW_step(&walker)) {
		if (!select) {
			BM_select_history_remove(bm, ele);
		}
		BM_elem_select_set(bm, ele, select);
	}
	BMW_end(&walker);
}

static int loop_multiselect(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMEdge *eed;
	BMEdge **edarray;
	int edindex;
	int looptype = RNA_boolean_get(op->ptr, "ring");
	
	BMIter iter;
	int totedgesel = 0;

	for (eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
	    eed; eed = BM_iter_step(&iter)) {

		if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
			totedgesel++;
		}
	}

	
	edarray = MEM_mallocN(sizeof(BMEdge *)*totedgesel,"edge array");
	edindex = 0;
	
	for (eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
	     eed;
	     eed = BM_iter_step(&iter))
	{

		if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
			edarray[edindex] = eed;
			edindex++;
		}
	}
	
	if (looptype) {
		for (edindex = 0; edindex < totedgesel; edindex += 1) {
			eed = edarray[edindex];
			walker_select(em, BMW_EDGERING, eed, 1);
		}
		EDBM_selectmode_flush(em);
	}
	else{
		for (edindex = 0; edindex < totedgesel; edindex += 1) {
			eed = edarray[edindex];
			walker_select(em, BMW_LOOP, eed, 1);
		}
		EDBM_selectmode_flush(em);
	}
	MEM_freeN(edarray);
//	if (EM_texFaceCheck())
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_loop_multi_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Multi Select Loops";
	ot->idname = "MESH_OT_loop_multi_select";
	
	/* api callbacks */
	ot->exec = loop_multiselect;
	ot->poll = ED_operator_editmesh;
	ot->description = "Select a loop of connected edges by connection type";
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "ring", 0, "Ring", "");
}

		
/* ***************** MAIN MOUSE SELECTION ************** */


/* ***************** loop select (non modal) ************** */

static void mouse_mesh_loop(bContext *C, int mval[2], short extend, short ring)
{
	ViewContext vc;
	BMEditMesh *em;
	BMEdge *eed;
	int select = TRUE;
	int dist = 50;
	
	em_setup_viewcontext(C, &vc);
	vc.mval[0] = mval[0];
	vc.mval[1] = mval[1];
	em = vc.em;
	
	/* no afterqueue (yet), so we check it now, otherwise the bm_xxxofs indices are bad */
	view3d_validate_backbuf(&vc);

	eed = EDBM_findnearestedge(&vc, &dist);
	if (eed) {
		if (extend == 0) {
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		}
	
		if (BM_elem_flag_test(eed, BM_ELEM_SELECT) == 0) {
			select = TRUE;
		}
		else if (extend) {
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
//			if (EM_texFaceCheck())
		
		/* sets as active, useful for other tools */
		if (select) {
			if (em->selectmode & SCE_SELECT_VERTEX) {
				/* TODO: would be nice if the edge vertex chosen here
				 * was the one closer to the selection pointer, instead
				 * of arbitrarily selecting the first one */
				EDBM_store_selection(em, eed->v1);
			}
			else if (em->selectmode & SCE_SELECT_EDGE) {
				EDBM_store_selection(em, eed);
			}
			/* TODO: would be nice if the nearest face that
			 * belongs to the selected edge could be set to
			 * active here in face select mode */
		}

		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit);
	}
}

static int mesh_select_loop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	view3d_operator_needs_opengl(C);
	
	mouse_mesh_loop(C, event->mval, RNA_boolean_get(op->ptr, "extend"),
					RNA_boolean_get(op->ptr, "ring"));
	
	/* cannot do tweaks for as long this keymap is after transform map */
	return OPERATOR_FINISHED;
}

void MESH_OT_loop_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Loop Select";
	ot->idname = "MESH_OT_loop_select";
	ot->description = "Select a loop";
	
	/* api callbacks */
	ot->invoke = mesh_select_loop_invoke;
	ot->poll = ED_operator_editmesh_region_view3d;
	ot->description = "Select a loop of connected edges";
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "Extend the selection");
	RNA_def_boolean(ot->srna, "ring", 0, "Select Ring", "Select ring");
}

void MESH_OT_edgering_select (wmOperatorType *ot)
{
	/* description */
	ot->name = "Edge Ring Select";
	ot->idname = "MESH_OT_edgering_select";
	ot->description = "Select an edge ring";
	
	/* callbacks */
	ot->invoke = mesh_select_loop_invoke;
	ot->poll = ED_operator_editmesh_region_view3d;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
	RNA_def_boolean(ot->srna, "ring", 1, "Select Ring", "Select ring");
}

/* ******************* edgetag_shortest_path and helpers ****************** */

static float edgetag_cut_cost(BMEditMesh *UNUSED(em), BMEdge *e1, BMEdge *e2, BMVert *v)
{
	BMVert *v1 = (e1->v1 == v) ? e1->v2 : e1->v1;
	BMVert *v2 = (e2->v1 == v) ? e2->v2 : e2->v1;
	float cost, d1[3], d2[3];

	/* The cost is based on the simple sum of the length of the two edgees... */
	sub_v3_v3v3(d1, v->co, v1->co);
	sub_v3_v3v3(d2, v2->co, v->co);
	cost = len_v3(d1);
	cost += len_v3(d2);

	/* but is biased to give higher values to sharp turns, so that it will take
	 * paths with fewer "turns" when selecting between equal-weighted paths between
	 * the two edges */
	cost = cost + 0.5f * cost * (2.0f - sqrt(fabs(dot_v3v3(d1, d2))));

	return cost;
}

static void edgetag_add_adjacent(BMEditMesh *em, SmallHash *visithash, Heap *heap, int mednum, int vertnum, 
								 int *nedges, int *edges, int *prevedge, float *cost)
{
	BMEdge *e1 = EDBM_get_edge_for_index(em, mednum);
	BMVert *v = EDBM_get_vert_for_index(em, vertnum);
	int startadj, endadj = nedges[vertnum + 1];

	for (startadj = nedges[vertnum]; startadj < endadj; startadj++) {
		int adjnum = edges[startadj];
		BMEdge *e2 = EDBM_get_edge_for_index(em, adjnum);
		float newcost;
		float cutcost;

		if (BLI_smallhash_haskey(visithash, (uintptr_t)e2))
			continue;

		cutcost = edgetag_cut_cost(em, e1, e2, v);
		newcost = cost[mednum] + cutcost;

		if (cost[adjnum] > newcost) {
			cost[adjnum] = newcost;
			prevedge[adjnum] = mednum;
			BLI_heap_insert(heap, newcost, SET_INT_IN_POINTER(adjnum));
		}
	}
}

static void edgetag_context_set(BMEditMesh *em, Scene *scene, BMEdge *e, int val)
{
	
	switch (scene->toolsettings->edge_mode) {
	case EDGE_MODE_SELECT:
		BM_elem_select_set(em->bm, e, val);
		break;
	case EDGE_MODE_TAG_SEAM:
		BM_elem_flag_set(e, BM_ELEM_SEAM, val);
		break;
	case EDGE_MODE_TAG_SHARP:
		BM_elem_flag_set(e, BM_ELEM_SMOOTH, !val);
		break;
	case EDGE_MODE_TAG_CREASE:
	 {
		float *crease = CustomData_bmesh_get(&em->bm->edata, e->head.data, CD_CREASE);
		
		if (val)		*crease = 1.0f;
		else			*crease = 0.0f;
		break;
	 }
	case EDGE_MODE_TAG_BEVEL:
	 {
		float *bweight = CustomData_bmesh_get(&em->bm->edata, e->head.data, CD_BWEIGHT);

		if (val)		*bweight = 1.0f;
		else			*bweight = 0.0f;
		break;
	 }
	}
}

static int edgetag_context_check(Scene *scene, BMEditMesh *em, BMEdge *e)
{
	switch (scene->toolsettings->edge_mode) {
	case EDGE_MODE_SELECT:
		return BM_elem_flag_test(e, BM_ELEM_SELECT) ? 1 : 0;
	case EDGE_MODE_TAG_SEAM:
		return BM_elem_flag_test(e, BM_ELEM_SEAM);
	case EDGE_MODE_TAG_SHARP:
		return !BM_elem_flag_test(e, BM_ELEM_SMOOTH);
	case EDGE_MODE_TAG_CREASE:	
		return BM_elem_float_data_get(&em->bm->edata, e, CD_CREASE) ? 1 : 0;
	case EDGE_MODE_TAG_BEVEL:
		return BM_elem_float_data_get(&em->bm->edata, e, CD_BWEIGHT) ? 1 : 0;
	}
	return 0;
}

static int edgetag_shortest_path(Scene *scene, BMEditMesh *em, BMEdge *source, BMEdge *target)
{
	BMEdge *e;
	BMIter iter;
	Heap *heap;
	SmallHash visithash;
	float *cost;
	int i, totvert = 0, totedge = 0, *nedges, *edges, *prevedge, mednum = -1, nedgeswap = 0;
	int targetnum;

	BLI_smallhash_init(&visithash);

	/* note, would pass BM_EDGE except we are looping over all edges anyway */
	BM_mesh_elem_index_ensure(em->bm, BM_VERT /* | BM_EDGE */);

	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		e->oflags[0].f = 0; /* XXX, whats this for, BMESH_TODO, double check if this is needed */
		if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
			BLI_smallhash_insert(&visithash, (uintptr_t)e, NULL);
		}

		BM_elem_index_set(e, totedge); /* set_inline */
		totedge++;
	}
	em->bm->elem_index_dirty &= ~BM_EDGE;

	/* alloc */
	totvert = em->bm->totvert;
	nedges = MEM_callocN(sizeof(*nedges) * totvert + 1, "SeamPathNEdges");
	edges = MEM_mallocN(sizeof(*edges) * totedge * 2, "SeamPathEdges");
	prevedge = MEM_mallocN(sizeof(*prevedge) * totedge, "SeamPathPrevious");
	cost = MEM_mallocN(sizeof(*cost) * totedge, "SeamPathCost");

	/* count edges, compute adjacent edges offsets and fill adjacent */
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		nedges[BM_elem_index_get(e->v1) + 1]++;
		nedges[BM_elem_index_get(e->v2) + 1]++;
	}

	for (i = 1; i < totvert; i++) {
		int newswap = nedges[i + 1];
		nedges[i + 1] = nedgeswap + nedges[i];
		nedgeswap = newswap;
	}
	nedges[0] = nedges[1] = 0;

	i = 0;
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		edges[nedges[BM_elem_index_get(e->v1) + 1]++] = i;
		edges[nedges[BM_elem_index_get(e->v2) + 1]++] = i;

		cost[i] = 1e20f;
		prevedge[i] = -1;
		i++;
	}

	/*
	 * Arrays are now filled as follows:
	 *
	 *	nedges[n] = sum of the # of edges incident to all vertices numbered 0 thru n - 1
	 *	edges[edges[n]..edges[n - 1]] = the indices of of the edges incident to vertex n
	 *
	 * As the search continues, prevedge[n] will be the previous edge on the shortest
	 * path found so far to edge n. The visitedhash will of course contain entries
	 * for edges that have been visited, cost[n] will contain the length of the shortest
	 * path to edge n found so far, Finally, heap is a priority heap which is built on the
	 * the same data as the cost arry, but inverted: it is a worklist of edges prioritized
	 * by the shortest path found so far to the edge.
	*/

#if 0 /* UNUSED */ /* this block does nothing, not sure why its here? - campbell */
	for (i = 0; i < totvert; i++) {
		int start = nedges[i], end = nedges[i + 1], cur;
		for (cur = start; cur < end; cur++) {
			BMEdge *e = EDBM_get_edge_for_index(em, edges[cur]);
		}
	}
#endif

	/* regular dijkstra shortest path, but over edges instead of vertices */
	heap = BLI_heap_new();
	BLI_heap_insert(heap, 0.0f, SET_INT_IN_POINTER(BM_elem_index_get(source)));
	cost[BM_elem_index_get(source)] = 0.0f;
	EDBM_init_index_arrays(em, 1, 1, 0);
	targetnum = BM_elem_index_get(target);

	while (!BLI_heap_empty(heap)) {
		mednum = GET_INT_FROM_POINTER(BLI_heap_popmin(heap));
		e = EDBM_get_edge_for_index(em, mednum);

		if (mednum == targetnum)
			break;

		if (BLI_smallhash_haskey(&visithash, (uintptr_t)e))
			continue;

		BLI_smallhash_insert(&visithash, (uintptr_t)e, NULL);

		edgetag_add_adjacent(em, &visithash, heap, mednum, BM_elem_index_get(e->v1), nedges, edges, prevedge, cost);
		edgetag_add_adjacent(em, &visithash, heap, mednum, BM_elem_index_get(e->v2), nedges, edges, prevedge, cost);
	}
	
	if (mednum == targetnum) {
		short allseams = 1;

		/* Check whether the path is already completely tagged.
		 * if it is, the tags will be cleared instead of set. */
		mednum = targetnum;
		do {
			e = EDBM_get_edge_for_index(em, mednum);
			if (!edgetag_context_check(scene, em, e)) {
				allseams = 0;
				break;
			}
			mednum = prevedge[mednum];
		} while (mednum != BM_elem_index_get(source));

		/* Follow path back and source and add or remove tags */
		mednum = targetnum;
		do {
			e = EDBM_get_edge_for_index(em, mednum);
			if (allseams)
				edgetag_context_set(em, scene, e, 0);
			else
				edgetag_context_set(em, scene, e, 1);
			mednum = prevedge[mednum];
		} while (mednum != -1);
	}

	EDBM_free_index_arrays(em);
	MEM_freeN(nedges);
	MEM_freeN(edges);
	MEM_freeN(prevedge);
	MEM_freeN(cost);
	BLI_heap_free(heap, NULL);
	BLI_smallhash_release(&visithash);

	return 1;
}

/* ******************* mesh shortest path select, uses prev-selected edge ****************** */

/* since you want to create paths with multiple selects, it doesn't have extend option */
static void mouse_mesh_shortest_path(bContext *C, int mval[2])
{
	Object *ob = CTX_data_edit_object(C);
	ViewContext vc;
	BMEditMesh *em;
	BMEdge *e;
	int dist = 50;
	
	em_setup_viewcontext(C, &vc);
	vc.mval[0] = mval[0];
	vc.mval[1] = mval[1];
	em = vc.em;
	
	e = EDBM_findnearestedge(&vc, &dist);
	if (e) {
		Mesh *me = vc.obedit->data;
		int path = 0;
		
		if (em->bm->selected.last) {
			BMEditSelection *ese = em->bm->selected.last;
			
			if (ese && ese->htype == BM_EDGE) {
				BMEdge *e_act;
				e_act = (BMEdge *)ese->ele;
				if (e_act != e) {
					if (edgetag_shortest_path(vc.scene, em, e_act, e)) {
						EDBM_remove_selection(em, e_act);
						path = 1;
					}
				}
			}
		}
		if (path == 0) {
			int act = (edgetag_context_check(vc.scene, em, e) == 0);
			edgetag_context_set(em, vc.scene, e, act); /* switch the edge option */
		}
		
		EDBM_selectmode_flush(em);

		/* even if this is selected it may not be in the selection list */
		if (edgetag_context_check(vc.scene, em, e) == 0)
			EDBM_remove_selection(em, e);
		else
			EDBM_store_selection(em, e);
	
		/* force drawmode for mesh */
		switch (CTX_data_tool_settings(C)->edge_mode) {
			
			case EDGE_MODE_TAG_SEAM:
				me->drawflag |= ME_DRAWSEAMS;
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
		
		DAG_id_tag_update(ob->data, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);
	}
}


static int mesh_shortest_path_select_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	
	view3d_operator_needs_opengl(C);

	mouse_mesh_shortest_path(C, event->mval);
	
	return OPERATOR_FINISHED;
}
	
void MESH_OT_select_shortest_path(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shortest Path Select";
	ot->idname = "MESH_OT_select_shortest_path";
	
	/* api callbacks */
	ot->invoke = mesh_shortest_path_select_invoke;
	ot->poll = ED_operator_editmesh;
	ot->description = "Select shortest path between two selections";
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "");
}

/* ************************************************** */
/* here actual select happens */
/* gets called via generic mouse select operator */
int mouse_mesh(bContext *C, const int mval[2], short extend)
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
		
		if (extend == 0) EDBM_flag_disable_all(vc.em, BM_ELEM_SELECT);
		
		if (efa) {
			/* set the last selected face */
			BM_active_face_set(vc.em->bm, efa);
			
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
				EDBM_store_selection(vc.em, efa);
				BM_elem_select_set(vc.em->bm, efa, TRUE);
			}
			else if (extend) {
				EDBM_remove_selection(vc.em, efa);
				BM_elem_select_set(vc.em->bm, efa, FALSE);
			}
		}
		else if (eed) {
			if (!BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
				EDBM_store_selection(vc.em, eed);
				BM_elem_select_set(vc.em->bm, eed, TRUE);
			}
			else if (extend) {
				EDBM_remove_selection(vc.em, eed);
				BM_elem_select_set(vc.em->bm, eed, FALSE);
			}
		}
		else if (eve) {
			if (!BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
				EDBM_store_selection(vc.em, eve);
				BM_elem_select_set(vc.em->bm, eve, TRUE);
			}
			else if (extend) {
				EDBM_remove_selection(vc.em, eve);
				BM_elem_select_set(vc.em->bm, eve, FALSE);
			}
		}
		
		EDBM_selectmode_flush(vc.em);
		  
//		if (EM_texFaceCheck()) {

		if (efa && efa->mat_nr != vc.obedit->actcol - 1) {
			vc.obedit->actcol = efa->mat_nr + 1;
			vc.em->mat_nr = efa->mat_nr;
//			BIF_preview_changed(ID_MA);
		}

		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit);
		return 1;
	}

	return 0;
}

static void EDBM_strip_selections(BMEditMesh *em)
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

/* when switching select mode, makes sure selection is consistant for editing */
/* also for paranoia checks to make sure edge or face mode works */
void EDBM_selectmode_set(BMEditMesh *em)
{
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;
	
	em->bm->selectmode = em->selectmode;

	EDBM_strip_selections(em); /* strip BMEditSelections from em->selected that are not relevant to new mode */
	
	if (em->selectmode & SCE_SELECT_VERTEX) {
		EDBM_select_flush(em);
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		/* deselect vertices, and select again based on edge select */
		eve = BM_iter_new(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
		for ( ; eve; eve = BM_iter_step(&iter)) BM_elem_select_set(em->bm, eve, FALSE);
		
		eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; eed; eed = BM_iter_step(&iter)) {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
				BM_elem_select_set(em->bm, eed, TRUE);
			}
		}
		
		/* selects faces based on edge status */
		EDBM_selectmode_flush(em);
	}
	else if (em->selectmode & SCE_SELECT_FACE) {
		/* deselect eges, and select again based on face select */
		eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; eed; eed = BM_iter_step(&iter)) BM_elem_select_set(em->bm, eed, FALSE);
		
		efa = BM_iter_new(&iter, em->bm, BM_FACES_OF_MESH, NULL);
		for ( ; efa; efa = BM_iter_step(&iter)) {
			if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
				BM_elem_select_set(em->bm, efa, TRUE);
			}
		}
	}
}

void EDBM_convertsel(BMEditMesh *em, short oldmode, short selectmode)
{
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;

	/* have to find out what the selectionmode was previously */
	if (oldmode == SCE_SELECT_VERTEX) {
		if (selectmode == SCE_SELECT_EDGE) {
			/* select all edges associated with every selected vertex */
			eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
			for ( ; eed; eed = BM_iter_step(&iter)) {
				if ( (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) ||
				      BM_elem_flag_test(eed->v2, BM_ELEM_SELECT)))
				{
					BM_elem_select_set(em->bm, eed, TRUE);
				}
			}
		}		
		else if (selectmode == SCE_SELECT_FACE) {
			BMIter liter;
			BMLoop *l;

			/* select all faces associated with every selected vertex */
			efa = BM_iter_new(&iter, em->bm, BM_FACES_OF_MESH, NULL);
			for ( ; efa; efa = BM_iter_step(&iter)) {
				l = BM_iter_new(&liter, em->bm, BM_LOOPS_OF_FACE, efa);
				for (; l; l = BM_iter_step(&liter)) {
					if (BM_elem_flag_test(l->v, BM_ELEM_SELECT)) {
						BM_elem_select_set(em->bm, efa, TRUE);
						break;
					}
				}
			}
		}
	}
	
	if (oldmode == SCE_SELECT_EDGE) {
		if (selectmode == SCE_SELECT_FACE) {
			BMIter liter;
			BMLoop *l;

			/* select all faces associated with every selected vertex */
			efa = BM_iter_new(&iter, em->bm, BM_FACES_OF_MESH, NULL);
			for ( ; efa; efa = BM_iter_step(&iter)) {
				l = BM_iter_new(&liter, em->bm, BM_LOOPS_OF_FACE, efa);
				for (; l; l = BM_iter_step(&liter)) {
					if (BM_elem_flag_test(l->v, BM_ELEM_SELECT)) {
						BM_elem_select_set(em->bm, efa, TRUE);
						break;
					}
				}
			}
		}
	}
}


void EDBM_deselect_by_material(struct BMEditMesh *em, const short index, const short select)
{
	BMIter iter;
	BMFace *efa;

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
			continue;
		if (efa->mat_nr == index) {
			BM_elem_select_set(em->bm, efa, select);
		}
	}
}


void EDBM_select_swap(BMEditMesh *em) /* exported for UV */
{
	BMIter iter;
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	
	if (em->bm->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN))
				continue;
			BM_elem_select_set(em->bm, eve, !BM_elem_flag_test(eve, BM_ELEM_SELECT));
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
			if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			BM_elem_select_set(em->bm, eed, !BM_elem_flag_test(eed, BM_ELEM_SELECT));
		}
	}
	else {
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
				continue;
			BM_elem_select_set(em->bm, efa, !BM_elem_flag_test(efa, BM_ELEM_SELECT));
		}

	}
//	if (EM_texFaceCheck())
}

int EDBM_select_interior_faces(struct BMEditMesh *em)
{
	BMesh *bm = em->bm;
	BMIter iter;
	BMIter eiter;
	BMFace *efa;
	BMEdge *eed;
	int ok;
	int change = FALSE;

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
			continue;


		ok = TRUE;
		BM_ITER(eed, &eiter, bm, BM_EDGES_OF_FACE, efa) {
			if (BM_edge_face_count(eed) < 3) {
				ok = FALSE;
				break;
			}
		}

		if (ok) {
			BM_elem_select_set(bm, efa, TRUE);
			change = TRUE;
		}
	}

	return change;
}

static void linked_limit_default(bContext *C, wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "limit")) {
		Object *obedit = CTX_data_edit_object(C);
		BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
		if (em->selectmode == SCE_SELECT_FACE)
			RNA_boolean_set(op->ptr, "limit", TRUE);
		else
			RNA_boolean_set(op->ptr, "limit", FALSE);
	}
}

static int select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
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
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	
		return OPERATOR_CANCELLED;
	}
	
	if (em->selectmode == SCE_SELECT_FACE) {
		BMIter iter;

		if (efa == NULL)
			return OPERATOR_CANCELLED;

		if (limit) {
			/* hflag no-seam --> bmo-tag */
			BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
				/* BMESH_TODO, don't use 'BM_ELEM_SELECT' here, its a HFLAG only! */
				BMO_elem_flag_set(bm, e, BM_ELEM_SELECT, !BM_elem_flag_test(e, BM_ELEM_SEAM));
			}
		}

		/* walk */
		BMW_init(&walker, bm, BMW_ISLAND,
		         BMW_MASK_NOP, limit ? BM_ELEM_SELECT : BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_NIL_LAY);

		e = BMW_begin(&walker, efa);
		for (; efa; efa = BMW_step(&walker)) {
			BM_elem_select_set(bm, efa, sel);
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
		         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_NIL_LAY);

		e = BMW_begin(&walker, eed->v1);
		for ( ; e; e = BMW_step(&walker)) {
			BM_elem_select_set(bm, e, sel);
		}
		BMW_end(&walker);
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "MESH_OT_select_linked_pick";
	
	/* api callbacks */
	ot->invoke = select_linked_pick_invoke;
	ot->poll = ED_operator_editmesh;
	ot->description = "(De)select all vertices linked to the edge under the mouse cursor";
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
	RNA_def_boolean(ot->srna, "limit", 0, "Limit by Seams", "");
}


static int select_linked_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
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

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			BM_elem_flag_set(efa, BM_ELEM_TAG, (BM_elem_flag_test(efa, BM_ELEM_SELECT) &&
			                                    !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)));
		}

		if (limit) {
			BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
				/* BMESH_TODO, don't use 'BM_ELEM_SELECT' here, its a HFLAG only! */
				BMO_elem_flag_set(bm, e, BM_ELEM_SELECT, !BM_elem_flag_test(e, BM_ELEM_SEAM));
			}
		}

		BMW_init(&walker, bm, BMW_ISLAND,
		         BMW_MASK_NOP, limit ? BM_ELEM_SELECT : BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_NIL_LAY);

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
				e = BMW_begin(&walker, efa);
				for (; efa; efa = BMW_step(&walker)) {
					BM_elem_select_set(bm, efa, TRUE);
				}
			}
		}
		BMW_end(&walker);
	}
	else  {
		BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT) && !BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
				BM_elem_flag_enable(v, BM_ELEM_TAG);
			}
			else {
				BM_elem_flag_disable(v, BM_ELEM_TAG);
			}
		}

		BMW_init(&walker, em->bm, BMW_SHELL,
		         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_NIL_LAY);
		BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
				e = BMW_begin(&walker, v);
				for (; e; e = BMW_step(&walker)) {
					BM_elem_select_set(em->bm, e->v1, TRUE);
					BM_elem_select_set(em->bm, e->v2, TRUE);
				}
			}
		}
		BMW_end(&walker);
	}
	EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked All";
	ot->idname = "MESH_OT_select_linked";
	
	/* api callbacks */
	ot->exec = select_linked_exec;
	ot->poll = ED_operator_editmesh;
	ot->description = "Select all vertices linked to the active mesh";
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "limit", 0, "Limit by Seams", "");
}

/* ******************** **************** */

static int select_more(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;

	EDBM_select_more(em);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "MESH_OT_select_more";
	ot->description = "Select more vertices, edges or faces connected to initial selection";

	/* api callbacks */
	ot->exec = select_more;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int select_less(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;

	EDBM_select_less(em);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "MESH_OT_select_less";
	ot->description = "Deselect vertices, edges or faces at the boundary of each selection region";

	/* api callbacks */
	ot->exec = select_less;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* Walk all reachable elements of the same type as h_act in breadth-first
   order, starting from h_act. Deselects elements if the depth when they
   are reached is not a multiple of "nth". */
static void walker_deselect_nth(BMEditMesh *em, int nth, int offset, BMHeader *h_act)
{
	BMElem *ele;
	BMesh *bm = em->bm;
	BMWalker walker;
	BMIter iter;
	int walktype = 0, itertype = 0, flushtype = 0;
	short mask_vert = 0, mask_edge = 0, mask_loop = 0, mask_face = 0;

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

	/* Walker restrictions uses BMO flags, not header flags,
	 * so transfer BM_ELEM_SELECT from HFlags onto a BMO flag layer. */
	BMO_push(bm, NULL);
	BM_ITER(ele, &iter, bm, itertype, NULL) {
		if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
			/* BMESH_TODO, don't use 'BM_ELEM_SELECT' here, its a HFLAG only! */
			BMO_elem_flag_enable(bm, (BMElemF *)ele, BM_ELEM_SELECT);
		}
	}

	/* Walk over selected elements starting at active */
	BMW_init(&walker, bm, walktype,
	         mask_vert, mask_edge, mask_loop, mask_face,
	         BMW_NIL_LAY);

	BLI_assert(walker.order == BMW_BREADTH_FIRST);
	for (ele = BMW_begin(&walker, h_act); ele != NULL; ele = BMW_step(&walker)) {
		/* Deselect elements that aren't at "nth" depth from active */
		if ((offset + BMW_current_depth(&walker)) % nth) {
			BM_elem_select_set(bm, ele, FALSE);
		}
	}
	BMW_end(&walker);

	BMO_pop(bm);

	/* Flush selection up */
	EDBM_selectmode_flush_ex(em, flushtype);
}

static void deselect_nth_active(BMEditMesh *em, BMVert **v_p, BMEdge **e_p, BMFace **f_p)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	BMIter iter;
	BMEditSelection *ese;

	*v_p = NULL;
	*e_p = NULL;
	*f_p = NULL;

	EDBM_selectmode_flush(em);
	ese = (BMEditSelection *)em->bm->selected.last;

	if (ese) {
		switch(ese->htype) {
		case BM_VERT:
			*v_p = (BMVert *)ese->ele;
			return;
		case BM_EDGE:
			*e_p = (BMEdge *)ese->ele;
			return;
		case BM_FACE:
			*f_p = (BMFace *)ese->ele;
			return;
		}
	}

	if (em->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				*v_p = v;
				return;
			}
		}
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
				*e_p = e;
				return;
			}
		}
	}
	else if (em->selectmode & SCE_SELECT_FACE) {
		f = BM_active_face_get(em->bm, TRUE);
		if (f) {
			*f_p = f;
			return;
		}
	}
}

static int EM_deselect_nth(BMEditMesh *em, int nth, int offset)
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

static int mesh_select_nth_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	int nth = RNA_int_get(op->ptr, "nth");
	int offset = RNA_int_get(op->ptr, "offset");

	offset = MIN2(nth, offset);

	if (EM_deselect_nth(em, nth, offset) == 0) {
		BKE_report(op->reports, RPT_ERROR, "Mesh has no active vert/edge/face");
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}


void MESH_OT_select_nth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Nth";
	ot->description = "";
	ot->idname = "MESH_OT_select_nth";

	/* api callbacks */
	ot->exec = mesh_select_nth_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "nth", 2, 2, 100, "Nth Selection", "", 1, INT_MAX);
	RNA_def_int(ot->srna, "offset", 0, 0, 100, "Offset", "", 0, INT_MAX);
}

void em_setup_viewcontext(bContext *C, ViewContext *vc)
{
	view3d_set_viewcontext(C, vc);
	
	if (vc->obedit) {
		Mesh *me = vc->obedit->data;
		vc->em = me->edit_btmesh;
	}
}

/* poll call for mesh operators requiring a view3d context */
int EM_view3d_poll(bContext *C)
{
	if (ED_operator_editmesh(C) && ED_operator_view3d_active(C))
		return 1;
	return 0;
}


static int select_sharp_edges_exec(bContext *C, wmOperator *op)
{
	/* Find edges that have exactly two neighboring faces,
	* check the angle between those faces, and if angle is
	* small enough, select the edge
	*/
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMIter iter;
	BMEdge *e;
	BMLoop *l1, *l2;
	float sharp = RNA_float_get(op->ptr, "sharpness"), angle;

	sharp = DEG2RADF(sharp);

	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (BM_elem_flag_test(e, BM_ELEM_HIDDEN) || !e->l)
			continue;

		l1 = e->l;
		l2 = l1->radial_next;

		if (l1 == l2)
			continue;

		/* edge has exactly two neighboring faces, check angle */
		angle = angle_normalized_v3v3(l1->f->no, l2->f->no);

		if (fabsf(angle) > sharp) {
			BM_elem_select_set(em->bm, e, TRUE);
		}

	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_edges_select_sharp(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Sharp Edges";
	ot->description = "Marked selected edges as sharp";
	ot->idname = "MESH_OT_edges_select_sharp";
	
	/* api callbacks */
	ot->exec = select_sharp_edges_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "sharpness", 1.0f, 0.01f, FLT_MAX, "sharpness", "", 1.0f, 180.0f);
}

static int select_linked_flat_faces_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMIter iter, liter, liter2;
	BMFace *f, **stack = NULL;
	BLI_array_declare(stack);
	BMLoop *l, *l2;
	float sharp = RNA_float_get(op->ptr, "sharpness");
	int i;

	sharp = (sharp * M_PI) / 180.0;

	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BM_elem_flag_disable(f, BM_ELEM_TAG);
	}

	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (BM_elem_flag_test(f, BM_ELEM_HIDDEN) || !BM_elem_flag_test(f, BM_ELEM_SELECT) || BM_elem_flag_test(f, BM_ELEM_TAG))
			continue;

		BLI_array_empty(stack);
		i = 1;

		BLI_array_growone(stack);
		stack[i - 1] = f;

		while (i) {
			f = stack[i - 1];
			i--;

			BM_elem_select_set(em->bm, f, TRUE);

			BM_elem_flag_enable(f, BM_ELEM_TAG);

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
				BM_ITER(l2, &liter2, em->bm, BM_LOOPS_OF_LOOP, l) {
					float angle;

					if (BM_elem_flag_test(l2->f, BM_ELEM_TAG) || BM_elem_flag_test(l2->f, BM_ELEM_HIDDEN))
						continue;

					/* edge has exactly two neighboring faces, check angle */
					angle = angle_normalized_v3v3(f->no, l2->f->no);

					/* invalidate: edge too sharp */
					if (angle < sharp) {
						BLI_array_growone(stack);
						stack[i] = l2->f;
						i++;
					}
				}
			}
		}
	}

	BLI_array_free(stack);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_faces_select_linked_flat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked Flat Faces";
	ot->description = "Select linked faces by angle";
	ot->idname = "MESH_OT_faces_select_linked_flat";
	
	/* api callbacks */
	ot->exec = select_linked_flat_faces_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "sharpness", 1.0f, 0.01f, FLT_MAX, "sharpness", "", 1.0f, 180.0f);
}

static int select_non_manifold_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMVert *v;
	BMEdge *e;
	BMIter iter;

	/* Selects isolated verts, and edges that do not have 2 neighboring
	 * faces
	 */
	
	if (em->selectmode == SCE_SELECT_FACE) {
		BKE_report(op->reports, RPT_ERROR, "Doesn't work in face selection mode");
		return OPERATOR_CANCELLED;
	}
	
	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN) && !BM_vert_is_manifold(em->bm, v)) {
			BM_elem_select_set(em->bm, v, TRUE);
		}
	}
	
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN) && BM_edge_face_count(e) != 2) {
			BM_elem_select_set(em->bm, e, TRUE);
		}
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_non_manifold(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Non Manifold";
	ot->description = "Select all non-manifold vertices or edges";
	ot->idname = "MESH_OT_select_non_manifold";
	
	/* api callbacks */
	ot->exec = select_non_manifold_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int mesh_select_random_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;
	float randfac =  RNA_float_get(op->ptr, "percent")/100.0f;

	BLI_srand(BLI_rand()); /* random seed */
	
	if (!RNA_boolean_get(op->ptr, "extend"))
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	if (em->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BLI_frand() < randfac) {
				BM_elem_select_set(em->bm, eve, TRUE);
			}
		}
		EDBM_selectmode_flush(em);
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && BLI_frand() < randfac) {
				BM_elem_select_set(em->bm, eed, TRUE);
			}
		}
		EDBM_selectmode_flush(em);
	}
	else {
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && BLI_frand() < randfac) {
				BM_elem_select_set(em->bm, efa, TRUE);
			}
		}
		EDBM_selectmode_flush(em);
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Random";
	ot->description = "Randomly select vertices";
	ot->idname = "MESH_OT_select_random";

	/* api callbacks */
	ot->exec = mesh_select_random_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f,
	                         "Percent", "Percentage of elements to select randomly", 0.f, 100.0f);
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend Selection", "Extend selection instead of deselecting everything first");
}

static int select_next_loop(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;
	BMFace *f;
	BMVert *v;
	BMIter iter;
	
	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		BM_elem_flag_disable(v, BM_ELEM_TAG);
	}
	
	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BMLoop *l;
		BMIter liter;
		
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
			if (BM_elem_flag_test(l->v, BM_ELEM_SELECT) && !BM_elem_flag_test(l->v, BM_ELEM_HIDDEN)) {
				BM_elem_flag_enable(l->next->v, BM_ELEM_TAG);
				BM_elem_select_set(em->bm, l->v, FALSE);
			}
		}
	}

	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
			BM_elem_select_set(em->bm, v, TRUE);
		}
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_next_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Next Loop";
	ot->idname = "MESH_OT_select_next_loop";
	ot->description = "";

	/* api callbacks */
	ot->exec = select_next_loop;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int region_to_loop(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMFace *f;
	BMEdge *e;
	BMIter iter;
	ViewContext vc;
	
	em_setup_viewcontext(C, &vc);
	
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		BM_elem_flag_disable(e, BM_ELEM_TAG);
	}

	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BMLoop *l1, *l2;
		BMIter liter1, liter2;
		
		BM_ITER(l1, &liter1, em->bm, BM_LOOPS_OF_FACE, f) {
			int tot = 0, totsel = 0;
			
			BM_ITER(l2, &liter2, em->bm, BM_LOOPS_OF_EDGE, l1->e) {
				tot++;
				totsel += BM_elem_flag_test(l2->f, BM_ELEM_SELECT) != 0;
			}
			
			if ((tot != totsel && totsel > 0) || (totsel == 1 && tot == 1))
				BM_elem_flag_enable(l1->e, BM_ELEM_TAG);
		}
	}

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG) && !BM_elem_flag_test(e, BM_ELEM_HIDDEN))
			BM_edge_select_set(em->bm, e, TRUE);
	}

	/* If in face-only select mode, switch to edge select mode so that
	   an edge-only selection is not inconsistent state */
	if (em->selectmode == SCE_SELECT_FACE) {
		em->selectmode = SCE_SELECT_EDGE;
		EDBM_selectmode_set(em);
		EDBM_selectmode_to_scene(C);
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_region_to_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Boundary Loop";
	ot->idname = "MESH_OT_region_to_loop";

	/* api callbacks */
	ot->exec = region_to_loop;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int loop_find_region(BMEditMesh *em, BMLoop *l, int flag, 
	SmallHash *fhash, BMFace ***region_out)
{
	BLI_array_declare(region);
	BLI_array_declare(stack);
	BMFace **region = NULL, *f;
	BMFace **stack = NULL;
	
	BLI_array_append(stack, l->f);
	BLI_smallhash_insert(fhash, (uintptr_t)l->f, NULL);
	
	while (BLI_array_count(stack) > 0) {
		BMIter liter1, liter2;
		BMLoop *l1, *l2;
		
		f = BLI_array_pop(stack);
		BLI_array_append(region, f);
		
		BM_ITER(l1, &liter1, em->bm, BM_LOOPS_OF_FACE, f) {
			if (BM_elem_flag_test(l1->e, flag))
				continue;
			
			BM_ITER(l2, &liter2, em->bm, BM_LOOPS_OF_EDGE, l1->e) {
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
	
	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BM_elem_flag_disable(f, BM_ELEM_TAG);
	}

	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
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
		BMFace **region = NULL, **r;
		int c, tot = 0;
		
		e = edges[i];
		
		if (!BM_elem_flag_test(e, BM_ELEM_TAG))
			continue;
		
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_EDGE, e) {
			if (BLI_smallhash_haskey(&visithash, (uintptr_t)l->f))
				continue;
						
			c = loop_find_region(em, l, BM_ELEM_SELECT, &visithash, &r);

			if (!region || (selbigger ? c >= tot : c < tot)) {
				/* this region is the best seen so far */
				tot = c;
				if (region) {
					/* free the previous best */
					MEM_freeN(region);
				}
				/* track the current region as the new best */
				region = r;
			}
			else {
				/* this region is not as good as best so far, just free it */
				MEM_freeN(r);
			}
		}
		
		if (region) {
			int j;
			
			for (j = 0; j < tot; j++) {
				BM_elem_flag_enable(region[j], BM_ELEM_TAG);
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, region[j]) {
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

static int loop_to_region(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
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
	
	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (BM_elem_flag_test(f, BM_ELEM_TAG) && !BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
			BM_face_select_set(em->bm, f, TRUE);
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;
}

void MESH_OT_loop_to_region(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Loop Inner-Region";
	ot->idname = "MESH_OT_loop_to_region";

	/* api callbacks */
	ot->exec = loop_to_region;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "select_bigger", 0, "Select Bigger", "Select bigger regions instead of smaller ones");
}
