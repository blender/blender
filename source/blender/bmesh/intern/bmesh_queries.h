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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_QUERIES_H__
#define __BMESH_QUERIES_H__

/** \file blender/bmesh/intern/bmesh_queries.h
 *  \ingroup bmesh
 */

bool    BM_vert_in_face(BMFace *f, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int     BM_verts_in_face_count(BMFace *f, BMVert **varr, int len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_verts_in_face(BMFace *f, BMVert **varr, int len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool    BM_edge_in_face(BMEdge *e, BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool    BM_edge_in_loop(const BMEdge *e, const BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

BLI_INLINE bool    BM_vert_in_edge(const BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool    BM_verts_in_edge(const BMVert *v1, const BMVert *v2, const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

float   BM_edge_calc_length(BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float   BM_edge_calc_length_squared(BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_edge_face_pair(BMEdge *e, BMFace **r_fa, BMFace **r_fb) ATTR_NONNULL();
bool    BM_edge_loop_pair(BMEdge *e, BMLoop **r_la, BMLoop **r_lb) ATTR_NONNULL();
BLI_INLINE BMVert *BM_edge_other_vert(BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_edge_other_loop(BMEdge *e, BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_face_other_edge_loop(BMFace *f, BMEdge *e, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_loop_other_edge_loop(BMLoop *l, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_face_other_vert_loop(BMFace *f, BMVert *v_prev, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_loop_other_vert_loop(BMLoop *l, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_vert_step_fan_loop(BMLoop *l, BMEdge **e_step) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_vert_find_first_loop(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool    BM_vert_pair_share_face_check(
        BMVert *v_a, BMVert *v_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_vert_pair_share_face_check_cb(
        BMVert *v_a, BMVert *v_b,
        bool (*test_fn)(BMFace *f, void *user_data), void *user_data) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);
BMFace *BM_vert_pair_share_face_by_len(
        BMVert *v_a, BMVert *v_b,
        BMLoop **r_l_a, BMLoop **r_l_b,
        const bool allow_adjacent) ATTR_NONNULL();
BMFace *BM_vert_pair_share_face_by_angle(
        BMVert *v_a, BMVert *v_b,
        BMLoop **r_l_a, BMLoop **r_l_b,
        const bool allow_adjacent) ATTR_NONNULL();

int     BM_vert_edge_count_nonwire(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int     BM_vert_edge_count(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int     BM_edge_face_count(BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int     BM_vert_face_count(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMEdge *BM_vert_other_disk_edge(BMVert *v, BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool    BM_vert_is_edge_pair(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_vert_is_wire(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool    BM_edge_is_wire(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool    BM_vert_is_manifold(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool    BM_edge_is_manifold(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_vert_is_boundary(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool    BM_edge_is_boundary(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool    BM_edge_is_contiguous(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_edge_is_convex(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool    BM_loop_is_convex(const BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool BM_loop_is_adjacent(const BMLoop *l_a, const BMLoop *l_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

float   BM_loop_calc_face_angle(BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void    BM_loop_calc_face_normal(BMLoop *l, float r_normal[3]) ATTR_NONNULL();
void    BM_loop_calc_face_direction(BMLoop *l, float r_normal[3]);
void    BM_loop_calc_face_tangent(BMLoop *l, float r_tangent[3]);

float   BM_edge_calc_face_angle_ex(const BMEdge *e, const float fallback) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float   BM_edge_calc_face_angle(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float   BM_edge_calc_face_angle_signed_ex(const BMEdge *e, const float fallback) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float   BM_edge_calc_face_angle_signed(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void    BM_edge_calc_face_tangent(const BMEdge *e, const BMLoop *e_loop, float r_tangent[3]) ATTR_NONNULL();

float   BM_vert_calc_edge_angle(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float   BM_vert_calc_shell_factor(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float   BM_vert_calc_shell_factor_ex(BMVert *v, const char hflag) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float   BM_vert_calc_mean_tagged_edge_length(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

BMLoop *BM_face_find_shortest_loop(BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_face_find_longest_loop(BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

BMEdge *BM_edge_exists(BMVert *v1, BMVert *v2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMEdge *BM_edge_find_double(BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool    BM_face_exists(BMVert **varr, int len, BMFace **r_existface) ATTR_NONNULL(1);

bool    BM_face_exists_multi(BMVert **varr, BMEdge **earr, int len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_face_exists_multi_edge(BMEdge **earr, int len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool    BM_face_exists_overlap(BMVert **varr, const int len, BMFace **r_f_overlap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
bool    BM_face_exists_overlap_subset(BMVert **varr, const int len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

int     BM_face_share_face_count(BMFace *f_a, BMFace *f_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int     BM_face_share_edge_count(BMFace *f1, BMFace *f2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool    BM_face_share_face_check(BMFace *f1, BMFace *f2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_face_share_edge_check(BMFace *f1, BMFace *f2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_edge_share_face_check(BMEdge *e1, BMEdge *e2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_edge_share_quad_check(BMEdge *e1, BMEdge *e2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool    BM_edge_share_vert_check(BMEdge *e1, BMEdge *e2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

BMVert *BM_edge_share_vert(BMEdge *e1, BMEdge *e2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_edge_vert_share_loop(BMLoop *l, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_face_vert_share_loop(BMFace *f, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMLoop *BM_face_edge_share_loop(BMFace *f, BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

void    BM_edge_ordered_verts(const BMEdge *edge, BMVert **r_v1, BMVert **r_v2) ATTR_NONNULL();
void    BM_edge_ordered_verts_ex(const BMEdge *edge, BMVert **r_v1, BMVert **r_v2,
                                 const BMLoop *edge_loop) ATTR_NONNULL();

bool BM_vert_is_all_edge_flag_test(const BMVert *v, const char hflag, const bool respect_hide) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BM_vert_is_all_face_flag_test(const BMVert *v, const char hflag, const bool respect_hide) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BM_edge_is_all_face_flag_test(const BMEdge *e, const char hflag, const bool respect_hide) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool BM_edge_is_any_vert_flag_test(const BMEdge *e, const char hflag) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BM_face_is_any_vert_flag_test(const BMFace *f, const char hflag) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BM_face_is_any_edge_flag_test(const BMFace *f, const char hflag) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool BM_face_is_normal_valid(const BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

float BM_mesh_calc_volume(BMesh *bm, bool is_signed) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

int   BM_mesh_calc_face_groups(BMesh *bm, int *r_groups_array, int (**r_group_index)[2],
                               BMElemFilterFunc filter_fn, void *user_data,
                               const char hflag_test, const char htype_step) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);
int   BM_mesh_calc_edge_groups(BMesh *bm, int *r_groups_array, int (**r_group_index)[2],
                               BMElemFilterFunc filter_fn, void *user_data,
                               const char hflag_test) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);

/* not really any good place  to put this */
float bmesh_subd_falloff_calc(const int falloff, float val) ATTR_WARN_UNUSED_RESULT;

#include "bmesh_queries_inline.h"

#endif /* __BMESH_QUERIES_H__ */
