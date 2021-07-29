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

/** \file source/blender/freestyle/intern/python/UnaryFunction0D/UnaryFunction0D_double/BPy_LocalAverageDepthF0D.cpp
 *  \ingroup freestyle
 */

#include "BPy_LocalAverageDepthF0D.h"

#include "../../../stroke/AdvancedFunctions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char LocalAverageDepthF0D___doc__[] =
"Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > :class:`freestyle.types.UnaryFunction0DDouble` > :class:`LocalAverageDepthF0D`\n"
"\n"
".. method:: __init__(mask_size=5.0)\n"
"\n"
"   Builds a LocalAverageDepthF0D object.\n"
"\n"
"   :arg mask_size: The size of the mask.\n"
"   :type mask_size: float\n"
"\n"
".. method:: __call__(it)\n"
"\n"
"   Returns the average depth around the\n"
"   :class:`freestyle.types.Interface0D` pointed by the\n"
"   Interface0DIterator.  The result is obtained by querying the depth\n"
"   buffer on a window around that point.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`freestyle.types.Interface0DIterator`\n"
"   :return: The average depth around the pointed Interface0D.\n"
"   :rtype: float\n";

static int LocalAverageDepthF0D___init__(BPy_LocalAverageDepthF0D *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"mask_size", NULL};
	double d = 5.0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|d", (char **)kwlist, &d))
		return -1;
	self->py_uf0D_double.uf0D_double = new Functions0D::LocalAverageDepthF0D(d);
	self->py_uf0D_double.uf0D_double->py_uf0D = (PyObject *)self;
	return 0;
}

/*-----------------------BPy_LocalAverageDepthF0D type definition ------------------------------*/

PyTypeObject LocalAverageDepthF0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"LocalAverageDepthF0D",         /* tp_name */
	sizeof(BPy_LocalAverageDepthF0D), /* tp_basicsize */
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
	LocalAverageDepthF0D___doc__,   /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DDouble_Type,    /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)LocalAverageDepthF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
