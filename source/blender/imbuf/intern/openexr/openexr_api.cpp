/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/imbuf/intern/openexr/openexr_api.cpp
 *  \ingroup openexr
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <fstream>
#include <string>
#include <set>
#include <errno.h>
#include <algorithm>

#include <openexr_api.h>

#if defined (WIN32) && !defined(FREE_WINDOWS)
#include "utfconv.h"
#endif

extern "C"
{

// The following prevents a linking error in debug mode for MSVC using the libs in CVS
#if defined(WITH_OPENEXR) && defined(_WIN32) && defined(_DEBUG) && !defined(__MINGW32__) && !defined(__CYGWIN__)
_CRTIMP void __cdecl _invalid_parameter_noinfo(void)
{
}
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_color.h"
#include "BLI_threads.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"
#include "IMB_metadata.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

#include "openexr_multi.h"
}

#include <iostream>

#include <half.h>
#include <Iex.h>
#include <ImfVersion.h>
#include <ImathBox.h>
#include <ImfArray.h>
#include <ImfIO.h>
#include <ImfChannelList.h>
#include <ImfPixelType.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>
#include <ImfCompression.h>
#include <ImfCompressionAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfStandardAttributes.h>

using namespace Imf;
using namespace Imath;

/* Memory Input Stream */

class Mem_IStream : public Imf::IStream
{
public:

	Mem_IStream (unsigned char *exrbuf, size_t exrsize) :
		IStream("dummy"), _exrpos(0), _exrsize(exrsize)
	{
		_exrbuf = exrbuf;
	}

	virtual bool    read(char c[], int n);
	virtual Int64   tellg();
	virtual void    seekg(Int64 pos);
	virtual void    clear();
	//virtual ~Mem_IStream() {}; // unused

private:

	Int64 _exrpos;
	Int64 _exrsize;
	unsigned char *_exrbuf;
};

bool Mem_IStream::read(char c[], int n)
{
	if (n + _exrpos <= _exrsize) {
		memcpy(c, (void *)(&_exrbuf[_exrpos]), n);
		_exrpos += n;
		return true;
	}
	else
		return false;
}

Int64 Mem_IStream::tellg()
{
	return _exrpos;
}

void Mem_IStream::seekg(Int64 pos)
{
	_exrpos = pos;
}

void Mem_IStream::clear()
{
}

/* File Input Stream */

class IFileStream : public Imf::IStream
{
public:
	IFileStream(const char *filename)
	: IStream(filename)
	{
		/* utf-8 file path support on windows */
#if defined (WIN32) && !defined(FREE_WINDOWS)
		wchar_t *wfilename = alloc_utf16_from_8(filename, 0);
		ifs.open(wfilename, std::ios_base::binary);
		free(wfilename);
#else
		ifs.open(filename, std::ios_base::binary);
#endif

		if (!ifs)
			Iex::throwErrnoExc();
	}

	virtual bool read(char c[], int n)
	{
		if (!ifs)
			throw Iex::InputExc("Unexpected end of file.");

		errno = 0;
		ifs.read(c, n);
		return check_error();
	}

	virtual Int64 tellg()
	{
		return std::streamoff(ifs.tellg());
	}

	virtual void seekg(Int64 pos)
	{
		ifs.seekg(pos);
		check_error();
	}

	virtual void clear()
	{
		ifs.clear();
	}

private:
	bool check_error()
	{
		if (!ifs) {
			if (errno)
				Iex::throwErrnoExc();

			return false;
		}

		return true;
	}

	std::ifstream ifs;
};

/* File Output Stream */

class OFileStream : public OStream
{
public:
	OFileStream(const char *filename)
	: OStream(filename)
	{
		/* utf-8 file path support on windows */
#if defined (WIN32) && !defined(FREE_WINDOWS)
		wchar_t *wfilename = alloc_utf16_from_8(filename, 0);
		ofs.open(wfilename, std::ios_base::binary);
		free(wfilename);
#else
		ofs.open(filename, std::ios_base::binary);
#endif

		if (!ofs)
			Iex::throwErrnoExc();
	}

	virtual void write(const char c[], int n)
	{
		errno = 0;
		ofs.write(c, n);
		check_error();
	}

	virtual Int64 tellp()
	{
		return std::streamoff(ofs.tellp());
	}

	virtual void seekp(Int64 pos)
	{
		ofs.seekp(pos);
		check_error();
	}

private:
	void check_error()
	{
		if (!ofs) {
			if (errno)
				Iex::throwErrnoExc();

			throw Iex::ErrnoExc("File output failed.");
		}
	}

	std::ofstream ofs;
};

struct _RGBAZ {
	half r;
	half g;
	half b;
	half a;
	half z;
};

typedef struct _RGBAZ RGBAZ;

