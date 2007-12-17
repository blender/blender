/* 
 * $Id: Metaball.h 10269 2007-03-15 01:09:14Z campbellbarton $
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

#ifndef EXPP_METABALL_H
#define EXPP_METABALL_H

#include <Python.h>
#include "DNA_meta_types.h"


extern PyTypeObject Metaball_Type;

#define BPy_Metaball_Check(v) ((v)->ob_type==&Metaball_Type)


/* Python BPy_Metaball structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	MetaBall * metaball; /* libdata must be second */
} BPy_Metaball;


extern PyTypeObject Metaelem_Type;

#define BPy_Metaelem_Check(v) ((v)->ob_type==&Metaelem_Type)

/* Python BPy_Metaelem structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	MetaElem * metaelem;
} BPy_Metaelem;

extern PyTypeObject MetaElemSeq_Type;

#define BPy_MetaElemSeq_Check(v) ((v)->ob_type==&MetaElemSeq_Type)

/* Python BPy_MetaElemSeq structure definition */
typedef struct {
	PyObject_HEAD		/* required py macro */
	BPy_Metaball *bpymetaball; /* link to the python group so we can know if its been removed */
	MetaElem * iter; /* so we can iterate over the objects */
} BPy_MetaElemSeq;

/*
 * prototypes
 */

PyObject *Metaball_Init( void );
PyObject *Metaball_CreatePyObject( MetaBall * mball );
MetaBall *Metaball_FromPyObject( PyObject * py_obj );

#endif				/* EXPP_METABALL_H */
