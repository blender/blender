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

#if !defined IMAGEBUFF_H
#define IMAGEBUFF_H


#include "Common.h"

#include "ImageBase.h"


/// class for image buffer
class ImageBuff : public ImageBase
{
public:
	/// constructor
	ImageBuff (void) : ImageBase(true) {}

	/// destructor
	virtual ~ImageBuff (void) {}

	/// load image from buffer
	void load (unsigned char * img, short width, short height);

	/// refresh image - do nothing
	virtual void refresh (void) {}
};


#endif

