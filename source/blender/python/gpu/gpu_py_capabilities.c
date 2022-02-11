/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_capabilities.h"

#include "gpu_py_capabilities.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Functions
 * \{ */

PyDoc_STRVAR(pygpu_max_texture_size_get_doc,
             ".. function:: max_texture_size_get()\n"
             "\n"
             "   Get estimated maximum texture size to be able to handle.\n"
             "\n"
             "   :return: Texture size.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_texture_size_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_texture_size());
}

PyDoc_STRVAR(pygpu_max_texture_layers_get_doc,
             ".. function:: max_texture_layers_get()\n"
             "\n"
             "   Get maximum number of layers in texture.\n"
             "\n"
             "   :return: Number of layers.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_texture_layers_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_texture_layers());
}

PyDoc_STRVAR(pygpu_max_textures_get_doc,
             ".. function:: max_textures_get()\n"
             "\n"
             "   Get maximum supported texture image units used for\n"
             "   accessing texture maps from the vertex shader and the\n"
             "   fragment processor.\n"
             "\n"
             "   :return: Texture image units.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_textures_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_textures());
}

PyDoc_STRVAR(pygpu_max_textures_vert_get_doc,
             ".. function:: max_textures_vert_get()\n"
             "\n"
             "   Get maximum supported texture image units used for\n"
             "   accessing texture maps from the vertex shader.\n"
             "\n"
             "   :return: Texture image units.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_textures_vert_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_textures_vert());
}

PyDoc_STRVAR(pygpu_max_textures_geom_get_doc,
             ".. function:: max_textures_geom_get()\n"
             "\n"
             "   Get maximum supported texture image units used for\n"
             "   accessing texture maps from the geometry shader.\n"
             "\n"
             "   :return: Texture image units.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_textures_geom_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_textures_geom());
}

PyDoc_STRVAR(pygpu_max_textures_frag_get_doc,
             ".. function:: max_textures_frag_get()\n"
             "\n"
             "   Get maximum supported texture image units used for\n"
             "   accessing texture maps from the fragment shader.\n"
             "\n"
             "   :return: Texture image units.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_textures_frag_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_textures_frag());
}

PyDoc_STRVAR(pygpu_max_uniforms_vert_get_doc,
             ".. function:: max_uniforms_vert_get()\n"
             "\n"
             "   Get maximum number of values held in uniform variable\n"
             "   storage for a vertex shader.\n"
             "\n"
             "   :return: Number of values.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_uniforms_vert_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_uniforms_vert());
}

PyDoc_STRVAR(pygpu_max_uniforms_frag_get_doc,
             ".. function:: max_uniforms_frag_get()\n"
             "\n"
             "   Get maximum number of values held in uniform variable\n"
             "   storage for a fragment shader.\n"
             "\n"
             "   :return: Number of values.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_uniforms_frag_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_uniforms_frag());
}

PyDoc_STRVAR(pygpu_max_batch_indices_get_doc,
             ".. function:: max_batch_indices_get()\n"
             "\n"
             "   Get maximum number of vertex array indices.\n"
             "\n"
             "   :return: Number of indices.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_batch_indices_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_batch_indices());
}

PyDoc_STRVAR(pygpu_max_batch_vertices_get_doc,
             ".. function:: max_batch_vertices_get()\n"
             "\n"
             "   Get maximum number of vertex array vertices.\n"
             "\n"
             "   :return: Number of vertices.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_batch_vertices_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_batch_vertices());
}

PyDoc_STRVAR(pygpu_max_vertex_attribs_get_doc,
             ".. function:: max_vertex_attribs_get()\n"
             "\n"
             "   Get maximum number of vertex attributes accessible to\n"
             "   a vertex shader.\n"
             "\n"
             "   :return: Number of attributes.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_vertex_attribs_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_vertex_attribs());
}

PyDoc_STRVAR(pygpu_max_varying_floats_get_doc,
             ".. function:: max_varying_floats_get()\n"
             "\n"
             "   Get maximum number of varying variables used by\n"
             "   vertex and fragment shaders.\n"
             "\n"
             "   :return: Number of variables.\n"
             "   :rtype: int\n");
static PyObject *pygpu_max_varying_floats_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_varying_floats());
}

