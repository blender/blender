/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

/* --- bpy build options --- */
#ifdef WITH_PYTHON_SAFETY

/**
 * Play it safe and keep optional for now,
 * need to test further now this affects looping on 10000's of verts for eg.
 */
#  define USE_WEAKREFS

/* method to invalidate removed py data, XXX, slow to remove objects, otherwise no overhead */
/* #define USE_PYRNA_INVALIDATE_GC */

/* different method */
#  define USE_PYRNA_INVALIDATE_WEAKREF

/* support for inter references, currently only needed for corner case */
#  define USE_PYRNA_STRUCT_REFERENCE

#else /* WITH_PYTHON_SAFETY */

/* default, no defines! */

#endif /* !WITH_PYTHON_SAFETY */

/* Sanity checks on above defines. */
#if defined(USE_PYRNA_INVALIDATE_WEAKREF) && !defined(USE_WEAKREFS)
#  define USE_WEAKREFS
#endif

#if defined(USE_PYRNA_INVALIDATE_GC) && defined(USE_PYRNA_INVALIDATE_WEAKREF)
#  error "Only 1 reference check method at a time!"
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sub-classes of #pyrna_struct_Type which support idprop definitions use this as a meta-class.
 * \note tp_base member is set to `&PyType_Type` on initialization.
 */
extern PyTypeObject pyrna_struct_meta_idprop_Type;
extern PyTypeObject pyrna_struct_Type;
extern PyTypeObject pyrna_prop_Type;
extern PyTypeObject pyrna_prop_array_Type;
extern PyTypeObject pyrna_prop_collection_Type;
extern PyTypeObject pyrna_func_Type;

#define BPy_StructRNA_Check(v) (PyObject_TypeCheck(v, &pyrna_struct_Type))
#define BPy_StructRNA_CheckExact(v) (Py_TYPE(v) == &pyrna_struct_Type)
#define BPy_PropertyRNA_Check(v) (PyObject_TypeCheck(v, &pyrna_prop_Type))
#define BPy_PropertyRNA_CheckExact(v) (Py_TYPE(v) == &pyrna_prop_Type)

#define PYRNA_STRUCT_CHECK_OBJ(obj) \
  if (UNLIKELY(pyrna_struct_validity_check(obj) == -1)) { \
    return NULL; \
  } \
  (void)0
#define PYRNA_STRUCT_CHECK_INT(obj) \
  if (UNLIKELY(pyrna_struct_validity_check(obj) == -1)) { \
    return -1; \
  } \
  (void)0

#define PYRNA_PROP_CHECK_OBJ(obj) \
  if (UNLIKELY(pyrna_prop_validity_check(obj) == -1)) { \
    return NULL; \
  } \
  (void)0
#define PYRNA_PROP_CHECK_INT(obj) \
  if (UNLIKELY(pyrna_prop_validity_check(obj) == -1)) { \
    return -1; \
  } \
  (void)0

#define PYRNA_STRUCT_IS_VALID(pysrna) (LIKELY(((BPy_StructRNA *)(pysrna))->ptr.type != NULL))
#define PYRNA_PROP_IS_VALID(pysrna) (LIKELY(((BPy_PropertyRNA *)(pysrna))->ptr.type != NULL))

/* 'in_weakreflist' MUST be aligned */

typedef struct {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif
  PointerRNA ptr;
} BPy_DummyPointerRNA;

typedef struct {
  PyObject_HEAD /* Required Python macro. */
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
  bool freeptr; /* needed in some cases if ptr.data is created on the fly, free when deallocing */
#endif          /* PYRNA_FREE_SUPPORT */
} BPy_StructRNA;

typedef struct {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif
  PointerRNA ptr;
  PropertyRNA *prop;
} BPy_PropertyRNA;

typedef struct {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif
  PointerRNA ptr;
  PropertyRNA *prop;

  /* Arystan: this is a hack to allow sub-item r/w access like: face.uv[n][m] */
  /** Array dimension, e.g: 0 for face.uv, 2 for face.uv[n][m], etc. */
  int arraydim;
  /** Array first item offset, e.g. if face.uv is [4][2], arrayoffset for face.uv[n] is 2n. */
  int arrayoffset;
} BPy_PropertyArrayRNA;

typedef struct {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif

  /* collection iterator specific parts */
  CollectionPropertyIterator iter;
} BPy_PropertyCollectionIterRNA;

typedef struct {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif
  PointerRNA ptr;
  FunctionRNA *func;
} BPy_FunctionRNA;

