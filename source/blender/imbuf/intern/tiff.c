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
 * Contributor(s): Jonathan Merritt.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/tiff.c
 *  \ingroup imbuf
 */


/**
 * Provides TIFF file loading and saving for Blender, via libtiff.
 *
 * The task of loading is complicated somewhat by the fact that Blender has
 * already loaded the file into a memory buffer.  libtiff is not well
 * configured to handle files in memory, so a client wrapper is written to
 * surround the memory and turn it into a virtual file.  Currently, reading
 * of TIFF files is done using libtiff's RGBAImage support.  This is a 
 * high-level routine that loads all images as 32-bit RGBA, handling all the
 * required conversions between many different TIFF types internally.
 * 
 * Saving supports RGB, RGBA and BW (grayscale) images correctly, with
 * 8 bits per channel in all cases.  The "deflate" compression algorithm is
 * used to compress images.
 */

#include <string.h>

#include "imbuf.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
 
#include "BKE_global.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

#include "tiffio.h"

#ifdef WIN32
#include "utfconv.h"
#endif

/***********************
 * Local declarations. *
 ***********************/
/* Reading and writing of an in-memory TIFF file. */
static tsize_t imb_tiff_ReadProc(thandle_t handle, tdata_t data, tsize_t n);
static tsize_t imb_tiff_WriteProc(thandle_t handle, tdata_t data, tsize_t n);
static toff_t  imb_tiff_SeekProc(thandle_t handle, toff_t ofs, int whence);
static int     imb_tiff_CloseProc(thandle_t handle);
static toff_t  imb_tiff_SizeProc(thandle_t handle);
static int     imb_tiff_DummyMapProc(thandle_t fd, tdata_t *pbase, toff_t *psize);
static void    imb_tiff_DummyUnmapProc(thandle_t fd, tdata_t base, toff_t size);


/* Structure for in-memory TIFF file. */
typedef struct ImbTIFFMemFile {
	const unsigned char *mem;   /* Location of first byte of TIFF file. */
	toff_t offset;              /* Current offset within the file.      */
	tsize_t size;               /* Size of the TIFF file.               */
} ImbTIFFMemFile;
#define IMB_TIFF_GET_MEMFILE(x) ((ImbTIFFMemFile *)(x))



/*****************************
 * Function implementations. *
 *****************************/


static void imb_tiff_DummyUnmapProc(thandle_t fd, tdata_t base, toff_t size)
{
	(void)fd;
	(void)base;
	(void)size;
}

static int imb_tiff_DummyMapProc(thandle_t fd, tdata_t *pbase, toff_t *psize)
{
	(void)fd;
	(void)pbase;
	(void)psize;

	return (0);
}

/**
 * Reads data from an in-memory TIFF file.
 *
 * \param handle: Handle of the TIFF file (pointer to ImbTIFFMemFile).
 * \param data:   Buffer to contain data (treat as (void *)).
 * \param n:      Number of bytes to read.
 *
 * \return: Number of bytes actually read.
 *   0 = EOF.
 */
static tsize_t imb_tiff_ReadProc(thandle_t handle, tdata_t data, tsize_t n)
{
	tsize_t nRemaining, nCopy;
	ImbTIFFMemFile *mfile;
	void *srcAddr;

	/* get the pointer to the in-memory file */
	mfile = IMB_TIFF_GET_MEMFILE(handle);
	if (!mfile || !mfile->mem) {
		fprintf(stderr, "imb_tiff_ReadProc: !mfile || !mfile->mem!\n");
		return 0;
	}

	/* find the actual number of bytes to read (copy) */
	nCopy = n;
	if ((tsize_t)mfile->offset >= mfile->size)
		nRemaining = 0;
	else
		nRemaining = mfile->size - mfile->offset;
	
	if (nCopy > nRemaining)
		nCopy = nRemaining;
	
	/* on EOF, return immediately and read (copy) nothing */
	if (nCopy <= 0)
		return (0);

	/* all set -> do the read (copy) */
	srcAddr = (void *)(&(mfile->mem[mfile->offset]));
	memcpy((void *)data, srcAddr, nCopy);
	mfile->offset += nCopy;     /* advance file ptr by copied bytes */
	return nCopy;
}



