/* SPDX-FileCopyrightText: 2011 Peter Schlaile <peter [at] schlaile [dot] de>.
 * SPDX-FileCopyrightText: 2024-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_endian_switch.h"
#include "BLI_fileops.h"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "CLG_log.h"

#include "MOV_read.hh"

#include "ffmpeg_swscale.hh"
#include "movie_proxy_indexer.hh"
#include "movie_read.hh"
#include "movie_util.hh"

#ifdef WITH_FFMPEG
extern "C" {
#  include "ffmpeg_compat.h"
#  include <libavutil/imgutils.h>
}
#endif

namespace blender {

static CLG_LogRef LOG = {"video.proxy"};

static const IMB_Proxy_Size proxy_sizes[] = {
    IMB_PROXY_25, IMB_PROXY_50, IMB_PROXY_75, IMB_PROXY_100};
static const float proxy_fac[] = {0.25, 0.50, 0.75, 1.00};

static int proxy_size_to_array_index(IMB_Proxy_Size pr_size)
{
  switch (pr_size) {
    case IMB_PROXY_NONE:
      return -1;
    case IMB_PROXY_25:
      return 0;
    case IMB_PROXY_50:
      return 1;
    case IMB_PROXY_75:
      return 2;
    case IMB_PROXY_100:
      return 3;
    default:
      BLI_assert_msg(0, "Unhandled proxy size enum!");
      return -1;
  }
}

static void get_proxy_dir(const MovieReader *anim, char *proxy_dir, size_t proxy_dir_maxncpy)
{
  if (!anim->proxy_dir[0]) {
    char filename[FILE_MAXFILE];
    char dirname[FILE_MAXDIR];
    BLI_path_split_dir_file(anim->filepath, dirname, sizeof(dirname), filename, sizeof(filename));
    BLI_path_join(proxy_dir, proxy_dir_maxncpy, dirname, "BL_proxy", filename);
  }
  else {
    BLI_strncpy(proxy_dir, anim->proxy_dir, proxy_dir_maxncpy);
  }
}

static bool get_proxy_filepath(const MovieReader *anim,
                               IMB_Proxy_Size preview_size,
                               char *filepath,
                               bool temp)
{
  char proxy_dir[FILE_MAXDIR];
  int i = proxy_size_to_array_index(preview_size);

  BLI_assert(i >= 0);

  char proxy_name[FILE_MAXFILE];
  char stream_suffix[20];
  const char *name = (temp) ? "proxy_%d%s_part.avi" : "proxy_%d%s.avi";

  stream_suffix[0] = 0;

  if (anim->streamindex > 0) {
    SNPRINTF(stream_suffix, "_st%d", anim->streamindex);
  }

  SNPRINTF(proxy_name, name, int(proxy_fac[i] * 100), stream_suffix, anim->suffix);

  get_proxy_dir(anim, proxy_dir, sizeof(proxy_dir));

  if (BLI_path_ncmp(anim->filepath, proxy_dir, FILE_MAXDIR) == 0) {
    return false;
  }

  BLI_path_join(filepath, FILE_MAXFILE + FILE_MAXDIR, proxy_dir, proxy_name);
  return true;
}

/* ----------------------------------------------------------------------
 * - ffmpeg rebuilder
 * ---------------------------------------------------------------------- */

#ifdef WITH_FFMPEG

struct proxy_output_ctx {
  AVFormatContext *of;
  AVStream *st;
  AVCodecContext *c;
  const AVCodec *codec;
  SwsContext *sws_ctx;
  AVFrame *frame;
  int cfra;
  AVRational output_timebase;
  IMB_Proxy_Size proxy_size;
  int orig_height;
  MovieReader *anim;
};

