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

#include "ImageBase.h"
extern "C" {
#include "BGL.h"
}
#include "GL/glew.h"

#include <vector>
#include <string.h>

#include <PyObjectPlus.h>
#include <structmember.h>

#include "FilterBase.h"

#include "Exception.h"

#if (defined(WIN32) || defined(WIN64)) && !defined(FREE_WINDOWS)
#define strcasecmp	_stricmp
#endif

// ImageBase class implementation

// constructor
ImageBase::ImageBase (bool staticSrc) : m_image(NULL), m_imgSize(0),
m_avail(false), m_scale(false), m_scaleChange(false), m_flip(false),
m_staticSources(staticSrc), m_pyfilter(NULL)
{
	m_size[0] = m_size[1] = 0;
	m_exports = 0;
}


// destructor
ImageBase::~ImageBase (void)
{
	// release image
	delete [] m_image;
}


// release python objects
bool ImageBase::release (void)
{
	// iterate sources
	for (ImageSourceList::iterator it = m_sources.begin(); it != m_sources.end(); ++it)
	{
		// release source object
		delete *it;
		*it = NULL;
	}
	// release filter object
	Py_XDECREF(m_pyfilter);
	m_pyfilter = NULL;
	return true;
}


// get image
unsigned int * ImageBase::getImage (unsigned int texId, double ts)
{
	// if image is not available
	if (!m_avail)
	{
		// if there are any sources
		if (!m_sources.empty())
		{
			// get images from sources
			for (ImageSourceList::iterator it = m_sources.begin(); it != m_sources.end(); ++it)
				// get source image
				(*it)->getImage(ts);
			// init image
			init(m_sources[0]->getSize()[0], m_sources[0]->getSize()[1]);
		}
		// calculate new image
		calcImage(texId, ts);
	}
	// if image is available, return it, otherwise NULL
	return m_avail ? m_image : NULL;
}


// refresh image source
void ImageBase::refresh (void)
{
	// invalidate this image
	m_avail = false;
	// refresh all sources
	for (ImageSourceList::iterator it = m_sources.begin(); it != m_sources.end(); ++it)
		(*it)->refresh();
}


// get source object
PyImage * ImageBase::getSource (const char * id)
{
	// find source
	ImageSourceList::iterator src = findSource(id);
	// return it, if found
	return src != m_sources.end() ? (*src)->getSource() : NULL;
}


// set source object
bool ImageBase::setSource (const char * id, PyImage * source)
{
	// find source
	ImageSourceList::iterator src = findSource(id);
	// check source loop
	if (source != NULL && source->m_image->loopDetect(this))
		return false;
	// if found, set new object
	if (src != m_sources.end())
		// if new object is not empty or sources are static
		if (source != NULL || m_staticSources)
			// replace previous source
			(*src)->setSource(source);
		// otherwise delete source
		else
			m_sources.erase(src);
	// if source is not found and adding is allowed
	else
		if (!m_staticSources)
		{
			// create new source
			ImageSource * newSrc = newSource(id);
			newSrc->setSource(source);
			// if source was created, add it to source list
			if (newSrc != NULL) m_sources.push_back(newSrc);
		}
		// otherwise source wasn't set
		else 
			return false;
	// source was set
	return true;
}


// set pixel filter
void ImageBase::setFilter (PyFilter * filt)
{
	// reference new filter
	if (filt != NULL) Py_INCREF(filt);
	// release previous filter
	Py_XDECREF(m_pyfilter);
	// set new filter
	m_pyfilter = filt;
}

ExceptionID ImageHasExports;
ExceptionID InvalidColorChannel;

ExpDesc ImageHasExportsDesc (ImageHasExports, "Image has exported buffers, cannot resize");
ExpDesc InvalidColorChannelDesc (InvalidColorChannel, "Invalid or too many color channels specified. At most 4 values within R, G, B, A, 0, 1");