extern "C"
{

/**
 * Test presence of OpenEXR file.
 * \param mem pointer to loaded OpenEXR bitstream
 */
int imb_is_a_openexr(unsigned char *mem)
{
	return Imf::isImfMagic((const char *)mem);
}

static void openexr_header_compression(Header *header, int compression)
{
	switch (compression) {
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
			header->compression() = ZIP_COMPRESSION;
			break;
	}
}

static void openexr_header_metadata(Header *header, struct ImBuf *ibuf)
{
	ImMetaData *info;

	for (info = ibuf->metadata; info; info = info->next)
		header->insert(info->key, StringAttribute(info->value));

	if (ibuf->ppm[0] > 0.0)
		addXDensity(*header, ibuf->ppm[0] / 39.3700787); /* 1 meter = 39.3700787 inches */
}

static int imb_save_openexr_half(struct ImBuf *ibuf, const char *name, int flags)
{
	const int channels = ibuf->channels;
	const int is_alpha = (channels >= 4) && (ibuf->planes == 32);
	const int is_zbuf = (flags & IB_zbuffloat) && ibuf->zbuf_float != NULL; /* summarize */
	const int width = ibuf->x;
	const int height = ibuf->y;

	try
	{
		Header header(width, height);

		openexr_header_compression(&header, ibuf->ftype & OPENEXR_COMPRESS);
		openexr_header_metadata(&header, ibuf);

		header.channels().insert("R", Channel(HALF));
		header.channels().insert("G", Channel(HALF));
		header.channels().insert("B", Channel(HALF));
		if (is_alpha)
			header.channels().insert("A", Channel(HALF));
		if (is_zbuf)     // z we do as float always
			header.channels().insert("Z", Channel(Imf::FLOAT));

		FrameBuffer frameBuffer;

		/* manually create ofstream, so we can handle utf-8 filepaths on windows */
		OFileStream file_stream(name);
		OutputFile file(file_stream, header);

		/* we store first everything in half array */
		RGBAZ *pixels = new RGBAZ[height * width];
		RGBAZ *to = pixels;
		int xstride = sizeof(RGBAZ);
		int ystride = xstride * width;

		/* indicate used buffers */
		frameBuffer.insert("R", Slice(HALF,  (char *) &pixels[0].r, xstride, ystride));
		frameBuffer.insert("G", Slice(HALF,  (char *) &pixels[0].g, xstride, ystride));
		frameBuffer.insert("B", Slice(HALF,  (char *) &pixels[0].b, xstride, ystride));
		if (is_alpha)
			frameBuffer.insert("A", Slice(HALF, (char *) &pixels[0].a, xstride, ystride));
		if (is_zbuf)
			frameBuffer.insert("Z", Slice(Imf::FLOAT, (char *)(ibuf->zbuf_float + (height - 1) * width),
			                              sizeof(float), sizeof(float) * -width));
		if (ibuf->rect_float) {
			float *from;

			for (int i = ibuf->y - 1; i >= 0; i--) {
				from = ibuf->rect_float + channels * i * width;

				for (int j = ibuf->x; j > 0; j--) {
					to->r = from[0];
					to->g = (channels >= 2) ? from[1] : from[0];
					to->b = (channels >= 3) ? from[2] : from[0];
					to->a = (channels >= 4) ? from[3] : 1.0f;
					to++; from += channels;
				}
			}
		}
		else {
			unsigned char *from;

			for (int i = ibuf->y - 1; i >= 0; i--) {
				from = (unsigned char *)ibuf->rect + 4 * i * width;

				for (int j = ibuf->x; j > 0; j--) {
					to->r = srgb_to_linearrgb((float)from[0] / 255.0f);
					to->g = srgb_to_linearrgb((float)from[1] / 255.0f);
					to->b = srgb_to_linearrgb((float)from[2] / 255.0f);
					to->a = channels >= 4 ? (float)from[3] / 255.0f : 1.0f;
					to++; from += 4;
				}
			}
		}

//		printf("OpenEXR-save: Writing OpenEXR file of height %d.\n", height);

		file.setFrameBuffer(frameBuffer);
		file.writePixels(height);

		delete[] pixels;
	}
	catch (const std::exception &exc)
	{
		printf("OpenEXR-save: ERROR: %s\n", exc.what());

		return (0);
	}

	return (1);
}

static int imb_save_openexr_float(struct ImBuf *ibuf, const char *name, int flags)
{
	const int channels = ibuf->channels;
	const int is_alpha = (channels >= 4) && (ibuf->planes == 32);
	const int is_zbuf = (flags & IB_zbuffloat) && ibuf->zbuf_float != NULL; /* summarize */
	const int width = ibuf->x;
	const int height = ibuf->y;

	try
	{
		Header header(width, height);

		openexr_header_compression(&header, ibuf->ftype & OPENEXR_COMPRESS);
		openexr_header_metadata(&header, ibuf);

		header.channels().insert("R", Channel(Imf::FLOAT));
		header.channels().insert("G", Channel(Imf::FLOAT));
		header.channels().insert("B", Channel(Imf::FLOAT));
		if (is_alpha)
			header.channels().insert("A", Channel(Imf::FLOAT));
		if (is_zbuf)
			header.channels().insert("Z", Channel(Imf::FLOAT));

		FrameBuffer frameBuffer;

		/* manually create ofstream, so we can handle utf-8 filepaths on windows */
		OFileStream file_stream(name);
		OutputFile file(file_stream, header);

		int xstride = sizeof(float) * channels;
		int ystride = -xstride * width;
		float *rect[4] = {NULL, NULL, NULL, NULL};

		/* last scanline, stride negative */
		rect[0] = ibuf->rect_float + channels * (height - 1) * width;
		rect[1] = (channels >= 2) ? rect[0] + 1 : rect[0];
		rect[2] = (channels >= 3) ? rect[0] + 2 : rect[0];
		rect[3] = (channels >= 4) ? rect[0] + 3 : rect[0]; /* red as alpha, is this needed since alpha isn't written? */

		frameBuffer.insert("R", Slice(Imf::FLOAT,  (char *)rect[0], xstride, ystride));
		frameBuffer.insert("G", Slice(Imf::FLOAT,  (char *)rect[1], xstride, ystride));
		frameBuffer.insert("B", Slice(Imf::FLOAT,  (char *)rect[2], xstride, ystride));
		if (is_alpha)
			frameBuffer.insert("A", Slice(Imf::FLOAT,  (char *)rect[3], xstride, ystride));
		if (is_zbuf)
			frameBuffer.insert("Z", Slice(Imf::FLOAT, (char *) (ibuf->zbuf_float + (height - 1) * width),
			                              sizeof(float), sizeof(float) * -width));
		file.setFrameBuffer(frameBuffer);
		file.writePixels(height);
	}
	catch (const std::exception &exc)
	{
		printf("OpenEXR-save: ERROR: %s\n", exc.what());

		return (0);
	}

	return (1);
	//	printf("OpenEXR-save: Done.\n");
}


int imb_save_openexr(struct ImBuf *ibuf, const char *name, int flags)
{
	if (flags & IB_mem) {
		printf("OpenEXR-save: Create EXR in memory CURRENTLY NOT SUPPORTED !\n");
		imb_addencodedbufferImBuf(ibuf);
		ibuf->encodedsize = 0;
		return(0);
	}

	if (ibuf->ftype & OPENEXR_HALF)
		return imb_save_openexr_half(ibuf, name, flags);
	else {
		/* when no float rect, we save as half (16 bits is sufficient) */
		if (ibuf->rect_float == NULL)
			return imb_save_openexr_half(ibuf, name, flags);
		else
			return imb_save_openexr_float(ibuf, name, flags);
	}
}

/* ********************* Nicer API, MultiLayer and with Tile file support ************************************ */

/* naming rules:
 * - parse name from right to left
 * - last character is channel ID, 1 char like 'A' 'R' 'G' 'B' 'X' 'Y' 'Z' 'W' 'U' 'V'
 * - separated with a dot; the Pass name (like "Depth", "Color", "Diffuse" or "Combined")
 * - separated with a dot: the Layer name (like "Lamp1" or "Walls" or "Characters")
 */

static ListBase exrhandles = {NULL, NULL};

typedef struct ExrHandle {
	struct ExrHandle *next, *prev;

	IFileStream *ifile_stream;
	InputFile *ifile;

	OFileStream *ofile_stream;
	TiledOutputFile *tofile;
	OutputFile *ofile;

	int tilex, tiley;
	int width, height;
	int mipmap;

	ListBase channels;  /* flattened out, ExrChannel */
	ListBase layers;    /* hierarchical, pointing in end to ExrChannel */
} ExrHandle;

/* flattened out channel */
typedef struct ExrChannel {
	struct ExrChannel *next, *prev;

	char name[EXR_TOT_MAXNAME + 1];  /* full name of layer+pass */
	int xstride, ystride;            /* step to next pixel, to next scanline */
	float *rect;                     /* first pointer to write in */
	char chan_id;                    /* quick lookup of channel char */
} ExrChannel;


/* hierarchical; layers -> passes -> channels[] */
typedef struct ExrPass {
	struct ExrPass *next, *prev;
	char name[EXR_PASS_MAXNAME];
	int totchan;
	float *rect;
	struct ExrChannel *chan[EXR_PASS_MAXCHAN];
	char chan_id[EXR_PASS_MAXCHAN];
} ExrPass;

typedef struct ExrLayer {
	struct ExrLayer *next, *prev;
	char name[EXR_LAY_MAXNAME + 1];
	ListBase passes;
} ExrLayer;

/* ********************** */

void *IMB_exr_get_handle(void)
{
	ExrHandle *data = (ExrHandle *)MEM_callocN(sizeof(ExrHandle), "exr handle");
	BLI_addtail(&exrhandles, data);
	return data;
}

/* adds flattened ExrChannels */
/* xstride, ystride and rect can be done in set_channel too, for tile writing */
void IMB_exr_add_channel(void *handle, const char *layname, const char *passname, int xstride, int ystride, float *rect)
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrChannel *echan;

	echan = (ExrChannel *)MEM_callocN(sizeof(ExrChannel), "exr tile channel");

	if (layname) {
		char lay[EXR_LAY_MAXNAME + 1], pass[EXR_PASS_MAXNAME + 1];
		BLI_strncpy(lay, layname, EXR_LAY_MAXNAME);
		BLI_strncpy(pass, passname, EXR_PASS_MAXNAME);

		BLI_snprintf(echan->name, sizeof(echan->name), "%s.%s", lay, pass);
	}
	else {
		BLI_strncpy(echan->name, passname, EXR_TOT_MAXNAME - 1);
	}

	echan->xstride = xstride;
	echan->ystride = ystride;
	echan->rect = rect;

	// printf("added channel %s\n", echan->name);
	BLI_addtail(&data->channels, echan);
}

