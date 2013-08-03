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
#define TSE_CLOSED		1
#define TSE_SELECTED	2
#define TSE_TEXTBUT		4
#define TSE_CHILDSEARCH 8
#define TSE_SEARCHMATCH 16

/* TreeStoreElem types in BIF_outliner.h */

#endif

