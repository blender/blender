/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 *
 * This file defines custom-data types which can't be accessed as primitive
 * Python types such as #MDeformVert. It also exposed UV map data in a way
 * compatible with the (deprecated) #MLoopUV type.
 * MLoopUV used to be a struct containing both the UV information and various
 * selection flags. This has since been split up into a float2 attribute
 * and three boolean attributes for the selection/pin states.
 * For backwards compatibility, the original #MLoopUV is emulated in the
 * python API. This comes at a performance penalty however, and the plan is
 * to provide direct access to the boolean layers for faster access. Eventually
 * (probably in 4.0) #BPy_BMLoopUV should be removed on the Python side as well.
 */

#include <Python.h>

#include "../mathutils/mathutils.hh"

#include "DNA_meshdata_types.h"

#include "BKE_customdata.hh"

#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_deform.hh"

#include "bmesh.hh"
#include "bmesh_py_types_meshdata.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_utildefines.hh"

/* Mesh Loop UV
 * ************ */

#define BPy_BMLoopUV_Check(v) (Py_TYPE(v) == &BPy_BMLoopUV_Type)

struct BPy_BMLoopUV {
  PyObject_VAR_HEAD
  float *uv;
  /**
   * Pin may be null, signifying the layer doesn't exist.
   *
   * Currently its always created on a #BMesh because adding UV layers to an existing #BMesh is
   * slow and invalidates existing Python objects having pointers into the original data-blocks
   * (since adding a layer re-generates all blocks).
   * But eventually the plan is to lazily allocate the boolean layers "on demand".
   * Therefore the code handles cases where the pin layer doesn't exist.
   */
  bool *pin;
  BMLoop *loop;
};

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloopuv_uv_doc,
    "Loops UV (as a 2D Vector).\n"
    "\n"
    ":type: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmloopuv_uv_get(BPy_BMLoopUV *self, void * /*closure*/)
{
  return Vector_CreatePyObject_wrap(self->uv, 2, nullptr);
}

static int bpy_bmloopuv_uv_set(BPy_BMLoopUV *self, PyObject *value, void * /*closure*/)
{
  float tvec[2];
  if (mathutils_array_parse(tvec, 2, 2, value, "BMLoopUV.uv") != -1) {
    copy_v2_v2(self->uv, tvec);
    return 0;
  }

  return -1;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloopuv_pin_uv_doc,
    "UV pin state.\n"
    "\n"
    ":type: bool\n");

static PyObject *bpy_bmloopuv_pin_uv_get(BPy_BMLoopUV *self, void * /*closure*/)
{
  /* A non existing pin layer means nothing is currently pinned */
  return self->pin ? PyBool_FromLong(*self->pin) : nullptr;
}

static int bpy_bmloopuv_pin_uv_set(BPy_BMLoopUV *self, PyObject *value, void * /*closure*/)
{
  /* TODO: if we add lazy allocation of the associated uv map bool layers to BMesh we need
   * to add a pin layer and update self->pin in the case of self->pin being nullptr.
   * This isn't easy to do currently as adding CustomData layers to a BMesh invalidates
   * existing python objects. So for now lazy allocation isn't done and self->pin should
   * never be nullptr. */
  BLI_assert(self->pin);
  if (self->pin) {
    *self->pin = PyC_Long_AsBool(value);
  }
  else {
    PyErr_SetString(PyExc_RuntimeError,
                    "active uv layer has no associated pin layer. This is a bug!");
    return -1;
  }
  return 0;
}

