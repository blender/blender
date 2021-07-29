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

/** \file source/blender/freestyle/intern/python/UnaryFunction0D/UnaryFunction0D_unsigned_int/BPy_QuantitativeInvisibilityF0D.cpp
 *  \ingroup freestyle
 */

#include "BPy_QuantitativeInvisibilityF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char QuantitativeInvisibilityF0D___doc__[] =
"Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > :class:`freestyle.types.UnaryFunction0DUnsigned` > :class:`QuantitativeInvisibilityF0D`\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Builds a QuantitativeInvisibilityF0D object.\n"
"\n"
".. method:: __call__(it)\n"
"\n"
"   Returns the quantitative invisibility of the\n"
"   :class:`freestyle.types.Interface0D` pointed by the\n"
"   Interface0DIterator.  This evaluation can be ambiguous (in the case of\n"
"   a :class:`freestyle.types.TVertex` for example).  This functor tries\n"
"   to remove this ambiguity using the context offered by the 1D element\n"
"   to which the Interface0D belongs to.  However, there still can be\n"
"   problematic cases, and the user willing to deal with this cases in a\n"
"   specific way should implement its own getQIF0D functor.\n"
"\n"
"   :arg it: An Interface0DIterator object.\n"
"   :type it: :class:`freestyle.types.Interface0DIterator`\n"
"   :return: The quantitative invisibility of the pointed Interface0D.\n"
"   :rtype: int\n";

static int QuantitativeInvisibilityF0D___init__(BPy_QuantitativeInvisibilityF0D *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist))
		return -1;
	self->py_uf0D_unsigned.uf0D_unsigned = new Functions0D::QuantitativeInvisibilityF0D();
	self->py_uf0D_unsigned.uf0D_unsigned->py_uf0D = (PyObject *)self;
	return 0;
}

/*-----------------------BPy_QuantitativeInvisibilityF0D type definition ------------------------------*/

PyTypeObject QuantitativeInvisibilityF0D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"QuantitativeInvisibilityF0D",  /* tp_name */
	sizeof(BPy_QuantitativeInvisibilityF0D), /* tp_basicsize */
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
	QuantitativeInvisibilityF0D___doc__, /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0DUnsigned_Type,  /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)QuantitativeInvisibilityF0D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
