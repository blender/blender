/*
 * tiff.c
 *
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Jonathan Merritt.
 *
 * ***** END GPL LICENSE BLOCK *****
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
 * Saving supports RGB, RGBA and BW (greyscale) images correctly, with
 * 8 bits per channel in all cases.  The "deflate" compression algorithm is
 * used to compress images.
 */

#include <string.h>

#include "imbuf.h"
#include "imbuf_patch.h"

#include "BKE_global.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_cmap.h"
#include "IMB_tiff.h"

#include "dynlibtiff.h"



/***********************
 * Local declarations. *
 ***********************/
/* Reading and writing of an in-memory TIFF file. */
tsize_t imb_tiff_ReadProc(thandle_t handle, tdata_t data, tsize_t n);
tsize_t imb_tiff_WriteProc(thandle_t handle, tdata_t data, tsize_t n);
toff_t  imb_tiff_SeekProc(thandle_t handle, toff_t ofs, int whence);
int     imb_tiff_CloseProc(thandle_t handle);
toff_t  imb_tiff_SizeProc(thandle_t handle);
int     imb_tiff_DummyMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize);
void    imb_tiff_DummyUnmapProc(thandle_t fd, tdata_t base, toff_t size);


/* Structure for in-memory TIFF file. */
struct ImbTIFFMemFile {
	unsigned char *mem;	/* Location of first byte of TIFF file. */
	toff_t offset;		/* Current offset within the file.      */
	tsize_t size;		/* Size of the TIFF file.               */
};
#define IMB_TIFF_GET_MEMFILE(x) ((struct ImbTIFFMemFile*)(x));



/*****************************
 * Function implementations. *
 *****************************/


void imb_tiff_DummyUnmapProc(thandle_t fd, tdata_t base, toff_t size)
{
}

int imb_tiff_DummyMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize) 
{
            return (0);
}

/**
 * Reads data from an in-memory TIFF file.
 *
 * @param handle: Handle of the TIFF file (pointer to ImbTIFFMemFile).
 * @param data:   Buffer to contain data (treat as void*).
 * @param n:      Number of bytes to read.
 *
 * @return: Number of bytes actually read.
 * 	 0 = EOF.
 */
tsize_t imb_tiff_ReadProc(thandle_t handle, tdata_t data, tsize_t n)
{
	tsize_t nRemaining, nCopy;
	struct ImbTIFFMemFile* mfile;
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
	srcAddr = (void*)(&(mfile->mem[mfile->offset]));
	memcpy((void*)data, srcAddr, nCopy);
	mfile->offset += nCopy;		/* advance file ptr by copied bytes */
	return nCopy;
}



/**
 * Writes data to an in-memory TIFF file.
 *
 * NOTE: The current Blender implementation should not need this function.  It
 *       is simply a stub.
 */
tsize_t imb_tiff_WriteProc(thandle_t handle, tdata_t data, tsize_t n)
{
	printf("imb_tiff_WriteProc: this function should not be called.\n");
	return (-1);
}



/**
 * Seeks to a new location in an in-memory TIFF file.
 *
 * @param handle: Handle of the TIFF file (pointer to ImbTIFFMemFile).
 * @param ofs:    Offset value (interpreted according to whence below).
 * @param whence: This can be one of three values:
 * 	SEEK_SET - The offset is set to ofs bytes.
 * 	SEEK_CUR - The offset is set to its current location plus ofs bytes.
 * 	SEEK_END - (This is unsupported and will return -1, indicating an
 * 	            error).
 *
 * @return: Resulting offset location within the file, measured in bytes from
 *          the beginning of the file.  (-1) indicates an error.
 */
