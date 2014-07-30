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
struct BMesh;
struct BMFace;
struct BMVert;
struct BMLoop;
struct BMBVHTree;
struct BVHTree;
struct Scene;

typedef struct BMBVHTree BMBVHTree;

BMBVHTree      *BKE_bmbvh_new_from_editmesh(
        struct BMEditMesh *em, int flag,
        const float (*cos_cage)[3], const bool cos_cage_free);
BMBVHTree      *BKE_bmbvh_new_ex(
        struct BMesh *bm, struct BMLoop *(*looptris)[3], int looptris_tot, int flag,
        const float (*cos_cage)[3], const bool cos_cage_free,
        bool (*test_fn)(struct BMFace *, void *user_data), void *user_data);
BMBVHTree      *BKE_bmbvh_new(
        struct BMesh *bm, struct BMLoop *(*looptris)[3], int looptris_tot, int flag,
        const float (*cos_cage)[3], const bool cos_cage_free);
void            BKE_bmbvh_free(BMBVHTree *tree);
struct BVHTree *BKE_bmbvh_tree_get(BMBVHTree *tree);
struct BMFace  *BKE_bmbvh_ray_cast(BMBVHTree *tree, const float co[3], const float dir[3], const float radius,
                                   float *r_dist, float r_hitout[3], float r_cagehit[3]);
/* find a face intersecting a segment (but not apart of the segment) */
struct BMFace  *BKE_bmbvh_find_face_segment(BMBVHTree *tree, const float co_a[3], const float co_b[3],
                                            float *r_fac, float r_hitout[3], float r_cagehit[3]);
/* find a vert closest to co in a sphere of radius dist_max */
struct BMVert  *BKE_bmbvh_find_vert_closest(BMBVHTree *tree, const float co[3], const float dist_max);

/* BKE_bmbvh_new flag parameter */
enum {
	BMBVH_RETURN_ORIG     = (1 << 0), /* use with 'cos_cage', returns hits in relation to original geometry */
	BMBVH_RESPECT_SELECT  = (1 << 1), /* restrict to hidden geometry (overrides BMBVH_RESPECT_HIDDEN) */
	BMBVH_RESPECT_HIDDEN  = (1 << 2)  /* omit hidden geometry */
};

#endif  /* __BKE_EDITMESH_BVH_H__ */
