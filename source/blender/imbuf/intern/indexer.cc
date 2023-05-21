/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Peter Schlaile <peter [at] schlaile [dot] de>. */

/** \file
 * \ingroup imbuf
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#ifdef _WIN32
#  include "BLI_winstuff.h"
#endif

#include "PIL_time.h"

#include "IMB_anim.h"
#include "IMB_imbuf.h"
#include "IMB_indexer.h"
#include "imbuf.h"

#ifdef WITH_AVI
#  include "AVI_avi.h"
#endif

#ifdef WITH_FFMPEG
extern "C" {
#  include "ffmpeg_compat.h"
#  include <libavutil/imgutils.h>
}
#endif

static const char binary_header_str[] = "BlenMIdx";
static const char temp_ext[] = "_part";

static const IMB_Proxy_Size proxy_sizes[] = {
    IMB_PROXY_25, IMB_PROXY_50, IMB_PROXY_75, IMB_PROXY_100};
static const float proxy_fac[] = {0.25, 0.50, 0.75, 1.00};

#ifdef WITH_FFMPEG
static IMB_Timecode_Type tc_types[] = {
    IMB_TC_RECORD_RUN,
    IMB_TC_FREE_RUN,
    IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN,
    IMB_TC_RECORD_RUN_NO_GAPS,
};
#endif

#define INDEX_FILE_VERSION 2

/* ----------------------------------------------------------------------
 * - time code index functions
 * ---------------------------------------------------------------------- */

anim_index_builder *IMB_index_builder_create(const char *filepath)
{

  anim_index_builder *rv = MEM_cnew<anim_index_builder>("index builder");

  fprintf(stderr, "Starting work on index: %s\n", filepath);

  STRNCPY(rv->filepath, filepath);

  STRNCPY(rv->filepath_temp, filepath);
  BLI_string_join(rv->filepath_temp, sizeof(rv->filepath_temp), filepath, temp_ext);

  BLI_file_ensure_parent_dir_exists(rv->filepath_temp);

  rv->fp = BLI_fopen(rv->filepath_temp, "wb");

  if (!rv->fp) {
    fprintf(stderr,
            "Couldn't open index target: %s! "
            "Index build broken!\n",
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

void IMB_index_builder_add_entry(anim_index_builder *fp,
                                 int frameno,
                                 uint64_t seek_pos,
                                 uint64_t seek_pos_pts,
                                 uint64_t seek_pos_dts,
                                 uint64_t pts)
{
  fwrite(&frameno, sizeof(int), 1, fp->fp);
  fwrite(&seek_pos, sizeof(uint64_t), 1, fp->fp);
  fwrite(&seek_pos_pts, sizeof(uint64_t), 1, fp->fp);
  fwrite(&seek_pos_dts, sizeof(uint64_t), 1, fp->fp);
  fwrite(&pts, sizeof(uint64_t), 1, fp->fp);
}

void IMB_index_builder_proc_frame(anim_index_builder *fp,
                                  uchar *buffer,
                                  int data_size,
                                  int frameno,
                                  uint64_t seek_pos,
                                  uint64_t seek_pos_pts,
                                  uint64_t seek_pos_dts,
                                  uint64_t pts)
{
  if (fp->proc_frame) {
    anim_index_entry e;
    e.frameno = frameno;
    e.seek_pos = seek_pos;
    e.seek_pos_pts = seek_pos_pts;
    e.seek_pos_dts = seek_pos_dts;
    e.pts = pts;

    fp->proc_frame(fp, buffer, data_size, &e);
  }
  else {
    IMB_index_builder_add_entry(fp, frameno, seek_pos, seek_pos_pts, seek_pos_dts, pts);
  }
}

void IMB_index_builder_finish(anim_index_builder *fp, int rollback)
{
  if (fp->delete_priv_data) {
    fp->delete_priv_data(fp);
  }

  fclose(fp->fp);

  if (rollback) {
    unlink(fp->filepath_temp);
  }
  else {
    unlink(fp->filepath);
    BLI_rename(fp->filepath_temp, fp->filepath);
  }

  MEM_freeN(fp);
}

struct anim_index *IMB_indexer_open(const char *filepath)
{
  char header[13];
  struct anim_index *idx;
  FILE *fp = BLI_fopen(filepath, "rb");
  int i;

  if (!fp) {
    return nullptr;
  }

  if (fread(header, 12, 1, fp) != 1) {
    fprintf(stderr, "Couldn't read indexer file: %s\n", filepath);
    fclose(fp);
    return nullptr;
  }

  header[12] = 0;

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

  idx = MEM_cnew<anim_index>("anim_index");

  STRNCPY(idx->filepath, filepath);

  fseek(fp, 0, SEEK_END);

  idx->num_entries = (ftell(fp) - 12) / (sizeof(int) +      /* framepos */
                                         sizeof(uint64_t) + /* seek_pos */
                                         sizeof(uint64_t) + /* seek_pos_pts */
                                         sizeof(uint64_t) + /* seek_pos_dts */
                                         sizeof(uint64_t)   /* pts */
                                        );

  fseek(fp, 12, SEEK_SET);

  idx->entries = static_cast<anim_index_entry *>(
      MEM_callocN(sizeof(anim_index_entry) * idx->num_entries, "anim_index_entries"));

  size_t items_read = 0;
  for (i = 0; i < idx->num_entries; i++) {
    items_read += fread(&idx->entries[i].frameno, sizeof(int), 1, fp);
    items_read += fread(&idx->entries[i].seek_pos, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].seek_pos_pts, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].seek_pos_dts, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].pts, sizeof(uint64_t), 1, fp);
  }

  if (UNLIKELY(items_read != idx->num_entries * 5)) {
    fprintf(stderr, "Error: Element data size mismatch in: %s\n", filepath);
    MEM_freeN(idx->entries);
    MEM_freeN(idx);
    fclose(fp);
    return nullptr;
  }

  if ((ENDIAN_ORDER == B_ENDIAN) != (header[8] == 'V')) {
    for (i = 0; i < idx->num_entries; i++) {
      BLI_endian_switch_int32(&idx->entries[i].frameno);
      BLI_endian_switch_uint64(&idx->entries[i].seek_pos);
      BLI_endian_switch_uint64(&idx->entries[i].seek_pos_pts);
      BLI_endian_switch_uint64(&idx->entries[i].seek_pos_dts);
      BLI_endian_switch_uint64(&idx->entries[i].pts);
    }
  }

  fclose(fp);

  return idx;
}

