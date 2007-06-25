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

#include <Common.h>
#include <DirectDrawSurface.h>
#include <BlockDXT.h>

#include <stdio.h> // printf
#include <math.h>  // sqrt

/*** declarations ***/

#if !defined(MAKEFOURCC)
#	define MAKEFOURCC(ch0, ch1, ch2, ch3) \
		((unsigned int)((unsigned char)(ch0)) | \
		((unsigned int)((unsigned char)(ch1)) << 8) | \
		((unsigned int)((unsigned char)(ch2)) << 16) | \
		((unsigned int)((unsigned char)(ch3)) << 24 ))
#endif

static const unsigned int FOURCC_DDS = MAKEFOURCC('D', 'D', 'S', ' ');
static const unsigned int FOURCC_DXT1 = MAKEFOURCC('D', 'X', 'T', '1');
static const unsigned int FOURCC_DXT2 = MAKEFOURCC('D', 'X', 'T', '2');
static const unsigned int FOURCC_DXT3 = MAKEFOURCC('D', 'X', 'T', '3');
static const unsigned int FOURCC_DXT4 = MAKEFOURCC('D', 'X', 'T', '4');
static const unsigned int FOURCC_DXT5 = MAKEFOURCC('D', 'X', 'T', '5');
static const unsigned int FOURCC_RXGB = MAKEFOURCC('R', 'X', 'G', 'B');
static const unsigned int FOURCC_ATI1 = MAKEFOURCC('A', 'T', 'I', '1');
static const unsigned int FOURCC_ATI2 = MAKEFOURCC('A', 'T', 'I', '2');

// RGB formats.
static const unsigned int D3DFMT_R8G8B8 = 20;
static const unsigned int D3DFMT_A8R8G8B8 = 21;
static const unsigned int D3DFMT_X8R8G8B8 = 22;
static const unsigned int D3DFMT_R5G6B5 = 23;
static const unsigned int D3DFMT_X1R5G5B5 = 24;
static const unsigned int D3DFMT_A1R5G5B5 = 25;
static const unsigned int D3DFMT_A4R4G4B4 = 26;
static const unsigned int D3DFMT_R3G3B2 = 27;
static const unsigned int D3DFMT_A8 = 28;
static const unsigned int D3DFMT_A8R3G3B2 = 29;
static const unsigned int D3DFMT_X4R4G4B4 = 30;
static const unsigned int D3DFMT_A2B10G10R10 = 31;
static const unsigned int D3DFMT_A8B8G8R8 = 32;
static const unsigned int D3DFMT_X8B8G8R8 = 33;
static const unsigned int D3DFMT_G16R16 = 34;
static const unsigned int D3DFMT_A2R10G10B10 = 35;
static const unsigned int D3DFMT_A16B16G16R16 = 36;

// Palette formats.
static const unsigned int D3DFMT_A8P8 = 40;
static const unsigned int D3DFMT_P8 = 41;
	
// Luminance formats.
static const unsigned int D3DFMT_L8 = 50;
static const unsigned int D3DFMT_A8L8 = 51;
static const unsigned int D3DFMT_A4L4 = 52;

// Floating point formats
static const unsigned int D3DFMT_R16F = 111;
static const unsigned int D3DFMT_G16R16F = 112;
static const unsigned int D3DFMT_A16B16G16R16F = 113;
static const unsigned int D3DFMT_R32F = 114;
static const unsigned int D3DFMT_G32R32F = 115;
static const unsigned int D3DFMT_A32B32G32R32F = 116;
	
static const unsigned int DDSD_CAPS = 0x00000001U;
static const unsigned int DDSD_PIXELFORMAT = 0x00001000U;
static const unsigned int DDSD_WIDTH = 0x00000004U;
static const unsigned int DDSD_HEIGHT = 0x00000002U;
static const unsigned int DDSD_PITCH = 0x00000008U;
static const unsigned int DDSD_MIPMAPCOUNT = 0x00020000U;
static const unsigned int DDSD_LINEARSIZE = 0x00080000U;
static const unsigned int DDSD_DEPTH = 0x00800000U;
	
