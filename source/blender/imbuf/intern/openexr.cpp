/**
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
 * Copyright by Gernot Ziegler <gz@lysator.liu.se>.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string>

#include <iostream>

#include <half.h>
#include <ImfVersion.h>
#include <ImathBox.h>
#include <ImfArray.h>
#include <ImfIO.h>
#include <ImfChannelList.h>
#include <ImfPixelType.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>

using namespace Imf;
using namespace Imath;

extern "C"
{
#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_cmap.h"
}


int imb_is_a_openexr(void *mem)
{
	return Imf::isImfMagic ((const char *)mem);
}


class Mem_IStream: public IStream
{
	public:

		Mem_IStream (unsigned char *exrbuf, int exrsize):
		IStream("dummy"), _exrpos (0), _exrsize(exrsize)  { _exrbuf = exrbuf; }

		virtual bool  read (char c[], int n);
		virtual Int64 tellg ();
		virtual void  seekg (Int64 pos);
		virtual void  clear ();
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
	short imb_save_openexr(struct ImBuf *ibuf, char *name, int flags)
	{
		int width = ibuf->x;
		int height = ibuf->y;
		int i;

												  // summarize
		int write_zbuf = (flags & IB_zbuf) && ibuf->zbuf != 0;

		printf("OpenEXR-save: Saving %s image of %d x %d\n",
			write_zbuf ? "RGBAZ" : "RGBA", width, height);

		try
		{
			Header header (width, height);
			header.channels().insert ("R", Channel (HALF));
			header.channels().insert ("G", Channel (HALF));
			header.channels().insert ("B", Channel (HALF));
			header.channels().insert ("A", Channel (HALF));
			if (write_zbuf)
				header.channels().insert ("Z", Channel (HALF));

			FrameBuffer frameBuffer;
			OutputFile *file;

			if (flags & IB_mem)
			{
				printf("OpenEXR-save: Create EXR in memory CURRENTLY NOT SUPPORTED !\n");
				imb_addencodedbufferImBuf(ibuf);
				ibuf->encodedsize = 0;
				return(0);
			}
			else
			{
				printf("OpenEXR-save: Creating output file %s\n", name);
				file = new OutputFile(name, header);
			}

			RGBAZ *pixels = new RGBAZ[height * width];

			int bytesperpixel = (ibuf->depth + 7) >> 3;
			if ((bytesperpixel > 4) || (bytesperpixel == 2))
			{
				printf("OpenEXR-save: unsupported bytes per pixel: %d\n", bytesperpixel);
				return (0);
			}

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

			if (write_zbuf)
				frameBuffer.insert ("Z",
					Slice (HALF,
					(char *) &pixels[0].z,
					sizeof (pixels[0]) * 1,
					sizeof (pixels[0]) * width));

			if (!ibuf->rect_float)
			{
				printf("OpenEXR-save: Converting Blender 8/8/8/8 pixels to OpenEXR format\n");

				RGBAZ *to = pixels;
				unsigned char *from = (unsigned char *) ibuf->rect;

				for (i = ibuf->x * ibuf->y; i > 0; i--)
				{
					to->r = (float)(from[0])/255.0;
					to->g = (float)(from[1])/255.0;
					to->b = (float)(from[2])/255.0;
					to->a = (float)(from[3])/255.0;
					to++; from += 4;
				}
			}
			else
			{
				printf("OpenEXR-save: Converting Blender FLOAT pixels to OpenEXR format\n");

				RGBAZ *to = pixels;
				float *from = ibuf->rect_float;

				for (i = ibuf->x * ibuf->y; i > 0; i--)
				{
					to->r = from[0];
					to->g = from[1];
					to->b = from[2];
					to->a = from[3];
					to++; from += 4;
				}
			}

			if (write_zbuf)
			{
				RGBAZ *to = pixels;
				int *fromz = ibuf->zbuf;

				for (int i = ibuf->x * ibuf->y; i > 0; i--)
				{
					to->z =  (0.5+((float)(*fromz/65536)/65536.0));
					to++; fromz ++;
				}
			}

			printf("OpenEXR-save: Writing OpenEXR file of height %d.\n", height);

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
		printf("OpenEXR-save: Done.\n");
	}

	struct ImBuf *imb_load_openexr(unsigned char *mem, int size, int flags)
	{
		struct ImBuf *ibuf = 0;

		printf("OpenEXR-load: testing input, size is %d\n", size);
		if (imb_is_a_openexr(mem) == 0) return(0);

		InputFile *file = NULL;

		try
		{
			printf("OpenEXR-load: Creating InputFile from mem source\n");
			Mem_IStream membuf(mem, size);
			file = new InputFile(membuf);

			Box2i dw = file->header().dataWindow();
			int width  = dw.max.x - dw.min.x + 1;
			int height = dw.max.y - dw.min.y + 1;

			printf("OpenEXR-load: image data window %d %d %d %d\n",
				dw.min.x, dw.min.y, dw.max.x, dw.max.y);

			const ChannelList &channels = file->header().channels();

			for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
			{
				const Channel &channel = i.channel();
				printf("OpenEXR-load: Found channel %s of type %d\n", i.name(), channel.type);
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

			printf("OpenEXR-load: Reading pixel data\n");
			file->setFrameBuffer (frameBuffer);
			file->readPixels (dw.min.y, dw.max.y);

			printf("OpenEXR-load: Converting to Blender ibuf\n");

			int bytesperpixel = 4;				  // since OpenEXR fills in unknown channels
			ibuf = IMB_allocImBuf(width, height, 8 * bytesperpixel, 0, 0);

			if (ibuf)
			{
				ibuf->ftype = PNG;
				imb_addrectImBuf(ibuf);

				if (!(flags & IB_test))
				{
					unsigned char *to = (unsigned char *) ibuf->rect;
					RGBAZ *from = pixels;
					RGBAZ prescale;

					for (int i = ibuf->x * ibuf->y; i > 0; i--)
					{
						to[0] = (unsigned char)(((float)from->r > 1.0) ? 1.0 : (float)from->r)  * 255;
						to[1] = (unsigned char)(((float)from->g > 1.0) ? 1.0 : (float)from->g)  * 255;
						to[2] = (unsigned char)(((float)from->b > 1.0) ? 1.0 : (float)from->b)  * 255;
						to[3] = (unsigned char)(((float)from->a > 1.0) ? 1.0 : (float)from->a)  * 255;
						to += 4; from++;
					}
				}

			}
			else
				printf("Couldn't allocate memory for PNG image\n");

			printf("OpenEXR-load: Done\n");

			return(ibuf);
		}
		catch (const std::exception &exc)
		{
			std::cerr << exc.what() << std::endl;
			if (ibuf) IMB_freeImBuf(ibuf);

			return (0);
		}

	}

}												  // export "C"