static PyGetSetDef bpy_bmloopuv_getseters[] = {
    /* attributes match rna_def_mloopuv. */
    {"uv", (getter)bpy_bmloopuv_uv_get, (setter)bpy_bmloopuv_uv_set, bpy_bmloopuv_uv_doc, nullptr},
    {"pin_uv",
     (getter)bpy_bmloopuv_pin_uv_get,
     (setter)bpy_bmloopuv_pin_uv_set,
     bpy_bmloopuv_pin_uv_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

PyTypeObject BPy_BMLoopUV_Type; /* bm.loops.layers.uv.active */

static void bm_init_types_bmloopuv()
{
  BPy_BMLoopUV_Type.tp_basicsize = sizeof(BPy_BMLoopUV);

  BPy_BMLoopUV_Type.tp_name = "BMLoopUV";

  BPy_BMLoopUV_Type.tp_doc = nullptr; /* todo */

  BPy_BMLoopUV_Type.tp_getset = bpy_bmloopuv_getseters;

  BPy_BMLoopUV_Type.tp_flags = Py_TPFLAGS_DEFAULT;

  PyType_Ready(&BPy_BMLoopUV_Type);
}

int BPy_BMLoopUV_AssignPyObject(BMesh *bm, BMLoop *loop, PyObject *value)
{
  if (UNLIKELY(!BPy_BMLoopUV_Check(value))) {
    PyErr_Format(PyExc_TypeError, "expected BMLoopUV, not a %.200s", Py_TYPE(value)->tp_name);
    return -1;
  }

  BPy_BMLoopUV *src = (BPy_BMLoopUV *)value;
  const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);

  float *luv = BM_ELEM_CD_GET_FLOAT_P(loop, offsets.uv);
  copy_v2_v2(luv, src->uv);
  if (src->pin) {
    BM_ELEM_CD_SET_BOOL(loop, offsets.pin, *src->pin);
  }
  return 0;
}

PyObject *BPy_BMLoopUV_CreatePyObject(BMesh *bm, BMLoop *loop, int layer)
{
  BPy_BMLoopUV *self = PyObject_New(BPy_BMLoopUV, &BPy_BMLoopUV_Type);

  const BMUVOffsets offsets = BM_uv_map_offsets_from_layer(bm, layer);

  self->uv = BM_ELEM_CD_GET_FLOAT_P(loop, offsets.uv);
  self->pin = offsets.pin >= 0 ? BM_ELEM_CD_GET_BOOL_P(loop, offsets.pin) : nullptr;

  return (PyObject *)self;
}

/* --- End Mesh Loop UV --- */

/* Mesh Vert Skin
 * ************ */

#define BPy_BMVertSkin_Check(v) (Py_TYPE(v) == &BPy_BMVertSkin_Type)

struct BPy_BMVertSkin {
  PyObject_VAR_HEAD
  MVertSkin *data;
};

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvertskin_radius_doc,
    "Vert skin radii (as a 2D Vector).\n"
    "\n"
    ":type: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmvertskin_radius_get(BPy_BMVertSkin *self, void * /*closure*/)
{
  return Vector_CreatePyObject_wrap(self->data->radius, 2, nullptr);
}

static int bpy_bmvertskin_radius_set(BPy_BMVertSkin *self, PyObject *value, void * /*closure*/)
{
  float tvec[2];
  if (mathutils_array_parse(tvec, 2, 2, value, "BMVertSkin.radius") != -1) {
    copy_v2_v2(self->data->radius, tvec);
    return 0;
  }

  return -1;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvertskin_flag__use_root_doc,
    "Use as root vertex. Setting this flag does not clear other roots in the same mesh island.\n"
    "\n"
    ":type: bool\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvertskin_flag__use_loose_doc,
    "Use loose vertex.\n"
    "\n"
    ":type: bool\n");

static PyObject *bpy_bmvertskin_flag_get(BPy_BMVertSkin *self, void *flag_p)
{
  const int flag = POINTER_AS_INT(flag_p);
  return PyBool_FromLong(self->data->flag & flag);
}

static int bpy_bmvertskin_flag_set(BPy_BMVertSkin *self, PyObject *value, void *flag_p)
{
  const int flag = POINTER_AS_INT(flag_p);

  switch (PyC_Long_AsBool(value)) {
    case true:
      self->data->flag |= flag;
      return 0;
    case false:
      self->data->flag &= ~flag;
      return 0;
    default:
      /* error is set */
      return -1;
  }
}

static PyGetSetDef bpy_bmvertskin_getseters[] = {
    /* attributes match rna_mesh_gen. */
    {"radius",
     (getter)bpy_bmvertskin_radius_get,
     (setter)bpy_bmvertskin_radius_set,
     bpy_bmvertskin_radius_doc,
     nullptr},
    {"use_root",
     (getter)bpy_bmvertskin_flag_get,
     (setter)bpy_bmvertskin_flag_set,
     bpy_bmvertskin_flag__use_root_doc,
     (void *)MVERT_SKIN_ROOT},
    {"use_loose",
     (getter)bpy_bmvertskin_flag_get,
     (setter)bpy_bmvertskin_flag_set,
     bpy_bmvertskin_flag__use_loose_doc,
     (void *)MVERT_SKIN_LOOSE},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyTypeObject BPy_BMVertSkin_Type; /* bm.loops.layers.skin.active */

static void bm_init_types_bmvertskin()
{
  BPy_BMVertSkin_Type.tp_basicsize = sizeof(BPy_BMVertSkin);

  BPy_BMVertSkin_Type.tp_name = "BMVertSkin";

  BPy_BMVertSkin_Type.tp_doc = nullptr; /* todo */

  BPy_BMVertSkin_Type.tp_getset = bpy_bmvertskin_getseters;

  BPy_BMVertSkin_Type.tp_flags = Py_TPFLAGS_DEFAULT;

  PyType_Ready(&BPy_BMVertSkin_Type);
}

int BPy_BMVertSkin_AssignPyObject(MVertSkin *mvertskin, PyObject *value)
{
  if (UNLIKELY(!BPy_BMVertSkin_Check(value))) {
    PyErr_Format(PyExc_TypeError, "expected BMVertSkin, not a %.200s", Py_TYPE(value)->tp_name);
    return -1;
  }

  *(mvertskin) = *(((BPy_BMVertSkin *)value)->data);
  return 0;
}

PyObject *BPy_BMVertSkin_CreatePyObject(MVertSkin *mvertskin)
{
  BPy_BMVertSkin *self = PyObject_New(BPy_BMVertSkin, &BPy_BMVertSkin_Type);
  self->data = mvertskin;
  return (PyObject *)self;
}

/* --- End Mesh Vert Skin --- */

/* Mesh Loop Color
 * *************** */

/* This simply provides a color wrapper for
 * color which uses mathutils callbacks for mathutils.Color
 */

#define MLOOPCOL_FROM_CAPSULE(color_capsule) \
  ((MLoopCol *)PyCapsule_GetPointer(color_capsule, nullptr))

static void mloopcol_to_float(const MLoopCol *mloopcol, float r_col[4])
{
  rgba_uchar_to_float(r_col, (const uchar *)&mloopcol->r);
}

static void mloopcol_from_float(MLoopCol *mloopcol, const float col[4])
{
  rgba_float_to_uchar((uchar *)&mloopcol->r, col);
}

static uchar mathutils_bmloopcol_cb_index = -1;

static int mathutils_bmloopcol_check(BaseMathObject * /*bmo*/)
{
  /* always ok */
  return 0;
}

static int mathutils_bmloopcol_get(BaseMathObject *bmo, int /*subtype*/)
{
  MLoopCol *mloopcol = MLOOPCOL_FROM_CAPSULE(bmo->cb_user);
  mloopcol_to_float(mloopcol, bmo->data);
  return 0;
}

static int mathutils_bmloopcol_set(BaseMathObject *bmo, int /*subtype*/)
{
  MLoopCol *mloopcol = MLOOPCOL_FROM_CAPSULE(bmo->cb_user);
  mloopcol_from_float(mloopcol, bmo->data);
  return 0;
}

static int mathutils_bmloopcol_get_index(BaseMathObject *bmo, int subtype, int /*index*/)
{
  /* Lazy, avoid repeating the case statement. */
  if (mathutils_bmloopcol_get(bmo, subtype) == -1) {
    return -1;
  }
  return 0;
}

static int mathutils_bmloopcol_set_index(BaseMathObject *bmo, int subtype, int index)
{
  const float f = bmo->data[index];

  /* Lazy, avoid repeating the case statement. */
  if (mathutils_bmloopcol_get(bmo, subtype) == -1) {
    return -1;
  }

  bmo->data[index] = f;
  return mathutils_bmloopcol_set(bmo, subtype);
}

static Mathutils_Callback mathutils_bmloopcol_cb = {
    mathutils_bmloopcol_check,
    mathutils_bmloopcol_get,
    mathutils_bmloopcol_set,
    mathutils_bmloopcol_get_index,
    mathutils_bmloopcol_set_index,
};

static void bm_init_types_bmloopcol()
{
  /* pass */
  mathutils_bmloopcol_cb_index = Mathutils_RegisterCallback(&mathutils_bmloopcol_cb);
}

int BPy_BMLoopColor_AssignPyObject(MLoopCol *mloopcol, PyObject *value)
{
  float tvec[4];
  if (mathutils_array_parse(tvec, 4, 4, value, "BMLoopCol") != -1) {
    mloopcol_from_float(mloopcol, tvec);
    return 0;
  }

  return -1;
}

PyObject *BPy_BMLoopColor_CreatePyObject(MLoopCol *mloopcol)
{
  PyObject *color_capsule;
  color_capsule = PyCapsule_New(mloopcol, nullptr, nullptr);
  return Vector_CreatePyObject_cb(color_capsule, 4, mathutils_bmloopcol_cb_index, 0);
}

#undef MLOOPCOL_FROM_CAPSULE

/* --- End Mesh Loop Color --- */

/* Mesh Deform Vert
 * **************** */

/**
 * This is python type wraps a deform vert as a python dictionary,
 * hiding the #MDeformWeight on access, since the mapping is very close, eg:
 *
 * \code{.c}
 * weight = BKE_defvert_find_weight(dv, group_nr);
 * BKE_defvert_remove_group(dv, dw)
 * \endcode
 *
 * \code{.py}
 * weight = dv[group_nr]
 * del dv[group_nr]
 * \endcode
 *
 * \note There is nothing BMesh specific here,
 * its only that BMesh is the only part of blender that uses a hand written API like this.
 * This type could eventually be used to access lattice weights.
 *
 * \note Many of Blender-API's dictionary-like-wrappers act like ordered dictionaries,
 * This is intentionally _not_ ordered, the weights can be in any order and it won't matter,
 * the order should not be used in the API in any meaningful way (as with a python dict)
 * only expose as mapping, not a sequence.
 */

#define BPy_BMDeformVert_Check(v) (Py_TYPE(v) == &BPy_BMDeformVert_Type)

struct BPy_BMDeformVert {
  PyObject_VAR_HEAD
  MDeformVert *data;
};

/* Mapping Protocols
 * ================= */

static Py_ssize_t bpy_bmdeformvert_len(BPy_BMDeformVert *self)
{
  return self->data->totweight;
}

static PyObject *bpy_bmdeformvert_subscript(BPy_BMDeformVert *self, PyObject *key)
{
  if (PyIndex_Check(key)) {
    int i;
    i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }

    MDeformWeight *dw = BKE_defvert_find_index(self->data, i);

    if (dw == nullptr) {
      PyErr_SetString(PyExc_KeyError,
                      "BMDeformVert[key] = x: "
                      "key not found");
      return nullptr;
    }

    return PyFloat_FromDouble(dw->weight);
  }

  PyErr_Format(
      PyExc_TypeError, "BMDeformVert keys must be integers, not %.200s", Py_TYPE(key)->tp_name);
  return nullptr;
}

static int bpy_bmdeformvert_ass_subscript(BPy_BMDeformVert *self, PyObject *key, PyObject *value)
{
  if (PyIndex_Check(key)) {
    int i;

    i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return -1;
    }

    if (value) {
      /* Handle `dvert[group_index] = 0.5`. */
      if (i < 0) {
        PyErr_SetString(PyExc_KeyError,
                        "BMDeformVert[key] = x: "
                        "weight keys cannot be negative");
        return -1;
      }

      MDeformWeight *dw = BKE_defvert_ensure_index(self->data, i);
      const float f = PyFloat_AsDouble(value);
      if (f == -1 && PyErr_Occurred()) { /* Parsed key not a number. */
        PyErr_SetString(PyExc_TypeError,
                        "BMDeformVert[key] = x: "
                        "assigned value not a number");
        return -1;
      }

      dw->weight = clamp_f(f, 0.0f, 1.0f);
    }
    else {
      /* Handle `del dvert[group_index]`. */
      MDeformWeight *dw = BKE_defvert_find_index(self->data, i);

      if (dw == nullptr) {
        PyErr_SetString(PyExc_KeyError,
                        "del BMDeformVert[key]: "
                        "key not found");
      }
      BKE_defvert_remove_group(self->data, dw);
    }

    return 0;
  }

  PyErr_Format(
      PyExc_TypeError, "BMDeformVert keys must be integers, not %.200s", Py_TYPE(key)->tp_name);
  return -1;
}

