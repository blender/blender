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

/** \file source/blender/freestyle/intern/python/BPy_ContextFunctions.cpp
 *  \ingroup freestyle
 */

#include "BPy_ContextFunctions.h"
#include "BPy_Convert.h"

#include "../stroke/ContextFunctions.h"

using namespace Freestyle;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------ MODULE FUNCTIONS ----------------------------------

static char ContextFunctions_get_time_stamp___doc__[] =
".. function:: get_time_stamp()\n"
"\n"
"   Returns the system time stamp.\n"
"\n"
"   :return: The system time stamp.\n"
"   :rtype: int\n";

static PyObject *
ContextFunctions_get_time_stamp(PyObject *self)
{
	return PyLong_FromLong(ContextFunctions::GetTimeStampCF());
}

static char ContextFunctions_get_canvas_width___doc__[] =
".. method:: get_canvas_width()\n"
"\n"
"   Returns the canvas width.\n"
"\n"
"   :return: The canvas width.\n"
"   :rtype: int\n";

static PyObject *
ContextFunctions_get_canvas_width(PyObject *self)
{
	return PyLong_FromLong(ContextFunctions::GetCanvasWidthCF());
}

static char ContextFunctions_get_canvas_height___doc__[] =
".. method:: get_canvas_height()\n"
"\n"
"   Returns the canvas height.\n"
"\n"
"   :return: The canvas height.\n"
"   :rtype: int\n";

static PyObject *
ContextFunctions_get_canvas_height(PyObject *self)
{
	return PyLong_FromLong(ContextFunctions::GetCanvasHeightCF());
}

static char ContextFunctions_get_border___doc__[] =
".. method:: get_border()\n"
"\n"
"   Returns the border.\n"
"\n"
"   :return: A tuple of 4 numbers (xmin, ymin, xmax, ymax).\n"
"   :rtype: tuple\n";

static PyObject *
ContextFunctions_get_border(PyObject *self)
{
	BBox<Vec2i> border(ContextFunctions::GetBorderCF());
	PyObject *v = PyTuple_New(4);
	PyTuple_SET_ITEM(v, 0, PyLong_FromLong(border.getMin().x()));
	PyTuple_SET_ITEM(v, 1, PyLong_FromLong(border.getMin().y()));
	PyTuple_SET_ITEM(v, 2, PyLong_FromLong(border.getMax().x()));
	PyTuple_SET_ITEM(v, 3, PyLong_FromLong(border.getMax().y()));
	return v;
}

static char ContextFunctions_load_map___doc__[] =
".. function:: load_map(file_name, map_name, num_levels=4, sigma=1.0)\n"
"\n"
"   Loads an image map for further reading.\n"
"\n"
"   :arg file_name: The name of the image file.\n"
"   :type file_name: str\n"
"   :arg map_name: The name that will be used to access this image.\n"
"   :type map_name: str\n"
"   :arg num_levels: The number of levels in the map pyramid\n"
"      (default = 4).  If num_levels == 0, the complete pyramid is\n"
"      built.\n"
"   :type num_levels: int\n"
"   :arg sigma: The sigma value of the gaussian function.\n"
"   :type sigma: float\n";

static PyObject *
ContextFunctions_load_map(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"file_name", "map_name", "num_levels", "sigma", NULL};
	char *fileName, *mapName;
	unsigned nbLevels = 4;
	float sigma = 1.0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|If", (char **)kwlist, &fileName, &mapName, &nbLevels, &sigma))
		return NULL;
	ContextFunctions::LoadMapCF(fileName, mapName, nbLevels, sigma);
	Py_RETURN_NONE;
}

static char ContextFunctions_read_map_pixel___doc__[] =
".. function:: read_map_pixel(map_name, level, x, y)\n"
"\n"
"   Reads a pixel in a user-defined map.\n"
"\n"
"   :arg map_name: The name of the map.\n"
"   :type map_name: str\n"
"   :arg level: The level of the pyramid in which we wish to read the\n"
"      pixel.\n"
"   :type level: int\n"
"   :arg x: The x coordinate of the pixel we wish to read.  The origin\n"
"      is in the lower-left corner.\n"
"   :type x: int\n"
"   :arg y: The y coordinate of the pixel we wish to read.  The origin\n"
"      is in the lower-left corner.\n"
"   :type y: int\n"
"   :return: The floating-point value stored for that pixel.\n"
"   :rtype: float\n";

static PyObject *
ContextFunctions_read_map_pixel(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"map_name", "level", "x", "y", NULL};
	char *mapName;
	int level;
	unsigned x, y;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "siII", (char **)kwlist, &mapName, &level, &x, &y))
		return NULL;
	return PyFloat_FromDouble(ContextFunctions::ReadMapPixelCF(mapName, level, x, y));
}