toff_t imb_tiff_SeekProc(thandle_t handle, toff_t ofs, int whence)
{
	struct ImbTIFFMemFile *mfile;
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
 * @param handle: Handle of the TIFF file (pointer to ImbTIFFMemFile).
 *
 * @return: 0
 */
int imb_tiff_CloseProc(thandle_t handle)
{
	struct ImbTIFFMemFile *mfile;

	/* get the pointer to the in-memory file */
	mfile = IMB_TIFF_GET_MEMFILE(handle);
	if (!mfile || !mfile->mem) {
		fprintf(stderr,"imb_tiff_CloseProc: !mfile || !mfile->mem!\n");
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
 * @return: Size of file (in bytes).
 */
toff_t imb_tiff_SizeProc(thandle_t handle)
{
	struct ImbTIFFMemFile* mfile;

	/* get the pointer to the in-memory file */
	mfile = IMB_TIFF_GET_MEMFILE(handle);
	if (!mfile || !mfile->mem) {
		fprintf(stderr,"imb_tiff_SizeProc: !mfile || !mfile->mem!\n");
		return (0);
	}

	/* return the size */
	return (toff_t)(mfile->size);
}



/**
 * Checks whether a given memory buffer contains a TIFF file.
 *
 * FIXME: Possible memory leak if mem is less than IMB_TIFF_NCB bytes long.
 *        However, changing this will require up-stream modifications.
 *
 * This method uses the format identifiers from:
 *     http://www.faqs.org/faqs/graphics/fileformats-faq/part4/section-9.html
 * The first four bytes of big-endian and little-endian TIFF files
 * respectively are (hex):
 * 	4d 4d 00 2a
 * 	49 49 2a 00
 * Note that TIFF files on *any* platform can be either big- or little-endian;
 * it's not platform-specific.
 *
 * AFAICT, libtiff doesn't provide a method to do this automatically, and
 * hence my manual comparison. - Jonathan Merritt (lancelet) 4th Sept 2005.
 */
#define IMB_TIFF_NCB 4		/* number of comparison bytes used */
int imb_is_a_tiff(void *mem)
{
	char big_endian[IMB_TIFF_NCB] = { 0x4d, 0x4d, 0x00, 0x2a };
	char lil_endian[IMB_TIFF_NCB] = { 0x49, 0x49, 0x2a, 0x00 };

	return ( (memcmp(big_endian, mem, IMB_TIFF_NCB) == 0) ||
		 (memcmp(lil_endian, mem, IMB_TIFF_NCB) == 0) );
}



/**
 * Loads a TIFF file.
 *
 * This function uses the "RGBA Image" support from libtiff, which enables
 * it to load most commonly-encountered TIFF formats.  libtiff handles format
 * conversion, color depth conversion, etc.
 *
 * @param mem:   Memory containing the TIFF file.
 * @param size:  Size of the mem buffer.
 * @param flags: If flags has IB_test set then the file is not actually loaded,
 *                but all other operations take place.
 *
 * @return: A newly allocated ImBuf structure if successful, otherwise NULL.
 */
struct ImBuf *imb_loadtiff(unsigned char *mem, int size, int flags)
{
	TIFF *image = NULL;
	struct ImBuf *ibuf = NULL;
	struct ImbTIFFMemFile memFile;
	uint32 width, height;
	int bytesperpixel, bitspersample;
	int success;
	unsigned int pixel_i, byte_i;
	uint32 *raster = NULL;
	uint32 pixel;
	unsigned char *to = NULL;

	memFile.mem = mem;
	memFile.offset = 0;
	memFile.size = size;

	/* check whether or not we have a TIFF file */
	if (size < IMB_TIFF_NCB) {
		fprintf(stderr, "imb_loadtiff: size < IMB_TIFF_NCB\n");
		return NULL;
	}
	if (imb_is_a_tiff(mem) == 0)
		return NULL;

	/* open the TIFF client layer interface to the in-memory file */
	image = libtiff_TIFFClientOpen("(Blender TIFF Interface Layer)", 
		"r", (thandle_t)(&memFile),
		imb_tiff_ReadProc, imb_tiff_WriteProc,
		imb_tiff_SeekProc, imb_tiff_CloseProc,
		imb_tiff_SizeProc, imb_tiff_DummyMapProc, imb_tiff_DummyUnmapProc);
	if (image == NULL) {
		printf("imb_loadtiff: could not open TIFF IO layer.\n");
		return NULL;
	}

	/* allocate the image buffer */
	bytesperpixel = 4;  /* 1 byte per channel, 4 channels */
	libtiff_TIFFGetField(image, TIFFTAG_IMAGEWIDTH,  &width);
	libtiff_TIFFGetField(image, TIFFTAG_IMAGELENGTH, &height);
	libtiff_TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	ibuf = IMB_allocImBuf(width, height, 8*bytesperpixel, 0, 0);
	if (ibuf) {
		ibuf->ftype = TIF;
		ibuf->profile = IB_PROFILE_SRGB;
	} else {
		fprintf(stderr, 
			"imb_loadtiff: could not allocate memory for TIFF " \
			"image.\n");
		libtiff_TIFFClose(image);
		return NULL;
	}

	/* read in the image data */
	if (!(flags & IB_test)) {

		/* allocate memory for the ibuf->rect */
		imb_addrectImBuf(ibuf);

		/* perform actual read */
		raster = (uint32*)libtiff__TIFFmalloc(
				width*height * sizeof(uint32));
		if (raster == NULL) {
			libtiff_TIFFClose(image);
			return NULL;
		}
		success = libtiff_TIFFReadRGBAImage(
				image, width, height, raster, 0);
		if (!success) {
			fprintf(stderr,
				"imb_loadtiff: This TIFF format is not " 
				"currently supported by Blender.\n");
			libtiff__TIFFfree(raster);
			libtiff_TIFFClose(image);
			return NULL;
		}

		/* copy raster to ibuf->rect; we do a fast copy if possible,
		 * otherwise revert to a slower component-wise copy */
		if (sizeof(unsigned int) == sizeof(uint32)) {
			memcpy(ibuf->rect, raster, 
				width*height*sizeof(uint32));
		} else {
			/* this may not be entirely necessary, but is put here
			 * in case sizeof(unsigned int) is not a 32-bit
			 * quantity */
			fprintf(stderr,
				"imb_loadtiff: using (slower) component-wise "
				"buffer copy.\n");
			to = (unsigned char*)ibuf->rect;
			for (pixel_i=0; pixel_i < width*height; pixel_i++)
			{	
				byte_i = sizeof(unsigned int)*pixel_i;
				pixel = raster[pixel_i];
	
				to[byte_i++] = (unsigned char)TIFFGetR(pixel);
				to[byte_i++] = (unsigned char)TIFFGetG(pixel);
				to[byte_i++] = (unsigned char)TIFFGetB(pixel);
				to[byte_i++] = (unsigned char)TIFFGetA(pixel);
			}
		}

		libtiff__TIFFfree(raster);
	}

	/* close the client layer interface to the in-memory file */
	libtiff_TIFFClose(image);

	if (ENDIAN_ORDER == B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);

	/* return successfully */
	return (ibuf);
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
 * @param ibuf:  Image buffer.
 * @param name:  Name of the TIFF file to create.
 * @param flags: Currently largely ignored.
 *
 * @return: 1 if the function is successful, 0 on failure.
 */

#define FTOUSHORT(val) ((val >= 1.0f-0.5f/65535)? 65535: (val <= 0.0f)? 0: (unsigned short)(val*65535.0f + 0.5f))

short imb_savetiff(struct ImBuf *ibuf, char *name, int flags)
{
	TIFF *image = NULL;
	uint16 samplesperpixel, bitspersample;
	size_t npixels;
	unsigned char *pixels = NULL;
	unsigned char *from = NULL, *to = NULL;
	unsigned short *pixels16 = NULL, *to16 = NULL;
	float *fromf = NULL;
	int x, y, from_i, to_i, i;
	int extraSampleTypes[1] = { EXTRASAMPLE_ASSOCALPHA };

	/* check for a valid number of bytes per pixel.  Like the PNG writer,
	 * the TIFF writer supports 1, 3 or 4 bytes per pixel, corresponding
	 * to gray, RGB, RGBA respectively. */
	samplesperpixel = (uint16)((ibuf->depth + 7) >> 3);
	if ((samplesperpixel > 4) || (samplesperpixel == 2)) {
		fprintf(stderr,
			"imb_savetiff: unsupported number of bytes per " 
			"pixel: %d\n", samplesperpixel);
		return (0);
	}

	if((ibuf->ftype & TIF_16BIT) && ibuf->rect_float)
		bitspersample = 16;
	else
		bitspersample = 8;

	/* open TIFF file for writing */
	if (flags & IB_mem) {
		/* bork at the creation of a TIFF in memory */
		fprintf(stderr,
			"imb_savetiff: creation of in-memory TIFF files is " 
			"not yet supported.\n");
		return (0);
	} else {
		/* create image as a file */
		image = libtiff_TIFFOpen(name, "w");
	}
	if (image == NULL) {
		fprintf(stderr,
			"imb_savetiff: could not open TIFF for writing.\n");
		return (0);
	}

	/* allocate array for pixel data */
	npixels = ibuf->x * ibuf->y;
	if(bitspersample == 16)
		pixels16 = (unsigned short*)libtiff__TIFFmalloc(npixels *
			samplesperpixel * sizeof(unsigned short));
	else
		pixels = (unsigned char*)libtiff__TIFFmalloc(npixels *
			samplesperpixel * sizeof(unsigned char));

	if (pixels == NULL && pixels16 == NULL) {
		fprintf(stderr,
			"imb_savetiff: could not allocate pixels array.\n");
		libtiff_TIFFClose(image);
		return (0);
	}

	/* setup pointers */
	if(bitspersample == 16) {
		fromf = ibuf->rect_float;
		to16   = pixels16;
	}
	else {
		from = (unsigned char*)ibuf->rect;
		to   = pixels;
	}

	/* setup samples per pixel */
	libtiff_TIFFSetField(image, TIFFTAG_BITSPERSAMPLE, bitspersample);
	libtiff_TIFFSetField(image, TIFFTAG_SAMPLESPERPIXEL, samplesperpixel);

	if(samplesperpixel == 4) {
		/* RGBA images */
		libtiff_TIFFSetField(image, TIFFTAG_EXTRASAMPLES, 1,
				extraSampleTypes);
		libtiff_TIFFSetField(image, TIFFTAG_PHOTOMETRIC, 
				PHOTOMETRIC_RGB);
	}
	else if(samplesperpixel == 3) {
		/* RGB images */
		libtiff_TIFFSetField(image, TIFFTAG_PHOTOMETRIC,
				PHOTOMETRIC_RGB);
	}
	else if(samplesperpixel == 1) {
		/* greyscale images, 1 channel */
		libtiff_TIFFSetField(image, TIFFTAG_PHOTOMETRIC,
				PHOTOMETRIC_MINISBLACK);
	}

	/* copy pixel data.  While copying, we flip the image vertically. */
	for (x = 0; x < ibuf->x; x++) {
		for (y = 0; y < ibuf->y; y++) {
			from_i = 4*(y*ibuf->x+x);
			to_i   = samplesperpixel*((ibuf->y-y-1)*ibuf->x+x);

			if(pixels16) {
				for (i = 0; i < samplesperpixel; i++, to_i++, from_i++)
					to16[to_i] = FTOUSHORT(fromf[from_i]);
			}
			else {
				for (i = 0; i < samplesperpixel; i++, to_i++, from_i++)
					to[to_i] = from[from_i];
			}
		}
	}

	/* write the actual TIFF file */
	libtiff_TIFFSetField(image, TIFFTAG_IMAGEWIDTH,      ibuf->x);
	libtiff_TIFFSetField(image, TIFFTAG_IMAGELENGTH,     ibuf->y);
	libtiff_TIFFSetField(image, TIFFTAG_ROWSPERSTRIP,    ibuf->y);
	libtiff_TIFFSetField(image, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
	libtiff_TIFFSetField(image, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
	libtiff_TIFFSetField(image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	libtiff_TIFFSetField(image, TIFFTAG_XRESOLUTION,     150.0);
	libtiff_TIFFSetField(image, TIFFTAG_YRESOLUTION,     150.0);
	libtiff_TIFFSetField(image, TIFFTAG_RESOLUTIONUNIT,  RESUNIT_INCH);
	if (libtiff_TIFFWriteEncodedStrip(image, 0,
			(bitspersample == 16)? (unsigned char*)pixels16: pixels,
			ibuf->x*ibuf->y*samplesperpixel*bitspersample/8) == -1) {
		fprintf(stderr,
			"imb_savetiff: Could not write encoded TIFF.\n");
		libtiff_TIFFClose(image);
		if(pixels) libtiff__TIFFfree(pixels);
		if(pixels16) libtiff__TIFFfree(pixels16);
		return (1);
	}

	/* close the TIFF file */
	libtiff_TIFFClose(image);
	if(pixels) libtiff__TIFFfree(pixels);
	if(pixels16) libtiff__TIFFfree(pixels16);
	return (1);
}

