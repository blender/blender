/**
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
 
#include <string.h>
#include <stdio.h>

#ifndef BOP_TAG_H
#define BOP_TAG_H

#define IN_TAG           0x02  // Below the plane
#define ON_TAG           0x00  // On the plane
#define OUT_TAG          0x01  // Above the plane  
#define INOUT_TAG        0x0E  // Above and below the plane  
#define INON_TAG         0x12  // Below and on the plane
#define OUTON_TAG        0x11  // Above and on the plane  
#define UNCLASSIFIED_TAG 0x0F  // Expecting to be classified

#define PHANTOM_TAG      0x0C  // Phantom face: verts form collinear triangle
#define OVERLAPPED_TAG   0x0D  // Overlapped face
#define BROKEN_TAG       0x0B  // Splitted and unused ... 

#define ON_ON_IN_TAG     IN_TAG
#define ON_IN_ON_TAG     IN_TAG << 2
#define IN_ON_ON_TAG     IN_TAG << 4

#define ON_ON_OUT_TAG    OUT_TAG
#define ON_OUT_ON_TAG    OUT_TAG << 2
#define OUT_ON_ON_TAG    OUT_TAG << 4

#define ON_ON_ON_TAG     ON_TAG
#define IN_IN_IN_TAG     IN_ON_ON_TAG | ON_IN_ON_TAG | ON_ON_IN_TAG
#define OUT_OUT_OUT_TAG  OUT_ON_ON_TAG | ON_OUT_ON_TAG | ON_ON_OUT_TAG

#define IN_IN_ON_TAG     IN_ON_ON_TAG | ON_IN_ON_TAG
#define IN_ON_IN_TAG     IN_ON_ON_TAG | ON_ON_IN_TAG
#define ON_IN_IN_TAG     ON_IN_ON_TAG | ON_ON_IN_TAG

#define OUT_OUT_ON_TAG   OUT_ON_ON_TAG | ON_OUT_ON_TAG
#define OUT_ON_OUT_TAG   OUT_ON_ON_TAG | ON_ON_OUT_TAG
#define ON_OUT_OUT_TAG   ON_OUT_ON_TAG | ON_ON_OUT_TAG

#define IN_OUT_OUT_TAG   IN_ON_ON_TAG | ON_OUT_OUT_TAG
#define OUT_IN_OUT_TAG   ON_IN_ON_TAG | OUT_ON_OUT_TAG
#define OUT_OUT_IN_TAG   ON_ON_IN_TAG | OUT_OUT_ON_TAG

#define OUT_IN_IN_TAG    ON_IN_IN_TAG | OUT_ON_ON_TAG
#define IN_OUT_IN_TAG    IN_ON_IN_TAG | ON_OUT_ON_TAG
#define IN_IN_OUT_TAG    IN_IN_ON_TAG | ON_ON_OUT_TAG

#define IN_ON_OUT_TAG    IN_ON_ON_TAG | ON_ON_OUT_TAG
#define IN_OUT_ON_TAG    IN_ON_ON_TAG | ON_OUT_ON_TAG
#define ON_IN_OUT_TAG    ON_IN_ON_TAG | ON_ON_OUT_TAG
#define ON_OUT_IN_TAG    ON_ON_IN_TAG | ON_OUT_ON_TAG
#define OUT_IN_ON_TAG    ON_IN_ON_TAG | OUT_ON_ON_TAG
#define OUT_ON_IN_TAG    ON_ON_IN_TAG | OUT_ON_ON_TAG

typedef enum BOP_TAGEnum {
	IN           = IN_TAG,
	ON           = ON_TAG,
	OUT          = OUT_TAG,
	INOUT        = INOUT_TAG,
	INON         = INON_TAG,
	OUTON        = OUTON_TAG,
	UNCLASSIFIED = UNCLASSIFIED_TAG,
	PHANTOM      = PHANTOM_TAG,
	OVERLAPPED   = OVERLAPPED_TAG,
	BROKEN       = BROKEN_TAG,
	IN_ON_ON     = IN_ON_ON_TAG,
	ON_IN_ON     = ON_IN_ON_TAG,
	ON_ON_IN     = ON_ON_IN_TAG,
	OUT_ON_ON    = OUT_ON_ON_TAG,
	ON_OUT_ON    = ON_OUT_ON_TAG,
	ON_ON_OUT    = ON_ON_OUT_TAG,
	ON_ON_ON     = ON_ON_ON_TAG,
	IN_IN_IN     = IN_IN_IN_TAG,
	OUT_OUT_OUT  = OUT_OUT_OUT_TAG,
	IN_IN_ON     = IN_IN_ON_TAG,
	IN_ON_IN     = IN_ON_IN_TAG,
	ON_IN_IN     = ON_IN_IN_TAG,
	OUT_OUT_ON   = OUT_OUT_ON_TAG,
	OUT_ON_OUT   = OUT_ON_OUT_TAG,
	ON_OUT_OUT   = ON_OUT_OUT_TAG,
	IN_OUT_OUT   = IN_OUT_OUT_TAG,
	OUT_IN_OUT   = OUT_IN_OUT_TAG,
	OUT_OUT_IN   = OUT_OUT_IN_TAG,
	OUT_IN_IN    = OUT_IN_IN_TAG,
	IN_OUT_IN    = IN_OUT_IN_TAG,
	IN_IN_OUT    = IN_IN_OUT_TAG,
	IN_ON_OUT    = IN_ON_OUT_TAG,
	IN_OUT_ON    = IN_OUT_ON_TAG,
	ON_IN_OUT    = ON_IN_OUT_TAG,
	ON_OUT_IN    = ON_OUT_IN_TAG,
	OUT_IN_ON    = OUT_IN_ON_TAG,
	OUT_ON_IN    = OUT_ON_IN_TAG } BOP_TAG;

inline BOP_TAG BOP_createTAG(BOP_TAG tag1, BOP_TAG tag2, BOP_TAG tag3)
{
	return (BOP_TAG) (tag1 << 4 | tag2 << 2 | tag3); 
}
	
inline BOP_TAG BOP_createTAG(int i)
{
	return i < 0 ? IN : i > 0 ? OUT : ON;
}

inline BOP_TAG BOP_addON(BOP_TAG tag)
{
  return (tag==IN?INON:(tag==OUT?OUTON:tag));
}

void BOP_stringTAG(BOP_TAG tag, char *dest);

inline bool BOP_compTAG(BOP_TAG tag1, BOP_TAG tag2)
{
  return (tag1==tag2) || (BOP_addON(tag1) == BOP_addON(tag2));
}

#endif