uint64_t IMB_indexer_get_seek_pos(struct anim_index *idx, int frame_index)
{
  /* This is hard coded, because our current timecode files return non zero seek position for index
   * 0. Only when seeking to 0 it is guaranteed, that first packet will be read. */
  if (frame_index <= 0) {
    return 0;
  }
  if (frame_index >= idx->num_entries) {
    frame_index = idx->num_entries - 1;
  }
  return idx->entries[frame_index].seek_pos;
}

uint64_t IMB_indexer_get_seek_pos_pts(struct anim_index *idx, int frame_index)
{
  if (frame_index < 0) {
    frame_index = 0;
  }
  if (frame_index >= idx->num_entries) {
    frame_index = idx->num_entries - 1;
  }
  return idx->entries[frame_index].seek_pos_pts;
}

uint64_t IMB_indexer_get_seek_pos_dts(struct anim_index *idx, int frame_index)
{
  if (frame_index < 0) {
    frame_index = 0;
  }
  if (frame_index >= idx->num_entries) {
    frame_index = idx->num_entries - 1;
  }
  return idx->entries[frame_index].seek_pos_dts;
}

int IMB_indexer_get_frame_index(struct anim_index *idx, int frameno)
{
  int len = idx->num_entries;
  int half;
  int middle;
  int first = 0;

  /* Binary-search (lower bound) the right index. */

  while (len > 0) {
    half = len >> 1;
    middle = first;

    middle += half;

    if (idx->entries[middle].frameno < frameno) {
      first = middle;
      first++;
      len = len - half - 1;
    }
    else {
      len = half;
    }
  }

  if (first == idx->num_entries) {
    return idx->num_entries - 1;
  }

  return first;
}

