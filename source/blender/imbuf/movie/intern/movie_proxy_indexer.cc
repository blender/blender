/* SPDX-FileCopyrightText: 2011 Peter Schlaile <peter [at] schlaile [dot] de>.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.h"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_threads.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "MOV_read.hh"

#include "ffmpeg_swscale.hh"
#include "movie_proxy_indexer.hh"
#include "movie_read.hh"

#ifdef WITH_FFMPEG
extern "C" {
#  include "ffmpeg_compat.h"
#  include <libavutil/imgutils.h>
}

static const char temp_ext[] = "_part";
#endif

static const char binary_header_str[] = "BlenMIdx";

static const IMB_Proxy_Size proxy_sizes[] = {
    IMB_PROXY_25, IMB_PROXY_50, IMB_PROXY_75, IMB_PROXY_100};
static const float proxy_fac[] = {0.25, 0.50, 0.75, 1.00};

#define INDEX_FILE_VERSION 2

/* ----------------------------------------------------------------------
 * - time code index functions
 * ---------------------------------------------------------------------- */

#ifdef WITH_FFMPEG

struct MovieIndexBuilder {
  FILE *fp;
  char filepath[FILE_MAX];
  char filepath_temp[FILE_MAX];
};

static MovieIndexBuilder *index_builder_create(const char *filepath)
{
  MovieIndexBuilder *rv = MEM_cnew<MovieIndexBuilder>("index builder");

  STRNCPY(rv->filepath, filepath);

  STRNCPY(rv->filepath_temp, filepath);
  BLI_string_join(rv->filepath_temp, sizeof(rv->filepath_temp), filepath, temp_ext);

  BLI_file_ensure_parent_dir_exists(rv->filepath_temp);

  rv->fp = BLI_fopen(rv->filepath_temp, "wb");

  if (!rv->fp) {
    fprintf(stderr,
            "Failed to build index for '%s': could not open '%s' for writing\n",
            filepath,
            rv->filepath_temp);
    MEM_freeN(rv);
    return nullptr;
  }

  fprintf(rv->fp,
          "%s%c%.3d",
          binary_header_str,
          (ENDIAN_ORDER == B_ENDIAN) ? 'V' : 'v',
          INDEX_FILE_VERSION);

  return rv;
}

static void index_builder_add_entry(
    MovieIndexBuilder *fp, int frameno, uint64_t seek_pos_pts, uint64_t seek_pos_dts, uint64_t pts)
{
  uint64_t pad = 0;
  fwrite(&frameno, sizeof(int), 1, fp->fp);
  fwrite(&pad, sizeof(uint64_t), 1, fp->fp);
  fwrite(&seek_pos_pts, sizeof(uint64_t), 1, fp->fp);
  fwrite(&seek_pos_dts, sizeof(uint64_t), 1, fp->fp);
  fwrite(&pts, sizeof(uint64_t), 1, fp->fp);
}

static void index_builder_finish(MovieIndexBuilder *fp, bool rollback)
{
  fclose(fp->fp);

  if (rollback) {
    BLI_delete(fp->filepath_temp, false, false);
  }
  else {
    BLI_rename_overwrite(fp->filepath_temp, fp->filepath);
  }

  MEM_freeN(fp);
}

#endif

