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

/** \file ImageViewport.h
 *  \ingroup bgevideotex
 */

#ifndef __IMAGEVIEWPORT_H__
#define __IMAGEVIEWPORT_H__


#include "Common.h"

#include "ImageBase.h"
#include "RAS_IOffScreen.h"


/// class for viewport access
class ImageViewport : public ImageBase
{
public:
	/// constructor
	ImageViewport (PyRASOffScreen *offscreen=NULL);

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
	void setCaptureSize (short size[2] = NULL);

	/// get position in viewport
	GLint * getPosition (void) { return m_position; }
	/// set position in viewport
	void setPosition (GLint pos[2] = NULL);

	/// capture image from viewport to user buffer
	virtual bool loadImage(unsigned int *buffer, unsigned int size, unsigned int format, double ts);

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
	virtual void calcImage (unsigned int texId, double ts) { calcViewport(texId, ts, GL_RGBA); }

	/// capture image from viewport
	virtual void calcViewport (unsigned int texId, double ts, unsigned int format);

	/// get viewport size
	GLint * getViewportSize (void) { return m_viewport + 2; }
};

PyObject *ImageViewport_getCaptureSize(PyImage *self, void *closure);
int ImageViewport_setCaptureSize(PyImage *self, PyObject *value, void *closure);
PyObject *ImageViewport_getWhole(PyImage *self, void *closure);
int ImageViewport_setWhole(PyImage *self, PyObject *value, void *closure);
PyObject *ImageViewport_getAlpha(PyImage *self, void *closure);
int ImageViewport_setAlpha(PyImage *self, PyObject *value, void *closure);

#endif

