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
 */

#ifndef __FFMPEG_COMPAT_H__
#define __FFMPEG_COMPAT_H__

#include <libavformat/avformat.h>

/* check our ffmpeg is new enough, avoids user complaints */
#if (LIBAVFORMAT_VERSION_MAJOR < 52) || \
    ((LIBAVFORMAT_VERSION_MAJOR == 52) && (LIBAVFORMAT_VERSION_MINOR <= 64))
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

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || \
    ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 101))
#  define FFMPEG_HAVE_PARSE_UTILS 1
#  include <libavutil/parseutils.h>
#endif

#include <libswscale/swscale.h>

/* Stupid way to distinguish FFmpeg from Libav:
 * - FFmpeg's MICRO version starts from 100 and goes up, while
 * - Libav's micro is always below 100.
 */
#if LIBAVCODEC_VERSION_MICRO >= 100
#  define AV_USING_FFMPEG
#else
#  define AV_USING_LIBAV
#endif

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || \
    ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 105))
#  define FFMPEG_HAVE_AVIO 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 53) || \
    ((LIBAVCODEC_VERSION_MAJOR == 53) && (LIBAVCODEC_VERSION_MINOR > 1)) || \
    ((LIBAVCODEC_VERSION_MAJOR == 53) && (LIBAVCODEC_VERSION_MINOR == 1) && \
     (LIBAVCODEC_VERSION_MICRO >= 1)) || \
    ((LIBAVCODEC_VERSION_MAJOR == 52) && (LIBAVCODEC_VERSION_MINOR >= 121))
#  define FFMPEG_HAVE_DEFAULT_VAL_UNION 1
#endif

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || \
    ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 101))
#  define FFMPEG_HAVE_AV_DUMP_FORMAT 1
#endif

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || \
    ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 45))
#  define FFMPEG_HAVE_AV_GUESS_FORMAT 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 52) || \
    ((LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 23))
#  define FFMPEG_HAVE_DECODE_AUDIO3 1
#  define FFMPEG_HAVE_DECODE_VIDEO2 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 52) || \
    ((LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 64))
#  define FFMPEG_HAVE_AVMEDIA_TYPES 1
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 52) || \
     (LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 29)) && \
    ((LIBSWSCALE_VERSION_MAJOR > 0) || \
     (LIBSWSCALE_VERSION_MAJOR >= 0) && (LIBSWSCALE_VERSION_MINOR >= 10))
#  define FFMPEG_SWSCALE_COLOR_SPACE_SUPPORT
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 54) || \
     (LIBAVCODEC_VERSION_MAJOR >= 54) && (LIBAVCODEC_VERSION_MINOR > 14))
#  define FFMPEG_HAVE_CANON_H264_RESOLUTION_FIX
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 53) || \
     (LIBAVCODEC_VERSION_MAJOR >= 53) && (LIBAVCODEC_VERSION_MINOR >= 60))
#  define FFMPEG_HAVE_ENCODE_AUDIO2
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 53) || \
     (LIBAVCODEC_VERSION_MAJOR >= 53) && (LIBAVCODEC_VERSION_MINOR >= 42))
#  define FFMPEG_HAVE_DECODE_AUDIO4
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 54) || \
     (LIBAVCODEC_VERSION_MAJOR >= 54) && (LIBAVCODEC_VERSION_MINOR >= 13))
#  define FFMPEG_HAVE_AVFRAME_SAMPLE_RATE
#endif

#if ((LIBAVUTIL_VERSION_MAJOR > 51) || \
     (LIBAVUTIL_VERSION_MAJOR == 51) && (LIBAVUTIL_VERSION_MINOR >= 21))
#  define FFMPEG_FFV1_ALPHA_SUPPORTED
#  define FFMPEG_SAMPLE_FMT_S16P_SUPPORTED
#else

FFMPEG_INLINE
int av_sample_fmt_is_planar(enum AVSampleFormat sample_fmt)
{
  /* no planar formats in FFmpeg < 0.9 */
  (void)sample_fmt;
  return 0;
}

#endif

/* XXX TODO Probably fix to correct modern flags in code? Not sure how old FFMPEG we want to
 * support though, so for now this will do. */

#ifndef FF_MIN_BUFFER_SIZE
#  ifdef AV_INPUT_BUFFER_MIN_SIZE
#    define FF_MIN_BUFFER_SIZE AV_INPUT_BUFFER_MIN_SIZE
#  endif
#endif

