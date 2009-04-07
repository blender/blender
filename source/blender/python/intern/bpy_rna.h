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
#ifndef BPY_RNA_H
#define BPY_RNA_H

#include <Python.h>

#include "RNA_access.h"
#include "RNA_types.h"
#include "BKE_idprop.h"

extern PyTypeObject pyrna_struct_Type;
extern PyTypeObject pyrna_prop_Type;
extern PyTypeObject pyrna_func_Type;

#define BPy_StructRNA_Check(v)			(PyObject_TypeCheck(v, &pyrna_struct_Type))
#define BPy_StructRNA_CheckExact(v)		(Py_TYPE(v) == &pyrna_struct_Type)
#define BPy_PropertyRNA_Check(v)		(PyObject_TypeCheck(v, &pyrna_prop_Type))
#define BPy_PropertyRNA_CheckExact(v)	(Py_TYPE(v) == &pyrna_prop_Type)
#define BPy_FunctionRNA_Check(v)		(PyObject_TypeCheck(v, &pyrna_func_Type))
#define BPy_FunctionRNA_CheckExact(v)	(Py_TYPE(v) == &pyrna_func_Type)

typedef struct {
	void * _a;
	void * _b;
	PyTypeObject *py_type;
} BPy_StructFakeType;


typedef struct {
	PyObject_HEAD /* required python macro   */
	PointerRNA ptr;
	int freeptr; /* needed in some cases if ptr.data is created on the fly, free when deallocing */
} BPy_StructRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
	PointerRNA ptr;
	PropertyRNA *prop;
} BPy_PropertyRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
	PointerRNA ptr;
	FunctionRNA *func;
} BPy_FunctionRNA;

/* cheap trick */
#define BPy_BaseTypeRNA BPy_PropertyRNA

PyObject *BPY_rna_module( void );
/*PyObject *BPY_rna_doc( void );*/
PyObject *BPY_rna_types( void );

PyObject *pyrna_struct_CreatePyObject( PointerRNA *ptr );
PyObject *pyrna_prop_CreatePyObject( PointerRNA *ptr, PropertyRNA *prop );
PyObject *pyrna_func_CreatePyObject( PointerRNA *ptr, FunctionRNA *func );

/* operators also need this to set args */
int pyrna_py_to_prop(PointerRNA *ptr, PropertyRNA *prop, PyObject *value);
PyObject * pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop);

PyObject *pyrna_func_to_py( PointerRNA *ptr, FunctionRNA *func );

/* functions for setting up new props - experemental */
PyObject *BPy_FloatProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_IntProperty(PyObject *self, PyObject *args, PyObject *kw);
PyObject *BPy_BoolProperty(PyObject *self, PyObject *args, PyObject *kw);


#endif
