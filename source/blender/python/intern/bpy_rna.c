/*
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
 */

/** \file
 * \ingroup pythonintern
 *
 * This file is the main interface between Python and Blender's data api (RNA),
 * exposing RNA to Python so blender data can be accessed in a Python like way.
 *
 * The two main types are #BPy_StructRNA and #BPy_PropertyRNA - the base
 * classes for most of the data Python accesses in blender.
 */

#include <Python.h>

#include <float.h> /* FLT_MIN/MAX */
#include <stddef.h>

#include "RNA_types.h"

#include "BLI_bitmap.h"
#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BPY_extern.h"
#include "BPY_extern_clog.h"

#include "bpy_capi_utils.h"
#include "bpy_intern_string.h"
#include "bpy_props.h"
#include "bpy_rna.h"
#include "bpy_rna_anim.h"
#include "bpy_rna_callback.h"

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
#  include "BLI_ghash.h"
#endif

#include "RNA_access.h"
#include "RNA_define.h" /* RNA_def_property_free_identifier */
#include "RNA_enum_types.h"

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_global.h" /* evil G.* */
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_main.h"
#include "BKE_report.h"

/* Only for types. */
#include "BKE_node.h"

#include "DEG_depsgraph_query.h"

#include "../generic/idprop_py_api.h" /* For IDprop lookups. */
#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

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
BPy_StructRNA *bpy_context_module = NULL; /* for fast access */

static PyObject *pyrna_struct_Subtype(PointerRNA *ptr);
static PyObject *pyrna_prop_collection_values(BPy_PropertyRNA *self);

static PyObject *pyrna_register_class(PyObject *self, PyObject *py_class);
static PyObject *pyrna_unregister_class(PyObject *self, PyObject *py_class);

#define BPY_DOC_ID_PROP_TYPE_NOTE \
  "   .. note::\n" \
  "\n" \
  "      Only the :class:`bpy.types.ID`, :class:`bpy.types.Bone` and\n" \
  "      :class:`bpy.types.PoseBone` classes support custom properties.\n"

int pyrna_struct_validity_check(BPy_StructRNA *pysrna)
{
  if (pysrna->ptr.type) {
    return 0;
  }
  PyErr_Format(
      PyExc_ReferenceError, "StructRNA of type %.200s has been removed", Py_TYPE(pysrna)->tp_name);
  return -1;
}

int pyrna_prop_validity_check(BPy_PropertyRNA *self)
{
  if (self->ptr.type) {
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
  RNA_POINTER_INVALIDATE(&self->ptr);
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
          PyType_IsSubtype(Py_TYPE(ob), &pyrna_prop_Type)) {
        BPy_DummyPointerRNA *ob_ptr = (BPy_DummyPointerRNA *)ob;
        if (ob_ptr->ptr.owner_id == id) {
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
//#define DEBUG_RNA_WEAKREF

struct GHash *id_weakref_pool = NULL;
static PyObject *id_free_weakref_cb(PyObject *weakinfo_pair, PyObject *weakref);
static PyMethodDef id_free_weakref_cb_def = {
    "id_free_weakref_cb", (PyCFunction)id_free_weakref_cb, METH_O, NULL};

/* Adds a reference to the list, remember to decref. */
static GHash *id_weakref_pool_get(ID *id)
{
  GHash *weakinfo_hash = NULL;

  if (id_weakref_pool) {
    weakinfo_hash = BLI_ghash_lookup(id_weakref_pool, (void *)id);
  }
  else {
    /* First time, allocate pool. */
    id_weakref_pool = BLI_ghash_ptr_new("rna_global_pool");
    weakinfo_hash = NULL;
  }

  if (weakinfo_hash == NULL) {
    /* We use a ghash as a set, we could use libHX's HXMAP_SINGULAR, but would be an extra dep. */
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

  weakref_capsule = PyCapsule_New(weakinfo_hash, NULL, NULL);
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
  GHash *weakinfo_hash = PyCapsule_GetPointer(weakinfo_pair, NULL);

  if (BLI_ghash_len(weakinfo_hash) > 1) {
    BLI_ghash_remove(weakinfo_hash, weakref, NULL, NULL);
  }
  else { /* Get the last id and free it. */
    BLI_ghash_remove(weakinfo_hash, weakref, NULL, value_id_set);
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

  BLI_ghash_remove(id_weakref_pool, (void *)id, NULL, NULL);
  BLI_ghash_free(weakinfo_hash, NULL, NULL);

  if (BLI_ghash_len(id_weakref_pool) == 0) {
    BLI_ghash_free(id_weakref_pool, NULL, NULL);
    id_weakref_pool = NULL;
#  ifdef DEBUG_RNA_WEAKREF
    printf("id_release_weakref freeing pool\n");
#  endif
  }
}

static void id_release_weakref(struct ID *id)
{
  GHash *weakinfo_hash = BLI_ghash_lookup(id_weakref_pool, (void *)id);
  if (weakinfo_hash) {
    id_release_weakref_list(id, weakinfo_hash);
  }
}

#endif /* USE_PYRNA_INVALIDATE_WEAKREF */

void BPY_id_release(struct ID *id)
{
#ifdef USE_PYRNA_INVALIDATE_GC
  id_release_gc(id);
#endif

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
  if (id_weakref_pool) {
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
      BLI_assert(idtype != NULL);
      PyErr_Format(PyExc_AttributeError,
                   "Writing to ID classes in this context is not allowed: "
                   "%.200s, %.200s datablock, error setting %.200s.%.200s",
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
bool pyrna_write_check(void)
{
  return !rna_disallow_writes;
}

void pyrna_write_set(bool val)
{
  rna_disallow_writes = !val;
}
#else  /* USE_PEDANTIC_WRITE */
bool pyrna_write_check(void)
{
  return true;
}
void pyrna_write_set(bool UNUSED(val))
{
  /* pass */
}
#endif /* USE_PEDANTIC_WRITE */

static Py_ssize_t pyrna_prop_collection_length(BPy_PropertyRNA *self);
static Py_ssize_t pyrna_prop_array_length(BPy_PropertyArrayRNA *self);
static int pyrna_py_to_prop(
    PointerRNA *ptr, PropertyRNA *prop, void *data, PyObject *value, const char *error_prefix);
static int deferred_register_prop(StructRNA *srna, PyObject *key, PyObject *item);

#ifdef USE_MATHUTILS
#  include "../mathutils/mathutils.h" /* So we can have mathutils callbacks. */

static PyObject *pyrna_prop_array_subscript_slice(BPy_PropertyArrayRNA *self,
                                                  PointerRNA *ptr,
                                                  PropertyRNA *prop,
                                                  Py_ssize_t start,
                                                  Py_ssize_t stop,
                                                  Py_ssize_t length);
static short pyrna_rotation_euler_order_get(PointerRNA *ptr,
                                            const short order_fallback,
                                            PropertyRNA **r_prop_eul_order);

/* bpyrna vector/euler/quat callbacks. */
static uchar mathutils_rna_array_cb_index = -1; /* Index for our callbacks. */

/* Subtype not used much yet. */
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

  if (self->prop == NULL) {
    return -1;
  }

  RNA_property_float_get_array(&self->ptr, self->prop, bmo->data);

  /* Euler order exception. */
  if (subtype == MATHUTILS_CB_SUBTYPE_EUL) {
    EulerObject *eul = (EulerObject *)bmo;
    PropertyRNA *prop_eul_order = NULL;
    eul->order = pyrna_rotation_euler_order_get(&self->ptr, eul->order, &prop_eul_order);
  }

  return 0;
}

static int mathutils_rna_vector_set(BaseMathObject *bmo, int subtype)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;
  float min, max;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == NULL) {
    return -1;
  }

#  ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
    return -1;
  }
#  endif /* USE_PEDANTIC_WRITE */

  if (!RNA_property_editable_flag(&self->ptr, self->prop)) {
    PyErr_Format(PyExc_AttributeError,
                 "bpy_prop \"%.200s.%.200s\" is read-only",
                 RNA_struct_identifier(self->ptr.type),
                 RNA_property_identifier(self->prop));
    return -1;
  }

  RNA_property_float_range(&self->ptr, self->prop, &min, &max);

  if (min != -FLT_MAX || max != FLT_MAX) {
    int i, len = RNA_property_array_length(&self->ptr, self->prop);
    for (i = 0; i < len; i++) {
      CLAMP(bmo->data[i], min, max);
    }
  }

  RNA_property_float_set_array(&self->ptr, self->prop, bmo->data);
  if (RNA_property_update_check(self->prop)) {
    RNA_property_update(BPY_context_get(), &self->ptr, self->prop);
  }

  /* Euler order exception. */
  if (subtype == MATHUTILS_CB_SUBTYPE_EUL) {
    EulerObject *eul = (EulerObject *)bmo;
    PropertyRNA *prop_eul_order = NULL;
    const short order = pyrna_rotation_euler_order_get(&self->ptr, eul->order, &prop_eul_order);
    if (order != eul->order) {
      RNA_property_enum_set(&self->ptr, prop_eul_order, eul->order);
      if (RNA_property_update_check(prop_eul_order)) {
        RNA_property_update(BPY_context_get(), &self->ptr, prop_eul_order);
      }
    }
  }
  return 0;
}

static int mathutils_rna_vector_get_index(BaseMathObject *bmo, int UNUSED(subtype), int index)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == NULL) {
    return -1;
  }

  bmo->data[index] = RNA_property_float_get_index(&self->ptr, self->prop, index);
  return 0;
}

static int mathutils_rna_vector_set_index(BaseMathObject *bmo, int UNUSED(subtype), int index)
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == NULL) {
    return -1;
  }

#  ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
    return -1;
  }
#  endif /* USE_PEDANTIC_WRITE */

  if (!RNA_property_editable_flag(&self->ptr, self->prop)) {
    PyErr_Format(PyExc_AttributeError,
                 "bpy_prop \"%.200s.%.200s\" is read-only",
                 RNA_struct_identifier(self->ptr.type),
                 RNA_property_identifier(self->prop));
    return -1;
  }

  RNA_property_float_clamp(&self->ptr, self->prop, &bmo->data[index]);
  RNA_property_float_set_index(&self->ptr, self->prop, index, bmo->data[index]);

  if (RNA_property_update_check(self->prop)) {
    RNA_property_update(BPY_context_get(), &self->ptr, self->prop);
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

/* bpyrna matrix callbacks */
static uchar mathutils_rna_matrix_cb_index = -1; /* Index for our callbacks. */

static int mathutils_rna_matrix_get(BaseMathObject *bmo, int UNUSED(subtype))
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == NULL) {
    return -1;
  }

  RNA_property_float_get_array(&self->ptr, self->prop, bmo->data);
  return 0;
}

static int mathutils_rna_matrix_set(BaseMathObject *bmo, int UNUSED(subtype))
{
  BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

  PYRNA_PROP_CHECK_INT(self);

  if (self->prop == NULL) {
    return -1;
  }

#  ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
    return -1;
  }
#  endif /* USE_PEDANTIC_WRITE */

  if (!RNA_property_editable_flag(&self->ptr, self->prop)) {
    PyErr_Format(PyExc_AttributeError,
                 "bpy_prop \"%.200s.%.200s\" is read-only",
                 RNA_struct_identifier(self->ptr.type),
                 RNA_property_identifier(self->prop));
    return -1;
  }

  /* Can ignore clamping here. */
  RNA_property_float_set_array(&self->ptr, self->prop, bmo->data);

  if (RNA_property_update_check(self->prop)) {
    RNA_property_update(BPY_context_get(), &self->ptr, self->prop);
  }
  return 0;
}

static Mathutils_Callback mathutils_rna_matrix_cb = {
    mathutils_rna_generic_check,
    mathutils_rna_matrix_get,
    mathutils_rna_matrix_set,
    NULL,
    NULL,
};

