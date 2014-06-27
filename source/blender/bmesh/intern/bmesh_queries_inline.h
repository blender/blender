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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_queries_inline.h
 *  \ingroup bmesh
 */


#ifndef __BMESH_QUERIES_INLINE_H__
#define __BMESH_QUERIES_INLINE_H__

/**
 * Returns whether or not a given vertex is
 * is part of a given edge.
 */
BLI_INLINE bool BM_vert_in_edge(const BMEdge *e, const BMVert *v)
{
	return (ELEM(v, e->v1, e->v2));
}

/**
 * Returns whether or not a given edge is is part of a given loop.
 */
BLI_INLINE bool BM_edge_in_loop(const BMEdge *e, const BMLoop *l)
{
	return (l->e == e || l->prev->e == e);
}

/**
 * Returns whether or not two vertices are in
 * a given edge
 */
BLI_INLINE bool BM_verts_in_edge(const BMVert *v1, const BMVert *v2, const BMEdge *e)
{
	return ((e->v1 == v1 && e->v2 == v2) ||
	        (e->v1 == v2 && e->v2 == v1));
}

/**
 * Given a edge and one of its vertices, returns
 * the other vertex.
 */
BLI_INLINE BMVert *BM_edge_other_vert(BMEdge *e, const BMVert *v)
{
	if (e->v1 == v) {
		return e->v2;
	}
	else if (e->v2 == v) {
		return e->v1;
	}
	return NULL;
}

/**
 * Tests whether or not the edge is part of a wire.
 * (ie: has no faces attached to it)
 */
BLI_INLINE bool BM_edge_is_wire(const BMEdge *e)
{
	return (e->l == NULL);
}

/**
 * Tests whether or not this edge is manifold.
 * A manifold edge has exactly 2 faces attached to it.
 */

#if 1 /* fast path for checking manifold */
BLI_INLINE bool BM_edge_is_manifold(const BMEdge *e)
{
	const BMLoop *l = e->l;
	return (l && (l->radial_next != l) &&             /* not 0 or 1 face users */
	             (l->radial_next->radial_next == l)); /* 2 face users */
}
#else
BLI_INLINE int BM_edge_is_manifold(BMEdge *e)
{
	return (BM_edge_face_count(e) == 2);
}
#endif

/**
 * Tests that the edge is manifold and
 * that both its faces point the same way.
 */
BLI_INLINE bool BM_edge_is_contiguous(const BMEdge *e)
{
	const BMLoop *l = e->l;
	const BMLoop *l_other;
	return (l && ((l_other = l->radial_next) != l) &&  /* not 0 or 1 face users */
	             (l_other->radial_next == l) &&        /* 2 face users */
	             (l_other->v != l->v));
}

/**
 * Tests whether or not an edge is on the boundary
 * of a shell (has one face associated with it)
 */

#if 1 /* fast path for checking boundary */
BLI_INLINE bool BM_edge_is_boundary(const BMEdge *e)
{
	const BMLoop *l = e->l;
	return (l && (l->radial_next == l));
}
#else
BLI_INLINE int BM_edge_is_boundary(BMEdge *e)
{
	return (BM_edge_face_count(e) == 1);
}
#endif

/**
 * Tests whether one loop is next to another within the same face.
 */
BLI_INLINE bool BM_loop_is_adjacent(const BMLoop *l_a, const BMLoop *l_b)
{
	BLI_assert(l_a->f == l_b->f);
	BLI_assert(l_a != l_b);
	return (ELEM(l_b, l_a->next, l_a->prev));
}

/**
 * Check if we have a single wire edge user.
 */
BLI_INLINE bool BM_vert_is_wire_endpoint(const BMVert *v)
{
	const BMEdge *e = v->e;
	if (e && e->l == NULL) {
		return (BM_DISK_EDGE_NEXT(e, v) == e);
	}
	return false;
}

#endif /* __BMESH_QUERIES_INLINE_H__ */
