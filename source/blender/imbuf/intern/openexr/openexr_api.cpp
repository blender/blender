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
#include <stdexcept>
#include <fstream>
#include <string>
#include <set>
#include <errno.h>
#include <algorithm>
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

/* multiview/multipart */
#include <ImfMultiView.h>
#include <ImfMultiPartInputFile.h>
#include <ImfInputPart.h>
#include <ImfOutputPart.h>
#include <ImfMultiPartOutputFile.h>
#include <ImfTiledOutputPart.h>
#include <ImfPartType.h>
#include <ImfPartHelper.h>

#include "DNA_scene_types.h" /* For OpenEXR compression constants */

#include <openexr_api.h>

#if defined (WIN32)
#include "utfconv.h"
#endif

extern "C"
{

// The following prevents a linking error in debug mode for MSVC using the libs in CVS
#if defined(WITH_OPENEXR) && defined(_WIN32) && defined(DEBUG) && _MSC_VER < 1900
_CRTIMP void __cdecl _invalid_parameter_noinfo(void)
{
}
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_color.h"
#include "BLI_threads.h"

#include "BKE_idprop.h"
#include "BKE_image.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"
#include "IMB_metadata.h"

#include "openexr_multi.h"
}

extern "C" {
#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"
}

using namespace Imf;
using namespace Imath;

extern "C"
{
/* prototype */
static struct ExrPass *imb_exr_get_pass(ListBase *lb, char *passname);
static bool exr_has_multiview(MultiPartInputFile& file);
static bool exr_has_multipart_file(MultiPartInputFile& file);
static bool exr_has_alpha(MultiPartInputFile& file);
static bool exr_has_zbuffer(MultiPartInputFile& file);
static void exr_printf(const char *__restrict format, ...);
static void imb_exr_type_by_channels(ChannelList& channels, StringVector& views,
                                     bool *r_singlelayer, bool *r_multilayer, bool *r_multiview);
}

/* Memory Input Stream */

class Mem_IStream : public Imf::IStream
{
public:

