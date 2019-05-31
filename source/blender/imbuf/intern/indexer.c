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

#include "BLI_utildefines.h"
#include "BLI_endian_switch.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_ghash.h"

#include "IMB_indexer.h"
#include "IMB_anim.h"
#include "imbuf.h"

#include "BKE_global.h"

#ifdef WITH_AVI
#  include "AVI_avi.h"
#endif

#ifdef WITH_FFMPEG
#  include "ffmpeg_compat.h"
#endif

static const char magic[] = "BlenMIdx";
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

#define INDEX_FILE_VERSION 1

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

  fprintf(rv->fp, "%s%c%.3d", magic, (ENDIAN_ORDER == B_ENDIAN) ? 'V' : 'v', INDEX_FILE_VERSION);

  return rv;
}

void IMB_index_builder_add_entry(anim_index_builder *fp,
                                 int frameno,
                                 unsigned long long seek_pos,
                                 unsigned long long seek_pos_dts,
                                 unsigned long long pts)
{
  fwrite(&frameno, sizeof(int), 1, fp->fp);
  fwrite(&seek_pos, sizeof(unsigned long long), 1, fp->fp);
  fwrite(&seek_pos_dts, sizeof(unsigned long long), 1, fp->fp);
  fwrite(&pts, sizeof(unsigned long long), 1, fp->fp);
}

void IMB_index_builder_proc_frame(anim_index_builder *fp,
                                  unsigned char *buffer,
                                  int data_size,
                                  int frameno,
                                  unsigned long long seek_pos,
                                  unsigned long long seek_pos_dts,
                                  unsigned long long pts)
{
  if (fp->proc_frame) {
    anim_index_entry e;
    e.frameno = frameno;
    e.seek_pos = seek_pos;
    e.seek_pos_dts = seek_pos_dts;
    e.pts = pts;

    fp->proc_frame(fp, buffer, data_size, &e);
  }
  else {
    IMB_index_builder_add_entry(fp, frameno, seek_pos, seek_pos_dts, pts);
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
    fclose(fp);
    return NULL;
  }

  header[12] = 0;

  if (memcmp(header, magic, 8) != 0) {
    fclose(fp);
    return NULL;
  }

  if (atoi(header + 9) != INDEX_FILE_VERSION) {
    fclose(fp);
    return NULL;
  }

  idx = MEM_callocN(sizeof(struct anim_index), "anim_index");

  BLI_strncpy(idx->name, name, sizeof(idx->name));

  fseek(fp, 0, SEEK_END);

  idx->num_entries = (ftell(fp) - 12) / (sizeof(int) +                /* framepos */
                                         sizeof(unsigned long long) + /* seek_pos */
                                         sizeof(unsigned long long) + /* seek_pos_dts */
                                         sizeof(unsigned long long)   /* pts */
                                        );

  fseek(fp, 12, SEEK_SET);

  idx->entries = MEM_callocN(sizeof(struct anim_index_entry) * idx->num_entries,
                             "anim_index_entries");

  for (i = 0; i < idx->num_entries; i++) {
    fread(&idx->entries[i].frameno, sizeof(int), 1, fp);
    fread(&idx->entries[i].seek_pos, sizeof(unsigned long long), 1, fp);
    fread(&idx->entries[i].seek_pos_dts, sizeof(unsigned long long), 1, fp);
    fread(&idx->entries[i].pts, sizeof(unsigned long long), 1, fp);
  }

  if (((ENDIAN_ORDER == B_ENDIAN) != (header[8] == 'V'))) {
    for (i = 0; i < idx->num_entries; i++) {
      BLI_endian_switch_int32(&idx->entries[i].frameno);
      BLI_endian_switch_int64((int64_t *)&idx->entries[i].seek_pos);
      BLI_endian_switch_int64((int64_t *)&idx->entries[i].seek_pos_dts);
      BLI_endian_switch_int64((int64_t *)&idx->entries[i].pts);
    }
  }

  fclose(fp);

  return idx;
}

