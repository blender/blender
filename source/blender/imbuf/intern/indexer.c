/*
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
 * Peter Schlaile <peter [at] schlaile [dot] de> 2011
 */

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
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#ifdef _WIN32
#  include "BLI_winstuff.h"
#endif

#include "PIL_time.h"

#include "IMB_anim.h"
#include "IMB_indexer.h"
#include "imbuf.h"

#ifdef WITH_AVI
#  include "AVI_avi.h"
#endif

#ifdef WITH_FFMPEG
#  include "ffmpeg_compat.h"
#  include <libavutil/imgutils.h>
#endif

static const char binary_header_str[] = "BlenMIdx";
static const char temp_ext[] = "_part";

static const int proxy_sizes[] = {IMB_PROXY_25, IMB_PROXY_50, IMB_PROXY_75, IMB_PROXY_100};
static const float proxy_fac[] = {0.25, 0.50, 0.75, 1.00};

#ifdef WITH_FFMPEG
static int tc_types[] = {
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

anim_index_builder *IMB_index_builder_create(const char *name)
{

  anim_index_builder *rv = MEM_callocN(sizeof(struct anim_index_builder), "index builder");

  fprintf(stderr, "Starting work on index: %s\n", name);

  BLI_strncpy(rv->name, name, sizeof(rv->name));
  BLI_strncpy(rv->temp_name, name, sizeof(rv->temp_name));

  strcat(rv->temp_name, temp_ext);

  BLI_make_existing_file(rv->temp_name);

  rv->fp = BLI_fopen(rv->temp_name, "wb");

  if (!rv->fp) {
    fprintf(stderr,
            "Couldn't open index target: %s! "
            "Index build broken!\n",
            rv->temp_name);
    MEM_freeN(rv);
    return NULL;
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
    unlink(fp->temp_name);
  }
  else {
    unlink(fp->name);
    BLI_rename(fp->temp_name, fp->name);
  }

  MEM_freeN(fp);
}

struct anim_index *IMB_indexer_open(const char *name)
{
  char header[13];
  struct anim_index *idx;
  FILE *fp = BLI_fopen(name, "rb");
  int i;

  if (!fp) {
    return NULL;
  }

  if (fread(header, 12, 1, fp) != 1) {
    fprintf(stderr, "Couldn't read indexer file: %s\n", name);
    fclose(fp);
    return NULL;
  }

  header[12] = 0;

  if (memcmp(header, binary_header_str, 8) != 0) {
    fprintf(stderr, "Error reading %s: Binary file type string mismatch\n", name);
    fclose(fp);
    return NULL;
  }

  if (atoi(header + 9) != INDEX_FILE_VERSION) {
    fprintf(stderr, "Error reading %s: File version mismatch\n", name);
    fclose(fp);
    return NULL;
  }

  idx = MEM_callocN(sizeof(struct anim_index), "anim_index");

  BLI_strncpy(idx->name, name, sizeof(idx->name));

  fseek(fp, 0, SEEK_END);

  idx->num_entries = (ftell(fp) - 12) / (sizeof(int) +      /* framepos */
                                         sizeof(uint64_t) + /* seek_pos */
                                         sizeof(uint64_t) + /* seek_pos_pts */
                                         sizeof(uint64_t) + /* seek_pos_dts */
                                         sizeof(uint64_t)   /* pts */
                                        );

  fseek(fp, 12, SEEK_SET);

  idx->entries = MEM_callocN(sizeof(struct anim_index_entry) * idx->num_entries,
                             "anim_index_entries");

  size_t items_read = 0;
  for (i = 0; i < idx->num_entries; i++) {
    items_read += fread(&idx->entries[i].frameno, sizeof(int), 1, fp);
    items_read += fread(&idx->entries[i].seek_pos, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].seek_pos_pts, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].seek_pos_dts, sizeof(uint64_t), 1, fp);
    items_read += fread(&idx->entries[i].pts, sizeof(uint64_t), 1, fp);
  }

  if (UNLIKELY(items_read != idx->num_entries * 5)) {
    fprintf(stderr, "Error: Element data size mismatch in: %s\n", name);
    MEM_freeN(idx->entries);
    MEM_freeN(idx);
    fclose(fp);
    return NULL;
  }

  if (((ENDIAN_ORDER == B_ENDIAN) != (header[8] == 'V'))) {
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

  /* bsearch (lower bound) the right index */

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

static void get_index_dir(struct anim *anim, char *index_dir, size_t index_dir_len)
{
  if (!anim->index_dir[0]) {
    char fname[FILE_MAXFILE];
    BLI_split_dirfile(anim->name, index_dir, fname, index_dir_len, sizeof(fname));
    BLI_path_append(index_dir, index_dir_len, "BL_proxy");
    BLI_path_append(index_dir, index_dir_len, fname);
  }
  else {
    BLI_strncpy(index_dir, anim->index_dir, index_dir_len);
  }
}

void IMB_anim_get_fname(struct anim *anim, char *file, int size)
{
  char fname[FILE_MAXFILE];
  BLI_split_dirfile(anim->name, file, fname, size, sizeof(fname));
  BLI_strncpy(file, fname, size);
}

static bool get_proxy_filename(struct anim *anim,
                               IMB_Proxy_Size preview_size,
                               char *fname,
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
    BLI_snprintf(stream_suffix, sizeof(stream_suffix), "_st%d", anim->streamindex);
  }

  BLI_snprintf(proxy_name,
               sizeof(proxy_name),
               name,
               (int)(proxy_fac[i] * 100),
               stream_suffix,
               anim->suffix);

  get_index_dir(anim, index_dir, sizeof(index_dir));

  if (BLI_path_ncmp(anim->name, index_dir, FILE_MAXDIR) == 0) {
    return false;
  }

  BLI_join_dirfile(fname, FILE_MAXFILE + FILE_MAXDIR, index_dir, proxy_name);
  return true;
}

