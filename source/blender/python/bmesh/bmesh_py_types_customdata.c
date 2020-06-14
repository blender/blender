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
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup pybmesh
 *
 * This file defines the types for 'BMesh.verts/edges/faces/loops.layers'
 * customdata layer access.
 */

#include <Python.h>

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "bmesh.h"

#include "bmesh_py_types.h"
#include "bmesh_py_types_customdata.h"
#include "bmesh_py_types_meshdata.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"
#include "../mathutils/mathutils.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

static CustomData *bpy_bm_customdata_get(BMesh *bm, char htype)
{
  switch (htype) {
    case BM_VERT:
      return &bm->vdata;
    case BM_EDGE:
      return &bm->edata;
    case BM_FACE:
      return &bm->pdata;
    case BM_LOOP:
      return &bm->ldata;
  }

  BLI_assert(0);
  return NULL;
}

static CustomDataLayer *bpy_bmlayeritem_get(BPy_BMLayerItem *self)
{
  CustomData *data = bpy_bm_customdata_get(self->bm, self->htype);
  const int index_absolute = CustomData_get_layer_index_n(data, self->type, self->index);
  if (index_absolute != -1) {
    return &data->layers[index_absolute];
  }
  else {
    PyErr_SetString(PyExc_RuntimeError, "layer has become invalid");
    return NULL;
  }
}

/* py-type definitions
 * ******************* */

/* getseters
 * ========= */

/* used for many different types  */

PyDoc_STRVAR(bpy_bmlayeraccess_collection__float_doc,
             "Generic float custom-data layer.\n\ntype: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__int_doc,
             "Generic int custom-data layer.\n\ntype: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__string_doc,
             "Generic string custom-data layer (exposed as bytes, 255 max length).\n\ntype: "
             ":class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__deform_doc,
             "Vertex deform weight :class:`BMDeformVert` (TODO).\n\ntype: "
             ":class:`BMLayerCollection`"  // TYPE DOESN'T EXIST YET
);
PyDoc_STRVAR(
    bpy_bmlayeraccess_collection__shape_doc,
    "Vertex shapekey absolute location (as a 3D Vector).\n\n:type: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__bevel_weight_doc,
             "Bevel weight float in [0 - 1].\n\n:type: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__crease_doc,
             "Edge crease for subdivision surface - float in [0 - 1].\n\n:type: :class:`BMLayerCollection`");
PyDoc_STRVAR(
    bpy_bmlayeraccess_collection__uv_doc,
    "Accessor for :class:`BMLoopUV` UV (as a 2D Vector).\n\ntype: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__color_doc,
             "Accessor for vertex color layer.\n\ntype: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__skin_doc,
             "Accessor for skin layer.\n\ntype: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__paint_mask_doc,
             "Accessor for paint mask layer.\n\ntype: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__face_map_doc,
             "FaceMap custom-data layer.\n\ntype: :class:`BMLayerCollection`");
#ifdef WITH_FREESTYLE
PyDoc_STRVAR(bpy_bmlayeraccess_collection__freestyle_edge_doc,
             "Accessor for Freestyle edge layer.\n\ntype: :class:`BMLayerCollection`");
PyDoc_STRVAR(bpy_bmlayeraccess_collection__freestyle_face_doc,
             "Accessor for Freestyle face layer.\n\ntype: :class:`BMLayerCollection`");
#endif

static PyObject *bpy_bmlayeraccess_collection_get(BPy_BMLayerAccess *self, void *flag)
{
  const int type = (int)POINTER_AS_INT(flag);

  BPY_BM_CHECK_OBJ(self);

  return BPy_BMLayerCollection_CreatePyObject(self->bm, self->htype, type);
}

PyDoc_STRVAR(bpy_bmlayercollection_active_doc,
             "The active layer of this type (read-only).\n\n:type: :class:`BMLayerItem`");
static PyObject *bpy_bmlayercollection_active_get(BPy_BMLayerItem *self, void *UNUSED(flag))
{
  CustomData *data;
  int index;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_active_layer(data, self->type); /* type relative */

  if (index != -1) {
    return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
  }
  else {
    Py_RETURN_NONE;
  }
}

PyDoc_STRVAR(
    bpy_bmlayercollection_is_singleton_doc,
    "True if there can exists only one layer of this type (read-only).\n\n:type: boolean");
static PyObject *bpy_bmlayercollection_is_singleton_get(BPy_BMLayerItem *self, void *UNUSED(flag))
{
  BPY_BM_CHECK_OBJ(self);

  return PyBool_FromLong(CustomData_layertype_is_singleton(self->type));
}

PyDoc_STRVAR(bpy_bmlayercollection_name_doc,
             "The layers unique name (read-only).\n\n:type: string");
