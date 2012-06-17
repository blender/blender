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
 * Contributor(s):
 *
 * Partial Copyright (c) 2006 Peter Schlaile
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/writeffmpeg.c
 *  \ingroup bke
 */

#ifdef WITH_FFMPEG
#include <string.h>
#include <stdio.h>

#if defined(_WIN32) && defined(DEBUG) && !defined(__MINGW32__) && !defined(__CYGWIN__)
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

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_blenlib.h"

#ifdef WITH_AUDASPACE
#  include "AUD_C-API.h"
#endif

#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_sound.h"
#include "BKE_writeffmpeg.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ffmpeg_compat.h"

extern void do_init_ffmpeg(void);

static int ffmpeg_type = 0;
static int ffmpeg_codec = CODEC_ID_MPEG4;
static int ffmpeg_audio_codec = CODEC_ID_NONE;
static int ffmpeg_video_bitrate = 1150;
static int ffmpeg_audio_bitrate = 128;
static int ffmpeg_gop_size = 12;
static int ffmpeg_autosplit = 0;
static int ffmpeg_autosplit_count = 0;

static AVFormatContext *outfile = 0;
static AVStream *video_stream = 0;
static AVStream *audio_stream = 0;
static AVFrame *current_frame = 0;
static struct SwsContext *img_convert_ctx = 0;

static uint8_t *video_buffer = 0;
static int video_buffersize = 0;

static uint8_t *audio_input_buffer = 0;
static int audio_input_samples = 0;
static uint8_t *audio_output_buffer = 0;
static int audio_outbuf_size = 0;
static double audio_time = 0.0f;

#ifdef WITH_AUDASPACE
static AUD_Device *audio_mixdown_device = 0;
#endif

#define FFMPEG_AUTOSPLIT_SIZE 2000000000

/* Delete a picture buffer */

static void delete_picture(AVFrame *f)
{
	if (f) {
		if (f->data[0]) MEM_freeN(f->data[0]);
		av_free(f);
	}
}

#ifdef WITH_AUDASPACE
static int write_audio_frame(void) 
{
	AVCodecContext *c = NULL;
	AVPacket pkt;

	c = audio_stream->codec;

	av_init_packet(&pkt);
	pkt.size = 0;

	AUD_readDevice(audio_mixdown_device, audio_input_buffer, audio_input_samples);
	audio_time += (double) audio_input_samples / (double) c->sample_rate;

	pkt.size = avcodec_encode_audio(c, audio_output_buffer,
	                                audio_outbuf_size,
	                                (short *)audio_input_buffer);

	if (pkt.size < 0) {
		// XXX error("Error writing audio packet");
		return -1;
	}

	pkt.data = audio_output_buffer;

	if (c->coded_frame && c->coded_frame->pts != AV_NOPTS_VALUE) {
		pkt.pts = av_rescale_q(c->coded_frame->pts,
		                       c->time_base, audio_stream->time_base);
		fprintf(stderr, "Audio Frame PTS: %d\n", (int)pkt.pts);
	}

	pkt.stream_index = audio_stream->index;

	pkt.flags |= AV_PKT_FLAG_KEY;

	if (av_interleaved_write_frame(outfile, &pkt) != 0) {
		fprintf(stderr, "Error writing audio packet!\n");
		return -1;
	}
	return 0;
}
#endif // #ifdef WITH_AUDASPACE

/* Allocate a temporary frame */
static AVFrame *alloc_picture(int pix_fmt, int width, int height)
{
	AVFrame *f;
	uint8_t *buf;
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
	avpicture_fill((AVPicture *)f, buf, pix_fmt, width, height);
	return f;
}

/* Get the correct file extensions for the requested format,
 * first is always desired guess_format parameter */
static const char **get_file_extensions(int format)
{
	switch (format) {
		case FFMPEG_DV: {
			static const char *rv[] = { ".dv", NULL };
			return rv;
		}
		case FFMPEG_MPEG1: {
			static const char *rv[] = { ".mpg", ".mpeg", NULL };
			return rv;
		}
		case FFMPEG_MPEG2: {
			static const char *rv[] = { ".dvd", ".vob", ".mpg", ".mpeg",
				                        NULL };
			return rv;
		}
		case FFMPEG_MPEG4: {
			static const char *rv[] = { ".mp4", ".mpg", ".mpeg", NULL };
			return rv;
		}
		case FFMPEG_AVI: {
			static const char *rv[] = { ".avi", NULL };
			return rv;
		}
		case FFMPEG_MOV: {
			static const char *rv[] = { ".mov", NULL };
			return rv;
		}
		case FFMPEG_H264: {
			/* FIXME: avi for now... */
			static const char *rv[] = { ".avi", NULL };
			return rv;
		}

		case FFMPEG_XVID: {
			/* FIXME: avi for now... */
			static const char *rv[] = { ".avi", NULL };
			return rv;
		}
		case FFMPEG_FLV: {
			static const char *rv[] = { ".flv", NULL };
			return rv;
		}
		case FFMPEG_MKV: {
			static const char *rv[] = { ".mkv", NULL };
			return rv;
		}
		case FFMPEG_OGG: {
			static const char *rv[] = { ".ogg", ".ogv", NULL };
			return rv;
		}
		case FFMPEG_MP3: {
			static const char *rv[] = { ".mp3", NULL };
			return rv;
		}
		case FFMPEG_WAV: {
			static const char *rv[] = { ".wav", NULL };
			return rv;
		}
		default:
			return NULL;
	}
}

