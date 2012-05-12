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
 * The Original Code is Copyright (C) 2010 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_bvh.c
 *  \ingroup edmesh
 */

#define IN_EDITMESHBVH


#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"


#include "BLI_math.h"
#include "BLI_smallhash.h"

#include "BKE_DerivedMesh.h"
#include "BKE_tessmesh.h"

#include "ED_mesh.h"
#include "ED_view3d.h"


typedef struct BMBVHTree {
	BMEditMesh *em;
	BMesh *bm;
	BVHTree *tree;
	float epsilon;
	float maxdist; //for nearest point search
	float uv[2];
	
	/* stuff for topological vert search */
	BMVert *v, *curv;
	GHash *gh;
	float curw, curd;
	float co[3], (*cagecos)[3], (*cos)[3];
	int curtag, flag;
	
	Object *ob;
	Scene *scene;
} BMBVHTree;

static void cage_mapped_verts_callback(void *userData, int index, const float co[3],
                                       const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	void **data = userData;
	BMEditMesh *em = data[0];
	float (*cagecos)[3] = data[1];
	SmallHash *hash = data[2];
	
	if (index >= 0 && index < em->bm->totvert && !BLI_smallhash_haskey(hash, index)) {
		BLI_smallhash_insert(hash, index, NULL);
		copy_v3_v3(cagecos[index], co);
	}
}

BMBVHTree *BMBVH_NewBVH(BMEditMesh *em, int flag, Scene *scene, Object *obedit)
{
	BMBVHTree *tree = MEM_callocN(sizeof(*tree), "BMBVHTree");
	DerivedMesh *cage, *final;
	SmallHash shash;
	float cos[3][3], (*cagecos)[3] = NULL;
	int i;
	int tottri;

	/* when initializing cage verts, we only want the first cage coordinate for each vertex,
	 * so that e.g. mirror or array use original vertex coordinates and not mirrored or duplicate */
	BLI_smallhash_init(&shash);
	
	BMEdit_RecalcTessellation(em);

	tree->ob = obedit;
	tree->scene = scene;
	tree->em = em;
	tree->bm = em->bm;
	tree->epsilon = FLT_EPSILON * 2.0f;
	tree->flag = flag;

	if (flag & (BMBVH_RESPECT_SELECT)) {
		tottri = 0;
		for (i = 0; i < em->tottri; i++) {
			if (BM_elem_flag_test(em->looptris[i][0]->f, BM_ELEM_SELECT)) {
				tottri++;
			}
		}
	}
	else if (flag & (BMBVH_RESPECT_HIDDEN)) {
		tottri = 0;
		for (i = 0; i < em->tottri; i++) {
			if (!BM_elem_flag_test(em->looptris[i][0]->f, BM_ELEM_HIDDEN)) {
				tottri++;
			}
		}
	}
	else {
		tottri = em->tottri;
	}

	tree->tree = BLI_bvhtree_new(tottri, tree->epsilon, 8, 8);
	
	if (flag & BMBVH_USE_CAGE) {
		BMIter iter;
		BMVert *v;
		void *data[3];
		
		tree->cos = MEM_callocN(sizeof(float) * 3 * em->bm->totvert, "bmbvh cos");
		BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, i) {
			BM_elem_index_set(v, i); /* set_inline */
			copy_v3_v3(tree->cos[i], v->co);
		}
		em->bm->elem_index_dirty &= ~BM_VERT;


		cage = editbmesh_get_derived_cage_and_final(scene, obedit, em, &final, CD_MASK_DERIVEDMESH);
		cagecos = MEM_callocN(sizeof(float) * 3 * em->bm->totvert, "bmbvh cagecos");
		
		data[0] = em;
		data[1] = cagecos;
		data[2] = &shash;
		
		cage->foreachMappedVert(cage, cage_mapped_verts_callback, data);
	}
	
	tree->cagecos = cagecos;
	
	for (i = 0; i < em->tottri; i++) {


		if (flag & BMBVH_RESPECT_SELECT) {
			/* note, the arrays wont allign now! take care */
			if (!BM_elem_flag_test(em->looptris[i][0]->f, BM_ELEM_SELECT)) {
				continue;
			}
		}
		else if (flag & BMBVH_RESPECT_HIDDEN) {
			/* note, the arrays wont allign now! take care */
			if (BM_elem_flag_test(em->looptris[i][0]->f, BM_ELEM_HIDDEN)) {
				continue;
			}
		}

		if (flag & BMBVH_USE_CAGE) {
			copy_v3_v3(cos[0], cagecos[BM_elem_index_get(em->looptris[i][0]->v)]);
			copy_v3_v3(cos[1], cagecos[BM_elem_index_get(em->looptris[i][1]->v)]);
			copy_v3_v3(cos[2], cagecos[BM_elem_index_get(em->looptris[i][2]->v)]);
		}
		else {
			copy_v3_v3(cos[0], em->looptris[i][0]->v->co);
			copy_v3_v3(cos[1], em->looptris[i][1]->v->co);
			copy_v3_v3(cos[2], em->looptris[i][2]->v->co);
		}

		BLI_bvhtree_insert(tree->tree, i, (float *)cos, 3);
	}
	
	BLI_bvhtree_balance(tree->tree);
	BLI_smallhash_release(&shash);
	
	return tree;
}

