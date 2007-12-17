/*
 * $Id: CurNurb.h 10269 2007-03-15 01:09:14Z campbellbarton $
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
 * Contributor(s): Stephen Swaney
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef EXPP_NURB_H
#define EXPP_NURB_H

#include <Python.h>
#include "DNA_curve_types.h"

extern PyTypeObject CurNurb_Type;

#define BPy_CurNurb_Check(v)  ((v)->ob_type == &CurNurb_Type)	/* for type checking */

/* Python BPy_CurNurb structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	Nurb * nurb;		/* pointer to Blender data */

	/* iterator stuff */
	/* internal ptrs to point data.  do not free */
	BPoint *bp;
	BezTriple *bezt;
	int atEnd;		/* iter exhausted flag  */
	int nextPoint;

} BPy_CurNurb;


/*
 *  prototypes
 */

PyObject *CurNurb_Init( void );
PyObject *CurNurb_CreatePyObject( Nurb * bzt );
Nurb *CurNurb_FromPyObject( PyObject * pyobj );

PyObject *CurNurb_getPoint( BPy_CurNurb * self, int index );
PyObject *CurNurb_pointAtIndex( Nurb * nurb, int index );

PyObject *CurNurb_appendPointToNurb( Nurb * nurb, PyObject * args );

#endif				/* EXPP_NURB_H */