static MovieIndex *movie_index_open(const char *filepath)
{
  FILE *fp = BLI_fopen(filepath, "rb");
  if (!fp) {
    return nullptr;
  }

  constexpr int64_t header_size = 12;
  char header[header_size + 1];
  if (fread(header, header_size, 1, fp) != 1) {
    fprintf(stderr, "Couldn't read indexer file: %s\n", filepath);
    fclose(fp);
    return nullptr;
  }

  header[header_size] = 0;

  if (memcmp(header, binary_header_str, 8) != 0) {
    fprintf(stderr, "Error reading %s: Binary file type string mismatch\n", filepath);
    fclose(fp);
    return nullptr;
  }

  if (atoi(header + 9) != INDEX_FILE_VERSION) {
    fprintf(stderr, "Error reading %s: File version mismatch\n", filepath);
    fclose(fp);
    return nullptr;
  }

  MovieIndex *idx = MEM_new<MovieIndex>("MovieIndex");

  STRNCPY(idx->filepath, filepath);

  fseek(fp, 0, SEEK_END);

  constexpr int64_t entry_size = sizeof(int) +      /* framepos */
                                 sizeof(uint64_t) + /* _pad */
                                 sizeof(uint64_t) + /* seek_pos_pts */
                                 sizeof(uint64_t) + /* seek_pos_dts */
                                 sizeof(uint64_t);  /* pts */

  int64_t num_entries = (ftell(fp) - header_size) / entry_size;
  fseek(fp, header_size, SEEK_SET);

  idx->entries.resize(num_entries);

  int64_t items_read = 0;
  uint64_t pad;
  for (int64_t i = 0; i < num_entries; i++) {
    items_read += fread(&idx->entries[i].frameno, sizeof(int), 1, fp);
    items_read += fread(&pad, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].seek_pos_pts, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].seek_pos_dts, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].pts, sizeof(uint64_t), 1, fp);
  }

  if (items_read != num_entries * 5) {
    fprintf(stderr, "Error: Element data size mismatch in: %s\n", filepath);
    MEM_delete(idx);
    fclose(fp);
    return nullptr;
  }

  if ((ENDIAN_ORDER == B_ENDIAN) != (header[8] == 'V')) {
    for (int64_t i = 0; i < num_entries; i++) {
      BLI_endian_switch_int32(&idx->entries[i].frameno);
      BLI_endian_switch_uint64(&idx->entries[i].seek_pos_pts);
      BLI_endian_switch_uint64(&idx->entries[i].seek_pos_dts);
      BLI_endian_switch_uint64(&idx->entries[i].pts);
    }
  }

  fclose(fp);

  return idx;
}

uint64_t MovieIndex::get_seek_pos_pts(int frame_index) const
{
  frame_index = blender::math::clamp<int>(frame_index, 0, this->entries.size() - 1);
  return this->entries[frame_index].seek_pos_pts;
}

uint64_t MovieIndex::get_seek_pos_dts(int frame_index) const
{
  frame_index = blender::math::clamp<int>(frame_index, 0, this->entries.size() - 1);
  return this->entries[frame_index].seek_pos_dts;
}

int MovieIndex::get_frame_index(int frameno) const
{
  int len = int(this->entries.size());
  int first = 0;

  /* Binary-search (lower bound) the right index. */
  while (len > 0) {
    int half = len >> 1;
    int middle = first + half;

    if (this->entries[middle].frameno < frameno) {
      first = middle;
      first++;
      len = len - half - 1;
    }
    else {
      len = half;
    }
  }

  if (first == this->entries.size()) {
    return int(this->entries.size()) - 1;
  }

  return first;
}

uint64_t MovieIndex::get_pts(int frame_index) const
{
  frame_index = blender::math::clamp<int>(frame_index, 0, this->entries.size() - 1);
  return this->entries[frame_index].pts;
}

int MovieIndex::get_duration() const
{
  if (this->entries.is_empty()) {
    return 0;
  }
  return this->entries.last().frameno + 1;
}

static void movie_index_free(MovieIndex *idx)
{
  MEM_delete(idx);
}

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

/* ----------------------------------------------------------------------
 * - rebuild helper functions
 * ---------------------------------------------------------------------- */

static void get_index_dir(const MovieReader *anim, char *index_dir, size_t index_dir_maxncpy)
{
  if (!anim->index_dir[0]) {
    char filename[FILE_MAXFILE];
    char dirname[FILE_MAXDIR];
    BLI_path_split_dir_file(anim->filepath, dirname, sizeof(dirname), filename, sizeof(filename));
    BLI_path_join(index_dir, index_dir_maxncpy, dirname, "BL_proxy", filename);
  }
  else {
    BLI_strncpy(index_dir, anim->index_dir, index_dir_maxncpy);
  }
}

static bool get_proxy_filepath(const MovieReader *anim,
                               IMB_Proxy_Size preview_size,
                               char *filepath,
                               bool temp)
{
  char index_dir[FILE_MAXDIR];
  int i = proxy_size_to_array_index(preview_size);

  BLI_assert(i >= 0);

  char proxy_name[256];
  char stream_suffix[20];
  const char *name = (temp) ? "proxy_%d%s_part.avi" : "proxy_%d%s.avi";

  stream_suffix[0] = 0;

  if (anim->streamindex > 0) {
    SNPRINTF(stream_suffix, "_st%d", anim->streamindex);
  }

  SNPRINTF(proxy_name, name, int(proxy_fac[i] * 100), stream_suffix, anim->suffix);

  get_index_dir(anim, index_dir, sizeof(index_dir));

  if (BLI_path_ncmp(anim->filepath, index_dir, FILE_MAXDIR) == 0) {
    return false;
  }

  BLI_path_join(filepath, FILE_MAXFILE + FILE_MAXDIR, index_dir, proxy_name);
  return true;
}

