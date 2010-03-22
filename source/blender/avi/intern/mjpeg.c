/**
 * mjpeg.c
 *
 * This is external code. Converts between avi and mpeg/jpeg.
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
 *  */

#include "AVI_avi.h"
#include <stdlib.h>
#include <string.h>
#include "jpeglib.h"
#include "jerror.h"
#include "MEM_guardedalloc.h"

#include "mjpeg.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define PADUP(num, amt)	((num+(amt-1))&~(amt-1))

static void jpegmemdestmgr_build (j_compress_ptr cinfo, unsigned char *buffer, int bufsize);
static void jpegmemsrcmgr_build (j_decompress_ptr dinfo, unsigned char *buffer, int bufsize);

static int numbytes;

static void add_huff_table (j_decompress_ptr dinfo, JHUFF_TBL **htblptr, const UINT8 *bits, const UINT8 *val) {
	if (*htblptr == NULL)
		*htblptr = jpeg_alloc_huff_table((j_common_ptr) dinfo);

	memcpy((*htblptr)->bits, bits, sizeof((*htblptr)->bits));
	memcpy((*htblptr)->huffval, val, sizeof((*htblptr)->huffval));

	/* Initialize sent_table FALSE so table will be written to JPEG file. */
	(*htblptr)->sent_table = FALSE;
}

/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */

static void std_huff_tables (j_decompress_ptr dinfo) {
	static const UINT8 bits_dc_luminance[17] =
	{ /* 0-base */
		0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 	};
	static const UINT8 val_dc_luminance[] =
	{ 
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 	};

	static const UINT8 bits_dc_chrominance[17] =
	{ /* 0-base */
		0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 	};
	static const UINT8 val_dc_chrominance[] =
	{ 
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 	};

	static const UINT8 bits_ac_luminance[17] =
	{ /* 0-base */
		0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d 	};
	static const UINT8 val_ac_luminance[] =
	{ 
		0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
		0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
		0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
		0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
		0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
		0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
		0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
		0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
		0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
		0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
		0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
		0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
		0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
		0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
		0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
		0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
		0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
		0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
		0xf9, 0xfa 	};
	static const UINT8 bits_ac_chrominance[17] =
	{ /* 0-base */
		0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 	};
	static const UINT8 val_ac_chrominance[] =
	{ 
		0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
		0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
		0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
		0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
		0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
		0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
		0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
		0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
		0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
		0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
		0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
		0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
		0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
		0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
		0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
		0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
		0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
		0xf9, 0xfa 	};

	add_huff_table(dinfo, &dinfo->dc_huff_tbl_ptrs[0],
		bits_dc_luminance, val_dc_luminance);
	add_huff_table(dinfo, &dinfo->ac_huff_tbl_ptrs[0],
		bits_ac_luminance, val_ac_luminance);
	add_huff_table(dinfo, &dinfo->dc_huff_tbl_ptrs[1],
		bits_dc_chrominance, val_dc_chrominance);
	add_huff_table(dinfo, &dinfo->ac_huff_tbl_ptrs[1],
		bits_ac_chrominance, val_ac_chrominance);
}

static int Decode_JPEG(unsigned char *inBuffer, unsigned char *outBuffer, unsigned int width, unsigned int height, int bufsize) {
	int rowstride;
	unsigned int y;
	struct jpeg_decompress_struct dinfo;
	struct jpeg_error_mgr jerr;

	numbytes= 0;

	dinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&dinfo);
	jpegmemsrcmgr_build(&dinfo, inBuffer, bufsize);
	jpeg_read_header(&dinfo, TRUE);
	if (dinfo.dc_huff_tbl_ptrs[0] == NULL){
		std_huff_tables(&dinfo);
	}
	dinfo.out_color_space = JCS_RGB;
	dinfo.dct_method = JDCT_IFAST;

	jpeg_start_decompress(&dinfo);

	rowstride= dinfo.output_width*dinfo.output_components;
	for (y= 0; y<dinfo.output_height; y++) {
		jpeg_read_scanlines(&dinfo, (JSAMPARRAY) &outBuffer, 1);
		outBuffer += rowstride;
	}
	jpeg_finish_decompress(&dinfo);

	if (dinfo.output_height >= height) return 0;
	
	inBuffer+= numbytes;
	jpegmemsrcmgr_build(&dinfo, inBuffer, bufsize-numbytes);

	numbytes= 0;
	jpeg_read_header(&dinfo, TRUE);
	if (dinfo.dc_huff_tbl_ptrs[0] == NULL){
		std_huff_tables(&dinfo);
	}

	jpeg_start_decompress(&dinfo);
	rowstride= dinfo.output_width*dinfo.output_components;
	for (y= 0; y<dinfo.output_height; y++){
		jpeg_read_scanlines(&dinfo, (JSAMPARRAY) &outBuffer, 1);
		outBuffer += rowstride;
	}
	jpeg_finish_decompress(&dinfo);
	jpeg_destroy_decompress(&dinfo);
	
	return 1;
}

