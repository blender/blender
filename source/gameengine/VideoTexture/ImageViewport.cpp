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
 * Copyright (c) 2007 The Zdeno Ash Miklas
 *
 * This source file is part of VideoTexture library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/VideoTexture/ImageViewport.cpp
 *  \ingroup bgevideotex
 */

// implementation

#include "PyObjectPlus.h"
#include <structmember.h>

#include "GL/glew.h"

#include "KX_PythonInit.h"
#include "RAS_ICanvas.h"
#include "Texture.h"
#include "ImageBase.h"
#include "VideoBase.h"
#include "FilterSource.h"
#include "ImageViewport.h"


// constructor
ImageViewport::ImageViewport (void) : m_alpha(false), m_texInit(false)
{
	// get viewport rectangle
	RAS_Rect rect = KX_GetActiveEngine()->GetCanvas()->GetWindowArea();
	m_viewport[0] = rect.GetLeft();
	m_viewport[1] = rect.GetBottom();
	m_viewport[2] = rect.GetWidth();
	m_viewport[3] = rect.GetHeight();
	
	//glGetIntegerv(GL_VIEWPORT, m_viewport);
	// create buffer for viewport image
	// Warning: this buffer is also used to get the depth buffer as an array of
	//          float (1 float = 4 bytes per pixel)
	m_viewportImage = new BYTE [4 * getViewportSize()[0] * getViewportSize()[1]];
	// set attributes
	setWhole(false);
}

// destructor
ImageViewport::~ImageViewport (void)
{
	delete [] m_viewportImage;
}


// use whole viewport to capture image
void ImageViewport::setWhole (bool whole)
{
	// set whole
	m_whole = whole;
	// set capture size to viewport size, if whole,
	// otherwise place area in the middle of viewport
	for (int idx = 0; idx < 2; ++idx)
	{
		// capture size
		m_capSize[idx] = whole ? short(getViewportSize()[idx])
			: calcSize(short(getViewportSize()[idx]));
		// position
		m_position[idx] = whole ? 0 : ((getViewportSize()[idx] - m_capSize[idx]) >> 1);
	}
	// init image
	init(m_capSize[0], m_capSize[1]);
	// set capture position
	setPosition();
}

void ImageViewport::setCaptureSize (short size[2])
{
	m_whole = false;
	if (size == NULL) 
		size = m_capSize;
	for (int idx = 0; idx < 2; ++idx)
	{
		if (size[idx] < 1)
			m_capSize[idx] = 1;
		else if (size[idx] > getViewportSize()[idx])
			m_capSize[idx] = short(getViewportSize()[idx]);
		else
			m_capSize[idx] = size[idx];
	}
	init(m_capSize[0], m_capSize[1]);
	// set capture position
	setPosition();
}

// set position of capture rectangle
void ImageViewport::setPosition (GLint pos[2])
{
	// if new position is not provided, use existing position
	if (pos == NULL) pos = m_position;
	// save position
	for (int idx = 0; idx < 2; ++idx)
		m_position[idx] = pos[idx] < 0 ? 0 : pos[idx] >= getViewportSize()[idx]
		- m_capSize[idx] ? getViewportSize()[idx] - m_capSize[idx] : pos[idx];
	// recalc up left corner
	for (int idx = 0; idx < 2; ++idx)
		m_upLeft[idx] = m_position[idx] + m_viewport[idx];
}