static void get_tc_filepath(MovieReader *anim, IMB_Timecode_Type tc, char *filepath)
{
  char index_dir[FILE_MAXDIR];
  int i = tc == IMB_TC_RECORD_RUN_NO_GAPS ? 1 : 0;

  const char *index_names[] = {
      "record_run%s%s.blen_tc",
      "record_run_no_gaps%s%s.blen_tc",
  };

  char stream_suffix[20];
  char index_name[256];

  stream_suffix[0] = 0;

  if (anim->streamindex > 0) {
    SNPRINTF(stream_suffix, "_st%d", anim->streamindex);
  }

  SNPRINTF(index_name, index_names[i], stream_suffix, anim->suffix);

  get_index_dir(anim, index_dir, sizeof(index_dir));

  BLI_path_join(filepath, FILE_MAXFILE + FILE_MAXDIR, index_dir, index_name);
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
  proxy_output_ctx *rv = MEM_cnew<proxy_output_ctx>("alloc_proxy_output");

  char filepath[FILE_MAX];

  rv->proxy_size = proxy_size;
  rv->anim = anim;

  get_proxy_filepath(rv->anim, rv->proxy_size, filepath, true);
  if (!BLI_file_ensure_parent_dir_exists(filepath)) {
    MEM_freeN(rv);
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
    fprintf(stderr, "Could not build proxy '%s': failed to create video encoder\n", filepath);
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_freeN(rv);
    return nullptr;
  }

  rv->c->width = width;
  rv->c->height = height;
  rv->c->gop_size = 10;
  rv->c->max_b_frames = 0;

  if (rv->codec->pix_fmts) {
    rv->c->pix_fmt = rv->codec->pix_fmts[0];
  }
  else {
    rv->c->pix_fmt = AV_PIX_FMT_YUVJ420P;
  }

  rv->c->sample_aspect_ratio = rv->st->sample_aspect_ratio = st->sample_aspect_ratio;

  rv->c->time_base.den = 25;
  rv->c->time_base.num = 1;
  rv->st->time_base = rv->c->time_base;

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
    rv->c->thread_count = BLI_system_thread_count();
  }

  if (rv->codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
    rv->c->thread_type = FF_THREAD_FRAME;
  }
  else if (rv->codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
    rv->c->thread_type = FF_THREAD_SLICE;
  }

  if (rv->of->flags & AVFMT_GLOBALHEADER) {
    rv->c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  rv->c->color_range = codec_ctx->color_range;
  rv->c->color_primaries = codec_ctx->color_primaries;
  rv->c->color_trc = codec_ctx->color_trc;
  rv->c->colorspace = codec_ctx->colorspace;

  avcodec_parameters_from_context(rv->st->codecpar, rv->c);

  ffmpeg_copy_display_matrix(st, rv->st);

  int ret = avio_open(&rv->of->pb, filepath, AVIO_FLAG_WRITE);

  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    fprintf(stderr,
            "Could not build proxy '%s': failed to create output file (%s)\n",
            filepath,
            error_str);
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_freeN(rv);
    return nullptr;
  }

  ret = avcodec_open2(rv->c, rv->codec, &codec_opts);
  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    fprintf(stderr,
            "Could not build proxy '%s': failed to open video codec (%s)\n",
            filepath,
            error_str);
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_freeN(rv);
    return nullptr;
  }

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
                                         width,
                                         height,
                                         rv->c->pix_fmt,
                                         SWS_FAST_BILINEAR);
  }

  ret = avformat_write_header(rv->of, nullptr);
  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    fprintf(
        stderr, "Could not build proxy '%s': failed to write header (%s)\n", filepath, error_str);

    if (rv->frame) {
      av_frame_free(&rv->frame);
    }

    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_freeN(rv);
    return nullptr;
  }

  return rv;
}

