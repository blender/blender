/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

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

#include <PyObjectPlus.h>

#include <RAS_GLExtensionManager.h>

#include <RAS_IPolygonMaterial.h>

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
	if (!PyArg_ParseTuple(args, "Os:materialID", &obj, &matName))
		return NULL;
	// get material id
	short matID = getMaterialID(obj, matName);
	// if material was not found, report errot
	if (matID < 0)
	{
		PyErr_SetString(PyExc_RuntimeError, "VideoTexture.materialID(ob, string): Object doesn't have material with given name");
		return NULL;
	}
	// return material ID
	return Py_BuildValue("h", matID);
}


// get last error description
static PyObject * getLastError (PyObject *self, PyObject *args)
{
	return PyUnicode_FromString(Exception::m_lastError.c_str());
}

// set log file
static PyObject * setLogFile (PyObject *self, PyObject *args)
{
	// get parameters
	if (!PyArg_ParseTuple(args, "s:setLogFile", &Exception::m_logFile))
		return Py_BuildValue("i", -1);
	// log file was loaded
	return Py_BuildValue("i", 0);
}


// image to numpy array
static PyObject * imageToArray (PyObject * self, PyObject *args)
{
	// parameter is Image object
	PyObject * pyImg;
	char *mode = NULL;
	if (!PyArg_ParseTuple(args, "O|s:imageToArray", &pyImg, &mode) || !pyImageTypes.in(pyImg->ob_type))
	{
		// if object is incorect, report error
		PyErr_SetString(PyExc_TypeError, "VideoTexture.imageToArray(image): The value must be a image source object");
		return NULL;
	}
	// get image structure
	PyImage * img = reinterpret_cast<PyImage*>(pyImg);
	return Image_getImage(img, mode);
}


// metody modulu
static PyMethodDef moduleMethods[] =
{
	{"materialID", getMaterialID, METH_VARARGS, "Gets object's Blender Material ID"},
	{"getLastError", getLastError, METH_NOARGS, "Gets last error description"},
	{"setLogFile", setLogFile, METH_VARARGS, "Sets log file name"},
	{"imageToArray", imageToArray, METH_VARARGS, "get buffer from image source, color channels are selectable"},
	{NULL}  /* Sentinel */
};

#if WITH_FFMPEG
extern PyTypeObject VideoFFmpegType;
extern PyTypeObject ImageFFmpegType;
#endif
extern PyTypeObject FilterBlueScreenType;
extern PyTypeObject FilterGrayType;
extern PyTypeObject FilterColorType;
extern PyTypeObject FilterLevelType;
extern PyTypeObject FilterNormalType;
extern PyTypeObject FilterRGB24Type;
extern PyTypeObject FilterRGBA32Type;
extern PyTypeObject FilterBGR24Type;
extern PyTypeObject ImageBuffType;
extern PyTypeObject ImageMixType;
extern PyTypeObject ImageRenderType;
extern PyTypeObject ImageMirrorType;
extern PyTypeObject ImageViewportType;
extern PyTypeObject ImageViewportType;


static void registerAllTypes(void)
{
#if WITH_FFMPEG
	pyImageTypes.add(&VideoFFmpegType, "VideoFFmpeg");
	pyImageTypes.add(&ImageFFmpegType, "ImageFFmpeg");
#endif
	pyImageTypes.add(&ImageBuffType, "ImageBuff");
	pyImageTypes.add(&ImageMixType, "ImageMix");
	pyImageTypes.add(&ImageRenderType, "ImageRender");
	pyImageTypes.add(&ImageMirrorType, "ImageMirror");
	pyImageTypes.add(&ImageViewportType, "ImageViewport");

	pyFilterTypes.add(&FilterBlueScreenType, "FilterBlueScreen");
	pyFilterTypes.add(&FilterGrayType, "FilterGray");
	pyFilterTypes.add(&FilterColorType, "FilterColor");
	pyFilterTypes.add(&FilterLevelType, "FilterLevel");
	pyFilterTypes.add(&FilterNormalType, "FilterNormal");
	pyFilterTypes.add(&FilterRGB24Type, "FilterRGB24");
	pyFilterTypes.add(&FilterRGBA32Type, "FilterRGBA32");
	pyFilterTypes.add(&FilterBGR24Type, "FilterBGR24");
}

static struct PyModuleDef VideoTexture_module_def = {
	{}, /* m_base */
	"VideoTexture",  /* m_name */
	"Module that allows to play video files on textures in GameBlender.",  /* m_doc */
	0,  /* m_size */
	moduleMethods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

PyObject* initVideoTexture(void) 
{
	PyObject * m;
	
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

	/* Use existing module where possible
	 * be careful not to init any runtime vars after this */
	m = PyImport_ImportModule( "VideoTexture" );
	if(m) {
		Py_DECREF(m);
		return m;
	}
	else {
		PyErr_Clear();
	
		m = PyModule_Create(&VideoTexture_module_def);
		PyDict_SetItemString(PySys_GetObject("modules"), VideoTexture_module_def.m_name, m);
	}
	
	if (m == NULL) 
		return NULL;

	// initialize classes
	pyImageTypes.reg(m);
	pyFilterTypes.reg(m);

	Py_INCREF(&TextureType);
	PyModule_AddObject(m, (char*)"Texture", (PyObject*)&TextureType);
	
	// init last error description
	Exception::m_lastError[0] = '\0';
	
	return m;
}

// registration to Image types, put here because of silly linker bug
