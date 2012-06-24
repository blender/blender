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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/jp2.c
 *  \ingroup imbuf
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "imbuf.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"
#include "IMB_filetype.h"

#include "openjpeg.h"

#define JP2_FILEHEADER_SIZE 14

static char JP2_HEAD[] = {0x0, 0x0, 0x0, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A};

/* We only need this because of how the presets are set */
/* this typedef is copied from 'openjpeg-1.5.0/applications/codec/image_to_j2k.c' */
typedef struct img_folder {
	/** The directory path of the folder containing input images*/
	char *imgdirpath;
	/** Output format*/
	char *out_format;
	/** Enable option*/
	char set_imgdir;
	/** Enable Cod Format for output*/
	char set_out_format;
	/** User specified rate stored in case of cinema option*/
	float *rates;
} img_fol_t;

static int check_jp2(unsigned char *mem) /* J2K_CFMT */
{
	return memcmp(JP2_HEAD, mem, 12) ? 0 : 1;
}

int imb_is_a_jp2(unsigned char *buf)
{	
	return check_jp2(buf);
}


/**
 * sample error callback expecting a FILE* client object
 */
static void error_callback(const char *msg, void *client_data)
{
	FILE *stream = (FILE *)client_data;
	fprintf(stream, "[ERROR] %s", msg);
}
/**
 * sample warning callback expecting a FILE* client object
 */
static void warning_callback(const char *msg, void *client_data)
{
	FILE *stream = (FILE *)client_data;
	fprintf(stream, "[WARNING] %s", msg);
}
/**
 * sample debug callback expecting no client object
 */
static void info_callback(const char *msg, void *client_data)
{
	(void)client_data;
	fprintf(stdout, "[INFO] %s", msg);
}

#   define PIXEL_LOOPER_BEGIN(_rect)                                          \
	for (y = h - 1; y != (unsigned int)(-1); y--) {                           \
		for (i = y * w, i_next = (y + 1) * w;                                 \
		     i < i_next;                                                      \
		     i++, _rect += 4)                                                 \
		{                                                                     \

#   define PIXEL_LOOPER_END \
	} \
	} (void)0 \

struct ImBuf *imb_jp2_decode(unsigned char *mem, size_t size, int flags)
{
	struct ImBuf *ibuf = NULL;
	int use_float = FALSE; /* for precision higher then 8 use float */
	int use_alpha = FALSE;
	
	long signed_offsets[4] = {0, 0, 0, 0};
	int float_divs[4] = {1, 1, 1, 1};

	unsigned int i, i_next, w, h, planes;
	unsigned int y;
	int *r, *g, *b, *a; /* matching 'opj_image_comp.data' type */
	
	opj_dparameters_t parameters;   /* decompression parameters */
	
	opj_event_mgr_t event_mgr;      /* event manager */
	opj_image_t *image = NULL;

	opj_dinfo_t *dinfo = NULL;  /* handle to a decompressor */
	opj_cio_t *cio = NULL;

	if (check_jp2(mem) == 0) return(NULL);

	/* configure the event callbacks (not required) */
	memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
	event_mgr.error_handler = error_callback;
	event_mgr.warning_handler = warning_callback;
	event_mgr.info_handler = info_callback;


	/* set decoding parameters to default values */
	opj_set_default_decoder_parameters(&parameters);


	/* JPEG 2000 compressed image data */

	/* get a decoder handle */
	dinfo = opj_create_decompress(CODEC_JP2);

	/* catch events using our callbacks and give a local context */
	opj_set_event_mgr((opj_common_ptr)dinfo, &event_mgr, stderr);

	/* setup the decoder decoding parameters using the current image and user parameters */
	opj_setup_decoder(dinfo, &parameters);

	/* open a byte stream */
	cio = opj_cio_open((opj_common_ptr)dinfo, mem, size);

	/* decode the stream and fill the image structure */
	image = opj_decode(dinfo, cio);
	
	if (!image) {
		fprintf(stderr, "ERROR -> j2k_to_image: failed to decode image!\n");
		opj_destroy_decompress(dinfo);
		opj_cio_close(cio);
		return NULL;
	}

	/* close the byte stream */
	opj_cio_close(cio);


