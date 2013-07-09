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

/** \file source/blender/freestyle/intern/python/StrokeShader/BPy_TextureAssignerShader.cpp
 *  \ingroup freestyle
 */

#include "BPy_TextureAssignerShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char TextureAssignerShader___doc__[] =
"Class hierarchy: :class:`StrokeShader` > :class:`TextureAssignerShader`\n"
"\n"
"[Texture shader]\n"
"\n"
".. method:: __init__(preset)\n"
"\n"
"   Builds a TextureAssignerShader object.\n"
"\n"
"   :arg preset: The preset number to use.\n"
"   :type preset: int\n"
"\n"
".. method:: shade(stroke)\n"
"\n"
"   Assigns a texture to the stroke in order to simulate its marks\n"
"   system.  This shader takes as input an integer value telling which\n"
"   texture and blending mode to use among a set of predefined\n"
"   textures.  Here are the different presets:\n"
"\n"
"   * 0: `/brushes/charcoalAlpha.bmp`, `Stroke.HUMID_MEDIUM`\n"
"   * 1: `/brushes/washbrushAlpha.bmp`, `Stroke.HUMID_MEDIUM`\n"
"   * 2: `/brushes/oil.bmp`, `Stroke.HUMID_MEDIUM`\n"
"   * 3: `/brushes/oilnoblend.bmp`, `Stroke.HUMID_MEDIUM`\n"
"   * 4: `/brushes/charcoalAlpha.bmp`, `Stroke.DRY_MEDIUM`\n"
"   * 5: `/brushes/washbrushAlpha.bmp`, `Stroke.DRY_MEDIUM`\n"
"   * 6: `/brushes/opaqueDryBrushAlpha.bmp`, `Stroke.OPAQUE_MEDIUM`\n"
"   * 7: `/brushes/opaqueBrushAlpha.bmp`, `Stroke.OPAQUE_MEDIUM`\n"
"\n"
"   Any other value will lead to the following preset:\n"
"\n"
"   * Default: `/brushes/smoothAlpha.bmp`, `Stroke.OPAQUE_MEDIUM`\n"
"\n"
"   :arg stroke: A Stroke object.\n"
"   :type stroke: :class:`Stroke`\n";

static int TextureAssignerShader___init__(BPy_TextureAssignerShader *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"preset", NULL};
	int i;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", (char **)kwlist, &i))
		return -1;
	self->py_ss.ss = new StrokeShaders::TextureAssignerShader(i);
	return 0;
}

/*-----------------------BPy_TextureAssignerShader type definition ------------------------------*/

PyTypeObject TextureAssignerShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"TextureAssignerShader",        /* tp_name */
	sizeof(BPy_TextureAssignerShader), /* tp_basicsize */
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
	TextureAssignerShader___doc__,  /* tp_doc */
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
	(initproc)TextureAssignerShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
