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

/** \file source/blender/freestyle/intern/python/StrokeShader/BPy_StrokeTextureShader.cpp
 *  \ingroup freestyle
 */

#include "BPy_StrokeTextureShader.h"

#include "../../stroke/BasicStrokeShaders.h"
#include "../BPy_Convert.h"
#include "../BPy_MediumType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char StrokeTextureShader___doc__[] =
"Class hierarchy: :class:`StrokeShader` > :class:`StrokeTextureShader`\n"
"\n"
"[Texture shader]\n"
"\n"
".. method:: __init__(texture_file, medium_type=Stroke.OPAQUE_MEDIUM, tips=False)\n"
"\n"
"   Builds a StrokeTextureShader object.\n"
"\n"
"   :arg texture_file: \n"
"   :type texture_file: str\n"
"   :arg medium_type: The medium type and therefore, the blending mode\n"
"      that must be used for the rendering of this stroke.\n"
"   :type medium_type: :class:`MediumType`\n"
"   :arg tips: Tells whether the texture includes tips or not.  If it\n"
"      is the case, the texture image must respect the following format.\n"
"   :type tips: bool\n"
"\n"
"   The format of a texture image including tips::\n"
"\n"
"       ___________\n"
"      |           |\n"
"      |     A     |\n"
"      |___________|\n"
"      |     |     |\n"
"      |  B  |  C  |\n"
"      |_____|_____|\n"
"\n"
"   * A : The stroke's corpus texture.\n"
"   * B : The stroke's left extremity texture.\n"
"   * C : The stroke's right extremity texture.\n"
"\n"
".. method:: shade(stroke)\n"
"\n"
"   Assigns a texture and a blending mode to the stroke in order to\n"
"   simulate its marks system.\n"
"\n"
"   :arg stroke: A Stroke object.\n"
"   :type stroke: :class:`Stroke`\n";

static int StrokeTextureShader___init__(BPy_StrokeTextureShader *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"texture_file", "medium_type", "tips", NULL};
	const char *s1;
	PyObject *obj2 = 0, *obj3 = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|O!O!", (char **)kwlist,
	                                 &s1, &MediumType_Type, &obj2, &PyBool_Type, &obj3))
	{
		return -1;
	}
	Stroke::MediumType mt = (!obj2) ? Stroke::OPAQUE_MEDIUM : MediumType_from_BPy_MediumType(obj2);
	bool b = (!obj3) ? false : bool_from_PyBool(obj3);
	self->py_ss.ss = new StrokeShaders::StrokeTextureShader(s1, mt, b);
	return 0;
}

/*-----------------------BPy_StrokeTextureShader type definition ------------------------------*/

PyTypeObject StrokeTextureShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"StrokeTextureShader",          /* tp_name */
	sizeof(BPy_StrokeTextureShader), /* tp_basicsize */
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
	StrokeTextureShader___doc__,    /* tp_doc */
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
	(initproc)StrokeTextureShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