uint64_t IMB_indexer_get_pts(struct anim_index *idx, int frame_index)
{
  if (frame_index < 0) {
    frame_index = 0;
  }
  if (frame_index >= idx->num_entries) {
    frame_index = idx->num_entries - 1;
  }
  return idx->entries[frame_index].pts;
}

int IMB_indexer_get_duration(struct anim_index *idx)
{
  if (idx->num_entries == 0) {
    return 0;
  }
  return idx->entries[idx->num_entries - 1].frameno + 1;
}

int IMB_indexer_can_scan(struct anim_index *idx, int old_frame_index, int new_frame_index)
{
  /* makes only sense, if it is the same I-Frame and we are not
   * trying to run backwards in time... */
  return (IMB_indexer_get_seek_pos(idx, old_frame_index) ==
              IMB_indexer_get_seek_pos(idx, new_frame_index) &&
          old_frame_index < new_frame_index);
}

void IMB_indexer_close(struct anim_index *idx)
{
  MEM_freeN(idx->entries);
  MEM_freeN(idx);
}

int IMB_proxy_size_to_array_index(IMB_Proxy_Size pr_size)
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

int IMB_timecode_to_array_index(IMB_Timecode_Type tc)
{
  switch (tc) {
    case IMB_TC_NONE:
      return -1;
    case IMB_TC_RECORD_RUN:
      return 0;
    case IMB_TC_FREE_RUN:
      return 1;
    case IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN:
      return 2;
    case IMB_TC_RECORD_RUN_NO_GAPS:
      return 3;
    default:
      BLI_assert_msg(0, "Unhandled timecode type enum!");
      return -1;
  }
}

/* ----------------------------------------------------------------------
 * - rebuild helper functions
 * ---------------------------------------------------------------------- */

static void get_index_dir(struct anim *anim, char *index_dir, size_t index_dir_maxncpy)
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

void IMB_anim_get_filename(struct anim *anim, char *filename, int filename_maxncpy)
{
  BLI_path_split_file_part(anim->filepath, filename, filename_maxncpy);
}

