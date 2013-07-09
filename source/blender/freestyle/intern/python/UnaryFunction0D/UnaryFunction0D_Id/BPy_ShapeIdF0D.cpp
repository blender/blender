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

/** \file source/blender/freestyle/intern/python/UnaryFunction0D/UnaryFunction0D_Id/BPy_ShapeIdF0D.cpp
 *  \ingroup freestyle
 */

#include "BPy_ShapeIdF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ShapeIdF0D___doc__[] =
"Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DId` > :class:`ShapeIdF0D`\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Builds a ShapeIdF0D object.\n"
"\n"
".. method:: __call__(it)\n"
"\n"
"   Returns the :class:`Id` of the Shape the :class:`Interface0D`\n"
"   pointed by the Interface0DIterator belongs to. This evaluation can\n"
"   be ambiguous (in the case of a :class:`TVertex` for example).  This\n"
"   functor tries to remove this ambiguity using the context offered by\n"
"   the 1D element to which the Interface0DIterator belongs to.\n"
"   However, there still can be problematic cases, and the user willing\n"
"   to deal with this cases in a specific way should implement its own\n"
"   getShapeIdF0D functor.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`Interface0DIterator`\n"
"   :return: The Id of the Shape the pointed Interface0D belongs to.\n"
"   :rtype: :class:`Id`\n";

static int ShapeIdF0D___init__(BPy_ShapeIdF0D *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist))
		return -1;
	self->py_uf0D_id.uf0D_id = new Functions0D::ShapeIdF0D();
	self->py_uf0D_id.uf0D_id->py_uf0D = (PyObject *)self;
	return 0;
}

/*-----------------------BPy_ShapeIdF0D type definition ------------------------------*/

PyTypeObject ShapeIdF0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ShapeIdF0D",                   /* tp_name */
	sizeof(BPy_ShapeIdF0D),         /* tp_basicsize */
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
	ShapeIdF0D___doc__,             /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DId_Type,        /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ShapeIdF0D___init__,  /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
