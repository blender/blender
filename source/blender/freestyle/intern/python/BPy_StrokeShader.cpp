/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_StrokeShader.h"

#include "BPy_Convert.h"
#include "Interface1D/BPy_Stroke.h"

#include "StrokeShader/BPy_BackboneStretcherShader.h"
#include "StrokeShader/BPy_BezierCurveShader.h"
#include "StrokeShader/BPy_BlenderTextureShader.h"
#include "StrokeShader/BPy_CalligraphicShader.h"
#include "StrokeShader/BPy_ColorNoiseShader.h"
#include "StrokeShader/BPy_ConstantColorShader.h"
#include "StrokeShader/BPy_ConstantThicknessShader.h"
#include "StrokeShader/BPy_ConstrainedIncreasingThicknessShader.h"
#include "StrokeShader/BPy_GuidingLinesShader.h"
#include "StrokeShader/BPy_IncreasingColorShader.h"
#include "StrokeShader/BPy_IncreasingThicknessShader.h"
#include "StrokeShader/BPy_PolygonalizationShader.h"
#include "StrokeShader/BPy_SamplingShader.h"
#include "StrokeShader/BPy_SmoothingShader.h"
#include "StrokeShader/BPy_SpatialNoiseShader.h"
#include "StrokeShader/BPy_StrokeTextureStepShader.h"
#include "StrokeShader/BPy_ThicknessNoiseShader.h"
#include "StrokeShader/BPy_TipRemoverShader.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int StrokeShader_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&StrokeShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&StrokeShader_Type);
  PyModule_AddObject(module, "StrokeShader", (PyObject *)&StrokeShader_Type);

  if (PyType_Ready(&BackboneStretcherShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&BackboneStretcherShader_Type);
  PyModule_AddObject(module, "BackboneStretcherShader", (PyObject *)&BackboneStretcherShader_Type);

  if (PyType_Ready(&BezierCurveShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&BezierCurveShader_Type);
  PyModule_AddObject(module, "BezierCurveShader", (PyObject *)&BezierCurveShader_Type);

  if (PyType_Ready(&BlenderTextureShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&BlenderTextureShader_Type);
  PyModule_AddObject(module, "BlenderTextureShader", (PyObject *)&BlenderTextureShader_Type);

  if (PyType_Ready(&CalligraphicShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&CalligraphicShader_Type);
  PyModule_AddObject(module, "CalligraphicShader", (PyObject *)&CalligraphicShader_Type);

  if (PyType_Ready(&ColorNoiseShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ColorNoiseShader_Type);
  PyModule_AddObject(module, "ColorNoiseShader", (PyObject *)&ColorNoiseShader_Type);

  if (PyType_Ready(&ConstantColorShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ConstantColorShader_Type);
  PyModule_AddObject(module, "ConstantColorShader", (PyObject *)&ConstantColorShader_Type);

  if (PyType_Ready(&ConstantThicknessShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ConstantThicknessShader_Type);
  PyModule_AddObject(module, "ConstantThicknessShader", (PyObject *)&ConstantThicknessShader_Type);

  if (PyType_Ready(&ConstrainedIncreasingThicknessShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ConstrainedIncreasingThicknessShader_Type);
  PyModule_AddObject(module,
                     "ConstrainedIncreasingThicknessShader",
                     (PyObject *)&ConstrainedIncreasingThicknessShader_Type);

  if (PyType_Ready(&GuidingLinesShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GuidingLinesShader_Type);
  PyModule_AddObject(module, "GuidingLinesShader", (PyObject *)&GuidingLinesShader_Type);

  if (PyType_Ready(&IncreasingColorShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&IncreasingColorShader_Type);
  PyModule_AddObject(module, "IncreasingColorShader", (PyObject *)&IncreasingColorShader_Type);

  if (PyType_Ready(&IncreasingThicknessShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&IncreasingThicknessShader_Type);
  PyModule_AddObject(
      module, "IncreasingThicknessShader", (PyObject *)&IncreasingThicknessShader_Type);

  if (PyType_Ready(&PolygonalizationShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&PolygonalizationShader_Type);
  PyModule_AddObject(module, "PolygonalizationShader", (PyObject *)&PolygonalizationShader_Type);

  if (PyType_Ready(&SamplingShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&SamplingShader_Type);
  PyModule_AddObject(module, "SamplingShader", (PyObject *)&SamplingShader_Type);

  if (PyType_Ready(&SmoothingShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&SmoothingShader_Type);
  PyModule_AddObject(module, "SmoothingShader", (PyObject *)&SmoothingShader_Type);

  if (PyType_Ready(&SpatialNoiseShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&SpatialNoiseShader_Type);
  PyModule_AddObject(module, "SpatialNoiseShader", (PyObject *)&SpatialNoiseShader_Type);

  if (PyType_Ready(&StrokeTextureStepShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&StrokeTextureStepShader_Type);
  PyModule_AddObject(module, "StrokeTextureStepShader", (PyObject *)&StrokeTextureStepShader_Type);

  if (PyType_Ready(&ThicknessNoiseShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ThicknessNoiseShader_Type);
  PyModule_AddObject(module, "ThicknessNoiseShader", (PyObject *)&ThicknessNoiseShader_Type);

  if (PyType_Ready(&TipRemoverShader_Type) < 0) {
    return -1;
  }
  Py_INCREF(&TipRemoverShader_Type);
  PyModule_AddObject(module, "TipRemoverShader", (PyObject *)&TipRemoverShader_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char StrokeShader___doc__[] =
    "Base class for stroke shaders. Any stroke shader must inherit from\n"
    "this class and overload the shade() method. A StrokeShader is\n"
    "designed to modify stroke attributes such as thickness, color,\n"
    "geometry, texture, blending mode, and so on. The basic way for this\n"
    "operation is to iterate over the stroke vertices of the :class:`Stroke`\n"
    "and to modify the :class:`StrokeAttribute` of each vertex. Here is a\n"
    "code example of such an iteration::\n"
    "\n"
    "  it = ioStroke.strokeVerticesBegin()\n"
    "  while not it.is_end:\n"
    "      att = it.object.attribute\n"
    "      ## perform here any attribute modification\n"
    "      it.increment()\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n";

static int StrokeShader___init__(BPy_StrokeShader *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->ss = new StrokeShader();
  self->ss->py_ss = (PyObject *)self;
  return 0;
}

static void StrokeShader___dealloc__(BPy_StrokeShader *self)
{
  delete self->ss;
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *StrokeShader___repr__(BPy_StrokeShader *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->ss);
}

static char StrokeShader_shade___doc__[] =
    ".. method:: shade(stroke)\n"
    "\n"
    "   The shading method. Must be overloaded by inherited classes.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`Stroke`\n";

static PyObject *StrokeShader_shade(BPy_StrokeShader *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"stroke", nullptr};
  PyObject *py_s = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &Stroke_Type, &py_s)) {
    return nullptr;
  }

  if (typeid(*(self->ss)) == typeid(StrokeShader)) {
    PyErr_SetString(PyExc_TypeError, "shade method not properly overridden");
    return nullptr;
  }
  if (self->ss->shade(*(((BPy_Stroke *)py_s)->s)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " shade method failed").c_str());
    }
    return nullptr;
  }
  Py_RETURN_NONE;
}

static PyMethodDef BPy_StrokeShader_methods[] = {
    {"shade",
     (PyCFunction)StrokeShader_shade,
     METH_VARARGS | METH_KEYWORDS,
     StrokeShader_shade___doc__},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------StrokeShader get/setters ----------------------------*/

PyDoc_STRVAR(StrokeShader_name_doc,
             "The name of the stroke shader.\n"
             "\n"
             ":type: str");

static PyObject *StrokeShader_name_get(BPy_StrokeShader *self, void * /*closure*/)
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

static PyGetSetDef BPy_StrokeShader_getseters[] = {
    {"name", (getter)StrokeShader_name_get, (setter) nullptr, StrokeShader_name_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_StrokeShader type definition ------------------------------*/

PyTypeObject StrokeShader_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "StrokeShader",
    /*tp_basicsize*/ sizeof(BPy_StrokeShader),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)StrokeShader___dealloc__,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)StrokeShader___repr__,
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
    /*tp_doc*/ StrokeShader___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_StrokeShader_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_StrokeShader_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)StrokeShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
