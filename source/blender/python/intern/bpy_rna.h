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

#define BPy_StructRNA_Check(v)			(PyObject_TypeCheck(v, &pyrna_struct_Type))
#define BPy_StructRNA_CheckExact(v)		(Py_TYPE(v) == &pyrna_struct_Type)
#define BPy_PropertyRNA_Check(v)		(PyObject_TypeCheck(v, &pyrna_prop_Type))
#define BPy_PropertyRNA_CheckExact(v)	(Py_TYPE(v) == &pyrna_prop_Type)

typedef struct {
	PyObject_HEAD /* required python macro   */
	PointerRNA ptr;
} BPy_DummyPointerRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
	PointerRNA ptr;
	int freeptr; /* needed in some cases if ptr.data is created on the fly, free when deallocing */
} BPy_StructRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
	PointerRNA ptr;
	PropertyRNA *prop;

	/* Arystan: this is a hack to allow sub-item r/w access like: face.uv[n][m] */
	int arraydim; /* array dimension, e.g: 0 for face.uv, 2 for face.uv[n][m], etc. */
	int arrayoffset; /* array first item offset, e.g. if face.uv is [4][2], arrayoffset for face.uv[n] is 2n */
} BPy_PropertyRNA;

/* cheap trick */
#define BPy_BaseTypeRNA BPy_PropertyRNA

StructRNA *srna_from_self(PyObject *self);
StructRNA *pyrna_struct_as_srna(PyObject *self);

void      BPY_rna_init( void );
PyObject *BPY_rna_module( void );
void	  BPY_update_rna_module( void );
/*PyObject *BPY_rna_doc( void );*/
PyObject *BPY_rna_types( void );

PyObject *pyrna_struct_CreatePyObject( PointerRNA *ptr );
PyObject *pyrna_prop_CreatePyObject( PointerRNA *ptr, PropertyRNA *prop );

/* operators also need this to set args */
int pyrna_py_to_prop(PointerRNA *ptr, PropertyRNA *prop, void *data, PyObject *value, const char *error_prefix);
int pyrna_pydict_to_props(PointerRNA *ptr, PyObject *kw, int all_args, const char *error_prefix);
PyObject * pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop);

/* function for registering types */
PyObject *pyrna_basetype_register(PyObject *self, PyObject *args);
PyObject *pyrna_basetype_unregister(PyObject *self, PyObject *args);

int pyrna_deferred_register_props(struct StructRNA *srna, PyObject *class_dict);

/* called before stopping python */
void pyrna_alloc_types(void);
void pyrna_free_types(void);

/* primitive type conversion */
int pyrna_py_to_array(PointerRNA *ptr, PropertyRNA *prop, char *param_data, PyObject *py, const char *error_prefix);
int pyrna_py_to_array_index(PointerRNA *ptr, PropertyRNA *prop, int arraydim, int arrayoffset, int index, PyObject *py, const char *error_prefix);

PyObject *pyrna_py_from_array(PointerRNA *ptr, PropertyRNA *prop);
PyObject *pyrna_py_from_array_index(BPy_PropertyRNA *self, int index);
PyObject *pyrna_math_object_from_array(PointerRNA *ptr, PropertyRNA *prop);
int pyrna_array_contains_py(PointerRNA *ptr, PropertyRNA *prop, PyObject *value);

#endif
