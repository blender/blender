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
 * Contributor(s): mar-2001 nzc
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_group_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_GROUP_TYPES_H__
#define __DNA_GROUP_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_ID.h"

struct Object;

typedef struct GroupObject {
	struct GroupObject *next, *prev;
	struct Object *ob;
	void *lampren;		/* used while render */
	short recalc;			/* copy of ob->recalc, used to set animated groups OK */
	char pad[6];
} GroupObject;


typedef struct Group {
	ID id;
	
	ListBase gobject;	/* GroupObject */
	
	/* Bad design, since layers stored in the scenes 'Base'
	 * the objects that show in the group can change depending
	 * on the last used scene */
	unsigned int layer;
	float dupli_ofs[3];
} Group;

#endif
