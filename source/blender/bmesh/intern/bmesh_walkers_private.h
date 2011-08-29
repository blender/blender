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

typedef struct shellWalker{
	struct shellWalker *prev;
	BMVert *base;			
	BMEdge *curedge, *current;
} shellWalker;

typedef struct islandboundWalker {
	struct islandboundWalker *prev;
	BMLoop *base;
	BMVert *lastv;
	BMLoop *curloop;
} islandboundWalker;

typedef struct islandWalker {
	struct islandWalker * prev;
	BMFace *cur;
} islandWalker;

typedef struct loopWalker {
	struct loopWalker * prev;
	BMEdge *cur, *start;
	BMVert *lastv, *startv;
	int startrad, stage2;
} loopWalker;

typedef struct faceloopWalker {
	struct faceloopWalker * prev;
	BMLoop *l;
	int nocalc;
} faceloopWalker;

typedef struct edgeringWalker {
	struct edgeringWalker * prev;
	BMLoop *l;
	BMEdge *wireedge;
} edgeringWalker;

typedef struct uvedgeWalker {
	struct uvedgeWalker *prev;
	BMLoop *l;
} uvedgeWalker;
