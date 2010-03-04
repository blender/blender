 /* $Id:
 *
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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_key_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_types.h"
#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"
#include "BLI_array.h"
#include "BLI_kdopbvh.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "UI_interface.h"

#include "mesh_intern.h"
#include "bmesh.h"

#define IN_EDITMESHBVH
#include "editbmesh_bvh.h"

typedef struct BMBVHTree {
	BMEditMesh *em;
	BMesh *bm;
	BVHTree *tree;
	float epsilon;
	float maxdist; //for nearest point search

	/*stuff for topological vert search*/
	BMVert *v, *curv;
	GHash *gh;
	float curw, curd;
	float co[3];
	int curtag;
} BMBVHTree;

BMBVHTree *BMBVH_NewBVH(BMEditMesh *em)
{
	BMBVHTree *tree = MEM_callocN(sizeof(*tree), "BMBVHTree");
	float cos[3][3];
	int i;

	BMEdit_RecalcTesselation(em);

	tree->em = em;
	tree->bm = em->bm;
	tree->epsilon = FLT_EPSILON*2.0f;

	tree->tree = BLI_bvhtree_new(em->tottri, tree->epsilon, 8, 8);

	for (i=0; i<em->tottri; i++) {
		VECCOPY(cos[0], em->looptris[i][0]->v->co);
		VECCOPY(cos[1], em->looptris[i][1]->v->co);
		VECCOPY(cos[2], em->looptris[i][2]->v->co);

		BLI_bvhtree_insert(tree->tree, i, (float*)cos, 3);
	}
	
	BLI_bvhtree_balance(tree->tree);
	
	return tree;
}

void BMBVH_FreeBVH(BMBVHTree *tree)
{
	BLI_bvhtree_free(tree->tree);
	MEM_freeN(tree);
}

/*taken from bvhutils.c*/
static float ray_tri_intersection(const BVHTreeRay *ray, const float m_dist, float *v0, 
				  float *v1, float *v2, float *uv, float e)
{
	float dist;
#if 0
	float vv1[3], vv2[3], vv3[3], cent[3];

	/*expand triangle by an epsilon.  this is probably a really stupid
	  way of doing it, but I'm too tired to do better work.*/
	VECCOPY(vv1, v0);
	VECCOPY(vv2, v1);
	VECCOPY(vv3, v2);

	add_v3_v3v3(cent, vv1, vv2);
	add_v3_v3v3(cent, cent, vv3);
	mul_v3_fl(cent, 1.0f/3.0f);

	sub_v3_v3v3(vv1, vv1, cent);
	sub_v3_v3v3(vv2, vv2, cent);
	sub_v3_v3v3(vv3, vv3, cent);

	mul_v3_fl(vv1, 1.0f + e);
	mul_v3_fl(vv2, 1.0f + e);
	mul_v3_fl(vv3, 1.0f + e);

	add_v3_v3v3(vv1, vv1, cent);
	add_v3_v3v3(vv2, vv2, cent);
	add_v3_v3v3(vv3, vv3, cent);

	if(isect_ray_tri_v3((float*)ray->origin, (float*)ray->direction, vv1, vv2, vv3, &dist, uv))
		return dist;
#else
	if(isect_ray_tri_v3((float*)ray->origin, (float*)ray->direction, v0, v1, v2, &dist, uv))
		return dist;
#endif

	return FLT_MAX;
}

static void raycallback(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	BMBVHTree *tree = userdata;
	BMLoop **ls = tree->em->looptris[index];
	float dist, uv[2], co1[3], co2[3], co3[3];

	dist = ray_tri_intersection(ray, hit->dist, ls[0]->v->co, ls[1]->v->co,
	                            ls[2]->v->co, uv, tree->epsilon);
	if (dist < hit->dist) {
		hit->dist = dist;
		hit->index = index;
		
		VECCOPY(hit->no, ls[0]->v->no);

		VECCOPY(co1, ls[0]->v->co);
		VECCOPY(co2, ls[1]->v->co);
		VECCOPY(co3, ls[2]->v->co);

		mul_v3_fl(co1, uv[0]);
		mul_v3_fl(co2, uv[1]);
		mul_v3_fl(co3, 1.0f-uv[0]-uv[1]);

		add_v3_v3v3(hit->co, co1, co2);
		add_v3_v3v3(hit->co, hit->co, co3);
	}
}

