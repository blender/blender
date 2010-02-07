/*
 * ffmpeg-write support
 *
 * Partial Copyright (c) 2006 Peter Schlaile
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifdef WITH_FFMPEG
#include <string.h>
#include <stdio.h>

#if defined(_WIN32) && defined(_DEBUG) && !defined(__MINGW32__) && !defined(__CYGWIN__)
/* This does not seem necessary or present on MSVC 8, but may be needed in earlier versions? */
#if _MSC_VER < 1400
#include <stdint.h>
#endif
#endif

#include <stdlib.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <libavcodec/opt.h>

#if LIBAVFORMAT_VERSION_INT < (49 << 16)
#define FFMPEG_OLD_FRAME_RATE 1
#else
#define FFMPEG_CODEC_IS_POINTER 1
#define FFMPEG_CODEC_TIME_BASE  1
#endif

#if LIBAVFORMAT_VERSION_INT >= (52 << 16)
#define OUTFILE_PB (outfile->pb)
#else
#define OUTFILE_PB (&outfile->pb)
#endif

#if defined(WIN32) && (!(defined snprintf))
#define snprintf _snprintf
#endif

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_blenlib.h"

#include "AUD_C-API.h" /* must be before BKE_sound.h for define */

#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_sound.h"
#include "BKE_writeffmpeg.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

extern void do_init_ffmpeg();

static int ffmpeg_type = 0;
static int ffmpeg_codec = CODEC_ID_MPEG4;
static int ffmpeg_audio_codec = CODEC_ID_NONE;
static int ffmpeg_video_bitrate = 1150;
static int ffmpeg_audio_bitrate = 128;
static int ffmpeg_gop_size = 12;
static int ffmpeg_autosplit = 0;
static int ffmpeg_autosplit_count = 0;

static AVFormatContext* outfile = 0;
static AVStream* video_stream = 0;
static AVStream* audio_stream = 0;
static AVFrame* current_frame = 0;
static struct SwsContext *img_convert_ctx = 0;

static uint8_t* video_buffer = 0;
static int video_buffersize = 0;

static uint8_t* audio_input_buffer = 0;
static int audio_input_frame_size = 0;
static uint8_t* audio_output_buffer = 0;
static int audio_outbuf_size = 0;
static double audio_time = 0.0f;

static AUD_Device* audio_mixdown_device = 0;

#define FFMPEG_AUTOSPLIT_SIZE 2000000000

/* Delete a picture buffer */

static void delete_picture(AVFrame* f)
{
	if (f) {
		if (f->data[0]) MEM_freeN(f->data[0]);
		av_free(f);
	}
}

#ifdef FFMPEG_CODEC_IS_POINTER
static AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return stream->codec;
}
#else
static AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return &stream->codec;
}
#endif

static int write_audio_frame(void) 
{
	AVCodecContext* c = NULL;
	AVPacket pkt;
	AVStream* str = audio_stream;

	c = get_codec_from_stream(audio_stream);

	av_init_packet(&pkt);
	pkt.size = 0;

	AUD_readDevice(audio_mixdown_device, audio_input_buffer, audio_input_frame_size);
	audio_time += (double) audio_input_frame_size / (double) c->sample_rate;

	pkt.size = avcodec_encode_audio(c, audio_output_buffer,
					audio_outbuf_size,
					(short*) audio_input_buffer);

	if(pkt.size <= 0)
	{
		// XXX error("Error writing audio packet");
		return -1;
	}

	pkt.data = audio_output_buffer;

	if(c->coded_frame && c->coded_frame->pts != AV_NOPTS_VALUE)
	{
#ifdef FFMPEG_CODEC_TIME_BASE
		pkt.pts = av_rescale_q(c->coded_frame->pts,
					   c->time_base, audio_stream->time_base);
#else
		pkt.pts = c->coded_frame->pts;
#endif
		fprintf(stderr, "Audio Frame PTS: %d\n", (int)pkt.pts);
	}

	pkt.stream_index = audio_stream->index;
	pkt.flags |= PKT_FLAG_KEY;
	if (av_interleaved_write_frame(outfile, &pkt) != 0) {
		// XXX error("Error writing audio packet");
		return -1;
	}
	return 0;
}

/* Allocate a temporary frame */
static AVFrame* alloc_picture(int pix_fmt, int width, int height) 
{
	AVFrame* f;
	uint8_t* buf;
	int size;
	
	/* allocate space for the struct */
	f = avcodec_alloc_frame();
	if (!f) return NULL;
	size = avpicture_get_size(pix_fmt, width, height);
	/* allocate the actual picture buffer */
	buf = MEM_mallocN(size, "AVFrame buffer");
	if (!buf) {
		free(f);
		return NULL;
	}
	avpicture_fill((AVPicture*)f, buf, pix_fmt, width, height);
	return f;
}

/* Get the correct file extensions for the requested format,
   first is always desired guess_format parameter */