PyDoc_STRVAR(pygpu_extensions_get_doc,
             ".. function:: extensions_get()\n"
             "\n"
             "   Get supported extensions in the current context.\n"
             "\n"
             "   :return: Extensions.\n"
             "   :rtype: tuple of string\n");
static PyObject *pygpu_extensions_get(PyObject *UNUSED(self))
{
  int extensions_len = GPU_extensions_len();
  PyObject *ret = PyTuple_New(extensions_len);
  PyObject **ob_items = ((PyTupleObject *)ret)->ob_item;
  for (int i = 0; i < extensions_len; i++) {
    ob_items[i] = PyUnicode_FromString(GPU_extension_get(i));
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

static struct PyMethodDef pygpu_capabilities__tp_methods[] = {
    {"max_texture_size_get",
     (PyCFunction)pygpu_max_texture_size_get,
     METH_NOARGS,
     pygpu_max_texture_size_get_doc},
    {"max_texture_layers_get",
     (PyCFunction)pygpu_max_texture_layers_get,
     METH_NOARGS,
     pygpu_max_texture_layers_get_doc},
    {"max_textures_get",
     (PyCFunction)pygpu_max_textures_get,
     METH_NOARGS,
     pygpu_max_textures_get_doc},
    {"max_textures_vert_get",
     (PyCFunction)pygpu_max_textures_vert_get,
     METH_NOARGS,
     pygpu_max_textures_vert_get_doc},
    {"max_textures_geom_get",
     (PyCFunction)pygpu_max_textures_geom_get,
     METH_NOARGS,
     pygpu_max_textures_geom_get_doc},
    {"max_textures_frag_get",
     (PyCFunction)pygpu_max_textures_frag_get,
     METH_NOARGS,
     pygpu_max_textures_frag_get_doc},
    {"max_uniforms_vert_get",
     (PyCFunction)pygpu_max_uniforms_vert_get,
     METH_NOARGS,
     pygpu_max_uniforms_vert_get_doc},
    {"max_uniforms_frag_get",
     (PyCFunction)pygpu_max_uniforms_frag_get,
     METH_NOARGS,
     pygpu_max_uniforms_frag_get_doc},
    {"max_batch_indices_get",
     (PyCFunction)pygpu_max_batch_indices_get,
     METH_NOARGS,
     pygpu_max_batch_indices_get_doc},
    {"max_batch_vertices_get",
     (PyCFunction)pygpu_max_batch_vertices_get,
     METH_NOARGS,
     pygpu_max_batch_vertices_get_doc},
    {"max_vertex_attribs_get",
     (PyCFunction)pygpu_max_vertex_attribs_get,
     METH_NOARGS,
     pygpu_max_vertex_attribs_get_doc},
    {"max_varying_floats_get",
     (PyCFunction)pygpu_max_varying_floats_get,
     METH_NOARGS,
     pygpu_max_varying_floats_get_doc},
    {"extensions_get", (PyCFunction)pygpu_extensions_get, METH_NOARGS, pygpu_extensions_get_doc},
    {NULL, NULL, 0, NULL},
};

PyDoc_STRVAR(pygpu_capabilities__tp_doc, "This module provides access to the GPU capabilities.");
static PyModuleDef pygpu_capabilities_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpu.capabilities",
    .m_doc = pygpu_capabilities__tp_doc,
    .m_methods = pygpu_capabilities__tp_methods,
};

PyObject *bpygpu_capabilities_init(void)
{
  PyObject *submodule;

  submodule = PyModule_Create(&pygpu_capabilities_module_def);

  return submodule;
}

/** \} */