void BMBVH_FreeBVH(BMBVHTree *tree)
{
	BLI_bvhtree_free(tree->tree);
	
	if (tree->cagecos)
		MEM_freeN(tree->cagecos);
	if (tree->cos)
		MEM_freeN(tree->cos);
	
	MEM_freeN(tree);
}

/* taken from bvhutils.c */
static float ray_tri_intersection(const BVHTreeRay *ray, const float UNUSED(m_dist),
                                  const float v0[3],  const float v1[3], const float v2[3],
                                  float r_uv[2], float UNUSED(e))
{
	float dist;

	if (isect_ray_tri_v3((float *)ray->origin, (float *)ray->direction, v0, v1, v2, &dist, r_uv)) {
		return dist;
	}

	return FLT_MAX;
}

static void raycallback(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	BMBVHTree *tree = userdata;
	BMLoop **ls = tree->em->looptris[index];
	float dist, uv[2];
	 
	if (!ls[0] || !ls[1] || !ls[2])
		return;
	
	dist = ray_tri_intersection(ray, hit->dist, ls[0]->v->co, ls[1]->v->co,
	                            ls[2]->v->co, uv, tree->epsilon);
	if (dist < hit->dist) {
		hit->dist = dist;
		hit->index = index;
		
		copy_v3_v3(hit->no, ls[0]->v->no);

		copy_v3_v3(hit->co, ray->direction);
		normalize_v3(hit->co);
		mul_v3_fl(hit->co, dist);
		add_v3_v3(hit->co, ray->origin);
		
		copy_v2_v2(tree->uv, uv);
	}
}

BMFace *BMBVH_RayCast(BMBVHTree *tree, const float co[3], const float dir[3],
                      float r_hitout[3], float r_cagehit[3])
{
	BVHTreeRayHit hit;

	hit.dist = FLT_MAX;
	hit.index = -1;
	
	tree->uv[0] = tree->uv[1] = 0.0f;
	
	BLI_bvhtree_ray_cast(tree->tree, co, dir, 0.0f, &hit, raycallback, tree);
	if (hit.dist != FLT_MAX && hit.index != -1) {
		if (r_hitout) {
			if (tree->flag & BMBVH_RETURN_ORIG) {
				BMVert *v1, *v2, *v3;
				int i;
				
				v1 = tree->em->looptris[hit.index][0]->v;
				v2 = tree->em->looptris[hit.index][1]->v;
				v3 = tree->em->looptris[hit.index][2]->v;
				
				for (i = 0; i < 3; i++) {
					r_hitout[i] = v1->co[i] + ((v2->co[i] - v1->co[i]) * tree->uv[0]) +
					                          ((v3->co[i] - v1->co[i]) * tree->uv[1]);
				}
			}
			else {
				copy_v3_v3(r_hitout, hit.co);
			}

			if (r_cagehit) {
				copy_v3_v3(r_cagehit, hit.co);
			}
		}

		return tree->em->looptris[hit.index][0]->f;
	}

	return NULL;
}

BVHTree *BMBVH_BVHTree(BMBVHTree *tree)
{
	return tree->tree;
}

static void vertsearchcallback(void *userdata, int index, const float *UNUSED(co), BVHTreeNearest *hit)
{
	BMBVHTree *tree = userdata;
	BMLoop **ls = tree->em->looptris[index];
	float dist, maxdist, v[3];
	int i;

	maxdist = tree->maxdist;

	for (i = 0; i < 3; i++) {
		sub_v3_v3v3(v, hit->co, ls[i]->v->co);

		dist = len_v3(v);
		if (dist < hit->dist && dist < maxdist) {
			copy_v3_v3(hit->co, ls[i]->v->co);
			copy_v3_v3(hit->no, ls[i]->v->no);
			hit->dist = dist;
			hit->index = index;
		}
	}
}

