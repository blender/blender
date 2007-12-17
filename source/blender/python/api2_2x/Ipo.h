/* 
 * $Id: Ipo.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * Contributor(s): Jacques Guignot
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_IPO_H
#define EXPP_IPO_H

#include <Python.h>
#include "DNA_ipo_types.h"

/*****************************************************************************/
/* Python BPy_Ipo structure definition:                                     */
/*****************************************************************************/
typedef struct {
	PyObject_HEAD		/* required macro */
	Ipo * ipo;		/* libdata must be second */
	short iter;
	short mtex; 
} BPy_Ipo;

extern PyTypeObject Ipo_Type;

#define BPy_Ipo_Check(v)  ((v)->ob_type == &Ipo_Type)	/* for type checking */


/* 
 *   prototypes
 */

PyObject *Ipo_Init( void );
PyObject *Ipo_CreatePyObject( struct Ipo *ipo );
Ipo *Ipo_FromPyObject( PyObject * py_obj );


#endif				/* EXPP_IPO_H */