static bool get_proxy_filepath(struct anim *anim,
                               IMB_Proxy_Size preview_size,
                               char *filepath,
                               bool temp)
{
  char index_dir[FILE_MAXDIR];
  int i = IMB_proxy_size_to_array_index(preview_size);

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

static void get_tc_filename(struct anim *anim, IMB_Timecode_Type tc, char *filepath)
{
  char index_dir[FILE_MAXDIR];
  int i = IMB_timecode_to_array_index(tc);

  BLI_assert(i >= 0);

  const char *index_names[] = {
      "record_run%s%s.blen_tc",
      "free_run%s%s.blen_tc",
      "interp_free_run%s%s.blen_tc",
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
 * - common rebuilder structures
 * ---------------------------------------------------------------------- */

typedef struct IndexBuildContext {
  int anim_type;
} IndexBuildContext;

/* ----------------------------------------------------------------------
 * - ffmpeg rebuilder
 * ---------------------------------------------------------------------- */

#ifdef WITH_FFMPEG

struct proxy_output_ctx {
  AVFormatContext *of;
  AVStream *st;
  AVCodecContext *c;
  const AVCodec *codec;
  struct SwsContext *sws_ctx;
  AVFrame *frame;
  int cfra;
  IMB_Proxy_Size proxy_size;
  int orig_height;
  struct anim *anim;
};

static struct proxy_output_ctx *alloc_proxy_output_ffmpeg(
    struct anim *anim, AVStream *st, IMB_Proxy_Size proxy_size, int width, int height, int quality)
{
  proxy_output_ctx *rv = MEM_cnew<proxy_output_ctx>("alloc_proxy_output");

  char filepath[FILE_MAX];

  rv->proxy_size = proxy_size;
  rv->anim = anim;

  get_proxy_filepath(rv->anim, rv->proxy_size, filepath, true);
  if (!BLI_file_ensure_parent_dir_exists(filepath)) {
    return nullptr;
  }

  rv->of = avformat_alloc_context();
  rv->of->oformat = av_guess_format("avi", nullptr, nullptr);

  rv->of->url = av_strdup(filepath);

  fprintf(stderr, "Starting work on proxy: %s\n", rv->of->url);

  rv->st = avformat_new_stream(rv->of, nullptr);
  rv->st->id = 0;

  rv->codec = avcodec_find_encoder(AV_CODEC_ID_H264);

  rv->c = avcodec_alloc_context3(rv->codec);

  if (!rv->codec) {
    fprintf(stderr,
            "No ffmpeg encoder available? "
            "Proxy not built!\n");
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

  avcodec_parameters_from_context(rv->st->codecpar, rv->c);

  int ret = avio_open(&rv->of->pb, filepath, AVIO_FLAG_WRITE);

  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    fprintf(stderr,
            "Couldn't open IO: %s\n"
            "Proxy not built!\n",
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
            "Couldn't open codec: %s\n"
            "Proxy not built!\n",
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
    rv->frame = av_frame_alloc();

    av_image_fill_arrays(rv->frame->data,
                         rv->frame->linesize,
                         static_cast<const uint8_t *>(MEM_mallocN(
                             av_image_get_buffer_size(rv->c->pix_fmt, width, height, 1),
                             "alloc proxy output frame")),
                         rv->c->pix_fmt,
                         width,
                         height,
                         1);

    rv->frame->format = rv->c->pix_fmt;
    rv->frame->width = width;
    rv->frame->height = height;

    rv->sws_ctx = sws_getContext(st->codecpar->width,
                                 rv->orig_height,
                                 AVPixelFormat(st->codecpar->format),
                                 width,
                                 height,
                                 rv->c->pix_fmt,
                                 SWS_FAST_BILINEAR | SWS_PRINT_INFO,
                                 nullptr,
                                 nullptr,
                                 nullptr);
  }

  ret = avformat_write_header(rv->of, nullptr);
  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    fprintf(stderr,
            "Couldn't write header: %s\n"
            "Proxy not built!\n",
            error_str);

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

static void add_to_proxy_output_ffmpeg(struct proxy_output_ctx *ctx, AVFrame *frame)
{
  if (!ctx) {
    return;
  }

  if (ctx->sws_ctx && frame &&
      (frame->data[0] || frame->data[1] || frame->data[2] || frame->data[3]))
  {
    sws_scale(ctx->sws_ctx,
              (const uint8_t *const *)frame->data,
              frame->linesize,
              0,
              ctx->orig_height,
              ctx->frame->data,
              ctx->frame->linesize);
  }

  frame = ctx->sws_ctx ? (frame ? ctx->frame : 0) : frame;

  if (frame) {
    frame->pts = ctx->cfra++;
  }

  int ret = avcodec_send_frame(ctx->c, frame);
  if (ret < 0) {
    /* Can't send frame to encoder. This shouldn't happen. */
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);

    fprintf(stderr, "Can't send video frame: %s\n", error_str);
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
              "Error encoding proxy frame %d for '%s': %s\n",
              ctx->cfra - 1,
              ctx->of->url,
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
              "Error writing proxy frame %d "
              "into '%s': %s\n",
              ctx->cfra - 1,
              ctx->of->url,
              error_str);
      break;
    }
  }

  av_packet_free(&packet);
}

static void free_proxy_output_ffmpeg(struct proxy_output_ctx *ctx, int rollback)
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
    sws_freeContext(ctx->sws_ctx);

    MEM_freeN(ctx->frame->data[0]);
    av_free(ctx->frame);
  }

  get_proxy_filepath(ctx->anim, ctx->proxy_size, filepath_tmp, true);

  if (rollback) {
    unlink(filepath_tmp);
  }
  else {
    get_proxy_filepath(ctx->anim, ctx->proxy_size, filepath, false);
    unlink(filepath);
    BLI_rename(filepath_tmp, filepath);
  }

  MEM_freeN(ctx);
}

