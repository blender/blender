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
#ifndef __DNA_KEY_TYPES_H__
#define __DNA_KEY_TYPES_H__

/** \file DNA_key_types.h
 *  \ingroup DNA
 */

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"

struct AnimData;
struct Ipo;

typedef struct KeyBlock {
	struct KeyBlock *next, *prev;
	
	float pos;
	float curval;
	short type, adrcode, relative, flag;	/* relative == 0 means first key is reference */
	int totelem, pad2;
	
	void *data;
	float *weights;
	char  name[64];	/* MAX_NAME */
	char vgroup[64];	/* MAX_VGROUP_NAME */

	float slidermin;
	float slidermax;

	int uid, pad3;
} KeyBlock;


typedef struct Key {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 
	
	KeyBlock *refkey;
	char elemstr[64];	/* MAX_NAME */
	int elemsize;
	float curval  DNA_DEPRECATED;
	
	ListBase block;
	struct Ipo *ipo  DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */
	
	ID *from;

	short type, totkey;
	short slurph, flag;
	float ctime;

	/*can never be 0, this is used for detecting old data*/
	int uidgen; /*current free uid for keyblocks*/
} Key;

/* **************** KEY ********************* */

/* key->type */
#define KEY_NORMAL      0
#define KEY_RELATIVE    1

/* key->flag */
#define KEY_DS_EXPAND	1

/* keyblock->type */
#define KEY_LINEAR      0
#define KEY_CARDINAL    1
#define KEY_BSPLINE     2

/* keyblock->flag */
#define KEYBLOCK_MUTE			(1<<0)
#define KEYBLOCK_SEL			(1<<1)
#define KEYBLOCK_LOCKED			(1<<2)
#define KEYBLOCK_MISSING		(1<<3) /*temporary flag*/
#endif