static short pyrna_rotation_euler_order_get(PointerRNA *ptr,
                                            const short order_fallback,
                                            PropertyRNA **r_prop_eul_order)
{
  /* Attempt to get order. */
  if (*r_prop_eul_order == NULL) {
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
  PyObject *ret = NULL;

#ifdef USE_MATHUTILS
  int subtype, totdim;
  int len;
  const int flag = RNA_property_flag(prop);
  const int type = RNA_property_type(prop);
  const bool is_thick = (flag & PROP_THICK_WRAP) != 0;

  /* disallow dynamic sized arrays to be wrapped since the size could change
   * to a size mathutils does not support */
  if (flag & PROP_DYNAMIC) {
    return NULL;
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
      return NULL;
    }
  }
  else {
    return NULL;
  }

  subtype = RNA_property_subtype(prop);
  totdim = RNA_property_array_dimension(ptr, prop, NULL);

  if (totdim == 1 || (totdim == 2 && subtype == PROP_MATRIX)) {
    if (!is_thick) {
      /* Owned by the mathutils PyObject. */
      ret = pyrna_prop_CreatePyObject(ptr, prop);
    }

    switch (subtype) {
      case PROP_ALL_VECTOR_SUBTYPES:
        if (len >= 2 && len <= 4) {
          if (is_thick) {
            ret = Vector_CreatePyObject(NULL, len, NULL);
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
            ret = Matrix_CreatePyObject(NULL, 4, 4, NULL);
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
            ret = Matrix_CreatePyObject(NULL, 3, 3, NULL);
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
            PropertyRNA *prop_eul_order = NULL;
            const short order = pyrna_rotation_euler_order_get(
                ptr, EULER_ORDER_XYZ, &prop_eul_order);

            ret = Euler_CreatePyObject(NULL, order, NULL); /* TODO, get order from RNA. */
            RNA_property_float_get_array(ptr, prop, ((EulerObject *)ret)->eul);
          }
          else {
            /* Order will be updated from callback on use. */
            /* TODO, get order from RNA. */
            PyObject *eul_cb = Euler_CreatePyObject_cb(
                ret, EULER_ORDER_XYZ, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_EUL);
            Py_DECREF(ret); /* The euler owns 'ret' now. */
            ret = eul_cb;   /* Return the euler instead. */
          }
        }
        else if (len == 4) {
          if (is_thick) {
            ret = Quaternion_CreatePyObject(NULL, NULL);
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
            ret = Color_CreatePyObject(NULL, NULL);
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

  if (ret == NULL) {
    if (is_thick) {
      /* This is an array we can't reference (since it is not thin wrappable)
       * and cannot be coerced into a mathutils type, so return as a list. */
    thick_wrap_slice:
      ret = pyrna_prop_array_subscript_slice(NULL, ptr, prop, 0, len, len);
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

/**
 * Same as #RNA_enum_value_from_id, but raises an exception.
 */
int pyrna_enum_value_from_id(const EnumPropertyItem *item,
                             const char *identifier,
                             int *r_value,
                             const char *error_prefix)
{
  if (RNA_enum_value_from_id(item, identifier, r_value) == 0) {
    const char *enum_str = BPy_enum_as_string(item);
    PyErr_Format(
        PyExc_ValueError, "%s: '%.200s' not found in (%s)", error_prefix, identifier, enum_str);
    MEM_freeN((void *)enum_str);
    return -1;
  }

  return 0;
}

/* note on __cmp__:
 * checking the 'ptr->data' matches works in almost all cases,
 * however there are a few RNA properties that are fake sub-structs and
 * share the pointer with the parent, in those cases this happens 'a.b == a'
 * see: r43352 for example.
 *
 * So compare the 'ptr->type' as well to avoid this problem.
 * It's highly unlikely this would happen that 'ptr->data' and 'ptr->prop' would match,
 * but _not_ 'ptr->type' but include this check for completeness.
 * - campbell */

static int pyrna_struct_compare(BPy_StructRNA *a, BPy_StructRNA *b)
{
  return (((a->ptr.data == b->ptr.data) && (a->ptr.type == b->ptr.type)) ? 0 : -1);
}

static int pyrna_prop_compare(BPy_PropertyRNA *a, BPy_PropertyRNA *b)
{
  return (((a->prop == b->prop) && (a->ptr.data == b->ptr.data) && (a->ptr.type == b->ptr.type)) ?
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
      return NULL;
  }

  return Py_INCREF_RET(res);
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
      return NULL;
  }

  return Py_INCREF_RET(res);
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

  ID *id = self->ptr.owner_id;
  if (id && id != DEG_get_original_id(id)) {
    extra_info = ", evaluated";
  }

  /* Print name if available.
   *
   * Always include the pointer address since it can help identify unique data,
   * or when data is re-allocated internally. */
  name = RNA_struct_name_get_alloc(&self->ptr, NULL, 0, NULL);
  if (name) {
    ret = PyUnicode_FromFormat("<bpy_struct, %.200s(\"%.200s\") at %p%s>",
                               RNA_struct_identifier(self->ptr.type),
                               name,
                               self->ptr.data,
                               extra_info);
    MEM_freeN((void *)name);
    return ret;
  }

  return PyUnicode_FromFormat("<bpy_struct, %.200s at %p%s>",
                              RNA_struct_identifier(self->ptr.type),
                              self->ptr.data,
                              extra_info);
}

static PyObject *pyrna_struct_repr(BPy_StructRNA *self)
{
  ID *id = self->ptr.owner_id;
  PyObject *tmp_str;
  PyObject *ret;

  if (id == NULL || !PYRNA_STRUCT_IS_VALID(self) || (DEG_get_original_id(id) != id)) {
    /* fallback */
    return pyrna_struct_str(self);
  }

  tmp_str = PyUnicode_FromString(id->name + 2);

  if (RNA_struct_is_ID(self->ptr.type) && (id->flag & LIB_EMBEDDED_DATA) == 0) {
    ret = PyUnicode_FromFormat(
        "bpy.data.%s[%R]", BKE_idtype_idcode_to_name_plural(GS(id->name)), tmp_str);
  }
  else {
    const char *path;
    ID *real_id = NULL;
    path = RNA_path_from_real_ID_to_struct(G_MAIN, &self->ptr, &real_id);
    if (path != NULL) {
      /* 'real_id' may be NULL in some cases, although the only valid one is evaluated data,
       * which should have already been caught above.
       * So assert, but handle it without crashing for release builds. */
      BLI_assert(real_id != NULL);

      if (real_id != NULL) {
        Py_DECREF(tmp_str);
        tmp_str = PyUnicode_FromString(real_id->name + 2);
        ret = PyUnicode_FromFormat("bpy.data.%s[%R].%s",
                                   BKE_idtype_idcode_to_name_plural(GS(real_id->name)),
                                   tmp_str,
                                   path);
      }
      else {
        /* Can't find the path, print something useful as a fallback. */
        ret = PyUnicode_FromFormat("bpy.data.%s[%R]...%s",
                                   BKE_idtype_idcode_to_name_plural(GS(id->name)),
                                   tmp_str,
                                   RNA_struct_identifier(self->ptr.type));
      }
      MEM_freeN((void *)path);
    }
    else {
      /* Can't find the path, print something useful as a fallback. */
      ret = PyUnicode_FromFormat("bpy.data.%s[%R]...%s",
                                 BKE_idtype_idcode_to_name_plural(GS(id->name)),
                                 tmp_str,
                                 RNA_struct_identifier(self->ptr.type));
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
  const char *type_id = NULL;
  char type_fmt[64] = "";
  int type;

  PYRNA_PROP_CHECK_OBJ(self);

  type = RNA_property_type(self->prop);

  if (RNA_enum_id_from_value(rna_enum_property_type_items, type, &type_id) == 0) {
    /* Should never happen. */
    PyErr_SetString(PyExc_RuntimeError, "could not use property type, internal error");
    return NULL;
  }

  /* This should never fail. */
  int len = -1;
  char *c = type_fmt;

  while ((*c++ = tolower(*type_id++))) {
  }

  if (type == PROP_COLLECTION) {
    len = pyrna_prop_collection_length(self);
  }
  else if (RNA_property_array_check(self->prop)) {
    len = pyrna_prop_array_length((BPy_PropertyArrayRNA *)self);
  }

  if (len != -1) {
    sprintf(--c, "[%d]", len);
  }

  /* If a pointer, try to print name of pointer target too. */
  if (type == PROP_POINTER) {
    ptr = RNA_property_pointer_get(&self->ptr, self->prop);
    name = RNA_struct_name_get_alloc(&ptr, NULL, 0, NULL);

    if (name) {
      ret = PyUnicode_FromFormat("<bpy_%.200s, %.200s.%.200s(\"%.200s\")>",
                                 type_fmt,
                                 RNA_struct_identifier(self->ptr.type),
                                 RNA_property_identifier(self->prop),
                                 name);
      MEM_freeN((void *)name);
      return ret;
    }
  }
  if (type == PROP_COLLECTION) {
    PointerRNA r_ptr;
    if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
      return PyUnicode_FromFormat(
          "<bpy_%.200s, %.200s>", type_fmt, RNA_struct_identifier(r_ptr.type));
    }
  }

  return PyUnicode_FromFormat("<bpy_%.200s, %.200s.%.200s>",
                              type_fmt,
                              RNA_struct_identifier(self->ptr.type),
                              RNA_property_identifier(self->prop));
}

static PyObject *pyrna_prop_repr_ex(BPy_PropertyRNA *self, const int index_dim, const int index)
{
  ID *id = self->ptr.owner_id;
  PyObject *tmp_str;
  PyObject *ret;
  const char *path;

  PYRNA_PROP_CHECK_OBJ(self);

  if (id == NULL) {
    /* Fallback. */
    return pyrna_prop_str(self);
  }

  tmp_str = PyUnicode_FromString(id->name + 2);

  /* Note that using G_MAIN is absolutely not ideal, but we have no access to actual Main DB from
   * here. */
  ID *real_id = NULL;
  path = RNA_path_from_real_ID_to_property_index(
      G_MAIN, &self->ptr, self->prop, index_dim, index, &real_id);

  if (path) {
    if (real_id != id) {
      Py_DECREF(tmp_str);
      tmp_str = PyUnicode_FromString(real_id->name + 2);
    }
    const char *data_delim = (path[0] == '[') ? "" : ".";
    ret = PyUnicode_FromFormat("bpy.data.%s[%R]%s%s",
                               BKE_idtype_idcode_to_name_plural(GS(real_id->name)),
                               tmp_str,
                               data_delim,
                               path);

    MEM_freeN((void *)path);
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
                              RNA_struct_identifier(self->ptr.type),
                              RNA_function_identifier(self->func));
}

static Py_hash_t pyrna_struct_hash(BPy_StructRNA *self)
{
  return _Py_HashPointer(self->ptr.data);
}

/* From Python's meth_hash v3.1.2. */
static long pyrna_prop_hash(BPy_PropertyRNA *self)
{
  long x, y;
  if (self->ptr.data == NULL) {
    x = 0;
  }
  else {
    x = _Py_HashPointer(self->ptr.data);
    if (x == -1) {
      return -1;
    }
  }
  y = _Py_HashPointer((void *)(self->prop));
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

/* Use our own dealloc so we can free a property if we use one. */
static void pyrna_struct_dealloc(BPy_StructRNA *self)
{
#ifdef PYRNA_FREE_SUPPORT
  if (self->freeptr && self->ptr.data) {
    IDP_FreeProperty(self->ptr.data);
    self->ptr.data = NULL;
  }
#endif /* PYRNA_FREE_SUPPORT */

#ifdef USE_WEAKREFS
  if (self->in_weakreflist != NULL) {
    PyObject_ClearWeakRefs((PyObject *)self);
  }
#endif

#ifdef USE_PYRNA_STRUCT_REFERENCE
  if (self->reference) {
    PyObject_GC_UnTrack(self);
    pyrna_struct_clear(self);
  }
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

  /* Note, for subclassed PyObjects calling PyObject_DEL() directly crashes. */
  Py_TYPE(self)->tp_free(self);
}

#ifdef USE_PYRNA_STRUCT_REFERENCE
static void pyrna_struct_reference_set(BPy_StructRNA *self, PyObject *reference)
{
  if (self->reference) {
    PyObject_GC_UnTrack(self);
    Py_CLEAR(self->reference);
  }
  /* Reference is now NULL. */

  if (reference) {
    self->reference = reference;
    Py_INCREF(reference);
    PyObject_GC_Track(self);
  }
}
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

/* Use our own dealloc so we can free a property if we use one. */
static void pyrna_prop_dealloc(BPy_PropertyRNA *self)
{
#ifdef USE_WEAKREFS
  if (self->in_weakreflist != NULL) {
    PyObject_ClearWeakRefs((PyObject *)self);
  }
#endif
  /* Note, for subclassed PyObjects calling PyObject_DEL() directly crashes. */
  Py_TYPE(self)->tp_free(self);
}

static void pyrna_prop_array_dealloc(BPy_PropertyRNA *self)
{
#ifdef USE_WEAKREFS
  if (self->in_weakreflist != NULL) {
    PyObject_ClearWeakRefs((PyObject *)self);
  }
#endif
  /* Note, for subclassed PyObjects calling PyObject_DEL() directly crashes. */
  Py_TYPE(self)->tp_free(self);
}

static const char *pyrna_enum_as_string(PointerRNA *ptr, PropertyRNA *prop)
{
  const EnumPropertyItem *item;
  const char *result;
  bool free = false;

  RNA_property_enum_items(BPY_context_get(), ptr, prop, &item, NULL, &free);
  if (item) {
    result = BPy_enum_as_string(item);
  }
  else {
    result = "";
  }

  if (free) {
    MEM_freeN((void *)item);
  }

  return result;
}

static int pyrna_string_to_enum(
    PyObject *item, PointerRNA *ptr, PropertyRNA *prop, int *r_value, const char *error_prefix)
{
  const char *param = PyUnicode_AsUTF8(item);

  if (param == NULL) {
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
    MEM_freeN((void *)enum_str);
    return -1;
  }

  return 0;
}

/**
 * Takes a set of strings and map it to and array of booleans.
 *
 * Useful when the values aren't flags.
 *
 * \param type_convert_sign: Maps signed to unsigned range,
 * needed when we want to use the full range of a signed short/char.
 */
BLI_bitmap *pyrna_set_to_enum_bitmap(const EnumPropertyItem *items,
                                     PyObject *value,
                                     int type_size,
                                     bool type_convert_sign,
                                     int bitmap_size,
                                     const char *error_prefix)
{
  /* Set looping. */
  Py_ssize_t pos = 0;
  Py_ssize_t hash = 0;
  PyObject *key;

  BLI_bitmap *bitmap = BLI_BITMAP_NEW(bitmap_size, __func__);

  while (_PySet_NextEntry(value, &pos, &key, &hash)) {
    const char *param = PyUnicode_AsUTF8(key);
    if (param == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s expected a string, not %.200s",
                   error_prefix,
                   Py_TYPE(key)->tp_name);
      goto error;
    }

    int ret;
    if (pyrna_enum_value_from_id(items, param, &ret, error_prefix) == -1) {
      goto error;
    }

    int index = ret;

    if (type_convert_sign) {
      if (type_size == 2) {
        union {
          signed short as_signed;
          ushort as_unsigned;
        } ret_convert;
        ret_convert.as_signed = (signed short)ret;
        index = (int)ret_convert.as_unsigned;
      }
      else if (type_size == 1) {
        union {
          signed char as_signed;
          uchar as_unsigned;
        } ret_convert;
        ret_convert.as_signed = (signed char)ret;
        index = (int)ret_convert.as_unsigned;
      }
      else {
        BLI_assert(0);
      }
    }
    BLI_assert(index < bitmap_size);
    BLI_BITMAP_ENABLE(bitmap, index);
  }

  return bitmap;

error:
  MEM_freeN(bitmap);
  return NULL;
}

/* 'value' _must_ be a set type, error check before calling. */
int pyrna_set_to_enum_bitfield(const EnumPropertyItem *items,
                               PyObject *value,
                               int *r_value,
                               const char *error_prefix)
{
  /* Set of enum items, concatenate all values with OR. */
  int ret, flag = 0;

  /* Set looping. */
  Py_ssize_t pos = 0;
  Py_ssize_t hash = 0;
  PyObject *key;

  *r_value = 0;

  while (_PySet_NextEntry(value, &pos, &key, &hash)) {
    const char *param = PyUnicode_AsUTF8(key);

    if (param == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s expected a string, not %.200s",
                   error_prefix,
                   Py_TYPE(key)->tp_name);
      return -1;
    }

    if (pyrna_enum_value_from_id(items, param, &ret, error_prefix) == -1) {
      return -1;
    }

    flag |= ret;
  }

  *r_value = flag;
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

  RNA_property_enum_items(BPY_context_get(), ptr, prop, &item, NULL, &free);

  if (item) {
    ret = pyrna_set_to_enum_bitfield(item, value, r_value, error_prefix);
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
    MEM_freeN((void *)item);
  }

  return ret;
}

PyObject *pyrna_enum_bitfield_to_py(const EnumPropertyItem *items, int value)
{
  PyObject *ret = PySet_New(NULL);
  const char *identifier[RNA_ENUM_BITFLAG_SIZE + 1];

  if (RNA_enum_bitflag_identifiers(items, value, identifier)) {
    PyObject *item;
    int index;
    for (index = 0; identifier[index]; index++) {
      item = PyUnicode_FromString(identifier[index]);
      PySet_Add(ret, item);
      Py_DECREF(item);
    }
  }

  return ret;
}

static PyObject *pyrna_enum_to_py(PointerRNA *ptr, PropertyRNA *prop, int val)
{
  PyObject *item, *ret = NULL;

  if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
    const char *identifier[RNA_ENUM_BITFLAG_SIZE + 1];

    ret = PySet_New(NULL);

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
      RNA_property_enum_items_ex(NULL, ptr, prop, true, &enum_item, NULL, &free_dummy);
      BLI_assert(!free_dummy);

      /* Do not print warning in case of DummyRNA_NULL_items,
       * this one will never match any value... */
      if (enum_item != DummyRNA_NULL_items) {
        const char *ptr_name = RNA_struct_name_get_alloc(ptr, NULL, 0, NULL);

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
        BLI_snprintf(error_str,
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
          MEM_freeN((void *)ptr_name);
        }
      }

      ret = PyUnicode_FromString("");
#if 0
      PyErr_Format(PyExc_AttributeError, "RNA Error: Current value \"%d\" matches no enum", val);
      ret = NULL;
#endif
    }
  }

  return ret;
}

PyObject *pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop)
{
  PyObject *ret;
  const int type = RNA_property_type(prop);

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
      /* Only file paths get special treatment, they may contain non utf-8 chars. */
      if (subtype == PROP_BYTESTRING) {
        ret = PyBytes_FromStringAndSize(buf, buf_len);
      }
      else if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
        ret = PyC_UnicodeFromByteAndSize(buf, buf_len);
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
        MEM_freeN((void *)buf);
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
      ret = NULL;
      break;
  }

  return ret;
}

/**
 * This function is used by operators and converting dicts into collections.
 * It takes keyword args and fills them with property values.
 */
int pyrna_pydict_to_props(PointerRNA *ptr,
                          PyObject *kw,
                          const bool all_args,
                          const char *error_prefix)
{
  int error_val = 0;
  int totkw;
  const char *arg_name = NULL;
  PyObject *item;

  totkw = kw ? PyDict_Size(kw) : 0;

  RNA_STRUCT_BEGIN (ptr, prop) {
    arg_name = RNA_property_identifier(prop);

    if (STREQ(arg_name, "rna_type")) {
      continue;
    }

    if (kw == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s: no keywords, expected \"%.200s\"",
                   error_prefix,
                   arg_name ? arg_name : "<UNKNOWN>");
      error_val = -1;
      break;
    }

    item = PyDict_GetItemString(kw, arg_name); /* Wont set an error. */

    if (item == NULL) {
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
      if (pyrna_py_to_prop(ptr, prop, NULL, item, error_prefix)) {
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
      if (RNA_struct_find_property(ptr, arg_name) == NULL) {
        break;
      }
      arg_name = NULL;
    }

    PyErr_Format(PyExc_TypeError,
                 "%.200s: keyword \"%.200s\" unrecognized",
                 error_prefix,
                 arg_name ? arg_name : "<UNKNOWN>");
    error_val = -1;
  }

  return error_val;
}

static PyObject *pyrna_func_to_py(const PointerRNA *ptr, FunctionRNA *func)
{
  BPy_FunctionRNA *pyfunc = (BPy_FunctionRNA *)PyObject_NEW(BPy_FunctionRNA, &pyrna_func_Type);
  pyfunc->ptr = *ptr;
  pyfunc->func = func;
  return (PyObject *)pyfunc;
}

static int pyrna_py_to_prop(
    PointerRNA *ptr, PropertyRNA *prop, void *data, PyObject *value, const char *error_prefix)
{
  /* XXX hard limits should be checked here. */
  const int type = RNA_property_type(prop);

  if (RNA_property_array_check(prop)) {
    /* Done getting the length. */
    if (pyrna_py_to_array(ptr, prop, data, value, error_prefix) == -1) {
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

        int param_i = (int)param;
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
        const int subtype = RNA_property_subtype(prop);
        const char *param;

        if (value == Py_None) {
          if ((RNA_property_flag(prop) & PROP_NEVER_NULL) == 0) {
            if (data) {
              if (RNA_property_flag(prop) & PROP_THICK_WRAP) {
                *(char *)data = 0;
              }
              else {
                *((char **)data) = (char *)NULL;
              }
            }
            else {
              RNA_property_string_set(ptr, prop, NULL);
            }
          }
          else {
            PyC_Err_Format_Prefix(PyExc_TypeError,
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

          if (param == NULL) {
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
            if (RNA_property_flag(prop) & PROP_THICK_WRAP) {
              BLI_strncpy((char *)data, (char *)param, RNA_property_string_maxlength(prop));
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
          PyObject *value_coerce = NULL;
          if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
            /* TODO, get size. */
            param = PyC_UnicodeAsByte(value, &value_coerce);
          }
          else {
            param = PyUnicode_AsUTF8(value);
          }
#else  /* USE_STRING_COERCE */
          param = PyUnicode_AsUTF8(value);
#endif /* USE_STRING_COERCE */

          if (param == NULL) {
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

          /* Same as bytes. */
          /* XXX, this is suspect, but needed for function calls,
           * need to see if there's a better way. */
          if (data) {
            if (RNA_property_flag(prop) & PROP_THICK_WRAP) {
              BLI_strncpy((char *)data, (char *)param, RNA_property_string_maxlength(prop));
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
        PyObject *value_new = NULL;

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
        if ((ptr_type == &RNA_AnyType) && (BPy_StructRNA_Check(value))) {
          const StructRNA *base_type = RNA_struct_base_child_of(
              ((const BPy_StructRNA *)value)->ptr.type, NULL);
          if (ELEM(base_type, &RNA_Operator, &RNA_Gizmo)) {
            value = PyObject_GetAttr(value, bpy_intern_str_properties);
            value_new = value;
          }
        }

        /* if property is an OperatorProperties/GizmoProperties pointer and value is a map,
         * forward back to pyrna_pydict_to_props */
        if (PyDict_Check(value)) {
          const StructRNA *base_type = RNA_struct_base_child_of(ptr_type, NULL);
          if (base_type == &RNA_OperatorProperties) {
            PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
            return pyrna_pydict_to_props(&opptr, value, false, error_prefix);
          }
          if (base_type == &RNA_GizmoProperties) {
            PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
            return pyrna_pydict_to_props(&opptr, value, false, error_prefix);
          }
        }

        /* Another exception, allow passing a collection as an RNA property. */
        if (Py_TYPE(value) == &pyrna_prop_collection_Type) { /* Ok to ignore idprop collections. */
          PointerRNA c_ptr;
          BPy_PropertyRNA *value_prop = (BPy_PropertyRNA *)value;
          if (RNA_property_collection_type_get(&value_prop->ptr, value_prop->prop, &c_ptr)) {
            value = pyrna_struct_CreatePyObject(&c_ptr);
            value_new = value;
          }
          else {
            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s collection has no type, "
                         "can't be used as a %.200s type",
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
          param = NULL;
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

          const ID *value_owner_id = ((BPy_StructRNA *)value)->ptr.owner_id;
          if (value_owner_id != NULL) {
            if ((flag & PROP_ID_SELF_CHECK) && (ptr->owner_id == value_owner_id)) {
              PyErr_Format(PyExc_TypeError,
                           "%.200s %.200s.%.200s ID type does not support assignment to itself",
                           error_prefix,
                           RNA_struct_identifier(ptr->type),
                           RNA_property_identifier(prop));
              Py_XDECREF(value_new);
              return -1;
            }

            if (value_owner_id->tag & LIB_TAG_TEMP_MAIN) {
              /* Allow passing temporary ID's to functions, but not attribute assignment. */
              if (ptr->type != &RNA_Function) {
                PyErr_Format(PyExc_TypeError,
                             "%.200s %.200s.%.200s ID type assignment is temporary, can't assign",
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
              if (param == NULL) {
                memset(data, 0, sizeof(PointerRNA));
              }
              else if (RNA_struct_is_a(param->ptr.type, ptr_type)) {
                *((PointerRNA *)data) = param->ptr;
              }
              else {
                raise_error = true;
              }
            }
            else {
              /* For function calls, we sometimes want to pass the 'ptr' directly,
               * but watch out that it remains valid!
               * We could possibly support this later if needed. */
              BLI_assert(value_new == NULL);
              if (param == NULL) {
                *((void **)data) = NULL;
              }
              else if (RNA_struct_is_a(param->ptr.type, ptr_type)) {
                *((PointerRNA **)data) = &param->ptr;
              }
              else {
                raise_error = true;
              }
            }
          }
          else if (param == NULL) {
            *((void **)data) = NULL;
          }
          else if (RNA_struct_is_a(param->ptr.type, ptr_type)) {
            *((void **)data) = param->ptr.data;
          }
          else {
            raise_error = true;
          }
        }
        else {
          /* Data == NULL, assign to RNA. */
          if ((param == NULL) || RNA_struct_is_a(param->ptr.type, ptr_type)) {
            ReportList reports;
            BKE_reports_init(&reports, RPT_STORE);
            RNA_property_pointer_set(
                ptr, prop, (param == NULL) ? PointerRNA_NULL : param->ptr, &reports);
            const int err = (BPy_reports_to_error(&reports, PyExc_RuntimeError, true));
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
            PointerRNA tmp;
            RNA_pointer_create(NULL, ptr_type, NULL, &tmp);
            PyErr_Format(PyExc_TypeError,
                         "%.200s %.200s.%.200s expected a %.200s type, not %.200s",
                         error_prefix,
                         RNA_struct_identifier(ptr->type),
                         RNA_property_identifier(prop),
                         RNA_struct_identifier(tmp.type),
                         RNA_struct_identifier(param->ptr.type));
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
        ListBase *lb;
        CollectionPointerLink *link;

        lb = (data) ? (ListBase *)data : NULL;

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

          if (item == NULL) {
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
            link = MEM_callocN(sizeof(CollectionPointerLink), "PyCollectionPointerLink");
            link->ptr = itemptr;
            BLI_addtail(lb, link);
          }
          else {
            RNA_property_collection_add(ptr, prop, &itemptr);
          }

          if (pyrna_pydict_to_props(
                  &itemptr, item, true, "Converting a Python list to an RNA collection") == -1) {
            PyObject *msg = PyC_ExceptionBuffer();
            const char *msg_char = PyUnicode_AsUTF8(msg);

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
  return pyrna_py_from_array_index(self, &self->ptr, self->prop, index);
}

static int pyrna_py_to_prop_array_index(BPy_PropertyArrayRNA *self, int index, PyObject *value)
{
  int ret = 0;
  PointerRNA *ptr = &self->ptr;
  PropertyRNA *prop = self->prop;

  const int totdim = RNA_property_array_dimension(ptr, prop, NULL);

  if (totdim > 1) {
    // char error_str[512];
    if (pyrna_py_to_array_index(
            &self->ptr, self->prop, self->arraydim, self->arrayoffset, index, value, "") == -1) {
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

  if (RNA_property_array_dimension(&self->ptr, self->prop, NULL) > 1) {
    return RNA_property_multi_array_length(&self->ptr, self->prop, self->arraydim);
  }

  return RNA_property_array_length(&self->ptr, self->prop);
}

static Py_ssize_t pyrna_prop_collection_length(BPy_PropertyRNA *self)
{
  PYRNA_PROP_CHECK_INT(self);

  return RNA_property_collection_length(&self->ptr, self->prop);
}

/* bool functions are for speed, so we can avoid getting the length
 * of 1000's of items in a linked list for eg. */
static int pyrna_prop_array_bool(BPy_PropertyRNA *self)
{
  PYRNA_PROP_CHECK_INT(self);

  return RNA_property_array_length(&self->ptr, self->prop) ? 1 : 0;
}

static int pyrna_prop_collection_bool(BPy_PropertyRNA *self)
{
  /* No callback defined, just iterate and find the nth item. */
  CollectionPropertyIterator iter;
  int test;

  PYRNA_PROP_CHECK_INT(self);

  RNA_property_collection_begin(&self->ptr, self->prop, &iter);
  test = iter.valid;
  RNA_property_collection_end(&iter);
  return test;
}

/* notice getting the length of the collection is avoided unless negative
 * index is used or to detect internal error with a valid index.
 * This is done for faster lookups. */
#define PYRNA_PROP_COLLECTION_ABS_INDEX(ret_err) \
  if (keynum < 0) { \
    keynum_abs += RNA_property_collection_length(&self->ptr, self->prop); \
    if (keynum_abs < 0) { \
      PyErr_Format(PyExc_IndexError, "bpy_prop_collection[%d]: out of range.", keynum); \
      return ret_err; \
    } \
  } \
  (void)0

/* Internal use only. */
static PyObject *pyrna_prop_collection_subscript_int(BPy_PropertyRNA *self, Py_ssize_t keynum)
{
  PointerRNA newptr;
  Py_ssize_t keynum_abs = keynum;

  PYRNA_PROP_CHECK_OBJ(self);

  PYRNA_PROP_COLLECTION_ABS_INDEX(NULL);

  if (RNA_property_collection_lookup_int(&self->ptr, self->prop, keynum_abs, &newptr)) {
    return pyrna_struct_CreatePyObject(&newptr);
  }

  const int len = RNA_property_collection_length(&self->ptr, self->prop);
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

  return NULL;
}

/* Values type must have been already checked. */
static int pyrna_prop_collection_ass_subscript_int(BPy_PropertyRNA *self,
                                                   Py_ssize_t keynum,
                                                   PyObject *value)
{
  Py_ssize_t keynum_abs = keynum;
  const PointerRNA *ptr = (value == Py_None) ? (&PointerRNA_NULL) : &((BPy_StructRNA *)value)->ptr;

  PYRNA_PROP_CHECK_INT(self);

  PYRNA_PROP_COLLECTION_ABS_INDEX(-1);

  if (RNA_property_collection_assign_int(&self->ptr, self->prop, keynum_abs, ptr) == 0) {
    const int len = RNA_property_collection_length(&self->ptr, self->prop);
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
                   "failed assignment (unknown reason)",
                   keynum);
    }
    return -1;
  }

  return 0;
}

static PyObject *pyrna_prop_array_subscript_int(BPy_PropertyArrayRNA *self, int keynum)
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
  return NULL;
}

static PyObject *pyrna_prop_collection_subscript_str(BPy_PropertyRNA *self, const char *keyname)
{
  PointerRNA newptr;

  PYRNA_PROP_CHECK_OBJ(self);

  if (RNA_property_collection_lookup_string(&self->ptr, self->prop, keyname, &newptr)) {
    return pyrna_struct_CreatePyObject(&newptr);
  }

  PyErr_Format(PyExc_KeyError, "bpy_prop_collection[key]: key \"%.200s\" not found", keyname);
  return NULL;
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
  if (self->ptr.type != &RNA_BlendData) {
    PyErr_Format(PyExc_KeyError,
                 "%s: is only valid for bpy.data collections, not %.200s",
                 err_prefix,
                 RNA_struct_identifier(self->ptr.type));
    return -1;
  }
  if ((keyname = PyUnicode_AsUTF8(PyTuple_GET_ITEM(key, 0))) == NULL) {
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
    lib = NULL;
  }
  else if (PyUnicode_Check(keylib)) {
    Main *bmain = self->ptr.data;
    const char *keylib_str = PyUnicode_AsUTF8(keylib);
    lib = BLI_findstring(&bmain->libraries, keylib_str, offsetof(Library, filepath));
    if (lib == NULL) {
      if (err_not_found) {
        PyErr_Format(PyExc_KeyError,
                     "%s: lib name '%.240s' "
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

  /* lib is either a valid pointer or NULL,
   * either way can do direct comparison with id.lib */

  RNA_PROP_BEGIN (&self->ptr, itemptr, self->prop) {
    ID *id = itemptr.data; /* Always an ID. */
    if (id->lib == lib && (STREQLEN(keyname, id->name + 2, sizeof(id->name) - 2))) {
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

  return NULL;
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
  RNA_property_collection_begin(&self->ptr, self->prop, &rna_macro_iter);
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
 * TODO - dimensions
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

  totdim = RNA_property_array_dimension(ptr, prop, NULL);

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
          values = PyMem_MALLOC(sizeof(float) * length);
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
          values = PyMem_MALLOC(sizeof(bool) * length);
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
          values = PyMem_MALLOC(sizeof(int) * length);
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
        BLI_assert(!"Invalid array type");

        PyErr_SetString(PyExc_TypeError, "not an array type");
        Py_DECREF(tuple);
        tuple = NULL;
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
      return NULL;
    }

    return pyrna_prop_collection_subscript_int(self, i);
  }
  if (PySlice_Check(key)) {
    PySliceObject *key_slice = (PySliceObject *)key;
    Py_ssize_t step = 1;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return NULL;
    }
    if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "bpy_prop_collection[slice]: slice steps not supported");
      return NULL;
    }
    if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
    }

    Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

    /* Avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
    if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) {
      return NULL;
    }
    if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop)) {
      return NULL;
    }

    if (start < 0 || stop < 0) {
      /* Only get the length for negative values. */
      const Py_ssize_t len = (Py_ssize_t)RNA_property_collection_length(&self->ptr, self->prop);
      if (start < 0) {
        start += len;
      }
      if (stop < 0) {
        stop += len;
      }
    }

    if (stop - start <= 0) {
      return PyList_New(0);
    }

    return pyrna_prop_collection_subscript_slice(self, start, stop);
  }
  if (PyTuple_Check(key)) {
    /* Special case, for ID datablocks we. */
    return pyrna_prop_collection_subscript_str_lib_pair(
        self, key, "bpy_prop_collection[id, lib]", true);
  }

  PyErr_Format(PyExc_TypeError,
               "bpy_prop_collection[key]: invalid key, "
               "must be a string or an int, not %.200s",
               Py_TYPE(key)->tp_name);
  return NULL;
}

/* generic check to see if a PyObject is compatible with a collection
 * -1 on failure, 0 on success, sets the error */
static int pyrna_prop_collection_type_check(BPy_PropertyRNA *self, PyObject *value)
{
  StructRNA *prop_srna;

  if (value == Py_None) {
    if (RNA_property_flag(self->prop) & PROP_NEVER_NULL) {
      PyErr_Format(PyExc_TypeError,
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
  if ((prop_srna = RNA_property_pointer_type(&self->ptr, self->prop))) {
    StructRNA *value_srna = ((BPy_StructRNA *)value)->ptr.type;
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

  PyErr_Format(PyExc_TypeError,
               "bpy_prop_collection[key] = value: internal error, "
               "failed to get the collection type");
  return -1;
}

/* note: currently this is a copy of 'pyrna_prop_collection_subscript' with
 * large blocks commented, we may support slice/key indices later */
static int pyrna_prop_collection_ass_subscript(BPy_PropertyRNA *self,
                                               PyObject *key,
                                               PyObject *value)
{
  PYRNA_PROP_CHECK_INT(self);

  /* Validate the assigned value. */
  if (value == NULL) {
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
#if 0 /* TODO, fake slice assignment. */
  else if (PySlice_Check(key)) {
    PySliceObject *key_slice = (PySliceObject *)key;
    Py_ssize_t step = 1;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return NULL;
    }
    else if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "bpy_prop_collection[slice]: slice steps not supported");
      return NULL;
    }
    else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
    }
    else {
      Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

      /* Avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
      if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) {
        return NULL;
      }
      if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop)) {
        return NULL;
      }

      if (start < 0 || stop < 0) {
        /* Only get the length for negative values. */
        Py_ssize_t len = (Py_ssize_t)RNA_property_collection_length(&self->ptr, self->prop);
        if (start < 0) {
          start += len;
        }
        if (stop < 0) {
          stop += len;
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
               "must be a string or an int, not %.200s",
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
      return NULL;
    }
    return pyrna_prop_array_subscript_int(self, i);
  }
  if (PySlice_Check(key)) {
    Py_ssize_t step = 1;
    PySliceObject *key_slice = (PySliceObject *)key;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return NULL;
    }
    if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "bpy_prop_array[slice]: slice steps not supported");
      return NULL;
    }
    if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      /* Note: no significant advantage with optimizing [:] slice as with collections,
       * but include here for consistency with collection slice func */
      const Py_ssize_t len = (Py_ssize_t)pyrna_prop_array_length(self);
      return pyrna_prop_array_subscript_slice(self, &self->ptr, self->prop, 0, len, len);
    }

    const int len = pyrna_prop_array_length(self);
    Py_ssize_t start, stop, slicelength;

    if (PySlice_GetIndicesEx(key, len, &start, &stop, &step, &slicelength) < 0) {
      return NULL;
    }

    if (slicelength <= 0) {
      return PyTuple_New(0);
    }

    return pyrna_prop_array_subscript_slice(self, &self->ptr, self->prop, start, stop, len);
  }

  PyErr_SetString(PyExc_AttributeError, "bpy_prop_array[key]: invalid key, key must be an int");
  return NULL;
}

/**
 * Helpers for #prop_subscript_ass_array_slice
 */

static PyObject *prop_subscript_ass_array_slice__as_seq_fast(PyObject *value, int length)
{
  PyObject *value_fast;
  if (!(value_fast = PySequence_Fast(value,
                                     "bpy_prop_array[slice] = value: "
                                     "element in assignment is not a sequence type"))) {
    return NULL;
  }
  if (PySequence_Fast_GET_SIZE(value_fast) != length) {
    Py_DECREF(value_fast);
    PyErr_SetString(PyExc_ValueError,
                    "bpy_prop_array[slice] = value: "
                    "re-sizing bpy_struct element in arrays isn't supported");

    return NULL;
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
      if (UNLIKELY(subvalue == NULL)) {
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
      if (UNLIKELY(subvalue == NULL)) {
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
      if (UNLIKELY(subvalue == NULL)) {
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
  PyObject **value_items;
  void *values_alloc = NULL;
  int ret = 0;

  if (value_orig == NULL) {
    PyErr_SetString(
        PyExc_TypeError,
        "bpy_prop_array[slice] = value: deleting with list types is not supported by bpy_struct");
    return -1;
  }

  if (!(value = PySequence_Fast(
            value_orig, "bpy_prop_array[slice] = value: assignment is not a sequence type"))) {
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

  value_items = PySequence_Fast_ITEMS(value);
  switch (RNA_property_type(prop)) {
    case PROP_FLOAT: {
      float values_stack[PYRNA_STACK_ARRAY];
      float *values = (length_flat > PYRNA_STACK_ARRAY) ?
                          (values_alloc = PyMem_MALLOC(sizeof(*values) * length_flat)) :
                          values_stack;
      if (start != 0 || stop != length) {
        /* Partial assignment? - need to get the array. */
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
      int *values = (length_flat > PYRNA_STACK_ARRAY) ?
                        (values_alloc = PyMem_MALLOC(sizeof(*values) * length_flat)) :
                        values_stack;
      if (start != 0 || stop != length) {
        /* Partial assignment? - need to get the array. */
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
      bool *values = (length_flat > PYRNA_STACK_ARRAY) ?
                         (values_alloc = PyMem_MALLOC(sizeof(bool) * length_flat)) :
                         values_stack;

      if (start != 0 || stop != length) {
        /* Partial assignment? - need to get the array. */
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
  int len;

  PYRNA_PROP_CHECK_INT((BPy_PropertyRNA *)self);

  len = pyrna_prop_array_length(self);

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
  // char *keyname = NULL; /* Not supported yet. */
  int ret = -1;

  PYRNA_PROP_CHECK_INT((BPy_PropertyRNA *)self);

  if (!RNA_property_editable_flag(&self->ptr, self->prop)) {
    PyErr_Format(PyExc_AttributeError,
                 "bpy_prop_collection: attribute \"%.200s\" from \"%.200s\" is read-only",
                 RNA_property_identifier(self->prop),
                 RNA_struct_identifier(self->ptr.type));
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
      ret = prop_subscript_ass_array_slice(
          &self->ptr, self->prop, self->arraydim, self->arrayoffset, start, stop, len, value);
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
      RNA_property_update(BPY_context_get(), &self->ptr, self->prop);
    }
  }

  return ret;
}

/* For slice only. */
static PyMappingMethods pyrna_prop_array_as_mapping = {
    (lenfunc)pyrna_prop_array_length,              /* mp_length */
    (binaryfunc)pyrna_prop_array_subscript,        /* mp_subscript */
    (objobjargproc)pyrna_prop_array_ass_subscript, /* mp_ass_subscript */
};

static PyMappingMethods pyrna_prop_collection_as_mapping = {
    (lenfunc)pyrna_prop_collection_length,              /* mp_length */
    (binaryfunc)pyrna_prop_collection_subscript,        /* mp_subscript */
    (objobjargproc)pyrna_prop_collection_ass_subscript, /* mp_ass_subscript */
};

/* Only for fast bool's, large structs, assign nb_bool on init. */
static PyNumberMethods pyrna_prop_array_as_number = {
    NULL,                           /* nb_add */
    NULL,                           /* nb_subtract */
    NULL,                           /* nb_multiply */
    NULL,                           /* nb_remainder */
    NULL,                           /* nb_divmod */
    NULL,                           /* nb_power */
    NULL,                           /* nb_negative */
    NULL,                           /* nb_positive */
    NULL,                           /* nb_absolute */
    (inquiry)pyrna_prop_array_bool, /* nb_bool */
};
static PyNumberMethods pyrna_prop_collection_as_number = {
    NULL,                                /* nb_add */
    NULL,                                /* nb_subtract */
    NULL,                                /* nb_multiply */
    NULL,                                /* nb_remainder */
    NULL,                                /* nb_divmod */
    NULL,                                /* nb_power */
    NULL,                                /* nb_negative */
    NULL,                                /* nb_positive */
    NULL,                                /* nb_absolute */
    (inquiry)pyrna_prop_collection_bool, /* nb_bool */
};

static int pyrna_prop_array_contains(BPy_PropertyRNA *self, PyObject *value)
{
  return pyrna_array_contains_py(&self->ptr, self->prop, value);
}

static int pyrna_prop_collection_contains(BPy_PropertyRNA *self, PyObject *key)
{
  PointerRNA newptr; /* Not used, just so RNA_property_collection_lookup_string runs. */

  if (PyTuple_Check(key)) {
    /* Special case, for ID data-blocks. */
    return pyrna_prop_collection_subscript_str_lib_pair_ptr(
        self, key, "(id, lib) in bpy_prop_collection", false, NULL);
  }

  /* Key in dict style check. */
  const char *keyname = PyUnicode_AsUTF8(key);

  if (keyname == NULL) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_collection.__contains__: expected a string or a tuple of strings");
    return -1;
  }

  if (RNA_property_collection_lookup_string(&self->ptr, self->prop, keyname, &newptr)) {
    return 1;
  }

  return 0;
}

static int pyrna_struct_contains(BPy_StructRNA *self, PyObject *value)
{
  IDProperty *group;
  const char *name = PyUnicode_AsUTF8(value);

  PYRNA_STRUCT_CHECK_INT(self);

  if (!name) {
    PyErr_SetString(PyExc_TypeError, "bpy_struct.__contains__: expected a string");
    return -1;
  }

  if (RNA_struct_idprops_check(self->ptr.type) == 0) {
    PyErr_SetString(PyExc_TypeError, "bpy_struct: this type doesn't support IDProperties");
    return -1;
  }

  group = RNA_struct_idprops(&self->ptr, 0);

  if (!group) {
    return 0;
  }

  return IDP_GetPropertyFromGroup(group, name) ? 1 : 0;
}

static PySequenceMethods pyrna_prop_array_as_sequence = {
    (lenfunc)pyrna_prop_array_length,
    NULL, /* sq_concat */
    NULL, /* sq_repeat */
    (ssizeargfunc)pyrna_prop_array_subscript_int,
    /* sq_item */ /* Only set this so PySequence_Check() returns True */
    NULL,         /* sq_slice */
    (ssizeobjargproc)prop_subscript_ass_array_int, /* sq_ass_item */
    NULL,                                          /* *was* sq_ass_slice */
    (objobjproc)pyrna_prop_array_contains,         /* sq_contains */
    (binaryfunc)NULL,                              /* sq_inplace_concat */
    (ssizeargfunc)NULL,                            /* sq_inplace_repeat */
};

static PySequenceMethods pyrna_prop_collection_as_sequence = {
    (lenfunc)pyrna_prop_collection_length,
    NULL, /* sq_concat */
    NULL, /* sq_repeat */
    (ssizeargfunc)pyrna_prop_collection_subscript_int,
    /* sq_item */                         /* Only set this so PySequence_Check() returns True */
    NULL,                                 /* *was* sq_slice */
    (ssizeobjargproc)                     /* pyrna_prop_collection_ass_subscript_int */
    NULL /* let mapping take this one */, /* sq_ass_item */
    NULL,                                 /* *was* sq_ass_slice */
    (objobjproc)pyrna_prop_collection_contains, /* sq_contains */
    (binaryfunc)NULL,                           /* sq_inplace_concat */
    (ssizeargfunc)NULL,                         /* sq_inplace_repeat */
};

static PySequenceMethods pyrna_struct_as_sequence = {
    NULL, /* Can't set the len otherwise it can evaluate as false */
    NULL, /* sq_concat */
    NULL, /* sq_repeat */
    NULL,
    /* sq_item */                      /* Only set this so PySequence_Check() returns True */
    NULL,                              /* *was* sq_slice */
    NULL,                              /* sq_ass_item */
    NULL,                              /* *was* sq_ass_slice */
    (objobjproc)pyrna_struct_contains, /* sq_contains */
    (binaryfunc)NULL,                  /* sq_inplace_concat */
    (ssizeargfunc)NULL,                /* sq_inplace_repeat */
};

static PyObject *pyrna_struct_subscript(BPy_StructRNA *self, PyObject *key)
{
  /* Mostly copied from BPy_IDGroup_Map_GetItem. */
  IDProperty *group, *idprop;
  const char *name = PyUnicode_AsUTF8(key);

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (RNA_struct_idprops_check(self->ptr.type) == 0) {
    PyErr_SetString(PyExc_TypeError, "this type doesn't support IDProperties");
    return NULL;
  }

  if (name == NULL) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_struct[key]: only strings are allowed as keys of ID properties");
    return NULL;
  }

  group = RNA_struct_idprops(&self->ptr, 0);

  if (group == NULL) {
    PyErr_Format(PyExc_KeyError, "bpy_struct[key]: key \"%s\" not found", name);
    return NULL;
  }

  idprop = IDP_GetPropertyFromGroup(group, name);

  if (idprop == NULL) {
    PyErr_Format(PyExc_KeyError, "bpy_struct[key]: key \"%s\" not found", name);
    return NULL;
  }

  return BPy_IDGroup_WrapData(self->ptr.owner_id, idprop, group);
}

static int pyrna_struct_ass_subscript(BPy_StructRNA *self, PyObject *key, PyObject *value)
{
  IDProperty *group;

  PYRNA_STRUCT_CHECK_INT(self);

  group = RNA_struct_idprops(&self->ptr, 1);

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, key)) {
    return -1;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (group == NULL) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_struct[key] = val: id properties not supported for this type");
    return -1;
  }

  if (value && BPy_StructRNA_Check(value)) {
    BPy_StructRNA *val = (BPy_StructRNA *)value;
    if (val && self->ptr.type && val->ptr.type) {
      if (!RNA_struct_idprops_datablock_allowed(self->ptr.type) &&
          RNA_struct_idprops_contains_datablock(val->ptr.type)) {
        PyErr_SetString(
            PyExc_TypeError,
            "bpy_struct[key] = val: datablock id properties not supported for this type");
        return -1;
      }
    }
  }

  return BPy_Wrap_SetMapItem(group, key, value);
}

static PyMappingMethods pyrna_struct_as_mapping = {
    (lenfunc)NULL,                             /* mp_length */
    (binaryfunc)pyrna_struct_subscript,        /* mp_subscript */
    (objobjargproc)pyrna_struct_ass_subscript, /* mp_ass_subscript */
};

PyDoc_STRVAR(pyrna_struct_keys_doc,
             ".. method:: keys()\n"
             "\n"
             "   Returns the keys of this objects custom properties (matches Python's\n"
             "   dictionary function of the same name).\n"
             "\n"
             "   :return: custom property keys.\n"
             "   :rtype: list of strings\n"
             "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_keys(BPy_PropertyRNA *self)
{
  IDProperty *group;

  if (RNA_struct_idprops_check(self->ptr.type) == 0) {
    PyErr_SetString(PyExc_TypeError, "bpy_struct.keys(): this type doesn't support IDProperties");
    return NULL;
  }

  group = RNA_struct_idprops(&self->ptr, 0);

  if (group == NULL) {
    return PyList_New(0);
  }

  return BPy_Wrap_GetKeys(group);
}

PyDoc_STRVAR(pyrna_struct_items_doc,
             ".. method:: items()\n"
             "\n"
             "   Returns the items of this objects custom properties (matches Python's\n"
             "   dictionary function of the same name).\n"
             "\n"
             "   :return: custom property key, value pairs.\n"
             "   :rtype: list of key, value tuples\n"
             "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_items(BPy_PropertyRNA *self)
{
  IDProperty *group;

  if (RNA_struct_idprops_check(self->ptr.type) == 0) {
    PyErr_SetString(PyExc_TypeError, "bpy_struct.items(): this type doesn't support IDProperties");
    return NULL;
  }

  group = RNA_struct_idprops(&self->ptr, 0);

  if (group == NULL) {
    return PyList_New(0);
  }

  return BPy_Wrap_GetItems(self->ptr.owner_id, group);
}

PyDoc_STRVAR(pyrna_struct_values_doc,
             ".. method:: values()\n"
             "\n"
             "   Returns the values of this objects custom properties (matches Python's\n"
             "   dictionary function of the same name).\n"
             "\n"
             "   :return: custom property values.\n"
             "   :rtype: list\n"
             "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_values(BPy_PropertyRNA *self)
{
  IDProperty *group;

  if (RNA_struct_idprops_check(self->ptr.type) == 0) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_struct.values(): this type doesn't support IDProperties");
    return NULL;
  }

  group = RNA_struct_idprops(&self->ptr, 0);

  if (group == NULL) {
    return PyList_New(0);
  }

  return BPy_Wrap_GetValues(self->ptr.owner_id, group);
}

PyDoc_STRVAR(pyrna_struct_is_property_set_doc,
             ".. method:: is_property_set(property, ghost=True)\n"
             "\n"
             "   Check if a property is set, use for testing operator properties.\n"
             "\n"
             "   :arg ghost: Used for operators that re-run with previous settings.\n"
             "      In this case the property is not marked as set,\n"
             "      yet the value from the previous execution is used.\n"
             "\n"
             "      In rare cases you may want to set this option to false.\n"
             "\n"
             "   :type ghost: boolean\n"
             "   :return: True when the property has been set.\n"
             "   :rtype: boolean\n");
static PyObject *pyrna_struct_is_property_set(BPy_StructRNA *self, PyObject *args, PyObject *kw)
{
  PropertyRNA *prop;
  const char *name;
  bool use_ghost = true;

  PYRNA_STRUCT_CHECK_OBJ(self);

  static const char *_keywords[] = {"", "ghost", NULL};
  static _PyArg_Parser _parser = {"s|$O&:is_property_set", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &name, PyC_ParseBool, &use_ghost)) {
    return NULL;
  }

  if ((prop = RNA_struct_find_property(&self->ptr, name)) == NULL) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.is_property_set(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr.type),
                 name);
    return NULL;
  }

  return PyBool_FromLong(RNA_property_is_set_ex(&self->ptr, prop, use_ghost));
}

PyDoc_STRVAR(pyrna_struct_property_unset_doc,
             ".. method:: property_unset(property)\n"
             "\n"
             "   Unset a property, will use default value afterward.\n");
static PyObject *pyrna_struct_property_unset(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s:property_unset", &name)) {
    return NULL;
  }

  if ((prop = RNA_struct_find_property(&self->ptr, name)) == NULL) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.property_unset(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr.type),
                 name);
    return NULL;
  }

  RNA_property_unset(&self->ptr, prop);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(pyrna_struct_is_property_hidden_doc,
             ".. method:: is_property_hidden(property)\n"
             "\n"
             "   Check if a property is hidden.\n"
             "\n"
             "   :return: True when the property is hidden.\n"
             "   :rtype: boolean\n");
static PyObject *pyrna_struct_is_property_hidden(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s:is_property_hidden", &name)) {
    return NULL;
  }

  if ((prop = RNA_struct_find_property(&self->ptr, name)) == NULL) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.is_property_hidden(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr.type),
                 name);
    return NULL;
  }

  return PyBool_FromLong(RNA_property_flag(prop) & PROP_HIDDEN);
}

PyDoc_STRVAR(pyrna_struct_is_property_readonly_doc,
             ".. method:: is_property_readonly(property)\n"
             "\n"
             "   Check if a property is readonly.\n"
             "\n"
             "   :return: True when the property is readonly (not writable).\n"
             "   :rtype: boolean\n");
static PyObject *pyrna_struct_is_property_readonly(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s:is_property_readonly", &name)) {
    return NULL;
  }

  if ((prop = RNA_struct_find_property(&self->ptr, name)) == NULL) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.is_property_readonly(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr.type),
                 name);
    return NULL;
  }

  return PyBool_FromLong(!RNA_property_editable(&self->ptr, prop));
}

PyDoc_STRVAR(pyrna_struct_is_property_overridable_library_doc,
             ".. method:: is_property_overridable_library(property)\n"
             "\n"
             "   Check if a property is overridable.\n"
             "\n"
             "   :return: True when the property is overridable.\n"
             "   :rtype: boolean\n");
static PyObject *pyrna_struct_is_property_overridable_library(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s:is_property_overridable_library", &name)) {
    return NULL;
  }

  if ((prop = RNA_struct_find_property(&self->ptr, name)) == NULL) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.is_property_overridable_library(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr.type),
                 name);
    return NULL;
  }

  return PyBool_FromLong((long)RNA_property_overridable_get(&self->ptr, prop));
}

PyDoc_STRVAR(pyrna_struct_property_overridable_library_set_doc,
             ".. method:: property_overridable_library_set(property, overridable)\n"
             "\n"
             "   Define a property as overridable or not (only for custom properties!).\n"
             "\n"
             "   :return: True when the overridable status of the property was successfully set.\n"
             "   :rtype: boolean\n");
static PyObject *pyrna_struct_property_overridable_library_set(BPy_StructRNA *self, PyObject *args)
{
  PropertyRNA *prop;
  const char *name;
  int is_overridable;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "sp:property_overridable_library_set", &name, &is_overridable)) {
    return NULL;
  }

  if ((prop = RNA_struct_find_property(&self->ptr, name)) == NULL) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s.property_overridable_library_set(\"%.200s\") not found",
                 RNA_struct_identifier(self->ptr.type),
                 name);
    return NULL;
  }

  return PyBool_FromLong(
      (long)RNA_property_overridable_library_set(&self->ptr, prop, (bool)is_overridable));
}

PyDoc_STRVAR(pyrna_struct_path_resolve_doc,
             ".. method:: path_resolve(path, coerce=True)\n"
             "\n"
             "   Returns the property from the path, raise an exception when not found.\n"
             "\n"
             "   :arg path: path which this property resolves.\n"
             "   :type path: string\n"
             "   :arg coerce: optional argument, when True, the property will be converted\n"
             "      into its Python representation.\n"
             "   :type coerce: boolean\n");
static PyObject *pyrna_struct_path_resolve(BPy_StructRNA *self, PyObject *args)
{
  const char *path;
  PyObject *coerce = Py_True;
  PointerRNA r_ptr;
  PropertyRNA *r_prop;
  int index = -1;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|O!:path_resolve", &path, &PyBool_Type, &coerce)) {
    return NULL;
  }

  if (RNA_path_resolve_full(&self->ptr, path, &r_ptr, &r_prop, &index)) {
    if (r_prop) {
      if (index != -1) {
        if (index >= RNA_property_array_length(&r_ptr, r_prop) || index < 0) {
          PyErr_Format(PyExc_IndexError,
                       "%.200s.path_resolve(\"%.200s\") index out of range",
                       RNA_struct_identifier(self->ptr.type),
                       path);
          return NULL;
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
               RNA_struct_identifier(self->ptr.type),
               path);
  return NULL;
}

PyDoc_STRVAR(pyrna_struct_path_from_id_doc,
             ".. method:: path_from_id(property=\"\")\n"
             "\n"
             "   Returns the data path from the ID to this object (string).\n"
             "\n"
             "   :arg property: Optional property name which can be used if the path is\n"
             "      to a property of this object.\n"
             "   :type property: string\n"
             "   :return: The path from :class:`bpy.types.bpy_struct.id_data`\n"
             "      to this struct and property (when given).\n"
             "   :rtype: str\n");
static PyObject *pyrna_struct_path_from_id(BPy_StructRNA *self, PyObject *args)
{
  const char *name = NULL;
  const char *path;
  PropertyRNA *prop;
  PyObject *ret;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|s:path_from_id", &name)) {
    return NULL;
  }

  if (name) {
    prop = RNA_struct_find_property(&self->ptr, name);
    if (prop == NULL) {
      PyErr_Format(PyExc_AttributeError,
                   "%.200s.path_from_id(\"%.200s\") not found",
                   RNA_struct_identifier(self->ptr.type),
                   name);
      return NULL;
    }

    path = RNA_path_from_ID_to_property(&self->ptr, prop);
  }
  else {
    path = RNA_path_from_ID_to_struct(&self->ptr);
  }

  if (path == NULL) {
    if (name) {
      PyErr_Format(PyExc_ValueError,
                   "%.200s.path_from_id(\"%s\") found, but does not support path creation",
                   RNA_struct_identifier(self->ptr.type),
                   name);
    }
    else {
      PyErr_Format(PyExc_ValueError,
                   "%.200s.path_from_id() does not support path creation for this type",
                   RNA_struct_identifier(self->ptr.type));
    }
    return NULL;
  }

  ret = PyUnicode_FromString(path);
  MEM_freeN((void *)path);

  return ret;
}

PyDoc_STRVAR(pyrna_prop_path_from_id_doc,
             ".. method:: path_from_id()\n"
             "\n"
             "   Returns the data path from the ID to this property (string).\n"
             "\n"
             "   :return: The path from :class:`bpy.types.bpy_struct.id_data` to this property.\n"
             "   :rtype: str\n");
static PyObject *pyrna_prop_path_from_id(BPy_PropertyRNA *self)
{
  const char *path;
  PropertyRNA *prop = self->prop;
  PyObject *ret;

  path = RNA_path_from_ID_to_property(&self->ptr, self->prop);

  if (path == NULL) {
    PyErr_Format(PyExc_ValueError,
                 "%.200s.%.200s.path_from_id() does not support path creation for this type",
                 RNA_struct_identifier(self->ptr.type),
                 RNA_property_identifier(prop));
    return NULL;
  }

  ret = PyUnicode_FromString(path);
  MEM_freeN((void *)path);

  return ret;
}

PyDoc_STRVAR(pyrna_prop_as_bytes_doc,
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
                 RNA_struct_identifier(self->ptr.type),
                 RNA_property_identifier(self->prop));
    return NULL;
  }

  PyObject *ret;
  char buf_fixed[256], *buf;
  int buf_len;

  buf = RNA_property_string_get_alloc(
      &self->ptr, self->prop, buf_fixed, sizeof(buf_fixed), &buf_len);

  ret = PyBytes_FromStringAndSize(buf, buf_len);

  if (buf_fixed != buf) {
    MEM_freeN(buf);
  }

  return ret;
}

PyDoc_STRVAR(pyrna_prop_update_doc,
             ".. method:: update()\n"
             "\n"
             "   Execute the properties update callback.\n"
             "\n"
             "   .. note::\n"
             "      This is called when assigning a property,\n"
             "      however in rare cases it's useful to call explicitly.\n");
static PyObject *pyrna_prop_update(BPy_PropertyRNA *self)
{
  RNA_property_update(BPY_context_get(), &self->ptr, self->prop);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(pyrna_struct_type_recast_doc,
             ".. method:: type_recast()\n"
             "\n"
             "   Return a new instance, this is needed because types\n"
             "   such as textures can be changed at runtime.\n"
             "\n"
             "   :return: a new instance of this object with the type initialized again.\n"
             "   :rtype: subclass of :class:`bpy.types.bpy_struct`\n");
static PyObject *pyrna_struct_type_recast(BPy_StructRNA *self)
{
  PointerRNA r_ptr;

  PYRNA_STRUCT_CHECK_OBJ(self);

  RNA_pointer_recast(&self->ptr, &r_ptr);
  return pyrna_struct_CreatePyObject(&r_ptr);
}

/**
 * \note Return value is borrowed, caller must #Py_INCREF.
 */
static PyObject *pyrna_struct_bl_rna_find_subclass_recursive(PyObject *cls, const char *id)
{
  PyObject *ret_test = NULL;
  PyObject *subclasses = ((PyTypeObject *)cls)->tp_subclasses;
  if (subclasses) {
    /* Unfortunately we can't use the dict key because Python class names
     * don't match the bl_idname used internally. */
    BLI_assert(PyDict_CheckExact(subclasses));
    PyObject *key = NULL;
    Py_ssize_t pos = 0;
    PyObject *value = NULL;
    while (PyDict_Next(subclasses, &pos, &key, &value)) {
      BLI_assert(PyWeakref_CheckRef(value));
      PyObject *subcls = PyWeakref_GET_OBJECT(value);
      if (subcls != Py_None) {
        BPy_StructRNA *py_srna = (BPy_StructRNA *)PyDict_GetItem(((PyTypeObject *)subcls)->tp_dict,
                                                                 bpy_intern_str_bl_rna);
        if (py_srna) {
          StructRNA *srna = py_srna->ptr.data;
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

PyDoc_STRVAR(pyrna_struct_bl_rna_get_subclass_py_doc,
             ".. classmethod:: bl_rna_get_subclass_py(id, default=None)\n"
             "\n"
             "   :arg id: The RNA type identifier.\n"
             "   :type id: string\n"
             "   :return: The class or default when not found.\n"
             "   :rtype: type\n");
static PyObject *pyrna_struct_bl_rna_get_subclass_py(PyObject *cls, PyObject *args)
{
  char *id;
  PyObject *ret_default = Py_None;

  if (!PyArg_ParseTuple(args, "s|O:bl_rna_get_subclass_py", &id, &ret_default)) {
    return NULL;
  }
  PyObject *ret = pyrna_struct_bl_rna_find_subclass_recursive(cls, id);
  if (ret == NULL) {
    ret = ret_default;
  }
  return Py_INCREF_RET(ret);
}

PyDoc_STRVAR(pyrna_struct_bl_rna_get_subclass_doc,
             ".. classmethod:: bl_rna_get_subclass(id, default=None)\n"
             "\n"
             "   :arg id: The RNA type identifier.\n"
             "   :type id: string\n"
             "   :return: The RNA type or default when not found.\n"
             "   :rtype: :class:`bpy.types.Struct` subclass\n");
static PyObject *pyrna_struct_bl_rna_get_subclass(PyObject *cls, PyObject *args)
{
  char *id;
  PyObject *ret_default = Py_None;

  if (!PyArg_ParseTuple(args, "s|O:bl_rna_get_subclass", &id, &ret_default)) {
    return NULL;
  }

  const BPy_StructRNA *py_srna = (BPy_StructRNA *)PyDict_GetItem(((PyTypeObject *)cls)->tp_dict,
                                                                 bpy_intern_str_bl_rna);
  if (py_srna == NULL) {
    PyErr_SetString(PyExc_ValueError, "Not a registered class");
    return NULL;
  }
  const StructRNA *srna_base = py_srna->ptr.data;

  PointerRNA ptr;
  if (srna_base == &RNA_Node) {
    bNodeType *nt = nodeTypeFind(id);
    if (nt) {
      RNA_pointer_create(NULL, &RNA_Struct, nt->rna_ext.srna, &ptr);
      return pyrna_struct_CreatePyObject(&ptr);
    }
  }
  else {
    /* TODO, panels, menus etc. */
    PyErr_Format(
        PyExc_ValueError, "Class type \"%.200s\" not supported", RNA_struct_identifier(srna_base));
    return NULL;
  }

  return Py_INCREF_RET(ret_default);
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

  dict_ptr = _PyObject_GetDictPtr((PyObject *)self);

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

      if (RNA_property_collection_type_get(&self_prop->ptr, self_prop->prop, &r_ptr)) {
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

  /* For looping over attrs and funcs. */
  PointerRNA tptr;
  PropertyRNA *iterprop;

  {
    RNA_pointer_create(NULL, &RNA_Struct, ptr->type, &tptr);
    iterprop = RNA_struct_find_property(&tptr, "functions");

    RNA_PROP_BEGIN (&tptr, itemptr, iterprop) {
      FunctionRNA *func = itemptr.data;
      if (RNA_function_defined(func)) {
        idname = RNA_function_identifier(itemptr.data);
        PyList_APPEND(list, PyUnicode_FromString(idname));
      }
    }
    RNA_PROP_END;
  }

  {
    /*
     * Collect RNA attributes
     */
    char name[256], *nameptr;
    int namelen;

    iterprop = RNA_struct_iterator_property(ptr->type);

    RNA_PROP_BEGIN (ptr, itemptr, iterprop) {
      nameptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &namelen);

      if (nameptr) {
        PyList_APPEND(list, PyUnicode_FromStringAndSize(nameptr, namelen));

        if (name != nameptr) {
          MEM_freeN(nameptr);
        }
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

  pyrna_dir_members_rna(ret, &self->ptr);

  if (self->ptr.type == &RNA_Context) {
    ListBase lb = CTX_data_dir_get(self->ptr.data);
    LinkData *link;

    for (link = lb.first; link; link = link->next) {
      PyList_APPEND(ret, PyUnicode_FromString(link->data));
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

/* ---------------getattr-------------------------------------------- */
static PyObject *pyrna_struct_getattro(BPy_StructRNA *self, PyObject *pyname)
{
  const char *name = PyUnicode_AsUTF8(pyname);
  PyObject *ret;
  PropertyRNA *prop;
  FunctionRNA *func;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (name == NULL) {
    PyErr_SetString(PyExc_AttributeError, "bpy_struct: __getattr__ must be a string");
    ret = NULL;
  }
  else if (
      /* RNA can't start with a "_", so for __dict__ and similar we can skip using RNA lookups. */
      name[0] == '_') {
    /* Annoying exception, maybe we need to have different types for this... */
    if (STR_ELEM(name, "__getitem__", "__setitem__") &&
        !RNA_struct_idprops_check(self->ptr.type)) {
      PyErr_SetString(PyExc_AttributeError, "bpy_struct: no __getitem__ support for this type");
      ret = NULL;
    }
    else {
      ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
    }
  }
  else if ((prop = RNA_struct_find_property(&self->ptr, name))) {
    ret = pyrna_prop_to_py(&self->ptr, prop);
  }
  /* RNA function only if callback is declared (no optional functions). */
  else if ((func = RNA_struct_find_function(self->ptr.type, name)) && RNA_function_defined(func)) {
    ret = pyrna_func_to_py(&self->ptr, func);
  }
  else if (self->ptr.type == &RNA_Context) {
    bContext *C = self->ptr.data;
    if (C == NULL) {
      PyErr_Format(PyExc_AttributeError,
                   "bpy_struct: Context is 'NULL', can't get \"%.200s\" from context",
                   name);
      ret = NULL;
    }
    else {
      PointerRNA newptr;
      ListBase newlb;
      short newtype;

      const eContextResult done = CTX_data_get(C, name, &newptr, &newlb, &newtype);

      if (done == CTX_RESULT_OK) {
        switch (newtype) {
          case CTX_DATA_TYPE_POINTER:
            if (newptr.data == NULL) {
              ret = Py_None;
              Py_INCREF(ret);
            }
            else {
              ret = pyrna_struct_CreatePyObject(&newptr);
            }
            break;
          case CTX_DATA_TYPE_COLLECTION: {
            CollectionPointerLink *link;

            ret = PyList_New(0);

            for (link = newlb.first; link; link = link->next) {
              PyList_APPEND(ret, pyrna_struct_CreatePyObject(&link->ptr));
            }
            break;
          }
          default:
            /* Should never happen. */
            BLI_assert(!"Invalid context type");

            PyErr_Format(PyExc_AttributeError,
                         "bpy_struct: Context type invalid %d, can't get \"%.200s\" from context",
                         newtype,
                         name);
            ret = NULL;
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

      BLI_freelistN(&newlb);
    }
  }
  else {
#if 0
    PyErr_Format(PyExc_AttributeError, "bpy_struct: attribute \"%.200s\" not found", name);
    ret = NULL;
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
  if (UNLIKELY(dict == NULL)) {
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
  if ((ret == NULL)  /* || BPy_PropDeferred_CheckTypeExact(ret) */ ) {
    StructRNA *srna = srna_from_self(cls, "StructRNA.__getattr__");
    if (srna) {
      PropertyRNA *prop = RNA_struct_type_find_property(srna, PyUnicode_AsUTF8(attr));
      if (prop) {
        PointerRNA tptr;
        PyErr_Clear(); /* Clear error from tp_getattro. */
        RNA_pointer_create(NULL, &RNA_Property, prop, &tptr);
        ret = pyrna_struct_CreatePyObject(&tptr);
      }
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
      (is_deferred_prop || RNA_struct_type_find_property(srna, attr_str))) {
    PyErr_Format(PyExc_AttributeError,
                 "pyrna_struct_meta_idprop_setattro() "
                 "can't set in readonly state '%.200s.%S'",
                 ((PyTypeObject *)cls)->tp_name,
                 attr);
    return -1;
  }

  if (srna == NULL) {
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

  /* Fallback to standard py, delattr/setattr. */
  return PyType_Type.tp_setattro(cls, attr, value);
}

static int pyrna_struct_setattro(BPy_StructRNA *self, PyObject *pyname, PyObject *value)
{
  const char *name = PyUnicode_AsUTF8(pyname);
  PropertyRNA *prop = NULL;

  PYRNA_STRUCT_CHECK_INT(self);

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, pyname)) {
    return -1;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (name == NULL) {
    PyErr_SetString(PyExc_AttributeError, "bpy_struct: __setattr__ must be a string");
    return -1;
  }
  if (name[0] != '_' && (prop = RNA_struct_find_property(&self->ptr, name))) {
    if (!RNA_property_editable_flag(&self->ptr, prop)) {
      PyErr_Format(PyExc_AttributeError,
                   "bpy_struct: attribute \"%.200s\" from \"%.200s\" is read-only",
                   RNA_property_identifier(prop),
                   RNA_struct_identifier(self->ptr.type));
      return -1;
    }
  }
  else if (self->ptr.type == &RNA_Context) {
    /* Code just raises correct error, context prop's can't be set,
     * unless it's a part of the py class. */
    bContext *C = self->ptr.data;
    if (C == NULL) {
      PyErr_Format(PyExc_AttributeError,
                   "bpy_struct: Context is 'NULL', can't set \"%.200s\" from context",
                   name);
      return -1;
    }

    PointerRNA newptr;
    ListBase newlb;
    short newtype;

    const eContextResult done = CTX_data_get(C, name, &newptr, &newlb, &newtype);

    if (done == CTX_RESULT_OK) {
      PyErr_Format(
          PyExc_AttributeError, "bpy_struct: Context property \"%.200s\" is read-only", name);
      BLI_freelistN(&newlb);
      return -1;
    }

    BLI_freelistN(&newlb);
  }

  /* pyrna_py_to_prop sets its own exceptions */
  if (prop) {
    if (value == NULL) {
      PyErr_SetString(PyExc_AttributeError, "bpy_struct: del not supported");
      return -1;
    }
    return pyrna_py_to_prop(&self->ptr, prop, NULL, value, "bpy_struct: item.attr = val:");
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
    if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
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

  if (name == NULL) {
    PyErr_SetString(PyExc_AttributeError, "bpy_prop_collection: __getattr__ must be a string");
    return NULL;
  }
  if (name[0] != '_') {
    PyObject *ret;
    PropertyRNA *prop;
    FunctionRNA *func;

    PointerRNA r_ptr;
    if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
      if ((prop = RNA_struct_find_property(&r_ptr, name))) {
        ret = pyrna_prop_to_py(&r_ptr, prop);

        return ret;
      }
      if ((func = RNA_struct_find_function(r_ptr.type, name))) {
        PyObject *self_collection = pyrna_struct_CreatePyObject(&r_ptr);
        ret = pyrna_func_to_py(&((BPy_DummyPointerRNA *)self_collection)->ptr, func);
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
     * so as to support `bpy.data.library.load()` */

    PyObject *ret = PyObject_GenericGetAttr((PyObject *)self, pyname);

    if (ret == NULL && name[0] != '_') { /* Avoid inheriting `__call__` and similar. */
      /* Since this is least common case, handle it last. */
      PointerRNA r_ptr;
      if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
        PyObject *cls;

        PyObject *error_type, *error_value, *error_traceback;
        PyErr_Fetch(&error_type, &error_value, &error_traceback);
        PyErr_Clear();

        cls = pyrna_struct_Subtype(&r_ptr);
        ret = PyObject_GenericGetAttr(cls, pyname);
        Py_DECREF(cls);

        /* Restore the original error. */
        if (ret == NULL) {
          PyErr_Restore(error_type, error_value, error_traceback);
        }
        else {
          if (Py_TYPE(ret) == &PyMethodDescr_Type) {
            PyMethodDef *m = ((PyMethodDescrObject *)ret)->d_method;
            /* TODO: #METH_CLASS */
            if (m->ml_flags & METH_STATIC) {
              /* Keep 'ret' as-is. */
            }
            else {
              Py_DECREF(ret);
              ret = PyCMethod_New(m, (PyObject *)self, NULL, NULL);
            }
          }
        }
      }
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
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, pyname)) {
    return -1;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (name == NULL) {
    PyErr_SetString(PyExc_AttributeError, "bpy_prop: __setattr__ must be a string");
    return -1;
  }
  if (value == NULL) {
    PyErr_SetString(PyExc_AttributeError, "bpy_prop: del not supported");
    return -1;
  }
  if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
    if ((prop = RNA_struct_find_property(&r_ptr, name))) {
      /* pyrna_py_to_prop sets its own exceptions. */
      return pyrna_py_to_prop(&r_ptr, prop, NULL, value, "BPy_PropertyRNA - Attribute (setattr):");
    }
  }

  PyErr_Format(PyExc_AttributeError, "bpy_prop_collection: attribute \"%.200s\" not found", name);
  return -1;
}

/**
 * Odd case, we need to be able return a Python method from a #PyTypeObject.tp_getset.
 */
static PyObject *pyrna_prop_collection_idprop_add(BPy_PropertyRNA *self)
{
  PointerRNA r_ptr;

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
    return NULL;
  }
#endif /* USE_PEDANTIC_WRITE */

  RNA_property_collection_add(&self->ptr, self->prop, &r_ptr);
  if (!r_ptr.data) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_collection.add(): not supported for this collection");
    return NULL;
  }

  return pyrna_struct_CreatePyObject(&r_ptr);
}

static PyObject *pyrna_prop_collection_idprop_remove(BPy_PropertyRNA *self, PyObject *value)
{
  const int key = PyLong_AsLong(value);

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
    return NULL;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (key == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.remove(): expected one int argument");
    return NULL;
  }

  if (!RNA_property_collection_remove(&self->ptr, self->prop, key)) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_collection.remove() not supported for this collection");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *pyrna_prop_collection_idprop_clear(BPy_PropertyRNA *self)
{
#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
    return NULL;
  }
#endif /* USE_PEDANTIC_WRITE */

  RNA_property_collection_clear(&self->ptr, self->prop);

  Py_RETURN_NONE;
}

static PyObject *pyrna_prop_collection_idprop_move(BPy_PropertyRNA *self, PyObject *args)
{
  int key = 0, pos = 0;

#ifdef USE_PEDANTIC_WRITE
  if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
    return NULL;
  }
#endif /* USE_PEDANTIC_WRITE */

  if (!PyArg_ParseTuple(args, "ii", &key, &pos)) {
    PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.move(): expected two ints as arguments");
    return NULL;
  }

  if (!RNA_property_collection_move(&self->ptr, self->prop, key, pos)) {
    PyErr_SetString(PyExc_TypeError,
                    "bpy_prop_collection.move() not supported for this collection");
    return NULL;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(pyrna_struct_get_id_data_doc,
             "The :class:`bpy.types.ID` object this datablock is from or None, (not available for "
             "all data types)");
static PyObject *pyrna_struct_get_id_data(BPy_DummyPointerRNA *self)
{
  /* Used for struct and pointer since both have a ptr. */
  if (self->ptr.owner_id) {
    PointerRNA id_ptr;
    RNA_id_pointer_create((ID *)self->ptr.owner_id, &id_ptr);
    return pyrna_struct_CreatePyObject(&id_ptr);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(pyrna_struct_get_data_doc,
             "The data this property is using, *type* :class:`bpy.types.bpy_struct`");
static PyObject *pyrna_struct_get_data(BPy_DummyPointerRNA *self)
{
  return pyrna_struct_CreatePyObject(&self->ptr);
}

PyDoc_STRVAR(pyrna_struct_get_rna_type_doc, "The property type for introspection");
static PyObject *pyrna_struct_get_rna_type(BPy_PropertyRNA *self)
{
  PointerRNA tptr;
  RNA_pointer_create(NULL, &RNA_Property, self->prop, &tptr);
  return pyrna_struct_Subtype(&tptr);
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/

static PyGetSetDef pyrna_prop_getseters[] = {
    {"id_data",
     (getter)pyrna_struct_get_id_data,
     (setter)NULL,
     pyrna_struct_get_id_data_doc,
     NULL},
    {"data", (getter)pyrna_struct_get_data, (setter)NULL, pyrna_struct_get_data_doc, NULL},
    {"rna_type",
     (getter)pyrna_struct_get_rna_type,
     (setter)NULL,
     pyrna_struct_get_rna_type_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef pyrna_struct_getseters[] = {
    {"id_data",
     (getter)pyrna_struct_get_id_data,
     (setter)NULL,
     pyrna_struct_get_id_data_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyObject *pyrna_func_doc_get(BPy_FunctionRNA *self, void *closure);

static PyGetSetDef pyrna_func_getseters[] = {
    {"__doc__", (getter)pyrna_func_doc_get, (setter)NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

PyDoc_STRVAR(pyrna_prop_collection_keys_doc,
             ".. method:: keys()\n"
             "\n"
             "   Return the identifiers of collection members\n"
             "   (matching Python's dict.keys() functionality).\n"
             "\n"
             "   :return: the identifiers for each member of this collection.\n"
             "   :rtype: list of strings\n");
static PyObject *pyrna_prop_collection_keys(BPy_PropertyRNA *self)
{
  PyObject *ret = PyList_New(0);
  char name[256], *nameptr;
  int namelen;

  RNA_PROP_BEGIN (&self->ptr, itemptr, self->prop) {
    nameptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &namelen);

    if (nameptr) {
      PyList_APPEND(ret, PyUnicode_FromStringAndSize(nameptr, namelen));

      if (name != nameptr) {
        MEM_freeN(nameptr);
      }
    }
  }
  RNA_PROP_END;

  return ret;
}

PyDoc_STRVAR(pyrna_prop_collection_items_doc,
             ".. method:: items()\n"
             "\n"
             "   Return the identifiers of collection members\n"
             "   (matching Python's dict.items() functionality).\n"
             "\n"
             "   :return: (key, value) pairs for each member of this collection.\n"
             "   :rtype: list of tuples\n");
static PyObject *pyrna_prop_collection_items(BPy_PropertyRNA *self)
{
  PyObject *ret = PyList_New(0);
  PyObject *item;
  char name[256], *nameptr;
  int namelen;
  int i = 0;

  RNA_PROP_BEGIN (&self->ptr, itemptr, self->prop) {
    if (itemptr.data) {
      /* Add to Python list. */
      item = PyTuple_New(2);
      nameptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &namelen);
      if (nameptr) {
        PyTuple_SET_ITEM(item, 0, PyUnicode_FromStringAndSize(nameptr, namelen));
        if (name != nameptr) {
          MEM_freeN(nameptr);
        }
      }
      else {
        /* A bit strange, but better than returning an empty list. */
        PyTuple_SET_ITEM(item, 0, PyLong_FromLong(i));
      }
      PyTuple_SET_ITEM(item, 1, pyrna_struct_CreatePyObject(&itemptr));

      PyList_APPEND(ret, item);

      i++;
    }
  }
  RNA_PROP_END;

  return ret;
}

PyDoc_STRVAR(pyrna_prop_collection_values_doc,
             ".. method:: values()\n"
             "\n"
             "   Return the values of collection\n"
             "   (matching Python's dict.values() functionality).\n"
             "\n"
             "   :return: the members of this collection.\n"
             "   :rtype: list\n");
static PyObject *pyrna_prop_collection_values(BPy_PropertyRNA *self)
{
  /* Re-use slice. */
  return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
}

PyDoc_STRVAR(pyrna_struct_get_doc,
             ".. method:: get(key, default=None)\n"
             "\n"
             "   Returns the value of the custom property assigned to key or default\n"
             "   when not found (matches Python's dictionary function of the same name).\n"
             "\n"
             "   :arg key: The key associated with the custom property.\n"
             "   :type key: string\n"
             "   :arg default: Optional argument for the value to return if\n"
             "      *key* is not found.\n"
             "   :type default: Undefined\n"
             "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_get(BPy_StructRNA *self, PyObject *args)
{
  IDProperty *group, *idprop;

  const char *key;
  PyObject *def = Py_None;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
    return NULL;
  }

  /* Mostly copied from BPy_IDGroup_Map_GetItem. */
  if (RNA_struct_idprops_check(self->ptr.type) == 0) {
    PyErr_SetString(PyExc_TypeError, "this type doesn't support IDProperties");
    return NULL;
  }

  group = RNA_struct_idprops(&self->ptr, 0);
  if (group) {
    idprop = IDP_GetPropertyFromGroup(group, key);

    if (idprop) {
      return BPy_IDGroup_WrapData(self->ptr.owner_id, idprop, group);
    }
  }

  return Py_INCREF_RET(def);
}

PyDoc_STRVAR(pyrna_struct_pop_doc,
             ".. method:: pop(key, default=None)\n"
             "\n"
             "   Remove and return the value of the custom property assigned to key or default\n"
             "   when not found (matches Python's dictionary function of the same name).\n"
             "\n"
             "   :arg key: The key associated with the custom property.\n"
             "   :type key: string\n"
             "   :arg default: Optional argument for the value to return if\n"
             "      *key* is not found.\n"
             "   :type default: Undefined\n"
             "\n" BPY_DOC_ID_PROP_TYPE_NOTE);
static PyObject *pyrna_struct_pop(BPy_StructRNA *self, PyObject *args)
{
  IDProperty *group, *idprop;

  const char *key;
  PyObject *def = NULL;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
    return NULL;
  }

  /* Mostly copied from BPy_IDGroup_Map_GetItem. */
  if (RNA_struct_idprops_check(self->ptr.type) == 0) {
    PyErr_SetString(PyExc_TypeError, "this type doesn't support IDProperties");
    return NULL;
  }

  group = RNA_struct_idprops(&self->ptr, 0);
  if (group) {
    idprop = IDP_GetPropertyFromGroup(group, key);

    if (idprop) {
      PyObject *ret = BPy_IDGroup_WrapData(self->ptr.owner_id, idprop, group);
      IDP_RemoveFromGroup(group, idprop);
      return ret;
    }
  }

  if (def == NULL) {
    PyErr_SetString(PyExc_KeyError, "key not found");
    return NULL;
  }
  return Py_INCREF_RET(def);
}

PyDoc_STRVAR(pyrna_struct_as_pointer_doc,
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
  return PyLong_FromVoidPtr(self->ptr.data);
}

PyDoc_STRVAR(pyrna_prop_collection_get_doc,
             ".. method:: get(key, default=None)\n"
             "\n"
             "   Returns the value of the item assigned to key or default when not found\n"
             "   (matches Python's dictionary function of the same name).\n"
             "\n"
             "   :arg key: The identifier for the collection member.\n"
             "   :type key: string\n"
             "   :arg default: Optional argument for the value to return if\n"
             "      *key* is not found.\n"
             "   :type default: Undefined\n");
static PyObject *pyrna_prop_collection_get(BPy_PropertyRNA *self, PyObject *args)
{
  PointerRNA newptr;

  PyObject *key_ob;
  PyObject *def = Py_None;

  PYRNA_PROP_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O|O:get", &key_ob, &def)) {
    return NULL;
  }

  if (PyUnicode_Check(key_ob)) {
    const char *key = PyUnicode_AsUTF8(key_ob);

    if (RNA_property_collection_lookup_string(&self->ptr, self->prop, key, &newptr)) {
      return pyrna_struct_CreatePyObject(&newptr);
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

  return Py_INCREF_RET(def);
}

PyDoc_STRVAR(pyrna_prop_collection_find_doc,
             ".. method:: find(key)\n"
             "\n"
             "   Returns the index of a key in a collection or -1 when not found\n"
             "   (matches Python's string find function of the same name).\n"
             "\n"
             "   :arg key: The identifier for the collection member.\n"
             "   :type key: string\n"
             "   :return: index of the key.\n"
             "   :rtype: int\n");
static PyObject *pyrna_prop_collection_find(BPy_PropertyRNA *self, PyObject *key_ob)
{
  Py_ssize_t key_len_ssize_t;
  const char *key = PyUnicode_AsUTF8AndSize(key_ob, &key_len_ssize_t);
  const int key_len = (int)key_len_ssize_t; /* Compare with same type. */

  char name[256], *nameptr;
  int namelen;
  int i = 0;
  int index = -1;

  PYRNA_PROP_CHECK_OBJ(self);

  RNA_PROP_BEGIN (&self->ptr, itemptr, self->prop) {
    nameptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &namelen);

    if (nameptr) {
      if ((key_len == namelen) && memcmp(nameptr, key, key_len) == 0) {
        index = i;
        break;
      }

      if (name != nameptr) {
        MEM_freeN(nameptr);
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
                              bool *r_attr_signed)
{
  PropertyRNA *prop;
  bool attr_ok = true;
  *r_raw_type = PROP_RAW_UNSET;
  *r_attr_tot = 0;
  *r_attr_signed = false;

  /* NOTE: this is fail with zero length lists, so don't let this get called in that case. */
  RNA_PROP_BEGIN (&self->ptr, itemptr, self->prop) {
    prop = RNA_struct_find_property(&itemptr, attr);
    if (prop) {
      *r_raw_type = RNA_property_raw_type(prop);
      *r_attr_tot = RNA_property_array_length(&itemptr, prop);
      *r_attr_signed = (RNA_property_subtype(prop) != PROP_UNSIGNED);
    }
    else {
      attr_ok = false;
    }
    break;
  }
  RNA_PROP_END;

  return attr_ok;
}

/* pyrna_prop_collection_foreach_get/set both use this. */
static int foreach_parse_args(BPy_PropertyRNA *self,
                              PyObject *args,

                              /* Values to assign. */
                              const char **r_attr,
                              PyObject **r_seq,
                              int *r_tot,
                              int *r_size,
                              RawPropertyType *r_raw_type,
                              int *r_attr_tot,
                              bool *r_attr_signed)
{
#if 0
  int array_tot;
  int target_tot;
#endif

  *r_size = *r_attr_tot = 0;
  *r_attr_signed = false;
  *r_raw_type = PROP_RAW_UNSET;

  if (!PyArg_ParseTuple(args, "sO:foreach_get/set", r_attr, r_seq)) {
    return -1;
  }

  if (!PySequence_Check(*r_seq) && PyObject_CheckBuffer(*r_seq)) {
    PyErr_Format(
        PyExc_TypeError,
        "foreach_get/set expected second argument to be a sequence or buffer, not a %.200s",
        Py_TYPE(*r_seq)->tp_name);
    return -1;
  }

  /* TODO - buffer may not be a sequence! array.array() is though. */
  *r_tot = PySequence_Size(*r_seq);

  if (*r_tot > 0) {
    if (!foreach_attr_type(self, *r_attr, r_raw_type, r_attr_tot, r_attr_signed)) {
      PyErr_Format(PyExc_AttributeError,
                   "foreach_get/set '%.200s.%200s[...]' elements have no attribute '%.200s'",
                   RNA_struct_identifier(self->ptr.type),
                   RNA_property_identifier(self->prop),
                   *r_attr);
      return -1;
    }
    *r_size = RNA_raw_type_sizeof(*r_raw_type);

#if 0 /* Works fine, but not strictly needed. \
       * we could allow RNA_property_collection_raw_* to do the checks */
    if ((*r_attr_tot) < 1) {
      *r_attr_tot = 1;
    }

    if (RNA_property_type(self->prop) == PROP_COLLECTION) {
      array_tot = RNA_property_collection_length(&self->ptr, self->prop);
    }
    else {
      array_tot = RNA_property_array_length(&self->ptr, self->prop);
    }

    target_tot = array_tot * (*r_attr_tot);

    /* rna_access.c - rna_raw_access(...) uses this same method. */
    if (target_tot != (*r_tot)) {
      PyErr_Format(PyExc_TypeError,
                   "foreach_get(attr, sequence) sequence length mismatch given %d, needed %d",
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
    PyErr_SetString(PyExc_AttributeError, "attribute does not support foreach method");
    return -1;
  }
  return 0;
}

static bool foreach_compat_buffer(RawPropertyType raw_type, int attr_signed, const char *format)
{
  const char f = format ? *format : 'B'; /* B is assumed when not set */

  switch (raw_type) {
    case PROP_RAW_CHAR:
      if (attr_signed) {
        return (f == 'b') ? 1 : 0;
      }
      else {
        return (f == 'B') ? 1 : 0;
      }
    case PROP_RAW_SHORT:
      if (attr_signed) {
        return (f == 'h') ? 1 : 0;
      }
      else {
        return (f == 'H') ? 1 : 0;
      }
    case PROP_RAW_INT:
      if (attr_signed) {
        return (f == 'i') ? 1 : 0;
      }
      else {
        return (f == 'I') ? 1 : 0;
      }
    case PROP_RAW_BOOLEAN:
      return (f == '?') ? 1 : 0;
    case PROP_RAW_FLOAT:
      return (f == 'f') ? 1 : 0;
    case PROP_RAW_DOUBLE:
      return (f == 'd') ? 1 : 0;
    case PROP_RAW_UNSET:
      return 0;
  }

  return 0;
}

static PyObject *foreach_getset(BPy_PropertyRNA *self, PyObject *args, int set)
{
  PyObject *item = NULL;
  int i = 0, ok = 0;
  bool buffer_is_compat;
  void *array = NULL;

  /* Get/set both take the same args currently. */
  const char *attr;
  PyObject *seq;
  int tot, size, attr_tot;
  bool attr_signed;
  RawPropertyType raw_type;

  if (foreach_parse_args(
          self, args, &attr, &seq, &tot, &size, &raw_type, &attr_tot, &attr_signed) == -1) {
    return NULL;
  }

  if (tot == 0) {
    Py_RETURN_NONE;
  }

  if (set) { /* Get the array from python. */
    buffer_is_compat = false;
    if (PyObject_CheckBuffer(seq)) {
      Py_buffer buf;
      PyObject_GetBuffer(seq, &buf, PyBUF_SIMPLE | PyBUF_FORMAT);

      /* Check if the buffer matches. */

      buffer_is_compat = foreach_compat_buffer(raw_type, attr_signed, buf.format);

      if (buffer_is_compat) {
        ok = RNA_property_collection_raw_set(
            NULL, &self->ptr, self->prop, attr, buf.buf, raw_type, tot);
      }

      PyBuffer_Release(&buf);
    }

    /* Could not use the buffer, fallback to sequence. */
    if (!buffer_is_compat) {
      array = PyMem_Malloc(size * tot);

      for (; i < tot; i++) {
        item = PySequence_GetItem(seq, i);
        switch (raw_type) {
          case PROP_RAW_CHAR:
            ((char *)array)[i] = (char)PyLong_AsLong(item);
            break;
          case PROP_RAW_SHORT:
            ((short *)array)[i] = (short)PyLong_AsLong(item);
            break;
          case PROP_RAW_INT:
            ((int *)array)[i] = (int)PyLong_AsLong(item);
            break;
          case PROP_RAW_BOOLEAN:
            ((bool *)array)[i] = (int)PyLong_AsLong(item) != 0;
            break;
          case PROP_RAW_FLOAT:
            ((float *)array)[i] = (float)PyFloat_AsDouble(item);
            break;
          case PROP_RAW_DOUBLE:
            ((double *)array)[i] = (double)PyFloat_AsDouble(item);
            break;
          case PROP_RAW_UNSET:
            /* Should never happen. */
            BLI_assert(!"Invalid array type - set");
            break;
        }

        Py_DECREF(item);
      }

      ok = RNA_property_collection_raw_set(
          NULL, &self->ptr, self->prop, attr, array, raw_type, tot);
    }
  }
  else {
    buffer_is_compat = false;
    if (PyObject_CheckBuffer(seq)) {
      Py_buffer buf;
      PyObject_GetBuffer(seq, &buf, PyBUF_SIMPLE | PyBUF_FORMAT);

      /* Check if the buffer matches, TODO - signed/unsigned types. */

      buffer_is_compat = foreach_compat_buffer(raw_type, attr_signed, buf.format);

      if (buffer_is_compat) {
        ok = RNA_property_collection_raw_get(
            NULL, &self->ptr, self->prop, attr, buf.buf, raw_type, tot);
      }

      PyBuffer_Release(&buf);
    }

    /* Could not use the buffer, fallback to sequence. */
    if (!buffer_is_compat) {
      array = PyMem_Malloc(size * tot);

      ok = RNA_property_collection_raw_get(
          NULL, &self->ptr, self->prop, attr, array, raw_type, tot);

      if (!ok) {
        /* Skip the loop. */
        i = tot;
      }

      for (; i < tot; i++) {

        switch (raw_type) {
          case PROP_RAW_CHAR:
            item = PyLong_FromLong((long)((char *)array)[i]);
            break;
          case PROP_RAW_SHORT:
            item = PyLong_FromLong((long)((short *)array)[i]);
            break;
          case PROP_RAW_INT:
            item = PyLong_FromLong((long)((int *)array)[i]);
            break;
          case PROP_RAW_FLOAT:
            item = PyFloat_FromDouble((double)((float *)array)[i]);
            break;
          case PROP_RAW_DOUBLE:
            item = PyFloat_FromDouble((double)((double *)array)[i]);
            break;
          case PROP_RAW_BOOLEAN:
            item = PyBool_FromLong((long)((bool *)array)[i]);
            break;
          default: /* PROP_RAW_UNSET */
            /* Should never happen. */
            BLI_assert(!"Invalid array type - get");
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
    return NULL;
  }
  if (!ok) {
    PyErr_SetString(PyExc_RuntimeError, "internal error setting the array");
    return NULL;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(pyrna_prop_collection_foreach_get_doc,
             ".. method:: foreach_get(attr, seq)\n"
             "\n"
             "   This is a function to give fast access to attributes within a collection.\n");
static PyObject *pyrna_prop_collection_foreach_get(BPy_PropertyRNA *self, PyObject *args)
{
  PYRNA_PROP_CHECK_OBJ(self);

  return foreach_getset(self, args, 0);
}

PyDoc_STRVAR(pyrna_prop_collection_foreach_set_doc,
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
  PyObject *item = NULL;
  Py_ssize_t i, seq_size, size;
  void *array = NULL;
  const PropertyType prop_type = RNA_property_type(self->prop);

  /* Get/set both take the same args currently. */
  PyObject *seq;

  if (!ELEM(prop_type, PROP_INT, PROP_FLOAT)) {
    PyErr_Format(PyExc_TypeError, "foreach_get/set available only for int and float");
    return NULL;
  }

  if (!PyArg_ParseTuple(args, "O:foreach_get/set", &seq)) {
    return NULL;
  }

  if (!PySequence_Check(seq) && PyObject_CheckBuffer(seq)) {
    PyErr_Format(
        PyExc_TypeError,
        "foreach_get/set expected second argument to be a sequence or buffer, not a %.200s",
        Py_TYPE(seq)->tp_name);
    return NULL;
  }

  size = pyrna_prop_array_length(self);
  seq_size = PySequence_Size(seq);

  if (size != seq_size) {
    PyErr_Format(PyExc_TypeError, "expected sequence size %d, got %d", size, seq_size);
    return NULL;
  }

  Py_buffer buf;
  if (PyObject_GetBuffer(seq, &buf, PyBUF_SIMPLE | PyBUF_FORMAT) == -1) {
    PyErr_Clear();

    switch (prop_type) {
      case PROP_INT:
        array = PyMem_Malloc(sizeof(int) * size);
        if (do_set) {
          for (i = 0; i < size; i++) {
            item = PySequence_GetItem(seq, i);
            ((int *)array)[i] = (int)PyLong_AsLong(item);
            Py_DECREF(item);
          }

          RNA_property_int_set_array(&self->ptr, self->prop, array);
        }
        else {
          RNA_property_int_get_array(&self->ptr, self->prop, array);

          for (i = 0; i < size; i++) {
            item = PyLong_FromLong((long)((int *)array)[i]);
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
            ((float *)array)[i] = (float)PyFloat_AsDouble(item);
            Py_DECREF(item);
          }

          RNA_property_float_set_array(&self->ptr, self->prop, array);
        }
        else {
          RNA_property_float_get_array(&self->ptr, self->prop, array);

          for (i = 0; i < size; i++) {
            item = PyFloat_FromDouble((double)((float *)array)[i]);
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
        BLI_assert(false);
        break;
    }

    PyMem_Free(array);

    if (PyErr_Occurred()) {
      /* Maybe we could make our own error. */
      PyErr_Print();
      PyErr_SetString(PyExc_TypeError, "couldn't access the py sequence");
      return NULL;
    }
  }
  else {
    const char f = buf.format ? buf.format[0] : 0;
    if ((prop_type == PROP_INT && (buf.itemsize != sizeof(int) || (f != 'l' && f != 'i'))) ||
        (prop_type == PROP_FLOAT && (buf.itemsize != sizeof(float) || f != 'f'))) {
      PyBuffer_Release(&buf);
      PyErr_Format(PyExc_TypeError, "incorrect sequence item type: %s", buf.format);
      return NULL;
    }

    switch (prop_type) {
      case PROP_INT:
        if (do_set) {
          RNA_property_int_set_array(&self->ptr, self->prop, buf.buf);
        }
        else {
          RNA_property_int_get_array(&self->ptr, self->prop, buf.buf);
        }
        break;
      case PROP_FLOAT:
        if (do_set) {
          RNA_property_float_set_array(&self->ptr, self->prop, buf.buf);
        }
        else {
          RNA_property_float_get_array(&self->ptr, self->prop, buf.buf);
        }
        break;
      case PROP_BOOLEAN:
      case PROP_STRING:
      case PROP_ENUM:
      case PROP_POINTER:
      case PROP_COLLECTION:
        /* Should never happen. */
        BLI_assert(false);
        break;
    }

    PyBuffer_Release(&buf);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(pyrna_prop_array_foreach_get_doc,
             ".. method:: foreach_get(seq)\n"
             "\n"
             "   This is a function to give fast access to array data.\n");
static PyObject *pyrna_prop_array_foreach_get(BPy_PropertyArrayRNA *self, PyObject *args)
{
  PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

  return pyprop_array_foreach_getset(self, args, false);
}

PyDoc_STRVAR(pyrna_prop_array_foreach_set_doc,
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
  PyObject *iter = NULL;
  int len;

  PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

  len = pyrna_prop_array_length(self);
  ret = pyrna_prop_array_subscript_slice(self, &self->ptr, self->prop, 0, len, len);

  /* we know this is a list so no need to PyIter_Check
   * otherwise it could be NULL (unlikely) if conversion failed */
  if (ret) {
    iter = PyObject_GetIter(ret);
    Py_DECREF(ret);
  }

  return iter;
}

static PyObject *pyrna_prop_collection_iter(BPy_PropertyRNA *self);

#ifndef USE_PYRNA_ITER
static PyObject *pyrna_prop_collection_iter(BPy_PropertyRNA *self)
{
  /* Try get values from a collection. */
  PyObject *ret;
  PyObject *iter = NULL;
  ret = pyrna_prop_collection_values(self);

  /* we know this is a list so no need to PyIter_Check
   * otherwise it could be NULL (unlikely) if conversion failed */
  if (ret) {
    iter = PyObject_GetIter(ret);
    Py_DECREF(ret);
  }

  return iter;
}
#endif /* # !USE_PYRNA_ITER */

static struct PyMethodDef pyrna_struct_methods[] = {

    /* Only for PointerRNA's with ID'props. */
    {"keys", (PyCFunction)pyrna_struct_keys, METH_NOARGS, pyrna_struct_keys_doc},
    {"values", (PyCFunction)pyrna_struct_values, METH_NOARGS, pyrna_struct_values_doc},
    {"items", (PyCFunction)pyrna_struct_items, METH_NOARGS, pyrna_struct_items_doc},

    {"get", (PyCFunction)pyrna_struct_get, METH_VARARGS, pyrna_struct_get_doc},
    {"pop", (PyCFunction)pyrna_struct_pop, METH_VARARGS, pyrna_struct_pop_doc},

    {"as_pointer", (PyCFunction)pyrna_struct_as_pointer, METH_NOARGS, pyrna_struct_as_pointer_doc},

    /* bpy_rna_anim.c */
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
    {"__dir__", (PyCFunction)pyrna_struct_dir, METH_NOARGS, NULL},

/* experimental */
/* unused for now */
#if 0
    {"callback_add", (PyCFunction)pyrna_callback_add, METH_VARARGS, NULL},
    {"callback_remove", (PyCFunction)pyrna_callback_remove, METH_VARARGS, NULL},

    {"callback_add", (PyCFunction)pyrna_callback_classmethod_add, METH_VARARGS | METH_CLASS, NULL},
    {"callback_remove",
     (PyCFunction)pyrna_callback_classmethod_remove,
     METH_VARARGS | METH_CLASS,
     NULL},
#endif
    {NULL, NULL, 0, NULL},
};

static struct PyMethodDef pyrna_prop_methods[] = {
    {"path_from_id",
     (PyCFunction)pyrna_prop_path_from_id,
     METH_NOARGS,
     pyrna_prop_path_from_id_doc},
    {"as_bytes", (PyCFunction)pyrna_prop_as_bytes, METH_NOARGS, pyrna_prop_as_bytes_doc},
    {"update", (PyCFunction)pyrna_prop_update, METH_NOARGS, pyrna_prop_update_doc},
    {"__dir__", (PyCFunction)pyrna_prop_dir, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL},
};

static struct PyMethodDef pyrna_prop_array_methods[] = {
    {"foreach_get",
     (PyCFunction)pyrna_prop_array_foreach_get,
     METH_VARARGS,
     pyrna_prop_array_foreach_get_doc},
    {"foreach_set",
     (PyCFunction)pyrna_prop_array_foreach_set,
     METH_VARARGS,
     pyrna_prop_array_foreach_set_doc},

    {NULL, NULL, 0, NULL},
};

static struct PyMethodDef pyrna_prop_collection_methods[] = {
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
    {NULL, NULL, 0, NULL},
};

static struct PyMethodDef pyrna_prop_collection_idprop_methods[] = {
    {"add", (PyCFunction)pyrna_prop_collection_idprop_add, METH_NOARGS, NULL},
    {"remove", (PyCFunction)pyrna_prop_collection_idprop_remove, METH_O, NULL},
    {"clear", (PyCFunction)pyrna_prop_collection_idprop_clear, METH_NOARGS, NULL},
    {"move", (PyCFunction)pyrna_prop_collection_idprop_move, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL},
};

/* only needed for subtyping, so a new class gets a valid BPy_StructRNA
 * todo - also accept useful args */
static PyObject *pyrna_struct_new(PyTypeObject *type, PyObject *args, PyObject *UNUSED(kwds))
{
  if (PyTuple_GET_SIZE(args) == 1) {
    BPy_StructRNA *base = (BPy_StructRNA *)PyTuple_GET_ITEM(args, 0);
    if (Py_TYPE(base) == type) {
      Py_INCREF(base);
      return (PyObject *)base;
    }
    if (PyType_IsSubtype(Py_TYPE(base), &pyrna_struct_Type)) {
      /* this almost never runs, only when using user defined subclasses of built-in object.
       * this isn't common since it's NOT related to registerable subclasses. eg:
       *
       *  >>> class MyObSubclass(bpy.types.Object):
       *  ...     def test_func(self):
       *  ...         print(100)
       *  ...
       *  >>> myob = MyObSubclass(bpy.context.object)
       *  >>> myob.test_func()
       *  100
       *
       * Keep this since it could be useful.
       */
      BPy_StructRNA *ret;
      if ((ret = (BPy_StructRNA *)type->tp_alloc(type, 0))) {
        ret->ptr = base->ptr;
#ifdef USE_PYRNA_STRUCT_REFERENCE
        /* #PyType_GenericAlloc will have set tracking.
         * We only want tracking when `StructRNA.reference` has been set. */
        PyObject_GC_UnTrack(ret);
#endif
      }
      /* Pass on exception & NULL if tp_alloc fails. */
      return (PyObject *)ret;
    }

    /* Error, invalid type given. */
    PyErr_Format(PyExc_TypeError,
                 "bpy_struct.__new__(type): type '%.200s' is not a subtype of bpy_struct",
                 type->tp_name);
    return NULL;
  }

  PyErr_Format(PyExc_TypeError, "bpy_struct.__new__(type): expected a single argument");
  return NULL;
}

/* only needed for subtyping, so a new class gets a valid BPy_StructRNA
 * todo - also accept useful args */
static PyObject *pyrna_prop_new(PyTypeObject *type, PyObject *args, PyObject *UNUSED(kwds))
{
  BPy_PropertyRNA *base;

  if (!PyArg_ParseTuple(args, "O!:bpy_prop.__new__", &pyrna_prop_Type, &base)) {
    return NULL;
  }

  if (type == Py_TYPE(base)) {
    return Py_INCREF_RET((PyObject *)base);
  }
  if (PyType_IsSubtype(type, &pyrna_prop_Type)) {
    BPy_PropertyRNA *ret = (BPy_PropertyRNA *)type->tp_alloc(type, 0);
    ret->ptr = base->ptr;
    ret->prop = base->prop;
    return (PyObject *)ret;
  }

  PyErr_Format(PyExc_TypeError,
               "bpy_prop.__new__(type): type '%.200s' is not a subtype of bpy_prop",
               type->tp_name);
  return NULL;
}

static PyObject *pyrna_param_to_py(PointerRNA *ptr, PropertyRNA *prop, void *data)
{
  PyObject *ret;
  const int type = RNA_property_type(prop);
  const int flag = RNA_property_flag(prop);
  const int flag_parameter = RNA_parameter_flag(prop);

  if (RNA_property_array_check(prop)) {
    int a, len;

    if (flag & PROP_DYNAMIC) {
      ParameterDynAlloc *data_alloc = data;
      len = data_alloc->array_tot;
      data = data_alloc->array;
    }
    else {
      len = RNA_property_array_length(ptr, prop);
    }

    /* Resolve the array from a new pytype. */

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
            ret = Vector_CreatePyObject(data, len, NULL);
            break;
          case PROP_MATRIX:
            if (len == 16) {
              ret = Matrix_CreatePyObject(data, 4, 4, NULL);
              break;
            }
            else if (len == 9) {
              ret = Matrix_CreatePyObject(data, 3, 3, NULL);
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
        ret = NULL;
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
        PyObject *value_coerce = NULL;
        const int subtype = RNA_property_subtype(prop);

        if (flag & PROP_THICK_WRAP) {
          data_ch = (char *)data;
        }
        else {
          data_ch = *(char **)data;
        }

#ifdef USE_STRING_COERCE
        if (subtype == PROP_BYTESTRING) {
          ret = PyBytes_FromString(data_ch);
        }
        else if (ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
          ret = PyC_UnicodeFromByte(data_ch);
        }
        else {
          ret = PyUnicode_FromString(data_ch);
        }
#else
        if (subtype == PROP_BYTESTRING) {
          ret = PyBytes_FromString(buf);
        }
        else {
          ret = PyUnicode_FromString(data_ch);
        }
#endif

#ifdef USE_STRING_COERCE
        Py_XDECREF(value_coerce);
#endif

        break;
      }
      case PROP_ENUM: {
        ret = pyrna_enum_to_py(ptr, prop, *(int *)data);
        break;
      }
      case PROP_POINTER: {
        PointerRNA newptr;
        StructRNA *ptype = RNA_property_pointer_type(ptr, prop);

        if (flag_parameter & PARM_RNAPTR) {
          /* In this case we get the full ptr. */
          newptr = *(PointerRNA *)data;
        }
        else {
          if (RNA_struct_is_ID(ptype)) {
            RNA_id_pointer_create(*(void **)data, &newptr);
          }
          else {
            /* note: this is taken from the function's ID pointer
             * and will break if a function returns a pointer from
             * another ID block, watch this! - it should at least be
             * easy to debug since they are all ID's */
            RNA_pointer_create(ptr->owner_id, ptype, *(void **)data, &newptr);
          }
        }

        if (newptr.data) {
          ret = pyrna_struct_CreatePyObject(&newptr);
        }
        else {
          ret = Py_None;
          Py_INCREF(ret);
        }
        break;
      }
      case PROP_COLLECTION: {
        CollectionListBase *lb = (CollectionListBase *)data;
        CollectionPointerLink *link;

        ret = PyList_New(0);

        for (link = lb->first; link; link = link->next) {
          PyList_APPEND(ret, pyrna_struct_CreatePyObject(&link->ptr));
        }

        break;
      }
      default:
        PyErr_Format(PyExc_TypeError, "RNA Error: unknown type \"%d\" (pyrna_param_to_py)", type);
        ret = NULL;
        break;
    }
  }

  return ret;
}

/**
 * Use to replace PyDict_GetItemString() when the overhead of converting a
 * string into a Python unicode is higher than a non hash lookup.
 * works on small dict's such as keyword args.
 */
static PyObject *small_dict_get_item_string(PyObject *dict, const char *key_lookup)
{
  PyObject *key = NULL;
  Py_ssize_t pos = 0;
  PyObject *value = NULL;

  while (PyDict_Next(dict, &pos, &key, &value)) {
    if (PyUnicode_Check(key)) {
      if (STREQ(key_lookup, PyUnicode_AsUTF8(key))) {
        return value;
      }
    }
  }

  return NULL;
}

static PyObject *pyrna_func_call(BPy_FunctionRNA *self, PyObject *args, PyObject *kw)
{
  /* Note, both BPy_StructRNA and BPy_PropertyRNA can be used here. */
  PointerRNA *self_ptr = &self->ptr;
  FunctionRNA *self_func = self->func;

  PointerRNA funcptr;
  ParameterList parms;
  ParameterIterator iter;
  PropertyRNA *parm;
  PyObject *ret, *item;
  int i, pyargs_len, pykw_len, parms_len, ret_len, flag_parameter, err = 0, kw_tot = 0;
  bool kw_arg;

  PropertyRNA *pret_single = NULL;
  void *retdata_single = NULL;

  /* enable this so all strings are copied and freed after calling.
   * this exposes bugs where the pointer to the string is held and re-used */
  /* #define DEBUG_STRING_FREE */

#ifdef DEBUG_STRING_FREE
  PyObject *string_free_ls = PyList_New(0);
#endif

  /* Should never happen, but it does in rare cases. */
  BLI_assert(self_ptr != NULL);

  if (self_ptr == NULL) {
    PyErr_SetString(PyExc_RuntimeError,
                    "RNA functions internal RNA pointer is NULL, this is a bug. aborting");
    return NULL;
  }

  if (self_func == NULL) {
    PyErr_Format(
        PyExc_RuntimeError,
        "%.200s.<unknown>(): RNA function internal function is NULL, this is a bug. aborting",
        RNA_struct_identifier(self_ptr->type));
    return NULL;
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
  RNA_pointer_create(self_ptr->owner_id, &RNA_Function, self_func, &funcptr);

  pyargs_len = PyTuple_GET_SIZE(args);
  pykw_len = kw ? PyDict_Size(kw) : 0;

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
      if (pret_single == NULL) {
        pret_single = parm;
        retdata_single = iter.data;
      }

      continue;
    }

    item = NULL;

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

      item = PyTuple_GET_ITEM(args, i);
      kw_arg = false;
    }
    else if (kw != NULL) {
#if 0
      item = PyDict_GetItemString(kw, RNA_property_identifier(parm)); /* Borrow reference. */
#else
      item = small_dict_get_item_string(kw, RNA_property_identifier(parm)); /* Borrow reference. */
#endif
      if (item) {
        kw_tot++; /* Make sure invalid keywords are not given. */
      }

      kw_arg = true;
    }

    i++; /* Current argument. */

    if (item == NULL) {
      if (flag_parameter & PARM_REQUIRED) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s.%.200s(): required parameter \"%.200s\" not specified",
                     RNA_struct_identifier(self_ptr->type),
                     RNA_function_identifier(self_func),
                     RNA_property_identifier(parm));
        err = -1;
        break;
      }
      /* PyDict_GetItemString wont raise an error. */
      continue;
    }

#ifdef DEBUG_STRING_FREE
    if (item) {
      if (PyUnicode_Check(item)) {
        PyList_APPEND(string_free_ls, PyUnicode_FromString(PyUnicode_AsUTF8(item)));
      }
    }
#endif
    err = pyrna_py_to_prop(&funcptr, parm, iter.data, item, "");

    if (err != 0) {
      /* the error generated isn't that useful, so generate it again with a useful prefix
       * could also write a function to prepend to error messages */
      char error_prefix[512];
      PyErr_Clear(); /* Re-raise. */

      if (kw_arg == true) {
        BLI_snprintf(error_prefix,
                     sizeof(error_prefix),
                     "%.200s.%.200s(): error with keyword argument \"%.200s\" - ",
                     RNA_struct_identifier(self_ptr->type),
                     RNA_function_identifier(self_func),
                     RNA_property_identifier(parm));
      }
      else {
        BLI_snprintf(error_prefix,
                     sizeof(error_prefix),
                     "%.200s.%.200s(): error with argument %d, \"%.200s\" - ",
                     RNA_struct_identifier(self_ptr->type),
                     RNA_function_identifier(self_func),
                     i,
                     RNA_property_identifier(parm));
      }

      pyrna_py_to_prop(&funcptr, parm, iter.data, item, error_prefix);

      break;
    }
  }

  RNA_parameter_list_end(&iter);

  /* Check if we gave args that don't exist in the function
   * Printing the error is slow, but it should only happen when developing.
   * The "if" below is quick check to make sure less keyword args were passed than we gave.
   * (Don't overwrite the error if we have one,
   * otherwise can skip important messages and confuse with args).
   */
  if (err == 0 && kw && (pykw_len > kw_tot)) {
    PyObject *key, *value;
    Py_ssize_t pos = 0;

    DynStr *bad_args = BLI_dynstr_new();
    DynStr *good_args = BLI_dynstr_new();

    const char *arg_name, *bad_args_str, *good_args_str;
    bool found = false, first = true;

    while (PyDict_Next(kw, &pos, &key, &value)) {

      arg_name = PyUnicode_AsUTF8(key);
      found = false;

      if (arg_name == NULL) { /* Unlikely the argname is not a string, but ignore if it is. */
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
    MEM_freeN((void *)bad_args_str);
    MEM_freeN((void *)good_args_str);

    err = -1;
  }

  ret = NULL;
  if (err == 0) {
    /* Call function. */
    ReportList reports;
    bContext *C = BPY_context_get();

    BKE_reports_init(&reports, RPT_STORE);
    RNA_function_call(C, &reports, self_ptr, self_func, &parms);

    err = (BPy_reports_to_error(&reports, PyExc_RuntimeError, true));

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
        if (ret == NULL) {
          err = -1;
        }
      }
    }
  }

#ifdef DEBUG_STRING_FREE
#  if 0
  if (PyList_GET_SIZE(string_free_ls)) {
    printf("%.200s.%.200s():  has %d strings\n",
           RNA_struct_identifier(self_ptr->type),
           RNA_function_identifier(self_func),
           (int)PyList_GET_SIZE(string_free_ls));
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
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *pyrna_func_doc_get(BPy_FunctionRNA *self, void *UNUSED(closure))
{
  PyObject *ret;
  char *args;

  args = RNA_function_as_string_keywords(NULL, self->func, true, true, INT_MAX);

  ret = PyUnicode_FromFormat("%.200s.%.200s(%.200s)\n%s",
                             RNA_struct_identifier(self->ptr.type),
                             RNA_function_identifier(self->func),
                             args,
                             RNA_function_ui_description(self->func));

  MEM_freeN(args);

  return ret;
}

/* Subclasses of pyrna_struct_Type which support idprop definitions use this as a metaclass. */
/* note: tp_base member is set to &PyType_Type on init */
PyTypeObject pyrna_struct_meta_idprop_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_struct_meta_idprop", /* tp_name */

    /* NOTE! would be PyTypeObject, but subtypes of Type must be PyHeapTypeObject's */
    sizeof(PyHeapTypeObject), /* tp_basicsize */

    0, /* tp_itemsize */
    /* methods */
    NULL, /* tp_dealloc */
    0,    /* tp_vectorcall_offset */
    NULL, /* getattrfunc tp_getattr; */
    NULL, /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */ /* deprecated in Python 3.0! */
    NULL,            /* tp_repr */

    /* Method suites for standard classes */
    NULL, /* PyNumberMethods *tp_as_number; */
    NULL, /* PySequenceMethods *tp_as_sequence; */
    NULL, /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */
    NULL,                                                      /* hashfunc tp_hash; */
    NULL,                                                      /* ternaryfunc tp_call; */
    NULL,                                                      /* reprfunc tp_str; */
    NULL /*(getattrofunc) pyrna_struct_meta_idprop_getattro*/, /* getattrofunc tp_getattro; */
    (setattrofunc)pyrna_struct_meta_idprop_setattro,           /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    NULL, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons ***/
    NULL, /* richcmpfunc tp_richcompare; */

    /***  weak reference enabler ***/
    0, /* long tp_weaklistoffset; */

    /*** Added in release 2.2 ***/
    /*   Iterators */
    NULL, /* getiterfunc tp_iter; */
    NULL, /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    NULL, /* struct PyMethodDef *tp_methods; */
    NULL, /* struct PyMemberDef *tp_members; */
    NULL, /* struct PyGetSetDef *tp_getset; */
#if defined(_MSC_VER)
    NULL, /* defer assignment */
#else
    &PyType_Type, /* struct _typeobject *tp_base; */
#endif
    NULL, /* PyObject *tp_dict; */
    NULL, /* descrgetfunc tp_descr_get; */
    NULL, /* descrsetfunc tp_descr_set; */
    0,    /* long tp_dictoffset; */
    NULL, /* initproc tp_init; */
    NULL, /* allocfunc tp_alloc; */
    NULL, /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

/*-----------------------BPy_StructRNA method def------------------------------*/
PyTypeObject pyrna_struct_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_struct", /* tp_name */
    sizeof(BPy_StructRNA),                       /* tp_basicsize */
    0,                                           /* tp_itemsize */
    /* methods */
    (destructor)pyrna_struct_dealloc, /* tp_dealloc */
    0,                                /* tp_vectorcall_offset */
    NULL,                             /* getattrfunc tp_getattr; */
    NULL,                             /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */             /* DEPRECATED in Python 3.0! */
    (reprfunc)pyrna_struct_repr, /* tp_repr */

    /* Method suites for standard classes */

    NULL,                      /* PyNumberMethods *tp_as_number; */
    &pyrna_struct_as_sequence, /* PySequenceMethods *tp_as_sequence; */
    &pyrna_struct_as_mapping,  /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    (hashfunc)pyrna_struct_hash,         /* hashfunc tp_hash; */
    NULL,                                /* ternaryfunc tp_call; */
    (reprfunc)pyrna_struct_str,          /* reprfunc tp_str; */
    (getattrofunc)pyrna_struct_getattro, /* getattrofunc tp_getattro; */
    (setattrofunc)pyrna_struct_setattro, /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE
#ifdef USE_PYRNA_STRUCT_REFERENCE
        | Py_TPFLAGS_HAVE_GC
#endif
    , /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
/*** Assigned meaning in release 2.0 ***/
/* call function for all accessible objects */
#ifdef USE_PYRNA_STRUCT_REFERENCE
    (traverseproc)pyrna_struct_traverse, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    (inquiry)pyrna_struct_clear, /* inquiry tp_clear; */
#else
    NULL,         /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons ***/
    (richcmpfunc)pyrna_struct_richcmp, /* richcmpfunc tp_richcompare; */

/***  weak reference enabler ***/
#ifdef USE_WEAKREFS
    offsetof(BPy_StructRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
    0,
#endif
    /*** Added in release 2.2 ***/
    /*   Iterators */
    NULL, /* getiterfunc tp_iter; */
    NULL, /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    pyrna_struct_methods,   /* struct PyMethodDef *tp_methods; */
    NULL,                   /* struct PyMemberDef *tp_members; */
    pyrna_struct_getseters, /* struct PyGetSetDef *tp_getset; */
    NULL,                   /* struct _typeobject *tp_base; */
    NULL,                   /* PyObject *tp_dict; */
    NULL,                   /* descrgetfunc tp_descr_get; */
    NULL,                   /* descrsetfunc tp_descr_set; */
    0,                      /* long tp_dictoffset; */
    NULL,                   /* initproc tp_init; */
    NULL,                   /* allocfunc tp_alloc; */
    pyrna_struct_new,       /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

/*-----------------------BPy_PropertyRNA method def------------------------------*/
PyTypeObject pyrna_prop_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_prop", /* tp_name */
    sizeof(BPy_PropertyRNA),                   /* tp_basicsize */
    0,                                         /* tp_itemsize */
    /* methods */
    (destructor)pyrna_prop_dealloc, /* tp_dealloc */
    0,                              /* tp_vectorcall_offset */
    NULL,                           /* getattrfunc tp_getattr; */
    NULL,                           /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */           /* DEPRECATED in Python 3.0! */
    (reprfunc)pyrna_prop_repr, /* tp_repr */

    /* Method suites for standard classes */

    NULL, /* PyNumberMethods *tp_as_number; */
    NULL, /* PySequenceMethods *tp_as_sequence; */
    NULL, /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    (hashfunc)pyrna_prop_hash, /* hashfunc tp_hash; */
    NULL,                      /* ternaryfunc tp_call; */
    (reprfunc)pyrna_prop_str,  /* reprfunc tp_str; */

    /* will only use these if this is a subtype of a py class */
    NULL, /* getattrofunc tp_getattro; */
    NULL, /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    NULL, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons ***/
    (richcmpfunc)pyrna_prop_richcmp, /* richcmpfunc tp_richcompare; */

/***  weak reference enabler ***/
#ifdef USE_WEAKREFS
    offsetof(BPy_PropertyRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
    0,
#endif

    /*** Added in release 2.2 ***/
    /*   Iterators */
    NULL, /* getiterfunc tp_iter; */
    NULL, /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    pyrna_prop_methods,   /* struct PyMethodDef *tp_methods; */
    NULL,                 /* struct PyMemberDef *tp_members; */
    pyrna_prop_getseters, /* struct PyGetSetDef *tp_getset; */
    NULL,                 /* struct _typeobject *tp_base; */
    NULL,                 /* PyObject *tp_dict; */
    NULL,                 /* descrgetfunc tp_descr_get; */
    NULL,                 /* descrsetfunc tp_descr_set; */
    0,                    /* long tp_dictoffset; */
    NULL,                 /* initproc tp_init; */
    NULL,                 /* allocfunc tp_alloc; */
    pyrna_prop_new,       /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

PyTypeObject pyrna_prop_array_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_prop_array", /* tp_name */
    sizeof(BPy_PropertyArrayRNA),                    /* tp_basicsize */
    0,                                               /* tp_itemsize */
    /* methods */
    (destructor)pyrna_prop_array_dealloc, /* tp_dealloc */
    0,                                    /* tp_vectorcall_offset */
    NULL,                                 /* getattrfunc tp_getattr; */
    NULL,                                 /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */                 /* DEPRECATED in Python 3.0! */
    (reprfunc)pyrna_prop_array_repr, /* tp_repr */

    /* Method suites for standard classes */

    &pyrna_prop_array_as_number,   /* PyNumberMethods *tp_as_number; */
    &pyrna_prop_array_as_sequence, /* PySequenceMethods *tp_as_sequence; */
    &pyrna_prop_array_as_mapping,  /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    NULL, /* hashfunc tp_hash; */
    NULL, /* ternaryfunc tp_call; */
    NULL, /* reprfunc tp_str; */

    /* will only use these if this is a subtype of a py class */
    (getattrofunc)pyrna_prop_array_getattro, /* getattrofunc tp_getattro; */
    NULL,                                    /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    NULL, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons (subclassed) ***/
    NULL, /* richcmpfunc tp_richcompare; */

/***  weak reference enabler ***/
#ifdef USE_WEAKREFS
    offsetof(BPy_PropertyArrayRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
    0,
#endif
    /*** Added in release 2.2 ***/
    /*   Iterators */
    (getiterfunc)pyrna_prop_array_iter, /* getiterfunc tp_iter; */
    NULL,                               /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    pyrna_prop_array_methods,      /* struct PyMethodDef *tp_methods; */
    NULL,                          /* struct PyMemberDef *tp_members; */
    NULL /*pyrna_prop_getseters*/, /* struct PyGetSetDef *tp_getset; */
    &pyrna_prop_Type,              /* struct _typeobject *tp_base; */
    NULL,                          /* PyObject *tp_dict; */
    NULL,                          /* descrgetfunc tp_descr_get; */
    NULL,                          /* descrsetfunc tp_descr_set; */
    0,                             /* long tp_dictoffset; */
    NULL,                          /* initproc tp_init; */
    NULL,                          /* allocfunc tp_alloc; */
    NULL,                          /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

PyTypeObject pyrna_prop_collection_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_prop_collection", /* tp_name */
    sizeof(BPy_PropertyRNA),                              /* tp_basicsize */
    0,                                                    /* tp_itemsize */
    /* methods */
    (destructor)pyrna_prop_dealloc, /* tp_dealloc */
    0,                              /* tp_vectorcall_offset */
    NULL,                           /* getattrfunc tp_getattr; */
    NULL,                           /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */ /* DEPRECATED in Python 3.0! */
    NULL,
    /* subclassed */ /* tp_repr */

    /* Method suites for standard classes */

    &pyrna_prop_collection_as_number,   /* PyNumberMethods *tp_as_number; */
    &pyrna_prop_collection_as_sequence, /* PySequenceMethods *tp_as_sequence; */
    &pyrna_prop_collection_as_mapping,  /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    NULL, /* hashfunc tp_hash; */
    NULL, /* ternaryfunc tp_call; */
    NULL, /* reprfunc tp_str; */

    /* will only use these if this is a subtype of a py class */
    (getattrofunc)pyrna_prop_collection_getattro, /* getattrofunc tp_getattro; */
    (setattrofunc)pyrna_prop_collection_setattro, /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    NULL, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons (subclassed) ***/
    NULL, /* richcmpfunc tp_richcompare; */

/***  weak reference enabler ***/
#ifdef USE_WEAKREFS
    offsetof(BPy_PropertyRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
    0,
#endif

    /*** Added in release 2.2 ***/
    /*   Iterators */
    (getiterfunc)pyrna_prop_collection_iter, /* getiterfunc tp_iter; */
    NULL,                                    /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    pyrna_prop_collection_methods, /* struct PyMethodDef *tp_methods; */
    NULL,                          /* struct PyMemberDef *tp_members; */
    NULL /*pyrna_prop_getseters*/, /* struct PyGetSetDef *tp_getset; */
    &pyrna_prop_Type,              /* struct _typeobject *tp_base; */
    NULL,                          /* PyObject *tp_dict; */
    NULL,                          /* descrgetfunc tp_descr_get; */
    NULL,                          /* descrsetfunc tp_descr_set; */
    0,                             /* long tp_dictoffset; */
    NULL,                          /* initproc tp_init; */
    NULL,                          /* allocfunc tp_alloc; */
    NULL,                          /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

/* only for add/remove/move methods */
static PyTypeObject pyrna_prop_collection_idprop_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_prop_collection_idprop", /* tp_name */
    sizeof(BPy_PropertyRNA),                                     /* tp_basicsize */
    0,                                                           /* tp_itemsize */
    /* methods */
    (destructor)pyrna_prop_dealloc, /* tp_dealloc */
    0,                              /* tp_vectorcall_offset */
    NULL,                           /* getattrfunc tp_getattr; */
    NULL,                           /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */ /* DEPRECATED in Python 3.0! */
    NULL,
    /* subclassed */ /* tp_repr */

    /* Method suites for standard classes */

    NULL, /* PyNumberMethods *tp_as_number; */
    NULL, /* PySequenceMethods *tp_as_sequence; */
    NULL, /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    NULL, /* hashfunc tp_hash; */
    NULL, /* ternaryfunc tp_call; */
    NULL, /* reprfunc tp_str; */

    /* will only use these if this is a subtype of a py class */
    NULL, /* getattrofunc tp_getattro; */
    NULL, /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    NULL, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons (subclassed) ***/
    NULL, /* richcmpfunc tp_richcompare; */

/***  weak reference enabler ***/
#ifdef USE_WEAKREFS
    offsetof(BPy_PropertyRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
    0,
#endif

    /*** Added in release 2.2 ***/
    /*   Iterators */
    NULL, /* getiterfunc tp_iter; */
    NULL, /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    pyrna_prop_collection_idprop_methods, /* struct PyMethodDef *tp_methods; */
    NULL,                                 /* struct PyMemberDef *tp_members; */
    NULL /*pyrna_prop_getseters*/,        /* struct PyGetSetDef *tp_getset; */
    &pyrna_prop_collection_Type,          /* struct _typeobject *tp_base; */
    NULL,                                 /* PyObject *tp_dict; */
    NULL,                                 /* descrgetfunc tp_descr_get; */
    NULL,                                 /* descrsetfunc tp_descr_set; */
    0,                                    /* long tp_dictoffset; */
    NULL,                                 /* initproc tp_init; */
    NULL,                                 /* allocfunc tp_alloc; */
    NULL,                                 /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

/*-----------------------BPy_PropertyRNA method def------------------------------*/
PyTypeObject pyrna_func_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_func", /* tp_name */
    sizeof(BPy_FunctionRNA),                   /* tp_basicsize */
    0,                                         /* tp_itemsize */
    /* methods */
    NULL, /* tp_dealloc */
    0,    /* tp_vectorcall_offset */
    NULL, /* getattrfunc tp_getattr; */
    NULL, /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */           /* DEPRECATED in Python 3.0! */
    (reprfunc)pyrna_func_repr, /* tp_repr */

    /* Method suites for standard classes */

    NULL, /* PyNumberMethods *tp_as_number; */
    NULL, /* PySequenceMethods *tp_as_sequence; */
    NULL, /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    NULL,                         /* hashfunc tp_hash; */
    (ternaryfunc)pyrna_func_call, /* ternaryfunc tp_call; */
    NULL,                         /* reprfunc tp_str; */

    /* will only use these if this is a subtype of a py class */
    NULL, /* getattrofunc tp_getattro; */
    NULL, /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    NULL, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons ***/
    NULL, /* richcmpfunc tp_richcompare; */

/***  weak reference enabler ***/
#ifdef USE_WEAKREFS
    offsetof(BPy_PropertyRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
    0,
#endif

    /*** Added in release 2.2 ***/
    /*   Iterators */
    NULL, /* getiterfunc tp_iter; */
    NULL, /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    NULL,                 /* struct PyMethodDef *tp_methods; */
    NULL,                 /* struct PyMemberDef *tp_members; */
    pyrna_func_getseters, /* struct PyGetSetDef *tp_getset; */
    NULL,                 /* struct _typeobject *tp_base; */
    NULL,                 /* PyObject *tp_dict; */
    NULL,                 /* descrgetfunc tp_descr_get; */
    NULL,                 /* descrsetfunc tp_descr_set; */
    0,                    /* long tp_dictoffset; */
    NULL,                 /* initproc tp_init; */
    NULL,                 /* allocfunc tp_alloc; */
    NULL,                 /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

#ifdef USE_PYRNA_ITER
/* --- collection iterator: start --- */
/* wrap RNA collection iterator functions */
/*
 * RNA_property_collection_begin(...)
 * RNA_property_collection_next(...)
 * RNA_property_collection_end(...)
 */

static void pyrna_prop_collection_iter_dealloc(BPy_PropertyCollectionIterRNA *self);
static PyObject *pyrna_prop_collection_iter_next(BPy_PropertyCollectionIterRNA *self);

static PyTypeObject pyrna_prop_collection_iter_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_prop_collection_iter", /* tp_name */
    sizeof(BPy_PropertyCollectionIterRNA),                     /* tp_basicsize */
    0,                                                         /* tp_itemsize */
    /* methods */
    (destructor)pyrna_prop_collection_iter_dealloc, /* tp_dealloc */
    0,                                              /* tp_vectorcall_offset */
    NULL,                                           /* getattrfunc tp_getattr; */
    NULL,                                           /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */ /* DEPRECATED in Python 3.0! */
    NULL,
    /* subclassed */ /* tp_repr */

    /* Method suites for standard classes */

    NULL, /* PyNumberMethods *tp_as_number; */
    NULL, /* PySequenceMethods *tp_as_sequence; */
    NULL, /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    NULL, /* hashfunc tp_hash; */
    NULL, /* ternaryfunc tp_call; */
    NULL, /* reprfunc tp_str; */

    /* will only use these if this is a subtype of a py class */
    PyObject_GenericGetAttr, /* getattrofunc tp_getattro; */
    NULL,                    /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    NULL, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    NULL, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons (subclassed) ***/
    NULL, /* richcmpfunc tp_richcompare; */

/***  weak reference enabler ***/
#  ifdef USE_WEAKREFS
    offsetof(BPy_PropertyCollectionIterRNA, in_weakreflist), /* long tp_weaklistoffset; */
#  else
    0,
#  endif
    /*** Added in release 2.2 ***/
    /*   Iterators */
    PyObject_SelfIter,                             /* getiterfunc tp_iter; */
    (iternextfunc)pyrna_prop_collection_iter_next, /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    NULL, /* struct PyMethodDef *tp_methods; */
    NULL, /* struct PyMemberDef *tp_members; */
    NULL, /* struct PyGetSetDef *tp_getset; */
    NULL, /* struct _typeobject *tp_base; */
    NULL, /* PyObject *tp_dict; */
    NULL, /* descrgetfunc tp_descr_get; */
    NULL, /* descrsetfunc tp_descr_set; */
    0,    /* long tp_dictoffset; */
    NULL, /* initproc tp_init; */
    NULL, /* allocfunc tp_alloc; */
    NULL, /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
};

static PyObject *pyrna_prop_collection_iter_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop)
{
  BPy_PropertyCollectionIterRNA *self = PyObject_New(BPy_PropertyCollectionIterRNA,
                                                     &pyrna_prop_collection_iter_Type);

#  ifdef USE_WEAKREFS
  self->in_weakreflist = NULL;
#  endif

  RNA_property_collection_begin(ptr, prop, &self->iter);

  return (PyObject *)self;
}

static PyObject *pyrna_prop_collection_iter(BPy_PropertyRNA *self)
{
  return pyrna_prop_collection_iter_CreatePyObject(&self->ptr, self->prop);
}

static PyObject *pyrna_prop_collection_iter_next(BPy_PropertyCollectionIterRNA *self)
{
  if (self->iter.valid == false) {
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
  }

  BPy_StructRNA *pyrna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&self->iter.ptr);

#  ifdef USE_PYRNA_STRUCT_REFERENCE
  if (pyrna) { /* Unlikely, but may fail. */
    if ((PyObject *)pyrna != Py_None) {
      /* hold a reference to the iterator since it may have
       * allocated memory 'pyrna' needs. eg: introspecting dynamic enum's  */
      /* TODO, we could have an api call to know if this is
       * needed since most collections don't */
      pyrna_struct_reference_set(pyrna, (PyObject *)self);
    }
  }
#  endif /* !USE_PYRNA_STRUCT_REFERENCE */

  RNA_property_collection_next(&self->iter);

  return (PyObject *)pyrna;
}

static void pyrna_prop_collection_iter_dealloc(BPy_PropertyCollectionIterRNA *self)
{
#  ifdef USE_WEAKREFS
  if (self->in_weakreflist != NULL) {
    PyObject_ClearWeakRefs((PyObject *)self);
  }
#  endif

  RNA_property_collection_end(&self->iter);

  PyObject_DEL(self);
}

/* --- collection iterator: end --- */
#endif /* !USE_PYRNA_ITER */

static void pyrna_subtype_set_rna(PyObject *newclass, StructRNA *srna)
{
  PointerRNA ptr;
  PyObject *item;

  Py_INCREF(newclass);

  if (RNA_struct_py_type_get(srna)) {
    PyC_ObSpit("RNA WAS SET - ", RNA_struct_py_type_get(srna));
  }

  Py_XDECREF(((PyObject *)RNA_struct_py_type_get(srna)));

  RNA_struct_py_type_set(srna, (void *)newclass); /* Store for later use */

  /* Not 100% needed, but useful,
   * having an instance within a type looks wrong, but this instance _is_ an RNA type. */

  /* Python deals with the circular reference. */
  RNA_pointer_create(NULL, &RNA_Struct, srna, &ptr);
  item = pyrna_struct_CreatePyObject(&ptr);

  /* Note, must set the class not the __dict__ else the internal slots are not updated correctly.
   */
  PyObject_SetAttr(newclass, bpy_intern_str_bl_rna, item);
  Py_DECREF(item);

  /* Add staticmethods and classmethods. */
  {
    const PointerRNA func_ptr = {NULL, srna, NULL};
    const ListBase *lb;
    Link *link;

    lb = RNA_struct_type_functions(srna);
    for (link = lb->first; link; link = link->next) {
      FunctionRNA *func = (FunctionRNA *)link;
      const int flag = RNA_function_flag(func);
      if ((flag & FUNC_NO_SELF) &&         /* Is staticmethod or classmethod. */
          (flag & FUNC_REGISTER) == false) /* Is not for registration. */
      {
        /* We may want to set the type of this later. */
        PyObject *func_py = pyrna_func_to_py(&func_ptr, func);
        PyObject_SetAttrString(newclass, RNA_function_identifier(func), func_py);
        Py_DECREF(func_py);
      }
    }
  }

  /* Done with RNA instance. */
}

static PyObject *pyrna_srna_Subtype(StructRNA *srna);

/* Return a borrowed reference. */
static PyObject *pyrna_srna_PyBase(StructRNA *srna)  //, PyObject *bpy_types_dict)
{
  /* Assume RNA_struct_py_type_get(srna) was already checked. */
  StructRNA *base;

  PyObject *py_base = NULL;

  /* Get the base type. */
  base = RNA_struct_base(srna);

  if (base && base != srna) {
    // printf("debug subtype %s %p\n", RNA_struct_identifier(srna), srna);
    py_base = pyrna_srna_Subtype(base);  //, bpy_types_dict);
    Py_DECREF(py_base);                  /* Srna owns, this is only to pass as an arg. */
  }

  if (py_base == NULL) {
    py_base = (PyObject *)&pyrna_struct_Type;
  }

  return py_base;
}

/* Check if we have a native Python subclass, use it when it exists
 * return a borrowed reference. */
static PyObject *bpy_types_dict = NULL;

static PyObject *pyrna_srna_ExternalType(StructRNA *srna)
{
  const char *idname = RNA_struct_identifier(srna);
  PyObject *newclass;

  if (bpy_types_dict == NULL) {
    PyObject *bpy_types = PyImport_ImportModuleLevel("bpy_types", NULL, NULL, NULL, 0);

    if (bpy_types == NULL) {
      PyErr_Print();
      PyErr_Clear();
      CLOG_ERROR(BPY_LOG_RNA, "failed to find 'bpy_types' module");
      return NULL;
    }
    bpy_types_dict = PyModule_GetDict(bpy_types); /* Borrow. */
    Py_DECREF(bpy_types);                         /* Fairly safe to assume the dict is kept. */
  }

  newclass = PyDict_GetItemString(bpy_types_dict, idname);

  /* Sanity check, could skip this unless in debug mode. */
  if (newclass) {
    PyObject *base_compare = pyrna_srna_PyBase(srna);
    /* Can't do this because it gets superclasses values! */
    // PyObject *slots = PyObject_GetAttrString(newclass, "__slots__");
    /* Can do this, but faster not to. */
    // PyObject *bases = PyObject_GetAttrString(newclass, "__bases__");
    PyObject *tp_bases = ((PyTypeObject *)newclass)->tp_bases;
    PyObject *tp_slots = PyDict_GetItem(((PyTypeObject *)newclass)->tp_dict,
                                        bpy_intern_str___slots__);

    if (tp_slots == NULL) {
      CLOG_ERROR(
          BPY_LOG_RNA, "expected class '%s' to have __slots__ defined, see bpy_types.py", idname);
      newclass = NULL;
    }
    else if (PyTuple_GET_SIZE(tp_bases)) {
      PyObject *base = PyTuple_GET_ITEM(tp_bases, 0);

      if (base_compare != base) {
        char pyob_info[256];
        PyC_ObSpitStr(pyob_info, sizeof(pyob_info), base_compare);
        CLOG_ERROR(BPY_LOG_RNA,
                   "incorrect subclassing of SRNA '%s', expected '%s', see bpy_types.py",
                   idname,
                   pyob_info);
        newclass = NULL;
      }
      else {
        CLOG_INFO(BPY_LOG_RNA, 2, "SRNA sub-classed: '%s'", idname);
      }
    }
  }

  return newclass;
}

static PyObject *pyrna_srna_Subtype(StructRNA *srna)
{
  PyObject *newclass = NULL;

  /* Stupid/simple case. */
  if (srna == NULL) {
    newclass = NULL; /* Nothing to do. */
  }                  /* The class may have already been declared & allocated. */
  else if ((newclass = RNA_struct_py_type_get(srna))) {
    Py_INCREF(newclass);
  } /* Check if bpy_types.py module has the class defined in it. */
  else if ((newclass = pyrna_srna_ExternalType(srna))) {
    pyrna_subtype_set_rna(newclass, srna);
    Py_INCREF(newclass);
  } /* create a new class instance with the C api
     * mainly for the purposing of matching the C/RNA type hierarchy */
  else {
    /* subclass equivalents
     * - class myClass(myBase):
     *     some = 'value' # or ...
     * - myClass = type(
     *       name='myClass',
     *       bases=(myBase,), dict={'__module__': 'bpy.types', '__slots__': ()}
     *   )
     */

    /* Assume RNA_struct_py_type_get(srna) was already checked. */
    PyObject *py_base = pyrna_srna_PyBase(srna);
    PyObject *metaclass;
    const char *idname = RNA_struct_identifier(srna);

    /* Remove __doc__ for now. */
    // const char *descr = RNA_struct_ui_description(srna);
    // if (!descr) descr = "(no docs)";
    // "__doc__", descr

    if (RNA_struct_idprops_check(srna) &&
        !PyObject_IsSubclass(py_base, (PyObject *)&pyrna_struct_meta_idprop_Type)) {
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
      PyTuple_SET_ITEM(item, 0, Py_INCREF_RET(py_base));

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

    /* PyC_ObSpit("new class ref", newclass); */

    if (newclass) {
      /* srna owns one, and the other is owned by the caller. */
      pyrna_subtype_set_rna(newclass, srna);

      /* XXX, adding this back segfaults Blender on load. */
      // Py_DECREF(newclass); /* let srna own */
    }
    else {
      /* This should not happen. */
      CLOG_ERROR(BPY_LOG_RNA, "failed to register '%s'", idname);
      PyErr_Print();
      PyErr_Clear();
    }
  }

  return newclass;
}

/* Use for subtyping so we know which srna is used for a PointerRNA. */
static StructRNA *srna_from_ptr(PointerRNA *ptr)
{
  if (ptr->type == &RNA_Struct) {
    return ptr->data;
  }

  return ptr->type;
}

/* Always returns a new ref, be sure to decref when done. */
static PyObject *pyrna_struct_Subtype(PointerRNA *ptr)
{
  return pyrna_srna_Subtype(srna_from_ptr(ptr));
}

/*-----------------------CreatePyObject---------------------------------*/
PyObject *pyrna_struct_CreatePyObject(PointerRNA *ptr)
{
  BPy_StructRNA *pyrna = NULL;

  /* Note: don't rely on this to return None since NULL data with a valid type can often crash. */
  if (ptr->data == NULL && ptr->type == NULL) { /* Operator RNA has NULL data. */
    Py_RETURN_NONE;
  }

  /* New in 2.8x, since not many types support instancing
   * we may want to use a flag to avoid looping over all classes. - campbell */
  void **instance = ptr->data ? RNA_struct_instance(ptr) : NULL;
  if (instance && *instance) {
    pyrna = *instance;

    /* Refine may have changed types after the first instance was created. */
    if (ptr->type == pyrna->ptr.type) {
      Py_INCREF(pyrna);
      return (PyObject *)pyrna;
    }

    /* Existing users will need to use 'type_recast' method. */
    Py_DECREF(pyrna);
    *instance = NULL;
    /* Continue as if no instance was made. */
#if 0 /* No need to assign, will be written to next... */
      pyrna = NULL;
#endif
  }

  {
    PyTypeObject *tp = (PyTypeObject *)pyrna_struct_Subtype(ptr);

    if (tp) {
      pyrna = (BPy_StructRNA *)tp->tp_alloc(tp, 0);
#ifdef USE_PYRNA_STRUCT_REFERENCE
      /* #PyType_GenericAlloc will have set tracking.
       * We only want tracking when `StructRNA.reference` has been set. */
      if (pyrna != NULL) {
        PyObject_GC_UnTrack(pyrna);
      }
#endif
      Py_DECREF(tp); /* srna owns, can't hold a reference. */
    }
    else {
      CLOG_WARN(BPY_LOG_RNA, "could not make type '%s'", RNA_struct_identifier(ptr->type));

#ifdef USE_PYRNA_STRUCT_REFERENCE
      pyrna = (BPy_StructRNA *)PyObject_GC_New(BPy_StructRNA, &pyrna_struct_Type);
#else
      pyrna = (BPy_StructRNA *)PyObject_New(BPy_StructRNA, &pyrna_struct_Type);
#endif

#ifdef USE_WEAKREFS
      if (pyrna != NULL) {
        pyrna->in_weakreflist = NULL;
      }
#endif
    }
  }

  if (pyrna == NULL) {
    PyErr_SetString(PyExc_MemoryError, "couldn't create bpy_struct object");
    return NULL;
  }

  /* Blender's instance owns a reference (to avoid Python freeing it). */
  if (instance) {
    *instance = pyrna;
    Py_INCREF(pyrna);
  }

  pyrna->ptr = *ptr;
#ifdef PYRNA_FREE_SUPPORT
  pyrna->freeptr = false;
#endif

#ifdef USE_PYRNA_STRUCT_REFERENCE
  pyrna->reference = NULL;
#endif

  // PyC_ObSpit("NewStructRNA: ", (PyObject *)pyrna);

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
  if (ptr->owner_id) {
    id_weakref_pool_add(ptr->owner_id, (BPy_DummyPointerRNA *)pyrna);
  }
#endif
  return (PyObject *)pyrna;
}

PyObject *pyrna_prop_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop)
{
  BPy_PropertyRNA *pyrna;

  if (RNA_property_array_check(prop) == 0) {
    PyTypeObject *type;

    if (RNA_property_type(prop) != PROP_COLLECTION) {
      type = &pyrna_prop_Type;
    }
    else {
      if ((RNA_property_flag(prop) & PROP_IDPROPERTY) == 0) {
        type = &pyrna_prop_collection_Type;
      }
      else {
        type = &pyrna_prop_collection_idprop_Type;
      }
    }

    pyrna = (BPy_PropertyRNA *)PyObject_NEW(BPy_PropertyRNA, type);
#ifdef USE_WEAKREFS
    pyrna->in_weakreflist = NULL;
#endif
  }
  else {
    pyrna = (BPy_PropertyRNA *)PyObject_NEW(BPy_PropertyArrayRNA, &pyrna_prop_array_Type);
    ((BPy_PropertyArrayRNA *)pyrna)->arraydim = 0;
    ((BPy_PropertyArrayRNA *)pyrna)->arrayoffset = 0;
#ifdef USE_WEAKREFS
    ((BPy_PropertyArrayRNA *)pyrna)->in_weakreflist = NULL;
#endif
  }

  if (pyrna == NULL) {
    PyErr_SetString(PyExc_MemoryError, "couldn't create BPy_rna object");
    return NULL;
  }

  pyrna->ptr = *ptr;
  pyrna->prop = prop;

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
  if (ptr->owner_id) {
    id_weakref_pool_add(ptr->owner_id, (BPy_DummyPointerRNA *)pyrna);
  }
#endif

  return (PyObject *)pyrna;
}

/* Utility func to be used by external modules, sneaky! */
PyObject *pyrna_id_CreatePyObject(ID *id)
{
  if (id) {
    PointerRNA ptr;
    RNA_id_pointer_create(id, &ptr);
    return pyrna_struct_CreatePyObject(&ptr);
  }

  Py_RETURN_NONE;
}

bool pyrna_id_FromPyObject(PyObject *obj, ID **id)
{
  if (pyrna_id_CheckPyObject(obj)) {
    *id = ((BPy_StructRNA *)obj)->ptr.owner_id;
    return true;
  }

  *id = NULL;
  return false;
}

bool pyrna_id_CheckPyObject(PyObject *obj)
{
  return BPy_StructRNA_Check(obj) && (RNA_struct_is_ID(((BPy_StructRNA *)obj)->ptr.type));
}

void BPY_rna_init(void)
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
}

/* 'bpy.data' from Python. */
static PointerRNA *rna_module_ptr = NULL;
PyObject *BPY_rna_module(void)
{
  BPy_StructRNA *pyrna;
  PointerRNA ptr;

  /* For now, return the base RNA type rather than a real module. */
  RNA_main_pointer_create(G_MAIN, &ptr);
  pyrna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);

  rna_module_ptr = &pyrna->ptr;
  return (PyObject *)pyrna;
}

void BPY_update_rna_module(void)
{
  if (rna_module_ptr) {
#if 0
    RNA_main_pointer_create(G_MAIN, rna_module_ptr);
#else
    rna_module_ptr->data = G_MAIN; /* Just set data is enough. */
#endif
  }
}

#if 0
/* This is a way we can access doc-strings for RNA types
 * without having the data-types in Blender. */
PyObject *BPY_rna_doc(void)
{
  PointerRNA ptr;

  /* For now, return the base RNA type rather than a real module. */
  RNA_blender_rna_pointer_create(&ptr);

  return pyrna_struct_CreatePyObject(&ptr);
}
#endif

/* -------------------------------------------------------------------- */
/** \name RNA Types Module `bpy.types`
 * \{ */

/**
 * This could be a static variable as we only have one `bpy.types` module,
 * it just keeps the data isolated to store in the module it's self.
 *
 * This data doesn't change one initialized.
 */
struct BPy_TypesModule_State {
  /** `RNA_BlenderRNA`. */
  PointerRNA ptr;
  /** `RNA_BlenderRNA.structs`, exposed as `bpy.types` */
  PropertyRNA *prop;
};

static PyObject *bpy_types_module_getattro(PyObject *self, PyObject *pyname)
{
  struct BPy_TypesModule_State *state = PyModule_GetState(self);
  PointerRNA newptr;
  PyObject *ret;
  const char *name = PyUnicode_AsUTF8(pyname);

  if (name == NULL) {
    PyErr_SetString(PyExc_AttributeError, "bpy.types: __getattr__ must be a string");
    ret = NULL;
  }
  else if (RNA_property_collection_lookup_string(&state->ptr, state->prop, name, &newptr)) {
    ret = pyrna_struct_Subtype(&newptr);
    if (ret == NULL) {
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
    return NULL;
#endif
    /* The error raised here will be displayed. */
    ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
  }

  return ret;
}

static PyObject *bpy_types_module_dir(PyObject *self)
{
  struct BPy_TypesModule_State *state = PyModule_GetState(self);
  PyObject *ret = PyList_New(0);

  RNA_PROP_BEGIN (&state->ptr, itemptr, state->prop) {
    StructRNA *srna = itemptr.data;
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

static struct PyMethodDef bpy_types_module_methods[] = {
    {"__getattr__", (PyCFunction)bpy_types_module_getattro, METH_O, NULL},
    {"__dir__", (PyCFunction)bpy_types_module_dir, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(bpy_types_module_doc, "Access to internal Blender types");
static struct PyModuleDef bpy_types_module_def = {
    PyModuleDef_HEAD_INIT,
    "bpy.types",                          /* m_name */
    bpy_types_module_doc,                 /* m_doc */
    sizeof(struct BPy_TypesModule_State), /* m_size */
    bpy_types_module_methods,             /* m_methods */
    NULL,                                 /* m_reload */
    NULL,                                 /* m_traverse */
    NULL,                                 /* m_clear */
    NULL,                                 /* m_free */
};

/**
 * Accessed from Python as 'bpy.types'
 */
PyObject *BPY_rna_types(void)
{
  PyObject *submodule = PyModule_Create(&bpy_types_module_def);
  struct BPy_TypesModule_State *state = PyModule_GetState(submodule);

  RNA_blender_rna_pointer_create(&state->ptr);
  state->prop = RNA_struct_find_property(&state->ptr, "structs");

  /* Internal base types we have no other accessors for. */
  {
    static PyTypeObject *pyrna_types[] = {
        &pyrna_struct_meta_idprop_Type,
        &pyrna_struct_Type,
        &pyrna_prop_Type,
        &pyrna_prop_array_Type,
        &pyrna_prop_collection_Type,
        &pyrna_func_Type,
    };

    PyObject *submodule_dict = PyModule_GetDict(submodule);
    for (int i = 0; i < ARRAY_SIZE(pyrna_types); i += 1) {
      PyDict_SetItemString(submodule_dict, pyrna_types[i]->tp_name, (PyObject *)pyrna_types[i]);
    }
  }

  return submodule;
}

/** \} */

StructRNA *pyrna_struct_as_srna(PyObject *self, const bool parent, const char *error_prefix)
{
  BPy_StructRNA *py_srna = NULL;
  StructRNA *srna;

  /* Unfortunately PyObject_GetAttrString wont look up this types tp_dict first :/ */
  if (PyType_Check(self)) {
    py_srna = (BPy_StructRNA *)PyDict_GetItem(((PyTypeObject *)self)->tp_dict,
                                              bpy_intern_str_bl_rna);
    Py_XINCREF(py_srna);
  }

  if (parent) {
    /* be very careful with this since it will return a parent classes srna.
     * modifying this will do confusing stuff! */
    if (py_srna == NULL) {
      py_srna = (BPy_StructRNA *)PyObject_GetAttr(self, bpy_intern_str_bl_rna);
    }
  }

  if (py_srna == NULL) {
    PyErr_Format(PyExc_RuntimeError,
                 "%.200s, missing bl_rna attribute from '%.200s' instance (may not be registered)",
                 error_prefix,
                 Py_TYPE(self)->tp_name);
    return NULL;
  }

  if (!BPy_StructRNA_Check(py_srna)) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s, bl_rna attribute wrong type '%.200s' on '%.200s'' instance",
                 error_prefix,
                 Py_TYPE(py_srna)->tp_name,
                 Py_TYPE(self)->tp_name);
    Py_DECREF(py_srna);
    return NULL;
  }

  if (py_srna->ptr.type != &RNA_Struct) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s, bl_rna attribute not a RNA_Struct, on '%.200s'' instance",
                 error_prefix,
                 Py_TYPE(self)->tp_name);
    Py_DECREF(py_srna);
    return NULL;
  }

  srna = py_srna->ptr.data;
  Py_DECREF(py_srna);

  return srna;
}

/* Orphan functions, not sure where they should go. */
/* Get the srna for methods attached to types. */
/*
 * Caller needs to raise error.*/
StructRNA *srna_from_self(PyObject *self, const char *error_prefix)
{

  if (self == NULL) {
    return NULL;
  }
  if (PyCapsule_CheckExact(self)) {
    return PyCapsule_GetPointer(self, NULL);
  }
  if (PyType_Check(self) == 0) {
    return NULL;
  }

  /* These cases above not errors, they just mean the type was not compatible
   * After this any errors will be raised in the script */

  PyObject *error_type, *error_value, *error_traceback;
  StructRNA *srna;

  PyErr_Fetch(&error_type, &error_value, &error_traceback);
  PyErr_Clear();

  srna = pyrna_struct_as_srna(self, false, error_prefix);

  if (!PyErr_Occurred()) {
    PyErr_Restore(error_type, error_value, error_traceback);
  }

  return srna;
}

static int deferred_register_prop(StructRNA *srna, PyObject *key, PyObject *item)
{
  if (!BPy_PropDeferred_CheckTypeExact(item)) {
    /* No error, ignoring. */
    return 0;
  }

  /* We only care about results from C which
   * are for sure types, save some time with error */
  PyObject *py_func = ((BPy_PropDeferred *)item)->fn;
  PyObject *py_kw = ((BPy_PropDeferred *)item)->kw;
  PyObject *py_srna_cobject, *py_ret;

  PyObject *args_fake;

  if (*PyUnicode_AsUTF8(key) == '_') {
    PyErr_Format(PyExc_ValueError,
                 "bpy_struct \"%.200s\" registration error: "
                 "%.200s could not register because the property starts with an '_'\n",
                 RNA_struct_identifier(srna),
                 PyUnicode_AsUTF8(key));
    return -1;
  }
  py_srna_cobject = PyCapsule_New(srna, NULL, NULL);

  /* Not 100% nice :/, modifies the dict passed, should be ok. */
  PyDict_SetItem(py_kw, bpy_intern_str_attr, key);

  args_fake = PyTuple_New(1);
  PyTuple_SET_ITEM(args_fake, 0, py_srna_cobject);

  PyObject *type = PyDict_GetItemString(py_kw, "type");
  StructRNA *type_srna = srna_from_self(type, "");
  if (type_srna) {
    if (!RNA_struct_idprops_datablock_allowed(srna) &&
        (*(PyCFunctionWithKeywords)PyCFunction_GET_FUNCTION(py_func) == BPy_PointerProperty ||
         *(PyCFunctionWithKeywords)PyCFunction_GET_FUNCTION(py_func) == BPy_CollectionProperty) &&
        RNA_struct_idprops_contains_datablock(type_srna)) {
      PyErr_Format(PyExc_ValueError,
                   "bpy_struct \"%.200s\" doesn't support datablock properties\n",
                   RNA_struct_identifier(srna));
      return -1;
    }
  }

  py_ret = PyObject_Call(py_func, args_fake, py_kw);

  if (py_ret) {
    Py_DECREF(py_ret);
    Py_DECREF(args_fake); /* Free's py_srna_cobject too. */
  }
  else {
    /* _must_ print before decreffing args_fake. */
    PyErr_Print();
    PyErr_Clear();

    Py_DECREF(args_fake); /* Free's py_srna_cobject too. */

    // PyC_LineSpit();
    PyErr_Format(PyExc_ValueError,
                 "bpy_struct \"%.200s\" registration error: "
                 "%.200s could not register\n",
                 RNA_struct_identifier(srna),
                 PyUnicode_AsUTF8(key));
    return -1;
  }

  return 0;
}

/**
 * Extract `__annotations__` using `typing.get_type_hints` which handles the delayed evaluation.
 */
static int pyrna_deferred_register_class_from_type_hints(StructRNA *srna, PyTypeObject *py_class)
{
  PyObject *annotations_dict = NULL;

  /* `typing.get_type_hints(py_class)` */
  {
    PyObject *typing_mod = PyImport_ImportModuleLevel("typing", NULL, NULL, NULL, 0);
    if (typing_mod != NULL) {
      PyObject *get_type_hints_fn = PyObject_GetAttrString(typing_mod, "get_type_hints");
      if (get_type_hints_fn != NULL) {
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
  if (annotations_dict != NULL) {
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
      /* Should never happen, an error wont have been raised, so raise one. */
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
      PyDict_CheckExact(annotations_dict)) {
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
        !PyObject_IsSubclass((PyObject *)py_superclass, (PyObject *)&pyrna_struct_Type)) {
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
  if (!RNA_struct_idprops_register_check(srna)) {
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

/*-------------------- Type Registration ------------------------*/

static int rna_function_arg_count(FunctionRNA *func, int *min_count)
{
  const ListBase *lb = RNA_function_defined_parameters(func);
  PropertyRNA *parm;
  Link *link;
  const int flag = RNA_function_flag(func);
  const bool is_staticmethod = (flag & FUNC_NO_SELF) && !(flag & FUNC_USE_SELF_TYPE);
  int count = is_staticmethod ? 0 : 1;
  bool done_min_count = false;

  for (link = lb->first; link; link = link->next) {
    parm = (PropertyRNA *)link;
    if (!(RNA_parameter_flag(parm) & PARM_OUTPUT)) {
      if (!done_min_count && (RNA_parameter_flag(parm) & PARM_PYFUNC_OPTIONAL)) {
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

static int bpy_class_validate_recursive(PointerRNA *dummyptr,
                                        StructRNA *srna,
                                        void *py_data,
                                        int *have_function)
{
  const ListBase *lb;
  Link *link;
  const char *class_type = RNA_struct_identifier(srna);
  StructRNA *srna_base = RNA_struct_base(srna);
  PyObject *py_class = (PyObject *)py_data;
  PyObject *base_class = RNA_struct_py_type_get(srna);
  PyObject *item;
  int i, arg_count, func_arg_count, func_arg_min_count = 0;
  const char *py_class_name = ((PyTypeObject *)py_class)->tp_name; /* __name__ */

  if (srna_base) {
    if (bpy_class_validate_recursive(dummyptr, srna_base, py_data, have_function) != 0) {
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
  for (link = lb->first; link; link = link->next) {
    FunctionRNA *func = (FunctionRNA *)link;
    const int flag = RNA_function_flag(func);
    if (!(flag & FUNC_REGISTER)) {
      continue;
    }

    item = PyObject_GetAttrString(py_class, RNA_function_identifier(func));
    have_function[i] = (item != NULL);
    i++;

    if (item == NULL) {
      if ((flag & (FUNC_REGISTER_OPTIONAL & ~FUNC_REGISTER)) == 0) {
        PyErr_Format(PyExc_AttributeError,
                     "expected %.200s, %.200s class to have an \"%.200s\" attribute",
                     class_type,
                     py_class_name,
                     RNA_function_identifier(func));
        return -1;
      }
      PyErr_Clear();

      continue;
    }

    /* TODO(campbell): this is used for classmethod's too,
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

    func_arg_count = rna_function_arg_count(func, &func_arg_min_count);

    if (func_arg_count >= 0) { /* -1 if we don't care. */
      arg_count = ((PyCodeObject *)PyFunction_GET_CODE(item))->co_argcount;

      /* note, the number of args we check for and the number of args we give to
       * '@staticmethods' are different (quirk of Python),
       * this is why rna_function_arg_count() doesn't return the value -1*/
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

  /* Verify properties. */
  lb = RNA_struct_type_properties(srna);
  for (link = lb->first; link; link = link->next) {
    const char *identifier;
    PropertyRNA *prop = (PropertyRNA *)link;
    const int flag = RNA_property_flag(prop);

    if (!(flag & PROP_REGISTER)) {
      continue;
    }

    /* TODO(campbell): Use Python3.7x _PyObject_LookupAttr(), also in the macro below. */
    identifier = RNA_property_identifier(prop);
    item = PyObject_GetAttrString(py_class, identifier);

    if (item == NULL) {
      PyErr_Clear();
      /* Sneaky workaround to use the class name as the bl_idname. */

#define BPY_REPLACEMENT_STRING(rna_attr, py_attr) \
  else if (STREQ(identifier, rna_attr)) \
  { \
    if ((item = PyObject_GetAttr(py_class, py_attr))) { \
      if (item != Py_None) { \
        if (pyrna_py_to_prop(dummyptr, prop, NULL, item, "validating class:") != 0) { \
          Py_DECREF(item); \
          return -1; \
        } \
      } \
      Py_DECREF(item); \
    } \
    else { \
      PyErr_Clear(); \
    } \
  } /* Intentionally allow else here. */

      if (false) {
      } /* Needed for macro. */
      BPY_REPLACEMENT_STRING("bl_idname", bpy_intern_str___name__)
      BPY_REPLACEMENT_STRING("bl_description", bpy_intern_str___doc__)

#undef BPY_REPLACEMENT_STRING

      if (item == NULL && (((flag & PROP_REGISTER_OPTIONAL) != PROP_REGISTER_OPTIONAL))) {
        PyErr_Format(PyExc_AttributeError,
                     "expected %.200s, %.200s class to have an \"%.200s\" attribute",
                     class_type,
                     py_class_name,
                     identifier);
        return -1;
      }

      PyErr_Clear();
    }
    else {
      if (pyrna_py_to_prop(dummyptr, prop, NULL, item, "validating class:") != 0) {
        Py_DECREF(item);
        return -1;
      }
      Py_DECREF(item);
    }
  }

  return 0;
}

static int bpy_class_validate(PointerRNA *dummyptr, void *py_data, int *have_function)
{
  return bpy_class_validate_recursive(dummyptr, dummyptr->type, py_data, have_function);
}

/* TODO - multiple return values like with RNA functions. */
static int bpy_class_call(bContext *C, PointerRNA *ptr, FunctionRNA *func, ParameterList *parms)
{
  PyObject *args;
  PyObject *ret = NULL, *py_srna = NULL, *py_class_instance = NULL, *parmitem;
  PyTypeObject *py_class;
  PropertyRNA *parm;
  ParameterIterator iter;
  PointerRNA funcptr;
  int err = 0, i, ret_len = 0, arg_count;
  const int flag = RNA_function_flag(func);
  const bool is_staticmethod = (flag & FUNC_NO_SELF) && !(flag & FUNC_USE_SELF_TYPE);
  const bool is_classmethod = (flag & FUNC_NO_SELF) && (flag & FUNC_USE_SELF_TYPE);

  PropertyRNA *pret_single = NULL;
  void *retdata_single = NULL;

  PyGILState_STATE gilstate;

#ifdef USE_PEDANTIC_WRITE
  const bool is_readonly_init = !(RNA_struct_is_a(ptr->type, &RNA_Operator) ||
                                  RNA_struct_is_a(ptr->type, &RNA_Gizmo));
  // const char *func_id = RNA_function_identifier(func);  /* UNUSED */
  /* Testing, for correctness, not operator and not draw function. */
  const bool is_readonly = !(RNA_function_flag(func) & FUNC_ALLOW_WRITE);
#endif

  py_class = RNA_struct_py_type_get(ptr->type);
  /* Rare case. can happen when registering subclasses. */
  if (py_class == NULL) {
    CLOG_WARN(BPY_LOG_RNA,
              "unable to get Python class for RNA struct '%.200s'",
              RNA_struct_identifier(ptr->type));
    return -1;
  }

  /* XXX, this is needed because render engine calls without a context
   * this should be supported at some point, but at the moment it's not! */
  if (C == NULL) {
    C = BPY_context_get();
  }

  /* Annoying! We need to check if the screen gets set to NULL which is a
   * hint that the file was actually re-loaded. */
  const bool is_valid_wm = (CTX_wm_manager(C) != NULL);

  bpy_context_set(C, &gilstate);

  if (!(is_staticmethod || is_classmethod)) {
    /* Some datatypes (operator, render engine) can store PyObjects for re-use. */
    if (ptr->data) {
      void **instance = RNA_struct_instance(ptr);

      if (instance) {
        if (*instance) {
          py_class_instance = *instance;
          Py_INCREF(py_class_instance);
        }
      }
    }
    /* End exception. */

    if (py_class_instance == NULL) {
      py_srna = pyrna_struct_CreatePyObject(ptr);
    }

    if (py_class_instance) {
      /* Special case, instance is cached. */
    }
    else if (py_srna == NULL) {
      py_class_instance = NULL;
    }
    else if (py_srna == Py_None) { /* Probably wont ever happen, but possible. */
      Py_DECREF(py_srna);
      py_class_instance = NULL;
    }
    else {
#if 1
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
        if (py_class->tp_init(py_srna, args, NULL) < 0) {
          Py_DECREF(py_srna);
          py_srna = NULL;
          /* Err set below. */
        }
        Py_DECREF(args);
#  ifdef USE_PEDANTIC_WRITE
        rna_disallow_writes = prev_write;
#  endif
      }
      py_class_instance = py_srna;

#else
      const int prev_write = rna_disallow_writes;
      rna_disallow_writes = true;

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
      args = PyTuple_New(1);
      PyTuple_SET_ITEM(args, 0, py_srna);
      py_class_instance = PyObject_Call(py_class, args, NULL);
      Py_DECREF(args);

      rna_disallow_writes = prev_write;

#endif

      if (py_class_instance == NULL) {
        err = -1; /* So the error is not overridden below. */
      }
    }
  }

  /* Initializing the class worked, now run its invoke function. */
  if (err != -1 && (is_staticmethod || is_classmethod || py_class_instance)) {
    PyObject *item = PyObject_GetAttrString((PyObject *)py_class, RNA_function_identifier(func));

    if (item) {
      RNA_pointer_create(NULL, &RNA_Function, func, &funcptr);

      if (is_staticmethod) {
        arg_count =
            ((PyCodeObject *)PyFunction_GET_CODE(((PyMethodObject *)item)->im_func))->co_argcount -
            1;
      }
      else {
        arg_count = ((PyCodeObject *)PyFunction_GET_CODE(item))->co_argcount;
      }
#if 0
      /* First arg is included in 'item'. */
      args = PyTuple_New(rna_function_arg_count(func));
#endif
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
          if (pret_single == NULL) {
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
      rna_disallow_writes = is_readonly ? true : false;
#endif
      /* *** Main Caller *** */

      ret = PyObject_Call(item, args, NULL);

      /* *** Done Calling *** */

#ifdef USE_PEDANTIC_WRITE
      rna_disallow_writes = false;
#endif

      RNA_parameter_list_end(&iter);
      Py_DECREF(item);
      Py_DECREF(args);
    }
    else {
      PyErr_Print();
      PyErr_Clear();
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
                   "could not create instance of %.200s to call callback function %.200s",
                   RNA_struct_identifier(ptr->type),
                   RNA_function_identifier(func));
      err = -1;
    }
  }

  if (ret == NULL) { /* Covers py_class_instance failing too. */
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

      /* when calling operator funcs only gives Function.result with
       * no line number since the func has finished calling on error,
       * re-raise the exception with more info since it would be slow to
       * create prefix on every call (when there are no errors) */
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
    /* Alert the user, else they wont know unless they see the console. */
    if ((!is_staticmethod) && (!is_classmethod) && (ptr->data) &&
        (RNA_struct_is_a(ptr->type, &RNA_Operator)) &&
        (is_valid_wm == (CTX_wm_manager(C) != NULL))) {
      wmOperator *op = ptr->data;
      reports = op->reports;
    }
    else {
      /* Wont alert users, but they can view in 'info' space. */
      reports = CTX_wm_reports(C);
    }

    BPy_errors_to_report(reports);

    /* Also print in the console for Python. */
    PyErr_Print();
    PyErr_Clear();
  }

  bpy_context_clear(C, &gilstate);

  return err;
}

static void bpy_class_free(void *pyob_ptr)
{
  PyObject *self = (PyObject *)pyob_ptr;
  PyGILState_STATE gilstate;

  gilstate = PyGILState_Ensure();

  /* Breaks re-registering classes. */
  // PyDict_Clear(((PyTypeObject *)self)->tp_dict);

  /* Remove the RNA attribute instead. */
  PyDict_DelItem(((PyTypeObject *)self)->tp_dict, bpy_intern_str_bl_rna);
  if (PyErr_Occurred()) {
    PyErr_Clear();
  }

#if 0 /* Needs further investigation, too annoying so quiet for now. */
  if (G.debug & G_DEBUG_PYTHON) {
    if (self->ob_refcnt > 1) {
      PyC_ObSpit("zombie class - reference should be 1", self);
    }
  }
#endif
  Py_DECREF((PyObject *)pyob_ptr);

  PyGILState_Release(gilstate);
}

/**
 * \note This isn't essential to run on startup, since subtypes will lazy initialize.
 * But keep running in debug mode so we get immediate notification of bad class hierarchy
 * or any errors in "bpy_types.py" at load time, so errors don't go unnoticed.
 */
void pyrna_alloc_types(void)
{
#ifdef DEBUG
  PyGILState_STATE gilstate;

  PointerRNA ptr;
  PropertyRNA *prop;

  gilstate = PyGILState_Ensure();

  /* Avoid doing this lookup for every getattr. */
  RNA_blender_rna_pointer_create(&ptr);
  prop = RNA_struct_find_property(&ptr, "structs");

  RNA_PROP_BEGIN (&ptr, itemptr, prop) {
    PyObject *item = pyrna_struct_Subtype(&itemptr);
    if (item == NULL) {
      if (PyErr_Occurred()) {
        PyErr_Print();
        PyErr_Clear();
      }
    }
    else {
      Py_DECREF(item);
    }
  }
  RNA_PROP_END;

  PyGILState_Release(gilstate);
#endif /* DEBUG */
}

void pyrna_free_types(void)
{
  PointerRNA ptr;
  PropertyRNA *prop;

  /* Avoid doing this lookup for every getattr. */
  RNA_blender_rna_pointer_create(&ptr);
  prop = RNA_struct_find_property(&ptr, "structs");

  RNA_PROP_BEGIN (&ptr, itemptr, prop) {
    StructRNA *srna = srna_from_ptr(&itemptr);
    void *py_ptr = RNA_struct_py_type_get(srna);

    if (py_ptr) {
#if 0 /* XXX - should be able to do this, but makes Python crash on exit. */
      bpy_class_free(py_ptr);
#endif
      RNA_struct_py_type_set(srna, NULL);
    }
  }
  RNA_PROP_END;
}

/**
 * \warning memory leak!
 *
 * There is currently a bug where moving the registration of a Python class does
 * not properly manage reference-counts from the Python class. As the `srna` owns
 * the Python class this should not be so tricky, but changing the references as
 * you'd expect when changing ownership crashes blender on exit so I had to comment out
 * the #Py_DECREF. This is not so bad because the leak only happens when re-registering
 * (continuously running `SCRIPT_OT_reload`).
 * - Should still be fixed - Campbell
 */
PyDoc_STRVAR(pyrna_register_class_doc,
             ".. method:: register_class(cls)\n"
             "\n"
             "   Register a subclass of a Blender type class.\n"
             "\n"
             "   :arg cls: Blender type class in:\n"
             "      :class:`bpy.types.Panel`, :class:`bpy.types.UIList`,\n"
             "      :class:`bpy.types.Menu`, :class:`bpy.types.Header`,\n"
             "      :class:`bpy.types.Operator`, :class:`bpy.types.KeyingSetInfo`,\n"
             "      :class:`bpy.types.RenderEngine`\n"
             "   :type cls: class\n"
             "   :raises ValueError:\n"
             "      if the class is not a subclass of a registerable blender class.\n"
             "\n"
             "   .. note::\n"
             "\n"
             "      If the class has a *register* class method it will be called\n"
             "      before registration.\n");
PyMethodDef meth_bpy_register_class = {
    "register_class", pyrna_register_class, METH_O, pyrna_register_class_doc};
static PyObject *pyrna_register_class(PyObject *UNUSED(self), PyObject *py_class)
{
  bContext *C = NULL;
  ReportList reports;
  StructRegisterFunc reg;
  StructRNA *srna;
  StructRNA *srna_new;
  const char *identifier;
  PyObject *py_cls_meth;
  const char *error_prefix = "register_class(...):";

  if (!PyType_Check(py_class)) {
    PyErr_Format(PyExc_ValueError,
                 "register_class(...): "
                 "expected a class argument, not '%.200s'",
                 Py_TYPE(py_class)->tp_name);
    return NULL;
  }

  if (PyDict_GetItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna)) {
    PyErr_Format(PyExc_ValueError,
                 "register_class(...): "
                 "already registered as a subclass '%.200s'",
                 ((PyTypeObject *)py_class)->tp_name);
    return NULL;
  }

  if (!pyrna_write_check()) {
    PyErr_Format(PyExc_RuntimeError,
                 "register_class(...): "
                 "can't run in readonly state '%.200s'",
                 ((PyTypeObject *)py_class)->tp_name);
    return NULL;
  }

  /* Warning: gets parent classes srna, only for the register function. */
  srna = pyrna_struct_as_srna(py_class, true, "register_class(...):");
  if (srna == NULL) {
    return NULL;
  }

  /* Fails in some cases, so can't use this check, but would like to :| */
#if 0
  if (RNA_struct_py_type_get(srna)) {
    PyErr_Format(PyExc_ValueError,
                 "register_class(...): %.200s's parent class %.200s is already registered, this "
                 "is not allowed",
                 ((PyTypeObject *)py_class)->tp_name,
                 RNA_struct_identifier(srna));
    return NULL;
  }
#endif

  /* Check that we have a register callback for this type. */
  reg = RNA_struct_register(srna);

  if (!reg) {
    PyErr_Format(PyExc_ValueError,
                 "register_class(...): expected a subclass of a registerable "
                 "RNA type (%.200s does not support registration)",
                 RNA_struct_identifier(srna));
    return NULL;
  }

  /* Get the context, so register callback can do necessary refreshes. */
  C = BPY_context_get();

  /* Call the register callback with reports & identifier. */
  BKE_reports_init(&reports, RPT_STORE);

  identifier = ((PyTypeObject *)py_class)->tp_name;

  srna_new = reg(CTX_data_main(C),
                 &reports,
                 py_class,
                 identifier,
                 bpy_class_validate,
                 bpy_class_call,
                 bpy_class_free);

  if (!BLI_listbase_is_empty(&reports.list)) {
    const bool has_error = BPy_reports_to_error(&reports, PyExc_RuntimeError, false);
    if (!has_error) {
      BPy_reports_write_stdout(&reports, error_prefix);
    }
    BKE_reports_clear(&reports);
    if (has_error) {
      return NULL;
    }
  }

  /* Python errors validating are not converted into reports so the check above will fail.
   * the cause for returning NULL will be printed as an error */
  if (srna_new == NULL) {
    return NULL;
  }

  /* Takes a reference to 'py_class'. */
  pyrna_subtype_set_rna(py_class, srna_new);

  /* Old srna still references us, keep the check in case registering somehow can free it. */
  if (RNA_struct_py_type_get(srna)) {
    RNA_struct_py_type_set(srna, NULL);
#if 0
    /* Should be able to do this XXX since the old RNA adds a new ref. */
    Py_DECREF(py_class);
#endif
  }

  /* Can't use this because it returns a dict proxy
   *
   * item = PyObject_GetAttrString(py_class, "__dict__");
   */
  if (pyrna_deferred_register_class(srna_new, (PyTypeObject *)py_class) != 0) {
    return NULL;
  }

  /* Call classed register method.
   * Note that zero falls through, no attribute, no error. */
  switch (_PyObject_LookupAttr(py_class, bpy_intern_str_register, &py_cls_meth)) {
    case 1: {
      PyObject *ret = PyObject_CallObject(py_cls_meth, NULL);
      Py_DECREF(py_cls_meth);
      if (ret) {
        Py_DECREF(ret);
      }
      else {
        return NULL;
      }
      break;
    }
    case -1: {
      return NULL;
    }
  }

  Py_RETURN_NONE;
}

static int pyrna_srna_contains_pointer_prop_srna(StructRNA *srna_props,
                                                 StructRNA *srna,
                                                 const char **r_prop_identifier)
{
  PropertyRNA *prop;
  LinkData *link;

  /* Verify properties. */
  const ListBase *lb = RNA_struct_type_properties(srna);

  for (link = lb->first; link; link = link->next) {
    prop = (PropertyRNA *)link;
    if (RNA_property_type(prop) == PROP_POINTER && !RNA_property_builtin(prop)) {
      PointerRNA tptr;
      RNA_pointer_create(NULL, &RNA_Struct, srna_props, &tptr);

      if (RNA_property_pointer_type(&tptr, prop) == srna) {
        *r_prop_identifier = RNA_property_identifier(prop);
        return 1;
      }
    }
  }

  return 0;
}

PyDoc_STRVAR(pyrna_unregister_class_doc,
             ".. method:: unregister_class(cls)\n"
             "\n"
             "   Unload the Python class from blender.\n"
             "\n"
             "   If the class has an *unregister* class method it will be called\n"
             "   before unregistering.\n");
PyMethodDef meth_bpy_unregister_class = {
    "unregister_class",
    pyrna_unregister_class,
    METH_O,
    pyrna_unregister_class_doc,
};
static PyObject *pyrna_unregister_class(PyObject *UNUSED(self), PyObject *py_class)
{
  bContext *C = NULL;
  StructUnregisterFunc unreg;
  StructRNA *srna;
  PyObject *py_cls_meth;

  if (!PyType_Check(py_class)) {
    PyErr_Format(PyExc_ValueError,
                 "register_class(...): "
                 "expected a class argument, not '%.200s'",
                 Py_TYPE(py_class)->tp_name);
    return NULL;
  }

#if 0
  if (PyDict_GetItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna) == NULL) {
    PWM_cursor_wait(false);
    PyErr_SetString(PyExc_ValueError, "unregister_class(): not a registered as a subclass");
    return NULL;
  }
#endif

  if (!pyrna_write_check()) {
    PyErr_Format(PyExc_RuntimeError,
                 "unregister_class(...): "
                 "can't run in readonly state '%.200s'",
                 ((PyTypeObject *)py_class)->tp_name);
    return NULL;
  }

  srna = pyrna_struct_as_srna(py_class, false, "unregister_class(...):");
  if (srna == NULL) {
    return NULL;
  }

  /* Check that we have a unregister callback for this type. */
  unreg = RNA_struct_unregister(srna);

  if (!unreg) {
    PyErr_SetString(
        PyExc_ValueError,
        "unregister_class(...): "
        "expected a Type subclassed from a registerable RNA type (no unregister supported)");
    return NULL;
  }

  /* Call classed unregister method.
   * Note that zero falls through, no attribute, no error. */
  switch (_PyObject_LookupAttr(py_class, bpy_intern_str_unregister, &py_cls_meth)) {
    case 1: {
      PyObject *ret = PyObject_CallObject(py_cls_meth, NULL);
      Py_DECREF(py_cls_meth);
      if (ret) {
        Py_DECREF(ret);
      }
      else {
        return NULL;
      }
      break;
    }
    case -1: {
      return NULL;
    }
  }

  /* Should happen all the time, however it's very slow. */
  if (G.debug & G_DEBUG_PYTHON) {
    /* Remove all properties using this class. */
    StructRNA *srna_iter;
    PointerRNA ptr_rna;
    PropertyRNA *prop_rna;
    const char *prop_identifier = NULL;

    RNA_blender_rna_pointer_create(&ptr_rna);
    prop_rna = RNA_struct_find_property(&ptr_rna, "structs");

    /* Loop over all structs. */
    RNA_PROP_BEGIN (&ptr_rna, itemptr, prop_rna) {
      srna_iter = itemptr.data;
      if (pyrna_srna_contains_pointer_prop_srna(srna_iter, srna, &prop_identifier)) {
        break;
      }
    }
    RNA_PROP_END;

    if (prop_identifier) {
      PyErr_Format(PyExc_RuntimeError,
                   "unregister_class(...): can't unregister %s because %s.%s pointer property is "
                   "using this",
                   RNA_struct_identifier(srna),
                   RNA_struct_identifier(srna_iter),
                   prop_identifier);
      return NULL;
    }
  }

  /* Get the context, so register callback can do necessary refreshes. */
  C = BPY_context_get();

  /* Call unregister. */
  unreg(CTX_data_main(C), srna); /* Calls bpy_class_free, this decref's py_class. */

  PyDict_DelItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna);
  if (PyErr_Occurred()) {
    PyErr_Clear();  // return NULL;
  }

  Py_RETURN_NONE;
}

/**
 * Extend RNA types with C/API methods, properties.
 */
void pyrna_struct_type_extend_capi(struct StructRNA *srna,
                                   struct PyMethodDef *method,
                                   struct PyGetSetDef *getset)
{
  /* See 'add_methods' in Python's 'typeobject.c'. */
  PyTypeObject *type = (PyTypeObject *)pyrna_srna_Subtype(srna);
  PyObject *dict = type->tp_dict;
  if (method != NULL) {
    for (; method->ml_name != NULL; method++) {
      PyObject *py_method;

      if (method->ml_flags & METH_CLASS) {
        PyObject *cfunc = PyCFunction_New(method, (PyObject *)type);
        py_method = PyClassMethod_New(cfunc);
        Py_DECREF(cfunc);
      }
      else if (method->ml_flags & METH_STATIC) {
        py_method = PyCFunction_New(method, NULL);
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

  if (getset != NULL) {
    for (; getset->name != NULL; getset++) {
      PyObject *descr = PyDescr_NewGetSet(type, getset);
      /* Ensure we're not overwriting anything that already exists. */
      BLI_assert(PyDict_GetItem(dict, PyDescr_NAME(descr)) == NULL);
      PyDict_SetItem(dict, PyDescr_NAME(descr), descr);
      Py_DECREF(descr);
    }
  }
  Py_DECREF(type);
}

/* Access to 'owner_id' internal global. */

static PyObject *pyrna_bl_owner_id_get(PyObject *UNUSED(self))
{
  const char *name = RNA_struct_state_owner_get();
  if (name) {
    return PyUnicode_FromString(name);
  }
  Py_RETURN_NONE;
}

static PyObject *pyrna_bl_owner_id_set(PyObject *UNUSED(self), PyObject *value)
{
  const char *name;
  if (value == Py_None) {
    name = NULL;
  }
  else if (PyUnicode_Check(value)) {
    name = PyUnicode_AsUTF8(value);
  }
  else {
    PyErr_Format(PyExc_ValueError,
                 "owner_set(...): "
                 "expected None or a string, not '%.200s'",
                 Py_TYPE(value)->tp_name);
    return NULL;
  }
  RNA_struct_state_owner_set(name);
  Py_RETURN_NONE;
}

PyMethodDef meth_bpy_owner_id_get = {
    "_bl_owner_id_get",
    (PyCFunction)pyrna_bl_owner_id_get,
    METH_NOARGS,
    NULL,
};
PyMethodDef meth_bpy_owner_id_set = {
    "_bl_owner_id_set",
    (PyCFunction)pyrna_bl_owner_id_set,
    METH_O,
    NULL,
};