static const unsigned int DDSCAPS_COMPLEX = 0x00000008U;
static const unsigned int DDSCAPS_TEXTURE = 0x00001000U;
static const unsigned int DDSCAPS_MIPMAP = 0x00400000U;
static const unsigned int DDSCAPS2_VOLUME = 0x00200000U;
static const unsigned int DDSCAPS2_CUBEMAP = 0x00000200U;

static const unsigned int DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400U;
static const unsigned int DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800U;
static const unsigned int DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000U;
static const unsigned int DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000U;
static const unsigned int DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000U;
static const unsigned int DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000U;
static const unsigned int DDSCAPS2_CUBEMAP_ALL_FACES = 0x0000FC00U;

static const unsigned int DDPF_ALPHAPIXELS = 0x00000001U;
static const unsigned int DDPF_ALPHA = 0x00000002U;
static const unsigned int DDPF_FOURCC = 0x00000004U;
static const unsigned int DDPF_RGB = 0x00000040U;
static const unsigned int DDPF_PALETTEINDEXED1 = 0x00000800U;
static const unsigned int DDPF_PALETTEINDEXED2 = 0x00001000U;
static const unsigned int DDPF_PALETTEINDEXED4 = 0x00000008U;
static const unsigned int DDPF_PALETTEINDEXED8 = 0x00000020U;
static const unsigned int DDPF_LUMINANCE = 0x00020000U;
static const unsigned int DDPF_ALPHAPREMULT = 0x00008000U;
static const unsigned int DDPF_NORMAL = 0x80000000U;	// @@ Custom nv flag.

/*** implementation ***/

void mem_read(Stream & mem, DDSPixelFormat & pf)
{
	mem_read(mem, pf.size);
	mem_read(mem, pf.flags);
	mem_read(mem, pf.fourcc);
	mem_read(mem, pf.bitcount);
	mem_read(mem, pf.rmask);
	mem_read(mem, pf.gmask);
	mem_read(mem, pf.bmask);
	mem_read(mem, pf.amask);
}

void mem_read(Stream & mem, DDSCaps & caps)
{
	mem_read(mem, caps.caps1);
	mem_read(mem, caps.caps2);
	mem_read(mem, caps.caps3);
	mem_read(mem, caps.caps4);
}

void mem_read(Stream & mem, DDSHeader & header)
{
	mem_read(mem, header.fourcc);
	mem_read(mem, header.size);
	mem_read(mem, header.flags);
	mem_read(mem, header.height);
	mem_read(mem, header.width);
	mem_read(mem, header.pitch);
	mem_read(mem, header.depth);
	mem_read(mem, header.mipmapcount);
	for (unsigned int i = 0; i < 11; i++) mem_read(mem, header.reserved[i]);
	mem_read(mem, header.pf);
	mem_read(mem, header.caps);
	mem_read(mem, header.notused);
}

DDSHeader::DDSHeader()
{
	this->fourcc = FOURCC_DDS;
	this->size = 124;
	this->flags  = (DDSD_CAPS|DDSD_PIXELFORMAT);
	this->height = 0;
	this->width = 0;
	this->pitch = 0;
	this->depth = 0;
	this->mipmapcount = 0;
	for (unsigned int i = 0; i < 11; i++) this->reserved[i] = 0;

	// Store version information on the reserved header attributes.
	this->reserved[9] = MAKEFOURCC('N', 'V', 'T', 'T');
	this->reserved[10] = (0 << 16) | (9 << 8) | (3);	// major.minor.revision

	this->pf.size = 32;
	this->pf.flags = 0;
	this->pf.fourcc = 0;
	this->pf.bitcount = 0;
	this->pf.rmask = 0;
	this->pf.gmask = 0;
	this->pf.bmask = 0;
	this->pf.amask = 0;
	this->caps.caps1 = DDSCAPS_TEXTURE;
	this->caps.caps2 = 0;
	this->caps.caps3 = 0;
	this->caps.caps4 = 0;
	this->notused = 0;
}

void DDSHeader::setWidth(unsigned int w)
{
	this->flags |= DDSD_WIDTH;
	this->width = w;
}

void DDSHeader::setHeight(unsigned int h)
{
	this->flags |= DDSD_HEIGHT;
	this->height = h;
}

