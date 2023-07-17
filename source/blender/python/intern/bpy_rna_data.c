/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
#include <stddef.h>

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "bpy_rna.h"
#include "bpy_rna_data.h"

typedef struct {
  PyObject_HEAD /* Required Python macro. */
  BPy_StructRNA *data_rna;
  char filepath[1024];
} BPy_DataContext;

static PyObject *bpy_rna_data_temp_data(PyObject *self, PyObject *args, PyObject *kwds);
static PyObject *bpy_rna_data_context_enter(BPy_DataContext *self);
static PyObject *bpy_rna_data_context_exit(BPy_DataContext *self, PyObject *args);

static PyMethodDef bpy_rna_data_context_methods[] = {
    {"__enter__", (PyCFunction)bpy_rna_data_context_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)bpy_rna_data_context_exit, METH_VARARGS},
    {NULL} /* sentinel */
};

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
    /*ob_base*/ PyVarObject_HEAD_INIT(NULL, 0)
    /*tp_name*/ "bpy_rna_data_context",
    /*tp_basicsize*/ sizeof(BPy_DataContext),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)bpy_rna_data_context_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ NULL,
    /*tp_setattr*/ NULL,
    /*tp_as_async*/ NULL,
    /*tp_repr*/ NULL,
    /*tp_as_number*/ NULL,
    /*tp_as_sequence*/ NULL,
    /*tp_as_mapping*/ NULL,
    /*tp_hash*/ NULL,
    /*tp_call*/ NULL,
    /*tp_str*/ NULL,
    /*tp_getattro*/ NULL,
    /*tp_setattro*/ NULL,
    /*tp_as_buffer*/ NULL,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    /*tp_doc*/ NULL,
    /*tp_traverse*/ (traverseproc)bpy_rna_data_context_traverse,
    /*tp_clear*/ (inquiry)bpy_rna_data_context_clear,
    /*tp_richcompare*/ NULL,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ NULL,
    /*tp_iternext*/ NULL,
    /*tp_methods*/ bpy_rna_data_context_methods,
    /*tp_members*/ NULL,
    /*tp_getset*/ NULL,
    /*tp_base*/ NULL,
    /*tp_dict*/ NULL,
    /*tp_descr_get*/ NULL,
    /*tp_descr_set*/ NULL,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ NULL,
    /*tp_alloc*/ NULL,
    /*tp_new*/ NULL,
    /*tp_free*/ NULL,
    /*tp_is_gc*/ NULL,
    /*tp_bases*/ NULL,
    /*tp_mro*/ NULL,
    /*tp_cache*/ NULL,
    /*tp_subclasses*/ NULL,
    /*tp_weaklist*/ NULL,
    /*tp_del*/ NULL,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ NULL,
    /*tp_vectorcall*/ NULL,
};

PyDoc_STRVAR(bpy_rna_data_context_load_doc,
             ".. method:: temp_data(filepath=None)\n"
             "\n"
             "   A context manager that temporarily creates blender file data.\n"
             "\n"
             "   :arg filepath: The file path for the newly temporary data. "
             "When None, the path of the currently open file is used.\n"
             "   :type filepath: str or NoneType\n"
             "\n"
             "   :return: Blend file data which is freed once the context exists.\n"
             "   :rtype: :class:`bpy.types.BlendData`\n");

static PyObject *bpy_rna_data_temp_data(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  BPy_DataContext *ret;
  const char *filepath = NULL;
  static const char *_keywords[] = {"filepath", NULL};
  static _PyArg_Parser _parser = {
      "|$" /* Optional keyword only arguments. */
      "z"  /* `filepath` */
      ":temp_data",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &filepath)) {
    return NULL;
  }

  ret = PyObject_GC_New(BPy_DataContext, &bpy_rna_data_context_Type);

  STRNCPY(ret->filepath, filepath ? filepath : G_MAIN->filepath);

  return (PyObject *)ret;
}

static PyObject *bpy_rna_data_context_enter(BPy_DataContext *self)
{
  Main *bmain_temp = BKE_main_new();
  PointerRNA ptr;
  RNA_pointer_create(NULL, &RNA_BlendData, bmain_temp, &ptr);

  self->data_rna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);

  BLI_assert(!PyObject_GC_IsTracked((PyObject *)self));
  PyObject_GC_Track(self);

  return (PyObject *)self->data_rna;
}

static PyObject *bpy_rna_data_context_exit(BPy_DataContext *self, PyObject *UNUSED(args))
{
  BKE_main_free(self->data_rna->ptr.data);
  RNA_POINTER_INVALIDATE(&self->data_rna->ptr);
  Py_RETURN_NONE;
}

PyMethodDef BPY_rna_data_context_method_def = {
    "temp_data",
    (PyCFunction)bpy_rna_data_temp_data,
    METH_STATIC | METH_VARARGS | METH_KEYWORDS,
    bpy_rna_data_context_load_doc,
};

int BPY_rna_data_context_type_ready(void)
{
  if (PyType_Ready(&bpy_rna_data_context_Type) < 0) {
    return -1;
  }

  return 0;
}