static void get_tc_filename(struct anim *anim, IMB_Timecode_Type tc, char *fname)
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
    BLI_snprintf(stream_suffix, 20, "_st%d", anim->streamindex);
  }

  BLI_snprintf(index_name, 256, index_names[i], stream_suffix, anim->suffix);

  get_index_dir(anim, index_dir, sizeof(index_dir));

  BLI_join_dirfile(fname, FILE_MAXFILE + FILE_MAXDIR, index_dir, index_name);
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
  AVCodec *codec;
  struct SwsContext *sws_ctx;
  AVFrame *frame;
  int cfra;
  int proxy_size;
  int orig_height;
  struct anim *anim;
};

static struct proxy_output_ctx *alloc_proxy_output_ffmpeg(
    struct anim *anim, AVStream *st, int proxy_size, int width, int height, int quality)
{
  struct proxy_output_ctx *rv = MEM_callocN(sizeof(struct proxy_output_ctx), "alloc_proxy_output");

  char fname[FILE_MAX];

  rv->proxy_size = proxy_size;
  rv->anim = anim;

  get_proxy_filename(rv->anim, rv->proxy_size, fname, true);
  BLI_make_existing_file(fname);

  rv->of = avformat_alloc_context();
  rv->of->oformat = av_guess_format("avi", NULL, NULL);

  rv->of->url = av_strdup(fname);

  fprintf(stderr, "Starting work on proxy: %s\n", rv->of->url);

  rv->st = avformat_new_stream(rv->of, NULL);
  rv->st->id = 0;

  rv->c = avcodec_alloc_context3(NULL);
  rv->c->codec_type = AVMEDIA_TYPE_VIDEO;
  rv->c->codec_id = AV_CODEC_ID_H264;

  rv->of->oformat->video_codec = rv->c->codec_id;
  rv->codec = avcodec_find_encoder(rv->c->codec_id);

  if (!rv->codec) {
    fprintf(stderr,
            "No ffmpeg encoder available? "
            "Proxy not built!\n");
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_freeN(rv);
    return NULL;
  }

  avcodec_get_context_defaults3(rv->c, rv->codec);

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

  AVDictionary *codec_opts = NULL;
  /* High quality preset value. */
  av_dict_set_int(&codec_opts, "crf", crf, 0);
  /* Prefer smaller file-size. Presets from `veryslow` to `veryfast` produce output with very
   * similar file-size, but there is big difference in performance.
   * In some cases `veryfast` preset will produce smallest file-size. */
  av_dict_set(&codec_opts, "preset", "veryfast", 0);
  av_dict_set(&codec_opts, "tune", "fastdecode", 0);