void DDSHeader::setDepth(unsigned int d)
{
	this->flags |= DDSD_DEPTH;
	this->height = d;
}

void DDSHeader::setMipmapCount(unsigned int count)
{
	if (count == 0)
	{
		this->flags &= ~DDSD_MIPMAPCOUNT;
		this->mipmapcount = 0;

		if (this->caps.caps2 == 0) {
			this->caps.caps1 = DDSCAPS_TEXTURE;
		}
		else {
			this->caps.caps1 = DDSCAPS_TEXTURE | DDSCAPS_COMPLEX;
		}
	}
	else
	{
		this->flags |= DDSD_MIPMAPCOUNT;
		this->mipmapcount = count;

		this->caps.caps1 |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;
	}
}

void DDSHeader::setTexture2D()
{
	// nothing to do here.
}

void DDSHeader::setTexture3D()
{
	this->caps.caps2 = DDSCAPS2_VOLUME;
}

void DDSHeader::setTextureCube()
{
	this->caps.caps1 |= DDSCAPS_COMPLEX;
	this->caps.caps2 = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALL_FACES;
}

void DDSHeader::setLinearSize(unsigned int size)
{
	this->flags &= ~DDSD_PITCH;
	this->flags |= DDSD_LINEARSIZE;
	this->pitch = size;
}

void DDSHeader::setPitch(unsigned int pitch)
{
	this->flags &= ~DDSD_LINEARSIZE;
	this->flags |= DDSD_PITCH;
	this->pitch = pitch;
}

void DDSHeader::setFourCC(unsigned char c0, unsigned char c1, unsigned char c2, unsigned char c3)
{
	// set fourcc pixel format.
	this->pf.flags = DDPF_FOURCC;
	this->pf.fourcc = MAKEFOURCC(c0, c1, c2, c3);
	this->pf.bitcount = 0;
	this->pf.rmask = 0;
	this->pf.gmask = 0;
	this->pf.bmask = 0;
	this->pf.amask = 0;
}

void DDSHeader::setPixelFormat(unsigned int bitcount, unsigned int rmask, unsigned int gmask, unsigned int bmask, unsigned int amask)
{
	// Make sure the masks are correct.
	if ((rmask & gmask) || \
		(rmask & bmask) || \
		(rmask & amask) || \
		(gmask & bmask) || \
		(gmask & amask) || \
		(bmask & amask)) {
		printf("DDS: bad RGBA masks, pixel format not set\n");
		return;
	}

	this->pf.flags = DDPF_RGB;

	if (amask != 0) {
		this->pf.flags |= DDPF_ALPHAPIXELS;
	}

	if (bitcount == 0)
	{
		// Compute bit count from the masks.
		unsigned int total = rmask | gmask | bmask | amask;
		while(total != 0) {
			bitcount++;
			total >>= 1;
		}
		// @@ Align to 8?
	}

	this->pf.fourcc = 0;
	this->pf.bitcount = bitcount;
	this->pf.rmask = rmask;
	this->pf.gmask = gmask;
	this->pf.bmask = bmask;
	this->pf.amask = amask;
}

void DDSHeader::setNormalFlag(bool b)
{
	if (b) this->pf.flags |= DDPF_NORMAL;
	else this->pf.flags &= ~DDPF_NORMAL;
}

/*
void DDSHeader::swapBytes()
{
	this->fourcc = POSH_LittleU32(this->fourcc);
	this->size = POSH_LittleU32(this->size);
	this->flags = POSH_LittleU32(this->flags);
	this->height = POSH_LittleU32(this->height);
	this->width = POSH_LittleU32(this->width);
	this->pitch = POSH_LittleU32(this->pitch);
	this->depth = POSH_LittleU32(this->depth);
	this->mipmapcount = POSH_LittleU32(this->mipmapcount);
	
	for(int i = 0; i < 11; i++) {
		this->reserved[i] = POSH_LittleU32(this->reserved[i]);
	}

	this->pf.size = POSH_LittleU32(this->pf.size);
	this->pf.flags = POSH_LittleU32(this->pf.flags);
	this->pf.fourcc = POSH_LittleU32(this->pf.fourcc);
	this->pf.bitcount = POSH_LittleU32(this->pf.bitcount);
	this->pf.rmask = POSH_LittleU32(this->pf.rmask);
	this->pf.gmask = POSH_LittleU32(this->pf.gmask);
	this->pf.bmask = POSH_LittleU32(this->pf.bmask);
	this->pf.amask = POSH_LittleU32(this->pf.amask);
	this->caps.caps1 = POSH_LittleU32(this->caps.caps1);
	this->caps.caps2 = POSH_LittleU32(this->caps.caps2);
	this->caps.caps3 = POSH_LittleU32(this->caps.caps3);
	this->caps.caps4 = POSH_LittleU32(this->caps.caps4);
	this->notused = POSH_LittleU32(this->notused);
}
*/