	if ((image->numcomps * image->x1 * image->y1) == 0) {
		fprintf(stderr, "\nError: invalid raw image parameters\n");
		return NULL;
	}
	
	w = image->comps[0].w;
	h = image->comps[0].h;
	
	switch (image->numcomps) {
		case 1: /* Greyscale */
		case 3: /* Color */
			planes = 24;
			use_alpha = FALSE;
			break;
		default: /* 2 or 4 - Greyscale or Color + alpha */
			planes = 32; /* greyscale + alpha */
			use_alpha = TRUE;
			break;
	}
	
	
	i = image->numcomps;
	if (i > 4) i = 4;
	
	while (i) {
		i--;
		
		if (image->comps[i].prec > 8)
			use_float = TRUE;
		
		if (image->comps[i].sgnd)
			signed_offsets[i] =  1 << (image->comps[i].prec - 1);
		
		/* only needed for float images but dosnt hurt to calc this */
		float_divs[i] = (1 << image->comps[i].prec) - 1;
	}
	
	ibuf = IMB_allocImBuf(w, h, planes, use_float ? IB_rectfloat : IB_rect);
	
	if (ibuf == NULL) {
		if (dinfo)
			opj_destroy_decompress(dinfo);
		return NULL;
	}
	
	ibuf->ftype = JP2;
	
	if (use_float) {
		float *rect_float = ibuf->rect_float;

		if (image->numcomps < 3) {
			r = image->comps[0].data;
			a = (use_alpha) ? image->comps[1].data : NULL;

			/* greyscale 12bits+ */
			if (use_alpha) {
				a = image->comps[1].data;
				PIXEL_LOOPER_BEGIN(rect_float) {
					rect_float[0] = rect_float[1] = rect_float[2] = (float)(r[i] + signed_offsets[0]) / float_divs[0];
					rect_float[3] = (a[i] + signed_offsets[1]) / float_divs[1];
				}
				PIXEL_LOOPER_END;
			}
			else {
				PIXEL_LOOPER_BEGIN(rect_float) {
					rect_float[0] = rect_float[1] = rect_float[2] = (float)(r[i] + signed_offsets[0]) / float_divs[0];
					rect_float[3] = 1.0f;
				}
				PIXEL_LOOPER_END;
			}
		}
		else {
			r = image->comps[0].data;
			g = image->comps[1].data;
			b = image->comps[2].data;

			/* rgb or rgba 12bits+ */
			if (use_alpha) {
				a = image->comps[3].data;
				PIXEL_LOOPER_BEGIN(rect_float) {
					rect_float[0] = (float)(r[i] + signed_offsets[0]) / float_divs[0];
					rect_float[1] = (float)(g[i] + signed_offsets[1]) / float_divs[1];
					rect_float[2] = (float)(b[i] + signed_offsets[2]) / float_divs[2];
					rect_float[3] = (float)(a[i] + signed_offsets[3]) / float_divs[3];
				}
				PIXEL_LOOPER_END;
			}
			else {
				PIXEL_LOOPER_BEGIN(rect_float) {
					rect_float[0] = (float)(r[i] + signed_offsets[0]) / float_divs[0];
					rect_float[1] = (float)(g[i] + signed_offsets[1]) / float_divs[1];
					rect_float[2] = (float)(b[i] + signed_offsets[2]) / float_divs[2];
					rect_float[3] = 1.0f;
				}
				PIXEL_LOOPER_END;
			}
		}
		
	}
	else {
		unsigned char *rect_uchar = (unsigned char *)ibuf->rect;

		if (image->numcomps < 3) {
			r = image->comps[0].data;
			a = (use_alpha) ? image->comps[1].data : NULL;

			/* greyscale */
			if (use_alpha) {
				a = image->comps[3].data;
				PIXEL_LOOPER_BEGIN(rect_uchar) {
					rect_uchar[0] = rect_uchar[1] = rect_uchar[2] = (r[i] + signed_offsets[0]);
					rect_uchar[3] = a[i] + signed_offsets[1];
				}
				PIXEL_LOOPER_END;
			}
			else {
				PIXEL_LOOPER_BEGIN(rect_uchar) {
					rect_uchar[0] = rect_uchar[1] = rect_uchar[2] = (r[i] + signed_offsets[0]);
					rect_uchar[3] = 255;
				}
				PIXEL_LOOPER_END;
			}
		}
		else {
			r = image->comps[0].data;
			g = image->comps[1].data;
			b = image->comps[2].data;

			/* 8bit rgb or rgba */
			if (use_alpha) {
				a = image->comps[3].data;
				PIXEL_LOOPER_BEGIN(rect_uchar) {
					rect_uchar[0] = r[i] + signed_offsets[0];
					rect_uchar[1] = g[i] + signed_offsets[1];
					rect_uchar[2] = b[i] + signed_offsets[2];
					rect_uchar[3] = a[i] + signed_offsets[3];
				}
				PIXEL_LOOPER_END;
			}
			else {
				PIXEL_LOOPER_BEGIN(rect_uchar) {
					rect_uchar[0] = r[i] + signed_offsets[0];
					rect_uchar[1] = g[i] + signed_offsets[1];
					rect_uchar[2] = b[i] + signed_offsets[2];
					rect_uchar[3] = 255;
				}
				PIXEL_LOOPER_END;
			}
		}
	}
	
