/* 
 * $Id: Lamp.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * Contributor(s): Willian P. Germano, Nathan Letwory
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_LAMP_H
#define EXPP_LAMP_H

#include <Python.h>
#include "DNA_lamp_types.h"
#include "rgbTuple.h"

extern PyTypeObject Lamp_Type;

#define BPy_Lamp_Check(v) \
    ((v)->ob_type == &Lamp_Type)	/* for type checking */

/* Python BPy_Lamp structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	Lamp * lamp;		/* libdata must be second */
	BPy_rgbTuple *color;
} BPy_Lamp;


/*
 *   prototypes
 */

PyObject *Lamp_Init( void );
PyObject *Lamp_CreatePyObject( struct Lamp *lamp );
Lamp *Lamp_FromPyObject( PyObject * pyobj );

#endif				/* EXPP_LAMP_H */
