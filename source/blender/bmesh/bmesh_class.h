/*
 *	BMesh API.
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
 * Contributor(s): Geoffrey Bantle, Levi Schooley, Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef _BMESH_CLASS_H
#define _BMESH_CLASS_H

/*bmesh data structures*/

#include "DNA_listBase.h"

#include "BKE_utildefines.h"
#include "BLI_utildefines.h"

struct BMesh;
struct BMVert;
struct BMEdge;
struct BMLoop;
struct BMFace;
struct BMVert;
struct BMEdge;
struct BMLoop;
struct BMFace;
struct BMLayerType;
struct BMSubClassLayer;
struct BMFlagLayer;
struct BLI_mempool;
struct Object;

/*note: it is very important for BMHeader to start with two
  pointers. this is a requirement of mempool's method of
  iteration.
*/
typedef struct BMHeader {
	void *data; /*customdata layers*/
	struct BMFlagLayer *flags;
	short type; /*element geometric type (verts/edges/loops/faces)*/
	short flag; /*this would be a CD layer, see below*/
	int index; /*note: use BM_GetIndex/SetIndex macros for index*/
} BMHeader;

/*note: need some way to specify custom locations for custom data layers.  so we can
make them point directly into structs.  and some way to make it only happen to the
active layer, and properly update when switching active layers.*/

typedef struct BMVert {
	BMHeader head;
	float co[3];
	float no[3];
	struct BMEdge *e;
} BMVert;

typedef struct BMEdge {
	BMHeader head;
	struct BMVert *v1, *v2;
	struct BMLoop *l;
	
	/*disk cycle pointers*/
	struct {
		struct BMEdge *next, *prev;
	} dlink1;
	struct {
		struct BMEdge *next, *prev;
	} dlink2;
} BMEdge;

typedef struct BMLoop {
	BMHeader head;
	struct BMVert *v;
	struct BMEdge *e;
	struct BMFace *f;

	struct BMLoop *radial_next, *radial_prev;
	
	/*private variables*/
	struct BMLoop *next, *prev; /*won't be able to use listbase API, ger, due to head*/\
	int _index; /*used for sorting during tesselation*/
} BMLoop;

typedef struct BMLoopList {
	struct BMLoopList *next, *prev;
	struct BMLoop *first, *last;
} BMLoopList;

typedef struct BMFace {
	BMHeader head;
	int len; /*includes all boundary loops*/\
	int totbounds; /*total boundaries, is one plus the number of holes in the face*/\
	ListBase loops;
	float no[3]; /*yes, we do store this here*/\
	short mat_nr;
} BMFace;

typedef struct BMFlagLayer {
	short f, pflag; /*flags*/
	int index; /*generic index*/
} BMFlagLayer;

typedef struct BMesh {
	int totvert, totedge, totloop, totface;
	int totvertsel, totedgesel, totfacesel;
	
	/*element pools*/
	struct BLI_mempool *vpool, *epool, *lpool, *fpool;

	/*operator api stuff*/
	struct BLI_mempool *toolflagpool;
	int stackdepth;
	struct BMOperator *currentop;
	
	CustomData vdata, edata, ldata, pdata;

	struct BLI_mempool *looplistpool;
	
	/*should be copy of scene select mode*/
	int selectmode;
	
	/*ID of the shape key this bmesh came from*/
	int shapenr;
	
	int walkers, totflags;
	ListBase selected, error_stack;
	
	BMFace *act_face;

	ListBase errorstack;
	struct Object *ob; /*owner object*/
	
	int opflag; /*current operator flag*/
} BMesh;

BMFace *BM_Copy_Face(BMesh *bm, BMFace *f, int copyedges, int copyverts);

#define BM_VERT		1
#define BM_EDGE		2
#define BM_LOOP		4
#define BM_FACE		8

#endif /* _BMESH_CLASS_H */
