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

/** \file source/blender/freestyle/intern/python/UnaryPredicate1D/BPy_DensityLowerThanUP1D.cpp
 *  \ingroup freestyle
 */

#include "BPy_DensityLowerThanUP1D.h"

#include "../../stroke/AdvancedPredicates1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char DensityLowerThanUP1D___doc__[] =
"Class hierarchy: :class:`freestyle.types.UnaryPredicate1D` > :class:`DensityLowerThanUP1D`\n"
"\n"
".. method:: __init__(threshold, sigma=2.0)\n"
"\n"
"   Builds a DensityLowerThanUP1D object.\n"
"\n"
"   :arg threshold: The value of the threshold density.  Any Interface1D\n"
"      having a density lower than this threshold will match.\n"
"   :type threshold: float\n"
"   :arg sigma: The sigma value defining the density evaluation window\n"
"      size used in the :class:`freestyle.functions.DensityF0D` functor.\n"
"   :type sigma: float\n"
"\n"
".. method:: __call__(inter)\n"
"\n"
"   Returns true if the density evaluated for the Interface1D is less\n"
"   than a user-defined density value.\n"
"\n"
"   :arg inter: An Interface1D object.\n"
"   :type inter: :class:`freestyle.types.Interface1D`\n"
"   :return: True if the density is lower than a threshold.\n"
"   :rtype: bool\n";

static int DensityLowerThanUP1D___init__(BPy_DensityLowerThanUP1D *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"threshold", "sigma", NULL};
	double d1, d2 = 2.0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "d|d", (char **)kwlist, &d1, &d2))
		return -1;
	self->py_up1D.up1D = new Predicates1D::DensityLowerThanUP1D(d1, d2);
	return 0;
}

/*-----------------------BPy_DensityLowerThanUP1D type definition ------------------------------*/

PyTypeObject DensityLowerThanUP1D_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"DensityLowerThanUP1D",         /* tp_name */
	sizeof(BPy_DensityLowerThanUP1D), /* tp_basicsize */
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
	DensityLowerThanUP1D___doc__,   /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryPredicate1D_Type,         /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)DensityLowerThanUP1D___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
