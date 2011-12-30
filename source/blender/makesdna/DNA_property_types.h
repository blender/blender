/*
 *
 *
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

/** \file DNA_property_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
 *  \attention Renderrecipe and scene decription. The fact that there is a
 *             hierarchy here is a bit strange, and not desirable.
 */

#ifndef DNA_PROPERTY_TYPES_H
#define DNA_PROPERTY_TYPES_H

/* ********************* PROPERTY ************************ */

typedef struct bProperty {
	struct bProperty *next, *prev;
	char name[32];
	short type, flag;
	int data;				/* data should be 4 bytes to store int,float stuff */
	void *poin;				/* references data unless its a string which is malloc'd */
	
} bProperty;

/* property->type XXX Game Property, not RNA */
#define GPROP_BOOL		0
#define GPROP_INT		1
#define GPROP_FLOAT		2
#define GPROP_STRING	3
#define GPROP_VECTOR	4
#define GPROP_TIME		5

/* property->flag */
#define PROP_DEBUG		1

#define MAX_PROPSTRING	128

#endif

