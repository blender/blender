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

#include "bpy_rna.h"
#include "bpy_rna_data.h"

typedef struct {
  PyObject_HEAD /* required python macro */
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
    PyVarObject_HEAD_INIT(NULL, 0) "bpy_rna_data_context", /* tp_name */
    sizeof(BPy_DataContext),                               /* tp_basicsize */
    0,                                                     /* tp_itemsize */
    /* methods */
    (destructor)bpy_rna_data_context_dealloc, /* tp_dealloc */
    0,                                        /* tp_vectorcall_offset */
    NULL,                                     /* getattrfunc tp_getattr; */
    NULL,                                     /* setattrfunc tp_setattr; */
    NULL,
    /* tp_compare */ /* DEPRECATED in python 3.0! */
    NULL,            /* tp_repr */

    /* Method suites for standard classes */

    NULL, /* PyNumberMethods *tp_as_number; */
    NULL, /* PySequenceMethods *tp_as_sequence; */
    NULL, /* PyMappingMethods *tp_as_mapping; */

    /* More standard operations (here for binary compatibility) */

    NULL, /* hashfunc tp_hash; */
    NULL, /* ternaryfunc tp_call; */
    NULL, /* reprfunc tp_str; */

    /* will only use these if this is a subtype of a py class */
    NULL, /* getattrofunc tp_getattro; */
    NULL, /* setattrofunc tp_setattro; */

    /* Functions to access object as input/output buffer */
    NULL, /* PyBufferProcs *tp_as_buffer; */

    /*** Flags to define presence of optional/expanded features ***/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /* long tp_flags; */

    NULL, /*  char *tp_doc;  Documentation string */
    /*** Assigned meaning in release 2.0 ***/
    /* call function for all accessible objects */
    (traverseproc)bpy_rna_data_context_traverse, /* traverseproc tp_traverse; */

    /* delete references to contained objects */
    (inquiry)bpy_rna_data_context_clear, /* inquiry tp_clear; */

    /***  Assigned meaning in release 2.1 ***/
    /*** rich comparisons (subclassed) ***/
    NULL, /* richcmpfunc tp_richcompare; */

    /***  weak reference enabler ***/
    0,
    /*** Added in release 2.2 ***/
    /*   Iterators */
    NULL, /* getiterfunc tp_iter; */
    NULL, /* iternextfunc tp_iternext; */

    /*** Attribute descriptor and subclassing stuff ***/
    bpy_rna_data_context_methods, /* struct PyMethodDef *tp_methods; */
    NULL,                         /* struct PyMemberDef *tp_members; */
    NULL,                         /* struct PyGetSetDef *tp_getset; */
    NULL,                         /* struct _typeobject *tp_base; */
    NULL,                         /* PyObject *tp_dict; */
    NULL,                         /* descrgetfunc tp_descr_get; */
    NULL,                         /* descrsetfunc tp_descr_set; */
    0,                            /* long tp_dictoffset; */
    NULL,                         /* initproc tp_init; */
    NULL,                         /* allocfunc tp_alloc; */
    NULL,                         /* newfunc tp_new; */
    /*  Low-level free-memory routine */
    NULL, /* freefunc tp_free;  */
    /* For PyObject_IS_GC */
    NULL, /* inquiry tp_is_gc;  */
    NULL, /* PyObject *tp_bases; */
    /* method resolution order */
    NULL, /* PyObject *tp_mro;  */
    NULL, /* PyObject *tp_cache; */
    NULL, /* PyObject *tp_subclasses; */
    NULL, /* PyObject *tp_weaklist; */
    NULL,
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
  static _PyArg_Parser _parser = {"|$z:temp_data", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &filepath)) {
    return NULL;
  }

  ret = PyObject_GC_New(BPy_DataContext, &bpy_rna_data_context_Type);

  STRNCPY(ret->filepath, filepath ? filepath : G_MAIN->name);

  return (PyObject *)ret;
}

static PyObject *bpy_rna_data_context_enter(BPy_DataContext *self)
{
  Main *bmain_temp = BKE_main_new();
  PointerRNA ptr;
  RNA_pointer_create(NULL, &RNA_BlendData, bmain_temp, &ptr);

  self->data_rna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);

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
