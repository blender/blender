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
extern int bm_totwalkers;


/* Pointer hiding*/
typedef struct bmesh_walkerGeneric {
	Link link;
	int depth;
} bmesh_walkerGeneric;


typedef struct shellWalker {
	bmesh_walkerGeneric header;
	BMEdge *curedge;
} shellWalker;

typedef struct islandboundWalker {
	bmesh_walkerGeneric header;
	BMLoop *base;
	BMVert *lastv;
	BMLoop *curloop;
} islandboundWalker;

typedef struct islandWalker {
	bmesh_walkerGeneric header;
	BMFace *cur;
} islandWalker;

typedef struct loopWalker {
	bmesh_walkerGeneric header;
	BMEdge *cur, *start;
	BMVert *lastv, *startv;
	int startrad, stage2;
} loopWalker;

typedef struct faceloopWalker {
	bmesh_walkerGeneric header;
	BMLoop *l;
	int nocalc;
} faceloopWalker;

typedef struct edgeringWalker {
	bmesh_walkerGeneric header;
	BMLoop *l;
	BMEdge *wireedge;
} edgeringWalker;

typedef struct uvedgeWalker {
	bmesh_walkerGeneric header;
	BMLoop *l;
} uvedgeWalker;

typedef struct connectedVertexWalker {
	bmesh_walkerGeneric header;
	BMVert *curvert;
} connectedVertexWalker;

#endif /* __BMESH_WALKERS_PRIVATE_H__ */