/* Write a frame to the output file */
static int write_video_frame(RenderData *rd, int cfra, AVFrame *frame, ReportList *reports)
{
	int outsize = 0;
	int ret, success = 1;
	AVCodecContext *c = video_stream->codec;

	frame->pts = cfra;

	if (rd->mode & R_FIELDS) {
		frame->top_field_first = ((rd->mode & R_ODDFIELD) != 0);
	}

	outsize = avcodec_encode_video(c, video_buffer, video_buffersize, 
	                               frame);

	if (outsize > 0) {
		AVPacket packet;
		av_init_packet(&packet);

		if (c->coded_frame->pts != AV_NOPTS_VALUE) {
			packet.pts = av_rescale_q(c->coded_frame->pts,
			                          c->time_base,
			                          video_stream->time_base);
			fprintf(stderr, "Video Frame PTS: %d\n", (int)packet.pts);
		}
		else {
			fprintf(stderr, "Video Frame PTS: not set\n");
		}
		if (c->coded_frame->key_frame)
			packet.flags |= AV_PKT_FLAG_KEY;
		packet.stream_index = video_stream->index;
		packet.data = video_buffer;
		packet.size = outsize;
		ret = av_interleaved_write_frame(outfile, &packet);
		success = (ret == 0);
	}
	else if (outsize < 0) {
		success = 0;
	}

	if (!success)
		BKE_report(reports, RPT_ERROR, "Error writing frame.");

	return success;
}