static int bpy_bmdeformvert_contains(BPy_BMDeformVert *self, PyObject *value)
{
  const int key = PyLong_AsSsize_t(value);

  if (key == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "BMDeformVert.__contains__: expected an int");
    return -1;
  }

  return (BKE_defvert_find_index(self->data, key) != nullptr) ? 1 : 0;
}

/* only defined for __contains__ */
static PySequenceMethods bpy_bmdeformvert_as_sequence = {
    /*sq_length*/ (lenfunc)bpy_bmdeformvert_len,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /* NOTE: if this is set #PySequence_Check() returns True,
     * but in this case we don't want to be treated as a seq. */
    /*sq_item*/ nullptr,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ (objobjproc)bpy_bmdeformvert_contains,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods bpy_bmdeformvert_as_mapping = {
    /*mp_length*/ (lenfunc)bpy_bmdeformvert_len,
    /*mp_subscript*/ (binaryfunc)bpy_bmdeformvert_subscript,
    /*mp_ass_subscript*/ (objobjargproc)bpy_bmdeformvert_ass_subscript,
};

/* Methods
 * ======= */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmdeformvert_keys_doc,
    ".. method:: keys()\n"
    "\n"
    "   Return the group indices used by this vertex\n"
    "   (matching Python's dict.keys() functionality).\n"
    "\n"
    "   :return: the deform group this vertex uses\n"
    "   :rtype: list[int]\n");