DirectDrawSurface::DirectDrawSurface(unsigned char *mem, unsigned int size) : stream(mem, size), header()
{
	mem_read(stream, header);
}

DirectDrawSurface::~DirectDrawSurface()
{
}

bool DirectDrawSurface::isValid() const
{
	if (header.fourcc != FOURCC_DDS || header.size != 124)
	{
		return false;
	}
	
	const unsigned int required = (DDSD_WIDTH|DDSD_HEIGHT|DDSD_CAPS|DDSD_PIXELFORMAT);
	if( (header.flags & required) != required ) {
		return false;
	}
	
	if (header.pf.size != 32) {
		return false;
	}

	/* in some files DDSCAPS_TEXTURE is missing: silently ignore */
	/*
	if( !(header.caps.caps1 & DDSCAPS_TEXTURE) ) {
		return false;
	}
	*/

	return true;
}

bool DirectDrawSurface::isSupported() const
{
	if (header.pf.flags & DDPF_FOURCC)
	{
		if (header.pf.fourcc != FOURCC_DXT1 &&
		    header.pf.fourcc != FOURCC_DXT2 &&
		    header.pf.fourcc != FOURCC_DXT3 &&
		    header.pf.fourcc != FOURCC_DXT4 &&
		    header.pf.fourcc != FOURCC_DXT5 &&
		    header.pf.fourcc != FOURCC_RXGB &&
		    header.pf.fourcc != FOURCC_ATI1 &&
		    header.pf.fourcc != FOURCC_ATI2)
		{
			// Unknown fourcc code.
			return false;
		}
	}
	/*
	else if (header.pf.flags & DDPF_RGB)
	{
		if (header.pf.bitcount == 24)
		{
			return false;
		}
		else if (header.pf.bitcount == 32)
		{
			return false;
		}
		else
		{
			// Unsupported pixel format.
			return false;
		}
	}
	*/
	else
	{
		return false;
	}
	
	if (isTextureCube() && (header.caps.caps2 & DDSCAPS2_CUBEMAP_ALL_FACES) != DDSCAPS2_CUBEMAP_ALL_FACES)
	{
		// Cubemaps must contain all faces.
		return false;
	}
	
	if (isTexture3D())
	{
		// @@ 3D textures not supported yet.
		return false;
	}
	
	return true;
}


unsigned int DirectDrawSurface::mipmapCount() const
{
	if (header.flags & DDSD_MIPMAPCOUNT) return header.mipmapcount;
	else return 0;
}


unsigned int DirectDrawSurface::width() const
{
	if (header.flags & DDSD_WIDTH) return header.width;
	else return 1;
}

unsigned int DirectDrawSurface::height() const
{
	if (header.flags & DDSD_HEIGHT) return header.height;
	else return 1;
}

unsigned int DirectDrawSurface::depth() const
{
	if (header.flags & DDSD_DEPTH) return header.depth;
	else return 1;
}

bool DirectDrawSurface::hasAlpha() const
{
	if (header.pf.fourcc == FOURCC_DXT1) return false;
	else return true;
}

bool DirectDrawSurface::isTexture2D() const
{
	return !isTexture3D() && !isTextureCube();
}

bool DirectDrawSurface::isTexture3D() const
{
	return (header.caps.caps2 & DDSCAPS2_VOLUME) != 0;
}