typedef struct FFmpegIndexBuilderContext {
  int anim_type;

  AVFormatContext *iFormatCtx;
  AVCodecContext *iCodecCtx;
  const AVCodec *iCodec;
  AVStream *iStream;
  int videoStream;

  int num_proxy_sizes;
  int num_indexers;

  struct proxy_output_ctx *proxy_ctx[IMB_PROXY_MAX_SLOT];
  anim_index_builder *indexer[IMB_TC_MAX_SLOT];

  int tcs_in_use;
  int proxy_sizes_in_use;

  uint64_t seek_pos;
  uint64_t seek_pos_pts;
  uint64_t seek_pos_dts;
  uint64_t last_seek_pos;
  uint64_t last_seek_pos_pts;
  uint64_t last_seek_pos_dts;
  uint64_t start_pts;
  double frame_rate;
  double pts_time_base;
  int frameno, frameno_gapless;
  int start_pts_set;

  bool build_only_on_bad_performance;
  bool building_cancelled;
} FFmpegIndexBuilderContext;

static IndexBuildContext *index_ffmpeg_create_context(struct anim *anim,
                                                      int tcs_in_use,
                                                      int proxy_sizes_in_use,
                                                      int quality,
                                                      bool build_only_on_bad_performance)
{
  FFmpegIndexBuilderContext *context = MEM_cnew<FFmpegIndexBuilderContext>(
      "FFmpeg index builder context");
  int num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  int num_indexers = IMB_TC_MAX_SLOT;
  int i, streamcount;

  context->tcs_in_use = tcs_in_use;
  context->proxy_sizes_in_use = proxy_sizes_in_use;
  context->num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  context->num_indexers = IMB_TC_MAX_SLOT;
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
      context->proxy_ctx[i] = alloc_proxy_output_ffmpeg(anim,
                                                        context->iStream,
                                                        proxy_sizes[i],
                                                        context->iCodecCtx->width * proxy_fac[i],
                                                        context->iCodecCtx->height * proxy_fac[i],
                                                        quality);
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

  for (i = 0; i < num_indexers; i++) {
    if (tcs_in_use & tc_types[i]) {
      char filepath[FILE_MAX];

      get_tc_filename(anim, tc_types[i], filepath);

      context->indexer[i] = IMB_index_builder_create(filepath);
      if (!context->indexer[i]) {
        tcs_in_use &= ~int(tc_types[i]);
      }
    }
  }

  return (IndexBuildContext *)context;
}

static void index_rebuild_ffmpeg_finish(FFmpegIndexBuilderContext *context, const bool stop)
{
  int i;

  const bool do_rollback = stop || context->building_cancelled;

  for (i = 0; i < context->num_indexers; i++) {
    if (context->tcs_in_use & tc_types[i]) {
      IMB_index_builder_finish(context->indexer[i], do_rollback);
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

static void index_rebuild_ffmpeg_proc_decoded_frame(FFmpegIndexBuilderContext *context,
                                                    AVPacket *curr_packet,
                                                    AVFrame *in_frame)
{
  int i;
  uint64_t s_pos = context->seek_pos;
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
    s_pos = context->last_seek_pos;
    s_pts = context->last_seek_pos_pts;
    s_dts = context->last_seek_pos_dts;
  }

  for (i = 0; i < context->num_indexers; i++) {
    if (context->tcs_in_use & tc_types[i]) {
      int tc_frameno = context->frameno;

      if (tc_types[i] == IMB_TC_RECORD_RUN_NO_GAPS) {
        tc_frameno = context->frameno_gapless;
      }

      IMB_index_builder_proc_frame(context->indexer[i],
                                   curr_packet->data,
                                   curr_packet->size,
                                   tc_frameno,
                                   s_pos,
                                   s_pts,
                                   s_dts,
                                   pts);
    }
  }

  context->frameno_gapless++;
}

static int index_rebuild_ffmpeg(FFmpegIndexBuilderContext *context,
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
          context->last_seek_pos = context->seek_pos;
          context->last_seek_pos_pts = context->seek_pos_pts;
          context->last_seek_pos_dts = context->seek_pos_dts;

          context->seek_pos = in_frame->pkt_pos;
          context->seek_pos_pts = in_frame->pts;
          context->seek_pos_dts = in_frame->pkt_dts;
        }

        index_rebuild_ffmpeg_proc_decoded_frame(context, next_packet, in_frame);
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
      index_rebuild_ffmpeg_proc_decoded_frame(context, next_packet, in_frame);
    }
  }

  av_packet_free(&next_packet);
  av_free(in_frame);

  return 1;
}