	/* free remaining structures */
	if (dinfo) {
		opj_destroy_decompress(dinfo);
	}
	
	/* free image data structure */
	opj_image_destroy(image);
	
	if (flags & IB_rect) {
		IMB_rect_from_float(ibuf);
	}
	
	return(ibuf);
}

//static opj_image_t* rawtoimage(const char *filename, opj_cparameters_t *parameters, raw_cparameters_t *raw_cp)
/* prec can be 8, 12, 16 */

/* use inline because the float passed can be a function call that would end up being called many times */
#if 0
#define UPSAMPLE_8_TO_12(_val) ((_val << 4) | (_val & ((1 << 4) - 1)))
#define UPSAMPLE_8_TO_16(_val) ((_val << 8) + _val)

#define DOWNSAMPLE_FLOAT_TO_8BIT(_val)  (_val) <= 0.0f ? 0 : ((_val) >= 1.0f ? 255 : (int)(255.0f * (_val)))
#define DOWNSAMPLE_FLOAT_TO_12BIT(_val) (_val) <= 0.0f ? 0 : ((_val) >= 1.0f ? 4095 : (int)(4095.0f * (_val)))
#define DOWNSAMPLE_FLOAT_TO_16BIT(_val) (_val) <= 0.0f ? 0 : ((_val) >= 1.0f ? 65535 : (int)(65535.0f * (_val)))
#else

BLI_INLINE int UPSAMPLE_8_TO_12(const unsigned char _val)
{
	return (_val << 4) | (_val & ((1 << 4) - 1));
}
BLI_INLINE int UPSAMPLE_8_TO_16(const unsigned char _val)
{
	return (_val << 8) + _val;
}

BLI_INLINE int DOWNSAMPLE_FLOAT_TO_8BIT(const float _val)
{
	return (_val) <= 0.0f ? 0 : ((_val) >= 1.0f ? 255 : (int)(255.0f * (_val)));
}
BLI_INLINE int DOWNSAMPLE_FLOAT_TO_12BIT(const float _val)
{
	return (_val) <= 0.0f ? 0 : ((_val) >= 1.0f ? 4095 : (int)(4095.0f * (_val)));
}
BLI_INLINE int DOWNSAMPLE_FLOAT_TO_16BIT(const float _val)
{
	return (_val) <= 0.0f ? 0 : ((_val) >= 1.0f ? 65535 : (int)(65535.0f * (_val)));
}
#endif

/*
 * 2048x1080 (2K) at 24 fps or 48 fps, or 4096x2160 (4K) at 24 fps; 3x12 bits per pixel, XYZ color space
 *
 * - In 2K, for Scope (2.39:1) presentation 2048x858 pixels of the imager is used
 * - In 2K, for Flat (1.85:1) presentation 1998x1080 pixels of the imager is used
 */

/* ****************************** COPIED FROM image_to_j2k.c */

/* ----------------------------------------------------------------------- */
#define CINEMA_24_CS 1302083    /*Codestream length for 24fps*/
#define CINEMA_48_CS 651041     /*Codestream length for 48fps*/
#define COMP_24_CS 1041666      /*Maximum size per color component for 2K & 4K @ 24fps*/
#define COMP_48_CS 520833       /*Maximum size per color component for 2K @ 48fps*/