/**
 * Writes data to an in-memory TIFF file.
 *
 * NOTE: The current Blender implementation should not need this function.  It
 *       is simply a stub.
 */
static tsize_t imb_tiff_WriteProc(thandle_t handle, tdata_t data, tsize_t n)
{
	(void)handle;
	(void)data;
	(void)n;
	
	printf("imb_tiff_WriteProc: this function should not be called.\n");
	return (-1);
}



/**
 * Seeks to a new location in an in-memory TIFF file.
 *
 * \param handle: Handle of the TIFF file (pointer to ImbTIFFMemFile).
 * \param ofs:    Offset value (interpreted according to whence below).
 * \param whence: This can be one of three values:
 *  SEEK_SET - The offset is set to ofs bytes.
 *  SEEK_CUR - The offset is set to its current location plus ofs bytes.
 *  SEEK_END - (This is unsupported and will return -1, indicating an
 *              error).
 *
 * \return: Resulting offset location within the file, measured in bytes from
 *          the beginning of the file.  (-1) indicates an error.
 */
static toff_t imb_tiff_SeekProc(thandle_t handle, toff_t ofs, int whence)
{
	ImbTIFFMemFile *mfile;
	toff_t new_offset;

	/* get the pointer to the in-memory file */
	mfile = IMB_TIFF_GET_MEMFILE(handle);
	if (!mfile || !mfile->mem) {
		fprintf(stderr, "imb_tiff_SeekProc: !mfile || !mfile->mem!\n");
		return (-1);
	}

	/* find the location we plan to seek to */
	switch (whence) {
		case SEEK_SET:
			new_offset = ofs;
			break;
		case SEEK_CUR:
			new_offset = mfile->offset + ofs;
			break;
		default:
			/* no other types are supported - return an error */
			fprintf(stderr, 
			        "imb_tiff_SeekProc: "
			        "Unsupported TIFF SEEK type.\n");
			return (-1);
	}

	/* set the new location */
	mfile->offset = new_offset;
	return mfile->offset;
}



/**
 * Closes (virtually) an in-memory TIFF file.
 *
 * NOTE: All this function actually does is sets the data pointer within the
 *       TIFF file to NULL.  That should trigger assertion errors if attempts
 *       are made to access the file after that point.  However, no such
 *       attempts should ever be made (in theory).
 *
 * \param handle: Handle of the TIFF file (pointer to ImbTIFFMemFile).
 *
 * \return: 0
 */
static int imb_tiff_CloseProc(thandle_t handle)
{
	ImbTIFFMemFile *mfile;

	/* get the pointer to the in-memory file */
	mfile = IMB_TIFF_GET_MEMFILE(handle);
	if (!mfile || !mfile->mem) {
		fprintf(stderr, "imb_tiff_CloseProc: !mfile || !mfile->mem!\n");
		return (0);
	}
	
	/* virtually close the file */
	mfile->mem    = NULL;
	mfile->offset = 0;
	mfile->size   = 0;
	
	return (0);
}



/**
 * Returns the size of an in-memory TIFF file in bytes.
 *
 * \return: Size of file (in bytes).
 */
static toff_t imb_tiff_SizeProc(thandle_t handle)
{
	ImbTIFFMemFile *mfile;

	/* get the pointer to the in-memory file */
	mfile = IMB_TIFF_GET_MEMFILE(handle);
	if (!mfile || !mfile->mem) {
		fprintf(stderr, "imb_tiff_SizeProc: !mfile || !mfile->mem!\n");
		return (0);
	}

	/* return the size */
	return (toff_t)(mfile->size);
}

static TIFF *imb_tiff_client_open(ImbTIFFMemFile *memFile, const unsigned char *mem, size_t size)
{
	/* open the TIFF client layer interface to the in-memory file */
	memFile->mem = mem;
	memFile->offset = 0;
	memFile->size = size;

	return TIFFClientOpen("(Blender TIFF Interface Layer)", 
	                      "r", (thandle_t)(memFile),
	                      imb_tiff_ReadProc, imb_tiff_WriteProc,
	                      imb_tiff_SeekProc, imb_tiff_CloseProc,
	                      imb_tiff_SizeProc, imb_tiff_DummyMapProc, imb_tiff_DummyUnmapProc);
}

