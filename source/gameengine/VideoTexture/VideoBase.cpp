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

/** \file gameengine/VideoTexture/VideoBase.cpp
 *  \ingroup bgevideotex
 */

#if defined WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "VideoBase.h"

#include "FilterSource.h"

// VideoBase implementation


// initialize image data
void VideoBase::init(short width, short height)
{
	// save original sizes
	m_orgSize[0] = width;
	m_orgSize[1] = height;
	// call base class initialization
	ImageBase::init(width, height);
}


// process video frame
void VideoBase::process (BYTE *sample)
{
	// if scale was changed
	if (m_scaleChange)
		// reset image
		init(m_orgSize[0], m_orgSize[1]);
	// if image is allocated and is able to store new image
	if (m_image != NULL && !m_avail)
	{
		// filters used
		// convert video format to image
		switch (m_format)
		{
		case RGBA32:
			{
				FilterRGBA32 filtRGBA;
				// use filter object for format to convert image
				filterImage(filtRGBA, sample, m_orgSize);
				// finish
				break;
			}
		case RGB24:
			{
				FilterRGB24 filtRGB;
				// use filter object for format to convert image
				filterImage(filtRGB, sample, m_orgSize);
				// finish
				break;
			}
		case YV12:
			{
				// use filter object for format to convert image
				FilterYV12 filtYUV;
				filtYUV.setBuffs(sample, m_orgSize);
				filterImage(filtYUV, sample, m_orgSize);
				// finish
				break;
			}
		case None:
			break; /* assert? */
		}
	}
}


// python functions


// exceptions for video source initialization
ExceptionID SourceVideoEmpty, SourceVideoCreation;
ExpDesc SourceVideoEmptyDesc(SourceVideoEmpty, "Source Video is empty");
ExpDesc SourceVideoCreationDesc(SourceVideoCreation, "SourceVideo object was not created");

// open video source
void Video_open(VideoBase *self, char *file, short captureID)
{
	// if file is empty, throw exception
	if (file == NULL) THRWEXCP(SourceVideoEmpty, S_OK);

	// open video file or capture device
	if (captureID >= 0) 
		self->openCam(file, captureID);
	else 
		self->openFile(file);
}


// play video
PyObject *Video_play(PyImage *self)
{ if (getVideo(self)->play()) Py_RETURN_TRUE; else Py_RETURN_FALSE; }

// pause video
PyObject *Video_pause(PyImage *self)
{ if (getVideo(self)->pause()) Py_RETURN_TRUE; else Py_RETURN_FALSE; }

PyObject *Video_stop(PyImage *self)
{ if (getVideo(self)->stop()) Py_RETURN_TRUE; else Py_RETURN_FALSE; }

// get status
PyObject *Video_getStatus(PyImage *self, void *closure)
{
	return Py_BuildValue("h", getVideo(self)->getStatus());
}

// refresh video
PyObject *Video_refresh(PyImage *self)
{
	getVideo(self)->refresh();
	return Video_getStatus(self, NULL);
}


// get range
PyObject *Video_getRange(PyImage *self, void *closure)
{
	return Py_BuildValue("[ff]", getVideo(self)->getRange()[0],
		getVideo(self)->getRange()[1]);
}

// set range
int Video_setRange(PyImage *self, PyObject *value, void *closure)
{
	// check validity of parameter
	if (value == NULL || !PySequence_Check(value) || PySequence_Size(value) != 2 ||
	    /* XXX - this is incorrect if the sequence is not a list/tuple! */
	    !PyFloat_Check(PySequence_Fast_GET_ITEM(value, 0)) ||
	    !PyFloat_Check(PySequence_Fast_GET_ITEM(value, 1)))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a sequence of 2 float");
		return -1;
	}
	// set range
	getVideo(self)->setRange(PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 0)),
		PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 1)));
	// success
	return 0;
}

// get repeat
PyObject *Video_getRepeat (PyImage *self, void *closure)
{ return Py_BuildValue("h", getVideo(self)->getRepeat()); }

// set repeat
int Video_setRepeat(PyImage *self, PyObject *value, void *closure)
{
	// check validity of parameter
	if (value == NULL || !PyLong_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be an int");
		return -1;
	}
	// set repeat
	getVideo(self)->setRepeat(int(PyLong_AsLong(value)));
	// success
	return 0;
}

// get frame rate
PyObject *Video_getFrameRate (PyImage *self, void *closure)
{ return Py_BuildValue("f", double(getVideo(self)->getFrameRate())); }

// set frame rate
int Video_setFrameRate(PyImage *self, PyObject *value, void *closure)
{
	// check validity of parameter
	if (value == NULL || !PyFloat_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a float");
		return -1;
	}
	// set repeat
	getVideo(self)->setFrameRate(float(PyFloat_AsDouble(value)));
	// success
	return 0;
}