static int initialise_4K_poc(opj_poc_t *POC, int numres)
{
	POC[0].tile  = 1; 
	POC[0].resno0  = 0; 
	POC[0].compno0 = 0;
	POC[0].layno1  = 1;
	POC[0].resno1  = numres - 1;
	POC[0].compno1 = 3;
	POC[0].prg1 = CPRL;
	POC[1].tile  = 1;
	POC[1].resno0  = numres - 1;
	POC[1].compno0 = 0;
	POC[1].layno1  = 1;
	POC[1].resno1  = numres;
	POC[1].compno1 = 3;
	POC[1].prg1 = CPRL;
	return 2;
}

static void cinema_parameters(opj_cparameters_t *parameters)
{
	parameters->tile_size_on = 0; /* FALSE */
	parameters->cp_tdx = 1;
	parameters->cp_tdy = 1;
	
	/*Tile part*/
	parameters->tp_flag = 'C';
	parameters->tp_on = 1;

	/*Tile and Image shall be at (0, 0)*/
	parameters->cp_tx0 = 0;
	parameters->cp_ty0 = 0;
	parameters->image_offset_x0 = 0;
	parameters->image_offset_y0 = 0;

	/*Codeblock size = 32 * 32*/
	parameters->cblockw_init = 32;	
	parameters->cblockh_init = 32;
	parameters->csty |= 0x01;

	/*The progression order shall be CPRL*/
	parameters->prog_order = CPRL;

	/* No ROI */
	parameters->roi_compno = -1;

	parameters->subsampling_dx = 1;     parameters->subsampling_dy = 1;

	/* 9-7 transform */
	parameters->irreversible = 1;

}

static void cinema_setup_encoder(opj_cparameters_t *parameters, opj_image_t *image, img_fol_t *img_fol)
{
	int i;
	float temp_rate;

	switch (parameters->cp_cinema) {
		case CINEMA2K_24:
		case CINEMA2K_48:
			if (parameters->numresolution > 6) {
				parameters->numresolution = 6;
			}
			if (!((image->comps[0].w == 2048) || (image->comps[0].h == 1080))) {
				fprintf(stdout, "Image coordinates %d x %d is not 2K compliant.\nJPEG Digital Cinema Profile-3 "
				        "(2K profile) compliance requires that at least one of coordinates match 2048 x 1080\n",
				        image->comps[0].w, image->comps[0].h);
				parameters->cp_rsiz = STD_RSIZ;
			}
			break;
	
		case CINEMA4K_24:
			if (parameters->numresolution < 1) {
				parameters->numresolution = 1;
			}
			else if (parameters->numresolution > 7) {
				parameters->numresolution = 7;
			}
			if (!((image->comps[0].w == 4096) || (image->comps[0].h == 2160))) {
				fprintf(stdout, "Image coordinates %d x %d is not 4K compliant.\nJPEG Digital Cinema Profile-4"
				        "(4K profile) compliance requires that at least one of coordinates match 4096 x 2160\n",
				        image->comps[0].w, image->comps[0].h);
				parameters->cp_rsiz = STD_RSIZ;
			}
			parameters->numpocs = initialise_4K_poc(parameters->POC, parameters->numresolution);
			break;
		case OFF:
			/* do nothing */
			break;
	}

	switch (parameters->cp_cinema) {
		case CINEMA2K_24:
		case CINEMA4K_24:
			for (i = 0; i < parameters->tcp_numlayers; i++) {
				temp_rate = 0;
				if (img_fol->rates[i] == 0) {
					parameters->tcp_rates[0] = ((float) (image->numcomps * image->comps[0].w * image->comps[0].h * image->comps[0].prec)) /
					                           (CINEMA_24_CS * 8 * image->comps[0].dx * image->comps[0].dy);
				}
				else {
					temp_rate = ((float) (image->numcomps * image->comps[0].w * image->comps[0].h * image->comps[0].prec)) /
					            (img_fol->rates[i] * 8 * image->comps[0].dx * image->comps[0].dy);
					if (temp_rate > CINEMA_24_CS) {
						parameters->tcp_rates[i] = ((float) (image->numcomps * image->comps[0].w * image->comps[0].h * image->comps[0].prec)) /
						                           (CINEMA_24_CS * 8 * image->comps[0].dx * image->comps[0].dy);
					}
					else {
						parameters->tcp_rates[i] = img_fol->rates[i];
					}
				}
			}
			parameters->max_comp_size = COMP_24_CS;
			break;
		
		case CINEMA2K_48:
			for (i = 0; i < parameters->tcp_numlayers; i++) {
				temp_rate = 0;
				if (img_fol->rates[i] == 0) {
					parameters->tcp_rates[0] = ((float) (image->numcomps * image->comps[0].w * image->comps[0].h * image->comps[0].prec)) /
					                           (CINEMA_48_CS * 8 * image->comps[0].dx * image->comps[0].dy);
				}
				else {
					temp_rate = ((float) (image->numcomps * image->comps[0].w * image->comps[0].h * image->comps[0].prec)) /
					            (img_fol->rates[i] * 8 * image->comps[0].dx * image->comps[0].dy);
					if (temp_rate > CINEMA_48_CS) {
						parameters->tcp_rates[0] = ((float) (image->numcomps * image->comps[0].w * image->comps[0].h * image->comps[0].prec)) /
						                           (CINEMA_48_CS * 8 * image->comps[0].dx * image->comps[0].dy);
					}
					else {
						parameters->tcp_rates[i] = img_fol->rates[i];
					}
				}
			}
			parameters->max_comp_size = COMP_48_CS;
			break;
		case OFF:
			/* do nothing */
			break;
	}
	parameters->cp_disto_alloc = 1;
}


