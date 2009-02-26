
/**
 * $Id$
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BPY_OPERATOR_H
#define BPY_OPERATOR_H

#include <Python.h>

#include "RNA_access.h"
#include "RNA_types.h"
#include "DNA_windowmanager_types.h"
#include "BKE_context.h"

extern PyTypeObject pyop_base_Type;
extern PyTypeObject pyop_func_Type;

#define BPy_OperatorFunc_Check(v)	(PyObject_TypeCheck(v, &pyop_func_Type))
#define BPy_PropertyRNA_Check(v)	(PyObject_TypeCheck(v, &pyop_func_Type))

typedef struct {
	PyObject_HEAD /* required python macro   */
	bContext *C;
} BPy_OperatorBase;

typedef struct {
	PyObject_HEAD /* required python macro   */
	char name[OP_MAX_TYPENAME];
	bContext *C;
} BPy_OperatorFunc;

PyObject *BPY_operator_module(bContext *C );

PyObject *pyop_base_CreatePyObject(bContext *C );
PyObject *pyop_func_CreatePyObject(bContext *C, char *name );

/* fill in properties from a python dict */
int PYOP_props_from_dict(PointerRNA *ptr, PyObject *kw);

#endif
