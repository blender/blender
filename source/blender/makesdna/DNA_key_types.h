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
 *
 * This file defines structures for Shape-Keys (not animation keyframes),
 * attached to Mesh, Curve and Lattice Data. Even though Key's are ID blocks they
 * aren't intended to be shared between multiple data blocks as with other ID types.
 */

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"

struct AnimData;
struct Ipo;

typedef struct KeyBlock {
	struct KeyBlock *next, *prev;

	float pos;         /* point in time   (Key->type == KEY_NORMAL) only,
	                    * for historic reasons this is relative to (Key->ctime / 100),
	                    * so this value increments by 0.1f per frame. */
	float curval;      /* influence (typically [0 - 1] but can be more), (Key->type == KEY_RELATIVE) only.*/

	short type;        /* interpolation type (Key->type == KEY_NORMAL) only. */
	short pad1;

	short relative;    /* relative == 0 means first key is reference, otherwise the index of Key->blocks */
	short flag;

	int totelem;       /* total number if items in the keyblock (compare with mesh/curve verts to check we match) */
	int uid;           /* for meshes only, match the unique number with the customdata layer */
	
	void  *data;       /* array of shape key values, size is (Key->elemsize * KeyBlock->totelem) */
	char   name[64];   /* MAX_NAME (unique name, user assigned) */
	char   vgroup[64]; /* MAX_VGROUP_NAME (optional vertex group), array gets allocated into 'weights' when set */

	/* ranges, for RNA and UI only to clamp 'curval' */
	float slidermin;
	float slidermax;

} KeyBlock;


typedef struct Key {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 

	/* commonly called 'Basis', (Key->type == KEY_RELATIVE) only.
	 * Looks like this is  _always_ 'key->block.first',
	 * perhaps later on it could be defined as some other KeyBlock - campbell */
	KeyBlock *refkey;

	/* this is not a regular string, although it is \0 terminated
	 * this is an array of (element_array_size, element_type) pairs
	 * (each one char) used for calculating shape key-blocks */
	char elemstr[32];
	int elemsize;  /* size of each element in #KeyBlock.data, use for allocation and stride */
	int pad;
	
	ListBase block;  /* list of KeyBlock's */
	struct Ipo *ipo  DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */

	ID *from;

	int totkey;  /* (totkey == BLI_listbase_count(&key->block)) */
	short flag;
	char type;  /* absolute or relative shape key */
	char pad2;

	/* only used when (Key->type == KEY_NORMAL), this value is used as a time slider,
	 * rather then using the scenes time, this value can be animated to give greater control */
	float ctime;

	/* can never be 0, this is used for detecting old data */
	int uidgen; /* current free uid for keyblocks */
} Key;

/* **************** KEY ********************* */

/* Key->type: KeyBlocks are interpreted as... */
enum {
	/* Sequential positions over time (using KeyBlock->pos and Key->ctime) */
	KEY_NORMAL      = 0,

	/* States to blend between (default) */
	KEY_RELATIVE    = 1
};

/* Key->flag */
enum {
	KEY_DS_EXPAND   = 1
};

/* KeyBlock->type */
enum {
	KEY_LINEAR      = 0,
	KEY_CARDINAL    = 1,
	KEY_BSPLINE     = 2,
	KEY_CATMULL_ROM = 3,
};

/* KeyBlock->flag */
enum {
	KEYBLOCK_MUTE       = (1 << 0),
	KEYBLOCK_SEL        = (1 << 1),
	KEYBLOCK_LOCKED     = (1 << 2)
};

#endif /* __DNA_KEY_TYPES_H__  */
