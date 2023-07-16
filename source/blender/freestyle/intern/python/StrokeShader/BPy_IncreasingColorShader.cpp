/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_IncreasingColorShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char IncreasingColorShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`IncreasingColorShader`\n"
    "\n"
    "[Color shader]\n"
    "\n"
    ".. method:: __init__(red_min, green_min, blue_min, alpha_min, red_max, green_max, blue_max, "
    "alpha_max)\n"
    "\n"
    "   Builds an IncreasingColorShader object.\n"
    "\n"
    "   :arg red_min: The first color red component.\n"
    "   :type red_min: float\n"
    "   :arg green_min: The first color green component.\n"
    "   :type green_min: float\n"
    "   :arg blue_min: The first color blue component.\n"
    "   :type blue_min: float\n"
    "   :arg alpha_min: The first color alpha value.\n"
    "   :type alpha_min: float\n"
    "   :arg red_max: The second color red component.\n"
    "   :type red_max: float\n"
    "   :arg green_max: The second color green component.\n"
    "   :type green_max: float\n"
    "   :arg blue_max: The second color blue component.\n"
    "   :type blue_max: float\n"
    "   :arg alpha_max: The second color alpha value.\n"
    "   :type alpha_max: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Assigns a varying color to the stroke. The user specifies two\n"
    "   colors A and B. The stroke color will change linearly from A to B\n"
    "   between the first and the last vertex.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int IncreasingColorShader___init__(BPy_IncreasingColorShader *self,
                                          PyObject *args,
                                          PyObject *kwds)
{
  static const char *kwlist[] = {
      "red_min",
      "green_min",
      "blue_min",
      "alpha_min",
      "red_max",
      "green_max",
      "blue_max",
      "alpha_max",
      nullptr,
  };
  float f1, f2, f3, f4, f5, f6, f7, f8;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "ffffffff", (char **)kwlist, &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8))
  {
    return -1;
  }
  self->py_ss.ss = new StrokeShaders::IncreasingColorShader(f1, f2, f3, f4, f5, f6, f7, f8);
  return 0;
}

/*-----------------------BPy_IncreasingColorShader type definition ------------------------------*/

PyTypeObject IncreasingColorShader_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "IncreasingColorShader",
    /*tp_basicsize*/ sizeof(BPy_IncreasingColorShader),
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
    /*tp_doc*/ IncreasingColorShader___doc__,
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
    /*tp_init*/ (initproc)IncreasingColorShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
