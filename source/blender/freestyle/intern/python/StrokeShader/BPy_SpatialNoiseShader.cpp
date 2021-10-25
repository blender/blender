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

/** \file source/blender/freestyle/intern/python/StrokeShader/BPy_SpatialNoiseShader.cpp
 *  \ingroup freestyle
 */

#include "BPy_SpatialNoiseShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"
#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char SpatialNoiseShader___doc__[] =
"Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`SpatialNoiseShader`\n"
"\n"
"[Geometry shader]\n"
"\n"
".. method:: __init__(amount, scale, num_octaves, smooth, pure_random)\n"
"\n"
"   Builds a SpatialNoiseShader object.\n"
"\n"
"   :arg amount: The amplitude of the noise.\n"
"   :type amount: float\n"
"   :arg scale: The noise frequency.\n"
"   :type scale: float\n"
"   :arg num_octaves: The number of octaves\n"
"   :type num_octaves: int\n"
"   :arg smooth: True if you want the noise to be smooth.\n"
"   :type smooth: bool\n"
"   :arg pure_random: True if you don't want any coherence.\n"
"   :type pure_random: bool\n"
"\n"
".. method:: shade(stroke)\n"
"\n"
"   Spatial Noise stroke shader.  Moves the vertices to make the stroke\n"
"   more noisy.\n"
"\n"
"   :arg stroke: A Stroke object.\n"
"   :type stroke: :class:`freestyle.types.Stroke`\n";

static int SpatialNoiseShader___init__(BPy_SpatialNoiseShader *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"amount", "scale", "num_octaves", "smooth", "pure_random", NULL};
	float f1, f2;
	int i3;
	PyObject *obj4 = 0, *obj5 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ffiO!O!", (char **)kwlist,
	                                 &f1, &f2, &i3, &PyBool_Type, &obj4, &PyBool_Type, &obj5))
	{
		return -1;
	}
	self->py_ss.ss = new SpatialNoiseShader(f1, f2, i3, bool_from_PyBool(obj4), bool_from_PyBool(obj5));
	return 0;
}

/*-----------------------BPy_SpatialNoiseShader type definition ------------------------------*/

PyTypeObject SpatialNoiseShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SpatialNoiseShader",          /* tp_name */
	sizeof(BPy_SpatialNoiseShader), /* tp_basicsize */
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
	SpatialNoiseShader___doc__,     /* tp_doc */
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
	(initproc)SpatialNoiseShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
