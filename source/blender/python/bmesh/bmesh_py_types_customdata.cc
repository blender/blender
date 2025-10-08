/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 *
 * This file defines the types for 'BMesh.verts/edges/faces/loops.layers'
 * customdata layer access.
 */

#include <Python.h>

#include <algorithm>

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "bmesh.hh"

#include "bmesh_py_types.hh"
#include "bmesh_py_types_customdata.hh"
#include "bmesh_py_types_meshdata.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_utildefines.hh"
#include "../mathutils/mathutils.hh"

#include "BKE_customdata.hh"

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

  BLI_assert_unreachable();
  return nullptr;
}

static CustomDataLayer *bpy_bmlayeritem_get(BPy_BMLayerItem *self)
{
  CustomData *data = bpy_bm_customdata_get(self->bm, self->htype);
  const int index_absolute = CustomData_get_layer_index_n(
      data, eCustomDataType(self->type), self->index);
  if (index_absolute != -1) {
    return &data->layers[index_absolute];
  }

  PyErr_SetString(PyExc_RuntimeError, "layer has become invalid");
  return nullptr;
}

/* py-type definitions
 * ******************* */

/* getseters
 * ========= */

/* used for many different types. */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__float_doc,
    "Generic float custom-data layer.\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerCollection` of float\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__int_doc,
    "Generic int custom-data layer.\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerCollection` of int\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__bool_doc,
    "Generic boolean custom-data layer.\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerCollection` of boolean\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__float_vector_doc,
    "Generic 3D vector with float precision custom-data layer.\n"
    "\n"
    ":type: "
    ":class:`bmesh.types.BMLayerCollection` of :class:`mathutils.Vector`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__float_color_doc,
    "Generic RGBA color with float precision custom-data layer.\n"
    "\n"
    ":type: "
    ":class:`bmesh.types.BMLayerCollection` of :class:`mathutils.Vector`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__color_doc,
    "Generic RGBA color with 8-bit precision custom-data layer.\n"
    "\n"
    ":type: "
    ":class:`bmesh.types.BMLayerCollection` of :class:`mathutils.Vector`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__string_doc,
    "Generic string custom-data layer (exposed as bytes, 255 max length).\n"
    "\n"
    ":type: "
    ":class:`bmesh.types.BMLayerCollection` of bytes\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__deform_doc,
    "Vertex deform weight :class:`bmesh.types.BMDeformVert` (TODO).\n"
    "\n"
    ":type: "
    /* TYPE DOESN'T EXIST YET */
    ":class:`bmesh.types.BMLayerCollection` of :class:`bmesh.types.BMDeformVert`");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__shape_doc,
    "Vertex shape-key absolute location (as a 3D Vector).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerCollection` of :class:`mathutils.Vector`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__uv_doc,
    "Accessor for :class:`bmesh.types.BMLoopUV` UV (as a 2D Vector).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerCollection` of :class:`bmesh.types.BMLoopUV`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_collection__skin_doc,
    "Accessor for skin layer.\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerCollection` of :class:`bmesh.types.BMVertSkin`\n");

static PyObject *bpy_bmlayeraccess_collection_get(BPy_BMLayerAccess *self, void *flag)
{
  const int type = int(POINTER_AS_INT(flag));

  BPY_BM_CHECK_OBJ(self);

  return BPy_BMLayerCollection_CreatePyObject(self->bm, self->htype, type);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_active_doc,
    "The active layer of this type (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerItem`\n");
