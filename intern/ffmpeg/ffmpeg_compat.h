/*
 * compatibility macros to make every ffmpeg installation appear
 * like the most current installation (wrapping some functionality sometimes)
 * it also includes all ffmpeg header files at once, no need to do it 
 * separately.
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

#ifndef __FFMPEG_COMPAT_H__
#define __FFMPEG_COMPAT_H__

#include <libavformat/avformat.h>

/* check our ffmpeg is new enough, avoids user complaints */
#if (LIBAVFORMAT_VERSION_MAJOR < 52) || ((LIBAVFORMAT_VERSION_MAJOR == 52) && (LIBAVFORMAT_VERSION_MINOR <= 64))
#  error "FFmpeg 0.7 or newer is needed, Upgrade your FFmpeg or disable it"
#endif
/* end sanity check */

/* visual studio 2012 does not define inline for C */
#ifdef _MSC_VER
#  define FFMPEG_INLINE static __inline
#else
#  define FFMPEG_INLINE static inline
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/rational.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 101))
#  define FFMPEG_HAVE_PARSE_UTILS 1
#  include <libavutil/parseutils.h>
#endif

#include <libswscale/swscale.h>

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 105))
#  define FFMPEG_HAVE_AVIO 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 53) || ((LIBAVCODEC_VERSION_MAJOR == 53) && (LIBAVCODEC_VERSION_MINOR > 1)) || ((LIBAVCODEC_VERSION_MAJOR == 53) && (LIBAVCODEC_VERSION_MINOR == 1) && (LIBAVCODEC_VERSION_MICRO >= 1)) || ((LIBAVCODEC_VERSION_MAJOR == 52) && (LIBAVCODEC_VERSION_MINOR >= 121))
#  define FFMPEG_HAVE_DEFAULT_VAL_UNION 1
#endif

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 101))
#  define FFMPEG_HAVE_AV_DUMP_FORMAT 1
#endif

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 45))
#  define FFMPEG_HAVE_AV_GUESS_FORMAT 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 52) || ((LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 23))
#  define FFMPEG_HAVE_DECODE_AUDIO3 1
#  define FFMPEG_HAVE_DECODE_VIDEO2 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 52) || ((LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 64))
#  define FFMPEG_HAVE_AVMEDIA_TYPES 1
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 52) || (LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 29)) && \
	((LIBSWSCALE_VERSION_MAJOR > 0) || (LIBSWSCALE_VERSION_MAJOR >= 0) && (LIBSWSCALE_VERSION_MINOR >= 10))
#  define FFMPEG_SWSCALE_COLOR_SPACE_SUPPORT
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 54) || (LIBAVCODEC_VERSION_MAJOR >= 54) && (LIBAVCODEC_VERSION_MINOR > 14))
#  define FFMPEG_HAVE_CANON_H264_RESOLUTION_FIX
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 53) || (LIBAVCODEC_VERSION_MAJOR >= 53) && (LIBAVCODEC_VERSION_MINOR >= 60))
#  define FFMPEG_HAVE_ENCODE_AUDIO2
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 53) || (LIBAVCODEC_VERSION_MAJOR >= 53) && (LIBAVCODEC_VERSION_MINOR >= 42))
#  define FFMPEG_HAVE_DECODE_AUDIO4
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 54) || (LIBAVCODEC_VERSION_MAJOR >= 54) && (LIBAVCODEC_VERSION_MINOR >= 13))
#  define FFMPEG_HAVE_AVFRAME_SAMPLE_RATE
#endif

#if ((LIBAVUTIL_VERSION_MAJOR > 51) || (LIBAVUTIL_VERSION_MAJOR == 51) && (LIBAVUTIL_VERSION_MINOR >= 21))
#  define FFMPEG_FFV1_ALPHA_SUPPORTED
#  define FFMPEG_SAMPLE_FMT_S16P_SUPPORTED
#else

FFMPEG_INLINE
int av_sample_fmt_is_planar(enum AVSampleFormat sample_fmt)
{
	/* no planar formats in FFmpeg < 0.9 */
	return 0;
}

#endif