	Mem_IStream(unsigned char *exrbuf, size_t exrsize) :
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
#if defined (WIN32)
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
#if defined (WIN32)
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
int imb_is_a_openexr(const unsigned char *mem)
{
	return Imf::isImfMagic((const char *)mem);
}

static void openexr_header_compression(Header *header, int compression)
{
	switch (compression) {
		case R_IMF_EXR_CODEC_NONE:
			header->compression() = NO_COMPRESSION;
			break;
		case R_IMF_EXR_CODEC_PXR24:
			header->compression() = PXR24_COMPRESSION;
			break;
		case R_IMF_EXR_CODEC_ZIP:
			header->compression() = ZIP_COMPRESSION;
			break;
		case R_IMF_EXR_CODEC_PIZ:
			header->compression() = PIZ_COMPRESSION;
			break;
		case R_IMF_EXR_CODEC_RLE:
			header->compression() = RLE_COMPRESSION;
			break;
		case R_IMF_EXR_CODEC_ZIPS:
			header->compression() = ZIPS_COMPRESSION;
			break;
		case R_IMF_EXR_CODEC_B44:
			header->compression() = B44_COMPRESSION;
			break;
		case R_IMF_EXR_CODEC_B44A:
			header->compression() = B44A_COMPRESSION;
			break;
#if OPENEXR_VERSION_MAJOR >= 2 && OPENEXR_VERSION_MINOR >= 2
		case R_IMF_EXR_CODEC_DWAA:
			header->compression() = DWAA_COMPRESSION;
			break;
		case R_IMF_EXR_CODEC_DWAB:
			header->compression() = DWAB_COMPRESSION;
			break;
#endif
		default:
			header->compression() = ZIP_COMPRESSION;
			break;
	}
}

static void openexr_header_metadata(Header *header, struct ImBuf *ibuf)
{
	if (ibuf->metadata) {
		IDProperty *prop;

		for (prop = (IDProperty *)ibuf->metadata->data.group.first; prop; prop = prop->next) {
			if (prop->type == IDP_STRING) {
				header->insert(prop->name, StringAttribute(IDP_String(prop)));
			}
		}
	}

	if (ibuf->ppm[0] > 0.0)
		addXDensity(*header, ibuf->ppm[0] / 39.3700787); /* 1 meter = 39.3700787 inches */
}

static void openexr_header_metadata_callback(void *data, const char *propname, char *prop, int UNUSED(len))
{
	Header *header = (Header *)data;
	header->insert(propname, StringAttribute(prop));
}


static bool imb_save_openexr_half(
        ImBuf *ibuf, const char *name, const int flags, const int totviews,
        const char * (*getview)(void *base, int view_id),
        ImBuf *(*getbuffer)(void *base, const int view_id))
{
	const int channels = ibuf->channels;
	const bool is_alpha = (channels >= 4) && (ibuf->planes == 32);
	const bool is_zbuf = (flags & IB_zbuffloat) && ibuf->zbuf_float != NULL; /* summarize */
	const int width = ibuf->x;
	const int height = ibuf->y;
	const bool is_multiview = (flags & IB_multiview) && ibuf->userdata;

	BLI_assert((!is_multiview) || (getview && getbuffer));

	std::vector <string> views;
	int view_id;

	try
	{
		Header header(width, height);

		openexr_header_compression(&header, ibuf->foptions.flag & OPENEXR_COMPRESS);
		openexr_header_metadata(&header, ibuf);

		/* create views when possible */
		for (view_id = 0; view_id < totviews; view_id ++)
			views.push_back(is_multiview ? getview(ibuf->userdata, view_id) : "");

		if (is_multiview)
			addMultiView(header, views);

		for (view_id = 0; view_id < totviews; view_id ++) {
			header.channels().insert(insertViewName("R", views, view_id), Channel(HALF));
			header.channels().insert(insertViewName("G", views, view_id), Channel(HALF));
			header.channels().insert(insertViewName("B", views, view_id), Channel(HALF));
			if (is_alpha)
				header.channels().insert(insertViewName("A", views, view_id), Channel(HALF));
			if (is_zbuf)     // z we do as float always
				header.channels().insert(insertViewName("Z", views, view_id), Channel(Imf::FLOAT));
		}

		FrameBuffer frameBuffer;

		/* manually create ofstream, so we can handle utf-8 filepaths on windows */
		OFileStream file_stream(name);
		OutputFile file(file_stream, header);

		/* we store first everything in half array */
		std::vector<RGBAZ> pixels(height * width * totviews);
		int xstride = sizeof(RGBAZ);
		int ystride = xstride * width;

		for (view_id = 0; view_id < totviews; view_id ++) {
			ImBuf *view_ibuf = is_multiview ? getbuffer(ibuf->userdata, view_id) : ibuf;
			const size_t offset = view_id * width * height;
			RGBAZ *to = &pixels[offset];

			/* TODO (dfelinto)
			 * In some cases we get NULL ibufs, it needs investigation, meanwhile prevent crash
			 * Multiview Render + Image Editor + OpenEXR + Multi-View
			 */
			if (view_ibuf == NULL) {
				throw std::runtime_error(std::string("Missing data to write to ") + name);
			}

			/* indicate used buffers */
			frameBuffer.insert(insertViewName("R", views, view_id), Slice(HALF,  (char *) &pixels[offset].r, xstride, ystride));
			frameBuffer.insert(insertViewName("G", views, view_id), Slice(HALF,  (char *) &pixels[offset].g, xstride, ystride));
			frameBuffer.insert(insertViewName("B", views, view_id), Slice(HALF,  (char *) &pixels[offset].b, xstride, ystride));
			if (is_alpha)
				frameBuffer.insert(insertViewName("A", views, view_id), Slice(HALF, (char *) &pixels[offset].a, xstride, ystride));
			if (is_zbuf)
				frameBuffer.insert(insertViewName("Z", views, view_id), Slice(Imf::FLOAT, (char *)(view_ibuf->zbuf_float + (height - 1) * width),
				                   sizeof(float), sizeof(float) * -width));
			if (view_ibuf->rect_float) {
				float *from;

				for (int i = view_ibuf->y - 1; i >= 0; i--) {
					from = view_ibuf->rect_float + channels * i * width;

					for (int j = view_ibuf->x; j > 0; j--) {
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

				for (int i = view_ibuf->y - 1; i >= 0; i--) {
					from = (unsigned char *)view_ibuf->rect + 4 * i * width;

					for (int j = view_ibuf->x; j > 0; j--) {
						to->r = srgb_to_linearrgb((float)from[0] / 255.0f);
						to->g = srgb_to_linearrgb((float)from[1] / 255.0f);
						to->b = srgb_to_linearrgb((float)from[2] / 255.0f);
						to->a = channels >= 4 ? (float)from[3] / 255.0f : 1.0f;
						to++; from += 4;
					}
				}
			}

			if (is_multiview)
				IMB_freeImBuf(view_ibuf);
		}

		exr_printf("OpenEXR-save: Writing OpenEXR file of height %d.\n", height);

		file.setFrameBuffer(frameBuffer);
		file.writePixels(height);
	}
	catch (const std::exception& exc)
	{
		printf("OpenEXR-save: ERROR: %s\n", exc.what());

		return false;
	}

	return true;
}

static bool imb_save_openexr_float(
        ImBuf *ibuf, const char *name, const int flags, const int totviews,
        const char * (*getview)(void *base, const int view_id),
        ImBuf *(*getbuffer)(void *base, const int view_id))
{
	const int channels = ibuf->channels;
	const bool is_alpha = (channels >= 4) && (ibuf->planes == 32);
	const bool is_zbuf = (flags & IB_zbuffloat) && ibuf->zbuf_float != NULL; /* summarize */
	const int width = ibuf->x;
	const int height = ibuf->y;
	const bool is_multiview = (flags & IB_multiview) && ibuf->userdata;

	BLI_assert((!is_multiview) || (getview && getbuffer));

	std::vector <string> views;
	int view_id;

	try
	{
		Header header(width, height);

		openexr_header_compression(&header, ibuf->foptions.flag & OPENEXR_COMPRESS);
		openexr_header_metadata(&header, ibuf);

		/* create views when possible */
		for (view_id = 0; view_id < totviews; view_id ++)
			views.push_back(is_multiview ? getview(ibuf->userdata, view_id) : "");

		if (is_multiview)
			addMultiView(header, views);

		for (view_id = 0; view_id < totviews; view_id ++) {
			header.channels().insert(insertViewName("R", views, view_id), Channel(Imf::FLOAT));
			header.channels().insert(insertViewName("G", views, view_id), Channel(Imf::FLOAT));
			header.channels().insert(insertViewName("B", views, view_id), Channel(Imf::FLOAT));
			if (is_alpha)
				header.channels().insert(insertViewName("A", views, view_id), Channel(Imf::FLOAT));
			if (is_zbuf)
				header.channels().insert(insertViewName("Z", views, view_id), Channel(Imf::FLOAT));
		}

		FrameBuffer frameBuffer;

		/* manually create ofstream, so we can handle utf-8 filepaths on windows */
		OFileStream file_stream(name);
		OutputFile file(file_stream, header);

		int xstride = sizeof(float) * channels;
		int ystride = -xstride * width;

		for (view_id = 0; view_id < totviews; view_id ++) {
			float *rect[4] = {NULL, NULL, NULL, NULL};
			ImBuf *view_ibuf = is_multiview ? getbuffer(ibuf->userdata, view_id) : ibuf;

			/* TODO (dfelinto)
			 * In some cases we get NULL ibufs, it needs investigation, meanwhile prevent crash
			 * Multiview Render + Image Editor + OpenEXR + Multi-View
			 */
			if (view_ibuf == NULL) {
				throw std::runtime_error(std::string("Missing data to write to ") + name);
			}

			/* last scanline, stride negative */
			rect[0] = view_ibuf->rect_float + channels * (height - 1) * width;
			rect[1] = (channels >= 2) ? rect[0] + 1 : rect[0];
			rect[2] = (channels >= 3) ? rect[0] + 2 : rect[0];
			rect[3] = (channels >= 4) ? rect[0] + 3 : rect[0]; /* red as alpha, is this needed since alpha isn't written? */

			frameBuffer.insert(insertViewName("R", views, view_id), Slice(Imf::FLOAT,  (char *)rect[0], xstride, ystride));
			frameBuffer.insert(insertViewName("G", views, view_id), Slice(Imf::FLOAT,  (char *)rect[1], xstride, ystride));
			frameBuffer.insert(insertViewName("B", views, view_id), Slice(Imf::FLOAT,  (char *)rect[2], xstride, ystride));
			if (is_alpha)
				frameBuffer.insert(insertViewName("A", views, view_id), Slice(Imf::FLOAT,  (char *)rect[3], xstride, ystride));
			if (is_zbuf)
				frameBuffer.insert(insertViewName("Z", views, view_id), Slice(Imf::FLOAT, (char *) (view_ibuf->zbuf_float + (height - 1) * width),
				                                  sizeof(float), sizeof(float) * -width));

			if (is_multiview)
				IMB_freeImBuf(view_ibuf);
		}
		file.setFrameBuffer(frameBuffer);
		file.writePixels(height);
	}
	catch (const std::exception& exc)
	{
		printf("OpenEXR-save: ERROR: %s\n", exc.what());
		return false;
	}

	return true;
}

int imb_save_openexr(struct ImBuf *ibuf, const char *name, int flags)
{
	if (flags & IB_mem) {
		printf("OpenEXR-save: Create EXR in memory CURRENTLY NOT SUPPORTED !\n");
		imb_addencodedbufferImBuf(ibuf);
		ibuf->encodedsize = 0;
		return(0);
	}

	if (ibuf->foptions.flag & OPENEXR_HALF)
		return (int) imb_save_openexr_half(ibuf, name, flags, 1, NULL, NULL);
	else {
		/* when no float rect, we save as half (16 bits is sufficient) */
		if (ibuf->rect_float == NULL)
			return (int) imb_save_openexr_half(ibuf, name, flags, 1, NULL, NULL);
		else
			return (int) imb_save_openexr_float(ibuf, name, flags, 1, NULL, NULL);
	}
}

static bool imb_save_openexr_multiview(
        ImBuf *ibuf, const char *name, const int flags, const int totviews,
        const char *(*getview)(void *base, const int view_id),
        ImBuf *(*getbuffer)(void *base, const int view_id))
{
	if (flags & IB_mem) {
		printf("OpenEXR-save: Create multiview EXR in memory CURRENTLY NOT SUPPORTED !\n");
		imb_addencodedbufferImBuf(ibuf);
		ibuf->encodedsize = 0;
		return false;
	}

	if (ibuf->foptions.flag & OPENEXR_HALF)
		return imb_save_openexr_half(ibuf, name, flags, totviews, getview, getbuffer);
	else {
		/* when no float rect, we save as half (16 bits is sufficient) */
		if (ibuf->rect_float == NULL)
			return imb_save_openexr_half(ibuf, name, flags, totviews, getview, getbuffer);
		else
			return imb_save_openexr_float(ibuf, name, flags, totviews, getview, getbuffer);
	}
}

/* Save single-layer multiview OpenEXR
 * If we have more multiview formats in the future, the function below could be incorporated
 * in our ImBuf write functions, meanwhile this is an OpenEXR special case only */
bool IMB_exr_multiview_save(
        ImBuf *ibuf, const char *name, const int flags, const int totviews,
        const char *(*getview)(void *base, const int view_id),
        ImBuf *(*getbuffer)(void *base, const int view_id))
{
	return imb_save_openexr_multiview(ibuf, name, flags, totviews, getview, getbuffer);
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
	char name[FILE_MAX];

	IStream *ifile_stream;
	MultiPartInputFile *ifile;

	OFileStream *ofile_stream;
	MultiPartOutputFile *mpofile;
	OutputFile *ofile;

	int tilex, tiley;
	int width, height;
	int mipmap;

	StringVector *multiView; /* it needs to be a pointer due to Windows release builds of EXR2.0 segfault when opening EXR bug */
	int parts;

	ListBase channels;  /* flattened out, ExrChannel */
	ListBase layers;    /* hierarchical, pointing in end to ExrChannel */

	int num_half_channels;  /* used during filr save, allows faster temporary buffers allocation */
} ExrHandle;

/* flattened out channel */
typedef struct ExrChannel {
	struct ExrChannel *next, *prev;

	char name[EXR_TOT_MAXNAME + 1];  /* full name with everything */
	struct MultiViewChannelName *m;  /* struct to store all multipart channel info */
	int xstride, ystride;            /* step to next pixel, to next scanline */
	float *rect;                     /* first pointer to write in */
	char chan_id;                    /* quick lookup of channel char */
	int view_id;                     /* quick lookup of channel view */
	bool use_half_float;             /* when saving use half float for file storage */
} ExrChannel;


/* hierarchical; layers -> passes -> channels[] */
typedef struct ExrPass {
	struct ExrPass *next, *prev;
	char name[EXR_PASS_MAXNAME];
	int totchan;
	float *rect;
	struct ExrChannel *chan[EXR_PASS_MAXCHAN];
	char chan_id[EXR_PASS_MAXCHAN];

	char internal_name[EXR_PASS_MAXNAME]; /* name with no view */
	char view[EXR_VIEW_MAXNAME];
	int view_id;
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
	data->multiView = new StringVector();

	BLI_addtail(&exrhandles, data);
	return data;
}

void *IMB_exr_get_handle_name(const char *name)
{
	ExrHandle *data = (ExrHandle *) BLI_rfindstring(&exrhandles, name, offsetof(ExrHandle, name));

	if (data == NULL) {
		data = (ExrHandle *)IMB_exr_get_handle();
		BLI_strncpy(data->name, name, strlen(name) + 1);
	}
	return data;
}

/* multiview functions */
} // extern "C"

extern "C"
{

void IMB_exr_add_view(void *handle, const char *name)
{
	ExrHandle *data = (ExrHandle *)handle;
	data->multiView->push_back(name);
}

static int imb_exr_get_multiView_id(StringVector& views, const std::string& name)
{
	int count = 0;
	for (StringVector::const_iterator i = views.begin(); count < views.size(); ++i) {
		if (name == *i)
			return count;
		else
			count ++;
	}

	/* no views or wrong name */
	return -1;
}

static void imb_exr_get_views(MultiPartInputFile& file, StringVector& views)
{
	if (exr_has_multipart_file(file) == false) {
		if (exr_has_multiview(file)) {
			StringVector sv = multiView(file.header(0));
			for (StringVector::const_iterator i = sv.begin(); i != sv.end(); ++i)
				views.push_back(*i);
		}
	}

	else {
		for (int p = 0; p < file.parts(); p++) {
			std::string view = "";
			if (file.header(p).hasView())
				view = file.header(p).view();

			if (imb_exr_get_multiView_id(views, view) == -1)
				views.push_back(view);
		}
	}
}

/* Multilayer Blender files have the view name in all the passes (even the default view one) */
static void imb_exr_insert_view_name(char *name_full, const char *passname, const char *viewname)
{
	BLI_assert(!ELEM(name_full, passname, viewname));

	if (viewname == NULL || viewname[0] == '\0') {
		BLI_strncpy(name_full, passname, sizeof(((ExrChannel *)NULL)->name));
		return;
	}

	const char delims[] = {'.', '\0'};
	const char *sep;
	const char *token;
	size_t len;

	len = BLI_str_rpartition(passname, delims, &sep, &token);

	if (sep) {
		BLI_snprintf(name_full, EXR_PASS_MAXNAME, "%.*s.%s.%s", (int)len, passname, viewname, token);
	}
	else {
		BLI_snprintf(name_full, EXR_PASS_MAXNAME, "%s.%s", passname, viewname);
	}
}

/* adds flattened ExrChannels */
/* xstride, ystride and rect can be done in set_channel too, for tile writing */
/* passname does not include view */
void IMB_exr_add_channel(void *handle,
                         const char *layname, const char *passname, const char *viewname,
                         int xstride, int ystride, float *rect,
                         bool use_half_float)
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrChannel *echan;

	echan = (ExrChannel *)MEM_callocN(sizeof(ExrChannel), "exr channel");
	echan->m = new MultiViewChannelName ();

	if (layname && layname[0] != '\0') {
		echan->m->name = layname;
		echan->m->name.append(".");
		echan->m->name.append(passname);
	}
	else {
		echan->m->name.assign(passname);
	}

	echan->m->internal_name = echan->m->name;

	echan->m->view.assign(viewname ? viewname : "");

	/* quick look up */
	echan->view_id = std::max(0, imb_exr_get_multiView_id(*data->multiView, echan->m->view));

	/* name has to be unique, thus it's a combination of layer, pass, view, and channel */
	if (layname && layname[0] != '\0') {
		imb_exr_insert_view_name(echan->name, echan->m->name.c_str(), echan->m->view.c_str());
	}
	else if (data->multiView->size() > 1) {
		std::string raw_name = insertViewName(echan->m->name, *data->multiView, echan->view_id);
		BLI_strncpy(echan->name, raw_name.c_str(), sizeof(echan->name));
	}
	else {
		BLI_strncpy(echan->name, echan->m->name.c_str(), sizeof(echan->name));
	}

	echan->xstride = xstride;
	echan->ystride = ystride;
	echan->rect = rect;
	echan->use_half_float = use_half_float;

	if (echan->use_half_float) {
		data->num_half_channels++;
	}

	exr_printf("added channel %s\n", echan->name);
	BLI_addtail(&data->channels, echan);
}

/* used for output files (from RenderResult) (single and multilayer, single and multiview) */
int IMB_exr_begin_write(void *handle, const char *filename, int width, int height, int compress, const StampData *stamp)
{
	ExrHandle *data = (ExrHandle *)handle;
	Header header(width, height);
	ExrChannel *echan;

	data->width = width;
	data->height = height;

	bool is_singlelayer, is_multilayer, is_multiview;

	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
		header.channels().insert(echan->name,
		                         Channel(echan->use_half_float ? Imf::HALF : Imf::FLOAT));
	}

	openexr_header_compression(&header, compress);
	BKE_stamp_info_callback(&header, const_cast<StampData *>(stamp), openexr_header_metadata_callback, false);
	/* header.lineOrder() = DECREASING_Y; this crashes in windows for file read! */

	imb_exr_type_by_channels(header.channels(), *data->multiView, &is_singlelayer, &is_multilayer, &is_multiview);

	if (is_multilayer)
		header.insert("BlenderMultiChannel", StringAttribute("Blender V2.55.1 and newer"));

	if (is_multiview)
		addMultiView(header, *data->multiView);

	/* avoid crash/abort when we don't have permission to write here */
	/* manually create ofstream, so we can handle utf-8 filepaths on windows */
	try {
		data->ofile_stream = new OFileStream(filename);
		data->ofile = new OutputFile(*(data->ofile_stream), header);
	}
	catch (const std::exception& exc) {
		std::cerr << "IMB_exr_begin_write: ERROR: " << exc.what() << std::endl;

		delete data->ofile;
		delete data->ofile_stream;

		data->ofile = NULL;
		data->ofile_stream = NULL;
	}

	return (data->ofile != NULL);
}

/* only used for writing temp. render results (not image files)
 * (FSA and Save Buffers) */
void IMB_exrtile_begin_write(void *handle, const char *filename, int mipmap, int width, int height, int tilex, int tiley)
{
	ExrHandle *data = (ExrHandle *)handle;
	Header header(width, height);
	std::vector<Header> headers;
	ExrChannel *echan;

	data->tilex = tilex;
	data->tiley = tiley;
	data->width = width;
	data->height = height;
	data->mipmap = mipmap;

	header.setTileDescription(TileDescription(tilex, tiley, (mipmap) ? MIPMAP_LEVELS : ONE_LEVEL));
	header.compression() = RLE_COMPRESSION;
	header.setType(TILEDIMAGE);

	header.insert("BlenderMultiChannel", StringAttribute("Blender V2.43"));

	int numparts = data->multiView->size();

	/* copy header from all parts of input to our header array
	 * those temporary files have one part per view */
	for (int i = 0; i < numparts; i++) {
		headers.push_back (header);
		headers[headers.size() - 1].setView((*(data->multiView))[i]);
		headers[headers.size() - 1].setName((*(data->multiView))[i]);
	}

	exr_printf("\nIMB_exrtile_begin_write\n");
	exr_printf("%s %-6s %-22s \"%s\"\n", "p", "view", "name", "internal_name");
	exr_printf("---------------------------------------------------------------\n");

	/* assign channels  */
	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
		/* Tiles are expected to be saved with full float currently. */
		BLI_assert(echan->use_half_float == 0);

		echan->m->internal_name = echan->m->name;
		echan->m->part_number = echan->view_id;

		headers[echan->view_id].channels().insert(echan->m->internal_name, Channel(Imf::FLOAT));
		exr_printf("%d %-6s %-22s \"%s\"\n", echan->m->part_number, echan->m->view.c_str(), echan->m->name.c_str(), echan->m->internal_name.c_str());
	}