// initialize image data
void ImageBase::init (short width, short height)
{
	// if image has to be scaled
	if (m_scale)
	{
		// recalc sizes of image
		width = calcSize(width);
		height = calcSize(height);
	}
	// if sizes differ
	if (width != m_size[0] || height != m_size[1])
	{
		if (m_exports > 0)
			THRWEXCP(ImageHasExports,S_OK);

		// new buffer size
		unsigned int newSize = width * height;
		// if new buffer is larger than previous
		if (newSize > m_imgSize)
		{
			// set new buffer size
			m_imgSize = newSize;
			// release previous and create new buffer
			delete [] m_image;
			m_image = new unsigned int[m_imgSize];
		}
		// new image size
		m_size[0] = width;
		m_size[1] = height;
		// scale was processed
		m_scaleChange = false;
	}
}


// find source
ImageSourceList::iterator ImageBase::findSource (const char * id)
{
	// iterate sources
	ImageSourceList::iterator it;
	for (it = m_sources.begin(); it != m_sources.end(); ++it)
		// if id matches, return iterator
		if ((*it)->is(id)) return it;
	// source not found
	return it;
}


// check sources sizes
bool ImageBase::checkSourceSizes (void)
{
	// reference size
	short * refSize = NULL;
	// iterate sources
	for (ImageSourceList::iterator it = m_sources.begin(); it != m_sources.end(); ++it)
	{
		// get size of current source
		short * curSize = (*it)->getSize();
		// if size is available and is not empty
		if (curSize[0] != 0 && curSize[1] != 0) {
			// if reference size is not set
			if (refSize == NULL) {
				// set current size as reference
				refSize = curSize;
		// otherwise check with current size
			} else if (curSize[0] != refSize[0] || curSize[1] != refSize[1]) {
				// if they don't match, report it
				return false;
			}
		}
	}
	// all sizes match
	return true;
}


// compute nearest power of 2 value
short ImageBase::calcSize (short size)
{
	// while there is more than 1 bit in size value
	while ((size & (size - 1)) != 0)
		// clear last bit
		size = size & (size - 1);
	// return result
	return size;
}


// perform loop detection
bool ImageBase::loopDetect (ImageBase * img)
{
	// if this object is the same as parameter, loop is detected
	if (this == img) return true;
	// check all sources
	for (ImageSourceList::iterator it = m_sources.begin(); it != m_sources.end(); ++it)
		// if source detected loop, return this result
		if ((*it)->getSource() != NULL && (*it)->getSource()->m_image->loopDetect(img))
			return true;
	// no loop detected
	return false;
}


// ImageSource class implementation

// constructor
ImageSource::ImageSource (const char * id) : m_source(NULL), m_image(NULL)
{
	// copy id
	int idx;
	for (idx = 0; id[idx] != '\0' && idx < SourceIdSize - 1; ++idx)
		m_id[idx] = id[idx];
	m_id[idx] = '\0';
}

// destructor
ImageSource::~ImageSource (void)
{
	// release source
	setSource(NULL);
}


// compare id
bool ImageSource::is (const char * id)
{
	for (char * myId = m_id; *myId != '\0'; ++myId, ++id)
		if (*myId != *id) return false;
	return *id == '\0';
}


// set source object
void ImageSource::setSource (PyImage * source)
{
	// reference new source
	if (source != NULL) Py_INCREF(source);
	// release previous source
	Py_XDECREF(m_source);
	// set new source
	m_source = source;
}


// get image from source
unsigned int * ImageSource::getImage (double ts)
{
	// if source is available
	if (m_source != NULL)
		// get image from source
		m_image = m_source->m_image->getImage(0, ts);
	// otherwise reset buffer
	else
		m_image = NULL;
	// return image
	return m_image;
}


// refresh source
void ImageSource::refresh (void)
{
	// if source is available, refresh it
	if (m_source != NULL) m_source->m_image->refresh();
}



// list of image types
PyTypeList pyImageTypes;



// functions for python interface

// object allocation
PyObject * Image_allocNew (PyTypeObject * type, PyObject * args, PyObject * kwds)
{
	// allocate object
	PyImage * self = reinterpret_cast<PyImage*>(type->tp_alloc(type, 0));
	// initialize object structure
	self->m_image = NULL;
	// return allocated object
	return reinterpret_cast<PyObject*>(self);
}