/* FFmpeg upstream 1.0 is the first who added AV_ prefix. */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 59, 100)
#  define AV_CODEC_ID_NONE CODEC_ID_NONE
#  define AV_CODEC_ID_MPEG4 CODEC_ID_MPEG4
#  define AV_CODEC_ID_MJPEG CODEC_ID_MJPEG
#  define AV_CODEC_ID_DNXHD CODEC_ID_DNXHD
#  define AV_CODEC_ID_MPEG2VIDEO CODEC_ID_MPEG2VIDEO
#  define AV_CODEC_ID_MPEG1VIDEO CODEC_ID_MPEG1VIDEO
#  define AV_CODEC_ID_DVVIDEO CODEC_ID_DVVIDEO
#  define AV_CODEC_ID_THEORA CODEC_ID_THEORA
#  define AV_CODEC_ID_PNG CODEC_ID_PNG
#  define AV_CODEC_ID_QTRLE CODEC_ID_QTRLE
#  define AV_CODEC_ID_FFV1 CODEC_ID_FFV1
#  define AV_CODEC_ID_HUFFYUV CODEC_ID_HUFFYUV
#  define AV_CODEC_ID_H264 CODEC_ID_H264
#  define AV_CODEC_ID_FLV1 CODEC_ID_FLV1

#  define AV_CODEC_ID_AAC CODEC_ID_AAC
#  define AV_CODEC_ID_AC3 CODEC_ID_AC3
#  define AV_CODEC_ID_MP3 CODEC_ID_MP3
#  define AV_CODEC_ID_MP2 CODEC_ID_MP2
#  define AV_CODEC_ID_FLAC CODEC_ID_FLAC
#  define AV_CODEC_ID_PCM_U8 CODEC_ID_PCM_U8
#  define AV_CODEC_ID_PCM_S16LE CODEC_ID_PCM_S16LE
#  define AV_CODEC_ID_PCM_S24LE CODEC_ID_PCM_S24LE
#  define AV_CODEC_ID_PCM_S32LE CODEC_ID_PCM_S32LE
#  define AV_CODEC_ID_PCM_F32LE CODEC_ID_PCM_F32LE
#  define AV_CODEC_ID_PCM_F64LE CODEC_ID_PCM_F64LE
#  define AV_CODEC_ID_VORBIS CODEC_ID_VORBIS
#endif

FFMPEG_INLINE
int av_get_cropped_height_from_codec(AVCodecContext *pCodecCtx)
{
	int y = pCodecCtx->height;

#ifndef FFMPEG_HAVE_CANON_H264_RESOLUTION_FIX
/* really bad hack to remove this dreadfull black bar at the bottom
   with Canon footage and old ffmpeg versions.
   (to fix this properly in older ffmpeg versions one has to write a new
   demuxer...) 
	   
   see the actual fix here for reference:

   http://git.libav.org/?p=libav.git;a=commit;h=30f515091c323da59c0f1b533703dedca2f4b95d

   We do our best to apply this only to matching footage.
*/
	if (pCodecCtx->width == 1920 && 
	    pCodecCtx->height == 1088 &&
	    pCodecCtx->pix_fmt == PIX_FMT_YUVJ420P &&
	    pCodecCtx->codec_id == AV_CODEC_ID_H264 ) {
		y = 1080;
	}
#endif

	return y;
}

#if ((LIBAVUTIL_VERSION_MAJOR < 51) || (LIBAVUTIL_VERSION_MAJOR == 51) && (LIBAVUTIL_VERSION_MINOR < 22))
FFMPEG_INLINE
int av_opt_set(void *obj, const char *name, const char *val, int search_flags)
{
	const AVOption *rv = NULL;
	av_set_string3(obj, name, val, 1, &rv);
	return rv != NULL;
}

FFMPEG_INLINE
int av_opt_set_int(void *obj, const char *name, int64_t val, int search_flags)
{
	const AVOption *rv = NULL;
	rv = av_set_int(obj, name, val);
	return rv != NULL;
}

FFMPEG_INLINE
int av_opt_set_double(void *obj, const char *name, double val, int search_flags)
{
	const AVOption *rv = NULL;
	rv = av_set_double(obj, name, val);
	return rv != NULL;
}

