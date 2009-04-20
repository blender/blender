/**
 * blenlib/DNA_property_types.h (mar-2001 nzc)
 *
 * Renderrecipe and scene decription. The fact that there is a
 * hierarchy here is a bit strange, and not desirable.
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
#ifndef DNA_PROPERTY_TYPES_H
#define DNA_PROPERTY_TYPES_H

/* ********************* PROPERTY ************************ */

typedef struct bProperty {
	struct bProperty *next, *prev;
	char name[32];
	short type, otype;		/* otype is for buttons, when a property type changes */
	int data;				/* data should be 4 bytes to store int,float stuff */
	int old;				/* old is for simul */
	short flag, pad;
	void *poin;
	void *oldpoin;			/* oldpoin is for simul */
	
} bProperty;

/* property->type */
#define PROP_BOOL		0
#define PROP_INT		1
#define PROP_FLOAT		2
#define PROP_STRING		3
#define PROP_VECTOR		4
#define PROP_TIME		5

/* property->flag */
#define PROP_DEBUG		1

#define MAX_PROPSTRING	128

#endif