  if (rv->codec->capabilities & AV_CODEC_CAP_AUTO_THREADS) {
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

  int ret = avio_open(&rv->of->pb, fname, AVIO_FLAG_WRITE);

  if (ret < 0) {
    fprintf(stderr,
            "Couldn't open IO: %s\n"
            "Proxy not built!\n",
            av_err2str(ret));
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_freeN(rv);
    return NULL;
  }

  ret = avcodec_open2(rv->c, rv->codec, &codec_opts);
  if (ret < 0) {
    fprintf(stderr,
            "Couldn't open codec: %s\n"
            "Proxy not built!\n",
            av_err2str(ret));
    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_freeN(rv);
    return NULL;
  }

  rv->orig_height = st->codecpar->height;

  if (st->codecpar->width != width || st->codecpar->height != height ||
      st->codecpar->format != rv->c->pix_fmt) {
    rv->frame = av_frame_alloc();

    av_image_fill_arrays(rv->frame->data,
                         rv->frame->linesize,
                         MEM_mallocN(av_image_get_buffer_size(rv->c->pix_fmt, width, height, 1),
                                     "alloc proxy output frame"),
                         rv->c->pix_fmt,
                         width,
                         height,
                         1);

    rv->frame->format = rv->c->pix_fmt;
    rv->frame->width = width;
    rv->frame->height = height;

    rv->sws_ctx = sws_getContext(st->codecpar->width,
                                 rv->orig_height,
                                 st->codecpar->format,
                                 width,
                                 height,
                                 rv->c->pix_fmt,
                                 SWS_FAST_BILINEAR | SWS_PRINT_INFO,
                                 NULL,
                                 NULL,
                                 NULL);
  }

  ret = avformat_write_header(rv->of, NULL);
  if (ret < 0) {
    fprintf(stderr,
            "Couldn't write header: %s\n"
            "Proxy not built!\n",
            av_err2str(ret));

    if (rv->frame) {
      av_frame_free(&rv->frame);
    }

    avcodec_free_context(&rv->c);
    avformat_free_context(rv->of);
    MEM_freeN(rv);
    return NULL;
  }

  return rv;
}

static void add_to_proxy_output_ffmpeg(struct proxy_output_ctx *ctx, AVFrame *frame)
{
  if (!ctx) {
    return;
  }

  if (ctx->sws_ctx && frame &&
      (frame->data[0] || frame->data[1] || frame->data[2] || frame->data[3])) {
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
    fprintf(stderr, "Can't send video frame: %s\n", av_err2str(ret));
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
      fprintf(stderr,
              "Error encoding proxy frame %d for '%s': %s\n",
              ctx->cfra - 1,
              ctx->of->url,
              av_err2str(ret));
      break;
    }

    packet->stream_index = ctx->st->index;
    av_packet_rescale_ts(packet, ctx->c->time_base, ctx->st->time_base);
#  ifdef FFMPEG_USE_DURATION_WORKAROUND
    my_guess_pkt_duration(ctx->of, ctx->st, packet);
#  endif

    int write_ret = av_interleaved_write_frame(ctx->of, packet);
    if (write_ret != 0) {
      fprintf(stderr,
              "Error writing proxy frame %d "
              "into '%s': %s\n",
              ctx->cfra - 1,
              ctx->of->url,
              av_err2str(write_ret));
      break;
    }
  }

  av_packet_free(&packet);
}