#ifndef FF_INPUT_BUFFER_PADDING_SIZE
#  ifdef AV_INPUT_BUFFER_PADDING_SIZE
#    define FF_INPUT_BUFFER_PADDING_SIZE AV_INPUT_BUFFER_PADDING_SIZE
#  endif
#endif

#ifndef CODEC_FLAG_GLOBAL_HEADER
#  ifdef AV_CODEC_FLAG_GLOBAL_HEADER
#    define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#  endif
#endif

#ifndef CODEC_FLAG_GLOBAL_HEADER
#  ifdef AV_CODEC_FLAG_GLOBAL_HEADER
#    define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#  endif
#endif

#ifndef CODEC_FLAG_INTERLACED_DCT
#  ifdef AV_CODEC_FLAG_INTERLACED_DCT
#    define CODEC_FLAG_INTERLACED_DCT AV_CODEC_FLAG_INTERLACED_DCT
#  endif
#endif

#ifndef CODEC_FLAG_INTERLACED_ME
#  ifdef AV_CODEC_FLAG_INTERLACED_ME
#    define CODEC_FLAG_INTERLACED_ME AV_CODEC_FLAG_INTERLACED_ME
#  endif
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
  if (pCodecCtx->width == 1920 && pCodecCtx->height == 1088 &&
      pCodecCtx->pix_fmt == PIX_FMT_YUVJ420P && pCodecCtx->codec_id == AV_CODEC_ID_H264) {
    y = 1080;
  }
#endif

  return y;
}

#if ((LIBAVUTIL_VERSION_MAJOR < 51) || \
     (LIBAVUTIL_VERSION_MAJOR == 51) && (LIBAVUTIL_VERSION_MINOR < 22))
FFMPEG_INLINE
int av_opt_set(void *obj, const char *name, const char *val, int search_flags)
{
  const AVOption *rv = NULL;
  (void)search_flags;
  av_set_string3(obj, name, val, 1, &rv);
  return rv != NULL;
}

FFMPEG_INLINE
int av_opt_set_int(void *obj, const char *name, int64_t val, int search_flags)
{
  const AVOption *rv = NULL;
  (void)search_flags;
  rv = av_set_int(obj, name, val);
  return rv != NULL;
}

FFMPEG_INLINE
int av_opt_set_double(void *obj, const char *name, double val, int search_flags)
{
  const AVOption *rv = NULL;
  (void)search_flags;
  rv = av_set_double(obj, name, val);
  return rv != NULL;
}

#  define AV_OPT_TYPE_INT FF_OPT_TYPE_INT
#  define AV_OPT_TYPE_INT64 FF_OPT_TYPE_INT64
#  define AV_OPT_TYPE_STRING FF_OPT_TYPE_STRING
#  define AV_OPT_TYPE_CONST FF_OPT_TYPE_CONST
#  define AV_OPT_TYPE_DOUBLE FF_OPT_TYPE_DOUBLE
#  define AV_OPT_TYPE_FLOAT FF_OPT_TYPE_FLOAT
#endif

#if ((LIBAVUTIL_VERSION_MAJOR < 51) || \
     (LIBAVUTIL_VERSION_MAJOR == 51) && (LIBAVUTIL_VERSION_MINOR < 54))
FFMPEG_INLINE
enum AVSampleFormat av_get_packed_sample_fmt(enum AVSampleFormat sample_fmt)
{
  if (sample_fmt < 0 || sample_fmt >= AV_SAMPLE_FMT_NB)
    return AV_SAMPLE_FMT_NONE;
  return sample_fmt;
}
#endif

#if ((LIBAVCODEC_VERSION_MAJOR < 53) || \
     (LIBAVCODEC_VERSION_MAJOR == 53 && LIBAVCODEC_VERSION_MINOR < 35))
FFMPEG_INLINE
int avcodec_open2(AVCodecContext *avctx, AVCodec *codec, AVDictionary **options)
{
  /* TODO: no options are taking into account */
  (void)options;
  return avcodec_open(avctx, codec);
}
#endif

#if ((LIBAVFORMAT_VERSION_MAJOR < 53) || \
     (LIBAVFORMAT_VERSION_MAJOR == 53 && LIBAVFORMAT_VERSION_MINOR < 21))
FFMPEG_INLINE
AVStream *avformat_new_stream(AVFormatContext *s, AVCodec *c)
{
  /* TODO: no codec is taking into account */
  (void)c;
  return av_new_stream(s, 0);
}

FFMPEG_INLINE
int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options)
{
  /* TODO: no options are taking into account */
  (void)options;
  return av_find_stream_info(ic);
}
#endif