static PyObject *bpy_bmdeformvert_keys(BPy_BMDeformVert *self)
{
  PyObject *ret;
  int i;
  MDeformWeight *dw = self->data->dw;

  ret = PyList_New(self->data->totweight);
  for (i = 0; i < self->data->totweight; i++, dw++) {
    PyList_SET_ITEM(ret, i, PyLong_FromLong(dw->def_nr));
  }

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmdeformvert_values_doc,
    ".. method:: values()\n"
    "\n"
    "   Return the weights of the deform vertex\n"
    "   (matching Python's dict.values() functionality).\n"
    "\n"
    "   :return: The weights that influence this vertex\n"
    "   :rtype: list[float]\n");
static PyObject *bpy_bmdeformvert_values(BPy_BMDeformVert *self)
{
  PyObject *ret;
  int i;
  MDeformWeight *dw = self->data->dw;

  ret = PyList_New(self->data->totweight);
  for (i = 0; i < self->data->totweight; i++, dw++) {
    PyList_SET_ITEM(ret, i, PyFloat_FromDouble(dw->weight));
  }

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmdeformvert_items_doc,
    ".. method:: items()\n"
    "\n"
    "   Return (group, weight) pairs for this vertex\n"
    "   (matching Python's dict.items() functionality).\n"
    "\n"
    "   :return: (key, value) pairs for each deform weight of this vertex.\n"
    "   :rtype: list[tuple[int, float]]\n");
