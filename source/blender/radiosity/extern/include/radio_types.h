/*
 * radio_types.h
 *
 * $Id$
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

/*  #include "misc_util.h" */ /* for listbase...*/


#ifndef RADIO_TYPES_H
#define RADIO_TYPES_H

#include "DNA_listBase.h" 
#include "DNA_material_types.h" 

#define DTWIRE		0
#define DTGOUR		2
#define DTSOLID		1

#define PI  M_PI
#define RAD_MAXFACETAB	1024
#define RAD_NEXTFACE(a)	if( ((a) & 1023)==0 ) face= RG.facebase[(a)>>10]; else face++;

/* RG.phase */
#define RAD_SHOOTE 	1
#define RAD_SHOOTP 	2
#define RAD_SOLVE 	3

typedef struct RadView {
	float cam[3], tar[3], up[3];
	float wx1, wx2, wy1, wy2;
	float mynear, myfar;
	float viewmat[4][4], winmat[4][4];
	unsigned int *rect, *rectz;
	short rectx, recty;
	int wid;

} RadView;

/* rn->f */
#define RAD_PATCH		1
#define RAD_SHOOT		2
#define RAD_SUBDIV		4
#define RAD_BACKFACE	8


typedef struct RNode {					/* lengte: 76 */
	struct RNode *down1, *down2, *up;
	struct RNode *ed1, *ed2, *ed3, *ed4;
	struct RPatch *par;

	char lev1, lev2, lev3, lev4;		/* edgelevels */
	short type;							/* type: 4==QUAD, 3==TRIA */
	short f;							
	float *v1, *v2, *v3, *v4;
	float totrad[3], area;
	
	unsigned int col;
} RNode;


typedef struct Elem {					/* lengte: 44 */
	struct RPatch *par;

	short type;							/* type: 4==QUAD, 3==TRIA */
	short f;							/* bit 0: patch, bit 1: shootelement */
	float *v1, *v2, *v3, *v4;
	float totrad[3], area;
	
	unsigned int col;
} Elem;


typedef struct Face {					/* lengte: 24 */
	float *v1, *v2, *v3, *v4;
	unsigned int col, matindex;
} Face;

/* rp->f1 */
#define RAD_NO_SPLIT	1

typedef struct RPatch {
	struct RPatch *next, *prev;
	RNode *first;			/* first node==patch */

	struct Object *from;
	
	int type;				/* 3: TRIA, 4: QUAD */
	short f, f1;			/* flags f: als node, alleen subdiv */

	float ref[3], emit[3], unshot[3];
	float cent[3], norm[3];
	float area;
	int matindex;
	
} RPatch;


typedef struct VeNoCo {				/* nodig voor splitconnected */
	struct VeNoCo *next;
	float *v;
	float *n;
	float *col;
	int flag;
} VeNoCo;


typedef	struct EdSort {					/* sorteren edges */
	float *v1, *v2;
	RNode *node;
	int nr;
} EdSort;

typedef struct {
	struct Radio *radio;
	unsigned int *hemibuf;
	struct ListBase patchbase;
	int totpatch, totelem, totvert, totlamp;
	RNode **elem;						/* globaal array van alle pointers */
	VeNoCo *verts;						/* tijdelijk vertices van patches */
	float *formfactors;				    /* een factor per element */
	float *topfactors, *sidefactors;    /* LUT voor delta's */
	int *index;						/* LUT voor bovenstaande LUT */
	Face **facebase;
	int totface;
	float min[3], max[3], size[3], cent[3];	/* world */
	float maxsize, totenergy;
	float patchmin, patchmax;
	float elemmin, elemmax;
	float radfactor, lostenergy, igamma;		/* radfac zit in button, radfactor wordt berekend */
	int phase;
	/* to preserve materials as used before, max 16 */
	Material *matar[MAXMAT];
	int totmat;
	
		/* this part is a copy of struct Radio */
	short hemires, maxiter;
	short drawtype, flag;			/* bit 0 en 1: limits laten zien */
	short subshootp, subshoote, nodelim, maxsublamp;
	int maxnode;
	float convergence;
	float radfac, gamma;		/* voor afbeelden */

} RadGlobal;

#endif /* radio_types.h */
