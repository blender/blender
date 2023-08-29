/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_GuidingLinesShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char GuidingLinesShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`GuidingLinesShader`\n"
    "\n"
    "[Geometry shader]\n"
    "\n"
    ".. method:: __init__(offset)\n"
    "\n"
    "   Builds a GuidingLinesShader object.\n"
    "\n"
    "   :arg offset: The line that replaces the stroke is initially in the\n"
    "      middle of the initial stroke bounding box. offset is the value\n"
    "      of the displacement which is applied to this line along its\n"
    "      normal.\n"
    "   :type offset: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Shader to modify the Stroke geometry so that it corresponds to its\n"
    "   main direction line. This shader must be used together with the\n"
    "   splitting operator using the curvature criterion. Indeed, the\n"
    "   precision of the approximation will depend on the size of the\n"
    "   stroke's pieces. The bigger the pieces are, the rougher the\n"
    "   approximation is.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int GuidingLinesShader___init__(BPy_GuidingLinesShader *self,
                                       PyObject *args,
                                       PyObject *kwds)
{
  static const char *kwlist[] = {"offset", nullptr};
  float f;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "f", (char **)kwlist, &f)) {
    return -1;
  }
  self->py_ss.ss = new StrokeShaders::GuidingLinesShader(f);
  return 0;
}

/*-----------------------BPy_GuidingLinesShader type definition ------------------------------*/

PyTypeObject GuidingLinesShader_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GuidingLinesShader",
    /*tp_basicsize*/ sizeof(BPy_GuidingLinesShader),
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
    /*tp_doc*/ GuidingLinesShader___doc__,
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
    /*tp_init*/ (initproc)GuidingLinesShader___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