static PyObject *bpy_bmdeformvert_items(BPy_BMDeformVert *self)
{
  PyObject *ret;
  PyObject *item;
  int i;
  MDeformWeight *dw = self->data->dw;

  ret = PyList_New(self->data->totweight);
  for (i = 0; i < self->data->totweight; i++, dw++) {
    item = PyTuple_New(2);
    PyTuple_SET_ITEMS(item, PyLong_FromLong(dw->def_nr), PyFloat_FromDouble(dw->weight));
    PyList_SET_ITEM(ret, i, item);
  }

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmdeformvert_get_doc,
    ".. method:: get(key, default=None)\n"
    "\n"
    "   Returns the deform weight matching the key or default\n"
    "   when not found (matches Python's dictionary function of the same name).\n"
    "\n"
    "   :arg key: The key associated with deform weight.\n"
    "   :type key: int\n"
    "   :arg default: Optional argument for the value to return if\n"
    "      *key* is not found.\n"
    "   :type default: Any\n");
static PyObject *bpy_bmdeformvert_get(BPy_BMDeformVert *self, PyObject *args)
{
  int key;
  PyObject *def = Py_None;

  if (!PyArg_ParseTuple(args, "i|O:get", &key, &def)) {
    return nullptr;
  }

  MDeformWeight *dw = BKE_defvert_find_index(self->data, key);

  if (dw) {
    return PyFloat_FromDouble(dw->weight);
  }

  return Py_NewRef(def);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmdeformvert_clear_doc,
    ".. method:: clear()\n"
    "\n"
    "   Clears all weights.\n");
