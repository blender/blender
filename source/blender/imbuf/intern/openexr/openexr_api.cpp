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
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"

#define WITH_OPENEXR
#include "openexr_multi.h"
}

#include <iostream>

#if defined (_WIN32) && !defined(FREE_WINDOWS)
#include <half.h>
#include <IlmImf/ImfVersion.h>
#include <IlmImf/ImfArray.h>
#include <IlmImf/ImfIO.h>
#include <IlmImf/ImfChannelList.h>
#include <IlmImf/ImfPixelType.h>
#include <IlmImf/ImfInputFile.h>
#include <IlmImf/ImfOutputFile.h>
#include <IlmImf/ImfCompression.h>
#include <IlmImf/ImfCompressionAttribute.h>
#include <IlmImf/ImfStringAttribute.h>
#include <Imath/ImathBox.h>
#else
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
#include <OpenEXR/ImfStringAttribute.h>
#endif

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
	int write_zbuf = (flags & IB_zbuffloat) && ibuf->zbuf_float != NULL;   // summarize
	
	try
	{
		Header header (width, height);
		
		openexr_header_compression(&header, ibuf->ftype & OPENEXR_COMPRESS);
		
		header.channels().insert ("R", Channel (HALF));
		header.channels().insert ("G", Channel (HALF));
		header.channels().insert ("B", Channel (HALF));
		if (ibuf->depth==32)
			header.channels().insert ("A", Channel (HALF));
		if (write_zbuf)		// z we do as float always
			header.channels().insert ("Z", Channel (FLOAT));
		
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
		if (ibuf->depth==32)
			frameBuffer.insert ("A", Slice (HALF, (char *) &pixels[0].a, xstride, ystride));
		if (write_zbuf)
			frameBuffer.insert ("Z", Slice (FLOAT, (char *) ibuf->zbuf_float + 4*(height-1)*width,
											sizeof(float), sizeof(float) * -width));
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
		delete pixels;
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
	int write_zbuf = (flags & IB_zbuffloat) && ibuf->zbuf_float != NULL;   // summarize

	try
	{
		Header header (width, height);
		
		openexr_header_compression(&header, ibuf->ftype & OPENEXR_COMPRESS);
		
		header.channels().insert ("R", Channel (FLOAT));
		header.channels().insert ("G", Channel (FLOAT));
		header.channels().insert ("B", Channel (FLOAT));
		if (ibuf->depth==32)
			header.channels().insert ("A", Channel (FLOAT));
		if (write_zbuf)
			header.channels().insert ("Z", Channel (FLOAT));
		
		FrameBuffer frameBuffer;			
		OutputFile *file = new OutputFile(name, header);			
		float *first= ibuf->rect_float + 4*(height-1)*width;
		int xstride = sizeof(float) * 4;
		int ystride = - xstride*width;

		frameBuffer.insert ("R", Slice (FLOAT,  (char *) first, xstride, ystride));
		frameBuffer.insert ("G", Slice (FLOAT,  (char *) (first+1), xstride, ystride));
		frameBuffer.insert ("B", Slice (FLOAT,  (char *) (first+2), xstride, ystride));
		if (ibuf->depth==32)
			frameBuffer.insert ("A", Slice (FLOAT,  (char *) (first+3), xstride, ystride));
		if (write_zbuf)
			frameBuffer.insert ("Z", Slice (FLOAT, (char *) ibuf->zbuf_float + 4*(height-1)*width,
											sizeof(float), sizeof(float) * -width));
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

/* ********************* Tile file support ************************************ */

typedef struct ExrHandle {
	InputFile *ifile;
	TiledOutputFile *tofile;
	OutputFile *ofile;
	int tilex, tiley;
	int width, height;
	ListBase channels;
} ExrHandle;

#define CHANMAXNAME 64
typedef struct ExrChannel {
	struct ExrChannel *next, *prev;
	char name[2*CHANMAXNAME + 1];
	int xstride, ystride;
	float *rect;
} ExrChannel;

/* not threaded! write one tiled file at a time */
void *IMB_exr_get_handle(void)
{
	static ExrHandle data;
	
	memset(&data, sizeof(ExrHandle), 0);
	
	return &data;
}

/* still clumsy name handling, layers/channels can be ordered as list in list later */
void IMB_exr_add_channel(void *handle, const char *layname, const char *channame)
{
	ExrHandle *data= (ExrHandle *)handle;
	ExrChannel *echan;
	
	echan= (ExrChannel *)MEM_callocN(sizeof(ExrChannel), "exr tile channel");
	
	if(layname) {
		char lay[CHANMAXNAME], chan[CHANMAXNAME];
		strncpy(lay, layname, CHANMAXNAME-1);
		strncpy(chan, channame, CHANMAXNAME-1);

		sprintf(echan->name, "%s.%s", lay, chan);
	}
	else
		strncpy(echan->name, channame, 2*CHANMAXNAME);
	printf("added channel %s\n", echan->name);
	BLI_addtail(&data->channels, echan);
}

void IMB_exr_begin_write(void *handle, char *filename, int width, int height)
{
	ExrHandle *data= (ExrHandle *)handle;
	Header header (width, height);
	ExrChannel *echan;
	
	data->width= width;
	data->height= height;
	
	for(echan= (ExrChannel *)data->channels.first; echan; echan= echan->next)
		header.channels().insert (echan->name, Channel (FLOAT));
	
	header.insert ("comments", StringAttribute ("Blender MultiChannel"));
	
	data->ofile = new OutputFile(filename, header);
}

void IMB_exrtile_begin_write(void *handle, char *filename, int width, int height, int tilex, int tiley)
{
	ExrHandle *data= (ExrHandle *)handle;
	Header header (width, height);
	ExrChannel *echan;
	
	data->tilex= tilex;
	data->tiley= tiley;
	data->width= width;
	data->height= height;
	
	for(echan= (ExrChannel *)data->channels.first; echan; echan= echan->next)
		header.channels().insert (echan->name, Channel (FLOAT));
	
	header.setTileDescription (TileDescription (tilex, tiley, ONE_LEVEL));
	header.lineOrder() = RANDOM_Y,
	header.compression() = NO_COMPRESSION;
	
	header.insert ("comments", StringAttribute ("Blender MultiChannel"));
	
	data->tofile = new TiledOutputFile(filename, header);
}

int IMB_exr_begin_read(void *handle, char *filename, int *width, int *height)
{
	ExrHandle *data= (ExrHandle *)handle;
	
	data->ifile = new InputFile(filename);
	if(data->ifile) {
		Box2i dw = data->ifile->header().dataWindow();
		data->width= *width  = dw.max.x - dw.min.x + 1;
		data->height= *height = dw.max.y - dw.min.y + 1;
		
		const ChannelList &channels = data->ifile->header().channels();
		
		for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
			IMB_exr_add_channel(data, NULL, i.name());
		
		return 1;
	}
	return 0;
}

/* still clumsy name handling, layers/channels can be ordered as list in list later */
void IMB_exr_set_channel(void *handle, char *layname, char *channame, int xstride, int ystride, float *rect)
{
	ExrHandle *data= (ExrHandle *)handle;
	ExrChannel *echan;
	char name[2*CHANMAXNAME + 1];
	
	if(layname) {
		char lay[CHANMAXNAME], chan[CHANMAXNAME];
		strncpy(lay, layname, CHANMAXNAME-1);
		strncpy(chan, channame, CHANMAXNAME-1);
		
		sprintf(name, "%s.%s", lay, chan);
	}
	else
		strncpy(name, channame, 2*CHANMAXNAME);
	
	
	for(echan= (ExrChannel *)data->channels.first; echan; echan= echan->next)
		if(strcmp(echan->name, name)==0)
			break;
	
	if(echan) {
		echan->xstride= xstride;
		echan->ystride= ystride;
		echan->rect= rect;
	}
	else
		printf("IMB_exrtile_set_channel error %s\n", name);
}


void IMB_exrtile_write_channels(void *handle, int partx, int party)
{
	ExrHandle *data= (ExrHandle *)handle;
	FrameBuffer frameBuffer;
	ExrChannel *echan;
	
	for(echan= (ExrChannel *)data->channels.first; echan; echan= echan->next) {
		float *rect= echan->rect - echan->xstride*partx - echan->ystride*party;

		frameBuffer.insert (echan->name, Slice (FLOAT,  (char *)rect, 
							echan->xstride*sizeof(float), echan->ystride*sizeof(float)));
	}
	
	data->tofile->setFrameBuffer (frameBuffer);
	printf("write tile %d %d\n", partx/data->tilex, party/data->tiley);
	data->tofile->writeTile (partx/data->tilex, party/data->tiley);	
	
}

void IMB_exr_write_channels(void *handle)
{
	ExrHandle *data= (ExrHandle *)handle;
	FrameBuffer frameBuffer;
	ExrChannel *echan;
	
	for(echan= (ExrChannel *)data->channels.first; echan; echan= echan->next)
		frameBuffer.insert (echan->name, Slice (FLOAT,  (char *)echan->rect, 
												echan->xstride*sizeof(float), echan->ystride*sizeof(float)));
	
	data->ofile->setFrameBuffer (frameBuffer);
	data->ofile->writePixels (data->height);	
	
}

void IMB_exr_read_channels(void *handle)
{
	ExrHandle *data= (ExrHandle *)handle;
	FrameBuffer frameBuffer;
	ExrChannel *echan;
	
	for(echan= (ExrChannel *)data->channels.first; echan; echan= echan->next) {
		/* no datawindow correction needed */
		if(echan->rect)
			frameBuffer.insert (echan->name, Slice (FLOAT,  (char *)echan->rect, 
												echan->xstride*sizeof(float), echan->ystride*sizeof(float)));
	}
	
	data->ifile->setFrameBuffer (frameBuffer);
	data->ifile->readPixels (0, data->height-1);	
}


void IMB_exr_close(void *handle)
{
	ExrHandle *data= (ExrHandle *)handle;
	ExrChannel *echan;
	
	if(data->ifile)
		delete data->ifile;
	else if(data->ofile)
		delete data->ofile;
	else if(data->tofile)
		delete data->tofile;
	
	data->ifile= NULL;
	data->ofile= NULL;
	data->tofile= NULL;
	
	BLI_freelistN(&data->channels);
}


/* ********************************************************* */

typedef struct RGBA
{
	float r;
	float g;
	float b;
	float a;
} RGBA;


static void exr_print_filecontents(InputFile *file)
{
	const ChannelList &channels = file->header().channels();
	
	for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
	{
		const Channel &channel = i.channel();
		printf("OpenEXR-load: Found channel %s of type %d\n", i.name(), channel.type);
	}
}

static int exr_has_zbuffer(InputFile *file)
{
	const ChannelList &channels = file->header().channels();
	
	for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
	{
		const Channel &channel = i.channel();
		if(strcmp("Z", i.name())==0)
			return 1;
	}
	return 0;
}

static int exr_is_renderresult(InputFile *file)
{
	const StringAttribute *comments= file->header().findTypedAttribute<StringAttribute>("comments");
	if(comments)
		if(comments->value() == "Blender MultiChannel")
			return 1;
	return 0;
}

struct ImBuf *imb_load_openexr(unsigned char *mem, int size, int flags)
{
	struct ImBuf *ibuf = NULL;
	InputFile *file = NULL;
	
	if (imb_is_a_openexr(mem) == 0) return(NULL);
	
	try
	{
		Mem_IStream membuf(mem, size); 
		file = new InputFile(membuf);
		
		Box2i dw = file->header().dataWindow();
		int width  = dw.max.x - dw.min.x + 1;
		int height = dw.max.y - dw.min.y + 1;
		
		//printf("OpenEXR-load: image data window %d %d %d %d\n", 
		//	   dw.min.x, dw.min.y, dw.max.x, dw.max.y);

		//exr_print_filecontents(file);
		int flipped= exr_is_renderresult(file);
		
		ibuf = IMB_allocImBuf(width, height, 32, 0, 0);
		
		if (ibuf) 
		{
			ibuf->ftype = OPENEXR;
			
			if (!(flags & IB_test))
			{
				FrameBuffer frameBuffer;
				float *first;
				int xstride = sizeof(float) * 4;
				int ystride = flipped ? xstride*width : - xstride*width;
				
				imb_addrectfloatImBuf(ibuf);
				
				/* inverse correct first pixel for datawindow coordinates (- dw.min.y because of y flip) */
				first= ibuf->rect_float - 4*(dw.min.x - dw.min.y*width);
				/* but, since we read y-flipped (negative y stride) we move to last scanline */
				if(!flipped) first+= 4*(height-1)*width;
				
				frameBuffer.insert ("R", Slice (FLOAT,  (char *) first, xstride, ystride));
				frameBuffer.insert ("G", Slice (FLOAT,  (char *) (first+1), xstride, ystride));
				frameBuffer.insert ("B", Slice (FLOAT,  (char *) (first+2), xstride, ystride));
																		/* 1.0 is fill value */
				frameBuffer.insert ("A", Slice (FLOAT,  (char *) (first+3), xstride, ystride, 1, 1, 1.0f));

				if(exr_has_zbuffer(file)) 
				{
					float *firstz;
					
					addzbuffloatImBuf(ibuf);
					firstz= ibuf->zbuf_float - (dw.min.x - dw.min.y*width);
					if(!flipped) firstz+= (height-1)*width;
					frameBuffer.insert ("Z", Slice (FLOAT,  (char *)firstz , sizeof(float), -width*sizeof(float)));
				}
				
				file->setFrameBuffer (frameBuffer);
				file->readPixels (dw.min.y, dw.max.y);
				
				IMB_rect_from_float(ibuf);
			}
		}
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
