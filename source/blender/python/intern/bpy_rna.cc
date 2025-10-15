/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file is the main interface between Python and Blender's data API (RNA),
 * exposing RNA to Python so blender data can be accessed in a Python like way.
 *
 * The two main types are #BPy_StructRNA and #BPy_PropertyRNA - the base
 * classes for most of the data Python accesses in blender.
 */

#include <Python.h>

#include <cfloat> /* FLT_MIN/MAX */
#include <cstddef>
#include <optional>

#include "RNA_path.hh"
#include "RNA_types.hh"

#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BPY_extern.hh"
#include "BPY_extern_clog.hh"

#include "bpy_capi_utils.hh"
#include "bpy_intern_string.hh"
#include "bpy_props.hh"
#include "bpy_rna.hh"
#include "bpy_rna_anim.hh"
#include "bpy_rna_callback.hh"

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
#  include "BLI_ghash.h"
#endif

#include "RNA_access.hh"
#include "RNA_define.hh" /* RNA_def_property_free_identifier */
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_windowmanager_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh" /* evil G.* */
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

/* Only for types. */
#include "BKE_node.hh"

#include "DEG_depsgraph_query.hh"

#include "../generic/idprop_py_api.hh" /* For IDprop lookups. */
#include "../generic/idprop_py_ui_api.hh"
#include "../generic/py_capi_rna.hh"
#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */
#include "../generic/python_utildefines.hh"

#define USE_PEDANTIC_WRITE
#define USE_MATHUTILS
#define USE_STRING_COERCE

/**
 * This _must_ be enabled to support Python 3.10's postponed annotations,
 * `from __future__ import annotations`.
 *
 * This has the disadvantage of evaluating strings at run-time, in the future we might be able to
 * reinstate the older, more efficient logic using descriptors, see: pep-0649
 */
#define USE_POSTPONED_ANNOTATIONS

/* Unfortunately Python needs to hold a global reference to the context.
 * If we remove this is means `bpy.context` won't be usable from some parts of the code:
 * `bpy.app.handler` callbacks for example.
 * Even though this is arguably "correct", it's going to cause problems for existing scripts,
 * so accept having this for the time being. */

BPy_StructRNA *bpy_context_module = nullptr; /* for fast access */

/**
 * 'Name' identifier for PyCapsule objects used internally by `bpy_rna.cc` to pass a #PointerRNA
 * pointer as argument when creating #BPy_StructRNA objects.
 */
static const char *BPy_capsule_PointerRNA_identifier = "BPy_PointerRNA_PyCapsule";

/** Basic container for a PropertyRNA and its PointerRNA. */
struct BPy_PropertyPointerRNA_Reference {
  const PointerRNA *ptr;
  PropertyRNA *prop;
};
/**
 * 'Name' identifier for PyCapsule objects used internally by `bpy_rna.cc` to pass a
 * #BPy_PropertyPointerRNA_Reference pointer as argument when creating #BPy_PropertyRNA and similar
 * objects.
 */
static const char *BPy_PropertyPointerRNA_capsule_identifier = "BPy_PropertyPointerRNA_PyCapsule";

/**
 * Return a new #BPy_FunctionRNA wrapping `ptr` & `func`.
 */
static PyObject *pyrna_func_CreatePyObject(const PointerRNA *ptr, FunctionRNA *func);

static PyObject *pyrna_struct_CreatePyObject_from_type(const PointerRNA *ptr,
                                                       PyTypeObject *tp,
                                                       void **instance);

static PyObject *pyrna_srna_Subtype(StructRNA *srna);
static PyObject *pyrna_struct_Subtype(PointerRNA *ptr);
static PyObject *pyrna_prop_collection_values(BPy_PropertyRNA *self);

static PyObject *pyrna_register_class(PyObject *self, PyObject *py_class);
static PyObject *pyrna_unregister_class(PyObject *self, PyObject *py_class);

static StructRNA *srna_from_ptr(PointerRNA *ptr);

/**
 * The `bpy_types-custom_properties` references is created as part of API doc generation.
 * When expanded line reads: "Limited to: Types with Custom Property Support".
 */
#define BPY_DOC_ID_PROP_TYPE_NOTE \
  "   .. note::\n" \
  "\n" \
  "      Limited to: :ref:`bpy_types-custom_properties`.\n"

int pyrna_struct_validity_check_only(const BPy_StructRNA *pysrna)
{
  if (pysrna->ptr->type) {
    return 0;
  }
  return -1;
}

void pyrna_struct_validity_exception_only(const BPy_StructRNA *pysrna)
{
  PyErr_Format(
      PyExc_ReferenceError, "StructRNA of type %.200s has been removed", Py_TYPE(pysrna)->tp_name);
}

int pyrna_struct_validity_check(const BPy_StructRNA *pysrna)
{
  if (pysrna->ptr->type) {
    return 0;
  }
  pyrna_struct_validity_exception_only(pysrna);
  return -1;
}

int pyrna_prop_validity_check(const BPy_PropertyRNA *self)
{
  if (self->ptr->type) {
    return 0;
  }
  PyErr_Format(PyExc_ReferenceError,
               "PropertyRNA of type %.200s.%.200s has been removed",
               Py_TYPE(self)->tp_name,
               RNA_property_identifier(self->prop));
  return -1;
}

void pyrna_invalidate(BPy_DummyPointerRNA *self)
{
  self->ptr->invalidate();
}

static void pyrna_prop_warn_deprecated(const PointerRNA *ptr,
                                       const PropertyRNA *prop,
                                       const DeprecatedRNA *deprecated)
{
  PyErr_WarnFormat(PyExc_DeprecationWarning,
                   1,
                   "'%s.%s' is expected to be removed in Blender %d.%d",
                   RNA_struct_identifier(ptr->type),
                   RNA_property_identifier(prop),
                   deprecated->removal_version / 100,
                   deprecated->removal_version % 100,
                   deprecated->note);
}

#ifdef USE_PYRNA_INVALIDATE_GC
#  define FROM_GC(g) ((PyObject *)(((PyGC_Head *)g) + 1))

/* Only for sizeof(). */
struct gc_generation {
  PyGC_Head head;
  int threshold;
  int count;
} gc_generation;

static void id_release_gc(struct ID *id)
{
  uint j;
  // uint i = 0;
  for (j = 0; j < 3; j++) {
    /* Hack below to get the 2 other lists from _PyGC_generation0 that are normally not exposed. */
    PyGC_Head *gen = (PyGC_Head *)(((char *)_PyGC_generation0) + (sizeof(gc_generation) * j));
    PyGC_Head *g = gen->gc.gc_next;
    while ((g = g->gc.gc_next) != gen) {
      PyObject *ob = FROM_GC(g);
      if (PyType_IsSubtype(Py_TYPE(ob), &pyrna_struct_Type) ||
          PyType_IsSubtype(Py_TYPE(ob), &pyrna_prop_Type))
      {
        BPy_DummyPointerRNA *ob_ptr = (BPy_DummyPointerRNA *)ob;
        if (ob_ptr->ptr->owner_id == id) {
          pyrna_invalidate(ob_ptr);
          // printf("freeing: %p %s, %.200s\n", (void *)ob, id->name, Py_TYPE(ob)->tp_name);
          // i++;
        }
      }
    }
  }
  // printf("id_release_gc freed '%s': %d\n", id->name, i);
}
#endif

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
// #define DEBUG_RNA_WEAKREF

struct GHash *id_weakref_pool = nullptr;
static PyObject *id_free_weakref_cb(PyObject *weakinfo_pair, PyObject *weakref);
static PyMethodDef id_free_weakref_cb_def = {
    "id_free_weakref_cb", (PyCFunction)id_free_weakref_cb, METH_O, nullptr};

/**
 * Only used when there are values left on exit (causing memory leaks).
 */
static void id_weakref_pool_free_value_fn(void *p)
{
  GHash *weakinfo_hash = static_cast<GHash *>(p);
  BLI_ghash_free(weakinfo_hash, nullptr, nullptr);
}

/* Adds a reference to the list, remember to decref. */
static GHash *id_weakref_pool_get(ID *id)
{
  GHash *weakinfo_hash = static_cast<GHash *>(BLI_ghash_lookup(id_weakref_pool, (void *)id));
  if (weakinfo_hash == nullptr) {
    /* This could be a set, values are used to keep a reference back to the ID
     * (all of them are the same). */
    weakinfo_hash = BLI_ghash_ptr_new("rna_id");
    BLI_ghash_insert(id_weakref_pool, id, weakinfo_hash);
  }
  return weakinfo_hash;
}

/* Called from pyrna_struct_CreatePyObject() and pyrna_prop_CreatePyObject(). */
static void id_weakref_pool_add(ID *id, BPy_DummyPointerRNA *pyrna)
{
  PyObject *weakref;
  PyObject *weakref_capsule;
  PyObject *weakref_cb_py;

  /* Create a new function instance and insert the list as 'self'
   * so we can remove ourself from it. */
  GHash *weakinfo_hash = id_weakref_pool_get(id); /* New or existing. */

  weakref_capsule = PyCapsule_New(weakinfo_hash, nullptr, nullptr);
  weakref_cb_py = PyCFunction_New(&id_free_weakref_cb_def, weakref_capsule);
  Py_DECREF(weakref_capsule);

  /* Add weakref to weakinfo_hash list. */
  weakref = PyWeakref_NewRef((PyObject *)pyrna, weakref_cb_py);

  Py_DECREF(weakref_cb_py); /* Function owned by the weakref now. */

  /* Important to add at the end of the hash, since first removal looks at the end. */

  /* Using a hash table as a set, all 'id's are the same. */
  BLI_ghash_insert(weakinfo_hash, weakref, id);
  /* weakinfo_hash owns the weakref */
}

/* Workaround to get the last id without a lookup. */
static ID *_id_tmp_ptr;
static void value_id_set(void *id)
{
  _id_tmp_ptr = (ID *)id;
}

static void id_release_weakref_list(struct ID *id, GHash *weakinfo_hash);
static PyObject *id_free_weakref_cb(PyObject *weakinfo_pair, PyObject *weakref)
{
  /* Important to search backwards. */
  GHash *weakinfo_hash = static_cast<GHash *>(PyCapsule_GetPointer(weakinfo_pair, nullptr));

  if (BLI_ghash_len(weakinfo_hash) > 1) {
    BLI_ghash_remove(weakinfo_hash, weakref, nullptr, nullptr);
  }
  else { /* Get the last id and free it. */
    BLI_ghash_remove(weakinfo_hash, weakref, nullptr, value_id_set);
    id_release_weakref_list(_id_tmp_ptr, weakinfo_hash);
  }

  Py_DECREF(weakref);

  Py_RETURN_NONE;
}

static void id_release_weakref_list(struct ID *id, GHash *weakinfo_hash)
{
  GHashIterator weakinfo_hash_iter;

  BLI_ghashIterator_init(&weakinfo_hash_iter, weakinfo_hash);

#  ifdef DEBUG_RNA_WEAKREF
  fprintf(stdout, "id_release_weakref: '%s', %d items\n", id->name, BLI_ghash_len(weakinfo_hash));
#  endif

  while (!BLI_ghashIterator_done(&weakinfo_hash_iter)) {
    PyObject *weakref = (PyObject *)BLI_ghashIterator_getKey(&weakinfo_hash_iter);
    PyObject *item = PyWeakref_GET_OBJECT(weakref);
    if (item != Py_None) {

#  ifdef DEBUG_RNA_WEAKREF
      PyC_ObSpit("id_release_weakref item ", item);
#  endif

      pyrna_invalidate((BPy_DummyPointerRNA *)item);
    }

    Py_DECREF(weakref);

    BLI_ghashIterator_step(&weakinfo_hash_iter);
  }

  BLI_ghash_remove(id_weakref_pool, (void *)id, nullptr, nullptr);
  BLI_ghash_free(weakinfo_hash, nullptr, nullptr);
}

static void id_release_weakref(struct ID *id)
{
  GHash *weakinfo_hash = static_cast<GHash *>(BLI_ghash_lookup(id_weakref_pool, (void *)id));
  if (weakinfo_hash) {
    id_release_weakref_list(id, weakinfo_hash);
  }
}

#endif /* USE_PYRNA_INVALIDATE_WEAKREF */

struct BPy_NamePropAsPyObject_Cache {
  PyObject *(*nameprop_as_py_object_fn)(const char *, Py_ssize_t);
  PropertyRNA *nameprop;
};

/**
 * Wrapper for #RNA_struct_name_get_alloc_ex that handles non UTF8 names, see #142909.
 */
static PyObject *pyrna_struct_get_nameprop_as_pyobject(
    PointerRNA *ptr, BPy_NamePropAsPyObject_Cache &nameprop_cache)
{
  char fixedbuf[256];
  int name_len;
  PropertyRNA *nameprop;
  char *name_ptr = RNA_struct_name_get_alloc_ex(
      ptr, fixedbuf, sizeof(fixedbuf), &name_len, &nameprop);
  if (LIKELY(name_ptr)) {
    /* In most cases this only runs once. */
    if (UNLIKELY(nameprop != nameprop_cache.nameprop)) {
      nameprop_cache.nameprop = nameprop;
      const PropertySubType subtype = RNA_property_subtype(nameprop);
      if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
        nameprop_cache.nameprop_as_py_object_fn = PyC_UnicodeFromBytesAndSize;
      }
      else {
        nameprop_cache.nameprop_as_py_object_fn = PyUnicode_FromStringAndSize;
      }
    }
    PyObject *result = nameprop_cache.nameprop_as_py_object_fn(name_ptr, name_len);
    /* The string data may be corrupt if this asserts,
     * or not using a file-path sub-type when it should.. */
    BLI_assert(result != nullptr);
    if (name_ptr != fixedbuf) {
      MEM_freeN(name_ptr);
    }
    return result;
  }
  return nullptr;
}

void BPY_id_release(ID *id)
{
#ifdef USE_PYRNA_INVALIDATE_GC
  id_release_gc(id);
#endif

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
  /* Check for nullptr since this may run before Python has been started. */
  if (id_weakref_pool != nullptr) {
    PyGILState_STATE gilstate = PyGILState_Ensure();

    id_release_weakref(id);

    PyGILState_Release(gilstate);
  }
#endif /* USE_PYRNA_INVALIDATE_WEAKREF */

  (void)id;
}

#ifdef USE_PEDANTIC_WRITE
static bool rna_disallow_writes = false;

static bool rna_id_write_error(PointerRNA *ptr, PyObject *key)
{
  ID *id = ptr->owner_id;
  if (id) {
    const short idcode = GS(id->name);
    /* May need more ID types added here. */
    if (!ELEM(idcode, ID_WM, ID_SCR, ID_WS)) {
      const char *idtype = BKE_idtype_idcode_to_name(idcode);
      const char *pyname;
      if (key && PyUnicode_Check(key)) {
        pyname = PyUnicode_AsUTF8(key);
      }
      else {
        pyname = "<UNKNOWN>";
      }

      /* Make a nice string error. */
      BLI_assert(idtype != nullptr);
      PyErr_Format(PyExc_AttributeError,
                   "Writing to ID classes in this context is not allowed: "
                   "%.200s, %.200s data-block, error setting %.200s.%.200s",
                   id->name + 2,
                   idtype,
                   RNA_struct_identifier(ptr->type),
                   pyname);

      return true;
    }
  }
  return false;
}
#endif /* USE_PEDANTIC_WRITE */

#ifdef USE_PEDANTIC_WRITE

/* NOTE: Without the GIL, this can cause problems when called from threads, see: #127767. */

bool pyrna_write_check()
{
  BLI_assert(PyGILState_Check());

  return !rna_disallow_writes;
}

void pyrna_write_set(bool val)
{
  BLI_assert(PyGILState_Check());

  rna_disallow_writes = !val;
}
#else  /* USE_PEDANTIC_WRITE */
bool pyrna_write_check()
{
  BLI_assert(PyGILState_Check());

  return true;
}
void pyrna_write_set(bool /*val*/)
{
  BLI_assert(PyGILState_Check());

  /* pass */
}
#endif /* USE_PEDANTIC_WRITE */

static Py_ssize_t pyrna_prop_collection_length(BPy_PropertyRNA *self);
static Py_ssize_t pyrna_prop_array_length(BPy_PropertyArrayRNA *self);
static int pyrna_py_to_prop(
    PointerRNA *ptr, PropertyRNA *prop, void *data, PyObject *value, const char *error_prefix);
static int deferred_register_prop(StructRNA *srna, PyObject *key, PyObject *item);

#ifdef USE_MATHUTILS
#  include "../mathutils/mathutils.hh" /* So we can have mathutils callbacks. */

static PyObject *pyrna_prop_array_subscript_slice(BPy_PropertyArrayRNA *self,
                                                  PointerRNA *ptr,
                                                  PropertyRNA *prop,
                                                  Py_ssize_t start,
                                                  Py_ssize_t stop,
                                                  Py_ssize_t length);
static short pyrna_rotation_euler_order_get(PointerRNA *ptr,
                                            const short order_fallback,
                                            PropertyRNA **r_prop_eul_order);

/* `bpyrna` vector/euler/quaternion callbacks. */
static uchar mathutils_rna_array_cb_index = -1; /* Index for our callbacks. */

/* Sub-type not used much yet. */
#  define MATHUTILS_CB_SUBTYPE_EUL 0
#  define MATHUTILS_CB_SUBTYPE_VEC 1
#  define MATHUTILS_CB_SUBTYPE_QUAT 2
#  define MATHUTILS_CB_SUBTYPE_COLOR 3

static int mathutils_rna_generic_check(BaseMathObject *bmo)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  return self->prop ? 0 : -1;
}

static int mathutils_rna_vector_get(BaseMathObject *bmo, int subtype)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == nullptr) {
    return -1;
  }

  RNA_property_float_get_array(&self->ptr.value(), self->prop, bmo->data);

  /* Euler order exception. */
  if (subtype == MATHUTILS_CB_SUBTYPE_EUL) {
    EulerObject *eul = (EulerObject *)bmo;
    PropertyRNA *prop_eul_order = nullptr;
    eul->order = pyrna_rotation_euler_order_get(&self->ptr.value(), eul->order, &prop_eul_order);
  }

  return 0;
}

static int mathutils_rna_vector_set(BaseMathObject *bmo, int subtype)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;
  float min, max;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == nullptr) {
    return -1;
  }

#  ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), nullptr)) {
    return -1;
  }
#  endif /* USE_PEDANTIC_WRITE */

  if (!RNA_property_editable_flag(&self->ptr.value(), self->prop)) {
    PyErr_Format(PyExc_AttributeError,
                 "bpy_prop \"%.200s.%.200s\" is read-only",
                 RNA_struct_identifier(self->ptr->type),
                 RNA_property_identifier(self->prop));
    return -1;
  }

  RNA_property_float_range(&self->ptr.value(), self->prop, &min, &max);

  if (min != -FLT_MAX || max != FLT_MAX) {
    int i, len = RNA_property_array_length(&self->ptr.value(), self->prop);
    for (i = 0; i < len; i++) {
      CLAMP(bmo->data[i], min, max);
    }
  }

  RNA_property_float_set_array(&self->ptr.value(), self->prop, bmo->data);
  if (RNA_property_update_check(self->prop)) {
    RNA_property_update(BPY_context_get(), &self->ptr.value(), self->prop);
  }

  /* Euler order exception. */
  if (subtype == MATHUTILS_CB_SUBTYPE_EUL) {
    EulerObject *eul = (EulerObject *)bmo;
    PropertyRNA *prop_eul_order = nullptr;
    const short order = pyrna_rotation_euler_order_get(
        &self->ptr.value(), eul->order, &prop_eul_order);
    if (order != eul->order) {
      RNA_property_enum_set(&self->ptr.value(), prop_eul_order, eul->order);
      if (RNA_property_update_check(prop_eul_order)) {
        RNA_property_update(BPY_context_get(), &self->ptr.value(), prop_eul_order);
      }
    }
  }
  return 0;
}

static int mathutils_rna_vector_get_index(BaseMathObject *bmo, int /*subtype*/, int index)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == nullptr) {
    return -1;
  }

  bmo->data[index] = RNA_property_float_get_index(&self->ptr.value(), self->prop, index);
  return 0;
}

static int mathutils_rna_vector_set_index(BaseMathObject *bmo, int /*subtype*/, int index)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == nullptr) {
    return -1;
  }

#  ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), nullptr)) {
    return -1;
  }
#  endif /* USE_PEDANTIC_WRITE */

  if (!RNA_property_editable_flag(&self->ptr.value(), self->prop)) {
    PyErr_Format(PyExc_AttributeError,
                 "bpy_prop \"%.200s.%.200s\" is read-only",
                 RNA_struct_identifier(self->ptr->type),
                 RNA_property_identifier(self->prop));
    return -1;
  }

  RNA_property_float_clamp(&self->ptr.value(), self->prop, &bmo->data[index]);
  RNA_property_float_set_index(&self->ptr.value(), self->prop, index, bmo->data[index]);

  if (RNA_property_update_check(self->prop)) {
    RNA_property_update(BPY_context_get(), &self->ptr.value(), self->prop);
  }

  return 0;
}

static Mathutils_Callback mathutils_rna_array_cb = {
    (BaseMathCheckFunc)mathutils_rna_generic_check,
    (BaseMathGetFunc)mathutils_rna_vector_get,
    (BaseMathSetFunc)mathutils_rna_vector_set,
    (BaseMathGetIndexFunc)mathutils_rna_vector_get_index,
    (BaseMathSetIndexFunc)mathutils_rna_vector_set_index,
};

/* BPY/RNA matrix callbacks. */
static uchar mathutils_rna_matrix_cb_index = -1; /* Index for our callbacks. */

static int mathutils_rna_matrix_get(BaseMathObject *bmo, int /*subtype*/)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == nullptr) {
    return -1;
  }

  RNA_property_float_get_array(&self->ptr.value(), self->prop, bmo->data);
  return 0;
}

static int mathutils_rna_matrix_set(BaseMathObject *bmo, int /*subtype*/)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == nullptr) {
    return -1;
  }

#  ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), nullptr)) {
    return -1;
  }
#  endif /* USE_PEDANTIC_WRITE */

  if (!RNA_property_editable_flag(&self->ptr.value(), self->prop)) {
    PyErr_Format(PyExc_AttributeError,
                 "bpy_prop \"%.200s.%.200s\" is read-only",
                 RNA_struct_identifier(self->ptr->type),
                 RNA_property_identifier(self->prop));
    return -1;
  }

  /* Can ignore clamping here. */
  RNA_property_float_set_array(&self->ptr.value(), self->prop, bmo->data);

  if (RNA_property_update_check(self->prop)) {
    RNA_property_update(BPY_context_get(), &self->ptr.value(), self->prop);
  }
  return 0;
}

static Mathutils_Callback mathutils_rna_matrix_cb = {
    mathutils_rna_generic_check,
    mathutils_rna_matrix_get,
    mathutils_rna_matrix_set,
    nullptr,
    nullptr,
};

static short pyrna_rotation_euler_order_get(PointerRNA *ptr,
                                            const short order_fallback,
                                            PropertyRNA **r_prop_eul_order)
{
  /* Attempt to get order. */
  if (*r_prop_eul_order == nullptr) {
    *r_prop_eul_order = RNA_struct_find_property(ptr, "rotation_mode");
  }

  if (*r_prop_eul_order) {
    const short order = RNA_property_enum_get(ptr, *r_prop_eul_order);
    /* Could be quaternion or axis-angle. */
    if (order >= EULER_ORDER_XYZ && order <= EULER_ORDER_ZYX) {
      return order;
    }
  }

  return order_fallback;
}

#endif /* USE_MATHUTILS */

/**
 * Note that #PROP_NONE is included as a vector subtype. this is because it is handy to
 * have x/y access to fcurve keyframes and other fixed size float arrays of length 2-4.
 */
#define PROP_ALL_VECTOR_SUBTYPES \
  PROP_COORDS: \
  case PROP_TRANSLATION: \
  case PROP_DIRECTION: \
  case PROP_VELOCITY: \
  case PROP_ACCELERATION: \
  case PROP_XYZ: \
  case PROP_XYZ_LENGTH

PyObject *pyrna_math_object_from_array(PointerRNA *ptr, PropertyRNA *prop)
{
  PyObject *ret = nullptr;

#ifdef USE_MATHUTILS
  int subtype, totdim;
  int len;
  const int flag = RNA_property_flag(prop);
  const int type = RNA_property_type(prop);
  const bool is_thick = (flag & PROP_THICK_WRAP) != 0;

  /* disallow dynamic sized arrays to be wrapped since the size could change
   * to a size mathutils does not support */
  if (flag & PROP_DYNAMIC) {
    return nullptr;
  }

  len = RNA_property_array_length(ptr, prop);
  if (type == PROP_FLOAT) {
    /* pass */
  }
  else if (type == PROP_INT) {
    if (is_thick) {
      goto thick_wrap_slice;
    }
    else {
      return nullptr;
    }
  }
  else {
    return nullptr;
  }

  subtype = RNA_property_subtype(prop);
  totdim = RNA_property_array_dimension(ptr, prop, nullptr);

  if (totdim == 1 || (totdim == 2 && subtype == PROP_MATRIX)) {
    if (!is_thick) {
      /* Owned by the mathutils PyObject. */
      ret = pyrna_prop_CreatePyObject(ptr, prop);
    }

    switch (subtype) {
      case PROP_ALL_VECTOR_SUBTYPES:
        if (len >= 2 && len <= 4) {
          if (is_thick) {
            ret = Vector_CreatePyObject(nullptr, len, nullptr);
            RNA_property_float_get_array(ptr, prop, ((VectorObject *)ret)->vec);
          }
          else {
            PyObject *vec_cb = Vector_CreatePyObject_cb(
                ret, len, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_VEC);
            Py_DECREF(ret); /* The vector owns 'ret' now. */
            ret = vec_cb;   /* Return the vector instead. */
          }
        }
        break;
      case PROP_MATRIX:
        if (len == 16) {
          if (is_thick) {
            ret = Matrix_CreatePyObject(nullptr, 4, 4, nullptr);
            RNA_property_float_get_array(ptr, prop, ((MatrixObject *)ret)->matrix);
          }
          else {
            PyObject *mat_cb = Matrix_CreatePyObject_cb(
                ret, 4, 4, mathutils_rna_matrix_cb_index, 0);
            Py_DECREF(ret); /* The matrix owns 'ret' now. */
            ret = mat_cb;   /* Return the matrix instead. */
          }
        }
        else if (len == 9) {
          if (is_thick) {
            ret = Matrix_CreatePyObject(nullptr, 3, 3, nullptr);
            RNA_property_float_get_array(ptr, prop, ((MatrixObject *)ret)->matrix);
          }
          else {
            PyObject *mat_cb = Matrix_CreatePyObject_cb(
                ret, 3, 3, mathutils_rna_matrix_cb_index, 0);
            Py_DECREF(ret); /* The matrix owns 'ret' now. */
            ret = mat_cb;   /* Return the matrix instead. */
          }
        }
        break;
      case PROP_EULER:
      case PROP_QUATERNION:
        if (len == 3) { /* Euler. */
          if (is_thick) {
            /* Attempt to get order,
             * only needed for thick types since wrapped with update via callbacks. */
            PropertyRNA *prop_eul_order = nullptr;
            const short order = pyrna_rotation_euler_order_get(
                ptr, EULER_ORDER_XYZ, &prop_eul_order);

            ret = Euler_CreatePyObject(nullptr, order, nullptr); /* TODO: get order from RNA. */
            RNA_property_float_get_array(ptr, prop, ((EulerObject *)ret)->eul);
          }
          else {
            /* Order will be updated from callback on use. */
            /* TODO: get order from RNA. */
            PyObject *eul_cb = Euler_CreatePyObject_cb(
                ret, EULER_ORDER_XYZ, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_EUL);
            Py_DECREF(ret); /* The euler owns 'ret' now. */
            ret = eul_cb;   /* Return the euler instead. */
          }
        }
        else if (len == 4) {
          if (is_thick) {
            ret = Quaternion_CreatePyObject(nullptr, nullptr);
            RNA_property_float_get_array(ptr, prop, ((QuaternionObject *)ret)->quat);
          }
          else {
            PyObject *quat_cb = Quaternion_CreatePyObject_cb(
                ret, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_QUAT);
            Py_DECREF(ret); /* The quat owns 'ret' now. */
            ret = quat_cb;  /* Return the quat instead. */
          }
        }
        break;
      case PROP_COLOR:
      case PROP_COLOR_GAMMA:
        if (len == 3) { /* Color. */
          if (is_thick) {
            ret = Color_CreatePyObject(nullptr, nullptr);
            RNA_property_float_get_array(ptr, prop, ((ColorObject *)ret)->col);
          }
          else {
            PyObject *col_cb = Color_CreatePyObject_cb(
                ret, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_COLOR);
            Py_DECREF(ret); /* The color owns 'ret' now. */
            ret = col_cb;   /* Return the color instead. */
          }
        }
        break;
      default:
        break;
    }
  }

  if (ret == nullptr) {
    if (is_thick) {
    /* This is an array we can't reference (since it is not thin wrappable)
     * and cannot be coerced into a mathutils type, so return as a list. */
    thick_wrap_slice:
      ret = pyrna_prop_array_subscript_slice(nullptr, ptr, prop, 0, len, len);
    }
    else {
      ret = pyrna_prop_CreatePyObject(ptr, prop); /* Owned by the mathutils PyObject. */
    }
  }
#else  /* USE_MATHUTILS */
  (void)ptr;
  (void)prop;
#endif /* USE_MATHUTILS */

  return ret;
}

/* NOTE(@ideasman42): Regarding comparison `__cmp__`:
 * checking the 'ptr->data' matches works in almost all cases,
 * however there are a few RNA properties that are fake sub-structs and
 * share the pointer with the parent, in those cases this happens 'a.b == a'
 * see: r43352 for example.
 *
 * So compare the 'ptr->type' as well to avoid this problem.
 * It's highly unlikely this would happen that 'ptr->data' and 'ptr->prop' would match,
 * but _not_ 'ptr->type' but include this check for completeness. */

static int pyrna_struct_compare(BPy_StructRNA *a, BPy_StructRNA *b)
{
  return (((a->ptr->data == b->ptr->data) && (a->ptr->type == b->ptr->type)) ? 0 : -1);
}

static int pyrna_prop_compare(BPy_PropertyRNA *a, BPy_PropertyRNA *b)
{
  return (
      ((a->prop == b->prop) && (a->ptr->data == b->ptr->data) && (a->ptr->type == b->ptr->type)) ?
          0 :
          -1);
}

static PyObject *pyrna_struct_richcmp(PyObject *a, PyObject *b, int op)
{
  PyObject *res;
  int ok = -1; /* Zero is true. */

  if (BPy_StructRNA_Check(a) && BPy_StructRNA_Check(b)) {
    ok = pyrna_struct_compare((BPy_StructRNA *)a, (BPy_StructRNA *)b);
  }

  switch (op) {
    case Py_NE:
      ok = !ok;
      ATTR_FALLTHROUGH;
    case Py_EQ:
      res = ok ? Py_False : Py_True;
      break;

    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE:
      res = Py_NotImplemented;
      break;
    default:
      PyErr_BadArgument();
      return nullptr;
  }

  return Py_NewRef(res);
}

static PyObject *pyrna_prop_richcmp(PyObject *a, PyObject *b, int op)
{
  PyObject *res;
  int ok = -1; /* Zero is true. */

  if (BPy_PropertyRNA_Check(a) && BPy_PropertyRNA_Check(b)) {
    ok = pyrna_prop_compare((BPy_PropertyRNA *)a, (BPy_PropertyRNA *)b);
  }

  switch (op) {
    case Py_NE:
      ok = !ok;
      ATTR_FALLTHROUGH;
    case Py_EQ:
      res = ok ? Py_False : Py_True;
      break;

    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE:
      res = Py_NotImplemented;
      break;
    default:
      PyErr_BadArgument();
      return nullptr;
  }

  return Py_NewRef(res);
}

/*----------------------repr--------------------------------------------*/
static PyObject *pyrna_struct_str(BPy_StructRNA *self)
{
  PyObject *ret;
  const char *name;
  const char *extra_info = "";

  if (!PYRNA_STRUCT_IS_VALID(self)) {
    return PyUnicode_FromFormat("<bpy_struct, %.200s invalid>", Py_TYPE(self)->tp_name);
  }

  ID *id = self->ptr->owner_id;
  if (id && id != DEG_get_original(id)) {
    extra_info = ", evaluated";
  }

  /* Print name if available.
   *
   * Always include the pointer address since it can help identify unique data,
   * or when data is re-allocated internally. */
  name = RNA_struct_name_get_alloc(&self->ptr.value(), nullptr, 0, nullptr);
  if (name) {
    ret = PyUnicode_FromFormat("<bpy_struct, %.200s(\"%.200s\") at %p%s>",
                               RNA_struct_identifier(self->ptr->type),
                               name,
                               self->ptr->data,
                               extra_info);
    MEM_freeN(name);
    return ret;
  }

  return PyUnicode_FromFormat("<bpy_struct, %.200s at %p%s>",
                              RNA_struct_identifier(self->ptr->type),
                              self->ptr->data,
                              extra_info);
}

static PyObject *pyrna_struct_repr(BPy_StructRNA *self)
{
  ID *id = self->ptr->owner_id;
  PyObject *tmp_str;
  PyObject *ret;

  if (id == nullptr || !PYRNA_STRUCT_IS_VALID(self) || (DEG_get_original(id) != id)) {
    /* fallback */
    return pyrna_struct_str(self);
  }

  tmp_str = PyUnicode_FromString(id->name + 2);

  if (RNA_struct_is_ID(self->ptr->type) && (id->flag & ID_FLAG_EMBEDDED_DATA) == 0) {
    ret = PyUnicode_FromFormat(
        "bpy.data.%s[%R]", BKE_idtype_idcode_to_name_plural(GS(id->name)), tmp_str);
  }
  else {
    ID *real_id = nullptr;
    const std::optional<std::string> path = RNA_path_from_real_ID_to_struct(
        G_MAIN, &self->ptr.value(), &real_id);
    if (path) {
      /* 'real_id' may be nullptr in some cases, although the only valid one is evaluated data,
       * which should have already been caught above.
       * So assert, but handle it without crashing for release builds. */
      BLI_assert(real_id != nullptr);

      if (real_id != nullptr) {
        Py_DECREF(tmp_str);
        tmp_str = PyUnicode_FromString(real_id->name + 2);
        ret = PyUnicode_FromFormat("bpy.data.%s[%R].%s",
                                   BKE_idtype_idcode_to_name_plural(GS(real_id->name)),
                                   tmp_str,
                                   path->c_str());
      }
      else {
        /* Can't find the path, print something useful as a fallback. */
        ret = PyUnicode_FromFormat("bpy.data.%s[%R]...%s",
                                   BKE_idtype_idcode_to_name_plural(GS(id->name)),
                                   tmp_str,
                                   RNA_struct_identifier(self->ptr->type));
      }
    }
    else {
      /* Can't find the path, print something useful as a fallback. */
      ret = PyUnicode_FromFormat("bpy.data.%s[%R]...%s",
                                 BKE_idtype_idcode_to_name_plural(GS(id->name)),
                                 tmp_str,
                                 RNA_struct_identifier(self->ptr->type));
    }
  }

  Py_DECREF(tmp_str);

  return ret;
}

static PyObject *pyrna_prop_str(BPy_PropertyRNA *self)
{
  PyObject *ret;
  PointerRNA ptr;
  const char *name;
  const char *type_id = nullptr;
  char type_lower[64];
  char type_count[16];
  int type;

  PYRNA_PROP_CHECK_OBJ(self);

  type = RNA_property_type(self->prop);

  if (RNA_enum_id_from_value(rna_enum_property_type_items, type, &type_id) == 0) {
    /* Should never happen. */
    PyErr_SetString(PyExc_RuntimeError, "could not use property type, internal error");
    return nullptr;
  }

  STRNCPY_UTF8(type_lower, type_id);
  BLI_str_tolower_ascii(type_lower, sizeof(type_lower));

  int len = -1;
  if (type == PROP_COLLECTION) {
    len = pyrna_prop_collection_length(self);
  }
  else if (RNA_property_array_check(self->prop)) {
    len = pyrna_prop_array_length((BPy_PropertyArrayRNA *)self);
  }

  if (len != -1) {
    SNPRINTF_UTF8(type_count, "[%d]", len);
  }
  else {
    type_count[0] = '\0';
  }

  /* If a pointer, try to print name of pointer target too. */
  if (type == PROP_POINTER) {
    ptr = RNA_property_pointer_get(&self->ptr.value(), self->prop);
    name = RNA_struct_name_get_alloc(&ptr, nullptr, 0, nullptr);

    if (name) {
      ret = PyUnicode_FromFormat("<bpy_%.200s%.200s, %.200s.%.200s(\"%.200s\")>",
                                 type_lower,
                                 type_count,
                                 RNA_struct_identifier(self->ptr->type),
                                 RNA_property_identifier(self->prop),
                                 name);
      MEM_freeN(name);
      return ret;
    }
  }
  if (type == PROP_COLLECTION) {
    PointerRNA r_ptr;
    if (RNA_property_collection_type_get(&self->ptr.value(), self->prop, &r_ptr)) {
      return PyUnicode_FromFormat(
          "<bpy_%.200s%.200s, %.200s>", type_lower, type_count, RNA_struct_identifier(r_ptr.type));
    }
  }

  return PyUnicode_FromFormat("<bpy_%.200s%.200s, %.200s.%.200s>",
                              type_lower,
                              type_count,
                              RNA_struct_identifier(self->ptr->type),
                              RNA_property_identifier(self->prop));
}

static PyObject *pyrna_prop_repr_ex(BPy_PropertyRNA *self, const int index_dim, const int index)
{
  ID *id = self->ptr->owner_id;
  PyObject *tmp_str;
  PyObject *ret;

  PYRNA_PROP_CHECK_OBJ(self);

  if (id == nullptr) {
    /* Fallback. */
    return pyrna_prop_str(self);
  }

  tmp_str = PyUnicode_FromString(id->name + 2);

  /* Note that using G_MAIN is absolutely not ideal, but we have no access to actual Main DB from
   * here. */
  ID *real_id = nullptr;
  const std::optional<std::string> path = RNA_path_from_real_ID_to_property_index(
      G_MAIN, &self->ptr.value(), self->prop, index_dim, index, &real_id);

  if (path) {
    if (real_id != id) {
      Py_DECREF(tmp_str);
      tmp_str = PyUnicode_FromString(real_id->name + 2);
    }
    const char *data_delim = ((*path)[0] == '[') ? "" : ".";
    ret = PyUnicode_FromFormat("bpy.data.%s[%R]%s%s",
                               BKE_idtype_idcode_to_name_plural(GS(real_id->name)),
                               tmp_str,
                               data_delim,
                               path->c_str());
  }
  else {
    /* Can't find the path, print something useful as a fallback. */
    ret = PyUnicode_FromFormat("bpy.data.%s[%R]...%s",
                               BKE_idtype_idcode_to_name_plural(GS(id->name)),
                               tmp_str,
                               RNA_property_identifier(self->prop));
  }

  Py_DECREF(tmp_str);

  return ret;
}

static PyObject *pyrna_prop_repr(BPy_PropertyRNA *self)
{
  return pyrna_prop_repr_ex(self, 0, -1);
}

static PyObject *pyrna_prop_array_repr(BPy_PropertyArrayRNA *self)
{
  return pyrna_prop_repr_ex((BPy_PropertyRNA *)self, self->arraydim, self->arrayoffset);
}

static PyObject *pyrna_func_repr(BPy_FunctionRNA *self)
{
  return PyUnicode_FromFormat("<%.200s %.200s.%.200s()>",
                              Py_TYPE(self)->tp_name,
                              RNA_struct_identifier(self->ptr->type),
                              RNA_function_identifier(self->func));
}

static Py_hash_t pyrna_struct_hash(BPy_StructRNA *self)
{
  return Py_HashPointer(self->ptr->data);
}

/* From Python's meth_hash v3.1.2. */
static long pyrna_prop_hash(BPy_PropertyRNA *self)
{
  long x, y;
  if (self->ptr->data == nullptr) {
    x = 0;
  }
  else {
    x = Py_HashPointer(self->ptr->data);
    if (x == -1) {
      return -1;
    }
  }
  y = Py_HashPointer((void *)(self->prop));
  if (y == -1) {
    return -1;
  }
  x ^= y;
  if (x == -1) {
    x = -2;
  }
  return x;
}

#ifdef USE_PYRNA_STRUCT_REFERENCE
static int pyrna_struct_traverse(BPy_StructRNA *self, visitproc visit, void *arg)
{
  Py_VISIT(self->reference);
  return 0;
}

static int pyrna_struct_clear(BPy_StructRNA *self)
{
  Py_CLEAR(self->reference);
  return 0;
}
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

#ifdef USE_PYRNA_STRUCT_REFERENCE
static void pyrna_struct_reference_set(BPy_StructRNA *self, PyObject *reference)
{
  if (self->reference) {
    PyObject_GC_UnTrack(self);
    Py_CLEAR(self->reference);
  }
  /* Reference is now nullptr. */

  if (reference) {
    self->reference = reference;
    Py_INCREF(reference);
    BLI_assert(!PyObject_GC_IsTracked((PyObject *)self));
    PyObject_GC_Track(self);
  }
}
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

static const char *pyrna_enum_as_string(PointerRNA *ptr, PropertyRNA *prop)
{
  const EnumPropertyItem *item;
  const char *result;
  bool free = false;

  RNA_property_enum_items(BPY_context_get(), ptr, prop, &item, nullptr, &free);
  if (item) {
    result = pyrna_enum_repr(item);
  }
  else {
    result = "";
  }

  if (free) {
    MEM_freeN(item);
  }

  return result;
}

static int pyrna_string_to_enum(
    PyObject *item, PointerRNA *ptr, PropertyRNA *prop, int *r_value, const char *error_prefix)
{
  const char *param = PyUnicode_AsUTF8(item);

  if (param == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s expected a string enum, not %.200s",
                 error_prefix,
                 Py_TYPE(item)->tp_name);
    return -1;
  }

  if (!RNA_property_enum_value(BPY_context_get(), ptr, prop, param, r_value)) {
    const char *enum_str = pyrna_enum_as_string(ptr, prop);
    PyErr_Format(PyExc_TypeError,
                 "%.200s enum \"%.200s\" not found in (%s)",
                 error_prefix,
                 param,
                 enum_str);
    MEM_freeN(enum_str);
    return -1;
  }

  return 0;
}

static int pyrna_prop_to_enum_bitfield(
    PointerRNA *ptr, PropertyRNA *prop, PyObject *value, int *r_value, const char *error_prefix)
{
  const EnumPropertyItem *item;
  int ret;
  bool free = false;

  *r_value = 0;

  if (!PyAnySet_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s, %.200s.%.200s expected a set, not a %.200s",
                 error_prefix,
                 RNA_struct_identifier(ptr->type),
                 RNA_property_identifier(prop),
                 Py_TYPE(value)->tp_name);
    return -1;
  }

  RNA_property_enum_items(BPY_context_get(), ptr, prop, &item, nullptr, &free);

  if (item) {
    ret = pyrna_enum_bitfield_from_set(item, value, r_value, error_prefix);
  }
  else {
    if (PySet_GET_SIZE(value)) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s: empty enum \"%.200s\" could not have any values assigned",
                   error_prefix,
                   RNA_property_identifier(prop));
      ret = -1;
    }
    else {
      ret = 0;
    }
  }

  if (free) {
    MEM_freeN(item);
  }

  return ret;
}

static PyObject *pyrna_enum_to_py(PointerRNA *ptr, PropertyRNA *prop, int val)
{
  PyObject *item, *ret = nullptr;

  if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
    const char *identifier[RNA_ENUM_BITFLAG_SIZE + 1];

    ret = PySet_New(nullptr);

    if (RNA_property_enum_bitflag_identifiers(BPY_context_get(), ptr, prop, val, identifier)) {
      int index;

      for (index = 0; identifier[index]; index++) {
        item = PyUnicode_FromString(identifier[index]);
        PySet_Add(ret, item);
        Py_DECREF(item);
      }
    }
  }
  else {
    const char *identifier;
    if (RNA_property_enum_identifier(BPY_context_get(), ptr, prop, val, &identifier)) {
      ret = PyUnicode_FromString(identifier);
    }
    else {
      /* Static, no need to free. */
      const EnumPropertyItem *enum_item;
      bool free_dummy;
      RNA_property_enum_items_ex(nullptr, ptr, prop, true, &enum_item, nullptr, &free_dummy);
      BLI_assert(!free_dummy);

      /* Do not print warning in case of #rna_enum_dummy_NULL_items,
       * this one will never match any value... */
      if (enum_item != rna_enum_dummy_NULL_items) {
        const char *ptr_name = RNA_struct_name_get_alloc(ptr, nullptr, 0, nullptr);

        /* Prefer not to fail silently in case of API errors, maybe disable it later. */
        CLOG_WARN(BPY_LOG_RNA,
                  "current value '%d' "
                  "matches no enum in '%s', '%s', '%s'",
                  val,
                  RNA_struct_identifier(ptr->type),
                  ptr_name,
                  RNA_property_identifier(prop));

#if 0 /* Gives Python decoding errors while generating docs :( */
        char error_str[256];
        BLI_snprintf_utf8(error_str,
                     sizeof(error_str),
                     "RNA Warning: Current value \"%d\" "
                     "matches no enum in '%s', '%s', '%s'",
                     val,
                     RNA_struct_identifier(ptr->type),
                     ptr_name,
                     RNA_property_identifier(prop));

        PyErr_Warn(PyExc_RuntimeWarning, error_str);
#endif

        if (ptr_name) {
          MEM_freeN(ptr_name);
        }
      }

      ret = PyUnicode_FromString("");
#if 0
      PyErr_Format(PyExc_AttributeError, "RNA Error: Current value \"%d\" matches no enum", val);
      ret = nullptr;
#endif
    }
  }

  return ret;
}

PyObject *pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop)
{
  PyObject *ret;
  const int type = RNA_property_type(prop);

  if (const DeprecatedRNA *deprecated = RNA_property_deprecated(prop)) {
    pyrna_prop_warn_deprecated(ptr, prop, deprecated);
  }

  if (RNA_property_array_check(prop)) {
    return pyrna_py_from_array(ptr, prop);
  }

  /* See if we can coerce into a Python type - 'PropertyType'. */
  switch (type) {
    case PROP_BOOLEAN:
      ret = PyBool_FromLong(RNA_property_boolean_get(ptr, prop));
      break;
    case PROP_INT:
      ret = PyLong_FromLong(RNA_property_int_get(ptr, prop));
      break;
    case PROP_FLOAT:
      ret = PyFloat_FromDouble(RNA_property_float_get(ptr, prop));
      break;
    case PROP_STRING: {
      const int subtype = RNA_property_subtype(prop);
      const char *buf;
      int buf_len;
      char buf_fixed[32];

      buf = RNA_property_string_get_alloc(ptr, prop, buf_fixed, sizeof(buf_fixed), &buf_len);
#ifdef USE_STRING_COERCE
      /* Only file paths get special treatment, they may contain non UTF8 chars. */
      if (subtype == PROP_BYTESTRING) {
        ret = PyBytes_FromStringAndSize(buf, buf_len);
      }
      else if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
        ret = PyC_UnicodeFromBytesAndSize(buf, buf_len);
      }
      else {
        ret = PyUnicode_FromStringAndSize(buf, buf_len);
      }
#else  /* USE_STRING_COERCE */
      if (subtype == PROP_BYTESTRING) {
        ret = PyBytes_FromStringAndSize(buf, buf_len);
      }
      else {
        ret = PyUnicode_FromStringAndSize(buf, buf_len);
      }
#endif /* USE_STRING_COERCE */
      if (buf_fixed != buf) {
        MEM_freeN(buf);
      }
      break;
    }
    case PROP_ENUM: {
      ret = pyrna_enum_to_py(ptr, prop, RNA_property_enum_get(ptr, prop));
      break;
    }
    case PROP_POINTER: {
      PointerRNA newptr;
      newptr = RNA_property_pointer_get(ptr, prop);
      if (newptr.data) {
        ret = pyrna_struct_CreatePyObject(&newptr);
      }
      else {
        ret = Py_None;
        Py_INCREF(ret);
      }
      break;
    }
    case PROP_COLLECTION:
      ret = pyrna_prop_CreatePyObject(ptr, prop);
      break;
    default:
      PyErr_Format(PyExc_TypeError,
                   "bpy_struct internal error: unknown type '%d' (pyrna_prop_to_py)",
                   type);
      ret = nullptr;
      break;
  }

  return ret;
}

int pyrna_pydict_to_props(PointerRNA *ptr,
                          PyObject *kw,
                          const bool all_args,
                          const char *error_prefix)
{
  int error_val = 0;
  int totkw;
  const char *arg_name = nullptr;
  PyObject *item;

  totkw = kw ? PyDict_Size(kw) : 0;

  RNA_STRUCT_BEGIN (ptr, prop) {
    arg_name = RNA_property_identifier(prop);

    if (STREQ(arg_name, "rna_type")) {
      continue;
    }

    if (kw == nullptr) {
      PyErr_Format(
          PyExc_TypeError, "%.200s: no keywords, expected \"%.200s\"", error_prefix, arg_name);
      error_val = -1;
      break;
    }

    item = PyDict_GetItemString(kw, arg_name); /* Won't set an error. */

    if (item == nullptr) {
      if (all_args) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s: keyword \"%.200s\" missing",
                     error_prefix,
                     arg_name ? arg_name : "<UNKNOWN>");
        error_val = -1; /* pyrna_py_to_prop sets the error. */
        break;
      }
    }
    else {
      if (pyrna_py_to_prop(ptr, prop, nullptr, item, error_prefix)) {
        error_val = -1;
        break;
      }
      totkw--;
    }
  }
  RNA_STRUCT_END;

  if (error_val == 0 && totkw > 0) { /* Some keywords were given that were not used :/. */
    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(kw, &pos, &key, &value)) {
      arg_name = PyUnicode_AsUTF8(key);
      if (RNA_struct_find_property(ptr, arg_name) == nullptr) {
        break;
      }
      arg_name = nullptr;
    }

    PyErr_Format(PyExc_TypeError,
                 "%.200s: keyword \"%.200s\" unrecognized",
                 error_prefix,
                 arg_name ? arg_name : "<UNKNOWN>");
    error_val = -1;
  }

  return error_val;
}

static int pyrna_py_to_prop(
    PointerRNA *ptr, PropertyRNA *prop, void *data, PyObject *value, const char *error_prefix)
{
  /* XXX hard limits should be checked here. */
  const int type = RNA_property_type(prop);

  if (const DeprecatedRNA *deprecated = RNA_property_deprecated(prop)) {
    pyrna_prop_warn_deprecated(ptr, prop, deprecated);
  }

  if (RNA_property_array_check(prop)) {
    /* Done getting the length. */
    if (pyrna_py_to_array(ptr, prop, static_cast<char *>(data), value, error_prefix) == -1) {
      return -1;
    }
  }
  else {
    /* Normal Property (not an array). */

    /* See if we can coerce into a Python type - 'PropertyType'. */
    switch (type) {
      case PROP_BOOLEAN: {
        int param;
        /* Prefer not to have an exception here
         * however so many poll functions return None or a valid Object.
         * It's a hassle to convert these into a bool before returning. */
        if (RNA_parameter_flag(prop) & PARM_OUTPUT) {
          param = PyObject_IsTrue(value);
        }
        else {
          param = PyC_Long_AsI32(value);

          if (UNLIKELY(param & ~1)) { /* Only accept 0/1. */
            param = -1;               /* Error out below. */
          }
        }

        if (param == -1) {
          PyErr_Format(PyExc_TypeError,
                       "%.200s %.200s.%.200s expected True/False or 0/1, not %.200s",
                       error_prefix,
                       RNA_struct_identifier(ptr->type),
                       RNA_property_identifier(prop),
                       Py_TYPE(value)->tp_name);
          return -1;
        }

        if (data) {
          *((bool *)data) = param;
        }
        else {
          RNA_property_boolean_set(ptr, prop, param);
        }

        break;
      }
      case PROP_INT: {
        int overflow;
        const long param = PyLong_AsLongAndOverflow(value, &overflow);
        if (overflow || (param > INT_MAX) || (param < INT_MIN)) {
          PyErr_Format(PyExc_ValueError,
                       "%.200s %.200s.%.200s value not in 'int' range "
                       "(" STRINGIFY(INT_MIN) ", " STRINGIFY(INT_MAX) ")",
                       error_prefix,
                       RNA_struct_identifier(ptr->type),
                       RNA_property_identifier(prop));
          return -1;
        }
        if (param == -1 && PyErr_Occurred()) {
          PyErr_Format(PyExc_TypeError,
                       "%.200s %.200s.%.200s expected an int type, not %.200s",
                       error_prefix,
                       RNA_struct_identifier(ptr->type),
                       RNA_property_identifier(prop),
                       Py_TYPE(value)->tp_name);
          return -1;
        }

        int param_i = int(param);
        if (data) {
          RNA_property_int_clamp(ptr, prop, &param_i);
          *((int *)data) = param_i;
        }
        else {
          RNA_property_int_set(ptr, prop, param_i);
        }

        break;
      }
      case PROP_FLOAT: {
        const float param = PyFloat_AsDouble(value);
        if (PyErr_Occurred()) {
          PyErr_Format(PyExc_TypeError,
                       "%.200s %.200s.%.200s expected a float type, not %.200s",
                       error_prefix,
                       RNA_struct_identifier(ptr->type),
                       RNA_property_identifier(prop),
                       Py_TYPE(value)->tp_name);
          return -1;
        }

        if (data) {
          RNA_property_float_clamp(ptr, prop, (float *)&param);
          *((float *)data) = param;
        }
        else {
          RNA_property_float_set(ptr, prop, param);
        }

        break;
      }
      case PROP_STRING: {
        const int flag = RNA_property_flag(prop);
        const int subtype = RNA_property_subtype(prop);
        const char *param;

        if (value == Py_None) {
          if ((flag & PROP_NEVER_NULL) == 0) {
            if (data) {
              if (flag & PROP_THICK_WRAP) {
                *(char *)data = 0;
              }
              else {
                *((char **)data) = (char *)nullptr;
              }
            }
            else {
              RNA_property_string_set(ptr, prop, nullptr);
            }
          }
          else {
            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s doesn't support None from string types",
                         error_prefix,
                         RNA_struct_identifier(ptr->type),
                         RNA_property_identifier(prop));
            return -1;
          }
        }
        else if (subtype == PROP_BYTESTRING) {

          /* Byte String. */

          param = PyBytes_AsString(value);

          if (param == nullptr) {
            if (PyBytes_Check(value)) {
              /* there was an error assigning a string type,
               * rather than setting a new error, prefix the existing one
               */
              PyC_Err_Format_Prefix(PyExc_TypeError,
                                    "%.200s %.200s.%.200s error assigning bytes",
                                    error_prefix,
                                    RNA_struct_identifier(ptr->type),
                                    RNA_property_identifier(prop));
            }
            else {
              PyErr_Format(PyExc_TypeError,
                           "%.200s %.200s.%.200s expected a bytes type, not %.200s",
                           error_prefix,
                           RNA_struct_identifier(ptr->type),
                           RNA_property_identifier(prop),
                           Py_TYPE(value)->tp_name);
            }

            return -1;
          }

          if (data) {
            if (flag & PROP_THICK_WRAP) {
              BLI_strncpy((char *)data, param, RNA_property_string_maxlength(prop));
            }
            else {
              *((char **)data) = (char *)param;
            }
          }
          else {
            RNA_property_string_set_bytes(ptr, prop, param, PyBytes_Size(value));
          }
        }
        else {
/* Unicode String. */
#ifdef USE_STRING_COERCE
          PyObject *value_coerce = nullptr;
          if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
            /* TODO: get size. */
            param = PyC_UnicodeAsBytes(value, &value_coerce);
          }
          else {
            param = PyUnicode_AsUTF8(value);
          }
#else  /* USE_STRING_COERCE */
          param = PyUnicode_AsUTF8(value);
#endif /* USE_STRING_COERCE */

          if (param == nullptr) {
            if (PyUnicode_Check(value)) {
              /* there was an error assigning a string type,
               * rather than setting a new error, prefix the existing one
               */
              PyC_Err_Format_Prefix(PyExc_TypeError,
                                    "%.200s %.200s.%.200s error assigning string",
                                    error_prefix,
                                    RNA_struct_identifier(ptr->type),
                                    RNA_property_identifier(prop));
            }
            else {
              PyErr_Format(PyExc_TypeError,
                           "%.200s %.200s.%.200s expected a string type, not %.200s",
                           error_prefix,
                           RNA_struct_identifier(ptr->type),
                           RNA_property_identifier(prop),
                           Py_TYPE(value)->tp_name);
            }

            return -1;
          }

          if ((flag & PROP_PATH_SUPPORTS_BLEND_RELATIVE) == 0) {
            if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH)) {
              if (BLI_path_is_rel(param)) {
                char warning_buf[256];
                SNPRINTF_UTF8(warning_buf,
                              "%.200s.%.200s: does not support blend relative \"//\" prefix",
                              RNA_struct_identifier(ptr->type),
                              RNA_property_identifier(prop));
                PyErr_WarnEx(PyExc_RuntimeWarning, warning_buf, 1);
              }
            }
          }

          /* Same as bytes (except for UTF8 string copy). */
          /* XXX, this is suspect, but needed for function calls,
           * need to see if there's a better way. */
          if (data) {
            if (flag & PROP_THICK_WRAP) {
              BLI_strncpy_utf8((char *)data, param, RNA_property_string_maxlength(prop));
            }
            else {
              *((char **)data) = (char *)param;
            }
          }
          else {
            RNA_property_string_set(ptr, prop, param);
          }

#ifdef USE_STRING_COERCE
          Py_XDECREF(value_coerce);
#endif /* USE_STRING_COERCE */
        }
        break;
      }
      case PROP_ENUM: {
        int val = 0;

        /* Type checking is done by each function. */
        if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
          /* Set of enum items, concatenate all values with OR. */
          if (pyrna_prop_to_enum_bitfield(ptr, prop, value, &val, error_prefix) == -1) {
            return -1;
          }
        }
        else {
          /* Simple enum string. */
          if (pyrna_string_to_enum(value, ptr, prop, &val, error_prefix) == -1) {
            return -1;
          }
        }

        if (data) {
          *((int *)data) = val;
        }
        else {
          RNA_property_enum_set(ptr, prop, val);
        }

        break;
      }
      case PROP_POINTER: {
        PyObject *value_new = nullptr;

        StructRNA *ptr_type = RNA_property_pointer_type(ptr, prop);
        const int flag = RNA_property_flag(prop);
        const int flag_parameter = RNA_parameter_flag(prop);

        /* This is really nasty! Done so we can fake the operator having direct properties, eg:
         * layout.prop(self, "filepath")
         * ... which in fact should be:
         * layout.prop(self.properties, "filepath")
         *
         * we need to do this trick.
         * if the prop is not an operator type and the PyObject is an operator,
         * use its properties in place of itself.
         *
         * This is so bad that it is almost a good reason to do away with fake
         * 'self.properties -> self'
         * class mixing. If this causes problems in the future it should be removed.
         */
        if ((ptr_type == &RNA_AnyType) && BPy_StructRNA_Check(value)) {
          const StructRNA *base_type = RNA_struct_base_child_of(
              ((const BPy_StructRNA *)value)->ptr->type, nullptr);
          if (ELEM(base_type, &RNA_Operator, &RNA_Gizmo)) {
            value = PyObject_GetAttr(value, bpy_intern_str_properties);
            value_new = value;
          }
        }

        /* if property is an OperatorProperties/GizmoProperties pointer and value is a map,
         * forward back to pyrna_pydict_to_props */
        if (PyDict_Check(value)) {
          const StructRNA *base_type = RNA_struct_base_child_of(ptr_type, nullptr);
          if (ELEM(base_type, &RNA_OperatorProperties, &RNA_GizmoProperties)) {
            PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
            if (opptr.type) {
              return pyrna_pydict_to_props(&opptr, value, false, error_prefix);
            }
            /* Converting a dictionary to properties is not supported
             * for function arguments, this would be nice to support but is complicated
             * because the operator type needs to be read from another function argument
             * and allocated data needs to be freed. See #135245. */

            /* This is only expected to happen for RNA functions. */
            BLI_assert(ptr->type == &RNA_Function);
            if (ptr->type != &RNA_Function) {
              PyErr_Format(PyExc_TypeError,
                           "%.200s %.200s.%.200s internal error coercing a dict for %.200s type",
                           error_prefix,
                           RNA_struct_identifier(ptr->type),
                           RNA_property_identifier(prop),
                           RNA_struct_identifier(ptr_type));
              return -1;
            }
          }
        }

        /* Another exception, allow passing a collection as an RNA property. */
        if (Py_TYPE(value) == &pyrna_prop_collection_Type) { /* Ok to ignore idprop collections. */
          PointerRNA c_ptr;
          BPy_PropertyRNA *value_prop = (BPy_PropertyRNA *)value;
          if (RNA_property_collection_type_get(&value_prop->ptr.value(), value_prop->prop, &c_ptr))
          {
            value = pyrna_struct_CreatePyObject(&c_ptr);
            value_new = value;
          }
          else {
            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s collection has no type, "
                         "cannot be used as a %.200s type",
                         error_prefix,
                         RNA_struct_identifier(ptr->type),
                         RNA_property_identifier(prop),
                         RNA_struct_identifier(ptr_type));
            return -1;
          }
        }

        BPy_StructRNA *param;
        if (value == Py_None) {
          if (flag & PROP_NEVER_NULL) {
            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s does not support a 'None' assignment %.200s type",
                         error_prefix,
                         RNA_struct_identifier(ptr->type),
                         RNA_property_identifier(prop),
                         RNA_struct_identifier(ptr_type));
            Py_XDECREF(value_new);
            return -1;
          }
          param = nullptr;
        }
        else {
          if (!BPy_StructRNA_Check(value)) {
            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s expected a %.200s type, not %.200s",
                         error_prefix,
                         RNA_struct_identifier(ptr->type),
                         RNA_property_identifier(prop),
                         RNA_struct_identifier(ptr_type),
                         Py_TYPE(value)->tp_name);
            Py_XDECREF(value_new);
            return -1;
          }
          param = (BPy_StructRNA *)value;

          const ID *value_owner_id = ((BPy_StructRNA *)value)->ptr->owner_id;
          if (value_owner_id != nullptr) {
            if ((flag & PROP_ID_SELF_CHECK) && (ptr->owner_id == value_owner_id)) {
              PyErr_Format(PyExc_TypeError,
                           "%.200s %.200s.%.200s ID type does not support assignment to itself",
                           error_prefix,
                           RNA_struct_identifier(ptr->type),
                           RNA_property_identifier(prop));
              Py_XDECREF(value_new);
              return -1;
            }

            if (value_owner_id->tag & ID_TAG_TEMP_MAIN) {
              /* Allow passing temporary ID's to functions, but not attribute assignment. */
              if (ptr->type != &RNA_Function) {
                PyErr_Format(PyExc_TypeError,
                             "%.200s %.200s.%.200s ID type assignment is temporary, cannot assign",
                             error_prefix,
                             RNA_struct_identifier(ptr->type),
                             RNA_property_identifier(prop));
                Py_XDECREF(value_new);
                return -1;
              }
            }
          }
        }

        bool raise_error = false;
        if (data) {

          if (flag_parameter & PARM_RNAPTR) {
            if (flag & PROP_THICK_WRAP) {
              if (param == nullptr) {
                *reinterpret_cast<PointerRNA *>(data) = {};
              }
              else if (RNA_struct_is_a(param->ptr->type, ptr_type)) {
                *reinterpret_cast<PointerRNA *>(data) = *param->ptr;
              }
              else {
                raise_error = true;
              }
            }
            else {
              /* For function calls, we sometimes want to pass the 'ptr' directly,
               * but watch out that it remains valid!
               * We could possibly support this later if needed. */
              BLI_assert(value_new == nullptr);
              if (param == nullptr) {
                *((void **)data) = nullptr;
              }
              else if (RNA_struct_is_a(param->ptr->type, ptr_type)) {
                *((PointerRNA **)data) = &param->ptr.value();
              }
              else {
                raise_error = true;
              }
            }
          }
          else if (param == nullptr) {
            *((void **)data) = nullptr;
          }
          else if (RNA_struct_is_a(param->ptr->type, ptr_type)) {
            *((void **)data) = param->ptr->data;
          }
          else {
            raise_error = true;
          }
        }
        else {
          /* Data == nullptr, assign to RNA. */
          if ((param == nullptr) || RNA_struct_is_a(param->ptr->type, ptr_type)) {
            ReportList reports;
            BKE_reports_init(&reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);
            RNA_property_pointer_set(
                ptr, prop, (param == nullptr) ? PointerRNA_NULL : *param->ptr, &reports);
            const int err = BPy_reports_to_error(&reports, PyExc_RuntimeError, true);
            if (err == -1) {
              Py_XDECREF(value_new);
              return -1;
            }
          }
          else {
            raise_error = true;
          }
        }

        if (raise_error) {
          if (pyrna_struct_validity_check(param) == -1) {
            /* Error set. */
          }
          else {
            PointerRNA tmp = RNA_pointer_create_discrete(nullptr, ptr_type, nullptr);
            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s expected a %.200s type, not %.200s",
                         error_prefix,
                         RNA_struct_identifier(ptr->type),
                         RNA_property_identifier(prop),
                         RNA_struct_identifier(tmp.type),
                         RNA_struct_identifier(param->ptr->type));
          }
          Py_XDECREF(value_new);
          return -1;
        }

        Py_XDECREF(value_new);

        break;
      }
      case PROP_COLLECTION: {
        Py_ssize_t seq_len, i;
        PyObject *item;
        PointerRNA itemptr;
        CollectionVector *lb;

        lb = (data) ? (CollectionVector *)data : nullptr;

        /* Convert a sequence of dict's into a collection. */
        if (!PySequence_Check(value)) {
          PyErr_Format(
              PyExc_TypeError,
              "%.200s %.200s.%.200s expected a sequence for an RNA collection, not %.200s",
              error_prefix,
              RNA_struct_identifier(ptr->type),
              RNA_property_identifier(prop),
              Py_TYPE(value)->tp_name);
          return -1;
        }

        seq_len = PySequence_Size(value);
        for (i = 0; i < seq_len; i++) {
          item = PySequence_GetItem(value, i);

          if (item == nullptr) {
            PyErr_Format(
                PyExc_TypeError,
                "%.200s %.200s.%.200s failed to get sequence index '%d' for an RNA collection",
                error_prefix,
                RNA_struct_identifier(ptr->type),
                RNA_property_identifier(prop),
                i);
            Py_XDECREF(item);
            return -1;
          }

          if (PyDict_Check(item) == 0) {
            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s expected a each sequence "
                         "member to be a dict for an RNA collection, not %.200s",
                         error_prefix,
                         RNA_struct_identifier(ptr->type),
                         RNA_property_identifier(prop),
                         Py_TYPE(item)->tp_name);
            Py_XDECREF(item);
            return -1;
          }

          if (lb) {
            lb->items.append(itemptr);
          }
          else {
            RNA_property_collection_add(ptr, prop, &itemptr);
          }

          if (pyrna_pydict_to_props(
                  &itemptr, item, true, "Converting a Python list to an RNA collection") == -1)
          {
            PyObject *msg = PyC_ExceptionBuffer();
            const char *msg_char = PyUnicode_AsUTF8(msg);
            PyErr_Clear();

            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s error converting a member of a collection "
                         "from a dicts into an RNA collection, failed with: %s",
                         error_prefix,
                         RNA_struct_identifier(ptr->type),
                         RNA_property_identifier(prop),
                         msg_char);

            Py_DECREF(item);
            Py_DECREF(msg);
            return -1;
          }
          Py_DECREF(item);
        }

        break;
      }
      default:
        PyErr_Format(PyExc_AttributeError,
                     "%.200s %.200s.%.200s unknown property type (pyrna_py_to_prop)",
                     error_prefix,
                     RNA_struct_identifier(ptr->type),
                     RNA_property_identifier(prop));
        return -1;
    }
  }

  /* Run RNA property functions. */
  if (RNA_property_update_check(prop)) {
    RNA_property_update(BPY_context_get(), ptr, prop);
  }

  return 0;
}

static PyObject *pyrna_prop_array_to_py_index(BPy_PropertyArrayRNA *self, int index)
{
  PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);
  return pyrna_py_from_array_index(self, &self->ptr.value(), self->prop, index);
}

static int pyrna_py_to_prop_array_index(BPy_PropertyArrayRNA *self, int index, PyObject *value)
{
  int ret = 0;
  PointerRNA *ptr = &self->ptr.value();
  PropertyRNA *prop = self->prop;

  const int totdim = RNA_property_array_dimension(ptr, prop, nullptr);

  if (totdim > 1) {
    // char error_str[512];
    if (pyrna_py_to_array_index(
            &self->ptr.value(), self->prop, self->arraydim, self->arrayoffset, index, value, "") ==
        -1)
    {
      /* Error is set. */
      ret = -1;
    }
  }
  else {
    /* See if we can coerce into a Python type - 'PropertyType'. */
    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN: {
        const int param = PyC_Long_AsBool(value);

        if (param == -1) {
          /* Error is set. */
          ret = -1;
        }
        else {
          RNA_property_boolean_set_index(ptr, prop, index, param);
        }
        break;
      }
      case PROP_INT: {
        int param = PyC_Long_AsI32(value);
        if (param == -1 && PyErr_Occurred()) {
          PyErr_SetString(PyExc_TypeError, "expected an int type");
          ret = -1;
        }
        else {
          RNA_property_int_clamp(ptr, prop, &param);
          RNA_property_int_set_index(ptr, prop, index, param);
        }
        break;
      }
      case PROP_FLOAT: {
        float param = PyFloat_AsDouble(value);
        if (PyErr_Occurred()) {
          PyErr_SetString(PyExc_TypeError, "expected a float type");
          ret = -1;
        }
        else {
          RNA_property_float_clamp(ptr, prop, &param);
          RNA_property_float_set_index(ptr, prop, index, param);
        }
        break;
      }
      default:
        PyErr_SetString(PyExc_AttributeError, "not an array type");
        ret = -1;
        break;
    }
  }

  /* Run RNA property functions. */
  if (RNA_property_update_check(prop)) {
    RNA_property_update(BPY_context_get(), ptr, prop);
  }

  return ret;
}

/* ---------------sequence------------------------------------------- */
static Py_ssize_t pyrna_prop_array_length(BPy_PropertyArrayRNA *self)
{
  PYRNA_PROP_CHECK_INT((BPy_PropertyRNA *)self);

  if (RNA_property_array_dimension(&self->ptr.value(), self->prop, nullptr) > 1) {
    return RNA_property_multi_array_length(&self->ptr.value(), self->prop, self->arraydim);
  }

  return RNA_property_array_length(&self->ptr.value(), self->prop);
}

static Py_ssize_t pyrna_prop_collection_length(BPy_PropertyRNA *self)
{
  PYRNA_PROP_CHECK_INT(self);

  return RNA_property_collection_length(&self->ptr.value(), self->prop);
}

/* bool functions are for speed, so we can avoid getting the length
 * of 1000's of items in a linked list for eg. */
static int pyrna_prop_array_bool(BPy_PropertyRNA *self)
{
  PYRNA_PROP_CHECK_INT(self);

  return RNA_property_array_length(&self->ptr.value(), self->prop) ? 1 : 0;
}

static int pyrna_prop_collection_bool(BPy_PropertyRNA *self)
{
  PYRNA_PROP_CHECK_INT(self);

  return !RNA_property_collection_is_empty(&self->ptr.value(), self->prop);
}

/* notice getting the length of the collection is avoided unless negative
 * index is used or to detect internal error with a valid index.
 * This is done for faster lookups. */
#define PYRNA_PROP_COLLECTION_ABS_INDEX(ret_err) \
  if (keynum < 0) { \
    keynum_abs += RNA_property_collection_length(&self->ptr.value(), self->prop); \
    if (keynum_abs < 0) { \
      PyErr_Format(PyExc_IndexError, "bpy_prop_collection[%d]: out of range.", keynum); \
      return ret_err; \
    } \
  } \
  (void)0

/**
 * \param result: The result of calling a subscription operation on a collection (never nullptr).
 */
static int pyrna_prop_collection_subscript_is_valid_or_error(const PyObject *value)
{
  if (value != Py_None) {
    BLI_assert(BPy_StructRNA_Check(value));
    const BPy_StructRNA *value_pyrna = (const BPy_StructRNA *)value;
    if (UNLIKELY(value_pyrna->ptr->type == nullptr)) {
      /* It's important to use a `TypeError` as that is what's returned when `__getitem__` is
       * called on an object that doesn't support item access. */
      PyErr_Format(PyExc_TypeError,
                   "'%.200s' object is not subscriptable (only iteration is supported)",
                   Py_TYPE(value)->tp_name);
      return -1;
    }
  }
  return 0;
}

static void pyrna_prop_collection_string_subscript_unsupported_error(BPy_PropertyRNA *self,
                                                                     const char *error_prefix)
{
  PyErr_Format(PyExc_TypeError,
               "%.200s: %.200s.%.200s does not support string lookups",
               error_prefix,
               RNA_struct_identifier(self->ptr->type),
               RNA_property_identifier(self->prop));
}

static int pyrna_prop_collection_string_subscript_supported_or_error(BPy_PropertyRNA *self,
                                                                     const char *error_prefix)
{
  BLI_assert(BPy_PropertyRNA_Check(self));
  if (RNA_property_collection_lookup_string_supported(self->prop)) {
    return 0;
  }
  pyrna_prop_collection_string_subscript_unsupported_error(self, error_prefix);
  return -1;
}

/* Internal use only. */
static PyObject *pyrna_prop_collection_subscript_int(BPy_PropertyRNA *self, Py_ssize_t keynum)
{
  PointerRNA newptr;
  Py_ssize_t keynum_abs = keynum;

  PYRNA_PROP_CHECK_OBJ(self);

  PYRNA_PROP_COLLECTION_ABS_INDEX(nullptr);

  if (RNA_property_collection_lookup_int_has_fn(self->prop)) {
    if (RNA_property_collection_lookup_int(&self->ptr.value(), self->prop, keynum_abs, &newptr)) {
      return pyrna_struct_CreatePyObject(&newptr);
    }
  }
  else {
    /* No callback defined, just iterate and find the nth item. */
    const int key = int(keynum_abs);
    PyObject *result = nullptr;
    bool found = false;
    CollectionPropertyIterator iter;
    RNA_property_collection_begin(&self->ptr.value(), self->prop, &iter);
    for (int i = 0; iter.valid; RNA_property_collection_next(&iter), i++) {
      if (i == key) {
        result = pyrna_struct_CreatePyObject(&iter.ptr);
        found = true;
        break;
      }
    }
    /* It's important to end the iterator after `result` has been created
     * so iterators may optionally invalidate items that were iterated over, see: #100286. */
    RNA_property_collection_end(&iter);
    if (found) {
      if (result && (pyrna_prop_collection_subscript_is_valid_or_error(result) == -1)) {
        Py_DECREF(result);
        result = nullptr; /* The exception has been set. */
      }
      return result;
    }
  }

  const int len = RNA_property_collection_length(&self->ptr.value(), self->prop);
  if (keynum_abs >= len) {
    PyErr_Format(PyExc_IndexError,
                 "bpy_prop_collection[index]: "
                 "index %d out of range, size %d",
                 keynum,
                 len);
  }
  else {
    PyErr_Format(PyExc_RuntimeError,
                 "bpy_prop_collection[index]: internal error, "
                 "valid index %d given in %d sized collection, but value not found",
                 keynum_abs,
                 len);
  }

  return nullptr;
}

/* Values type must have been already checked. */
static int pyrna_prop_collection_ass_subscript_int(BPy_PropertyRNA *self,
                                                   Py_ssize_t keynum,
                                                   PyObject *value)
{
  Py_ssize_t keynum_abs = keynum;
  const PointerRNA *ptr = (value == Py_None) ?
                              (&PointerRNA_NULL) :
                              &(reinterpret_cast<BPy_StructRNA *>(value))->ptr.value();

  PYRNA_PROP_CHECK_INT(self);

  PYRNA_PROP_COLLECTION_ABS_INDEX(-1);

  if (!RNA_property_collection_assign_int(&self->ptr.value(), self->prop, keynum_abs, ptr)) {
    const int len = RNA_property_collection_length(&self->ptr.value(), self->prop);
    if (keynum_abs >= len) {
      PyErr_Format(PyExc_IndexError,
                   "bpy_prop_collection[index] = value: "
                   "index %d out of range, size %d",
                   keynum,
                   len);
    }
    else {
      PyErr_Format(PyExc_IndexError,
                   "bpy_prop_collection[index] = value: "
                   "index %d failed assignment (unknown reason)",
                   keynum);
    }
    return -1;
  }

  return 0;
}

static PyObject *pyrna_prop_array_subscript_int(BPy_PropertyArrayRNA *self, Py_ssize_t keynum)
{
  int len;

  PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

  len = pyrna_prop_array_length(self);

  if (keynum < 0) {
    keynum += len;
  }

  if (keynum >= 0 && keynum < len) {
    return pyrna_prop_array_to_py_index(self, keynum);
  }

  PyErr_Format(PyExc_IndexError, "bpy_prop_array[index]: index %d out of range", keynum);
  return nullptr;
}

static PyObject *pyrna_prop_collection_subscript_str(BPy_PropertyRNA *self, const char *keyname)
{
  PointerRNA newptr;

  PYRNA_PROP_CHECK_OBJ(self);

  if (RNA_property_collection_lookup_string_has_fn(self->prop)) {
    if (RNA_property_collection_lookup_string(&self->ptr.value(), self->prop, keyname, &newptr)) {
      return pyrna_struct_CreatePyObject(&newptr);
    }
  }
  else if (RNA_property_collection_lookup_string_has_nameprop(self->prop)) {
    /* No callback defined, just iterate and find the nth item. */
    const int key_len = strlen(keyname);
    char name[256];
    int name_len;
    PyObject *result = nullptr;
    bool found = false;
    CollectionPropertyIterator iter;
    RNA_property_collection_begin(&self->ptr.value(), self->prop, &iter);
    for (; iter.valid; RNA_property_collection_next(&iter)) {
      PropertyRNA *nameprop = RNA_struct_name_property(iter.ptr.type);
      /* The #RNA_property_collection_lookup_string_has_nameprop check should account for this.
       * Although it's technically possible a sub-type clears the name property,
       * this seems unlikely. */
      BLI_assert(nameprop != nullptr);
      char *name_ptr = RNA_property_string_get_alloc(
          &iter.ptr, nameprop, name, sizeof(name), &name_len);
      if ((key_len == name_len) && STREQ(name_ptr, keyname)) {
        found = true;
      }
      if (name != name_ptr) {
        MEM_freeN(name_ptr);
      }
      if (found) {
        result = pyrna_struct_CreatePyObject(&iter.ptr);
        break;
      }
    }
    /* It's important to end the iterator after `result` has been created
     * so iterators may optionally invalidate items that were iterated over, see: #100286. */
    RNA_property_collection_end(&iter);
    if (found) {
      if (result && (pyrna_prop_collection_subscript_is_valid_or_error(result) == -1)) {
        Py_DECREF(result);
        result = nullptr; /* The exception has been set. */
      }
      return result;
    }
  }
  else {
    pyrna_prop_collection_string_subscript_unsupported_error(self, "bpy_prop_collection[key]");
    return nullptr;
  }

  PyErr_Format(PyExc_KeyError, "bpy_prop_collection[key]: key \"%.200s\" not found", keyname);
  return nullptr;
}
// static PyObject *pyrna_prop_array_subscript_str(BPy_PropertyRNA *self, char *keyname)

/**
 * Special case: `bpy.data.objects["some_id_name", "//some_lib_name.blend"]`
 * also for:     `bpy.data.objects.get(("some_id_name", "//some_lib_name.blend"), fallback)`
 *
 * \note
 * error codes since this is not to be called directly from Python,
 * this matches Python's `__contains__` values C-API.
 * - -1: exception set
 * -  0: not found
 * -  1: found
 */
static int pyrna_prop_collection_subscript_str_lib_pair_ptr(BPy_PropertyRNA *self,
                                                            PyObject *key,
                                                            const char *err_prefix,
                                                            const short err_not_found,
                                                            PointerRNA *r_ptr)
{
  const char *keyname;

  /* First validate the args, all we know is that they are a tuple. */
  if (PyTuple_GET_SIZE(key) != 2) {
    PyErr_Format(PyExc_KeyError,
                 "%s: tuple key must be a pair, not size %d",
                 err_prefix,
                 PyTuple_GET_SIZE(key));
    return -1;
  }
  if (self->ptr->type != &RNA_BlendData) {
    PyErr_Format(PyExc_KeyError,
                 "%s: is only valid for bpy.data collections, not %.200s",
                 err_prefix,
                 RNA_struct_identifier(self->ptr->type));
    return -1;
  }
  if ((keyname = PyUnicode_AsUTF8(PyTuple_GET_ITEM(key, 0))) == nullptr) {
    PyErr_Format(PyExc_KeyError,
                 "%s: id must be a string, not %.200s",
                 err_prefix,
                 Py_TYPE(PyTuple_GET_ITEM(key, 0))->tp_name);
    return -1;
  }

  PyObject *keylib = PyTuple_GET_ITEM(key, 1);
  Library *lib;
  bool found = false;

  if (keylib == Py_None) {
    lib = nullptr;
  }
  else if (PyUnicode_Check(keylib)) {
    Main *bmain = static_cast<Main *>(self->ptr->data);
    const char *keylib_str = PyUnicode_AsUTF8(keylib);
    lib = static_cast<Library *>(
        BLI_findstring(&bmain->libraries, keylib_str, offsetof(Library, filepath)));
    if (lib == nullptr) {
      if (err_not_found) {
        PyErr_Format(PyExc_KeyError,
                     "%s: lib filepath '%.1024s' "
                     "does not reference a valid library",
                     err_prefix,
                     keylib_str);
        return -1;
      }

      return 0;
    }
  }
  else {
    PyErr_Format(PyExc_KeyError,
                 "%s: lib must be a string or None, not %.200s",
                 err_prefix,
                 Py_TYPE(keylib)->tp_name);
    return -1;
  }

  /* lib is either a valid pointer or nullptr,
   * either way can do direct comparison with id.lib */

  RNA_PROP_BEGIN (&self->ptr.value(), itemptr, self->prop) {
    ID *id = static_cast<ID *>(itemptr.data); /* Always an ID. */
    if (id->lib == lib && STREQLEN(keyname, id->name + 2, sizeof(id->name) - 2)) {
      found = true;
      if (r_ptr) {
        *r_ptr = itemptr;
      }
      break;
    }
  }
  RNA_PROP_END;

  /* We may want to fail silently as with collection.get(). */
  if ((found == false) && err_not_found) {
    /* Only runs for getitem access so use fixed string. */
    PyErr_SetString(PyExc_KeyError, "bpy_prop_collection[key, lib]: not found");
    return -1;
  }

  return found; /* 1 / 0, no exception. */
}

static PyObject *pyrna_prop_collection_subscript_str_lib_pair(BPy_PropertyRNA *self,
                                                              PyObject *key,
                                                              const char *err_prefix,
                                                              const bool err_not_found)
{
  PointerRNA ptr;
  const int contains = pyrna_prop_collection_subscript_str_lib_pair_ptr(
      self, key, err_prefix, err_not_found, &ptr);

  if (contains == 1) {
    return pyrna_struct_CreatePyObject(&ptr);
  }

  return nullptr;
}

static PyObject *pyrna_prop_collection_subscript_slice(BPy_PropertyRNA *self,
                                                       Py_ssize_t start,
                                                       Py_ssize_t stop)
{
  CollectionPropertyIterator rna_macro_iter;
  int count;

  PyObject *list;
  PyObject *item;

  PYRNA_PROP_CHECK_OBJ(self);

  list = PyList_New(0);

  /* Skip to start. */
  RNA_property_collection_begin(&self->ptr.value(), self->prop, &rna_macro_iter);
  RNA_property_collection_skip(&rna_macro_iter, start);

  /* Add items until stop. */
  for (count = start; rna_macro_iter.valid; RNA_property_collection_next(&rna_macro_iter)) {
    item = pyrna_struct_CreatePyObject(&rna_macro_iter.ptr);
    PyList_APPEND(list, item);

    count++;
    if (count == stop) {
      break;
    }
  }

  RNA_property_collection_end(&rna_macro_iter);

  return list;
}

/**
 * TODO: dimensions
 * \note Could also use pyrna_prop_array_to_py_index(self, count) in a loop, but it's much slower
 * since at the moment it reads (and even allocates) the entire array for each index.
 */
static PyObject *pyrna_prop_array_subscript_slice(BPy_PropertyArrayRNA *self,
                                                  PointerRNA *ptr,
                                                  PropertyRNA *prop,
                                                  Py_ssize_t start,
                                                  Py_ssize_t stop,
                                                  Py_ssize_t length)
{
  int count, totdim;
  PyObject *tuple;

  /* Isn't needed, internal use only. */
  // PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

  tuple = PyTuple_New(stop - start);

  totdim = RNA_property_array_dimension(ptr, prop, nullptr);

  if (totdim > 1) {
    for (count = start; count < stop; count++) {
      PyTuple_SET_ITEM(tuple, count - start, pyrna_prop_array_to_py_index(self, count));
    }
  }
  else {
    switch (RNA_property_type(prop)) {
      case PROP_FLOAT: {
        float values_stack[PYRNA_STACK_ARRAY];
        float *values;
        if (length > PYRNA_STACK_ARRAY) {
          values = static_cast<float *>(PyMem_MALLOC(sizeof(float) * length));
        }
        else {
          values = values_stack;
        }
        RNA_property_float_get_array(ptr, prop, values);

        for (count = start; count < stop; count++) {
          PyTuple_SET_ITEM(tuple, count - start, PyFloat_FromDouble(values[count]));
        }

        if (values != values_stack) {
          PyMem_FREE(values);
        }
        break;
      }
      case PROP_BOOLEAN: {
        bool values_stack[PYRNA_STACK_ARRAY];
        bool *values;
        if (length > PYRNA_STACK_ARRAY) {
          values = static_cast<bool *>(PyMem_MALLOC(sizeof(bool) * length));
        }
        else {
          values = values_stack;
        }

        RNA_property_boolean_get_array(ptr, prop, values);
        for (count = start; count < stop; count++) {
          PyTuple_SET_ITEM(tuple, count - start, PyBool_FromLong(values[count]));
        }

        if (values != values_stack) {
          PyMem_FREE(values);
        }
        break;
      }
      case PROP_INT: {
        int values_stack[PYRNA_STACK_ARRAY];
        int *values;
        if (length > PYRNA_STACK_ARRAY) {
          values = static_cast<int *>(PyMem_MALLOC(sizeof(int) * length));
        }
        else {
          values = values_stack;
        }

        RNA_property_int_get_array(ptr, prop, values);
        for (count = start; count < stop; count++) {
          PyTuple_SET_ITEM(tuple, count - start, PyLong_FromLong(values[count]));
        }

        if (values != values_stack) {
          PyMem_FREE(values);
        }
        break;
      }
      default:
        BLI_assert_msg(0, "Invalid array type");

        PyErr_SetString(PyExc_TypeError, "not an array type");
        Py_DECREF(tuple);
        tuple = nullptr;
        break;
    }
  }
  return tuple;
}

static PyObject *pyrna_prop_collection_subscript(BPy_PropertyRNA *self, PyObject *key)
{
  PYRNA_PROP_CHECK_OBJ(self);

  if (PyUnicode_Check(key)) {
    return pyrna_prop_collection_subscript_str(self, PyUnicode_AsUTF8(key));
  }
  if (PyIndex_Check(key)) {
    const Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }

    return pyrna_prop_collection_subscript_int(self, i);
  }
  if (PySlice_Check(key)) {
    PySliceObject *key_slice = (PySliceObject *)key;
    Py_ssize_t step = 1;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return nullptr;
    }
    if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "bpy_prop_collection[slice]: slice steps not supported");
      return nullptr;
    }
    if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
    }

    Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

    /* Avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
    if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) {
      return nullptr;
    }
    if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop)) {
      return nullptr;
    }

    if (start < 0 || stop < 0) {
      /* Only get the length for negative values. */
      const Py_ssize_t len = (Py_ssize_t)RNA_property_collection_length(&self->ptr.value(),
                                                                        self->prop);
      if (start < 0) {
        start += len;
        CLAMP_MIN(start, 0);
      }
      if (stop < 0) {
        stop += len;
        CLAMP_MIN(stop, 0);
      }
    }

    if (stop - start <= 0) {
      return PyList_New(0);
    }

    return pyrna_prop_collection_subscript_slice(self, start, stop);
  }
  if (PyTuple_Check(key)) {
    /* Special case, for ID data-blocks. */
    return pyrna_prop_collection_subscript_str_lib_pair(
        self, key, "bpy_prop_collection[id, lib]", true);
  }

  PyErr_Format(PyExc_TypeError,
               "bpy_prop_collection[key]: invalid key, "
               "must be a string or an int, not %.200s",
               Py_TYPE(key)->tp_name);
  return nullptr;
}

/* generic check to see if a PyObject is compatible with a collection
 * -1 on failure, 0 on success, sets the error */
static int pyrna_prop_collection_type_check(BPy_PropertyRNA *self, PyObject *value)
{
  StructRNA *prop_srna;

  if (value == Py_None) {
    if (RNA_property_flag(self->prop) & PROP_NEVER_NULL) {
      PyErr_SetString(PyExc_TypeError,
                      "bpy_prop_collection[key] = value: invalid, "
                      "this collection doesn't support None assignment");
      return -1;
    }

    return 0; /* None is OK. */
  }
  if (BPy_StructRNA_Check(value) == 0) {
    PyErr_Format(PyExc_TypeError,
                 "bpy_prop_collection[key] = value: invalid, "
                 "expected a StructRNA type or None, not a %.200s",
                 Py_TYPE(value)->tp_name);
    return -1;
  }
  if ((prop_srna = RNA_property_pointer_type(&self->ptr.value(), self->prop))) {
    StructRNA *value_srna = ((BPy_StructRNA *)value)->ptr->type;
    if (RNA_struct_is_a(value_srna, prop_srna) == 0) {
      PyErr_Format(PyExc_TypeError,
                   "bpy_prop_collection[key] = value: invalid, "
                   "expected a '%.200s' type or None, not a '%.200s'",
                   RNA_struct_identifier(prop_srna),
                   RNA_struct_identifier(value_srna));
      return -1;
    }

    return 0; /* OK, this is the correct type! */
  }

  PyErr_SetString(PyExc_TypeError,
                  "bpy_prop_collection[key] = value: internal error, "
                  "failed to get the collection type");
  return -1;
}

/* NOTE: currently this is a copy of 'pyrna_prop_collection_subscript' with
 * large blocks commented, we may support slice/key indices later */
static int pyrna_prop_collection_ass_subscript(BPy_PropertyRNA *self,
                                               PyObject *key,
                                               PyObject *value)
{
  PYRNA_PROP_CHECK_INT(self);

  /* Validate the assigned value. */
  if (value == nullptr) {
    PyErr_SetString(PyExc_TypeError, "del bpy_prop_collection[key]: not supported");
    return -1;
  }
  if (pyrna_prop_collection_type_check(self, value) == -1) {
    return -1; /* Exception is set. */
  }

#if 0
  if (PyUnicode_Check(key)) {
    return pyrna_prop_collection_subscript_str(self, PyUnicode_AsUTF8(key));
  }
  else
#endif
  if (PyIndex_Check(key)) {
    const Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }

    return pyrna_prop_collection_ass_subscript_int(self, i, value);
  }
#if 0 /* TODO: fake slice assignment. */
  else if (PySlice_Check(key)) {
    PySliceObject *key_slice = (PySliceObject *)key;
    Py_ssize_t step = 1;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return nullptr;
    }
    else if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "bpy_prop_collection[slice]: slice steps not supported");
      return nullptr;
    }
    else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
    }
    else {
      Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

      /* Avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
      if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) {
        return nullptr;
      }
      if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop)) {
        return nullptr;
      }

      if (start < 0 || stop < 0) {
        /* Only get the length for negative values. */
        Py_ssize_t len = (Py_ssize_t)RNA_property_collection_length(&self->ptr.value(), self->prop);
        if (start < 0) {
          start += len;
          CLAMP_MIN(start, 0);
        }
        if (stop < 0) {
          stop += len;
          CLAMP_MIN(stop, 0);
        }
      }

      if (stop - start <= 0) {
        return PyList_New(0);
      }
      else {
        return pyrna_prop_collection_subscript_slice(self, start, stop);
      }
    }
  }
#endif

  PyErr_Format(PyExc_TypeError,
               "bpy_prop_collection[key]: invalid key, "
               "must be an int, not %.200s",
               Py_TYPE(key)->tp_name);
  return -1;
}

static PyObject *pyrna_prop_array_subscript(BPy_PropertyArrayRNA *self, PyObject *key)
{
  PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

#if 0
  if (PyUnicode_Check(key)) {
    return pyrna_prop_array_subscript_str(self, PyUnicode_AsUTF8(key));
  }
  else
#endif
  if (PyIndex_Check(key)) {
    const Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }
    return pyrna_prop_array_subscript_int(self, i);
  }
  if (PySlice_Check(key)) {
    Py_ssize_t step = 1;
    PySliceObject *key_slice = (PySliceObject *)key;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return nullptr;
    }
    if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "bpy_prop_array[slice]: slice steps not supported");
      return nullptr;
    }
    if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      /* NOTE: no significant advantage with optimizing [:] slice as with collections,
       * but include here for consistency with collection slice func */
      const Py_ssize_t len = pyrna_prop_array_length(self);
      return pyrna_prop_array_subscript_slice(self, &self->ptr.value(), self->prop, 0, len, len);
    }

    const int len = pyrna_prop_array_length(self);
    Py_ssize_t start, stop, slicelength;

    if (PySlice_GetIndicesEx(key, len, &start, &stop, &step, &slicelength) < 0) {
      return nullptr;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }

    return pyrna_prop_array_subscript_slice(
        self, &self->ptr.value(), self->prop, start, stop, len);
  }

  PyErr_SetString(PyExc_AttributeError, "bpy_prop_array[key]: invalid key, key must be an int");
  return nullptr;
}

/**
 * Helpers for #prop_subscript_ass_array_slice
 */

static PyObject *prop_subscript_ass_array_slice__as_seq_fast(PyObject *value, int length)
{
  PyObject *value_fast;
  if (!(value_fast = PySequence_Fast(value,
                                     "bpy_prop_array[slice] = value: "
                                     "element in assignment is not a sequence type")))
  {
    return nullptr;
  }
  if (PySequence_Fast_GET_SIZE(value_fast) != length) {
    Py_DECREF(value_fast);
    PyErr_SetString(PyExc_ValueError,
                    "bpy_prop_array[slice] = value: "
                    "re-sizing bpy_struct element in arrays isn't supported");
    return nullptr;
  }

  return value_fast;
}

static int prop_subscript_ass_array_slice__float_recursive(
    PyObject **value_items, float *value, int totdim, const int dimsize[], const float range[2])
{
  const int length = dimsize[0];
  if (totdim > 1) {
    int index = 0;
    int i;
    for (i = 0; i != length; i++) {
      PyObject *subvalue = prop_subscript_ass_array_slice__as_seq_fast(value_items[i], dimsize[1]);
      if (UNLIKELY(subvalue == nullptr)) {
        return 0;
      }

      index += prop_subscript_ass_array_slice__float_recursive(
          PySequence_Fast_ITEMS(subvalue), &value[index], totdim - 1, &dimsize[1], range);

      Py_DECREF(subvalue);
    }
    return index;
  }

  BLI_assert(totdim == 1);
  const float min = range[0], max = range[1];
  int i;
  for (i = 0; i != length; i++) {
    float v = PyFloat_AsDouble(value_items[i]);
    CLAMP(v, min, max);
    value[i] = v;
  }
  return i;
}

static int prop_subscript_ass_array_slice__int_recursive(
    PyObject **value_items, int *value, int totdim, const int dimsize[], const int range[2])
{
  const int length = dimsize[0];
  if (totdim > 1) {
    int index = 0;
    int i;
    for (i = 0; i != length; i++) {
      PyObject *subvalue = prop_subscript_ass_array_slice__as_seq_fast(value_items[i], dimsize[1]);
      if (UNLIKELY(subvalue == nullptr)) {
        return 0;
      }

      index += prop_subscript_ass_array_slice__int_recursive(
          PySequence_Fast_ITEMS(subvalue), &value[index], totdim - 1, &dimsize[1], range);

      Py_DECREF(subvalue);
    }
    return index;
  }

  BLI_assert(totdim == 1);
  const int min = range[0], max = range[1];
  int i;
  for (i = 0; i != length; i++) {
    int v = PyLong_AsLong(value_items[i]);
    CLAMP(v, min, max);
    value[i] = v;
  }
  return i;
}

static int prop_subscript_ass_array_slice__bool_recursive(PyObject **value_items,
                                                          bool *value,
                                                          int totdim,
                                                          const int dimsize[])
{
  const int length = dimsize[0];
  if (totdim > 1) {
    int index = 0;
    int i;
    for (i = 0; i != length; i++) {
      PyObject *subvalue = prop_subscript_ass_array_slice__as_seq_fast(value_items[i], dimsize[1]);
      if (UNLIKELY(subvalue == nullptr)) {
        return 0;
      }

      index += prop_subscript_ass_array_slice__bool_recursive(
          PySequence_Fast_ITEMS(subvalue), &value[index], totdim - 1, &dimsize[1]);

      Py_DECREF(subvalue);
    }
    return index;
  }

  BLI_assert(totdim == 1);
  int i;
  for (i = 0; i != length; i++) {
    const int v = PyLong_AsLong(value_items[i]);
    value[i] = v;
  }
  return i;
}

/* Could call `pyrna_py_to_prop_array_index(self, i, value)` in a loop, but it is slow. */
static int prop_subscript_ass_array_slice(PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          int arraydim,
                                          int arrayoffset,
                                          int start,
                                          int stop,
                                          int length,
                                          PyObject *value_orig)
{
  const int length_flat = RNA_property_array_length(ptr, prop);
  PyObject *value;
  void *values_alloc = nullptr;
  int ret = 0;

  if (value_orig == nullptr) {
    PyErr_SetString(
        PyExc_TypeError,
        "bpy_prop_array[slice] = value: deleting with list types is not supported by bpy_struct");
    return -1;
  }

  if (!(value = PySequence_Fast(
            value_orig, "bpy_prop_array[slice] = value: assignment is not a sequence type")))
  {
    return -1;
  }

  if (PySequence_Fast_GET_SIZE(value) != stop - start) {
    Py_DECREF(value);
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_array[slice] = value: re-sizing bpy_struct arrays isn't supported");
    return -1;
  }

  int dimsize[3];
  const int totdim = RNA_property_array_dimension(ptr, prop, dimsize);
  if (totdim > 1) {
    BLI_assert(dimsize[arraydim] == length);
  }

  int span = 1;
  if (totdim > 1) {
    for (int i = arraydim + 1; i < totdim; i++) {
      span *= dimsize[i];
    }
  }

  /* Assigning as subset of the whole array.
   *
   * When false, the whole array is being assigned, otherwise the array be read into `values`,
   * the subset updated & the whole array written back (since RNA doesn't support sub-ranges). */
  const bool is_subset = start != 0 || stop != length || arrayoffset != 0 || arraydim != 0;

  PyObject **value_items = PySequence_Fast_ITEMS(value);
  switch (RNA_property_type(prop)) {
    case PROP_FLOAT: {
      float values_stack[PYRNA_STACK_ARRAY];
      float *values = static_cast<float *>(
          (length_flat > PYRNA_STACK_ARRAY) ?
              (values_alloc = PyMem_MALLOC(sizeof(*values) * length_flat)) :
              values_stack);
      if (is_subset) {
        RNA_property_float_get_array(ptr, prop, values);
      }

      float range[2];
      RNA_property_float_range(ptr, prop, &range[0], &range[1]);

      dimsize[arraydim] = stop - start;
      prop_subscript_ass_array_slice__float_recursive(value_items,
                                                      &values[arrayoffset + (start * span)],
                                                      totdim - arraydim,
                                                      &dimsize[arraydim],
                                                      range);

      if (PyErr_Occurred()) {
        ret = -1;
      }
      else {
        RNA_property_float_set_array(ptr, prop, values);
      }
      break;
    }
    case PROP_INT: {
      int values_stack[PYRNA_STACK_ARRAY];
      int *values = static_cast<int *>(
          (length_flat > PYRNA_STACK_ARRAY) ?
              (values_alloc = PyMem_MALLOC(sizeof(*values) * length_flat)) :
              values_stack);
      if (is_subset) {
        RNA_property_int_get_array(ptr, prop, values);
      }

      int range[2];
      RNA_property_int_range(ptr, prop, &range[0], &range[1]);

      dimsize[arraydim] = stop - start;
      prop_subscript_ass_array_slice__int_recursive(value_items,
                                                    &values[arrayoffset + (start * span)],
                                                    totdim - arraydim,
                                                    &dimsize[arraydim],
                                                    range);

      if (PyErr_Occurred()) {
        ret = -1;
      }
      else {
        RNA_property_int_set_array(ptr, prop, values);
      }
      break;
    }
    case PROP_BOOLEAN: {
      bool values_stack[PYRNA_STACK_ARRAY];
      bool *values = static_cast<bool *>(
          (length_flat > PYRNA_STACK_ARRAY) ?
              (values_alloc = PyMem_MALLOC(sizeof(bool) * length_flat)) :
              values_stack);

      if (is_subset) {
        RNA_property_boolean_get_array(ptr, prop, values);
      }

      dimsize[arraydim] = stop - start;
      prop_subscript_ass_array_slice__bool_recursive(value_items,
                                                     &values[arrayoffset + (start * span)],
                                                     totdim - arraydim,
                                                     &dimsize[arraydim]);

      if (PyErr_Occurred()) {
        ret = -1;
      }
      else {
        RNA_property_boolean_set_array(ptr, prop, values);
      }
      break;
    }
    default:
      PyErr_SetString(PyExc_TypeError, "not an array type");
      ret = -1;
      break;
  }

  Py_DECREF(value);

  if (values_alloc) {
    PyMem_FREE(values_alloc);
  }

  return ret;
}

static int prop_subscript_ass_array_int(BPy_PropertyArrayRNA *self,
                                        Py_ssize_t keynum,
                                        PyObject *value)
{
  PYRNA_PROP_CHECK_INT((BPy_PropertyRNA *)self);

  int len = pyrna_prop_array_length(self);

  if (keynum < 0) {
    keynum += len;
  }

  if (keynum >= 0 && keynum < len) {
    return pyrna_py_to_prop_array_index(self, keynum, value);
  }

  PyErr_SetString(PyExc_IndexError, "bpy_prop_array[index] = value: index out of range");
  return -1;
}

static int pyrna_prop_array_ass_subscript(BPy_PropertyArrayRNA *self,
                                          PyObject *key,
                                          PyObject *value)
{
  // char *keyname = nullptr; /* Not supported yet. */
  int ret = -1;

  PYRNA_PROP_CHECK_INT((BPy_PropertyRNA *)self);

  if (!RNA_property_editable_flag(&self->ptr.value(), self->prop)) {
    PyErr_Format(PyExc_AttributeError,
                 "bpy_prop_collection: attribute \"%.200s\" from \"%.200s\" is read-only",
                 RNA_property_identifier(self->prop),
                 RNA_struct_identifier(self->ptr->type));
    ret = -1;
  }

  else if (PyIndex_Check(key)) {
    const Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      ret = -1;
    }
    else {
      ret = prop_subscript_ass_array_int(self, i, value);
    }
  }
  else if (PySlice_Check(key)) {
    const Py_ssize_t len = pyrna_prop_array_length(self);
    Py_ssize_t start, stop, step, slicelength;

    if (PySlice_GetIndicesEx(key, len, &start, &stop, &step, &slicelength) < 0) {
      ret = -1;
    }
    else if (slicelength <= 0) {
      ret = 0; /* Do nothing. */
    }
    else if (step == 1) {
      ret = prop_subscript_ass_array_slice(&self->ptr.value(),
                                           self->prop,
                                           self->arraydim,
                                           self->arrayoffset,
                                           start,
                                           stop,
                                           len,
                                           value);
    }
    else {
      PyErr_SetString(PyExc_TypeError, "slice steps not supported with RNA");
      ret = -1;
    }
  }
  else {
    PyErr_SetString(PyExc_AttributeError, "invalid key, key must be an int");
    ret = -1;
  }

  if (ret != -1) {
    if (RNA_property_update_check(self->prop)) {
      RNA_property_update(BPY_context_get(), &self->ptr.value(), self->prop);
    }
  }

  return ret;
}

/* For slice only. */
static PyMappingMethods pyrna_prop_array_as_mapping = {
    /*mp_length*/ (lenfunc)pyrna_prop_array_length,
    /*mp_subscript*/ (binaryfunc)pyrna_prop_array_subscript,
    /*mp_ass_subscript*/ (objobjargproc)pyrna_prop_array_ass_subscript,
};

static PyMappingMethods pyrna_prop_collection_as_mapping = {
    /*mp_length*/ (lenfunc)pyrna_prop_collection_length,
    /*mp_subscript*/ (binaryfunc)pyrna_prop_collection_subscript,
    /*mp_ass_subscript*/ (objobjargproc)pyrna_prop_collection_ass_subscript,
};

/* Only for fast bool's, large structs, assign nb_bool on init. */
static PyNumberMethods pyrna_prop_array_as_number = {
    /*nb_add*/ nullptr,
    /*nb_subtract*/ nullptr,
    /*nb_multiply*/ nullptr,
    /*nb_remainder*/ nullptr,
    /*nb_divmod*/ nullptr,
    /*nb_power*/ nullptr,
    /*nb_negative*/ nullptr,
    /*nb_positive*/ nullptr,
    /*nb_absolute*/ nullptr,
    /*nb_bool*/ (inquiry)pyrna_prop_array_bool,
};
static PyNumberMethods pyrna_prop_collection_as_number = {
    /*nb_add*/ nullptr,
    /*nb_subtract*/ nullptr,
    /*nb_multiply*/ nullptr,
    /*nb_remainder*/ nullptr,
    /*nb_divmod*/ nullptr,
    /*nb_power*/ nullptr,
    /*nb_negative*/ nullptr,
    /*nb_positive*/ nullptr,
    /*nb_absolute*/ nullptr,
    /*nb_bool*/ (inquiry)pyrna_prop_collection_bool,
    /*nb_invert*/ nullptr,
    /*nb_lshift*/ nullptr,
    /*nb_rshift*/ nullptr,
    /*nb_and*/ nullptr,
    /*nb_xor*/ nullptr,
    /*nb_or*/ nullptr,
    /*nb_int*/ nullptr,
    /*nb_reserved*/ nullptr,
    /*nb_float*/ nullptr,
    /*nb_inplace_add*/ nullptr,
    /*nb_inplace_subtract*/ nullptr,
    /*nb_inplace_multiply*/ nullptr,
    /*nb_inplace_remainder*/ nullptr,
    /*nb_inplace_power*/ nullptr,
    /*nb_inplace_lshift*/ nullptr,
    /*nb_inplace_rshift*/ nullptr,
    /*nb_inplace_and*/ nullptr,
    /*nb_inplace_xor*/ nullptr,
    /*nb_inplace_or*/ nullptr,
    /*nb_floor_divide*/ nullptr,
    /*nb_true_divide*/ nullptr,
    /*nb_inplace_floor_divide*/ nullptr,
    /*nb_inplace_true_divide*/ nullptr,
    /*nb_index*/ nullptr,
    /*nb_matrix_multiply*/ nullptr,
    /*nb_inplace_matrix_multiply*/ nullptr,
};

static int pyrna_prop_array_contains(BPy_PropertyRNA *self, PyObject *value)
{
  return pyrna_array_contains_py(&self->ptr.value(), self->prop, value);
}

static int pyrna_prop_collection_contains(BPy_PropertyRNA *self, PyObject *key)
{
  PointerRNA newptr; /* Not used, just so RNA_property_collection_lookup_string runs. */

  if (PyTuple_Check(key)) {
    /* Special case, for ID data-blocks. */
    return pyrna_prop_collection_subscript_str_lib_pair_ptr(
        self, key, "(id, lib) in bpy_prop_collection", false, nullptr);
  }

  /* Key in dict style check. */
  const char *keyname = PyUnicode_AsUTF8(key);

  if (keyname == nullptr) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_collection.__contains__: expected a string or a tuple of strings");
    return -1;
  }

  if (RNA_property_collection_lookup_string(&self->ptr.value(), self->prop, keyname, &newptr)) {
    return 1;
  }
  if (pyrna_prop_collection_string_subscript_supported_or_error(
          self, "bpy_prop_collection.__contains__") == -1)
  {
    return -1;
  }

  return 0;
}

static int pyrna_struct_contains(BPy_StructRNA *self, PyObject *value)
{
  const char *name = PyUnicode_AsUTF8(value);

  PYRNA_STRUCT_CHECK_INT(self);

  if (!name) {
    PyErr_SetString(PyExc_TypeError, "bpy_struct.__contains__: expected a string");
    return -1;
  }

  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "bpy_struct: this type doesn't support IDProperties");
    return -1;
  }

  IDProperty *group = RNA_struct_idprops(&self->ptr.value(), false);

  if (!group) {
    return 0;
  }

  return IDP_GetPropertyFromGroup(group, name) ? 1 : 0;
}

static PySequenceMethods pyrna_prop_array_as_sequence = {
    /*sq_length*/ (lenfunc)pyrna_prop_array_length,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /* Only set this so `PySequence_Check()` returns True. */
    /*sq_item*/ (ssizeargfunc)pyrna_prop_array_subscript_int,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ (ssizeobjargproc)prop_subscript_ass_array_int,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ (objobjproc)pyrna_prop_array_contains,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PySequenceMethods pyrna_prop_collection_as_sequence = {
    /*sq_length*/ (lenfunc)pyrna_prop_collection_length,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /* Only set this so PySequence_Check() returns True */
    /*sq_item*/ (ssizeargfunc)pyrna_prop_collection_subscript_int,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /* Let mapping take this one: #pyrna_prop_collection_ass_subscript_int */
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ (objobjproc)pyrna_prop_collection_contains,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PySequenceMethods pyrna_struct_as_sequence = {
    /*sq_length*/ nullptr, /* Can't set the len otherwise it can evaluate as false */
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /* Only set this so `PySequence_Check()` returns True. */
    /*sq_item*/ nullptr,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ (objobjproc)pyrna_struct_contains,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyObject *pyrna_struct_subscript(BPy_StructRNA *self, PyObject *key)
{
  /* Mostly copied from BPy_IDGroup_Map_GetItem. */
  const char *name = PyUnicode_AsUTF8(key);

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "this type doesn't support IDProperties");
    return nullptr;
  }

  if (name == nullptr) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_struct[key]: only strings are allowed as keys of ID properties");
    return nullptr;
  }

  IDProperty *group = RNA_struct_idprops(&self->ptr.value(), false);

  if (group == nullptr) {
    PyErr_Format(PyExc_KeyError, "bpy_struct[key]: key \"%s\" not found", name);
    return nullptr;
  }

  IDProperty *idprop = IDP_GetPropertyFromGroup(group, name);

  if (idprop == nullptr) {
    PyErr_Format(PyExc_KeyError, "bpy_struct[key]: key \"%s\" not found", name);
    return nullptr;
  }

  return BPy_IDGroup_WrapData(self->ptr->owner_id, idprop, group);
}

static int pyrna_struct_ass_subscript(BPy_StructRNA *self, PyObject *key, PyObject *value)
{
  PYRNA_STRUCT_CHECK_INT(self);

  IDProperty *group = RNA_struct_idprops(&self->ptr.value(), true);

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), key)) {
    return -1;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (group == nullptr) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_struct[key] = val: id properties not supported for this type");
    return -1;
  }

  if (value && BPy_StructRNA_Check(value)) {
    BPy_StructRNA *val = (BPy_StructRNA *)value;
    if (val && self->ptr->type && val->ptr->type) {
      if (!RNA_struct_idprops_datablock_allowed(self->ptr->type) &&
          RNA_struct_idprops_contains_datablock(val->ptr->type))
      {
        PyErr_SetString(
            PyExc_TypeError,
            "bpy_struct[key] = val: data-block id properties not supported for this type");
        return -1;
      }
    }
  }

  return BPy_Wrap_SetMapItem(group, key, value);
}

static PyMappingMethods pyrna_struct_as_mapping = {
    /*mp_length*/ (lenfunc) nullptr,
    /*mp_subscript*/ (binaryfunc)pyrna_struct_subscript,
    /*mp_ass_subscript*/ (objobjargproc)pyrna_struct_ass_subscript,
};

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_keys_doc,
    ".. method:: keys()\n"
    "\n"
    "   Returns the keys of this objects custom properties (matches Python's\n"
    "   dictionary function of the same name).\n"
    "\n"
    "   :return: custom property keys.\n"
    "   :rtype: :class:`idprop.types.IDPropertyGroupViewKeys`\n"
    "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_keys(BPy_StructRNA *self)
{
  PYRNA_STRUCT_CHECK_OBJ(self);

  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "bpy_struct.keys(): this type doesn't support IDProperties");
    return nullptr;
  }

  /* `group` may be nullptr. */
  IDProperty *group = RNA_struct_idprops(&self->ptr.value(), false);
  return BPy_Wrap_GetKeys_View_WithID(self->ptr->owner_id, group);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_items_doc,
    ".. method:: items()\n"
    "\n"
    "   Returns the items of this objects custom properties (matches Python's\n"
    "   dictionary function of the same name).\n"
    "\n"
    "   :return: custom property key, value pairs.\n"
    "   :rtype: :class:`idprop.types.IDPropertyGroupViewItems`\n"
    "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_items(BPy_StructRNA *self)
{
  PYRNA_STRUCT_CHECK_OBJ(self);

  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "bpy_struct.items(): this type doesn't support IDProperties");
    return nullptr;
  }

  /* `group` may be nullptr. */
  IDProperty *group = RNA_struct_idprops(&self->ptr.value(), false);
  return BPy_Wrap_GetItems_View_WithID(self->ptr->owner_id, group);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_values_doc,
    ".. method:: values()\n"
    "\n"
    "   Returns the values of this objects custom properties (matches Python's\n"
    "   dictionary function of the same name).\n"
    "\n"
    "   :return: custom property values.\n"
    "   :rtype: :class:`idprop.types.IDPropertyGroupViewValues`\n"
    "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_values(BPy_StructRNA *self)
{
  PYRNA_STRUCT_CHECK_OBJ(self);

  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_struct.values(): this type doesn't support IDProperties");
    return nullptr;
  }

  /* `group` may be nullptr. */
  IDProperty *group = RNA_struct_idprops(&self->ptr.value(), false);
  return BPy_Wrap_GetValues_View_WithID(self->ptr->owner_id, group);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_is_property_set_doc,
    ".. method:: is_property_set(property, /, *, ghost=True)\n"
    "\n"
    "   Check if a property is set, use for testing operator properties.\n"
    "\n"
    "   :arg property: Property name.\n"
    "   :type property: str\n"
    "   :arg ghost: Used for operators that re-run with previous settings.\n"
    "      In this case the property is not marked as set,\n"
    "      yet the value from the previous execution is used.\n"
    "\n"
    "      In rare cases you may want to set this option to false.\n"
    "\n"
    "   :type ghost: bool\n"
    "   :return: True when the property has been set.\n"
    "   :rtype: bool\n");
static PyObject *pyrna_struct_is_property_set(BPy_StructRNA *self, PyObject *args, PyObject *kw)
{
  PropertyRNA *prop;
  const char *name;
  bool use_ghost = true;

  PYRNA_STRUCT_CHECK_OBJ(self);

  static const char *_keywords[] = {"", "ghost", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "s"  /* `name` (positional). */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `ghost` */
      ":is_property_set",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &name, PyC_ParseBool, &use_ghost)) {
    return nullptr;
  }

  if ((prop = RNA_struct_find_property(&self->ptr.value(), name)) == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.is_property_set(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr->type),
                 name);
    return nullptr;
  }

  return PyBool_FromLong(RNA_property_is_set_ex(&self->ptr.value(), prop, use_ghost));
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_property_unset_doc,
    ".. method:: property_unset(property, /)\n"
    "\n"
    "   Unset a property, will use default value afterward.\n"
    "\n"
    "   :arg property: Property name.\n"
    "   :type property: str\n");
static PyObject *pyrna_struct_property_unset(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s:property_unset", &name)) {
    return nullptr;
  }

  if ((prop = RNA_struct_find_property(&self->ptr.value(), name)) == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.property_unset(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr->type),
                 name);
    return nullptr;
  }

  RNA_property_unset(&self->ptr.value(), prop);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_is_property_hidden_doc,
    ".. method:: is_property_hidden(property, /)\n"
    "\n"
    "   Check if a property is hidden.\n"
    "\n"
    "   :arg property: Property name.\n"
    "   :type property: str\n"
    "   :return: True when the property is hidden.\n"
    "   :rtype: bool\n");
static PyObject *pyrna_struct_is_property_hidden(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s:is_property_hidden", &name)) {
    return nullptr;
  }

  if ((prop = RNA_struct_find_property(&self->ptr.value(), name)) == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.is_property_hidden(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr->type),
                 name);
    return nullptr;
  }

  return PyBool_FromLong(RNA_property_flag(prop) & PROP_HIDDEN);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_is_property_readonly_doc,
    ".. method:: is_property_readonly(property, /)\n"
    "\n"
    "   Check if a property is readonly.\n"
    "\n"
    "   :arg property: Property name.\n"
    "   :type property: str\n"
    "   :return: True when the property is readonly (not writable).\n"
    "   :rtype: bool\n");
static PyObject *pyrna_struct_is_property_readonly(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s:is_property_readonly", &name)) {
    return nullptr;
  }

  if ((prop = RNA_struct_find_property(&self->ptr.value(), name)) == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.is_property_readonly(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr->type),
                 name);
    return nullptr;
  }

  return PyBool_FromLong(!RNA_property_editable(&self->ptr.value(), prop));
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_is_property_overridable_library_doc,
    ".. method:: is_property_overridable_library(property, /)\n"
    "\n"
    "   Check if a property is overridable.\n"
    "\n"
    "   :arg property: Property name.\n"
    "   :type property: str\n"
    "   :return: True when the property is overridable.\n"
    "   :rtype: bool\n");
static PyObject *pyrna_struct_is_property_overridable_library(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s:is_property_overridable_library", &name)) {
    return nullptr;
  }

  if ((prop = RNA_struct_find_property(&self->ptr.value(), name)) == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.is_property_overridable_library(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr->type),
                 name);
    return nullptr;
  }

  return PyBool_FromLong(long(RNA_property_overridable_get(&self->ptr.value(), prop)));
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_property_overridable_library_set_doc,
    ".. method:: property_overridable_library_set(property, overridable, /)\n"
    "\n"
    "   Define a property as overridable or not (only for custom properties!).\n"
    "\n"
    "   :arg property: Property name.\n"
    "   :type property: str\n"
    "   :arg overridable: Overridable status to set.\n"
    "   :type overridable: bool\n"
    "   :return: True when the overridable status of the property was successfully set.\n"
    "   :rtype: bool\n");
static PyObject *pyrna_struct_property_overridable_library_set(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;
  int is_overridable;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "sp:property_overridable_library_set", &name, &is_overridable)) {
    return nullptr;
  }

  if ((prop = RNA_struct_find_property(&self->ptr.value(), name)) == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.property_overridable_library_set(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr->type),
                 name);
    return nullptr;
  }

  return PyBool_FromLong(
      long(RNA_property_overridable_library_set(&self->ptr.value(), prop, bool(is_overridable))));
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_path_resolve_doc,
    ".. method:: path_resolve(path, coerce=True, /)\n"
    "\n"
    "   Returns the property from the path, raise an exception when not found.\n"
    "\n"
    "   :arg path: path which this property resolves.\n"
    "   :type path: str\n"
    "   :arg coerce: optional argument, when True, the property will be converted\n"
    "      into its Python representation.\n"
    "   :type coerce: bool\n"
    "   :return: Property value or property object.\n"
    "   :rtype: Any | :class:`bpy.types.bpy_prop`\n");
static PyObject *pyrna_struct_path_resolve(BPy_StructRNA *self, PyObject *args)
{
  const char *path;
  PyObject *coerce = Py_True;
  PointerRNA r_ptr;
  PropertyRNA *r_prop;
  int index = -1;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|O!:path_resolve", &path, &PyBool_Type, &coerce)) {
    return nullptr;
  }

  if (RNA_path_resolve_full_maybe_null(&self->ptr.value(), path, &r_ptr, &r_prop, &index)) {
    if (r_prop) {
      if (index != -1) {
        if (index >= RNA_property_array_length(&r_ptr, r_prop) || index < 0) {
          PyErr_Format(PyExc_IndexError,
                       "%.200s.path_resolve(\"%.200s\") index out of range",
                       RNA_struct_identifier(self->ptr->type),
                       path);
          return nullptr;
        }

        return pyrna_array_index(&r_ptr, r_prop, index);
      }

      if (coerce == Py_False) {
        return pyrna_prop_CreatePyObject(&r_ptr, r_prop);
      }

      return pyrna_prop_to_py(&r_ptr, r_prop);
    }

    return pyrna_struct_CreatePyObject(&r_ptr);
  }

  PyErr_Format(PyExc_ValueError,
               "%.200s.path_resolve(\"%.200s\") could not be resolved",
               RNA_struct_identifier(self->ptr->type),
               path);
  return nullptr;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_path_from_module_doc,
    ".. method:: path_from_module(property=\"\", index=-1, /)\n"
    "\n"
    "   Returns the full data path to this struct (as a string) from the bpy module.\n"
    "\n"
    "   :arg property: Optional property name to get the full path from\n"
    "   :type property: str\n"
    "   :arg index: Optional index of the property.\n"
    "      \"-1\" means that the property has no indices.\n"
    "   :type index: int\n"
    "   :return: The full path to the data.\n"
    "   :rtype: str\n"
    "\n"
    "   :raises ValueError:\n"
    "      if the input data cannot be converted into a full data path.\n"
    "\n"
    "      .. note:: Even if all input data is correct, this function might\n"
    "         error out because Blender cannot derive a valid path.\n"
    "         The incomplete path will be printed in the error message.\n");
static PyObject *pyrna_struct_path_from_module(BPy_StructRNA *self, PyObject *args)
{
  const char *error_prefix = "path_from_module(...)";
  const char *name = nullptr;
  PropertyRNA *prop;
  int index = -1;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|si:path_from_module", &name, &index)) {
    return nullptr;
  }
  if (index < -1) {
    PyErr_Format(PyExc_ValueError, "%s: indices below -1 are not supported", error_prefix);
    return nullptr;
  }

  std::optional<std::string> path;
  if (name) {
    prop = RNA_struct_find_property(&self->ptr.value(), name);
    if (prop == nullptr) {
      PyErr_Format(PyExc_AttributeError,
                   "%.200s.path_from_module(\"%.200s\") not found",
                   RNA_struct_identifier(self->ptr->type),
                   name);
      return nullptr;
    }
    path = RNA_path_full_property_py_ex(&self->ptr.value(), prop, index, true);
  }
  else {
    if (RNA_struct_is_ID(self->ptr->type)) {
      path = RNA_path_full_ID_py(self->ptr->owner_id);
    }
    else {
      path = RNA_path_full_struct_py(&self->ptr.value());
    }
  }

  if (!path) {
    if (name) {
      PyErr_Format(PyExc_ValueError,
                   "%.200s.path_from_module(\"%s\", %d) found, but does not support path creation",
                   RNA_struct_identifier(self->ptr->type),
                   name,
                   index);
    }
    else {
      PyErr_Format(PyExc_ValueError,
                   "%.200s.path_from_module() does not support path creation for this type",
                   RNA_struct_identifier(self->ptr->type));
    }
    return nullptr;
  }

  if (path.value().back() == '.') {
    PyErr_Format(PyExc_ValueError,
                 "%.200s.path_from_module() could not derive a complete path for this type.\n"
                 "Only got \"%.200s\" as an incomplete path",
                 RNA_struct_identifier(self->ptr->type),
                 path.value().c_str());
    return nullptr;
  }

  return PyC_UnicodeFromStdStr(path.value());
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_path_from_module_doc,
    ".. method:: path_from_module()\n"
    "\n"
    "   Returns the full data path to this struct (as a string) from the bpy module.\n"
    "\n"
    "   :return: The full path to the data.\n"
    "   :rtype: str\n"
    "\n"
    "   :raises ValueError:\n"
    "      if the input data cannot be converted into a full data path.\n"
    "\n"
    "      .. note:: Even if all input data is correct, this function might\n"
    "         error out because Blender cannot derive a valid path.\n"
    "         The incomplete path will be printed in the error message.\n");
static PyObject *pyrna_prop_path_from_module(BPy_PropertyRNA *self)
{
  PropertyRNA *prop = self->prop;

  const std::optional<std::string> path = RNA_path_full_property_py_ex(
      &self->ptr.value(), prop, -1, true);

  if (!path) {
    PyErr_Format(PyExc_ValueError,
                 "%.200s.%.200s.path_from_module() does not support path creation for this type",
                 RNA_struct_identifier(self->ptr->type),
                 RNA_property_identifier(prop));
    return nullptr;
  }

  if (path.value().back() == '.') {
    PyErr_Format(
        PyExc_ValueError,
        "%.200s.%.200s.path_from_module() could not derive a complete path for this type.\n"
        "Only got \"%.200s\" as an incomplete path",
        RNA_struct_identifier(self->ptr->type),
        RNA_property_identifier(prop),
        path.value().c_str());
    return nullptr;
  }

  return PyC_UnicodeFromStdStr(path.value());
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_path_from_id_doc,
    ".. method:: path_from_id(property=\"\", /)\n"
    "\n"
    "   Returns the data path from the ID to this object (string).\n"
    "\n"
    "   :arg property: Optional property name which can be used if the path is\n"
    "      to a property of this object.\n"
    "   :type property: str\n"
    "   :return: The path from :class:`bpy.types.bpy_struct.id_data`\n"
    "      to this struct and property (when given).\n"
    "   :rtype: str\n");
static PyObject *pyrna_struct_path_from_id(BPy_StructRNA *self, PyObject *args)
{
  const char *name = nullptr;
  PropertyRNA *prop;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|s:path_from_id", &name)) {
    return nullptr;
  }

  std::optional<std::string> path;
  if (name) {
    prop = RNA_struct_find_property(&self->ptr.value(), name);
    if (prop == nullptr) {
      PyErr_Format(PyExc_AttributeError,
                   "%.200s.path_from_id(\"%.200s\") not found",
                   RNA_struct_identifier(self->ptr->type),
                   name);
      return nullptr;
    }

    path = RNA_path_from_ID_to_property(&self->ptr.value(), prop);
  }
  else {
    path = RNA_path_from_ID_to_struct(&self->ptr.value());
  }

  if (!path) {
    if (name) {
      PyErr_Format(PyExc_ValueError,
                   "%.200s.path_from_id(\"%s\") found, but does not support path creation",
                   RNA_struct_identifier(self->ptr->type),
                   name);
    }
    else {
      PyErr_Format(PyExc_ValueError,
                   "%.200s.path_from_id() does not support path creation for this type",
                   RNA_struct_identifier(self->ptr->type));
    }
    return nullptr;
  }

  return PyC_UnicodeFromStdStr(path.value());
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_path_from_id_doc,
    ".. method:: path_from_id()\n"
    "\n"
    "   Returns the data path from the ID to this property (string).\n"
    "\n"
    "   :return: The path from :class:`bpy.types.bpy_struct.id_data` to this property.\n"
    "   :rtype: str\n");
static PyObject *pyrna_prop_path_from_id(BPy_PropertyRNA *self)
{
  PropertyRNA *prop = self->prop;

  const std::optional<std::string> path = RNA_path_from_ID_to_property(&self->ptr.value(),
                                                                       self->prop);

  if (!path) {
    PyErr_Format(PyExc_ValueError,
                 "%.200s.%.200s.path_from_id() does not support path creation for this type",
                 RNA_struct_identifier(self->ptr->type),
                 RNA_property_identifier(prop));
    return nullptr;
  }

  return PyC_UnicodeFromStdStr(path.value());
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_as_bytes_doc,
    ".. method:: as_bytes()\n"
    "\n"
    "   Returns this string property as a byte rather than a Python string.\n"
    "\n"
    "   :return: The string as bytes.\n"
    "   :rtype: bytes\n");
static PyObject *pyrna_prop_as_bytes(BPy_PropertyRNA *self)
{

  if (RNA_property_type(self->prop) != PROP_STRING) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.%.200s.as_bytes() must be a string",
                 RNA_struct_identifier(self->ptr->type),
                 RNA_property_identifier(self->prop));
    return nullptr;
  }

  PyObject *ret;
  char buf_fixed[256], *buf;
  int buf_len;

  buf = RNA_property_string_get_alloc(
      &self->ptr.value(), self->prop, buf_fixed, sizeof(buf_fixed), &buf_len);

  ret = PyBytes_FromStringAndSize(buf, buf_len);

  if (buf_fixed != buf) {
    MEM_freeN(buf);
  }

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_update_doc,
    ".. method:: update()\n"
    "\n"
    "   Execute the properties update callback.\n"
    "\n"
    "   .. note::\n"
    "      This is called when assigning a property,\n"
    "      however in rare cases it's useful to call explicitly.\n");
static PyObject *pyrna_prop_update(BPy_PropertyRNA *self)
{
  RNA_property_update(BPY_context_get(), &self->ptr.value(), self->prop);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_type_recast_doc,
    ".. method:: type_recast()\n"
    "\n"
    "   Return a new instance, this is needed because types\n"
    "   such as textures can be changed at runtime.\n"
    "\n"
    "   :return: a new instance of this object with the type initialized again.\n"
    "   :rtype: :class:`bpy.types.bpy_struct`\n");
static PyObject *pyrna_struct_type_recast(BPy_StructRNA *self)
{

  PYRNA_STRUCT_CHECK_OBJ(self);

  PointerRNA r_ptr = RNA_pointer_recast(&self->ptr.value());
  return pyrna_struct_CreatePyObject(&r_ptr);
}

/**
 * \note Return value is borrowed, caller must #Py_INCREF.
 */
static PyObject *pyrna_struct_bl_rna_find_subclass_recursive(PyObject *cls, const char *id)
{
  PyObject *ret_test = nullptr;
  PyObject *subclasses = (PyObject *)((PyTypeObject *)cls)->tp_subclasses;
  if (subclasses) {
    /* Unfortunately we can't use the dict key because Python class names
     * don't match the bl_idname used internally. */
    BLI_assert(PyDict_CheckExact(subclasses));
    PyObject *key = nullptr;
    Py_ssize_t pos = 0;
    PyObject *value = nullptr;
    while (PyDict_Next(subclasses, &pos, &key, &value)) {
      BLI_assert(PyWeakref_CheckRef(value));
      PyObject *subcls = PyWeakref_GET_OBJECT(value);
      if (subcls != Py_None) {
        BPy_StructRNA *py_srna = (BPy_StructRNA *)PyDict_GetItem(((PyTypeObject *)subcls)->tp_dict,
                                                                 bpy_intern_str_bl_rna);
        if (py_srna) {
          StructRNA *srna = static_cast<StructRNA *>(py_srna->ptr->data);
          if (STREQ(id, RNA_struct_identifier(srna))) {
            ret_test = subcls;
            break;
          }
        }
        ret_test = pyrna_struct_bl_rna_find_subclass_recursive(subcls, id);
        if (ret_test) {
          break;
        }
      }
    }
  }
  return ret_test;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_bl_rna_get_subclass_py_doc,
    ".. classmethod:: bl_rna_get_subclass_py(id, default=None, /)\n"
    "\n"
    "   :arg id: The RNA type identifier.\n"
    "   :type id: str\n"
    "   :return: The class or default when not found.\n"
    "   :rtype: type\n");
static PyObject *pyrna_struct_bl_rna_get_subclass_py(PyObject *cls, PyObject *args)
{
  char *id;
  PyObject *ret_default = Py_None;

  if (!PyArg_ParseTuple(args, "s|O:bl_rna_get_subclass_py", &id, &ret_default)) {
    return nullptr;
  }
  PyObject *ret = pyrna_struct_bl_rna_find_subclass_recursive(cls, id);
  if (ret == nullptr) {
    ret = ret_default;
  }
  return Py_NewRef(ret);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_bl_rna_get_subclass_doc,
    ".. classmethod:: bl_rna_get_subclass(id, default=None, /)\n"
    "\n"
    "   :arg id: The RNA type identifier.\n"
    "   :type id: str\n"
    "   :return: The RNA type or default when not found.\n"
    "   :rtype: :class:`bpy.types.Struct` subclass\n");
static PyObject *pyrna_struct_bl_rna_get_subclass(PyObject *cls, PyObject *args)
{
  const char *id;
  PyObject *ret_default = Py_None;

  if (!PyArg_ParseTuple(args, "s|O:bl_rna_get_subclass", &id, &ret_default)) {
    return nullptr;
  }

  const BPy_StructRNA *py_srna = (BPy_StructRNA *)PyDict_GetItem(((PyTypeObject *)cls)->tp_dict,
                                                                 bpy_intern_str_bl_rna);
  if (py_srna == nullptr) {
    PyErr_SetString(PyExc_ValueError, "Not a registered class");
    return nullptr;
  }
  const StructRNA *srna_base = static_cast<const StructRNA *>(py_srna->ptr->data);

  if (srna_base == &RNA_Node) {
    /* If the given idname is an alias, translate it to the proper idname. */
    id = blender::bke::node_type_find_alias(id).c_str();

    blender::bke::bNodeType *nt = blender::bke::node_type_find(id);
    if (nt) {
      PointerRNA ptr = RNA_pointer_create_discrete(nullptr, &RNA_Struct, nt->rna_ext.srna);
      return pyrna_struct_CreatePyObject(&ptr);
    }
  }
  else {
    /* TODO: panels, menus etc. */
    PyErr_Format(
        PyExc_ValueError, "Class type \"%.200s\" not supported", RNA_struct_identifier(srna_base));
    return nullptr;
  }

  return Py_NewRef(ret_default);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_get_ancestors_doc,
    ".. method:: rna_ancestors()\n"
    "\n"
    "   Return the chain of data containing this struct, if known.\n"
    "   The first item is the root (typically an ID), the last one is the immediate parent.\n"
    "   May be empty.\n"
    "\n"
    "   :return: a list of this object's ancestors.\n"
    "   :rtype: list[:class:`bpy.types.bpy_struct`]\n");
static PyObject *pyrna_struct_get_ancestors(BPy_StructRNA *self)
{
  PYRNA_STRUCT_CHECK_OBJ(self);

  PyObject *ret;
  const int ancestors_num(self->ptr->ancestors.size());

  ret = PyList_New(ancestors_num);

  for (int i = 0; i < ancestors_num; i++) {
    PointerRNA ancestor_ptr = RNA_pointer_create_from_ancestor(self->ptr.value(), i);
    PyObject *ancestor = pyrna_struct_CreatePyObject(&ancestor_ptr);
    PyList_SET_ITEM(ret, i, ancestor);
  }

  return ret;
}

static void pyrna_dir_members_py__add_keys(PyObject *list, PyObject *dict)
{
  PyObject *list_tmp;

  list_tmp = PyDict_Keys(dict);
  PyList_SetSlice(list, INT_MAX, INT_MAX, list_tmp);
  Py_DECREF(list_tmp);
}

static void pyrna_dir_members_py(PyObject *list, PyObject *self)
{
  PyObject *dict;
  PyObject **dict_ptr;

  dict_ptr = _PyObject_GetDictPtr(self);

  if (dict_ptr && (dict = *dict_ptr)) {
    pyrna_dir_members_py__add_keys(list, dict);
  }

  dict = ((PyTypeObject *)Py_TYPE(self))->tp_dict;
  if (dict) {
    pyrna_dir_members_py__add_keys(list, dict);
  }

  /* Since this is least common case, handle it last. */
  if (BPy_PropertyRNA_Check(self)) {
    BPy_PropertyRNA *self_prop = (BPy_PropertyRNA *)self;
    if (RNA_property_type(self_prop->prop) == PROP_COLLECTION) {
      PointerRNA r_ptr;

      if (RNA_property_collection_type_get(&self_prop->ptr.value(), self_prop->prop, &r_ptr)) {
        PyObject *cls = pyrna_struct_Subtype(&r_ptr); /* borrows */
        dict = ((PyTypeObject *)cls)->tp_dict;
        pyrna_dir_members_py__add_keys(list, dict);
        Py_DECREF(cls);
      }
    }
  }
}

static void pyrna_dir_members_rna(PyObject *list, PointerRNA *ptr)
{
  const char *idname;

  /* For looping over attributes and functions. */
  PropertyRNA *iterprop;

  {
    PointerRNA tptr = RNA_pointer_create_discrete(nullptr, &RNA_Struct, ptr->type);
    iterprop = RNA_struct_find_property(&tptr, "functions");

    RNA_PROP_BEGIN (&tptr, itemptr, iterprop) {
      FunctionRNA *func = static_cast<FunctionRNA *>(itemptr.data);
      if (RNA_function_defined(func)) {
        idname = RNA_function_identifier(static_cast<FunctionRNA *>(itemptr.data));
        PyList_APPEND(list, PyUnicode_FromString(idname));
      }
    }
    RNA_PROP_END;
  }

  {
    /*
     * Collect RNA attributes
     */
    iterprop = RNA_struct_iterator_property(ptr->type);

    BPy_NamePropAsPyObject_Cache nameprop_cache = {nullptr};
    RNA_PROP_BEGIN (ptr, itemptr, iterprop) {
      /* Custom-properties are exposed using `__getitem__`, exclude from `__dir__`. */
      if (RNA_property_is_idprop(static_cast<const PropertyRNA *>(itemptr.data))) {
        continue;
      }
      if (PyObject *name_py = pyrna_struct_get_nameprop_as_pyobject(&itemptr, nameprop_cache)) {
        PyList_APPEND(list, name_py);
      }
    }
    RNA_PROP_END;
  }
}

static PyObject *pyrna_struct_dir(BPy_StructRNA *self)
{
  PyObject *ret;

  PYRNA_STRUCT_CHECK_OBJ(self);

  /* Include this in case this instance is a subtype of a Python class
   * In these instances we may want to return a function or variable provided by the subtype. */
  ret = PyList_New(0);

  if (!BPy_StructRNA_CheckExact(self)) {
    pyrna_dir_members_py(ret, (PyObject *)self);
  }

  pyrna_dir_members_rna(ret, &self->ptr.value());

  if (self->ptr->type == &RNA_Context) {
    ListBase lb = CTX_data_dir_get(static_cast<const bContext *>(self->ptr->data));

    LISTBASE_FOREACH (LinkData *, link, &lb) {
      PyList_APPEND(ret, PyUnicode_FromString(static_cast<const char *>(link->data)));
    }

    BLI_freelistN(&lb);
  }

  {
    /* set(), this is needed to remove-doubles because the deferred
     * register-props will be in both the Python __dict__ and accessed as RNA */

    PyObject *set = PySet_New(ret);

    Py_DECREF(ret);
    ret = PySequence_List(set);
    Py_DECREF(set);
  }

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_id_properties_ensure_doc,
    ".. method:: id_properties_ensure()\n"
    "\n"
    "   :return: the parent group for an RNA struct's custom IDProperties.\n"
    "   :rtype: :class:`idprop.types.IDPropertyGroup`\n");
static PyObject *pyrna_struct_id_properties_ensure(BPy_StructRNA *self)
{
  PYRNA_STRUCT_CHECK_OBJ(self);

  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "This type doesn't support IDProperties");
    return nullptr;
  }

  IDProperty *idprops = RNA_struct_idprops(&self->ptr.value(), true);

  /* This is a paranoid check that theoretically might not be necessary.
   * It allows the possibility that some structs can't ensure IDProperties. */
  if (idprops == nullptr) {
    return Py_None;
  }

  BPy_IDProperty *group = PyObject_New(BPy_IDProperty, &BPy_IDGroup_Type);
  group->owner_id = self->ptr->owner_id;
  group->prop = idprops;
  group->parent = nullptr;
  return (PyObject *)group;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_id_properties_ui_doc,
    ".. method:: id_properties_ui(key, /)\n"
    "\n"
    "   :return: Return an object used to manage an IDProperty's UI data.\n"
    "   :arg key: String name of the property.\n"
    "   :type key: str.\n"
    "   :rtype: :class:`bpy.types.IDPropertyUIManager`\n");
static PyObject *pyrna_struct_id_properties_ui(BPy_StructRNA *self, PyObject *args)
{
  PYRNA_STRUCT_CHECK_OBJ(self);

  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "This type doesn't support IDProperties");
    return nullptr;
  }

  const char *key;
  if (!PyArg_ParseTuple(args, "s:ui_data", &key)) {
    return nullptr;
  }

  IDProperty *parent_group = RNA_struct_idprops(&self->ptr.value(), true);

  /* This is a paranoid check that theoretically might not be necessary.
   * It allows the possibility that some structs can't ensure IDProperties. */
  if (parent_group == nullptr) {
    return Py_None;
  }

  IDProperty *property = IDP_GetPropertyFromGroup(parent_group, key);
  if (property == nullptr) {
    PyErr_SetString(PyExc_KeyError, "Property not found in IDProperty group");
    return nullptr;
  }

  if (!IDP_ui_data_supported(property)) {
    PyErr_Format(PyExc_TypeError, "IDProperty \"%s\" does not support UI data", property->name);
    return nullptr;
  }

  BPy_IDPropertyUIManager *ui_manager = PyObject_New(BPy_IDPropertyUIManager,
                                                     &BPy_IDPropertyUIManager_Type);
  ui_manager->property = property;
  return (PyObject *)ui_manager;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_id_properties_clear_doc,
    ".. method:: id_properties_clear()\n"
    "\n"
    "   :return: Remove the parent group for an RNA struct's custom IDProperties.\n");
static PyObject *pyrna_struct_id_properties_clear(BPy_StructRNA *self)
{
  PYRNA_STRUCT_CHECK_OBJ(self);

  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "This type doesn't support IDProperties");
    return nullptr;
  }

  IDProperty **idprops = RNA_struct_idprops_p(&self->ptr.value());

  if (*idprops) {
    IDP_FreeProperty(*idprops);
    *idprops = nullptr;
  }

  Py_RETURN_NONE;
}

/* ---------------getattr-------------------------------------------- */
static PyObject *pyrna_struct_getattro(BPy_StructRNA *self, PyObject *pyname)
{
  const char *name = PyUnicode_AsUTF8(pyname);
  PyObject *ret;
  PropertyRNA *prop;
  FunctionRNA *func;

  /* Allow `__class__` so `isinstance(ob, cls)` can be used without raising an exception. */
  PYRNA_STRUCT_CHECK_OBJ_UNLESS(self, name && STREQ(name, "__class__"));

  if (name == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "bpy_struct: __getattr__ must be a string");
    ret = nullptr;
  }
  else if (
      /* RNA can't start with a "_", so for __dict__ and similar we can skip using RNA lookups. */
      name[0] == '_')
  {
    /* Annoying exception, maybe we need to have different types for this... */
    if (STR_ELEM(name, "__getitem__", "__setitem__") && !RNA_struct_idprops_check(self->ptr->type))
    {
      PyErr_SetString(PyExc_AttributeError, "bpy_struct: no __getitem__ support for this type");
      ret = nullptr;
    }
    else {
      ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
    }
  }
  else if ((prop = RNA_struct_find_property(&self->ptr.value(), name))) {
    ret = pyrna_prop_to_py(&self->ptr.value(), prop);
  }
  /* RNA function only if callback is declared (no optional functions). */
  else if ((func = RNA_struct_find_function(self->ptr->type, name)) && RNA_function_defined(func))
  {
    ret = pyrna_func_CreatePyObject(&self->ptr.value(), func);
  }
  else if (self->ptr->type == &RNA_Context) {
    bContext *C = static_cast<bContext *>(self->ptr->data);
    if (C == nullptr) {
      PyErr_Format(PyExc_AttributeError,
                   "bpy_struct: Context is 'null', cannot get \"%.200s\" from context",
                   name);
      ret = nullptr;
    }
    else {
      PointerRNA newptr;
      blender::Vector<PointerRNA> newlb;
      PropertyRNA *newprop;
      int newindex;
      blender::StringRef newstr;
      std::optional<int64_t> newint;
      ContextDataType newtype;

      /* An empty string is used to implement #CTX_data_dir_get,
       * without this check `getattr(context, "")` succeeds. */
      eContextResult done;
      if (name[0]) {
        done = eContextResult(CTX_data_get(
            C, name, &newptr, &newlb, &newprop, &newindex, &newstr, &newint, &newtype));
      }
      else {
        /* Fall through to built-in `getattr`. */
        done = CTX_RESULT_MEMBER_NOT_FOUND;
      }

      if (done == CTX_RESULT_OK) {
        switch (newtype) {
          case ContextDataType::Pointer:
            if (newptr.data == nullptr) {
              ret = Py_None;
              Py_INCREF(ret);
            }
            else {
              ret = pyrna_struct_CreatePyObject(&newptr);
            }
            break;
          case ContextDataType::String: {
            if (newstr.is_empty()) {
              ret = Py_None;
              Py_INCREF(ret);
            }
            else {
              ret = PyUnicode_FromStringAndSize(newstr.data(), newstr.size());
            }
            break;
          }
          case ContextDataType::Int64: {
            if (!newint.has_value()) {
              ret = Py_None;
              Py_INCREF(ret);
            }
            else {
              ret = PyLong_FromLong(*newint);
            }
            break;
          }
          case ContextDataType::Collection: {
            ret = PyList_New(0);
            for (PointerRNA &ptr : newlb) {
              PyList_APPEND(ret, pyrna_struct_CreatePyObject(&ptr));
            }
            break;
          }
          case ContextDataType::Property: {
            if (newprop != nullptr) {
              /* Create pointer to parent ID, and path from ID to property. */
              PointerRNA idptr;

              PointerRNA *base_ptr;
              std::optional<std::string> path_str;

              if (newptr.owner_id) {
                path_str = RNA_path_from_ID_to_property(&newptr, newprop);
                idptr = RNA_id_pointer_create(newptr.owner_id);
                base_ptr = &idptr;
              }
              else {
                path_str = RNA_path_from_ptr_to_property_index(&newptr, newprop, 0, -1);
                base_ptr = &newptr;
              }

              if (path_str) {
                ret = PyTuple_New(3);
                PyTuple_SET_ITEMS(ret,
                                  pyrna_struct_CreatePyObject(base_ptr),
                                  PyC_UnicodeFromStdStr(path_str.value()),
                                  PyLong_FromLong(newindex));
              }
              else {
                ret = Py_None;
                Py_INCREF(ret);
              }
            }
            else {
              ret = Py_None;
              Py_INCREF(ret);
            }
            break;
          }
          default:
            /* Should never happen. */
            BLI_assert_msg(0, "Invalid context type");

            PyErr_Format(PyExc_AttributeError,
                         "bpy_struct: Context type invalid %d, cannot get \"%.200s\" from context",
                         newtype,
                         name);
            ret = nullptr;
            break;
        }
      }
      else if (done == CTX_RESULT_NO_DATA) {
        ret = Py_None;
        Py_INCREF(ret);
      }
      else { /* Not found in the context. */
        /* Lookup the subclass. raise an error if it's not found. */
        ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
      }
    }
  }
  else {
#if 0
    PyErr_Format(PyExc_AttributeError, "bpy_struct: attribute \"%.200s\" not found", name);
    ret = nullptr;
#endif
    /* Include this in case this instance is a subtype of a Python class
     * In these instances we may want to return a function or variable provided by the subtype
     *
     * Also needed to return methods when it's not a subtype.
     */

    /* The error raised here will be displayed */
    ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
  }

  return ret;
}

#if 0
static int pyrna_struct_pydict_contains(PyObject *self, PyObject *pyname)
{
  PyObject *dict = *(_PyObject_GetDictPtr((PyObject *)self));
  if (UNLIKELY(dict == nullptr)) {
    return 0;
  }

  return PyDict_Contains(dict, pyname);
}
#endif

/* --------------- setattr------------------------------------------- */

#if 0
static PyObject *pyrna_struct_meta_idprop_getattro(PyObject *cls, PyObject *attr)
{
  PyObject *ret = PyType_Type.tp_getattro(cls, attr);

/* Allows:
 * >>> bpy.types.Scene.foo = BoolProperty()
 * >>> bpy.types.Scene.foo
 * <bpy_struct, BoolProperty("foo")>
 * ...rather than returning the deferred class register tuple
 * as checked by BPy_PropDeferred_CheckTypeExact()
 *
 * Disable for now,
 * this is faking internal behavior in a way that's too tricky to maintain well. */
#  if 0
  if ((ret == nullptr) /* || BPy_PropDeferred_CheckTypeExact(ret) */) {
    PyErr_Clear(); /* Clear error from tp_getattro. */
    StructRNA *srna = srna_from_self(cls, "StructRNA.__getattr__");
    if (srna) {
      PropertyRNA *prop = RNA_struct_type_find_property_no_base(srna, PyUnicode_AsUTF8(attr));
      if (prop) {
        PointerRNA tptr = RNA_pointer_create_discrete(nullptr, &RNA_Property, prop);
        ret = pyrna_struct_CreatePyObject(&tptr);
      }
    }
    if (ret == nullptr) {
      PyErr_Format(PyExc_AttributeError,
                   "StructRNA.__getattr__: attribute \"%.200s\" not found",
                   PyUnicode_AsUTF8(attr));
    }
  }
#  endif

  return ret;
}
#endif

static int pyrna_struct_meta_idprop_setattro(PyObject *cls, PyObject *attr, PyObject *value)
{
  StructRNA *srna = srna_from_self(cls, "StructRNA.__setattr__");
  const bool is_deferred_prop = (value && BPy_PropDeferred_CheckTypeExact(value));
  const char *attr_str = PyUnicode_AsUTF8(attr);

  if (srna && !pyrna_write_check() &&
      (is_deferred_prop || RNA_struct_type_find_property_no_base(srna, attr_str)))
  {
    PyErr_Format(PyExc_AttributeError,
                 "pyrna_struct_meta_idprop_setattro() "
                 "cannot set in readonly state '%.200s.%S'",
                 ((PyTypeObject *)cls)->tp_name,
                 attr);
    return -1;
  }

  if (srna == nullptr) {
/* Allow setting on unregistered classes which can be registered later on. */
#if 0
    if (value && is_deferred_prop) {
      PyErr_Format(PyExc_AttributeError,
                   "pyrna_struct_meta_idprop_setattro() unable to get srna from class '%.200s'",
                   ((PyTypeObject *)cls)->tp_name);
      return -1;
    }
#endif
    /* srna_from_self may set an error. */
    PyErr_Clear();
    return PyType_Type.tp_setattro(cls, attr, value);
  }

  if (value) {
    /* Check if the value is a property. */
    if (is_deferred_prop) {
      const int ret = deferred_register_prop(srna, attr, value);
      if (ret == -1) {
        /* Error set. */
        return ret;
      }

      /* pass through and assign to the classes __dict__ as well
       * so when the value isn't assigned it still creates the RNA property,
       * but gets confusing from script writers POV if the assigned value can't be read back. */
    }
    else {
      /* Remove existing property if it's set or we also end up with confusion. */
      RNA_def_property_free_identifier(srna, attr_str); /* Ignore on failure. */
    }
  }
  else { /* __delattr__ */
    /* First find if this is a registered property. */
    const int ret = RNA_def_property_free_identifier(srna, attr_str);
    if (ret == -1) {
      PyErr_Format(
          PyExc_TypeError, "struct_meta_idprop.detattr(): '%s' not a dynamic property", attr_str);
      return -1;
    }
  }

  /* Fall back to standard Python's `delattr/setattr`. */
  return PyType_Type.tp_setattro(cls, attr, value);
}

static int pyrna_struct_setattro(BPy_StructRNA *self, PyObject *pyname, PyObject *value)
{
  const char *name = PyUnicode_AsUTF8(pyname);
  PropertyRNA *prop = nullptr;

  PYRNA_STRUCT_CHECK_INT(self);

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), pyname)) {
    return -1;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (name == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "bpy_struct: __setattr__ must be a string");
    return -1;
  }
  if (name[0] != '_' && (prop = RNA_struct_find_property(&self->ptr.value(), name))) {
    if (!RNA_property_editable_flag(&self->ptr.value(), prop)) {
      PyErr_Format(PyExc_AttributeError,
                   "bpy_struct: attribute \"%.200s\" from \"%.200s\" is read-only",
                   RNA_property_identifier(prop),
                   RNA_struct_identifier(self->ptr->type));
      return -1;
    }
  }
  else if (self->ptr->type == &RNA_Context) {
    /* Code just raises correct error, context prop's can't be set,
     * unless it's a part of the py class. */
    bContext *C = static_cast<bContext *>(self->ptr->data);
    if (C == nullptr) {
      PyErr_Format(PyExc_AttributeError,
                   "bpy_struct: Context is 'null', cannot set \"%.200s\" from context",
                   name);
      return -1;
    }

    PointerRNA newptr;
    blender::Vector<PointerRNA> newlb;
    PropertyRNA *newprop;
    int newindex;
    blender::StringRef newstr;
    std::optional<int64_t> newint;
    ContextDataType newtype;

    const eContextResult done = eContextResult(
        CTX_data_get(C, name, &newptr, &newlb, &newprop, &newindex, &newstr, &newint, &newtype));

    if (done == CTX_RESULT_OK) {
      PyErr_Format(
          PyExc_AttributeError, "bpy_struct: Context property \"%.200s\" is read-only", name);
      return -1;
    }
  }

  /* pyrna_py_to_prop sets its own exceptions */
  if (prop) {
    if (value == nullptr) {
      PyErr_SetString(PyExc_AttributeError, "bpy_struct: del not supported");
      return -1;
    }
    return pyrna_py_to_prop(
        &self->ptr.value(), prop, nullptr, value, "bpy_struct: item.attr = val:");
  }

  return PyObject_GenericSetAttr((PyObject *)self, pyname, value);
}

static PyObject *pyrna_prop_dir(BPy_PropertyRNA *self)
{
  PyObject *ret;
  PointerRNA r_ptr;

  /* Include this in case this instance is a subtype of a Python class
   * In these instances we may want to return a function or variable provided by the subtype. */
  ret = PyList_New(0);

  if (!BPy_PropertyRNA_CheckExact(self)) {
    pyrna_dir_members_py(ret, (PyObject *)self);
  }

  if (RNA_property_type(self->prop) == PROP_COLLECTION) {
    if (RNA_property_collection_type_get(&self->ptr.value(), self->prop, &r_ptr)) {
      pyrna_dir_members_rna(ret, &r_ptr);
    }
  }

  return ret;
}

static PyObject *pyrna_prop_array_getattro(BPy_PropertyRNA *self, PyObject *pyname)
{
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
}

static PyObject *pyrna_prop_collection_getattro(BPy_PropertyRNA *self, PyObject *pyname)
{
  const char *name = PyUnicode_AsUTF8(pyname);

  if (name == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "bpy_prop_collection: __getattr__ must be a string");
    return nullptr;
  }
  if (name[0] != '_') {
    PyObject *ret;
    PropertyRNA *prop;
    FunctionRNA *func;

    PointerRNA r_ptr;
    if (RNA_property_collection_type_get(&self->ptr.value(), self->prop, &r_ptr)) {
      if ((prop = RNA_struct_find_property(&r_ptr, name))) {
        ret = pyrna_prop_to_py(&r_ptr, prop);

        return ret;
      }
      if ((func = RNA_struct_find_function(r_ptr.type, name))) {
        PyObject *self_collection = pyrna_struct_CreatePyObject(&r_ptr);
        ret = pyrna_func_CreatePyObject(
            &(reinterpret_cast<BPy_DummyPointerRNA *>(self_collection))->ptr.value(), func);
        Py_DECREF(self_collection);

        return ret;
      }
    }
  }

#if 0
  return PyObject_GenericGetAttr((PyObject *)self, pyname);
#else
  {
    /* Could just do this except for 1 awkward case.
     * `PyObject_GenericGetAttr((PyObject *)self, pyname);`
     * so as to support `bpy.data.libraries.load()` */

    PyObject *ret = _PyObject_GenericGetAttrWithDict((PyObject *)self, pyname, nullptr, 1);

    /* Check the '_' prefix to avoid inheriting `__call__` and similar. */
    if ((ret == nullptr) && (name[0] != '_')) {
      /* Since this is least common case, handle it last. */
      PointerRNA r_ptr;
      if (RNA_property_collection_type_get(&self->ptr.value(), self->prop, &r_ptr)) {
        PyObject *cls = pyrna_struct_Subtype(&r_ptr);
        ret = _PyObject_GenericGetAttrWithDict(cls, pyname, nullptr, 1);
        Py_DECREF(cls);

        if (ret != nullptr) {
          if (Py_TYPE(ret) == &PyMethodDescr_Type) {
            PyMethodDef *m = ((PyMethodDescrObject *)ret)->d_method;
            /* TODO: #METH_CLASS */
            if (m->ml_flags & METH_STATIC) {
              /* Keep 'ret' as-is. */
            }
            else {
              Py_DECREF(ret);
              ret = PyCMethod_New(m, (PyObject *)self, nullptr, nullptr);
            }
          }
        }
      }
    }

    if (ret == nullptr) {
      PyErr_Format(
          PyExc_AttributeError, "bpy_prop_collection: attribute \"%.200s\" not found", name);
    }

    return ret;
  }
#endif
}

/* --------------- setattr------------------------------------------- */
static int pyrna_prop_collection_setattro(BPy_PropertyRNA *self, PyObject *pyname, PyObject *value)
{
  const char *name = PyUnicode_AsUTF8(pyname);
  PropertyRNA *prop;
  PointerRNA r_ptr;

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), pyname)) {
    return -1;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (name == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "bpy_prop: __setattr__ must be a string");
    return -1;
  }
  if (value == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "bpy_prop: del not supported");
    return -1;
  }
  if (RNA_property_collection_type_get(&self->ptr.value(), self->prop, &r_ptr)) {
    if ((prop = RNA_struct_find_property(&r_ptr, name))) {
      /* pyrna_py_to_prop sets its own exceptions. */
      return pyrna_py_to_prop(
          &r_ptr, prop, nullptr, value, "BPy_PropertyRNA - Attribute (setattr):");
    }
  }

  PyErr_Format(PyExc_AttributeError, "bpy_prop_collection: attribute \"%.200s\" not found", name);
  return -1;
}

/**
 * Odd case, we need to be able return a Python method from a #PyTypeObject.tp_getset.
 */
PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_idprop_add_doc,
    ".. method:: add()\n"
    "\n"
    "   This is a function to add a new item to a collection.\n"
    "\n"
    "   :return: A newly created item.\n"
    "   :rtype: Any\n");
static PyObject *pyrna_prop_collection_idprop_add(BPy_PropertyRNA *self)
{
  PointerRNA r_ptr;

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), nullptr)) {
    return nullptr;
  }
#endif /* USE_PEDANTIC_WRITE */

  RNA_property_collection_add(&self->ptr.value(), self->prop, &r_ptr);
  if (!r_ptr.data) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_collection.add(): not supported for this collection");
    return nullptr;
  }

  return pyrna_struct_CreatePyObject(&r_ptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_idprop_remove_doc,
    ".. method:: remove(index)\n"
    "\n"
    "   This is a function to remove an item from a collection.\n"
    "\n"
    "   :arg index: Index of the item to be removed.\n"
    "   :type index: int\n");
static PyObject *pyrna_prop_collection_idprop_remove(BPy_PropertyRNA *self, PyObject *value)
{
  const int key = PyLong_AsLong(value);

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), nullptr)) {
    return nullptr;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (key == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.remove(): expected one int argument");
    return nullptr;
  }

  if (!RNA_property_collection_remove(&self->ptr.value(), self->prop, key)) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_collection.remove() not supported for this collection");
    return nullptr;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_idprop_clear_doc,
    ".. method:: clear()\n"
    "\n"
    "   This is a function to remove all items from a collection.\n");
static PyObject *pyrna_prop_collection_idprop_clear(BPy_PropertyRNA *self)
{
#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), nullptr)) {
    return nullptr;
  }
#endif /* USE_PEDANTIC_WRITE */

  RNA_property_collection_clear(&self->ptr.value(), self->prop);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_idprop_move_doc,
    ".. method:: move(src_index, dst_index)\n"
    "\n"
    "   This is a function to move an item in a collection.\n"
    "\n"
    "   :arg src_index: Source item index.\n"
    "   :type src_index: int\n"
    "   :arg dst_index: Destination item index.\n"
    "   :type dst_index: int\n");
static PyObject *pyrna_prop_collection_idprop_move(BPy_PropertyRNA *self, PyObject *args)
{
  int key = 0, pos = 0;

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr.value(), nullptr)) {
    return nullptr;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (!PyArg_ParseTuple(args, "ii", &key, &pos)) {
    PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.move(): expected two ints as arguments");
    return nullptr;
  }

  if (!RNA_property_collection_move(&self->ptr.value(), self->prop, key, pos)) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_collection.move() not supported for this collection");
    return nullptr;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_get_id_data_doc,
    "The :class:`bpy.types.ID` object this data-block is from or None, "
    "(not available for all data types)\n"
    "\n"
    ":type: :class:`bpy.types.ID`, (readonly)\n");
static PyObject *pyrna_struct_get_id_data(BPy_DummyPointerRNA *self, void * /*closure*/)
{
  /* Used for struct and pointer since both have a ptr. */
  if (self->ptr->owner_id) {
    PointerRNA id_ptr = RNA_id_pointer_create((ID *)self->ptr->owner_id);
    return pyrna_struct_CreatePyObject(&id_ptr);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_get_data_doc,
    "The data this property is using, *type* :class:`bpy.types.bpy_struct`");
static PyObject *pyrna_struct_get_data(BPy_DummyPointerRNA *self, void * /*closure*/)
{
  return pyrna_struct_CreatePyObject(&self->ptr.value());
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_get_rna_type_doc,
    "The property type for introspection.");
static PyObject *pyrna_struct_get_rna_type(BPy_PropertyRNA *self, void * /*closure*/)
{
  PointerRNA tptr = RNA_pointer_create_discrete(nullptr, &RNA_Property, self->prop);
  return pyrna_struct_Subtype(&tptr);
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/

static PyGetSetDef pyrna_prop_getseters[] = {
    {"id_data",
     (getter)pyrna_struct_get_id_data,
     (setter) nullptr,
     pyrna_struct_get_id_data_doc,
     nullptr},
    {"data", (getter)pyrna_struct_get_data, (setter) nullptr, pyrna_struct_get_data_doc, nullptr},
    {"rna_type",
     (getter)pyrna_struct_get_rna_type,
     (setter) nullptr,
     pyrna_struct_get_rna_type_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef pyrna_struct_getseters[] = {
    {"id_data",
     (getter)pyrna_struct_get_id_data,
     (setter) nullptr,
     pyrna_struct_get_id_data_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyObject *pyrna_func_doc_get(BPy_FunctionRNA *self, void *closure);

static PyGetSetDef pyrna_func_getseters[] = {
    {"__doc__", (getter)pyrna_func_doc_get, (setter) nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_keys_doc,
    ".. method:: keys()\n"
    "\n"
    "   Return the identifiers of collection members\n"
    "   (matching Python's dict.keys() functionality).\n"
    "\n"
    "   :return: the identifiers for each member of this collection.\n"
    "   :rtype: list[str]\n");
static PyObject *pyrna_prop_collection_keys(BPy_PropertyRNA *self)
{
  PyObject *ret = PyList_New(0);

  BPy_NamePropAsPyObject_Cache nameprop_cache = {nullptr};
  RNA_PROP_BEGIN (&self->ptr.value(), itemptr, self->prop) {
    if (PyObject *name_py = pyrna_struct_get_nameprop_as_pyobject(&itemptr, nameprop_cache)) {
      PyList_APPEND(ret, name_py);
    }
  }
  RNA_PROP_END;

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_items_doc,
    ".. method:: items()\n"
    "\n"
    "   Return the identifiers of collection members\n"
    "   (matching Python's dict.items() functionality).\n"
    "\n"
    "   :return: (key, value) pairs for each member of this collection.\n"
    "   :rtype: list[tuple[str, :class:`bpy.types.bpy_struct`]]\n");
static PyObject *pyrna_prop_collection_items(BPy_PropertyRNA *self)
{
  PyObject *ret = PyList_New(0);
  int i = 0;

  BPy_NamePropAsPyObject_Cache nameprop_cache = {nullptr};
  RNA_PROP_BEGIN (&self->ptr.value(), itemptr, self->prop) {
    if (UNLIKELY(itemptr.data == nullptr)) {
      continue;
    }
    /* Add to Python list. */
    PyObject *item = PyTuple_New(2);
    PyObject *name_py = pyrna_struct_get_nameprop_as_pyobject(&itemptr, nameprop_cache);
    /* Strange to use an index `i` when `name_py` is null, better than excluding it's value.
     * In practice this should not happen. */
    PyTuple_SET_ITEM(item, 0, name_py ? name_py : PyLong_FromLong(i));
    PyTuple_SET_ITEM(item, 1, pyrna_struct_CreatePyObject(&itemptr));

    PyList_APPEND(ret, item);

    i++;
  }
  RNA_PROP_END;

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_values_doc,
    ".. method:: values()\n"
    "\n"
    "   Return the values of collection\n"
    "   (matching Python's dict.values() functionality).\n"
    "\n"
    "   :return: The members of this collection.\n"
    "   :rtype: list[:class:`bpy.types.bpy_struct` | None]\n");
static PyObject *pyrna_prop_collection_values(BPy_PropertyRNA *self)
{
  /* Re-use slice. */
  return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_get_doc,
    ".. method:: get(key, default=None, /)\n"
    "\n"
    "   Returns the value of the custom property assigned to key or default\n"
    "   when not found (matches Python's dictionary function of the same name).\n"
    "\n"
    "   :arg key: The key associated with the custom property.\n"
    "   :type key: str\n"
    "   :arg default: Optional argument for the value to return if\n"
    "      *key* is not found.\n"
    "   :type default: Any\n"
    "   :return: Custom property value or default.\n"
    "   :rtype: Any\n"
    "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_get(BPy_StructRNA *self, PyObject *args)
{
  IDProperty *group, *idprop;

  const char *key;
  PyObject *def = Py_None;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
    return nullptr;
  }

  /* Mostly copied from BPy_IDGroup_Map_GetItem. */
  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "this type doesn't support IDProperties");
    return nullptr;
  }

  group = RNA_struct_idprops(&self->ptr.value(), false);
  if (group) {
    idprop = IDP_GetPropertyFromGroup(group, key);

    if (idprop) {
      return BPy_IDGroup_WrapData(self->ptr->owner_id, idprop, group);
    }
  }

  return Py_NewRef(def);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_pop_doc,
    ".. method:: pop(key, default=None, /)\n"
    "\n"
    "   Remove and return the value of the custom property assigned to key or default\n"
    "   when not found (matches Python's dictionary function of the same name).\n"
    "\n"
    "   :arg key: The key associated with the custom property.\n"
    "   :type key: str\n"
    "   :arg default: Optional argument for the value to return if\n"
    "      *key* is not found.\n"
    "   :type default: Any\n"
    "   :return: Custom property value or default.\n"
    "   :rtype: Any\n"
    "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_pop(BPy_StructRNA *self, PyObject *args)
{
  IDProperty *group, *idprop;

  const char *key;
  PyObject *def = nullptr;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
    return nullptr;
  }

  /* Mostly copied from BPy_IDGroup_Map_GetItem. */
  if (RNA_struct_idprops_check(self->ptr->type) == 0) {
    PyErr_SetString(PyExc_TypeError, "this type doesn't support IDProperties");
    return nullptr;
  }

  group = RNA_struct_idprops(&self->ptr.value(), false);
  if (group) {
    idprop = IDP_GetPropertyFromGroup(group, key);

    if (idprop) {
      /* Don't use #BPy_IDGroup_WrapData as the id-property is being removed from the ID. */
      PyObject *ret = BPy_IDGroup_MapDataToPy(idprop);
      /* Internal error. */
      if (UNLIKELY(ret == nullptr)) {
        return nullptr;
      }
      IDP_FreeFromGroup(group, idprop);
      return ret;
    }
  }

  if (def == nullptr) {
    PyErr_SetString(PyExc_KeyError, "key not found");
    return nullptr;
  }
  return Py_NewRef(def);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_struct_as_pointer_doc,
    ".. method:: as_pointer()\n"
    "\n"
    "   Returns the memory address which holds a pointer to Blender's internal data\n"
    "\n"
    "   :return: int (memory address).\n"
    "   :rtype: int\n"
    "\n"
    "   .. note:: This is intended only for advanced script writers who need to\n"
    "      pass blender data to their own C/Python modules.\n");
static PyObject *pyrna_struct_as_pointer(BPy_StructRNA *self)
{
  return PyLong_FromVoidPtr(self->ptr->data);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_get_doc,
    ".. method:: get(key, default=None)\n"
    "\n"
    "   Returns the value of the item assigned to key or default when not found\n"
    "   (matches Python's dictionary function of the same name).\n"
    "\n"
    "   :arg key: The identifier for the collection member.\n"
    "   :type key: str\n"
    "   :arg default: Optional argument for the value to return if\n"
    "      *key* is not found.\n"
    "   :type default: Any\n");
static PyObject *pyrna_prop_collection_get(BPy_PropertyRNA *self, PyObject *args)
{
  PointerRNA newptr;

  PyObject *key_ob;
  PyObject *def = Py_None;

  PYRNA_PROP_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O|O:get", &key_ob, &def)) {
    return nullptr;
  }

  if (PyUnicode_Check(key_ob)) {
    const char *key = PyUnicode_AsUTF8(key_ob);

    if (RNA_property_collection_lookup_string(&self->ptr.value(), self->prop, key, &newptr)) {
      return pyrna_struct_CreatePyObject(&newptr);
    }
    if (pyrna_prop_collection_string_subscript_supported_or_error(self,
                                                                  "bpy_prop_collection.get") == -1)
    {
      return nullptr;
    }
  }
  else if (PyTuple_Check(key_ob)) {
    PyObject *ret = pyrna_prop_collection_subscript_str_lib_pair(
        self, key_ob, "bpy_prop_collection.get((id, lib))", false);
    if (ret) {
      return ret;
    }
  }
  else {
    PyErr_Format(PyExc_KeyError,
                 "bpy_prop_collection.get(key, ...): key must be a string or tuple, not %.200s",
                 Py_TYPE(key_ob)->tp_name);
  }

  return Py_NewRef(def);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_find_doc,
    ".. method:: find(key)\n"
    "\n"
    "   Returns the index of a key in a collection or -1 when not found\n"
    "   (matches Python's string find function of the same name).\n"
    "\n"
    "   :arg key: The identifier for the collection member.\n"
    "   :type key: str\n"
    "   :return: index of the key.\n"
    "   :rtype: int\n");
static PyObject *pyrna_prop_collection_find(BPy_PropertyRNA *self, PyObject *key_ob)
{
  Py_ssize_t key_len_ssize;
  const char *key = PyUnicode_AsUTF8AndSize(key_ob, &key_len_ssize);
  const int key_len = int(key_len_ssize); /* Compare with same type. */

  char name[256], *name_ptr;
  int name_len;
  int i = 0;
  int index = -1;

  PYRNA_PROP_CHECK_OBJ(self);

  RNA_PROP_BEGIN (&self->ptr.value(), itemptr, self->prop) {
    name_ptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &name_len);

    if (name_ptr) {
      if ((key_len == name_len) && memcmp(name_ptr, key, key_len) == 0) {
        index = i;
        break;
      }

      if (name != name_ptr) {
        MEM_freeN(name_ptr);
      }
    }

    i++;
  }
  RNA_PROP_END;

  return PyLong_FromLong(index);
}

static bool foreach_attr_type(BPy_PropertyRNA *self,
                              const char *attr,
                              /* Values to assign. */
                              RawPropertyType *r_raw_type,
                              int *r_attr_tot,
                              bool *r_attr_signed,
                              bool *r_is_empty)
{
  PropertyRNA *prop;
  bool attr_ok = true;
  *r_raw_type = PROP_RAW_UNSET;
  *r_attr_tot = 0;
  *r_attr_signed = false;
  *r_is_empty = true;

  /* NOTE: this is fail with zero length lists, so don't let this get called in that case. */
  RNA_PROP_BEGIN (&self->ptr.value(), itemptr, self->prop) {
    prop = RNA_struct_find_property(&itemptr, attr);
    if (prop) {
      *r_raw_type = RNA_property_raw_type(prop);
      *r_attr_tot = RNA_property_array_length(&itemptr, prop);
      *r_attr_signed = (RNA_property_subtype(prop) != PROP_UNSIGNED);
    }
    else {
      attr_ok = false;
    }
    *r_is_empty = false;
    break;
  }
  RNA_PROP_END;

  return attr_ok;
}

/* pyrna_prop_collection_foreach_get/set both use this. */
static int foreach_parse_args(BPy_PropertyRNA *self,
                              PyObject *args,
                              const char *function_name,

                              /* Values to assign. */
                              const char **r_attr,
                              PyObject **r_seq,
                              int *r_tot,
                              size_t *r_size,
                              RawPropertyType *r_raw_type,
                              int *r_attr_tot,
                              bool *r_attr_signed)
{
  *r_size = *r_attr_tot = 0;
  *r_attr_signed = false;
  *r_raw_type = PROP_RAW_UNSET;

  if (!PyArg_ParseTuple(args, "sO:foreach_get/set", r_attr, r_seq)) {
    return -1;
  }

  if (!PySequence_Check(*r_seq) && PyObject_CheckBuffer(*r_seq)) {
    PyErr_Format(PyExc_TypeError,
                 "%s(..) expected second argument to be a sequence or buffer, not a %.200s",
                 function_name,
                 Py_TYPE(*r_seq)->tp_name);
    return -1;
  }

  /* TODO: buffer may not be a sequence! array.array() is though. */
  *r_tot = PySequence_Size(*r_seq);

  if (*r_tot > 0) {
#if 0
    /* Avoid a full collection count when all that's needed is to check it's empty. */
    int array_tot;

    if (RNA_property_type(self->prop) == PROP_COLLECTION) {
      array_tot = RNA_property_collection_length(&self->ptr.value(), self->prop);
    }
    else {
      array_tot = RNA_property_array_length(&self->ptr.value(), self->prop);
    }
    if (array_tot == 0) {
      PyErr_Format(PyExc_TypeError,
                   "%s(..) sequence length mismatch given %d, needed 0",
                   function_name,
                   *r_tot);
      return -1;
    }
#endif

    bool is_empty = false; /* `array_tot == 0`. */
    if (!foreach_attr_type(self, *r_attr, r_raw_type, r_attr_tot, r_attr_signed, &is_empty)) {
      PyErr_Format(PyExc_AttributeError,
                   "%s(..) '%.200s.%200s[...]' elements have no attribute '%.200s'",
                   function_name,
                   RNA_struct_identifier(self->ptr->type),
                   RNA_property_identifier(self->prop),
                   *r_attr);
      return -1;
    }

    if (is_empty) {
      PyErr_Format(PyExc_TypeError,
                   "%s(..) sequence length mismatch given %d, needed 0",
                   function_name,
                   *r_tot);
      return -1;
    }

    *r_size = RNA_raw_type_sizeof(*r_raw_type);

#if 0
    /* This size check does not work as the size check is based on the size of the
     * first element and elements in the collection/array can have different sizes
     * (i.e. for mixed quad/triangle meshes). See for example issue #111117. */

    if ((*r_attr_tot) < 1) {
      *r_attr_tot = 1;
    }

    const int target_tot = array_tot * (*r_attr_tot);

    /* rna_access.cc - rna_raw_access(...) uses this same method. */
    if (target_tot != (*r_tot)) {
      PyErr_Format(PyExc_TypeError,
                   "%s(..) sequence length mismatch given %d, needed %d",
                   function_name,
                   *r_tot,
                   target_tot);
      return -1;
    }
#endif
  }

  /* Check 'r_attr_tot' otherwise we don't know if any values were set.
   * This isn't ideal because it means running on an empty list may
   * fail silently when it's not compatible. */
  if (*r_size == 0 && *r_attr_tot != 0) {
    PyErr_Format(
        PyExc_AttributeError, "%s(..): attribute does not support foreach method", function_name);
    return -1;
  }
  return 0;
}

static bool foreach_compat_buffer(RawPropertyType raw_type, int attr_signed, const char *format)
{
  const char f = format ? *format : 'B'; /* B is assumed when not set */

  switch (raw_type) {
    case PROP_RAW_INT8:
      if (attr_signed) {
        return (f == 'b') ? true : false;
      }
      else {
        return (f == 'B') ? true : false;
      }
    case PROP_RAW_CHAR:
    case PROP_RAW_UINT8:
      return (f == 'B') ? true : false;
    case PROP_RAW_SHORT:
      if (attr_signed) {
        return (f == 'h') ? true : false;
      }
      else {
        return (f == 'H') ? true : false;
      }
    case PROP_RAW_UINT16:
      return (f == 'H') ? true : false;
    case PROP_RAW_INT:
      if (attr_signed) {
        return (f == 'i') ? true : false;
      }
      else {
        return (f == 'I') ? true : false;
      }
    case PROP_RAW_BOOLEAN:
      return (f == '?') ? true : false;
    case PROP_RAW_FLOAT:
      return (f == 'f') ? true : false;
    case PROP_RAW_DOUBLE:
      return (f == 'd') ? true : false;
    case PROP_RAW_INT64:
      if (attr_signed) {
        return (f == 'q') ? true : false;
      }
      else {
        return (f == 'Q') ? true : false;
      }
    case PROP_RAW_UINT64:
      return (f == 'Q') ? true : false;
    case PROP_RAW_UNSET:
      return false;
  }

  return false;
}

static PyObject *foreach_getset(BPy_PropertyRNA *self, PyObject *args, int set)
{
  PyObject *item = nullptr;
  int i = 0, ok = 0;
  bool buffer_is_compat;
  void *array = nullptr;

  /* Get/set both take the same args currently. */
  const char *attr;
  PyObject *seq;
  int tot, attr_tot;
  size_t size;
  bool attr_signed;
  RawPropertyType raw_type;

  if (foreach_parse_args(self,
                         args,
                         set ? "foreach_set" : "foreach_get",
                         &attr,
                         &seq,
                         &tot,
                         &size,
                         &raw_type,
                         &attr_tot,
                         &attr_signed) == -1)
  {
    return nullptr;
  }

  if (tot == 0) {
    Py_RETURN_NONE;
  }

  if (set) { /* Get the array from python. */
    buffer_is_compat = false;
    if (PyObject_CheckBuffer(seq)) {
      Py_buffer buf;
      if (PyObject_GetBuffer(seq, &buf, PyBUF_ND | PyBUF_FORMAT) == -1) {
        /* Request failed. A `PyExc_BufferError` will have been raised,
         * so clear it to silently fall back to accessing as a sequence. */
        PyErr_Clear();
      }
      else {
        /* Check if the buffer matches. */

        buffer_is_compat = foreach_compat_buffer(raw_type, attr_signed, buf.format);

        if (buffer_is_compat) {
          ok = RNA_property_collection_raw_set(
              nullptr, &self->ptr.value(), self->prop, attr, buf.buf, raw_type, tot);
        }

        PyBuffer_Release(&buf);
      }
    }

    /* Could not use the buffer, fall back to sequence. */
    if (!buffer_is_compat) {
      array = PyMem_Malloc(size * tot);

      for (; i < tot; i++) {
        item = PySequence_GetItem(seq, i);
        switch (raw_type) {
          case PROP_RAW_CHAR:
            ((char *)array)[i] = char(PyC_Long_AsU8(item));
            break;
          case PROP_RAW_INT8:
            ((int8_t *)array)[i] = PyC_Long_AsI8(item);
            break;
          case PROP_RAW_UINT8:
            ((uint8_t *)array)[i] = PyC_Long_AsU8(item);
            break;
          case PROP_RAW_SHORT:
            ((short *)array)[i] = short(PyC_Long_AsI16(item));
            break;
          case PROP_RAW_UINT16:
            ((uint16_t *)array)[i] = PyC_Long_AsU16(item);
            break;
          case PROP_RAW_INT:
            ((int *)array)[i] = int(PyC_Long_AsI32(item));
            break;
          case PROP_RAW_BOOLEAN:
            ((bool *)array)[i] = bool(PyC_Long_AsBool(item));
            break;
          case PROP_RAW_FLOAT:
            ((float *)array)[i] = float(PyFloat_AsDouble(item));
            break;
          case PROP_RAW_DOUBLE:
            ((double *)array)[i] = PyFloat_AsDouble(item);
            break;
          case PROP_RAW_INT64:
            ((int64_t *)array)[i] = PyC_Long_AsI64(item);
            break;
          case PROP_RAW_UINT64:
            ((uint64_t *)array)[i] = PyC_Long_AsU64(item);
            break;
          case PROP_RAW_UNSET:
            /* Should never happen. */
            BLI_assert_msg(0, "Invalid array type - set");
            break;
        }

        Py_DECREF(item);
      }

      ok = RNA_property_collection_raw_set(
          nullptr, &self->ptr.value(), self->prop, attr, array, raw_type, tot);
    }
  }
  else {
    buffer_is_compat = false;
    if (PyObject_CheckBuffer(seq)) {
      Py_buffer buf;
      if (PyObject_GetBuffer(seq, &buf, PyBUF_ND | PyBUF_FORMAT) == -1) {
        /* Request failed. A `PyExc_BufferError` will have been raised,
         * so clear it to silently fall back to accessing as a sequence. */
        PyErr_Clear();
      }
      else {
        /* Check if the buffer matches. */

        buffer_is_compat = foreach_compat_buffer(raw_type, attr_signed, buf.format);

        if (buffer_is_compat) {
          ok = RNA_property_collection_raw_get(
              nullptr, &self->ptr.value(), self->prop, attr, buf.buf, raw_type, tot);
        }

        PyBuffer_Release(&buf);
      }
    }

    /* Could not use the buffer, fall back to sequence. */
    if (!buffer_is_compat) {
      array = PyMem_Malloc(size * tot);

      ok = RNA_property_collection_raw_get(
          nullptr, &self->ptr.value(), self->prop, attr, array, raw_type, tot);

      if (!ok) {
        /* Skip the loop. */
        i = tot;
      }

      for (; i < tot; i++) {

        switch (raw_type) {
          case PROP_RAW_CHAR:
            item = PyLong_FromLong(long(((char *)array)[i]));
            break;
          case PROP_RAW_INT8:
            item = PyLong_FromLong(long(((int8_t *)array)[i]));
            break;
          case PROP_RAW_UINT8:
            item = PyLong_FromLong(long(((uint8_t *)array)[i]));
            break;
          case PROP_RAW_SHORT:
            item = PyLong_FromLong(long(((short *)array)[i]));
            break;
          case PROP_RAW_UINT16:
            item = PyLong_FromLong(long(((uint16_t *)array)[i]));
            break;
          case PROP_RAW_INT:
            item = PyLong_FromLong(long(((int *)array)[i]));
            break;
          case PROP_RAW_FLOAT:
            item = PyFloat_FromDouble(double(((float *)array)[i]));
            break;
          case PROP_RAW_DOUBLE:
            item = PyFloat_FromDouble(((double *)array)[i]);
            break;
          case PROP_RAW_BOOLEAN:
            item = PyBool_FromLong(long(((bool *)array)[i]));
            break;
          case PROP_RAW_INT64:
            item = PyLong_FromLongLong(((int64_t *)array)[i]);
            break;
          case PROP_RAW_UINT64:
            item = PyLong_FromUnsignedLongLong(((uint64_t *)array)[i]);
            break;
          default: /* PROP_RAW_UNSET */
            /* Should never happen. */
            BLI_assert_msg(0, "Invalid array type - get");
            item = Py_None;
            Py_INCREF(item);
            break;
        }

        PySequence_SetItem(seq, i, item);
        Py_DECREF(item);
      }
    }
  }

  if (array) {
    PyMem_Free(array);
  }

  if (PyErr_Occurred()) {
    /* Maybe we could make our own error. */
    PyErr_Print();
    PyErr_SetString(PyExc_TypeError, "couldn't access the py sequence");
    return nullptr;
  }
  if (!ok) {
    PyErr_SetString(PyExc_RuntimeError, "internal error setting the array");
    return nullptr;
  }

  if (set) {
    RNA_property_update(BPY_context_get(), &self->ptr.value(), self->prop);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_foreach_get_doc,
    ".. method:: foreach_get(attr, seq)\n"
    "\n"
    "   This is a function to give fast access to attributes within a collection.\n");
static PyObject *pyrna_prop_collection_foreach_get(BPy_PropertyRNA *self, PyObject *args)
{
  PYRNA_PROP_CHECK_OBJ(self);

  return foreach_getset(self, args, 0);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_collection_foreach_set_doc,
    ".. method:: foreach_set(attr, seq)\n"
    "\n"
    "   This is a function to give fast access to attributes within a collection.\n");
static PyObject *pyrna_prop_collection_foreach_set(BPy_PropertyRNA *self, PyObject *args)
{
  PYRNA_PROP_CHECK_OBJ(self);

  return foreach_getset(self, args, 1);
}

static PyObject *pyprop_array_foreach_getset(BPy_PropertyArrayRNA *self,
                                             PyObject *args,
                                             const bool do_set)
{
  PyObject *item = nullptr;
  Py_ssize_t i, seq_size, size;
  void *array = nullptr;
  const PropertyType prop_type = RNA_property_type(self->prop);

  /* Get/set both take the same args currently. */
  PyObject *seq;

  if (!ELEM(prop_type, PROP_INT, PROP_FLOAT)) {
    PyErr_SetString(PyExc_TypeError, "foreach_get/set available only for int and float");
    return nullptr;
  }

  if (!PyArg_ParseTuple(args, "O:foreach_get/set", &seq)) {
    return nullptr;
  }

  if (!PySequence_Check(seq) && PyObject_CheckBuffer(seq)) {
    PyErr_Format(
        PyExc_TypeError,
        "foreach_get/set expected second argument to be a sequence or buffer, not a %.200s",
        Py_TYPE(seq)->tp_name);
    return nullptr;
  }

  /* NOTE: in this case it's important to use the flat-array size and *not* the result of
   * `len()`, which uses #pyrna_prop_array_length, see !116457 for details. */
  size = RNA_property_array_length(&self->ptr.value(), self->prop);
  seq_size = PySequence_Size(seq);

  if (size != seq_size) {
    PyErr_Format(PyExc_TypeError, "expected sequence size %d, got %d", size, seq_size);
    return nullptr;
  }

  Py_buffer buf;
  if (PyObject_GetBuffer(seq, &buf, PyBUF_ND | PyBUF_FORMAT) == -1) {
    PyErr_Clear();

    switch (prop_type) {
      case PROP_INT:
        array = PyMem_Malloc(sizeof(int) * size);
        if (do_set) {
          for (i = 0; i < size; i++) {
            item = PySequence_GetItem(seq, i);
            ((int *)array)[i] = int(PyLong_AsLong(item));
            Py_DECREF(item);
          }

          RNA_property_int_set_array(
              &self->ptr.value(), self->prop, static_cast<const int *>(array));
        }
        else {
          RNA_property_int_get_array(&self->ptr.value(), self->prop, static_cast<int *>(array));

          for (i = 0; i < size; i++) {
            item = PyLong_FromLong(long(((int *)array)[i]));
            PySequence_SetItem(seq, i, item);
            Py_DECREF(item);
          }
        }

        break;
      case PROP_FLOAT:
        array = PyMem_Malloc(sizeof(float) * size);
        if (do_set) {
          for (i = 0; i < size; i++) {
            item = PySequence_GetItem(seq, i);
            ((float *)array)[i] = float(PyFloat_AsDouble(item));
            Py_DECREF(item);
          }

          RNA_property_float_set_array(
              &self->ptr.value(), self->prop, static_cast<const float *>(array));
        }
        else {
          RNA_property_float_get_array(
              &self->ptr.value(), self->prop, static_cast<float *>(array));

          for (i = 0; i < size; i++) {
            item = PyFloat_FromDouble(double(((float *)array)[i]));
            PySequence_SetItem(seq, i, item);
            Py_DECREF(item);
          }
        }
        break;
      case PROP_BOOLEAN:
      case PROP_STRING:
      case PROP_ENUM:
      case PROP_POINTER:
      case PROP_COLLECTION:
        /* Should never happen. */
        BLI_assert_unreachable();
        break;
    }

    PyMem_Free(array);

    if (PyErr_Occurred()) {
      /* Maybe we could make our own error. */
      PyErr_Print();
      PyErr_SetString(PyExc_TypeError, "couldn't access the py sequence");
      return nullptr;
    }
  }
  else {
    const char f = buf.format ? buf.format[0] : 0;
    if ((prop_type == PROP_INT && (buf.itemsize != sizeof(int) || !ELEM(f, 'l', 'i'))) ||
        (prop_type == PROP_FLOAT && (buf.itemsize != sizeof(float) || f != 'f')))
    {
      PyBuffer_Release(&buf);
      PyErr_Format(PyExc_TypeError, "incorrect sequence item type: %s", buf.format);
      return nullptr;
    }

    switch (prop_type) {
      case PROP_INT:
        if (do_set) {
          RNA_property_int_set_array(
              &self->ptr.value(), self->prop, static_cast<const int *>(buf.buf));
        }
        else {
          RNA_property_int_get_array(&self->ptr.value(), self->prop, static_cast<int *>(buf.buf));
        }
        break;
      case PROP_FLOAT:
        if (do_set) {
          RNA_property_float_set_array(
              &self->ptr.value(), self->prop, static_cast<const float *>(buf.buf));
        }
        else {
          RNA_property_float_get_array(
              &self->ptr.value(), self->prop, static_cast<float *>(buf.buf));
        }
        break;
      case PROP_BOOLEAN:
      case PROP_STRING:
      case PROP_ENUM:
      case PROP_POINTER:
      case PROP_COLLECTION:
        /* Should never happen. */
        BLI_assert_unreachable();
        break;
    }

    PyBuffer_Release(&buf);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_array_foreach_get_doc,
    ".. method:: foreach_get(seq)\n"
    "\n"
    "   This is a function to give fast access to array data.\n");
static PyObject *pyrna_prop_array_foreach_get(BPy_PropertyArrayRNA *self, PyObject *args)
{
  PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

  return pyprop_array_foreach_getset(self, args, false);
}

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_prop_array_foreach_set_doc,
    ".. method:: foreach_set(seq)\n"
    "\n"
    "   This is a function to give fast access to array data.\n");
static PyObject *pyrna_prop_array_foreach_set(BPy_PropertyArrayRNA *self, PyObject *args)
{
  PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

  return pyprop_array_foreach_getset(self, args, true);
}

/* A bit of a kludge, make a list out of a collection or array,
 * then return the list's iter function, not especially fast, but convenient for now. */
static PyObject *pyrna_prop_array_iter(BPy_PropertyArrayRNA *self)
{
  /* Try get values from a collection. */
  PyObject *ret;
  PyObject *iter = nullptr;
  int len;

  PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

  len = pyrna_prop_array_length(self);
  ret = pyrna_prop_array_subscript_slice(self, &self->ptr.value(), self->prop, 0, len, len);

  /* we know this is a list so no need to PyIter_Check
   * otherwise it could be nullptr (unlikely) if conversion failed */
  if (ret) {
    iter = PyObject_GetIter(ret);
    Py_DECREF(ret);
  }

  return iter;
}

static PyObject *pyrna_prop_collection_iter(PyObject *self);

#ifndef USE_PYRNA_ITER
static PyObject *pyrna_prop_collection_iter(PyObject *self)
{
  BPy_PropertyRNA *self_property = reinterpret_cast<BPy_PropertyRNA *>(self);

  /* Try get values from a collection. */
  PyObject *ret;
  PyObject *iter = nullptr;
  ret = pyrna_prop_collection_values(self_property);

  /* we know this is a list so no need to PyIter_Check
   * otherwise it could be nullptr (unlikely) if conversion failed */
  if (ret) {
    iter = PyObject_GetIter(ret);
    Py_DECREF(ret);
  }

  return iter;
}
#endif /* # !USE_PYRNA_ITER */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef pyrna_struct_methods[] = {

    /* Only for PointerRNA's with ID'props. */
    {"keys", (PyCFunction)pyrna_struct_keys, METH_NOARGS, pyrna_struct_keys_doc},
    {"values", (PyCFunction)pyrna_struct_values, METH_NOARGS, pyrna_struct_values_doc},
    {"items", (PyCFunction)pyrna_struct_items, METH_NOARGS, pyrna_struct_items_doc},

    {"get", (PyCFunction)pyrna_struct_get, METH_VARARGS, pyrna_struct_get_doc},
    {"pop", (PyCFunction)pyrna_struct_pop, METH_VARARGS, pyrna_struct_pop_doc},

    {"as_pointer", (PyCFunction)pyrna_struct_as_pointer, METH_NOARGS, pyrna_struct_as_pointer_doc},

    /* `bpy_rna_anim.cc` */
    {"keyframe_insert",
     (PyCFunction)pyrna_struct_keyframe_insert,
     METH_VARARGS | METH_KEYWORDS,
     pyrna_struct_keyframe_insert_doc},
    {"keyframe_delete",
     (PyCFunction)pyrna_struct_keyframe_delete,
     METH_VARARGS | METH_KEYWORDS,
     pyrna_struct_keyframe_delete_doc},
    {"driver_add",
     (PyCFunction)pyrna_struct_driver_add,
     METH_VARARGS,
     pyrna_struct_driver_add_doc},
    {"driver_remove",
     (PyCFunction)pyrna_struct_driver_remove,
     METH_VARARGS,
     pyrna_struct_driver_remove_doc},

    {"is_property_set",
     (PyCFunction)pyrna_struct_is_property_set,
     METH_VARARGS | METH_KEYWORDS,
     pyrna_struct_is_property_set_doc},
    {"property_unset",
     (PyCFunction)pyrna_struct_property_unset,
     METH_VARARGS,
     pyrna_struct_property_unset_doc},
    {"is_property_hidden",
     (PyCFunction)pyrna_struct_is_property_hidden,
     METH_VARARGS,
     pyrna_struct_is_property_hidden_doc},
    {"is_property_readonly",
     (PyCFunction)pyrna_struct_is_property_readonly,
     METH_VARARGS,
     pyrna_struct_is_property_readonly_doc},
    {"is_property_overridable_library",
     (PyCFunction)pyrna_struct_is_property_overridable_library,
     METH_VARARGS,
     pyrna_struct_is_property_overridable_library_doc},
    {"property_overridable_library_set",
     (PyCFunction)pyrna_struct_property_overridable_library_set,
     METH_VARARGS,
     pyrna_struct_property_overridable_library_set_doc},
    {"path_resolve",
     (PyCFunction)pyrna_struct_path_resolve,
     METH_VARARGS,
     pyrna_struct_path_resolve_doc},
    {"path_from_id",
     (PyCFunction)pyrna_struct_path_from_id,
     METH_VARARGS,
     pyrna_struct_path_from_id_doc},
    {"path_from_module",
     (PyCFunction)pyrna_struct_path_from_module,
     METH_VARARGS,
     pyrna_struct_path_from_module_doc},
    {"type_recast",
     (PyCFunction)pyrna_struct_type_recast,
     METH_NOARGS,
     pyrna_struct_type_recast_doc},
    {"bl_rna_get_subclass_py",
     (PyCFunction)pyrna_struct_bl_rna_get_subclass_py,
     METH_VARARGS | METH_CLASS,
     pyrna_struct_bl_rna_get_subclass_py_doc},
    {"bl_rna_get_subclass",
     (PyCFunction)pyrna_struct_bl_rna_get_subclass,
     METH_VARARGS | METH_CLASS,
     pyrna_struct_bl_rna_get_subclass_doc},
    {"rna_ancestors",
     (PyCFunction)pyrna_struct_get_ancestors,
     METH_NOARGS,
     pyrna_struct_get_ancestors_doc},
    {"__dir__", (PyCFunction)pyrna_struct_dir, METH_NOARGS, nullptr},
    {"id_properties_ensure",
     (PyCFunction)pyrna_struct_id_properties_ensure,
     METH_NOARGS,
     pyrna_struct_id_properties_ensure_doc},
    {"id_properties_clear",
     (PyCFunction)pyrna_struct_id_properties_clear,
     METH_NOARGS,
     pyrna_struct_id_properties_clear_doc},
    {"id_properties_ui",
     (PyCFunction)pyrna_struct_id_properties_ui,
     METH_VARARGS,
     pyrna_struct_id_properties_ui_doc},

/* experimental */
/* unused for now */
#if 0
    {"callback_add", (PyCFunction)pyrna_callback_add, METH_VARARGS, nullptr},
    {"callback_remove", (PyCFunction)pyrna_callback_remove, METH_VARARGS, nullptr},

    {"callback_add",
     (PyCFunction)pyrna_callback_classmethod_add,
     METH_VARARGS | METH_CLASS,
     nullptr},
    {"callback_remove",
     (PyCFunction)pyrna_callback_classmethod_remove,
     METH_VARARGS | METH_CLASS,
     nullptr},
#endif
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef pyrna_prop_methods[] = {
    {"path_from_id",
     (PyCFunction)pyrna_prop_path_from_id,
     METH_NOARGS,
     pyrna_prop_path_from_id_doc},
    {"path_from_module",
     (PyCFunction)pyrna_prop_path_from_module,
     METH_NOARGS,
     pyrna_prop_path_from_module_doc},
    {"as_bytes", (PyCFunction)pyrna_prop_as_bytes, METH_NOARGS, pyrna_prop_as_bytes_doc},
    {"update", (PyCFunction)pyrna_prop_update, METH_NOARGS, pyrna_prop_update_doc},
    {"__dir__", (PyCFunction)pyrna_prop_dir, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef pyrna_prop_array_methods[] = {
    {"foreach_get",
     (PyCFunction)pyrna_prop_array_foreach_get,
     METH_VARARGS,
     pyrna_prop_array_foreach_get_doc},
    {"foreach_set",
     (PyCFunction)pyrna_prop_array_foreach_set,
     METH_VARARGS,
     pyrna_prop_array_foreach_set_doc},

    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef pyrna_prop_collection_methods[] = {
    {"foreach_get",
     (PyCFunction)pyrna_prop_collection_foreach_get,
     METH_VARARGS,
     pyrna_prop_collection_foreach_get_doc},
    {"foreach_set",
     (PyCFunction)pyrna_prop_collection_foreach_set,
     METH_VARARGS,
     pyrna_prop_collection_foreach_set_doc},

    {"keys", (PyCFunction)pyrna_prop_collection_keys, METH_NOARGS, pyrna_prop_collection_keys_doc},
    {"items",
     (PyCFunction)pyrna_prop_collection_items,
     METH_NOARGS,
     pyrna_prop_collection_items_doc},
    {"values",
     (PyCFunction)pyrna_prop_collection_values,
     METH_NOARGS,
     pyrna_prop_collection_values_doc},

    {"get", (PyCFunction)pyrna_prop_collection_get, METH_VARARGS, pyrna_prop_collection_get_doc},
    {"find", (PyCFunction)pyrna_prop_collection_find, METH_O, pyrna_prop_collection_find_doc},
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef pyrna_prop_collection_idprop_methods[] = {
    {"add",
     (PyCFunction)pyrna_prop_collection_idprop_add,
     METH_NOARGS,
     pyrna_prop_collection_idprop_add_doc},
    {"remove",
     (PyCFunction)pyrna_prop_collection_idprop_remove,
     METH_O,
     pyrna_prop_collection_idprop_remove_doc},
    {"clear",
     (PyCFunction)pyrna_prop_collection_idprop_clear,
     METH_NOARGS,
     pyrna_prop_collection_idprop_clear_doc},
    {"move",
     (PyCFunction)pyrna_prop_collection_idprop_move,
     METH_VARARGS,
     pyrna_prop_collection_idprop_move_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyObject *pyrna_param_to_py(PointerRNA *ptr, PropertyRNA *prop, void *data)
{
  PyObject *ret;
  const int type = RNA_property_type(prop);
  const int flag = RNA_property_flag(prop);
  const int flag_parameter = RNA_parameter_flag(prop);

  if (RNA_property_array_check(prop)) {
    int a, len;

    if (flag & PROP_DYNAMIC) {
      ParameterDynAlloc *data_alloc = static_cast<ParameterDynAlloc *>(data);
      len = data_alloc->array_tot;
      data = data_alloc->array;
    }
    else {
      len = RNA_property_array_length(ptr, prop);
    }

    /* Resolve the array from a new Python type. */

    /* TODO(Kazanbas): make multi-dimensional sequences here. */

    switch (type) {
      case PROP_BOOLEAN:
        ret = PyTuple_New(len);
        for (a = 0; a < len; a++) {
          PyTuple_SET_ITEM(ret, a, PyBool_FromLong(((bool *)data)[a]));
        }
        break;
      case PROP_INT:
        ret = PyTuple_New(len);
        for (a = 0; a < len; a++) {
          PyTuple_SET_ITEM(ret, a, PyLong_FromLong(((int *)data)[a]));
        }
        break;
      case PROP_FLOAT:
        switch (RNA_property_subtype(prop)) {
#ifdef USE_MATHUTILS
          case PROP_ALL_VECTOR_SUBTYPES:
            ret = Vector_CreatePyObject(static_cast<const float *>(data), len, nullptr);
            break;
          case PROP_MATRIX:
            if (len == 16) {
              ret = Matrix_CreatePyObject(static_cast<const float *>(data), 4, 4, nullptr);
              break;
            }
            else if (len == 9) {
              ret = Matrix_CreatePyObject(static_cast<const float *>(data), 3, 3, nullptr);
              break;
            }
            ATTR_FALLTHROUGH;
#endif
          default:
            ret = PyTuple_New(len);
            for (a = 0; a < len; a++) {
              PyTuple_SET_ITEM(ret, a, PyFloat_FromDouble(((float *)data)[a]));
            }
            break;
        }
        break;
      default:
        PyErr_Format(
            PyExc_TypeError, "RNA Error: unknown array type \"%d\" (pyrna_param_to_py)", type);
        ret = nullptr;
        break;
    }
  }
  else {
    /* See if we can coerce into a python type - PropertyType. */
    switch (type) {
      case PROP_BOOLEAN:
        ret = PyBool_FromLong(*(bool *)data);
        break;
      case PROP_INT:
        ret = PyLong_FromLong(*(int *)data);
        break;
      case PROP_FLOAT:
        ret = PyFloat_FromDouble(*(float *)data);
        break;
      case PROP_STRING: {
        const char *data_ch;
        const int subtype = RNA_property_subtype(prop);
        size_t data_ch_len;

        if (flag & PROP_DYNAMIC) {
          ParameterDynAlloc *data_alloc = static_cast<ParameterDynAlloc *>(data);
          data_ch = static_cast<const char *>(data_alloc->array);
          data_ch_len = data_alloc->array_tot;
          BLI_assert((data_ch == nullptr) || strlen(data_ch) == data_ch_len);
        }
        else {
          data_ch = (flag & PROP_THICK_WRAP) ? (char *)data : *(char **)data;
          data_ch_len = data_ch ? strlen(data_ch) : 0;
        }

        if (UNLIKELY(data_ch == nullptr)) {
          BLI_assert((flag & PROP_NEVER_NULL) == 0);
          ret = Py_None;
          Py_INCREF(ret);
        }
#ifdef USE_STRING_COERCE
        else if (subtype == PROP_BYTESTRING) {
          ret = PyBytes_FromStringAndSize(data_ch, data_ch_len);
        }
        else if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
          ret = PyC_UnicodeFromBytesAndSize(data_ch, data_ch_len);
        }
        else {
          ret = PyUnicode_FromStringAndSize(data_ch, data_ch_len);
        }
#else
        else if (subtype == PROP_BYTESTRING) {
          ret = PyBytes_FromString(buf);
        }
        else {
          ret = PyUnicode_FromString(data_ch);
        }
#endif

        break;
      }
      case PROP_ENUM: {
        ret = pyrna_enum_to_py(ptr, prop, *(int *)data);
        break;
      }
      case PROP_POINTER: {
        PointerRNA newptr;
        PointerRNA *newptr_p = nullptr;
        StructRNA *ptype = RNA_property_pointer_type(ptr, prop);

        if (flag_parameter & PARM_RNAPTR) {
          if (flag & PROP_THICK_WRAP) {
            /* `data` points to a PointerRNA. */
            newptr_p = static_cast<PointerRNA *>(data);
          }
          else {
            /* `data` points to a pointer to a PointerRNA. */
            newptr_p = *reinterpret_cast<PointerRNA **>(data);
          }
        }
        else {
          if (RNA_struct_is_ID(ptype)) {
            newptr = RNA_id_pointer_create(static_cast<ID *>(*(void **)data));
          }
          else {
            /* NOTE: this is taken from the function's ID pointer
             * and will break if a function returns a pointer from
             * another ID block, watch this! - it should at least be
             * easy to debug since they are all ID's */
            newptr = RNA_pointer_create_discrete(ptr->owner_id, ptype, *(void **)data);
          }
          newptr_p = &newptr;
        }

        if (newptr_p->data) {
          ret = pyrna_struct_CreatePyObject(newptr_p);
        }
        else {
          ret = Py_None;
          Py_INCREF(ret);
        }
        break;
      }
      case PROP_COLLECTION: {
        CollectionVector *lb = (CollectionVector *)data;
        ret = PyList_New(0);
        for (PointerRNA &ptr_iter : lb->items) {
          PyList_APPEND(ret, pyrna_struct_CreatePyObject(&ptr_iter));
        }
        break;
      }
      default:
        PyErr_Format(PyExc_TypeError, "RNA Error: unknown type \"%d\" (pyrna_param_to_py)", type);
        ret = nullptr;
        break;
    }
  }

  return ret;
}

inline static PyObject *kwnames_get_item_string(PyObject *const *args,
                                                Py_ssize_t nargs,
                                                PyObject *kwnames,
                                                Py_ssize_t nkw,
                                                const char *parm_name)
{
  const Py_ssize_t parm_name_len = strlen(parm_name);

  for (Py_ssize_t i = 0; i < nkw; i++) {
    PyObject *key = PyTuple_GET_ITEM(kwnames, i); /* Borrow reference. */
    if (PyUnicode_Check(key)) {
      Py_ssize_t key_buf_len;
      const char *key_buf = PyUnicode_AsUTF8AndSize(key, &key_buf_len);
      if ((parm_name_len == key_buf_len) && (memcmp(parm_name, key_buf, parm_name_len) == 0)) {
        return args[nargs + i]; /* Borrow reference. */
      }
    }
  }

  return nullptr;
}

/**
 * \param parm_index: The argument index or -1 for keyword arguments.
 */
static void pyrna_func_error_prefix(BPy_FunctionRNA *self,
                                    PropertyRNA *parm,
                                    const int parm_index,
                                    char *error,
                                    const size_t error_size)
{
  PointerRNA *self_ptr = &self->ptr.value();
  FunctionRNA *self_func = self->func;
  if (parm_index == -1) {
    BLI_snprintf_utf8(error,
                      error_size,
                      "%.200s.%.200s(): error with keyword argument \"%.200s\" - ",
                      RNA_struct_identifier(self_ptr->type),
                      RNA_function_identifier(self_func),
                      RNA_property_identifier(parm));
  }
  else {
    BLI_snprintf_utf8(error,
                      error_size,
                      "%.200s.%.200s(): error with argument %d, \"%.200s\" - ",
                      RNA_struct_identifier(self_ptr->type),
                      RNA_function_identifier(self_func),
                      parm_index + 1,
                      RNA_property_identifier(parm));
  }
}

/**
 * Vectorcall implementation for BPy_FunctionRNA instances.
 *
 * Required by PEP 590 to support tp_vectorcall_offset.
 */
static PyObject *pyrna_func_vectorcall(PyObject *callable,
                                       PyObject *const *args,
                                       size_t nargsf,
                                       PyObject *kwnames)
{
  /* NOTE: both BPy_StructRNA and BPy_PropertyRNA can be used here. */
  BPy_FunctionRNA *self = reinterpret_cast<BPy_FunctionRNA *>(callable);
  PointerRNA *self_ptr = &self->ptr.value();
  FunctionRNA *self_func = self->func;

  ParameterList parms;
  ParameterIterator iter;
  PropertyRNA *parm;
  PyObject *ret, *item;
  int i, parms_len, ret_len, flag_parameter, err = 0, kw_tot = 0;
  bool kw_arg;

  PropertyRNA *pret_single = nullptr;
  void *retdata_single = nullptr;

  /* enable this so all strings are copied and freed after calling.
   * this exposes bugs where the pointer to the string is held and re-used */
  // #define DEBUG_STRING_FREE

#ifdef DEBUG_STRING_FREE
  PyObject *string_free_ls = PyList_New(0);
#endif

  /* Should never happen, but it does in rare cases. */
  BLI_assert(self_ptr != nullptr);

  if (self_ptr == nullptr) {
    PyErr_SetString(PyExc_RuntimeError,
                    "RNA functions internal RNA pointer is null, this is a bug. aborting");
    return nullptr;
  }

  if (self_func == nullptr) {
    PyErr_Format(
        PyExc_RuntimeError,
        "%.200s.<unknown>(): RNA function internal function is null, this is a bug. aborting",
        RNA_struct_identifier(self_ptr->type));
    return nullptr;
  }

/* For testing. */
#if 0
  {
    const char *fn;
    int lineno;
    PyC_FileAndNum(&fn, &lineno);
    printf("pyrna_func_call > %.200s.%.200s : %.200s:%d\n",
           RNA_struct_identifier(self_ptr->type),
           RNA_function_identifier(self_func),
           fn,
           lineno);
  }
#endif

  /* include the ID pointer for pyrna_param_to_py() so we can include the
   * ID pointer on return values, this only works when returned values have
   * the same ID as the functions. */
  PointerRNA funcptr = RNA_pointer_create_discrete(self_ptr->owner_id, &RNA_Function, self_func);

  const Py_ssize_t pyargs_len = PyVectorcall_NARGS(nargsf);
  const Py_ssize_t pykw_len = kwnames ? PyTuple_GET_SIZE(kwnames) : 0;

  RNA_parameter_list_create(&parms, self_ptr, self_func);
  RNA_parameter_list_begin(&parms, &iter);
  parms_len = RNA_parameter_list_arg_count(&parms);
  ret_len = 0;

  if (pyargs_len + pykw_len > parms_len) {
    RNA_parameter_list_end(&iter);
    PyErr_Format(PyExc_TypeError,
                 "%.200s.%.200s(): takes at most %d arguments, got %d",
                 RNA_struct_identifier(self_ptr->type),
                 RNA_function_identifier(self_func),
                 parms_len,
                 pyargs_len + pykw_len);
    err = -1;
  }

  /* Parse function parameters. */
  for (i = 0; iter.valid && err == 0; RNA_parameter_list_next(&iter)) {
    parm = iter.parm;
    flag_parameter = RNA_parameter_flag(parm);
    /* Only useful for single argument returns, we'll need another list loop for multiple. */
    if (flag_parameter & PARM_OUTPUT) {
      ret_len++;
      if (pret_single == nullptr) {
        pret_single = parm;
        retdata_single = iter.data;
      }

      continue;
    }

    item = nullptr;

    if (i < pyargs_len) {
      /* New in 2.8x, optional arguments must be keywords. */
      if (UNLIKELY((flag_parameter & PARM_REQUIRED) == 0)) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s.%.200s(): required parameter \"%.200s\" to be a keyword argument!",
                     RNA_struct_identifier(self_ptr->type),
                     RNA_function_identifier(self_func),
                     RNA_property_identifier(parm));
        err = -1;
        break;
      }

      item = args[i];
      kw_arg = false;
    }
    else if (kwnames != nullptr) {
      item = kwnames_get_item_string(args,
                                     pyargs_len,
                                     kwnames,
                                     pykw_len,
                                     RNA_property_identifier(parm)); /* Borrow reference. */

      if (item) {
        kw_tot++; /* Make sure invalid keywords are not given. */
      }

      kw_arg = true;
    }

    if (item == nullptr) {
      if (flag_parameter & PARM_REQUIRED) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s.%.200s(): required parameter \"%.200s\" not specified",
                     RNA_struct_identifier(self_ptr->type),
                     RNA_function_identifier(self_func),
                     RNA_property_identifier(parm));
        err = -1;
        break;
      }
      /* PyDict_GetItemString won't raise an error. */
    }
    else {

#ifdef DEBUG_STRING_FREE
      if (item) {
        if (PyUnicode_Check(item)) {
          PyList_APPEND(string_free_ls, PyUnicode_FromString(PyUnicode_AsUTF8(item)));
        }
      }
#endif

      /* the error generated isn't that useful, so generate it again with a useful prefix
       * could also write a function to prepend to error messages */
      char error_prefix[512];

      err = pyrna_py_to_prop(&funcptr, parm, iter.data, item, "");

      if (err != 0) {
        PyErr_Clear(); /* Re-raise. */
        pyrna_func_error_prefix(self, parm, kw_arg ? -1 : i, error_prefix, sizeof(error_prefix));
        pyrna_py_to_prop(&funcptr, parm, iter.data, item, error_prefix);

        break;
      }
    }

    i++; /* Current argument. */
  }

  RNA_parameter_list_end(&iter);

  /* Check if we gave args that don't exist in the function
   * Printing the error is slow, but it should only happen when developing.
   * The "if" below is quick check to make sure less keyword args were passed than we gave.
   * (Don't overwrite the error if we have one,
   * otherwise can skip important messages and confuse with args).
   */
  if (UNLIKELY(err == 0 && kwnames && (pykw_len > kw_tot))) {
    DynStr *bad_args = BLI_dynstr_new();
    DynStr *good_args = BLI_dynstr_new();

    const char *arg_name, *bad_args_str, *good_args_str;
    bool found = false, first = true;

    for (Py_ssize_t i = 0; i < pykw_len; i++) {
      PyObject *key = PyTuple_GET_ITEM(kwnames, i); /* Borrow reference. */

      arg_name = PyUnicode_AsUTF8(key);
      found = false;

      if (arg_name == nullptr)
      { /* Unlikely the `arg_name` is not a string, but ignore if it is. */
        PyErr_Clear();
      }
      else {
        /* Search for arg_name. */
        RNA_parameter_list_begin(&parms, &iter);
        for (; iter.valid; RNA_parameter_list_next(&iter)) {
          parm = iter.parm;
          if (STREQ(arg_name, RNA_property_identifier(parm))) {
            found = true;
            break;
          }
        }

        RNA_parameter_list_end(&iter);

        if (found == false) {
          BLI_dynstr_appendf(bad_args, first ? "%s" : ", %s", arg_name);
          first = false;
        }
      }
    }

    /* List good args. */
    first = true;

    RNA_parameter_list_begin(&parms, &iter);
    for (; iter.valid; RNA_parameter_list_next(&iter)) {
      parm = iter.parm;
      if (RNA_parameter_flag(parm) & PARM_OUTPUT) {
        continue;
      }

      BLI_dynstr_appendf(good_args, first ? "%s" : ", %s", RNA_property_identifier(parm));
      first = false;
    }
    RNA_parameter_list_end(&iter);

    bad_args_str = BLI_dynstr_get_cstring(bad_args);
    good_args_str = BLI_dynstr_get_cstring(good_args);

    PyErr_Format(
        PyExc_TypeError,
        "%.200s.%.200s(): was called with invalid keyword argument(s) (%s), expected (%s)",
        RNA_struct_identifier(self_ptr->type),
        RNA_function_identifier(self_func),
        bad_args_str,
        good_args_str);

    BLI_dynstr_free(bad_args);
    BLI_dynstr_free(good_args);
    MEM_freeN(bad_args_str);
    MEM_freeN(good_args_str);

    err = -1;
  }

  ret = nullptr;
  if (err == 0) {
    /* Call function. */
    ReportList reports;
    bContext *C = BPY_context_get();

    /* No need to print any reports. We will turn errors into Python exceptions, and
     * Python API calls should be silent and not print info or warning messages. */
    BKE_reports_init(&reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);
    RNA_function_call(C, &reports, self_ptr, self_func, &parms);

    err = BPy_reports_to_error(&reports, PyExc_RuntimeError, true);

    /* Return value. */
    if (err != -1) {
      if (ret_len > 0) {
        if (ret_len > 1) {
          ret = PyTuple_New(ret_len);
          i = 0; /* Arg index. */

          RNA_parameter_list_begin(&parms, &iter);

          for (; iter.valid; RNA_parameter_list_next(&iter)) {
            parm = iter.parm;

            if (RNA_parameter_flag(parm) & PARM_OUTPUT) {
              PyTuple_SET_ITEM(ret, i++, pyrna_param_to_py(&funcptr, parm, iter.data));
            }
          }

          RNA_parameter_list_end(&iter);
        }
        else {
          ret = pyrna_param_to_py(&funcptr, pret_single, retdata_single);
        }

        /* Possible there is an error in conversion. */
        if (ret == nullptr) {
          err = -1;
        }
      }
    }
  }

#ifdef DEBUG_STRING_FREE
#  if 0
  if (PyList_GET_SIZE(string_free_ls)) {
    printf("%.200s.%.200s(): has %d strings\n",
           RNA_struct_identifier(self_ptr->type),
           RNA_function_identifier(self_func),
           int(PyList_GET_SIZE(string_free_ls)));
  }
#  endif
  Py_DECREF(string_free_ls);
#  undef DEBUG_STRING_FREE
#endif

  /* Cleanup. */
  RNA_parameter_list_end(&iter);
  RNA_parameter_list_free(&parms);

  if (ret) {
    return ret;
  }

  if (err == -1) {
    return nullptr;
  }

  Py_RETURN_NONE;
}

static PyObject *pyrna_func_doc_get(BPy_FunctionRNA *self, void * /*closure*/)
{
  PyObject *ret;

  std::string args = RNA_function_as_string_keywords(nullptr, self->func, true, true, INT_MAX);

  ret = PyUnicode_FromFormat("%.200s.%.200s(%.200s)\n%s",
                             RNA_struct_identifier(self->ptr->type),
                             RNA_function_identifier(self->func),
                             args.c_str(),
                             RNA_function_ui_description(self->func));

  return ret;
}

PyTypeObject pyrna_struct_meta_idprop_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_struct_meta_idprop",
    /* NOTE: would be `sizeof(PyTypeObject)`,
     * but sub-types of Type must be #PyHeapTypeObject's. */
    /*tp_basicsize*/ sizeof(PyHeapTypeObject),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr, /* Sub-classed: #pyrna_struct_meta_idprop_getattro. */
    /*tp_setattro*/ (setattrofunc)pyrna_struct_meta_idprop_setattro,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
#if defined(_MSC_VER)
    /*tp_base*/ nullptr, /* Defer assignment. */
#else
    /*tp_base*/ &PyType_Type,
#endif
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

/*-----------------------BPy_StructRNA method def------------------------------*/

/* -------------------------------------------------------------------- */
/** \name BPY RNA Struct Type
 *
 * These BPy_StructRNA objects should be created the standard way (calling their type objects
 * using #PyObject_CallOneArg or similar). One and only one arg is expected currently.
 *
 * This special handling allows to construct an object from a python-defined derived type of
 * `bpy_struct`, using an existing base struct object as source of data.
 *
 * It also allows internal bpy_rna.cc code to pass the private #BPy_RNANoInit_object token to
 * create property objects without initializing them, which avoids double-initialization from
 * functions like #pyrna_struct_CreatePyObject etc.
 *
 * \note: Subclassing from Python isn't common since it's NOT related to registerable sub-classes.
 * eg:
 *
 * \code{.unparsed}
 * >>> class MyObSubclass(bpy.types.Object):
 * ...     def test_func(self):
 * ...         print(100)
 * ...
 * >>> myob = MyObSubclass(bpy.context.object)
 * >>> myob.test_func()
 * 100
 * \endcode
 *
 * \{ */

static PyObject *pyrna_struct_new(PyTypeObject *type, PyObject *args, PyObject * /*kwds*/);
static int pyrna_struct_init(PyObject *self, PyObject *args, PyObject * /*kwds*/);
static void pyrna_struct_dealloc(PyObject *self);

PyTypeObject pyrna_struct_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_struct",
    /*tp_basicsize*/ sizeof(BPy_StructRNA),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ pyrna_struct_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)pyrna_struct_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ &pyrna_struct_as_sequence,
    /*tp_as_mapping*/ &pyrna_struct_as_mapping,
    /*tp_hash*/ (hashfunc)pyrna_struct_hash,
    /*tp_call*/ nullptr,
    /*tp_str*/ (reprfunc)pyrna_struct_str,
    /*tp_getattro*/ (getattrofunc)pyrna_struct_getattro,
    /*tp_setattro*/ (setattrofunc)pyrna_struct_setattro,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE
#ifdef USE_PYRNA_STRUCT_REFERENCE
        | Py_TPFLAGS_HAVE_GC
#endif
    ,
    /*tp_doc*/ nullptr,
#ifdef USE_PYRNA_STRUCT_REFERENCE
    /*tp_traverse*/ (traverseproc)pyrna_struct_traverse,
    /*tp_clear*/ (inquiry)pyrna_struct_clear,
#else
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
#endif /* !USE_PYRNA_STRUCT_REFERENCE */
    /*tp_richcompare*/ (richcmpfunc)pyrna_struct_richcmp,
#ifdef USE_WEAKREFS
    /*tp_weaklistoffset*/ offsetof(BPy_StructRNA, in_weakreflist),
#else
    0,
#endif
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pyrna_struct_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pyrna_struct_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ pyrna_struct_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pyrna_struct_new,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

static PyObject *pyrna_struct_new(PyTypeObject *type, PyObject *args, PyObject * /*kwds*/)
{
  if (PyTuple_GET_SIZE(args) != 1) {
    PyErr_Format(PyExc_TypeError, "bpy_struct.__new__(struct): expected a single argument");
    return nullptr;
  }

  PyObject *arg_1 = PyTuple_GET_ITEM(args, 0);

  /* Ignore the special 'PyCapsule' argument used only by internal bpy_rna code. */
  if (!PyCapsule_CheckExact(arg_1)) {
    BPy_StructRNA *base = reinterpret_cast<BPy_StructRNA *>(arg_1);
    /* Error, invalid type given. */
    if (!PyType_IsSubtype(Py_TYPE(base), &pyrna_struct_Type)) {
      PyErr_Format(
          PyExc_TypeError,
          "bpy_struct.__new__(struct): struct type '%.200s' is not a subtype of bpy_struct",
          Py_TYPE(base)->tp_name);
      return nullptr;
    }
    /* Same type, simply use the given data. */
    if (Py_TYPE(base) == type) {
      BLI_assert(base->ptr.has_value());
      Py_INCREF(base);
      return reinterpret_cast<PyObject *>(base);
    }
    /* NOTE: Further copy of the 'base' content into the new object will happen in
     * #pyrna_struct_init. */
  }

  /* Only allocate the #PyObject data, do not construct/initialize anything else. */
  PyObject *self = type->tp_alloc(type, 0);
  BPy_StructRNA *self_struct = reinterpret_cast<BPy_StructRNA *>(self);
  if (self) {
#ifdef USE_PYRNA_STRUCT_REFERENCE
    /* #PyType_GenericAlloc will have set tracking.
     * We only want tracking when `StructRNA.reference` has been set. */
    PyObject_GC_UnTrack(self);
#endif
    self_struct->ptr = std::nullopt;
  }
  /* Pass on exception & nullptr if tp_alloc fails. */
  return self;
}

static int pyrna_struct_init(PyObject *self, PyObject *args, PyObject * /*kwds*/)
{
  BPy_StructRNA *self_struct = reinterpret_cast<BPy_StructRNA *>(self);

  size_t args_num = PyTuple_GET_SIZE(args);
  if (args_num != 1) {
    PyErr_Format(PyExc_TypeError, "bpy_struct.__init__(self, struct): expected a single argument");
    return -1;
  }

  PyObject *arg_1 = PyTuple_GET_ITEM(args, 0);
  const PointerRNA *ptr = nullptr;
  if (PyCapsule_CheckExact(arg_1)) {
    /* `bpy_rna` internal code will call object creation with a PyCapsule argument wrapping up a
     * PointerRNA pointer. */
    ptr = static_cast<PointerRNA *>(
        PyCapsule_GetPointer(arg_1, BPy_capsule_PointerRNA_identifier));
  }
  else {
    BPy_StructRNA *base_struct = reinterpret_cast<BPy_StructRNA *>(arg_1);

    /* Error, invalid type given. */
    if (!PyType_IsSubtype(Py_TYPE(base_struct), &pyrna_struct_Type)) {
      PyErr_Format(PyExc_TypeError,
                   "bpy_struct.__init__(self, struct): struct type '%.200s' is not a subtype of "
                   "bpy_struct",
                   Py_TYPE(base_struct)->tp_name);
      return -1;
    }
    /* Same object as the 'base' argument (see #pyrna_struct_new), nothing to do . */
    if (base_struct == self_struct) {
      BLI_assert(self_struct->ptr.has_value());
      return 0;
    }

    /* Else, use data from the base to initialize this object. */
    ptr = &base_struct->ptr.value();
  }

  if (ptr == nullptr) {
    PyErr_Format(PyExc_TypeError,
                 "bpy_struct.__init__(self, struct): failed to get a valid PointerRNA data "
                 "from the given `struct` argument");
    return -1;
  }

  self_struct->ptr.reset();
  self_struct->ptr = *ptr;

  return 0;
}

/* Own dealloc so that the PointerRNA can be destructed, and the IDProperty storage can be freed if
 * needed.
 *
 * Note: This should rather be in `tp_finalize` (aka `__del__`), but static types default dealloc
 * never calls this. */
static void pyrna_struct_dealloc(PyObject *self)
{
  /* Save the current exception, if any. */
  PyObject *error_type, *error_value, *error_traceback;
  PyErr_Fetch(&error_type, &error_value, &error_traceback);

  BPy_StructRNA *self_struct = reinterpret_cast<BPy_StructRNA *>(self);

#ifdef PYRNA_FREE_SUPPORT
  if (self_struct->freeptr && self_struct->ptr->data) {
    IDP_FreeProperty(self_struct->ptr->data);
    self_struct->ptr->data = nullptr;
  }
#endif /* PYRNA_FREE_SUPPORT */

#ifdef USE_WEAKREFS
  if (self_struct->in_weakreflist != nullptr) {
    PyObject_ClearWeakRefs(self);
  }
#endif

#ifdef USE_PYRNA_STRUCT_REFERENCE
  if (self_struct->reference) {
    PyObject_GC_UnTrack(self);
    pyrna_struct_clear(self_struct);
  }
  else {
    PyTypeObject *base = Py_TYPE(self)->tp_base;
    /* Python temporarily tracks these types when freeing, see Python bug 26617. */
    if (base && PyType_IS_GC(base)) {
      PyObject_GC_UnTrack(self);
    }
    BLI_assert(!PyObject_GC_IsTracked(self));
  }
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

  self_struct->ptr.~optional();

  Py_TYPE(self)->tp_free(self);

  /* Restore the saved exception. */
  PyErr_Restore(error_type, error_value, error_traceback);
}

/** \} */

/*-----------------------BPy_PropertyRNA method def------------------------------*/

/* -------------------------------------------------------------------- */
/** \name BPY RNA Property Types
 *
 * These BPy_PropertyRNA objects should be created the standard way (calling their type objects
 * using #PyObject_CallOneArg or similar). One and only one arg is expected currently.
 *
 * This special handling allows to construct an object from a python-defined derived type of
 * `bpy_property`, using an existing base property object as source of data.
 *
 * It also allows internal bpy_rna.cc code to pass the private #BPy_RNANoInit_object token to
 * create property objects without initializing them, which avoids double-initialization from
 * functions like #pyrna_prop_CreatePyObject etc.
 *
 * \note: This isn't common since it's NOT related to registerable subclasses. eg:
 *
 * TODO Add python code example of using this overriding feature.
 *
 * \{ */

static PyObject *pyrna_property_new(PyTypeObject *type, PyObject *args, PyObject * /*kwds*/);
static int pyrna_property_init(PyObject *self, PyObject *args, PyObject * /*kwds*/);
static void pyrna_property_dealloc(PyObject *self);
static int pyrna_property_array_init(PyObject *self, PyObject *args, PyObject * /*kwds*/);

PyTypeObject pyrna_prop_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_prop",
    /*tp_basicsize*/ sizeof(BPy_PropertyRNA),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ pyrna_property_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)pyrna_prop_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ (hashfunc)pyrna_prop_hash,
    /*tp_call*/ nullptr,
    /*tp_str*/ (reprfunc)pyrna_prop_str,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ (richcmpfunc)pyrna_prop_richcmp,
#ifdef USE_WEAKREFS
    /*tp_weaklistoffset*/ offsetof(BPy_PropertyRNA, in_weakreflist),
#else
    /*tp_weaklistoffset*/ 0,
#endif
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pyrna_prop_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pyrna_prop_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ pyrna_property_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pyrna_property_new,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

PyTypeObject pyrna_prop_array_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_prop_array",
    /*tp_basicsize*/ sizeof(BPy_PropertyArrayRNA),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr, /* Inherited from #pyrna_prop_Type. */
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)pyrna_prop_array_repr,
    /*tp_as_number*/ &pyrna_prop_array_as_number,
    /*tp_as_sequence*/ &pyrna_prop_array_as_sequence,
    /*tp_as_mapping*/ &pyrna_prop_array_as_mapping,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ (getattrofunc)pyrna_prop_array_getattro,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
#ifdef USE_WEAKREFS
    /*tp_weaklistoffset*/ offsetof(BPy_PropertyArrayRNA, in_weakreflist),
#else
    /*tp_weaklistoffset*/ 0,
#endif
    /*tp_iter*/ (getiterfunc)pyrna_prop_array_iter,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pyrna_prop_array_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr /* Sub-classed: #pyrna_prop_getseters. */,
    /*tp_base*/ &pyrna_prop_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ pyrna_property_array_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr, /* Inherited from #pyrna_prop_Type. */
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

PyTypeObject pyrna_prop_collection_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_prop_collection",
    /*tp_basicsize*/ sizeof(BPy_PropertyRNA),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr, /* Inherited from #pyrna_prop_Type. */
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr, /* Sub-classed, no need to define. */
    /*tp_as_number*/ &pyrna_prop_collection_as_number,
    /*tp_as_sequence*/ &pyrna_prop_collection_as_sequence,
    /*tp_as_mapping*/ &pyrna_prop_collection_as_mapping,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ (getattrofunc)pyrna_prop_collection_getattro,
    /*tp_setattro*/ (setattrofunc)pyrna_prop_collection_setattro,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
#ifdef USE_WEAKREFS
    /*tp_weaklistoffset*/ offsetof(BPy_PropertyRNA, in_weakreflist),
#else
    /*tp_weaklistoffset*/ 0,
#endif
    /*tp_iter*/ (getiterfunc)pyrna_prop_collection_iter,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pyrna_prop_collection_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr /*Sub-classed: see #pyrna_prop_getseters. */,
    /*tp_base*/ &pyrna_prop_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr, /* Inherited from #pyrna_prop_Type. */
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr, /* Inherited from #pyrna_prop_Type. */
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

/* only for add/remove/move methods */
static PyTypeObject pyrna_prop_collection_idprop_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_prop_collection_idprop",
    /*tp_basicsize*/ sizeof(BPy_PropertyRNA),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr, /* Inherited from #pyrna_prop_Type. */
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_compare*/ nullptr, /* DEPRECATED. */
    /*tp_repr*/ nullptr,    /* Sub-classed, no need to define. */
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
#ifdef USE_WEAKREFS
    /*tp_weaklistoffset*/ offsetof(BPy_PropertyRNA, in_weakreflist),
#else
    /*tp_weaklistoffset*/ 0,
#endif
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ pyrna_prop_collection_idprop_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr /* Sub-classed: #pyrna_prop_getseters. */,
    /*tp_base*/ &pyrna_prop_collection_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr, /* Inherited from #pyrna_prop_Type. */
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr, /* Inherited from #pyrna_prop_Type. */
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

static PyObject *pyrna_property_new(PyTypeObject *type, PyObject *args, PyObject * /*kwds*/)
{
  if (PyTuple_GET_SIZE(args) != 1) {
    PyErr_Format(PyExc_TypeError, "bpy_prop.__new__(property): expected a single argument");
    return nullptr;
  }

  PyObject *arg_1 = PyTuple_GET_ITEM(args, 0);

  /* Ignore the special 'PyCapsule' argument used only by internal bpy_rna code. */
  if (!PyCapsule_CheckExact(arg_1)) {
    BPy_PropertyRNA *base = reinterpret_cast<BPy_PropertyRNA *>(arg_1);
    /* Error, invalid type given. */
    if (!PyType_IsSubtype(Py_TYPE(base), &pyrna_prop_Type)) {
      PyErr_Format(
          PyExc_TypeError,
          "bpy_prop.__new__(property): property type '%.200s' is not a subtype of bpy_prop",
          Py_TYPE(base)->tp_name);
      return nullptr;
    }
    /* Same type, simply use the given data. */
    if (Py_TYPE(base) == type) {
      BLI_assert(base->ptr.has_value());
      Py_INCREF(base);
      return reinterpret_cast<PyObject *>(base);
    }
    /* NOTE: Further copy of the 'base' content into the new object will happen in
     * #pyrna_property_init. */
  }

  /* Only allocate the #PyObject data, do not construct/initialize anything else. */
  PyObject *self = type->tp_alloc(type, 0);
  BPy_PropertyRNA *self_property = reinterpret_cast<BPy_PropertyRNA *>(self);
  if (self) {
    self_property->ptr = std::nullopt;
  }
  /* Pass on exception & nullptr if tp_alloc fails. */
  return self;
}

static int pyrna_property_init(PyObject *self, PyObject *args, PyObject * /*kwds*/)
{
  BPy_PropertyRNA *self_property = reinterpret_cast<BPy_PropertyRNA *>(self);

  size_t args_num = PyTuple_GET_SIZE(args);
  if (args_num != 1) {
    PyErr_Format(PyExc_TypeError, "bpy_prop.__init__(self, property): expected a single argument");
    return -1;
  }

  PyObject *arg_1 = PyTuple_GET_ITEM(args, 0);
  const PointerRNA *ptr = nullptr;
  PropertyRNA *prop = nullptr;
  if (PyCapsule_CheckExact(arg_1)) {
    /* `bpy_rna` internal code will call object creation with a PyCapsule argument wrapping up a
     * PointerRNA and PropertyRNA pointers. */
    BPy_PropertyPointerRNA_Reference *pypropptr_rna =
        static_cast<BPy_PropertyPointerRNA_Reference *>(
            PyCapsule_GetPointer(arg_1, BPy_PropertyPointerRNA_capsule_identifier));
    if (pypropptr_rna) {
      ptr = pypropptr_rna->ptr;
      prop = pypropptr_rna->prop;
    }
  }
  else {
    BPy_PropertyRNA *base_property = reinterpret_cast<BPy_PropertyRNA *>(arg_1);

    /* Error, invalid type given. */
    if (!PyType_IsSubtype(Py_TYPE(base_property), &pyrna_prop_Type)) {
      PyErr_Format(
          PyExc_TypeError,
          "bpy_prop.__init__(self, property): property type '%.200s' is not a subtype of bpy_prop",
          Py_TYPE(base_property)->tp_name);
      return -1;
    }
    /* Same object as the 'base' argument (see #pyrna_property_new), nothing to do . */
    if (base_property == self_property) {
      BLI_assert(self_property->ptr.has_value());
      return 0;
    }
    ptr = &base_property->ptr.value();
    prop = base_property->prop;
  }

  self_property->ptr = *ptr;
  self_property->prop = prop;

  return 0;
}

static void pyrna_property_dealloc(PyObject *self)
{
  /* Save the current exception, if any. */
  PyObject *error_type, *error_value, *error_traceback;
  PyErr_Fetch(&error_type, &error_value, &error_traceback);

  BPy_PropertyRNA *self_property = reinterpret_cast<BPy_PropertyRNA *>(self);
#ifdef USE_WEAKREFS
  if (self_property->in_weakreflist != nullptr) {
    PyObject_ClearWeakRefs(self);
  }
#endif

  self_property->ptr.~optional();

  Py_TYPE(self)->tp_free(self);

  /* Restore the saved exception. */
  PyErr_Restore(error_type, error_value, error_traceback);
}

static int pyrna_property_array_init(PyObject *self, PyObject *args, PyObject * /*kwds*/)
{
  BPy_PropertyArrayRNA *self_property = reinterpret_cast<BPy_PropertyArrayRNA *>(self);

  size_t args_num = PyTuple_GET_SIZE(args);
  if (args_num != 1) {
    PyErr_Format(PyExc_TypeError,
                 "bpy_prop_array.__init__(self, property): expected a single argument");
    return -1;
  }

  PyObject *arg_1 = PyTuple_GET_ITEM(args, 0);
  const PointerRNA *ptr = nullptr;
  PropertyRNA *prop = nullptr;
  if (PyCapsule_CheckExact(arg_1)) {
    /* `bpy_rna` internal code will call object creation with a PyCapsule argument wrapping up a
     * PointerRNA and PropertyRNA pointers. */
    BPy_PropertyPointerRNA_Reference *pypropptr_rna =
        static_cast<BPy_PropertyPointerRNA_Reference *>(
            PyCapsule_GetPointer(arg_1, BPy_PropertyPointerRNA_capsule_identifier));
    if (pypropptr_rna) {
      ptr = pypropptr_rna->ptr;
      prop = pypropptr_rna->prop;
    }
  }
  else {
    BPy_PropertyArrayRNA *base_property = reinterpret_cast<BPy_PropertyArrayRNA *>(arg_1);

    /* Error, invalid type given. */
    if (!PyType_IsSubtype(Py_TYPE(base_property), &pyrna_prop_array_Type)) {
      PyErr_Format(PyExc_TypeError,
                   "bpy_prop_array.__init__(self, property): property type '%.200s' is not a "
                   "subtype of bpy_prop_array",
                   Py_TYPE(base_property)->tp_name);
      return -1;
    }
    /* Same object as the 'base' argument (see #pyrna_property_new), nothing to do . */
    if (base_property == self_property) {
      BLI_assert(self_property->ptr.has_value());
      return 0;
    }
    ptr = &base_property->ptr.value();
    prop = base_property->prop;
  }

  self_property->prop = prop;
  self_property->arraydim = 0;
  self_property->arrayoffset = 0;
  self_property->ptr = *ptr;

  return 0;
}

#ifdef USE_PYRNA_ITER
/* --- collection iterator: start --- */
/* Wrap RNA collection iterator functions. */
/*
 * RNA_property_collection_begin(...)
 * RNA_property_collection_next(...)
 * RNA_property_collection_end(...)
 */

static PyObject *pyrna_prop_collection_iter_new(PyTypeObject *type,
                                                PyObject * /*args*/,
                                                PyObject * /*kwds*/);
static int pyrna_prop_collection_iter_init(PyObject *self,
                                           PyObject * /*args*/,
                                           PyObject * /*kwds*/);
static void pyrna_prop_collection_iter_dealloc(PyObject *self);
static PyObject *pyrna_prop_collection_iter_next(PyObject *self);

static PyTypeObject pyrna_prop_collection_iter_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_prop_collection_iter",
    /*tp_basicsize*/ sizeof(BPy_PropertyCollectionIterRNA),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ pyrna_prop_collection_iter_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr, /* No need to define, sub-classed. */
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ PyObject_GenericGetAttr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
#  ifdef USE_WEAKREFS
    /*tp_weaklistoffset*/ offsetof(BPy_PropertyCollectionIterRNA, in_weakreflist),
#  else
    /*tp_weaklistoffset*/ 0,
#  endif
    /*tp_iter*/ PyObject_SelfIter,
    /*tp_iternext*/ pyrna_prop_collection_iter_next,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ pyrna_prop_collection_iter_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ pyrna_prop_collection_iter_new,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

static PyObject *pyrna_prop_collection_iter_new(PyTypeObject *type,
                                                PyObject *args,
                                                PyObject * /*kwds*/)
{
  if (PyTuple_GET_SIZE(args) != 1) {
    PyErr_Format(PyExc_TypeError,
                 "bpy_prop_collection_iter.__new__(arg): expected a single argument");
    return nullptr;
  }

  PyObject *arg_1 = PyTuple_GET_ITEM(args, 0);
  /* Only accept the special 'PyCapsule' argument used by internal bpy_rna code. */
  if (!PyCapsule_CheckExact(arg_1)) {
    PyErr_Format(PyExc_TypeError,
                 "bpy_prop_collection_iter.__new__(arg): arg type '%.200s' is not a PyCapsule",
                 Py_TYPE(arg_1)->tp_name);
    return nullptr;
  }

  /* Only allocate the #PyObject data, do not construct/initialize anything else. */
  PyObject *self = type->tp_alloc(type, 0);
  BPy_PropertyCollectionIterRNA *self_prop_iter =
      reinterpret_cast<BPy_PropertyCollectionIterRNA *>(self);
  if (self_prop_iter) {
    self_prop_iter->iter = std::nullopt;
  }

  return self;
}

static int pyrna_prop_collection_iter_init(PyObject *self, PyObject *args, PyObject * /*kwds*/)
{
  BPy_PropertyCollectionIterRNA *self_prop_iter =
      reinterpret_cast<BPy_PropertyCollectionIterRNA *>(self);

  size_t args_num = PyTuple_GET_SIZE(args);
  if (args_num != 1) {
    PyErr_Format(
        PyExc_TypeError,
        "bpy_prop_collection_iter.__init__(self, arg): expected at most a single argument");
    return -1;
  }

  PyObject *arg_1 = PyTuple_GET_ITEM(args, 0);
  const PointerRNA *ptr = nullptr;
  PropertyRNA *prop = nullptr;
  if (PyCapsule_CheckExact(arg_1)) {
    /* `bpy_rna` internal code will call object creation with a PyCapsule argument wrapping up a
     * PointerRNA and PropertyRNA pointers. */
    BPy_PropertyPointerRNA_Reference *pypropptr_rna =
        static_cast<BPy_PropertyPointerRNA_Reference *>(
            PyCapsule_GetPointer(arg_1, BPy_PropertyPointerRNA_capsule_identifier));
    if (pypropptr_rna) {
      ptr = pypropptr_rna->ptr;
      prop = pypropptr_rna->prop;
    }
  }
  else {
    PyErr_Format(
        PyExc_TypeError,
        "bpy_prop_collection_iter.__init__(self, arg): arg type '%.200s' is not a PyCapsule",
        Py_TYPE(arg_1)->tp_name);
  }

  if (self_prop_iter->iter.has_value()) {
    RNA_property_collection_end(&self_prop_iter->iter.value());
    self_prop_iter->iter.reset();
  }
  self_prop_iter->iter = CollectionPropertyIterator();
  RNA_property_collection_begin(
      const_cast<PointerRNA *>(ptr), prop, &self_prop_iter->iter.value());

  return 0;
}

static void pyrna_prop_collection_iter_dealloc(PyObject *self)
{
  /* Save the current exception, if any. */
  PyObject *error_type, *error_value, *error_traceback;
  PyErr_Fetch(&error_type, &error_value, &error_traceback);

  BPy_PropertyCollectionIterRNA *self_property = reinterpret_cast<BPy_PropertyCollectionIterRNA *>(
      self);
#  ifdef USE_WEAKREFS
  if (self_property->in_weakreflist != nullptr) {
    PyObject_ClearWeakRefs(self);
  }
#  endif

  if (self_property->iter.has_value()) {
    RNA_property_collection_end(&self_property->iter.value());
    self_property->iter.reset();
  }

  Py_TYPE(self)->tp_free(self);

  /* Restore the saved exception. */
  PyErr_Restore(error_type, error_value, error_traceback);
}

static PyObject *pyrna_prop_collection_iter_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop)
{
  /* Pass the PointerRNA and PropertyRNA to `__new__`/`__init__` functions as an opaque PyCapsule
   * object. */
  BPy_PropertyPointerRNA_Reference prop_ptr{ptr, prop};
  PyObject *pypropptr_rna = PyCapsule_New(
      &prop_ptr, BPy_PropertyPointerRNA_capsule_identifier, nullptr);

  PyObject *self = PyObject_CallOneArg(
      reinterpret_cast<PyObject *>(&pyrna_prop_collection_iter_Type), pypropptr_rna);
  BPy_PropertyCollectionIterRNA *self_property = reinterpret_cast<BPy_PropertyCollectionIterRNA *>(
      self);

  BLI_assert(self_property->iter.has_value());
  Py_DECREF(pypropptr_rna);

#  ifdef USE_WEAKREFS
  self_property->in_weakreflist = nullptr;
#  else
  UNUSED_VARS_NDEBUG(self_property);
#  endif

  return self;
}

static PyObject *pyrna_prop_collection_iter(PyObject *self)
{
  BPy_PropertyRNA *self_property = reinterpret_cast<BPy_PropertyRNA *>(self);
  return pyrna_prop_collection_iter_CreatePyObject(&self_property->ptr.value(),
                                                   self_property->prop);
}

static PyObject *pyrna_prop_collection_iter_next(PyObject *self)
{
  BPy_PropertyCollectionIterRNA *self_property = reinterpret_cast<BPy_PropertyCollectionIterRNA *>(
      self);
  if (self_property->iter->valid == false) {
    PyErr_SetNone(PyExc_StopIteration);
    return nullptr;
  }

  PyObject *iter_data = pyrna_struct_CreatePyObject(&self_property->iter->ptr);

#  ifdef USE_PYRNA_STRUCT_REFERENCE
  if (iter_data) { /* Unlikely, but may fail. */
    BPy_StructRNA *iter_data_struct = reinterpret_cast<BPy_StructRNA *>(iter_data);
    if (iter_data != Py_None) {
      /* hold a reference to the iterator since it may have
       * allocated memory `pyrna` needs. eg: introspecting dynamic enum's. */
      /* TODO: we could have an API call to know if this is
       * needed since most collections don't */
      pyrna_struct_reference_set(iter_data_struct, self);
    }
  }
#  endif /* !USE_PYRNA_STRUCT_REFERENCE */

  RNA_property_collection_next(&self_property->iter.value());

  return iter_data;
}

/* --- collection iterator: end --- */
#endif /* !USE_PYRNA_ITER */

/** \} */

/*-----------------------BPy_PropertyRNA method def------------------------------*/

/* -------------------------------------------------------------------- */
/** \name BPY RNA Function
 * \{ */

static void pyrna_func_dealloc(PyObject *self);

PyTypeObject pyrna_func_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_func",
    /*tp_basicsize*/ sizeof(BPy_FunctionRNA),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ pyrna_func_dealloc,
    /*tp_vectorcall_offset*/ offsetof(BPy_FunctionRNA, vectorcall),
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)pyrna_func_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ PyVectorcall_Call,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_VECTORCALL,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
#ifdef USE_WEAKREFS
    /*tp_weaklistoffset*/ offsetof(BPy_PropertyRNA, in_weakreflist),
#else
    /*tp_weaklistoffset*/ 0,
#endif
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pyrna_func_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

static PyObject *pyrna_func_CreatePyObject(const PointerRNA *ptr, FunctionRNA *func)
{
  PyObject *self = pyrna_func_Type.tp_alloc(&pyrna_func_Type, 0);
  BPy_FunctionRNA *pyfunc = reinterpret_cast<BPy_FunctionRNA *>(self);
  if (pyfunc) {
    pyfunc->func = func;
    pyfunc->ptr = *ptr;
    pyfunc->vectorcall = pyrna_func_vectorcall;
  }

  if (!pyfunc) {
    PyErr_SetString(PyExc_MemoryError, "couldn't create bpy_func object");
    return nullptr;
  }

  BLI_assert(pyfunc->ptr.has_value());

  return reinterpret_cast<PyObject *>(pyfunc);
}

static void pyrna_func_dealloc(PyObject *self)
{
  /* Save the current exception, if any. */
  PyObject *error_type, *error_value, *error_traceback;
  PyErr_Fetch(&error_type, &error_value, &error_traceback);

  BPy_FunctionRNA *self_func = reinterpret_cast<BPy_FunctionRNA *>(self);

#ifdef USE_WEAKREFS
  if (self_func->in_weakreflist != nullptr) {
    PyObject_ClearWeakRefs(self);
  }
#endif

  self_func->ptr.~optional();

  Py_TYPE(self)->tp_free(self);

  /* Restore the saved exception. */
  PyErr_Restore(error_type, error_value, error_traceback);
}

/** \} */

static void pyrna_subtype_set_rna(PyObject *newclass, StructRNA *srna)
{
  Py_INCREF(newclass);

  if (PyObject *oldclass = static_cast<PyObject *>(RNA_struct_py_type_get(srna))) {
    PyC_ObSpit("RNA WAS SET - ", oldclass);
    Py_DECREF(oldclass);
  }
  RNA_struct_py_type_set(srna, (void *)newclass); /* Store for later use */

  /* Not 100% needed, but useful,
   * having an instance within a type looks wrong, but this instance _is_ an RNA type. */

  /* Python deals with the circular reference. */
  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, &RNA_Struct, srna);

  /* NOTE: using `pyrna_struct_CreatePyObject(&ptr)` is close to what is needed,
   * however the it isn't correct because the result of:
   * `type(bpy.types.Object.bl_rna) == bpy.types.Object`.
   * In this case the type of `bl_rna` should be `bpy.types.Struct`.
   * This is passed in explicitly, while #pyrna_struct_CreatePyObject could
   * take this as an argument it's such a corner case that using a lower level
   * function that takes the type is preferable. */
  {
    BLI_assert(RNA_struct_instance(&ptr) == nullptr);
    PyTypeObject *tp = (PyTypeObject *)pyrna_srna_Subtype(&RNA_Struct);
    PyObject *item = pyrna_struct_CreatePyObject_from_type(&ptr, tp, nullptr);
    Py_DECREF(tp); /* `srna` owns, can't hold a reference. */

    /* NOTE: must set the class not the `__dict__`
     * else the internal slots are not updated correctly. */
    PyObject_SetAttr(newclass, bpy_intern_str_bl_rna, item);
    Py_DECREF(item);
  }

  /* Add `staticmethod` and `classmethod` functions. */
  {
    const PointerRNA func_ptr = {nullptr, srna, nullptr};
    const ListBase *lb;

    lb = RNA_struct_type_functions(srna);
    LISTBASE_FOREACH (Link *, link, lb) {
      FunctionRNA *func = (FunctionRNA *)link;
      const int flag = RNA_function_flag(func);
      if ((flag & FUNC_NO_SELF) &&         /* Is `staticmethod` or `classmethod`. */
          (flag & FUNC_REGISTER) == false) /* Is not for registration. */
      {
        /* We may want to set the type of this later. */
        PyObject *func_py = pyrna_func_CreatePyObject(&func_ptr, func);
        PyObject_SetAttrString(newclass, RNA_function_identifier(func), func_py);
        Py_DECREF(func_py);
      }
    }
  }

  /* Done with RNA instance. */
}

/**
 * \return borrowed reference.
 */
static PyObject *pyrna_srna_PyBase(StructRNA *srna)
{
  /* Assume RNA_struct_py_type_get(srna) was already checked. */
  StructRNA *base;

  PyObject *py_base = nullptr;

  /* Get the base type. */
  base = RNA_struct_base(srna);

  if (base && base != srna) {
    // printf("debug subtype %s %p\n", RNA_struct_identifier(srna), srna);
    py_base = pyrna_srna_Subtype(base);  //, bpy_types_dict);
    Py_DECREF(py_base);                  /* `srna` owns, this is only to pass as an argument. */
  }

  if (py_base == nullptr) {
    py_base = (PyObject *)&pyrna_struct_Type;
  }

  return py_base;
}

/**
 * Check if we have a native Python subclass, use it when it exists
 * return a borrowed reference.
 */
static PyObject *bpy_types_dict = nullptr;
void BPY_rna_types_dict_set(PyObject *dict)
{
  bpy_types_dict = dict; /* Borrow. */
}

/**
 * Return the #PyTypeObject or null,
 *
 * \return borrowed reference.
 */
static PyObject *pyrna_srna_ExternalType(StructRNA *srna)
{
  BLI_assert(bpy_types_dict);

  const char *idname = RNA_struct_identifier(srna);
  PyObject *newclass = PyDict_GetItemString(bpy_types_dict, idname);

  /* Sanity check, could skip this unless in debug mode. */
  if (newclass) {
    PyObject *base_compare = pyrna_srna_PyBase(srna);
    /* Can't do this because it gets super-classes values! */
    // PyObject *slots = PyObject_GetAttrString(newclass, "__slots__");
    /* Can do this, but faster not to. */
    // PyObject *bases = PyObject_GetAttrString(newclass, "__bases__");
    PyObject *tp_bases = ((PyTypeObject *)newclass)->tp_bases;
    PyObject *tp_slots = PyDict_GetItem(((PyTypeObject *)newclass)->tp_dict,
                                        bpy_intern_str___slots__);

    if (tp_slots == nullptr) {
      CLOG_ERROR(
          BPY_LOG_RNA, "expected class '%s' to have __slots__ defined, see _bpy_types.py", idname);
      newclass = nullptr;
    }
    else if (PyTuple_GET_SIZE(tp_bases)) {
      PyObject *base = PyTuple_GET_ITEM(tp_bases, 0);

      if (base_compare != base) {
        char pyob_info[256];
        PyC_ObSpitStr(pyob_info, sizeof(pyob_info), base_compare);
        CLOG_ERROR(BPY_LOG_RNA,
                   "incorrect subclassing of SRNA '%s', expected '%s', see _bpy_types.py",
                   idname,
                   pyob_info);
        newclass = nullptr;
      }
      else {
        CLOG_TRACE(BPY_LOG_RNA, "SRNA sub-classed: '%s'", idname);
      }
    }
  }

  return newclass;
}

/**
 * Return the #PyTypeObject or null with an exception set.
 *
 * \return new reference.
 */
static PyObject *pyrna_srna_Subtype(StructRNA *srna)
{
  PyObject *newclass = nullptr;

  /* Stupid/simple case. */
  if (srna == nullptr) {
    newclass = nullptr; /* Nothing to do. */
  } /* The class may have already been declared & allocated. */
  else if ((newclass = static_cast<PyObject *>(RNA_struct_py_type_get(srna)))) {
    /* Add a reference for the return value. */
    Py_INCREF(newclass);
  } /* Check if `_bpy_types.py` module has the class defined in it. */
  else if ((newclass = pyrna_srna_ExternalType(srna))) {
    pyrna_subtype_set_rna(newclass, srna);
    /* Add a reference for the return value. */
    Py_INCREF(newclass);
  } /* Create a new class instance with the C API
     * mainly for the purposing of matching the C/RNA type hierarchy. */
  else {
    /* subclass equivalents
     * - class myClass(myBase):
     *     some = 'value' # or ...
     * - myClass = type(
     *       name='myClass',
     *       bases=(myBase,), dict={'__module__': 'bpy.types', '__slots__': ()}
     *   )
     */

    /* Assume `RNA_struct_py_type_get(srna)` was already checked. */
    PyObject *py_base = pyrna_srna_PyBase(srna);
    PyObject *metaclass;
    const char *idname = RNA_struct_identifier(srna);

/* Remove `__doc__` for now because we don't need it to generate docs. */
#if 0
    const char *descr = RNA_struct_ui_description(srna);
    if (!descr) {
      descr = "(no docs)";
    }
#endif

    if (RNA_struct_system_idprops_check(srna) &&
        !PyObject_IsSubclass(py_base, (PyObject *)&pyrna_struct_meta_idprop_Type))
    {
      metaclass = (PyObject *)&pyrna_struct_meta_idprop_Type;
    }
    else {
      metaclass = (PyObject *)&PyType_Type;
    }

/* Always use O not N when calling, N causes refcount errors. */
#if 0
    newclass = PyObject_CallFunction(
        metaclass, "s(O) {sss()}", idname, py_base, "__module__", "bpy.types", "__slots__");
#else
    {
      /* Longhand of the call above. */
      PyObject *args, *item, *value;
      int ok;

      args = PyTuple_New(3);

      /* arg[0] (name=...) */
      PyTuple_SET_ITEM(args, 0, PyUnicode_FromString(idname));

      /* arg[1] (bases=...) */
      PyTuple_SET_ITEM(args, 1, item = PyTuple_New(1));
      PyTuple_SET_ITEM(item, 0, Py_NewRef(py_base));

      /* arg[2] (dict=...) */
      PyTuple_SET_ITEM(args, 2, item = PyDict_New());
      ok = PyDict_SetItem(item, bpy_intern_str___module__, bpy_intern_str_bpy_types);
      BLI_assert(ok != -1);
      ok = PyDict_SetItem(item, bpy_intern_str___slots__, value = PyTuple_New(0));
      Py_DECREF(value);
      BLI_assert(ok != -1);

      newclass = PyObject_CallObject(metaclass, args);
      Py_DECREF(args);

      (void)ok;
    }
#endif

    /* Newclass will now have 2 ref's, ???,
     * probably 1 is internal since #Py_DECREF here segfaults. */

    // PyC_ObSpit("new class ref", newclass);

    if (newclass) {
      /* srna owns one, and the other is owned by the caller. */
      pyrna_subtype_set_rna(newclass, srna);
    }
    else {
      /* This should not happen. */
      CLOG_ERROR(BPY_LOG_RNA, "failed to register '%s'", idname);
      PyErr_Print();
    }
  }

  return newclass;
}

/**
 * Use for sub-typing so we know which SRNA is used for a #PointerRNA.
 */
static StructRNA *srna_from_ptr(PointerRNA *ptr)
{
  if (ptr->type == &RNA_Struct) {
    return static_cast<StructRNA *>(ptr->data);
  }

  return ptr->type;
}

/**
 * \return new reference.
 */
static PyObject *pyrna_struct_Subtype(PointerRNA *ptr)
{
  return pyrna_srna_Subtype(srna_from_ptr(ptr));
}

/*-----------------------CreatePyObject---------------------------------*/

/**
 * A lower level version of #pyrna_struct_CreatePyObject,
 * use this when type (`tp`) needs to be set to a non-standard value.
 *
 * \return new reference.
 */
static PyObject *pyrna_struct_CreatePyObject_from_type(const PointerRNA *ptr,
                                                       PyTypeObject *tp,
                                                       void **instance)
{
  /* Pass the PointerRNA to `__new__`/`__init__` functions as an opaque PyCapsule object. */
  PyObject *pyptr_rna = PyCapsule_New(
      const_cast<PointerRNA *>(ptr), BPy_capsule_PointerRNA_identifier, nullptr);

  BPy_StructRNA *pyrna = nullptr;
  if (tp) {
    pyrna = reinterpret_cast<BPy_StructRNA *>(
        PyObject_CallOneArg(reinterpret_cast<PyObject *>(tp), pyptr_rna));
  }
  else {
    CLOG_WARN(BPY_LOG_RNA, "could not make type '%s'", RNA_struct_identifier(ptr->type));

    pyrna = reinterpret_cast<BPy_StructRNA *>(
        PyObject_CallOneArg(reinterpret_cast<PyObject *>(&pyrna_struct_Type), pyptr_rna));
  }

#ifdef USE_PYRNA_STRUCT_REFERENCE
  /* #PyType_GenericAlloc will have set tracking.
   * We only want tracking when `StructRNA.reference` has been set. */
  if (pyrna != nullptr) {
    PyObject_GC_UnTrack(pyrna);
  }
#endif

#ifdef USE_WEAKREFS
  if (pyrna != nullptr) {
    pyrna->in_weakreflist = nullptr;
  }
#endif

  if (pyrna == nullptr) {
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_MemoryError, "couldn't create bpy_struct object");
    }
    return nullptr;
  }

  BLI_assert(pyrna->ptr.has_value());
  Py_DECREF(pyptr_rna);

  /* Blender's instance owns a reference (to avoid Python freeing it). */
  if (instance) {
    *instance = pyrna;
    Py_INCREF(pyrna);
  }

#ifdef PYRNA_FREE_SUPPORT
  pyrna->freeptr = false;
#endif

#ifdef USE_PYRNA_STRUCT_REFERENCE
  pyrna->reference = nullptr;
#endif

  // PyC_ObSpit("NewStructRNA: ", (PyObject *)pyrna);

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
  if (ptr->owner_id) {
    id_weakref_pool_add(ptr->owner_id, (BPy_DummyPointerRNA *)pyrna);
  }
#endif

  return reinterpret_cast<PyObject *>(pyrna);
}

PyObject *pyrna_struct_CreatePyObject(PointerRNA *ptr)
{
  /* NOTE: don't rely on this to return None since nullptr data with a valid type can often crash.
   */
  if (ptr->data == nullptr && ptr->type == nullptr) { /* Operator RNA has nullptr data. */
    Py_RETURN_NONE;
  }

  BPy_StructRNA *pyrna = nullptr;

  /* NOTE(@ideasman42): New in 2.8x, since not many types support instancing
   * we may want to use a flag to avoid looping over all classes. */
  void **instance = ptr->data ? RNA_struct_instance(ptr) : nullptr;
  if (instance && *instance) {
    pyrna = static_cast<BPy_StructRNA *>(*instance);

    /* Refine may have changed types after the first instance was created. */
    if (ptr->type == pyrna->ptr->type) {
      Py_INCREF(pyrna);
      return reinterpret_cast<PyObject *>(pyrna);
    }

    /* Existing users will need to use 'type_recast' method. */
    Py_DECREF(pyrna);
    *instance = nullptr;
    /* Continue as if no instance was available. */
  }

  PyTypeObject *tp = reinterpret_cast<PyTypeObject *>(pyrna_struct_Subtype(ptr));
  pyrna = reinterpret_cast<BPy_StructRNA *>(
      pyrna_struct_CreatePyObject_from_type(ptr, tp, instance));
  Py_XDECREF(tp); /* `srna` owns, can't hold a reference. */

  return reinterpret_cast<PyObject *>(pyrna);
}

PyObject *pyrna_struct_CreatePyObject_with_primitive_support(PointerRNA *ptr)
{
  if (ptr->type == &RNA_PrimitiveString) {
    const PrimitiveStringRNA *data = static_cast<const PrimitiveStringRNA *>(ptr->data);
    return PyC_UnicodeFromBytes(data->value);
  }
  if (ptr->type == &RNA_PrimitiveInt) {
    const PrimitiveIntRNA *data = static_cast<const PrimitiveIntRNA *>(ptr->data);
    return PyLong_FromLong(data->value);
  }
  if (ptr->type == &RNA_PrimitiveFloat) {
    const PrimitiveFloatRNA *data = static_cast<const PrimitiveFloatRNA *>(ptr->data);
    return PyFloat_FromDouble(data->value);
  }
  if (ptr->type == &RNA_PrimitiveBoolean) {
    const PrimitiveBooleanRNA *data = static_cast<const PrimitiveBooleanRNA *>(ptr->data);
    return PyBool_FromLong(data->value);
  }
  return pyrna_struct_CreatePyObject(ptr);
}

PyObject *pyrna_prop_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop)
{
  PyTypeObject *type;
  if (RNA_property_array_check(prop)) {
    type = &pyrna_prop_array_Type;
  }
  else if (RNA_property_type(prop) == PROP_COLLECTION) {
    if (RNA_property_flag(prop) & PROP_IDPROPERTY) {
      type = &pyrna_prop_collection_idprop_Type;
    }
    else {
      type = &pyrna_prop_collection_Type;
    }
  }
  else {
    type = &pyrna_prop_Type;
  }

  /* Pass the PointerRNA and PropertyRNA to `__new__`/`__init__` functions as an opaque PyCapsule
   * object. */
  BPy_PropertyPointerRNA_Reference prop_ptr{ptr, prop};
  PyObject *pypropptr_rna = PyCapsule_New(
      &prop_ptr, BPy_PropertyPointerRNA_capsule_identifier, nullptr);

  BPy_PropertyRNA *pyrna = reinterpret_cast<BPy_PropertyRNA *>(
      PyObject_CallOneArg(reinterpret_cast<PyObject *>(type), pypropptr_rna));

  if (pyrna == nullptr) {
    PyErr_SetString(PyExc_MemoryError, "couldn't create BPy_rna object");
    return nullptr;
  }

  BLI_assert(pyrna->ptr.has_value());
  Py_DECREF(pypropptr_rna);

#ifdef USE_WEAKREFS
  pyrna->in_weakreflist = nullptr;
#endif

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
  if (ptr->owner_id) {
    id_weakref_pool_add(ptr->owner_id, (BPy_DummyPointerRNA *)pyrna);
  }
#endif

  return (PyObject *)pyrna;
}

PyObject *pyrna_id_CreatePyObject(ID *id)
{
  if (id) {
    PointerRNA ptr = RNA_id_pointer_create(id);
    return pyrna_struct_CreatePyObject(&ptr);
  }

  Py_RETURN_NONE;
}

bool pyrna_id_FromPyObject(PyObject *obj, ID **id)
{
  if (pyrna_id_CheckPyObject(obj)) {
    *id = ((BPy_StructRNA *)obj)->ptr->owner_id;
    return true;
  }

  *id = nullptr;
  return false;
}

bool pyrna_id_CheckPyObject(PyObject *obj)
{
  return BPy_StructRNA_Check(obj) && RNA_struct_is_ID(((BPy_StructRNA *)obj)->ptr->type);
}

void BPY_rna_init()
{
#ifdef USE_MATHUTILS /* Register mathutils callbacks, ok to run more than once. */
  mathutils_rna_array_cb_index = Mathutils_RegisterCallback(&mathutils_rna_array_cb);
  mathutils_rna_matrix_cb_index = Mathutils_RegisterCallback(&mathutils_rna_matrix_cb);
#endif

/* For some reason MSVC complains of these. */
#if defined(_MSC_VER)
  pyrna_struct_meta_idprop_Type.tp_base = &PyType_Type;
#endif

  /* metaclass */
  if (PyType_Ready(&pyrna_struct_meta_idprop_Type) < 0) {
    return;
  }

  if (PyType_Ready(&pyrna_struct_Type) < 0) {
    return;
  }

  if (PyType_Ready(&pyrna_prop_Type) < 0) {
    return;
  }

  if (PyType_Ready(&pyrna_prop_array_Type) < 0) {
    return;
  }

  if (PyType_Ready(&pyrna_prop_collection_Type) < 0) {
    return;
  }

  if (PyType_Ready(&pyrna_prop_collection_idprop_Type) < 0) {
    return;
  }

  if (PyType_Ready(&pyrna_func_Type) < 0) {
    return;
  }

#ifdef USE_PYRNA_ITER
  if (PyType_Ready(&pyrna_prop_collection_iter_Type) < 0) {
    return;
  }
#endif

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
  BLI_assert(id_weakref_pool == nullptr);
  id_weakref_pool = BLI_ghash_ptr_new("rna_global_pool");
#endif
}

void BPY_rna_exit()
{
#ifdef USE_PYRNA_INVALIDATE_WEAKREF
  /* This can help track down which kinds of data were not released.
   * If they were in fact freed by Blender, printing their names
   * will crash giving a useful error with address sanitizer. The likely cause
   * for this list not being empty is a missing call to: #BKE_libblock_free_data_py. */
  const int id_weakref_pool_len = BLI_ghash_len(id_weakref_pool);
  if (id_weakref_pool_len != 0) {
    printf("Found %d unreleased ID's\n", id_weakref_pool_len);
    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, id_weakref_pool) {
      ID *id = static_cast<ID *>(BLI_ghashIterator_getKey(&gh_iter));
      printf("ID: %s\n", id->name);
    }
  }
  BLI_ghash_free(id_weakref_pool, nullptr, id_weakref_pool_free_value_fn);
  id_weakref_pool = nullptr;
#endif
}

/* 'bpy.data' from Python. */
static PointerRNA *rna_module_ptr = nullptr;
PyObject *BPY_rna_module()
{
  BPy_StructRNA *pyrna;

  /* For now, return the base RNA type rather than a real module. */
  PointerRNA ptr = RNA_main_pointer_create(G_MAIN);
  pyrna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);

  rna_module_ptr = &pyrna->ptr.value();
  return (PyObject *)pyrna;
}

void BPY_update_rna_module()
{
  if (rna_module_ptr) {
#if 0
    *rna_module_ptr = RNA_main_pointer_create(G_MAIN);
#else
    rna_module_ptr->data = G_MAIN; /* Just set data is enough. */
#endif
  }
}

#if 0
/* This is a way we can access doc-strings for RNA types
 * without having the data-types in Blender. */
PyObject *BPY_rna_doc()
{

  /* For now, return the base RNA type rather than a real module. */
  PointerRNA ptr = RNA_blender_rna_pointer_create();

  return pyrna_struct_CreatePyObject(&ptr);
}
#endif

/* -------------------------------------------------------------------- */
/** \name RNA Types Module `bpy.types`
 * \{ */

/**
 * This could be a static variable as we only have one `bpy.types` module,
 * it just keeps the data isolated to store in the module itself.
 *
 * This data doesn't change once initialized.
 */
struct BPy_TypesModule_State {
  /** `RNA_BlenderRNA`. */
  std::optional<PointerRNA> ptr;
  /** `RNA_BlenderRNA.structs`, exposed as `bpy.types` */
  PropertyRNA *prop;
};

static PyObject *bpy_types_module_getattro(PyObject *self, PyObject *pyname)
{
  BPy_TypesModule_State *state = static_cast<BPy_TypesModule_State *>(PyModule_GetState(self));
  BLI_assert(state->ptr.has_value());

  PointerRNA newptr;
  PyObject *ret;
  const char *name = PyUnicode_AsUTF8(pyname);

  if (name == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "bpy.types: __getattr__ must be a string");
    ret = nullptr;
  }
  else if (RNA_property_collection_lookup_string(&state->ptr.value(), state->prop, name, &newptr))
  {
    ret = pyrna_struct_Subtype(&newptr);
    if (ret == nullptr) {
      PyErr_Format(PyExc_RuntimeError,
                   "bpy.types.%.200s subtype could not be generated, this is a bug!",
                   PyUnicode_AsUTF8(pyname));
    }
  }
  else {
#if 0
    PyErr_Format(PyExc_AttributeError,
                 "bpy.types.%.200s RNA_Struct does not exist",
                 PyUnicode_AsUTF8(pyname));
    return nullptr;
#endif
    /* The error raised here will be displayed. */
    ret = PyObject_GenericGetAttr(self, pyname);
  }

  return ret;
}

static PyObject *bpy_types_module_dir(PyObject *self)
{
  BPy_TypesModule_State *state = static_cast<BPy_TypesModule_State *>(PyModule_GetState(self));
  BLI_assert(state->ptr.has_value());

  PyObject *ret = PyList_New(0);

  RNA_PROP_BEGIN (&state->ptr.value(), itemptr, state->prop) {
    StructRNA *srna = static_cast<StructRNA *>(itemptr.data);
    PyList_APPEND(ret, PyUnicode_FromString(RNA_struct_identifier(srna)));
  }
  RNA_PROP_END;

  /* Include the modules `__dict__` for Python only types. */
  PyObject *submodule_dict = PyModule_GetDict(self);
  PyObject *key, *value;
  Py_ssize_t pos = 0;
  while (PyDict_Next(submodule_dict, &pos, &key, &value)) {
    PyList_Append(ret, key);
  }
  return ret;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef bpy_types_module_methods[] = {
    {"__getattr__", (PyCFunction)bpy_types_module_getattro, METH_O, nullptr},
    {"__dir__", (PyCFunction)bpy_types_module_dir, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static void bpy_types_module_free(void *self)
{
  /* Module's `m_free` is quite different from `PyTypeObject` `tp_free`. It does not have to free
   * python-allocated memory (like the module state itself), but only memory managed outside of
   * Python. In that sense, it is way closer to `tp_finalize`. */

  PyObject *submodule = static_cast<PyObject *>(self);
  BPy_TypesModule_State *state = static_cast<BPy_TypesModule_State *>(
      PyModule_GetState(submodule));

  state->ptr.~optional();
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_types_module_doc,
    "Access to internal Blender types.");
static PyModuleDef bpy_types_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bpy.types",
    /*m_doc*/ bpy_types_module_doc,
    /*m_size*/ sizeof(BPy_TypesModule_State),
    /*m_methods*/ bpy_types_module_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ bpy_types_module_free,
};

PyObject *BPY_rna_types()
{
  PyObject *submodule = PyModule_Create(&bpy_types_module_def);
  BPy_TypesModule_State *state = static_cast<BPy_TypesModule_State *>(
      PyModule_GetState(submodule));

  state->ptr = RNA_blender_rna_pointer_create();
  state->prop = RNA_struct_find_property(&state->ptr.value(), "structs");

  /* Internal base types we have no other accessors for. */
  {
    static PyTypeObject *pyrna_types[] = {
        &pyrna_struct_meta_idprop_Type,
        &pyrna_struct_Type,
        &pyrna_prop_Type,
        &pyrna_prop_array_Type,
        &pyrna_prop_collection_Type,
        &pyrna_prop_collection_idprop_Type,
        &pyrna_func_Type,
    };

    PyObject *submodule_dict = PyModule_GetDict(submodule);
    for (int i = 0; i < ARRAY_SIZE(pyrna_types); i += 1) {
      PyDict_SetItemString(submodule_dict, pyrna_types[i]->tp_name, (PyObject *)pyrna_types[i]);
    }
  }

  return submodule;
}

void BPY_rna_types_finalize_external_types(PyObject *submodule)
{
  /* NOTE: Blender is generally functional without running this logic
   * however failure set the classes `bl_rna` (via `pyrna_subtype_set_rna`)
   * means *partially* initialized classes exist.
   * It's simpler to avoid this altogether as it's a corner case Python developers should
   * not have to concern themselves with as it could cause errors with RNA introspection.
   *
   * If the classes are accessed via `bpy.types` they will be initialized correctly
   * however classes can also be accessed via `bpy.types.ID.__subclasses__()`
   * which doesn't ensure the `bl_rna` is set. See: #127127. */

  BPy_TypesModule_State *state = static_cast<BPy_TypesModule_State *>(
      PyModule_GetState(submodule));
  BLI_assert(state->ptr.has_value());

  PyObject *arg_key, *arg_value;
  Py_ssize_t arg_pos = 0;
  while (PyDict_Next(bpy_types_dict, &arg_pos, &arg_key, &arg_value)) {
    const char *key_str = PyUnicode_AsUTF8(arg_key);
    if (key_str[0] == '_') {
      continue;
    }

    BLI_assert_msg(
        PyObject_IsSubclass(arg_value, (PyObject *)&pyrna_struct_Type),
        "Members of _bpy_types.py which are not StructRNA sub-classes must use a \"_\" prefix!");

    PointerRNA newptr;
    if (RNA_property_collection_lookup_string(&state->ptr.value(), state->prop, key_str, &newptr))
    {
      StructRNA *srna = srna_from_ptr(&newptr);
      /* Within the Python logic of `./scripts/modules/_bpy_types.py`
       * it's possible this was already initialized. */
      if (RNA_struct_py_type_get(srna) == nullptr) {
        pyrna_subtype_set_rna(arg_value, srna);
      }
    }
#ifndef NDEBUG
    else {
      /* Avoid noisy warnings based on build-options. */
#  ifndef WITH_USD
      if (STREQ(key_str, "USDHook")) {
        continue;
      }
#  endif
      CLOG_WARN(
          BPY_LOG_RNA, "_bpy_types.py defines \"%.200s\" which is not a known RNA type!", key_str);
    }
#endif
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Struct Access: #StructRNA
 *
 * Utilities for accessing & creating #StructRNA on demand.
 * \{ */

StructRNA *pyrna_struct_as_srna(PyObject *self, const bool parent, const char *error_prefix)
{
  BPy_StructRNA *py_srna = nullptr;
  StructRNA *srna;

  /* Unfortunately PyObject_GetAttrString won't look up this types tp_dict first :/ */
  if (PyType_Check(self)) {
    py_srna = (BPy_StructRNA *)PyDict_GetItem(((PyTypeObject *)self)->tp_dict,
                                              bpy_intern_str_bl_rna);
    Py_XINCREF(py_srna);
  }

  if (parent) {
    /* be very careful with this since it will return a parent classes srna.
     * modifying this will do confusing stuff! */
    if (py_srna == nullptr) {
      py_srna = (BPy_StructRNA *)PyObject_GetAttr(self, bpy_intern_str_bl_rna);
    }
  }

  if (py_srna == nullptr) {
    PyErr_Format(PyExc_RuntimeError,
                 "%.200s, missing bl_rna attribute from '%.200s' instance (may not be registered)",
                 error_prefix,
                 Py_TYPE(self)->tp_name);
    return nullptr;
  }

  if (!BPy_StructRNA_Check(py_srna)) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s, bl_rna attribute wrong type '%.200s' on '%.200s'' instance",
                 error_prefix,
                 Py_TYPE(py_srna)->tp_name,
                 Py_TYPE(self)->tp_name);
    Py_DECREF(py_srna);
    return nullptr;
  }

  if (py_srna->ptr->type != &RNA_Struct) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s, bl_rna attribute not a RNA_Struct, on '%.200s'' instance",
                 error_prefix,
                 Py_TYPE(self)->tp_name);
    Py_DECREF(py_srna);
    return nullptr;
  }

  srna = static_cast<StructRNA *>(py_srna->ptr->data);
  Py_DECREF(py_srna);

  return srna;
}

const PointerRNA *pyrna_struct_as_ptr(PyObject *py_obj, const StructRNA *srna)
{
  BPy_StructRNA *bpy_srna = (BPy_StructRNA *)py_obj;
  if (!BPy_StructRNA_Check(py_obj) || !RNA_struct_is_a(bpy_srna->ptr->type, srna)) {
    PyErr_Format(PyExc_TypeError,
                 "Expected a \"bpy.types.%.200s\" not a \"%.200s\"",
                 RNA_struct_identifier(srna),
                 Py_TYPE(py_obj)->tp_name);
    return nullptr;
  }
  PYRNA_STRUCT_CHECK_OBJ(bpy_srna);
  return &bpy_srna->ptr.value();
}

const PointerRNA *pyrna_struct_as_ptr_or_null(PyObject *py_obj, const StructRNA *srna)
{
  if (py_obj == Py_None) {
    return &PointerRNA_NULL;
  }
  return pyrna_struct_as_ptr(py_obj, srna);
}

int pyrna_struct_as_ptr_parse(PyObject *o, void *p)
{
  BPy_StructRNA_Parse *srna_parse = static_cast<BPy_StructRNA_Parse *>(p);
  BLI_assert(srna_parse->type != nullptr);
  srna_parse->ptr = pyrna_struct_as_ptr(o, srna_parse->type);
  if (srna_parse->ptr == nullptr) {
    return 0;
  }
  return 1;
}

int pyrna_struct_as_ptr_or_null_parse(PyObject *o, void *p)
{
  BPy_StructRNA_Parse *srna_parse = static_cast<BPy_StructRNA_Parse *>(p);
  BLI_assert(srna_parse->type != nullptr);
  srna_parse->ptr = pyrna_struct_as_ptr_or_null(o, srna_parse->type);
  if (srna_parse->ptr == nullptr) {
    return 0;
  }
  return 1;
}

/* Orphan functions, not sure where they should go. */

StructRNA *srna_from_self(PyObject *self, const char *error_prefix)
{
  if (self == nullptr) {
    return nullptr;
  }
  if (PyCapsule_CheckExact(self)) {
    return static_cast<StructRNA *>(PyCapsule_GetPointer(self, nullptr));
  }
  if (PyType_Check(self) == 0) {
    return nullptr;
  }

  /* These cases above not errors, they just mean the type was not compatible
   * After this any errors will be raised in the script. */
  return pyrna_struct_as_srna(self, false, error_prefix);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Class Registration: Deferred
 * \{ */

static int deferred_register_prop(StructRNA *srna, PyObject *key, PyObject *item)
{
  if (!BPy_PropDeferred_CheckTypeExact(item)) {
    /* No error, ignoring. */
    return 0;
  }

  /* We only care about results from C which
   * are for sure types, save some time with error */
  PyObject *py_func = static_cast<PyObject *>(((BPy_PropDeferred *)item)->fn);
  PyObject *py_kw = ((BPy_PropDeferred *)item)->kw;

  /* Show the function name in errors to help give context. */
  BLI_assert(PyCFunction_CheckExact(py_func));
  PyMethodDef *py_func_method_def = ((PyCFunctionObject *)py_func)->m_ml;
  const char *func_name = py_func_method_def->ml_name;

  const char *key_str = PyUnicode_AsUTF8(key);

  if (*key_str == '_') {
    PyErr_Format(PyExc_ValueError,
                 "bpy_struct \"%.200s\" registration error: "
                 "'%.200s' %.200s could not register because it starts with an '_'",
                 RNA_struct_identifier(srna),
                 key_str,
                 func_name);
    return -1;
  }

  PyObject *type = PyDict_GetItemString(py_kw, "type");
  StructRNA *type_srna = srna_from_self(type, "");
  if (type_srna) {
    if (!RNA_struct_idprops_datablock_allowed(srna)) {
      PyCFunctionWithKeywords py_func_ref = *(
          PyCFunctionWithKeywords)(void *)PyCFunction_GET_FUNCTION(py_func);
      if (ELEM(py_func_ref, BPy_PointerProperty, BPy_CollectionProperty)) {
        if (RNA_struct_idprops_contains_datablock(type_srna)) {

          PyErr_Format(PyExc_ValueError,
                       "bpy_struct \"%.200s\" registration error: "
                       "'%.200s' %.200s could not register because "
                       "this type doesn't support data-block properties",
                       RNA_struct_identifier(srna),
                       key_str,
                       func_name);
          return -1;
        }
      }
    }
  }

  PyObject *py_srna_cobject = PyCapsule_New(srna, nullptr, nullptr);

  /* Not 100% nice :/, modifies the dict passed, should be ok. */
  PyDict_SetItem(py_kw, bpy_intern_str_attr, key);

  PyObject *args_fake = PyTuple_New(1);
  PyTuple_SET_ITEM(args_fake, 0, py_srna_cobject);

  PyObject *py_ret = PyObject_Call(py_func, args_fake, py_kw);

  if (py_ret) {
    Py_DECREF(py_ret);
    Py_DECREF(args_fake); /* Free's py_srna_cobject too. */
  }
  else {
    /* _must_ print before decrefing args_fake. */
    PyErr_Print();

    Py_DECREF(args_fake); /* Free's py_srna_cobject too. */

    PyErr_Format(PyExc_ValueError,
                 "bpy_struct \"%.200s\" registration error: "
                 "'%.200s' %.200s could not register (see previous error)",
                 RNA_struct_identifier(srna),
                 key_str,
                 func_name);
    return -1;
  }

  return 0;
}

/**
 * Extract `__annotations__` using `typing.get_type_hints` which handles the delayed evaluation.
 */
static int pyrna_deferred_register_class_from_type_hints(StructRNA *srna, PyTypeObject *py_class)
{
  PyObject *annotations_dict = nullptr;

  /* `typing.get_type_hints(py_class)` */
  {
    PyObject *typing_mod = PyImport_ImportModuleLevel("typing", nullptr, nullptr, nullptr, 0);
    if (typing_mod != nullptr) {
      PyObject *get_type_hints_fn = PyObject_GetAttrString(typing_mod, "get_type_hints");
      if (get_type_hints_fn != nullptr) {
        PyObject *args = PyTuple_New(1);

        PyTuple_SET_ITEM(args, 0, (PyObject *)py_class);
        Py_INCREF(py_class);

        annotations_dict = PyObject_CallObject(get_type_hints_fn, args);

        Py_DECREF(args);
        Py_DECREF(get_type_hints_fn);
      }
      Py_DECREF(typing_mod);
    }
  }

  int ret = 0;
  if (annotations_dict != nullptr) {
    if (PyDict_CheckExact(annotations_dict)) {
      PyObject *item, *key;
      Py_ssize_t pos = 0;

      while (PyDict_Next(annotations_dict, &pos, &key, &item)) {
        ret = deferred_register_prop(srna, key, item);
        if (ret != 0) {
          break;
        }
      }
    }
    else {
      /* Should never happen, an error won't have been raised, so raise one. */
      PyErr_Format(PyExc_TypeError,
                   "typing.get_type_hints returned: %.200s, expected dict\n",
                   Py_TYPE(annotations_dict)->tp_name);
      ret = -1;
    }

    Py_DECREF(annotations_dict);
  }
  else {
    BLI_assert(PyErr_Occurred());
    fprintf(stderr, "typing.get_type_hints failed with: %.200s\n", py_class->tp_name);
    ret = -1;
  }

  return ret;
}

static int pyrna_deferred_register_props(StructRNA *srna, PyObject *class_dict)
{
  PyObject *annotations_dict;
  PyObject *item, *key;
  Py_ssize_t pos = 0;
  int ret = 0;

  /* in both cases PyDict_CheckExact(class_dict) will be true even
   * though Operators have a metaclass dict namespace */
  if ((annotations_dict = PyDict_GetItem(class_dict, bpy_intern_str___annotations__)) &&
      PyDict_CheckExact(annotations_dict))
  {
    while (PyDict_Next(annotations_dict, &pos, &key, &item)) {
      ret = deferred_register_prop(srna, key, item);

      if (ret != 0) {
        break;
      }
    }
  }

  return ret;
}

static int pyrna_deferred_register_class_recursive(StructRNA *srna, PyTypeObject *py_class)
{
  const int len = PyTuple_GET_SIZE(py_class->tp_bases);
  int i, ret;

  /* First scan base classes for registerable properties. */
  for (i = 0; i < len; i++) {
    PyTypeObject *py_superclass = (PyTypeObject *)PyTuple_GET_ITEM(py_class->tp_bases, i);

    /* the rules for using these base classes are not clear,
     * 'object' is of course not worth looking into and
     * existing subclasses of RNA would cause a lot more dictionary
     * looping then is needed (SomeOperator would scan Operator.__dict__)
     * which is harmless, but not at all useful.
     *
     * So only scan base classes which are not subclasses if blender types.
     * This best fits having 'mix-in' classes for operators and render engines.
     */
    if (py_superclass != &PyBaseObject_Type &&
        !PyObject_IsSubclass((PyObject *)py_superclass, (PyObject *)&pyrna_struct_Type))
    {
      ret = pyrna_deferred_register_class_recursive(srna, py_superclass);

      if (ret != 0) {
        return ret;
      }
    }
  }

  /* Not register out own properties. */
  /* getattr(..., "__dict__") returns a proxy. */
  return pyrna_deferred_register_props(srna, py_class->tp_dict);
}

int pyrna_deferred_register_class(StructRNA *srna, PyTypeObject *py_class)
{
  /* Panels and Menus don't need this
   * save some time and skip the checks here */
  if (!RNA_struct_system_idprops_register_check(srna)) {
    return 0;
  }

#ifdef USE_POSTPONED_ANNOTATIONS
  const bool use_postponed_annotations = true;
#else
  const bool use_postponed_annotations = false;
#endif

  if (use_postponed_annotations) {
    return pyrna_deferred_register_class_from_type_hints(srna, py_class);
  }
  return pyrna_deferred_register_class_recursive(srna, py_class);
}

static int rna_function_register_arg_count(FunctionRNA *func, int *min_count)
{
  const ListBase *lb = RNA_function_defined_parameters(func);
  PropertyRNA *parm;
  const int flag = RNA_function_flag(func);
  const bool is_staticmethod = (flag & FUNC_NO_SELF) && !(flag & FUNC_USE_SELF_TYPE);
  int count = is_staticmethod ? 0 : 1;
  bool done_min_count = false;

  LISTBASE_FOREACH (Link *, link, lb) {
    parm = (PropertyRNA *)link;
    if (!(RNA_parameter_flag(parm) & PARM_OUTPUT)) {
      if (!done_min_count && (RNA_parameter_flag(parm) & PARM_PYFUNC_REGISTER_OPTIONAL)) {
        /* From now on, the following parameters are optional in a Python function. */
        if (min_count) {
          *min_count = count;
        }
        done_min_count = true;
      }
      count++;
    }
  }

  if (!done_min_count && min_count) {
    *min_count = count;
  }
  return count;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Class Registration: Utilities
 *
 * Mainly helpers for `register_class` & `unregister_class`.
 * \{ */

static int bpy_class_validate_recursive(PointerRNA *dummy_ptr,
                                        StructRNA *srna,
                                        void *py_data,
                                        bool *have_function)
{
  const ListBase *lb;
  const char *class_type = RNA_struct_identifier(srna);
  StructRNA *srna_base = RNA_struct_base(srna);
  PyObject *py_class = (PyObject *)py_data;
  PyObject *base_class = static_cast<PyObject *>(RNA_struct_py_type_get(srna));
  int i, arg_count, func_arg_count, func_arg_min_count = 0;
  const char *py_class_name = ((PyTypeObject *)py_class)->tp_name; /* __name__ */

  if (srna_base) {
    if (bpy_class_validate_recursive(dummy_ptr, srna_base, py_data, have_function) != 0) {
      return -1;
    }
  }

  if (base_class) {
    if (!PyObject_IsSubclass(py_class, base_class)) {
      PyErr_Format(PyExc_TypeError,
                   "expected %.200s subclass of class \"%.200s\"",
                   class_type,
                   py_class_name);
      return -1;
    }
  }

  /* Verify callback functions. */
  lb = RNA_struct_type_functions(srna);
  i = 0;
  LISTBASE_FOREACH (Link *, link, lb) {
    FunctionRNA *func = (FunctionRNA *)link;
    const int flag = RNA_function_flag(func);
    if (!(flag & FUNC_REGISTER)) {
      continue;
    }

    PyObject *item;
    switch (PyObject_GetOptionalAttrString(py_class, RNA_function_identifier(func), &item)) {
      case 1: {
        break;
      }
      case 0: {
        if ((flag & (FUNC_REGISTER_OPTIONAL & ~FUNC_REGISTER)) == 0) {
          PyErr_Format(PyExc_AttributeError,
                       "expected %.200s, %.200s class to have an \"%.200s\" attribute",
                       class_type,
                       py_class_name,
                       RNA_function_identifier(func));
          return -1;
        }
        break;
      }
      case -1: { /* Unexpected error, an exception will have been raise. */
        return -1;
      }
    }

    have_function[i] = (item != nullptr);
    i++;

    if (item == nullptr) {
      continue;
    }

    /* TODO(@ideasman42): this is used for classmethod's too,
     * even though class methods should have 'FUNC_USE_SELF_TYPE' set, see Operator.poll for eg.
     * Keep this as-is since it's working, but we should be using
     * 'FUNC_USE_SELF_TYPE' for many functions. */
    const bool is_staticmethod = (flag & FUNC_NO_SELF) && !(flag & FUNC_USE_SELF_TYPE);

    /* Store original so we can decrement its reference before returning. */
    PyObject *item_orig = item;

    if (is_staticmethod) {
      if (PyMethod_Check(item) == 0) {
        PyErr_Format(PyExc_TypeError,
                     "expected %.200s, %.200s class \"%.200s\" "
                     "attribute to be a static/class method, not a %.200s",
                     class_type,
                     py_class_name,
                     RNA_function_identifier(func),
                     Py_TYPE(item)->tp_name);
        Py_DECREF(item_orig);
        return -1;
      }
      item = ((PyMethodObject *)item)->im_func;
    }
    else {
      if (PyFunction_Check(item) == 0) {
        PyErr_Format(PyExc_TypeError,
                     "expected %.200s, %.200s class \"%.200s\" "
                     "attribute to be a function, not a %.200s",
                     class_type,
                     py_class_name,
                     RNA_function_identifier(func),
                     Py_TYPE(item)->tp_name);
        Py_DECREF(item_orig);
        return -1;
      }
    }

    func_arg_count = rna_function_register_arg_count(func, &func_arg_min_count);

    if (func_arg_count >= 0) { /* -1 if we don't care. */
      arg_count = ((PyCodeObject *)PyFunction_GET_CODE(item))->co_argcount;

      /* NOTE: the number of args we check for and the number of args we give to
       * `@staticmethods` are different (quirk of Python),
       * this is why #rna_function_register_arg_count() doesn't return the value -1. */
      if (is_staticmethod) {
        func_arg_count++;
        func_arg_min_count++;
      }

      if (arg_count < func_arg_min_count || arg_count > func_arg_count) {
        if (func_arg_min_count != func_arg_count) {
          PyErr_Format(
              PyExc_ValueError,
              "expected %.200s, %.200s class \"%.200s\" function to have between %d and %d "
              "args, found %d",
              class_type,
              py_class_name,
              RNA_function_identifier(func),
              func_arg_count,
              func_arg_min_count,
              arg_count);
        }
        else {
          PyErr_Format(
              PyExc_ValueError,
              "expected %.200s, %.200s class \"%.200s\" function to have %d args, found %d",
              class_type,
              py_class_name,
              RNA_function_identifier(func),
              func_arg_count,
              arg_count);
        }
        Py_DECREF(item_orig);
        return -1;
      }
    }
    Py_DECREF(item_orig);
  }

  /* Allow Python `__name__` to be used for `bl_idname` for convenience. */
  struct {
    const char *rna_attr;
    PyObject *py_attr;
  } bpy_property_substitutions[] = {
      {"bl_idname", bpy_intern_str___name__},
      {"bl_description", bpy_intern_str___doc__},
  };

  /* Verify properties. */
  lb = RNA_struct_type_properties(srna);
  LISTBASE_FOREACH (Link *, link, lb) {
    PropertyRNA *prop = (PropertyRNA *)link;
    const int flag = RNA_property_flag(prop);

    if (!(flag & PROP_REGISTER)) {
      continue;
    }

    const char *identifier = RNA_property_identifier(prop);
    PyObject *item = nullptr;
    switch (PyObject_GetOptionalAttrString(py_class, identifier, &item)) {
      case 1: { /* Found. */
        if (pyrna_py_to_prop(dummy_ptr, prop, nullptr, item, "validating class:") != 0) {
          Py_DECREF(item);
          return -1;
        }
        Py_DECREF(item);
        break;
      }
      case -1: { /* Not found (an unexpected error). */
        /* Typically the attribute will exist or not, in previous releases all errors
         * were assumed to be missing attributes, so print the error and move on. */
        PyErr_Print();
        [[fallthrough]];
      }
      case 0: { /* Not found, check for fallbacks. */

        /* Sneaky workaround to use the class name as the `bl_idname`. */
        int i;
        for (i = 0; i < ARRAY_SIZE(bpy_property_substitutions); i += 1) {
          if (STREQ(identifier, bpy_property_substitutions[i].rna_attr)) {
            break;
          }
        }

        if (i < ARRAY_SIZE(bpy_property_substitutions)) {
          PyObject *py_attr = bpy_property_substitutions[i].py_attr;
          switch (PyObject_GetOptionalAttr(py_class, py_attr, &item)) {
            case 1: { /* Found. */
              if (UNLIKELY(item == Py_None)) {
                Py_DECREF(item);
                item = nullptr;
              }
              else {
                if (pyrna_py_to_prop(dummy_ptr, prop, nullptr, item, "validating class:") != 0) {
                  Py_DECREF(item);
                  return -1;
                }
                Py_DECREF(item);
              }
              break;
            }
            case -1: { /* Not found (an unexpected error). */
              PyErr_Print();
              break;
            }
          }
        }

        if (item == nullptr && ((flag & PROP_REGISTER_OPTIONAL) != PROP_REGISTER_OPTIONAL)) {
          PyErr_Format(PyExc_AttributeError,
                       "expected %.200s, %.200s class to have an \"%.200s\" attribute",
                       class_type,
                       py_class_name,
                       identifier);
          return -1;
        }
        break;
      }
    }
  }

  return 0;
}

static int bpy_class_validate(PointerRNA *dummy_ptr, void *py_data, bool *have_function)
{
  return bpy_class_validate_recursive(dummy_ptr, dummy_ptr->type, py_data, have_function);
}

/* TODO: multiple return values like with RNA functions. */
static int bpy_class_call(bContext *C, PointerRNA *ptr, FunctionRNA *func, ParameterList *parms)
{
  PyObject *args;
  PyObject *ret = nullptr, *py_srna = nullptr, *py_class_instance = nullptr, *parmitem;
  PyTypeObject *py_class;
  PropertyRNA *parm;
  ParameterIterator iter;
  PointerRNA funcptr;
  int err = 0, i, ret_len = 0;
  const int flag = RNA_function_flag(func);
  const bool is_staticmethod = (flag & FUNC_NO_SELF) && !(flag & FUNC_USE_SELF_TYPE);
  const bool is_classmethod = (flag & FUNC_NO_SELF) && (flag & FUNC_USE_SELF_TYPE);

  PropertyRNA *pret_single = nullptr;
  void *retdata_single = nullptr;

  PyGILState_STATE gilstate;

#ifdef USE_PEDANTIC_WRITE
  const bool is_readonly_init = !(RNA_struct_is_a(ptr->type, &RNA_Operator) ||
                                  RNA_struct_is_a(ptr->type, &RNA_Gizmo));
  // const char *func_id = RNA_function_identifier(func);  /* UNUSED */
  /* Testing, for correctness, not operator and not draw function. */
  const bool is_readonly = !(RNA_function_flag(func) & FUNC_ALLOW_WRITE);
#endif

  py_class = static_cast<PyTypeObject *>(RNA_struct_py_type_get(ptr->type));
  /* Rare case. can happen when registering subclasses. */
  if (py_class == nullptr) {
    CLOG_WARN(BPY_LOG_RNA,
              "unable to get Python class for RNA struct '%.200s'",
              RNA_struct_identifier(ptr->type));
    return -1;
  }

  /* XXX, this is needed because render engine calls without a context
   * this should be supported at some point, but at the moment it's not! */
  if (C == nullptr) {
    C = BPY_context_get();
  }

  bpy_context_set(C, &gilstate);

  /* Annoying! We need to check if the screen gets set to nullptr which is a
   * hint that the file was actually re-loaded. */
  const bool is_valid_wm = (CTX_wm_manager(C) != nullptr);

  if (!(is_staticmethod || is_classmethod)) {
    /* Some data-types (operator, render engine) can store PyObjects for re-use. */
    if (ptr->data) {
      void **instance = RNA_struct_instance(ptr);

      if (instance) {
        if (*instance) {
          py_class_instance = static_cast<PyObject *>(*instance);
          Py_INCREF(py_class_instance);
        }
      }
    }
    /* End exception. */

    if (py_class_instance == nullptr) {
      py_srna = pyrna_struct_CreatePyObject(ptr);
    }

    if (py_class_instance) {
      /* Special case, instance is cached. */
    }
    else if (py_srna == nullptr) {
      py_class_instance = nullptr;
      if (PyErr_Occurred()) {
        err = -1; /* So the error is not overridden below. */
      }
    }
    else if (py_srna == Py_None) { /* Probably won't ever happen, but possible. */
      Py_DECREF(py_srna);
      py_class_instance = nullptr;
      if (PyErr_Occurred()) {
        err = -1; /* So the error is not overridden below. */
      }
    }
    else {
#if 0
      /* Skip the code below and call init directly on the allocated 'py_srna'
       * otherwise __init__() always needs to take a second self argument, see pyrna_struct_new().
       * Although this is annoying to have to implement a part of Python's
       * typeobject.c:type_call().
       */
      if (py_class->tp_init) {
#  ifdef USE_PEDANTIC_WRITE
        const int prev_write = rna_disallow_writes;
        rna_disallow_writes = is_readonly_init ? false :
                                                 true; /* Only operators can write on __init__. */
#  endif

        /* True in most cases even when the class itself doesn't define an __init__ function. */
        args = PyTuple_New(0);
        if (py_class->tp_init(py_srna, args, nullptr) < 0) {
          Py_DECREF(py_srna);
          py_srna = nullptr;
          /* Err set below. */
        }
        Py_DECREF(args);
#  ifdef USE_PEDANTIC_WRITE
        rna_disallow_writes = prev_write;
#  endif
      }
      py_class_instance = py_srna;

#else
#  ifdef USE_PEDANTIC_WRITE
      const int prev_write = rna_disallow_writes;
      rna_disallow_writes = is_readonly_init ? false :
                                               true; /* Only operators can write on __init__. */
#  endif

/* 'almost' all the time calling the class isn't needed.
 * We could just do... */
#  if 0
      py_class_instance = py_srna;
      Py_INCREF(py_class_instance);
#  endif
      /*
       * This would work fine, but means __init__ functions wouldn't run.
       * None of Blender's default scripts use __init__ but it's nice to call it
       * for general correctness. just to note why this is here when it could be safely removed.
       */
      py_class_instance = PyObject_CallOneArg(reinterpret_cast<PyObject *>(py_class), py_srna);

#  ifdef USE_PEDANTIC_WRITE
      rna_disallow_writes = prev_write;
#  endif

#endif

      if (py_class_instance == nullptr) {
        if (PyErr_Occurred()) {
          err = -1; /* So the error is not overridden below. */
        }
      }

      Py_DECREF(py_srna);
    }
  }

  /* Initializing the class worked, now run its invoke function. */
  if (err != -1 && (is_staticmethod || is_classmethod || py_class_instance)) {
    PyObject *item = PyObject_GetAttrString((PyObject *)py_class, RNA_function_identifier(func));

    const bool item_type_valid = (item != nullptr) &&
                                 (is_staticmethod ? PyMethod_Check(item) : PyFunction_Check(item));
    if (item_type_valid) {
      funcptr = RNA_pointer_create_discrete(nullptr, &RNA_Function, func);
      int arg_count;

      /* NOTE: registration will have already checked the argument count matches
       * #rna_function_register_arg_count so there is no need to inspect the RNA function. */

      if (is_staticmethod) {
        arg_count =
            ((PyCodeObject *)PyFunction_GET_CODE(((PyMethodObject *)item)->im_func))->co_argcount -
            1;
      }
      else {
        arg_count = ((PyCodeObject *)PyFunction_GET_CODE(item))->co_argcount;
      }
      args = PyTuple_New(arg_count); /* First arg is included in 'item'. */

      if (is_staticmethod) {
        i = 0;
      }
      else if (is_classmethod) {
        PyTuple_SET_ITEM(args, 0, (PyObject *)py_class);
        i = 1;
      }
      else {
        PyTuple_SET_ITEM(args, 0, py_class_instance);
        i = 1;
      }

      RNA_parameter_list_begin(parms, &iter);

      /* Parse function parameters. */
      for (; iter.valid; RNA_parameter_list_next(&iter)) {
        parm = iter.parm;

        /* Only useful for single argument returns, we'll need another list loop for multiple. */
        if (RNA_parameter_flag(parm) & PARM_OUTPUT) {
          ret_len++;
          if (pret_single == nullptr) {
            pret_single = parm;
            retdata_single = iter.data;
          }

          continue;
        }

        if (i < arg_count) {
          parmitem = pyrna_param_to_py(&funcptr, parm, iter.data);
          PyTuple_SET_ITEM(args, i, parmitem);
          i++;
        }
      }

#ifdef USE_PEDANTIC_WRITE
      /* Handle nested draw calls, see: #89253. */
      const bool rna_disallow_writes_prev = rna_disallow_writes;
      rna_disallow_writes = is_readonly ? true : false;
#endif
      /* *** Main Caller *** */

      ret = PyObject_Call(item, args, nullptr);

      /* *** Done Calling *** */

#ifdef USE_PEDANTIC_WRITE
      rna_disallow_writes = rna_disallow_writes_prev;
#endif

      RNA_parameter_list_end(&iter);
      Py_DECREF(item);
      Py_DECREF(args);
    }
    else {
      PyErr_Print();
      PyErr_Format(PyExc_TypeError,
                   "could not find function %.200s in %.200s to execute callback",
                   RNA_function_identifier(func),
                   RNA_struct_identifier(ptr->type));
      err = -1;
    }
  }
  else {
    /* The error may be already set if the class instance couldn't be created. */
    if (err != -1) {
      PyErr_Format(PyExc_RuntimeError,
                   "could not create instance of %.200s to call callback function '%.200s'",
                   RNA_struct_identifier(ptr->type),
                   RNA_function_identifier(func));
      err = -1;
    }
  }

  if (ret == nullptr) { /* Covers py_class_instance failing too. */
    err = -1;
  }
  else {
    if (ret_len == 0 && ret != Py_None) {
      PyErr_Format(PyExc_RuntimeError,
                   "expected class %.200s, function %.200s to return None, not %.200s",
                   RNA_struct_identifier(ptr->type),
                   RNA_function_identifier(func),
                   Py_TYPE(ret)->tp_name);
      err = -1;
    }
    else if (ret_len == 1) {
      err = pyrna_py_to_prop(&funcptr, pret_single, retdata_single, ret, "");

      /* When calling operator functions only gives `Function.result` with no line number
       * since the function has finished calling on error, re-raise the exception with more
       * information since it would be slow to create prefix on every call
       * (when there are no errors). */
      if (err == -1) {
        PyC_Err_Format_Prefix(PyExc_RuntimeError,
                              "class %.200s, function %.200s: incompatible return value ",
                              RNA_struct_identifier(ptr->type),
                              RNA_function_identifier(func));
      }
    }
    else if (ret_len > 1) {

      if (PyTuple_Check(ret) == 0) {
        PyErr_Format(
            PyExc_RuntimeError,
            "expected class %.200s, function %.200s to return a tuple of size %d, not %.200s",
            RNA_struct_identifier(ptr->type),
            RNA_function_identifier(func),
            ret_len,
            Py_TYPE(ret)->tp_name);
        err = -1;
      }
      else if (PyTuple_GET_SIZE(ret) != ret_len) {
        PyErr_Format(PyExc_RuntimeError,
                     "class %.200s, function %.200s to returned %d items, expected %d",
                     RNA_struct_identifier(ptr->type),
                     RNA_function_identifier(func),
                     PyTuple_GET_SIZE(ret),
                     ret_len);
        err = -1;
      }
      else {

        RNA_parameter_list_begin(parms, &iter);

        /* Parse function parameters. */
        for (i = 0; iter.valid; RNA_parameter_list_next(&iter)) {
          parm = iter.parm;

          /* Only useful for single argument returns, we'll need another list loop for multiple. */
          if (RNA_parameter_flag(parm) & PARM_OUTPUT) {
            err = pyrna_py_to_prop(
                &funcptr, parm, iter.data, PyTuple_GET_ITEM(ret, i++), "calling class function:");
            if (err) {
              break;
            }
          }
        }

        RNA_parameter_list_end(&iter);
      }
    }
    Py_DECREF(ret);
  }

  if (err != 0) {
    ReportList *reports;
    /* Alert the user, else they won't know unless they see the console. */
    if ((!is_staticmethod) && (!is_classmethod) && (ptr->data) &&
        RNA_struct_is_a(ptr->type, &RNA_Operator) &&
        (is_valid_wm == (CTX_wm_manager(C) != nullptr)))
    {
      wmOperator *op = static_cast<wmOperator *>(ptr->data);
      reports = op->reports;
    }
    else {
      /* Won't alert users, but they can view in 'info' space. */
      reports = CTX_wm_reports(C);
    }

    if (reports) {
      BPy_errors_to_report(reports);
    }

    /* Also print in the console for Python. */
    PyErr_Print();
    /* Print a small line at ERROR level so that tests that rely on --debug-exit-on-error can
     * fail. This assumes that the majority of the information is already seen in the console via
     * PyErr_Print and should not be duplicated */
    CLOG_ERROR(BPY_LOG_RNA,
               "Python script error in %.200s.%.200s",
               RNA_struct_identifier(ptr->type),
               RNA_function_identifier(func));
  }

  bpy_context_clear(C, &gilstate);

  return err;
}

/**
 * \param decref: When true, decrease the reference.
 */
static void bpy_class_free_ex(PyObject *self, bool decref)
{
#ifdef WITH_PYTHON_MODULE
  /* This can happen when Python has exited before all Blender's RNA types have been freed.
   * In this Python memory management can't run.
   *
   * NOTE(@ideasman42): While I wasn't able to redo locally, it resolves the problem.
   * This happens:
   * - With AUDASPACE on macOS, see: #125376.
   * - With the build-bot on Linux, see: #135195.
   * Ideally this would be resolved
   * by correcting the order classes are freed (before Python exits). */
  if (!Py_IsInitialized()) {
    return;
  }
#endif

  PyGILState_STATE gilstate = PyGILState_Ensure();

  /* Breaks re-registering classes. */
  // PyDict_Clear(((PyTypeObject *)self)->tp_dict);

  /* Remove the RNA attribute instead. */

  /* NOTE: it's important to use `delattr` instead of `PyDict_DelItem`
   * to ensure the internal slots are updated (which is also used for assignment). */
  if (PyObject_DelAttr(self, bpy_intern_str_bl_rna) == -1) {
    PyErr_Clear();
  }

#if 0 /* Needs further investigation, too annoying so quiet for now. */
  if (G.debug & G_DEBUG_PYTHON) {
    if (self->ob_refcnt > 1) {
      PyC_ObSpit("zombie class - reference should be 1", self);
    }
  }
#endif

  if (decref) {
    Py_DECREF(self);
  }

  PyGILState_Release(gilstate);
}

static void bpy_class_free(void *pyob_ptr)
{
  /* Don't remove a reference because the argument passed in is from #ExtensionRNA::data
   * which doesn't own the reference.
   * This value is typically stored in #StructRNA::py_type which is handled separately. */
  bool decref = false;
  bpy_class_free_ex(static_cast<PyObject *>(pyob_ptr), decref);
}

/**
 * \return the first base-class which is already registered or null.
 */
static PyTypeObject *bpy_class_check_any_bases_registered(PyTypeObject *cls)
{
  if (PyObject *bases = cls->tp_bases) {
    const int bases_num = PyTuple_GET_SIZE(bases);
    for (int i = 0; i < bases_num; i++) {
      PyTypeObject *base_cls = (PyTypeObject *)PyTuple_GET_ITEM(bases, i);
      BLI_assert(PyType_Check(base_cls));
      if (base_cls->tp_dict) {
        if (BPy_StructRNA *py_srna = (BPy_StructRNA *)PyDict_GetItem(base_cls->tp_dict,
                                                                     bpy_intern_str_bl_rna))
        {
          if (const StructRNA *srna = static_cast<const StructRNA *>(py_srna->ptr->data)) {
            if (srna->flag & STRUCT_RUNTIME) {
              return base_cls;
            }
          }
        }
      }

      if (PyTypeObject *base_cls_test = bpy_class_check_any_bases_registered(base_cls)) {
        return base_cls_test;
      }
    }
  }
  return nullptr;
}

/**
 * \return the first sub-class which is already registered or null.
 */
static PyTypeObject *bpy_class_check_any_subclasses_registered(PyTypeObject *cls)
{
  PyObject *subclasses = static_cast<PyObject *>(cls->tp_subclasses);
  if (subclasses) {
    BLI_assert(PyDict_CheckExact(subclasses));
    PyObject *key = nullptr;
    Py_ssize_t pos = 0;
    PyObject *value = nullptr;
    while (PyDict_Next(subclasses, &pos, &key, &value)) {
      BLI_assert(PyWeakref_CheckRef(value));
      PyObject *value_ref = PyWeakref_GET_OBJECT(value);
      if (value_ref == Py_None) {
        continue;
      }

      PyTypeObject *sub_cls = reinterpret_cast<PyTypeObject *>(value_ref);
      if (sub_cls->tp_dict) {
        if (BPy_StructRNA *py_srna = reinterpret_cast<BPy_StructRNA *>(
                PyDict_GetItem(sub_cls->tp_dict, bpy_intern_str_bl_rna)))
        {
          if (const StructRNA *srna = static_cast<const StructRNA *>(py_srna->ptr->data)) {
            if (srna->flag & STRUCT_RUNTIME) {
              return sub_cls;
            }
          }
        }
      }

      if (PyTypeObject *sub_cls_test = bpy_class_check_any_subclasses_registered(sub_cls)) {
        return sub_cls_test;
      }
    }
  }
  return nullptr;
}

void pyrna_alloc_types()
{
  /* NOTE: This isn't essential to run on startup, since sub-types will lazy initialize.
   * But keep running in debug mode so we get immediate notification of bad class hierarchy
   * or any errors in `_bpy_types.py` at load time, so errors don't go unnoticed. */

#ifndef NDEBUG
  PyGILState_STATE gilstate = PyGILState_Ensure();

  PropertyRNA *prop;

  /* Avoid doing this lookup for every getattr. */
  PointerRNA ptr = RNA_blender_rna_pointer_create();
  prop = RNA_struct_find_property(&ptr, "structs");

  RNA_PROP_BEGIN (&ptr, itemptr, prop) {
    PyObject *item = pyrna_struct_Subtype(&itemptr);
    if (item == nullptr) {
      if (PyErr_Occurred()) {
        PyErr_Print();
      }
    }
    else {
      Py_DECREF(item);
    }
  }
  RNA_PROP_END;

  PyGILState_Release(gilstate);
#endif /* !NDEBUG */
}

void BPY_free_srna_pytype(StructRNA *srna)
{
  PyObject *py_ptr = static_cast<PyObject *>(RNA_struct_py_type_get(srna));

  if (py_ptr) {
    /* Remove a reference because `srna` owns it. */
    bool decref = true;
    bpy_class_free_ex(py_ptr, decref);
    RNA_struct_py_type_set(srna, nullptr);
  }
}

/* -------------------------------------------------------------------- */
/** \name RNA Class Register Method
 * \{ */

#define BPY_TYPEDEF_REGISTERABLE_DOC \
  "type[" \
  ":class:`bpy.types.Panel` | " \
  ":class:`bpy.types.UIList` | " \
  ":class:`bpy.types.Menu` | " \
  ":class:`bpy.types.Header` | " \
  ":class:`bpy.types.Operator` | " \
  ":class:`bpy.types.KeyingSetInfo` | " \
  ":class:`bpy.types.RenderEngine` | " \
  ":class:`bpy.types.AssetShelf` | " \
  ":class:`bpy.types.FileHandler` | " \
  ":class:`bpy.types.PropertyGroup` | " \
  ":class:`bpy.types.AddonPreferences` | " \
  ":class:`bpy.types.NodeTree` | " \
  ":class:`bpy.types.Node` | " \
  ":class:`bpy.types.NodeSocket`" \
  "]"

/**
 * \warning memory leak!
 *
 * NOTE(@ideasman42): There is currently a bug where moving the registration of a Python class does
 * not properly manage reference-counts from the Python class. As the `srna` owns
 * the Python class this should not be so tricky, but changing the references as
 * you'd expect when changing ownership crashes blender on exit so I had to comment out
 * the #Py_DECREF. This is not so bad because the leak only happens when re-registering
 * (continuously running `SCRIPT_OT_reload`).
 * This should still be fixed.
 */
PyDoc_STRVAR(
    /* Wrap. */
    pyrna_register_class_doc,
    ".. function:: register_class(cls)\n"
    "\n"
    "   Register a subclass of a Blender type class.\n"
    "\n"
    "   :arg cls: Registerable Blender class type.\n"
    "   :type cls: " BPY_TYPEDEF_REGISTERABLE_DOC
    "\n"
    "\n"
    "   :raises ValueError:\n"
    "      if the class is not a subclass of a registerable blender class.\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      If the class has a *register* class method it will be called\n"
    "      before registration.\n");
PyMethodDef meth_bpy_register_class = {
    "register_class", pyrna_register_class, METH_O, pyrna_register_class_doc};
static PyObject *pyrna_register_class(PyObject * /*self*/, PyObject *py_class)
{
  bContext *C = nullptr;
  ReportList reports;
  StructRegisterFunc reg;
  StructRNA *srna;
  StructRNA *srna_new;
  const char *identifier;
  PyObject *py_cls_meth;
  const char *error_prefix = "register_class(...):";

  if (!PyType_Check(py_class)) {
    PyErr_Format(PyExc_ValueError,
                 "%s expected a class argument, not '%.200s'",
                 error_prefix,
                 Py_TYPE(py_class)->tp_name);
    return nullptr;
  }

  if (PyDict_GetItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna)) {
    PyErr_Format(PyExc_ValueError,
                 "%s already registered as a subclass '%.200s'",
                 error_prefix,
                 ((PyTypeObject *)py_class)->tp_name);
    return nullptr;
  }

  if (!pyrna_write_check()) {
    PyErr_Format(PyExc_RuntimeError,
                 "%s cannot run in readonly state '%.200s'",
                 error_prefix,
                 ((PyTypeObject *)py_class)->tp_name);
    return nullptr;
  }

  /* WARNING: gets parent classes srna, only for the register function. */
  srna = pyrna_struct_as_srna(py_class, true, "register_class(...):");
  if (srna == nullptr) {
    return nullptr;
  }

  if (UNLIKELY(G.debug & G_DEBUG_PYTHON)) {
    /* Warn if a class being registered uses an already registered base-class or sub-class,
     * both checks are needed otherwise the order of registering could suppress the warning.
     *
     * NOTE(@ideasman42) This is mainly to ensure good practice.
     * Mix-in classes are preferred when sharing functionality is needed,
     * otherwise changes to an Operator for example could unintentionally
     * break another operator that sub-classes it. */
    if (PyTypeObject *base_cls_test = bpy_class_check_any_bases_registered(
            (PyTypeObject *)py_class))
    {
      fprintf(stderr,
              "%s warning, %.200s: references and already registered base-class %.200s\n",
              error_prefix,
              ((PyTypeObject *)py_class)->tp_name,
              base_cls_test->tp_name);
    }
    if (PyTypeObject *sub_cls_test = bpy_class_check_any_subclasses_registered(
            (PyTypeObject *)py_class))
    {
      fprintf(stderr,
              "%s warning, %.200s: references and already registered sub-class %.200s\n",
              error_prefix,
              ((PyTypeObject *)py_class)->tp_name,
              sub_cls_test->tp_name);
    }

    /* In practice it isn't useful to manipulate Python properties for `PropertyGroup`
     * instances since the Python objects themselves are not shared,
     * meaning a new Python instance is returned on each attribute access.
     * It may be useful to include other classes in this check - extend as needed.
     * See #141948. */
    if (RNA_struct_is_a(srna, &RNA_PropertyGroup)) {
      if (!PyDict_GetItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str___slots__)) {
        fprintf(stderr,
                "%s warning, %.200s: is expected to contain a \"__slots__\" member "
                "to prevent arbitrary assignments.\n",
                error_prefix,
                ((PyTypeObject *)py_class)->tp_name);
      }
    }
  }

/* Fails in some cases, so can't use this check, but would like to :| */
#if 0
  if (RNA_struct_py_type_get(srna)) {
    PyErr_Format(PyExc_ValueError,
                 "%s %.200s's parent class %.200s is already registered, this is not allowed",
                 error_prefix,
                 ((PyTypeObject *)py_class)->tp_name,
                 RNA_struct_identifier(srna));
    return nullptr;
  }
#endif

  /* Check that we have a register callback for this type. */
  reg = RNA_struct_register(srna);

  if (!reg) {
    PyErr_Format(PyExc_ValueError,
                 "%s expected a subclass of a registerable "
                 "RNA type (%.200s does not support registration)",
                 error_prefix,
                 RNA_struct_identifier(srna));
    return nullptr;
  }

  /* Get the context, so register callback can do necessary refreshes. */
  C = BPY_context_get();

  /* Call the register callback with reports & identifier. */
  BKE_reports_init(&reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);

  identifier = ((PyTypeObject *)py_class)->tp_name;

  srna_new = reg(CTX_data_main(C),
                 &reports,
                 py_class,
                 identifier,
                 bpy_class_validate,
                 bpy_class_call,
                 bpy_class_free);

  if (!BLI_listbase_is_empty(&reports.list)) {
    const bool has_error = (BPy_reports_to_error(&reports, PyExc_RuntimeError, false) == -1);
    if (!has_error) {
      BKE_report_print_level_set(&reports, CLG_quiet_get() ? RPT_WARNING : RPT_DEBUG);
      BPy_reports_write_stdout(&reports, error_prefix);
    }
    if (has_error) {
      BKE_reports_free(&reports);
      return nullptr;
    }
  }
  BKE_reports_free(&reports);

  /* Python errors validating are not converted into reports so the check above will fail.
   * the cause for returning nullptr will be printed as an error */
  if (srna_new == nullptr) {
    return nullptr;
  }

  /* Takes a reference to 'py_class'. */
  pyrna_subtype_set_rna(py_class, srna_new);

  /* Old srna still references us, keep the check in case registering somehow can free it. */
  if (PyObject *old_py_class = static_cast<PyObject *>(RNA_struct_py_type_get(srna))) {
    RNA_struct_py_type_set(srna, nullptr);
    Py_DECREF(old_py_class);
  }

  /* Can't use this because it returns a dict proxy
   *
   * item = PyObject_GetAttrString(py_class, "__dict__");
   */
  if (pyrna_deferred_register_class(srna_new, (PyTypeObject *)py_class) != 0) {
    return nullptr;
  }

  /* Call classed register method.
   * Note that zero falls through, no attribute, no error. */
  switch (PyObject_GetOptionalAttr(py_class, bpy_intern_str_register, &py_cls_meth)) {
    case 1: {
      PyObject *ret = PyObject_CallObject(py_cls_meth, nullptr);
      Py_DECREF(py_cls_meth);
      if (ret) {
        Py_DECREF(ret);
      }
      else {
        return nullptr;
      }
      break;
    }
    case -1: {
      return nullptr;
    }
  }

  Py_RETURN_NONE;
}

static int pyrna_srna_contains_pointer_prop_srna(StructRNA *srna_props,
                                                 StructRNA *srna,
                                                 const char **r_prop_identifier)
{
  PropertyRNA *prop;

  /* Verify properties. */
  const ListBase *lb = RNA_struct_type_properties(srna);

  LISTBASE_FOREACH (LinkData *, link, lb) {
    prop = (PropertyRNA *)link;
    if (RNA_property_type(prop) == PROP_POINTER && !RNA_property_builtin(prop)) {
      PointerRNA tptr = RNA_pointer_create_discrete(nullptr, &RNA_Struct, srna_props);

      if (RNA_property_pointer_type(&tptr, prop) == srna) {
        *r_prop_identifier = RNA_property_identifier(prop);
        return 1;
      }
    }
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Class Unregister Method
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_unregister_class_doc,
    ".. function:: unregister_class(cls)\n"
    "\n"
    "   Unload the Python class from blender.\n"
    "\n"
    "   :arg cls: Blender type class, \n"
    "      see :mod:`bpy.utils.register_class` for classes which can \n"
    "      be registered.\n"
    "   :type cls: " BPY_TYPEDEF_REGISTERABLE_DOC
    "\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      If the class has an *unregister* class method it will be called\n"
    "      before unregistering.\n");
PyMethodDef meth_bpy_unregister_class = {
    "unregister_class",
    pyrna_unregister_class,
    METH_O,
    pyrna_unregister_class_doc,
};
static PyObject *pyrna_unregister_class(PyObject * /*self*/, PyObject *py_class)
{
  bContext *C = nullptr;
  StructUnregisterFunc unreg;
  StructRNA *srna;
  PyObject *py_cls_meth;
  const char *error_prefix = "unregister_class(...):";

  if (!PyType_Check(py_class)) {
    PyErr_Format(PyExc_ValueError,
                 "%s expected a class argument, not '%.200s'",
                 error_prefix,
                 Py_TYPE(py_class)->tp_name);
    return nullptr;
  }

#if 0
  if (PyDict_GetItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna) == nullptr) {
    PyErr_Format(PyExc_ValueError, "%s not a registered as a subclass", error_prefix);
    return nullptr;
  }
#endif

  if (!pyrna_write_check()) {
    PyErr_Format(PyExc_RuntimeError,
                 "%s cannot run in readonly state '%.200s'",
                 error_prefix,
                 ((PyTypeObject *)py_class)->tp_name);
    return nullptr;
  }

  srna = pyrna_struct_as_srna(py_class, false, error_prefix);
  if (srna == nullptr) {
    return nullptr;
  }

  if ((srna->flag & STRUCT_RUNTIME) == 0) {
    PyErr_Format(PyExc_RuntimeError,
                 "%s can't unregister a built-in class '%.200s'",
                 error_prefix,
                 ((PyTypeObject *)py_class)->tp_name);
    return nullptr;
  }

  /* Check that we have a unregister callback for this type. */
  unreg = RNA_struct_unregister(srna);

  if (!unreg) {
    PyErr_Format(PyExc_ValueError,
                 "%s expected type '%.200s' subclassed from a registerable RNA type "
                 "(unregister not supported)",
                 error_prefix,
                 ((PyTypeObject *)py_class)->tp_name);
    return nullptr;
  }

  /* Call classed unregister method.
   * Note that zero falls through, no attribute, no error. */
  switch (PyObject_GetOptionalAttr(py_class, bpy_intern_str_unregister, &py_cls_meth)) {
    case 1: {
      PyObject *ret = PyObject_CallObject(py_cls_meth, nullptr);
      Py_DECREF(py_cls_meth);
      if (ret) {
        Py_DECREF(ret);
      }
      else {
        return nullptr;
      }
      break;
    }
    case -1: {
      return nullptr;
    }
  }

  /* Should happen all the time, however it's very slow. */
  if (G.debug & G_DEBUG_PYTHON) {
    /* Remove all properties using this class. */
    StructRNA *srna_iter;
    PropertyRNA *prop_rna;
    const char *prop_identifier = nullptr;

    PointerRNA ptr_rna = RNA_blender_rna_pointer_create();
    prop_rna = RNA_struct_find_property(&ptr_rna, "structs");

    /* Loop over all structs. */
    RNA_PROP_BEGIN (&ptr_rna, itemptr, prop_rna) {
      srna_iter = static_cast<StructRNA *>(itemptr.data);
      if (pyrna_srna_contains_pointer_prop_srna(srna_iter, srna, &prop_identifier)) {
        break;
      }
    }
    RNA_PROP_END;

    if (prop_identifier) {
      PyErr_Format(PyExc_RuntimeError,
                   "%s cannot unregister %s because %s.%s pointer property is using this",
                   error_prefix,
                   RNA_struct_identifier(srna),
                   RNA_struct_identifier(srna_iter),
                   prop_identifier);
      return nullptr;
    }
  }

  /* Get the context, so register callback can do necessary refreshes. */
  C = BPY_context_get();

  /* Call unregister. */
  unreg(CTX_data_main(C), srna); /* Calls bpy_class_free, this decref's py_class. */

  /* Typically `bpy_class_free` will have removed, remove here just in case. */
  if (UNLIKELY(PyDict_Contains(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna))) {
    if (PyDict_DelItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna) == -1) {
      PyErr_Clear();
    }
  }

  Py_RETURN_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Support for extended via the C-API
 * \{ */

void pyrna_struct_type_extend_capi(StructRNA *srna, PyMethodDef *method, PyGetSetDef *getset)
{
  /* See 'add_methods' in Python's 'typeobject.c'. */
  PyTypeObject *type = (PyTypeObject *)pyrna_srna_Subtype(srna);
  PyObject *dict = type->tp_dict;
  if (method != nullptr) {
    for (; method->ml_name != nullptr; method++) {
      PyObject *py_method;

      if (method->ml_flags & METH_CLASS) {
        PyObject *cfunc = PyCFunction_New(method, (PyObject *)type);
        py_method = PyClassMethod_New(cfunc);
        Py_DECREF(cfunc);
      }
      else if (method->ml_flags & METH_STATIC) {
        py_method = PyCFunction_New(method, nullptr);
      }
      else {
        py_method = PyDescr_NewMethod(type, method);
      }

      const int err = PyDict_SetItemString(dict, method->ml_name, py_method);
      Py_DECREF(py_method);
      BLI_assert(!(err < 0));
      UNUSED_VARS_NDEBUG(err);
    }
  }

  if (getset != nullptr) {
    for (; getset->name != nullptr; getset++) {
      PyObject *descr = PyDescr_NewGetSet(type, getset);
      /* Ensure we're not overwriting anything that already exists. */
      BLI_assert(PyDict_GetItem(dict, PyDescr_NAME(descr)) == nullptr);
      PyDict_SetItem(dict, PyDescr_NAME(descr), descr);
      Py_DECREF(descr);
    }
  }
  Py_DECREF(type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exported Methods
 * \{ */

/* Access to the 'owner_id' so work-spaces can filter by add-on. */

static PyObject *pyrna_bl_owner_id_get(PyObject * /*self*/)
{
  const char *name = RNA_struct_state_owner_get();
  if (name) {
    return PyUnicode_FromString(name);
  }
  Py_RETURN_NONE;
}

static PyObject *pyrna_bl_owner_id_set(PyObject * /*self*/, PyObject *value)
{
  const char *name;
  if (value == Py_None) {
    name = nullptr;
  }
  else if (PyUnicode_Check(value)) {
    name = PyUnicode_AsUTF8(value);
  }
  else {
    PyErr_Format(PyExc_ValueError,
                 "owner_set(...): "
                 "expected None or a string, not '%.200s'",
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }
  RNA_struct_state_owner_set(name);
  Py_RETURN_NONE;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

PyMethodDef meth_bpy_owner_id_get = {
    "_bl_owner_id_get",
    (PyCFunction)pyrna_bl_owner_id_get,
    METH_NOARGS,
    nullptr,
};
PyMethodDef meth_bpy_owner_id_set = {
    "_bl_owner_id_set",
    (PyCFunction)pyrna_bl_owner_id_set,
    METH_O,
    nullptr,
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/** \} */