static PyObject *bpy_bmlayeritem_name_get(BPy_BMLayerItem *self, void *UNUSED(flag))
{
  CustomDataLayer *layer;

  BPY_BM_CHECK_OBJ(self);

  layer = bpy_bmlayeritem_get(self);
  if (layer) {
    return PyUnicode_FromString(layer->name);
  }
  else {
    return NULL;
  }
}

static PyGetSetDef bpy_bmlayeraccess_vert_getseters[] = {
    {"deform",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__deform_doc,
     (void *)CD_MDEFORMVERT},

    {"float",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__float_doc,
     (void *)CD_PROP_FLOAT},
    {"int",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__int_doc,
     (void *)CD_PROP_INT32},
    {"string",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__string_doc,
     (void *)CD_PROP_STRING},

    {"shape",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__shape_doc,
     (void *)CD_SHAPEKEY},
    {"bevel_weight",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__bevel_weight_doc,
     (void *)CD_BWEIGHT},
    {"skin",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__skin_doc,
     (void *)CD_MVERT_SKIN},
    {"paint_mask",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__paint_mask_doc,
     (void *)CD_PAINT_MASK},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeraccess_edge_getseters[] = {
    {"float",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__float_doc,
     (void *)CD_PROP_FLOAT},
    {"int",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__int_doc,
     (void *)CD_PROP_INT32},
    {"string",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__string_doc,
     (void *)CD_PROP_STRING},

    {"bevel_weight",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__bevel_weight_doc,
     (void *)CD_BWEIGHT},
    {"crease",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__crease_doc,
     (void *)CD_CREASE},
#ifdef WITH_FREESTYLE
    {"freestyle",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__freestyle_edge_doc,
     (void *)CD_FREESTYLE_EDGE},
#endif

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeraccess_face_getseters[] = {
    {"float",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__float_doc,
     (void *)CD_PROP_FLOAT},
    {"int",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__int_doc,
     (void *)CD_PROP_INT32},
    {"string",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__string_doc,
     (void *)CD_PROP_STRING},
    {"face_map",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__face_map_doc,
     (void *)CD_FACEMAP},

#ifdef WITH_FREESTYLE
    {"freestyle",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__freestyle_face_doc,
     (void *)CD_FREESTYLE_FACE},
#endif

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeraccess_loop_getseters[] = {
    {"float",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__float_doc,
     (void *)CD_PROP_FLOAT},
    {"int",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__int_doc,
     (void *)CD_PROP_INT32},
    {"string",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__string_doc,
     (void *)CD_PROP_STRING},

    {"uv",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__uv_doc,
     (void *)CD_MLOOPUV},
    {"color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter)NULL,
     bpy_bmlayeraccess_collection__color_doc,
     (void *)CD_MLOOPCOL},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmlayercollection_getseters[] = {
    /* BMESH_TODO, make writeable */
    {"active",
     (getter)bpy_bmlayercollection_active_get,
     (setter)NULL,
     bpy_bmlayercollection_active_doc,
     NULL},
    {"is_singleton",
     (getter)bpy_bmlayercollection_is_singleton_get,
     (setter)NULL,
     bpy_bmlayercollection_is_singleton_doc,
     NULL},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeritem_getseters[] = {
    /* BMESH_TODO, make writeable */
    {"name", (getter)bpy_bmlayeritem_name_get, (setter)NULL, bpy_bmlayercollection_name_doc, NULL},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/* Methods
 * ======= */

/* BMLayerCollection
 * ----------------- */

PyDoc_STRVAR(bpy_bmlayeritem_copy_from_doc,
             ".. method:: copy_from(other)\n"
             "\n"
             "   Return a copy of the layer\n"
             "\n"
             "   :arg other: Another layer to copy from.\n"
             "   :arg other: :class:`BMLayerItem`\n");
static PyObject *bpy_bmlayeritem_copy_from(BPy_BMLayerItem *self, BPy_BMLayerItem *value)
{
  CustomData *data;

  if (!BPy_BMLayerItem_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "layer.copy_from(x): expected BMLayerItem, not '%.200s'",
                 Py_TYPE(value)->tp_name);
    return NULL;
  }

  BPY_BM_CHECK_OBJ(self);
  BPY_BM_CHECK_SOURCE_OBJ(self->bm, "layer.copy_from()", value);

  if ((self->htype != value->htype) || (self->type != value->type)) {
    PyErr_SetString(PyExc_ValueError, "layer.copy_from(other): layer type mismatch");
  }

  else if (self->index == value->index) {
    Py_RETURN_NONE;
  }

  data = bpy_bm_customdata_get(self->bm, self->htype);

  if ((bpy_bmlayeritem_get(self) == NULL) || (bpy_bmlayeritem_get(value) == NULL)) {
    return NULL;
  }

  BM_data_layer_copy(self->bm, data, self->type, value->index, self->index);

  Py_RETURN_NONE;
}

/* similar to new(), but no name arg. */
PyDoc_STRVAR(bpy_bmlayercollection_verify_doc,
             ".. method:: verify()\n"
             "\n"
             "   Create a new layer or return an existing active layer\n"
             "\n"
             "   :return: The newly verified layer.\n"
             "   :rtype: :class:`BMLayerItem`\n");
static PyObject *bpy_bmlayercollection_verify(BPy_BMLayerCollection *self)
{
  int index;
  CustomData *data;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);

  index = CustomData_get_active_layer(data, self->type); /* type relative */

  if (index == -1) {
    BM_data_layer_add(self->bm, data, self->type);
    index = 0;
  }

  BLI_assert(index >= 0);

  return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
}

PyDoc_STRVAR(bpy_bmlayercollection_new_doc,
             ".. method:: new(name)\n"
             "\n"
             "   Create a new layer\n"
             "\n"
             "   :arg name: Optional name argument (will be made unique).\n"
             "   :type name: string\n"
             "   :return: The newly created layer.\n"
             "   :rtype: :class:`BMLayerItem`\n");
static PyObject *bpy_bmlayercollection_new(BPy_BMLayerCollection *self, PyObject *args)
{
  const char *name = NULL;
  int index;
  CustomData *data;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|s:new", &name)) {
    return NULL;
  }

  data = bpy_bm_customdata_get(self->bm, self->htype);

  if (CustomData_layertype_is_singleton(self->type) && CustomData_has_layer(data, self->type)) {
    PyErr_SetString(PyExc_ValueError, "layers.new(): is a singleton, use verify() instead");
    return NULL;
  }

  if (name) {
    BM_data_layer_add_named(self->bm, data, self->type, name);
  }
  else {
    BM_data_layer_add(self->bm, data, self->type);
  }

  index = CustomData_number_of_layers(data, self->type) - 1;
  BLI_assert(index >= 0);

  return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
}

PyDoc_STRVAR(bpy_bmlayercollection_remove_doc,
             ".. method:: remove(layer)\n"
             "\n"
             "   Remove a layer\n"
             "\n"
             "   :arg layer: The layer to remove.\n"
             "   :type layer: :class:`BMLayerItem`\n");
static PyObject *bpy_bmlayercollection_remove(BPy_BMLayerCollection *self, BPy_BMLayerItem *value)
{
  CustomData *data;

  BPY_BM_CHECK_OBJ(self);

  if (!BPy_BMLayerItem_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "layers.remove(x): expected BMLayerItem, not '%.200s'",
                 Py_TYPE(value)->tp_name);
    return NULL;
  }

  BPY_BM_CHECK_OBJ(value);

  if ((self->bm != value->bm) || (self->type != value->type) || (self->htype != value->htype)) {
    PyErr_SetString(PyExc_ValueError, "layers.remove(x): x not in layers");
  }

  data = bpy_bm_customdata_get(self->bm, self->htype);
  BM_data_layer_free_n(self->bm, data, self->type, value->index);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bmlayercollection_keys_doc,
             ".. method:: keys()\n"
             "\n"
             "   Return the identifiers of collection members\n"
             "   (matching pythons dict.keys() functionality).\n"
             "\n"
             "   :return: the identifiers for each member of this collection.\n"
             "   :rtype: list of strings\n");
static PyObject *bpy_bmlayercollection_keys(BPy_BMLayerCollection *self)
{
  PyObject *ret;
  PyObject *item;
  int index;
  CustomData *data;
  int tot, i;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);

  /* Absolute, but no need to make relative. */
  index = CustomData_get_layer_index(data, self->type);

  tot = (index != -1) ? CustomData_number_of_layers(data, self->type) : 0;

  ret = PyList_New(tot);

  for (i = 0; tot-- > 0; index++) {
    item = PyUnicode_FromString(data->layers[index].name);
    PyList_SET_ITEM(ret, i++, item);
  }

  return ret;
}

PyDoc_STRVAR(bpy_bmlayercollection_items_doc,
             ".. method:: items()\n"
             "\n"
             "   Return the identifiers of collection members\n"
             "   (matching pythons dict.items() functionality).\n"
             "\n"
             "   :return: (key, value) pairs for each member of this collection.\n"
             "   :rtype: list of tuples\n");
static PyObject *bpy_bmlayercollection_items(BPy_BMLayerCollection *self)
{
  PyObject *ret;
  PyObject *item;
  int index;
  CustomData *data;
  int tot, i;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_layer_index(data, self->type);
  tot = (index != -1) ? CustomData_number_of_layers(data, self->type) : 0;

  ret = PyList_New(tot);

  for (i = 0; tot-- > 0; index++) {
    item = PyTuple_New(2);
    PyTuple_SET_ITEMS(item,
                      PyUnicode_FromString(data->layers[index].name),
                      BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, i));
    PyList_SET_ITEM(ret, i++, item);
  }

  return ret;
}

