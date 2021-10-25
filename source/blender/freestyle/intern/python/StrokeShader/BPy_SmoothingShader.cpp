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

/** \file source/blender/freestyle/intern/python/StrokeShader/BPy_SmoothingShader.cpp
 *  \ingroup freestyle
 */

#include "BPy_SmoothingShader.h"

#include "../../stroke/AdvancedStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char SmoothingShader___doc__[] =
"Class hierarchy: :class:`freestyle.types.StrokeShader` > :class:`SmoothingShader`\n"
"\n"
"[Geometry shader]\n"
"\n"
".. method:: __init__(num_iterations=100, factor_point=0.1,\n"
"      factor_curvature=0.0, factor_curvature_difference=0.2,\n"
"      aniso_point=0.0, aniso_normal=0.0, aniso_curvature=0.0,\n"
"      carricature_factor=1.0)\n"
"\n"
"   Builds a SmoothingShader object.\n"
"\n"
"   :arg num_iterations: The number of iterations.\n"
"   :type num_iterations: int\n"
"   :arg factor_point: 0.1\n"
"   :type factor_point: float\n"
"   :arg factor_curvature: 0.0\n"
"   :type factor_curvature: float\n"
"   :arg factor_curvature_difference: 0.2\n"
"   :type factor_curvature_difference: float\n"
"   :arg aniso_point: 0.0\n"
"   :type aniso_point: float\n"
"   :arg aniso_normal: 0.0\n"
"   :type aniso_normal: float\n"
"   :arg aniso_curvature: 0.0\n"
"   :type aniso_curvature: float\n"
"   :arg carricature_factor: 1.0\n"
"   :type carricature_factor: float\n"
"\n"
".. method:: shade(stroke)\n"
"\n"
"   Smoothes the stroke by moving the vertices to make the stroke\n"
"   smoother.  Uses curvature flow to converge towards a curve of\n"
"   constant curvature.  The diffusion method we use is anisotropic to\n"
"   prevent the diffusion across corners.\n"
"\n"
"   :arg stroke: A Stroke object.\n"
"   :type stroke: :class:`freestyle.types.Stroke`\n";

static int SmoothingShader___init__(BPy_SmoothingShader *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"num_iterations", "factor_point", "factor_curvature",
	                               "factor_curvature_difference", "aniso_point", "aniso_normal",
	                               "aniso_curvature", "carricature_factor", NULL};
	int i1 = 100;
	double d2 = 0.1, d3 = 0.0, d4 = 0.2, d5 = 0.0, d6 = 0.0, d7 = 0.0, d8 = 1.0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iddddddd", (char **)kwlist,
	                                 &i1, &d2, &d3, &d4, &d5, &d6, &d7, &d8))
	{
		return -1;
	}
	self->py_ss.ss = new SmoothingShader(i1, d2, d3, d4, d5, d6, d7, d8);
	return 0;
}

/*-----------------------BPy_SmoothingShader type definition ------------------------------*/

PyTypeObject SmoothingShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SmoothingShader",              /* tp_name */
	sizeof(BPy_SmoothingShader),    /* tp_basicsize */
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
	SmoothingShader___doc__,        /* tp_doc */
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
	(initproc)SmoothingShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
