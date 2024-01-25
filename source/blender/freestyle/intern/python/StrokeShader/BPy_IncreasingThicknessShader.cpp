/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_IncreasingThicknessShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    IncreasingThicknessShader___doc__,
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`IncreasingThicknessShader`\n"
    "\n"
    "[Thickness shader]\n"
    "\n"
    ".. method:: __init__(thickness_A, thickness_B)\n"
    "\n"
    "   Builds an IncreasingThicknessShader object.\n"
    "\n"
    "   :arg thickness_A: The first thickness value.\n"
    "   :type thickness_A: float\n"
    "   :arg thickness_B: The second thickness value.\n"
    "   :type thickness_B: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Assigns thicknesses values such as the thickness increases from a\n"
    "   thickness value A to a thickness value B between the first vertex\n"
    "   to the midpoint vertex and then decreases from B to a A between\n"
    "   this midpoint vertex and the last vertex. The thickness is\n"
    "   linearly interpolated from A to B.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n");

static int IncreasingThicknessShader___init__(BPy_IncreasingThicknessShader *self,
                                              PyObject *args,
                                              PyObject *kwds)
{
  static const char *kwlist[] = {"thickness_A", "thickness_B", nullptr};
  float f1, f2;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "ff", (char **)kwlist, &f1, &f2)) {
    return -1;
  }
  self->py_ss.ss = new StrokeShaders::IncreasingThicknessShader(f1, f2);
  return 0;
}

/*-----------------------BPy_IncreasingThicknessShader type definition --------------------------*/

PyTypeObject IncreasingThicknessShader_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "IncreasingThicknessShader",
    /*tp_basicsize*/ sizeof(BPy_IncreasingThicknessShader),
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
    /*tp_doc*/ IncreasingThicknessShader___doc__,
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
    /*tp_init*/ (initproc)IncreasingThicknessShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
