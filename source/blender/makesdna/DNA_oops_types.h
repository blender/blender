/**
 * blenlib/DNA_oops_types.h (mar-2001 nzc)
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
#ifndef DNA_OOPS_TYPES_H
#define DNA_OOPS_TYPES_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define OOPSX	5.0
#define OOPSY	1.8

#include "DNA_listBase.h"

struct ID;

typedef struct Oops {
	struct Oops *next, *prev;
	short type, flag, dt, hide;
	float x, y;		/* linksonder */
	float dx, dy;	/* shuffle */
	struct ID *id;
	ListBase link;
} Oops;

#
#
typedef struct OopsLink {
	struct OopsLink *next, *prev;
	short type, flag;
	ID **idfrom;
	Oops *to, *from;	/* from is voor temp */
	float xof, yof;
	char name[12];
} OopsLink;

/* oops->flag  (1==SELECT) */
#define OOPS_DOSELECT	2
#define OOPS_REFER		4

#endif

