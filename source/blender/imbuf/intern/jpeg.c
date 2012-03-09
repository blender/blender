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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/jpeg.c
 *  \ingroup imbuf
 */



/* This little block needed for linking to Blender... */
#include <stdio.h>
#include <setjmp.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_metadata.h"
#include "IMB_filetype.h"
#include "jpeglib.h" 
#include "jerror.h"

#define IS_jpg(x)		(x->ftype & JPG)
#define IS_stdjpg(x)	((x->ftype & JPG_MSK) == JPG_STD)
#define IS_vidjpg(x)	((x->ftype & JPG_MSK) == JPG_VID)
#define IS_jstjpg(x)	((x->ftype & JPG_MSK) == JPG_JST)
#define IS_maxjpg(x)	((x->ftype & JPG_MSK) == JPG_MAX)

/* the types are from the jpeg lib */
static void jpeg_error (j_common_ptr cinfo);
static void init_source(j_decompress_ptr cinfo);
static boolean fill_input_buffer(j_decompress_ptr cinfo);
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes);
static void term_source(j_decompress_ptr cinfo);
static void memory_source(j_decompress_ptr cinfo, unsigned char *buffer, size_t size);
static boolean handle_app1 (j_decompress_ptr cinfo);
static ImBuf * ibJpegImageFromCinfo(struct jpeg_decompress_struct * cinfo, int flags);


/*
 * In principle there are 4 jpeg formats.
 *
 * 1. jpeg - standard printing, u & v at quarter of resulution
 * 2. jvid - standaard video, u & v half resolution, frame not interlaced
 *
 * type 3 is unsupported as of jul 05 2000 Frank.
 *
 * 3. jstr - as 2, but written in 2 separate fields
 *
 * 4. jmax - no scaling in the components
 */

static int jpeg_default_quality;
static int ibuf_ftype;

int imb_is_a_jpeg(unsigned char *mem)
{
	if ((mem[0]== 0xFF) && (mem[1] == 0xD8))return 1;
	return 0;
}

//----------------------------------------------------------
//	JPG ERROR HANDLING
//----------------------------------------------------------

typedef struct my_error_mgr {
	struct jpeg_error_mgr pub;	/* "public" fields */

	jmp_buf setjmp_buffer;	/* for return to caller */
} my_error_mgr;

typedef my_error_mgr * my_error_ptr;

static void jpeg_error (j_common_ptr cinfo)
{
	my_error_ptr err = (my_error_ptr)cinfo->err;

	/* Always display the message */
	(*cinfo->err->output_message) (cinfo);

	/* Let the memory manager delete any temp files before we die */
	jpeg_destroy(cinfo);

	/* return control to the setjmp point */
	longjmp(err->setjmp_buffer, 1);
}

//----------------------------------------------------------
//	INPUT HANDLER FROM MEMORY
//----------------------------------------------------------

typedef struct {
	unsigned char	*buffer;
	int		filled;
} buffer_struct;

typedef struct {
	struct jpeg_source_mgr pub;	/* public fields */

	unsigned char	*buffer;
	int				size;
	JOCTET			terminal[2];
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

static void init_source(j_decompress_ptr cinfo)
{
	(void)cinfo; /* unused */
}


static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	/* Since we have given all we have got already
	 * we simply fake an end of file
	 */

	src->pub.next_input_byte = src->terminal;
	src->pub.bytes_in_buffer = 2;
	src->terminal[0] = (JOCTET) 0xFF;
	src->terminal[1] = (JOCTET) JPEG_EOI;

	return TRUE;
}


static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	if(num_bytes > 0) {
		// prevent skipping over file end
		size_t skip_size = (size_t)num_bytes <= src->pub.bytes_in_buffer ? num_bytes : src->pub.bytes_in_buffer;

		src->pub.next_input_byte = src->pub.next_input_byte + skip_size;
		src->pub.bytes_in_buffer = src->pub.bytes_in_buffer - skip_size;
	}
}


static void term_source(j_decompress_ptr cinfo)
{
	(void)cinfo; /* unused */
}

static void memory_source(j_decompress_ptr cinfo, unsigned char *buffer, size_t size)
{
	my_src_ptr src;

	if (cinfo->src == NULL) { /* first time for this JPEG object? */
		cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)
			((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(my_source_mgr));
	}

	src = (my_src_ptr) cinfo->src;
	src->pub.init_source		= init_source;
	src->pub.fill_input_buffer	= fill_input_buffer;
	src->pub.skip_input_data	= skip_input_data;
	src->pub.resync_to_restart	= jpeg_resync_to_restart;
	src->pub.term_source		= term_source;

	src->pub.bytes_in_buffer	= size;
	src->pub.next_input_byte	= buffer;

	src->buffer = buffer;
	src->size = size;
}