PyDoc_STRVAR(bpy_bmlayercollection_values_doc,
             ".. method:: values()\n"
             "\n"
             "   Return the values of collection\n"
             "   (matching pythons dict.values() functionality).\n"
             "\n"
             "   :return: the members of this collection.\n"
             "   :rtype: list\n");
static PyObject *bpy_bmlayercollection_values(BPy_BMLayerCollection *self)
{
  PyObject *ret;
  PyObject *item;
  int index;
  CustomData *data;
  int tot, i;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_layer_index(data, self->type);
  tot = (index != -1) ? CustomData_number_of_layers(data, self->type) : 0;

  ret = PyList_New(tot);

  for (i = 0; tot-- > 0; index++) {
    item = BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, i);
    PyList_SET_ITEM(ret, i++, item);
  }

  return ret;
}

PyDoc_STRVAR(bpy_bmlayercollection_get_doc,
             ".. method:: get(key, default=None)\n"
             "\n"
             "   Returns the value of the layer matching the key or default\n"
             "   when not found (matches pythons dictionary function of the same name).\n"
             "\n"
             "   :arg key: The key associated with the layer.\n"
             "   :type key: string\n"
             "   :arg default: Optional argument for the value to return if\n"
             "      *key* is not found.\n"
             "   :type default: Undefined\n");
