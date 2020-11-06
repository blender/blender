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

#include "BPy_IncreasingThicknessShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char IncreasingThicknessShader___doc__[] =
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
    "   this midpoint vertex and the last vertex.  The thickness is\n"
    "   linearly interpolated from A to B.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

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
    PyVarObject_HEAD_INIT(nullptr, 0) "IncreasingThicknessShader", /* tp_name */
    sizeof(BPy_IncreasingThicknessShader),                      /* tp_basicsize */
    0,                                                          /* tp_itemsize */
    nullptr,                                                          /* tp_dealloc */
    nullptr,                                                          /* tp_print */
    nullptr,                                                          /* tp_getattr */
    nullptr,                                                          /* tp_setattr */
    nullptr,                                                          /* tp_reserved */
    nullptr,                                                          /* tp_repr */
    nullptr,                                                          /* tp_as_number */
    nullptr,                                                          /* tp_as_sequence */
    nullptr,                                                          /* tp_as_mapping */
    nullptr,                                                          /* tp_hash  */
    nullptr,                                                          /* tp_call */
    nullptr,                                                          /* tp_str */
    nullptr,                                                          /* tp_getattro */
    nullptr,                                                          /* tp_setattro */
    nullptr,                                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                   /* tp_flags */
    IncreasingThicknessShader___doc__,                          /* tp_doc */
    nullptr,                                                          /* tp_traverse */
    nullptr,                                                          /* tp_clear */
    nullptr,                                                          /* tp_richcompare */
    0,                                                          /* tp_weaklistoffset */
    nullptr,                                                          /* tp_iter */
    nullptr,                                                          /* tp_iternext */
    nullptr,                                                          /* tp_methods */
    nullptr,                                                          /* tp_members */
    nullptr,                                                          /* tp_getset */
    &StrokeShader_Type,                                         /* tp_base */
    nullptr,                                                          /* tp_dict */
    nullptr,                                                          /* tp_descr_get */
    nullptr,                                                          /* tp_descr_set */
    0,                                                          /* tp_dictoffset */
    (initproc)IncreasingThicknessShader___init__,               /* tp_init */
    nullptr,                                                          /* tp_alloc */
    nullptr,                                                          /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