// capture image from viewport
void ImageViewport::calcImage (unsigned int texId, double ts)
{
	// if scale was changed
	if (m_scaleChange)
		// reset image
		init(m_capSize[0], m_capSize[1]);
	// if texture wasn't initialized
	if (!m_texInit) {
		// initialize it
		loadTexture(texId, m_image, m_size);
		m_texInit = true;
	}
	// if texture can be directly created
	if (texId != 0 && m_pyfilter == NULL && m_capSize[0] == calcSize(m_capSize[0])
	    && m_capSize[1] == calcSize(m_capSize[1]) && !m_flip && !m_zbuff && !m_depth)
	{
		// just copy current viewport to texture
		glBindTexture(GL_TEXTURE_2D, texId);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_upLeft[0], m_upLeft[1], (GLsizei)m_capSize[0], (GLsizei)m_capSize[1]);
		// image is not available
		m_avail = false;
	}
	// otherwise copy viewport to buffer, if image is not available
	else if (!m_avail) {
		if (m_zbuff) {
			// Use read pixels with the depth buffer
			// *** misusing m_viewportImage here, but since it has the correct size
			//     (4 bytes per pixel = size of float) and we just need it to apply
			//     the filter, it's ok
			glReadPixels(m_upLeft[0], m_upLeft[1], (GLsizei)m_capSize[0], (GLsizei)m_capSize[1],
			        GL_DEPTH_COMPONENT, GL_FLOAT, m_viewportImage);
			// filter loaded data
			FilterZZZA filt;
			filterImage(filt, (float *)m_viewportImage, m_capSize);
		}
		else {

			if (m_depth) {
				// Use read pixels with the depth buffer
				// See warning above about m_viewportImage.
				glReadPixels(m_upLeft[0], m_upLeft[1], (GLsizei)m_capSize[0], (GLsizei)m_capSize[1],
				        GL_DEPTH_COMPONENT, GL_FLOAT, m_viewportImage);
				// filter loaded data
				FilterDEPTH filt;
				filterImage(filt, (float *)m_viewportImage, m_capSize);
			}
			else {

				// get frame buffer data
				if (m_alpha) {
					glReadPixels(m_upLeft[0], m_upLeft[1], (GLsizei)m_capSize[0], (GLsizei)m_capSize[1], GL_RGBA,
					        GL_UNSIGNED_BYTE, m_viewportImage);
					// filter loaded data
					FilterRGBA32 filt;
					filterImage(filt, m_viewportImage, m_capSize);
				}
				else {
					glReadPixels(m_upLeft[0], m_upLeft[1], (GLsizei)m_capSize[0], (GLsizei)m_capSize[1], GL_RGB,
					        GL_UNSIGNED_BYTE, m_viewportImage);
					// filter loaded data
					FilterRGB24 filt;
					filterImage(filt, m_viewportImage, m_capSize);
				}
			}
		}
	}
}



// cast Image pointer to ImageViewport
inline ImageViewport * getImageViewport (PyImage *self)
{ return static_cast<ImageViewport*>(self->m_image); }


// python methods


// get whole
PyObject *ImageViewport_getWhole (PyImage *self, void *closure)
{
	if (self->m_image != NULL && getImageViewport(self)->getWhole()) Py_RETURN_TRUE;
	else Py_RETURN_FALSE;
}

// set whole
int ImageViewport_setWhole(PyImage *self, PyObject *value, void *closure)
{
	// check parameter, report failure
	if (value == NULL || !PyBool_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return -1;
	}
	try
	{
		// set whole, can throw in case of resize and buffer exports
		if (self->m_image != NULL) getImageViewport(self)->setWhole(value == Py_True);
	}
	catch (Exception & exp)
	{
		exp.report();
		return -1;
	}
	// success
	return 0;
}

// get alpha
PyObject *ImageViewport_getAlpha (PyImage *self, void *closure)
{
	if (self->m_image != NULL && getImageViewport(self)->getAlpha()) Py_RETURN_TRUE;
	else Py_RETURN_FALSE;
}

// set whole
int ImageViewport_setAlpha(PyImage *self, PyObject *value, void *closure)
{
	// check parameter, report failure
	if (value == NULL || !PyBool_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return -1;
	}
	// set alpha
	if (self->m_image != NULL) getImageViewport(self)->setAlpha(value == Py_True);
	// success
	return 0;
}


// get position
static PyObject *ImageViewport_getPosition (PyImage *self, void *closure)
{
	GLint *pos = getImageViewport(self)->getPosition();
	PyObject *ret = PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, PyLong_FromLong(pos[0]));
	PyTuple_SET_ITEM(ret, 1, PyLong_FromLong(pos[1]));
	return ret;
}