// object deallocation
void Image_dealloc (PyImage * self)
{
	// release object attributes
	if (self->m_image != NULL)
	{
		if (self->m_image->m_exports > 0)
		{
			PyErr_SetString(PyExc_SystemError,
				            "deallocated Image object has exported buffers");
			PyErr_Print();
		}
		// if release requires deleting of object, do it
		if (self->m_image->release())
			delete self->m_image;
		self->m_image = NULL;
	}
}

// get image data
PyObject * Image_getImage (PyImage * self, char * mode)
{
	try
	{
		unsigned int * image = self->m_image->getImage();
		if (image) 
		{
			// build BGL buffer
			int dimensions = self->m_image->getBuffSize();
			Buffer * buffer;
			if (mode == NULL || !strcasecmp(mode, "RGBA"))
			{
				buffer = BGL_MakeBuffer( GL_BYTE, 1, &dimensions, image);
			}
			else 
			{
				int i, c, ncolor, pixels;
				int offset[4];
				unsigned char *s, *d;
				// scan the mode to get the channels requested, no more than 4
				for (i=ncolor=0; mode[i] != 0 && ncolor < 4; i++)
				{
					switch (toupper(mode[i]))
					{
					case 'R':
						offset[ncolor++] = 0;
						break;
					case 'G':
						offset[ncolor++] = 1;
						break;
					case 'B':
						offset[ncolor++] = 2;
						break;
					case 'A':
						offset[ncolor++] = 3;
						break;
					case '0':
						offset[ncolor++] = -1;
						break;
					case '1':
						offset[ncolor++] = -2;
						break;
					// if you add more color code, change the switch further down
					default:
						THRWEXCP(InvalidColorChannel,S_OK);
					}
				}
				if (mode[i] != 0) {
					THRWEXCP(InvalidColorChannel,S_OK);
				}
				// first get the number of pixels
				pixels = dimensions / 4;
				// multiple by the number of channels, each is one byte
				dimensions = pixels * ncolor;
				// get an empty buffer
				buffer = BGL_MakeBuffer( GL_BYTE, 1, &dimensions, NULL);
				// and fill it
				for (i=0, d=(unsigned char*)buffer->buf.asbyte, s=(unsigned char*)image; 
					 i<pixels; 
					 ++i, d+=ncolor, s+=4)
				{
					for (c=0; c<ncolor; c++)
					{
						switch (offset[c])
						{
						case 0: d[c] = s[0]; break;
						case 1: d[c] = s[1]; break;
						case 2: d[c] = s[2]; break;
						case 3: d[c] = s[3]; break;
						case -1: d[c] = 0; break;
						case -2: d[c] = 0xFF; break;
						}
					}
				}
			}
			return (PyObject*)buffer;
		}
	}
	catch (Exception & exp)
	{
		exp.report();
		return NULL;
	}
	Py_RETURN_NONE;
}

// get image size
PyObject * Image_getSize (PyImage * self, void * closure)
{
	return Py_BuildValue("(hh)", self->m_image->getSize()[0],
		self->m_image->getSize()[1]);
}

// refresh image
PyObject * Image_refresh (PyImage * self)
{
	self->m_image->refresh();
	Py_RETURN_NONE;
}

// get scale
PyObject * Image_getScale (PyImage * self, void * closure)
{
	if (self->m_image != NULL && self->m_image->getScale()) Py_RETURN_TRUE;
	else Py_RETURN_FALSE;
}

// set scale
int Image_setScale (PyImage * self, PyObject * value, void * closure)
{
	// check parameter, report failure
	if (value == NULL || !PyBool_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return -1;
	}
	// set scale
	if (self->m_image != NULL) self->m_image->setScale(value == Py_True);
	// success
	return 0;
}

// get flip
PyObject * Image_getFlip (PyImage * self, void * closure)
{
	if (self->m_image != NULL && self->m_image->getFlip()) Py_RETURN_TRUE;
	else Py_RETURN_FALSE;
}

// set flip
int Image_setFlip (PyImage * self, PyObject * value, void * closure)
{
	// check parameter, report failure
	if (value == NULL || !PyBool_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return -1;
	}
	// set scale
	if (self->m_image != NULL) self->m_image->setFlip(value == Py_True);
	// success
	return 0;
}


