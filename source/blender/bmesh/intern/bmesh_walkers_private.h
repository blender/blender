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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_WALKERS_PRIVATE_H__
#define __BMESH_WALKERS_PRIVATE_H__

/** \file blender/bmesh/intern/bmesh_walkers_private.h
 *  \ingroup bmesh
 *
 * BMesh walker API.
 */

extern BMWalker *bm_walker_types[];
extern const int bm_totwalkers;


/* Pointer hiding */
typedef struct BMwGenericWalker {
	Link link;
	int depth;
} BMwGenericWalker;


typedef struct BMwShellWalker {
	BMwGenericWalker header;
	BMEdge *curedge;
} BMwShellWalker;

typedef struct BMwIslandboundWalker {
	BMwGenericWalker header;
	BMLoop *base;
	BMVert *lastv;
	BMLoop *curloop;
} BMwIslandboundWalker;

typedef struct BMwIslandWalker {
	BMwGenericWalker header;
	BMFace *cur;
} BMwIslandWalker;

typedef struct BMwLoopWalker {
	BMwGenericWalker header;
	BMEdge *cur, *start;
	BMVert *lastv, *startv;
	BMFace *f_hub;
	bool is_boundary; /* boundary looping changes behavior */
	bool is_single;  /* single means the edge verts are only connected to 1 face */
} BMwLoopWalker;

typedef struct BMwFaceLoopWalker {
	BMwGenericWalker header;
	BMLoop *l;
	bool no_calc;
} BMwFaceLoopWalker;

typedef struct BMwEdgeringWalker {
	BMwGenericWalker header;
	BMLoop *l;
	BMEdge *wireedge;
} BMwEdgeringWalker;

typedef struct BMwEdgeboundaryWalker {
	BMwGenericWalker header;
	BMEdge *e;
} BMwEdgeboundaryWalker;

typedef struct BMwUVEdgeWalker {
	BMwGenericWalker header;
	BMLoop *l;
} BMwUVEdgeWalker;

typedef struct BMwConnectedVertexWalker {
	BMwGenericWalker header;
	BMVert *curvert;
} BMwConnectedVertexWalker;

#endif /* __BMESH_WALKERS_PRIVATE_H__ */