#define MAKESTMT(stuff)		do { stuff } while (0)

#define INPUT_VARS(cinfo)  \
	struct jpeg_source_mgr * datasrc = (cinfo)->src;  \
	const JOCTET * next_input_byte = datasrc->next_input_byte;  \
	size_t bytes_in_buffer = datasrc->bytes_in_buffer

/* Unload the local copies --- do this only at a restart boundary */
#define INPUT_SYNC(cinfo)  \
	( datasrc->next_input_byte = next_input_byte,  \
	  datasrc->bytes_in_buffer = bytes_in_buffer )

/* Reload the local copies --- seldom used except in MAKE_BYTE_AVAIL */
#define INPUT_RELOAD(cinfo)  \
	( next_input_byte = datasrc->next_input_byte,  \
	  bytes_in_buffer = datasrc->bytes_in_buffer )

/* Internal macro for INPUT_BYTE and INPUT_2BYTES: make a byte available.
 * Note we do *not* do INPUT_SYNC before calling fill_input_buffer,
 * but we must reload the local copies after a successful fill.
 */
#define MAKE_BYTE_AVAIL(cinfo, action)                                        \
	if (bytes_in_buffer == 0) {                                               \
		if (! (*datasrc->fill_input_buffer) (cinfo))                          \
			{ action; }                                                       \
		INPUT_RELOAD(cinfo);  \
	}


/* Read a byte into variable V.
 * If must suspend, take the specified action (typically "return FALSE").
 */
#define INPUT_BYTE(cinfo,V,action)  \
	MAKESTMT( MAKE_BYTE_AVAIL(cinfo,action); \
		  bytes_in_buffer--; \
		  V = GETJOCTET(*next_input_byte++); )

/* As above, but read two bytes interpreted as an unsigned 16-bit integer.
 * V should be declared unsigned int or perhaps INT32.
 */
#define INPUT_2BYTES(cinfo,V,action)  \
	MAKESTMT( MAKE_BYTE_AVAIL(cinfo,action); \
		  bytes_in_buffer--; \
		  V = ((unsigned int) GETJOCTET(*next_input_byte++)) << 8; \
		  MAKE_BYTE_AVAIL(cinfo,action); \
		  bytes_in_buffer--; \
		  V += GETJOCTET(*next_input_byte++); )


static boolean
handle_app1 (j_decompress_ptr cinfo)
{
	INT32 length; /* initialized by the macro */
	INT32 i;
	char neogeo[128];
	
	INPUT_VARS(cinfo);

	INPUT_2BYTES(cinfo, length, return FALSE);
	length -= 2;
	
	if (length < 16) {
		for (i = 0; i < length; i++) INPUT_BYTE(cinfo, neogeo[i], return FALSE);
		length = 0;
		if (strncmp(neogeo, "NeoGeo", 6) == 0) memcpy(&ibuf_ftype, neogeo + 6, 4);
		ibuf_ftype = BIG_LONG(ibuf_ftype);
	}
	INPUT_SYNC(cinfo);		/* do before skip_input_data */
	if (length > 0) (*cinfo->src->skip_input_data) (cinfo, length);
	return TRUE;
}


