#include "codec.h"
#include "format.h"
#include "debayer.h"

#include <openjpeg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void error_callback(const char *msg, void *client_data) {
	FILE *stream = (FILE*)client_data;
	fprintf(stream, "[R3D ERR] %s", msg);
}

static void warning_callback(const char *msg, void *client_data) {
	FILE *stream = (FILE*)client_data;
	fprintf(stream, "[R3D WARN] %s", msg);
}

static void info_callback(const char *msg, void *client_data) {
	(void)client_data;
	fprintf(stdout, "[R3D INFO] %s", msg);
}

#define J2K_CFMT 0
#define JP2_CFMT 1
#define JPT_CFMT 2

struct redcode_frame_raw * redcode_decode_video_raw(
	struct redcode_frame * frame, int scale)
{
	struct redcode_frame_raw * rv = NULL;
	opj_dparameters_t parameters;	/* decompression parameters */
	opj_event_mgr_t event_mgr;		/* event manager */
	opj_image_t *image = NULL;
	opj_dinfo_t* dinfo = NULL;	/* handle to a decompressor */
	opj_cio_t *cio = NULL;

	memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
	event_mgr.error_handler = error_callback;
	event_mgr.warning_handler = warning_callback;
	event_mgr.info_handler = info_callback;

	opj_set_default_decoder_parameters(&parameters);

	parameters.decod_format = JP2_CFMT;

	if (scale == 2) {
		parameters.cp_reduce = 1;
	} else if (scale == 4) {
		parameters.cp_reduce = 2;
	} else if (scale == 8) {
		parameters.cp_reduce = 3;
	}

	/* JPEG 2000 compressed image data */
	
	/* get a decoder handle */
	dinfo = opj_create_decompress(CODEC_JP2);

	/* catch events using our callbacks and give a local context */
	opj_set_event_mgr((opj_common_ptr)dinfo, &event_mgr, stderr);
	
	/* setup the decoder decoding parameters using the current image 
	   and user parameters */
	opj_setup_decoder(dinfo, &parameters);
			
	/* open a byte stream */
	cio = opj_cio_open((opj_common_ptr)dinfo, 
			   frame->data + frame->offset, frame->length);

	image = opj_decode(dinfo, cio);			

	if(!image) {
		fprintf(stderr, 
			"ERROR -> j2k_to_image: failed to decode image!\n");
		opj_destroy_decompress(dinfo);
		opj_cio_close(cio);
		return 0;
	}

	/* close the byte stream */
	opj_cio_close(cio);

	/* free remaining structures */
	if(dinfo) {
		opj_destroy_decompress(dinfo);
	}

	if((image->numcomps * image->x1 * image->y1) == 0) {
		opj_image_destroy(image);
		return 0;
	}
		
	rv = (struct redcode_frame_raw *) calloc(
		1, sizeof(struct redcode_frame_raw));

	rv->data = image;
	rv->width = image->comps[0].w;
	rv->height = image->comps[0].h;

	return rv;
}

int redcode_decode_video_float(struct redcode_frame_raw * frame, 
			       float * out, int scale)
{
	int* planes[4];
	int i;
	opj_image_t *image = (opj_image_t*) frame->data;

	if (image->numcomps != 4) {
		fprintf(stderr, "R3D: need 4 planes, but got: %d\n",
			image->numcomps);
		return 0;
	}

	for (i = 0; i < 4; i++) {
		planes[i] = image->comps[i].data;
	}

	if (scale == 1) {
		redcode_ycbcr2rgb_fullscale(
			planes, frame->width, frame->height, out);
	} else if (scale == 2) {
		redcode_ycbcr2rgb_halfscale(
			planes, frame->width, frame->height, out);
	} else if (scale == 4) {
		redcode_ycbcr2rgb_quarterscale(
			planes, frame->width, frame->height, out);
	}

	opj_image_destroy(image);

	free(frame);

	return 1;
}