BMFace *BMBVH_RayCast(BMBVHTree *tree, float *co, float *dir, float *hitout)
{
	BVHTreeRayHit hit;

	hit.dist = FLT_MAX;
	hit.index = -1;

	BLI_bvhtree_ray_cast(tree->tree, co, dir, FLT_MAX, &hit, raycallback, tree);
	if (hit.dist != FLT_MAX && hit.index != -1) {
		if (hitout) {
			VECCOPY(hitout, hit.co);
		}

		return tree->em->looptris[hit.index][0]->f;
	}

	return NULL;
}


static void vertsearchcallback(void *userdata, int index, const float *co, BVHTreeNearest *hit)
{
	BMBVHTree *tree = userdata;
	BMLoop **ls = tree->em->looptris[index];
	float dist, maxdist, v[3];
	int i;

	maxdist = tree->maxdist;

	for (i=0; i<3; i++) {
		sub_v3_v3v3(v, hit->co, ls[i]->v->co);

		dist = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
		if (dist < hit->dist && dist < maxdist) {
			VECCOPY(hit->co, ls[i]->v->co);
			VECCOPY(hit->no, ls[i]->v->no);
			hit->dist = dist;
		}
	}
}

BMVert *BMBVH_FindClosestVert(BMBVHTree *tree, float *co, float maxdist)
{
	BVHTreeNearest hit;

	VECCOPY(hit.co, co);
	hit.dist = maxdist*5;
	hit.index = -1;

	tree->maxdist = maxdist;

	BLI_bvhtree_find_nearest(tree->tree, co, &hit, vertsearchcallback, tree);
	if (hit.dist != FLT_MAX && hit.index != -1) {
		BMLoop **ls = tree->em->looptris[hit.index];
		float dist, curdist = tree->maxdist, v[3];
		int cur=0, i;

		maxdist = tree->maxdist;

		for (i=0; i<3; i++) {
			sub_v3_v3v3(v, hit.co, ls[i]->v->co);

			dist = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
			if (dist < curdist) {
				cur = i;
				curdist = dist;
			}
		}

		return ls[i]->v;
	}

	return NULL;
}

typedef struct walklist {
	BMVert *v;
	int valence;
	int depth;
	float w, r;
	int totwalked;

	/*state data*/
	BMVert *lastv;
	BMLoop *curl, *firstl;
	BMEdge *cure;
} walklist;


static short winding(float *v1, float *v2, float *v3)
/* is v3 to the right of v1-v2 ? With exception: v3==v1 || v3==v2 */
{
	double inp;

	//inp= (v2[cox]-v1[cox])*(v1[coy]-v3[coy]) +(v1[coy]-v2[coy])*(v1[cox]-v3[cox]);
	inp= (v2[0]-v1[0])*(v1[1]-v3[1]) +(v1[1]-v2[1])*(v1[0]-v3[0]);

	if(inp<0.0) return 0;
	else if(inp==0) {
		if(v1[0]==v3[0] && v1[1]==v3[1]) return 0;
		if(v2[0]==v3[0] && v2[1]==v3[1]) return 0;
	}
	return 1;
}