StructRNA *srna_from_self(PyObject *self, const char *error_prefix);
StructRNA *pyrna_struct_as_srna(PyObject *self, bool parent, const char *error_prefix);

void BPY_rna_init(void);
void BPY_rna_exit(void);
PyObject *BPY_rna_module(void);
void BPY_update_rna_module(void);
// PyObject *BPY_rna_doc(void);
PyObject *BPY_rna_types(void);

PyObject *pyrna_struct_CreatePyObject_with_primitive_support(PointerRNA *ptr);
PyObject *pyrna_struct_CreatePyObject(PointerRNA *ptr);
PyObject *pyrna_prop_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop);

/* extern'd by other modules which don't deal closely with RNA */
PyObject *pyrna_id_CreatePyObject(struct ID *id);
bool pyrna_id_FromPyObject(PyObject *obj, struct ID **id);
bool pyrna_id_CheckPyObject(PyObject *obj);

/* operators also need this to set args */
int pyrna_pydict_to_props(PointerRNA *ptr, PyObject *kw, bool all_args, const char *error_prefix);
PyObject *pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop);

int pyrna_deferred_register_class(struct StructRNA *srna, PyTypeObject *py_class);

const PointerRNA *pyrna_struct_as_ptr(PyObject *py_obj, const StructRNA *srna);
const PointerRNA *pyrna_struct_as_ptr_or_null(PyObject *py_obj, const StructRNA *srna);

/**
 * Struct used for RNA argument parsing functions:
 * - #pyrna_struct_as_ptr_parse
 * - #pyrna_struct_as_ptr_or_null_parse
 */
struct BPy_StructRNA_Parse {
  /** The struct RNA must match this type. */
  StructRNA *type;
  /** Result, may be `PointerRNA_NULL` if #pyrna_struct_as_ptr_or_null_parse is used. */
  const PointerRNA *ptr;
};

/**
 * Sets #BPy_StructRNA_Parse.ptr to the value in the #BPy_StructRNA.ptr (from `o`)
 * or raise an error if the type isn't a #BPy_StructRNA.
 *
 * Use with #PyArg_ParseTuple's `O&` formatting.
 */
int pyrna_struct_as_ptr_parse(PyObject *o, void *p);
/**
 * A version of #pyrna_struct_as_ptr_parse that maps Python's `None` to #PointerRNA_NULL.
 */
int pyrna_struct_as_ptr_or_null_parse(PyObject *o, void *p);

void pyrna_struct_type_extend_capi(struct StructRNA *srna,
                                   struct PyMethodDef *py_method,
                                   struct PyGetSetDef *py_getset);

/* Called before stopping Python. */

void pyrna_alloc_types(void);
void pyrna_free_types(void);

/* Primitive type conversion. */

int pyrna_py_to_array(
    PointerRNA *ptr, PropertyRNA *prop, char *param_data, PyObject *py, const char *error_prefix);
int pyrna_py_to_array_index(PointerRNA *ptr,
                            PropertyRNA *prop,
                            int arraydim,
                            int arrayoffset,
                            int index,
                            PyObject *py,
                            const char *error_prefix);
PyObject *pyrna_array_index(PointerRNA *ptr, PropertyRNA *prop, int index);

PyObject *pyrna_py_from_array(PointerRNA *ptr, PropertyRNA *prop);
PyObject *pyrna_py_from_array_index(BPy_PropertyArrayRNA *self,
                                    PointerRNA *ptr,
                                    PropertyRNA *prop,
                                    int index);
PyObject *pyrna_math_object_from_array(PointerRNA *ptr, PropertyRNA *prop);
int pyrna_array_contains_py(PointerRNA *ptr, PropertyRNA *prop, PyObject *value);

bool pyrna_write_check(void);
void pyrna_write_set(bool val);

void pyrna_invalidate(BPy_DummyPointerRNA *self);
int pyrna_struct_validity_check(BPy_StructRNA *pysrna);
int pyrna_prop_validity_check(BPy_PropertyRNA *self);

/* bpy.utils.(un)register_class */
extern PyMethodDef meth_bpy_register_class;
extern PyMethodDef meth_bpy_unregister_class;

/* bpy.utils._bl_owner_(get/set) */
extern PyMethodDef meth_bpy_owner_id_set;
extern PyMethodDef meth_bpy_owner_id_get;

extern BPy_StructRNA *bpy_context_module;

#ifdef __cplusplus
}
#endif
