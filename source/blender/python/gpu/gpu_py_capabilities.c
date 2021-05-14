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
 * \ingroup bpygpu
 *
 * - Use ``bpygpu_`` for local API.
 * - Use ``BPyGPU`` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_capabilities.h"

#include "gpu_py_capabilities.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Functions
 * \{ */

static PyObject *pygpu_max_texture_size_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_texture_size());
}

static PyObject *pygpu_max_texture_layers_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_texture_layers());
}

static PyObject *pygpu_max_textures_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_textures());
}

static PyObject *pygpu_max_textures_vert_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_textures_vert());
}

static PyObject *pygpu_max_textures_geom_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_textures_geom());
}

static PyObject *pygpu_max_textures_frag_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_textures_frag());
}

static PyObject *pygpu_max_uniforms_vert_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_uniforms_vert());
}

static PyObject *pygpu_max_uniforms_frag_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_uniforms_frag());
}

static PyObject *pygpu_max_batch_indices_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_batch_indices());
}

static PyObject *pygpu_max_batch_vertices_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_batch_vertices());
}

static PyObject *pygpu_max_vertex_attribs_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_vertex_attribs());
}

static PyObject *pygpu_max_varying_floats_get(PyObject *UNUSED(self))
{
  return PyLong_FromLong(GPU_max_varying_floats());
}

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
    {"max_texture_size_get", (PyCFunction)pygpu_max_texture_size_get, METH_NOARGS, NULL},
    {"max_texture_layers_get", (PyCFunction)pygpu_max_texture_layers_get, METH_NOARGS, NULL},
    {"max_textures_get", (PyCFunction)pygpu_max_textures_get, METH_NOARGS, NULL},
    {"max_textures_vert_get", (PyCFunction)pygpu_max_textures_vert_get, METH_NOARGS, NULL},
    {"max_textures_geom_get", (PyCFunction)pygpu_max_textures_geom_get, METH_NOARGS, NULL},
    {"max_textures_frag_get", (PyCFunction)pygpu_max_textures_frag_get, METH_NOARGS, NULL},
    {"max_uniforms_vert_get", (PyCFunction)pygpu_max_uniforms_vert_get, METH_NOARGS, NULL},
    {"max_uniforms_frag_get", (PyCFunction)pygpu_max_uniforms_frag_get, METH_NOARGS, NULL},
    {"max_batch_indices_get", (PyCFunction)pygpu_max_batch_indices_get, METH_NOARGS, NULL},
    {"max_batch_vertices_get", (PyCFunction)pygpu_max_batch_vertices_get, METH_NOARGS, NULL},
    {"max_vertex_attribs_get", (PyCFunction)pygpu_max_vertex_attribs_get, METH_NOARGS, NULL},
    {"max_varying_floats_get", (PyCFunction)pygpu_max_varying_floats_get, METH_NOARGS, NULL},
    {"extensions_get", (PyCFunction)pygpu_extensions_get, METH_NOARGS, NULL},
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