static float topo_compare(BMesh *bm, BMVert *v1, BMVert *v2, int tag)
{
	BMIter iter1, iter2;
	BMEdge *e1, *e2, *cure1 = NULL, *cure2 = NULL;
	BMLoop *l1, *l2;
	BMVert *lastv1, *lastv2;
	GHash *gh;
	walklist *stack1=NULL, *stack2=NULL;
	BLI_array_declare(stack1);
	BLI_array_declare(stack2);
	float vec1[3], vec2[3], minangle=FLT_MAX, w;
	int lvl=1;
	static int maxlevel = 8;

	/*ok.  see how similar v is to v2, based on topological similaritys in the local
	  topological neighborhood*/

	/*step 1: find two edges, one that contains v and one that contains v2, with the
	  smallest angle between the two edges*/

	BM_ITER(e1, &iter1, bm, BM_EDGES_OF_VERT, v1) {
		BM_ITER(e2, &iter2, bm, BM_EDGES_OF_VERT, v2) {
			float angle;
			
			if (e1->v1 == e2->v1 || e1->v2 == e2->v2 || e1->v1 == e2->v2 || e1->v2 == e2->v1)
				continue;

			sub_v3_v3v3(vec1, BM_OtherEdgeVert(e1, v1)->co, v1->co);
			sub_v3_v3v3(vec2, BM_OtherEdgeVert(e2, v2)->co, v2->co);

			angle = fabs(angle_v3v3(vec1, vec2));

			if (angle < minangle) {
				minangle = angle;
				cure1 = e1;
				cure2 = e2;
			}
		}
	}

	if (!cure1 || !cure1->loop || !cure2->loop) {
		/*just return 1.0 in this case*/
		return 1.0f;
	}

	/*assumtions
	
	  we assume a 2-manifold mesh here.  if at any time this isn't the case,
	  e.g. a hole or an edge with more then 2 faces around it, we um ignore
	  that edge I guess, and try to make the algorithm go around as necassary.*/

	l1 = cure1->loop;
	l2 = cure2->loop;

	lastv1 = l1->v == v1 ? ((BMLoop*)l1->head.next)->v : ((BMLoop*)l1->head.prev)->v;
	lastv2 = l2->v == v2 ? ((BMLoop*)l2->head.next)->v : ((BMLoop*)l2->head.prev)->v;

	/*we can only provide meaningful comparisons if v1 and v2 have the same valence*/
	if (BM_Vert_EdgeCount(v1) != BM_Vert_EdgeCount(v2))
		return 1.0f; /*full mismatch*/

	gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

#define SPUSH(s, d, vt, lv, e)\
	if (BLI_array_count(s) <= lvl) BLI_array_growone(s);\
	memset((s+lvl), 0, sizeof(*s));\
	s[lvl].depth = d;\
	s[lvl].v = vt;\
	s[lvl].cure = e;\
	s[lvl].lastv = lv;\
	s[lvl].valence = BM_Vert_EdgeCount(vt);\

	lvl = 0;

	SPUSH(stack1, 0, v1, lastv1, cure1);
	SPUSH(stack2, 0, v2, lastv2, cure2);

	BLI_srand( BLI_rand() ); /* random seed */

	lvl = 1;
	while (lvl) {
		int term = 0;
		walklist *s1 = stack1 + lvl - 1, *s2 = stack2 + lvl - 1;

		/*pop from the stack*/
		lvl--;

		if (s1->curl && s1->curl->e == s1->cure)
			term = 1;
		if (s2->curl && s2->curl->e == s2->cure)
			term = 1;

		/*find next case to do*/
		if (!s1->curl)
			s1->curl = s1->cure->loop;
		if (!s2->curl) {
			int wind1, wind2;
			
			s2->curl = s2->cure->loop;

			/*find which of two possible faces to use*/
			wind1 = winding(s1->v->co, s1->lastv->co, 
				s1->v == s1->curl->v ? ((BMLoop*)s1->curl->head.prev->prev)->v->co : ((BMLoop*)s1->curl->head.next->next)->v->co);

			wind2 = winding(s2->v->co, s2->lastv->co, 
				s2->v == s2->curl->v ? ((BMLoop*)s2->curl->head.prev->prev)->v->co : ((BMLoop*)s2->curl->head.next->next)->v->co);
			
			if (wind1 == wind2)
				s2->curl = s2->curl->radial.next->data;
		}

		/*handle termination cases of having already looped through all child
		  nodes, or the valence mismatching between v1 and v2, or we hit max
		  recursion depth*/
		term |= s1->valence != s2->valence || lvl+1 > maxlevel;
		term |= s1->curl->radial.next->data == (BMLoop*)l1;
		term |= s2->curl->radial.next->data == (BMLoop*)l2;

		if (!term) {
			lastv1 = s1->v;
			lastv2 = s2->v;
			v1 = BM_OtherEdgeVert(s1->curl->e, lastv1);
			v2 = BM_OtherEdgeVert(s2->curl->e, lastv2);
			
			e1 = s1->curl->e;
			e2 = s2->curl->e;

			if (!BLI_ghash_haskey(gh, v1) && !BLI_ghash_haskey(gh, v2)) {
				/*repush the current stack item*/
				lvl++;
				
				if (maxlevel % 2 == 0) {
					BLI_ghash_insert(gh, v1, NULL);
					BLI_ghash_insert(gh, v2, NULL);
				}

				/*now push the child node*/
				SPUSH(stack1, lvl, v1, lastv1, e1);
				SPUSH(stack2, lvl, v2, lastv2, e2);

				lvl++;

				s1 = stack1 + lvl - 2;
				s2 = stack2 + lvl - 2;
			}

			s1->curl = s1->curl->v == s1->v ? (BMLoop*) s1->curl->head.prev : (BMLoop*) s1->curl->head.next;
			s2->curl = s2->curl->v == s2->v ? (BMLoop*) s2->curl->head.prev : (BMLoop*) s2->curl->head.next;
		
			s1->curl = (BMLoop*) s1->curl->radial.next->data;
			s2->curl = (BMLoop*) s2->curl->radial.next->data;
		}

#define WADD(stack, s)\
		if (lvl) {/*silly attempt to make this non-commutative: randomize\
			      how much this particular weight adds to the total*/\
			stack[lvl-1].r += r;\
			s->w *= r;\
			stack[lvl-1].totwalked++;\
			stack[lvl-1].w += s->w;\
		}

		/*if no next case to do, update parent weight*/
		if (term) {
			float r = 0.8f + BLI_frand()*0.2f - FLT_EPSILON;

			if (s1->totwalked) {
				s1->w /= s1->r;
			} else
				s1->w = s1->valence == s2->valence ? 1.0f : 0.0f;

			WADD(stack1, s1);

			if (s2->totwalked) {
				s2->w /= s2->r;
			} else
				s2->w = s1->valence == s2->valence ? 1.0f : 0.0f;
			
			WADD(stack2, s2);

			/*apply additional penalty to weight mismatch*/
			if (s2->w != s1->w)
				s2->w *= 0.8f;
		}
	}

	w = (stack1[0].w + stack2[0].w)*0.5f;

	BLI_array_free(stack1);
	BLI_array_free(stack2);

	BLI_ghash_free(gh, NULL, NULL);

	return 1.0f - w;
}