static PyObject *bpy_bmlayercollection_get(BPy_BMLayerCollection *self, PyObject *args)
{
  const char *key;
  PyObject *def = Py_None;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
    return NULL;
  }
  else {
    CustomData *data;
    int index;

    data = bpy_bm_customdata_get(self->bm, self->htype);
    index = CustomData_get_named_layer(data, self->type, key); /* type relative */

    if (index != -1) {
      return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
    }
  }

  return Py_INCREF_RET(def);
}

static struct PyMethodDef bpy_bmlayeritem_methods[] = {
    {"copy_from", (PyCFunction)bpy_bmlayeritem_copy_from, METH_O, bpy_bmlayeritem_copy_from_doc},
    {NULL, NULL, 0, NULL},
};

static struct PyMethodDef bpy_bmelemseq_methods[] = {
    {"verify",
     (PyCFunction)bpy_bmlayercollection_verify,
     METH_NOARGS,
     bpy_bmlayercollection_verify_doc},
    {"new", (PyCFunction)bpy_bmlayercollection_new, METH_VARARGS, bpy_bmlayercollection_new_doc},
    {"remove",
     (PyCFunction)bpy_bmlayercollection_remove,
     METH_O,
     bpy_bmlayercollection_remove_doc},

    {"keys", (PyCFunction)bpy_bmlayercollection_keys, METH_NOARGS, bpy_bmlayercollection_keys_doc},
    {"values",
     (PyCFunction)bpy_bmlayercollection_values,
     METH_NOARGS,
     bpy_bmlayercollection_values_doc},
    {"items",
     (PyCFunction)bpy_bmlayercollection_items,
     METH_NOARGS,
     bpy_bmlayercollection_items_doc},
    {"get", (PyCFunction)bpy_bmlayercollection_get, METH_VARARGS, bpy_bmlayercollection_get_doc},
    {NULL, NULL, 0, NULL},
};

/* Sequences
 * ========= */

static Py_ssize_t bpy_bmlayercollection_length(BPy_BMLayerCollection *self)
{
  CustomData *data;

  BPY_BM_CHECK_INT(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);

  return CustomData_number_of_layers(data, self->type);
}

static PyObject *bpy_bmlayercollection_subscript_str(BPy_BMLayerCollection *self,
                                                     const char *keyname)
{
  CustomData *data;
  int index;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_named_layer(data, self->type, keyname); /* type relative */

  if (index != -1) {
    return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
  }
  else {
    PyErr_Format(PyExc_KeyError, "BMLayerCollection[key]: key \"%.200s\" not found", keyname);
    return NULL;
  }
}

static PyObject *bpy_bmlayercollection_subscript_int(BPy_BMLayerCollection *self, int keynum)
{
  Py_ssize_t len;
  BPY_BM_CHECK_OBJ(self);

  len = bpy_bmlayercollection_length(self);

  if (keynum < 0) {
    keynum += len;
  }
  if (keynum >= 0) {
    if (keynum < len) {
      return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, keynum);
    }
  }

  PyErr_Format(PyExc_IndexError, "BMLayerCollection[index]: index %d out of range", keynum);
  return NULL;
}