#  define AV_OPT_TYPE_INT     FF_OPT_TYPE_INT
#  define AV_OPT_TYPE_INT64   FF_OPT_TYPE_INT64
#  define AV_OPT_TYPE_STRING  FF_OPT_TYPE_STRING
#  define AV_OPT_TYPE_CONST   FF_OPT_TYPE_CONST
#  define AV_OPT_TYPE_DOUBLE  FF_OPT_TYPE_DOUBLE
#  define AV_OPT_TYPE_FLOAT   FF_OPT_TYPE_FLOAT
#endif

#if ((LIBAVUTIL_VERSION_MAJOR < 51) || (LIBAVUTIL_VERSION_MAJOR == 51) && (LIBAVUTIL_VERSION_MINOR < 54))
FFMPEG_INLINE
enum AVSampleFormat av_get_packed_sample_fmt(enum AVSampleFormat sample_fmt)
{
    if (sample_fmt < 0 || sample_fmt >= AV_SAMPLE_FMT_NB)
        return AV_SAMPLE_FMT_NONE;
    return sample_fmt;
}
#endif

#if ((LIBAVFORMAT_VERSION_MAJOR < 53) || ((LIBAVFORMAT_VERSION_MAJOR == 53) && (LIBAVFORMAT_VERSION_MINOR < 24)) || ((LIBAVFORMAT_VERSION_MAJOR == 53) && (LIBAVFORMAT_VERSION_MINOR < 24) && (LIBAVFORMAT_VERSION_MICRO < 2)))
#  define avformat_close_input(x) av_close_input_file(*(x))
#endif

#if ((LIBAVCODEC_VERSION_MAJOR < 53) || (LIBAVCODEC_VERSION_MAJOR == 53 && LIBAVCODEC_VERSION_MINOR < 35))
FFMPEG_INLINE
int avcodec_open2(AVCodecContext *avctx, AVCodec *codec, AVDictionary **options)
{
	/* TODO: no options are taking into account */
	return avcodec_open(avctx, codec);
}
#endif

#if ((LIBAVFORMAT_VERSION_MAJOR < 53) || (LIBAVFORMAT_VERSION_MAJOR == 53 && LIBAVFORMAT_VERSION_MINOR < 21))
FFMPEG_INLINE
AVStream *avformat_new_stream(AVFormatContext *s, AVCodec *c)
{
	/* TODO: no codec is taking into account */
	return av_new_stream(s, 0);
}

FFMPEG_INLINE
int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options)
{
	/* TODO: no options are taking into account */
	return av_find_stream_info(ic);
}
#endif

#if ((LIBAVFORMAT_VERSION_MAJOR > 53) || ((LIBAVFORMAT_VERSION_MAJOR == 53) && (LIBAVFORMAT_VERSION_MINOR > 32)) || ((LIBAVFORMAT_VERSION_MAJOR == 53) && (LIBAVFORMAT_VERSION_MINOR == 24) && (LIBAVFORMAT_VERSION_MICRO >= 100)))
FFMPEG_INLINE
void my_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp)
{
	int i;

	for (i = 0; i < s->nb_streams; i++) {
		AVStream *st = s->streams[i];

		st->cur_dts = av_rescale(timestamp,
		                         st->time_base.den * (int64_t)ref_st->time_base.num,
		                         st->time_base.num * (int64_t)ref_st->time_base.den);
	}
}

FFMPEG_INLINE
void av_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp)
{
	my_update_cur_dts(s, ref_st, timestamp);
}
#endif

#if ((LIBAVCODEC_VERSION_MAJOR < 54) || (LIBAVCODEC_VERSION_MAJOR == 54 && LIBAVCODEC_VERSION_MINOR < 28))
FFMPEG_INLINE
void avcodec_free_frame(AVFrame **frame)
{
	/* don't need to do anything with old AVFrame
	 * since it does not have malloced members */
	(void)frame;
}
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 54) || (LIBAVCODEC_VERSION_MAJOR >= 54) && (LIBAVCODEC_VERSION_MINOR >= 13))
#  define FFMPEG_HAVE_AVFRAME_SAMPLE_RATE
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 54) || (LIBAVCODEC_VERSION_MAJOR == 54 && LIBAVCODEC_VERSION_MINOR >= 13))
#  define FFMPEG_HAVE_FRAME_CHANNEL_LAYOUT
#endif

