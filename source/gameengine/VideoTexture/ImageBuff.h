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

/** \file ImageBuff.h
 *  \ingroup bgevideotex
 */
 
#ifndef __IMAGEBUFF_H__
#define __IMAGEBUFF_H__


#include "Common.h"

#include "ImageBase.h"

struct ImBuf;

/// class for image buffer
class ImageBuff : public ImageBase
{
private:
	struct ImBuf* m_imbuf;		// temporary structure for buffer manipulation
public:
	/// constructor
	ImageBuff (void) : ImageBase(true), m_imbuf(NULL) {}

	/// destructor
	virtual ~ImageBuff (void);

	/// load image from buffer
	void load (unsigned char * img, short width, short height);
	/// clear image with color set on RGB channels and 0xFF on alpha channel
	void clear (short width, short height, unsigned char color);

	/// plot image from extern RGBA buffer to image at position x,y using one of IMB_BlendMode
	void plot (unsigned char * img, short width, short height, short x, short y, short mode);
	/// plot image from other ImageBuf to image at position x,y using one of IMB_BlendMode
	void plot (ImageBuff* img, short x, short y, short mode);

	/// refresh image - do nothing
	virtual void refresh (void) {}
};


#endif

