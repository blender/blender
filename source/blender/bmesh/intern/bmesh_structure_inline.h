/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BMesh inline operator functions.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
