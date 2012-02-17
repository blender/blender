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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributors: Amorilia (amorilia@users.sourceforge.net)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/dds/DirectDrawSurface.h
 *  \ingroup imbdds
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

#ifndef __DIRECTDRAWSURFACE_H__
#define __DIRECTDRAWSURFACE_H__

#include <Common.h>
#include <Stream.h>
#include <ColorBlock.h>
#include <Image.h>

struct DDSPixelFormat
{
	uint size;
	uint flags;
	uint fourcc;
	uint bitcount;
	uint rmask;
	uint gmask;
	uint bmask;
	uint amask;
};

struct DDSCaps
{
	uint caps1;
	uint caps2;
	uint caps3;
	uint caps4;
};

/// DDS file header for DX10.
struct DDSHeader10
{
	uint dxgiFormat;
	uint resourceDimension;
	uint miscFlag;
	uint arraySize;
	uint reserved;
};

/// DDS file header.
struct DDSHeader
{
	uint fourcc;
	uint size;
	uint flags;
	uint height;
	uint width;
	uint pitch;
	uint depth;
	uint mipmapcount;
	uint reserved[11];
	DDSPixelFormat pf;
	DDSCaps caps;
	uint notused;
	DDSHeader10 header10;


	// Helper methods.
	DDSHeader();
	
	void setWidth(uint w);
	void setHeight(uint h);
	void setDepth(uint d);
	void setMipmapCount(uint count);
	void setTexture2D();
	void setTexture3D();
	void setTextureCube();
	void setLinearSize(uint size);
	void setPitch(uint pitch);
	void setFourCC(uint8 c0, uint8 c1, uint8 c2, uint8 c3);
	void setFormatCode(uint code);
	void setSwizzleCode(uint8 c0, uint8 c1, uint8 c2, uint8 c3);
	void setPixelFormat(uint bitcount, uint rmask, uint gmask, uint bmask, uint amask);
	void setDX10Format(uint format);
	void setNormalFlag(bool b);
    void setSrgbFlag(bool b);
	void setHasAlphaFlag(bool b);
        void setUserVersion(int version);
	
	/*void swapBytes();*/
	
	bool hasDX10Header() const;
    uint signature() const;
    uint toolVersion() const;
    uint userVersion() const;
    bool isNormalMap() const;
    bool isSrgb() const;
    bool hasAlpha() const;
    uint d3d9Format() const;
};

/// DirectDraw Surface. (DDS)
class DirectDrawSurface
{
public:
	DirectDrawSurface(unsigned char *mem, uint size);
	~DirectDrawSurface();
	
	bool isValid() const;
	bool isSupported() const;

	bool hasAlpha() const;
	
	uint mipmapCount() const;
	uint width() const;
	uint height() const;
	uint depth() const;
	bool isTexture1D() const;
	bool isTexture2D() const;
	bool isTexture3D() const;
	bool isTextureCube() const;

	void setNormalFlag(bool b);
	void setHasAlphaFlag(bool b);
        void setUserVersion(int version);
	
	void mipmap(Image * img, uint f, uint m);
	//	void mipmap(FloatImage * img, uint f, uint m);
	
	void printInfo() const;

private:
	
	uint blockSize() const;
	uint faceSize() const;
	uint mipmapSize(uint m) const;
	
	uint offset(uint f, uint m);
	
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
void mem_read(Stream & mem, DDSHeader10 & header);

#endif // __DIRECTDRAWSURFACE_H__
