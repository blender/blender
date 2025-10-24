/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines the API to support temporarily creating #Main data.
 * The only use case for this is currently to support temporarily loading data-blocks
 * which can be freed, without them polluting the current #G_MAIN.
 *
 * This is exposed via a context manager `bpy.types.BlendData.temp_data(...)`
 * which returns a new `bpy.types.BlendData` that is freed once the context manager exits.
 */

#include <Python.h>
#include <cstddef>

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "BLI_string.h"

#include "BKE_global.hh"
#include "BKE_main.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "bpy_rna.hh"
#include "bpy_rna_data.hh"

struct BPy_DataContext {
  PyObject_HEAD /* Required Python macro. */
  BPy_StructRNA *data_rna;
  char filepath[1024];
};

static PyObject *bpy_rna_data_temp_data(PyObject *self, PyObject *args, PyObject *kw);
static PyObject *bpy_rna_data_context_enter(BPy_DataContext *self);
static PyObject *bpy_rna_data_context_exit(BPy_DataContext *self, PyObject *args);

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef bpy_rna_data_context_methods[] = {
    {"__enter__", (PyCFunction)bpy_rna_data_context_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)bpy_rna_data_context_exit, METH_VARARGS},
    {nullptr} /* sentinel */
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static int bpy_rna_data_context_traverse(BPy_DataContext *self, visitproc visit, void *arg)
{
  Py_VISIT(self->data_rna);
  return 0;
}

static int bpy_rna_data_context_clear(BPy_DataContext *self)
{
  Py_CLEAR(self->data_rna);
  return 0;
}

static void bpy_rna_data_context_dealloc(BPy_DataContext *self)
{
  PyObject_GC_UnTrack(self);
  Py_CLEAR(self->data_rna);
  PyObject_GC_Del(self);
}
static PyTypeObject bpy_rna_data_context_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_rna_data_context",
    /*tp_basicsize*/ sizeof(BPy_DataContext),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)bpy_rna_data_context_dealloc,
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
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ (traverseproc)bpy_rna_data_context_traverse,
    /*tp_clear*/ (inquiry)bpy_rna_data_context_clear,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ bpy_rna_data_context_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
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

PyDoc_STRVAR(
    /* Wrap. */
    bpy_rna_data_context_load_doc,
    ".. method:: temp_data(*, filepath=None)\n"
    "\n"
    "   A context manager that temporarily creates blender file data.\n"
    "\n"
    "   :arg filepath: The file path for the newly temporary data. "
    "When None, the path of the currently open file is used.\n"
    "   :type filepath: str | bytes | None\n"
    "\n"
    "   :return: Blend file data which is freed once the context exists.\n"
    "   :rtype: :class:`bpy.types.BlendData`\n");
static PyObject *bpy_rna_data_temp_data(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};
  BPy_DataContext *ret;
  static const char *_keywords[] = {"filepath", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "|$" /* Optional keyword only arguments. */
      "O&" /* `filepath` */
      ":temp_data",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, PyC_ParseUnicodeAsBytesAndSize_OrNone, &filepath_data))
  {
    return nullptr;
  }

  ret = PyObject_GC_New(BPy_DataContext, &bpy_rna_data_context_Type);

  STRNCPY(ret->filepath, filepath_data.value ? filepath_data.value : G_MAIN->filepath);
  Py_XDECREF(filepath_data.value_coerce);

  return (PyObject *)ret;
}

static PyObject *bpy_rna_data_context_enter(BPy_DataContext *self)
{
  Main *bmain_temp = BKE_main_new();
  STRNCPY(bmain_temp->filepath, self->filepath);

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, &RNA_BlendData, bmain_temp);

  self->data_rna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);

  BLI_assert(!PyObject_GC_IsTracked((PyObject *)self));
  PyObject_GC_Track(self);

  return (PyObject *)self->data_rna;
}

static PyObject *bpy_rna_data_context_exit(BPy_DataContext *self, PyObject * /*args*/)
{
  BKE_main_free(static_cast<Main *>(self->data_rna->ptr->data));
  self->data_rna->ptr->invalidate();
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

PyMethodDef BPY_rna_data_context_method_def = {
    "temp_data",
    (PyCFunction)bpy_rna_data_temp_data,
    METH_STATIC | METH_VARARGS | METH_KEYWORDS,
    bpy_rna_data_context_load_doc,
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

int BPY_rna_data_context_type_ready()
{
  if (PyType_Ready(&bpy_rna_data_context_Type) < 0) {
    return -1;
  }

  return 0;
}