static opj_image_t *ibuftoimage(ImBuf *ibuf, opj_cparameters_t *parameters)
{
	unsigned char *rect_uchar;
	float *rect_float;
	
	unsigned int subsampling_dx = parameters->subsampling_dx;
	unsigned int subsampling_dy = parameters->subsampling_dy;
	
	unsigned int i, i_next, numcomps, w, h, prec;
	unsigned int y;
	int *r, *g, *b, *a; /* matching 'opj_image_comp.data' type */
	OPJ_COLOR_SPACE color_space;
	opj_image_cmptparm_t cmptparm[4];   /* maximum of 4 components */
	opj_image_t *image = NULL;
	
	img_fol_t img_fol; /* only needed for cinema presets */
	memset(&img_fol, 0, sizeof(img_fol_t));
	
	if (ibuf->ftype & JP2_CINE) {
		
		if (ibuf->x == 4096 || ibuf->y == 2160)
			parameters->cp_cinema = CINEMA4K_24;
		else {
			if (ibuf->ftype & JP2_CINE_48FPS) {
				parameters->cp_cinema = CINEMA2K_48;
			}
			else {
				parameters->cp_cinema = CINEMA2K_24;
			}
		}
		if (parameters->cp_cinema) {
			img_fol.rates = (float *)MEM_mallocN(parameters->tcp_numlayers * sizeof(float), "jp2_rates");
			for (i = 0; i < parameters->tcp_numlayers; i++) {
				img_fol.rates[i] = parameters->tcp_rates[i];
			}
			cinema_parameters(parameters);
		}
		
		color_space = CLRSPC_SYCC;
		prec = 12;
		numcomps = 3;
	}
	else { 
		/* Get settings from the imbuf */
		color_space = (ibuf->ftype & JP2_YCC) ? CLRSPC_SYCC : CLRSPC_SRGB;
		
		if (ibuf->ftype & JP2_16BIT) prec = 16;
		else if (ibuf->ftype & JP2_12BIT) prec = 12;
		else prec = 8;
		
		/* 32bit images == alpha channel */
		/* grayscale not supported yet */
		numcomps = (ibuf->planes == 32) ? 4 : 3;
	}
	
	w = ibuf->x;
	h = ibuf->y;
	
	
	/* initialize image components */
	memset(&cmptparm, 0, 4 * sizeof(opj_image_cmptparm_t));
	for (i = 0; i < numcomps; i++) {
		cmptparm[i].prec = prec;
		cmptparm[i].bpp = prec;
		cmptparm[i].sgnd = 0;
		cmptparm[i].dx = subsampling_dx;
		cmptparm[i].dy = subsampling_dy;
		cmptparm[i].w = w;
		cmptparm[i].h = h;
	}
	/* create the image */
	image = opj_image_create(numcomps, &cmptparm[0], color_space);
	if (!image) {
		printf("Error: opj_image_create() failed\n");
		return NULL;
	}

	/* set image offset and reference grid */
	image->x0 = parameters->image_offset_x0;
	image->y0 = parameters->image_offset_y0;
	image->x1 = image->x0 + (w - 1) * subsampling_dx + 1 + image->x0;
	image->y1 = image->y0 + (h - 1) * subsampling_dy + 1 + image->y0;

	/* set image data */
	rect_uchar = (unsigned char *) ibuf->rect;
	rect_float = ibuf->rect_float;
	
	/* set the destination channels */
	r = image->comps[0].data;
	g = image->comps[1].data;
	b = image->comps[2].data;
	a = (numcomps == 4) ? image->comps[3].data : NULL;

	if (rect_float && rect_uchar && prec == 8) {
		/* No need to use the floating point buffer, just write the 8 bits from the char buffer */
		rect_float = NULL;
	}
	
	if (rect_float) {
		switch (prec) {
			case 8: /* Convert blenders float color channels to 8, 12 or 16bit ints */
				if (numcomps == 4) {
					if (ibuf->profile == IB_PROFILE_LINEAR_RGB) {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_8BIT(linearrgb_to_srgb(rect_float[0]));
							g[i] = DOWNSAMPLE_FLOAT_TO_8BIT(linearrgb_to_srgb(rect_float[1]));
							b[i] = DOWNSAMPLE_FLOAT_TO_8BIT(linearrgb_to_srgb(rect_float[2]));
							a[i] = DOWNSAMPLE_FLOAT_TO_8BIT(rect_float[3]);
						}
						PIXEL_LOOPER_END;
					}
					else {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_8BIT(rect_float[0]);
							g[i] = DOWNSAMPLE_FLOAT_TO_8BIT(rect_float[1]);
							b[i] = DOWNSAMPLE_FLOAT_TO_8BIT(rect_float[2]);
							a[i] = DOWNSAMPLE_FLOAT_TO_8BIT(rect_float[3]);
						}
						PIXEL_LOOPER_END;
					}
				}
				else {
					if (ibuf->profile == IB_PROFILE_LINEAR_RGB) {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_8BIT(linearrgb_to_srgb(rect_float[0]));
							g[i] = DOWNSAMPLE_FLOAT_TO_8BIT(linearrgb_to_srgb(rect_float[1]));
							b[i] = DOWNSAMPLE_FLOAT_TO_8BIT(linearrgb_to_srgb(rect_float[2]));
						}
						PIXEL_LOOPER_END;
					}
					else {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_8BIT(rect_float[0]);
							g[i] = DOWNSAMPLE_FLOAT_TO_8BIT(rect_float[1]);
							b[i] = DOWNSAMPLE_FLOAT_TO_8BIT(rect_float[2]);
						}
						PIXEL_LOOPER_END;
					}
				}
				break;
			
			case 12:
				if (numcomps == 4) {
					if (ibuf->profile == IB_PROFILE_LINEAR_RGB) {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_12BIT(linearrgb_to_srgb(rect_float[0]));
							g[i] = DOWNSAMPLE_FLOAT_TO_12BIT(linearrgb_to_srgb(rect_float[1]));
							b[i] = DOWNSAMPLE_FLOAT_TO_12BIT(linearrgb_to_srgb(rect_float[2]));
							a[i] = DOWNSAMPLE_FLOAT_TO_12BIT(rect_float[3]);
						}
						PIXEL_LOOPER_END;
					}
					else {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_12BIT(rect_float[0]);
							g[i] = DOWNSAMPLE_FLOAT_TO_12BIT(rect_float[1]);
							b[i] = DOWNSAMPLE_FLOAT_TO_12BIT(rect_float[2]);
							a[i] = DOWNSAMPLE_FLOAT_TO_12BIT(rect_float[3]);
						}
						PIXEL_LOOPER_END;
					}
				}
				else {
					if (ibuf->profile == IB_PROFILE_LINEAR_RGB) {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_12BIT(linearrgb_to_srgb(rect_float[0]));
							g[i] = DOWNSAMPLE_FLOAT_TO_12BIT(linearrgb_to_srgb(rect_float[1]));
							b[i] = DOWNSAMPLE_FLOAT_TO_12BIT(linearrgb_to_srgb(rect_float[2]));
						}
						PIXEL_LOOPER_END;
					}
					else {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_12BIT(rect_float[0]);
							g[i] = DOWNSAMPLE_FLOAT_TO_12BIT(rect_float[1]);
							b[i] = DOWNSAMPLE_FLOAT_TO_12BIT(rect_float[2]);
						}
						PIXEL_LOOPER_END;
					}
				}
				break;

			case 16:
				if (numcomps == 4) {
					if (ibuf->profile == IB_PROFILE_LINEAR_RGB) {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_16BIT(linearrgb_to_srgb(rect_float[0]));
							g[i] = DOWNSAMPLE_FLOAT_TO_16BIT(linearrgb_to_srgb(rect_float[1]));
							b[i] = DOWNSAMPLE_FLOAT_TO_16BIT(linearrgb_to_srgb(rect_float[2]));
							a[i] = DOWNSAMPLE_FLOAT_TO_16BIT(rect_float[3]);
						}
						PIXEL_LOOPER_END;
					}
					else {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_16BIT(rect_float[0]);
							g[i] = DOWNSAMPLE_FLOAT_TO_16BIT(rect_float[1]);
							b[i] = DOWNSAMPLE_FLOAT_TO_16BIT(rect_float[2]);
							a[i] = DOWNSAMPLE_FLOAT_TO_16BIT(rect_float[3]);
						}
						PIXEL_LOOPER_END;
					}
				}
				else {
					if (ibuf->profile == IB_PROFILE_LINEAR_RGB) {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_16BIT(linearrgb_to_srgb(rect_float[0]));
							g[i] = DOWNSAMPLE_FLOAT_TO_16BIT(linearrgb_to_srgb(rect_float[1]));
							b[i] = DOWNSAMPLE_FLOAT_TO_16BIT(linearrgb_to_srgb(rect_float[2]));
						}
						PIXEL_LOOPER_END;
					}
					else {
						PIXEL_LOOPER_BEGIN(rect_float)
						{
							r[i] = DOWNSAMPLE_FLOAT_TO_16BIT(rect_float[0]);
							g[i] = DOWNSAMPLE_FLOAT_TO_16BIT(rect_float[1]);
							b[i] = DOWNSAMPLE_FLOAT_TO_16BIT(rect_float[2]);
						}
						PIXEL_LOOPER_END;
					}
				}
				break;
		}
	}
	else {
		/* just use rect*/
		switch (prec) {
			case 8:
				if (numcomps == 4) {
					PIXEL_LOOPER_BEGIN(rect_uchar)
					{
						r[i] = rect_uchar[0];
						g[i] = rect_uchar[1];
						b[i] = rect_uchar[2];
						a[i] = rect_uchar[3];
					}
					PIXEL_LOOPER_END;
				}
				else {
					PIXEL_LOOPER_BEGIN(rect_uchar)
					{
						r[i] = rect_uchar[0];
						g[i] = rect_uchar[1];
						b[i] = rect_uchar[2];
					}
					PIXEL_LOOPER_END;
				}
				break;
			
			case 12: /* Up Sampling, a bit pointless but best write the bit depth requested */
				if (numcomps == 4) {
					PIXEL_LOOPER_BEGIN(rect_uchar)
					{
						r[i] = UPSAMPLE_8_TO_12(rect_uchar[0]);
						g[i] = UPSAMPLE_8_TO_12(rect_uchar[1]);
						b[i] = UPSAMPLE_8_TO_12(rect_uchar[2]);
						a[i] = UPSAMPLE_8_TO_12(rect_uchar[3]);
					}
					PIXEL_LOOPER_END;
				}
				else {
					PIXEL_LOOPER_BEGIN(rect_uchar)
					{
						r[i] = UPSAMPLE_8_TO_12(rect_uchar[0]);
						g[i] = UPSAMPLE_8_TO_12(rect_uchar[1]);
						b[i] = UPSAMPLE_8_TO_12(rect_uchar[2]);
					}
					PIXEL_LOOPER_END;
				}
				break;

			case 16:
				if (numcomps == 4) {
					PIXEL_LOOPER_BEGIN(rect_uchar)
					{
						r[i] = UPSAMPLE_8_TO_16(rect_uchar[0]);
						g[i] = UPSAMPLE_8_TO_16(rect_uchar[1]);
						b[i] = UPSAMPLE_8_TO_16(rect_uchar[2]);
						a[i] = UPSAMPLE_8_TO_16(rect_uchar[3]);
					}
					PIXEL_LOOPER_END;
				}
				else {
					PIXEL_LOOPER_BEGIN(rect_uchar)
					{
						r[i] = UPSAMPLE_8_TO_16(rect_uchar[0]);
						g[i] = UPSAMPLE_8_TO_16(rect_uchar[1]);
						b[i] = UPSAMPLE_8_TO_16(rect_uchar[2]);
					}
					PIXEL_LOOPER_END;
				}
				break;
		}
	}
	
	/* Decide if MCT should be used */
	parameters->tcp_mct = image->numcomps == 3 ? 1 : 0;
	
	if (parameters->cp_cinema) {
		cinema_setup_encoder(parameters, image, &img_fol);
	}
	
	if (img_fol.rates)
		MEM_freeN(img_fol.rates);
	
	return image;
}