static ImBuf * ibJpegImageFromCinfo(struct jpeg_decompress_struct * cinfo, int flags)
{
	JSAMPARRAY row_pointer;
	JSAMPLE * buffer = NULL;
	int row_stride;
	int x, y, depth, r, g, b, k;
	struct ImBuf * ibuf = NULL;
	uchar * rect;
	jpeg_saved_marker_ptr marker;
	char *str, *key, *value;

	/* install own app1 handler */
	ibuf_ftype = 0;
	jpeg_set_marker_processor(cinfo, 0xe1, handle_app1);
	cinfo->dct_method = JDCT_FLOAT;
	jpeg_save_markers(cinfo, JPEG_COM, 0xffff);

	if (jpeg_read_header(cinfo, FALSE) == JPEG_HEADER_OK) {
		x = cinfo->image_width;
		y = cinfo->image_height;
		depth = cinfo->num_components;

		if (cinfo->jpeg_color_space == JCS_YCCK) cinfo->out_color_space = JCS_CMYK;

		jpeg_start_decompress(cinfo);

		if (ibuf_ftype == 0) {
			ibuf_ftype = JPG_STD;
			if (cinfo->max_v_samp_factor == 1) {
				if (cinfo->max_h_samp_factor == 1) ibuf_ftype = JPG_MAX;
				else ibuf_ftype = JPG_VID;
			}
		}

		if (flags & IB_test) {
			jpeg_abort_decompress(cinfo);
			ibuf = IMB_allocImBuf(x, y, 8 * depth, 0);
		}
		else if ((ibuf = IMB_allocImBuf(x, y, 8 * depth, IB_rect)) == NULL) {
			jpeg_abort_decompress(cinfo);
		}
		else {
			row_stride = cinfo->output_width * depth;

			row_pointer = (*cinfo->mem->alloc_sarray) ((j_common_ptr) cinfo, JPOOL_IMAGE, row_stride, 1);
			
			for (y = ibuf->y - 1; y >= 0; y--) {
				jpeg_read_scanlines(cinfo, row_pointer, 1);
				rect = (uchar *) (ibuf->rect + y * ibuf->x);
				buffer = row_pointer[0];
				
				switch(depth) {
					case 1:
						for (x=ibuf->x; x >0; x--) {
							rect[3] = 255;
							rect[0] = rect[1] = rect[2] = *buffer++;
							rect += 4;
						}
						break;
					case 3:
						for (x=ibuf->x; x >0; x--) {
							rect[3] = 255;
							rect[0] = *buffer++;
							rect[1] = *buffer++;
							rect[2] = *buffer++;
							rect += 4;
						}
						break;
					case 4:
						for (x=ibuf->x; x >0; x--) {
							r = *buffer++;
							g = *buffer++;
							b = *buffer++;
							k = *buffer++;
							
							k = 255 - k;
							r -= k;
							if (r & 0xffffff00) {
								if (r < 0) r = 0;
								else r = 255;
							}
							g -= k;
							if (g & 0xffffff00) {
								if (g < 0) g = 0;
								else g = 255;
							}
							b -= k;
							if (b & 0xffffff00) {
								if (b < 0) b = 0;
								else b = 255;
							}
							
							rect[3] = 255 - k;
							rect[2] = b;
							rect[1] = g;
							rect[0] = r;
							rect += 4;
						}
				}
			}

			marker= cinfo->marker_list;
			while(marker) {
				if(marker->marker != JPEG_COM)
					goto next_stamp_marker;

				/*
				 * Because JPEG format don't support the
				 * pair "key/value" like PNG, we store the
				 * stampinfo in a single "encode" string:
				 *	"Blender:key:value"
				 *
				 * That is why we need split it to the
				 * common key/value here.
				 */
				if(strncmp((char *) marker->data, "Blender", 7)) {
					/*
					 * Maybe the file have text that
					 * we don't know "what it's", in that
					 * case we keep the text (with a
					 * key "None").
					 * This is only for don't "lose"
					 * the information when we write
					 * it back to disk.
					 */
					IMB_metadata_add_field(ibuf, "None", (char *) marker->data);
					ibuf->flags |= IB_metadata;
					goto next_stamp_marker;
				}

				str = BLI_strdup ((char *) marker->data);
				key = strchr (str, ':');
				/*
				 * A little paranoid, but the file maybe
				 * is broken... and a "extra" check is better
				 * then segfault ;)
				 */
				if (!key) {
					MEM_freeN(str);
					goto next_stamp_marker;
				}

				key++;
				value = strchr (key, ':');
				if (!value) {
					MEM_freeN(str);
					goto next_stamp_marker;
				}

				*value = '\0'; /* need finish the key string */
				value++;
				IMB_metadata_add_field(ibuf, key, value);
				ibuf->flags |= IB_metadata;
				MEM_freeN(str);
next_stamp_marker:
				marker= marker->next;
			}

			jpeg_finish_decompress(cinfo);
		}
		
		jpeg_destroy((j_common_ptr) cinfo);
		if(ibuf) {
			ibuf->ftype = ibuf_ftype;
			ibuf->profile = IB_PROFILE_SRGB;
		}
	}

	return(ibuf);
}