bool DirectDrawSurface::isTextureCube() const
{
	return (header.caps.caps2 & DDSCAPS2_CUBEMAP) != 0;
}

void DirectDrawSurface::mipmap(Image * img, unsigned int face, unsigned int mipmap)
{
	stream.seek(offset(face, mipmap));
	
	unsigned int w = width();
	unsigned int h = height();
	
	// Compute width and height.
	for (unsigned int m = 0; m < mipmap; m++)
	{
		w = max(w/2, 1U);
		h = max(h/2, 1U);
	}
	
	img->allocate(w, h);
	
	if (header.pf.flags & DDPF_RGB) 
	{
		readLinearImage(img);
	}
	else if (header.pf.flags & DDPF_FOURCC)
	{
		readBlockImage(img);
	}
}

void DirectDrawSurface::readLinearImage(Image * img)
{
	// @@ Read linear RGB images.
	printf("DDS: linear RGB images not supported\n");
}

void DirectDrawSurface::readBlockImage(Image * img)
{
	const unsigned int w = img->width();
	const unsigned int h = img->height();
	
	const unsigned int bw = (w + 3) / 4;
	const unsigned int bh = (h + 3) / 4;
	
	for (unsigned int by = 0; by < bh; by++)
	{
		for (unsigned int bx = 0; bx < bw; bx++)
		{
			ColorBlock block;
			
			// Read color block.
			readBlock(&block);
			
			// Write color block.
			for (unsigned int y = 0; y < min(4U, h-4*by); y++)
			{
				for (unsigned int x = 0; x < min(4U, w-4*bx); x++)
				{
					img->pixel(4*bx+x, 4*by+y) = block.color(x, y);
				}
			}
		}
	}
}

static Color32 buildNormal(unsigned char x, unsigned char y)
{
	float nx = 2 * (x / 255) - 1;
	float ny = 2 * (x / 255) - 1;
	float nz = sqrt(1 - nx*nx - ny*ny);
	unsigned char z = clamp(int(255 * (nz + 1) / 2), 0, 255);
	
	return Color32(x, y, z);
}


void DirectDrawSurface::readBlock(ColorBlock * rgba)
{
	if (header.pf.fourcc == FOURCC_DXT1)
	{
		BlockDXT1 block;
		mem_read(stream, block);
		block.decodeBlock(rgba);
	}
	else if (header.pf.fourcc == FOURCC_DXT2 ||
	    header.pf.fourcc == FOURCC_DXT3)
	{
		BlockDXT3 block;
		mem_read(stream, block);
		block.decodeBlock(rgba);
	}
	else if (header.pf.fourcc == FOURCC_DXT4 ||
	    header.pf.fourcc == FOURCC_DXT5 ||
	    header.pf.fourcc == FOURCC_RXGB)
	{
		BlockDXT5 block;
		mem_read(stream, block);
		block.decodeBlock(rgba);
		
		if (header.pf.fourcc == FOURCC_RXGB)
		{
			// Swap R & A.
			for (int i = 0; i < 16; i++)
			{
				Color32 & c = rgba->color(i);
				unsigned int tmp = c.r;
				c.r = c.a;
				c.a = tmp;
			}
		}
	}
	else if (header.pf.fourcc == FOURCC_ATI1)
	{
		BlockATI1 block;
		mem_read(stream, block);
		block.decodeBlock(rgba);
	}
	else if (header.pf.fourcc == FOURCC_ATI2)
	{
		BlockATI2 block;
		mem_read(stream, block);
		block.decodeBlock(rgba);
	}
	
	// If normal flag set, convert to normal.
	if (header.pf.flags & DDPF_NORMAL)
	{
		if (header.pf.fourcc == FOURCC_ATI2)
		{
			for (int i = 0; i < 16; i++)
			{
				Color32 & c = rgba->color(i);
				c = buildNormal(c.r, c.g);
			}
		}
		else if (header.pf.fourcc == FOURCC_DXT5)
		{
			for (int i = 0; i < 16; i++)
			{
				Color32 & c = rgba->color(i);
				c = buildNormal(c.g, c.a);
			}
		}
	}
}


