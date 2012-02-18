/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_rna.h
 *  \ingroup pythonintern
 */

#ifndef __BPY_RNA_H__
#define __BPY_RNA_H__

/* --- bpy build options --- */
#ifdef WITH_PYTHON_SAFETY

/* play it safe and keep optional for now, need to test further now this affects looping on 10000's of verts for eg. */
#define USE_WEAKREFS

/* method to invalidate removed py data, XXX, slow to remove objects, otherwise no overhead */
/* #define USE_PYRNA_INVALIDATE_GC */

/* different method */
#define USE_PYRNA_INVALIDATE_WEAKREF

/* support for inter references, currently only needed for corner case */
#define USE_PYRNA_STRUCT_REFERENCE

#else /* WITH_PYTHON_SAFETY */

 /* default, no defines! */

#endif /* !WITH_PYTHON_SAFETY */


/* sanity checks on above defs */
#if defined(USE_PYRNA_INVALIDATE_WEAKREF) && !defined(USE_WEAKREFS)
#define USE_WEAKREFS
#endif

#if defined(USE_PYRNA_INVALIDATE_GC) && defined(USE_PYRNA_INVALIDATE_WEAKREF)
#error "Only 1 reference check method at a time!"
#endif

/* only used by operator introspection get_rna(), this is only used for doc gen
 * so prefer the leak to the memory bloat for now. */
// #define PYRNA_FREE_SUPPORT

/* use real collection iterators rather than faking with a list
 * this is needed so enums can be iterated over without crashing,
 * since finishing the iteration frees temp allocated enums */
#define USE_PYRNA_ITER

/* --- end bpy build options --- */

struct ID;

extern PyTypeObject pyrna_struct_meta_idprop_Type;
extern PyTypeObject pyrna_struct_Type;
extern PyTypeObject pyrna_prop_Type;
extern PyTypeObject pyrna_prop_array_Type;
extern PyTypeObject pyrna_prop_collection_Type;
extern PyTypeObject pyrna_func_Type;

#define BPy_StructRNA_Check(v)			(PyObject_TypeCheck(v, &pyrna_struct_Type))
#define BPy_StructRNA_CheckExact(v)		(Py_TYPE(v) == &pyrna_struct_Type)
#define BPy_PropertyRNA_Check(v)		(PyObject_TypeCheck(v, &pyrna_prop_Type))
#define BPy_PropertyRNA_CheckExact(v)	(Py_TYPE(v) == &pyrna_prop_Type)

#define PYRNA_STRUCT_CHECK_OBJ(obj) if(pyrna_struct_validity_check(obj) == -1) { return NULL; }
#define PYRNA_STRUCT_CHECK_INT(obj) if(pyrna_struct_validity_check(obj) == -1) { return -1; }

#define PYRNA_PROP_CHECK_OBJ(obj) if(pyrna_prop_validity_check(obj) == -1) { return NULL; }
#define PYRNA_PROP_CHECK_INT(obj) if(pyrna_prop_validity_check(obj) == -1) { return -1; }

#define PYRNA_STRUCT_IS_VALID(pysrna) (((BPy_StructRNA *)(pysrna))->ptr.type != NULL)
#define PYRNA_PROP_IS_VALID(pysrna) (((BPy_PropertyRNA *)(pysrna))->ptr.type != NULL)

/* 'in_weakreflist' MUST be aligned */

typedef struct {
	PyObject_HEAD /* required python macro   */
#ifdef USE_WEAKREFS
	PyObject *in_weakreflist;
#endif
	PointerRNA	ptr;
} BPy_DummyPointerRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
#ifdef USE_WEAKREFS
	PyObject *in_weakreflist;
#endif
	PointerRNA ptr;
#ifdef USE_PYRNA_STRUCT_REFERENCE
	/* generic PyObject we hold a reference to, example use:
	 * hold onto the collection iterator to prevent it from freeing allocated data we may use */
	PyObject *reference;
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