static const char** get_file_extensions(int format) 
{
	switch(format) {
	case FFMPEG_DV: {
		static const char * rv[] = { ".dv", NULL };
		return rv;
	}
	case FFMPEG_MPEG1: {
		static const char * rv[] = { ".mpg", ".mpeg", NULL };
		return rv;
	}
	case FFMPEG_MPEG2: {
		static const char * rv[] = { ".dvd", ".vob", ".mpg", ".mpeg",
					     NULL };
		return rv;
	}
	case FFMPEG_MPEG4: {
		static const char * rv[] = { ".mp4", ".mpg", ".mpeg", NULL };
		return rv;
	}
	case FFMPEG_AVI: {
		static const char * rv[] = { ".avi", NULL };
		return rv;
	}
	case FFMPEG_MOV: {
		static const char * rv[] = { ".mov", NULL };
		return rv;
	}
	case FFMPEG_H264: {
		/* FIXME: avi for now... */
		static const char * rv[] = { ".avi", NULL };
		return rv;
	}

	case FFMPEG_XVID: {
		/* FIXME: avi for now... */
		static const char * rv[] = { ".avi", NULL };
		return rv;
	}
	case FFMPEG_FLV: {
		static const char * rv[] = { ".flv", NULL };
		return rv;
	}
	case FFMPEG_MKV: {
		static const char * rv[] = { ".mkv", NULL };
		return rv;
	}
	case FFMPEG_OGG: {
		static const char * rv[] = { ".ogg", ".ogv", NULL };
		return rv;
	}
	case FFMPEG_MP3: {
		static const char * rv[] = { ".mp3", NULL };
		return rv;
	}
	case FFMPEG_WAV: {
		static const char * rv[] = { ".wav", NULL };
		return rv;
	}
	default:
		return NULL;
	}
}

/* Write a frame to the output file */
static int write_video_frame(RenderData *rd, AVFrame* frame, ReportList *reports)
{
	int outsize = 0;
	int ret, success= 1;
	AVCodecContext* c = get_codec_from_stream(video_stream);
#ifdef FFMPEG_CODEC_TIME_BASE
	frame->pts = rd->cfra - rd->sfra;
#endif
	if (rd->mode & R_FIELDS) {
		frame->top_field_first = ((rd->mode & R_ODDFIELD) != 0);
	}

	outsize = avcodec_encode_video(c, video_buffer, video_buffersize, 
				       frame);
	if (outsize != 0) {
		AVPacket packet;
		av_init_packet(&packet);

		if (c->coded_frame->pts != AV_NOPTS_VALUE) {
#ifdef FFMPEG_CODEC_TIME_BASE
			packet.pts = av_rescale_q(c->coded_frame->pts,
						  c->time_base,
						  video_stream->time_base);
#else
			packet.pts = c->coded_frame->pts;
#endif
			fprintf(stderr, "Video Frame PTS: %d\n", (int)packet.pts);
		} else {
			fprintf(stderr, "Video Frame PTS: not set\n");
		}
		if (c->coded_frame->key_frame)
			packet.flags |= PKT_FLAG_KEY;
		packet.stream_index = video_stream->index;
		packet.data = video_buffer;
		packet.size = outsize;
		ret = av_interleaved_write_frame(outfile, &packet);
	} else ret = 0;

	if (ret != 0) {
		success= 0;
		BKE_report(reports, RPT_ERROR, "Error writing frame.");
	}

	return success;
}

/* read and encode a frame of audio from the buffer */
static AVFrame* generate_video_frame(uint8_t* pixels, ReportList *reports) 
{
	uint8_t* rendered_frame;

	AVCodecContext* c = get_codec_from_stream(video_stream);
	int width = c->width;
	int height = c->height;
	AVFrame* rgb_frame;

	if (c->pix_fmt != PIX_FMT_BGR32) {
		rgb_frame = alloc_picture(PIX_FMT_BGR32, width, height);
		if (!rgb_frame) {
			BKE_report(reports, RPT_ERROR, "Couldn't allocate temporary frame.");
			return NULL;
		}
	} else {
		rgb_frame = current_frame;
	}

	rendered_frame = pixels;

	/* Do RGBA-conversion and flipping in one step depending
	   on CPU-Endianess */

	if (ENDIAN_ORDER == L_ENDIAN) {
		int y;
		for (y = 0; y < height; y++) {
			uint8_t* target = rgb_frame->data[0]
				+ width * 4 * (height - y - 1);
			uint8_t* src = rendered_frame + width * 4 * y;
			uint8_t* end = src + width * 4;
			while (src != end) {
				target[3] = src[3];
				target[2] = src[2];
				target[1] = src[1];
				target[0] = src[0];

				target += 4;
				src += 4;
			}
		}
	} else {
		int y;
		for (y = 0; y < height; y++) {
			uint8_t* target = rgb_frame->data[0]
				+ width * 4 * (height - y - 1);
			uint8_t* src = rendered_frame + width * 4 * y;
			uint8_t* end = src + width * 4;
			while (src != end) {
				target[3] = src[0];
				target[2] = src[1];
				target[1] = src[2];
				target[0] = src[3];

				target += 4;
				src += 4;
			}
		}
	}

	if (c->pix_fmt != PIX_FMT_BGR32) {
		sws_scale(img_convert_ctx, rgb_frame->data,
			  rgb_frame->linesize, 0, c->height, 
			  current_frame->data, current_frame->linesize);
		delete_picture(rgb_frame);
	}
	return current_frame;
}