#ifndef FFMPEG_HAVE_AVIO
#  define AVIO_FLAG_WRITE URL_WRONLY
#  define avio_open url_fopen
#  define avio_tell url_ftell
#  define avio_close url_fclose
#  define avio_size url_fsize
#endif

/* there are some version inbetween, which have avio_... functions but no
 * AVIO_FLAG_... */
#ifndef AVIO_FLAG_WRITE
#  define AVIO_FLAG_WRITE URL_WRONLY
#endif

#ifndef AV_PKT_FLAG_KEY
#  define AV_PKT_FLAG_KEY PKT_FLAG_KEY
#endif

#ifndef FFMPEG_HAVE_AV_DUMP_FORMAT
#  define av_dump_format dump_format
#endif

#ifndef FFMPEG_HAVE_AV_GUESS_FORMAT
#  define av_guess_format guess_format
#endif

#ifndef FFMPEG_HAVE_PARSE_UTILS
#  define av_parse_video_rate av_parse_video_frame_rate
#endif

#ifdef FFMPEG_HAVE_DEFAULT_VAL_UNION
#  define FFMPEG_DEF_OPT_VAL_INT(OPT) OPT->default_val.i64
#  define FFMPEG_DEF_OPT_VAL_DOUBLE(OPT) OPT->default_val.dbl
#else
#  define FFMPEG_DEF_OPT_VAL_INT(OPT) OPT->default_val
#  define FFMPEG_DEF_OPT_VAL_DOUBLE(OPT) OPT->default_val
#endif

#ifndef FFMPEG_HAVE_AVMEDIA_TYPES
#  define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#  define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif

#ifndef FFMPEG_HAVE_DECODE_AUDIO3
FFMPEG_INLINE 
int avcodec_decode_audio3(AVCodecContext *avctx, int16_t *samples,
			  int *frame_size_ptr, AVPacket *avpkt)
{
	return avcodec_decode_audio2(avctx, samples,
				     frame_size_ptr, avpkt->data,
				     avpkt->size);
}
#endif

#ifndef FFMPEG_HAVE_DECODE_VIDEO2
FFMPEG_INLINE
int avcodec_decode_video2(AVCodecContext *avctx, AVFrame *picture,
                         int *got_picture_ptr,
                         AVPacket *avpkt)
{
	return avcodec_decode_video(avctx, picture, got_picture_ptr,
				    avpkt->data, avpkt->size);
}
#endif

FFMPEG_INLINE
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

/* obsolete constant formerly defined in FFMpeg libavcodec/avcodec.h */
#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
#  define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 1, 0)
FFMPEG_INLINE
int avcodec_encode_video2(AVCodecContext *avctx, AVPacket *pkt,
                          const AVFrame *frame, int *got_output)
{
	int outsize, ret;

	ret = av_new_packet(pkt, avctx->width * avctx->height * 7 + 10000);
	if (ret < 0)
		return ret;

	outsize = avcodec_encode_video(avctx, pkt->data, pkt->size, frame);
	if (outsize <= 0) {
		*got_output = 0;
		av_free_packet(pkt);
	}
	else {
		*got_output = 1;
		av_shrink_packet(pkt, outsize);
		if (avctx->coded_frame) {
			pkt->pts = avctx->coded_frame->pts;
			if (avctx->coded_frame->key_frame)
				pkt->flags |= AV_PKT_FLAG_KEY;
		}
	}

	return outsize >= 0 ? 0 : outsize;
}

#endif

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53, 17, 0)
FFMPEG_INLINE
void avformat_close_input(AVFormatContext **ctx)
{
    av_close_input_file(*ctx);
    *ctx = NULL;
}
#endif

#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(52, 8, 0)
FFMPEG_INLINE
AVFrame *av_frame_alloc(void)
{
    return avcodec_alloc_frame();
}

FFMPEG_INLINE
void av_frame_free(AVFrame **frame)
{
    av_freep(frame);
}
#endif

FFMPEG_INLINE
AVRational av_get_r_frame_rate_compat(const AVStream *stream)
{
	/* Stupid way to distinguish FFmpeg from Libav. */
#if LIBAVCODEC_VERSION_MICRO >= 100
	return stream->r_frame_rate;
#else
	return stream->avg_frame_rate;
#endif
}

#endif
