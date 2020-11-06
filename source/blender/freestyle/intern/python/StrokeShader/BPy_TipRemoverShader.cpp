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

#include "BPy_TipRemoverShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char TipRemoverShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`TipRemoverShader`\n"
    "\n"
    "[Geometry shader]\n"
    "\n"
    ".. method:: __init__(tip_length)\n"
    "\n"
    "   Builds a TipRemoverShader object.\n"
    "\n"
    "   :arg tip_length: The length of the piece of stroke we want to remove\n"
    "      at each extremity.\n"
    "   :type tip_length: float\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Removes the stroke's extremities.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int TipRemoverShader___init__(BPy_TipRemoverShader *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"tip_length", nullptr};
  double d;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "d", (char **)kwlist, &d)) {
    return -1;
  }
  self->py_ss.ss = new StrokeShaders::TipRemoverShader(d);
  return 0;
}

/*-----------------------BPy_TipRemoverShader type definition ------------------------------*/

PyTypeObject TipRemoverShader_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "TipRemoverShader", /* tp_name */
    sizeof(BPy_TipRemoverShader),                      /* tp_basicsize */
    0,                                                 /* tp_itemsize */
    nullptr,                                                 /* tp_dealloc */
    nullptr,                                                 /* tp_print */
    nullptr,                                                 /* tp_getattr */
    nullptr,                                                 /* tp_setattr */
    nullptr,                                                 /* tp_reserved */
    nullptr,                                                 /* tp_repr */
    nullptr,                                                 /* tp_as_number */
    nullptr,                                                 /* tp_as_sequence */
    nullptr,                                                 /* tp_as_mapping */
    nullptr,                                                 /* tp_hash  */
    nullptr,                                                 /* tp_call */
    nullptr,                                                 /* tp_str */
    nullptr,                                                 /* tp_getattro */
    nullptr,                                                 /* tp_setattro */
    nullptr,                                                 /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,          /* tp_flags */
    TipRemoverShader___doc__,                          /* tp_doc */
    nullptr,                                                 /* tp_traverse */
    nullptr,                                                 /* tp_clear */
    nullptr,                                                 /* tp_richcompare */
    0,                                                 /* tp_weaklistoffset */
    nullptr,                                                 /* tp_iter */
    nullptr,                                                 /* tp_iternext */
    nullptr,                                                 /* tp_methods */
    nullptr,                                                 /* tp_members */
    nullptr,                                                 /* tp_getset */
    &StrokeShader_Type,                                /* tp_base */
    nullptr,                                                 /* tp_dict */
    nullptr,                                                 /* tp_descr_get */
    nullptr,                                                 /* tp_descr_set */
    0,                                                 /* tp_dictoffset */
    (initproc)TipRemoverShader___init__,               /* tp_init */
    nullptr,                                                 /* tp_alloc */
    nullptr,                                                 /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