static void set_ffmpeg_property_option(AVCodecContext* c, IDProperty * prop)
{
	char name[128];
	char * param;
	const AVOption * rv = NULL;

	fprintf(stderr, "FFMPEG expert option: %s: ", prop->name);

	strncpy(name, prop->name, 128);

	param = strchr(name, ':');

	if (param) {
		*param++ = 0;
	}

	switch(prop->type) {
	case IDP_STRING:
		fprintf(stderr, "%s.\n", IDP_String(prop));
		rv = av_set_string(c, prop->name, IDP_String(prop));
		break;
	case IDP_FLOAT:
		fprintf(stderr, "%g.\n", IDP_Float(prop));
		rv = av_set_double(c, prop->name, IDP_Float(prop));
		break;
	case IDP_INT:
		fprintf(stderr, "%d.\n", IDP_Int(prop));
		
		if (param) {
			if (IDP_Int(prop)) {
				rv = av_set_string(c, name, param);
			} else {
				return;
			}
		} else {
			rv = av_set_int(c, prop->name, IDP_Int(prop));
		}
		break;
	}

	if (!rv) {
		fprintf(stderr, "ffmpeg-option not supported: %s! Skipping.\n",
			prop->name);
	}
}

static void set_ffmpeg_properties(RenderData *rd, AVCodecContext *c, const char * prop_name)
{
	IDProperty * prop;
	void * iter;
	IDProperty * curr;

	if (!rd->ffcodecdata.properties) {
		return;
	}
	
	prop = IDP_GetPropertyFromGroup(
		rd->ffcodecdata.properties, (char*) prop_name);
	if (!prop) {
		return;
	}

	iter = IDP_GetGroupIterator(prop);

	while ((curr = IDP_GroupIterNext(iter)) != NULL) {
		set_ffmpeg_property_option(c, curr);
	}
}

/* prepare a video stream for the output file */

