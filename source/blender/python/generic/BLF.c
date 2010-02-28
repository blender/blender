/**
 * $Id:
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
#include "BLF.h"

#include "../../blenfont/BLF_api.h"

static char py_blf_position_doc[] =
".. function:: position(x, y, z)\n"
"\n"
"   Set the position for drawing text.";

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

/*----------------------------MODULE INIT-------------------------*/
struct PyMethodDef BLF_methods[] = {
	{"position", (PyCFunction)py_blf_position, METH_VARARGS, py_blf_position_doc},
	{"size", (PyCFunction) py_blf_size, METH_VARARGS, py_blf_size_doc},
	{"aspect", (PyCFunction) py_blf_aspect, METH_VARARGS, py_blf_aspect_doc},
	{"blur", (PyCFunction) py_blf_blur, METH_VARARGS, py_blf_blur_doc},

	{"draw", (PyCFunction) py_blf_draw, METH_VARARGS, py_blf_draw_doc},

	{"dimensions", (PyCFunction) py_blf_dimensions, METH_VARARGS, py_blf_dimensions_doc},
	{NULL, NULL, 0, NULL}
};

static char BLF_doc[] =
"This module provides access to blenders text drawing functions.\n";

static struct PyModuleDef BLF_module_def = {
	PyModuleDef_HEAD_INIT,
	"BLF",  /* m_name */
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

	return (submodule);
}
