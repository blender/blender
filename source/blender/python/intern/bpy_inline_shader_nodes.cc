/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_node.hh"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "DNA_world_types.h"
#include "NOD_shader_nodes_inline.hh"

#include "bpy_inline_shader_nodes.hh"
#include "bpy_rna.hh"

#include "../generic/py_capi_utils.hh"

extern PyTypeObject bpy_inline_shader_nodes_Type;

struct BPy_InlineShaderNodes {
  PyObject_HEAD
  bNodeTree *inline_node_tree;
};

static BPy_InlineShaderNodes *create_from_shader_node_tree(const bNodeTree &tree)
{
  BPy_InlineShaderNodes *self = reinterpret_cast<BPy_InlineShaderNodes *>(
      bpy_inline_shader_nodes_Type.tp_alloc(&bpy_inline_shader_nodes_Type, 0));
  if (!self) {
    return nullptr;
  }
  self->inline_node_tree = blender::bke::node_tree_add_tree(
      nullptr, (blender::StringRef(tree.id.name) + " Inlined").c_str(), tree.idname);
  blender::nodes::InlineShaderNodeTreeParams params;
  blender::nodes::inline_shader_node_tree(tree, *self->inline_node_tree, params);
  return self;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_inline_shader_nodes_from_material_doc,
    ".. staticmethod:: from_material(material)\n"
    "\n"
    "   Create an inlined shader node tree from a material.\n"
    "\n"
    "   :arg material: The material to inline the node tree of.\n"
    "   :type material: bpy.types.Material\n");
static BPy_InlineShaderNodes *BPy_InlineShaderNodes_static_from_material(PyObject * /*self*/,
                                                                         PyObject *args,
                                                                         PyObject *kwds)
{
  static const char *kwlist[] = {"material", nullptr};
  PyObject *py_material;
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", const_cast<char **>(kwlist), &py_material)) {
    return nullptr;
  }
  ID *material_id = nullptr;
  if (!pyrna_id_FromPyObject(py_material, &material_id)) {
    PyErr_Format(
        PyExc_TypeError, "Expected a Material, not %.200s", Py_TYPE(py_material)->tp_name);
    return nullptr;
  }
  if (GS(material_id->name) != ID_MA) {
    PyErr_Format(PyExc_TypeError,
                 "Expected a Material, not %.200s",
                 BKE_idtype_idcode_to_name(GS(material_id->name)));
    return nullptr;
  }
  Material *material = blender::id_cast<Material *>(material_id);
  if (!material->nodetree) {
    PyErr_Format(PyExc_TypeError, "Material '%s' has no node tree", BKE_id_name(*material_id));
    return nullptr;
  }
  return create_from_shader_node_tree(*material->nodetree);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_inline_shader_nodes_from_light_doc,
    ".. staticmethod:: from_light(light)\n"
    "\n"
    "   Create an inlined shader node tree from a light.\n"
    "\n"
    "   :arg light: The light to online the node tree of.\n"
    "   :type light: bpy.types.Light\n");
static BPy_InlineShaderNodes *BPy_InlineShaderNodes_static_from_light(PyObject * /*self*/,
                                                                      PyObject *args,
                                                                      PyObject *kwds)
{
  static const char *kwlist[] = {"light", nullptr};
  PyObject *py_light;
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", const_cast<char **>(kwlist), &py_light)) {
    return nullptr;
  }
  ID *light_id = nullptr;
  if (!pyrna_id_FromPyObject(py_light, &light_id)) {
    PyErr_Format(PyExc_TypeError, "Expected a Light, not %.200s", Py_TYPE(py_light)->tp_name);
    return nullptr;
  }
  if (GS(light_id->name) != ID_LA) {
    PyErr_Format(PyExc_TypeError,
                 "Expected a Light, not %.200s",
                 BKE_idtype_idcode_to_name(GS(light_id->name)));
    return nullptr;
  }
  Light *light = blender::id_cast<Light *>(light_id);
  if (!light->nodetree) {
    PyErr_Format(PyExc_TypeError, "Light '%s' has no node tree", BKE_id_name(*light_id));
    return nullptr;
  }
  return create_from_shader_node_tree(*light->nodetree);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_inline_shader_nodes_from_world_doc,
    ".. staticmethod:: from_world(world)\n"
    "\n"
    "   Create an inlined shader node tree from a world.\n"
    "\n"
    "   :arg world: The world to inline the node tree of.\n"
    "   :type world: bpy.types.World\n");
