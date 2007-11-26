/* display list (or rather multi purpose list) stuff */
/* 
	$Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****

*/

#ifndef BKE_DISPLIST_H
#define BKE_DISPLIST_H

#include "DNA_customdata_types.h"
#include "BKE_customdata.h"

/* dl->type */
#define DL_POLY                 0
#define DL_SEGM                 1
#define DL_SURF                 2
#define DL_INDEX3               4
#define DL_INDEX4               5
#define DL_VERTCOL              6
#define DL_VERTS				7

/* dl->flag */
#define DL_CYCL_U		1
#define DL_CYCL_V		2
#define DL_FRONT_CURVE	4
#define DL_BACK_CURVE	8

#define DL_SURFINDEX(cyclu, cyclv, sizeu, sizev)	    \
\
if( (cyclv)==0 && a==(sizev)-1) break;		    \
if(cyclu) {						    \
	p1= sizeu*a;					    \
		p2= p1+ sizeu-1;				    \
			p3= p1+ sizeu;					    \
				p4= p2+ sizeu;					    \
					b= 0;						    \
}							    \
else {						    \
	p2= sizeu*a;					    \
		p1= p2+1;					    \
			p4= p2+ sizeu;					    \
				p3= p1+ sizeu;					    \
					b= 1;						    \
}							    \
if( (cyclv) && a==sizev-1) {			    \
	p3-= sizeu*sizev;				    \
		p4-= sizeu*sizev;				    \
}


/* prototypes */

struct Base;
struct Object;
struct Curve;
struct ListBase;
struct Material;
struct Bone;
struct Mesh;


/* used for curves, nurbs, mball, importing */
typedef struct DispList {
    struct DispList *next, *prev;
    short type, flag;
    int parts, nr;
    short col, rt;              /* rt used by initrenderNurbs */
	float *verts, *nors;
	int *index;
	unsigned int *col1, *col2;
	int charidx;
	int totindex;				/* indexed array drawing surfaces */

	unsigned int *bevelSplitFlag;
} DispList;

extern void copy_displist(struct ListBase *lbn, struct ListBase *lb);
extern void free_disp_elem(DispList *dl);
extern DispList *find_displist_create(struct ListBase *lb, int type);
extern DispList *find_displist(struct ListBase *lb, int type);
extern void addnormalsDispList(struct Object *ob, struct ListBase *lb);
extern void count_displist(struct ListBase *lb, int *totvert, int *totface);
extern void freedisplist(struct ListBase *lb);
extern int displist_has_faces(struct ListBase *lb);
extern void makeDerivedMesh(struct Object *ob, CustomDataMask dataMask);
extern void makeDispListSurf(struct Object *ob, struct ListBase *dispbase, int forRender);
extern void makeDispListCurveTypes(struct Object *ob, int forOrco);
extern void makeDispListMBall(struct Object *ob);
extern void shadeDispList(struct Base *base);
extern void shadeMeshMCol(struct Object *ob, struct Mesh *me);

void imagestodisplist(void);
void reshadeall_displist(void);
void filldisplist(struct ListBase *dispbase, struct ListBase *to);

void fastshade_free_render(void);

float calc_taper(struct Object *taperobj, int cur, int tot);

#endif

