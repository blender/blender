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
#include "BLI_arithb.h"
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

	VecAddf(cent, vv1, vv2);
	VecAddf(cent, cent, vv3);
	VecMulf(cent, 1.0f/3.0f);

	VecSubf(vv1, vv1, cent);
	VecSubf(vv2, vv2, cent);
	VecSubf(vv3, vv3, cent);

	VecMulf(vv1, 1.0f + e);
	VecMulf(vv2, 1.0f + e);
	VecMulf(vv3, 1.0f + e);

	VecAddf(vv1, vv1, cent);
	VecAddf(vv2, vv2, cent);
	VecAddf(vv3, vv3, cent);

	if(RayIntersectsTriangle((float*)ray->origin, (float*)ray->direction, vv1, vv2, vv3, &dist, uv))
		return dist;
#else
	if(RayIntersectsTriangle((float*)ray->origin, (float*)ray->direction, v0, v1, v2, &dist, uv))
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

		VecMulf(co1, uv[0]);
		VecMulf(co2, uv[1]);
		VecMulf(co3, 1.0f-uv[0]-uv[1]);

		VecAddf(hit->co, co1, co2);
		VecAddf(hit->co, hit->co, co3);
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
	Mat4Invert(invmat, obedit->obmat);
	Mat4MulVecfl(invmat, origin);

	VECCOPY(co1, e->v1->co);
	VecAddf(co2, e->v1->co, e->v2->co);
	VecMulf(co2, 0.5f);
	VECCOPY(co3, e->v2->co);
	
	/*ok, idea is to generate rays going from the camera origin to the 
	  three points on the edge (v1, mid, v2)*/
	VecSubf(dir1, origin, co1);
	VecSubf(dir2, origin, co2);
	VecSubf(dir3, origin, co3);
	
	Normalize(dir1);
	Normalize(dir2);
	Normalize(dir3);

	VecMulf(dir1, epsilon);
	VecMulf(dir2, epsilon);
	VecMulf(dir3, epsilon);
	
	/*offset coordinates slightly along view vectors, to avoid
	  hitting the faces that own the edge.*/
	VecAddf(co1, co1, dir1);
	VecAddf(co2, co2, dir2);
	VecAddf(co3, co3, dir3);

	Normalize(dir1);
	Normalize(dir2);
	Normalize(dir3);

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
