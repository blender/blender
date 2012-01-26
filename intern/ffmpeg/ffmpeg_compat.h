#ifndef __ffmpeg_compat_h_included__
#define __ffmpeg_compat_h_included__ 1

/*
 *
 * compatibility macros to make every ffmpeg installation appear
 * like the most current installation (wrapping some functionality sometimes)
 * it also includes all ffmpeg header files at once, no need to do it 
 * seperately.
 *
 * Copyright (c) 2011 Peter Schlaile
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


#include <libavformat/avformat.h>


/* check our ffmpeg is new enough, avoids user complaints */
#if (LIBAVFORMAT_VERSION_MAJOR < 52) || ((LIBAVFORMAT_VERSION_MAJOR == 52) && (LIBAVFORMAT_VERSION_MINOR <= 64))
#  error "FFmpeg 0.7 or newer is needed, Upgrade your FFmpeg or disable it"
#endif
/* end sanity check */


#include <libavcodec/avcodec.h>
#include <libavutil/rational.h>
#include <libavutil/opt.h>

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 101))
#define FFMPEG_HAVE_PARSE_UTILS 1
#include <libavutil/parseutils.h>
#endif

#include <libswscale/swscale.h>
#include <libavcodec/opt.h>

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 105))
#define FFMPEG_HAVE_AVIO 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 53) || ((LIBAVCODEC_VERSION_MAJOR == 53) && (LIBAVCODEC_VERSION_MINOR > 1)) || ((LIBAVCODEC_VERSION_MAJOR == 53) && (LIBAVCODEC_VERSION_MINOR == 1) && (LIBAVCODEC_VERSION_MICRO >= 1)) || ((LIBAVCODEC_VERSION_MAJOR == 52) && (LIBAVCODEC_VERSION_MINOR >= 121))
#define FFMPEG_HAVE_DEFAULT_VAL_UNION 1
#endif

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 101))
#define FFMPEG_HAVE_AV_DUMP_FORMAT 1
#endif

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 45))
#define FFMPEG_HAVE_AV_GUESS_FORMAT 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 52) || ((LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 23))
#define FFMPEG_HAVE_DECODE_AUDIO3 1
#define FFMPEG_HAVE_DECODE_VIDEO2 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 52) || ((LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 64))
#define FFMPEG_HAVE_AVMEDIA_TYPES 1
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 52) || (LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 29)) && \
	((LIBSWSCALE_VERSION_MAJOR > 0) || (LIBSWSCALE_VERSION_MAJOR >= 0) && (LIBSWSCALE_VERSION_MINOR >= 10))
#define FFMPEG_SWSCALE_COLOR_SPACE_SUPPORT
#endif

#ifndef FFMPEG_HAVE_AVIO
#define AVIO_FLAG_WRITE URL_WRONLY
#define avio_open url_fopen
#define avio_tell url_ftell
#define avio_close url_fclose
#define avio_size url_fsize
#endif

/* there are some version inbetween, which have avio_... functions but no
   AVIO_FLAG_... */
#ifndef AVIO_FLAG_WRITE
#define AVIO_FLAG_WRITE URL_WRONLY
#endif

#ifndef AV_PKT_FLAG_KEY
#define AV_PKT_FLAG_KEY PKT_FLAG_KEY
#endif

#ifndef FFMPEG_HAVE_AV_DUMP_FORMAT
#define av_dump_format dump_format
#endif

#ifndef FFMPEG_HAVE_AV_GUESS_FORMAT
#define av_guess_format guess_format
#endif

#ifndef FFMPEG_HAVE_PARSE_UTILS
#define av_parse_video_rate av_parse_video_frame_rate
#endif

#ifdef FFMPEG_HAVE_DEFAULT_VAL_UNION
#define FFMPEG_DEF_OPT_VAL_INT(OPT) OPT->default_val.i64
#define FFMPEG_DEF_OPT_VAL_DOUBLE(OPT) OPT->default_val.dbl
#else
#define FFMPEG_DEF_OPT_VAL_INT(OPT) OPT->default_val
#define FFMPEG_DEF_OPT_VAL_DOUBLE(OPT) OPT->default_val
#endif

#ifndef FFMPEG_HAVE_AVMEDIA_TYPES
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif

#ifndef FFMPEG_HAVE_DECODE_AUDIO3
static inline 
int avcodec_decode_audio3(AVCodecContext *avctx, int16_t *samples,
			  int *frame_size_ptr, AVPacket *avpkt)
{
	return avcodec_decode_audio2(avctx, samples,
				     frame_size_ptr, avpkt->data,
				     avpkt->size);
}
#endif

#ifndef FFMPEG_HAVE_DECODE_VIDEO2
static inline
int avcodec_decode_video2(AVCodecContext *avctx, AVFrame *picture,
                         int *got_picture_ptr,
                         AVPacket *avpkt)
{
	return avcodec_decode_video(avctx, picture, got_picture_ptr,
				    avpkt->data, avpkt->size);
}
#endif

static inline
int64_t av_get_pts_from_frame(AVFormatContext *avctx, AVFrame * picture)
{
	int64_t pts = picture->pkt_pts;

	if (pts == AV_NOPTS_VALUE) {
		pts = picture->pkt_dts;
	}
	if (pts == AV_NOPTS_VALUE) {
		pts = 0;
	}

	(void)avctx;
	return pts;
}

#endif
