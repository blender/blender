/**
 * $Id$
 *
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <Python.h>
#include "blf_api.h"

#include "../../blenfont/BLF_api.h"

static char py_blf_position_doc[] =
".. function:: position(x, y, z)\n"
"\n"
"   Set the position for drawing text.\n";

static PyObject *py_blf_position(PyObject *self, PyObject *args)
{
	float x, y, z;

	if (!PyArg_ParseTuple(args, "fff:BLF.position", &x, &y, &z))
		return NULL;

	BLF_position(x, y, z);

	Py_RETURN_NONE;
}


static char py_blf_size_doc[] =
".. function:: size(size, dpi)\n"
"\n"
"   Set the size and dpi for drawing text.\n"
"\n"
"   :arg size: Point size of the font.\n"
"   :type size: int\n"
"   :arg dpi: dots per inch value to use for drawing.\n"
"   :type dpi: int\n";

static PyObject *py_blf_size(PyObject *self, PyObject *args)
{
	int size, dpi;

	if (!PyArg_ParseTuple(args, "ii:BLF.size", &size, &dpi))
		return NULL;

	BLF_size(size, dpi);

	Py_RETURN_NONE;
}


static char py_blf_aspect_doc[] =
".. function:: aspect(aspect)\n"
"\n"
"   Set the aspect for drawing text.\n"
"\n"
"   :arg aspect: The aspect ratio for text drawing to use.\n"
"   :type aspect: float\n";

static PyObject *py_blf_aspect(PyObject *self, PyObject *args)
{
	float aspect;

	if (!PyArg_ParseTuple(args, "f:BLF.aspect", &aspect))
		return NULL;

	BLF_aspect(aspect);

	Py_RETURN_NONE;
}


static char py_blf_blur_doc[] =
".. function:: blur(radius)\n"
"\n"
"   Set the blur radius for drawing text.\n"
"\n"
"   :arg radius: The radius for blurring text (in pixels).\n"
"   :type radius: int\n";

static PyObject *py_blf_blur(PyObject *self, PyObject *args)
{
	int blur;

	if (!PyArg_ParseTuple(args, "i:BLF.blur", &blur))
		return NULL;

	BLF_blur(blur);

	Py_RETURN_NONE;
}


static char py_blf_draw_doc[] =
".. function:: draw(text)\n"
"\n"
"   Draw text in the current context.\n"
"\n"
"   :arg text: the text to draw.\n"
"   :type text: string\n";

static PyObject *py_blf_draw(PyObject *self, PyObject *args)
{
	char *text;

	if (!PyArg_ParseTuple(args, "s:BLF.draw", &text))
		return NULL;

	BLF_draw(text);

	Py_RETURN_NONE;
}

static char py_blf_dimensions_doc[] =
".. function:: dimensions(text)\n"
"\n"
"   Return the width and hight of the text.\n"
"\n"
"   :arg text: the text to draw.\n"
"   :type text: string\n"
"   :return: the width and height of the text.\n"
"   :rtype: tuple of 2 floats\n";

static PyObject *py_blf_dimensions(PyObject *self, PyObject *args)
{
	char *text;
	float r_width, r_height;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "s:BLF.dimensions", &text))
		return NULL;

	BLF_width_and_height(text, &r_width, &r_height);

	ret= PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, PyFloat_FromDouble(r_width));
	PyTuple_SET_ITEM(ret, 1, PyFloat_FromDouble(r_height));
	return ret;
}

static char py_blf_clipping_doc[] =
".. function:: clipping(xmin, ymin, xmax, ymax)\n"
"\n"
"   Set the clipping, enable/disable using CLIPPING.\n";

static PyObject *py_blf_clipping(PyObject *self, PyObject *args)
{
	float xmin, ymin, xmax, ymax;

	if (!PyArg_ParseTuple(args, "ffff:BLF.clipping", &xmin, &ymin, &xmax, &ymax))
		return NULL;

	BLF_clipping(xmin, ymin, xmax, ymax);

	Py_RETURN_NONE;
}

static char py_blf_disable_doc[] =
".. function:: disable(option)\n"
"\n"
"   Disable option.\n"
"\n"
"   :arg option: One of ROTATION, CLIPPING, SHADOW or KERNING_DEFAULT.\n"
"   :type option: int\n";

static PyObject *py_blf_disable(PyObject *self, PyObject *args)
{
	int option;

	if (!PyArg_ParseTuple(args, "i:BLF.disable", &option))
		return NULL;

	BLF_disable(option);

	Py_RETURN_NONE;
}

static char py_blf_enable_doc[] =
".. function:: enable(option)\n"
"\n"
"   Enable option.\n"
"\n"
"   :arg option: One of ROTATION, CLIPPING, SHADOW or KERNING_DEFAULT.\n"
"   :type option: int\n";

static PyObject *py_blf_enable(PyObject *self, PyObject *args)
{
	int option;

	if (!PyArg_ParseTuple(args, "i:BLF.enable", &option))
		return NULL;

	BLF_enable(option);

	Py_RETURN_NONE;
}

static char py_blf_rotation_doc[] =
".. function:: rotation(angle)\n"
"\n"
"   Set the text rotation angle, enable/disable using ROTATION.\n"
"\n"
"   :arg angle: The angle for text drawing to use.\n"
"   :type aspect: float\n";

static PyObject *py_blf_rotation(PyObject *self, PyObject *args)
{
	float angle;

	if (!PyArg_ParseTuple(args, "f:BLF.rotation", &angle))
		return NULL;
		
	BLF_rotation(angle);

	Py_RETURN_NONE;
}

static char py_blf_shadow_doc[] =
".. function:: shadow(level, r, g, b, a)\n"
"\n"
"   Shadow options, enable/disable using SHADOW .\n"
"\n"
"   :arg level: The blur level, can be 3, 5 or 0.\n"
"   :type level: int\n";

static PyObject *py_blf_shadow(PyObject *self, PyObject *args)
{
	int level;
	float r, g, b, a;

	if (!PyArg_ParseTuple(args, "iffff:BLF.shadow", &level, &r, &g, &b, &a))
		return NULL;

	if (level != 0 && level != 3 && level != 5) {
		PyErr_SetString(PyExc_TypeError, "blf.shadow expected arg to be in (0, 3, 5)");
		return NULL;
	}

	BLF_shadow(level, r, g, b, a);

	Py_RETURN_NONE;
}

static char py_blf_shadow_offset_doc[] =
".. function:: shadow_offset(x, y)\n"
"\n"
"   Set the offset for shadow text.\n";

static PyObject *py_blf_shadow_offset(PyObject *self, PyObject *args)
{
	int x, y;

	if (!PyArg_ParseTuple(args, "ii:BLF.shadow_offset", &x, &y))
		return NULL;

	BLF_shadow_offset(x, y);

	Py_RETURN_NONE;
}

/*----------------------------MODULE INIT-------------------------*/
struct PyMethodDef BLF_methods[] = {
	{"aspect", (PyCFunction) py_blf_aspect, METH_VARARGS, py_blf_aspect_doc},
	{"blur", (PyCFunction) py_blf_blur, METH_VARARGS, py_blf_blur_doc},
	{"clipping", (PyCFunction) py_blf_clipping, METH_VARARGS, py_blf_clipping_doc},
	{"disable", (PyCFunction) py_blf_disable, METH_VARARGS, py_blf_disable_doc},
	{"dimensions", (PyCFunction) py_blf_dimensions, METH_VARARGS, py_blf_dimensions_doc},
	{"draw", (PyCFunction) py_blf_draw, METH_VARARGS, py_blf_draw_doc},
	{"enable", (PyCFunction) py_blf_enable, METH_VARARGS, py_blf_enable_doc},
	{"position", (PyCFunction)py_blf_position, METH_VARARGS, py_blf_position_doc},
	{"rotation", (PyCFunction) py_blf_rotation, METH_VARARGS, py_blf_rotation_doc},
	{"shadow", (PyCFunction) py_blf_shadow, METH_VARARGS, py_blf_shadow_doc},
	{"shadow_offset", (PyCFunction) py_blf_shadow_offset, METH_VARARGS, py_blf_shadow_offset_doc},
	{"size", (PyCFunction) py_blf_size, METH_VARARGS, py_blf_size_doc},
	{NULL, NULL, 0, NULL}
};

static char BLF_doc[] =
"This module provides access to blenders text drawing functions.\n";

static struct PyModuleDef BLF_module_def = {
	PyModuleDef_HEAD_INIT,
	"blf",  /* m_name */
	BLF_doc,  /* m_doc */
	0,  /* m_size */
	BLF_methods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

PyObject *BLF_Init(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BLF_module_def);
	PyDict_SetItemString(PySys_GetObject("modules"), BLF_module_def.m_name, submodule);

	PyModule_AddIntConstant(submodule, "ROTATION", BLF_ROTATION);
	PyModule_AddIntConstant(submodule, "CLIPPING", BLF_CLIPPING);
	PyModule_AddIntConstant(submodule, "SHADOW", BLF_SHADOW);
	PyModule_AddIntConstant(submodule, "KERNING_DEFAULT", BLF_KERNING_DEFAULT);

	return (submodule);
}