// get filter source object
PyObject * Image_getSource (PyImage * self, PyObject * args)
{
	// get arguments
	char * id;
	if (!PyArg_ParseTuple(args, "s:getSource", &id))
		return NULL;
	if (self->m_image != NULL)
	{
		// get source object
		PyObject * src = reinterpret_cast<PyObject*>(self->m_image->getSource(id));
		// if source is available
		if (src != NULL)
		{
			// return source
			Py_INCREF(src);
			return src;
		}
	}
	// source was not found
	Py_RETURN_NONE;
}


// set filter source object
PyObject * Image_setSource (PyImage * self, PyObject * args)
{
	// get arguments
	char * id;
	PyObject * obj;
	if (!PyArg_ParseTuple(args, "sO:setSource", &id, &obj))
		return NULL;
	if (self->m_image != NULL)
	{
		// check type of object
		if (pyImageTypes.in(obj->ob_type))
		{
			// convert to image struct
			PyImage * img = reinterpret_cast<PyImage*>(obj);
			// set source
			if (!self->m_image->setSource(id, img))
			{
				// if not set, retport error
				PyErr_SetString(PyExc_RuntimeError, "Invalid source or id");
				return NULL;
			}
		}
		// else report error
		else
		{
			PyErr_SetString(PyExc_RuntimeError, "Invalid type of object");
			return NULL;
		}
	}
	// return none
	Py_RETURN_NONE;
}


// get pixel filter object
PyObject * Image_getFilter (PyImage * self, void * closure)
{
	// if image object is available
	if (self->m_image != NULL)
	{
		// pixel filter object
		PyObject * filt = reinterpret_cast<PyObject*>(self->m_image->getFilter());
		// if filter is present
		if (filt != NULL)
		{
			// return it
			Py_INCREF(filt);
			return filt;
		}
	}
	// otherwise return none
	Py_RETURN_NONE;
}


// set pixel filter object
int Image_setFilter (PyImage * self, PyObject * value, void * closure)
{
	// if image object is available
	if (self->m_image != NULL)
	{
		// check new value
		if (value == NULL || !pyFilterTypes.in(value->ob_type))
		{
			// report value error
			PyErr_SetString(PyExc_TypeError, "Invalid type of value");
			return -1;
		}
		// set new value
		self->m_image->setFilter(reinterpret_cast<PyFilter*>(value));
	}
	// return success
	return 0;
}
PyObject * Image_valid(PyImage * self, void * closure)
{
	if (self->m_image->isImageAvailable())
	{
		Py_RETURN_TRUE;
	}
	else
	{
		Py_RETURN_FALSE;
	}
}

int Image_getbuffer(PyImage *self, Py_buffer *view, int flags)
{
	unsigned int * image;
    int ret;

	try
	{
		// can throw in case of resize
		image = self->m_image->getImage();
	}
	catch (Exception & exp)
	{
		// cannot return -1, this creates a crash in Python, for now we will just return an empty buffer
		//exp.report();
		//return -1;
		goto error;
	}

	if (!image)
	{
		// same remark, see above
		//PyErr_SetString(PyExc_BufferError, "Image buffer is not available");
		//return -1;
		goto error;
	}
    if (view == NULL) 
	{
        self->m_image->m_exports++;
        return 0;
    }
    ret = PyBuffer_FillInfo(view, (PyObject*)self, image, self->m_image->getBuffSize(), 0, flags);
    if (ret >= 0)
        self->m_image->m_exports++;
    return ret;

error:
	// Return a empty buffer to avoid a crash in Python 3.1
	// The bug is fixed in Python SVN 77916, as soon as the python revision used by Blender is
	// updated, you can simply return -1 and set the error
	static char* buf = "";
    ret = PyBuffer_FillInfo(view, (PyObject*)self, buf, 0, 0, flags);
    if (ret >= 0)
        self->m_image->m_exports++;
    return ret;
	
}

void Image_releaseBuffer(PyImage *self, Py_buffer *buffer)
{
	self->m_image->m_exports--;
}

PyBufferProcs imageBufferProcs = 
{
	(getbufferproc)Image_getbuffer,
	(releasebufferproc)Image_releaseBuffer
};