static PyObject *bpy_bmlayercollection_active_get(BPy_BMLayerItem *self, void * /*flag*/)
{
  CustomData *data;
  int index;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_active_layer(data, eCustomDataType(self->type)); /* type relative */

  if (index != -1) {
    return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_is_singleton_doc,
    "True if there can exists only one layer of this type (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmlayercollection_is_singleton_get(BPy_BMLayerItem *self, void * /*flag*/)
{
  BPY_BM_CHECK_OBJ(self);

  return PyBool_FromLong(CustomData_layertype_is_singleton(eCustomDataType(self->type)));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_name_doc,
    "The layers unique name (read-only).\n"
    "\n"
    ":type: str\n");
static PyObject *bpy_bmlayeritem_name_get(BPy_BMLayerItem *self, void * /*flag*/)
{
  CustomDataLayer *layer;

  BPY_BM_CHECK_OBJ(self);

  layer = bpy_bmlayeritem_get(self);
  if (layer) {
    return PyUnicode_FromString(layer->name);
  }

  return nullptr;
}

static PyGetSetDef bpy_bmlayeraccess_vert_getseters[] = {
    {"deform",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__deform_doc,
     (void *)CD_MDEFORMVERT},

    {"float",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_doc,
     (void *)CD_PROP_FLOAT},
    {"bool",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__bool_doc,
     (void *)CD_PROP_BOOL},
    {"int",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__int_doc,
     (void *)CD_PROP_INT32},
    {"float_vector",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_vector_doc,
     (void *)CD_PROP_FLOAT3},
    {"float_color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_color_doc,
     (void *)CD_PROP_COLOR},
    {"color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__color_doc,
     (void *)CD_PROP_BYTE_COLOR},
    {"string",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__string_doc,
     (void *)CD_PROP_STRING},

    {"shape",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__shape_doc,
     (void *)CD_SHAPEKEY},
    {"skin",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__skin_doc,
     (void *)CD_MVERT_SKIN},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeraccess_edge_getseters[] = {
    {"float",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_doc,
     (void *)CD_PROP_FLOAT},
    {"int",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__int_doc,
     (void *)CD_PROP_INT32},
    {"bool",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__bool_doc,
     (void *)CD_PROP_BOOL},
    {"float_vector",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_vector_doc,
     (void *)CD_PROP_FLOAT3},
    {"float_color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_color_doc,
     (void *)CD_PROP_COLOR},
    {"color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__color_doc,
     (void *)CD_PROP_BYTE_COLOR},
    {"string",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__string_doc,
     (void *)CD_PROP_STRING},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeraccess_face_getseters[] = {
    {"float",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_doc,
     (void *)CD_PROP_FLOAT},
    {"int",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__int_doc,
     (void *)CD_PROP_INT32},
    {"bool",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__bool_doc,
     (void *)CD_PROP_BOOL},
    {"float_vector",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_vector_doc,
     (void *)CD_PROP_FLOAT3},
    {"float_color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_color_doc,
     (void *)CD_PROP_COLOR},
    {"color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__color_doc,
     (void *)CD_PROP_BYTE_COLOR},
    {"string",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__string_doc,
     (void *)CD_PROP_STRING},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeraccess_loop_getseters[] = {
    {"float",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_doc,
     (void *)CD_PROP_FLOAT},
    {"int",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__int_doc,
     (void *)CD_PROP_INT32},
    {"bool",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__bool_doc,
     (void *)CD_PROP_BOOL},
    {"float_vector",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_vector_doc,
     (void *)CD_PROP_FLOAT3},
    {"float_color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__float_color_doc,
     (void *)CD_PROP_COLOR},
    {"string",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__string_doc,
     (void *)CD_PROP_STRING},
    {"uv",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__uv_doc,
     (void *)CD_PROP_FLOAT2},
    {"color",
     (getter)bpy_bmlayeraccess_collection_get,
     (setter) nullptr,
     bpy_bmlayeraccess_collection__color_doc,
     (void *)CD_PROP_BYTE_COLOR},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmlayercollection_getseters[] = {
    /* BMESH_TODO, make writeable */
    {"active",
     (getter)bpy_bmlayercollection_active_get,
     (setter) nullptr,
     bpy_bmlayercollection_active_doc,
     nullptr},
    {"is_singleton",
     (getter)bpy_bmlayercollection_is_singleton_get,
     (setter) nullptr,
     bpy_bmlayercollection_is_singleton_doc,
     nullptr},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmlayeritem_getseters[] = {
    /* BMESH_TODO, make writeable */
    {"name",
     (getter)bpy_bmlayeritem_name_get,
     (setter) nullptr,
     bpy_bmlayercollection_name_doc,
     nullptr},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/* Methods
 * ======= */

/* BMLayerCollection
 * ----------------- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeritem_copy_from_doc,
    ".. method:: copy_from(other)\n"
    "\n"
    "   Return a copy of the layer\n"
    "\n"
    "   :arg other: Another layer to copy from.\n"
    "   :type other: :class:`bmesh.types.BMLayerItem`\n");
static PyObject *bpy_bmlayeritem_copy_from(BPy_BMLayerItem *self, BPy_BMLayerItem *value)
{
  const char *error_prefix = "layer.copy_from(...)";
  CustomData *data;

  if (!BPy_BMLayerItem_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "%s: expected BMLayerItem, not '%.200s'",
                 error_prefix,
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(self);
  BPY_BM_CHECK_SOURCE_OBJ(self->bm, error_prefix, value);

  if ((self->htype != value->htype) || (self->type != value->type)) {
    PyErr_Format(PyExc_ValueError, "%s: layer type mismatch", error_prefix);
  }

  else if (self->index == value->index) {
    Py_RETURN_NONE;
  }

  data = bpy_bm_customdata_get(self->bm, self->htype);

  if ((bpy_bmlayeritem_get(self) == nullptr) || (bpy_bmlayeritem_get(value) == nullptr)) {
    return nullptr;
  }

  BM_data_layer_copy(self->bm, data, self->type, value->index, self->index);

  Py_RETURN_NONE;
}

/* similar to new(), but no name arg. */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_verify_doc,
    ".. method:: verify()\n"
    "\n"
    "   Create a new layer or return an existing active layer\n"
    "\n"
    "   :return: The newly verified layer.\n"
    "   :rtype: :class:`bmesh.types.BMLayerItem`\n");
static PyObject *bpy_bmlayercollection_verify(BPy_BMLayerCollection *self)
{
  int index;
  CustomData *data;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);

  index = CustomData_get_active_layer(data, eCustomDataType(self->type)); /* type relative */

  if (index == -1) {
    BM_data_layer_add(self->bm, data, self->type);
    index = 0;
  }
  if (self->type == CD_PROP_FLOAT2 && self->htype == BM_LOOP) {
    /* Because adding CustomData layers to a bmesh will invalidate any existing pointers
     * in Py objects we can't lazily add the associated bool layers. So add them all right
     * now. */
    BM_uv_map_attr_pin_ensure_for_all_layers(self->bm);
  }

  BLI_assert(index >= 0);

  return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_new_doc,
    ".. method:: new(name)\n"
    "\n"
    "   Create a new layer\n"
    "\n"
    "   :arg name: Optional name argument (will be made unique).\n"
    "   :type name: str\n"
    "   :return: The newly created layer.\n"
    "   :rtype: :class:`bmesh.types.BMLayerItem`\n");
static PyObject *bpy_bmlayercollection_new(BPy_BMLayerCollection *self, PyObject *args)
{
  const char *name = nullptr;
  int index;
  CustomData *data;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|s:new", &name)) {
    return nullptr;
  }

  data = bpy_bm_customdata_get(self->bm, self->htype);

  if (CustomData_layertype_is_singleton(eCustomDataType(self->type)) &&
      CustomData_has_layer(data, eCustomDataType(self->type)))
  {
    PyErr_SetString(PyExc_ValueError, "layers.new(): is a singleton, use verify() instead");
    return nullptr;
  }

  if (name) {
    BM_data_layer_add_named(self->bm, data, self->type, name);
  }
  else {
    BM_data_layer_add(self->bm, data, self->type);
  }

  if (self->type == CD_PROP_FLOAT2 && self->htype == BM_LOOP) {
    /* Because adding CustomData layers to a bmesh will invalidate any existing pointers
     * in Py objects we can't lazily add the associated bool layers. So add them all right
     * now. */
    BM_uv_map_attr_pin_ensure_for_all_layers(self->bm);
  }

  index = CustomData_number_of_layers(data, eCustomDataType(self->type)) - 1;
  BLI_assert(index >= 0);

  return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_remove_doc,
    ".. method:: remove(layer)\n"
    "\n"
    "   Remove a layer\n"
    "\n"
    "   :arg layer: The layer to remove.\n"
    "   :type layer: :class:`bmesh.types.BMLayerItem`\n");
static PyObject *bpy_bmlayercollection_remove(BPy_BMLayerCollection *self, BPy_BMLayerItem *value)
{
  CustomData *data;

  BPY_BM_CHECK_OBJ(self);

  if (!BPy_BMLayerItem_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "layers.remove(x): expected BMLayerItem, not '%.200s'",
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(value);

  if ((self->bm != value->bm) || (self->type != value->type) || (self->htype != value->htype)) {
    PyErr_SetString(PyExc_ValueError, "layers.remove(x): x not in layers");
  }

  data = bpy_bm_customdata_get(self->bm, self->htype);
  BM_data_layer_free_n(self->bm, data, self->type, value->index);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_keys_doc,
    ".. method:: keys()\n"
    "\n"
    "   Return the identifiers of collection members\n"
    "   (matching Python's dict.keys() functionality).\n"
    "\n"
    "   :return: the identifiers for each member of this collection.\n"
    "   :rtype: list[str]\n");
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
  index = CustomData_get_layer_index(data, eCustomDataType(self->type));

  tot = (index != -1) ? CustomData_number_of_layers(data, eCustomDataType(self->type)) : 0;

  ret = PyList_New(tot);

  for (i = 0; tot-- > 0; index++) {
    item = PyUnicode_FromString(data->layers[index].name);
    PyList_SET_ITEM(ret, i++, item);
  }

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_items_doc,
    ".. method:: items()\n"
    "\n"
    "   Return the identifiers of collection members\n"
    "   (matching Python's dict.items() functionality).\n"
    "\n"
    "   :return: (key, value) pairs for each member of this collection.\n"
    "   :rtype: list[tuple[str, :class:`bmesh.types.BMLayerItem`]]\n");
static PyObject *bpy_bmlayercollection_items(BPy_BMLayerCollection *self)
{
  PyObject *ret;
  PyObject *item;
  int index;
  CustomData *data;
  int tot, i;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_layer_index(data, eCustomDataType(self->type));
  tot = (index != -1) ? CustomData_number_of_layers(data, eCustomDataType(self->type)) : 0;

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

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_values_doc,
    ".. method:: values()\n"
    "\n"
    "   Return the values of collection\n"
    "   (matching Python's dict.values() functionality).\n"
    "\n"
    "   :return: the members of this collection.\n"
    "   :rtype: list[:class:`bmesh.types.BMLayerItem`]\n");
static PyObject *bpy_bmlayercollection_values(BPy_BMLayerCollection *self)
{
  PyObject *ret;
  PyObject *item;
  int index;
  CustomData *data;
  int tot, i;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_layer_index(data, eCustomDataType(self->type));
  tot = (index != -1) ? CustomData_number_of_layers(data, eCustomDataType(self->type)) : 0;

  ret = PyList_New(tot);

  for (i = 0; tot-- > 0; index++) {
    item = BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, i);
    PyList_SET_ITEM(ret, i++, item);
  }

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_get_doc,
    ".. method:: get(key, default=None)\n"
    "\n"
    "   Returns the value of the layer matching the key or default\n"
    "   when not found (matches Python's dictionary function of the same name).\n"
    "\n"
    "   :arg key: The key associated with the layer.\n"
    "   :type key: str\n"
    "   :arg default: Optional argument for the value to return if\n"
    "      *key* is not found.\n"
    "   :type default: Any\n");
static PyObject *bpy_bmlayercollection_get(BPy_BMLayerCollection *self, PyObject *args)
{
  const char *key;
  PyObject *def = Py_None;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|O:get", &key, &def)) {
    return nullptr;
  }

  CustomData *data;
  int index;

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_named_layer(data, eCustomDataType(self->type), key); /* type relative */

  if (index != -1) {
    return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
  }

  return Py_NewRef(def);
}

static PyMethodDef bpy_bmlayeritem_methods[] = {
    {"copy_from", (PyCFunction)bpy_bmlayeritem_copy_from, METH_O, bpy_bmlayeritem_copy_from_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef bpy_bmelemseq_methods[] = {
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
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/* Sequences
 * ========= */

static Py_ssize_t bpy_bmlayercollection_length(BPy_BMLayerCollection *self)
{
  CustomData *data;

  BPY_BM_CHECK_INT(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);

  return CustomData_number_of_layers(data, eCustomDataType(self->type));
}

static PyObject *bpy_bmlayercollection_subscript_str(BPy_BMLayerCollection *self,
                                                     const char *keyname)
{
  CustomData *data;
  int index;

  BPY_BM_CHECK_OBJ(self);

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_named_layer(
      data, eCustomDataType(self->type), keyname); /* type relative */

  if (index != -1) {
    return BPy_BMLayerItem_CreatePyObject(self->bm, self->htype, self->type, index);
  }

  PyErr_Format(PyExc_KeyError, "BMLayerCollection[key]: key \"%.200s\" not found", keyname);
  return nullptr;
}

static PyObject *bpy_bmlayercollection_subscript_int(BPy_BMLayerCollection *self,
                                                     Py_ssize_t keynum)
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
  return nullptr;
}

static PyObject *bpy_bmlayercollection_subscript_slice(BPy_BMLayerCollection *self,
                                                       Py_ssize_t start,
                                                       Py_ssize_t stop)
{
  const Py_ssize_t len = bpy_bmlayercollection_length(self);
  int count = 0;

  PyObject *tuple;

  BPY_BM_CHECK_OBJ(self);

  start = std::min(start, len);
  stop = std::min(stop, len);

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
    return bpy_bmlayercollection_subscript_str(self, PyUnicode_AsUTF8(key));
  }
  if (PyIndex_Check(key)) {
    const Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }
    return bpy_bmlayercollection_subscript_int(self, i);
  }
  if (PySlice_Check(key)) {
    PySliceObject *key_slice = (PySliceObject *)key;
    Py_ssize_t step = 1;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return nullptr;
    }
    if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "BMLayerCollection[slice]: slice steps not supported");
      return nullptr;
    }
    if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      return bpy_bmlayercollection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
    }

    Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

    /* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
    if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) {
      return nullptr;
    }
    if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop)) {
      return nullptr;
    }

    if (start < 0 || stop < 0) {
      /* only get the length for negative values */
      const Py_ssize_t len = bpy_bmlayercollection_length(self);
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
      return PyTuple_New(0);
    }

    return bpy_bmlayercollection_subscript_slice(self, start, stop);
  }

  PyErr_SetString(PyExc_AttributeError, "BMLayerCollection[key]: invalid key, key must be an int");
  return nullptr;
}