	/* avoid crash/abort when we don't have permission to write here */
	/* manually create ofstream, so we can handle utf-8 filepaths on windows */
	try {
		data->ofile_stream = new OFileStream(filename);
		data->mpofile = new MultiPartOutputFile(*(data->ofile_stream), &headers[0], headers.size());
	}
	catch (const std::exception &) {
		delete data->mpofile;
		delete data->ofile_stream;

		data->mpofile = NULL;
		data->ofile_stream = NULL;
	}
}

/* read from file */
int IMB_exr_begin_read(void *handle, const char *filename, int *width, int *height)
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrChannel *echan;

	if (BLI_exists(filename) && BLI_file_size(filename) > 32) {   /* 32 is arbitrary, but zero length files crashes exr */
		/* avoid crash/abort when we don't have permission to write here */
		try {
			data->ifile_stream = new IFileStream(filename);
			data->ifile = new MultiPartInputFile(*(data->ifile_stream));
		}
		catch (const std::exception &) {
			delete data->ifile;
			delete data->ifile_stream;

			data->ifile = NULL;
			data->ifile_stream = NULL;
		}

		if (data->ifile) {
			Box2i dw = data->ifile->header(0).dataWindow();
			data->width = *width  = dw.max.x - dw.min.x + 1;
			data->height = *height = dw.max.y - dw.min.y + 1;

			imb_exr_get_views(*data->ifile, *data->multiView);

			std::vector<MultiViewChannelName> channels;
			GetChannelsInMultiPartFile(*data->ifile, channels);

			for (size_t i = 0; i < channels.size(); i++) {
				IMB_exr_add_channel(data, NULL, channels[i].name.c_str(), channels[i].view.c_str(), 0, 0, NULL, false);
		
				echan = (ExrChannel *)data->channels.last;
				echan->m->name = channels[i].name;
				echan->m->view = channels[i].view;
				echan->m->part_number = channels[i].part_number;
				echan->m->internal_name = channels[i].internal_name;
			}

			return 1;
		}
	}
	return 0;
}

