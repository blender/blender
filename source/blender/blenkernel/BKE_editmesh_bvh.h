/*
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
 */

/** \file
 * \ingroup bke
 */

#ifndef __BKE_EDITMESH_BVH_H__
#define __BKE_EDITMESH_BVH_H__

struct BMBVHTree;
struct BMEditMesh;
struct BMFace;
struct BMLoop;
struct BMVert;
struct BMesh;
struct BVHTree;

typedef struct BMBVHTree BMBVHTree;

typedef bool (*BMBVHTree_FaceFilter)(struct BMFace *f, void *userdata);

BMBVHTree *BKE_bmbvh_new_from_editmesh(struct BMEditMesh *em,
                                       int flag,
                                       const float (*cos_cage)[3],
                                       const bool cos_cage_free);
BMBVHTree *BKE_bmbvh_new_ex(struct BMesh *bm,
                            struct BMLoop *(*looptris)[3],
                            int looptris_tot,
                            int flag,
                            const float (*cos_cage)[3],
                            const bool cos_cage_free,
                            bool (*test_fn)(struct BMFace *, void *user_data),
                            void *user_data);
BMBVHTree *BKE_bmbvh_new(struct BMesh *bm,
                         struct BMLoop *(*looptris)[3],
                         int looptris_tot,
                         int flag,
                         const float (*cos_cage)[3],
                         const bool cos_cage_free);
void BKE_bmbvh_free(BMBVHTree *tree);
struct BVHTree *BKE_bmbvh_tree_get(BMBVHTree *tree);

struct BMFace *BKE_bmbvh_ray_cast(BMBVHTree *tree,
                                  const float co[3],
                                  const float dir[3],
                                  const float radius,
                                  float *r_dist,
                                  float r_hitout[3],
                                  float r_cagehit[3]);

struct BMFace *BKE_bmbvh_ray_cast_filter(BMBVHTree *tree,
                                         const float co[3],
                                         const float dir[3],
                                         const float radius,
                                         float *r_dist,
                                         float r_hitout[3],
                                         float r_cagehit[3],
                                         BMBVHTree_FaceFilter filter,
                                         void *filter_cb);

/* find a vert closest to co in a sphere of radius dist_max */
struct BMVert *BKE_bmbvh_find_vert_closest(BMBVHTree *tree,
                                           const float co[3],
                                           const float dist_max);
struct BMFace *BKE_bmbvh_find_face_closest(BMBVHTree *tree,
                                           const float co[3],
                                           const float dist_max);

struct BVHTreeOverlap *BKE_bmbvh_overlap(const BMBVHTree *bmtree_a,
                                         const BMBVHTree *bmtree_b,
                                         unsigned int *r_overlap_tot);

/** #BKE_bmbvh_new flag parameter. */
enum {
  /** Use with 'cos_cage', returns hits in relation to original geometry. */
  BMBVH_RETURN_ORIG = (1 << 0),
  /** Restrict to hidden geometry (overrides BMBVH_RESPECT_HIDDEN). */
  BMBVH_RESPECT_SELECT = (1 << 1),
  /** Omit hidden geometry. */
  BMBVH_RESPECT_HIDDEN = (1 << 2),
};

#endif /* __BKE_EDITMESH_BVH_H__ */