static void Compress_JPEG(int quality, unsigned char *outbuffer, unsigned char *inBuffer, int width, int height, int bufsize) {
	int i, rowstride;
	unsigned int y;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	unsigned char marker[60];

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpegmemdestmgr_build(&cinfo, outbuffer, bufsize);

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	jpeg_set_colorspace (&cinfo, JCS_YCbCr);
		
	jpeg_set_quality (&cinfo, quality, TRUE);

	cinfo.dc_huff_tbl_ptrs[0]->sent_table = TRUE;
	cinfo.dc_huff_tbl_ptrs[1]->sent_table = TRUE;
	cinfo.ac_huff_tbl_ptrs[0]->sent_table = TRUE;
	cinfo.ac_huff_tbl_ptrs[1]->sent_table = TRUE;

	cinfo.comp_info[0].component_id = 0;
	cinfo.comp_info[0].v_samp_factor = 1;
	cinfo.comp_info[1].component_id = 1;
	cinfo.comp_info[2].component_id = 2;

	cinfo.write_JFIF_header = FALSE;

	jpeg_start_compress(&cinfo, FALSE);

	i=0;
	marker[i++] = 'A';
	marker[i++] = 'V';
	marker[i++] = 'I';
	marker[i++] = '1';
	marker[i++] = 0;
	while (i<60)
		marker[i++] = 32;

	jpeg_write_marker (&cinfo, JPEG_APP0, marker, 60);

	i=0;
	while (i<60)
		marker[i++] = 0;

	jpeg_write_marker (&cinfo, JPEG_COM, marker, 60);

	rowstride= cinfo.image_width*cinfo.input_components;
	for (y = 0; y < cinfo.image_height; y++){
		jpeg_write_scanlines(&cinfo, (JSAMPARRAY) &inBuffer, 1);
		inBuffer += rowstride;
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
}

static void interlace(unsigned char *to, unsigned char *from, int width, int height) {
	int i, rowstride= width*3;
	
	for (i=0; i<height; i++) {
		if (i&1)
			memcpy (&to[i*rowstride], &from[(i/2 + height/2)*rowstride], rowstride);
		else 
			memcpy (&to[i*rowstride], &from[(i/2)*rowstride], rowstride);
	}
}

static void deinterlace(int odd, unsigned char *to, unsigned char *from, int width, int height) {
	int i, rowstride= width*3;
	
	for (i=0; i<height; i++) {
		if ((i&1)==odd)
			memcpy (&to[(i/2 + height/2)*rowstride], &from[i*rowstride], rowstride);
		else 
			memcpy (&to[(i/2)*rowstride], &from[i*rowstride], rowstride);
	}
}

static int check_and_decode_jpeg(unsigned char *inbuf, unsigned char *outbuf, int width, int height, int bufsize) {
		/* JPEG's are always multiples of 16, extra is cropped out AVI's */	
	if ((width&0xF) || (height&0xF)) {
		int i, rrowstride, jrowstride;
		int jwidth= PADUP(width, 16);
		int jheight= PADUP(height, 16);
		unsigned char *tmpbuf= MEM_mallocN(jwidth*jheight*3, "avi.check_and_decode_jpeg");
		int ret= Decode_JPEG(inbuf, tmpbuf, jwidth, jheight, bufsize);
		
			/* crop the tmpbuf into the real buffer */
		rrowstride= width*3;
		jrowstride= jwidth*3;
		for (i=0; i<height; i++)
			memcpy(&outbuf[i*rrowstride], &tmpbuf[i*jrowstride], rrowstride);
		MEM_freeN(tmpbuf);
		
		return ret;
	} else {
		return Decode_JPEG(inbuf, outbuf, width, height, bufsize);
	}
}

static void check_and_compress_jpeg(int quality, unsigned char *outbuf, unsigned char *inbuf, int width, int height, int bufsize) {
		/* JPEG's are always multiples of 16, extra is ignored in AVI's */	
	if ((width&0xF) || (height&0xF)) {
		int i, rrowstride, jrowstride;
		int jwidth= PADUP(width, 16);
		int jheight= PADUP(height, 16);
		unsigned char *tmpbuf= MEM_mallocN(jwidth*jheight*3, "avi.check_and_compress_jpeg");
		
			/* resize the realbuf into the tmpbuf */
		rrowstride= width*3;
		jrowstride= jwidth*3;
		for (i=0; i<jheight; i++) {
			if (i<height)
				memcpy(&tmpbuf[i*jrowstride], &inbuf[i*rrowstride], rrowstride);
			else
				memset(&tmpbuf[i*jrowstride], 0, rrowstride);
			memset(&tmpbuf[i*jrowstride+rrowstride], 0, jrowstride-rrowstride);
		}

		Compress_JPEG(quality, outbuf, tmpbuf, jwidth, jheight, bufsize);

		MEM_freeN(tmpbuf);
	} else {
		Compress_JPEG(quality, outbuf, inbuf, width, height, bufsize);
	}
}

void *avi_converter_from_mjpeg (AviMovie *movie, int stream, unsigned char *buffer, int *size) {
	int deint;
	unsigned char *buf;
		
	buf= MEM_mallocN (movie->header->Height * movie->header->Width * 3, "avi.avi_converter_from_mjpeg 1");

	deint= check_and_decode_jpeg(buffer, buf, movie->header->Width, movie->header->Height, *size);
	
	MEM_freeN(buffer);
	
	if (deint) {
		buffer = MEM_mallocN (movie->header->Height * movie->header->Width * 3, "avi.avi_converter_from_mjpeg 2");
		interlace (buffer, buf, movie->header->Width, movie->header->Height);
		MEM_freeN (buf);
	
		buf= buffer;
	}
		
	return buf;
}

void *avi_converter_to_mjpeg (AviMovie *movie, int stream, unsigned char *buffer, int *size) {
	unsigned char *buf;
	int bufsize= *size;
	
	numbytes = 0;
	*size= 0;

	buf = MEM_mallocN (movie->header->Height * movie->header->Width * 3, "avi.avi_converter_to_mjpeg 1");	
	if (!movie->interlace) {
		check_and_compress_jpeg(movie->streams[stream].sh.Quality/100, buf, buffer,  movie->header->Width, movie->header->Height, bufsize);
	} else {
		deinterlace (movie->odd_fields, buf, buffer, movie->header->Width, movie->header->Height);
		MEM_freeN (buffer);
	
		buffer= buf;
		buf= MEM_mallocN (movie->header->Height * movie->header->Width * 3, "avi.avi_converter_to_mjpeg 2");
	
		check_and_compress_jpeg(movie->streams[stream].sh.Quality/100, buf, buffer,  movie->header->Width, movie->header->Height/2, bufsize/2);
		*size+= numbytes;
		numbytes=0;
		check_and_compress_jpeg(movie->streams[stream].sh.Quality/100, buf+*size, buffer+(movie->header->Height/2)*movie->header->Width*3,  movie->header->Width, movie->header->Height/2, bufsize/2);
	}
	*size += numbytes;	

	MEM_freeN (buffer);
	return buf;
}


/* Compression from memory */

static void jpegmemdestmgr_init_destination(j_compress_ptr cinfo) {
	;
}

static boolean jpegmemdestmgr_empty_output_buffer(j_compress_ptr cinfo) {
	return TRUE;
}

static void jpegmemdestmgr_term_destination(j_compress_ptr cinfo) {
	numbytes-= cinfo->dest->free_in_buffer;

	MEM_freeN(cinfo->dest);
}

static void jpegmemdestmgr_build(j_compress_ptr cinfo, unsigned char *buffer, int bufsize) {
	cinfo->dest= MEM_mallocN(sizeof(*(cinfo->dest)), "avi.jpegmemdestmgr_build");
	
	cinfo->dest->init_destination= jpegmemdestmgr_init_destination;
	cinfo->dest->empty_output_buffer= jpegmemdestmgr_empty_output_buffer;
	cinfo->dest->term_destination= jpegmemdestmgr_term_destination;

	cinfo->dest->next_output_byte= buffer;
	cinfo->dest->free_in_buffer= bufsize;
	
	numbytes= bufsize;
}

/* Decompression from memory */

static void jpegmemsrcmgr_init_source(j_decompress_ptr dinfo) {
	;
}

static boolean jpegmemsrcmgr_fill_input_buffer(j_decompress_ptr dinfo) {
	unsigned char *buf= (unsigned char*) dinfo->src->next_input_byte-2;
	
		/* if we get called, must have run out of data */
	WARNMS(dinfo, JWRN_JPEG_EOF);
	
	buf[0]= (JOCTET) 0xFF;
	buf[1]= (JOCTET) JPEG_EOI;
	
	dinfo->src->next_input_byte= buf;
	dinfo->src->bytes_in_buffer= 2;
	
	return TRUE;
}

static void jpegmemsrcmgr_skip_input_data(j_decompress_ptr dinfo, long skipcnt) {
	if (dinfo->src->bytes_in_buffer<skipcnt)
		skipcnt= dinfo->src->bytes_in_buffer;

	dinfo->src->next_input_byte+= skipcnt;
	dinfo->src->bytes_in_buffer-= skipcnt;
}

static void jpegmemsrcmgr_term_source(j_decompress_ptr dinfo) {
	numbytes-= dinfo->src->bytes_in_buffer;
	
	MEM_freeN(dinfo->src);
}

static void jpegmemsrcmgr_build(j_decompress_ptr dinfo, unsigned char *buffer, int bufsize) {
	dinfo->src= MEM_mallocN(sizeof(*(dinfo->src)), "avi.jpegmemsrcmgr_build");
	
	dinfo->src->init_source= jpegmemsrcmgr_init_source;
	dinfo->src->fill_input_buffer= jpegmemsrcmgr_fill_input_buffer;
	dinfo->src->skip_input_data= jpegmemsrcmgr_skip_input_data;
	dinfo->src->resync_to_restart= jpeg_resync_to_restart;
	dinfo->src->term_source= jpegmemsrcmgr_term_source;
	
	dinfo->src->bytes_in_buffer= bufsize;
	dinfo->src->next_input_byte= buffer;

	numbytes= bufsize;
}