/* Found write info at http://users.ece.gatech.edu/~slabaugh/personal/c/bitmapUnix.c */
int imb_savejp2(struct ImBuf *ibuf, const char *name, int flags)
{
	int quality = ibuf->ftype & 0xff;
	
	int bSuccess;
	opj_cparameters_t parameters;   /* compression parameters */
	opj_event_mgr_t event_mgr;      /* event manager */
	opj_image_t *image = NULL;
	
	(void)flags; /* unused */
	
	/*
	 * configure the event callbacks (not required)
	 * setting of each callback is optionnal
	 */
	memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
	event_mgr.error_handler = error_callback;
	event_mgr.warning_handler = warning_callback;
	event_mgr.info_handler = info_callback;
	
	/* set encoding parameters to default values */
	opj_set_default_encoder_parameters(&parameters);
	
	/* compression ratio */
	/* invert range, from 10-100, 100-1
	 * where jpeg see's 1 and highest quality (lossless) and 100 is very low quality*/
	parameters.tcp_rates[0] = ((100 - quality) / 90.0f * 99.0f) + 1;

	
	parameters.tcp_numlayers = 1; // only one resolution
	parameters.cp_disto_alloc = 1;

	image = ibuftoimage(ibuf, &parameters);
	
	
	{   /* JP2 format output */
		int codestream_length;
		opj_cio_t *cio = NULL;
		FILE *f = NULL;

		/* get a JP2 compressor handle */
		opj_cinfo_t *cinfo = opj_create_compress(CODEC_JP2);

		/* catch events using our callbacks and give a local context */
		opj_set_event_mgr((opj_common_ptr)cinfo, &event_mgr, stderr);

		/* setup the encoder parameters using the current image and using user parameters */
		opj_setup_encoder(cinfo, &parameters, image);

		/* open a byte stream for writing */
		/* allocate memory for all tiles */
		cio = opj_cio_open((opj_common_ptr)cinfo, NULL, 0);

		/* encode the image */
		bSuccess = opj_encode(cinfo, cio, image, NULL); /* last arg used to be parameters.index but this deprecated */
		
		if (!bSuccess) {
			opj_cio_close(cio);
			fprintf(stderr, "failed to encode image\n");
			return 0;
		}
		codestream_length = cio_tell(cio);

		/* write the buffer to disk */
		f = BLI_fopen(name, "wb");
		
		if (!f) {
			fprintf(stderr, "failed to open %s for writing\n", name);
			return 1;
		}
		fwrite(cio->buffer, 1, codestream_length, f);
		fclose(f);
		fprintf(stderr, "Generated outfile %s\n", name);
		/* close and free the byte stream */
		opj_cio_close(cio);
		
		/* free remaining compression structures */
		opj_destroy_compress(cinfo);
	}

	/* free image data */
	opj_image_destroy(image);
	
	return 1;
}
