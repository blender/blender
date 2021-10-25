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

/** \file source/blender/freestyle/intern/python/UnaryFunction0D/UnaryFunction0D_float/BPy_GetViewMapGradientNormF0D.cpp
 *  \ingroup freestyle
 */

#include "BPy_GetViewMapGradientNormF0D.h"

#include "../../../stroke/AdvancedFunctions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char GetViewMapGradientNormF0D___doc__[] =
"Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > :class:`freestyle.types.UnaryFunction0DFloat` > :class:`GetViewMapGradientNormF0D`\n"
"\n"
".. method:: __init__(level)\n"
"\n"
"   Builds a GetViewMapGradientNormF0D object.\n"
"\n"
"   :arg level: The level of the pyramid from which the pixel must be\n"
"      read.\n"
"   :type level: int\n"
"\n"
".. method:: __call__(it)\n"
"\n"
"   Returns the norm of the gradient of the global viewmap density\n"
"   image.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`freestyle.types.Interface0DIterator`\n"
"   :return: The norm of the gradient of the global viewmap density\n"
"      image.\n"
"   :rtype: float\n";

static int GetViewMapGradientNormF0D___init__(BPy_GetViewMapGradientNormF0D *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"level", NULL};
	int i;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", (char **)kwlist, &i))
		return -1;
	self->py_uf0D_float.uf0D_float = new Functions0D::GetViewMapGradientNormF0D(i);
	self->py_uf0D_float.uf0D_float->py_uf0D = (PyObject *)self;
	return 0;
}

/*-----------------------BPy_GetViewMapGradientNormF0D type definition ------------------------------*/

PyTypeObject GetViewMapGradientNormF0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"GetViewMapGradientNormF0D",    /* tp_name */
	sizeof(BPy_GetViewMapGradientNormF0D), /* tp_basicsize */
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
	GetViewMapGradientNormF0D___doc__, /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DFloat_Type,     /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)GetViewMapGradientNormF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
