/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ColorNoiseShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ColorNoiseShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`ColorNoiseShader`\n"
    "\n"
    "[Color shader]\n"
    "\n"
    ".. method:: __init__(amplitude, period)\n"
    "\n"
    "   Builds a ColorNoiseShader object.\n"
    "\n"
    "   :arg amplitude: The amplitude of the noise signal.\n"
    "   :type amplitude: float\n"
    "   :arg period: The period of the noise signal.\n"
    "   :type period: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Shader to add noise to the stroke colors.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int ColorNoiseShader___init__(BPy_ColorNoiseShader *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"amplitude", "period", nullptr};
  float f1, f2;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "ff", (char **)kwlist, &f1, &f2)) {
    return -1;
  }
  self->py_ss.ss = new StrokeShaders::ColorNoiseShader(f1, f2);
  return 0;
}

/*-----------------------BPy_ColorNoiseShader type definition ------------------------------*/

PyTypeObject ColorNoiseShader_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ColorNoiseShader",
    /*tp_basicsize*/ sizeof(BPy_ColorNoiseShader),
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
    /*tp_doc*/ ColorNoiseShader___doc__,
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
    /*tp_init*/ (initproc)ColorNoiseShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
