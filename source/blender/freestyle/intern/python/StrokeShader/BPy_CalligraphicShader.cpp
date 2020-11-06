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

#include "BPy_CalligraphicShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char CalligraphicShader___doc__[] =
    "Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`CalligraphicShader`\n"
    "\n"
    "[Thickness Shader]\n"
    "\n"
    ".. method:: __init__(thickness_min, thickness_max, orientation, clamp)\n"
    "\n"
    "   Builds a CalligraphicShader object.\n"
    "\n"
    "   :arg thickness_min: The minimum thickness in the direction\n"
    "      perpendicular to the main direction.\n"
    "   :type thickness_min: float\n"
    "   :arg thickness_max: The maximum thickness in the main direction.\n"
    "   :type thickness_max: float\n"
    "   :arg orientation: The 2D vector giving the main direction.\n"
    "   :type orientation: :class:`mathutils.Vector`\n"
    "   :arg clamp: If true, the strokes are drawn in black when the stroke\n"
    "      direction is between -90 and 90 degrees with respect to the main\n"
    "      direction and drawn in white otherwise.  If false, the strokes\n"
    "      are always drawn in black.\n"
    "   :type clamp: bool\n"
    "\n"
    ".. method:: shade(stroke)\n"
    "\n"
    "   Assigns thicknesses to the stroke vertices so that the stroke looks\n"
    "   like made with a calligraphic tool, i.e. the stroke will be the\n"
    "   thickest in a main direction, and the thinnest in the direction\n"
    "   perpendicular to this one, and an interpolation in between.\n"
    "\n"
    "   :arg stroke: A Stroke object.\n"
    "   :type stroke: :class:`freestyle.types.Stroke`\n";

static int CalligraphicShader___init__(BPy_CalligraphicShader *self,
                                       PyObject *args,
                                       PyObject *kwds)
{
  static const char *kwlist[] = {"thickness_min", "thickness_max", "orientation", "clamp", nullptr};
  double d1, d2;
  float f3[2];
  PyObject *obj4 = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "ddO&O!", (char **)kwlist, &d1, &d2, convert_v2, f3, &PyBool_Type, &obj4)) {
    return -1;
  }
  Vec2f v(f3[0], f3[1]);
  self->py_ss.ss = new CalligraphicShader(d1, d2, v, bool_from_PyBool(obj4));
  return 0;
}

/*-----------------------BPy_CalligraphicShader type definition ------------------------------*/

PyTypeObject CalligraphicShader_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "CalligraphicShader", /* tp_name */
    sizeof(BPy_CalligraphicShader),                      /* tp_basicsize */
    0,                                                   /* tp_itemsize */
    nullptr,                                                   /* tp_dealloc */
    nullptr,                                                   /* tp_print */
    nullptr,                                                   /* tp_getattr */
    nullptr,                                                   /* tp_setattr */
    nullptr,                                                   /* tp_reserved */
    nullptr,                                                   /* tp_repr */
    nullptr,                                                   /* tp_as_number */
    nullptr,                                                   /* tp_as_sequence */
    nullptr,                                                   /* tp_as_mapping */
    nullptr,                                                   /* tp_hash  */
    nullptr,                                                   /* tp_call */
    nullptr,                                                   /* tp_str */
    nullptr,                                                   /* tp_getattro */
    nullptr,                                                   /* tp_setattro */
    nullptr,                                                   /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,            /* tp_flags */
    CalligraphicShader___doc__,                          /* tp_doc */
    nullptr,                                                   /* tp_traverse */
    nullptr,                                                   /* tp_clear */
    nullptr,                                                   /* tp_richcompare */
    0,                                                   /* tp_weaklistoffset */
    nullptr,                                                   /* tp_iter */
    nullptr,                                                   /* tp_iternext */
    nullptr,                                                   /* tp_methods */
    nullptr,                                                   /* tp_members */
    nullptr,                                                   /* tp_getset */
    &StrokeShader_Type,                                  /* tp_base */
    nullptr,                                                   /* tp_dict */
    nullptr,                                                   /* tp_descr_get */
    nullptr,                                                   /* tp_descr_set */
    0,                                                   /* tp_dictoffset */
    (initproc)CalligraphicShader___init__,               /* tp_init */
    nullptr,                                                   /* tp_alloc */
    nullptr,                                                   /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