static AVStream* alloc_video_stream(RenderData *rd, int codec_id, AVFormatContext* of,
				    int rectx, int recty) 
{
	AVStream* st;
	AVCodecContext* c;
	AVCodec* codec;
	st = av_new_stream(of, 0);
	if (!st) return NULL;

	/* Set up the codec context */
	
	c = get_codec_from_stream(st);
	c->codec_id = codec_id;
	c->codec_type = CODEC_TYPE_VIDEO;


	/* Get some values from the current render settings */
	
	c->width = rectx;
	c->height = recty;

#ifdef FFMPEG_CODEC_TIME_BASE
	/* FIXME: Really bad hack (tm) for NTSC support */
	if (ffmpeg_type == FFMPEG_DV && rd->frs_sec != 25) {
		c->time_base.den = 2997;
		c->time_base.num = 100;
	} else if ((double) ((int) rd->frs_sec_base) == 
		   rd->frs_sec_base) {
		c->time_base.den = rd->frs_sec;
		c->time_base.num = (int) rd->frs_sec_base;
	} else {
		c->time_base.den = rd->frs_sec * 100000;
		c->time_base.num = ((double) rd->frs_sec_base) * 100000;
	}
#else
	/* FIXME: Really bad hack (tm) for NTSC support */
	if (ffmpeg_type == FFMPEG_DV && rd->frs_sec != 25) {
		c->frame_rate = 2997;
		c->frame_rate_base = 100;
	} else if ((double) ((int) rd->frs_sec_base) == 
		   rd->frs_sec_base) {
		c->frame_rate = rd->frs_sec;
		c->frame_rate_base = rd->frs_sec_base;
	} else {
		c->frame_rate = rd->frs_sec * 100000;
		c->frame_rate_base = ((double) rd->frs_sec_base)*100000;
	}
#endif
	
	c->gop_size = ffmpeg_gop_size;
	c->bit_rate = ffmpeg_video_bitrate*1000;
	c->rc_max_rate = rd->ffcodecdata.rc_max_rate*1000;
	c->rc_min_rate = rd->ffcodecdata.rc_min_rate*1000;
	c->rc_buffer_size = rd->ffcodecdata.rc_buffer_size * 1024;
	c->rc_initial_buffer_occupancy 
		= rd->ffcodecdata.rc_buffer_size*3/4;
	c->rc_buffer_aggressivity = 1.0;
	c->me_method = ME_EPZS;
	
	codec = avcodec_find_encoder(c->codec_id);
	if (!codec) return NULL;
	
	/* Be sure to use the correct pixel format(e.g. RGB, YUV) */
	
	if (codec->pix_fmts) {
		c->pix_fmt = codec->pix_fmts[0];
	} else {
		/* makes HuffYUV happy ... */
		c->pix_fmt = PIX_FMT_YUV422P;
	}

	if (codec_id == CODEC_ID_XVID) {
		/* arghhhh ... */
		c->pix_fmt = PIX_FMT_YUV420P;
	}
	
	if ((of->oformat->flags & AVFMT_GLOBALHEADER)
//		|| !strcmp(of->oformat->name, "mp4")
//	    || !strcmp(of->oformat->name, "mov")
//	    || !strcmp(of->oformat->name, "3gp")
		) {
		fprintf(stderr, "Using global header\n");
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	
	/* Determine whether we are encoding interlaced material or not */
	if (rd->mode & R_FIELDS) {
		fprintf(stderr, "Encoding interlaced video\n");
		c->flags |= CODEC_FLAG_INTERLACED_DCT;
		c->flags |= CODEC_FLAG_INTERLACED_ME;
	}

	/* xasp & yasp got float lately... */

	st->sample_aspect_ratio = c->sample_aspect_ratio = av_d2q(
		((double) rd->xasp / (double) rd->yasp), 255);

	set_ffmpeg_properties(rd, c, "video");
	
	if (avcodec_open(c, codec) < 0) {
		//
		//XXX error("Couldn't initialize codec");
		return NULL;
	}

	video_buffersize = 2000000;
	video_buffer = (uint8_t*)MEM_mallocN(video_buffersize, 
					     "FFMPEG video buffer");
	
	current_frame = alloc_picture(c->pix_fmt, c->width, c->height);

	img_convert_ctx = sws_getContext(c->width, c->height,
					 PIX_FMT_BGR32,
					 c->width, c->height,
					 c->pix_fmt,
					 SWS_BICUBIC,
					 NULL, NULL, NULL);
	return st;
}

/* Prepare an audio stream for the output file */

static AVStream* alloc_audio_stream(RenderData *rd, int codec_id, AVFormatContext* of) 
{
	AVStream* st;
	AVCodecContext* c;
	AVCodec* codec;

	st = av_new_stream(of, 1);
	if (!st) return NULL;

	c = get_codec_from_stream(st);
	c->codec_id = codec_id;
	c->codec_type = CODEC_TYPE_AUDIO;

	c->sample_rate = rd->ffcodecdata.audio_mixrate;
	c->bit_rate = ffmpeg_audio_bitrate*1000;
	c->sample_fmt = SAMPLE_FMT_S16;
	c->channels = 2;
	codec = avcodec_find_encoder(c->codec_id);
	if (!codec) {
		//XXX error("Couldn't find a valid audio codec");
		return NULL;
	}

	set_ffmpeg_properties(rd, c, "audio");

	if (avcodec_open(c, codec) < 0) {
		//XXX error("Couldn't initialize audio codec");
		return NULL;
	}

	audio_outbuf_size = FF_MIN_BUFFER_SIZE;

	audio_output_buffer = (uint8_t*)MEM_mallocN(
		audio_outbuf_size, "FFMPEG audio encoder input buffer");

	if((c->codec_id >= CODEC_ID_PCM_S16LE) && (c->codec_id <= CODEC_ID_PCM_DVD))
		audio_input_frame_size = audio_outbuf_size * 8 / c->bits_per_coded_sample / c->channels;
	else
		audio_input_frame_size = c->frame_size;

	audio_input_buffer = (uint8_t*)MEM_mallocN(
		audio_input_frame_size * c->channels * sizeof(int16_t),
		"FFMPEG audio encoder output buffer");

	audio_time = 0.0f;

	return st;
}
/* essential functions -- start, append, end */

static int start_ffmpeg_impl(struct RenderData *rd, int rectx, int recty, ReportList *reports)
{
	/* Handle to the output file */
	AVFormatContext* of;
	AVOutputFormat* fmt;
	char name[256];
	const char ** exts;

	ffmpeg_type = rd->ffcodecdata.type;
	ffmpeg_codec = rd->ffcodecdata.codec;
	ffmpeg_audio_codec = rd->ffcodecdata.audio_codec;
	ffmpeg_video_bitrate = rd->ffcodecdata.video_bitrate;
	ffmpeg_audio_bitrate = rd->ffcodecdata.audio_bitrate;
	ffmpeg_gop_size = rd->ffcodecdata.gop_size;
	ffmpeg_autosplit = rd->ffcodecdata.flags
		& FFMPEG_AUTOSPLIT_OUTPUT;
	
	do_init_ffmpeg();

	/* Determine the correct filename */
	filepath_ffmpeg(name, rd);
	fprintf(stderr, "Starting output to %s(ffmpeg)...\n"
		"  Using type=%d, codec=%d, audio_codec=%d,\n"
		"  video_bitrate=%d, audio_bitrate=%d,\n"
		"  gop_size=%d, autosplit=%d\n"
		"  render width=%d, render height=%d\n", 
		name, ffmpeg_type, ffmpeg_codec, ffmpeg_audio_codec,
		ffmpeg_video_bitrate, ffmpeg_audio_bitrate,
		ffmpeg_gop_size, ffmpeg_autosplit, rectx, recty);
	
	exts = get_file_extensions(ffmpeg_type);
	if (!exts) {
		BKE_report(reports, RPT_ERROR, "No valid formats found.");
		return 0;
	}
	fmt = guess_format(NULL, exts[0], NULL);
	if (!fmt) {
		BKE_report(reports, RPT_ERROR, "No valid formats found.");
		return 0;
	}

	of = av_alloc_format_context();
	if (!of) {
		BKE_report(reports, RPT_ERROR, "Error opening output file");
		return 0;
	}
	
	of->oformat = fmt;
	of->packet_size= rd->ffcodecdata.mux_packet_size;
	if (ffmpeg_audio_codec != CODEC_ID_NONE) {
		of->mux_rate = rd->ffcodecdata.mux_rate;
	} else {
		of->mux_rate = 0;
	}

	of->preload = (int)(0.5*AV_TIME_BASE);
	of->max_delay = (int)(0.7*AV_TIME_BASE);

	fmt->audio_codec = ffmpeg_audio_codec;

	snprintf(of->filename, sizeof(of->filename), "%s", name);
	/* set the codec to the user's selection */
	switch(ffmpeg_type) {
	case FFMPEG_AVI:
	case FFMPEG_MOV:
	case FFMPEG_OGG:
	case FFMPEG_MKV:
		fmt->video_codec = ffmpeg_codec;
		break;
	case FFMPEG_DV:
		fmt->video_codec = CODEC_ID_DVVIDEO;
		break;
	case FFMPEG_MPEG1:
		fmt->video_codec = CODEC_ID_MPEG1VIDEO;
		break;
	case FFMPEG_MPEG2:
		fmt->video_codec = CODEC_ID_MPEG2VIDEO;
		break;
	case FFMPEG_H264:
		fmt->video_codec = CODEC_ID_H264;
		break;
	case FFMPEG_XVID:
		fmt->video_codec = CODEC_ID_XVID;
		break;
	case FFMPEG_FLV:
		fmt->video_codec = CODEC_ID_FLV1;
		break;
	case FFMPEG_MP3:
		fmt->audio_codec = CODEC_ID_MP3;
	case FFMPEG_WAV:
		fmt->video_codec = CODEC_ID_NONE;
		break;
	case FFMPEG_MPEG4:
	default:
		fmt->video_codec = CODEC_ID_MPEG4;
		break;
	}
	if (fmt->video_codec == CODEC_ID_DVVIDEO) {
		if (rectx != 720) {
			BKE_report(reports, RPT_ERROR, "Render width has to be 720 pixels for DV!");
			return 0;
		}
		if (rd->frs_sec != 25 && recty != 480) {
			BKE_report(reports, RPT_ERROR, "Render height has to be 480 pixels for DV-NTSC!");
			return 0;
		}
		if (rd->frs_sec == 25 && recty != 576) {
			BKE_report(reports, RPT_ERROR, "Render height has to be 576 pixels for DV-PAL!");
			return 0;
		}
	}
	
	if (ffmpeg_type == FFMPEG_DV) {
		fmt->audio_codec = CODEC_ID_PCM_S16LE;
		if (ffmpeg_audio_codec != CODEC_ID_NONE && rd->ffcodecdata.audio_mixrate != 48000) {
			BKE_report(reports, RPT_ERROR, "FFMPEG only supports 48khz / stereo audio for DV!");
			return 0;
		}
	}
	
	if (fmt->video_codec != CODEC_ID_NONE) {
		video_stream = alloc_video_stream(rd, fmt->video_codec, of, rectx, recty);
		printf("alloc video stream %p\n", video_stream);
		if (!video_stream) {
			BKE_report(reports, RPT_ERROR, "Error initializing video stream.");
			return 0;
		}
	}

	if (ffmpeg_audio_codec != CODEC_ID_NONE) {
		audio_stream = alloc_audio_stream(rd, fmt->audio_codec, of);
		if (!audio_stream) {
			BKE_report(reports, RPT_ERROR, "Error initializing audio stream.");
			return 0;
		}
	}
	if (av_set_parameters(of, NULL) < 0) {
		BKE_report(reports, RPT_ERROR, "Error setting output parameters.");
		return 0;
	}
	if (!(fmt->flags & AVFMT_NOFILE)) {
		if (url_fopen(&of->pb, name, URL_WRONLY) < 0) {
			BKE_report(reports, RPT_ERROR, "Could not open file for writing.");
			return 0;
		}
	}

	av_write_header(of);
	outfile = of;
	dump_format(of, 0, name, 1);

	return 1;
}

/* **********************************************************************
   * public interface
   ********************************************************************** */

/* Get the output filename-- similar to the other output formats */
void filepath_ffmpeg(char* string, RenderData* rd) {
	char autosplit[20];

	const char ** exts = get_file_extensions(rd->ffcodecdata.type);
	const char ** fe = exts;

	if (!string || !exts) return;

	strcpy(string, rd->pic);
	BLI_convertstringcode(string, G.sce);

	BLI_make_existing_file(string);

	autosplit[0] = 0;

	if ((rd->ffcodecdata.flags & FFMPEG_AUTOSPLIT_OUTPUT) != 0) {
		sprintf(autosplit, "_%03d", ffmpeg_autosplit_count);
	}

	while (*fe) {
		if (BLI_strcasecmp(string + strlen(string) - strlen(*fe), 
				   *fe) == 0) {
			break;
		}
		fe++;
	}

	if (!*fe) {
		strcat(string, autosplit);

		BLI_convertstringframe_range(string, rd->sfra, rd->efra, 4);
		strcat(string, *exts);
	} else {
		*(string + strlen(string) - strlen(*fe)) = 0;
		strcat(string, autosplit);
		strcat(string, *fe);
	}
}

int start_ffmpeg(struct Scene *scene, RenderData *rd, int rectx, int recty, ReportList *reports)
{
	int success;

	ffmpeg_autosplit_count = 0;

	success = start_ffmpeg_impl(rd, rectx, recty, reports);

	if(audio_stream)
	{
		AVCodecContext* c = get_codec_from_stream(audio_stream);
		AUD_DeviceSpecs specs;
		specs.channels = c->channels;
		specs.format = AUD_FORMAT_S16;
		specs.rate = rd->ffcodecdata.audio_mixrate;
		audio_mixdown_device = sound_mixdown(scene, specs, rd->sfra, rd->ffcodecdata.audio_volume);
	}

	return success;
}

void end_ffmpeg(void);

static void write_audio_frames(double to_pts)
{
	int finished = 0;

	while (audio_stream && !finished) {
		if((audio_time >= to_pts) ||
		   (write_audio_frame())) {
			finished = 1;
		}
	}
}

int append_ffmpeg(RenderData *rd, int frame, int *pixels, int rectx, int recty, ReportList *reports) 
{
	AVFrame* avframe;
	int success = 1;

	fprintf(stderr, "Writing frame %i, "
		"render width=%d, render height=%d\n", frame,
		rectx, recty);

// why is this done before writing the video frame and again at end_ffmpeg?
//	write_audio_frames(frame / (((double)rd->frs_sec) / rd->frs_sec_base));

	if(video_stream)
	{
		avframe= generate_video_frame((unsigned char*) pixels, reports);
		success= (avframe && write_video_frame(rd, avframe, reports));

		if (ffmpeg_autosplit) {
			if (url_ftell(OUTFILE_PB) > FFMPEG_AUTOSPLIT_SIZE) {
				end_ffmpeg();
				ffmpeg_autosplit_count++;
				success &= start_ffmpeg_impl(rd, rectx, recty, reports);
			}
		}
	}

	write_audio_frames(frame / (((double)rd->frs_sec) / rd->frs_sec_base));

	return success;
}


void end_ffmpeg(void)
{
	int i;
	
	fprintf(stderr, "Closing ffmpeg...\n");

/*	if (audio_stream) { SEE UPPER
		write_audio_frames();
	}*/

	if(audio_mixdown_device)
	{
		AUD_closeReadDevice(audio_mixdown_device);
		audio_mixdown_device = 0;
	}
	
	if (outfile) {
		av_write_trailer(outfile);
	}
	
	/* Close the video codec */

	if (video_stream && get_codec_from_stream(video_stream)) {
		avcodec_close(get_codec_from_stream(video_stream));
		video_stream = 0;
		printf("zero video stream %p\n", video_stream);
	}

	
	/* Close the output file */
	if (outfile) {
		for (i = 0; i < outfile->nb_streams; i++) {
			if (&outfile->streams[i]) {
				av_freep(&outfile->streams[i]);
			}
		}
	}
	/* free the temp buffer */
	if (current_frame) {
		delete_picture(current_frame);
		current_frame = 0;
	}
	if (outfile && outfile->oformat) {
		if (!(outfile->oformat->flags & AVFMT_NOFILE)) {
			url_fclose(OUTFILE_PB);
		}
	}
	if (outfile) {
		av_free(outfile);
		outfile = 0;
	}
	if (video_buffer) {
		MEM_freeN(video_buffer);
		video_buffer = 0;
	}
	if (audio_output_buffer) {
		MEM_freeN(audio_output_buffer);
		audio_output_buffer = 0;
	}
	if (audio_input_buffer) {
		MEM_freeN(audio_input_buffer);
		audio_input_buffer = 0;
	}

	if (img_convert_ctx) {
		sws_freeContext(img_convert_ctx);
		img_convert_ctx = 0;
	}
}

/* properties */

void ffmpeg_property_del(RenderData *rd, void *type, void *prop_)
{
	struct IDProperty *prop = (struct IDProperty *) prop_;
	IDProperty * group;
	
	if (!rd->ffcodecdata.properties) {
		return;
	}

	group = IDP_GetPropertyFromGroup(
		rd->ffcodecdata.properties, (char*) type);
	if (group && prop) {
		IDP_RemFromGroup(group, prop);
		IDP_FreeProperty(prop);
		MEM_freeN(prop);
	}
}

IDProperty *ffmpeg_property_add(RenderData *rd, char * type, int opt_index, int parent_index)
{
	AVCodecContext c;
	const AVOption * o;
	const AVOption * parent;
	IDProperty * group;
	IDProperty * prop;
	IDPropertyTemplate val;
	int idp_type;
	char name[256];

	avcodec_get_context_defaults(&c);

	o = c.av_class->option + opt_index;
	parent = c.av_class->option + parent_index;

	if (!rd->ffcodecdata.properties) {
		IDPropertyTemplate val;

		rd->ffcodecdata.properties 
			= IDP_New(IDP_GROUP, val, "ffmpeg"); 
	}

	group = IDP_GetPropertyFromGroup(
		rd->ffcodecdata.properties, (char*) type);
	
	if (!group) {
		IDPropertyTemplate val;
		
		group = IDP_New(IDP_GROUP, val, (char*) type); 
		IDP_AddToGroup(rd->ffcodecdata.properties, group);
	}

	if (parent_index) {
		sprintf(name, "%s:%s", parent->name, o->name);
	} else {
		strcpy(name, o->name);
	}

	fprintf(stderr, "ffmpeg_property_add: %s %d %d %s\n",
		type, parent_index, opt_index, name);

	prop = IDP_GetPropertyFromGroup(group, name);
	if (prop) {
		return prop;
	}

	switch (o->type) {
	case FF_OPT_TYPE_INT:
	case FF_OPT_TYPE_INT64:
		val.i = o->default_val;
		idp_type = IDP_INT;
		break;
	case FF_OPT_TYPE_DOUBLE:
	case FF_OPT_TYPE_FLOAT:
		val.f = o->default_val;
		idp_type = IDP_FLOAT;
		break;
	case FF_OPT_TYPE_STRING:
		val.str = "                                                                               ";
		idp_type = IDP_STRING;
		break;
	case FF_OPT_TYPE_CONST:
		val.i = 1;
		idp_type = IDP_INT;
		break;
	default:
		return NULL;
	}
	prop = IDP_New(idp_type, val, name);
	IDP_AddToGroup(group, prop);
	return prop;
}

/* not all versions of ffmpeg include that, so here we go ... */

static const AVOption *my_av_find_opt(void *v, const char *name, 
				      const char *unit, int mask, int flags){
	AVClass *c= *(AVClass**)v; 
	const AVOption *o= c->option;

	for(;o && o->name; o++){
		if(!strcmp(o->name, name) && 
		   (!unit || (o->unit && !strcmp(o->unit, unit))) && 
		   (o->flags & mask) == flags )
			return o;
	}
	return NULL;
}

int ffmpeg_property_add_string(RenderData *rd, const char * type, const char * str)
{
	AVCodecContext c;
	const AVOption * o = 0;
	const AVOption * p = 0;
	char name_[128];
	char * name;
	char * param;
	IDProperty * prop;
	
	avcodec_get_context_defaults(&c);

	strncpy(name_, str, 128);

	name = name_;
	while (*name == ' ') name++;

	param = strchr(name, ':');

	if (!param) {
		param = strchr(name, ' ');
	}
	if (param) {
		*param++ = 0;
		while (*param == ' ') param++;
	}
	
	o = my_av_find_opt(&c, name, NULL, 0, 0);	
	if (!o) {
		return 0;
	}
	if (param && o->type == FF_OPT_TYPE_CONST) {
		return 0;
	}
	if (param && o->type != FF_OPT_TYPE_CONST && o->unit) {
		p = my_av_find_opt(&c, param, o->unit, 0, 0);	
		prop = ffmpeg_property_add(rd,
			(char*) type, p - c.av_class->option, 
			o - c.av_class->option);
	} else {
		prop = ffmpeg_property_add(rd,
			(char*) type, o - c.av_class->option, 0);
	}
		

	if (!prop) {
		return 0;
	}

	if (param && !p) {
		switch (prop->type) {
		case IDP_INT:
			IDP_Int(prop) = atoi(param);
			break;
		case IDP_FLOAT:
			IDP_Float(prop) = atof(param);
			break;
		case IDP_STRING:
			strncpy(IDP_String(prop), param, prop->len);
			break;
		}
	}
	return 1;
}

void ffmpeg_set_preset(RenderData *rd, int preset)
{
	int isntsc = (rd->frs_sec != 25);

	switch (preset) {
	case FFMPEG_PRESET_VCD:
		rd->ffcodecdata.type = FFMPEG_MPEG1;
		rd->ffcodecdata.video_bitrate = 1150;
		rd->xsch = 352;
		rd->ysch = isntsc ? 240 : 288;
		rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
		rd->ffcodecdata.rc_max_rate = 1150;
		rd->ffcodecdata.rc_min_rate = 1150;
		rd->ffcodecdata.rc_buffer_size = 40*8;
		rd->ffcodecdata.mux_packet_size = 2324;
		rd->ffcodecdata.mux_rate = 2352 * 75 * 8;
		break;

	case FFMPEG_PRESET_SVCD:
		rd->ffcodecdata.type = FFMPEG_MPEG2;
		rd->ffcodecdata.video_bitrate = 2040;
		rd->xsch = 480;
		rd->ysch = isntsc ? 480 : 576;
		rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
		rd->ffcodecdata.rc_max_rate = 2516;
		rd->ffcodecdata.rc_min_rate = 0;
		rd->ffcodecdata.rc_buffer_size = 224*8;
		rd->ffcodecdata.mux_packet_size = 2324;
		rd->ffcodecdata.mux_rate = 0;
		break;

	case FFMPEG_PRESET_DVD:
		rd->ffcodecdata.type = FFMPEG_MPEG2;
		rd->ffcodecdata.video_bitrate = 6000;
		rd->xsch = 720;
		rd->ysch = isntsc ? 480 : 576;
		rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
		rd->ffcodecdata.rc_max_rate = 9000;
		rd->ffcodecdata.rc_min_rate = 0;
		rd->ffcodecdata.rc_buffer_size = 224*8;
		rd->ffcodecdata.mux_packet_size = 2048;
		rd->ffcodecdata.mux_rate = 10080000;
		break;

	case FFMPEG_PRESET_DV:
		rd->ffcodecdata.type = FFMPEG_DV;
		rd->xsch = 720;
		rd->ysch = isntsc ? 480 : 576;
		break;

	case FFMPEG_PRESET_H264:
		rd->ffcodecdata.type = FFMPEG_AVI;
		rd->ffcodecdata.codec = CODEC_ID_H264;
		rd->ffcodecdata.video_bitrate = 6000;
		rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
		rd->ffcodecdata.rc_max_rate = 9000;
		rd->ffcodecdata.rc_min_rate = 0;
		rd->ffcodecdata.rc_buffer_size = 224*8;
		rd->ffcodecdata.mux_packet_size = 2048;
		rd->ffcodecdata.mux_rate = 10080000;

		ffmpeg_property_add_string(rd, "video", "coder:vlc");
		ffmpeg_property_add_string(rd, "video", "flags:loop");
		ffmpeg_property_add_string(rd, "video", "cmp:chroma");
		ffmpeg_property_add_string(rd, "video", "partitions:parti4x4");
		ffmpeg_property_add_string(rd, "video", "partitions:partp8x8");
		ffmpeg_property_add_string(rd, "video", "partitions:partb8x8");
		ffmpeg_property_add_string(rd, "video", "me:hex");
		ffmpeg_property_add_string(rd, "video", "subq:5");
		ffmpeg_property_add_string(rd, "video", "me_range:16");
		ffmpeg_property_add_string(rd, "video", "keyint_min:25");
		ffmpeg_property_add_string(rd, "video", "sc_threshold:40");
		ffmpeg_property_add_string(rd, "video", "i_qfactor:0.71");
		ffmpeg_property_add_string(rd, "video", "b_strategy:1");

		break;

	case FFMPEG_PRESET_THEORA:
	case FFMPEG_PRESET_XVID:
		if(preset == FFMPEG_PRESET_XVID) {
			rd->ffcodecdata.type = FFMPEG_AVI;
			rd->ffcodecdata.codec = CODEC_ID_XVID;
		}
		else if(preset == FFMPEG_PRESET_THEORA) {
			rd->ffcodecdata.type = FFMPEG_OGG; // XXX broken
			rd->ffcodecdata.codec = CODEC_ID_THEORA;
		}

		rd->ffcodecdata.video_bitrate = 6000;
		rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
		rd->ffcodecdata.rc_max_rate = 9000;
		rd->ffcodecdata.rc_min_rate = 0;
		rd->ffcodecdata.rc_buffer_size = 224*8;
		rd->ffcodecdata.mux_packet_size = 2048;
		rd->ffcodecdata.mux_rate = 10080000;
		break;

	}
}

void ffmpeg_verify_image_type(RenderData *rd)
{
	int audio= 0;

	if(rd->imtype == R_FFMPEG) {
		if(rd->ffcodecdata.type <= 0 ||
		   rd->ffcodecdata.codec <= 0 ||
		   rd->ffcodecdata.audio_codec <= 0 ||
		   rd->ffcodecdata.video_bitrate <= 1) {

			rd->ffcodecdata.codec = CODEC_ID_MPEG2VIDEO;
			ffmpeg_set_preset(rd, FFMPEG_PRESET_DVD);
		}

		audio= 1;
	}
	else if(rd->imtype == R_H264) {
		if(rd->ffcodecdata.codec != CODEC_ID_H264) {
			ffmpeg_set_preset(rd, FFMPEG_PRESET_H264);
			audio= 1;
		}
	}
	else if(rd->imtype == R_XVID) {
		if(rd->ffcodecdata.codec != CODEC_ID_XVID) {
			ffmpeg_set_preset(rd, FFMPEG_PRESET_XVID);
			audio= 1;
		}
	}
	else if(rd->imtype == R_THEORA) {
		if(rd->ffcodecdata.codec != CODEC_ID_THEORA) {
			ffmpeg_set_preset(rd, FFMPEG_PRESET_THEORA);
			audio= 1;
		}
	}

	if(audio && rd->ffcodecdata.audio_codec < 0) {
		rd->ffcodecdata.audio_codec = CODEC_ID_NONE;
		rd->ffcodecdata.audio_bitrate = 128;
	}
}

#endif

