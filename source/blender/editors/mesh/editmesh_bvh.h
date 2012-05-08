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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_bvh.h
 *  \ingroup edmesh
 */

#ifndef __EDITBMESH_BVH_H__
#define __EDITBMESH_BVH_H__

struct BMEditMesh;
struct BMFace;
struct BMEdge;
struct BMVert;
struct RegionView3D;
struct BMBVHTree;
struct BVHTree;
struct Scene;
struct Object;

#ifndef IN_EDITMESHBVH
typedef struct BMBVHTree BMBVHTree;
#endif

struct BMBVHTree *BMBVH_NewBVH(struct BMEditMesh *em, int flag, struct Scene *scene, struct Object *obedit);
void BMBVH_FreeBVH(struct BMBVHTree *tree);
struct BVHTree *BMBVH_BVHTree(struct BMBVHTree *tree);

struct BMFace *BMBVH_RayCast(struct BMBVHTree *tree, float *co, float *dir, float *hitout, float *cagehit);

int BMBVH_EdgeVisible(struct BMBVHTree *tree, struct BMEdge *e, 
                      struct ARegion *ar, struct View3D *v3d, struct Object *obedit);

#define BM_SEARCH_MAXDIST	0.4f

/*find a vert closest to co in a sphere of radius maxdist*/
struct BMVert *BMBVH_FindClosestVert(struct BMBVHTree *tree, float *co, float maxdist);
                                         
/* BMBVH_NewBVH flag parameter */
enum {
	BMBVH_USE_CAGE        = 1, /* project geometry onto modifier cage */
	BMBVH_RETURN_ORIG     = 2, /* use with BMBVH_USE_CAGE, returns hits in relation to original geometry */
	BMBVH_RESPECT_SELECT  = 4, /* restrict to hidden geometry (overrides BMBVH_RESPECT_HIDDEN) */
	BMBVH_RESPECT_HIDDEN  = 8  /* omit hidden geometry */
};

#endif /* __EDITBMESH_BVH_H__ */
