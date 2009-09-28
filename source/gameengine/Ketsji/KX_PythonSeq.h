/**
 * $Id: 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 * Readonly sequence wrapper for lookups on logic bricks
 */
 
#ifndef _adr_py_seq_h_				// only process once,
#define _adr_py_seq_h_				// even if multiply included

#include "PyObjectPlus.h"

// -------------------------
enum KX_PYGENSEQ_TYPE {
	KX_PYGENSEQ_CONT_TYPE_SENSORS,
	KX_PYGENSEQ_CONT_TYPE_ACTUATORS,
	KX_PYGENSEQ_OB_TYPE_SENSORS,
	KX_PYGENSEQ_OB_TYPE_CONTROLLERS,
	KX_PYGENSEQ_OB_TYPE_ACTUATORS,
	KX_PYGENSEQ_OB_TYPE_CONSTRAINTS,
	KX_PYGENSEQ_OB_TYPE_CHANNELS,
};

/* The Main PyType Object defined in Main.c */
extern PyTypeObject KX_PythonSeq_Type;

#define BPy_KX_PythonSeq_Check(v) \
    ((v)->ob_type == &KX_PythonSeq_Type)

typedef struct {
	PyObject_VAR_HEAD
	PyObject *base;
	short type;
	short iter;
} KX_PythonSeq;

PyObject *KX_PythonSeq_CreatePyObject(PyObject *base, short type);

#endif // _adr_py_seq_h_
