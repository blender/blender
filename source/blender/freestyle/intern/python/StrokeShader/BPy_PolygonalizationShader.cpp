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

/** \file source/blender/freestyle/intern/python/StrokeShader/BPy_PolygonalizationShader.cpp
 *  \ingroup freestyle
 */

#include "BPy_PolygonalizationShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char PolygonalizationShader___doc__[] =
"Class hierarchy: :class:`StrokeShader` > :class:`PolygonalizationShader`\n"
"\n"
"[Geometry shader]\n"
"\n"
".. method:: __init__(error)\n"
"\n"
"   Builds a PolygonalizationShader object.\n"
"\n"
"   :arg error: The error we want our polygonal approximation to have\n"
"      with respect to the original geometry.  The smaller, the closer\n"
"      the new stroke is to the orinal one.  This error corresponds to\n"
"      the maximum distance between the new stroke and the old one.\n"
"   :type error: float\n"
"\n"
".. method:: shade(stroke)\n"
"\n"
"   Modifies the Stroke geometry so that it looks more \"polygonal\".\n"
"   The basic idea is to start from the minimal stroke approximation\n"
"   consisting in a line joining the first vertex to the last one and\n"
"   to subdivide using the original stroke vertices until a certain\n"
"   error is reached.\n"
"\n"
"   :arg stroke: A Stroke object.\n"
"   :type stroke: :class:`Stroke`\n";

static int PolygonalizationShader___init__(BPy_PolygonalizationShader *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"error", NULL};
	float f;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "f", (char **)kwlist, &f))
		return -1;
	self->py_ss.ss = new StrokeShaders::PolygonalizationShader(f);
	return 0;
}

/*-----------------------BPy_PolygonalizationShader type definition ------------------------------*/

PyTypeObject PolygonalizationShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"PolygonalizationShader",          /* tp_name */
	sizeof(BPy_PolygonalizationShader), /* tp_basicsize */
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
	PolygonalizationShader___doc__, /* tp_doc */
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
	(initproc)PolygonalizationShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
