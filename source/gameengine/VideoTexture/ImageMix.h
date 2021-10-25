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

/** \file ImageMix.h
 *  \ingroup bgevideotex
 */

#ifndef __IMAGEMIX_H__
#define __IMAGEMIX_H__


#include "Common.h"

#include "ImageBase.h"
#include "FilterBase.h"


/// class for source mixing
class ImageSourceMix : public ImageSource
{
public:
	/// constructor
	ImageSourceMix (const char *id) : ImageSource(id), m_offset(0), m_weight(0x100) {}
	/// destructor
	virtual ~ImageSourceMix (void) {}

	/// get offset
	long long getOffset (void) { return m_offset; }
	/// set offset
	void setOffset (unsigned int * firstImg) { m_offset = m_image - firstImg; }

	/// get weight
	short getWeight (void) { return m_weight; }
	/// set weight
	void setWeight (short weight) { m_weight = weight; }

protected:
	/// buffer offset to the first source buffer
	long long m_offset;
	/// source weight
	short m_weight;
};


/// class for image mixer
class ImageMix : public ImageBase
{
public:
	/// constructor
	ImageMix (void) : ImageBase(false) {}

	/// destructor
	virtual ~ImageMix (void) {}

	/// get weight
	short getWeight(const char *id);
	/// set weight
	bool setWeight(const char *id, short weight);

protected:

	/// create new source
	virtual ImageSource *newSource(const char *id) { return new ImageSourceMix(id); }

	/// calculate image from sources and set its availability
	virtual void calcImage (unsigned int texId, double ts);
};


/// pixel filter for image mixer
class FilterImageMix : public FilterBase
{
public:
	/// constructor
	FilterImageMix (ImageSourceList & sources) : m_sources(sources) {}
	/// destructor
	virtual ~FilterImageMix (void) {}

protected:
	/// source list
	ImageSourceList & m_sources;

	/// filter pixel, source int buffer
	virtual unsigned int filter (unsigned int * src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val = 0)
	{
		// resulting pixel color
		int color[] = {0, 0, 0, 0};
		// iterate sources
		for (ImageSourceList::iterator it = m_sources.begin(); it != m_sources.end(); ++it)
		{
			// get pointer to mixer source
			ImageSourceMix * mixSrc = static_cast<ImageSourceMix*>(*it);
			// add weighted source pixel to result
			color[0] += mixSrc->getWeight() * (src[mixSrc->getOffset()] & 0xFF);
			color[1] += mixSrc->getWeight() * ((src[mixSrc->getOffset()] >> 8) & 0xFF);
			color[2] += mixSrc->getWeight() * ((src[mixSrc->getOffset()] >> 16) & 0xFF);
			color[3] += mixSrc->getWeight() * ((src[mixSrc->getOffset()] >> 24) & 0xFF);
		}
		// return resulting color
		return ((color[0] >> 8) & 0xFF) | (color[1] & 0xFF00)
			| ((color[2] << 8) & 0xFF0000) | ((color[3] << 16) & 0xFF000000);
	}
};


#endif

