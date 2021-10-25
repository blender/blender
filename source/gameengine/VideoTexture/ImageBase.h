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
 * This source file is part of blendTex library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ImageBase.h
 *  \ingroup bgevideotex
 */

#ifndef __IMAGEBASE_H__
#define __IMAGEBASE_H__

#include "Common.h"

#include <vector>
#include "EXP_PyObjectPlus.h"

#include "PyTypeList.h"

#include "FilterBase.h"

#include "GPU_glew.h"

// forward declarations
struct PyImage;
class ImageSource;


/// type for list of image sources
typedef std::vector<ImageSource*> ImageSourceList;


/// base class for image filters
class ImageBase
{
public:
	/// constructor
	ImageBase (bool staticSrc = false);
	/// destructor
	virtual ~ImageBase(void);
	/// release contained objects, if returns true, object should be deleted
	virtual bool release(void);

	/// is an image available
	bool isImageAvailable(void)
	{ return m_avail; }
	/// get image
	unsigned int *getImage(unsigned int texId = 0, double timestamp=-1.0);
	/// get image size
	short * getSize(void) { return m_size; }
	/// get image buffer size
	unsigned long getBuffSize(void)
	{ return m_size[0] * m_size[1] * sizeof(unsigned int); }
	/// refresh image - invalidate its current content
	virtual void refresh(void);

	/// get scale
	bool getScale(void) { return m_scale; }
	/// set scale
	void setScale(bool scale) { m_scale = scale; m_scaleChange = true; }
	/// get vertical flip
	bool getFlip(void) { return m_flip; }
	/// set vertical flip
	void setFlip(bool flip) { m_flip = flip; }
	/// get Z buffer
	bool getZbuff(void) { return m_zbuff; }
	/// set Z buffer
	void setZbuff(bool zbuff) { m_zbuff = zbuff; }
	/// get depth
	bool getDepth(void) { return m_depth; }
	/// set depth
	void setDepth(bool depth) { m_depth = depth; }

	/// get source object
	PyImage * getSource(const char *id);
	/// set source object, return true, if source was set
	bool setSource(const char *id, PyImage *source);

	/// get pixel filter
	PyFilter * getFilter(void) { return m_pyfilter; }
	/// set pixel filter
	void setFilter(PyFilter * filt);

	/// calculate size(nearest power of 2)
	static short calcSize(short size);

	/// calculate image from sources and send it to a target buffer instead of a texture
    /// format is GL_RGBA or GL_BGRA
	virtual bool loadImage(unsigned int *buffer, unsigned int size, unsigned int format, double ts);

	/// swap the B and R channel in-place in the image buffer
	void swapImageBR();

	/// number of buffer pointing to m_image, public because not handled by this class
	int m_exports;

protected:
	/// image buffer
	unsigned int * m_image;
	/// image buffer size
	unsigned int m_imgSize;
	/// image size
	short m_size[2];
	/// image is available
	bool m_avail;

	/// scale image to power 2 sizes
	bool m_scale;
	/// scale was changed
	bool m_scaleChange;
	/// flip image vertically
	bool m_flip;
	/// use the Z buffer as a texture
	bool m_zbuff;
	/// extract the Z buffer with unisgned int precision
	bool m_depth;

	/// source image list
	ImageSourceList m_sources;
	/// flag for disabling addition and deletion of sources
	bool m_staticSources;

	/// pixel filter
	PyFilter * m_pyfilter;

	/// initialize image data
	void init(short width, short height);

	/// find source
	ImageSourceList::iterator findSource(const char *id);

	/// create new source
	virtual ImageSource *newSource(const char *id) { return NULL; }

	/// check source sizes
	bool checkSourceSizes(void);

	/// calculate image from sources and set its availability
	virtual void calcImage(unsigned int texId, double ts) {}

	/// perform loop detection
	bool loopDetect(ImageBase * img);

	/// template for image conversion
	template<class FLT, class SRC> void convImage(FLT & filter, SRC srcBuff,
		short * srcSize)
	{
		// destination buffer
		unsigned int * dstBuff = m_image;
		// pixel size from filter
		unsigned int pixSize = filter.firstPixelSize();
		// if no scaling is needed
		if (srcSize[0] == m_size[0] && srcSize[1] == m_size[1])
			// if flipping isn't required
			if (!m_flip)
				// copy bitmap
				for (short y = 0; y < m_size[1]; ++y)
					for (short x = 0; x < m_size[0]; ++x, ++dstBuff, srcBuff += pixSize)
						// copy pixel
						*dstBuff = filter.convert(srcBuff, x, y, srcSize, pixSize);
		// otherwise flip image top to bottom
			else
			{
				// go to last row of image
				srcBuff += srcSize[0] * (srcSize[1] - 1) * pixSize;
				// copy bitmap
				for (short y = m_size[1] - 1; y >= 0; --y, srcBuff -= 2 * srcSize[0] * pixSize)
					for (short x = 0; x < m_size[0]; ++x, ++dstBuff, srcBuff += pixSize)
						// copy pixel
						*dstBuff = filter.convert(srcBuff, x, y, srcSize, pixSize);
			}
			// else scale picture (nearest neighbor)
		else
		{
			// interpolation accumulator
			int accHeight = srcSize[1] >> 1;
			// if flipping is required
			if (m_flip)
				// go to last row of image
				srcBuff += srcSize[0] * (srcSize[1] - 1) * pixSize;
			// process image rows
			for (int y = 0; y < srcSize[1]; ++y)
			{
				// increase height accum
				accHeight += m_size[1];
				// if pixel row has to be drawn
				if (accHeight >= srcSize[1])
				{
					// decrease accum
					accHeight -= srcSize[1];
					// width accum
					int accWidth = srcSize[0] >> 1;
					// process row
					for (int x = 0; x < srcSize[0]; ++x)
					{
						// increase width accum
						accWidth += m_size[0];
						// if pixel has to be drawn
						if (accWidth >= srcSize[0])
						{
							// decrease accum
							accWidth -= srcSize[0];
							// convert pixel
							*dstBuff = filter.convert(srcBuff, x, m_flip ? srcSize[1] - y - 1 : y,
								srcSize, pixSize);
							// next pixel
							++dstBuff;
						}
						// shift source pointer
						srcBuff += pixSize;
					}
				}
				// if pixel row will not be drawn
				else
					// move source pointer to next row
					srcBuff += pixSize * srcSize[0];
				// if y flipping is required
				if (m_flip)
					// go to previous row of image
					srcBuff -= 2 * pixSize * srcSize[0];
			}
		}
	}

