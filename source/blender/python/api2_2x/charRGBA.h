/* 
 * $Id: charRGBA.h 10280 2007-03-16 14:44:31Z campbellbarton $
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

#ifndef EXPP_charRGBA_H
#define EXPP_charRGBA_H

#include <Python.h>

/* Objects of <type 'charRGBA'> are used inside other Blender Python
 * objects, so this header file must contain only 'public' declarations */
/* there's also rgbTuple, for rgb floats and a color obj in NMesh, messy! */

/* Python BPy_charRGBA structure definition: */

typedef struct {
	PyObject_HEAD 
	char *rgba[4];	/* array of four pointers to chars */

} BPy_charRGBA;

extern PyTypeObject charRGBA_Type;

/*****************************************************************************/
/* Python API function prototypes for the charRGBA helper module.            */
/*****************************************************************************/
PyObject *charRGBA_New( char *rgba );
PyObject *charRGBA_getCol( BPy_charRGBA * self );
PyObject *charRGBA_setCol( BPy_charRGBA * self, PyObject * args );

#endif				/* EXPP_charRGBA_H */