/* only used for writing temp. render results (not image files) */
int IMB_exr_begin_write(void *handle, const char *filename, int width, int height, int compress)
{
	ExrHandle *data = (ExrHandle *)handle;
	Header header(width, height);
	ExrChannel *echan;

	data->width = width;
	data->height = height;

	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next)
		header.channels().insert(echan->name, Channel(Imf::FLOAT));

	openexr_header_compression(&header, compress);
	// openexr_header_metadata(&header, ibuf); // no imbuf. cant write
	/* header.lineOrder() = DECREASING_Y; this crashes in windows for file read! */

	header.insert("BlenderMultiChannel", StringAttribute("Blender V2.55.1 and newer"));

	/* avoid crash/abort when we don't have permission to write here */
	/* manually create ofstream, so we can handle utf-8 filepaths on windows */
	try {
		data->ofile_stream = new OFileStream(filename);
		data->ofile = new OutputFile(*(data->ofile_stream), header);
	}
	catch (const std::exception &exc) {
		std::cerr << "IMB_exr_begin_write: ERROR: " << exc.what() << std::endl;

		delete data->ofile;
		delete data->ofile_stream;

		data->ofile = NULL;
		data->ofile_stream = NULL;
	}

	return (data->ofile != NULL);
}

void IMB_exrtile_begin_write(void *handle, const char *filename, int mipmap, int width, int height, int tilex, int tiley)
{
	ExrHandle *data = (ExrHandle *)handle;
	Header header(width, height);
	ExrChannel *echan;

	data->tilex = tilex;
	data->tiley = tiley;
	data->width = width;
	data->height = height;
	data->mipmap = mipmap;

	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next)
		header.channels().insert(echan->name, Channel(Imf::FLOAT));

	header.setTileDescription(TileDescription(tilex, tiley, (mipmap) ? MIPMAP_LEVELS : ONE_LEVEL));
	header.lineOrder() = RANDOM_Y;
	header.compression() = RLE_COMPRESSION;

	header.insert("BlenderMultiChannel", StringAttribute("Blender V2.43"));

	/* avoid crash/abort when we don't have permission to write here */
	/* manually create ofstream, so we can handle utf-8 filepaths on windows */
	try {
		data->ofile_stream = new OFileStream(filename);
		data->tofile = new TiledOutputFile(*(data->ofile_stream), header);
	}
	catch (const std::exception &) {
		delete data->tofile;
		delete data->ofile_stream;

		data->tofile = NULL;
		data->ofile_stream = NULL;
	}
}