static PyObject *bpy_bmlayercollection_subscript_slice(BPy_BMLayerCollection *self,
                                                       Py_ssize_t start,
                                                       Py_ssize_t stop)
{
  Py_ssize_t len = bpy_bmlayercollection_length(self);
  int count = 0;

  PyObject *tuple;

  BPY_BM_CHECK_OBJ(self);

  if (start >= len) {
    start = len - 1;
  }
  if (stop >= len) {
    stop = len - 1;
  }

  tuple = PyTuple_New(stop - start);

  for (count = start; count < stop; count++) {
    PyTuple_SET_ITEM(tuple,
                     count - start,
                     BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, count));
  }

  return tuple;
}

static PyObject *bpy_bmlayercollection_subscript(BPy_BMLayerCollection *self, PyObject *key)
{
  /* don't need error check here */
  if (PyUnicode_Check(key)) {
    return bpy_bmlayercollection_subscript_str(self, _PyUnicode_AsString(key));
  }
  else if (PyIndex_Check(key)) {
    Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return NULL;
    }
    return bpy_bmlayercollection_subscript_int(self, i);
  }
  else if (PySlice_Check(key)) {
    PySliceObject *key_slice = (PySliceObject *)key;
    Py_ssize_t step = 1;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return NULL;
    }
    else if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "BMLayerCollection[slice]: slice steps not supported");
      return NULL;
    }
    else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      return bpy_bmlayercollection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
    }
    else {
      Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

      /* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
      if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) {
        return NULL;
      }
      if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop)) {
        return NULL;
      }

      if (start < 0 || stop < 0) {
        /* only get the length for negative values */
        Py_ssize_t len = bpy_bmlayercollection_length(self);
        if (start < 0) {
          start += len;
        }
        if (stop < 0) {
          stop += len;
        }
      }

      if (stop - start <= 0) {
        return PyTuple_New(0);
      }
      else {
        return bpy_bmlayercollection_subscript_slice(self, start, stop);
      }
    }
  }
  else {
    PyErr_SetString(PyExc_AttributeError,
                    "BMLayerCollection[key]: invalid key, key must be an int");
    return NULL;
  }
}

static int bpy_bmlayercollection_contains(BPy_BMLayerCollection *self, PyObject *value)
{
  const char *keyname = _PyUnicode_AsString(value);
  CustomData *data;
  int index;

  BPY_BM_CHECK_INT(self);

  if (keyname == NULL) {
    PyErr_SetString(PyExc_TypeError, "BMLayerCollection.__contains__: expected a string");
    return -1;
  }

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_named_layer_index(data, self->type, keyname);

  return (index != -1) ? 1 : 0;
}

static PySequenceMethods bpy_bmlayercollection_as_sequence = {
    (lenfunc)bpy_bmlayercollection_length, /* sq_length */
    NULL,                                  /* sq_concat */
    NULL,                                  /* sq_repeat */
    (ssizeargfunc)bpy_bmlayercollection_subscript_int,
    /* sq_item */          /* Only set this so PySequence_Check() returns True */
    NULL,                  /* sq_slice */
    (ssizeobjargproc)NULL, /* sq_ass_item */
    NULL,                  /* *was* sq_ass_slice */
    (objobjproc)bpy_bmlayercollection_contains, /* sq_contains */
    (binaryfunc)NULL,                           /* sq_inplace_concat */
    (ssizeargfunc)NULL,                         /* sq_inplace_repeat */
};

static PyMappingMethods bpy_bmlayercollection_as_mapping = {
    (lenfunc)bpy_bmlayercollection_length,       /* mp_length */
    (binaryfunc)bpy_bmlayercollection_subscript, /* mp_subscript */
    (objobjargproc)NULL,                         /* mp_ass_subscript */
};

/* Iterator
 * -------- */

static PyObject *bpy_bmlayercollection_iter(BPy_BMLayerCollection *self)
{
  /* fake it with a list iterator */
  PyObject *ret;
  PyObject *iter = NULL;

  BPY_BM_CHECK_OBJ(self);

  ret = bpy_bmlayercollection_subscript_slice(self, 0, PY_SSIZE_T_MIN);

  if (ret) {
    iter = PyObject_GetIter(ret);
    Py_DECREF(ret);
  }

  return iter;
}

PyDoc_STRVAR(bpy_bmlayeraccess_type_doc, "Exposes custom-data layer attributes.");

PyDoc_STRVAR(bpy_bmlayercollection_type_doc,
             "Gives access to a collection of custom-data layers of the same type and behaves "
             "like python dictionaries, "
             "except for the ability to do list like index access.");

PyDoc_STRVAR(bpy_bmlayeritem_type_doc,
             "Exposes a single custom data layer, "
             "their main purpose is for use as item accessors to custom-data when used with "
             "vert/edge/face/loop data.");