/**
 * Checks whether a given memory buffer contains a TIFF file.
 *
 * This method uses the format identifiers from:
 *     http://www.faqs.org/faqs/graphics/fileformats-faq/part4/section-9.html
 * The first four bytes of big-endian and little-endian TIFF files
 * respectively are (hex):
 *  4d 4d 00 2a
 *  49 49 2a 00
 * Note that TIFF files on *any* platform can be either big- or little-endian;
 * it's not platform-specific.
 *
 * AFAICT, libtiff doesn't provide a method to do this automatically, and
 * hence my manual comparison. - Jonathan Merritt (lancelet) 4th Sept 2005.
 */
#define IMB_TIFF_NCB 4      /* number of comparison bytes used */
int imb_is_a_tiff(const unsigned char *mem)
{
	char big_endian[IMB_TIFF_NCB] = { 0x4d, 0x4d, 0x00, 0x2a };
	char lil_endian[IMB_TIFF_NCB] = { 0x49, 0x49, 0x2a, 0x00 };

	return ((memcmp(big_endian, mem, IMB_TIFF_NCB) == 0) ||
	        (memcmp(lil_endian, mem, IMB_TIFF_NCB) == 0));
}

static void scanline_contig_16bit(float *rectf, const unsigned short *sbuf, int scanline_w, int spp)
{
	int i;
	for (i = 0; i < scanline_w; i++) {
		rectf[i * 4 + 0] = sbuf[i * spp + 0] / 65535.0;
		rectf[i * 4 + 1] = (spp >= 3) ? sbuf[i * spp + 1] / 65535.0 : sbuf[i * spp + 0] / 65535.0;
		rectf[i * 4 + 2] = (spp >= 3) ? sbuf[i * spp + 2] / 65535.0 : sbuf[i * spp + 0] / 65535.0;
		rectf[i * 4 + 3] = (spp == 4) ? (sbuf[i * spp + 3] / 65535.0) : 1.0;
	}
}

static void scanline_contig_32bit(float *rectf, const float *fbuf, int scanline_w, int spp)
{
	int i;
	for (i = 0; i < scanline_w; i++) {
		rectf[i * 4 + 0] = fbuf[i * spp + 0];
		rectf[i * 4 + 1] = (spp >= 3) ? fbuf[i * spp + 1] : fbuf[i * spp + 0];
		rectf[i * 4 + 2] = (spp >= 3) ? fbuf[i * spp + 2] : fbuf[i * spp + 0];
		rectf[i * 4 + 3] = (spp == 4) ? fbuf[i * spp + 3] : 1.0f;
	}
}

static void scanline_separate_16bit(float *rectf, const unsigned short *sbuf, int scanline_w, int chan)
{
	int i;
	for (i = 0; i < scanline_w; i++)
		rectf[i * 4 + chan] = sbuf[i] / 65535.0;
}

static void scanline_separate_32bit(float *rectf, const float *fbuf, int scanline_w, int chan)
{
	int i;
	for (i = 0; i < scanline_w; i++)
		rectf[i * 4 + chan] = fbuf[i];
}

static void imb_read_tiff_resolution(ImBuf *ibuf, TIFF *image)
{
	uint16 unit;
	float xres;
	float yres;

	TIFFGetFieldDefaulted(image, TIFFTAG_RESOLUTIONUNIT, &unit);
	TIFFGetFieldDefaulted(image, TIFFTAG_XRESOLUTION, &xres);
	TIFFGetFieldDefaulted(image, TIFFTAG_YRESOLUTION, &yres);

	if (unit == RESUNIT_CENTIMETER) {
		ibuf->ppm[0] = (double)xres * 100.0;
		ibuf->ppm[1] = (double)yres * 100.0;
	}
	else {
		ibuf->ppm[0] = (double)xres / 0.0254;
		ibuf->ppm[1] = (double)yres / 0.0254;
	}
}

