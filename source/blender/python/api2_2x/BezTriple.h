/* 
 * $Id: BezTriple.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * Contributor(s): Jacques Guignot, Stephen Swaney
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_BEZTRIPLE_H
#define EXPP_BEZTRIPLE_H

#include <Python.h>
#include "DNA_curve_types.h"

extern PyTypeObject BezTriple_Type;

/* type check macro */
#define BPy_BezTriple_Check(o) \
 ( (o)->ob_type == &BezTriple_Type)

/****************************************************************************
 Python BPy_BezTriple structure definition:              
****************************************************************************/

typedef struct {
	PyObject_HEAD		/* required python macro */
	BezTriple * beztriple;
	int own_memory;   /* true == we own this memory and must delete. */
} BPy_BezTriple;

/*
 *    prototypes
 */

PyObject *BezTriple_CreatePyObject( BezTriple * bzt );
BezTriple *BezTriple_FromPyObject( PyObject * pyobj );
PyObject *newBezTriple( float *args );
PyObject *BezTriple_Init( void );

#endif				/* EXPP_BEZTRIPLE_H */