/* still clumsy name handling, layers/channels can be ordered as list in list later */
/* passname here is the raw channel name without the layer */
void IMB_exr_set_channel(void *handle, const char *layname, const char *passname, int xstride, int ystride, float *rect)
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrChannel *echan;
	char name[EXR_TOT_MAXNAME + 1];

	if (layname && layname[0] != '\0') {
		char lay[EXR_LAY_MAXNAME + 1], pass[EXR_PASS_MAXNAME + 1];
		BLI_strncpy(lay, layname, EXR_LAY_MAXNAME);
		BLI_strncpy(pass, passname, EXR_PASS_MAXNAME);

		BLI_snprintf(name, sizeof(name), "%s.%s", lay, pass);
	}
	else {
		BLI_strncpy(name, passname, EXR_TOT_MAXNAME - 1);
	}

	echan = (ExrChannel *)BLI_findstring(&data->channels, name, offsetof(ExrChannel, name));

	if (echan) {
		echan->xstride = xstride;
		echan->ystride = ystride;
		echan->rect = rect;
	}
	else {
		printf("IMB_exr_set_channel error %s\n", name);
	}
}

float  *IMB_exr_channel_rect(void *handle, const char *layname, const char *passname, const char *viewname)
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

	/* name has to be unique, thus it's a combination of layer, pass, view, and channel */
	if (layname && layname[0] != '\0') {
		char temp_buf[EXR_PASS_MAXNAME];
		imb_exr_insert_view_name(temp_buf, name, viewname);
		BLI_strncpy(name, temp_buf, sizeof(name));
	}
	else if (data->multiView->size() > 1) {
		const int view_id = std::max(0, imb_exr_get_multiView_id(*data->multiView, viewname));
		std::string raw_name = insertViewName(name, *data->multiView, view_id);
		BLI_strncpy(name, raw_name.c_str(), sizeof(name));
	}

	echan = (ExrChannel *)BLI_findstring(&data->channels, name, offsetof(ExrChannel, name));

	if (echan)
		return echan->rect;

	return NULL;
}