/* read and encode a frame of audio from the buffer */
static AVFrame *generate_video_frame(uint8_t *pixels, ReportList *reports)
{
	uint8_t *rendered_frame;

	AVCodecContext *c = video_stream->codec;
	int width = c->width;
	int height = c->height;
	AVFrame *rgb_frame;

	if (c->pix_fmt != PIX_FMT_BGR32) {
		rgb_frame = alloc_picture(PIX_FMT_BGR32, width, height);
		if (!rgb_frame) {
			BKE_report(reports, RPT_ERROR, "Couldn't allocate temporary frame.");
			return NULL;
		}
	}
	else {
		rgb_frame = current_frame;
	}

	rendered_frame = pixels;

	/* Do RGBA-conversion and flipping in one step depending
	 * on CPU-Endianess */

	if (ENDIAN_ORDER == L_ENDIAN) {
		int y;
		for (y = 0; y < height; y++) {
			uint8_t *target = rgb_frame->data[0] + width * 4 * (height - y - 1);
			uint8_t *src = rendered_frame + width * 4 * y;
			uint8_t *end = src + width * 4;
			while (src != end) {
				target[3] = src[3];
				target[2] = src[2];
				target[1] = src[1];
				target[0] = src[0];

				target += 4;
				src += 4;
			}
		}
	}
	else {
		int y;
		for (y = 0; y < height; y++) {
			uint8_t *target = rgb_frame->data[0] + width * 4 * (height - y - 1);
			uint8_t *src = rendered_frame + width * 4 * y;
			uint8_t *end = src + width * 4;
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
		sws_scale(img_convert_ctx, (const uint8_t *const *) rgb_frame->data,
		          rgb_frame->linesize, 0, c->height,
		          current_frame->data, current_frame->linesize);
		delete_picture(rgb_frame);
	}
	return current_frame;
}

static void set_ffmpeg_property_option(AVCodecContext *c, IDProperty *prop)
{
	char name[128];
	char *param;
	const AVOption *rv = NULL;

	fprintf(stderr, "FFMPEG expert option: %s: ", prop->name);

	BLI_strncpy(name, prop->name, sizeof(name));

	param = strchr(name, ':');

	if (param) {
		*param++ = 0;
	}

	switch (prop->type) {
		case IDP_STRING:
			fprintf(stderr, "%s.\n", IDP_String(prop));
			av_set_string3(c, prop->name, IDP_String(prop), 1, &rv);
			break;
		case IDP_FLOAT:
			fprintf(stderr, "%g.\n", IDP_Float(prop));
			rv = av_set_double(c, prop->name, IDP_Float(prop));
			break;
		case IDP_INT:
			fprintf(stderr, "%d.\n", IDP_Int(prop));

			if (param) {
				if (IDP_Int(prop)) {
					av_set_string3(c, name, param, 1, &rv);
				}
				else {
					return;
				}
			}
			else {
				rv = av_set_int(c, prop->name, IDP_Int(prop));
			}
			break;
	}

	if (!rv) {
		fprintf(stderr, "ffmpeg-option not supported: %s! Skipping.\n",
		        prop->name);
	}
}

static int ffmpeg_proprty_valid(AVCodecContext *c, const char *prop_name, IDProperty *curr)
{
	int valid = 1;

	if (strcmp(prop_name, "video") == 0) {
		if (strcmp(curr->name, "bf") == 0) {
			/* flash codec doesn't support b frames */
			valid &= c->codec_id != CODEC_ID_FLV1;
		}
	}

	return valid;
}

static void set_ffmpeg_properties(RenderData *rd, AVCodecContext *c, const char *prop_name)
{
	IDProperty *prop;
	void *iter;
	IDProperty *curr;

	if (!rd->ffcodecdata.properties) {
		return;
	}
	
	prop = IDP_GetPropertyFromGroup(rd->ffcodecdata.properties, prop_name);
	if (!prop) {
		return;
	}

	iter = IDP_GetGroupIterator(prop);

	while ((curr = IDP_GroupIterNext(iter)) != NULL) {
		if (ffmpeg_proprty_valid(c, prop_name, curr))
			set_ffmpeg_property_option(c, curr);
	}
}

/* prepare a video stream for the output file */

static AVStream *alloc_video_stream(RenderData *rd, int codec_id, AVFormatContext *of,
                                    int rectx, int recty)
{
	AVStream *st;
	AVCodecContext *c;
	AVCodec *codec;
	st = av_new_stream(of, 0);
	if (!st) return NULL;

	/* Set up the codec context */
	
	c = st->codec;
	c->codec_id = codec_id;
	c->codec_type = AVMEDIA_TYPE_VIDEO;


	/* Get some values from the current render settings */
	
	c->width = rectx;
	c->height = recty;

	/* FIXME: Really bad hack (tm) for NTSC support */
	if (ffmpeg_type == FFMPEG_DV && rd->frs_sec != 25) {
		c->time_base.den = 2997;
		c->time_base.num = 100;
	}
	else if ((double) ((int) rd->frs_sec_base) ==
	         rd->frs_sec_base)
	{
		c->time_base.den = rd->frs_sec;
		c->time_base.num = (int) rd->frs_sec_base;
	}
	else {
		c->time_base.den = rd->frs_sec * 100000;
		c->time_base.num = ((double) rd->frs_sec_base) * 100000;
	}
	
	c->gop_size = ffmpeg_gop_size;
	c->bit_rate = ffmpeg_video_bitrate * 1000;
	c->rc_max_rate = rd->ffcodecdata.rc_max_rate * 1000;
	c->rc_min_rate = rd->ffcodecdata.rc_min_rate * 1000;
	c->rc_buffer_size = rd->ffcodecdata.rc_buffer_size * 1024;
	c->rc_initial_buffer_occupancy = rd->ffcodecdata.rc_buffer_size * 3 / 4;
	c->rc_buffer_aggressivity = 1.0;
	c->me_method = ME_EPZS;
	
	codec = avcodec_find_encoder(c->codec_id);
	if (!codec) return NULL;
	
	/* Be sure to use the correct pixel format(e.g. RGB, YUV) */

	if (codec->pix_fmts) {
		c->pix_fmt = codec->pix_fmts[0];
	}
	else {
		/* makes HuffYUV happy ... */
		c->pix_fmt = PIX_FMT_YUV422P;
	}

	if (ffmpeg_type == FFMPEG_XVID) {
		/* arghhhh ... */
		c->pix_fmt = PIX_FMT_YUV420P;
		c->codec_tag = (('D' << 24) + ('I' << 16) + ('V' << 8) + 'X');
	}

	if (codec_id == CODEC_ID_H264) {
		/* correct wrong default ffmpeg param which crash x264 */
		c->qmin = 10;
		c->qmax = 51;
	}
	
	// Keep lossless encodes in the RGB domain.
	if (codec_id == CODEC_ID_HUFFYUV) {
		/* HUFFYUV was PIX_FMT_YUV422P before */
		c->pix_fmt = PIX_FMT_RGB32;
	}

	if (codec_id == CODEC_ID_FFV1) {
#ifdef FFMPEG_FFV1_ALPHA_SUPPORTED
		if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
			c->pix_fmt = PIX_FMT_RGB32;
		}
		else {
			c->pix_fmt = PIX_FMT_BGR0;
		}
#else
		c->pix_fmt = PIX_FMT_RGB32;
#endif
	}

	if (codec_id == CODEC_ID_QTRLE) {
		if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
			c->pix_fmt = PIX_FMT_ARGB;
		}
	}

	if ((of->oformat->flags & AVFMT_GLOBALHEADER)
//		|| !strcmp(of->oformat->name, "mp4")
//	    || !strcmp(of->oformat->name, "mov")
//	    || !strcmp(of->oformat->name, "3gp")
	    )
	{
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

	if (codec_id == CODEC_ID_QTRLE) {
		// normally it should be enough to have buffer with actual image size,
		// but some codecs like QTRLE might store extra information in this buffer,
		// so it should be a way larger

		// maximum video buffer size is 6-bytes per pixel, plus DPX header size (1664)
		// (from FFmpeg sources)
		int size = c->width * c->height;
		video_buffersize = 7 * size + 10000;
	}
	else
		video_buffersize = avpicture_get_size(c->pix_fmt, c->width, c->height);

	video_buffer = (uint8_t *)MEM_mallocN(video_buffersize * sizeof(uint8_t),
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

static AVStream *alloc_audio_stream(RenderData *rd, int codec_id, AVFormatContext *of)
{
	AVStream *st;
	AVCodecContext *c;
	AVCodec *codec;

	st = av_new_stream(of, 1);
	if (!st) return NULL;

	c = st->codec;
	c->codec_id = codec_id;
	c->codec_type = AVMEDIA_TYPE_AUDIO;

	c->sample_rate = rd->ffcodecdata.audio_mixrate;
	c->bit_rate = ffmpeg_audio_bitrate * 1000;
	c->sample_fmt = SAMPLE_FMT_S16;
	c->channels = rd->ffcodecdata.audio_channels;
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

	/* need to prevent floating point exception when using vorbis audio codec,
	 * initialize this value in the same way as it's done in FFmpeg iteslf (sergey) */
	st->codec->time_base.num = 1;
	st->codec->time_base.den = st->codec->sample_rate;

	audio_outbuf_size = FF_MIN_BUFFER_SIZE;

	if ((c->codec_id >= CODEC_ID_PCM_S16LE) && (c->codec_id <= CODEC_ID_PCM_DVD))
		audio_input_samples = audio_outbuf_size * 8 / c->bits_per_coded_sample / c->channels;
	else {
		audio_input_samples = c->frame_size;
		if (c->frame_size * c->channels * sizeof(int16_t) * 4 > audio_outbuf_size)
			audio_outbuf_size = c->frame_size * c->channels * sizeof(int16_t) * 4;
	}

	audio_output_buffer = (uint8_t *)av_malloc(
	    audio_outbuf_size);

	audio_input_buffer = (uint8_t *)av_malloc(
	    audio_input_samples * c->channels * sizeof(int16_t));

	audio_time = 0.0f;

	return st;
}
/* essential functions -- start, append, end */

static int start_ffmpeg_impl(struct RenderData *rd, int rectx, int recty, ReportList *reports)
{
	/* Handle to the output file */
	AVFormatContext *of;
	AVOutputFormat *fmt;
	char name[256];
	const char **exts;

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
	BKE_ffmpeg_filepath_get(name, rd);
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
	fmt = av_guess_format(NULL, exts[0], NULL);
	if (!fmt) {
		BKE_report(reports, RPT_ERROR, "No valid formats found.");
		return 0;
	}

	of = avformat_alloc_context();
	if (!of) {
		BKE_report(reports, RPT_ERROR, "Error opening output file");
		return 0;
	}
	
	of->oformat = fmt;
	of->packet_size = rd->ffcodecdata.mux_packet_size;
	if (ffmpeg_audio_codec != CODEC_ID_NONE) {
		of->mux_rate = rd->ffcodecdata.mux_rate;
	}
	else {
		of->mux_rate = 0;
	}

	of->preload = (int)(0.5 * AV_TIME_BASE);
	of->max_delay = (int)(0.7 * AV_TIME_BASE);

	fmt->audio_codec = ffmpeg_audio_codec;

	BLI_snprintf(of->filename, sizeof(of->filename), "%s", name);
	/* set the codec to the user's selection */
	switch (ffmpeg_type) {
		case FFMPEG_AVI:
		case FFMPEG_MOV:
		case FFMPEG_MKV:
			fmt->video_codec = ffmpeg_codec;
			break;
		case FFMPEG_OGG:
			fmt->video_codec = CODEC_ID_THEORA;
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
			fmt->video_codec = CODEC_ID_MPEG4;
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
		if (ffmpeg_audio_codec != CODEC_ID_NONE && rd->ffcodecdata.audio_mixrate != 48000 && rd->ffcodecdata.audio_channels != 2) {
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
		if (avio_open(&of->pb, name, AVIO_FLAG_WRITE) < 0) {
			BKE_report(reports, RPT_ERROR, "Could not open file for writing.");
			return 0;
		}
	}

	if (av_write_header(of) < 0) {
		BKE_report(reports, RPT_ERROR, "Could not initialize streams. Probably unsupported codec combination.");
		return 0;
	}

	outfile = of;
	av_dump_format(of, 0, name, 1);

	return 1;
}

/**
 * Writes any delayed frames in the encoder. This function is called before 
 * closing the encoder.
 *
 * <p>
 * Since an encoder may use both past and future frames to predict 
 * inter-frames (H.264 B-frames, for example), it can output the frames 
 * in a different order from the one it was given.
 * For example, when sending frames 1, 2, 3, 4 to the encoder, it may write
 * them in the order 1, 4, 2, 3 - first the two frames used for predition, 
 * and then the bidirectionally-predicted frames. What this means in practice 
 * is that the encoder may not immediately produce one output frame for each 
 * input frame. These delayed frames must be flushed before we close the 
 * stream. We do this by calling avcodec_encode_video with NULL for the last 
 * parameter.
 * </p>
 */
void flush_ffmpeg(void)
{
	int outsize = 0;
	int ret = 0;
	
	AVCodecContext *c = video_stream->codec;
	/* get the delayed frames */
	while (1) {
		AVPacket packet;
		av_init_packet(&packet);
		
		outsize = avcodec_encode_video(c, video_buffer, video_buffersize, NULL);
		if (outsize < 0) {
			fprintf(stderr, "Error encoding delayed frame %d\n", outsize);
			break;
		}
		if (outsize == 0) {
			break;
		}
		if (c->coded_frame->pts != AV_NOPTS_VALUE) {
			packet.pts = av_rescale_q(c->coded_frame->pts,
			                          c->time_base,
			                          video_stream->time_base);
			fprintf(stderr, "Video Frame PTS: %d\n", (int)packet.pts);
		}
		else {
			fprintf(stderr, "Video Frame PTS: not set\n");
		}
		if (c->coded_frame->key_frame) {
			packet.flags |= AV_PKT_FLAG_KEY;
		}
		packet.stream_index = video_stream->index;
		packet.data = video_buffer;
		packet.size = outsize;
		ret = av_interleaved_write_frame(outfile, &packet);
		if (ret != 0) {
			fprintf(stderr, "Error writing delayed frame %d\n", ret);
			break;
		}
	}
	avcodec_flush_buffers(video_stream->codec);
}

/* **********************************************************************
 * * public interface
 * ********************************************************************** */

/* Get the output filename-- similar to the other output formats */
void BKE_ffmpeg_filepath_get(char *string, RenderData *rd)
{
	char autosplit[20];

	const char **exts = get_file_extensions(rd->ffcodecdata.type);
	const char **fe = exts;

	if (!string || !exts) return;

	strcpy(string, rd->pic);
	BLI_path_abs(string, G.main->name);

	BLI_make_existing_file(string);

	autosplit[0] = 0;

	if ((rd->ffcodecdata.flags & FFMPEG_AUTOSPLIT_OUTPUT) != 0) {
		sprintf(autosplit, "_%03d", ffmpeg_autosplit_count);
	}

	while (*fe) {
		if (BLI_strcasecmp(string + strlen(string) - strlen(*fe), 
		                   *fe) == 0)
		{
			break;
		}
		fe++;
	}

	if (!*fe) {
		strcat(string, autosplit);

		BLI_path_frame_range(string, rd->sfra, rd->efra, 4);
		strcat(string, *exts);
	}
	else {
		*(string + strlen(string) - strlen(*fe)) = 0;
		strcat(string, autosplit);
		strcat(string, *fe);
	}
}

int BKE_ffmpeg_start(struct Scene *scene, RenderData *rd, int rectx, int recty, ReportList *reports)
{
	int success;

	ffmpeg_autosplit_count = 0;

	success = start_ffmpeg_impl(rd, rectx, recty, reports);
#ifdef WITH_AUDASPACE
	if (audio_stream) {
		AVCodecContext *c = audio_stream->codec;
		AUD_DeviceSpecs specs;
		specs.channels = c->channels;
		specs.format = AUD_FORMAT_S16;
		specs.rate = rd->ffcodecdata.audio_mixrate;
		audio_mixdown_device = sound_mixdown(scene, specs, rd->sfra, rd->ffcodecdata.audio_volume);
#ifdef FFMPEG_CODEC_TIME_BASE
		c->time_base.den = specs.rate;
		c->time_base.num = 1;
#endif
	}
#endif
	return success;
}

void BKE_ffmpeg_end(void);

#ifdef WITH_AUDASPACE
static void write_audio_frames(double to_pts)
{
	int finished = 0;

	while (audio_stream && !finished) {
		if ((audio_time >= to_pts) ||
		    (write_audio_frame()))
		{
			finished = 1;
		}
	}
}
#endif

int BKE_ffmpeg_append(RenderData *rd, int start_frame, int frame, int *pixels, int rectx, int recty, ReportList *reports)
{
	AVFrame *avframe;
	int success = 1;

	fprintf(stderr, "Writing frame %i, "
	        "render width=%d, render height=%d\n", frame,
	        rectx, recty);

// why is this done before writing the video frame and again at end_ffmpeg?
//	write_audio_frames(frame / (((double)rd->frs_sec) / rd->frs_sec_base));

	if (video_stream) {
		avframe = generate_video_frame((unsigned char *) pixels, reports);
		success = (avframe && write_video_frame(rd, frame - start_frame, avframe, reports));

		if (ffmpeg_autosplit) {
			if (avio_tell(outfile->pb) > FFMPEG_AUTOSPLIT_SIZE) {
				BKE_ffmpeg_end();
				ffmpeg_autosplit_count++;
				success &= start_ffmpeg_impl(rd, rectx, recty, reports);
			}
		}
	}

#ifdef WITH_AUDASPACE
	write_audio_frames((frame - rd->sfra) / (((double)rd->frs_sec) / rd->frs_sec_base));
#endif
	return success;
}

void BKE_ffmpeg_end(void)
{
	unsigned int i;
	
	fprintf(stderr, "Closing ffmpeg...\n");

#if 0
	if (audio_stream) { /* SEE UPPER */
		write_audio_frames();
	}
#endif

#ifdef WITH_AUDASPACE
	if (audio_mixdown_device) {
		AUD_closeReadDevice(audio_mixdown_device);
		audio_mixdown_device = 0;
	}
#endif

	if (video_stream && video_stream->codec) {
		fprintf(stderr, "Flushing delayed frames...\n");
		flush_ffmpeg();
	}
	
	if (outfile) {
		av_write_trailer(outfile);
	}
	
	/* Close the video codec */

	if (video_stream && video_stream->codec) {
		avcodec_close(video_stream->codec);
		printf("zero video stream %p\n", video_stream);
		video_stream = 0;
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
			avio_close(outfile->pb);
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
		av_free(audio_output_buffer);
		audio_output_buffer = 0;
	}
	if (audio_input_buffer) {
		av_free(audio_input_buffer);
		audio_input_buffer = 0;
	}

	if (img_convert_ctx) {
		sws_freeContext(img_convert_ctx);
		img_convert_ctx = 0;
	}
}

/* properties */

void BKE_ffmpeg_property_del(RenderData *rd, void *type, void *prop_)
{
	struct IDProperty *prop = (struct IDProperty *) prop_;
	IDProperty *group;
	
	if (!rd->ffcodecdata.properties) {
		return;
	}

	group = IDP_GetPropertyFromGroup(rd->ffcodecdata.properties, type);
	if (group && prop) {
		IDP_RemFromGroup(group, prop);
		IDP_FreeProperty(prop);
		MEM_freeN(prop);
	}
}

IDProperty *BKE_ffmpeg_property_add(RenderData *rd, const char *type, int opt_index, int parent_index)
{
	AVCodecContext c;
	const AVOption *o;
	const AVOption *parent;
	IDProperty *group;
	IDProperty *prop;
	IDPropertyTemplate val;
	int idp_type;
	char name[256];
	
	val.i = 0;

	avcodec_get_context_defaults(&c);

	o = c.av_class->option + opt_index;
	parent = c.av_class->option + parent_index;

	if (!rd->ffcodecdata.properties) {
		rd->ffcodecdata.properties = IDP_New(IDP_GROUP, &val, "ffmpeg"); 
	}

	group = IDP_GetPropertyFromGroup(rd->ffcodecdata.properties, type);
	
	if (!group) {
		group = IDP_New(IDP_GROUP, &val, type);
		IDP_AddToGroup(rd->ffcodecdata.properties, group);
	}

	if (parent_index) {
		BLI_snprintf(name, sizeof(name), "%s:%s", parent->name, o->name);
	}
	else {
		BLI_strncpy(name, o->name, sizeof(name));
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
			val.i = FFMPEG_DEF_OPT_VAL_INT(o);
			idp_type = IDP_INT;
			break;
		case FF_OPT_TYPE_DOUBLE:
		case FF_OPT_TYPE_FLOAT:
			val.f = FFMPEG_DEF_OPT_VAL_DOUBLE(o);
			idp_type = IDP_FLOAT;
			break;
		case FF_OPT_TYPE_STRING:
			val.string.str = (char *)"                                                                               ";
			val.string.len = 80;
/*		val.str = (char *)"                                                                               ";*/
			idp_type = IDP_STRING;
			break;
		case FF_OPT_TYPE_CONST:
			val.i = 1;
			idp_type = IDP_INT;
			break;
		default:
			return NULL;
	}
	prop = IDP_New(idp_type, &val, name);
	IDP_AddToGroup(group, prop);
	return prop;
}

/* not all versions of ffmpeg include that, so here we go ... */

static const AVOption *my_av_find_opt(void *v, const char *name,
                                      const char *unit, int mask, int flags)
{
	AVClass *c = *(AVClass **)v;
	const AVOption *o = c->option;

	for (; o && o->name; o++) {
		if (!strcmp(o->name, name) &&
		    (!unit || (o->unit && !strcmp(o->unit, unit))) &&
		    (o->flags & mask) == flags)
		{
			return o;
		}
	}
	return NULL;
}

int BKE_ffmpeg_property_add_string(RenderData *rd, const char *type, const char *str)
{
	AVCodecContext c;
	const AVOption *o = 0;
	const AVOption *p = 0;
	char name_[128];
	char *name;
	char *param;
	IDProperty *prop;
	
	avcodec_get_context_defaults(&c);

	strncpy(name_, str, sizeof(name_));

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
		prop = BKE_ffmpeg_property_add(rd,
		                               (char *) type, p - c.av_class->option,
		                               o - c.av_class->option);
	}
	else {
		prop = BKE_ffmpeg_property_add(rd,
		                               (char *) type, o - c.av_class->option, 0);
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

static void ffmpeg_set_expert_options(RenderData *rd)
{
	int codec_id = rd->ffcodecdata.codec;

	if (rd->ffcodecdata.properties)
		IDP_FreeProperty(rd->ffcodecdata.properties);

	if (codec_id == CODEC_ID_H264) {
		/*
		 * All options here are for x264, but must be set via ffmpeg.
		 * The names are therefore different - Search for "x264 to FFmpeg option mapping"
		 * to get a list.
		 */

		/*
		 * Use CABAC coder. Using "coder:1", which should be equivalent,
		 * crashes Blender for some reason. Either way - this is no big deal.
		 */
		BKE_ffmpeg_property_add_string(rd, "video", "coder:vlc");

		/*
		 * The other options were taken from the libx264-default.preset
		 * included in the ffmpeg distribution.
		 */
//		ffmpeg_property_add_string(rd, "video", "flags:loop"); // this breaks compatibility for QT
		BKE_ffmpeg_property_add_string(rd, "video", "cmp:chroma");
		BKE_ffmpeg_property_add_string(rd, "video", "partitions:parti4x4");
		BKE_ffmpeg_property_add_string(rd, "video", "partitions:partp8x8");
		BKE_ffmpeg_property_add_string(rd, "video", "partitions:partb8x8");
		BKE_ffmpeg_property_add_string(rd, "video", "me:hex");
		BKE_ffmpeg_property_add_string(rd, "video", "subq:6");
		BKE_ffmpeg_property_add_string(rd, "video", "me_range:16");
		BKE_ffmpeg_property_add_string(rd, "video", "qdiff:4");
		BKE_ffmpeg_property_add_string(rd, "video", "keyint_min:25");
		BKE_ffmpeg_property_add_string(rd, "video", "sc_threshold:40");
		BKE_ffmpeg_property_add_string(rd, "video", "i_qfactor:0.71");
		BKE_ffmpeg_property_add_string(rd, "video", "b_strategy:1");
		BKE_ffmpeg_property_add_string(rd, "video", "bf:3");
		BKE_ffmpeg_property_add_string(rd, "video", "refs:2");
		BKE_ffmpeg_property_add_string(rd, "video", "qcomp:0.6");
		BKE_ffmpeg_property_add_string(rd, "video", "directpred:3");
		BKE_ffmpeg_property_add_string(rd, "video", "trellis:0");
		BKE_ffmpeg_property_add_string(rd, "video", "flags2:wpred");
		BKE_ffmpeg_property_add_string(rd, "video", "flags2:dct8x8");
		BKE_ffmpeg_property_add_string(rd, "video", "flags2:fastpskip");
		BKE_ffmpeg_property_add_string(rd, "video", "wpredp:2");

		if (rd->ffcodecdata.flags & FFMPEG_LOSSLESS_OUTPUT)
			BKE_ffmpeg_property_add_string(rd, "video", "cqp:0");
	}
#if 0   /* disabled for after release */
	else if (codec_id == CODEC_ID_DNXHD) {
		if (rd->ffcodecdata.flags & FFMPEG_LOSSLESS_OUTPUT)
			ffmpeg_property_add_string(rd, "video", "mbd:rd");
	}
#endif
}

void BKE_ffmpeg_preset_set(RenderData *rd, int preset)
{
	int isntsc = (rd->frs_sec != 25);

	if (rd->ffcodecdata.properties)
		IDP_FreeProperty(rd->ffcodecdata.properties);

	switch (preset) {
		case FFMPEG_PRESET_VCD:
			rd->ffcodecdata.type = FFMPEG_MPEG1;
			rd->ffcodecdata.video_bitrate = 1150;
			rd->xsch = 352;
			rd->ysch = isntsc ? 240 : 288;
			rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
			rd->ffcodecdata.rc_max_rate = 1150;
			rd->ffcodecdata.rc_min_rate = 1150;
			rd->ffcodecdata.rc_buffer_size = 40 * 8;
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
			rd->ffcodecdata.rc_buffer_size = 224 * 8;
			rd->ffcodecdata.mux_packet_size = 2324;
			rd->ffcodecdata.mux_rate = 0;
			break;

		case FFMPEG_PRESET_DVD:
			rd->ffcodecdata.type = FFMPEG_MPEG2;
			rd->ffcodecdata.video_bitrate = 6000;

			/* Don't set resolution, see [#21351]
			 * rd->xsch = 720;
			 * rd->ysch = isntsc ? 480 : 576; */

			rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
			rd->ffcodecdata.rc_max_rate = 9000;
			rd->ffcodecdata.rc_min_rate = 0;
			rd->ffcodecdata.rc_buffer_size = 224 * 8;
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
			rd->ffcodecdata.rc_buffer_size = 224 * 8;
			rd->ffcodecdata.mux_packet_size = 2048;
			rd->ffcodecdata.mux_rate = 10080000;

			break;

		case FFMPEG_PRESET_THEORA:
		case FFMPEG_PRESET_XVID:
			if (preset == FFMPEG_PRESET_XVID) {
				rd->ffcodecdata.type = FFMPEG_AVI;
				rd->ffcodecdata.codec = CODEC_ID_MPEG4;
			}
			else if (preset == FFMPEG_PRESET_THEORA) {
				rd->ffcodecdata.type = FFMPEG_OGG; // XXX broken
				rd->ffcodecdata.codec = CODEC_ID_THEORA;
			}

			rd->ffcodecdata.video_bitrate = 6000;
			rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
			rd->ffcodecdata.rc_max_rate = 9000;
			rd->ffcodecdata.rc_min_rate = 0;
			rd->ffcodecdata.rc_buffer_size = 224 * 8;
			rd->ffcodecdata.mux_packet_size = 2048;
			rd->ffcodecdata.mux_rate = 10080000;
			break;

	}

	ffmpeg_set_expert_options(rd);
}

void BKE_ffmpeg_image_type_verify(RenderData *rd, ImageFormatData *imf)
{
	int audio = 0;

	if (imf->imtype == R_IMF_IMTYPE_FFMPEG) {
		if (rd->ffcodecdata.type <= 0 ||
		    rd->ffcodecdata.codec <= 0 ||
		    rd->ffcodecdata.audio_codec <= 0 ||
		    rd->ffcodecdata.video_bitrate <= 1)
		{
			rd->ffcodecdata.codec = CODEC_ID_MPEG2VIDEO;

			BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_DVD);
		}
		if (rd->ffcodecdata.type == FFMPEG_OGG) {
			rd->ffcodecdata.type = FFMPEG_MPEG2;
		}

		audio = 1;
	}
	else if (imf->imtype == R_IMF_IMTYPE_H264) {
		if (rd->ffcodecdata.codec != CODEC_ID_H264) {
			BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_H264);
			audio = 1;
		}
	}
	else if (imf->imtype == R_IMF_IMTYPE_XVID) {
		if (rd->ffcodecdata.codec != CODEC_ID_MPEG4) {
			BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_XVID);
			audio = 1;
		}
	}
	else if (imf->imtype == R_IMF_IMTYPE_THEORA) {
		if (rd->ffcodecdata.codec != CODEC_ID_THEORA) {
			BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_THEORA);
			audio = 1;
		}
	}

	if (audio && rd->ffcodecdata.audio_codec < 0) {
		rd->ffcodecdata.audio_codec = CODEC_ID_NONE;
		rd->ffcodecdata.audio_bitrate = 128;
	}
}

void BKE_ffmpeg_codec_settings_verify(RenderData *rd)
{
	ffmpeg_set_expert_options(rd);
}

int BKE_ffmpeg_alpha_channel_is_supported(RenderData *rd)
{
	int codec = rd->ffcodecdata.codec;

	if (codec == CODEC_ID_QTRLE)
		return TRUE;

#ifdef FFMPEG_FFV1_ALPHA_SUPPORTED
	if (codec == CODEC_ID_FFV1)
		return TRUE;
#endif

	return FALSE;
}

#endif