static proxy_output_ctx *alloc_proxy_output_ffmpeg(MovieReader *anim,
                                                   AVCodecContext *codec_ctx,
                                                   AVStream *st,
                                                   IMB_Proxy_Size proxy_size,
                                                   int width,
                                                   int height,
                                                   int quality)
{
  proxy_output_ctx *rv = MEM_new_zeroed<proxy_output_ctx>("alloc_proxy_output");

  char filepath[FILE_MAX];

  rv->proxy_size = proxy_size;
  rv->anim = anim;

  get_proxy_filepath(rv->anim, rv->proxy_size, filepath, true);
  if (!BLI_file_ensure_parent_dir_exists(filepath)) {
    MEM_delete(rv);
    return nullptr;
  }

  rv->of = avformat_alloc_context();
  /* Note: we keep on using .avi extension for proxies,
   * but actual container can not be AVI, since it does not support
   * video rotation metadata. */
  rv->of->oformat = av_guess_format("mp4", nullptr, nullptr);

  rv->of->url = av_strdup(filepath);

  rv->st = avformat_new_stream(rv->of, nullptr);
  rv->st->id = 0;

  rv->codec = avcodec_find_encoder(AV_CODEC_ID_H264);

  rv->c = avcodec_alloc_context3(rv->codec);

  if (!rv->codec) {
    CLOG_ERROR(&LOG, "Could not build proxy '%s': failed to create video encoder", filepath);
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_delete(rv);
    return nullptr;
  }

  rv->c->width = width;
  rv->c->height = height;
  rv->c->gop_size = 10;
  rv->c->max_b_frames = 0;

  const enum AVPixelFormat *pix_fmts = ffmpeg_get_pix_fmts(rv->c, rv->codec);
  if (pix_fmts) {
    rv->c->pix_fmt = pix_fmts[0];
  }
  else {
    rv->c->pix_fmt = AV_PIX_FMT_YUVJ420P;
  }

  rv->c->sample_aspect_ratio = rv->st->sample_aspect_ratio = st->sample_aspect_ratio;

  /* Use same output timebase as input: we seek within the proxy file
   * using exact same frame numbers as if it was original file. So we want to
   * match original frame-rate, plus any variable frames in the source file. */
  rv->output_timebase = st->time_base;
  rv->c->time_base = st->time_base;
  rv->st->time_base = st->time_base;
  rv->st->avg_frame_rate = st->avg_frame_rate;

  /* This range matches #eFFMpegCrf. `crf_range_min` corresponds to lowest quality,
   * `crf_range_max` to highest quality. */
  const int crf_range_min = 32;
  const int crf_range_max = 17;
  int crf = round_fl_to_int((quality / 100.0f) * (crf_range_max - crf_range_min) + crf_range_min);

  AVDictionary *codec_opts = nullptr;
  /* High quality preset value. */
  av_dict_set_int(&codec_opts, "crf", crf, 0);
  /* Prefer smaller file-size. Presets from `veryslow` to `veryfast` produce output with very
   * similar file-size, but there is big difference in performance.
   * In some cases `veryfast` preset will produce smallest file-size. */
  av_dict_set(&codec_opts, "preset", "veryfast", 0);
  av_dict_set(&codec_opts, "tune", "fastdecode", 0);

  if (rv->codec->capabilities & AV_CODEC_CAP_OTHER_THREADS) {
    rv->c->thread_count = 0;
  }
  else {
    rv->c->thread_count = MOV_thread_count();
  }

  if (rv->codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
    rv->c->thread_type = FF_THREAD_FRAME;
  }
  else if (rv->codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
    rv->c->thread_type = FF_THREAD_SLICE;
  }

  if (rv->of->oformat->flags & AVFMT_GLOBALHEADER) {
    rv->c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  rv->c->color_range = codec_ctx->color_range;
  rv->c->color_primaries = codec_ctx->color_primaries;
  rv->c->color_trc = codec_ctx->color_trc;
  rv->c->colorspace = codec_ctx->colorspace;

  int ret = avio_open(&rv->of->pb, filepath, AVIO_FLAG_WRITE);

  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    CLOG_ERROR(&LOG,
               "Could not build proxy '%s': failed to create output file (%s)",
               filepath,
               error_str);
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_delete(rv);
    return nullptr;
  }

  ret = avcodec_open2(rv->c, rv->codec, &codec_opts);
  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    CLOG_ERROR(
        &LOG, "Could not build proxy '%s': failed to open video codec (%s)", filepath, error_str);
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_delete(rv);
    return nullptr;
  }

  avcodec_parameters_from_context(rv->st->codecpar, rv->c);
  ffmpeg_copy_display_matrix(st, rv->st);

  rv->orig_height = st->codecpar->height;

  if (st->codecpar->width != width || st->codecpar->height != height ||
      st->codecpar->format != rv->c->pix_fmt)
  {
    const size_t align = ffmpeg_get_buffer_alignment();
    rv->frame = av_frame_alloc();
    rv->frame->format = rv->c->pix_fmt;
    rv->frame->width = width;
    rv->frame->height = height;
    av_frame_get_buffer(rv->frame, align);

    rv->sws_ctx = ffmpeg_sws_get_context(st->codecpar->width,
                                         rv->orig_height,
                                         AVPixelFormat(st->codecpar->format),
                                         codec_ctx->color_range == AVCOL_RANGE_JPEG,
                                         -1,
                                         width,
                                         height,
                                         rv->c->pix_fmt,
                                         codec_ctx->color_range == AVCOL_RANGE_JPEG,
                                         -1,
                                         SWS_FAST_BILINEAR);
  }

  ret = avformat_write_header(rv->of, nullptr);
  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    CLOG_ERROR(
        &LOG, "Could not build proxy '%s': failed to write header (%s)", filepath, error_str);

    if (rv->frame) {
      av_frame_free(&rv->frame);
    }

    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_delete(rv);
    return nullptr;
  }

  return rv;
}