/* 
 * Use the libTIFF scanline API to read a TIFF image.
 * This method is most flexible and can handle multiple different bit depths 
 * and RGB channel orderings.
 */
static int imb_read_tiff_pixels(ImBuf *ibuf, TIFF *image)
{
	ImBuf *tmpibuf = NULL;
	int success = 0;
	short bitspersample, spp, config;
	size_t scanline;
	int ib_flag = 0, row, chan;
	float *fbuf = NULL;
	unsigned short *sbuf = NULL;

	TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	TIFFGetField(image, TIFFTAG_SAMPLESPERPIXEL, &spp);     /* number of 'channels' */
	TIFFGetField(image, TIFFTAG_PLANARCONFIG, &config);

	if (spp == 4) {
		/* HACK: this is really tricky hack, which is only needed to force libtiff
		 *       do not touch RGB channels when there's alpha channel present
		 *       The thing is: libtiff will premul RGB if alpha mode is set to
		 *       unassociated, which really conflicts with blender's assumptions
		 *
		 *       Alternative would be to unpremul after load, but it'll be really
		 *       lossy and unwanted behavior
		 *
		 *       So let's keep this thing here for until proper solution is found (sergey)
		 */

		unsigned short extraSampleTypes[1];
		extraSampleTypes[0] = EXTRASAMPLE_ASSOCALPHA;
		TIFFSetField(image, TIFFTAG_EXTRASAMPLES, 1, extraSampleTypes);
	}

	imb_read_tiff_resolution(ibuf, image);

	scanline = TIFFScanlineSize(image);
	
	if (bitspersample == 32) {
		ib_flag = IB_rectfloat;
		fbuf = (float *)_TIFFmalloc(scanline);
		if (!fbuf) {
			goto cleanup;
		}
	}
	else if (bitspersample == 16) {
		ib_flag = IB_rectfloat;
		sbuf = (unsigned short *)_TIFFmalloc(scanline);
		if (!sbuf) {
			goto cleanup;
		}
	}
	else {
		ib_flag = IB_rect;
	}
	
	tmpibuf = IMB_allocImBuf(ibuf->x, ibuf->y, ibuf->planes, ib_flag);
	if (!tmpibuf) {
		goto cleanup;
	}
	
	/* simple RGBA image */
	if (!(bitspersample == 32 || bitspersample == 16)) {
		success |= TIFFReadRGBAImage(image, ibuf->x, ibuf->y, tmpibuf->rect, 0);
	}
	/* contiguous channels: RGBRGBRGB */
	else if (config == PLANARCONFIG_CONTIG) {
		for (row = 0; row < ibuf->y; row++) {
			size_t ib_offset = (size_t)ibuf->x * 4 * ((size_t)ibuf->y - ((size_t)row + 1));
		
			if (bitspersample == 32) {
				success |= TIFFReadScanline(image, fbuf, row, 0);
				scanline_contig_32bit(tmpibuf->rect_float + ib_offset, fbuf, ibuf->x, spp);
				
			}
			else if (bitspersample == 16) {
				success |= TIFFReadScanline(image, sbuf, row, 0);
				scanline_contig_16bit(tmpibuf->rect_float + ib_offset, sbuf, ibuf->x, spp);
			}
		}
		/* separate channels: RRRGGGBBB */
	}
	else if (config == PLANARCONFIG_SEPARATE) {
		
		/* imbufs always have 4 channels of data, so we iterate over all of them
		 * but only fill in from the TIFF scanline where necessary. */
		for (chan = 0; chan < 4; chan++) {
			for (row = 0; row < ibuf->y; row++) {
				size_t ib_offset = (size_t)ibuf->x * 4 * ((size_t)ibuf->y - ((size_t)row + 1));
				
				if (bitspersample == 32) {
					if (chan == 3 && spp == 3) /* fill alpha if only RGB TIFF */
						copy_vn_fl(fbuf, ibuf->x, 1.0f);
					else if (chan >= spp) /* for grayscale, duplicate first channel into G and B */
						success |= TIFFReadScanline(image, fbuf, row, 0);
					else
						success |= TIFFReadScanline(image, fbuf, row, chan);
					scanline_separate_32bit(tmpibuf->rect_float + ib_offset, fbuf, ibuf->x, chan);
					
				}
				else if (bitspersample == 16) {
					if (chan == 3 && spp == 3) /* fill alpha if only RGB TIFF */
						copy_vn_ushort(sbuf, ibuf->x, 65535);
					else if (chan >= spp) /* for grayscale, duplicate first channel into G and B */
						success |= TIFFReadScanline(image, fbuf, row, 0);
					else
						success |= TIFFReadScanline(image, sbuf, row, chan);
					scanline_separate_16bit(tmpibuf->rect_float + ib_offset, sbuf, ibuf->x, chan);
					
				}
			}
		}
	}

	if (success) {
		/* Code seems to be not needed for 16 bits tif, on PPC G5 OSX (ton) */
		if (bitspersample < 16)
			if (ENDIAN_ORDER == B_ENDIAN)
				IMB_convert_rgba_to_abgr(tmpibuf);
		
		/* assign rect last */
		if (tmpibuf->rect_float)
			ibuf->rect_float = tmpibuf->rect_float;
		else
			ibuf->rect = tmpibuf->rect;
		ibuf->mall |= ib_flag;
		ibuf->flags |= ib_flag;
		
		tmpibuf->mall &= ~ib_flag;
	}

cleanup:
	if (bitspersample == 32)
		_TIFFfree(fbuf);
	else if (bitspersample == 16)
		_TIFFfree(sbuf);

	IMB_freeImBuf(tmpibuf);
	
	return success;
}

