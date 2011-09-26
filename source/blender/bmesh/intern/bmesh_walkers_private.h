/**
 *  bmesh_walkers_private.h    april 2011
 *
 *	BMesh walker API.
 *
 * $Id: $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Joseph Eagar, Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

extern BMWalker *bm_walker_types[];
extern int bm_totwalkers;


/* Pointer hiding*/
typedef struct bmesh_walkerGeneric{
	Link link;
	int depth;
} bmesh_walkerGeneric;


typedef struct shellWalker{
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
