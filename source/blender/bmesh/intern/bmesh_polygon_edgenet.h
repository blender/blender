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

#ifndef __BMESH_POLYGON_EDGENET_H__
#define __BMESH_POLYGON_EDGENET_H__

/** \file \ingroup bmesh
 */

bool BM_face_split_edgenet(
        BMesh *bm, BMFace *f,
        BMEdge **edge_net, const int edge_net_len,
        BMFace ***r_face_arr, int *r_face_arr_len);

bool BM_face_split_edgenet_connect_islands(
        BMesh *bm,
        BMFace *f, BMEdge **edge_net_init, const uint edge_net_init_len,
        bool use_partial_connect,
        struct MemArena *arena,
        BMEdge ***r_edge_net_new, uint *r_edge_net_new_len)
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3, 6, 7, 8);

#endif  /* __BMESH_POLYGON_EDGENET_H__ */
