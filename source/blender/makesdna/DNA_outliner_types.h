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

/** \file DNA_outliner_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_OUTLINER_TYPES_H__
#define __DNA_OUTLINER_TYPES_H__

#include "DNA_defs.h"

struct ID;

typedef struct TreeStoreElem {
	short type, nr, flag, used;

	/* XXX We actually also store non-ID data in this pointer for identifying
	 * the TreeStoreElem for a TreeElement when rebuilding the tree. Ugly! */
	struct ID *id;
} TreeStoreElem;

/* used only to store data in in blend files */
typedef struct TreeStore {
	int totelem  DNA_DEPRECATED; /* was previously used for memory preallocation */
	int usedelem;                /* number of elements in data array */
	TreeStoreElem *data;         /* elements to be packed from mempool in writefile.c
	                              * or extracted to mempool in readfile.c */
} TreeStore;

/* TreeStoreElem->flag */
enum {
	TSE_CLOSED      = (1 << 0),
	TSE_SELECTED    = (1 << 1),
	TSE_TEXTBUT     = (1 << 2),
	TSE_CHILDSEARCH = (1 << 3),
	TSE_SEARCHMATCH = (1 << 4),
	TSE_HIGHLIGHTED = (1 << 5),
	TSE_DRAG_INTO   = (1 << 6),
	TSE_DRAG_BEFORE = (1 << 7),
	TSE_DRAG_AFTER  = (1 << 8),
	TSE_DRAG_ANY    = (TSE_DRAG_INTO | TSE_DRAG_BEFORE | TSE_DRAG_AFTER),
};

/* TreeStoreElem->types */
#define TSE_NLA             1  /* NO ID */
#define TSE_NLA_ACTION      2
#define TSE_DEFGROUP_BASE   3
#define TSE_DEFGROUP        4
#define TSE_BONE            5
#define TSE_EBONE           6
#define TSE_CONSTRAINT_BASE 7
#define TSE_CONSTRAINT      8
#define TSE_MODIFIER_BASE   9
#define TSE_MODIFIER        10
#define TSE_LINKED_OB       11
/* #define TSE_SCRIPT_BASE     12 */  /* UNUSED */
#define TSE_POSE_BASE       13
#define TSE_POSE_CHANNEL    14
#define TSE_ANIM_DATA       15
#define TSE_DRIVER_BASE     16  /* NO ID */
/* #define TSE_DRIVER          17 */  /* UNUSED */

#define TSE_PROXY           18
#define TSE_R_LAYER_BASE    19
#define TSE_R_LAYER         20
/* #define TSE_R_PASS          21 */  /* UNUSED */
#define TSE_LINKED_MAT      22
/* NOTE, is used for light group */
#define TSE_LINKED_LAMP     23
#define TSE_POSEGRP_BASE    24
#define TSE_POSEGRP         25
#define TSE_SEQUENCE        26  /* NO ID */
#define TSE_SEQ_STRIP       27  /* NO ID */
#define TSE_SEQUENCE_DUP    28  /* NO ID */
#define TSE_LINKED_PSYS     29
#define TSE_RNA_STRUCT      30  /* NO ID */
#define TSE_RNA_PROPERTY    31  /* NO ID */
#define TSE_RNA_ARRAY_ELEM  32  /* NO ID */
#define TSE_NLA_TRACK       33  /* NO ID */
#define TSE_KEYMAP          34  /* NO ID */
#define TSE_KEYMAP_ITEM     35  /* NO ID */
#define TSE_ID_BASE         36  /* NO ID */
#define TSE_GP_LAYER        37  /* NO ID */
#define TSE_LAYER_COLLECTION      38
#define TSE_SCENE_COLLECTION_BASE 39
#define TSE_VIEW_COLLECTION_BASE  40
#define TSE_SCENE_OBJECTS_BASE    41


/* Check whether given TreeStoreElem should have a real ID in its ->id member. */
#define TSE_IS_REAL_ID(_tse) \
	(!ELEM((_tse)->type, TSE_NLA, TSE_NLA_TRACK, TSE_DRIVER_BASE, \
	                     TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP, \
                         TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM, \
                         TSE_KEYMAP, TSE_KEYMAP_ITEM, TSE_ID_BASE, TSE_GP_LAYER))


#endif