/* read from file */
int IMB_exr_begin_read(void *handle, const char *filename, int *width, int *height)
{
	ExrHandle *data = (ExrHandle *)handle;

	if (BLI_exists(filename) && BLI_file_size(filename) > 32) {   /* 32 is arbitrary, but zero length files crashes exr */
		/* avoid crash/abort when we don't have permission to write here */
		try {
			data->ifile_stream = new IFileStream(filename);
			data->ifile = new InputFile(*(data->ifile_stream));
		}
		catch (const std::exception &) {
			delete data->ifile;
			delete data->ifile_stream;

			data->ifile = NULL;
			data->ifile_stream = NULL;
		}

		if (data->ifile) {
			Box2i dw = data->ifile->header().dataWindow();
			data->width = *width  = dw.max.x - dw.min.x + 1;
			data->height = *height = dw.max.y - dw.min.y + 1;

			const ChannelList &channels = data->ifile->header().channels();

			for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
				IMB_exr_add_channel(data, NULL, i.name(), 0, 0, NULL);

			return 1;
		}
	}
	return 0;
}

/* still clumsy name handling, layers/channels can be ordered as list in list later */
void IMB_exr_set_channel(void *handle, const char *layname, const char *passname, int xstride, int ystride, float *rect)
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrChannel *echan;
	char name[EXR_TOT_MAXNAME + 1];

	if (layname) {
		char lay[EXR_LAY_MAXNAME + 1], pass[EXR_PASS_MAXNAME + 1];
		BLI_strncpy(lay, layname, EXR_LAY_MAXNAME);
		BLI_strncpy(pass, passname, EXR_PASS_MAXNAME);

		BLI_snprintf(name, sizeof(name), "%s.%s", lay, pass);
	}
	else
		BLI_strncpy(name, passname, EXR_TOT_MAXNAME - 1);

	echan = (ExrChannel *)BLI_findstring(&data->channels, name, offsetof(ExrChannel, name));

	if (echan) {
		echan->xstride = xstride;
		echan->ystride = ystride;
		echan->rect = rect;
	}
	else
		printf("IMB_exrtile_set_channel error %s\n", name);
}

void IMB_exrtile_clear_channels(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	BLI_freelistN(&data->channels);
}

