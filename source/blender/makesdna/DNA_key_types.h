/**
 * blenlib/DNA_key_types.h (mar-2001 nzc)
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
#ifndef DNA_KEY_TYPES_H
#define DNA_KEY_TYPES_H

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
	char  name[32];
	char vgroup[32];

	float slidermin;
	float slidermax;
} KeyBlock;


typedef struct Key {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 
	
	KeyBlock *refkey;
	char elemstr[32];
	int elemsize;
	float curval;
	
	ListBase block;
	struct Ipo *ipo;		// XXX depreceated... old animation system
	
	ID *from;

	short type, totkey;
	short slurph, flag;
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

#endif