static void free_proxy_output_ffmpeg(struct proxy_output_ctx *ctx, int rollback)
{
  char fname[FILE_MAX];
  char fname_tmp[FILE_MAX];

  if (!ctx) {
    return;
  }

  if (!rollback) {
    /* Flush the remaining packets. */
    add_to_proxy_output_ffmpeg(ctx, NULL);
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

  get_proxy_filename(ctx->anim, ctx->proxy_size, fname_tmp, true);

  if (rollback) {
    unlink(fname_tmp);
  }
  else {
    get_proxy_filename(ctx->anim, ctx->proxy_size, fname, false);
    unlink(fname);
    BLI_rename(fname_tmp, fname);
  }

  MEM_freeN(ctx);
}

typedef struct FFmpegIndexBuilderContext {
  int anim_type;

  AVFormatContext *iFormatCtx;
  AVCodecContext *iCodecCtx;
  AVCodec *iCodec;
  AVStream *iStream;
  int videoStream;

  int num_proxy_sizes;
  int num_indexers;

  struct proxy_output_ctx *proxy_ctx[IMB_PROXY_MAX_SLOT];
  anim_index_builder *indexer[IMB_TC_MAX_SLOT];

  IMB_Timecode_Type tcs_in_use;
  IMB_Proxy_Size proxy_sizes_in_use;

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
                                                      IMB_Timecode_Type tcs_in_use,
                                                      IMB_Proxy_Size proxy_sizes_in_use,
                                                      int quality,
                                                      bool build_only_on_bad_performance)
{
  FFmpegIndexBuilderContext *context = MEM_callocN(sizeof(FFmpegIndexBuilderContext),
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

  if (avformat_open_input(&context->iFormatCtx, anim->name, NULL, NULL) != 0) {
    MEM_freeN(context);
    return NULL;
  }

  if (avformat_find_stream_info(context->iFormatCtx, NULL) < 0) {
    avformat_close_input(&context->iFormatCtx);
    MEM_freeN(context);
    return NULL;
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
    return NULL;
  }

  context->iStream = context->iFormatCtx->streams[context->videoStream];

  context->iCodec = avcodec_find_decoder(context->iStream->codecpar->codec_id);

  if (context->iCodec == NULL) {
    avformat_close_input(&context->iFormatCtx);
    MEM_freeN(context);
    return NULL;
  }

  context->iCodecCtx = avcodec_alloc_context3(NULL);
  avcodec_parameters_to_context(context->iCodecCtx, context->iStream->codecpar);
  context->iCodecCtx->workaround_bugs = FF_BUG_AUTODETECT;

  if (context->iCodec->capabilities & AV_CODEC_CAP_AUTO_THREADS) {
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

  if (avcodec_open2(context->iCodecCtx, context->iCodec, NULL) < 0) {
    avformat_close_input(&context->iFormatCtx);
    avcodec_free_context(&context->iCodecCtx);
    MEM_freeN(context);
    return NULL;
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
        proxy_sizes_in_use &= ~proxy_sizes[i];
      }
    }
  }

  for (i = 0; i < num_indexers; i++) {
    if (tcs_in_use & tc_types[i]) {
      char fname[FILE_MAX];

      get_tc_filename(anim, tc_types[i], fname);

      context->indexer[i] = IMB_index_builder_create(fname);
      if (!context->indexer[i]) {
        tcs_in_use &= ~tc_types[i];
      }
    }
  }

  return (IndexBuildContext *)context;
}

static void index_rebuild_ffmpeg_finish(FFmpegIndexBuilderContext *context, int stop)
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
                                const short *stop,
                                short *do_update,
                                float *progress)
{
  AVFrame *in_frame = av_frame_alloc();
  AVPacket *next_packet = av_packet_alloc();
  uint64_t stream_size;

  stream_size = avio_size(context->iFormatCtx->pb);

  context->frame_rate = av_q2d(context->iStream->r_frame_rate);
  context->pts_time_base = av_q2d(context->iStream->time_base);

  while (av_read_frame(context->iFormatCtx, next_packet) >= 0) {
    float next_progress =
        (float)((int)floor(((double)next_packet->pos) * 100 / ((double)stream_size) + 0.5)) / 100;

    if (*progress != next_progress) {
      *progress = next_progress;
      *do_update = true;
    }

    if (*stop) {
      break;
    }

    if (next_packet->stream_index == context->videoStream) {
      if (next_packet->flags & AV_PKT_FLAG_KEY) {
        context->last_seek_pos = context->seek_pos;
        context->last_seek_pos_pts = context->seek_pos_pts;
        context->last_seek_pos_dts = context->seek_pos_dts;

        context->seek_pos = next_packet->pos;
        context->seek_pos_pts = next_packet->pts;
        context->seek_pos_dts = next_packet->dts;
      }

      int ret = avcodec_send_packet(context->iCodecCtx, next_packet);
      while (ret >= 0) {
        ret = avcodec_receive_frame(context->iCodecCtx, in_frame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          /* No more frames to flush. */
          break;
        }
        if (ret < 0) {
          fprintf(stderr, "Error decoding proxy frame: %s\n", av_err2str(ret));
          break;
        }
        index_rebuild_ffmpeg_proc_decoded_frame(context, next_packet, in_frame);
      }
    }
    av_packet_unref(next_packet);
  }

  /* process pictures still stuck in decoder engine after EOF
   * according to ffmpeg docs using NULL packets.
   *
   * At least, if we haven't already stopped... */

  if (!*stop) {
    int ret = avcodec_send_packet(context->iCodecCtx, NULL);

    while (ret >= 0) {
      ret = avcodec_receive_frame(context->iCodecCtx, in_frame);

      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        /* No more frames to flush. */
        break;
      }
      if (ret < 0) {
        fprintf(stderr, "Error flushing proxy frame: %s\n", av_err2str(ret));
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
      continue;
    }

    int ret = avcodec_send_packet(context->iCodecCtx, packet);
    while (ret >= 0) {
      ret = avcodec_receive_frame(context->iCodecCtx, in_frame);

      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      }

      if (ret < 0) {
        fprintf(stderr, "Error decoding proxy frame: %s\n", av_err2str(ret));
        break;
      }
      frames_decoded++;
    }

    const double end = PIL_check_seconds_timer();

    if (end > start + time_period) {
      break;
    }
  }

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
  }

  av_seek_frame(context->iFormatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
  return max_gop;
}