void IMB_exr_clear_channels(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrChannel *chan;

	for (chan = (ExrChannel *)data->channels.first; chan; chan = chan->next)
		delete chan->m;

	BLI_freelistN(&data->channels);
}

void IMB_exr_write_channels(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	FrameBuffer frameBuffer;
	ExrChannel *echan;

	if (data->channels.first) {
		const size_t num_pixels = ((size_t)data->width) * data->height;
		half *rect_half = NULL, *current_rect_half = NULL;

		/* We allocate teporary storage for half pixels for all the channels at once. */
		if (data->num_half_channels != 0) {
			rect_half = (half *)MEM_mallocN(sizeof(half) * data->num_half_channels * num_pixels, __func__);
			current_rect_half = rect_half;
		}

		for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
			/* Writting starts from last scanline, stride negative. */
			if (echan->use_half_float) {
				float *rect = echan->rect;
				half *cur = current_rect_half;
				for (size_t i = 0; i < num_pixels; ++i, ++cur) {
					*cur = rect[i * echan->xstride];
				}
				half *rect_to_write = current_rect_half + (data->height - 1) * data->width;
				frameBuffer.insert(echan->name, Slice(Imf::HALF,  (char *)rect_to_write,
				                                      sizeof(half), -data->width * sizeof(half)));
				current_rect_half += num_pixels;
			}
			else {
				float *rect = echan->rect + echan->xstride * (data->height - 1) * data->width;
				frameBuffer.insert(echan->name, Slice(Imf::FLOAT,  (char *)rect,
				                                      echan->xstride * sizeof(float), -echan->ystride * sizeof(float)));
			}
		}

		data->ofile->setFrameBuffer(frameBuffer);
		try {
			data->ofile->writePixels(data->height);
		}
		catch (const std::exception& exc) {
			std::cerr << "OpenEXR-writePixels: ERROR: " << exc.what() << std::endl;
		}
		/* Free temporary buffers. */
		if (rect_half != NULL) {
			MEM_freeN(rect_half);
		}
	}
	else {
		printf("Error: attempt to save MultiLayer without layers.\n");
	}
}

/* temporary function, used for FSA and Save Buffers */
/* called once per tile * view */
void IMB_exrtile_write_channels(void *handle, int partx, int party, int level, const char *viewname)
{
	ExrHandle *data = (ExrHandle *)handle;
	FrameBuffer frameBuffer;
	ExrChannel *echan;
	std::string view(viewname);
	const int view_id = imb_exr_get_multiView_id(*data->multiView, view);

	exr_printf("\nIMB_exrtile_write_channels(view: %s)\n", viewname);
	exr_printf("%s %-6s %-22s \"%s\"\n", "p", "view", "name", "internal_name");
	exr_printf("---------------------------------------------------------------------\n");

	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {

		/* eventually we can make the parts' channels to include
		   only the current view TODO */
		if (strcmp(viewname, echan->m->view.c_str()) != 0)
			continue;

		exr_printf("%d %-6s %-22s \"%s\"\n",
		           echan->m->part_number,
		           echan->m->view.c_str(),
		           echan->m->name.c_str(),
		           echan->m->internal_name.c_str()
		           );

		float *rect = echan->rect - echan->xstride * partx - echan->ystride * party;
		frameBuffer.insert(echan->m->internal_name,
		                   Slice(Imf::FLOAT,
		                         (char *)rect,
		                         echan->xstride * sizeof(float),
		                         echan->ystride * sizeof(float)
		                        )
		                  );
	}

	TiledOutputPart out (*data->mpofile, view_id);
	out.setFrameBuffer(frameBuffer);

	try {
		// printf("write tile %d %d\n", partx/data->tilex, party/data->tiley);
		out.writeTile(partx / data->tilex, party / data->tiley, level);
	}
	catch (const std::exception& exc) {
		std::cerr << "OpenEXR-writeTile: ERROR: " << exc.what() << std::endl;
	}
}

/* called only when handle has all views */
void IMB_exrmultiview_write_channels(void *handle, const char *viewname)
{
	ExrHandle *data = (ExrHandle *)handle;
	const int view_id = viewname ? imb_exr_get_multiView_id(*data->multiView, viewname) : -1;
	int numparts = (view_id == -1 ? data->parts : view_id + 1);
	std::vector <FrameBuffer> frameBuffers(numparts);
	std::vector <OutputPart> outputParts;
	ExrChannel *echan;
	int i, part;

	if (data->channels.first == NULL)
		return;

	exr_printf("\nIMB_exrmultiview_write_channels()\n");

	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
		if (view_id != -1 && echan->view_id != view_id)
			continue;

		part = (view_id == -1 ? echan->m->part_number : echan->view_id);

		/* last scanline, stride negative */
		float *rect = echan->rect + echan->xstride * (data->height - 1) * data->width;
		frameBuffers[part].insert(echan->m->internal_name,
		                          Slice(Imf::FLOAT,
		                                (char *)rect,
		                                echan->xstride * sizeof(float),
		                                -echan->ystride * sizeof(float))
		);
	}

	for (i = 0; i < numparts; i++) {
		OutputPart out(*data->mpofile, i);
		out.setFrameBuffer(frameBuffers[i]);
		outputParts.push_back(out);
	}

	try {
		for (i = 0; i < numparts; i++) {
			if (view_id != -1 && i != view_id)
				continue;

			outputParts[i].writePixels(data->height);
		}
	}
	catch (const std::exception& exc) {
		std::cerr << "OpenEXR-write Multi Part: ERROR: " << exc.what() << std::endl;
	}
}

