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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#ifndef __BMESH_STRUCTURE_H__
#define __BMESH_STRUCTURE_H__

/** \file
 * \ingroup bmesh
 *
 * The lowest level of functionality for manipulating bmesh structures.
 * None of these functions should ever be exported to the rest of Blender.
 *
 * in the vast majority of cases there shouldn't be used directly.
 * if absolutely necessary, see function definitions in code for
 * descriptive comments.  but seriously, don't use this stuff.
 */

/* LOOP CYCLE MANAGEMENT */
bool bmesh_loop_validate(BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* DISK CYCLE MANAGEMENT */
void bmesh_disk_edge_append(BMEdge *e, BMVert *v) ATTR_NONNULL();
void bmesh_disk_edge_remove(BMEdge *e, BMVert *v) ATTR_NONNULL();
BLI_INLINE BMEdge *bmesh_disk_edge_next_safe(const BMEdge *e,
                                             const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BLI_INLINE BMEdge *bmesh_disk_edge_prev_safe(const BMEdge *e,
                                             const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BLI_INLINE BMEdge *bmesh_disk_edge_next(const BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BLI_INLINE BMEdge *bmesh_disk_edge_prev(const BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
int bmesh_disk_facevert_count_at_most(const BMVert *v, const int count_max) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
int bmesh_disk_facevert_count(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BMEdge *bmesh_disk_faceedge_find_first(const BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BMLoop *bmesh_disk_faceloop_find_first(const BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BMLoop *bmesh_disk_faceloop_find_first_visible(const BMEdge *e,
                                               const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BMEdge *bmesh_disk_faceedge_find_next(const BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/* RADIAL CYCLE MANAGEMENT */
void bmesh_radial_loop_append(BMEdge *e, BMLoop *l) ATTR_NONNULL();
void bmesh_radial_loop_remove(BMEdge *e, BMLoop *l) ATTR_NONNULL();
void bmesh_radial_loop_unlink(BMLoop *l) ATTR_NONNULL();
/* note:
 *      bmesh_radial_loop_next(BMLoop *l) / prev.
 * just use member access l->radial_next, l->radial_prev now */

int bmesh_radial_facevert_count_at_most(const BMLoop *l,
                                        const BMVert *v,
                                        const int count_max) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
int bmesh_radial_facevert_count(const BMLoop *l, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool bmesh_radial_facevert_check(const BMLoop *l, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BMLoop *bmesh_radial_faceloop_find_first(const BMLoop *l, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BMLoop *bmesh_radial_faceloop_find_next(const BMLoop *l, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BMLoop *bmesh_radial_faceloop_find_vert(const BMFace *f, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool bmesh_radial_validate(int radlen, BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* EDGE UTILITIES */
void bmesh_disk_vert_swap(BMEdge *e, BMVert *v_dst, BMVert *v_src) ATTR_NONNULL();
void bmesh_edge_vert_swap(BMEdge *e, BMVert *v_dst, BMVert *v_src) ATTR_NONNULL();
void bmesh_disk_vert_replace(BMEdge *e, BMVert *v_dst, BMVert *v_src) ATTR_NONNULL();
BMEdge *bmesh_disk_edge_exists(const BMVert *v1, const BMVert *v2) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool bmesh_disk_validate(int len, BMEdge *e, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

#include "intern/bmesh_structure_inline.h"

#endif /* __BMESH_STRUCTURE_H__ */
