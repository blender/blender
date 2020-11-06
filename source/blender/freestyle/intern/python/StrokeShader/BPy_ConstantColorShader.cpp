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

#include "BPy_ConstantColorShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    PyVarObject_HEAD_INIT(nullptr, 0) "ConstantColorShader", /* tp_name */
    sizeof(BPy_ConstantColorShader),                      /* tp_basicsize */
    0,                                                    /* tp_itemsize */
    nullptr,                                                    /* tp_dealloc */
    nullptr,                                                    /* tp_print */
    nullptr,                                                    /* tp_getattr */
    nullptr,                                                    /* tp_setattr */
    nullptr,                                                    /* tp_reserved */
    nullptr,                                                    /* tp_repr */
    nullptr,                                                    /* tp_as_number */
    nullptr,                                                    /* tp_as_sequence */
    nullptr,                                                    /* tp_as_mapping */
    nullptr,                                                    /* tp_hash  */
    nullptr,                                                    /* tp_call */
    nullptr,                                                    /* tp_str */
    nullptr,                                                    /* tp_getattro */
    nullptr,                                                    /* tp_setattro */
    nullptr,                                                    /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,             /* tp_flags */
    ConstantColorShader___doc__,                          /* tp_doc */
    nullptr,                                                    /* tp_traverse */
    nullptr,                                                    /* tp_clear */
    nullptr,                                                    /* tp_richcompare */
    0,                                                    /* tp_weaklistoffset */
    nullptr,                                                    /* tp_iter */
    nullptr,                                                    /* tp_iternext */
    nullptr,                                                    /* tp_methods */
    nullptr,                                                    /* tp_members */
    nullptr,                                                    /* tp_getset */
    &StrokeShader_Type,                                   /* tp_base */
    nullptr,                                                    /* tp_dict */
    nullptr,                                                    /* tp_descr_get */
    nullptr,                                                    /* tp_descr_set */
    0,                                                    /* tp_dictoffset */
    (initproc)ConstantColorShader___init__,               /* tp_init */
    nullptr,                                                    /* tp_alloc */
    nullptr,                                                    /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