unsigned int DirectDrawSurface::blockSize() const
{
	switch(header.pf.fourcc)
	{
		case FOURCC_DXT1:
		case FOURCC_ATI1:
			return 8;
		case FOURCC_DXT2:
		case FOURCC_DXT3:
		case FOURCC_DXT4:
		case FOURCC_DXT5:
		case FOURCC_RXGB:
		case FOURCC_ATI2:
			return 16;
	};

	// Not a block image.
	return 0;
}

unsigned int DirectDrawSurface::mipmapSize(unsigned int mipmap) const
{
	unsigned int w = width();
	unsigned int h = height();
	unsigned int d = depth();
	
	for (unsigned int m = 0; m < mipmap; m++)
	{
		w = max(1U, w / 2);
		h = max(1U, h / 2);
		d = max(1U, d / 2);
	}

	if (header.pf.flags & DDPF_FOURCC)
	{
		// @@ How are 3D textures aligned?
		w = (w + 3) / 4;
		h = (h + 3) / 4;
		return blockSize() * w * h;
	}
	else if (header.pf.flags & DDPF_RGB)
	{
		// Align pixels to bytes.
		unsigned int byteCount = (header.pf.bitcount + 7) / 8;
		
		// Align pitch to 4 bytes.
		unsigned int pitch = 4 * ((w * byteCount + 3) / 4);
		
		return pitch * h * d;
	}
	else {
		printf("DDS: mipmap format not supported\n");
		return(0);
	};
}

unsigned int DirectDrawSurface::faceSize() const
{
	const unsigned int count = mipmapCount();
	unsigned int size = 0;
	
	for (unsigned int m = 0; m < count; m++)
	{
		size += mipmapSize(m);
	}
	
	return size;
}

unsigned int DirectDrawSurface::offset(const unsigned int face, const unsigned int mipmap)
{
	unsigned int size = sizeof(DDSHeader);
	
	if (face != 0)
	{
		size += face * faceSize();
	}
	
	for (unsigned int m = 0; m < mipmap; m++)
	{
		size += mipmapSize(m);
	}
	
	return size;
}


