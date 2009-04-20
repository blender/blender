/*
 * radio_types.h
 *
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*  #include "misc_util.h" */ /* for listbase...*/


#ifndef RADIO_TYPES_H
#define RADIO_TYPES_H

#include "DNA_listBase.h" 
#include "DNA_material_types.h" 

struct Render;
struct CustomData;

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
#define RAD_TWOSIDED	16


typedef struct RNode {					/* length: 104 */
	struct RNode *down1, *down2, *up;
	struct RNode *ed1, *ed2, *ed3, *ed4;
	struct RPatch *par;

	char lev1, lev2, lev3, lev4;		/* edgelevels */
	short type;							/* type: 4==QUAD, 3==TRIA */
	short f;							
	float *v1, *v2, *v3, *v4;
	float totrad[3], area;
	
	unsigned int col;
	int orig;							/* index in custom face data */
} RNode;


typedef struct Face {					/* length: 52 */
	float *v1, *v2, *v3, *v4;
	unsigned int col, matindex;
	int orig;							/* index in custom face data */
} Face;

/* rp->f1 */
#define RAD_NO_SPLIT	1

typedef struct RPatch {
	struct RPatch *next, *prev;
	RNode *first;			/* first node==patch */

	struct Object *from;
	
	int type;				/* 3: TRIA, 4: QUAD */
	short f, f1;			/* flags f: if node, only for subdiv */

	float ref[3], emit[3], unshot[3];
	float cent[3], norm[3];
	float area;
	int matindex;
	
} RPatch;


typedef struct VeNoCo {				/* needed for splitconnected */
	struct VeNoCo *next;
	float *v;
	float *n;
	float *col;
	int flag;
} VeNoCo;


typedef	struct EdSort {					/* sort edges */
	float *v1, *v2;
	RNode *node;
	int nr;
} EdSort;

typedef struct {
	struct Radio *radio;
	unsigned int *hemibuf;
	struct ListBase patchbase;
	int totpatch, totelem, totvert, totlamp;
	RNode **elem;						/* global array with all pointers */
	VeNoCo *verts;						/* temporal vertices from patches */
	float *formfactors;				    /* 1 factor per element */
	float *topfactors, *sidefactors;    /* LUT for delta's */
	int *index;						/* LUT for above LUT */
	Face **facebase;
	int totface;
	float min[3], max[3], size[3], cent[3];	/* world */
	float maxsize, totenergy;
	float patchmin, patchmax;
	float elemmin, elemmax;
	float radfactor, lostenergy, igamma;		/* radfac is in button, radfactor is calculated */
	int phase;
	struct Render *re;							/* for calling hemizbuf correctly */
	/* to preserve materials as used before, max 16 */
	Material *matar[MAXMAT];
	int totmat;

	/* for preserving face data */
	int mfdatatot;
	struct CustomData *mfdata;
	struct RNode **mfdatanodes;
	
		/* this part is a copy of struct Radio */
	short hemires, maxiter;
	short drawtype, flag;			/* bit 0 en 1: show limits */
	short subshootp, subshoote, nodelim, maxsublamp;
	int maxnode;
	float convergence;
	float radfac, gamma;		/* for display */

} RadGlobal;

#endif /* radio_types.h */

