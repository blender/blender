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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/freestyle/intern/python/BPy_ContextFunctions.cpp
 *  \ingroup freestyle
 */

#include "BPy_ContextFunctions.h"
#include "BPy_Convert.h"

#include "../stroke/ContextFunctions.h"

#ifdef __cplusplus
extern "C" {
#endif
  
///////////////////////////////////////////////////////////////////////////////////////////

//------------------------ MODULE FUNCTIONS ----------------------------------

static char ContextFunctions_GetTimeStampCF___doc__[] =
".. function:: GetTimeStampCF()\n"
"\n"
"   Returns the system time stamp.\n"
"\n"
"   :return: The system time stamp.\n"
"   :rtype: int\n";

static PyObject *
ContextFunctions_GetTimeStampCF( PyObject* self )
{
  return PyLong_FromLong( ContextFunctions::GetTimeStampCF() );
}

static char ContextFunctions_GetCanvasWidthCF___doc__[] =
".. method:: GetCanvasWidthCF()\n"
"\n"
"   Returns the canvas width.\n"
"\n"
"   :return: The canvas width.\n"
"   :rtype: int\n";

static PyObject *
ContextFunctions_GetCanvasWidthCF( PyObject* self )
{
  return PyLong_FromLong( ContextFunctions::GetCanvasWidthCF() );
}

static char ContextFunctions_GetCanvasHeightCF___doc__[] =
".. method:: GetCanvasHeightCF()\n"
"\n"
"   Returns the canvas height.\n"
"\n"
"   :return: The canvas height.\n"
"   :rtype: int\n";

static PyObject *
ContextFunctions_GetCanvasHeightCF( PyObject* self )
{
  return PyLong_FromLong( ContextFunctions::GetCanvasHeightCF() );
}

static char ContextFunctions_LoadMapCF___doc__[] =
".. function:: LoadMapCF(iFileName, iMapName, iNbLevels=4, iSigma=1.0)\n"
"\n"
"   Loads an image map for further reading.\n"
"\n"
"   :arg iFileName: The name of the image file.\n"
"   :type iFileName: str\n"
"   :arg iMapName: The name that will be used to access this image.\n"
"   :type iMapName: str\n"
"   :arg iNbLevels: The number of levels in the map pyramid\n"
"      (default = 4).  If iNbLevels == 0, the complete pyramid is\n"
"      built.\n"
"   :type iNbLevels: int\n"
"   :arg iSigma: The sigma value of the gaussian function.\n"
"   :type iSigma: float\n";

static PyObject *
ContextFunctions_LoadMapCF( PyObject *self, PyObject *args )
{
  char *fileName, *mapName;
  unsigned nbLevels;
  float sigma;

  if( !PyArg_ParseTuple(args, "ssIf", &fileName, &mapName, &nbLevels, &sigma) )
    return NULL;

  ContextFunctions::LoadMapCF(fileName, mapName, nbLevels, sigma);

  Py_RETURN_NONE;
}

static char ContextFunctions_ReadMapPixelCF___doc__[] =
".. function:: ReadMapPixelCF(iMapName, level, x, y)\n"
"\n"
"   Reads a pixel in a user-defined map.\n"
"\n"
"   :arg iMapName: The name of the map.\n"
"   :type iMapName: str\n"
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
ContextFunctions_ReadMapPixelCF( PyObject *self, PyObject *args )
{
  char *mapName;
  int level;
  unsigned x, y;

  if( !PyArg_ParseTuple(args, "siII", &mapName, &level, &x, &y) )
    return NULL;

  float f = ContextFunctions::ReadMapPixelCF(mapName, level, x, y);

  return PyFloat_FromDouble( f );
}

static char ContextFunctions_ReadCompleteViewMapPixelCF___doc__[] =
".. function:: ReadCompleteViewMapPixelCF(level, x, y)\n"
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
ContextFunctions_ReadCompleteViewMapPixelCF( PyObject *self, PyObject *args )
{
  int level;
  unsigned x, y;

  if( !PyArg_ParseTuple(args, "iII", &level, &x, &y) )
    return NULL;

  float f = ContextFunctions::ReadCompleteViewMapPixelCF(level, x, y);

  return PyFloat_FromDouble( f );
}

static char ContextFunctions_ReadDirectionalViewMapPixelCF___doc__[] =
".. function:: ReadDirectionalViewMapPixelCF(iOrientation, level, x, y)\n"
"\n"
"   Reads a pixel in one of the oriented view map images.\n"
"\n"
"   :arg iOrientation: The number telling which orientation we want to\n"
"      check.\n"
"   :type iOrientation: int\n"
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
ContextFunctions_ReadDirectionalViewMapPixelCF( PyObject *self, PyObject *args )
{
  int orientation, level;
  unsigned x, y;

  if( !PyArg_ParseTuple(args, "iiII", &orientation, &level, &x, &y) )
    return NULL;

  float f = ContextFunctions::ReadDirectionalViewMapPixelCF(orientation, level, x, y);

  return PyFloat_FromDouble( f );
}

static char ContextFunctions_GetSelectedFEdgeCF___doc__[] =
".. function:: GetSelectedFEdgeCF()\n"
"\n"
"   Returns the selected FEdge.\n"
"\n"
"   :return: The selected FEdge.\n"
"   :rtype: :class:`FEdge`\n";

static PyObject *
ContextFunctions_GetSelectedFEdgeCF( PyObject *self )
{
  FEdge *fe = ContextFunctions::GetSelectedFEdgeCF();
  if( fe )
    return Any_BPy_FEdge_from_FEdge( *fe );

  Py_RETURN_NONE;
}

/*-----------------------ContextFunctions module docstring-------------------------------*/

static char module_docstring[] = "The Blender Freestyle.ContextFunctions submodule\n\n";

/*-----------------------ContextFunctions module functions definitions-------------------*/

static PyMethodDef module_functions[] = {
  {"GetTimeStampCF", (PyCFunction)ContextFunctions_GetTimeStampCF, METH_NOARGS, ContextFunctions_GetTimeStampCF___doc__},
  {"GetCanvasWidthCF", (PyCFunction)ContextFunctions_GetCanvasWidthCF, METH_NOARGS, ContextFunctions_GetCanvasWidthCF___doc__},
  {"GetCanvasHeightCF", (PyCFunction)ContextFunctions_GetCanvasHeightCF, METH_NOARGS, ContextFunctions_GetCanvasHeightCF___doc__},
  {"LoadMapCF", (PyCFunction)ContextFunctions_LoadMapCF, METH_VARARGS, ContextFunctions_LoadMapCF___doc__},
  {"ReadMapPixelCF", (PyCFunction)ContextFunctions_ReadMapPixelCF, METH_VARARGS, ContextFunctions_ReadMapPixelCF___doc__},
  {"ReadCompleteViewMapPixelCF", (PyCFunction)ContextFunctions_ReadCompleteViewMapPixelCF, METH_VARARGS, ContextFunctions_ReadCompleteViewMapPixelCF___doc__},
  {"ReadDirectionalViewMapPixelCF", (PyCFunction)ContextFunctions_ReadDirectionalViewMapPixelCF, METH_VARARGS, ContextFunctions_ReadDirectionalViewMapPixelCF___doc__},
  {"GetSelectedFEdgeCF", (PyCFunction)ContextFunctions_GetSelectedFEdgeCF, METH_NOARGS, ContextFunctions_GetSelectedFEdgeCF___doc__},
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

int ContextFunctions_Init( PyObject *module )
{	
  PyObject *m, *d, *f;

  if( module == NULL )
    return -1;

  m = PyModule_Create(&module_definition);
  if (m == NULL)
    return -1;
  Py_INCREF(m);
  PyModule_AddObject(module, "ContextFunctions", m);

  // from ContextFunctions import *
  d = PyModule_GetDict(m);
  for (PyMethodDef *p = module_functions; p->ml_name; p++) {
	f = PyDict_GetItemString(d, p->ml_name);
	Py_INCREF(f);
	PyModule_AddObject(module, p->ml_name, f);
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