static char ContextFunctions_read_complete_view_map_pixel___doc__[] =
".. function:: read_complete_view_map_pixel(level, x, y)\n"
"\n"
"   Reads a pixel in the complete view map.\n"
"\n"
"   :arg level: The level of the pyramid in which we wish to read the\n"
"      pixel.\n"
"   :type level: int\n"
"   :arg x: The x coordinate of the pixel we wish to read.  The origin\n"
"      is in the lower-left corner.\n"
"   :type x: int\n"
"   :arg y: The y coordinate of the pixel we wish to read.  The origin\n"
"      is in the lower-left corner.\n"
"   :type y: int\n"
"   :return: The floating-point value stored for that pixel.\n"
"   :rtype: float\n";

static PyObject *
ContextFunctions_read_complete_view_map_pixel(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"level", "x", "y", NULL};
	int level;
	unsigned x, y;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iII", (char **)kwlist, &level, &x, &y))
		return NULL;
	return PyFloat_FromDouble(ContextFunctions::ReadCompleteViewMapPixelCF(level, x, y));
}

static char ContextFunctions_read_directional_view_map_pixel___doc__[] =
".. function:: read_directional_view_map_pixel(orientation, level, x, y)\n"
"\n"
"   Reads a pixel in one of the oriented view map images.\n"
"\n"
"   :arg orientation: The number telling which orientation we want to\n"
"      check.\n"
"   :type orientation: int\n"
"   :arg level: The level of the pyramid in which we wish to read the\n"
"      pixel.\n"
"   :type level: int\n"
"   :arg x: The x coordinate of the pixel we wish to read.  The origin\n"
"      is in the lower-left corner.\n"
"   :type x: int\n"
"   :arg y: The y coordinate of the pixel we wish to read.  The origin\n"
"      is in the lower-left corner.\n"
"   :type y: int\n"
"   :return: The floating-point value stored for that pixel.\n"
"   :rtype: float\n";

static PyObject *
ContextFunctions_read_directional_view_map_pixel(PyObject *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"orientation", "level", "x", "y", NULL};
	int orientation, level;
	unsigned x, y;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiII", (char **)kwlist, &orientation, &level, &x, &y))
		return NULL;
	return PyFloat_FromDouble(ContextFunctions::ReadDirectionalViewMapPixelCF(orientation, level, x, y));
}

static char ContextFunctions_get_selected_fedge___doc__[] =
".. function:: get_selected_fedge()\n"
"\n"
"   Returns the selected FEdge.\n"
"\n"
"   :return: The selected FEdge.\n"
"   :rtype: :class:`FEdge`\n";

static PyObject *
ContextFunctions_get_selected_fedge(PyObject *self)
{
	FEdge *fe = ContextFunctions::GetSelectedFEdgeCF();
	if (fe)
		return Any_BPy_FEdge_from_FEdge(*fe);
	Py_RETURN_NONE;
}

/*-----------------------ContextFunctions module docstring-------------------------------*/

static char module_docstring[] = "The Blender Freestyle.ContextFunctions submodule\n\n";

/*-----------------------ContextFunctions module functions definitions-------------------*/

static PyMethodDef module_functions[] = {
	{"get_time_stamp", (PyCFunction)ContextFunctions_get_time_stamp, METH_NOARGS,
	                   ContextFunctions_get_time_stamp___doc__},
	{"get_canvas_width", (PyCFunction)ContextFunctions_get_canvas_width, METH_NOARGS,
	                     ContextFunctions_get_canvas_width___doc__},
	{"get_canvas_height", (PyCFunction)ContextFunctions_get_canvas_height, METH_NOARGS,
	                      ContextFunctions_get_canvas_height___doc__},
	{"get_border", (PyCFunction)ContextFunctions_get_border, METH_NOARGS,
	               ContextFunctions_get_border___doc__},
	{"load_map", (PyCFunction)ContextFunctions_load_map, METH_VARARGS | METH_KEYWORDS,
	             ContextFunctions_load_map___doc__},
	{"read_map_pixel", (PyCFunction)ContextFunctions_read_map_pixel, METH_VARARGS | METH_KEYWORDS,
	                   ContextFunctions_read_map_pixel___doc__},
	{"read_complete_view_map_pixel", (PyCFunction)ContextFunctions_read_complete_view_map_pixel,
	                                 METH_VARARGS | METH_KEYWORDS,
	                                 ContextFunctions_read_complete_view_map_pixel___doc__},
	{"read_directional_view_map_pixel", (PyCFunction)ContextFunctions_read_directional_view_map_pixel,
	                                    METH_VARARGS | METH_KEYWORDS,
	                                    ContextFunctions_read_directional_view_map_pixel___doc__},
	{"get_selected_fedge", (PyCFunction)ContextFunctions_get_selected_fedge, METH_NOARGS,
	                       ContextFunctions_get_selected_fedge___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------ContextFunctions module definition--------------------------------*/

static PyModuleDef module_definition = {
    PyModuleDef_HEAD_INIT,
    "Freestyle.ContextFunctions",
    module_docstring,
    -1,
    module_functions
};

//------------------- MODULE INITIALIZATION --------------------------------

int ContextFunctions_Init(PyObject *module)
{
	PyObject *m;

	if (module == NULL)
		return -1;

	m = PyModule_Create(&module_definition);
	if (m == NULL)
		return -1;
	Py_INCREF(m);
	PyModule_AddObject(module, "ContextFunctions", m);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
