/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

extern "C" {
#include "ffmpeg_compat.h"

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
}

namespace {

bool test_vcodec(const AVCodec *codec, AVPixelFormat pixelformat)
{
  av_log_set_level(AV_LOG_QUIET);
  bool result = false;
  if (codec) {
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (ctx) {
      ctx->time_base.num = 1;
      ctx->time_base.den = 25;
      ctx->pix_fmt = pixelformat;
      ctx->width = 720;
      ctx->height = 576;
      int open = avcodec_open2(ctx, codec, NULL);
      if (open >= 0) {
        avcodec_free_context(&ctx);
        result = true;
      }
    }
  }
  return result;
}
bool test_acodec(const AVCodec *codec, AVSampleFormat fmt)
{
  av_log_set_level(AV_LOG_QUIET);
  bool result = false;
  if (codec) {
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (ctx) {
      ctx->sample_fmt = fmt;
      ctx->sample_rate = 48000;
#ifdef FFMPEG_USE_OLD_CHANNEL_VARS
      ctx->channel_layout = AV_CH_LAYOUT_MONO;
#else
      av_channel_layout_from_mask(&ctx->ch_layout, AV_CH_LAYOUT_MONO);
#endif
      ctx->bit_rate = 128000;
      int open = avcodec_open2(ctx, codec, NULL);
      if (open >= 0) {
        avcodec_free_context(&ctx);
        result = true;
      }
    }
  }
  return result;
}

bool test_codec_video_by_codecid(AVCodecID codec_id, AVPixelFormat pixelformat)
{
  bool result = false;
  const AVCodec *codec = avcodec_find_encoder(codec_id);
  if (codec)
    result = test_vcodec(codec, pixelformat);
  return result;
}

bool test_codec_video_by_name(const char *codecname, AVPixelFormat pixelformat)
{
  bool result = false;
  const AVCodec *codec = avcodec_find_encoder_by_name(codecname);
  if (codec)
    result = test_vcodec(codec, pixelformat);
  return result;
}

bool test_codec_audio_by_codecid(AVCodecID codec_id, AVSampleFormat fmt)
{
  bool result = false;
  const AVCodec *codec = avcodec_find_encoder(codec_id);
  if (codec)
    result = test_acodec(codec, fmt);
  return result;
}

bool test_codec_audio_by_name(const char *codecname, AVSampleFormat fmt)
{
  bool result = false;
  const AVCodec *codec = avcodec_find_encoder_by_name(codecname);
  if (codec)
    result = test_acodec(codec, fmt);
  return result;
}

#define str(s) #s
#define FFMPEG_TEST_VCODEC_ID(codec, fmt) \
  TEST(ffmpeg, codec##_##fmt) \
  { \
    EXPECT_TRUE(test_codec_video_by_codecid(codec, fmt)); \
  }

#define FFMPEG_TEST_VCODEC_NAME(codec, fmt) \
  TEST(ffmpeg, codec##_##fmt) \
  { \
    EXPECT_TRUE(test_codec_video_by_name(str(codec), fmt)); \
  }

#define FFMPEG_TEST_ACODEC_ID(codec, fmt) \
  TEST(ffmpeg, codec##_##fmt) \
  { \
    EXPECT_TRUE(test_codec_audio_by_codecid(codec, fmt)); \
  }

#define FFMPEG_TEST_ACODEC_NAME(codec, fmt) \
  TEST(ffmpeg, codec) \
  { \
    EXPECT_TRUE(test_codec_audio_by_name(str(codec), fmt)); \
  }

}  // namespace

/* generic codec ID's used in blender */

FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_HUFFYUV, AV_PIX_FMT_BGRA)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_HUFFYUV, AV_PIX_FMT_RGB32)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_FFV1, AV_PIX_FMT_RGB32)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_QTRLE, AV_PIX_FMT_ARGB)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_VP9, AV_PIX_FMT_YUVA420P)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_PNG, AV_PIX_FMT_RGBA)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_H264, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_MPEG4, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_THEORA, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_DVVIDEO, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_MPEG1VIDEO, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_MPEG2VIDEO, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_ID(AV_CODEC_ID_FLV1, AV_PIX_FMT_YUV420P)

/* Audio codecs */

FFMPEG_TEST_ACODEC_ID(AV_CODEC_ID_AAC, AV_SAMPLE_FMT_FLTP)
FFMPEG_TEST_ACODEC_ID(AV_CODEC_ID_AC3, AV_SAMPLE_FMT_FLTP)
FFMPEG_TEST_ACODEC_ID(AV_CODEC_ID_FLAC, AV_SAMPLE_FMT_S16)
FFMPEG_TEST_ACODEC_ID(AV_CODEC_ID_MP2, AV_SAMPLE_FMT_S16)
FFMPEG_TEST_ACODEC_ID(AV_CODEC_ID_MP3, AV_SAMPLE_FMT_FLTP)
FFMPEG_TEST_ACODEC_ID(AV_CODEC_ID_OPUS, AV_SAMPLE_FMT_FLT)
FFMPEG_TEST_ACODEC_ID(AV_CODEC_ID_PCM_S16LE, AV_SAMPLE_FMT_S16)
FFMPEG_TEST_ACODEC_ID(AV_CODEC_ID_VORBIS, AV_SAMPLE_FMT_FLTP)

/* Libraries we count on ffmpeg being linked against */

FFMPEG_TEST_VCODEC_NAME(libtheora, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_NAME(libx264, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_NAME(libvpx, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_NAME(libopenjpeg, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_VCODEC_NAME(libxvid, AV_PIX_FMT_YUV420P)
FFMPEG_TEST_ACODEC_NAME(libvorbis, AV_SAMPLE_FMT_FLTP)
FFMPEG_TEST_ACODEC_NAME(libopus, AV_SAMPLE_FMT_FLT)
FFMPEG_TEST_ACODEC_NAME(libmp3lame, AV_SAMPLE_FMT_FLTP)
