/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ConstantColorShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ConstantColorShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`ConstantColorShader`\n"
    "\n"
    "[Color shader]\n"
    "\n"
    ".. method:: __init__(red, green, blue, alpha=1.0)\n"
    "\n"
    "   Builds a ConstantColorShader object.\n"
    "\n"
    "   :arg red: The red component.\n"
    "   :type red: float\n"
    "   :arg green: The green component.\n"
    "   :type green: float\n"
    "   :arg blue: The blue component.\n"
    "   :type blue: float\n"
    "   :arg alpha: The alpha value.\n"
    "   :type alpha: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Assigns a constant color to every vertex of the Stroke.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int ConstantColorShader___init__(BPy_ConstantColorShader *self,
                                        PyObject *args,
                                        PyObject *kwds)
{
  static const char *kwlist[] = {"red", "green", "blue", "alpha", nullptr};
  float f1, f2, f3, f4 = 1.0;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "fff|f", (char **)kwlist, &f1, &f2, &f3, &f4)) {
    return -1;
  }
  self->py_ss.ss = new StrokeShaders::ConstantColorShader(f1, f2, f3, f4);
  return 0;
}

/*-----------------------BPy_ConstantColorShader type definition ------------------------------*/

PyTypeObject ConstantColorShader_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ConstantColorShader",
    /*tp_basicsize*/ sizeof(BPy_ConstantColorShader),
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
    /*tp_doc*/ ConstantColorShader___doc__,
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
    /*tp_init*/ (initproc)ConstantColorShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