void DirectDrawSurface::printInfo() const
{
	/* printf("FOURCC: %c%c%c%c\n", ((unsigned char *)&header.fourcc)[0], ((unsigned char *)&header.fourcc)[1], ((unsigned char *)&header.fourcc)[2], ((unsigned char *)&header.fourcc)[3]); */
	printf("Flags: 0x%.8X\n", header.flags);
	if (header.flags & DDSD_CAPS) printf("\tDDSD_CAPS\n");
	if (header.flags & DDSD_PIXELFORMAT) printf("\tDDSD_PIXELFORMAT\n");
	if (header.flags & DDSD_WIDTH) printf("\tDDSD_WIDTH\n");
	if (header.flags & DDSD_HEIGHT) printf("\tDDSD_HEIGHT\n");
	if (header.flags & DDSD_DEPTH) printf("\tDDSD_DEPTH\n");
	if (header.flags & DDSD_PITCH) printf("\tDDSD_PITCH\n");
	if (header.flags & DDSD_LINEARSIZE) printf("\tDDSD_LINEARSIZE\n");
	if (header.flags & DDSD_MIPMAPCOUNT) printf("\tDDSD_MIPMAPCOUNT\n");

	printf("Height: %d\n", header.height);
	printf("Width: %d\n", header.width);
	printf("Depth: %d\n", header.depth);
	if (header.flags & DDSD_PITCH) printf("Pitch: %d\n", header.pitch);
	else if (header.flags & DDSD_LINEARSIZE) printf("Linear size: %d\n", header.pitch);
	printf("Mipmap count: %d\n", header.mipmapcount);
	
	printf("Pixel Format:\n");
	/* printf("\tSize: %d\n", header.pf.size); */
	printf("\tFlags: 0x%.8X\n", header.pf.flags);
	if (header.pf.flags & DDPF_RGB) printf("\t\tDDPF_RGB\n");
	if (header.pf.flags & DDPF_FOURCC) printf("\t\tDDPF_FOURCC\n");
	if (header.pf.flags & DDPF_ALPHAPIXELS) printf("\t\tDDPF_ALPHAPIXELS\n");
	if (header.pf.flags & DDPF_ALPHA) printf("\t\tDDPF_ALPHA\n");
	if (header.pf.flags & DDPF_PALETTEINDEXED1) printf("\t\tDDPF_PALETTEINDEXED1\n");
	if (header.pf.flags & DDPF_PALETTEINDEXED2) printf("\t\tDDPF_PALETTEINDEXED2\n");
	if (header.pf.flags & DDPF_PALETTEINDEXED4) printf("\t\tDDPF_PALETTEINDEXED4\n");
	if (header.pf.flags & DDPF_PALETTEINDEXED8) printf("\t\tDDPF_PALETTEINDEXED8\n");
	if (header.pf.flags & DDPF_ALPHAPREMULT) printf("\t\tDDPF_ALPHAPREMULT\n");
	if (header.pf.flags & DDPF_NORMAL) printf("\t\tDDPF_NORMAL\n");
	
	printf("\tFourCC: '%c%c%c%c'\n", ((header.pf.fourcc >> 0) & 0xFF), ((header.pf.fourcc >> 8) & 0xFF), ((header.pf.fourcc >> 16) & 0xFF), ((header.pf.fourcc >> 24) & 0xFF));
	printf("\tBit count: %d\n", header.pf.bitcount);
	printf("\tRed mask: 0x%.8X\n", header.pf.rmask);
	printf("\tGreen mask: 0x%.8X\n", header.pf.gmask);
	printf("\tBlue mask: 0x%.8X\n", header.pf.bmask);
	printf("\tAlpha mask: 0x%.8X\n", header.pf.amask);

	printf("Caps:\n");
	printf("\tCaps 1: 0x%.8X\n", header.caps.caps1);
	if (header.caps.caps1 & DDSCAPS_COMPLEX) printf("\t\tDDSCAPS_COMPLEX\n");
	if (header.caps.caps1 & DDSCAPS_TEXTURE) printf("\t\tDDSCAPS_TEXTURE\n");
	if (header.caps.caps1 & DDSCAPS_MIPMAP) printf("\t\tDDSCAPS_MIPMAP\n");

	printf("\tCaps 2: 0x%.8X\n", header.caps.caps2);
	if (header.caps.caps2 & DDSCAPS2_VOLUME) printf("\t\tDDSCAPS2_VOLUME\n");
	else if (header.caps.caps2 & DDSCAPS2_CUBEMAP)
	{
		printf("\t\tDDSCAPS2_CUBEMAP\n");
		if ((header.caps.caps2 & DDSCAPS2_CUBEMAP_ALL_FACES) == DDSCAPS2_CUBEMAP_ALL_FACES) printf("\t\tDDSCAPS2_CUBEMAP_ALL_FACES\n");
		else {
			if (header.caps.caps2 & DDSCAPS2_CUBEMAP_POSITIVEX) printf("\t\tDDSCAPS2_CUBEMAP_POSITIVEX\n");
			if (header.caps.caps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) printf("\t\tDDSCAPS2_CUBEMAP_NEGATIVEX\n");
			if (header.caps.caps2 & DDSCAPS2_CUBEMAP_POSITIVEY) printf("\t\tDDSCAPS2_CUBEMAP_POSITIVEY\n");
			if (header.caps.caps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) printf("\t\tDDSCAPS2_CUBEMAP_NEGATIVEY\n");
			if (header.caps.caps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) printf("\t\tDDSCAPS2_CUBEMAP_POSITIVEZ\n");
			if (header.caps.caps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ) printf("\t\tDDSCAPS2_CUBEMAP_NEGATIVEZ\n");
		}
	}

	printf("\tCaps 3: 0x%.8X\n", header.caps.caps3);
	printf("\tCaps 4: 0x%.8X\n", header.caps.caps4);

	if (header.reserved[9] == MAKEFOURCC('N', 'V', 'T', 'T'))
	{
		int major = (header.reserved[10] >> 16) & 0xFF;
		int minor = (header.reserved[10] >> 8) & 0xFF;
		int revision= header.reserved[10] & 0xFF;
		
		printf("Version:\n");
		printf("\tNVIDIA Texture Tools %d.%d.%d\n", major, minor, revision);
	}
}

