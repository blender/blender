/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_SmoothingShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char SmoothingShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`SmoothingShader`\n"
    "\n"
    "[Geometry shader]\n"
    "\n"
    ".. method:: __init__(num_iterations=100, factor_point=0.1, \\\n"
    "      factor_curvature=0.0, factor_curvature_difference=0.2, \\\n"
    "      aniso_point=0.0, aniso_normal=0.0, aniso_curvature=0.0, \\\n"
    "      carricature_factor=1.0)\n"
    "\n"
    "   Builds a SmoothingShader object.\n"
    "\n"
    "   :arg num_iterations: The number of iterations.\n"
    "   :type num_iterations: int\n"
    "   :arg factor_point: 0.1\n"
    "   :type factor_point: float\n"
    "   :arg factor_curvature: 0.0\n"
    "   :type factor_curvature: float\n"
    "   :arg factor_curvature_difference: 0.2\n"
    "   :type factor_curvature_difference: float\n"
    "   :arg aniso_point: 0.0\n"
    "   :type aniso_point: float\n"
    "   :arg aniso_normal: 0.0\n"
    "   :type aniso_normal: float\n"
    "   :arg aniso_curvature: 0.0\n"
    "   :type aniso_curvature: float\n"
    "   :arg carricature_factor: 1.0\n"
    "   :type carricature_factor: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Smooths the stroke by moving the vertices to make the stroke\n"
    "   smoother.  Uses curvature flow to converge towards a curve of\n"
    "   constant curvature.  The diffusion method we use is anisotropic to\n"
    "   prevent the diffusion across corners.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int SmoothingShader___init__(BPy_SmoothingShader *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {
      "num_iterations",
      "factor_point",
      "factor_curvature",
      "factor_curvature_difference",
      "aniso_point",
      "aniso_normal",
      "aniso_curvature",
      "carricature_factor",
      nullptr,
  };
  int i1 = 100;
  double d2 = 0.1, d3 = 0.0, d4 = 0.2, d5 = 0.0, d6 = 0.0, d7 = 0.0, d8 = 1.0;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|iddddddd", (char **)kwlist, &i1, &d2, &d3, &d4, &d5, &d6, &d7, &d8))
  {
    return -1;
  }
  self->py_ss.ss = new SmoothingShader(i1, d2, d3, d4, d5, d6, d7, d8);
  return 0;
}

/*-----------------------BPy_SmoothingShader type definition ------------------------------*/

PyTypeObject SmoothingShader_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "SmoothingShader",
    /*tp_basicsize*/ sizeof(BPy_SmoothingShader),
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
    /*tp_doc*/ SmoothingShader___doc__,
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
    /*tp_init*/ (initproc)SmoothingShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
