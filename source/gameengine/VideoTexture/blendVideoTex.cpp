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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2006 The Zdeno Ash Miklas
 *
 * This source file is part of VideoTexture library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/VideoTexture/blendVideoTex.cpp
 *  \ingroup bgevideotex
 */

#include "EXP_PyObjectPlus.h"

#include "KX_PythonInit.h"

#include <RAS_IPolygonMaterial.h>

//Old API
//#include "TexPlayer.h"
//#include "TexImage.h"
//#include "TexFrameBuff.h"

//#include "TexPlayerGL.h"

#include "ImageBase.h"
#include "VideoBase.h"
#include "FilterBase.h"
#include "Texture.h"

#include "Exception.h"

// access to IMB_BLEND_* constants
extern "C"
{
#include "IMB_imbuf.h"
};


// get material id
static PyObject *getMaterialID (PyObject *self, PyObject *args)
{
	// parameters - game object with video texture
	PyObject *obj = NULL;
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
static PyObject *getLastError (PyObject *self, PyObject *args)
{
	return PyUnicode_FromString(Exception::m_lastError.c_str());
}

// set log file
static PyObject *setLogFile (PyObject *self, PyObject *args)
{
	// get parameters
	if (!PyArg_ParseTuple(args, "s:setLogFile", &Exception::m_logFile))
		return Py_BuildValue("i", -1);
	// log file was loaded
	return Py_BuildValue("i", 0);
}


// image to numpy array
static PyObject *imageToArray(PyObject *self, PyObject *args)
{
	// parameter is Image object
	PyObject *pyImg;
	char *mode = NULL;
	if (!PyArg_ParseTuple(args, "O|s:imageToArray", &pyImg, &mode) || !pyImageTypes.in(Py_TYPE(pyImg)))
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

#ifdef WITH_FFMPEG
extern PyTypeObject VideoFFmpegType;
extern PyTypeObject ImageFFmpegType;
#endif
#ifdef WITH_GAMEENGINE_DECKLINK
extern PyTypeObject VideoDeckLinkType;
extern PyTypeObject DeckLinkType;
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

static void registerAllTypes(void)
{
#ifdef WITH_FFMPEG
	pyImageTypes.add(&VideoFFmpegType, "VideoFFmpeg");
	pyImageTypes.add(&ImageFFmpegType, "ImageFFmpeg");
#endif
#ifdef WITH_GAMEENGINE_DECKLINK
	pyImageTypes.add(&VideoDeckLinkType, "VideoDeckLink");
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

PyDoc_STRVAR(VideoTexture_module_documentation,
"Module that allows to play video files on textures in GameBlender."
);

static struct PyModuleDef VideoTexture_module_def = {
	PyModuleDef_HEAD_INIT,
	"VideoTexture",  /* m_name */
	VideoTexture_module_documentation,  /* m_doc */
	0,  /* m_size */
	moduleMethods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

PyMODINIT_FUNC initVideoTexturePythonBinding(void)
{
	PyObject *m;
	
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
#ifdef WITH_GAMEENGINE_DECKLINK
	if (PyType_Ready(&DeckLinkType) < 0)
		return NULL;
#endif

	m = PyModule_Create(&VideoTexture_module_def);
	PyDict_SetItemString(PySys_GetObject("modules"), VideoTexture_module_def.m_name, m);

	if (m == NULL) 
		return NULL;

	// initialize classes
	pyImageTypes.reg(m);
	pyFilterTypes.reg(m);

	Py_INCREF(&TextureType);
	PyModule_AddObject(m, "Texture", (PyObject *)&TextureType);
#ifdef WITH_GAMEENGINE_DECKLINK
	Py_INCREF(&DeckLinkType);
	PyModule_AddObject(m, "DeckLink", (PyObject *)&DeckLinkType);
#endif
	PyModule_AddIntConstant(m, "SOURCE_ERROR", SourceError);
	PyModule_AddIntConstant(m, "SOURCE_EMPTY", SourceEmpty);
	PyModule_AddIntConstant(m, "SOURCE_READY", SourceReady);
	PyModule_AddIntConstant(m, "SOURCE_PLAYING", SourcePlaying);
	PyModule_AddIntConstant(m, "SOURCE_STOPPED", SourceStopped);

	PyModule_AddIntConstant(m, "IMB_BLEND_MIX", IMB_BLEND_MIX);
	PyModule_AddIntConstant(m, "IMB_BLEND_ADD", IMB_BLEND_ADD);
	PyModule_AddIntConstant(m, "IMB_BLEND_SUB", IMB_BLEND_SUB);
	PyModule_AddIntConstant(m, "IMB_BLEND_MUL", IMB_BLEND_MUL);
	PyModule_AddIntConstant(m, "IMB_BLEND_LIGHTEN", IMB_BLEND_LIGHTEN);
	PyModule_AddIntConstant(m, "IMB_BLEND_DARKEN", IMB_BLEND_DARKEN);
	PyModule_AddIntConstant(m, "IMB_BLEND_ERASE_ALPHA", IMB_BLEND_ERASE_ALPHA);
	PyModule_AddIntConstant(m, "IMB_BLEND_ADD_ALPHA", IMB_BLEND_ADD_ALPHA);
	PyModule_AddIntConstant(m, "IMB_BLEND_OVERLAY", IMB_BLEND_OVERLAY);
	PyModule_AddIntConstant(m, "IMB_BLEND_HARDLIGHT", IMB_BLEND_HARDLIGHT);
	PyModule_AddIntConstant(m, "IMB_BLEND_COLORBURN", IMB_BLEND_COLORBURN);
	PyModule_AddIntConstant(m, "IMB_BLEND_LINEARBURN", IMB_BLEND_LINEARBURN);
	PyModule_AddIntConstant(m, "IMB_BLEND_COLORDODGE", IMB_BLEND_COLORDODGE);
	PyModule_AddIntConstant(m, "IMB_BLEND_SCREEN", IMB_BLEND_SCREEN);
	PyModule_AddIntConstant(m, "IMB_BLEND_SOFTLIGHT", IMB_BLEND_SOFTLIGHT);
	PyModule_AddIntConstant(m, "IMB_BLEND_PINLIGHT", IMB_BLEND_PINLIGHT);
	PyModule_AddIntConstant(m, "IMB_BLEND_VIVIDLIGHT", IMB_BLEND_VIVIDLIGHT);
	PyModule_AddIntConstant(m, "IMB_BLEND_LINEARLIGHT", IMB_BLEND_LINEARLIGHT);
	PyModule_AddIntConstant(m, "IMB_BLEND_DIFFERENCE", IMB_BLEND_DIFFERENCE);
	PyModule_AddIntConstant(m, "IMB_BLEND_EXCLUSION", IMB_BLEND_EXCLUSION);
	PyModule_AddIntConstant(m, "IMB_BLEND_HUE", IMB_BLEND_HUE);
	PyModule_AddIntConstant(m, "IMB_BLEND_SATURATION", IMB_BLEND_SATURATION);
	PyModule_AddIntConstant(m, "IMB_BLEND_LUMINOSITY", IMB_BLEND_LUMINOSITY);
	PyModule_AddIntConstant(m, "IMB_BLEND_COLOR", IMB_BLEND_COLOR);

	PyModule_AddIntConstant(m, "IMB_BLEND_COPY", IMB_BLEND_COPY);
	PyModule_AddIntConstant(m, "IMB_BLEND_COPY_RGB", IMB_BLEND_COPY_RGB);
	PyModule_AddIntConstant(m, "IMB_BLEND_COPY_ALPHA", IMB_BLEND_COPY_ALPHA);

	// init last error description
	Exception::m_lastError = "";
	
	return m;
}

// registration to Image types, put here because of silly linker bug