void imb_inittiff(void)
{
	if (!(G.debug & G_DEBUG))
		TIFFSetErrorHandler(NULL);
}

/**
 * Loads a TIFF file.
 *
 *
 * \param mem:   Memory containing the TIFF file.
 * \param size:  Size of the mem buffer.
 * \param flags: If flags has IB_test set then the file is not actually loaded,
 *                but all other operations take place.
 *
 * \return: A newly allocated ImBuf structure if successful, otherwise NULL.
 */
ImBuf *imb_loadtiff(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	TIFF *image = NULL;
	ImBuf *ibuf = NULL, *hbuf;
	ImbTIFFMemFile memFile;
	uint32 width, height;
	char *format = NULL;
	int level;
	short spp;
	int ib_depth;
	int found;

	/* check whether or not we have a TIFF file */
	if (size < IMB_TIFF_NCB) {
		fprintf(stderr, "imb_loadtiff: size < IMB_TIFF_NCB\n");
		return NULL;
	}
	if (imb_is_a_tiff(mem) == 0)
		return NULL;

	/* both 8 and 16 bit PNGs are default to standard byte colorspace */
	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

	image = imb_tiff_client_open(&memFile, mem, size);

	if (image == NULL) {
		printf("imb_loadtiff: could not open TIFF IO layer.\n");
		return NULL;
	}

	/* allocate the image buffer */
	TIFFGetField(image, TIFFTAG_IMAGEWIDTH,  &width);
	TIFFGetField(image, TIFFTAG_IMAGELENGTH, &height);
	TIFFGetField(image, TIFFTAG_SAMPLESPERPIXEL, &spp);
	
	ib_depth = (spp == 3) ? 24 : 32;
	
	ibuf = IMB_allocImBuf(width, height, ib_depth, 0);
	if (ibuf) {
		ibuf->ftype = IMB_FTYPE_TIF;
	}
	else {
		fprintf(stderr, 
		        "imb_loadtiff: could not allocate memory for TIFF "
		        "image.\n");
		TIFFClose(image);
		return NULL;
	}

	/* get alpha mode from file header */
	if (flags & IB_alphamode_detect) {
		if (spp == 4) {
			unsigned short extra, *extraSampleTypes;

			found = TIFFGetField(image, TIFFTAG_EXTRASAMPLES, &extra, &extraSampleTypes);

			if (found && (extraSampleTypes[0] == EXTRASAMPLE_ASSOCALPHA)) {
				ibuf->flags |= IB_alphamode_premul;
			}
		}
	}

	/* if testing, we're done */
	if (flags & IB_test) {
		TIFFClose(image);
		return ibuf;
	}

	/* detect if we are reading a tiled/mipmapped texture, in that case
	 * we don't read pixels but leave it to the cache to load tiles */
	if (flags & IB_tilecache) {
		format = NULL;
		TIFFGetField(image, TIFFTAG_PIXAR_TEXTUREFORMAT, &format);

		if (format && STREQ(format, "Plain Texture") && TIFFIsTiled(image)) {
			int numlevel = TIFFNumberOfDirectories(image);

			/* create empty mipmap levels in advance */
			for (level = 0; level < numlevel; level++) {
				if (!TIFFSetDirectory(image, level))
					break;

				if (level > 0) {
					width = (width > 1) ? width / 2 : 1;
					height = (height > 1) ? height / 2 : 1;

					hbuf = IMB_allocImBuf(width, height, 32, 0);
					hbuf->miplevel = level;
					hbuf->ftype = ibuf->ftype;
					ibuf->mipmap[level - 1] = hbuf;
				}
				else
					hbuf = ibuf;

				hbuf->flags |= IB_tilecache;

				TIFFGetField(image, TIFFTAG_TILEWIDTH, &hbuf->tilex);
				TIFFGetField(image, TIFFTAG_TILELENGTH, &hbuf->tiley);

				hbuf->xtiles = ceil(hbuf->x / (float)hbuf->tilex);
				hbuf->ytiles = ceil(hbuf->y / (float)hbuf->tiley);

				imb_addtilesImBuf(hbuf);

				ibuf->miptot++;
			}
		}
	}

	/* read pixels */
	if (!(ibuf->flags & IB_tilecache) && !imb_read_tiff_pixels(ibuf, image)) {
		fprintf(stderr, "imb_loadtiff: Failed to read tiff image.\n");
		TIFFClose(image);
		return NULL;
	}

	/* close the client layer interface to the in-memory file */
	TIFFClose(image);

	/* return successfully */
	return ibuf;
}

