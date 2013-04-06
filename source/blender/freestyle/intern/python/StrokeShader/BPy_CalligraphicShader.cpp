/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/freestyle/intern/python/StrokeShader/BPy_CalligraphicShader.cpp
 *  \ingroup freestyle
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
"Class hierarchy: :class:`StrokeShader` > :class:`CalligraphicShader`\n"
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
"   thickest in a main direction, and the thinest in the direction\n"
"   perpendicular to this one, and an interpolation inbetween.\n"
"\n"
"   :arg stroke: A Stroke object.\n"
"   :type stroke: :class:`Stroke`\n";

static int convert_v2(PyObject *obj, void *v)
{
	return float_array_from_PyObject(obj, (float *)v, 2);
}

static int CalligraphicShader___init__(BPy_CalligraphicShader *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"thickness_min", "thickness_max", "orientation", "clamp", NULL};
	double d1, d2;
	float f3[2];
	PyObject *obj4 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ddO&O!", (char **)kwlist,
	                                 &d1, &d2, convert_v2, f3, &PyBool_Type, &obj4))
	{
		return -1;
	}
	Vec2f v(f3[0], f3[1]);
	self->py_ss.ss = new CalligraphicShader(d1, d2, v, bool_from_PyBool(obj4));
	return 0;
}

/*-----------------------BPy_CalligraphicShader type definition ------------------------------*/

PyTypeObject CalligraphicShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"CalligraphicShader",           /* tp_name */
	sizeof(BPy_CalligraphicShader), /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	CalligraphicShader___doc__,     /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&StrokeShader_Type,             /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)CalligraphicShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