#if ((LIBAVFORMAT_VERSION_MAJOR > 53) || \
     ((LIBAVFORMAT_VERSION_MAJOR == 53) && (LIBAVFORMAT_VERSION_MINOR > 32)) || \
     ((LIBAVFORMAT_VERSION_MAJOR == 53) && (LIBAVFORMAT_VERSION_MINOR == 24) && \
      (LIBAVFORMAT_VERSION_MICRO >= 100)))
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

#if ((LIBAVCODEC_VERSION_MAJOR < 54) || \
     (LIBAVCODEC_VERSION_MAJOR == 54 && LIBAVCODEC_VERSION_MINOR < 28))
FFMPEG_INLINE
void avcodec_free_frame(AVFrame **frame)
{
  /* don't need to do anything with old AVFrame
   * since it does not have malloced members */
  (void)frame;
}
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 54) || \
     (LIBAVCODEC_VERSION_MAJOR >= 54) && (LIBAVCODEC_VERSION_MINOR >= 13))
#  define FFMPEG_HAVE_AVFRAME_SAMPLE_RATE
#endif

#if ((LIBAVCODEC_VERSION_MAJOR > 54) || \
     (LIBAVCODEC_VERSION_MAJOR == 54 && LIBAVCODEC_VERSION_MINOR >= 13))
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
int avcodec_decode_audio3(AVCodecContext *avctx,
                          int16_t *samples,
                          int *frame_size_ptr,
                          AVPacket *avpkt)
{
  return avcodec_decode_audio2(avctx, samples, frame_size_ptr, avpkt->data, avpkt->size);
}
#endif

#ifndef FFMPEG_HAVE_DECODE_VIDEO2
FFMPEG_INLINE
int avcodec_decode_video2(AVCodecContext *avctx,
                          AVFrame *picture,
                          int *got_picture_ptr,
                          AVPacket *avpkt)
{
  return avcodec_decode_video(avctx, picture, got_picture_ptr, avpkt->data, avpkt->size);
}
#endif

FFMPEG_INLINE
int64_t av_get_pts_from_frame(AVFormatContext *avctx, AVFrame *picture)
{
  int64_t pts;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 34, 100)
  pts = picture->pts;
#else
  pts = picture->pkt_pts;
#endif

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
#  define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000  // 1 second of 48khz 32bit audio
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 1, 0)
FFMPEG_INLINE
int avcodec_encode_video2(AVCodecContext *avctx,
                          AVPacket *pkt,
                          const AVFrame *frame,
                          int *got_output)
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
const char *av_get_metadata_key_value(AVDictionary *metadata, const char *key)
{
  if (metadata == NULL) {
    return NULL;
  }
  AVDictionaryEntry *tag = NULL;
  while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
    if (!strcmp(tag->key, key)) {
      return tag->value;
    }
  }
  return NULL;
}

FFMPEG_INLINE
bool av_check_encoded_with_ffmpeg(AVFormatContext *ctx)
{
  const char *encoder = av_get_metadata_key_value(ctx->metadata, "ENCODER");
  if (encoder != NULL && !strncmp(encoder, "Lavf", 4)) {
    return true;
  }
  return false;
}

FFMPEG_INLINE
AVRational av_get_r_frame_rate_compat(AVFormatContext *ctx, const AVStream *stream)
{
  /* If the video is encoded with FFmpeg and we are decoding with FFmpeg
   * as well it seems to be more reliable to use r_frame_rate (tbr).
   *
   * For other cases we fall back to avg_frame_rate (fps) when possible.
   */
#ifdef AV_USING_FFMPEG
  if (av_check_encoded_with_ffmpeg(ctx)) {
    return stream->r_frame_rate;
  }
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 23, 1)
  /* For until r_frame_rate was deprecated use it. */
  return stream->r_frame_rate;
#else
#  ifdef AV_USING_FFMPEG
  /* Some of the videos might have average frame rate set to, while the
   * r_frame_rate will show a correct value. This happens, for example, for
   * OGG video files saved with Blender. */
  if (stream->avg_frame_rate.den == 0) {
    return stream->r_frame_rate;
  }
#  endif
  return stream->avg_frame_rate;
#endif
}

#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(51, 32, 0)
#  define AV_OPT_SEARCH_FAKE_OBJ 0
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 59, 100)
#  define FFMPEG_HAVE_DEPRECATED_FLAGS2
#endif

