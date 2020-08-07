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

/** \file
 * \ingroup bmesh
 *
 * BMesh inline operator functions.
 */

#pragma once

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2)
    BLI_INLINE BMDiskLink *bmesh_disk_edge_link_from_vert(const BMEdge *e, const BMVert *v)
{
  BLI_assert(BM_vert_in_edge(e, v));
  return (BMDiskLink *)&(&e->v1_disk_link)[v == e->v2];
}

/**
 * \brief Next Disk Edge
 *
 * Find the next edge in a disk cycle
 *
 * \return Pointer to the next edge in the disk cycle for the vertex v.
 */
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1)
    BLI_INLINE BMEdge *bmesh_disk_edge_next_safe(const BMEdge *e, const BMVert *v)
{
  if (v == e->v1) {
    return e->v1_disk_link.next;
  }
  if (v == e->v2) {
    return e->v2_disk_link.next;
  }
  return NULL;
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1)
    BLI_INLINE BMEdge *bmesh_disk_edge_prev_safe(const BMEdge *e, const BMVert *v)
{
  if (v == e->v1) {
    return e->v1_disk_link.prev;
  }
  if (v == e->v2) {
    return e->v2_disk_link.prev;
  }
  return NULL;
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2) BLI_INLINE BMEdge *bmesh_disk_edge_next(const BMEdge *e,
                                                                                   const BMVert *v)
{
  return BM_DISK_EDGE_NEXT(e, v);
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2) BLI_INLINE BMEdge *bmesh_disk_edge_prev(const BMEdge *e,
                                                                                   const BMVert *v)
{
  return BM_DISK_EDGE_PREV(e, v);
}