static int bpy_bmlayercollection_contains(BPy_BMLayerCollection *self, PyObject *value)
{
  const char *keyname = PyUnicode_AsUTF8(value);
  CustomData *data;
  int index;

  BPY_BM_CHECK_INT(self);

  if (keyname == nullptr) {
    PyErr_SetString(PyExc_TypeError, "BMLayerCollection.__contains__: expected a string");
    return -1;
  }

  data = bpy_bm_customdata_get(self->bm, self->htype);
  index = CustomData_get_named_layer_index(data, eCustomDataType(self->type), keyname);

  return (index != -1) ? 1 : 0;
}

static PySequenceMethods bpy_bmlayercollection_as_sequence = {
    /*sq_length*/ (lenfunc)bpy_bmlayercollection_length,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /* Only set this so `PySequence_Check()` returns True. */
    /*sq_item*/ (ssizeargfunc)bpy_bmlayercollection_subscript_int,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ (objobjproc)bpy_bmlayercollection_contains,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods bpy_bmlayercollection_as_mapping = {
    /*mp_length*/ (lenfunc)bpy_bmlayercollection_length,
    /*mp_subscript*/ (binaryfunc)bpy_bmlayercollection_subscript,
    /*mp_ass_subscript*/ (objobjargproc) nullptr,
};

/* Iterator
 * -------- */

static PyObject *bpy_bmlayercollection_iter(BPy_BMLayerCollection *self)
{
  /* fake it with a list iterator */
  PyObject *ret;
  PyObject *iter = nullptr;

  BPY_BM_CHECK_OBJ(self);

  ret = bpy_bmlayercollection_subscript_slice(self, 0, PY_SSIZE_T_MAX);

  if (ret) {
    iter = PyObject_GetIter(ret);
    Py_DECREF(ret);
  }

  return iter;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeraccess_type_doc,
    "Exposes custom-data layer attributes.\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayercollection_type_doc,
    "Gives access to a collection of custom-data layers of the same type and behaves "
    "like Python dictionaries, "
    "except for the ability to do list like index access.\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmlayeritem_type_doc,
    "Exposes a single custom data layer, "
    "their main purpose is for use as item accessors to custom-data when used with "
    "vert/edge/face/loop data.\n");

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
      BLI_assert_unreachable();
      type = nullptr;
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

void BPy_BM_init_types_customdata()
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

  BPy_BMLayerAccessVert_Type.tp_repr = (reprfunc) nullptr;
  BPy_BMLayerAccessEdge_Type.tp_repr = (reprfunc) nullptr;
  BPy_BMLayerAccessFace_Type.tp_repr = (reprfunc) nullptr;
  BPy_BMLayerAccessLoop_Type.tp_repr = (reprfunc) nullptr;
  BPy_BMLayerCollection_Type.tp_repr = (reprfunc) nullptr;
  BPy_BMLayerItem_Type.tp_repr = (reprfunc) nullptr;

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

  BPy_BMLayerAccessVert_Type.tp_dealloc = nullptr;
  BPy_BMLayerAccessEdge_Type.tp_dealloc = nullptr;
  BPy_BMLayerAccessFace_Type.tp_dealloc = nullptr;
  BPy_BMLayerAccessLoop_Type.tp_dealloc = nullptr;
  BPy_BMLayerCollection_Type.tp_dealloc = nullptr;
  BPy_BMLayerItem_Type.tp_dealloc = nullptr;

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
 * helper function for get/set, nullptr return means the error is set
 */
static void *bpy_bmlayeritem_ptr_get(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer)
{
  void *value;
  BMElem *ele = py_ele->ele;
  CustomData *data;

  /* error checking */
  if (UNLIKELY(!BPy_BMLayerItem_Check(py_layer))) {
    PyErr_SetString(PyExc_AttributeError, "BMElem[key]: invalid key, must be a BMLayerItem");
    return nullptr;
  }
  if (UNLIKELY(py_ele->bm != py_layer->bm)) {
    PyErr_SetString(PyExc_ValueError, "BMElem[layer]: layer is from another mesh");
    return nullptr;
  }
  if (UNLIKELY(ele->head.htype != py_layer->htype)) {
    char namestr_1[32], namestr_2[32];
    PyErr_Format(PyExc_ValueError,
                 "Layer/Element type mismatch, expected %.200s got layer type %.200s",
                 BPy_BMElem_StringFromHType_ex(ele->head.htype, namestr_1),
                 BPy_BMElem_StringFromHType_ex(py_layer->htype, namestr_2));
    return nullptr;
  }

  data = bpy_bm_customdata_get(py_layer->bm, py_layer->htype);

  value = CustomData_bmesh_get_n(
      data, ele->head.data, eCustomDataType(py_layer->type), py_layer->index);

  if (UNLIKELY(value == nullptr)) {
    /* this should be fairly unlikely but possible if layers move about after we get them */
    PyErr_SetString(PyExc_KeyError, "BMElem[key]: layer not found");
    return nullptr;
  }

  return value;
}

PyObject *BPy_BMLayerItem_GetItem(BPy_BMElem *py_ele, BPy_BMLayerItem *py_layer)
{
  void *value = bpy_bmlayeritem_ptr_get(py_ele, py_layer);
  PyObject *ret;

  if (UNLIKELY(value == nullptr)) {
    return nullptr;
  }

  switch (py_layer->type) {
    case CD_MDEFORMVERT: {
      ret = BPy_BMDeformVert_CreatePyObject(static_cast<MDeformVert *>(value));
      break;
    }
    case CD_PROP_FLOAT: {
      ret = PyFloat_FromDouble(*(float *)value);
      break;
    }
    case CD_PROP_INT32: {
      ret = PyLong_FromLong(*(int *)value);
      break;
    }
    case CD_PROP_BOOL: {
      ret = PyBool_FromLong(*(bool *)value);
      break;
    }
    case CD_PROP_FLOAT3: {
      ret = Vector_CreatePyObject_wrap((float *)value, 3, nullptr);
      break;
    }
    case CD_PROP_COLOR: {
      ret = Vector_CreatePyObject_wrap((float *)value, 4, nullptr);
      break;
    }
    case CD_PROP_STRING: {
      MStringProperty *mstring = static_cast<MStringProperty *>(value);
      ret = PyBytes_FromStringAndSize(mstring->s, mstring->s_len);
      break;
    }
    case CD_PROP_FLOAT2: {
      if (UNLIKELY(py_ele->bm != py_layer->bm)) {
        PyErr_SetString(PyExc_ValueError, "BMElem[layer]: layer is from another mesh");
        return nullptr;
      }
      ret = BPy_BMLoopUV_CreatePyObject(py_ele->bm, (BMLoop *)py_ele->ele, py_layer->index);
      break;
    }
    case CD_PROP_BYTE_COLOR: {
      ret = BPy_BMLoopColor_CreatePyObject(static_cast<MLoopCol *>(value));
      break;
    }
    case CD_SHAPEKEY: {
      ret = Vector_CreatePyObject_wrap((float *)value, 3, nullptr);
      break;
    }
    case CD_MVERT_SKIN: {
      ret = BPy_BMVertSkin_CreatePyObject(static_cast<MVertSkin *>(value));
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

  if (UNLIKELY(value == nullptr)) {
    return -1;
  }

  switch (py_layer->type) {
    case CD_MDEFORMVERT: {
      ret = BPy_BMDeformVert_AssignPyObject(static_cast<MDeformVert *>(value), py_value);
      break;
    }
    case CD_PROP_FLOAT: {
      const float tmp_val = PyFloat_AsDouble(py_value);
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
    case CD_PROP_INT32: {
      const int tmp_val = PyC_Long_AsI32(py_value);
      if (UNLIKELY(tmp_val == -1 && PyErr_Occurred())) {
        /* error is set */
        ret = -1;
      }
      else {
        *(int *)value = tmp_val;
      }
      break;
    }
    case CD_PROP_BOOL: {
      const int tmp_val = PyC_Long_AsBool(py_value);
      if (UNLIKELY(tmp_val == -1)) {
        /* The error has been set. */
        ret = -1;
      }
      else {
        *(bool *)value = tmp_val;
      }
      break;
    }
    case CD_PROP_FLOAT3: {
      if (mathutils_array_parse((float *)value, 3, 3, py_value, "BMElem Float Vector") == -1) {
        ret = -1;
      }
      break;
    }
    case CD_PROP_COLOR: {
      if (mathutils_array_parse((float *)value, 4, 4, py_value, "BMElem Float Color") == -1) {
        ret = -1;
      }
      break;
    }
    case CD_PROP_STRING: {
      MStringProperty *mstring = static_cast<MStringProperty *>(value);
      char *tmp_val;
      Py_ssize_t tmp_val_len;
      if (UNLIKELY(PyBytes_AsStringAndSize(py_value, &tmp_val, &tmp_val_len) == -1)) {
        PyErr_Format(PyExc_TypeError, "expected bytes, not a %.200s", Py_TYPE(py_value)->tp_name);
        ret = -1;
      }
      else {
        tmp_val_len = std::min<ulong>(tmp_val_len, sizeof(mstring->s));
        memcpy(mstring->s, tmp_val, tmp_val_len);
        mstring->s_len = tmp_val_len;
      }
      break;
    }
    case CD_PROP_FLOAT2: {
      if (UNLIKELY(py_ele->bm != py_layer->bm)) {
        PyErr_SetString(PyExc_ValueError, "BMElem[layer]: layer is from another mesh");
        ret = -1;
      }
      else {
        ret = BPy_BMLoopUV_AssignPyObject(py_ele->bm, (BMLoop *)py_ele->ele, py_value);
      }
      break;
    }
    case CD_PROP_BYTE_COLOR: {
      ret = BPy_BMLoopColor_AssignPyObject(static_cast<MLoopCol *>(value), py_value);
      break;
    }
    case CD_SHAPEKEY: {
      float tmp_val[3];
      if (UNLIKELY(mathutils_array_parse(tmp_val, 3, 3, py_value, "BMVert[shape] = value") == -1))
      {
        ret = -1;
      }
      else {
        copy_v3_v3((float *)value, tmp_val);
      }
      break;
    }
    case CD_MVERT_SKIN: {
      ret = BPy_BMVertSkin_AssignPyObject(static_cast<MVertSkin *>(value), py_value);
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