/* Get number of frames, that can be decoded in specified time period. */
static int indexer_performance_get_decode_rate(FFmpegIndexBuilderContext *context,
                                               const double time_period)
{
  AVFrame *in_frame = av_frame_alloc();
  AVPacket *packet = av_packet_alloc();

  const double start = PIL_check_seconds_timer();
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

    const double end = PIL_check_seconds_timer();

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
static int indexer_performance_get_max_gop_size(FFmpegIndexBuilderContext *context)
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
static bool indexer_need_to_build_proxy(FFmpegIndexBuilderContext *context)
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
 * - internal AVI (fallback) rebuilder
 * ---------------------------------------------------------------------- */

#ifdef WITH_AVI
typedef struct FallbackIndexBuilderContext {
  int anim_type;

  struct anim *anim;
  AviMovie *proxy_ctx[IMB_PROXY_MAX_SLOT];
  int proxy_sizes_in_use;
} FallbackIndexBuilderContext;

static AviMovie *alloc_proxy_output_avi(
    struct anim *anim, char *filepath, int width, int height, int quality)
{
  int x, y;
  AviFormat format;
  double framerate;
  AviMovie *avi;
  /* It doesn't really matter for proxies, but sane defaults help anyways. */
  short frs_sec = 25;
  float frs_sec_base = 1.0;

  IMB_anim_get_fps(anim, &frs_sec, &frs_sec_base, false);

  x = width;
  y = height;

  framerate = double(frs_sec) / double(frs_sec_base);

  avi = MEM_cnew<AviMovie>("avimovie");

  format = AVI_FORMAT_MJPEG;

  if (AVI_open_compress(filepath, avi, 1, format) != AVI_ERROR_NONE) {
    MEM_freeN(avi);
    return nullptr;
  }

  AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_WIDTH, &x);
  AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_HEIGHT, &y);
  AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_QUALITY, &quality);
  AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_FRAMERATE, &framerate);

  avi->interlace = 0;
  avi->odd_fields = 0;

  return avi;
}

static IndexBuildContext *index_fallback_create_context(struct anim *anim,
                                                        int /*tcs_in_use*/,
                                                        int proxy_sizes_in_use,
                                                        int quality)
{
  FallbackIndexBuilderContext *context;
  int i;

  /* since timecode indices only work with ffmpeg right now,
   * don't know a sensible fallback here...
   *
   * so no proxies...
   */
  if (proxy_sizes_in_use == IMB_PROXY_NONE) {
    return nullptr;
  }

  context = MEM_cnew<FallbackIndexBuilderContext>("fallback index builder context");

  context->anim = anim;
  context->proxy_sizes_in_use = proxy_sizes_in_use;

  memset(context->proxy_ctx, 0, sizeof(context->proxy_ctx));

  for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
    if (context->proxy_sizes_in_use & proxy_sizes[i]) {
      char filepath[FILE_MAX];

      get_proxy_filepath(anim, proxy_sizes[i], filepath, true);
      BLI_file_ensure_parent_dir_exists(filepath);

      context->proxy_ctx[i] = alloc_proxy_output_avi(
          anim, filepath, anim->x * proxy_fac[i], anim->y * proxy_fac[i], quality);
    }
  }

  return (IndexBuildContext *)context;
}

static void index_rebuild_fallback_finish(FallbackIndexBuilderContext *context, const bool stop)
{
  struct anim *anim = context->anim;
  char filepath[FILE_MAX];
  char filepath_tmp[FILE_MAX];
  int i;

  for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
    if (context->proxy_sizes_in_use & proxy_sizes[i]) {
      AVI_close_compress(context->proxy_ctx[i]);
      MEM_freeN(context->proxy_ctx[i]);

      get_proxy_filepath(anim, proxy_sizes[i], filepath_tmp, true);
      get_proxy_filepath(anim, proxy_sizes[i], filepath, false);

      if (stop) {
        unlink(filepath_tmp);
      }
      else {
        unlink(filepath);
        rename(filepath_tmp, filepath);
      }
    }
  }
}