PyTypeObject BPy_BMLayerAccessVert_Type; /* bm.verts.layers */
PyTypeObject BPy_BMLayerAccessEdge_Type; /* bm.edges.layers */
PyTypeObject BPy_BMLayerAccessFace_Type; /* bm.faces.layers */
PyTypeObject BPy_BMLayerAccessLoop_Type; /* bm.loops.layers */
PyTypeObject BPy_BMLayerCollection_Type; /* bm.loops.layers.uv */
PyTypeObject BPy_BMLayerItem_Type;       /* bm.loops.layers.uv["UVMap"] */

PyObject *BPy_BMLayerAccess_CreatePyObject(BMesh *bm, const char htype)
{
  BPy_BMLayerAccess *self;
  PyTypeObject *type;

  switch (htype) {
    case BM_VERT:
      type = &BPy_BMLayerAccessVert_Type;
      break;
    case BM_EDGE:
      type = &BPy_BMLayerAccessEdge_Type;
      break;
    case BM_FACE:
      type = &BPy_BMLayerAccessFace_Type;
      break;
    case BM_LOOP:
      type = &BPy_BMLayerAccessLoop_Type;
      break;
    default: {
      BLI_assert(0);
      type = NULL;
      break;
    }
  }

  self = PyObject_New(BPy_BMLayerAccess, type);
  self->bm = bm;
  self->htype = htype;
  return (PyObject *)self;
}

PyObject *BPy_BMLayerCollection_CreatePyObject(BMesh *bm, const char htype, int type)
{
  BPy_BMLayerCollection *self = PyObject_New(BPy_BMLayerCollection, &BPy_BMLayerCollection_Type);
  self->bm = bm;
  self->htype = htype;
  self->type = type;
  return (PyObject *)self;
}

PyObject *BPy_BMLayerItem_CreatePyObject(BMesh *bm, const char htype, int type, int index)
{
  BPy_BMLayerItem *self = PyObject_New(BPy_BMLayerItem, &BPy_BMLayerItem_Type);
  self->bm = bm;
  self->htype = htype;
  self->type = type;
  self->index = index;
  return (PyObject *)self;
}

void BPy_BM_init_types_customdata(void)
{
  BPy_BMLayerAccessVert_Type.tp_basicsize = sizeof(BPy_BMLayerAccess);
  BPy_BMLayerAccessEdge_Type.tp_basicsize = sizeof(BPy_BMLayerAccess);
  BPy_BMLayerAccessFace_Type.tp_basicsize = sizeof(BPy_BMLayerAccess);
  BPy_BMLayerAccessLoop_Type.tp_basicsize = sizeof(BPy_BMLayerAccess);
  BPy_BMLayerCollection_Type.tp_basicsize = sizeof(BPy_BMLayerCollection);
  BPy_BMLayerItem_Type.tp_basicsize = sizeof(BPy_BMLayerItem);

  BPy_BMLayerAccessVert_Type.tp_name = "BMLayerAccessVert";
  BPy_BMLayerAccessEdge_Type.tp_name = "BMLayerAccessEdge";
  BPy_BMLayerAccessFace_Type.tp_name = "BMLayerAccessFace";
  BPy_BMLayerAccessLoop_Type.tp_name = "BMLayerAccessLoop";
  BPy_BMLayerCollection_Type.tp_name = "BMLayerCollection";
  BPy_BMLayerItem_Type.tp_name = "BMLayerItem";

  /* todo */
  BPy_BMLayerAccessVert_Type.tp_doc = bpy_bmlayeraccess_type_doc;
  BPy_BMLayerAccessEdge_Type.tp_doc = bpy_bmlayeraccess_type_doc;
  BPy_BMLayerAccessFace_Type.tp_doc = bpy_bmlayeraccess_type_doc;
  BPy_BMLayerAccessLoop_Type.tp_doc = bpy_bmlayeraccess_type_doc;
  BPy_BMLayerCollection_Type.tp_doc = bpy_bmlayercollection_type_doc;
  BPy_BMLayerItem_Type.tp_doc = bpy_bmlayeritem_type_doc;

  BPy_BMLayerAccessVert_Type.tp_repr = (reprfunc)NULL;
  BPy_BMLayerAccessEdge_Type.tp_repr = (reprfunc)NULL;
  BPy_BMLayerAccessFace_Type.tp_repr = (reprfunc)NULL;
  BPy_BMLayerAccessLoop_Type.tp_repr = (reprfunc)NULL;
  BPy_BMLayerCollection_Type.tp_repr = (reprfunc)NULL;
  BPy_BMLayerItem_Type.tp_repr = (reprfunc)NULL;

  BPy_BMLayerAccessVert_Type.tp_getset = bpy_bmlayeraccess_vert_getseters;
  BPy_BMLayerAccessEdge_Type.tp_getset = bpy_bmlayeraccess_edge_getseters;
  BPy_BMLayerAccessFace_Type.tp_getset = bpy_bmlayeraccess_face_getseters;
  BPy_BMLayerAccessLoop_Type.tp_getset = bpy_bmlayeraccess_loop_getseters;
  BPy_BMLayerCollection_Type.tp_getset = bpy_bmlayercollection_getseters;
  BPy_BMLayerItem_Type.tp_getset = bpy_bmlayeritem_getseters;

  //  BPy_BMLayerAccess_Type.tp_methods     = bpy_bmeditselseq_methods;
  BPy_BMLayerCollection_Type.tp_methods = bpy_bmelemseq_methods;
  BPy_BMLayerItem_Type.tp_methods = bpy_bmlayeritem_methods;

  BPy_BMLayerCollection_Type.tp_as_sequence = &bpy_bmlayercollection_as_sequence;

  BPy_BMLayerCollection_Type.tp_as_mapping = &bpy_bmlayercollection_as_mapping;

  BPy_BMLayerCollection_Type.tp_iter = (getiterfunc)bpy_bmlayercollection_iter;

  BPy_BMLayerAccessVert_Type.tp_dealloc = NULL;
  BPy_BMLayerAccessEdge_Type.tp_dealloc = NULL;
  BPy_BMLayerAccessFace_Type.tp_dealloc = NULL;
  BPy_BMLayerAccessLoop_Type.tp_dealloc = NULL;
  BPy_BMLayerCollection_Type.tp_dealloc = NULL;
  BPy_BMLayerItem_Type.tp_dealloc = NULL;

  BPy_BMLayerAccessVert_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMLayerAccessEdge_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMLayerAccessFace_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMLayerAccessLoop_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMLayerCollection_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMLayerItem_Type.tp_flags = Py_TPFLAGS_DEFAULT;

  PyType_Ready(&BPy_BMLayerAccessVert_Type);
  PyType_Ready(&BPy_BMLayerAccessEdge_Type);
  PyType_Ready(&BPy_BMLayerAccessFace_Type);
  PyType_Ready(&BPy_BMLayerAccessLoop_Type);
  PyType_Ready(&BPy_BMLayerCollection_Type);
  PyType_Ready(&BPy_BMLayerItem_Type);
}

