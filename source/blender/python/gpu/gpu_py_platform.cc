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

#include "GPU_context.hh"
#include "GPU_platform.hh"

#include "gpu_py.hh"
#include "gpu_py_platform.hh" /* Own include. */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name GPU Device Type
 * \{ */

PyDoc_STRVAR(pygpu_device_index_doc,
             "Device index.\n"
             "\n"
             ":type: int\n");
static PyObject *pygpu_device_index_get(BPyGPUDevice *self, void * /*closure*/)
{
  return PyLong_FromLong(self->index);
}

PyDoc_STRVAR(pygpu_device_identifier_doc,
             "Device identifier.\n"
             "\n"
             ":type: str\n");
static PyObject *pygpu_device_identifier_get(BPyGPUDevice *self, void * /*closure*/)
{
  return PyUnicode_FromString(self->identifier);
}

PyDoc_STRVAR(pygpu_device_name_doc,
             "Device name.\n"
             "\n"
             ":type: str\n");
static PyObject *pygpu_device_name_get(BPyGPUDevice *self, void * /*closure*/)
{
  return PyUnicode_FromString(self->name);
}

/* Property descriptors */
static PyGetSetDef pygpu_device_getseters[] = {
    {"index",
     reinterpret_cast<getter>(pygpu_device_index_get),
     nullptr,
     pygpu_device_index_doc,
     nullptr},
    {"identifier",
     reinterpret_cast<getter>(pygpu_device_identifier_get),
     nullptr,
     pygpu_device_identifier_doc,
     nullptr},
    {"name",
     reinterpret_cast<getter>(pygpu_device_name_get),
     nullptr,
     pygpu_device_name_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr},
};

/* Representation */
static PyObject *pygpu_device__tp_repr(BPyGPUDevice *self)
{
  return PyUnicode_FromFormat("<GPUDevice index=%d identifier=\"%s\" name=\"%s\">",
                              self->index,
                              self->identifier,
                              self->name);
}

/** Rich comparison for GPUDevice types, compares by index. */
static PyObject *pygpu_device__tp_richcmp(BPyGPUDevice *self, PyObject *other, int op)
{
  if (!Py_IS_TYPE(other, &BPyGPU_DeviceType)) {
    Py_RETURN_NOTIMPLEMENTED;
  }
  BPyGPUDevice *other_device = reinterpret_cast<BPyGPUDevice *>(other);
  switch (op) {
    case Py_LT:
      return PyBool_FromLong(self->index < other_device->index);
    case Py_LE:
      return PyBool_FromLong(self->index <= other_device->index);
    case Py_EQ:
      return PyBool_FromLong(self->index == other_device->index);
    case Py_NE:
      return PyBool_FromLong(self->index != other_device->index);
    case Py_GT:
      return PyBool_FromLong(self->index > other_device->index);
    case Py_GE:
      return PyBool_FromLong(self->index >= other_device->index);
  }
  Py_RETURN_NOTIMPLEMENTED;
}

/* Type definition */
PyDoc_STRVAR(pygpu_device__tp_doc,
             ".. class:: GPUDevice\n"
             "\n"
             "   Represents a GPU device.\n"
             "\n"
             "   :ivar index: Device index.\n"
             "   :type index: int\n"
             "   :ivar identifier: Device identifier.\n"
             "   :type identifier: str\n"
             "   :ivar name: Device name.\n"
             "   :type name: str\n");

PyTypeObject BPyGPU_DeviceType = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "gpu.platform.GPUDevice",
    /*tp_basicsize*/ sizeof(BPyGPUDevice),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_compare*/ nullptr,
    /*tp_repr*/ reinterpret_cast<reprfunc>(pygpu_device__tp_repr),
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ pygpu_device__tp_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ reinterpret_cast<richcmpfunc>(pygpu_device__tp_richcmp),
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ pygpu_device_getseters,
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

static BPyGPUDevice *pygpu_device_new(int index, const char *identifier, const char *name)
{
  BPyGPUDevice *self = reinterpret_cast<BPyGPUDevice *>(
      BPyGPU_DeviceType.tp_alloc(&BPyGPU_DeviceType, 0));
  if (self != nullptr) {
    self->index = index;
    self->identifier = identifier;
    self->name = name;
  }
  return self;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Functions
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_platform_vendor_get_doc,
    ".. function:: vendor_get()\n"
    "\n"
    "   Get GPU vendor.\n"
    "\n"
    "   :return: Vendor name.\n"
    "   :rtype: str\n");
static PyObject *pygpu_platform_vendor_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  return PyUnicode_FromString(GPU_platform_vendor());
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_platform_renderer_get_doc,
    ".. function:: renderer_get()\n"
    "\n"
    "   Get GPU to be used for rendering.\n"
    "\n"
    "   :return: GPU name.\n"
    "   :rtype: str\n");
static PyObject *pygpu_platform_renderer_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  return PyUnicode_FromString(GPU_platform_renderer());
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_platform_version_get_doc,
    ".. function:: version_get()\n"
    "\n"
    "   Get GPU driver version.\n"
    "\n"
    "   :return: Driver version.\n"
    "   :rtype: str\n");