static void vertsearchcallback_topo(void *userdata, int index, const float *co, BVHTreeNearest *hit)
{
	BMBVHTree *tree = userdata;
	BMLoop **ls = tree->em->looptris[index];
	int i;
	float dist, maxdist, vec[3], w;

	maxdist = tree->maxdist;

	for (i=0; i<3; i++) {
		float dis;

		if (BLI_ghash_haskey(tree->gh, ls[i]->v))
			continue;

		sub_v3_v3v3(vec, tree->co, ls[i]->v->co);
		dis = dot_v3v3(vec, vec);

		w = topo_compare(tree->em->bm, tree->v, ls[i]->v, tree->curtag++);
		
		if (w < tree->curw-FLT_EPSILON*4) {
			tree->curw = w;
			tree->curv = ls[i]->v;
		   
			sub_v3_v3v3(vec, tree->co, ls[i]->v->co);
			tree->curd = dot_v3v3(vec, vec);

			/*we deliberately check for equality using (smallest possible float)*4 
			  comparison factor, to always prefer distance in cases of verts really
			  close to each other*/
		} else if (fabs(tree->curw - w) < FLT_EPSILON*4) {
			/*if w is equal to hitex->curw, sort by distance*/
			sub_v3_v3v3(vec, tree->co, ls[i]->v->co);
			dis = dot_v3v3(vec, vec);

			if (dis < tree->curd) {
				tree->curd = dis;
				tree->curv = ls[i]->v;
			}
		}

		BLI_ghash_insert(tree->gh, ls[i]->v, NULL);
	}
}