/* Per Element Get/Set
 * ******************* */

/**
 * helper function for get/set, NULL return means the error is set
 */
static void *bpy_bmlayeritem_ptr_get(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer)
{
  void *value;
  BMElem *ele = py_ele->ele;
  CustomData *data;

  /* error checking */
  if (UNLIKELY(!BPy_BMLayerItem_Check(py_layer))) {
    PyErr_SetString(PyExc_AttributeError, "BMElem[key]: invalid key, must be a BMLayerItem");
    return NULL;
  }
  else if (UNLIKELY(py_ele->bm != py_layer->bm)) {
    PyErr_SetString(PyExc_ValueError, "BMElem[layer]: layer is from another mesh");
    return NULL;
  }
  else if (UNLIKELY(ele->head.htype != py_layer->htype)) {
    char namestr_1[32], namestr_2[32];
    PyErr_Format(PyExc_ValueError,
                 "Layer/Element type mismatch, expected %.200s got layer type %.200s",
                 BPy_BMElem_StringFromHType_ex(ele->head.htype, namestr_1),
                 BPy_BMElem_StringFromHType_ex(py_layer->htype, namestr_2));
    return NULL;
  }

  data = bpy_bm_customdata_get(py_layer->bm, py_layer->htype);

  value = CustomData_bmesh_get_n(data, ele->head.data, py_layer->type, py_layer->index);

  if (UNLIKELY(value == NULL)) {
    /* this should be fairly unlikely but possible if layers move about after we get them */
    PyErr_SetString(PyExc_KeyError, "BMElem[key]: layer not found");
    return NULL;
  }
  else {
    return value;
  }
}

/**
 *\brief BMElem.__getitem__()
 *
 * assume all error checks are done, eg:
 *
 *     uv = vert[uv_layer]
 */