/* Assess scrubbing performance of provided file. This function is not meant to be very exact.
 * It compares number number of frames decoded in reasonable time with largest detected GOP size.
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
  IMB_Proxy_Size proxy_sizes_in_use;
} FallbackIndexBuilderContext;

static AviMovie *alloc_proxy_output_avi(
    struct anim *anim, char *filename, int width, int height, int quality)
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

  framerate = (double)frs_sec / (double)frs_sec_base;

  avi = MEM_mallocN(sizeof(AviMovie), "avimovie");

  format = AVI_FORMAT_MJPEG;

  if (AVI_open_compress(filename, avi, 1, format) != AVI_ERROR_NONE) {
    MEM_freeN(avi);
    return NULL;
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
                                                        IMB_Timecode_Type UNUSED(tcs_in_use),
                                                        IMB_Proxy_Size proxy_sizes_in_use,
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
    return NULL;
  }

  context = MEM_callocN(sizeof(FallbackIndexBuilderContext), "fallback index builder context");

  context->anim = anim;
  context->proxy_sizes_in_use = proxy_sizes_in_use;

  memset(context->proxy_ctx, 0, sizeof(context->proxy_ctx));

  for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
    if (context->proxy_sizes_in_use & proxy_sizes[i]) {
      char fname[FILE_MAX];

      get_proxy_filename(anim, proxy_sizes[i], fname, true);
      BLI_make_existing_file(fname);

      context->proxy_ctx[i] = alloc_proxy_output_avi(
          anim, fname, anim->x * proxy_fac[i], anim->y * proxy_fac[i], quality);
    }
  }

  return (IndexBuildContext *)context;
}

static void index_rebuild_fallback_finish(FallbackIndexBuilderContext *context, int stop)
{
  struct anim *anim = context->anim;
  char fname[FILE_MAX];
  char fname_tmp[FILE_MAX];
  int i;

  for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
    if (context->proxy_sizes_in_use & proxy_sizes[i]) {
      AVI_close_compress(context->proxy_ctx[i]);
      MEM_freeN(context->proxy_ctx[i]);

      get_proxy_filename(anim, proxy_sizes[i], fname_tmp, true);
      get_proxy_filename(anim, proxy_sizes[i], fname, false);

      if (stop) {
        unlink(fname_tmp);
      }
      else {
        unlink(fname);
        rename(fname_tmp, fname);
      }
    }
  }
}

static void index_rebuild_fallback(FallbackIndexBuilderContext *context,
                                   const short *stop,
                                   short *do_update,
                                   float *progress)
{
  int cnt = IMB_anim_get_duration(context->anim, IMB_TC_NONE);
  int i, pos;
  struct anim *anim = context->anim;

  for (pos = 0; pos < cnt; pos++) {
    struct ImBuf *ibuf = IMB_anim_absolute(anim, pos, IMB_TC_NONE, IMB_PROXY_NONE);
    struct ImBuf *tmp_ibuf = IMB_dupImBuf(ibuf);
    float next_progress = (float)pos / (float)cnt;

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

        AVI_write_frame(context->proxy_ctx[i], pos, AVI_FORMAT_RGB32, s_ibuf->rect, x * y * 4);

        /* note that libavi free's the buffer... */
        s_ibuf->rect = NULL;

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
                                                  IMB_Proxy_Size proxy_sizes_in_use,
                                                  int quality,
                                                  const bool overwrite,
                                                  GSet *file_list,
                                                  bool build_only_on_bad_performance)
{
  IndexBuildContext *context = NULL;
  IMB_Proxy_Size proxy_sizes_to_build = proxy_sizes_in_use;
  int i;

  /* Don't generate the same file twice! */
  if (file_list) {
    for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
      IMB_Proxy_Size proxy_size = proxy_sizes[i];
      if (proxy_size & proxy_sizes_to_build) {
        char filename[FILE_MAX];
        if (get_proxy_filename(anim, proxy_size, filename, false) == false) {
          return NULL;
        }
        void **filename_key_p;
        if (!BLI_gset_ensure_p_ex(file_list, filename, &filename_key_p)) {
          *filename_key_p = BLI_strdup(filename);
        }
        else {
          proxy_sizes_to_build &= ~proxy_size;
          printf("Proxy: %s already registered for generation, skipping\n", filename);
        }
      }
    }
  }

  if (!overwrite) {
    IMB_Proxy_Size built_proxies = IMB_anim_proxy_get_existing(anim);
    if (built_proxies != 0) {

      for (i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
        IMB_Proxy_Size proxy_size = proxy_sizes[i];
        if (proxy_size & built_proxies) {
          char filename[FILE_MAX];
          if (get_proxy_filename(anim, proxy_size, filename, false) == false) {
            return NULL;
          }
          printf("Skipping proxy: %s\n", filename);
        }
      }
    }
    proxy_sizes_to_build &= ~built_proxies;
  }

  fflush(stdout);

  if (proxy_sizes_to_build == 0) {
    return NULL;
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

#ifdef WITH_AVI
    default:
      context = index_fallback_create_context(anim, tcs_in_use, proxy_sizes_to_build, quality);
      break;
#endif
  }

  if (context) {
    context->anim_type = anim->curtype;
  }

  return context;

  UNUSED_VARS(tcs_in_use, proxy_sizes_in_use, quality);
}