static PyObject *bpy_bmdeformvert_clear(BPy_BMDeformVert *self)
{
  BKE_defvert_clear(self->data);

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

static PyMethodDef bpy_bmdeformvert_methods[] = {
    {"keys", (PyCFunction)bpy_bmdeformvert_keys, METH_NOARGS, bpy_bmdeformvert_keys_doc},
    {"values", (PyCFunction)bpy_bmdeformvert_values, METH_NOARGS, bpy_bmdeformvert_values_doc},
    {"items", (PyCFunction)bpy_bmdeformvert_items, METH_NOARGS, bpy_bmdeformvert_items_doc},
    {"get", (PyCFunction)bpy_bmdeformvert_get, METH_VARARGS, bpy_bmdeformvert_get_doc},
    /* BMESH_TODO `pop`, `popitem`, `update`. */
    {"clear", (PyCFunction)bpy_bmdeformvert_clear, METH_NOARGS, bpy_bmdeformvert_clear_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyTypeObject BPy_BMDeformVert_Type; /* bm.loops.layers.uv.active */

static void bm_init_types_bmdvert()
{
  BPy_BMDeformVert_Type.tp_basicsize = sizeof(BPy_BMDeformVert);

  BPy_BMDeformVert_Type.tp_name = "BMDeformVert";

  BPy_BMDeformVert_Type.tp_doc = nullptr; /* todo */

  BPy_BMDeformVert_Type.tp_as_sequence = &bpy_bmdeformvert_as_sequence;
  BPy_BMDeformVert_Type.tp_as_mapping = &bpy_bmdeformvert_as_mapping;

  BPy_BMDeformVert_Type.tp_methods = bpy_bmdeformvert_methods;

  BPy_BMDeformVert_Type.tp_flags = Py_TPFLAGS_DEFAULT;

  PyType_Ready(&BPy_BMDeformVert_Type);
}

int BPy_BMDeformVert_AssignPyObject(MDeformVert *dvert, PyObject *value)
{
  if (UNLIKELY(!BPy_BMDeformVert_Check(value))) {
    PyErr_Format(PyExc_TypeError, "expected BMDeformVert, not a %.200s", Py_TYPE(value)->tp_name);
    return -1;
  }

  MDeformVert *dvert_src = ((BPy_BMDeformVert *)value)->data;
  if (LIKELY(dvert != dvert_src)) {
    BKE_defvert_copy(dvert, dvert_src);
  }
  return 0;
}

PyObject *BPy_BMDeformVert_CreatePyObject(MDeformVert *dvert)
{
  BPy_BMDeformVert *self = PyObject_New(BPy_BMDeformVert, &BPy_BMDeformVert_Type);
  self->data = dvert;
  return (PyObject *)self;
}

/* --- End Mesh Deform Vert --- */

void BPy_BM_init_types_meshdata()
{
  bm_init_types_bmloopuv();
  bm_init_types_bmloopcol();
  bm_init_types_bmdvert();
  bm_init_types_bmvertskin();
}