void imb_loadtiletiff(ImBuf *ibuf, const unsigned char *mem, size_t size, int tx, int ty, unsigned int *rect)
{
	TIFF *image = NULL;
	uint32 width, height;
	ImbTIFFMemFile memFile;

	image = imb_tiff_client_open(&memFile, mem, size);

	if (image == NULL) {
		printf("imb_loadtiff: could not open TIFF IO layer for loading mipmap level.\n");
		return;
	}

	if (TIFFSetDirectory(image, ibuf->miplevel)) { /* allocate the image buffer */
		TIFFGetField(image, TIFFTAG_IMAGEWIDTH,  &width);
		TIFFGetField(image, TIFFTAG_IMAGELENGTH, &height);

		if (width == ibuf->x && height == ibuf->y) {
			if (rect) {
				/* tiff pixels are bottom to top, tiles are top to bottom */
				if (TIFFReadRGBATile(image, tx * ibuf->tilex, (ibuf->ytiles - 1 - ty) * ibuf->tiley, rect) == 1) {
					if (ibuf->tiley > ibuf->y)
						memmove(rect, rect + ibuf->tilex * (ibuf->tiley - ibuf->y), sizeof(int) * ibuf->tilex * ibuf->y);
				}
				else
					printf("imb_loadtiff: failed to read tiff tile at mipmap level %d\n", ibuf->miplevel);
			}
		}
		else
			printf("imb_loadtiff: mipmap level %d has unexpected size %ux%u instead of %dx%d\n", ibuf->miplevel, width, height, ibuf->x, ibuf->y);
	}
	else
		printf("imb_loadtiff: could not find mipmap level %d\n", ibuf->miplevel);

	/* close the client layer interface to the in-memory file */
	TIFFClose(image);
}

