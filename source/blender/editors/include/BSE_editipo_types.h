/**
 * $Id: BSE_editipo_types.h 2747 2004-07-05 09:15:02Z jesterking $
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

#ifndef BSE_EDITIPO_TYPES_H
#define BSE_EDITIPO_TYPES_H

struct BezTriple;

typedef struct IpoKey {
	struct IpoKey *next, *prev;
	short flag, rt;
	float val;
	struct BezTriple **data;
} IpoKey;

typedef struct EditIpo {
	char name[32];	// same length as keyblock->name
	IpoCurve *icu;
	short adrcode, flag;
	short disptype, rt;
	unsigned int col;
} EditIpo;

#endif /*  BSE_EDITIPO_TYPES_H */