static void add_to_proxy_output_ffmpeg(proxy_output_ctx *ctx,
                                       AVFrame *frame,
                                       AVRational input_timebase)
{
  if (!ctx) {
    return;
  }

  const int64_t src_pts = frame ? frame->pts : AV_NOPTS_VALUE;

  if (ctx->sws_ctx && frame &&
      (frame->data[0] || frame->data[1] || frame->data[2] || frame->data[3]))
  {
    ffmpeg_sws_scale_frame(ctx->sws_ctx, ctx->frame, frame);
  }

  frame = ctx->sws_ctx ? (frame ? ctx->frame : nullptr) : frame;

  if (frame) {
    if (src_pts != AV_NOPTS_VALUE) {
      frame->pts = av_rescale_q(src_pts, input_timebase, ctx->output_timebase);
    }
    else {
      frame->pts = ctx->cfra;
    }
    ctx->cfra++;
  }

  int ret = avcodec_send_frame(ctx->c, frame);
  if (ret < 0) {
    /* Can't send frame to encoder. This shouldn't happen. */
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    CLOG_ERROR(
        &LOG, "Building proxy '%s': failed to send video frame (%s)", ctx->of->url, error_str);
    return;
  }
  AVPacket *packet = av_packet_alloc();

  while (ret >= 0) {
    ret = avcodec_receive_packet(ctx->c, packet);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      /* No more packets to flush. */
      break;
    }
    if (ret < 0) {
      char error_str[AV_ERROR_MAX_STRING_SIZE];
      av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

      CLOG_ERROR(&LOG,
                 "Building proxy '%s': error encoding frame #%i (%s)",
                 ctx->of->url,
                 ctx->cfra - 1,
                 error_str);
      break;
    }

    packet->stream_index = ctx->st->index;
    av_packet_rescale_ts(packet, ctx->c->time_base, ctx->st->time_base);
#  ifdef FFMPEG_USE_DURATION_WORKAROUND
    my_guess_pkt_duration(ctx->of, ctx->st, packet);
#  endif

    int write_ret = av_interleaved_write_frame(ctx->of, packet);
    if (write_ret != 0) {
      char error_str[AV_ERROR_MAX_STRING_SIZE];
      av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, write_ret);

      CLOG_ERROR(&LOG,
                 "Building proxy '%s': error writing frame #%i (%s)",
                 ctx->of->url,
                 ctx->cfra - 1,
                 error_str);
      break;
    }
  }

  av_packet_free(&packet);
}

