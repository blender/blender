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

/** \file source/blender/freestyle/intern/python/StrokeShader/BPy_ThicknessVariationPatternShader.cpp
 *  \ingroup freestyle
 */

#include "BPy_ThicknessVariationPatternShader.h"

#include "../../stroke/BasicStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ThicknessVariationPatternShader___doc__[] =
"Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`ThicknessVariationPatternShader`\n"
"\n"
"[Thickness shader]\n"
"\n"
".. method:: __init__(pattern_name, thickness_min=1.0, thickness_max=5.0, stretch=True)\n"
"\n"
"   Builds a ThicknessVariationPatternShader object.\n"
"\n"
"   :arg pattern_name: The texture file name.\n"
"   :type pattern_name: str\n"
"   :arg thickness_min: The minimum thickness we don't want to exceed.\n"
"   :type thickness_min: float\n"
"   :arg thickness_max: The maximum thickness we don't want to exceed.\n"
"   :type thickness_max: float\n"
"   :arg stretch: Tells whether the pattern texture must be stretched\n"
"      or repeated to fit the stroke.\n"
"   :type stretch: bool\n"
"\n"
".. method:: shade(stroke)\n"
"\n"
"   Applies a pattern (texture) to vary thickness. The new thicknesses\n"
"   are the result of the multiplication of the pattern and the\n"
"   original thickness.\n"
"\n"
"   :arg stroke: A Stroke object.\n"
"   :type stroke: :class:`freestyle.types.Stroke`\n";

static int ThicknessVariationPatternShader___init__(BPy_ThicknessVariationPatternShader *self,
                                                    PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"pattern_name", "thickness_min", "thickness_max", "stretch", NULL};
	const char *s1;
	float f2 = 1.0, f3 = 5.0;
	PyObject *obj4 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|ffO!", (char **)kwlist, &s1, &f2, &f3, &PyBool_Type, &obj4))
		return -1;
	bool b = (!obj4) ? true : bool_from_PyBool(obj4);
	self->py_ss.ss = new StrokeShaders::ThicknessVariationPatternShader(s1, f2, f3, b);
	return 0;
}

/*-----------------------BPy_ThicknessVariationPatternShader type definition ------------------------------*/

PyTypeObject ThicknessVariationPatternShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ThicknessVariationPatternShader", /* tp_name */
	sizeof(BPy_ThicknessVariationPatternShader), /* tp_basicsize */
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
	ThicknessVariationPatternShader___doc__, /* tp_doc */
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
	(initproc)ThicknessVariationPatternShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
