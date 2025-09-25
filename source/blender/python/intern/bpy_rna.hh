/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#include <Python.h>

#include <optional>

/* --- bpy build options --- */
#include "intern/rna_internal_types.hh"
#ifdef WITH_PYTHON_SAFETY

/**
 * Play it safe and keep optional for now,
 * need to test further now this affects looping on 10000's of verts for eg.
 */
#  define USE_WEAKREFS

/* method to invalidate removed py data, XXX, slow to remove objects, otherwise no overhead */
// #define USE_PYRNA_INVALIDATE_GC

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

#define PYRNA_STRUCT_CHECK_OBJ_UNLESS(obj, unless) \
  { \
    const BPy_StructRNA *_obj = obj; \
    if (UNLIKELY(pyrna_struct_validity_check_only(_obj) == -1) && !(unless)) { \
      pyrna_struct_validity_exception_only(_obj); \
      return NULL; \
    } \
  } \
  (void)0

#define PYRNA_STRUCT_IS_VALID(pysrna) (LIKELY(((BPy_StructRNA *)(pysrna))->ptr->type != NULL))
#define PYRNA_PROP_IS_VALID(pysrna) (LIKELY(((BPy_PropertyRNA *)(pysrna))->ptr->type != NULL))

/* 'in_weakreflist' MUST be aligned */

struct BPy_DummyPointerRNA {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif

  std::optional<PointerRNA> ptr;
};

struct BPy_StructRNA {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif

  std::optional<PointerRNA> ptr;

#ifdef USE_PYRNA_STRUCT_REFERENCE
  /* generic PyObject we hold a reference to, example use:
   * hold onto the collection iterator to prevent it from freeing allocated data we may use */
  PyObject *reference;
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

#ifdef PYRNA_FREE_SUPPORT
  /** Needed in some cases if ptr.data is created on the fly, free when deallocating. */
  bool freeptr;
#endif /* PYRNA_FREE_SUPPORT */
};

struct BPy_PropertyRNA {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif

  std::optional<PointerRNA> ptr;
  PropertyRNA *prop;
};

struct BPy_PropertyArrayRNA {
  PyObject_HEAD /* Required Python macro. */

  /* START Must match #BPy_PropertyRNA. */

#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif

  std::optional<PointerRNA> ptr;
  PropertyRNA *prop;

  /* END Must match #BPy_PropertyRNA. */

  /* Arystan: this is a hack to allow sub-item r/w access like: face.uv[n][m] */
  /** Array dimension, e.g: 0 for face.uv, 2 for face.uv[n][m], etc. */
  int arraydim;
  /** Array first item offset, e.g. if face.uv is [4][2], arrayoffset for face.uv[n] is 2n. */
  int arrayoffset;
};

struct BPy_PropertyCollectionIterRNA {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif

  /* collection iterator specific parts */
  std::optional<CollectionPropertyIterator> iter;
};

struct BPy_FunctionRNA {
  PyObject_HEAD /* Required Python macro. */
#ifdef USE_WEAKREFS
  PyObject *in_weakreflist;
#endif

  std::optional<PointerRNA> ptr;
  FunctionRNA *func;
  /**
   * Instance call only. This is *always* set to `pyrna_func_vectorcall`.
   * Storing this value is required by the Python C-API (PEP 590).
   */
  vectorcallfunc vectorcall;
};

[[nodiscard]] StructRNA *srna_from_self(PyObject *self, const char *error_prefix);
[[nodiscard]] StructRNA *pyrna_struct_as_srna(PyObject *self,
                                              bool parent,
                                              const char *error_prefix);

void BPY_rna_init();
void BPY_rna_exit();
[[nodiscard]] PyObject *BPY_rna_module();
void BPY_update_rna_module();
// PyObject *BPY_rna_doc();
[[nodiscard]] PyObject *BPY_rna_types();
/**
 * Set the `_bpy_types.py` modules `__dict__`, needed for instancing RNA types.
 */
void BPY_rna_types_dict_set(PyObject *dict);
void BPY_rna_types_finalize_external_types(PyObject *submodule);

[[nodiscard]] PyObject *pyrna_struct_CreatePyObject_with_primitive_support(PointerRNA *ptr);
[[nodiscard]] PyObject *pyrna_struct_CreatePyObject(PointerRNA *ptr);
[[nodiscard]] PyObject *pyrna_prop_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop);

/* Made public for other modules which don't deal closely with RNA. */
[[nodiscard]] PyObject *pyrna_id_CreatePyObject(ID *id);
[[nodiscard]] bool pyrna_id_FromPyObject(PyObject *obj, ID **id);
[[nodiscard]] bool pyrna_id_CheckPyObject(PyObject *obj);

/* operators also need this to set args */
[[nodiscard]] int pyrna_pydict_to_props(PointerRNA *ptr,
                                        PyObject *kw,
                                        bool all_args,
                                        const char *error_prefix);
[[nodiscard]] PyObject *pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop);

[[nodiscard]] int pyrna_deferred_register_class(StructRNA *srna, PyTypeObject *py_class);

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
[[nodiscard]] int pyrna_struct_as_ptr_parse(PyObject *o, void *p);
/**
 * A version of #pyrna_struct_as_ptr_parse that maps Python's `None` to #PointerRNA_NULL.
 */
[[nodiscard]] int pyrna_struct_as_ptr_or_null_parse(PyObject *o, void *p);

void pyrna_struct_type_extend_capi(StructRNA *srna, PyMethodDef *method, PyGetSetDef *getset);

void pyrna_alloc_types();

/* Primitive type conversion. */

[[nodiscard]] int pyrna_py_to_array(
    PointerRNA *ptr, PropertyRNA *prop, char *param_data, PyObject *py, const char *error_prefix);
[[nodiscard]] int pyrna_py_to_array_index(PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          int arraydim,
                                          int arrayoffset,
                                          int index,
                                          PyObject *py,
                                          const char *error_prefix);
[[nodiscard]] PyObject *pyrna_array_index(PointerRNA *ptr, PropertyRNA *prop, int index);

[[nodiscard]] PyObject *pyrna_py_from_array(PointerRNA *ptr, PropertyRNA *prop);
[[nodiscard]] PyObject *pyrna_py_from_array_index(BPy_PropertyArrayRNA *self,
                                                  PointerRNA *ptr,
                                                  PropertyRNA *prop,
                                                  int index);
[[nodiscard]] PyObject *pyrna_math_object_from_array(PointerRNA *ptr, PropertyRNA *prop);
[[nodiscard]] int pyrna_array_contains_py(PointerRNA *ptr, PropertyRNA *prop, PyObject *value);

[[nodiscard]] bool pyrna_write_check();
void pyrna_write_set(bool val);

void pyrna_invalidate(BPy_DummyPointerRNA *self);

[[nodiscard]] int pyrna_struct_validity_check_only(const BPy_StructRNA *pysrna);
void pyrna_struct_validity_exception_only(const BPy_StructRNA *pysrna);
[[nodiscard]] int pyrna_struct_validity_check(const BPy_StructRNA *pysrna);

[[nodiscard]] int pyrna_prop_validity_check(const BPy_PropertyRNA *self);

/* bpy.utils.(un)register_class */
extern PyMethodDef meth_bpy_register_class;
extern PyMethodDef meth_bpy_unregister_class;

/* bpy.utils._bl_owner_(get/set) */
extern PyMethodDef meth_bpy_owner_id_set;
extern PyMethodDef meth_bpy_owner_id_get;

extern BPy_StructRNA *bpy_context_module;