#ifdef PYRNA_FREE_SUPPORT
	int freeptr; /* needed in some cases if ptr.data is created on the fly, free when deallocing */
#endif /* PYRNA_FREE_SUPPORT */
} BPy_StructRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
#ifdef USE_WEAKREFS
	PyObject *in_weakreflist;
#endif
	PointerRNA ptr;
	PropertyRNA *prop;
} BPy_PropertyRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
#ifdef USE_WEAKREFS
	PyObject *in_weakreflist;
#endif
	PointerRNA ptr;
	PropertyRNA *prop;

	/* Arystan: this is a hack to allow sub-item r/w access like: face.uv[n][m] */
	int arraydim; /* array dimension, e.g: 0 for face.uv, 2 for face.uv[n][m], etc. */
	int arrayoffset; /* array first item offset, e.g. if face.uv is [4][2], arrayoffset for face.uv[n] is 2n */
} BPy_PropertyArrayRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
#ifdef USE_WEAKREFS
	PyObject *in_weakreflist;
#endif

	/* collection iterator specific parts */
	CollectionPropertyIterator iter;
} BPy_PropertyCollectionIterRNA;

typedef struct {
	PyObject_HEAD /* required python macro   */
#ifdef USE_WEAKREFS
	PyObject *in_weakreflist;
#endif
	PointerRNA ptr;
	FunctionRNA *func;
} BPy_FunctionRNA;

/* cheap trick */
#define BPy_BaseTypeRNA BPy_PropertyRNA

StructRNA *srna_from_self(PyObject *self, const char *error_prefix);
StructRNA *pyrna_struct_as_srna(PyObject *self, int parent, const char *error_prefix);

void      BPY_rna_init(void);
PyObject *BPY_rna_module(void);
void	  BPY_update_rna_module(void);
/*PyObject *BPY_rna_doc(void);*/
PyObject *BPY_rna_types(void);

PyObject *pyrna_struct_CreatePyObject(PointerRNA *ptr);
PyObject *pyrna_prop_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop);

/* operators also need this to set args */
int pyrna_pydict_to_props(PointerRNA *ptr, PyObject *kw, int all_args, const char *error_prefix);
PyObject *pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop);

PyObject *pyrna_enum_bitfield_to_py(struct EnumPropertyItem *items, int value);
int pyrna_set_to_enum_bitfield(EnumPropertyItem *items, PyObject *value, int *r_value, const char *error_prefix);

int pyrna_enum_value_from_id(EnumPropertyItem *item, const char *identifier, int *value, const char *error_prefix);

int pyrna_deferred_register_class(struct StructRNA *srna, PyObject *py_class);

/* called before stopping python */
void pyrna_alloc_types(void);
void pyrna_free_types(void);

/* primitive type conversion */
int pyrna_py_to_array(PointerRNA *ptr, PropertyRNA *prop, char *param_data, PyObject *py, const char *error_prefix);
int pyrna_py_to_array_index(PointerRNA *ptr, PropertyRNA *prop, int arraydim, int arrayoffset, int index, PyObject *py, const char *error_prefix);
PyObject *pyrna_array_index(PointerRNA *ptr, PropertyRNA *prop, int index);

PyObject *pyrna_py_from_array(PointerRNA *ptr, PropertyRNA *prop);
PyObject *pyrna_py_from_array_index(BPy_PropertyArrayRNA *self, PointerRNA *ptr, PropertyRNA *prop, int index);
PyObject *pyrna_math_object_from_array(PointerRNA *ptr, PropertyRNA *prop);
int pyrna_array_contains_py(PointerRNA *ptr, PropertyRNA *prop, PyObject *value);

int pyrna_write_check(void);
void pyrna_write_set(int val);

int pyrna_struct_validity_check(BPy_StructRNA *pysrna);
int pyrna_prop_validity_check(BPy_PropertyRNA *self);

/* bpy.utils.(un)register_class */
extern PyMethodDef meth_bpy_register_class;
extern PyMethodDef meth_bpy_unregister_class;

#endif