static void index_rebuild_fallback(FallbackIndexBuilderContext *context,
                                   const bool *stop,
                                   bool *do_update,
                                   float *progress)
{
  int count = IMB_anim_get_duration(context->anim, IMB_TC_NONE);
  int i, pos;
  struct anim *anim = context->anim;

  for (pos = 0; pos < count; pos++) {
    struct ImBuf *ibuf = IMB_anim_absolute(anim, pos, IMB_TC_NONE, IMB_PROXY_NONE);
    struct ImBuf *tmp_ibuf = IMB_dupImBuf(ibuf);
    float next_progress = float(pos) / float(count);

    if (*progress != next_progress) {
      *progress = next_progress;
      *do_update = true;
    }

    if (*stop) {
      break;
    }

    IMB_flipy(tmp_ibuf);

    for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
      if (context->proxy_sizes_in_use & proxy_sizes[i]) {
        int x = anim->x * proxy_fac[i];
        int y = anim->y * proxy_fac[i];

        struct ImBuf *s_ibuf = IMB_dupImBuf(tmp_ibuf);

        IMB_scalefastImBuf(s_ibuf, x, y);

        IMB_convert_rgba_to_abgr(s_ibuf);

        /* note that libavi free's the buffer... */
        uint8_t *rect = IMB_steal_byte_buffer(s_ibuf);
        AVI_write_frame(context->proxy_ctx[i], pos, AVI_FORMAT_RGB32, rect, x * y * 4);

        IMB_freeImBuf(s_ibuf);
      }
    }

    IMB_freeImBuf(tmp_ibuf);
    IMB_freeImBuf(ibuf);
  }
}

#endif /* WITH_AVI */

/* ----------------------------------------------------------------------
 * - public API
 * ---------------------------------------------------------------------- */

IndexBuildContext *IMB_anim_index_rebuild_context(struct anim *anim,
                                                  IMB_Timecode_Type tcs_in_use,
                                                  int proxy_sizes_in_use,
                                                  int quality,
                                                  const bool overwrite,
                                                  GSet *file_list,
                                                  bool build_only_on_bad_performance)
{
  IndexBuildContext *context = nullptr;
  int proxy_sizes_to_build = proxy_sizes_in_use;
  int i;

  /* Don't generate the same file twice! */
  if (file_list) {
    for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
      IMB_Proxy_Size proxy_size = proxy_sizes[i];
      if (proxy_size & proxy_sizes_to_build) {
        char filename[FILE_MAX];
        if (get_proxy_filepath(anim, proxy_size, filename, false) == false) {
          return nullptr;
        }
        void **filename_key_p;
        if (!BLI_gset_ensure_p_ex(file_list, filename, &filename_key_p)) {
          *filename_key_p = BLI_strdup(filename);
        }
        else {
          proxy_sizes_to_build &= ~int(proxy_size);
          printf("Proxy: %s already registered for generation, skipping\n", filename);
        }
      }
    }
  }

  if (!overwrite) {
    int built_proxies = IMB_anim_proxy_get_existing(anim);
    if (built_proxies != 0) {

      for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
        IMB_Proxy_Size proxy_size = proxy_sizes[i];
        if (proxy_size & built_proxies) {
          char filename[FILE_MAX];
          if (get_proxy_filepath(anim, proxy_size, filename, false) == false) {
            return nullptr;
          }
          printf("Skipping proxy: %s\n", filename);
        }
      }
    }
    proxy_sizes_to_build &= ~built_proxies;
  }

  fflush(stdout);

  if (proxy_sizes_to_build == 0) {
    return nullptr;
  }

  switch (anim->curtype) {
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      context = index_ffmpeg_create_context(
          anim, tcs_in_use, proxy_sizes_to_build, quality, build_only_on_bad_performance);
      break;
#else
    UNUSED_VARS(build_only_on_bad_performance);
#endif

    default:
#ifdef WITH_AVI
      context = index_fallback_create_context(anim, tcs_in_use, proxy_sizes_to_build, quality);
#endif
      break;
  }

  if (context) {
    context->anim_type = anim->curtype;
  }

  return context;

  UNUSED_VARS(tcs_in_use, proxy_sizes_in_use, quality);
}