/* Since FFmpeg-1.1 this constant have AV_ prefix. */
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(52, 3, 100)
#  define AV_PIX_FMT_BGR32 PIX_FMT_BGR32
#  define AV_PIX_FMT_YUV422P PIX_FMT_YUV422P
#  define AV_PIX_FMT_BGRA PIX_FMT_BGRA
#  define AV_PIX_FMT_ARGB PIX_FMT_ARGB
#  define AV_PIX_FMT_RGBA PIX_FMT_RGBA
#endif

/* New API from FFmpeg-2.0 which soon became recommended one. */
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(52, 38, 100)
#  define av_frame_alloc avcodec_alloc_frame
#  define av_frame_free avcodec_free_frame
#  define av_frame_unref avcodec_get_frame_defaults
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 24, 102)

/* NOTE: The code in this block are from FFmpeg 2.6.4, which is licensed by LGPL. */

#  define MAX_NEG_CROP 1024

#  define times4(x) x, x, x, x
#  define times256(x) times4(times4(times4(times4(times4(x)))))

static const uint8_t ff_compat_crop_tab[256 + 2 * MAX_NEG_CROP] = {
    times256(0x00), 0x00, 0x01, 0x02, 0x03, 0x04,          0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B,           0x0C, 0x0D, 0x0E, 0x0F, 0x10,          0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17,           0x18, 0x19, 0x1A, 0x1B, 0x1C,          0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22,
    0x23,           0x24, 0x25, 0x26, 0x27, 0x28,          0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E,
    0x2F,           0x30, 0x31, 0x32, 0x33, 0x34,          0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x3B,           0x3C, 0x3D, 0x3E, 0x3F, 0x40,          0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
    0x47,           0x48, 0x49, 0x4A, 0x4B, 0x4C,          0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52,
    0x53,           0x54, 0x55, 0x56, 0x57, 0x58,          0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E,
    0x5F,           0x60, 0x61, 0x62, 0x63, 0x64,          0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
    0x6B,           0x6C, 0x6D, 0x6E, 0x6F, 0x70,          0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77,           0x78, 0x79, 0x7A, 0x7B, 0x7C,          0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82,
    0x83,           0x84, 0x85, 0x86, 0x87, 0x88,          0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E,
    0x8F,           0x90, 0x91, 0x92, 0x93, 0x94,          0x95, 0x96, 0x97, 0x98, 0x99, 0x9A,
    0x9B,           0x9C, 0x9D, 0x9E, 0x9F, 0xA0,          0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
    0xA7,           0xA8, 0xA9, 0xAA, 0xAB, 0xAC,          0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2,
    0xB3,           0xB4, 0xB5, 0xB6, 0xB7, 0xB8,          0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
    0xBF,           0xC0, 0xC1, 0xC2, 0xC3, 0xC4,          0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA,
    0xCB,           0xCC, 0xCD, 0xCE, 0xCF, 0xD0,          0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
    0xD7,           0xD8, 0xD9, 0xDA, 0xDB, 0xDC,          0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2,
    0xE3,           0xE4, 0xE5, 0xE6, 0xE7, 0xE8,          0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE,
    0xEF,           0xF0, 0xF1, 0xF2, 0xF3, 0xF4,          0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
    0xFB,           0xFC, 0xFD, 0xFE, 0xFF, times256(0xFF)};

#  undef times4
#  undef times256

/* filter parameters: [-1 4 2 4 -1] // 8 */
FFMPEG_INLINE
void deinterlace_line(uint8_t *dst,
                      const uint8_t *lum_m4,
                      const uint8_t *lum_m3,
                      const uint8_t *lum_m2,
                      const uint8_t *lum_m1,
                      const uint8_t *lum,
                      int size)
{
  const uint8_t *cm = ff_compat_crop_tab + MAX_NEG_CROP;
  int sum;

  for (; size > 0; size--) {
    sum = -lum_m4[0];
    sum += lum_m3[0] << 2;
    sum += lum_m2[0] << 1;
    sum += lum_m1[0] << 2;
    sum += -lum[0];
    dst[0] = cm[(sum + 4) >> 3];
    lum_m4++;
    lum_m3++;
    lum_m2++;
    lum_m1++;
    lum++;
    dst++;
  }
}

