/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexure library

Copyright (c) 2006 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

#define PY_ARRAY_UNIQUE_SYMBOL numpyPtr

#include <Python.h>

#include <RAS_GLExtensionManager.h>

#include <RAS_IPolygonMaterial.h>

#include <numpy/arrayobject.h>

//Old API
//#include "TexPlayer.h"
//#include "TexImage.h"
//#include "TexFrameBuff.h"

//#include "TexPlayerGL.h"

#include "ImageBase.h"
#include "FilterBase.h"
#include "Texture.h"

#include "Exception.h"


// get material id
static PyObject * getMaterialID (PyObject *self, PyObject *args)
{
	// parameters - game object with video texture
	PyObject * obj = NULL;
	// material name
	char * matName;

	// get parameters
	if (!PyArg_ParseTuple(args, "Os", &obj, &matName))
		return NULL;
	// get material id
	short matID = getMaterialID(obj, matName);
	// if material was not found, report errot
	if (matID < 0)
	{
		PyErr_SetString(PyExc_RuntimeError, "object doesn't have material with given name");
		return NULL;
	}
	// return material ID
	return Py_BuildValue("h", matID);
}


// get last error description
static PyObject * getLastError (PyObject *self, PyObject *args)
{
	return Py_BuildValue("s", Exception::m_lastError.c_str());
}

// set log file
static PyObject * setLogFile (PyObject *self, PyObject *args)
{
	// get parameters
	if (!PyArg_ParseTuple(args, "s", &Exception::m_logFile))
		return Py_BuildValue("i", -1);
	// log file was loaded
	return Py_BuildValue("i", 0);
}


// function to initialize numpy structures
static bool initNumpy (void)
{
	// init module and report failure
	import_array1(false);
	// report success
	return true;
}

// image to numpy array
static PyObject * imageToArray (PyObject * self, PyObject *args)
{
	// parameter is Image object
	PyObject * pyImg;
	if (!PyArg_ParseTuple(args, "O", &pyImg) || !pyImageTypes.in(pyImg->ob_type))
	{
		// if object is incorect, report error
		PyErr_SetString(PyExc_TypeError, "The value must be a image source object");
		return NULL;
	}
	// get image structure
	PyImage * img = reinterpret_cast<PyImage*>(pyImg);
	// check initialization of numpy interface, and initialize it if needed
	if (numpyPtr == NULL && !initNumpy()) Py_RETURN_NONE;
	// create array object
	npy_intp dim[1];
	dim[0] = img->m_image->getBuffSize() / sizeof(unsigned int);
	unsigned int * imgBuff = img->m_image->getImage();
	// if image is available, convert it to array
	if (imgBuff != NULL)
		return PyArray_SimpleNewFromData(1, dim, NPY_UBYTE, imgBuff);
	// otherwise return None
	Py_RETURN_NONE;
}


// metody modulu
static PyMethodDef moduleMethods[] =
{
	{"materialID", getMaterialID, METH_VARARGS, "Gets object's Blender Material ID"},
	{"getLastError", getLastError, METH_NOARGS, "Gets last error description"},
	{"setLogFile", setLogFile, METH_VARARGS, "Sets log file name"},
	{"imageToArray", imageToArray, METH_VARARGS, "get array from image source"},
	{NULL}  /* Sentinel */
};

#if WITH_FFMPEG
extern PyTypeObject VideoFFmpegType;
#endif
extern PyTypeObject FilterBlueScreenType;
extern PyTypeObject FilterGrayType;
extern PyTypeObject FilterColorType;
extern PyTypeObject FilterLevelType;
extern PyTypeObject FilterNormalType;
extern PyTypeObject FilterRGB24Type;
extern PyTypeObject FilterBGR24Type;
extern PyTypeObject ImageBuffType;
extern PyTypeObject ImageMixType;
extern PyTypeObject ImageRenderType;
extern PyTypeObject ImageViewportType;
extern PyTypeObject ImageViewportType;


static void registerAllTypes(void)
{
#if WITH_FFMPEG
	pyImageTypes.add(&VideoFFmpegType, "VideoFFmpeg");
#endif
	pyImageTypes.add(&ImageBuffType, "ImageBuff");
	pyImageTypes.add(&ImageMixType, "ImageMix");
	//pyImageTypes.add(&ImageRenderType, "ImageRender");
	pyImageTypes.add(&ImageViewportType, "ImageViewport");

	pyFilterTypes.add(&FilterBlueScreenType, "FilterBlueScreen");
	pyFilterTypes.add(&FilterGrayType, "FilterGray");
	pyFilterTypes.add(&FilterColorType, "FilterColor");
	pyFilterTypes.add(&FilterLevelType, "FilterLevel");
	pyFilterTypes.add(&FilterNormalType, "FilterNormal");
	pyFilterTypes.add(&FilterRGB24Type, "FilterRGB24");
	pyFilterTypes.add(&FilterBGR24Type, "FilterBGR24");
}

PyObject* initVideoTexture(void) 
{
	// initialize GL extensions
	//bgl::InitExtensions(0);

	// prepare classes
	registerAllTypes();
    registerAllExceptions();

	if (!pyImageTypes.ready()) 
		return NULL;
	if (!pyFilterTypes.ready()) 
		return NULL;
	if (PyType_Ready(&TextureType) < 0) 
		return NULL;

	PyObject * m = Py_InitModule4("VideoTexture", moduleMethods,
		"Module that allows to play video files on textures in GameBlender.",
		(PyObject*)NULL,PYTHON_API_VERSION);
	if (m == NULL) 
		return NULL;

	// prepare numpy array
	numpyPtr = NULL;

	// initialize classes
	pyImageTypes.reg(m);
	pyFilterTypes.reg(m);

	Py_INCREF(&TextureType);
	PyModule_AddObject(m, "Texture", (PyObject*)&TextureType);

	// init last error description
	Exception::m_lastError[0] = '\0';
	return m;
}

// registration to Image types, put here because of silly linker bug