void IMB_exrtile_write_channels(void *handle, int partx, int party, int level)
{
	ExrHandle *data = (ExrHandle *)handle;
	FrameBuffer frameBuffer;
	ExrChannel *echan;

	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
		float *rect = echan->rect - echan->xstride * partx - echan->ystride * party;

		frameBuffer.insert(echan->name, Slice(Imf::FLOAT,  (char *)rect,
		                                      echan->xstride * sizeof(float), echan->ystride * sizeof(float)));
	}

	data->tofile->setFrameBuffer(frameBuffer);

	try {
		// printf("write tile %d %d\n", partx/data->tilex, party/data->tiley);
		data->tofile->writeTile(partx / data->tilex, party / data->tiley, level);
	}
	catch (const std::exception &exc) {
		std::cerr << "OpenEXR-writeTile: ERROR: " << exc.what() << std::endl;
	}
}

void IMB_exr_write_channels(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	FrameBuffer frameBuffer;
	ExrChannel *echan;

	if (data->channels.first) {
		for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
			/* last scanline, stride negative */
			float *rect = echan->rect + echan->xstride * (data->height - 1) * data->width;

			frameBuffer.insert(echan->name, Slice(Imf::FLOAT,  (char *)rect,
			                                      echan->xstride * sizeof(float), -echan->ystride * sizeof(float)));
		}

		data->ofile->setFrameBuffer(frameBuffer);
		try {
			data->ofile->writePixels(data->height);
		}
		catch (const std::exception &exc) {
			std::cerr << "OpenEXR-writePixels: ERROR: " << exc.what() << std::endl;
		}
	}
	else {
		printf("Error: attempt to save MultiLayer without layers.\n");
	}
}

void IMB_exr_read_channels(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	FrameBuffer frameBuffer;
	ExrChannel *echan;

	/* check if exr was saved with previous versions of blender which flipped images */
	const StringAttribute *ta = data->ifile->header().findTypedAttribute <StringAttribute> ("BlenderMultiChannel");
	short flip = (ta && strncmp(ta->value().c_str(), "Blender V2.43", 13) == 0); /* 'previous multilayer attribute, flipped */

	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {

		if (echan->rect) {
			if (flip)
				frameBuffer.insert(echan->name, Slice(Imf::FLOAT,  (char *)echan->rect,
				                                      echan->xstride * sizeof(float), echan->ystride * sizeof(float)));
			else
				frameBuffer.insert(echan->name, Slice(Imf::FLOAT,  (char *)(echan->rect + echan->xstride * (data->height - 1) * data->width),
				                                      echan->xstride * sizeof(float), -echan->ystride * sizeof(float)));
		}
		else
			printf("warning, channel with no rect set %s\n", echan->name);
	}

	data->ifile->setFrameBuffer(frameBuffer);

	try {
		data->ifile->readPixels(0, data->height - 1);
	}
	catch (const std::exception &exc) {
		std::cerr << "OpenEXR-readPixels: ERROR: " << exc.what() << std::endl;
	}
}

void IMB_exr_multilayer_convert(void *handle, void *base,
                                void * (*addlayer)(void *base, const char *str),
                                void (*addpass)(void *base, void *lay, const char *str,
                                                float *rect, int totchan, const char *chan_id))
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrLayer *lay;
	ExrPass *pass;

	if (BLI_listbase_is_empty(&data->layers)) {
		printf("cannot convert multilayer, no layers in handle\n");
		return;
	}

	for (lay = (ExrLayer *)data->layers.first; lay; lay = lay->next) {
		void *laybase = addlayer(base, lay->name);
		if (laybase) {
			for (pass = (ExrPass *)lay->passes.first; pass; pass = pass->next) {
				addpass(base, laybase, pass->name, pass->rect, pass->totchan, pass->chan_id);
				pass->rect = NULL;
			}
		}
	}
}


void IMB_exr_close(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrLayer *lay;
	ExrPass *pass;

	delete data->ifile;
	delete data->ifile_stream;
	delete data->ofile;
	delete data->tofile;
	delete data->ofile_stream;

	data->ifile = NULL;
	data->ifile_stream = NULL;
	data->ofile = NULL;
	data->tofile = NULL;
	data->ofile_stream = NULL;

	BLI_freelistN(&data->channels);

	for (lay = (ExrLayer *)data->layers.first; lay; lay = lay->next) {
		for (pass = (ExrPass *)lay->passes.first; pass; pass = pass->next)
			if (pass->rect)
				MEM_freeN(pass->rect);
		BLI_freelistN(&lay->passes);
	}
	BLI_freelistN(&data->layers);

	BLI_remlink(&exrhandles, data);
	MEM_freeN(data);
}

/* ********* */

/* get a substring from the end of the name, separated by '.' */
static int imb_exr_split_token(const char *str, const char *end, const char **token)
{
	ptrdiff_t maxlen = end - str;
	int len = 0;
	while (len < maxlen && *(end - len - 1) != '.') {
		len++;
	}

	*token = end - len;
	return len;
}