// set position
static int ImageViewport_setPosition(PyImage *self, PyObject *value, void *closure)
{
	// check validity of parameter
	if (value == NULL ||
	    !(PyTuple_Check(value) || PyList_Check(value)) ||
	    PySequence_Fast_GET_SIZE(value) != 2 ||
	    !PyLong_Check(PySequence_Fast_GET_ITEM(value, 0)) ||
	    !PyLong_Check(PySequence_Fast_GET_ITEM(value, 1)))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a sequence of 2 ints");
		return -1;
	}
	// set position
	GLint pos[2] = {
	    GLint(PyLong_AsLong(PySequence_Fast_GET_ITEM(value, 0))),
	    GLint(PyLong_AsLong(PySequence_Fast_GET_ITEM(value, 1)))
	};
	getImageViewport(self)->setPosition(pos);
	// success
	return 0;
}

// get capture size
PyObject *ImageViewport_getCaptureSize (PyImage *self, void *closure)
{
	short *size = getImageViewport(self)->getCaptureSize();
	PyObject *ret = PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, PyLong_FromLong(size[0]));
	PyTuple_SET_ITEM(ret, 1, PyLong_FromLong(size[1]));
	return ret;
}

// set capture size
int ImageViewport_setCaptureSize(PyImage *self, PyObject *value, void *closure)
{
	// check validity of parameter
	if (value == NULL ||
	    !(PyTuple_Check(value) || PyList_Check(value)) ||
	    PySequence_Fast_GET_SIZE(value) != 2 ||
	    !PyLong_Check(PySequence_Fast_GET_ITEM(value, 0)) ||
	    !PyLong_Check(PySequence_Fast_GET_ITEM(value, 1)))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a sequence of 2 ints");
		return -1;
	}
	// set capture size
	short size[2] = {
	    short(PyLong_AsLong(PySequence_Fast_GET_ITEM(value, 0))),
	    short(PyLong_AsLong(PySequence_Fast_GET_ITEM(value, 1)))
	};
	try
	{
		// can throw in case of resize and buffer exports
		getImageViewport(self)->setCaptureSize(size);
	}
	catch (Exception & exp)
	{
		exp.report();
		return -1;
	}
	// success
	return 0;
}


// methods structure
static PyMethodDef imageViewportMethods[] =
{ // methods from ImageBase class
	{"refresh", (PyCFunction)Image_refresh, METH_NOARGS, "Refresh image - invalidate its current content"},
	{NULL}
};
// attributes structure
static PyGetSetDef imageViewportGetSets[] =
{ 
	{(char*)"whole", (getter)ImageViewport_getWhole, (setter)ImageViewport_setWhole, (char*)"use whole viewport to capture", NULL},
	{(char*)"position", (getter)ImageViewport_getPosition, (setter)ImageViewport_setPosition, (char*)"upper left corner of captured area", NULL},
	{(char*)"capsize", (getter)ImageViewport_getCaptureSize, (setter)ImageViewport_setCaptureSize, (char*)"size of viewport area being captured", NULL},
	{(char*)"alpha", (getter)ImageViewport_getAlpha, (setter)ImageViewport_setAlpha, (char*)"use alpha in texture", NULL},
	// attributes from ImageBase class
	{(char*)"valid", (getter)Image_valid, NULL, (char*)"bool to tell if an image is available", NULL},
	{(char*)"image", (getter)Image_getImage, NULL, (char*)"image data", NULL},
	{(char*)"size", (getter)Image_getSize, NULL, (char*)"image size", NULL},
	{(char*)"scale", (getter)Image_getScale, (setter)Image_setScale, (char*)"fast scale of image (near neighbor)", NULL},
	{(char*)"flip", (getter)Image_getFlip, (setter)Image_setFlip, (char*)"flip image vertically", NULL},
	{(char*)"zbuff", (getter)Image_getZbuff, (setter)Image_setZbuff, (char*)"use depth buffer as texture", NULL},
	{(char*)"depth", (getter)Image_getDepth, (setter)Image_setDepth, (char*)"get depth information from z-buffer as array of float", NULL},
	{(char*)"filter", (getter)Image_getFilter, (setter)Image_setFilter, (char*)"pixel filter", NULL},
	{NULL}
};


// define python type
PyTypeObject ImageViewportType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"VideoTexture.ImageViewport",   /*tp_name*/
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
	&imageBufferProcs,         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"Image source from viewport",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	imageViewportMethods,    /* tp_methods */
	0,                   /* tp_members */
	imageViewportGetSets,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)Image_init<ImageViewport>,     /* tp_init */
	0,                         /* tp_alloc */
	Image_allocNew,           /* tp_new */
};