static void free_proxy_output_ffmpeg(proxy_output_ctx *ctx, int rollback)
{
  char filepath[FILE_MAX];
  char filepath_tmp[FILE_MAX];

  if (!ctx) {
    return;
  }

  if (!rollback) {
    /* Flush the remaining packets. */
    add_to_proxy_output_ffmpeg(ctx, nullptr, {1, 1});
  }

  av_write_trailer(ctx->of);

  if (ctx->of->oformat) {
    if (!(ctx->of->oformat->flags & AVFMT_NOFILE)) {
      avio_close(ctx->of->pb);
    }
  }
  avcodec_free_context(&ctx->c);
  avformat_free_context(ctx->of);

  if (ctx->sws_ctx) {
    ffmpeg_sws_release_context(ctx->sws_ctx);
    ctx->sws_ctx = nullptr;
  }
  if (ctx->frame) {
    av_frame_free(&ctx->frame);
  }

  get_proxy_filepath(ctx->anim, ctx->proxy_size, filepath_tmp, true);

  if (rollback) {
    BLI_delete(filepath_tmp, false, false);
  }
  else {
    get_proxy_filepath(ctx->anim, ctx->proxy_size, filepath, false);
    BLI_rename_overwrite(filepath_tmp, filepath);
  }

  MEM_delete(ctx);
}

struct MovieProxyBuilder {

  AVFormatContext *iFormatCtx;
  AVCodecContext *iCodecCtx;
  const AVCodec *iCodec;
  AVStream *iStream;
  int videoStream;

  int num_proxy_sizes;

  proxy_output_ctx *proxy_ctx[IMB_PROXY_MAX_SLOT];

  int proxy_sizes_in_use;

  bool build_only_on_bad_performance;
  bool building_cancelled;
};

static MovieProxyBuilder *proxy_builder_create(MovieReader *anim,
                                               int proxy_sizes_in_use,
                                               int quality,
                                               bool build_only_on_bad_performance)
{
  /* Never build proxies for un-seekable single frame files. */
  if (anim->never_seek_decode_one_frame) {
    return nullptr;
  }

  MovieProxyBuilder *context = MEM_new_zeroed<MovieProxyBuilder>(__func__);
  int num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  int i, streamcount;

  context->proxy_sizes_in_use = proxy_sizes_in_use;
  context->num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  context->build_only_on_bad_performance = build_only_on_bad_performance;

  memset(context->proxy_ctx, 0, sizeof(context->proxy_ctx));

  if (avformat_open_input(&context->iFormatCtx, anim->filepath, nullptr, nullptr) != 0) {
    MEM_delete(context);
    return nullptr;
  }

  if (avformat_find_stream_info(context->iFormatCtx, nullptr) < 0) {
    avformat_close_input(&context->iFormatCtx);
    MEM_delete(context);
    return nullptr;
  }

  streamcount = anim->streamindex;

  /* Find the video stream */
  context->videoStream = -1;
  for (i = 0; i < context->iFormatCtx->nb_streams; i++) {
    if (context->iFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (streamcount > 0) {
        streamcount--;
        continue;
      }
      context->videoStream = i;
      break;
    }
  }

  if (context->videoStream == -1) {
    avformat_close_input(&context->iFormatCtx);
    MEM_delete(context);
    return nullptr;
  }

  context->iStream = context->iFormatCtx->streams[context->videoStream];

  context->iCodec = avcodec_find_decoder(context->iStream->codecpar->codec_id);

  if (context->iCodec == nullptr) {
    avformat_close_input(&context->iFormatCtx);
    MEM_delete(context);
    return nullptr;
  }

  context->iCodecCtx = avcodec_alloc_context3(nullptr);
  avcodec_parameters_to_context(context->iCodecCtx, context->iStream->codecpar);
  context->iCodecCtx->workaround_bugs = FF_BUG_AUTODETECT;

  if (context->iCodec->capabilities & AV_CODEC_CAP_OTHER_THREADS) {
    context->iCodecCtx->thread_count = 0;
  }
  else {
    context->iCodecCtx->thread_count = MOV_thread_count();
  }

  if (context->iCodec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
    context->iCodecCtx->thread_type = FF_THREAD_FRAME;
  }
  else if (context->iCodec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
    context->iCodecCtx->thread_type = FF_THREAD_SLICE;
  }

  if (avcodec_open2(context->iCodecCtx, context->iCodec, nullptr) < 0) {
    avformat_close_input(&context->iFormatCtx);
    avcodec_free_context(&context->iCodecCtx);
    MEM_delete(context);
    return nullptr;
  }

  for (i = 0; i < num_proxy_sizes; i++) {
    if (proxy_sizes_in_use & proxy_sizes[i]) {
      int width = context->iCodecCtx->width * proxy_fac[i];
      int height = context->iCodecCtx->height * proxy_fac[i];
      width += width % 2;
      height += height % 2;
      context->proxy_ctx[i] = alloc_proxy_output_ffmpeg(
          anim, context->iCodecCtx, context->iStream, proxy_sizes[i], width, height, quality);
      if (!context->proxy_ctx[i]) {
        proxy_sizes_in_use &= ~int(proxy_sizes[i]);
      }
    }
  }

  if (context->proxy_ctx[0] == nullptr && context->proxy_ctx[1] == nullptr &&
      context->proxy_ctx[2] == nullptr && context->proxy_ctx[3] == nullptr)
  {
    avformat_close_input(&context->iFormatCtx);
    avcodec_free_context(&context->iCodecCtx);
    MEM_delete(context);
    return nullptr; /* Nothing to transcode. */
  }

  return context;
}

