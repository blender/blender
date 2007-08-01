/* 
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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/* The code in Draw.[ch] and BGL.[ch] comes from opy_draw.c in the old
 * bpython/intern dir, with minor modifications to suit the current
 * implementation.  Important original comments are marked with an @ symbol. */

#ifndef EXPP_DRAW_H_
#define EXPP_DRAW_H_

#include <Python.h>
#include "DNA_space_types.h"
#include "DNA_text_types.h"

void initDraw( void );

/* 
 * Button Object stuct 
 */

typedef struct _Button {
	PyObject_VAR_HEAD	/* required Py Macro */
	int type;		/*@ 1 == int, 2 == float, 3 == string */
	unsigned int slen;	/*@ length of string (if type == 3) */
	union {
		int asint;
		float asfloat;
		char *asstr;
		float asvec[3];
	} val;
	char tooltip[256];
} Button;

#define BPY_MAX_TOOLTIP	255

#define BINT_TYPE			1
#define BFLOAT_TYPE			2
#define BSTRING_TYPE		3
#define BVECTOR_TYPE		4

/* 
 * these are declared in ../BPY_extern.h 
*/

PyObject *M_Draw_Init( void );
PyObject *Draw_Init( void );

#endif				/* EXPP_DRAW_H */
