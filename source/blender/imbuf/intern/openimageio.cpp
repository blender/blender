/*
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * $Id: png.c 36777 2011-05-19 11:54:03Z blendix $
 */

/** \file blender/imbuf/intern/png.c
 *  \ingroup imbuf
 */

#ifdef WITH_OPENIMAGEIO

#include "MEM_sys_types.h"

#include "imbuf.h"

extern "C" {

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_metadata.h"
#include "IMB_filetype.h"

}

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING

int imb_is_a_openimageio(unsigned char *buf)
{
	return 0;
}

int imb_is_a_filepath_openimageio(const char *filepath)
{
	ImageInput *in = ImageInput::create(filepath);
	ImageSpec spec;
	int recognized = in->open(filepath, spec);
	in->close();

	return recognized;
}

template<typename T> static void pack_pixels(T *pixels, int width, int height, int components, T alpha)
{
	if(components == 3) {
		for(int i = width*height-1; i >= 0; i--) {
			pixels[i*4+3] = alpha;
			pixels[i*4+2] = pixels[i*3+2];
			pixels[i*4+1] = pixels[i*3+1];
			pixels[i*4+0] = pixels[i*3+0];
		}
	}
	else if(components == 1) {
		for(int i = width*height-1; i >= 0; i--) {
			pixels[i*4+3] = alpha;
			pixels[i*4+2] = pixels[i];
			pixels[i*4+1] = pixels[i];
			pixels[i*4+0] = pixels[i];
		}
	}
}

int imb_ftype_openimageio(ImFileType *type, ImBuf *ibuf)
{
	return ibuf->ftype & (PNG|TGA|JPG|BMP|RADHDR|TIF|OPENEXR|CINEON|DPX|DDS|JP2);
}

static int format_name_to_ftype(const char *format_name)
{
	if(strcmp(format_name, "png") == 0)
		return PNG;
	else if(strcmp(format_name, "targa") == 0)
		return TGA; /* RAWTGA */
	else if(strcmp(format_name, "jpeg") == 0)
		return JPG;
	else if(strcmp(format_name, "bmp") == 0)
		return BMP;
	else if(strcmp(format_name, "hdr") == 0)
		return RADHDR;
	else if(strcmp(format_name, "tiff") == 0)
		return TIF; /* TIF_16BIT */
	else if(strcmp(format_name, "openexr") == 0)
		return OPENEXR; /* OPENEXR_HALF, OPENEXR_COMPRESS */
	else if(strcmp(format_name, "cineon") == 0)
		return CINEON;
	else if(strcmp(format_name, "dpx") == 0)
		return DPX;
	else if(strcmp(format_name, "dds") == 0)
		return DDS;
	else if(strcmp(format_name, "jpeg2000") == 0)
		return JP2; /* JP2_12BIT, JP2_16BIT, JP2_YCC , JP2_CINE , JP2_CINE_48FPS */

	/* not handled: "field3d", "fits", "ico", "iff", "pnm", "ptex", "sgi", "zfile" */

	return 0;
}

ImBuf *imb_load_openimageio(const char *filepath, int flags)
{
	ImageInput *in = ImageInput::create(filepath);
	ImageSpec spec;
	bool success;

	if(!in->open(filepath, spec)) {
		delete in;
		return NULL;
	}
	
	/* we only handle certain number of components */
	int width = spec.width;
	int height = spec.height;
	int components = spec.nchannels;

	if(!(components == 1 || components == 3 || components == 4)) {
		delete in;
		return NULL;
	}

	ImBuf *ibuf = IMB_allocImBuf(width, height, 32, 0);
	ibuf->ftype = format_name_to_ftype(in->format_name());

	/* TODO: handle oiio:ColorSpace, oiio:Gamma, metadata, multilayer, size_t_safe */

	/* read RGBA pixels */
	if(spec.format == TypeDesc::UINT8 || spec.format == TypeDesc::INT8) {
		//if(in->get_string_attribute("oiio:ColorSpace") == "sRGB")
		ibuf->profile = IB_PROFILE_SRGB;

		imb_addrectImBuf(ibuf);

		uint8_t *pixels = (uint8_t*)ibuf->rect;
		int scanlinesize = width*components;

		success = in->read_image(TypeDesc::UINT8,
			pixels + (height-1)*scanlinesize,
			AutoStride,
			-scanlinesize*sizeof(uint8_t),
			AutoStride);

		pack_pixels<uint8_t>(pixels, width, height, components, 255);
	}
	else {
		ibuf->profile = IB_PROFILE_LINEAR_RGB; /* XXX assumption */

		imb_addrectfloatImBuf(ibuf);

		float *pixels = ibuf->rect_float;
		int scanlinesize = width*components;

		success = in->read_image(TypeDesc::FLOAT,
			pixels + (height-1)*scanlinesize,
			AutoStride,
			-scanlinesize*sizeof(float),
			AutoStride);

		pack_pixels<float>(pixels, width, height, components, 1.0f);
	}

	if(!success)
		fprintf(stderr, "OpenImageIO: error loading image: %s\n", in->geterror().c_str());

	in->close();
	delete in;

	return ibuf;
}

int imb_save_openimageio(struct ImBuf *ibuf, const char *filepath, int flags)
{
	ImageOutput *out = ImageOutput::create(filepath);

	if(ibuf->rect_float) {
		/* XXX profile */

		/* save as float image XXX works? */
		ImageSpec spec(ibuf->x, ibuf->y, 4, TypeDesc::FLOAT);
		int scanlinesize = ibuf->x*4;

		out->open(filepath, spec);

		/* conversion for different top/bottom convention */
		out->write_image(TypeDesc::FLOAT,
			ibuf->rect_float + (ibuf->y-1)*scanlinesize,
			AutoStride,
			-scanlinesize*sizeof(float),
			AutoStride);
	}
	else {
		/* save as 8bit image */
		ImageSpec spec(ibuf->x, ibuf->y, 4, TypeDesc::UINT8);
		int scanlinesize = ibuf->x*4;

		out->open(filepath, spec);

		/* conversion for different top/bottom convention */
		out->write_image(TypeDesc::UINT8,
			(uint8_t*)ibuf->rect + (ibuf->y-1)*scanlinesize,
			AutoStride,
			-scanlinesize*sizeof(uint8_t),
			AutoStride);
	}

	out->close();
	delete out;

	return 1;
}

#endif /* WITH_OPENIMAGEIO */