static void proxy_builder_finish(MovieProxyBuilder *context, const bool stop)
{
  const bool do_rollback = stop || context->building_cancelled;

  for (int i = 0; i < context->num_proxy_sizes; i++) {
    if (context->proxy_sizes_in_use & proxy_sizes[i]) {
      free_proxy_output_ffmpeg(context->proxy_ctx[i], do_rollback);
    }
  }

  avcodec_free_context(&context->iCodecCtx);
  avformat_close_input(&context->iFormatCtx);

  MEM_delete(context);
}

static void proxy_builder_proc_decoded_frame(MovieProxyBuilder *context, AVFrame *in_frame)
{
  for (int i = 0; i < context->num_proxy_sizes; i++) {
    add_to_proxy_output_ffmpeg(context->proxy_ctx[i], in_frame, context->iStream->time_base);
  }
}

static int proxy_builder_process(MovieProxyBuilder *context,
                                 const bool *stop,
                                 bool *do_update,
                                 const FunctionRef<void(float progress)> set_progress_fn)
{
  AVFrame *in_frame = av_frame_alloc();
  AVPacket *next_packet = av_packet_alloc();
  uint64_t stream_size = avio_size(context->iFormatCtx->pb);

  float progress = 0.0f;
  while (av_read_frame(context->iFormatCtx, next_packet) >= 0) {
    float next_progress =
        float(int(floor(double(next_packet->pos) * 100 / double(stream_size) + 0.5))) / 100;

    if (progress != next_progress) {
      progress = next_progress;
      *do_update = true;
      if (set_progress_fn) {
        set_progress_fn(progress);
      }
    }

    if (*stop) {
      break;
    }

    if (next_packet->stream_index == context->videoStream) {
      int ret = avcodec_send_packet(context->iCodecCtx, next_packet);
      while (ret >= 0) {
        ret = avcodec_receive_frame(context->iCodecCtx, in_frame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          /* No more frames to flush. */
          break;
        }
        if (ret < 0) {
          char error_str[AV_ERROR_MAX_STRING_SIZE];
          av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
          CLOG_ERROR(&LOG, "Error decoding proxy frame: %s", error_str);
          break;
        }

        proxy_builder_proc_decoded_frame(context, in_frame);
      }
    }
    av_packet_unref(next_packet);
  }

  /* process pictures still stuck in decoder engine after EOF
   * according to ffmpeg docs using nullptr packets.
   *
   * At least, if we haven't already stopped... */

  if (!*stop) {
    int ret = avcodec_send_packet(context->iCodecCtx, nullptr);

    while (ret >= 0) {
      ret = avcodec_receive_frame(context->iCodecCtx, in_frame);

      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        /* No more frames to flush. */
        break;
      }
      if (ret < 0) {
        char error_str[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
        CLOG_ERROR(&LOG, "Error flushing proxy frame: %s", error_str);
        break;
      }
      proxy_builder_proc_decoded_frame(context, in_frame);
    }
  }

  av_packet_free(&next_packet);
  av_free(in_frame);

  return 1;
}

