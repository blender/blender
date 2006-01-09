/**
*
 * ***** BEGIN GPLLICENSE BLOCK *****
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Copyright by Gernot Ziegler <gz@lysator.liu.se>.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Austin Benesh, Ton Roosendaal (float, half, speedup, cleanup...).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string>


#include <openexr_api.h>

extern "C"
{
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
	
#include "IMB_allocimbuf.h"
}

#include <iostream>

#include <OpenEXR/half.h>
#include <OpenEXR/ImfVersion.h>
#include <OpenEXR/ImathBox.h>
#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfIO.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/ImfCompressionAttribute.h>

using namespace Imf;
using namespace Imath;

class Mem_IStream: public IStream
{
public:
	
	Mem_IStream (unsigned char *exrbuf, int exrsize):
    IStream("dummy"), _exrpos (0), _exrsize(exrsize)  { _exrbuf = exrbuf; }
	
	virtual bool	read (char c[], int n);
	virtual Int64	tellg ();
	virtual void	seekg (Int64 pos);
	virtual void	clear ();
	//virtual ~Mem_IStream() {}; // unused
	
private:
		
		Int64 _exrpos;
	Int64 _exrsize;
	unsigned char *_exrbuf;
};

bool Mem_IStream::read (char c[], int n)
{
	if (n + _exrpos <= _exrsize)
    {
		memcpy(c, (void *)(&_exrbuf[_exrpos]), n);
		_exrpos += n;
		return true;
    }
	else
		return false;
}

Int64 Mem_IStream::tellg ()
{
	return _exrpos;
}

void Mem_IStream::seekg (Int64 pos)
{
	_exrpos = pos;
}

void Mem_IStream::clear () 
{ 
}

struct _RGBAZ
{
	half r;
	half g;
	half b;
	half a;
	half z;
};

typedef struct _RGBAZ RGBAZ;

extern "C"
{
	
int imb_is_a_openexr(unsigned char *mem)
{
	return Imf::isImfMagic ((const char *)mem);
}

static void openexr_header_compression(Header *header, int compression)
{
	switch(compression)
	{
		case 0:
			header->compression() = NO_COMPRESSION;
			break;
		case 1:
			header->compression() = PXR24_COMPRESSION;
			break;
		case 2:
			header->compression() = ZIP_COMPRESSION;
			break;
		case 3:
			header->compression() = PIZ_COMPRESSION;
			break;
		case 4:
			header->compression() = RLE_COMPRESSION;
			break;
		default:
			header->compression() = NO_COMPRESSION;
			break; 
	}
}

static short imb_save_openexr_half(struct ImBuf *ibuf, char *name, int flags)
{
	
	int width = ibuf->x;
	int height = ibuf->y;
	int write_zbuf = (flags & IB_zbuf) && ibuf->zbuf != NULL;   // summarize
	
	try
	{
		Header header (width, height);
		
		openexr_header_compression(&header, ibuf->ftype & OPENEXR_COMPRESS);
		
		header.channels().insert ("R", Channel (HALF));
		header.channels().insert ("G", Channel (HALF));
		header.channels().insert ("B", Channel (HALF));
		header.channels().insert ("A", Channel (HALF));
		if (write_zbuf)		// z we do as uint always
			header.channels().insert ("Z", Channel (UINT));
		
		FrameBuffer frameBuffer;			
		OutputFile *file = new OutputFile(name, header);			
		
		/* we store first everything in half array */
		RGBAZ *pixels = new RGBAZ[height * width];
		RGBAZ *to = pixels;
		int xstride= sizeof (RGBAZ);
		int ystride= xstride*width;
		
		/* indicate used buffers */
		frameBuffer.insert ("R", Slice (HALF,  (char *) &pixels[0].r, xstride, ystride));	
		frameBuffer.insert ("G", Slice (HALF,  (char *) &pixels[0].g, xstride, ystride));
		frameBuffer.insert ("B", Slice (HALF,  (char *) &pixels[0].b, xstride, ystride));
		frameBuffer.insert ("A", Slice (HALF, (char *) &pixels[0].a, xstride, ystride));
	
		if (write_zbuf)
			frameBuffer.insert ("Z", Slice (UINT, (char *) ibuf->zbuf + 4*(height-1)*width,
											sizeof(int), sizeof(int) * -width));
		if(ibuf->rect_float) {
			float *from;
			
			for (int i = ibuf->y-1; i >= 0; i--) 
			{
				from= ibuf->rect_float + 4*i*width;
				
				for (int j = ibuf->x; j > 0; j--) 
				{
					to->r = from[0];
					to->g = from[1];
					to->b = from[2];
					to->a = from[3];
					to++; from += 4;
				}
			}
		}
		else {
			unsigned char *from;
			
			for (int i = ibuf->y-1; i >= 0; i--) 
			{
				from= (unsigned char *)(ibuf->rect + i*width);
				
				for (int j = ibuf->x; j > 0; j--) 
				{
					to->r = (float)(from[0])/255.0;
					to->g = (float)(from[1])/255.0;
					to->b = (float)(from[2])/255.0;
					to->a = (float)(from[3])/255.0;
					to++; from += 4;
				}
			}
		}
		