unsigned long long IMB_indexer_get_seek_pos(struct anim_index *idx, int frame_index)
{
  if (frame_index < 0) {
    frame_index = 0;
  }
  if (frame_index >= idx->num_entries) {
    frame_index = idx->num_entries - 1;
  }
  return idx->entries[frame_index].seek_pos;
}

unsigned long long IMB_indexer_get_seek_pos_dts(struct anim_index *idx, int frame_index)
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
  else {
    return first;
  }
}

unsigned long long IMB_indexer_get_pts(struct anim_index *idx, int frame_index)
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
      /* if we got here, something is broken anyways, so sane defaults... */
      return 0;
    case IMB_PROXY_25:
      return 0;
    case IMB_PROXY_50:
      return 1;
    case IMB_PROXY_75:
      return 2;
    case IMB_PROXY_100:
      return 3;
    default:
      return 0;
  }
}

int IMB_timecode_to_array_index(IMB_Timecode_Type tc)
{
  switch (tc) {
    case IMB_TC_NONE: /* if we got here, something is broken anyways,
                       * so sane defaults... */
      return 0;
    case IMB_TC_RECORD_RUN:
      return 0;
    case IMB_TC_FREE_RUN:
      return 1;
    case IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN:
      return 2;
    case IMB_TC_RECORD_RUN_NO_GAPS:
      return 3;
    default:
      return 0;
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

static void get_proxy_filename(struct anim *anim,
                               IMB_Proxy_Size preview_size,
                               char *fname,
                               bool temp)
{
  char index_dir[FILE_MAXDIR];
  int i = IMB_proxy_size_to_array_index(preview_size);

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

  BLI_join_dirfile(fname, FILE_MAXFILE + FILE_MAXDIR, index_dir, proxy_name);
}

static void get_tc_filename(struct anim *anim, IMB_Timecode_Type tc, char *fname)
{
  char index_dir[FILE_MAXDIR];
  int i = IMB_timecode_to_array_index(tc);
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

// work around stupid swscaler 16 bytes alignment bug...

static int round_up(int x, int mod)
{
  return x + ((mod - (x % mod)) % mod);
}

static struct proxy_output_ctx *alloc_proxy_output_ffmpeg(
    struct anim *anim, AVStream *st, int proxy_size, int width, int height, int quality)
{
  struct proxy_output_ctx *rv = MEM_callocN(sizeof(struct proxy_output_ctx), "alloc_proxy_output");

  char fname[FILE_MAX];
  int ffmpeg_quality;

  /* JPEG requires this */
  width = round_up(width, 8);
  height = round_up(height, 8);

  rv->proxy_size = proxy_size;
  rv->anim = anim;

  get_proxy_filename(rv->anim, rv->proxy_size, fname, true);
  BLI_make_existing_file(fname);

  rv->of = avformat_alloc_context();
  rv->of->oformat = av_guess_format("avi", NULL, NULL);

  BLI_strncpy(rv->of->filename, fname, sizeof(rv->of->filename));

  fprintf(stderr, "Starting work on proxy: %s\n", rv->of->filename);

  rv->st = avformat_new_stream(rv->of, NULL);
  rv->st->id = 0;

  rv->c = rv->st->codec;
  rv->c->codec_type = AVMEDIA_TYPE_VIDEO;
  rv->c->codec_id = AV_CODEC_ID_MJPEG;
  rv->c->width = width;
  rv->c->height = height;

  rv->of->oformat->video_codec = rv->c->codec_id;
  rv->codec = avcodec_find_encoder(rv->c->codec_id);

  if (!rv->codec) {
    fprintf(stderr,
            "No ffmpeg MJPEG encoder available? "
            "Proxy not built!\n");
    av_free(rv->of);
    return NULL;
  }

  if (rv->codec->pix_fmts) {
    rv->c->pix_fmt = rv->codec->pix_fmts[0];
  }
  else {
    rv->c->pix_fmt = AV_PIX_FMT_YUVJ420P;
  }

  rv->c->sample_aspect_ratio = rv->st->sample_aspect_ratio = st->codec->sample_aspect_ratio;

  rv->c->time_base.den = 25;
  rv->c->time_base.num = 1;
  rv->st->time_base = rv->c->time_base;

  /* there's no  way to set JPEG quality in the same way as in AVI JPEG and image sequence,
   * but this seems to be giving expected quality result */
  ffmpeg_quality = (int)(1.0f + 30.0f * (1.0f - (float)quality / 100.0f) + 0.5f);
  av_opt_set_int(rv->c, "qmin", ffmpeg_quality, 0);
  av_opt_set_int(rv->c, "qmax", ffmpeg_quality, 0);

  if (rv->of->flags & AVFMT_GLOBALHEADER) {
    rv->c->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  if (avio_open(&rv->of->pb, fname, AVIO_FLAG_WRITE) < 0) {
    fprintf(stderr,
            "Couldn't open outputfile! "
            "Proxy not built!\n");
    av_free(rv->of);
    return 0;
  }

  avcodec_open2(rv->c, rv->codec, NULL);

  rv->orig_height = av_get_cropped_height_from_codec(st->codec);

  if (st->codec->width != width || st->codec->height != height ||
      st->codec->pix_fmt != rv->c->pix_fmt) {
    rv->frame = av_frame_alloc();
    avpicture_fill((AVPicture *)rv->frame,
                   MEM_mallocN(avpicture_get_size(rv->c->pix_fmt, round_up(width, 16), height),
                               "alloc proxy output frame"),
                   rv->c->pix_fmt,
                   round_up(width, 16),
                   height);

    rv->sws_ctx = sws_getContext(st->codec->width,
                                 rv->orig_height,
                                 st->codec->pix_fmt,
                                 width,
                                 height,
                                 rv->c->pix_fmt,
                                 SWS_FAST_BILINEAR | SWS_PRINT_INFO,
                                 NULL,
                                 NULL,
                                 NULL);
  }

  if (avformat_write_header(rv->of, NULL) < 0) {
    fprintf(stderr,
            "Couldn't set output parameters? "
            "Proxy not built!\n");
    av_free(rv->of);
    return 0;
  }

  return rv;
}

static int add_to_proxy_output_ffmpeg(struct proxy_output_ctx *ctx, AVFrame *frame)
{
  AVPacket packet = {0};
  int ret, got_output;

  av_init_packet(&packet);

  if (!ctx) {
    return 0;
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

  ret = avcodec_encode_video2(ctx->c, &packet, frame, &got_output);
  if (ret < 0) {
    fprintf(stderr, "Error encoding proxy frame %d for '%s'\n", ctx->cfra - 1, ctx->of->filename);
    return 0;
  }

  if (got_output) {
    if (packet.pts != AV_NOPTS_VALUE) {
      packet.pts = av_rescale_q(packet.pts, ctx->c->time_base, ctx->st->time_base);
    }
    if (packet.dts != AV_NOPTS_VALUE) {
      packet.dts = av_rescale_q(packet.dts, ctx->c->time_base, ctx->st->time_base);
    }

    packet.stream_index = ctx->st->index;

    if (av_interleaved_write_frame(ctx->of, &packet) != 0) {
      fprintf(stderr,
              "Error writing proxy frame %d "
              "into '%s'\n",
              ctx->cfra - 1,
              ctx->of->filename);
      return 0;
    }

    return 1;
  }
  else {
    return 0;
  }
}

static void free_proxy_output_ffmpeg(struct proxy_output_ctx *ctx, int rollback)
{
  char fname[FILE_MAX];
  char fname_tmp[FILE_MAX];

  if (!ctx) {
    return;
  }

  if (!rollback) {
    while (add_to_proxy_output_ffmpeg(ctx, NULL)) {
    }
  }

  avcodec_flush_buffers(ctx->c);

  av_write_trailer(ctx->of);

  avcodec_close(ctx->c);

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

  unsigned long long seek_pos;
  unsigned long long last_seek_pos;
  unsigned long long seek_pos_dts;
  unsigned long long seek_pos_pts;
  unsigned long long last_seek_pos_dts;
  unsigned long long start_pts;
  double frame_rate;
  double pts_time_base;
  int frameno, frameno_gapless;
  int start_pts_set;
} FFmpegIndexBuilderContext;

static IndexBuildContext *index_ffmpeg_create_context(struct anim *anim,
                                                      IMB_Timecode_Type tcs_in_use,
                                                      IMB_Proxy_Size proxy_sizes_in_use,
                                                      int quality)
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
    if (context->iFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
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
  context->iCodecCtx = context->iStream->codec;

  context->iCodec = avcodec_find_decoder(context->iCodecCtx->codec_id);

  if (context->iCodec == NULL) {
    avformat_close_input(&context->iFormatCtx);
    MEM_freeN(context);
    return NULL;
  }

  context->iCodecCtx->workaround_bugs = 1;

  if (avcodec_open2(context->iCodecCtx, context->iCodec, NULL) < 0) {
    avformat_close_input(&context->iFormatCtx);
    MEM_freeN(context);
    return NULL;
  }

  for (i = 0; i < num_proxy_sizes; i++) {
    if (proxy_sizes_in_use & proxy_sizes[i]) {
      context->proxy_ctx[i] = alloc_proxy_output_ffmpeg(
          anim,
          context->iStream,
          proxy_sizes[i],
          context->iCodecCtx->width * proxy_fac[i],
          av_get_cropped_height_from_codec(context->iCodecCtx) * proxy_fac[i],
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

  for (i = 0; i < context->num_indexers; i++) {
    if (context->tcs_in_use & tc_types[i]) {
      IMB_index_builder_finish(context->indexer[i], stop);
    }
  }

  for (i = 0; i < context->num_proxy_sizes; i++) {
    if (context->proxy_sizes_in_use & proxy_sizes[i]) {
      free_proxy_output_ffmpeg(context->proxy_ctx[i], stop);
    }
  }

  avcodec_close(context->iCodecCtx);
  avformat_close_input(&context->iFormatCtx);

  MEM_freeN(context);
}

static void index_rebuild_ffmpeg_proc_decoded_frame(FFmpegIndexBuilderContext *context,
                                                    AVPacket *curr_packet,
                                                    AVFrame *in_frame)
{
  int i;
  unsigned long long s_pos = context->seek_pos;
  unsigned long long s_dts = context->seek_pos_dts;
  unsigned long long pts = av_get_pts_from_frame(context->iFormatCtx, in_frame);

  for (i = 0; i < context->num_proxy_sizes; i++) {
    add_to_proxy_output_ffmpeg(context->proxy_ctx[i], in_frame);
  }

  if (!context->start_pts_set) {
    context->start_pts = pts;
    context->start_pts_set = true;
  }

  context->frameno = floor(
      (pts - context->start_pts) * context->pts_time_base * context->frame_rate + 0.5);

  /* decoding starts *always* on I-Frames,
   * so: P-Frames won't work, even if all the
   * information is in place, when we seek
   * to the I-Frame presented *after* the P-Frame,
   * but located before the P-Frame within
   * the stream */

  if (pts < context->seek_pos_pts) {
    s_pos = context->last_seek_pos;
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
                                   s_dts,
                                   pts);
    }
  }

  context->frameno_gapless++;
}

static int index_rebuild_ffmpeg(FFmpegIndexBuilderContext *context,
                                short *stop,
                                short *do_update,
                                float *progress)
{
  AVFrame *in_frame = 0;
  AVPacket next_packet;
  uint64_t stream_size;

  memset(&next_packet, 0, sizeof(AVPacket));

  in_frame = av_frame_alloc();

  stream_size = avio_size(context->iFormatCtx->pb);

  context->frame_rate = av_q2d(av_get_r_frame_rate_compat(context->iFormatCtx, context->iStream));
  context->pts_time_base = av_q2d(context->iStream->time_base);

  while (av_read_frame(context->iFormatCtx, &next_packet) >= 0) {
    int frame_finished = 0;
    float next_progress =
        (float)((int)floor(((double)next_packet.pos) * 100 / ((double)stream_size) + 0.5)) / 100;

    if (*progress != next_progress) {
      *progress = next_progress;
      *do_update = true;
    }

    if (*stop) {
      av_free_packet(&next_packet);
      break;
    }

    if (next_packet.stream_index == context->videoStream) {
      if (next_packet.flags & AV_PKT_FLAG_KEY) {
        context->last_seek_pos = context->seek_pos;
        context->last_seek_pos_dts = context->seek_pos_dts;
        context->seek_pos = next_packet.pos;
        context->seek_pos_dts = next_packet.dts;
        context->seek_pos_pts = next_packet.pts;
      }

      avcodec_decode_video2(context->iCodecCtx, in_frame, &frame_finished, &next_packet);
    }

    if (frame_finished) {
      index_rebuild_ffmpeg_proc_decoded_frame(context, &next_packet, in_frame);
    }
    av_free_packet(&next_packet);
  }

  /* process pictures still stuck in decoder engine after EOF
   * according to ffmpeg docs using 0-size packets.
   *
   * At least, if we haven't already stopped... */

  /* this creates the 0-size packet and prevents a memory leak. */
  av_free_packet(&next_packet);

  if (!*stop) {
    int frame_finished;

    do {
      frame_finished = 0;

      avcodec_decode_video2(context->iCodecCtx, in_frame, &frame_finished, &next_packet);

      if (frame_finished) {
        index_rebuild_ffmpeg_proc_decoded_frame(context, &next_packet, in_frame);
      }
    } while (frame_finished);
  }

  av_free(in_frame);

  return 1;
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
  /* it doesn't really matter for proxies, but sane defaults help anyways...*/
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
                                   short *stop,
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
                                                  GSet *file_list)
{
  IndexBuildContext *context = NULL;
  IMB_Proxy_Size proxy_sizes_to_build = proxy_sizes_in_use;
  int i;

  /* Don't generate the same file twice! */
  if (file_list) {
    for (i = 0; i < IMB_PROXY_MAX_SLOT; ++i) {
      IMB_Proxy_Size proxy_size = proxy_sizes[i];
      if (proxy_size & proxy_sizes_to_build) {
        char filename[FILE_MAX];
        get_proxy_filename(anim, proxy_size, filename, false);

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

      for (i = 0; i < IMB_PROXY_MAX_SLOT; ++i) {
        IMB_Proxy_Size proxy_size = proxy_sizes[i];
        if (proxy_size & built_proxies) {
          char filename[FILE_MAX];
          get_proxy_filename(anim, proxy_size, filename, false);
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
      context = index_ffmpeg_create_context(anim, tcs_in_use, proxy_sizes_to_build, quality);
      break;
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
                            short *stop,
                            short *do_update,
                            float *progress)
{
  switch (context->anim_type) {
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      index_rebuild_ffmpeg((FFmpegIndexBuilderContext *)context, stop, do_update, progress);
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
  for (i = 0; i < num_proxy_sizes; ++i) {
    IMB_Proxy_Size proxy_size = proxy_sizes[i];
    char filename[FILE_MAX];
    get_proxy_filename(anim, proxy_size, filename, false);
    if (BLI_exists(filename)) {
      existing |= proxy_size;
    }
  }
  return existing;
}
