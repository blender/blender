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
 * \ingroup freestyle
 */

#include "BPy_GuidingLinesShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    "      middle of the initial stroke bounding box.  offset is the value\n"
    "      of the displacement which is applied to this line along its\n"
    "      normal.\n"
    "   :type offset: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Shader to modify the Stroke geometry so that it corresponds to its\n"
    "   main direction line.  This shader must be used together with the\n"
    "   splitting operator using the curvature criterion. Indeed, the\n"
    "   precision of the approximation will depend on the size of the\n"
    "   stroke's pieces.  The bigger the pieces are, the rougher the\n"
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
    PyVarObject_HEAD_INIT(nullptr, 0) "GuidingLinesShader", /* tp_name */
    sizeof(BPy_GuidingLinesShader),                         /* tp_basicsize */
    0,                                                      /* tp_itemsize */
    nullptr,                                                /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                  /* tp_getattr */
    nullptr,                                  /* tp_setattr */
    nullptr,                                  /* tp_reserved */
    nullptr,                                  /* tp_repr */
    nullptr,                                  /* tp_as_number */
    nullptr,                                  /* tp_as_sequence */
    nullptr,                                  /* tp_as_mapping */
    nullptr,                                  /* tp_hash  */
    nullptr,                                  /* tp_call */
    nullptr,                                  /* tp_str */
    nullptr,                                  /* tp_getattro */
    nullptr,                                  /* tp_setattro */
    nullptr,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    GuidingLinesShader___doc__,               /* tp_doc */
    nullptr,                                  /* tp_traverse */
    nullptr,                                  /* tp_clear */
    nullptr,                                  /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    nullptr,                                  /* tp_iter */
    nullptr,                                  /* tp_iternext */
    nullptr,                                  /* tp_methods */
    nullptr,                                  /* tp_members */
    nullptr,                                  /* tp_getset */
    &StrokeShader_Type,                       /* tp_base */
    nullptr,                                  /* tp_dict */
    nullptr,                                  /* tp_descr_get */
    nullptr,                                  /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)GuidingLinesShader___init__,    /* tp_init */
    nullptr,                                  /* tp_alloc */
    nullptr,                                  /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
