/**
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

#ifndef TRANSFORM_NUMINPUT_H
#define TRANSFORM_NUMINPUT_H

typedef struct NumInput {
    short  idx;
    short  idx_max;
    short  flags;        /* Different flags to indicate different behaviors                                */
    float  val[3];       /* Direct value of the input                                                      */
    short  ctrl[3];      /* Control to indicate what to do with the numbers that are typed                 */
} NumInput ;

/*
	The ctrl value has different meaning:
		0			: No value has been typed
		
		otherwise, |value| - 1 is where the cursor is located after the period
		Positive	: number is positive
		Negative	: number is negative
*/

void outputNumInput(NumInput *n, char *str);

short hasNumInput(NumInput *n);

void applyNumInput(NumInput *n, float *vec);

char handleNumInput(NumInput *n, unsigned short event);

#endif