void IMB_exr_read_channels(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	int numparts = data->ifile->parts();

	/* check if exr was saved with previous versions of blender which flipped images */
	const StringAttribute *ta = data->ifile->header(0).findTypedAttribute <StringAttribute> ("BlenderMultiChannel");
	short flip = (ta && STREQLEN(ta->value().c_str(), "Blender V2.43", 13)); /* 'previous multilayer attribute, flipped */

	exr_printf("\nIMB_exr_read_channels\n%s %-6s %-22s \"%s\"\n---------------------------------------------------------------------\n", "p", "view", "name", "internal_name");

	for (int i = 0; i < numparts; i++) {
		/* Read part header. */
		InputPart in(*data->ifile, i);
		Header header = in.header();
		Box2i dw = header.dataWindow();

		/* Insert all matching channel into framebuffer. */
		FrameBuffer frameBuffer;
		ExrChannel *echan;

		for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
			if(echan->m->part_number != i) {
				continue;
			}

			exr_printf("%d %-6s %-22s \"%s\"\n", echan->m->part_number, echan->m->view.c_str(), echan->m->name.c_str(), echan->m->internal_name.c_str());

			if (echan->rect) {
				float *rect = echan->rect;
				size_t xstride = echan->xstride * sizeof(float);
				size_t ystride = echan->ystride * sizeof(float);

				if (!flip) {
					/* inverse correct first pixel for datawindow coordinates */
					rect -= echan->xstride * (dw.min.x - dw.min.y * data->width);
					/* move to last scanline to flip to Blender convention */
					rect += echan->xstride * (data->height - 1) * data->width;
					ystride = -ystride;
				}
				else {
					/* inverse correct first pixel for datawindow coordinates */
					rect -= echan->xstride * (dw.min.x + dw.min.y * data->width);
				}

				frameBuffer.insert(echan->m->internal_name, Slice(Imf::FLOAT, (char *)rect, xstride, ystride));
			}
			else
				printf("warning, channel with no rect set %s\n", echan->m->internal_name.c_str());
		}

		/* Read pixels. */
		try {
			in.setFrameBuffer(frameBuffer);
			exr_printf("readPixels:readPixels[%d]: min.y: %d, max.y: %d\n", i, dw.min.y, dw.max.y);
			in.readPixels(dw.min.y, dw.max.y);
		}
		catch (const std::exception& exc) {
			std::cerr << "OpenEXR-readPixels: ERROR: " << exc.what() << std::endl;
			break;
		}
	}
}

void IMB_exr_multilayer_convert(void *handle, void *base,
                                void * (*addview)(void *base, const char *str),
                                void * (*addlayer)(void *base, const char *str),
                                void (*addpass)(void *base, void *lay, const char *str,
                                                float *rect, int totchan, const char *chan_id,
                                                const char *view))
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrLayer *lay;
	ExrPass *pass;

	/* RenderResult needs at least one RenderView */
	if (data->multiView->size() == 0) {
		addview(base, "");
	}
	else {
		/* add views to RenderResult */
		for (StringVector::const_iterator i = data->multiView->begin(); i != data->multiView->end(); ++i) {
			addview(base, (*i).c_str());
		}
	}

	if (BLI_listbase_is_empty(&data->layers)) {
		printf("cannot convert multilayer, no layers in handle\n");
		return;
	}

	for (lay = (ExrLayer *)data->layers.first; lay; lay = lay->next) {
		void *laybase = addlayer(base, lay->name);
		if (laybase) {
			for (pass = (ExrPass *)lay->passes.first; pass; pass = pass->next) {
				addpass(base, laybase, pass->internal_name, pass->rect, pass->totchan, pass->chan_id, pass->view);
				pass->rect = NULL;
			}
		}
	}
}

void IMB_exr_multiview_convert(void *handle, void *base,
                               void (*addview)(void *base, const char *str),
                               void (*addbuffer)(void *base, const char *str, ImBuf *ibuf, const int frame),
                               const int frame)
{
	ExrHandle *data = (ExrHandle *)handle;
	MultiPartInputFile *file = data->ifile;
	ExrLayer *lay;
	ExrPass *pass;
	ImBuf *ibuf = NULL;
	const bool is_alpha = exr_has_alpha(*file);
	Box2i dw = file->header(0).dataWindow();
	const size_t width  = dw.max.x - dw.min.x + 1;
	const size_t height = dw.max.y - dw.min.y + 1;
	const bool is_depth = exr_has_zbuffer(*file);
	
	/* add views to RenderResult */
	for (StringVector::const_iterator i = data->multiView->begin(); i != data->multiView->end(); ++i) {
		addview(base, (*i).c_str());
	}
	
	if (BLI_listbase_is_empty(&data->layers)) {
		printf("cannot convert multiviews, no views in handle\n");
		return;
	}
	
	/* there is one float/pass per layer (layer here is a view) */
	BLI_assert(BLI_listbase_count_ex(&data->layers, 2) == 1);
	lay = (ExrLayer *)data->layers.first;
	for (pass = (ExrPass *)lay->passes.first; pass; pass = pass->next) {
		if (STREQ(pass->chan_id, "RGB") || STREQ(pass->chan_id, "RGBA")) {
			ibuf = IMB_allocImBuf(width, height, is_alpha ? 32 : 24, IB_rectfloat);

			if (!ibuf) {
				printf("error creating multiview buffer\n");
				return;
			}

			IMB_buffer_float_from_float(
			        ibuf->rect_float, pass->rect, pass->totchan,
			        IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, false,
			        ibuf->x, ibuf->y, ibuf->x, ibuf->x);

			if (hasXDensity(file->header(0))) {
				ibuf->ppm[0] = xDensity(file->header(0)) * 39.3700787f;
				ibuf->ppm[1] = ibuf->ppm[0] * (double)file->header(0).pixelAspectRatio();
			}

			if (is_depth) {
				ExrPass *zpass;
				for (zpass = (ExrPass *)lay->passes.first; zpass; zpass = zpass->next) {
					if (STREQ(zpass->chan_id, "Z") && STREQ(zpass->view, pass->view)) {
						addzbuffloatImBuf(ibuf);
						memcpy(ibuf->zbuf_float, zpass->rect, sizeof(float) * ibuf->x * ibuf->y);
					}
				}
			}

			addbuffer(base, pass->view, ibuf, frame);
		}
	}
}

void IMB_exr_close(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	ExrLayer *lay;
	ExrPass *pass;
	ExrChannel *chan;

	delete data->ifile;
	delete data->ifile_stream;
	delete data->ofile;
	delete data->mpofile;
	delete data->ofile_stream;
	delete data->multiView;

	data->ifile = NULL;
	data->ifile_stream = NULL;
	data->ofile = NULL;
	data->mpofile = NULL;
	data->ofile_stream = NULL;

	for (chan = (ExrChannel *)data->channels.first; chan; chan = chan->next) {
		delete chan->m;
	}
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
	const char delims[] = {'.', '\0'};
	const char *sep;

	BLI_str_partition_ex(str, end, delims, &sep, token, true);

	if (!sep) {
		*token = str;
	}

	return (int)(end - *token);
}

