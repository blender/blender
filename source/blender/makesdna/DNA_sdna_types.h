/**
 * blenlib/DNA_sdna.h (mar-2001 nzc)
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
#ifndef DNA_SDNA_H
#define DNA_SDNA_H

#
#
struct SDNA {
	char *data;
	int datalen, nr_names;
	char **names;
	int nr_types, pointerlen;
	char **types;
	short *typelens;
	int nr_structs;
	short **structs;
	
		/* wrong place for this really, its a simple
		 * cache for findstruct_nr.
		 */
	int lastfind;
};

#
#
typedef struct BHead {
	int code, len;
	void *old;
	int SDNAnr, nr;
} BHead;
#
#
typedef struct BHead4 {
	int code, len;
	int old;
	int SDNAnr, nr;
} BHead4;
#
#
typedef struct BHead8 {
	int code, len;
#if defined(WIN32) && !defined(FREE_WINDOWS)
	/* This is a compiler type! */
	__int64 old;
#else
	long long old;
#endif	
	int SDNAnr, nr;
} BHead8;

#endif