static BPy_InlineShaderNodes *BPy_InlineShaderNodes_static_from_world(PyObject * /*self*/,
                                                                      PyObject *args,
                                                                      PyObject *kwds)
{
  static const char *kwlist[] = {"world", nullptr};
  PyObject *py_world;
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", const_cast<char **>(kwlist), &py_world)) {
    return nullptr;
  }
  ID *world_id = nullptr;
  if (!pyrna_id_FromPyObject(py_world, &world_id)) {
    PyErr_Format(PyExc_TypeError, "Expected a World, not %.200s", Py_TYPE(py_world)->tp_name);
    return nullptr;
  }
  if (GS(world_id->name) != ID_WO) {
    PyErr_Format(PyExc_TypeError,
                 "Expected a World, not %.200s",
                 BKE_idtype_idcode_to_name(GS(world_id->name)));
    return nullptr;
  }
  World *world = blender::id_cast<World *>(world_id);
  if (!world->nodetree) {
    PyErr_Format(PyExc_TypeError, "World '%s' has no node tree", BKE_id_name(*world_id));
    return nullptr;
  }
  return create_from_shader_node_tree(*world->nodetree);
}

static void BPy_InlineShaderNodes_dealloc(BPy_InlineShaderNodes *self)
{
  BKE_id_free(nullptr, self->inline_node_tree);
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject *>(self));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_inline_shader_nodes_node_tree_doc,
    "The inlined node tree.\n"
    "\n"
    ":type: :class:`bpy.types.NodeTree`\n");
static PyObject *BPy_InlineShaderNodes_get_node_tree(BPy_InlineShaderNodes *self,
                                                     void * /*closure*/)
{
  return pyrna_id_CreatePyObject(blender::id_cast<ID *>(self->inline_node_tree));
}

static PyGetSetDef BPy_InlineShaderNodes_getseters[] = {
    {"node_tree",
     (getter)BPy_InlineShaderNodes_get_node_tree,
     nullptr,
     bpy_inline_shader_nodes_node_tree_doc,
     nullptr},
    {nullptr},
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

static PyMethodDef BPy_InlineShaderNodes_methods[] = {
    {"from_material",
     (PyCFunction)BPy_InlineShaderNodes_static_from_material,
     METH_VARARGS | METH_KEYWORDS | METH_STATIC,
     bpy_inline_shader_nodes_from_material_doc},
    {"from_light",
     (PyCFunction)BPy_InlineShaderNodes_static_from_light,
     METH_VARARGS | METH_KEYWORDS | METH_STATIC,
     bpy_inline_shader_nodes_from_light_doc},
    {"from_world",
     (PyCFunction)BPy_InlineShaderNodes_static_from_world,
     METH_VARARGS | METH_KEYWORDS | METH_STATIC,
     bpy_inline_shader_nodes_from_world_doc},
    {nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyDoc_STRVAR(
    /* Wrap. */
    bpy_inline_shader_nodes_doc,
    "An inlined shader node tree.\n");
PyTypeObject bpy_inline_shader_nodes_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "InlineShaderNodes",
    /*tp_basicsize*/ sizeof(BPy_InlineShaderNodes),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ reinterpret_cast<destructor>(BPy_InlineShaderNodes_dealloc),
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
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ bpy_inline_shader_nodes_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_InlineShaderNodes_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_InlineShaderNodes_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

PyObject *BPyInit_inline_shader_nodes_type()
{
  if (PyType_Ready(&bpy_inline_shader_nodes_Type) < 0) {
    return nullptr;
  }
  return reinterpret_cast<PyObject *>(&bpy_inline_shader_nodes_Type);
}