void IMB_anim_index_rebuild(struct IndexBuildContext *context,
                            /* NOLINTNEXTLINE: readability-non-const-parameter. */
                            short *stop,
                            /* NOLINTNEXTLINE: readability-non-const-parameter. */
                            short *do_update,
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
#ifdef WITH_AVI
    default:
      index_rebuild_fallback((FallbackIndexBuilderContext *)context, stop, do_update, progress);
      break;
#endif
  }

  UNUSED_VARS(stop, do_update, progress);
}

void IMB_anim_index_rebuild_finish(IndexBuildContext *context, short stop)
{
  switch (context->anim_type) {
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      index_rebuild_ffmpeg_finish((FFmpegIndexBuilderContext *)context, stop);
      break;
#endif
#ifdef WITH_AVI
    default:
      index_rebuild_fallback_finish((FallbackIndexBuilderContext *)context, stop);
      break;
#endif
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
      anim->proxy_anim[i] = NULL;
    }
  }

  for (i = 0; i < IMB_TC_MAX_SLOT; i++) {
    if (anim->curr_idx[i]) {
      IMB_indexer_close(anim->curr_idx[i]);
      anim->curr_idx[i] = NULL;
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
  BLI_strncpy(anim->index_dir, dir, sizeof(anim->index_dir));

  IMB_free_indices(anim);
}

struct anim *IMB_anim_open_proxy(struct anim *anim, IMB_Proxy_Size preview_size)
{
  char fname[FILE_MAX];
  int i = IMB_proxy_size_to_array_index(preview_size);

  if (i < 0) {
    return NULL;
  }

  if (anim->proxy_anim[i]) {
    return anim->proxy_anim[i];
  }

  if (anim->proxies_tried & preview_size) {
    return NULL;
  }

  get_proxy_filename(anim, preview_size, fname, false);

  /* proxies are generated in the same color space as animation itself */
  anim->proxy_anim[i] = IMB_open_anim(fname, 0, 0, anim->colorspace);

  anim->proxies_tried |= preview_size;

  return anim->proxy_anim[i];
}

struct anim_index *IMB_anim_open_index(struct anim *anim, IMB_Timecode_Type tc)
{
  char fname[FILE_MAX];
  int i = IMB_timecode_to_array_index(tc);

  if (i < 0) {
    return NULL;
  }

  if (anim->curr_idx[i]) {
    return anim->curr_idx[i];
  }

  if (anim->indices_tried & tc) {
    return NULL;
  }

  get_tc_filename(anim, tc, fname);

  anim->curr_idx[i] = IMB_indexer_open(fname);

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

IMB_Proxy_Size IMB_anim_proxy_get_existing(struct anim *anim)
{
  const int num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  IMB_Proxy_Size existing = 0;
  int i;
  for (i = 0; i < num_proxy_sizes; i++) {
    IMB_Proxy_Size proxy_size = proxy_sizes[i];
    char filename[FILE_MAX];
    get_proxy_filename(anim, proxy_size, filename, false);
    if (BLI_exists(filename)) {
      existing |= proxy_size;
    }
  }
  return existing;
}