static int imb_exr_split_channel_name(ExrChannel *echan, char *layname, char *passname)
{
	const char *name = echan->name;
	const char *end = name + strlen(name);
	const char *token;
	char tokenbuf[EXR_TOT_MAXNAME];
	int len;
	
	/* some multilayers have the combined buffer with names A B G R saved */
	if (name[1] == 0) {
		echan->chan_id = name[0];
		layname[0] = '\0';

		if (ELEM(name[0], 'R', 'G', 'B', 'A'))
			strcpy(passname, "Combined");
		else if (name[0] == 'Z')
			strcpy(passname, "Depth");
		else
			strcpy(passname, name);

		return 1;
	}

	/* last token is single character channel identifier */
	len = imb_exr_split_token(name, end, &token);
	if (len == 0) {
		printf("multilayer read: bad channel name: %s\n", name);
		return 0;
	}
	else if (len == 1) {
		echan->chan_id = token[0];
	}
	else if (len > 1) {
		bool ok = false;

		if (len == 2) {
			/* some multilayers are using two-letter channels name,
			 * like, MX or NZ, which is basically has structure of
			 *   <pass_prefix><component>
			 *
			 * This is a bit silly, but see file from [#35658].
			 *
			 * Here we do some magic to distinguish such cases.
			 */
			if (ELEM(token[1], 'X', 'Y', 'Z') ||
			    ELEM(token[1], 'R', 'G', 'B') ||
			    ELEM(token[1], 'U', 'V', 'A'))
			{
				echan->chan_id = token[1];
				ok = true;
			}
		}

		if (ok == false) {
			BLI_strncpy(tokenbuf, token, std::min(len + 1, EXR_TOT_MAXNAME));
			printf("multilayer read: channel token too long: %s\n", tokenbuf);
			return 0;
		}
	}
	end -= len + 1; /* +1 to skip '.' separator */

	/* second token is pass name */
	len = imb_exr_split_token(name, end, &token);
	if (len == 0) {
		printf("multilayer read: bad channel name: %s\n", name);
		return 0;
	}
	BLI_strncpy(passname, token, len + 1);
	end -= len + 1; /* +1 to skip '.' separator */

	/* all preceding tokens combined as layer name */
	if (end > name)
		BLI_strncpy(layname, name, (int)(end - name) + 1);
	else
		layname[0] = '\0';

	return 1;
}

static ExrLayer *imb_exr_get_layer(ListBase *lb, char *layname)
{
	ExrLayer *lay = (ExrLayer *)BLI_findstring(lb, layname, offsetof(ExrLayer, name));

	if (lay == NULL) {
		lay = (ExrLayer *)MEM_callocN(sizeof(ExrLayer), "exr layer");
		BLI_addtail(lb, lay);
		BLI_strncpy(lay->name, layname, EXR_LAY_MAXNAME);
	}

	return lay;
}

static ExrPass *imb_exr_get_pass(ListBase *lb, char *passname)
{
	ExrPass *pass = (ExrPass *)BLI_findstring(lb, passname, offsetof(ExrPass, name));

	if (pass == NULL) {
		pass = (ExrPass *)MEM_callocN(sizeof(ExrPass), "exr pass");

		if (strcmp(passname, "Combined") == 0)
			BLI_addhead(lb, pass);
		else
			BLI_addtail(lb, pass);
	}

	BLI_strncpy(pass->name, passname, EXR_LAY_MAXNAME);

	return pass;
}

/* creates channels, makes a hierarchy and assigns memory to channels */
static ExrHandle *imb_exr_begin_read_mem(InputFile *file, int width, int height)
{
	ExrLayer *lay;
	ExrPass *pass;
	ExrChannel *echan;
	ExrHandle *data = (ExrHandle *)IMB_exr_get_handle();
	int a;
	char layname[EXR_TOT_MAXNAME], passname[EXR_TOT_MAXNAME];

	data->ifile = file;
	data->width = width;
	data->height = height;

	const ChannelList &channels = data->ifile->header().channels();

	for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
		IMB_exr_add_channel(data, NULL, i.name(), 0, 0, NULL);

	/* now try to sort out how to assign memory to the channels */
	/* first build hierarchical layer list */
	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
		if (imb_exr_split_channel_name(echan, layname, passname) ) {
			ExrLayer *lay = imb_exr_get_layer(&data->layers, layname);
			ExrPass *pass = imb_exr_get_pass(&lay->passes, passname);

			pass->chan[pass->totchan] = echan;
			pass->totchan++;
			if (pass->totchan >= EXR_PASS_MAXCHAN)
				break;
		}
	}
	if (echan) {
		printf("error, too many channels in one pass: %s\n", echan->name);
		IMB_exr_close(data);
		return NULL;
	}

	/* with some heuristics, try to merge the channels in buffers */
	for (lay = (ExrLayer *)data->layers.first; lay; lay = lay->next) {
		for (pass = (ExrPass *)lay->passes.first; pass; pass = pass->next) {
			if (pass->totchan) {
				pass->rect = (float *)MEM_mapallocN(width * height * pass->totchan * sizeof(float), "pass rect");
				if (pass->totchan == 1) {
					echan = pass->chan[0];
					echan->rect = pass->rect;
					echan->xstride = 1;
					echan->ystride = width;
					pass->chan_id[0] = echan->chan_id;
				}
				else {
					char lookup[256];

					memset(lookup, 0, sizeof(lookup));

					/* we can have RGB(A), XYZ(W), UVA */
					if (pass->totchan == 3 || pass->totchan == 4) {
						if (pass->chan[0]->chan_id == 'B' || pass->chan[1]->chan_id == 'B' ||  pass->chan[2]->chan_id == 'B') {
							lookup[(unsigned int)'R'] = 0;
							lookup[(unsigned int)'G'] = 1;
							lookup[(unsigned int)'B'] = 2;
							lookup[(unsigned int)'A'] = 3;
						}
						else if (pass->chan[0]->chan_id == 'Y' || pass->chan[1]->chan_id == 'Y' ||  pass->chan[2]->chan_id == 'Y') {
							lookup[(unsigned int)'X'] = 0;
							lookup[(unsigned int)'Y'] = 1;
							lookup[(unsigned int)'Z'] = 2;
							lookup[(unsigned int)'W'] = 3;
						}
						else {
							lookup[(unsigned int)'U'] = 0;
							lookup[(unsigned int)'V'] = 1;
							lookup[(unsigned int)'A'] = 2;
						}
						for (a = 0; a < pass->totchan; a++) {
							echan = pass->chan[a];
							echan->rect = pass->rect + lookup[(unsigned int)echan->chan_id];
							echan->xstride = pass->totchan;
							echan->ystride = width * pass->totchan;
							pass->chan_id[(unsigned int)lookup[(unsigned int)echan->chan_id]] = echan->chan_id;
						}
					}
					else { /* unknown */
						for (a = 0; a < pass->totchan; a++) {
							echan = pass->chan[a];
							echan->rect = pass->rect + a;
							echan->xstride = pass->totchan;
							echan->ystride = width * pass->totchan;
							pass->chan_id[a] = echan->chan_id;
						}
					}
				}
			}
		}
	}

	return data;
}


