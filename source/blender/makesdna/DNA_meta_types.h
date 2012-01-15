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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_meta_types.h
 *  \ingroup DNA
 */

#ifndef DNA_META_TYPES_H
#define DNA_META_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

struct BoundBox;
struct AnimData;
struct Ipo;
struct Material;


typedef struct MetaElem {
	struct MetaElem *next, *prev;

	struct BoundBox *bb;        /* Bound Box of MetaElem */

	short type, flag, selcol1, selcol2;
	float x, y, z;          /* Position of center of MetaElem */
	float quat[4];          /* Rotation of MetaElem (MUST be kept normalized) */
	float expx; /* dimension parameters, used for some types like cubes */
	float expy;
	float expz;
	float rad;              /* radius of the meta element */
	float rad2;             /* temp field, used only while processing */
	float s;                /* stiffness, how much of the element to fill */
	float len;              /* old, only used for backwards compat. use dimensions now */
	
	float *mat, *imat;      /* matrix and inverted matrix */
	
} MetaElem;

typedef struct MetaBall {
	ID id;
	struct AnimData *adt;
	
	struct BoundBox *bb;

	ListBase elems;
	ListBase disp;
	ListBase *editelems;		/* not saved in files, note we use pointer for editmode check */
	struct Ipo *ipo  DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */

	/* material of the mother ball will define the material used of all others */
	struct Material **mat; 

	char flag, flag2;			/* flag is enum for updates, flag2 is bitflags for settings */
	short totcol;
	short texflag, pad; /* used to store MB_AUTOSPACE */
	
	/* texture space, copied as one block in editobject.c */
	float loc[3];
	float size[3];
	float rot[3];
	
	float wiresize, rendersize; /* display and render res */
	
	/* bias elements to have an offset volume.
	mother ball changes will effect other objects thresholds,
	but these may also have their own thresh as an offset */
	float thresh;

	/* used in editmode */
	/*ListBase edit_elems;*/
	MetaElem *lastelem;	
} MetaBall;

/* **************** METABALL ********************* */

/* texflag */
#define MB_AUTOSPACE	1

/* mb->flag */
#define MB_UPDATE_ALWAYS	0
#define MB_UPDATE_HALFRES	1
#define MB_UPDATE_FAST		2
#define MB_UPDATE_NEVER		3

/* mb->flag2 */
#define MB_DS_EXPAND 	(1<<0)


/* ml->type */
#define MB_BALL		0
#define MB_TUBEX	1 /* depercated */
#define MB_TUBEY	2 /* depercated */
#define MB_TUBEZ	3 /* depercated */
#define MB_TUBE		4
#define MB_PLANE	5
#define MB_ELIPSOID	6
#define MB_CUBE 	7

/* ml->flag */
#define MB_NEGATIVE	2
#define MB_HIDE		8
#define MB_SCALE_RAD	16

#endif
