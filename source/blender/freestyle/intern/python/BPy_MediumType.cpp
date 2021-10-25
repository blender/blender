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

/** \file source/blender/freestyle/intern/python/BPy_MediumType.cpp
 *  \ingroup freestyle
 */

#include "BPy_MediumType.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*-----------------------BPy_MediumType type definition ------------------------------*/

PyDoc_STRVAR(MediumType_doc,
"Class hierarchy: int > :class:`MediumType`\n"
"\n"
"The different blending modes available to similate the interaction\n"
"media-medium:\n"
"\n"
"* Stroke.DRY_MEDIUM: To simulate a dry medium such as Pencil or Charcoal.\n"
"* Stroke.HUMID_MEDIUM: To simulate ink painting (color substraction blending).\n"
"* Stroke.OPAQUE_MEDIUM: To simulate an opaque medium (oil, spray...).");

PyTypeObject MediumType_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"MediumType",                   /* tp_name */
	sizeof(PyLongObject),           /* tp_basicsize */
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
	Py_TPFLAGS_DEFAULT,             /* tp_flags */
	MediumType_doc,                 /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&PyLong_Type,                   /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	0,                              /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

/*-----------------------BPy_IntegrationType instance definitions -------------------------*/

PyLongObject _BPy_MediumType_DRY_MEDIUM = {
	PyVarObject_HEAD_INIT(&MediumType_Type, 1)
	{ Stroke::DRY_MEDIUM }
};
PyLongObject _BPy_MediumType_HUMID_MEDIUM = {
	PyVarObject_HEAD_INIT(&MediumType_Type, 1)
	{ Stroke::HUMID_MEDIUM }
};
PyLongObject _BPy_MediumType_OPAQUE_MEDIUM = {
	PyVarObject_HEAD_INIT(&MediumType_Type, 1)
	{ Stroke::OPAQUE_MEDIUM }
};

//-------------------MODULE INITIALIZATION--------------------------------

int MediumType_Init(PyObject *module)
{	
	if (module == NULL)
		return -1;

	if (PyType_Ready(&MediumType_Type) < 0)
		return -1;
	Py_INCREF(&MediumType_Type);
	PyModule_AddObject(module, "MediumType", (PyObject *)&MediumType_Type);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