//		printf("OpenEXR-save: Writing OpenEXR file of height %d.\n", height);
		
		file->setFrameBuffer (frameBuffer);				  
		file->writePixels (height);					  
		delete file;
	}
	catch (const std::exception &exc)
	{      
		printf("OpenEXR-save: ERROR: %s\n", exc.what());
		if (ibuf) IMB_freeImBuf(ibuf);
		
		return (0);
	}
	
	return (1);
}

static short imb_save_openexr_float(struct ImBuf *ibuf, char *name, int flags)
{
	
	int width = ibuf->x;
	int height = ibuf->y;
	int write_zbuf = (flags & IB_zbuf) && ibuf->zbuf != NULL;   // summarize
	
	try
	{
		Header header (width, height);
		
		openexr_header_compression(&header, ibuf->ftype & OPENEXR_COMPRESS);
		
		header.channels().insert ("R", Channel (FLOAT));
		header.channels().insert ("G", Channel (FLOAT));
		header.channels().insert ("B", Channel (FLOAT));
		header.channels().insert ("A", Channel (FLOAT));
		if (write_zbuf)
			header.channels().insert ("Z", Channel (UINT));
		
		FrameBuffer frameBuffer;			
		OutputFile *file = new OutputFile(name, header);			
		float *first= ibuf->rect_float + 4*(height-1)*width;
		int xstride = sizeof(float) * 4;
		int ystride = - xstride*width;

		frameBuffer.insert ("R", Slice (FLOAT,  (char *) first, xstride, ystride));
		frameBuffer.insert ("G", Slice (FLOAT,  (char *) (first+1), xstride, ystride));
		frameBuffer.insert ("B", Slice (FLOAT,  (char *) (first+2), xstride, ystride));
		frameBuffer.insert ("A", Slice (FLOAT,  (char *) (first+3), xstride, ystride));

		if (write_zbuf)
			frameBuffer.insert ("Z", Slice (UINT, (char *) ibuf->zbuf + 4*(height-1)*width,
											sizeof(int), sizeof(int) * -width));
		
		file->setFrameBuffer (frameBuffer);				  
		file->writePixels (height);					  
		delete file;
	}
	catch (const std::exception &exc)
	{      
		printf("OpenEXR-save: ERROR: %s\n", exc.what());
		if (ibuf) IMB_freeImBuf(ibuf);
		
		return (0);
	}
	
	return (1);
	//	printf("OpenEXR-save: Done.\n");
}


short imb_save_openexr(struct ImBuf *ibuf, char *name, int flags)
{
	if (flags & IB_mem) 
	{
		printf("OpenEXR-save: Create EXR in memory CURRENTLY NOT SUPPORTED !\n");
		imb_addencodedbufferImBuf(ibuf);
		ibuf->encodedsize = 0;	  
		return(0);
	} 
	
	if (ibuf->ftype & OPENEXR_HALF) 
		return imb_save_openexr_half(ibuf, name, flags);
	else {
		/* when no float rect, we save as half (16 bits is sufficient) */
		if (ibuf->rect_float==NULL)
			return imb_save_openexr_half(ibuf, name, flags);
		else
			return imb_save_openexr_float(ibuf, name, flags);
	}
}

