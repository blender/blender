/**
 * blenlib/DNA_group_types.h (mar-2001 nzc)
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
#ifndef DNA_GROUP_TYPES_H
#define DNA_GROUP_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

struct Object;
struct Ipo;

typedef struct GroupKey {
	struct GroupKey *next, *prev;
	short sfra, efra;
	float cfra;
	char name[32];
} GroupKey;

typedef struct ObjectKey {
	struct ObjectKey *next, *prev;
	GroupKey *gkey;		/* for reference */

	/* copy of relevant data */
	short partype, pad;
	int par1, par2, par3;
	
	struct Object *parent, *track;
	struct Ipo *ipo;

	/* this block identical to object */	
	float loc[3], dloc[3], orig[3];
	float size[3], dsize[3];
	float rot[3], drot[3];
	float quat[4], dquat[4];
	float obmat[4][4];
	float parentinv[4][4];
	float imat[4][4];	/* voor bij render, tijdens simulate, tijdelijk: ipokeys van transform  */
	
	unsigned int lay;				/* kopie van Base */
	
	char transflag, ipoflag;
	char trackflag, upflag;
	
	float sf, ctime, padf;
		

} ObjectKey;

typedef struct GroupObject {
	struct GroupObject *next, *prev;
	struct Object *ob;
	ListBase okey;		/* ObjectKey */
	
} GroupObject;


typedef struct Group {
	ID id;
	
	ListBase gobject;	/* GroupObject */
	ListBase gkey;		/* GroupKey */
	
	GroupKey *active;
	
} Group;



#endif