static int imb_exr_split_channel_name(ExrChannel *echan, char *layname, char *passname)
{
	const char *name = echan->m->name.c_str();
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

		if (STREQ(passname, "Combined"))
			BLI_addhead(lb, pass);
		else
			BLI_addtail(lb, pass);
	}

	BLI_strncpy(pass->name, passname, EXR_LAY_MAXNAME);

	return pass;
}

/* creates channels, makes a hierarchy and assigns memory to channels */
static ExrHandle *imb_exr_begin_read_mem(IStream &file_stream, MultiPartInputFile &file, int width, int height)
{
	ExrLayer *lay;
	ExrPass *pass;
	ExrChannel *echan;
	ExrHandle *data = (ExrHandle *)IMB_exr_get_handle();
	int a;
	char layname[EXR_TOT_MAXNAME], passname[EXR_TOT_MAXNAME];

	data->ifile_stream = &file_stream;
	data->ifile = &file;

	data->width = width;
	data->height = height;

	std::vector<MultiViewChannelName> channels;
	GetChannelsInMultiPartFile(*data->ifile, channels);

	imb_exr_get_views(*data->ifile, *data->multiView);

	for (size_t i = 0; i < channels.size(); i++) {
		IMB_exr_add_channel(data, NULL, channels[i].name.c_str(), channels[i].view.c_str(), 0, 0, NULL, false);

		echan = (ExrChannel *)data->channels.last;
		echan->m->name = channels[i].name;
		echan->m->view = channels[i].view;
		echan->m->part_number = channels[i].part_number;
		echan->m->internal_name = channels[i].internal_name;
	}

	/* now try to sort out how to assign memory to the channels */
	/* first build hierarchical layer list */
	for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
		if (imb_exr_split_channel_name(echan, layname, passname)) {

			const char *view = echan->m->view.c_str();
			char internal_name[EXR_PASS_MAXNAME];

			BLI_strncpy(internal_name, passname, EXR_PASS_MAXNAME);

			if (view[0] != '\0') {
				char tmp_pass[EXR_PASS_MAXNAME];
				BLI_snprintf(tmp_pass, sizeof(tmp_pass), "%s.%s", passname, view);
				BLI_strncpy(passname, tmp_pass, sizeof(passname));
			}

			ExrLayer *lay = imb_exr_get_layer(&data->layers, layname);
			ExrPass *pass = imb_exr_get_pass(&lay->passes, passname);

			pass->chan[pass->totchan] = echan;
			pass->totchan++;
			pass->view_id = echan->view_id;
			BLI_strncpy(pass->view, view, sizeof(pass->view));
			BLI_strncpy(pass->internal_name, internal_name, EXR_PASS_MAXNAME);

			if (pass->totchan >= EXR_PASS_MAXCHAN)
				break;
		}
	}
	if (echan) {
		printf("error, too many channels in one pass: %s\n", echan->m->name.c_str());
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
static void exr_printf(const char *fmt, ...)
{
#if 0
	char output[1024];
	va_list args;
	va_start(args, fmt);
	std::vsprintf(output, fmt, args);
	va_end(args);
	printf("%s", output);
#else
	(void)fmt;
#endif
}

static void exr_print_filecontents(MultiPartInputFile& file)
{
	int numparts = file.parts();
	if (numparts == 1 && hasMultiView(file.header(0))) {
		const StringVector views = multiView(file.header(0));
		printf("OpenEXR-load: MultiView file\n");
		printf("OpenEXR-load: Default view: %s\n", defaultViewName(views).c_str());
		for (StringVector::const_iterator i = views.begin(); i != views.end(); ++i) {
			printf("OpenEXR-load: Found view %s\n", (*i).c_str());
		}
	}
	else if (numparts > 1) {
		printf("OpenEXR-load: MultiPart file\n");
		for (int i = 0; i < numparts; i++) {
			if (file.header(i).hasView())
				printf("OpenEXR-load: Part %d: view = \"%s\"\n", i, file.header(i).view().c_str());
		}
	}

	for (int j = 0; j < numparts; j++) {
		const ChannelList& channels = file.header(j).channels();
		for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
			const Channel& channel = i.channel();
			printf("OpenEXR-load: Found channel %s of type %d\n", i.name(), channel.type);
		}
	}
}

/* for non-multilayer, map  R G B A channel names to something that's in this file */
static const char *exr_rgba_channelname(MultiPartInputFile& file, const char *chan)
{
	const ChannelList& channels = file.header(0).channels();

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

static bool exr_has_rgb(MultiPartInputFile& file)
{
	return file.header(0).channels().findChannel("R") != NULL &&
	       file.header(0).channels().findChannel("G") != NULL &&
	       file.header(0).channels().findChannel("B") != NULL;
}

static bool exr_has_luma(MultiPartInputFile& file)
{
	/* Y channel is the luma and should always present fir luma space images,
	 * optionally it could be also channels for chromas called BY and RY.
	 */
	return file.header(0).channels().findChannel("Y") != NULL;
}

static bool exr_has_chroma(MultiPartInputFile& file)
{
	return file.header(0).channels().findChannel("BY") != NULL &&
	       file.header(0).channels().findChannel("RY") != NULL;
}

static bool exr_has_zbuffer(MultiPartInputFile& file)
{
	return !(file.header(0).channels().findChannel("Z") == NULL);
}

static bool exr_has_alpha(MultiPartInputFile& file)
{
	return !(file.header(0).channels().findChannel("A") == NULL);
}

static bool imb_exr_is_multilayer_file(MultiPartInputFile& file)
{
	const StringAttribute *comments = file.header(0).findTypedAttribute<StringAttribute>("BlenderMultiChannel");
	const ChannelList& channels = file.header(0).channels();
	std::set <std::string> layerNames;

	/* will not include empty layer names */
	channels.layers(layerNames);

	if (comments || layerNames.size() > 1)
		return true;

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
				return true;
		}
	}

	return false;
}

static void imb_exr_type_by_channels(ChannelList& channels, StringVector& views,
                                     bool *r_singlelayer, bool *r_multilayer, bool *r_multiview)
{
	std::set <std::string> layerNames;

	*r_singlelayer = true;
	*r_multilayer = *r_multiview = false;

	/* will not include empty layer names */
	channels.layers(layerNames);

	if (views.size() && views[0] != "") {
		*r_multiview = true;
	}
	else {
		*r_singlelayer = false;
		*r_multilayer = true;
		*r_multiview = false;
		return;
	}

	if (layerNames.size()) {
		/* if layerNames is not empty, it means at least one layer is non-empty,
		 * but it also could be layers without names in the file and such case
		 * shall be considered a multilayer exr
		 *
		 * that's what we do here: test whether there're empty layer names together
		 * with non-empty ones in the file
		 */
		for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); i++)
			for (std::set<string>::iterator i = layerNames.begin(); i != layerNames.end(); i++)
				/* see if any layername differs from a viewname */
				if (imb_exr_get_multiView_id(views, *i) == -1) {
					std::string layerName = *i;
					size_t pos = layerName.rfind ('.');

					if (pos == std::string::npos) {
						*r_multilayer = true;
						*r_singlelayer = false;
						return;
					}
				}
	}
	else {
		*r_singlelayer = true;
		*r_multilayer = false;
		*r_multiview = false;
	}

	BLI_assert(r_singlelayer != r_multilayer);
}