FFMPEG_INLINE
void deinterlace_line_inplace(
    uint8_t *lum_m4, uint8_t *lum_m3, uint8_t *lum_m2, uint8_t *lum_m1, uint8_t *lum, int size)
{
  const uint8_t *cm = ff_compat_crop_tab + MAX_NEG_CROP;
  int sum;

  for (; size > 0; size--) {
    sum = -lum_m4[0];
    sum += lum_m3[0] << 2;
    sum += lum_m2[0] << 1;
    lum_m4[0] = lum_m2[0];
    sum += lum_m1[0] << 2;
    sum += -lum[0];
    lum_m2[0] = cm[(sum + 4) >> 3];
    lum_m4++;
    lum_m3++;
    lum_m2++;
    lum_m1++;
    lum++;
  }
}

/* deinterlacing : 2 temporal taps, 3 spatial taps linear filter. The
   top field is copied as is, but the bottom field is deinterlaced
   against the top field. */
FFMPEG_INLINE
void deinterlace_bottom_field(
    uint8_t *dst, int dst_wrap, const uint8_t *src1, int src_wrap, int width, int height)
{
  const uint8_t *src_m2, *src_m1, *src_0, *src_p1, *src_p2;
  int y;

  src_m2 = src1;
  src_m1 = src1;
  src_0 = &src_m1[src_wrap];
  src_p1 = &src_0[src_wrap];
  src_p2 = &src_p1[src_wrap];
  for (y = 0; y < (height - 2); y += 2) {
    memcpy(dst, src_m1, width);
    dst += dst_wrap;
    deinterlace_line(dst, src_m2, src_m1, src_0, src_p1, src_p2, width);
    src_m2 = src_0;
    src_m1 = src_p1;
    src_0 = src_p2;
    src_p1 += 2 * src_wrap;
    src_p2 += 2 * src_wrap;
    dst += dst_wrap;
  }
  memcpy(dst, src_m1, width);
  dst += dst_wrap;
  /* do last line */
  deinterlace_line(dst, src_m2, src_m1, src_0, src_0, src_0, width);
}

FFMPEG_INLINE
int deinterlace_bottom_field_inplace(uint8_t *src1, int src_wrap, int width, int height)
{
  uint8_t *src_m1, *src_0, *src_p1, *src_p2;
  int y;
  uint8_t *buf = (uint8_t *)av_malloc(width);
  if (!buf)
    return AVERROR(ENOMEM);

  src_m1 = src1;
  memcpy(buf, src_m1, width);
  src_0 = &src_m1[src_wrap];
  src_p1 = &src_0[src_wrap];
  src_p2 = &src_p1[src_wrap];
  for (y = 0; y < (height - 2); y += 2) {
    deinterlace_line_inplace(buf, src_m1, src_0, src_p1, src_p2, width);
    src_m1 = src_p1;
    src_0 = src_p2;
    src_p1 += 2 * src_wrap;
    src_p2 += 2 * src_wrap;
  }
  /* do last line */
  deinterlace_line_inplace(buf, src_m1, src_0, src_0, src_0, width);
  av_free(buf);
  return 0;
}

#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#  endif

FFMPEG_INLINE
int avpicture_deinterlace(
    AVPicture *dst, const AVPicture *src, enum AVPixelFormat pix_fmt, int width, int height)
{
  int i, ret;

  if (pix_fmt != AV_PIX_FMT_YUV420P && pix_fmt != AV_PIX_FMT_YUVJ420P &&
      pix_fmt != AV_PIX_FMT_YUV422P && pix_fmt != AV_PIX_FMT_YUVJ422P &&
      pix_fmt != AV_PIX_FMT_YUV444P && pix_fmt != AV_PIX_FMT_YUV411P &&
      pix_fmt != AV_PIX_FMT_GRAY8)
    return -1;
  if ((width & 3) != 0 || (height & 3) != 0)
    return -1;

  for (i = 0; i < 3; i++) {
    if (i == 1) {
      switch (pix_fmt) {
        case AV_PIX_FMT_YUVJ420P:
        case AV_PIX_FMT_YUV420P:
          width >>= 1;
          height >>= 1;
          break;
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUVJ422P:
          width >>= 1;
          break;
        case AV_PIX_FMT_YUV411P:
          width >>= 2;
          break;
        default:
          break;
      }
      if (pix_fmt == AV_PIX_FMT_GRAY8) {
        break;
      }
    }
    if (src == dst) {
      ret = deinterlace_bottom_field_inplace(dst->data[i], dst->linesize[i], width, height);
      if (ret < 0)
        return ret;
    }
    else {
      deinterlace_bottom_field(
          dst->data[i], dst->linesize[i], src->data[i], src->linesize[i], width, height);
    }
  }
  return 0;
}

#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif

#endif

#endif
