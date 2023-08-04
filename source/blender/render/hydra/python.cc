/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include "final_engine.h"
#include "preview_engine.h"
#include "viewport_engine.h"

#include <Python.h>

#include "RE_engine.h"

#include "bpy_rna.h"

#include "BKE_context.h"

#include "RE_engine.h"

#include "RNA_prototypes.h"

#include "hydra/image.h"

namespace blender::render::hydra {

template<typename T> T *pyrna_to_pointer(PyObject *pyobject, const StructRNA *rnatype)
{
  const PointerRNA *ptr = pyrna_struct_as_ptr_or_null(pyobject, rnatype);
  return (ptr) ? static_cast<T *>(ptr->data) : nullptr;
}

static PyObject *engine_create_func(PyObject * /*self*/, PyObject *args)
{
  PyObject *pyengine;
  char *engine_type, *render_delegate_id;
  if (!PyArg_ParseTuple(args, "Oss", &pyengine, &engine_type, &render_delegate_id)) {
    Py_RETURN_NONE;
  }

  RenderEngine *bl_engine = pyrna_to_pointer<RenderEngine>(pyengine, &RNA_RenderEngine);

  CLOG_INFO(LOG_HYDRA_RENDER, 1, "Engine %s", engine_type);
  Engine *engine = nullptr;
  try {
    if (STREQ(engine_type, "VIEWPORT")) {
      engine = new ViewportEngine(bl_engine, render_delegate_id);
    }
    else if (STREQ(engine_type, "PREVIEW")) {
      engine = new PreviewEngine(bl_engine, render_delegate_id);
    }
    else {
      engine = new FinalEngine(bl_engine, render_delegate_id);
    }
  }
  catch (std::runtime_error &e) {
    CLOG_ERROR(LOG_HYDRA_RENDER, "%s", e.what());
  }

  CLOG_INFO(LOG_HYDRA_RENDER, 1, "Engine %p", engine);
  return PyLong_FromVoidPtr(engine);
}

static PyObject *engine_free_func(PyObject * /*self*/, PyObject *args)
{
  PyObject *pyengine;
  if (!PyArg_ParseTuple(args, "O", &pyengine)) {
    Py_RETURN_NONE;
  }

  Engine *engine = static_cast<Engine *>(PyLong_AsVoidPtr(pyengine));
  CLOG_INFO(LOG_HYDRA_RENDER, 1, "Engine %p", engine);
  delete engine;

  Py_RETURN_NONE;
}

static PyObject *engine_update_func(PyObject * /*self*/, PyObject *args)
{
  PyObject *pyengine, *pydepsgraph, *pycontext;
  if (!PyArg_ParseTuple(args, "OOO", &pyengine, &pydepsgraph, &pycontext)) {
    Py_RETURN_NONE;
  }

  Engine *engine = static_cast<Engine *>(PyLong_AsVoidPtr(pyengine));
  Depsgraph *depsgraph = pyrna_to_pointer<Depsgraph>(pydepsgraph, &RNA_Depsgraph);
  bContext *context = pyrna_to_pointer<bContext>(pycontext, &RNA_Context);

  CLOG_INFO(LOG_HYDRA_RENDER, 2, "Engine %p", engine);
  engine->sync(depsgraph, context);

  Py_RETURN_NONE;
}

static PyObject *engine_render_func(PyObject * /*self*/, PyObject *args)
{
  PyObject *pyengine;
  if (!PyArg_ParseTuple(args, "O", &pyengine)) {
    Py_RETURN_NONE;
  }

  Engine *engine = static_cast<Engine *>(PyLong_AsVoidPtr(pyengine));

  CLOG_INFO(LOG_HYDRA_RENDER, 2, "Engine %p", engine);

  /* Allow Blender to execute other Python scripts. */
  Py_BEGIN_ALLOW_THREADS;
  engine->render();
  Py_END_ALLOW_THREADS;

  Py_RETURN_NONE;
}

static PyObject *engine_view_draw_func(PyObject * /*self*/, PyObject *args)
{
  PyObject *pyengine, *pycontext;
  if (!PyArg_ParseTuple(args, "OO", &pyengine, &pycontext)) {
    Py_RETURN_NONE;
  }

  ViewportEngine *engine = static_cast<ViewportEngine *>(PyLong_AsVoidPtr(pyengine));
  bContext *context = pyrna_to_pointer<bContext>(pycontext, &RNA_Context);

  CLOG_INFO(LOG_HYDRA_RENDER, 3, "Engine %p", engine);

  /* Allow Blender to execute other Python scripts. */
  Py_BEGIN_ALLOW_THREADS;
  engine->render(context);
  Py_END_ALLOW_THREADS;

  Py_RETURN_NONE;
}

static pxr::VtValue get_setting_val(PyObject *pyval)
{
  pxr::VtValue val;
  if (PyBool_Check(pyval)) {
    val = Py_IsTrue(pyval);
  }
  else if (PyLong_Check(pyval)) {
    val = PyLong_AsLong(pyval);
  }
  else if (PyFloat_Check(pyval)) {
    val = PyFloat_AsDouble(pyval);
  }
  else if (PyUnicode_Check(pyval)) {
    val = std::string(PyUnicode_AsUTF8(pyval));
  }
  return val;
}

static PyObject *engine_set_render_setting_func(PyObject * /*self*/, PyObject *args)
{
  PyObject *pyengine, *pyval;
  char *key;
  if (!PyArg_ParseTuple(args, "OsO", &pyengine, &key, &pyval)) {
    Py_RETURN_NONE;
  }

  Engine *engine = static_cast<Engine *>(PyLong_AsVoidPtr(pyengine));

  CLOG_INFO(LOG_HYDRA_RENDER, 3, "Engine %p: %s", engine, key);
  engine->set_render_setting(key, get_setting_val(pyval));

  Py_RETURN_NONE;
}

static PyObject *cache_or_get_image_file_func(PyObject * /*self*/, PyObject *args)
{
  PyObject *pycontext, *pyimage;
  if (!PyArg_ParseTuple(args, "OO", &pycontext, &pyimage)) {
    Py_RETURN_NONE;
  }

  bContext *context = static_cast<bContext *>(PyLong_AsVoidPtr(pycontext));
  Image *image = static_cast<Image *>(PyLong_AsVoidPtr(pyimage));

  std::string image_path = io::hydra::cache_or_get_image_file(
      CTX_data_main(context), CTX_data_scene(context), image, nullptr);
  return PyUnicode_FromString(image_path.c_str());
}

static PyMethodDef methods[] = {
    {"engine_create", engine_create_func, METH_VARARGS, ""},
    {"engine_free", engine_free_func, METH_VARARGS, ""},
    {"engine_update", engine_update_func, METH_VARARGS, ""},
    {"engine_render", engine_render_func, METH_VARARGS, ""},
    {"engine_view_draw", engine_view_draw_func, METH_VARARGS, ""},
    {"engine_set_render_setting", engine_set_render_setting_func, METH_VARARGS, ""},

    {"cache_or_get_image_file", cache_or_get_image_file_func, METH_VARARGS, ""},

    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "_bpy_hydra",
    "Hydra render API",
    -1,
    methods,
    NULL,
    NULL,
    NULL,
    NULL,
};

}  // namespace blender::render::hydra

PyObject *BPyInit_hydra();

PyObject *BPyInit_hydra()
{
  PyObject *mod = PyModule_Create(&blender::render::hydra::module);
  return mod;
}
