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

#include "BPy_IncreasingColorShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    "   Assigns a varying color to the stroke.  The user specifies two\n"
    "   colors A and B.  The stroke color will change linearly from A to B\n"
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
      NULL,
  };
  float f1, f2, f3, f4, f5, f6, f7, f8;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "ffffffff", (char **)kwlist, &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8)) {
    return -1;
  }
  self->py_ss.ss = new StrokeShaders::IncreasingColorShader(f1, f2, f3, f4, f5, f6, f7, f8);
  return 0;
}

/*-----------------------BPy_IncreasingColorShader type definition ------------------------------*/

PyTypeObject IncreasingColorShader_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "IncreasingColorShader", /* tp_name */
    sizeof(BPy_IncreasingColorShader),                      /* tp_basicsize */
    0,                                                      /* tp_itemsize */
    0,                                                      /* tp_dealloc */
    0,                                                      /* tp_print */
    0,                                                      /* tp_getattr */
    0,                                                      /* tp_setattr */
    0,                                                      /* tp_reserved */
    0,                                                      /* tp_repr */
    0,                                                      /* tp_as_number */
    0,                                                      /* tp_as_sequence */
    0,                                                      /* tp_as_mapping */
    0,                                                      /* tp_hash  */
    0,                                                      /* tp_call */
    0,                                                      /* tp_str */
    0,                                                      /* tp_getattro */
    0,                                                      /* tp_setattro */
    0,                                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,               /* tp_flags */
    IncreasingColorShader___doc__,                          /* tp_doc */
    0,                                                      /* tp_traverse */
    0,                                                      /* tp_clear */
    0,                                                      /* tp_richcompare */
    0,                                                      /* tp_weaklistoffset */
    0,                                                      /* tp_iter */
    0,                                                      /* tp_iternext */
    0,                                                      /* tp_methods */
    0,                                                      /* tp_members */
    0,                                                      /* tp_getset */
    &StrokeShader_Type,                                     /* tp_base */
    0,                                                      /* tp_dict */
    0,                                                      /* tp_descr_get */
    0,                                                      /* tp_descr_set */
    0,                                                      /* tp_dictoffset */
    (initproc)IncreasingColorShader___init__,               /* tp_init */
    0,                                                      /* tp_alloc */
    0,                                                      /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