/**
 * Saves a TIFF file.
 *
 * ImBuf structures with 1, 3 or 4 bytes per pixel (GRAY, RGB, RGBA 
 * respectively) are accepted, and interpreted correctly.  Note that the TIFF
 * convention is to use pre-multiplied alpha, which can be achieved within
 * Blender by setting "Premul" alpha handling.  Other alpha conventions are
 * not strictly correct, but are permitted anyhow.
 *
 * \param ibuf:  Image buffer.
 * \param name:  Name of the TIFF file to create.
 * \param flags: Currently largely ignored.
 *
 * \return: 1 if the function is successful, 0 on failure.
 */

int imb_savetiff(ImBuf *ibuf, const char *name, int flags)
{
	TIFF *image = NULL;
	uint16 samplesperpixel, bitspersample;
	size_t npixels;
	unsigned char *pixels = NULL;
	unsigned char *from = NULL, *to = NULL;
	unsigned short *pixels16 = NULL, *to16 = NULL;
	float *fromf = NULL;
	float xres, yres;
	int x, y, from_i, to_i, i;
	int compress_mode = COMPRESSION_NONE;

	/* check for a valid number of bytes per pixel.  Like the PNG writer,
	 * the TIFF writer supports 1, 3 or 4 bytes per pixel, corresponding
	 * to gray, RGB, RGBA respectively. */
	samplesperpixel = (uint16)((ibuf->planes + 7) >> 3);
	if ((samplesperpixel > 4) || (samplesperpixel == 2)) {
		fprintf(stderr,
		        "imb_savetiff: unsupported number of bytes per "
		        "pixel: %d\n", samplesperpixel);
		return (0);
	}

	if ((ibuf->foptions.flag & TIF_16BIT) && ibuf->rect_float)
		bitspersample = 16;
	else
		bitspersample = 8;

	if (ibuf->foptions.flag & TIF_COMPRESS_DEFLATE)
		compress_mode = COMPRESSION_DEFLATE;
	else if (ibuf->foptions.flag & TIF_COMPRESS_LZW)
		compress_mode = COMPRESSION_LZW;
	else if (ibuf->foptions.flag & TIF_COMPRESS_PACKBITS)
		compress_mode = COMPRESSION_PACKBITS;

	/* open TIFF file for writing */
	if (flags & IB_mem) {
		/* bork at the creation of a TIFF in memory */
		fprintf(stderr,
		        "imb_savetiff: creation of in-memory TIFF files is "
		        "not yet supported.\n");
		return (0);
	}
	else {
		/* create image as a file */
#ifdef WIN32
		wchar_t *wname = alloc_utf16_from_8(name, 0);
		image = TIFFOpenW(wname, "w");
		free(wname);
#else
		image = TIFFOpen(name, "w");
#endif
	}
	if (image == NULL) {
		fprintf(stderr,
		        "imb_savetiff: could not open TIFF for writing.\n");
		return (0);
	}

	/* allocate array for pixel data */
	npixels = ibuf->x * ibuf->y;
	if (bitspersample == 16)
		pixels16 = (unsigned short *)_TIFFmalloc(npixels *
		                                         samplesperpixel * sizeof(unsigned short));
	else
		pixels = (unsigned char *)_TIFFmalloc(npixels *
		                                      samplesperpixel * sizeof(unsigned char));

	if (pixels == NULL && pixels16 == NULL) {
		fprintf(stderr,
		        "imb_savetiff: could not allocate pixels array.\n");
		TIFFClose(image);
		return (0);
	}

	/* setup pointers */
	if (bitspersample == 16) {
		fromf = ibuf->rect_float;
		to16   = pixels16;
	}
	else {
		from = (unsigned char *)ibuf->rect;
		to   = pixels;
	}

	/* setup samples per pixel */
	TIFFSetField(image, TIFFTAG_BITSPERSAMPLE, bitspersample);
	TIFFSetField(image, TIFFTAG_SAMPLESPERPIXEL, samplesperpixel);

	if (samplesperpixel == 4) {
		unsigned short extraSampleTypes[1];

		if (bitspersample == 16)
			extraSampleTypes[0] = EXTRASAMPLE_ASSOCALPHA;
		else
			extraSampleTypes[0] = EXTRASAMPLE_UNASSALPHA;

		/* RGBA images */
		TIFFSetField(image, TIFFTAG_EXTRASAMPLES, 1,
		             extraSampleTypes);
		TIFFSetField(image, TIFFTAG_PHOTOMETRIC, 
		             PHOTOMETRIC_RGB);
	}
	else if (samplesperpixel == 3) {
		/* RGB images */
		TIFFSetField(image, TIFFTAG_PHOTOMETRIC,
		             PHOTOMETRIC_RGB);
	}
	else if (samplesperpixel == 1) {
		/* grayscale images, 1 channel */
		TIFFSetField(image, TIFFTAG_PHOTOMETRIC,
		             PHOTOMETRIC_MINISBLACK);
	}

	/* copy pixel data.  While copying, we flip the image vertically. */
	const int channels_in_float = ibuf->channels ? ibuf->channels : 4;
	for (x = 0; x < ibuf->x; x++) {
		for (y = 0; y < ibuf->y; y++) {
			from_i = ((size_t)channels_in_float) * (y * ibuf->x + x);
			to_i   = samplesperpixel * ((ibuf->y - y - 1) * ibuf->x + x);

			if (pixels16) {
				/* convert from float source */
				float rgb[4];

				if (channels_in_float == 3 || channels_in_float == 4) {
					if (ibuf->float_colorspace ||
					    (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA))
					{
						/* Float buffer was managed already, no need in color
						 * space conversion.
						 */
						copy_v3_v3(rgb, &fromf[from_i]);
					}
					else {
						/* Standard linear-to-srgb conversion if float buffer
						 * wasn't managed.
						 */
						linearrgb_to_srgb_v3_v3(rgb, &fromf[from_i]);
					}
					if (channels_in_float == 4) {
						rgb[3] = fromf[from_i + 3];
					}
					else {
						rgb[3] = 1.0f;
					}
				}
				else {
					if (ibuf->float_colorspace ||
					    (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA))
					{
						rgb[0] = fromf[from_i];
					}
					else {
						rgb[0] = linearrgb_to_srgb(fromf[from_i]);
					}
					rgb[1] = rgb[2] = rgb[0];
					rgb[3] = 1.0f;
				}

				for (i = 0; i < samplesperpixel; i++, to_i++)
					to16[to_i] = FTOUSHORT(rgb[i]);
			}
			else {
				for (i = 0; i < samplesperpixel; i++, to_i++, from_i++)
					to[to_i] = from[from_i];
			}
		}
	}

	/* write the actual TIFF file */
	TIFFSetField(image, TIFFTAG_IMAGEWIDTH,      ibuf->x);
	TIFFSetField(image, TIFFTAG_IMAGELENGTH,     ibuf->y);
	TIFFSetField(image, TIFFTAG_ROWSPERSTRIP,    ibuf->y);
	TIFFSetField(image, TIFFTAG_COMPRESSION, compress_mode);
	TIFFSetField(image, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
	TIFFSetField(image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);


	if (ibuf->ppm[0] > 0.0 && ibuf->ppm[1] > 0.0) {
		xres = (float)(ibuf->ppm[0] * 0.0254);
		yres = (float)(ibuf->ppm[1] * 0.0254);
	}
	else {
		xres = yres = IMB_DPI_DEFAULT;
	}

	TIFFSetField(image, TIFFTAG_XRESOLUTION,     xres);
	TIFFSetField(image, TIFFTAG_YRESOLUTION,     yres);
	TIFFSetField(image, TIFFTAG_RESOLUTIONUNIT,  RESUNIT_INCH);
	if (TIFFWriteEncodedStrip(image, 0,
	                          (bitspersample == 16) ? (unsigned char *)pixels16 : pixels,
	                          (size_t)ibuf->x * ibuf->y * samplesperpixel * bitspersample / 8) == -1)
	{
		fprintf(stderr,
		        "imb_savetiff: Could not write encoded TIFF.\n");
		TIFFClose(image);
		if (pixels) _TIFFfree(pixels);
		if (pixels16) _TIFFfree(pixels16);
		return (1);
	}

	/* close the TIFF file */
	TIFFClose(image);
	if (pixels) _TIFFfree(pixels);
	if (pixels16) _TIFFfree(pixels16);
	return (1);
}