BMVert *BMBVH_FindClosestVertTopo(BMBVHTree *tree, float *co, float maxdist, BMVert *sourcev)
{
	BVHTreeNearest hit;
	BMVert *v;
	BMIter iter;

	memset(&hit, 0, sizeof(hit));

	VECCOPY(hit.co, co);
	VECCOPY(tree->co, co);
	hit.index = -1;
	hit.dist = 10.0f;

	tree->curw = FLT_MAX;
	tree->curd = FLT_MAX;
	tree->curv = NULL;
	tree->curtag = 1;

	BM_ITER(v, &iter, tree->bm, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(v, 0);
	}

	tree->gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

	tree->maxdist = maxdist;
	tree->v = sourcev;

	BLI_bvhtree_find_nearest(tree->tree, co, &hit, vertsearchcallback_topo, tree);
	
	BLI_ghash_free(tree->gh, NULL, NULL);
	tree->gh = NULL;

	return tree->curv;
}


#if 0 //BMESH_TODO: not implemented yet
int BMBVH_VertVisible(BMBVHTree *tree, BMEdge *e, RegionView3D *r3d)
{

}
#endif

static BMFace *edge_ray_cast(BMBVHTree *tree, float *co, float *dir, float *hitout, BMEdge *e)
{
	BMFace *f = BMBVH_RayCast(tree, co, dir, hitout);
	
	if (f && BM_Edge_In_Face(f, e))
		return NULL;

	return f;
}

int BMBVH_EdgeVisible(BMBVHTree *tree, BMEdge *e, RegionView3D *r3d, Object *obedit)
{
	BMFace *f;
	float co1[3], co2[3], co3[3], dir1[4], dir2[4], dir3[4];
	float origin[3], invmat[4][4];
	float epsilon = 0.01f; 
	
	VECCOPY(origin, r3d->viewinv[3]);
	invert_m4_m4(invmat, obedit->obmat);
	mul_m4_v3(invmat, origin);

	VECCOPY(co1, e->v1->co);
	add_v3_v3v3(co2, e->v1->co, e->v2->co);
	mul_v3_fl(co2, 0.5f);
	VECCOPY(co3, e->v2->co);
	
	/*ok, idea is to generate rays going from the camera origin to the 
	  three points on the edge (v1, mid, v2)*/
	sub_v3_v3v3(dir1, origin, co1);
	sub_v3_v3v3(dir2, origin, co2);
	sub_v3_v3v3(dir3, origin, co3);
	
	normalize_v3(dir1);
	normalize_v3(dir2);
	normalize_v3(dir3);

	mul_v3_fl(dir1, epsilon);
	mul_v3_fl(dir2, epsilon);
	mul_v3_fl(dir3, epsilon);
	
	/*offset coordinates slightly along view vectors, to avoid
	  hitting the faces that own the edge.*/
	add_v3_v3v3(co1, co1, dir1);
	add_v3_v3v3(co2, co2, dir2);
	add_v3_v3v3(co3, co3, dir3);

	normalize_v3(dir1);
	normalize_v3(dir2);
	normalize_v3(dir3);

	/*do three samplings: left, middle, right*/
	f = edge_ray_cast(tree, co1, dir1, NULL, e);
	if (f && !edge_ray_cast(tree, co2, dir2, NULL, e))
		return 1;
	else if (f && !edge_ray_cast(tree, co3, dir3, NULL, e))
		return 1;
	else if (!f)
		return 1;

	return 0;
}
