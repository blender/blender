/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bpygpu
 *
 * - Use `bpygpu_` for local API.
 * - Use `BPyGPU` for public API.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_state.h"

#include "../generic/python_compat.h"

#include "gpu_py.h"
#include "gpu_py_compute.h" /* own include */
#include "gpu_py_shader.h"

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_compute_dispatch_doc,
    ".. function:: dispatch(shader, groups_x_len,  groups_y_len,  groups_z_len)\n"
    "\n"
    "   Dispatches GPU compute.\n"
    "\n"
    "   :arg shader: The shader that you want to dispatch.\n"
    "   :type shader: :class:`gpu.types.GPUShader`\n"
    "   :arg groups_x_len: Int for group x length:\n"
    "   :type groups_x_len: int\n"
    "   :arg groups_y_len: Int for group y length:\n"
    "   :type groups_y_len: int\n"
    "   :arg groups_z_len: Int for group z length:\n"
    "   :type groups_z_len: int\n"
    "   :return: Shader object.\n"
    "   :rtype: :class:`bpy.types.GPUShader`\n");
static PyObject *pygpu_compute_dispatch(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
  BPyGPUShader *py_shader;
  int groups_x_len;
  int groups_y_len;
  int groups_z_len;

  static const char *_keywords[] = {
      "shader", "groups_x_len", "groups_y_len", "groups_z_len", nullptr};
  static _PyArg_Parser _parser = {
        PY_ARG_PARSER_HEAD_COMPAT()
        "O" /* `shader` */
        "i" /* `groups_x_len` */
        "i" /* `groups_y_len` */
        "i" /* `groups_z_len` */
        ":dispatch",
        _keywords,
        nullptr,
    };
  if (_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &py_shader, &groups_x_len, &groups_y_len, &groups_z_len))
  {

    if (!BPyGPUShader_Check(py_shader)) {
      PyErr_Format(PyExc_TypeError, "Expected a GPUShader, got %s", Py_TYPE(py_shader)->tp_name);
      return nullptr;
    }

    // Check that groups do not exceed GPU_max_work_group_count()
    const int max_work_group_count_x = GPU_max_work_group_count(0);
    const int max_work_group_count_y = GPU_max_work_group_count(1);
    const int max_work_group_count_z = GPU_max_work_group_count(2);

    // Report back to the user both the requested and the maximum supported value
    if (groups_x_len > GPU_max_work_group_count(0)) {
      PyErr_Format(PyExc_ValueError,
                   "groups_x_len (%d) exceeds maximum supported value (max work group count: %d)",
                   groups_x_len,
                   max_work_group_count_x);
      return nullptr;
    }
    if (groups_y_len > GPU_max_work_group_count(1)) {
      PyErr_Format(PyExc_ValueError,
                   "groups_y_len (%d) exceeds maximum supported value (max work group count: %d)",
                   groups_y_len,
                   max_work_group_count_y);
      return nullptr;
    }
    if (groups_z_len > GPU_max_work_group_count(2)) {
      PyErr_Format(PyExc_ValueError,
                   "groups_z_len (%d) exceeds maximum supported value (max work group count: %d)",
                   groups_z_len,
                   max_work_group_count_z);
      return nullptr;
    }

    GPUShader *shader = py_shader->shader;
    GPU_compute_dispatch(shader, groups_x_len, groups_y_len, groups_z_len);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  Py_RETURN_NONE;
}

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

static PyMethodDef pygpu_compute__tp_methods[] = {
    {"dispatch",
     (PyCFunction)pygpu_compute_dispatch,
     METH_VARARGS | METH_KEYWORDS,
     pygpu_compute_dispatch_doc},
    {nullptr, nullptr, 0, nullptr},
};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_compute__tp_doc,
    "This module provides access to the global GPU compute functions");
static PyModuleDef pygpu_compute_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "gpu.compute",
    /*m_doc*/ pygpu_compute__tp_doc,
    /*m_size*/ 0,
    /*m_methods*/ pygpu_compute__tp_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *bpygpu_compute_init()
{
  PyObject *submodule;

  submodule = bpygpu_create_module(&pygpu_compute_module_def);

  return submodule;
}

/** \} */