/* ********************************************************* */

/* debug only */
static void exr_print_filecontents(InputFile *file)
{
	const ChannelList &channels = file->header().channels();

	for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
		const Channel &channel = i.channel();
		printf("OpenEXR-load: Found channel %s of type %d\n", i.name(), channel.type);
	}
}

/* for non-multilayer, map  R G B A channel names to something that's in this file */
static const char *exr_rgba_channelname(InputFile *file, const char *chan)
{
	const ChannelList &channels = file->header().channels();

	for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
		/* const Channel &channel = i.channel(); */ /* Not used yet */
		const char *str = i.name();
		int len = strlen(str);
		if (len) {
			if (BLI_strcasecmp(chan, str + len - 1) == 0) {
				return str;
			}
		}
	}
	return chan;
}

static bool exr_has_rgb(InputFile *file)
{
	return file->header().channels().findChannel("R") != NULL &&
	       file->header().channels().findChannel("G") != NULL &&
	       file->header().channels().findChannel("B") != NULL;
}

static bool exr_has_luma(InputFile *file)
{
	/* Y channel is the luma and should always present fir luma space images,
	 * optionally it could be also channels for chromas called BY and RY.
	 */
	return file->header().channels().findChannel("Y") != NULL;
}

static bool exr_has_chroma(InputFile *file)
{
	return file->header().channels().findChannel("BY") != NULL &&
	       file->header().channels().findChannel("RY") != NULL;
}

static int exr_has_zbuffer(InputFile *file)
{
	return !(file->header().channels().findChannel("Z") == NULL);
}

static int exr_has_alpha(InputFile *file)
{
	return !(file->header().channels().findChannel("A") == NULL);
}

static bool exr_is_multilayer(InputFile *file)
{
	const StringAttribute *comments = file->header().findTypedAttribute<StringAttribute>("BlenderMultiChannel");
	const ChannelList &channels = file->header().channels();
	std::set <std::string> layerNames;

	/* will not include empty layer names */
	channels.layers(layerNames);

	if (comments || layerNames.size() > 1)
		return 1;

	if (layerNames.size()) {
		/* if layerNames is not empty, it means at least one layer is non-empty,
		 * but it also could be layers without names in the file and such case
		 * shall be considered a multilayer exr
		 *
		 * that's what we do here: test whether there're empty layer names together
		 * with non-empty ones in the file
		 */
		for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); i++) {
			std::string layerName = i.name();
			size_t pos = layerName.rfind ('.');

			if (pos == std::string::npos)
				return 1;
		}
	}

	return 0;
}

