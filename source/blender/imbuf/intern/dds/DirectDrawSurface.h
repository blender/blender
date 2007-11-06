/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributors: Amorilia (amorilia@gamebox.net)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*
 * This file is based on a similar file from the NVIDIA texture tools
 * (http://nvidia-texture-tools.googlecode.com/)
 *
 * Original license from NVIDIA follows.
 */

// Copyright NVIDIA Corporation 2007 -- Ignacio Castano <icastano@nvidia.com>
// 
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#ifndef _DDS_DIRECTDRAWSURFACE_H
#define _DDS_DIRECTDRAWSURFACE_H

#include <Stream.h>
#include <ColorBlock.h>
#include <Image.h>

struct DDSPixelFormat {
	unsigned int size;
	unsigned int flags;
	unsigned int fourcc;
	unsigned int bitcount;
	unsigned int rmask;
	unsigned int gmask;
	unsigned int bmask;
	unsigned int amask;
};

struct DDSCaps {
	unsigned int caps1;
	unsigned int caps2;
	unsigned int caps3;
	unsigned int caps4;
};

/// DDS file header.
struct DDSHeader {
	unsigned int fourcc;
	unsigned int size;
	unsigned int flags;
	unsigned int height;
	unsigned int width;
	unsigned int pitch;
	unsigned int depth;
	unsigned int mipmapcount;
	unsigned int reserved[11];
	DDSPixelFormat pf;
	DDSCaps caps;
	unsigned int notused;
	
	// Helper methods.
	DDSHeader();
	
	void setWidth(unsigned int w);
	void setHeight(unsigned int h);
	void setDepth(unsigned int d);
	void setMipmapCount(unsigned int count);
	void setTexture2D();
	void setTexture3D();
	void setTextureCube();
	void setLinearSize(unsigned int size);
	void setPitch(unsigned int pitch);
	void setFourCC(unsigned char c0, unsigned char c1, unsigned char c2, unsigned char c3);
	void setPixelFormat(unsigned int bitcount, unsigned int rmask, unsigned int gmask, unsigned int bmask, unsigned int amask);
	void setNormalFlag(bool b);
	
	/* void swapBytes(); */
};

/// DirectDraw Surface. (DDS)
class DirectDrawSurface
{
public:
	DirectDrawSurface(unsigned char *mem, unsigned int size);
	~DirectDrawSurface();
	
	bool isValid() const;
	bool isSupported() const;
	
	unsigned int mipmapCount() const;
	unsigned int width() const;
	unsigned int height() const;
	unsigned int depth() const;
	bool isTexture2D() const;
	bool isTexture3D() const;
	bool isTextureCube() const;
	bool hasAlpha() const; /* false for DXT1, true for all others */
	
	void mipmap(Image * img, unsigned int f, unsigned int m);
	
	void printInfo() const;

private:
	
	unsigned int blockSize() const;
	unsigned int faceSize() const;
	unsigned int mipmapSize(unsigned int m) const;
	
	unsigned int offset(unsigned int f, unsigned int m);
	
	void readLinearImage(Image * img);
	void readBlockImage(Image * img);
	void readBlock(ColorBlock * rgba);
	
	
private:
	Stream stream; // memory where DDS file resides
	DDSHeader header;
};

void mem_read(Stream & mem, DDSPixelFormat & pf);
void mem_read(Stream & mem, DDSCaps & caps);
void mem_read(Stream & mem, DDSHeader & header);

#endif // _DDS_DIRECTDRAWSURFACE_H