void IMB_anim_index_rebuild(struct IndexBuildContext *context,
                            /* NOLINTNEXTLINE: readability-non-const-parameter. */
                            bool *stop,
                            /* NOLINTNEXTLINE: readability-non-const-parameter. */
                            bool *do_update,
                            /* NOLINTNEXTLINE: readability-non-const-parameter. */
                            float *progress)
{
  switch (context->anim_type) {
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      if (indexer_need_to_build_proxy((FFmpegIndexBuilderContext *)context)) {
        index_rebuild_ffmpeg((FFmpegIndexBuilderContext *)context, stop, do_update, progress);
      }
      break;
#endif
    default:
#ifdef WITH_AVI
      index_rebuild_fallback((FallbackIndexBuilderContext *)context, stop, do_update, progress);
#endif
      break;
  }

  UNUSED_VARS(stop, do_update, progress);
}

void IMB_anim_index_rebuild_finish(IndexBuildContext *context, const bool stop)
{
  switch (context->anim_type) {
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      index_rebuild_ffmpeg_finish((FFmpegIndexBuilderContext *)context, stop);
      break;
#endif
    default:
#ifdef WITH_AVI
      index_rebuild_fallback_finish((FallbackIndexBuilderContext *)context, stop);
#endif
      break;
  }

  /* static defined at top of the file */
  UNUSED_VARS(stop, proxy_sizes);
}

void IMB_free_indices(struct anim *anim)
{
  int i;

  for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
    if (anim->proxy_anim[i]) {
      IMB_close_anim(anim->proxy_anim[i]);
      anim->proxy_anim[i] = nullptr;
    }
  }

  for (i = 0; i < IMB_TC_MAX_SLOT; i++) {
    if (anim->curr_idx[i]) {
      IMB_indexer_close(anim->curr_idx[i]);
      anim->curr_idx[i] = nullptr;
    }
  }

  anim->proxies_tried = 0;
  anim->indices_tried = 0;
}

void IMB_anim_set_index_dir(struct anim *anim, const char *dir)
{
  if (STREQ(anim->index_dir, dir)) {
    return;
  }
  STRNCPY(anim->index_dir, dir);

  IMB_free_indices(anim);
}

struct anim *IMB_anim_open_proxy(struct anim *anim, IMB_Proxy_Size preview_size)
{
  char filepath[FILE_MAX];
  int i = IMB_proxy_size_to_array_index(preview_size);

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
  anim->proxy_anim[i] = IMB_open_anim(filepath, 0, 0, anim->colorspace);

  anim->proxies_tried |= preview_size;

  return anim->proxy_anim[i];
}

struct anim_index *IMB_anim_open_index(struct anim *anim, IMB_Timecode_Type tc)
{
  char filepath[FILE_MAX];
  int i = IMB_timecode_to_array_index(tc);

  if (i < 0) {
    return nullptr;
  }

  if (anim->curr_idx[i]) {
    return anim->curr_idx[i];
  }

  if (anim->indices_tried & tc) {
    return nullptr;
  }

  get_tc_filename(anim, tc, filepath);

  anim->curr_idx[i] = IMB_indexer_open(filepath);

  anim->indices_tried |= tc;

  return anim->curr_idx[i];
}

int IMB_anim_index_get_frame_index(struct anim *anim, IMB_Timecode_Type tc, int position)
{
  struct anim_index *idx = IMB_anim_open_index(anim, tc);

  if (!idx) {
    return position;
  }

  return IMB_indexer_get_frame_index(idx, position);
}

int IMB_anim_proxy_get_existing(struct anim *anim)
{
  const int num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  int existing = IMB_PROXY_NONE;
  int i;
  for (i = 0; i < num_proxy_sizes; i++) {
    IMB_Proxy_Size proxy_size = proxy_sizes[i];
    char filename[FILE_MAX];
    get_proxy_filepath(anim, proxy_size, filename, false);
    if (BLI_exists(filename)) {
      existing |= int(proxy_size);
    }
  }
  return existing;
}