struct ImBuf *imb_load_openexr(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	struct ImBuf *ibuf = NULL;
	InputFile *file = NULL;

	if (imb_is_a_openexr(mem) == 0) return(NULL);

	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_FLOAT);

	try
	{
		Mem_IStream *membuf = new Mem_IStream(mem, size);
		bool is_multi;
		file = new InputFile(*membuf);

		Box2i dw = file->header().dataWindow();
		const int width  = dw.max.x - dw.min.x + 1;
		const int height = dw.max.y - dw.min.y + 1;

		//printf("OpenEXR-load: image data window %d %d %d %d\n",
		//	   dw.min.x, dw.min.y, dw.max.x, dw.max.y);

		if (0) // debug
			exr_print_filecontents(file);

		is_multi = exr_is_multilayer(file);

		/* do not make an ibuf when */
		if (is_multi && !(flags & IB_test) && !(flags & IB_multilayer)) {
			printf("Error: can't process EXR multilayer file\n");
		}
		else {
			const int is_alpha = exr_has_alpha(file);

			ibuf = IMB_allocImBuf(width, height, is_alpha ? 32 : 24, 0);

			if (hasXDensity(file->header())) {
				ibuf->ppm[0] = xDensity(file->header()) * 39.3700787f;
				ibuf->ppm[1] = ibuf->ppm[0] * (double)file->header().pixelAspectRatio();
			}

			ibuf->ftype = OPENEXR;

			if (!(flags & IB_test)) {
				if (is_multi) { /* only enters with IB_multilayer flag set */
					/* constructs channels for reading, allocates memory in channels */
					ExrHandle *handle = imb_exr_begin_read_mem(file, width, height);
					if (handle) {
						IMB_exr_read_channels(handle);
						ibuf->userdata = handle;         /* potential danger, the caller has to check for this! */
					}
				}
				else {
					const bool has_rgb = exr_has_rgb(file);
					const bool has_luma = exr_has_luma(file);
					FrameBuffer frameBuffer;
					float *first;
					int xstride = sizeof(float) * 4;
					int ystride = -xstride * width;

					imb_addrectfloatImBuf(ibuf);

					/* inverse correct first pixel for datawindow coordinates (- dw.min.y because of y flip) */
					first = ibuf->rect_float - 4 * (dw.min.x - dw.min.y * width);
					/* but, since we read y-flipped (negative y stride) we move to last scanline */
					first += 4 * (height - 1) * width;

					if (has_rgb) {
						frameBuffer.insert(exr_rgba_channelname(file, "R"),
						                   Slice(Imf::FLOAT,  (char *) first, xstride, ystride));
						frameBuffer.insert(exr_rgba_channelname(file, "G"),
						                   Slice(Imf::FLOAT,  (char *) (first + 1), xstride, ystride));
						frameBuffer.insert(exr_rgba_channelname(file, "B"),
						                   Slice(Imf::FLOAT,  (char *) (first + 2), xstride, ystride));
					}
					else if (has_luma) {
						frameBuffer.insert(exr_rgba_channelname(file, "Y"),
						                   Slice(Imf::FLOAT,  (char *) first, xstride, ystride));
						frameBuffer.insert(exr_rgba_channelname(file, "BY"),
						                   Slice(Imf::FLOAT,  (char *) (first + 1), xstride, ystride, 1, 1, 0.5f));
						frameBuffer.insert(exr_rgba_channelname(file, "RY"),
						                   Slice(Imf::FLOAT,  (char *) (first + 2), xstride, ystride, 1, 1, 0.5f));
					}

					/* 1.0 is fill value, this still needs to be assigned even when (is_alpha == 0) */
					frameBuffer.insert(exr_rgba_channelname(file, "A"),
					                   Slice(Imf::FLOAT,  (char *) (first + 3), xstride, ystride, 1, 1, 1.0f));

					if (exr_has_zbuffer(file)) {
						float *firstz;

						addzbuffloatImBuf(ibuf);
						firstz = ibuf->zbuf_float - (dw.min.x - dw.min.y * width);
						firstz += (height - 1) * width;
						frameBuffer.insert("Z", Slice(Imf::FLOAT,  (char *)firstz, sizeof(float), -width * sizeof(float)));
					}

					file->setFrameBuffer(frameBuffer);
					file->readPixels(dw.min.y, dw.max.y);

					// XXX, ImBuf has no nice way to deal with this.
					// ideally IM_rect would be used when the caller wants a rect BUT
					// at the moment all functions use IM_rect.
					// Disabling this is ok because all functions should check if a rect exists and create one on demand.
					//
					// Disabling this because the sequencer frees immediate.
					//
					// if (flag & IM_rect)
					//     IMB_rect_from_float(ibuf);

					if (!has_rgb && has_luma) {
						size_t a;
						if (exr_has_chroma(file)) {
							for (a = 0; a < (size_t) ibuf->x * ibuf->y; ++a) {
								float *color = ibuf->rect_float + a * 4;
								ycc_to_rgb(color[0] * 255.0f, color[1] * 255.0f, color[2] * 255.0f,
								           &color[0], &color[1], &color[2],
								           BLI_YCC_ITU_BT709);
							}
						}
						else {
							for (a = 0; a < (size_t) ibuf->x * ibuf->y; ++a) {
								float *color = ibuf->rect_float + a * 4;
								color[1] = color[2] = color[0];
							}
						}
					}

					/* file is no longer needed */
					delete file;
				}
			}

			if (flags & IB_alphamode_detect)
				ibuf->flags |= IB_alphamode_premul;
		}
		return(ibuf);
	}
	catch (const std::exception &exc)
	{
		std::cerr << exc.what() << std::endl;
		if (ibuf) IMB_freeImBuf(ibuf);
		delete file;

		return (0);
	}

}

void imb_initopenexr(void)
{
	int num_threads = BLI_system_thread_count();

	setGlobalThreadCount(num_threads);
}

} // export "C"