static void add_to_proxy_output_ffmpeg(proxy_output_ctx *ctx, AVFrame *frame)
{
  if (!ctx) {
    return;
  }

  if (ctx->sws_ctx && frame &&
      (frame->data[0] || frame->data[1] || frame->data[2] || frame->data[3]))
  {
    ffmpeg_sws_scale_frame(ctx->sws_ctx, ctx->frame, frame);
  }

  frame = ctx->sws_ctx ? (frame ? ctx->frame : nullptr) : frame;

  if (frame) {
    frame->pts = ctx->cfra++;
  }

  int ret = avcodec_send_frame(ctx->c, frame);
  if (ret < 0) {
    /* Can't send frame to encoder. This shouldn't happen. */
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    fprintf(
        stderr, "Building proxy '%s': failed to send video frame (%s)\n", ctx->of->url, error_str);
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

      fprintf(stderr,
              "Building proxy '%s': error encoding frame #%i (%s)\n",
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

      fprintf(stderr,
              "Building proxy '%s': error writing frame #%i (%s)\n",
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
    add_to_proxy_output_ffmpeg(ctx, nullptr);
  }

  avcodec_flush_buffers(ctx->c);

  av_write_trailer(ctx->of);

  avcodec_free_context(&ctx->c);

  if (ctx->of->oformat) {
    if (!(ctx->of->oformat->flags & AVFMT_NOFILE)) {
      avio_close(ctx->of->pb);
    }
  }
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

  MEM_freeN(ctx);
}

static IMB_Timecode_Type tc_types[IMB_TC_NUM_TYPES] = {IMB_TC_RECORD_RUN,
                                                       IMB_TC_RECORD_RUN_NO_GAPS};

struct MovieProxyBuilder {

  AVFormatContext *iFormatCtx;
  AVCodecContext *iCodecCtx;
  const AVCodec *iCodec;
  AVStream *iStream;
  int videoStream;

  int num_proxy_sizes;

  proxy_output_ctx *proxy_ctx[IMB_PROXY_MAX_SLOT];
  MovieIndexBuilder *indexer[IMB_TC_NUM_TYPES];

  int tcs_in_use;
  int proxy_sizes_in_use;

  uint64_t seek_pos_pts;
  uint64_t seek_pos_dts;
  uint64_t last_seek_pos_pts;
  uint64_t last_seek_pos_dts;
  uint64_t start_pts;
  double frame_rate;
  double pts_time_base;
  int frameno, frameno_gapless;
  int start_pts_set;

  bool build_only_on_bad_performance;
  bool building_cancelled;
};

static MovieProxyBuilder *index_ffmpeg_create_context(MovieReader *anim,
                                                      int tcs_in_use,
                                                      int proxy_sizes_in_use,
                                                      int quality,
                                                      bool build_only_on_bad_performance)
{
  /* Never build proxies for un-seekable single frame files. */
  if (anim->never_seek_decode_one_frame) {
    return nullptr;
  }

  MovieProxyBuilder *context = MEM_cnew<MovieProxyBuilder>("FFmpeg index builder context");
  int num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  int i, streamcount;

  context->tcs_in_use = tcs_in_use;
  context->proxy_sizes_in_use = proxy_sizes_in_use;
  context->num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  context->build_only_on_bad_performance = build_only_on_bad_performance;

  memset(context->proxy_ctx, 0, sizeof(context->proxy_ctx));
  memset(context->indexer, 0, sizeof(context->indexer));

  if (avformat_open_input(&context->iFormatCtx, anim->filepath, nullptr, nullptr) != 0) {
    MEM_freeN(context);
    return nullptr;
  }

  if (avformat_find_stream_info(context->iFormatCtx, nullptr) < 0) {
    avformat_close_input(&context->iFormatCtx);
    MEM_freeN(context);
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
    MEM_freeN(context);
    return nullptr;
  }

  context->iStream = context->iFormatCtx->streams[context->videoStream];

  context->iCodec = avcodec_find_decoder(context->iStream->codecpar->codec_id);

  if (context->iCodec == nullptr) {
    avformat_close_input(&context->iFormatCtx);
    MEM_freeN(context);
    return nullptr;
  }

  context->iCodecCtx = avcodec_alloc_context3(nullptr);
  avcodec_parameters_to_context(context->iCodecCtx, context->iStream->codecpar);
  context->iCodecCtx->workaround_bugs = FF_BUG_AUTODETECT;

  if (context->iCodec->capabilities & AV_CODEC_CAP_OTHER_THREADS) {
    context->iCodecCtx->thread_count = 0;
  }
  else {
    context->iCodecCtx->thread_count = BLI_system_thread_count();
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
    MEM_freeN(context);
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
    MEM_freeN(context);
    return nullptr; /* Nothing to transcode. */
  }

  for (i = 0; i < IMB_TC_NUM_TYPES; i++) {
    if (tcs_in_use & tc_types[i]) {
      char filepath[FILE_MAX];

      get_tc_filepath(anim, tc_types[i], filepath);

      context->indexer[i] = index_builder_create(filepath);
      if (!context->indexer[i]) {
        tcs_in_use &= ~int(tc_types[i]);
      }
    }
  }

  return context;
}

static void index_rebuild_ffmpeg_finish(MovieProxyBuilder *context, const bool stop)
{
  int i;

  const bool do_rollback = stop || context->building_cancelled;

  for (i = 0; i < IMB_TC_NUM_TYPES; i++) {
    if (context->tcs_in_use & tc_types[i]) {
      index_builder_finish(context->indexer[i], do_rollback);
    }
  }

  for (i = 0; i < context->num_proxy_sizes; i++) {
    if (context->proxy_sizes_in_use & proxy_sizes[i]) {
      free_proxy_output_ffmpeg(context->proxy_ctx[i], do_rollback);
    }
  }

  avcodec_free_context(&context->iCodecCtx);
  avformat_close_input(&context->iFormatCtx);

  MEM_freeN(context);
}

static void index_rebuild_ffmpeg_proc_decoded_frame(MovieProxyBuilder *context, AVFrame *in_frame)
{
  int i;
  uint64_t s_pts = context->seek_pos_pts;
  uint64_t s_dts = context->seek_pos_dts;
  uint64_t pts = av_get_pts_from_frame(in_frame);

  for (i = 0; i < context->num_proxy_sizes; i++) {
    add_to_proxy_output_ffmpeg(context->proxy_ctx[i], in_frame);
  }

  if (!context->start_pts_set) {
    context->start_pts = pts;
    context->start_pts_set = true;
  }

  context->frameno = floor(
      (pts - context->start_pts) * context->pts_time_base * context->frame_rate + 0.5);

  int64_t seek_pos_pts = timestamp_from_pts_or_dts(context->seek_pos_pts, context->seek_pos_dts);

  if (pts < seek_pos_pts) {
    /* Decoding starts *always* on I-Frames. In this case our position is
     * before our seek I-Frame. So we need to pick the previous available
     * I-Frame to be able to decode this one properly.
     */
    s_pts = context->last_seek_pos_pts;
    s_dts = context->last_seek_pos_dts;
  }

  for (i = 0; i < IMB_TC_NUM_TYPES; i++) {
    if (context->tcs_in_use & tc_types[i]) {
      int tc_frameno = context->frameno;

      if (tc_types[i] == IMB_TC_RECORD_RUN_NO_GAPS) {
        tc_frameno = context->frameno_gapless;
      }

      index_builder_add_entry(context->indexer[i], tc_frameno, s_pts, s_dts, pts);
    }
  }

  context->frameno_gapless++;
}

static int index_rebuild_ffmpeg(MovieProxyBuilder *context,
                                const bool *stop,
                                bool *do_update,
                                float *progress)
{
  AVFrame *in_frame = av_frame_alloc();
  AVPacket *next_packet = av_packet_alloc();
  uint64_t stream_size;

  stream_size = avio_size(context->iFormatCtx->pb);

  context->frame_rate = av_q2d(
      av_guess_frame_rate(context->iFormatCtx, context->iStream, nullptr));
  context->pts_time_base = av_q2d(context->iStream->time_base);

  while (av_read_frame(context->iFormatCtx, next_packet) >= 0) {
    float next_progress =
        float(int(floor(double(next_packet->pos) * 100 / double(stream_size) + 0.5))) / 100;

    if (*progress != next_progress) {
      *progress = next_progress;
      *do_update = true;
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
          fprintf(stderr, "Error decoding proxy frame: %s\n", error_str);
          break;
        }

        if (next_packet->flags & AV_PKT_FLAG_KEY) {
          context->last_seek_pos_pts = context->seek_pos_pts;
          context->last_seek_pos_dts = context->seek_pos_dts;

          context->seek_pos_pts = in_frame->pts;
          context->seek_pos_dts = in_frame->pkt_dts;
        }

        index_rebuild_ffmpeg_proc_decoded_frame(context, in_frame);
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
        fprintf(stderr, "Error flushing proxy frame: %s\n", error_str);
        break;
      }
      index_rebuild_ffmpeg_proc_decoded_frame(context, in_frame);
    }
  }

  av_packet_free(&next_packet);
  av_free(in_frame);

  return 1;
}

