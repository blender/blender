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
 */

#pragma once

/** \file
 * \ingroup bmesh
 */

struct BMCalcPathUVParams {
  uint use_topology_distance : 1;
  uint use_step_face : 1;
  uint cd_loop_uv_offset;
  float aspect_y;
};

struct LinkNode *BM_mesh_calc_path_uv_vert(BMesh *bm,
                                           BMLoop *l_src,
                                           BMLoop *l_dst,
                                           const struct BMCalcPathUVParams *params,
                                           bool (*filter_fn)(BMLoop *, void *),
                                           void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 5);

struct LinkNode *BM_mesh_calc_path_uv_face(BMesh *bm,
                                           BMFace *f_src,
                                           BMFace *f_dst,
                                           const struct BMCalcPathUVParams *params,
                                           bool (*filter_fn)(BMFace *, void *),
                                           void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 5);
