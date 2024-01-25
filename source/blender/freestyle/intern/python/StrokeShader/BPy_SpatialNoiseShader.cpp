/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_SpatialNoiseShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    SpatialNoiseShader___doc__,
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`SpatialNoiseShader`\n"
    "\n"
    "[Geometry shader]\n"
    "\n"
    ".. method:: __init__(amount, scale, num_octaves, smooth, pure_random)\n"
    "\n"
    "   Builds a SpatialNoiseShader object.\n"
    "\n"
    "   :arg amount: The amplitude of the noise.\n"
    "   :type amount: float\n"
    "   :arg scale: The noise frequency.\n"
    "   :type scale: float\n"
    "   :arg num_octaves: The number of octaves\n"
    "   :type num_octaves: int\n"
    "   :arg smooth: True if you want the noise to be smooth.\n"
    "   :type smooth: bool\n"
    "   :arg pure_random: True if you don't want any coherence.\n"
    "   :type pure_random: bool\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Spatial Noise stroke shader. Moves the vertices to make the stroke\n"
    "   more noisy.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n");

static int SpatialNoiseShader___init__(BPy_SpatialNoiseShader *self,
                                       PyObject *args,
                                       PyObject *kwds)
{
  static const char *kwlist[] = {
      "amount", "scale", "num_octaves", "smooth", "pure_random", nullptr};
  float f1, f2;
  int i3;
  PyObject *obj4 = nullptr, *obj5 = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwds,
                                   "ffiO!O!",
                                   (char **)kwlist,
                                   &f1,
                                   &f2,
                                   &i3,
                                   &PyBool_Type,
                                   &obj4,
                                   &PyBool_Type,
                                   &obj5))
  {
    return -1;
  }
  self->py_ss.ss = new SpatialNoiseShader(
      f1, f2, i3, bool_from_PyBool(obj4), bool_from_PyBool(obj5));
  return 0;
}

/*-----------------------BPy_SpatialNoiseShader type definition ------------------------------*/

PyTypeObject SpatialNoiseShader_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "SpatialNoiseShader",
    /*tp_basicsize*/ sizeof(BPy_SpatialNoiseShader),
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
    /*tp_doc*/ SpatialNoiseShader___doc__,
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
    /*tp_init*/ (initproc)SpatialNoiseShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