/* Get number of frames, that can be decoded in specified time period. */
static int indexer_performance_get_decode_rate(MovieProxyBuilder *context,
                                               const double time_period)
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
        fprintf(stderr, "Error decoding proxy frame: %s\n", error_str);
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
static int indexer_performance_get_max_gop_size(MovieProxyBuilder *context)
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
static bool indexer_need_to_build_proxy(MovieProxyBuilder *context)
{
  if (!context->build_only_on_bad_performance) {
    return true;
  }

  /* Make sure, that file is not cold read. */
  indexer_performance_get_decode_rate(context, 0.1);
  /* Get decode rate per 100ms. This is arbitrary, but seems to be good baseline cadence of
   * seeking. */
  const int decode_rate = indexer_performance_get_decode_rate(context, 0.1);
  const int max_gop_size = indexer_performance_get_max_gop_size(context);

  if (max_gop_size <= 10 || max_gop_size < decode_rate) {
    printf("Skipping proxy building for %s: Decoding performance is already good.\n",
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
                                           IMB_Timecode_Type tcs_in_use,
                                           int proxy_sizes_in_use,
                                           int quality,
                                           const bool overwrite,
                                           blender::Set<std::string> *processed_paths,
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
          printf("Skipping proxy: %s\n", filepath);
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
    context = index_ffmpeg_create_context(
        anim, tcs_in_use, proxy_sizes_to_build, quality, build_only_on_bad_performance);
  }
#else
  UNUSED_VARS(build_only_on_bad_performance);
#endif

  return context;

  UNUSED_VARS(tcs_in_use, proxy_sizes_in_use, quality);
}

void MOV_proxy_builder_process(MovieProxyBuilder *context,
                               /* NOLINTNEXTLINE: readability-non-const-parameter. */
                               bool *stop,
                               /* NOLINTNEXTLINE: readability-non-const-parameter. */
                               bool *do_update,
                               /* NOLINTNEXTLINE: readability-non-const-parameter. */
                               float *progress)
{
#ifdef WITH_FFMPEG
  if (context != nullptr) {
    if (indexer_need_to_build_proxy(context)) {
      index_rebuild_ffmpeg(context, stop, do_update, progress);
    }
  }
#endif
  UNUSED_VARS(context, stop, do_update, progress);
}

void MOV_proxy_builder_finish(MovieProxyBuilder *context, const bool stop)
{
#ifdef WITH_FFMPEG
  if (context != nullptr) {
    index_rebuild_ffmpeg_finish(context, stop);
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

  if (anim->record_run) {
    movie_index_free(anim->record_run);
    anim->record_run = nullptr;
  }
  if (anim->no_gaps) {
    movie_index_free(anim->no_gaps);
    anim->no_gaps = nullptr;
  }

  anim->proxies_tried = 0;
  anim->indices_tried = 0;
}

void MOV_set_custom_proxy_dir(MovieReader *anim, const char *dir)
{
  if (STREQ(anim->index_dir, dir)) {
    return;
  }
  STRNCPY(anim->index_dir, dir);

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

  /* proxies are generated in the same color space as animation itself */
  anim->proxy_anim[i] = MOV_open_file(filepath, 0, 0, anim->colorspace);

  anim->proxies_tried |= preview_size;

  return anim->proxy_anim[i];
}

const MovieIndex *movie_open_index(MovieReader *anim, IMB_Timecode_Type tc)
{
  char filepath[FILE_MAX];

  MovieIndex **index = nullptr;

  if (tc == IMB_TC_RECORD_RUN) {
    index = &anim->record_run;
  }
  else if (tc == IMB_TC_RECORD_RUN_NO_GAPS) {
    index = &anim->no_gaps;
  }

  if (anim->indices_tried & tc) {
    return nullptr;
  }
  if (index == nullptr) {
    return nullptr;
  }

  get_tc_filepath(anim, tc, filepath);

  *index = movie_index_open(filepath);

  anim->indices_tried |= tc;

  return *index;
}

int MOV_calc_frame_index_with_timecode(MovieReader *anim, IMB_Timecode_Type tc, int position)
{
  const MovieIndex *idx = movie_open_index(anim, tc);

  if (!idx) {
    return position;
  }

  return idx->get_frame_index(position);
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
