/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

Copyright (c) 2007 The Zdeno Ash Miklas

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

// implementation

#include <Python.h>
#include <structmember.h>

#include <KX_BlenderCanvas.h>
#include <KX_BlenderRenderTools.h>
#include <RAS_IRasterizer.h>
#include <RAS_OpenGLRasterizer.h>
#include <KX_WorldInfo.h>
#include <KX_Light.h>

#include "ImageRender.h"

#include "ImageBase.h"
#include "BlendType.h"
#include "Exception.h"


// constructor
ImageRender::ImageRender (KX_Scene * scene, KX_Camera * camera) : m_scene(scene),
m_camera(camera)
{
	// create screen area
	m_area.winrct.xmin = m_upLeft[0];
	m_area.winrct.ymin = m_upLeft[1];
	m_area.winx = m_size[0];
	m_area.winy = m_size[1];
	// create canvas
	m_canvas = new KX_BlenderCanvas(&m_area);
	// create render tools
	m_rendertools = new KX_BlenderRenderTools();
	// create rasterizer
	m_rasterizer = new RAS_OpenGLRasterizer(m_canvas);
	m_rasterizer->Init();
	// initialize background colour
	setBackground(0, 0, 255);
	// refresh lights
	refreshLights();
}

// destructor
ImageRender::~ImageRender (void)
{
	// release allocated objects
	delete m_rasterizer;
	delete m_rendertools;
	delete m_canvas;
}


// set background color
void ImageRender::setBackground (unsigned char red, unsigned char green, unsigned char blue)
{
	m_background[0] = red;
	m_background[1] = green;
	m_background[2] = blue;
	m_rasterizer->SetBackColor(m_background[0], m_background[1], m_background[2], 1.0);
}


// capture image from viewport
void ImageRender::calcImage (unsigned int texId)
{
	// setup camera
	bool cameraPasive = !m_camera->GetViewport();
	// render scene
	Render();
	// reset camera
	if (cameraPasive) m_camera->EnableViewport(false);
	// get image from viewport
	ImageViewport::calcImage(texId);
}

void ImageRender::Render()
{
    //
}

// refresh lights
void ImageRender::refreshLights (void)
{
	// clear lights list
	//m_rendertools->RemoveAllLights();
	// set lights
	//for (int idx = 0; idx < scene->GetLightList()->GetCount(); ++idx)
	//  m_rendertools->AddLight(((KX_LightObject*)(scene->GetLightList()->GetValue(idx)))->GetLightData());
}



// cast Image pointer to ImageRender
inline ImageRender * getImageRender (PyImage * self)
{ return static_cast<ImageRender*>(self->m_image); }


// python methods

// Blender Scene type
BlendType<KX_Scene> sceneType ("KX_Scene");
// Blender Camera type
BlendType<KX_Camera> cameraType ("KX_Camera");


ExceptionID SceneInvalid, CameraInvalid;
ExpDesc SceneInvalidDesc (SceneInvalid, "Scene object is invalid");
ExpDesc CameraInvalidDesc (CameraInvalid, "Camera object is invalid");

// object initialization
static int ImageRender_init (PyObject * pySelf, PyObject * args, PyObject * kwds)
{
	// parameters - scene object
	PyObject * scene;
	// camera object
	PyObject * camera;
	// parameter keywords
	static char *kwlist[] = {"sceneObj", "cameraObj", NULL};
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist, &scene, &camera))
		return -1;
	try
	{
		// get scene pointer
		KX_Scene * scenePtr (NULL);
		if (scene != NULL) scenePtr = sceneType.checkType(scene);
		// throw exception if scene is not available
		if (scenePtr == NULL) THRWEXCP(SceneInvalid, S_OK);

		// get camera pointer
		KX_Camera * cameraPtr (NULL);
		if (camera != NULL) cameraPtr = cameraType.checkType(camera);
		// throw exception if camera is not available
		if (cameraPtr == NULL) THRWEXCP(CameraInvalid, S_OK);

		// get pointer to image structure
		PyImage * self = reinterpret_cast<PyImage*>(pySelf);
		// create source object
		if (self->m_image != NULL) delete self->m_image;
		self->m_image = new ImageRender(scenePtr, cameraPtr);
	}
	catch (Exception & exp)
	{
		exp.report();
		return -1;
	}
	// initialization succeded
	return 0;
}


// get background color
PyObject * getBackground (PyImage * self, void * closure)
{
	return Py_BuildValue("[BBB]", getImageRender(self)->getBackground()[0],
		getImageRender(self)->getBackground()[1], getImageRender(self)->getBackground()[2]);
}

// set color
static int setBackground (PyImage * self, PyObject * value, void * closure)
{
	// check validity of parameter
	if (value == NULL || !PySequence_Check(value) || PySequence_Length(value) != 3
		|| !PyInt_Check(PySequence_Fast_GET_ITEM(value, 0))
		|| !PyInt_Check(PySequence_Fast_GET_ITEM(value, 1))
		|| !PyInt_Check(PySequence_Fast_GET_ITEM(value, 2)))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a sequence of 3 ints");
		return -1;
	}
	// set background color
	getImageRender(self)->setBackground((unsigned char)(PyInt_AsLong(PySequence_Fast_GET_ITEM(value, 0))),
		(unsigned char)(PyInt_AsLong(PySequence_Fast_GET_ITEM(value, 1))),
		(unsigned char)(PyInt_AsLong(PySequence_Fast_GET_ITEM(value, 2))));
	// success
	return 0;
}


// methods structure
static PyMethodDef imageRenderMethods[] =
{ // methods from ImageBase class
	{"refresh", (PyCFunction)Image_refresh, METH_NOARGS, "Refresh image - invalidate its current content"},
	{NULL}
};
// attributes structure
static PyGetSetDef imageRenderGetSets[] =
{ 
	{"background", (getter)getBackground, (setter)setBackground, "background color", NULL},
	// attributes from ImageBase class
	{"image", (getter)Image_getImage, NULL, "image data", NULL},
	{"size", (getter)Image_getSize, NULL, "image size", NULL},
	{"scale", (getter)Image_getScale, (setter)Image_setScale, "fast scale of image (near neighbour)",	NULL},
	{"flip", (getter)Image_getFlip, (setter)Image_setFlip, "flip image vertically", NULL},
	{"filter", (getter)Image_getFilter, (setter)Image_setFilter, "pixel filter", NULL},
	{NULL}
};


// define python type
PyTypeObject ImageRenderType =
{ 
	PyObject_HEAD_INIT(NULL)
	0,                         /*ob_size*/
	"VideoTexture.ImageRender",   /*tp_name*/
	sizeof(PyImage),          /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Image_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"Image source from render",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	imageRenderMethods,    /* tp_methods */
	0,                   /* tp_members */
	imageRenderGetSets,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)ImageRender_init,     /* tp_init */
	0,                         /* tp_alloc */
	Image_allocNew,           /* tp_new */
};