static PyObject *pygpu_platform_version_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  return PyUnicode_FromString(GPU_platform_version());
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_platform_device_type_get_doc,
    ".. function:: device_type_get()\n"
    "\n"
    "   Get GPU device type.\n"
    "\n"
    "   :return: Device type ('APPLE', 'NVIDIA', 'AMD', 'INTEL', 'SOFTWARE', 'QUALCOMM', "
    "'UNKNOWN').\n"
    "   :rtype: str\n");
static PyObject *pygpu_platform_device_type_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  const char *device;
  if (GPU_type_matches(GPU_DEVICE_APPLE, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    device = "APPLE";
  }
  else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    device = "NVIDIA";
  }
  else if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    device = "AMD";
  }
  else if (GPU_type_matches(GPU_DEVICE_INTEL | GPU_DEVICE_INTEL_UHD, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    device = "INTEL";
  }
  else if (GPU_type_matches(GPU_DEVICE_SOFTWARE, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    device = "SOFTWARE";
  }
  /* Right now we can only detect Qualcomm GPUs on Windows, not other OSes */
  else if (GPU_type_matches(GPU_DEVICE_QUALCOMM, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    device = "QUALCOMM";
  }
  else {
    device = "UNKNOWN";
  }
  return PyUnicode_FromString(device);
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_platform_backend_type_get_doc,
    ".. function:: backend_type_get()\n"
    "\n"
    "   Get active GPU backend.\n"
    "\n"
    "   :return: Backend type ('OPENGL', 'VULKAN', 'METAL', 'NONE', 'UNKNOWN').\n"
    "   :rtype: str\n");
static PyObject *pygpu_platform_backend_type_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  const char *backend = "UNKNOWN";
  switch (GPU_backend_get_type()) {
    case GPU_BACKEND_VULKAN: {
      backend = "VULKAN";
      break;
    }
    case GPU_BACKEND_METAL: {
      backend = "METAL";
      break;
    }
    case GPU_BACKEND_NONE: {
      backend = "NONE";
      break;
    }
    case GPU_BACKEND_OPENGL: {
      backend = "OPENGL";
      break;
    }
    case GPU_BACKEND_ANY:
      break;
  }
  return PyUnicode_FromString(backend);
}

PyDoc_STRVAR(
    /* Wrap. */
    pygpu_platform_devices_get_doc,
    ".. function:: devices_get()\n"
    "\n"
    "   Get all available GPU devices.\n"
    "\n"
    "   :return: List of :class:`GPUDevice` objects for each device.\n"
    "   :rtype: list\n");
static PyObject *pygpu_platform_devices_get(PyObject * /*self*/)
{
  BPYGPU_IS_INIT_OR_ERROR_OBJ;

  Span<GPUDevice> devices = GPU_platform_devices_list();
  PyObject *list = PyList_New(devices.size());
  for (int i = 0; i < devices.size(); i++) {
    const GPUDevice &dev = devices[i];
    PyObject *item = reinterpret_cast<PyObject *>(
        pygpu_device_new(dev.index, dev.identifier.c_str(), dev.name.c_str()));
    PyList_SET_ITEM(list, i, item);
  }
  /* Sort by index (first attribute) for deterministic ordering. */
  PyList_Sort(list);
  return list;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef pygpu_platform__tp_methods[] = {
    {"vendor_get",
     reinterpret_cast<PyCFunction>(pygpu_platform_vendor_get),
     METH_NOARGS,
     pygpu_platform_vendor_get_doc},
    {"renderer_get",
     reinterpret_cast<PyCFunction>(pygpu_platform_renderer_get),
     METH_NOARGS,
     pygpu_platform_renderer_get_doc},
    {"version_get",
     reinterpret_cast<PyCFunction>(pygpu_platform_version_get),
     METH_NOARGS,
     pygpu_platform_version_get_doc},
    {"device_type_get",
     reinterpret_cast<PyCFunction>(pygpu_platform_device_type_get),
     METH_NOARGS,
     pygpu_platform_device_type_get_doc},
    {"backend_type_get",
     reinterpret_cast<PyCFunction>(pygpu_platform_backend_type_get),
     METH_NOARGS,
     pygpu_platform_backend_type_get_doc},
    {"devices_get",
     reinterpret_cast<PyCFunction>(pygpu_platform_devices_get),
     METH_NOARGS,
     pygpu_platform_devices_get_doc},
    {nullptr, nullptr, 0, nullptr},
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
    pygpu_platform__tp_doc,
    "This module provides access to GPU Platform definitions.");
static PyModuleDef pygpu_platform_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "gpu.platform",
    /*m_doc*/ pygpu_platform__tp_doc,
    /*m_size*/ 0,
    /*m_methods*/ pygpu_platform__tp_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *bpygpu_platform_init()
{
  PyObject *submodule;

  submodule = PyModule_Create(&pygpu_platform_module_def);

  return submodule;
}

/** \} */

}  // namespace blender