struct ImBuf *imb_load_openexr(unsigned char *mem, int size, int flags)
{
	struct ImBuf *ibuf = 0;
	InputFile *file = NULL;
	
//	printf("OpenEXR-load: testing input, size is %d\n", size);
	if (imb_is_a_openexr(mem) == 0) return(NULL);
	
	try
	{
//		printf("OpenEXR-load: Creating InputFile from mem source\n");
		Mem_IStream membuf(mem, size); 
		file = new InputFile(membuf);
		
		Box2i dw = file->header().dataWindow();
		int width  = dw.max.x - dw.min.x + 1;
		int height = dw.max.y - dw.min.y + 1;
		
//		printf("OpenEXR-load: image data window %d %d %d %d\n", 
//			   dw.min.x, dw.min.y, dw.max.x, dw.max.y);
		
		const ChannelList &channels = file->header().channels();
		
		for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
		{
			const Channel &channel = i.channel();
//			printf("OpenEXR-load: Found channel %s of type %d\n", i.name(), channel.type);
			if (channel.type != 1)
			{
				printf("OpenEXR-load: Can only process HALF input !!\n");
				return(NULL);
			}
		}
		
		RGBAZ *pixels = new RGBAZ[height * width];
		
		FrameBuffer frameBuffer;
		
		frameBuffer.insert ("R",
							Slice (HALF,
								   (char *) &pixels[0].r,
								   sizeof (pixels[0]) * 1,
								   sizeof (pixels[0]) * width));
		
		frameBuffer.insert ("G",
							Slice (HALF,
								   (char *) &pixels[0].g,
								   sizeof (pixels[0]) * 1,
								   sizeof (pixels[0]) * width));
		
		frameBuffer.insert ("B",
							Slice (HALF,
								   (char *) &pixels[0].b,
								   sizeof (pixels[0]) * 1,
								   sizeof (pixels[0]) * width));
		
		frameBuffer.insert ("A",
							Slice (HALF,
								   (char *) &pixels[0].a,
								   sizeof (pixels[0]) * 1,
								   sizeof (pixels[0]) * width));
		
		// FIXME ? Would be able to read Z data or other channels here ! 
		
//		printf("OpenEXR-load: Reading pixel data\n");
		file->setFrameBuffer (frameBuffer);
		file->readPixels (dw.min.y, dw.max.y);
		
//		printf("OpenEXR-load: Converting to Blender float ibuf\n");
		
		int bytesperpixel = 4; // since OpenEXR fills in unknown channels
		ibuf = IMB_allocImBuf(width, height, 8 * bytesperpixel, 0, 0);
		
		if (ibuf) 
		{
			ibuf->ftype = OPENEXR;
			
			imb_addrectImBuf(ibuf);
			imb_addrectfloatImBuf(ibuf);
			
			if (!(flags & IB_test))
			{
				unsigned char *to = (unsigned char *) ibuf->rect;
				float *tof = ibuf->rect_float;
				RGBAZ *from = pixels;
				RGBAZ prescale;
				
				for (int i = ibuf->x * ibuf->y; i > 0; i--) 
				{
					to[0] = (unsigned char)(from->r > 1.0 ? 1.0 : (float)from->r)  * 255;
					to[1] = (unsigned char)(from->g > 1.0 ? 1.0 : (float)from->g)  * 255;
					to[2] = (unsigned char)(from->b > 1.0 ? 1.0 : (float)from->b)  * 255;
					to[3] = (unsigned char)(from->a > 1.0 ? 1.0 : (float)from->a)  * 255;
					to += 4; 
					
					tof[0] = from->r;
					tof[1] = from->g;
					tof[2] = from->b;
					tof[3] = from->a;
					
					from++;
				}
			}
			
			IMB_flipy(ibuf);
			
		} 
		else 
			printf("Couldn't allocate memory for OpenEXR image\n");
		
//		printf("OpenEXR-load: Done\n");
		
		return(ibuf);
	}
	catch (const std::exception &exc)
	{
		std::cerr << exc.what() << std::endl;
		if (ibuf) IMB_freeImBuf(ibuf);
		
		return (0);
	}
	
}


} // export "C"
