/*
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

/** \file ImageViewport.h
 *  \ingroup bgevideotex
 */
 
#ifndef IMAGEVIEWPORT_H
#define IMAGEVIEWPORT_H


#include "Common.h"

#include "ImageBase.h"


/// class for viewport access
class ImageViewport : public ImageBase
{
public:
	/// constructor
	ImageViewport (void);

	/// destructor
	virtual ~ImageViewport (void);

	/// is whole buffer used
	bool getWhole (void) { return m_whole; }
	/// set whole buffer use
	void setWhole (bool whole);

	/// is alpha channel used
	bool getAlpha (void) { return m_alpha; }
	/// set whole buffer use
	void setAlpha (bool alpha) { m_alpha = alpha; }

	/// get capture size in viewport
	short * getCaptureSize (void) { return m_capSize; }
	/// set capture size in viewport
	void setCaptureSize (short * size = NULL);

	/// get position in viewport
	GLint * getPosition (void) { return m_position; }
	/// set position in viewport
	void setPosition (GLint * pos = NULL);

protected:
	/// frame buffer rectangle
	GLint m_viewport[4];

	/// size of captured area
	short m_capSize[2];
	/// use whole viewport
	bool m_whole;
	/// use alpha channel
	bool m_alpha;

	/// position of capture rectangle in viewport
	GLint m_position[2];
	/// upper left point for capturing
	GLint m_upLeft[2];

	/// buffer to copy viewport
	BYTE * m_viewportImage;
	/// texture is initialized
	bool m_texInit;

	/// capture image from viewport
	virtual void calcImage (unsigned int texId, double ts);

	/// get viewport size
	GLint * getViewportSize (void) { return m_viewport + 2; }
};

PyObject * ImageViewport_getCaptureSize (PyImage * self, void * closure);
int ImageViewport_setCaptureSize (PyImage * self, PyObject * value, void * closure);
PyObject * ImageViewport_getWhole (PyImage * self, void * closure);
int ImageViewport_setWhole (PyImage * self, PyObject * value, void * closure);
PyObject * ImageViewport_getAlpha (PyImage * self, void * closure);
int ImageViewport_setAlpha (PyImage * self, PyObject * value, void * closure);

#endif