bool IMB_exr_has_singlelayer_multiview(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	MultiPartInputFile *file = data->ifile;
	std::set <std::string> layerNames;
	const ChannelList& channels = file->header(0).channels();
	const StringAttribute *comments;

	if (exr_has_multiview(*file) == false)
		return false;

	comments = file->header(0).findTypedAttribute<StringAttribute>("BlenderMultiChannel");

	if (comments)
		return false;

	/* will not include empty layer names */
	channels.layers(layerNames);

	/* returns false if any layer differs from views list */
	if (layerNames.size())
		for (std::set<string>::iterator i = layerNames.begin(); i != layerNames.end(); i++)
			if (imb_exr_get_multiView_id(*data->multiView, *i) == -1)
				return false;

	return true;
}

bool IMB_exr_has_multilayer(void *handle)
{
	ExrHandle *data = (ExrHandle *)handle;
	return imb_exr_is_multilayer_file(*data->ifile);
}

static bool exr_has_multiview(MultiPartInputFile& file)
{
	return hasMultiView(file.header(0));
}

static bool exr_has_multipart_file(MultiPartInputFile& file)
{
	return file.parts() > 1;
}

/* it returns true if the file is multilayer or multiview */
static bool imb_exr_is_multi(MultiPartInputFile& file)
{
	/* multipart files are treated as multilayer in blender - even if they are single layer openexr with multiview */
	if (exr_has_multipart_file(file))
		return true;

	if (exr_has_multiview(file))
		return true;

	if (imb_exr_is_multilayer_file(file))
		return true;

	return false;
}

struct ImBuf *imb_load_openexr(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	struct ImBuf *ibuf = NULL;
	Mem_IStream *membuf = NULL;
	MultiPartInputFile *file = NULL;

	if (imb_is_a_openexr(mem) == 0) return(NULL);

	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_FLOAT);

	try
	{
		bool is_multi;

		membuf = new Mem_IStream((unsigned char *)mem, size);
		file = new MultiPartInputFile(*membuf);

		Box2i dw = file->header(0).dataWindow();
		const int width  = dw.max.x - dw.min.x + 1;
		const int height = dw.max.y - dw.min.y + 1;

		//printf("OpenEXR-load: image data window %d %d %d %d\n",
		//	   dw.min.x, dw.min.y, dw.max.x, dw.max.y);

		if (0) // debug
			exr_print_filecontents(*file);

		is_multi = imb_exr_is_multi(*file);

		/* do not make an ibuf when */
		if (is_multi && !(flags & IB_test) && !(flags & IB_multilayer)) {
			printf("Error: can't process EXR multilayer file\n");
		}
		else {
			const int is_alpha = exr_has_alpha(*file);

			ibuf = IMB_allocImBuf(width, height, is_alpha ? 32 : 24, 0);

			if (hasXDensity(file->header(0))) {
				ibuf->ppm[0] = xDensity(file->header(0)) * 39.3700787f;
				ibuf->ppm[1] = ibuf->ppm[0] * (double)file->header(0).pixelAspectRatio();
			}

			ibuf->ftype = IMB_FTYPE_OPENEXR;

			if (!(flags & IB_test)) {

				if (flags & IB_metadata) {
					const Header & header = file->header(0);
					Header::ConstIterator iter;

					for (iter = header.begin(); iter != header.end(); iter++) {
						const StringAttribute *attrib = file->header(0).findTypedAttribute <StringAttribute> (iter.name());

						/* not all attributes are string attributes so we might get some NULLs here */
						if (attrib) {
							IMB_metadata_add_field(ibuf, iter.name(), attrib->value().c_str());
							ibuf->flags |= IB_metadata;
						}
					}
				}

				if (is_multi && ((flags & IB_thumbnail) == 0)) { /* only enters with IB_multilayer flag set */
					/* constructs channels for reading, allocates memory in channels */
					ExrHandle *handle = imb_exr_begin_read_mem(*membuf, *file, width, height);
					if (handle) {
						IMB_exr_read_channels(handle);
						ibuf->userdata = handle;         /* potential danger, the caller has to check for this! */
					}
				}
				else {
					const bool has_rgb = exr_has_rgb(*file);
					const bool has_luma = exr_has_luma(*file);
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
						frameBuffer.insert(exr_rgba_channelname(*file, "R"),
						                   Slice(Imf::FLOAT,  (char *) first, xstride, ystride));
						frameBuffer.insert(exr_rgba_channelname(*file, "G"),
						                   Slice(Imf::FLOAT,  (char *) (first + 1), xstride, ystride));
						frameBuffer.insert(exr_rgba_channelname(*file, "B"),
						                   Slice(Imf::FLOAT,  (char *) (first + 2), xstride, ystride));
					}
					else if (has_luma) {
						frameBuffer.insert(exr_rgba_channelname(*file, "Y"),
						                   Slice(Imf::FLOAT,  (char *) first, xstride, ystride));
						frameBuffer.insert(exr_rgba_channelname(*file, "BY"),
						                   Slice(Imf::FLOAT,  (char *) (first + 1), xstride, ystride, 1, 1, 0.5f));
						frameBuffer.insert(exr_rgba_channelname(*file, "RY"),
						                   Slice(Imf::FLOAT,  (char *) (first + 2), xstride, ystride, 1, 1, 0.5f));
					}

					/* 1.0 is fill value, this still needs to be assigned even when (is_alpha == 0) */
					frameBuffer.insert(exr_rgba_channelname(*file, "A"),
					                   Slice(Imf::FLOAT,  (char *) (first + 3), xstride, ystride, 1, 1, 1.0f));

					if (exr_has_zbuffer(*file)) {
						float *firstz;

						addzbuffloatImBuf(ibuf);
						firstz = ibuf->zbuf_float - (dw.min.x - dw.min.y * width);
						firstz += (height - 1) * width;
						frameBuffer.insert("Z", Slice(Imf::FLOAT,  (char *)firstz, sizeof(float), -width * sizeof(float)));
					}

					InputPart in (*file, 0);
					in.setFrameBuffer(frameBuffer);
					in.readPixels(dw.min.y, dw.max.y);

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
						if (exr_has_chroma(*file)) {
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
					delete membuf;
					delete file;
				}
			}
			else {
				delete membuf;
				delete file;
			}

			if (flags & IB_alphamode_detect)
				ibuf->flags |= IB_alphamode_premul;
		}
		return(ibuf);
	}
	catch (const std::exception& exc)
	{
		std::cerr << exc.what() << std::endl;
		if (ibuf) IMB_freeImBuf(ibuf);
		delete file;
		delete membuf;

		return (0);
	}

}

void imb_initopenexr(void)
{
	int num_threads = BLI_system_thread_count();

	setGlobalThreadCount(num_threads);
}

} // export "C"