	// template for specific filter preprocessing
	template <class F, class SRC> void filterImage (F & filt, SRC srcBuff, short *srcSize)
	{
		// find first filter in chain
		FilterBase * firstFilter = NULL;
		if (m_pyfilter != NULL) firstFilter = m_pyfilter->m_filter->findFirst();
		// if first filter is available
		if (firstFilter != NULL)
		{
			// python wrapper for filter
			PyFilter pyFilt;
			pyFilt.m_filter = &filt;
			// set specified filter as first in chain
			firstFilter->setPrevious(&pyFilt, false);
			// convert video image
			convImage(*(m_pyfilter->m_filter), srcBuff, srcSize);
			// delete added filter
			firstFilter->setPrevious(NULL, false);
		}
		// otherwise use given filter for conversion
		else convImage(filt, srcBuff, srcSize);
		// source was processed
		m_avail = true;
	}
};


// python structure for image filter
struct PyImage
{
	PyObject_HEAD
	// source object
	ImageBase * m_image;
};


// size of id
const int SourceIdSize = 32;


/// class for source of image
class ImageSource
{
public:
	/// constructor
	ImageSource (const char *id);
	/// destructor
	virtual ~ImageSource (void);

	/// get id
	const char * getId (void) { return m_id; }
	/// compare id to argument
	bool is (const char *id);

	/// get source object
	PyImage * getSource (void) { return m_source; }
	/// set source object
	void setSource (PyImage *source);

	/// get image from source
	unsigned int * getImage (double ts=-1.0);
	/// get buffered image
	unsigned int * getImageBuf (void) { return m_image; }
	/// refresh source
	void refresh (void);

	/// get image size
	short * getSize (void)
	{ 
		static short defSize [] = {0, 0};
		return m_source != NULL ? m_source->m_image->getSize() : defSize;
	}

protected:
	/// id of source
	char m_id [SourceIdSize];
	/// pointer to source structure
	PyImage * m_source;
	/// buffered image from source
	unsigned int * m_image;

private:
	/// default constructor is forbidden
	ImageSource (void) {}
};

// list of python image types
extern PyTypeList pyImageTypes;


// functions for python interface

// object initialization
template <class T> static int Image_init(PyObject *pySelf, PyObject *args, PyObject *kwds)
{
	PyImage *self = reinterpret_cast<PyImage *>(pySelf);
	// create source object
	if (self->m_image != NULL) delete self->m_image;
	self->m_image = new T();
	// initialization succeded
	return 0;
}

// object allocation
PyObject *Image_allocNew(PyTypeObject *type, PyObject *args, PyObject *kwds);
// object deallocation
void Image_dealloc(PyImage *self);

// get image data
PyObject *Image_getImage(PyImage *self, char *mode);
// get image size
PyObject *Image_getSize(PyImage *self, void *closure);
// refresh image - invalidate current content
PyObject *Image_refresh(PyImage *self, PyObject *args);

// get scale
PyObject *Image_getScale(PyImage *self, void *closure);
// set scale
int Image_setScale(PyImage *self, PyObject *value, void *closure);
// get flip
PyObject *Image_getFlip(PyImage *self, void *closure);
// set flip
int Image_setFlip(PyImage *self, PyObject *value, void *closure);

// get filter source object
PyObject *Image_getSource(PyImage *self, PyObject *args);
// set filter source object
PyObject *Image_setSource(PyImage *self, PyObject *args);
// get Z buffer
PyObject *Image_getZbuff(PyImage *self, void *closure);
// set Z buffer
int Image_setZbuff(PyImage *self, PyObject *value, void *closure);
// get depth
PyObject *Image_getDepth(PyImage *self, void *closure);
// set depth
int Image_setDepth(PyImage *self, PyObject *value, void *closure);
 
// get pixel filter object
PyObject *Image_getFilter(PyImage *self, void *closure);
// set pixel filter object
int Image_setFilter(PyImage *self, PyObject *value, void *closure);
// check if a buffer can be extracted
PyObject *Image_valid(PyImage *self, void *closure);
// for buffer access to PyImage objects
extern PyBufferProcs imageBufferProcs;

#endif