/* Get number of frames, that can be decoded in specified time period. */
static int performance_get_decode_rate(MovieProxyBuilder *context, const double time_period)
{
  AVFrame *in_frame = av_frame_alloc();
  AVPacket *packet = av_packet_alloc();

  const double start = BLI_time_now_seconds();
  int frames_decoded = 0;

  while (av_read_frame(context->iFormatCtx, packet) >= 0) {
    if (packet->stream_index != context->videoStream) {
      av_packet_unref(packet);
      continue;
    }

    int ret = avcodec_send_packet(context->iCodecCtx, packet);
    while (ret >= 0) {
      ret = avcodec_receive_frame(context->iCodecCtx, in_frame);

      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      }

      if (ret < 0) {
        char error_str[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
        CLOG_ERROR(&LOG, "Error decoding proxy frame: %s", error_str);
        break;
      }
      frames_decoded++;
    }

    const double end = BLI_time_now_seconds();

    if (end > start + time_period) {
      break;
    }
    av_packet_unref(packet);
  }

  av_packet_free(&packet);
  av_frame_free(&in_frame);

  avcodec_flush_buffers(context->iCodecCtx);
  av_seek_frame(context->iFormatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
  return frames_decoded;
}

/* Read up to 10k movie packets and return max GOP size detected.
 * Number of packets is arbitrary. It should be as large as possible, but processed within
 * reasonable time period, so detected GOP size is as close to real as possible. */
static int performance_get_max_gop_size(MovieProxyBuilder *context)
{
  AVPacket *packet = av_packet_alloc();

  const int packets_max = 10000;
  int packet_index = 0;
  int max_gop = 0;
  int cur_gop = 0;

  while (av_read_frame(context->iFormatCtx, packet) >= 0) {
    if (packet->stream_index != context->videoStream) {
      av_packet_unref(packet);
      continue;
    }
    packet_index++;
    cur_gop++;

    if (packet->flags & AV_PKT_FLAG_KEY) {
      max_gop = max_ii(max_gop, cur_gop);
      cur_gop = 0;
    }

    if (packet_index > packets_max) {
      break;
    }
    av_packet_unref(packet);
  }

  av_packet_free(&packet);

  av_seek_frame(context->iFormatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
  return max_gop;
}

/* Assess scrubbing performance of provided file. This function is not meant to be very exact.
 * It compares number of frames decoded in reasonable time with largest detected GOP size.
 * Because seeking happens in single GOP, it means, that maximum seek time can be detected this
 * way.
 * Since proxies use GOP size of 10 frames, skip building if detected GOP size is less or
 * equal.
 */
static bool need_to_build_proxy(MovieProxyBuilder *context)
{
  if (!context->build_only_on_bad_performance) {
    return true;
  }

  /* Make sure, that file is not cold read. */
  performance_get_decode_rate(context, 0.1);
  /* Get decode rate per 100ms. This is arbitrary, but seems to be good baseline cadence of
   * seeking. */
  const int decode_rate = performance_get_decode_rate(context, 0.1);
  const int max_gop_size = performance_get_max_gop_size(context);

  if (max_gop_size <= 10 || max_gop_size < decode_rate) {
    CLOG_INFO_NOCHECK(&LOG,
                      "Skipping proxy building for %s: Decoding performance is already good.",
                      context->iFormatCtx->url);
    context->building_cancelled = true;
    return false;
  }

  return true;
}

#endif

/* ----------------------------------------------------------------------
 * - public API
 * ---------------------------------------------------------------------- */

MovieProxyBuilder *MOV_proxy_builder_start(MovieReader *anim,
                                           int proxy_sizes_in_use,
                                           int quality,
                                           const bool overwrite,
                                           Set<std::string> *processed_paths,
                                           bool build_only_on_bad_performance)
{
  int proxy_sizes_to_build = proxy_sizes_in_use;

  /* Check which proxies are going to be generated in this session already. */
  if (processed_paths != nullptr) {
    for (int i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
      IMB_Proxy_Size proxy_size = proxy_sizes[i];
      if ((proxy_size & proxy_sizes_to_build) == 0) {
        continue;
      }
      char filepath[FILE_MAX];
      if (!get_proxy_filepath(anim, proxy_size, filepath, false)) {
        return nullptr;
      }
      if (!processed_paths->add(filepath)) {
        proxy_sizes_to_build &= ~int(proxy_size);
      }
    }
  }

  /* When not overwriting existing proxies, skip the ones that already exist. */
  if (!overwrite) {
    int built_proxies = MOV_get_existing_proxies(anim);
    if (built_proxies != 0) {
      for (int i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
        IMB_Proxy_Size proxy_size = proxy_sizes[i];
        if (proxy_size & built_proxies) {
          char filepath[FILE_MAX];
          if (!get_proxy_filepath(anim, proxy_size, filepath, false)) {
            return nullptr;
          }
          CLOG_INFO_NOCHECK(&LOG, "Skipping proxy: %s", filepath);
        }
      }
    }
    proxy_sizes_to_build &= ~built_proxies;
  }

  if (proxy_sizes_to_build == 0) {
    return nullptr;
  }

  MovieProxyBuilder *context = nullptr;
#ifdef WITH_FFMPEG
  if (anim->state == MovieReader::State::Valid) {
    context = proxy_builder_create(
        anim, proxy_sizes_to_build, quality, build_only_on_bad_performance);
  }
#else
  UNUSED_VARS(build_only_on_bad_performance);
#endif

  return context;

  UNUSED_VARS(proxy_sizes_in_use, quality);
}

void MOV_proxy_builder_process(MovieProxyBuilder *context,
                               /* NOLINTNEXTLINE: readability-non-const-parameter. */
                               const bool *stop,
                               /* NOLINTNEXTLINE: readability-non-const-parameter. */
                               bool *do_update,
                               const FunctionRef<void(float progress)> set_progress_fn)
{
#ifdef WITH_FFMPEG
  if (context != nullptr) {
    if (need_to_build_proxy(context)) {
      proxy_builder_process(context, stop, do_update, set_progress_fn);
    }
  }
#endif
  UNUSED_VARS(context, stop, do_update, set_progress_fn);
}

void MOV_proxy_builder_finish(MovieProxyBuilder *context, const bool stop)
{
#ifdef WITH_FFMPEG
  if (context != nullptr) {
    proxy_builder_finish(context, stop);
  }
#endif
  /* static defined at top of the file */
  UNUSED_VARS(context, stop, proxy_sizes);
}

void MOV_close_proxies(MovieReader *anim)
{
  if (anim == nullptr) {
    return;
  }

  for (int i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
    if (anim->proxy_anim[i]) {
      MOV_close(anim->proxy_anim[i]);
      anim->proxy_anim[i] = nullptr;
    }
  }

  anim->proxies_tried = 0;
}

void MOV_set_custom_proxy_dir(MovieReader *anim, const char *dir)
{
  if (STREQ(anim->proxy_dir, dir)) {
    return;
  }
  STRNCPY(anim->proxy_dir, dir);

  MOV_close_proxies(anim);
}

MovieReader *movie_open_proxy(MovieReader *anim, IMB_Proxy_Size preview_size)
{
  char filepath[FILE_MAX];
  int i = proxy_size_to_array_index(preview_size);

  if (i < 0) {
    return nullptr;
  }

  if (anim->proxy_anim[i]) {
    return anim->proxy_anim[i];
  }

  if (anim->proxies_tried & preview_size) {
    return nullptr;
  }

  get_proxy_filepath(anim, preview_size, filepath, false);

  /* Proxies are generated in the same color space as animation itself.
   *
   * Also skip any colorspace conversion to the color pipeline design as it helps performance and
   * the image buffers from the proxy builder are not used anywhere else in Blender. */
  anim->proxy_anim[i] = MOV_open_file(filepath, ImBufFlags::Zero, 0, true, anim->colorspace);

  anim->proxies_tried |= preview_size;

  return anim->proxy_anim[i];
}

int MOV_get_existing_proxies(const MovieReader *anim)
{
  const int num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  int existing = IMB_PROXY_NONE;
  for (int i = 0; i < num_proxy_sizes; i++) {
    IMB_Proxy_Size proxy_size = proxy_sizes[i];
    char filepath[FILE_MAX];
    get_proxy_filepath(anim, proxy_size, filepath, false);
    if (BLI_exists(filepath)) {
      existing |= int(proxy_size);
    }
  }
  return existing;
}

}  // namespace blender
