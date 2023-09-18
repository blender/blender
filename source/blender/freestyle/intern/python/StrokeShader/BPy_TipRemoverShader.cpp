/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_TipRemoverShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char TipRemoverShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`TipRemoverShader`\n"
    "\n"
    "[Geometry shader]\n"
    "\n"
    ".. method:: __init__(tip_length)\n"
    "\n"
    "   Builds a TipRemoverShader object.\n"
    "\n"
    "   :arg tip_length: The length of the piece of stroke we want to remove\n"
    "      at each extremity.\n"
    "   :type tip_length: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Removes the stroke's extremities.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int TipRemoverShader___init__(BPy_TipRemoverShader *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"tip_length", nullptr};
  double d;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "d", (char **)kwlist, &d)) {
    return -1;
  }
  self->py_ss.ss = new StrokeShaders::TipRemoverShader(d);
  return 0;
}

/*-----------------------BPy_TipRemoverShader type definition ------------------------------*/

PyTypeObject TipRemoverShader_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "TipRemoverShader",
    /*tp_basicsize*/ sizeof(BPy_TipRemoverShader),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
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
    /*tp_doc*/ TipRemoverShader___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &StrokeShader_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)TipRemoverShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