PyObject *BPy_BMLayerItem_GetItem(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer)
{
  void *value = bpy_bmlayeritem_ptr_get(py_ele, py_layer);
  PyObject *ret;

  if (UNLIKELY(value == NULL)) {
    return NULL;
  }

  switch (py_layer->type) {
    case CD_MDEFORMVERT: {
      ret = BPy_BMDeformVert_CreatePyObject(value);
      break;
    }
    case CD_PROP_FLOAT:
    case CD_PAINT_MASK: {
      ret = PyFloat_FromDouble(*(float *)value);
      break;
    }
    case CD_PROP_INT32:
    case CD_FACEMAP: {
      ret = PyLong_FromLong(*(int *)value);
      break;
    }
    case CD_PROP_STRING: {
      MStringProperty *mstring = value;
      ret = PyBytes_FromStringAndSize(mstring->s, mstring->s_len);
      break;
    }
    case CD_MLOOPUV: {
      ret = BPy_BMLoopUV_CreatePyObject(value);
      break;
    }
    case CD_MLOOPCOL: {
      ret = BPy_BMLoopColor_CreatePyObject(value);
      break;
    }
    case CD_SHAPEKEY: {
      ret = Vector_CreatePyObject_wrap((float *)value, 3, NULL);
      break;
    }
    case CD_BWEIGHT: {
      ret = PyFloat_FromDouble(*(float *)value);
      break;
    }
    case CD_CREASE: {
      ret = PyFloat_FromDouble(*(float *)value);
      break;
    }
    case CD_MVERT_SKIN: {
      ret = BPy_BMVertSkin_CreatePyObject(value);
      break;
    }
    default: {
      ret = Py_NotImplemented; /* TODO */
      Py_INCREF(ret);
      break;
    }
  }

  return ret;
}

int BPy_BMLayerItem_SetItem(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer, PyObject *py_value)
{
  int ret = 0;
  void *value = bpy_bmlayeritem_ptr_get(py_ele, py_layer);

  if (UNLIKELY(value == NULL)) {
    return -1;
  }

  switch (py_layer->type) {
    case CD_MDEFORMVERT: {
      ret = BPy_BMDeformVert_AssignPyObject(value, py_value);
      break;
    }
    case CD_PROP_FLOAT:
    case CD_PAINT_MASK: {
      float tmp_val = PyFloat_AsDouble(py_value);
      if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
        PyErr_Format(
            PyExc_TypeError, "expected a float, not a %.200s", Py_TYPE(py_value)->tp_name);
        ret = -1;
      }
      else {
        *(float *)value = tmp_val;
      }
      break;
    }
    case CD_PROP_INT32:
    case CD_FACEMAP: {
      int tmp_val = PyC_Long_AsI32(py_value);
      if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
        /* error is set */
        ret = -1;
      }
      else {
        *(int *)value = tmp_val;
      }
      break;
    }
    case CD_PROP_STRING: {
      MStringProperty *mstring = value;
      char *tmp_val;
      Py_ssize_t tmp_val_len;
      if (UNLIKELY(PyBytes_AsStringAndSize(py_value, &tmp_val, &tmp_val_len) == -1)) {
        PyErr_Format(PyExc_TypeError, "expected bytes, not a %.200s", Py_TYPE(py_value)->tp_name);
        ret = -1;
      }
      else {
        if (tmp_val_len > sizeof(mstring->s)) {
          tmp_val_len = sizeof(mstring->s);
        }
        memcpy(mstring->s, tmp_val, tmp_val_len);
        mstring->s_len = tmp_val_len;
      }
      break;
    }
    case CD_MLOOPUV: {
      ret = BPy_BMLoopUV_AssignPyObject(value, py_value);
      break;
    }
    case CD_MLOOPCOL: {
      ret = BPy_BMLoopColor_AssignPyObject(value, py_value);
      break;
    }
    case CD_SHAPEKEY: {
      float tmp_val[3];
      if (UNLIKELY(mathutils_array_parse(tmp_val, 3, 3, py_value, "BMVert[shape] = value") ==
                   -1)) {
        ret = -1;
      }
      else {
        copy_v3_v3((float *)value, tmp_val);
      }
      break;
    }
    case CD_BWEIGHT: {
      float tmp_val = PyFloat_AsDouble(py_value);
      if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
        PyErr_Format(
            PyExc_TypeError, "expected a float, not a %.200s", Py_TYPE(py_value)->tp_name);
        ret = -1;
      }
      else {
        *(float *)value = clamp_f(tmp_val, 0.0f, 1.0f);
      }
      break;
    }
    case CD_CREASE: {
      float tmp_val = PyFloat_AsDouble(py_value);
      if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
        PyErr_Format(
            PyExc_TypeError, "expected a float, not a %.200s", Py_TYPE(py_value)->tp_name);
        ret = -1;
      }
      else {
        *(float *)value = clamp_f(tmp_val, 0.0f, 1.0f);
      }
      break;
    }
    case CD_MVERT_SKIN: {
      ret = BPy_BMVertSkin_AssignPyObject(value, py_value);
      break;
    }
    default: {
      PyErr_SetString(PyExc_AttributeError, "readonly / unsupported type");
      ret = -1;
      break;
    }
  }

  return ret;
}