BMVert *BMBVH_FindClosestVert(BMBVHTree *tree, float *co, float maxdist)
{
	BVHTreeNearest hit;

	copy_v3_v3(hit.co, co);
	hit.dist = maxdist * 5;
	hit.index = -1;

	tree->maxdist = maxdist;

	BLI_bvhtree_find_nearest(tree->tree, co, &hit, vertsearchcallback, tree);
	if (hit.dist != FLT_MAX && hit.index != -1) {
		BMLoop **ls = tree->em->looptris[hit.index];
		float dist, curdist = tree->maxdist, v[3];
		int cur = 0, i;

		maxdist = tree->maxdist;

		for (i = 0; i < 3; i++) {
			sub_v3_v3v3(v, hit.co, ls[i]->v->co);

			dist = len_v3(v);
			if (dist < curdist) {
				cur = i;
				curdist = dist;
			}
		}

		return ls[cur]->v;
	}

	return NULL;
}

/* UNUSED */
#if 0
static short winding(const float v1[3], const float v2[3], const float v3[3])
/* is v3 to the right of (v1 - v2) ? With exception: v3 == v1 || v3 == v2 */
{
	double inp;

	//inp = (v2[cox] - v1[cox]) * (v1[coy] - v3[coy]) + (v1[coy] - v2[coy]) * (v1[cox] - v3[cox]);
	inp = (v2[0] - v1[0]) * (v1[1] - v3[1]) + (v1[1] - v2[1]) * (v1[0] - v3[0]);

	if (inp < 0.0) {
		return 0;
	}
	else if (inp == 0) {
		if (v1[0] == v3[0] && v1[1] == v3[1]) return 0;
		if (v2[0] == v3[0] && v2[1] == v3[1]) return 0;
	}
	return 1;
}
#endif

#if 0 //BMESH_TODO: not implemented yet
int BMBVH_VertVisible(BMBVHTree *tree, BMEdge *e, RegionView3D *r3d)
{

}
#endif

static BMFace *edge_ray_cast(BMBVHTree *tree, float *co, float *dir, float *hitout, BMEdge *e)
{
	BMFace *f = BMBVH_RayCast(tree, co, dir, hitout, NULL);
	
	if (f && BM_edge_in_face(f, e))
		return NULL;

	return f;
}

static void scale_point(float *c1, float *p, float s)
{
	sub_v3_v3(c1, p);
	mul_v3_fl(c1, s);
	add_v3_v3(c1, p);
}


int BMBVH_EdgeVisible(BMBVHTree *tree, BMEdge *e, ARegion *ar, View3D *v3d, Object *obedit)
{
	BMFace *f;
	float co1[3], co2[3], co3[3], dir1[4], dir2[4], dir3[4];
	float origin[3], invmat[4][4];
	float epsilon = 0.01f; 
	float mval_f[2], end[3];
	
	if (!ar) {
		printf("error in BMBVH_EdgeVisible!\n");
		return 0;
	}
	
	mval_f[0] = ar->winx / 2.0f;
	mval_f[1] = ar->winy / 2.0f;
	ED_view3d_win_to_segment_clip(ar, v3d, mval_f, origin, end);
	
	invert_m4_m4(invmat, obedit->obmat);
	mul_m4_v3(invmat, origin);

	copy_v3_v3(co1, e->v1->co);
	add_v3_v3v3(co2, e->v1->co, e->v2->co);
	mul_v3_fl(co2, 0.5f);
	copy_v3_v3(co3, e->v2->co);
	
	scale_point(co1, co2, 0.99);
	scale_point(co3, co2, 0.99);
	
	/* ok, idea is to generate rays going from the camera origin to the 
	 * three points on the edge (v1, mid, v2)*/
	sub_v3_v3v3(dir1, origin, co1);
	sub_v3_v3v3(dir2, origin, co2);
	sub_v3_v3v3(dir3, origin, co3);
	
	normalize_v3(dir1);
	normalize_v3(dir2);
	normalize_v3(dir3);

	mul_v3_fl(dir1, epsilon);
	mul_v3_fl(dir2, epsilon);
	mul_v3_fl(dir3, epsilon);
	
	/* offset coordinates slightly along view vectors, to avoid
	 * hitting the faces that own the edge.*/
	add_v3_v3v3(co1, co1, dir1);
	add_v3_v3v3(co2, co2, dir2);
	add_v3_v3v3(co3, co3, dir3);

	normalize_v3(dir1);
	normalize_v3(dir2);
	normalize_v3(dir3);

	/* do three samplings: left, middle, right */
	f = edge_ray_cast(tree, co1, dir1, NULL, e);
	if (f && !edge_ray_cast(tree, co2, dir2, NULL, e))
		return 1;
	else if (f && !edge_ray_cast(tree, co3, dir3, NULL, e))
		return 1;
	else if (!f)
		return 1;

	return 0;
}
