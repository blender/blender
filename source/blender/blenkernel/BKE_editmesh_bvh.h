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

/** \file BKE_editmesh_bvh.h
 *  \ingroup bke
 */

#ifndef __BKE_EDITMESH_BVH_H__
#define __BKE_EDITMESH_BVH_H__

struct BMEditMesh;
struct BMFace;
struct BMEdge;
struct BMVert;
struct RegionView3D;
struct BMBVHTree;
struct BVHTree;
struct Scene;
struct Object;

typedef struct BMBVHTree BMBVHTree;

BMBVHTree *BMBVH_NewBVH(struct BMEditMesh *em, int flag, struct Scene *scene);
void BMBVH_FreeBVH(BMBVHTree *tree);
struct BVHTree *BMBVH_BVHTree(BMBVHTree *tree);

struct BMFace *BMBVH_RayCast(BMBVHTree *tree, const float co[3], const float dir[3],
                             float r_hitout[3], float r_cagehit[3]);

/*find a vert closest to co in a sphere of radius maxdist*/
struct BMVert *BMBVH_FindClosestVert(BMBVHTree *tree, const float co[3], const float maxdist);

/* BMBVH_NewBVH flag parameter */
enum {
	BMBVH_USE_CAGE        = 1, /* project geometry onto modifier cage */
	BMBVH_RETURN_ORIG     = 2, /* use with BMBVH_USE_CAGE, returns hits in relation to original geometry */
	BMBVH_RESPECT_SELECT  = 4, /* restrict to hidden geometry (overrides BMBVH_RESPECT_HIDDEN) */
	BMBVH_RESPECT_HIDDEN  = 8  /* omit hidden geometry */
};

#endif  /* __BKE_EDITMESH_BVH_H__ */