ImBuf * imb_load_jpeg (unsigned char * buffer, size_t size, int flags)
{
	struct jpeg_decompress_struct _cinfo, *cinfo = &_cinfo;
	struct my_error_mgr jerr;
	ImBuf * ibuf;

	if(!imb_is_a_jpeg(buffer)) return NULL;
	
	cinfo->err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error;

	/* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer)) {
		/* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		 */
		jpeg_destroy_decompress(cinfo);
		return NULL;
	}

	jpeg_create_decompress(cinfo);
	memory_source(cinfo, buffer, size);

	ibuf = ibJpegImageFromCinfo(cinfo, flags);
	
	return(ibuf);
}


static void write_jpeg(struct jpeg_compress_struct * cinfo, struct ImBuf * ibuf)
{
	JSAMPLE * buffer = NULL;
	JSAMPROW row_pointer[1];
	uchar * rect;
	int x, y;
	char neogeo[128];
	ImMetaData *iptr;
	char *text;

	jpeg_start_compress(cinfo, TRUE);

	strcpy(neogeo, "NeoGeo");
	ibuf_ftype = BIG_LONG(ibuf->ftype);
	
	memcpy(neogeo + 6, &ibuf_ftype, 4);
	jpeg_write_marker(cinfo, 0xe1, (JOCTET*) neogeo, 10);

	if(ibuf->metadata) {
		/* key + max value + "Blender" */
		text= MEM_mallocN(530, "stamp info read");
		iptr= ibuf->metadata;
		while(iptr) {
			if (!strcmp (iptr->key, "None")) {
				jpeg_write_marker(cinfo, JPEG_COM, (JOCTET *) iptr->value, strlen (iptr->value) + 1);
				goto next_stamp_info;
			}

			/*
			 * The JPEG format don't support a pair "key/value"
			 * like PNG, so we "encode" the stamp in a
			 * single string:
			 *	"Blender:key:value"
			 *
			 * The first "Blender" is a simple identify to help
			 * in the read process.
			 */
			sprintf (text, "Blender:%s:%s", iptr->key, iptr->value);
			jpeg_write_marker(cinfo, JPEG_COM, (JOCTET *) text, strlen (text)+1);
next_stamp_info:
			iptr = iptr->next;
		}
		MEM_freeN(text);
	}

	row_pointer[0] =
		MEM_mallocN(sizeof(JSAMPLE) *
					 cinfo->input_components *
					 cinfo->image_width, "jpeg row_pointer");

	for(y = ibuf->y - 1; y >= 0; y--){
		rect = (uchar *) (ibuf->rect + y * ibuf->x);
		buffer = row_pointer[0];

		switch(cinfo->in_color_space){
			case JCS_RGB:
				for (x = 0; x < ibuf->x; x++) {
					*buffer++ = rect[0];
					*buffer++ = rect[1];
					*buffer++ = rect[2];
					rect += 4;
				}
				break;
			case JCS_GRAYSCALE:
				for (x = 0; x < ibuf->x; x++) {
					*buffer++ = rect[0];
					rect += 4;
				}
				break;
			case JCS_UNKNOWN:
				memcpy(buffer, rect, 4 * ibuf->x);
				break;
				/* default was missing... intentional ? */
			default:
				; /* do nothing */
		}

		jpeg_write_scanlines(cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(cinfo);
	MEM_freeN(row_pointer[0]);
}


static int init_jpeg(FILE * outfile, struct jpeg_compress_struct * cinfo, struct ImBuf *ibuf)
{
	int quality;

	quality = ibuf->ftype & 0xff;
	if (quality <= 0) quality = jpeg_default_quality;
	if (quality > 100) quality = 100;

	jpeg_create_compress(cinfo);
	jpeg_stdio_dest(cinfo, outfile);

	cinfo->image_width = ibuf->x;
	cinfo->image_height = ibuf->y;

	cinfo->in_color_space = JCS_RGB;
	if (ibuf->planes == 8) cinfo->in_color_space = JCS_GRAYSCALE;
#if 0
	/* just write RGBA as RGB,
	 * unsupported feature only confuses other s/w */

	if (ibuf->planes == 32) cinfo->in_color_space = JCS_UNKNOWN;
#endif
	switch(cinfo->in_color_space){
		case JCS_RGB:
			cinfo->input_components = 3;
			break;
		case JCS_GRAYSCALE:
			cinfo->input_components = 1;
			break;
		case JCS_UNKNOWN:
			cinfo->input_components = 4;
			break;
			/* default was missing... intentional ? */
		default:
			; /* do nothing */
	}
	jpeg_set_defaults(cinfo);
	
	/* own settings */

	cinfo->dct_method = JDCT_FLOAT;
	jpeg_set_quality(cinfo, quality, TRUE);

	return(0);
}


static int save_stdjpeg(const char *name, struct ImBuf *ibuf)
{
	FILE * outfile;
	struct jpeg_compress_struct _cinfo, *cinfo = &_cinfo;
	struct my_error_mgr jerr;

	if ((outfile = fopen(name, "wb")) == NULL) return 0;
	jpeg_default_quality = 75;

	cinfo->err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error;

	/* Establish the setjmp return context for jpeg_error to use. */
	if (setjmp(jerr.setjmp_buffer)) {
		/* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		 */
		jpeg_destroy_compress(cinfo);
		fclose(outfile);
		remove(name);
		return 0;
	}

	init_jpeg(outfile, cinfo, ibuf);

	write_jpeg(cinfo, ibuf);

	fclose(outfile);
	jpeg_destroy_compress(cinfo);

	return 1;
}


static int save_vidjpeg(const char *name, struct ImBuf *ibuf)
{
	FILE * outfile;
	struct jpeg_compress_struct _cinfo, *cinfo = &_cinfo;
	struct my_error_mgr jerr;

	if ((outfile = fopen(name, "wb")) == NULL) return 0;
	jpeg_default_quality = 90;

	cinfo->err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error;

	/* Establish the setjmp return context for jpeg_error to use. */
	if (setjmp(jerr.setjmp_buffer)) {
		/* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		 */
		jpeg_destroy_compress(cinfo);
		fclose(outfile);
		remove(name);
		return 0;
	}

	init_jpeg(outfile, cinfo, ibuf);

	/* adjust scaling factors */
	if (cinfo->in_color_space == JCS_RGB) {
		cinfo->comp_info[0].h_samp_factor = 2;
		cinfo->comp_info[0].v_samp_factor = 1;
	}

	write_jpeg(cinfo, ibuf);

	fclose(outfile);
	jpeg_destroy_compress(cinfo);

	return 1;
}

static int save_jstjpeg(const char *name, struct ImBuf *ibuf)
{
	char fieldname[1024];
	struct ImBuf * tbuf;
	int oldy, returnval;

	tbuf = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 24, IB_rect);
	tbuf->ftype = ibuf->ftype;
	tbuf->flags = ibuf->flags;
	
	oldy = ibuf->y;
	ibuf->x *= 2;
	ibuf->y /= 2;

	IMB_rectcpy(tbuf, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
	sprintf(fieldname, "%s.jf0", name);

	returnval = save_vidjpeg(fieldname, tbuf);
	if (returnval == 1) {
		IMB_rectcpy(tbuf, ibuf, 0, 0, tbuf->x, 0, ibuf->x, ibuf->y);
		sprintf(fieldname, "%s.jf1", name);
		returnval = save_vidjpeg(fieldname, tbuf);
	}

	ibuf->y = oldy;
	ibuf->x /= 2;
	IMB_freeImBuf(tbuf);

	return returnval;
}

static int save_maxjpeg(const char *name, struct ImBuf *ibuf)
{
	FILE * outfile;
	struct jpeg_compress_struct _cinfo, *cinfo = &_cinfo;
	struct my_error_mgr jerr;

	if ((outfile = fopen(name, "wb")) == NULL) return 0;
	jpeg_default_quality = 100;

	cinfo->err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error;

	/* Establish the setjmp return context for jpeg_error to use. */
	if (setjmp(jerr.setjmp_buffer)) {
		/* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		 */
		jpeg_destroy_compress(cinfo);
		fclose(outfile);
		remove(name);
		return 0;
	}

	init_jpeg(outfile, cinfo, ibuf);

	/* adjust scaling factors */
	if (cinfo->in_color_space == JCS_RGB) {
		cinfo->comp_info[0].h_samp_factor = 1;
		cinfo->comp_info[0].v_samp_factor = 1;
	}

	write_jpeg(cinfo, ibuf);

	fclose(outfile);
	jpeg_destroy_compress(cinfo);

	return 1;
}

int imb_savejpeg(struct ImBuf *ibuf, const char *name, int flags)
{
	
	ibuf->flags = flags;
	if (IS_stdjpg(ibuf)) return save_stdjpeg(name, ibuf);
	if (IS_jstjpg(ibuf)) return save_jstjpeg(name, ibuf);
	if (IS_maxjpg(ibuf)) return save_maxjpeg(name, ibuf);
	return save_vidjpeg(name, ibuf);
}

