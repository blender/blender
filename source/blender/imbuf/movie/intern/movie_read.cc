/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>

#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "CLG_log.h"

#include "MOV_read.hh"

#include "IMB_metadata.hh"
#include "movie_proxy_indexer.hh"
#include "movie_read.hh"

#ifdef WITH_FFMPEG
#  include "ffmpeg_swscale.hh"
#  include "movie_util.hh"

extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libavutil/imgutils.h>
#  include <libavutil/rational.h>
#  include <libswscale/swscale.h>

#  include "ffmpeg_compat.h"
}

#endif /* WITH_FFMPEG */

#ifdef WITH_FFMPEG
static CLG_LogRef LOG = {"video.read"};
#endif

#ifdef WITH_FFMPEG
static void free_anim_ffmpeg(MovieReader *anim);
#endif

static bool anim_getnew(MovieReader *anim);

void MOV_close(MovieReader *anim)
{
  if (anim == nullptr) {
    return;
  }

#ifdef WITH_FFMPEG
  free_anim_ffmpeg(anim);
#endif
  MOV_close_proxies(anim);
  IMB_metadata_free(anim->metadata);

  MEM_delete(anim);
}

void MOV_get_filename(const MovieReader *anim, char *filename, int filename_maxncpy)
{
  BLI_path_split_file_part(anim->filepath, filename, filename_maxncpy);
}

IDProperty *MOV_load_metadata(MovieReader *anim)
{
  if (anim->state == MovieReader::State::Valid) {
#ifdef WITH_FFMPEG
    BLI_assert(anim->pFormatCtx != nullptr);
    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "METADATA FETCH\n");

    AVDictionaryEntry *entry = nullptr;
    while (true) {
      entry = av_dict_get(anim->pFormatCtx->metadata, "", entry, AV_DICT_IGNORE_SUFFIX);
      if (entry == nullptr) {
        break;
      }

      /* Delay creation of the property group until there is actual metadata to put in there. */
      IMB_metadata_ensure(&anim->metadata);
      IMB_metadata_set_field(anim->metadata, entry->key, entry->value);
    }
#endif
  }
  return anim->metadata;
}

static void probe_video_colorspace(MovieReader *anim, char r_colorspace_name[IM_MAX_SPACE])
{
  /* Use default role as fallback (i.e. it is an unknown combination of colorspace and primaries)
   */
  BLI_strncpy_utf8(r_colorspace_name,
                   IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE),
                   IM_MAX_SPACE);

  if (anim->state == MovieReader::State::Uninitialized) {
    if (!anim_getnew(anim)) {
      return;
    }
  }

#ifdef WITH_FFMPEG
  /* Note that the ffmpeg enums are documented to match CICP codes. */
  const int cicp[4] = {anim->pCodecCtx->color_primaries,
                       anim->pCodecCtx->color_trc,
                       anim->pCodecCtx->colorspace,
                       anim->pCodecCtx->color_range};
  const ColorSpace *colorspace = IMB_colormanagement_space_from_cicp(
      cicp, ColorManagedFileOutput::Video);

  if (colorspace == nullptr) {
    return;
  }

  BLI_strncpy_utf8(
      r_colorspace_name, IMB_colormanagement_colorspace_get_name(colorspace), IM_MAX_SPACE);
#endif /* WITH_FFMPEG */
}

MovieReader *MOV_open_file(const char *filepath,
                           const int ib_flags,
                           const int streamindex,
                           const bool keep_original_colorspace,
                           char colorspace[IM_MAX_SPACE])
{
  MovieReader *anim;

  BLI_assert(!BLI_path_is_rel(filepath));

  anim = MEM_new<MovieReader>("anim struct");
  if (anim != nullptr) {

    STRNCPY(anim->filepath, filepath);
    anim->ib_flags = ib_flags;
    anim->streamindex = streamindex;
    anim->keep_original_colorspace = keep_original_colorspace;

    if (colorspace && colorspace[0] != '\0') {
      /* Use colorspace from argument, if provided. */
      STRNCPY_UTF8(anim->colorspace, colorspace);
    }
    else {
      /* Try to initialize colorspace from the FFmpeg stream by interpreting color information from
       * it. */
      char file_colorspace[IM_MAX_SPACE];
      probe_video_colorspace(anim, file_colorspace);
      STRNCPY_UTF8(anim->colorspace, file_colorspace);
      if (colorspace) {
        /* Copy the used colorspace into output argument. */
        BLI_strncpy_utf8(colorspace, file_colorspace, IM_MAX_SPACE);
      }
    }
  }
  return anim;
}

bool MOV_is_initialized_and_valid(const MovieReader *anim)
{
#if !defined(WITH_FFMPEG)
  UNUSED_VARS(anim);
#endif

#ifdef WITH_FFMPEG
  if (anim->pCodecCtx != nullptr) {
    return true;
  }
#endif
  return false;
}

void MOV_set_multiview_suffix(MovieReader *anim, const char *suffix)
{
  STRNCPY(anim->suffix, suffix);
}

#ifdef WITH_FFMPEG

static double ffmpeg_stream_start_time_get(const AVStream *stream)
{
  if (stream->start_time == AV_NOPTS_VALUE) {
    return 0.0;
  }

  return stream->start_time * av_q2d(stream->time_base);
}

static int ffmpeg_container_frame_count_get(const AVFormatContext *pFormatCtx,
                                            const AVStream *video_stream,
                                            const double frame_rate)
{
  /* Find audio stream to guess the duration of the video.
   * Sometimes the audio AND the video stream have a start offset.
   * The difference between these is the offset we want to use to
   * calculate the video duration.
   */
  const double video_start = ffmpeg_stream_start_time_get(video_stream);
  double audio_start = 0;

  for (int i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      AVStream *audio_stream = pFormatCtx->streams[i];
      audio_start = ffmpeg_stream_start_time_get(audio_stream);
      break;
    }
  }

  double stream_dur;

  if (video_start > audio_start) {
    stream_dur = double(pFormatCtx->duration) / AV_TIME_BASE - (video_start - audio_start);
  }
  else {
    /* The video stream starts before or at the same time as the audio stream!
     * We have to assume that the video stream is as long as the full pFormatCtx->duration.
     */
    stream_dur = double(pFormatCtx->duration) / AV_TIME_BASE;
  }

  return lround(stream_dur * frame_rate);
}

static int ffmpeg_frame_count_get(const AVFormatContext *pFormatCtx,
                                  const AVStream *video_stream,
                                  const double frame_rate)
{
  /* Use stream duration to determine frame count. */
  if (video_stream->duration != AV_NOPTS_VALUE) {
    const double stream_dur = video_stream->duration * av_q2d(video_stream->time_base);
    return lround(stream_dur * frame_rate);
  }

  /* Fall back to manually estimating the video stream duration.
   * This is because the video stream duration can be shorter than the `pFormatCtx->duration`.
   */
  if (pFormatCtx->duration != AV_NOPTS_VALUE) {
    return ffmpeg_container_frame_count_get(pFormatCtx, video_stream, frame_rate);
  }

  /* Read frame count from the stream if we can. Note, that this value can not be trusted. */
  if (video_stream->nb_frames != 0) {
    return video_stream->nb_frames;
  }

  /* The duration has not been set, happens for single JPEG2000 images.
   * NOTE: Leave the duration zeroed, although it could set to 1 so the file is recognized
   * as a movie with 1 frame, leave as-is since image loading code-paths are preferred
   * in this case. The following assertion should be valid in this case. */
  BLI_assert(pFormatCtx->duration == AV_NOPTS_VALUE);
  return 0;
}

static int calc_pix_fmt_max_component_bits(AVPixelFormat fmt)
{
  const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(fmt);
  if (desc == nullptr) {
    return 0;
  }
  int bits = 0;
  for (int i = 0; i < desc->nb_components; i++) {
    bits = max_ii(bits, desc->comp[i].depth);
  }
  return bits;
}

static AVFormatContext *init_format_context(const char *filepath,
                                            int video_stream_index,
                                            int &r_stream_index,
                                            const AVCodec *forced_video_decoder)
{
  AVFormatContext *format_ctx = nullptr;
  if (forced_video_decoder != nullptr) {
    format_ctx = avformat_alloc_context();
    format_ctx->video_codec_id = forced_video_decoder->id;
    format_ctx->video_codec = forced_video_decoder;
  }

  if (avformat_open_input(&format_ctx, filepath, nullptr, nullptr) != 0) {
    return nullptr;
  }

  if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
    avformat_close_input(&format_ctx);
    return nullptr;
  }

  av_dump_format(format_ctx, 0, filepath, 0);

  /* Find the video stream */
  r_stream_index = -1;
  for (int i = 0; i < format_ctx->nb_streams; i++) {
    if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_stream_index > 0) {
        video_stream_index--;
        continue;
      }
      r_stream_index = i;
      break;
    }
  }

  if (r_stream_index == -1) {
    avformat_close_input(&format_ctx);
    return nullptr;
  }

  return format_ctx;
}

static AVFormatContext *init_format_context_vpx_workarounds(const char *filepath,
                                                            int video_stream_index,
                                                            int &r_stream_index,
                                                            const AVCodec *&r_codec)
{
  AVFormatContext *format_ctx = init_format_context(
      filepath, video_stream_index, r_stream_index, nullptr);
  if (format_ctx == nullptr) {
    return nullptr;
  }

  /* By default FFMPEG uses built-in VP8/VP9 decoders, however those do not detect
   * alpha channel (see FFMPEG issue #8344 https://trac.ffmpeg.org/ticket/8344).
   * The trick for VP8/VP9 is to explicitly force use of LIBVPX decoder.
   * Only do this where alpha_mode=1 metadata is set. Note that in order to work,
   * the previously initialized format context must be closed and a fresh one
   * with explicitly requested codec must be created. */
  r_codec = nullptr;
  const AVStream *video_stream = format_ctx->streams[r_stream_index];
  if (ELEM(video_stream->codecpar->codec_id, AV_CODEC_ID_VP8, AV_CODEC_ID_VP9)) {
    AVDictionaryEntry *tag = nullptr;
    tag = av_dict_get(video_stream->metadata, "alpha_mode", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag && STREQ(tag->value, "1")) {
      r_codec = avcodec_find_decoder_by_name(
          video_stream->codecpar->codec_id == AV_CODEC_ID_VP8 ? "libvpx" : "libvpx-vp9");
      if (r_codec != nullptr) {
        avformat_close_input(&format_ctx);
        format_ctx = avformat_alloc_context();
        format_ctx = init_format_context(filepath, video_stream_index, r_stream_index, r_codec);
        if (format_ctx == nullptr) {
          return nullptr;
        }
      }
    }
  }

  if (r_codec == nullptr) {
    /* Use default decoder. */
    r_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
  }

  return format_ctx;
}

static int startffmpeg(MovieReader *anim)
{
  if (anim == nullptr) {
    return -1;
  }

  int video_stream_index;
  const AVCodec *pCodec = nullptr;
  AVFormatContext *pFormatCtx = init_format_context_vpx_workarounds(
      anim->filepath, anim->streamindex, video_stream_index, pCodec);
  if (pFormatCtx == nullptr || pCodec == nullptr) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  AVCodecContext *pCodecCtx = avcodec_alloc_context3(nullptr);
  AVStream *video_stream = pFormatCtx->streams[video_stream_index];
  avcodec_parameters_to_context(pCodecCtx, video_stream->codecpar);
  pCodecCtx->workaround_bugs = FF_BUG_AUTODETECT;

  if (pCodec->capabilities & AV_CODEC_CAP_OTHER_THREADS) {
    pCodecCtx->thread_count = 0;
  }
  else {
    pCodecCtx->thread_count = MOV_thread_count();
  }

  if (pCodec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
    pCodecCtx->thread_type = FF_THREAD_FRAME;
  }
  else if (pCodec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
    pCodecCtx->thread_type = FF_THREAD_SLICE;
  }

  if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }
  if (pCodecCtx->pix_fmt == AV_PIX_FMT_NONE) {
    avcodec_free_context(&anim->pCodecCtx);
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  /* Check if we need the "never seek, only decode one frame" ffmpeg bug workaround. */
  const bool is_ogg_container = STREQ(pFormatCtx->iformat->name, "ogg");
  const bool is_non_ogg_video = video_stream->codecpar->codec_id != AV_CODEC_ID_THEORA;
  const bool is_video_thumbnail = (video_stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0;
  anim->never_seek_decode_one_frame = is_ogg_container && is_non_ogg_video && is_video_thumbnail;

  anim->frame_rate = av_guess_frame_rate(pFormatCtx, video_stream, nullptr);
  if (anim->never_seek_decode_one_frame) {
    /* Files that need this workaround have nonsensical frame rates too, resulting
     * in "millions of frames" if done through regular math. Treat frame-rate as 24/1 instead. */
    anim->frame_rate = {24, 1};
  }
  int frs_num = anim->frame_rate.num;
  double frs_den = anim->frame_rate.den;

  frs_den *= AV_TIME_BASE;

  while (frs_num % 10 == 0 && frs_den >= 2.0 && frs_num > 10) {
    frs_num /= 10;
    frs_den /= 10;
  }

  anim->frs_sec = frs_num;
  anim->frs_sec_base = frs_den / AV_TIME_BASE;
  /* Save the relative start time for the video. IE the start time in relation to where playback
   * starts. */
  anim->start_offset = ffmpeg_stream_start_time_get(video_stream);
  anim->duration_in_frames = ffmpeg_frame_count_get(
      pFormatCtx, video_stream, av_q2d(anim->frame_rate));

  anim->x = pCodecCtx->width;
  anim->y = pCodecCtx->height;
  anim->video_rotation = ffmpeg_get_video_rotation(video_stream);

  /* Decode >8bit videos into floating point image. */
  anim->is_float = calc_pix_fmt_max_component_bits(pCodecCtx->pix_fmt) > 8;

  anim->pFormatCtx = pFormatCtx;
  anim->pCodecCtx = pCodecCtx;
  anim->pCodec = pCodec;
  anim->videoStream = video_stream_index;

  anim->cur_position = 0;
  anim->cur_pts = -1;
  anim->cur_key_frame_pts = -1;
  anim->cur_packet = av_packet_alloc();
  anim->cur_packet->stream_index = -1;

  anim->pFrame = av_frame_alloc();
  anim->pFrame_backup = av_frame_alloc();
  anim->pFrame_backup_complete = false;
  anim->pFrame_complete = false;
  anim->pFrameDeinterlaced = av_frame_alloc();
  anim->pFrameRGB = av_frame_alloc();
  /* Ideally we'd use AV_PIX_FMT_RGBAF32LE for floats, but currently (ffmpeg 6.1)
   * swscale does not support that as destination. So using AV_PIX_FMT_GBRAPF32LE
   * with manual interleaving to RGBA floats. */
  anim->pFrameRGB->format = anim->is_float ? AV_PIX_FMT_GBRAPF32LE : AV_PIX_FMT_RGBA;
  anim->pFrameRGB->width = anim->x;
  anim->pFrameRGB->height = anim->y;

  const size_t align = ffmpeg_get_buffer_alignment();
  if (av_frame_get_buffer(anim->pFrameRGB, align) < 0) {
    CLOG_ERROR(&LOG, "Could not allocate frame data.");
    avcodec_free_context(&anim->pCodecCtx);
    avformat_close_input(&anim->pFormatCtx);
    av_packet_free(&anim->cur_packet);
    av_frame_free(&anim->pFrameRGB);
    av_frame_free(&anim->pFrameDeinterlaced);
    av_frame_free(&anim->pFrame);
    av_frame_free(&anim->pFrame_backup);
    anim->pCodecCtx = nullptr;
    return -1;
  }

  if (anim->ib_flags & IB_animdeinterlace) {
    anim->pFrameDeinterlaced->format = anim->pCodecCtx->pix_fmt;
    anim->pFrameDeinterlaced->width = anim->pCodecCtx->width;
    anim->pFrameDeinterlaced->height = anim->pCodecCtx->height;
    av_image_fill_arrays(
        anim->pFrameDeinterlaced->data,
        anim->pFrameDeinterlaced->linesize,
        MEM_calloc_arrayN<uint8_t>(
            av_image_get_buffer_size(
                anim->pCodecCtx->pix_fmt, anim->pCodecCtx->width, anim->pCodecCtx->height, 1),
            "ffmpeg deinterlace"),
        anim->pCodecCtx->pix_fmt,
        anim->pCodecCtx->width,
        anim->pCodecCtx->height,
        1);
  }

  /* Use full_chroma_int + accurate_rnd YUV->RGB conversion flags. Otherwise
   * the conversion is not fully accurate and introduces some banding and color
   * shifts, particularly in dark regions. See issue #111703 or upstream
   * ffmpeg ticket https://trac.ffmpeg.org/ticket/1582 */
  anim->img_convert_ctx = ffmpeg_sws_get_context(anim->x,
                                                 anim->y,
                                                 anim->pCodecCtx->pix_fmt,
                                                 anim->pCodecCtx->color_range == AVCOL_RANGE_JPEG,
                                                 anim->pCodecCtx->colorspace,
                                                 anim->x,
                                                 anim->y,
                                                 anim->pFrameRGB->format,
                                                 false,
                                                 -1,
                                                 SWS_POINT | SWS_FULL_CHR_H_INT |
                                                     SWS_ACCURATE_RND);

  if (!anim->img_convert_ctx) {
    CLOG_ERROR(&LOG,
               "ffmpeg: swscale can't transform from pixel format %s to %s (%s)",
               av_get_pix_fmt_name(anim->pCodecCtx->pix_fmt),
               av_get_pix_fmt_name((AVPixelFormat)anim->pFrameRGB->format),
               anim->filepath);
    avcodec_free_context(&anim->pCodecCtx);
    avformat_close_input(&anim->pFormatCtx);
    av_packet_free(&anim->cur_packet);
    av_frame_free(&anim->pFrameRGB);
    av_frame_free(&anim->pFrameDeinterlaced);
    av_frame_free(&anim->pFrame);
    av_frame_free(&anim->pFrame_backup);
    anim->pCodecCtx = nullptr;
    return -1;
  }

  return 0;
}

static double ffmpeg_steps_per_frame_get(const MovieReader *anim)
{
  const AVStream *v_st = anim->pFormatCtx->streams[anim->videoStream];
  const AVRational time_base = v_st->time_base;
  return av_q2d(av_inv_q(av_mul_q(anim->frame_rate, time_base)));
}

/* Store backup frame.
 * With VFR movies, if PTS is not matched perfectly, scanning continues to look for next PTS.
 * It is likely to overshoot and scanning stops. Having previous frame backed up, it is possible
 * to use it when overshoot happens.
 */
static void ffmpeg_double_buffer_backup_frame_store(MovieReader *anim, int64_t pts_to_search)
{
  /* `anim->pFrame` is beyond `pts_to_search`. Don't store it. */
  if (anim->pFrame_backup_complete && anim->cur_pts >= pts_to_search) {
    return;
  }
  if (!anim->pFrame_complete) {
    return;
  }

  if (anim->pFrame_backup_complete) {
    av_frame_unref(anim->pFrame_backup);
  }

  av_frame_move_ref(anim->pFrame_backup, anim->pFrame);
  anim->pFrame_backup_complete = true;
}

/* Free stored backup frame. */
static void ffmpeg_double_buffer_backup_frame_clear(MovieReader *anim)
{
  if (anim->pFrame_backup_complete) {
    av_frame_unref(anim->pFrame_backup);
  }
  anim->pFrame_backup_complete = false;
}

/* Return recently decoded frame. If it does not exist, return frame from backup buffer. */
static AVFrame *ffmpeg_double_buffer_frame_fallback_get(MovieReader *anim)
{
  av_log(anim->pFormatCtx, AV_LOG_ERROR, "DECODE UNHAPPY: PTS not matched!\n");

  if (anim->pFrame_complete) {
    return anim->pFrame;
  }
  if (anim->pFrame_backup_complete) {
    return anim->pFrame_backup;
  }
  return nullptr;
}

/* Convert from ffmpeg planar GBRA layout to ImBuf interleaved RGBA, applying
 * video rotation in the same go if needed. */
static void float_planar_to_interleaved(const AVFrame *frame, const int rotation, ImBuf *ibuf)
{
  using namespace blender;
  const size_t src_linesize = frame->linesize[0];
  BLI_assert_msg(frame->linesize[1] == src_linesize && frame->linesize[2] == src_linesize &&
                     frame->linesize[3] == src_linesize,
                 "ffmpeg frame should be 4 same size planes for a floating point image case");
  threading::parallel_for(IndexRange(ibuf->y), 256, [&](const IndexRange y_range) {
    const int size_x = ibuf->x;
    const int size_y = ibuf->y;
    if (rotation == 90) {
      /* 90 degree rotation. */
      for (const int64_t y : y_range) {
        int64_t src_offset = src_linesize * (size_y - y - 1);
        const float *src_g = reinterpret_cast<const float *>(frame->data[0] + src_offset);
        const float *src_b = reinterpret_cast<const float *>(frame->data[1] + src_offset);
        const float *src_r = reinterpret_cast<const float *>(frame->data[2] + src_offset);
        const float *src_a = reinterpret_cast<const float *>(frame->data[3] + src_offset);
        float *dst = ibuf->float_buffer.data + (y + (size_x - 1) * size_y) * 4;
        for (int x = 0; x < size_x; x++) {
          dst[0] = *src_r++;
          dst[1] = *src_g++;
          dst[2] = *src_b++;
          dst[3] = *src_a++;
          dst -= size_y * 4;
        }
      }
    }
    else if (rotation == 180) {
      /* 180 degree rotation. */
      for (const int64_t y : y_range) {
        int64_t src_offset = src_linesize * (size_y - y - 1);
        const float *src_g = reinterpret_cast<const float *>(frame->data[0] + src_offset);
        const float *src_b = reinterpret_cast<const float *>(frame->data[1] + src_offset);
        const float *src_r = reinterpret_cast<const float *>(frame->data[2] + src_offset);
        const float *src_a = reinterpret_cast<const float *>(frame->data[3] + src_offset);
        float *dst = ibuf->float_buffer.data + ((size_y - y - 1) * size_x + size_x - 1) * 4;
        for (int x = 0; x < size_x; x++) {
          dst[0] = *src_r++;
          dst[1] = *src_g++;
          dst[2] = *src_b++;
          dst[3] = *src_a++;
          dst -= 4;
        }
      }
    }
    else if (rotation == 270) {
      /* 270 degree rotation. */
      for (const int64_t y : y_range) {
        int64_t src_offset = src_linesize * (size_y - y - 1);
        const float *src_g = reinterpret_cast<const float *>(frame->data[0] + src_offset);
        const float *src_b = reinterpret_cast<const float *>(frame->data[1] + src_offset);
        const float *src_r = reinterpret_cast<const float *>(frame->data[2] + src_offset);
        const float *src_a = reinterpret_cast<const float *>(frame->data[3] + src_offset);
        float *dst = ibuf->float_buffer.data + (size_y - y - 1) * 4;
        for (int x = 0; x < size_x; x++) {
          dst[0] = *src_r++;
          dst[1] = *src_g++;
          dst[2] = *src_b++;
          dst[3] = *src_a++;
          dst += size_y * 4;
        }
      }
    }
    else if (rotation == 0) {
      /* No rotation. */
      for (const int64_t y : y_range) {
        int64_t src_offset = src_linesize * (size_y - y - 1);
        const float *src_g = reinterpret_cast<const float *>(frame->data[0] + src_offset);
        const float *src_b = reinterpret_cast<const float *>(frame->data[1] + src_offset);
        const float *src_r = reinterpret_cast<const float *>(frame->data[2] + src_offset);
        const float *src_a = reinterpret_cast<const float *>(frame->data[3] + src_offset);
        float *dst = ibuf->float_buffer.data + size_x * y * 4;
        for (int x = 0; x < size_x; x++) {
          *dst++ = *src_r++;
          *dst++ = *src_g++;
          *dst++ = *src_b++;
          *dst++ = *src_a++;
        }
      }
    }
  });

  if (ELEM(rotation, 90, 270)) {
    std::swap(ibuf->x, ibuf->y);
  }
}

/**
 * Postprocess the image in anim->pFrame and do color conversion and de-interlacing stuff.
 *
 * \param ibuf: The frame just read by `ffmpeg_fetchibuf`, processed in-place.
 */
static void ffmpeg_postprocess(MovieReader *anim, AVFrame *input, ImBuf *ibuf)
{
  int filter_y = 0;

  /* This means the data wasn't read properly,
   * this check stops crashing */
  if (input->data[0] == nullptr && input->data[1] == nullptr && input->data[2] == nullptr &&
      input->data[3] == nullptr)
  {
    CLOG_ERROR(&LOG,
               "ffmpeg_fetchibuf: "
               "data not read properly...");
    return;
  }

  av_log(anim->pFormatCtx,
         AV_LOG_DEBUG,
         "  POSTPROC: AVFrame planes: %p %p %p %p\n",
         input->data[0],
         input->data[1],
         input->data[2],
         input->data[3]);

  if (anim->ib_flags & IB_animdeinterlace) {
    if (ffmpeg_deinterlace(anim->pFrameDeinterlaced,
                           anim->pFrame,
                           anim->pCodecCtx->pix_fmt,
                           anim->pCodecCtx->width,
                           anim->pCodecCtx->height) < 0)
    {
      filter_y = true;
    }
    else {
      input = anim->pFrameDeinterlaced;
    }
  }

  bool already_rotated = false;
  if (anim->is_float) {
    /* Float images are converted into planar GBRA layout by swscale (since
     * it does not support direct YUV->RGBA float interleaved conversion).
     * Do vertical flip and interleave into RGBA manually. */
    /* Decode, then do vertical flip into destination. */
    ffmpeg_sws_scale_frame(anim->img_convert_ctx, anim->pFrameRGB, input);

    float_planar_to_interleaved(anim->pFrameRGB, anim->video_rotation, ibuf);
    already_rotated = true;
  }
  else {
    /* If final destination image layout matches that of decoded RGB frame (including
     * any line padding done by ffmpeg for SIMD alignment), we can directly
     * decode into that, doing the vertical flip in the same step. Otherwise have
     * to do a separate flip. */
    const int ibuf_linesize = ibuf->x * 4;
    const int rgb_linesize = anim->pFrameRGB->linesize[0];
    bool scale_to_ibuf = (rgb_linesize == ibuf_linesize);
    /* swscale on arm64 before ffmpeg 6.0 (libswscale major version 7)
     * could not handle negative line sizes. That has been fixed in all major
     * ffmpeg releases in early 2023, but easier to just check for "below 7". */
#  if (defined(__aarch64__) || defined(_M_ARM64)) && (LIBSWSCALE_VERSION_MAJOR < 7)
    scale_to_ibuf = false;
#  endif
    uint8_t *rgb_data = anim->pFrameRGB->data[0];

    if (scale_to_ibuf) {
      /* Decode RGB and do vertical flip directly into destination image, by using negative
       * line size. */
      anim->pFrameRGB->linesize[0] = -ibuf_linesize;
      anim->pFrameRGB->data[0] = ibuf->byte_buffer.data + (ibuf->y - 1) * ibuf_linesize;

      ffmpeg_sws_scale_frame(anim->img_convert_ctx, anim->pFrameRGB, input);

      anim->pFrameRGB->linesize[0] = rgb_linesize;
      anim->pFrameRGB->data[0] = rgb_data;
    }
    else {
      /* Decode, then do vertical flip into destination. */
      ffmpeg_sws_scale_frame(anim->img_convert_ctx, anim->pFrameRGB, input);

      /* Use negative line size to do vertical image flip. */
      const int src_linesize[4] = {-rgb_linesize, 0, 0, 0};
      const uint8_t *const src[4] = {
          rgb_data + (anim->y - 1) * rgb_linesize, nullptr, nullptr, nullptr};
      int dst_size = av_image_get_buffer_size(AVPixelFormat(anim->pFrameRGB->format),
                                              anim->pFrameRGB->width,
                                              anim->pFrameRGB->height,
                                              1);
      av_image_copy_to_buffer(ibuf->byte_buffer.data,
                              dst_size,
                              src,
                              src_linesize,
                              AVPixelFormat(anim->pFrameRGB->format),
                              anim->x,
                              anim->y,
                              1);
    }
  }

  if (filter_y) {
    IMB_filtery(ibuf);
  }

  /* Rotate video if display matrix is multiple of 90 degrees. */
  if (!already_rotated && ELEM(anim->video_rotation, 90, 180, 270)) {
    IMB_rotate_orthogonal(ibuf, anim->video_rotation);
  }
}

static void final_frame_log(MovieReader *anim,
                            int64_t frame_pts_start,
                            int64_t frame_pts_end,
                            const char *str)
{
  av_log(anim->pFormatCtx,
         AV_LOG_INFO,
         "DECODE HAPPY: %s frame PTS range %" PRId64 " - %" PRId64 ".\n",
         str,
         frame_pts_start,
         frame_pts_end);
}

static bool ffmpeg_pts_isect(int64_t pts_start, int64_t pts_end, int64_t pts_to_search)
{
  return pts_start <= pts_to_search && pts_to_search < pts_end;
}

/* Return frame that matches `pts_to_search`, nullptr if matching frame does not exist. */
static AVFrame *ffmpeg_frame_by_pts_get(MovieReader *anim, int64_t pts_to_search)
{
  /* NOTE: `frame->pts + frame->pkt_duration` does not always match pts of next frame.
   * See footage from #86361. Here it is OK to use, because PTS must match current or backup frame.
   * If there is no current frame, return nullptr.
   */
  if (!anim->pFrame_complete) {
    return nullptr;
  }

  if (anim->never_seek_decode_one_frame) {
    /* If we only decode one frame, return it. */
    return anim->pFrame;
  }

  const bool backup_frame_ready = anim->pFrame_backup_complete;
  const int64_t recent_start = av_get_pts_from_frame(anim->pFrame);
  const int64_t recent_end = recent_start + av_get_frame_duration_in_pts_units(anim->pFrame);
  const int64_t backup_start = backup_frame_ready ? av_get_pts_from_frame(anim->pFrame_backup) : 0;

  AVFrame *best_frame = nullptr;
  if (ffmpeg_pts_isect(recent_start, recent_end, pts_to_search)) {
    final_frame_log(anim, recent_start, recent_end, "Recent");
    best_frame = anim->pFrame;
  }
  else if (backup_frame_ready && ffmpeg_pts_isect(backup_start, recent_start, pts_to_search)) {
    final_frame_log(anim, backup_start, recent_start, "Backup");
    best_frame = anim->pFrame_backup;
  }
  return best_frame;
}

static void ffmpeg_decode_store_frame_pts(MovieReader *anim)
{
  anim->cur_pts = av_get_pts_from_frame(anim->pFrame);

#  ifdef FFMPEG_OLD_KEY_FRAME_QUERY_METHOD
  if (anim->pFrame->key_frame)
#  else
  if (anim->pFrame->flags & AV_FRAME_FLAG_KEY)
#  endif
  {
    anim->cur_key_frame_pts = anim->cur_pts;
  }

  av_log(anim->pFormatCtx,
         AV_LOG_DEBUG,
         "  FRAME DONE: cur_pts=%" PRId64 ", guessed_pts=%" PRId64 "\n",
         av_get_pts_from_frame(anim->pFrame),
         int64_t(anim->cur_pts));
}

static int ffmpeg_read_video_frame(MovieReader *anim, AVPacket *packet)
{
  int ret = 0;
  while ((ret = av_read_frame(anim->pFormatCtx, packet)) >= 0) {
    if (packet->stream_index == anim->videoStream) {
      break;
    }
    av_packet_unref(packet);
    packet->stream_index = -1;
  }

  return ret;
}

/* decode one video frame also considering the packet read into cur_packet */
static int ffmpeg_decode_video_frame(MovieReader *anim)
{
  av_log(anim->pFormatCtx, AV_LOG_DEBUG, "  DECODE VIDEO FRAME\n");

  /* Sometimes, decoder returns more than one frame per sent packet. Check if frames are available.
   * This frames must be read, otherwise decoding will fail. See #91405. */
  anim->pFrame_complete = avcodec_receive_frame(anim->pCodecCtx, anim->pFrame) == 0;
  if (anim->pFrame_complete) {
    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "  DECODE FROM CODEC BUFFER\n");
    ffmpeg_decode_store_frame_pts(anim);
    return 1;
  }

  int rval = 0;
  if (anim->cur_packet->stream_index == anim->videoStream) {
    av_packet_unref(anim->cur_packet);
    anim->cur_packet->stream_index = -1;
  }

  while ((rval = ffmpeg_read_video_frame(anim, anim->cur_packet)) >= 0) {
    if (anim->cur_packet->stream_index != anim->videoStream) {
      continue;
    }

    av_log(anim->pFormatCtx,
           AV_LOG_DEBUG,
           "READ: strID=%d dts=%" PRId64 " pts=%" PRId64 " %s\n",
           anim->cur_packet->stream_index,
           (anim->cur_packet->dts == AV_NOPTS_VALUE) ? -1 : int64_t(anim->cur_packet->dts),
           (anim->cur_packet->pts == AV_NOPTS_VALUE) ? -1 : int64_t(anim->cur_packet->pts),
           (anim->cur_packet->flags & AV_PKT_FLAG_KEY) ? " KEY" : "");

    avcodec_send_packet(anim->pCodecCtx, anim->cur_packet);
    anim->pFrame_complete = avcodec_receive_frame(anim->pCodecCtx, anim->pFrame) == 0;

    if (anim->pFrame_complete) {
      ffmpeg_decode_store_frame_pts(anim);
      break;
    }
    av_packet_unref(anim->cur_packet);
    anim->cur_packet->stream_index = -1;
  }

  if (rval == AVERROR_EOF) {
    /* Flush any remaining frames out of the decoder. */
    avcodec_send_packet(anim->pCodecCtx, nullptr);
    anim->pFrame_complete = avcodec_receive_frame(anim->pCodecCtx, anim->pFrame) == 0;

    if (anim->pFrame_complete) {
      ffmpeg_decode_store_frame_pts(anim);
      rval = 0;
    }
  }

  if (rval < 0) {
    av_packet_unref(anim->cur_packet);
    anim->cur_packet->stream_index = -1;

    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, rval);

    av_log(anim->pFormatCtx,
           AV_LOG_ERROR,
           "  DECODE READ FAILED: av_read_frame() "
           "returned error: %s\n",
           error_str);
  }

  return (rval >= 0);
}

static int64_t ffmpeg_get_seek_pts(MovieReader *anim, int64_t pts_to_search)
{
  /* FFMPEG seeks internally using DTS values instead of PTS. In some files DTS and PTS values are
   * offset and sometimes FFMPEG fails to take this into account when seeking.
   * Therefore we need to seek backwards a certain offset to make sure the frame we want is in
   * front of us. It is not possible to determine the exact needed offset,
   * this value is determined experimentally.
   * NOTE: Too big offset can impact performance. Current 3 frame offset has no measurable impact.
   */
  int64_t seek_pts = pts_to_search - (ffmpeg_steps_per_frame_get(anim) * 3);

  seek_pts = std::max<int64_t>(seek_pts, 0);
  return seek_pts;
}

/* This gives us an estimate of which pts our requested frame will have.
 * Note that this might be off a bit in certain video files, but it should still be close enough.
 */
static int64_t ffmpeg_get_pts_to_search(MovieReader *anim,
                                        const MovieIndex *tc_index,
                                        int position)
{
  int64_t pts_to_search;

  if (tc_index) {
    int new_frame_index = tc_index->get_frame_index(position);
    pts_to_search = tc_index->get_pts(new_frame_index);
  }
  else {
    AVStream *v_st = anim->pFormatCtx->streams[anim->videoStream];
    int64_t start_pts = v_st->start_time;

    pts_to_search = round(position * ffmpeg_steps_per_frame_get(anim));

    if (start_pts != AV_NOPTS_VALUE) {
      pts_to_search += start_pts;
    }
  }
  return pts_to_search;
}

static bool ffmpeg_is_first_frame_decode(MovieReader *anim)
{
  return anim->pFrame_complete == false;
}

static void ffmpeg_scan_log(MovieReader *anim, int64_t pts_to_search)
{
  int64_t frame_pts_start = av_get_pts_from_frame(anim->pFrame);
  int64_t frame_pts_end = frame_pts_start + av_get_frame_duration_in_pts_units(anim->pFrame);
  av_log(anim->pFormatCtx,
         AV_LOG_DEBUG,
         "  SCAN WHILE: PTS range %" PRId64 " - %" PRId64 " in search of %" PRId64 "\n",
         frame_pts_start,
         frame_pts_end,
         pts_to_search);
}

/* Decode frames one by one until its PTS matches pts_to_search. */
static void ffmpeg_decode_video_frame_scan(MovieReader *anim, int64_t pts_to_search)
{
  const int64_t start_gop_frame = anim->cur_key_frame_pts;
  bool decode_error = false;

  while (!decode_error && anim->cur_pts < pts_to_search) {
    ffmpeg_scan_log(anim, pts_to_search);
    ffmpeg_double_buffer_backup_frame_store(anim, pts_to_search);
    decode_error = ffmpeg_decode_video_frame(anim) < 1;

    /* We should not get a new GOP keyframe while scanning if seeking is working as intended.
     * If this condition triggers, there may be and error in our seeking code.
     * NOTE: This seems to happen if DTS value is used for seeking in ffmpeg internally. There
     * seems to be no good way to handle such case. */
    if (anim->seek_before_decode && start_gop_frame != anim->cur_key_frame_pts) {
      av_log(anim->pFormatCtx, AV_LOG_ERROR, "SCAN: Frame belongs to an unexpected GOP!\n");
    }
  }
}

/* Wrapper over av_seek_frame(), for formats that doesn't have its own read_seek() or
 * read_seek2() functions defined. When seeking in these formats, rule to seek to last
 * necessary I-frame is not honored. It is not even guaranteed that I-frame, that must be
 * decoded will be read. See https://trac.ffmpeg.org/ticket/1607 & #86944. */
static int ffmpeg_generic_seek_workaround(MovieReader *anim,
                                          int64_t *requested_pts,
                                          int64_t pts_to_search)
{
  AVStream *v_st = anim->pFormatCtx->streams[anim->videoStream];
  int64_t start_pts = v_st->start_time;
  int64_t current_pts = *requested_pts;
  int64_t offset = 0;

  /* Step backward frame by frame until we find the key frame we are looking for. */
  while (current_pts != 0) {
    current_pts = *requested_pts - int64_t(round(offset * ffmpeg_steps_per_frame_get(anim)));
    current_pts = std::max(current_pts, int64_t(0));

    /* Seek to timestamp. */
    if (av_seek_frame(anim->pFormatCtx, anim->videoStream, current_pts, AVSEEK_FLAG_BACKWARD) < 0)
    {
      break;
    }

    /* Read first video stream packet. */
    AVPacket *read_packet = av_packet_alloc();
    while (av_read_frame(anim->pFormatCtx, read_packet) >= 0) {
      if (read_packet->stream_index == anim->videoStream) {
        break;
      }
      av_packet_unref(read_packet);
    }

    /* If this packet contains an I-frame, this could be the frame that we need. */
    const bool is_key_frame = read_packet->flags & AV_PKT_FLAG_KEY;
    /* We need to check the packet timestamp as the key frame could be for a GOP forward in the
     * video stream. So if it has a larger timestamp than the frame we want, ignore it.
     */
    const int64_t cur_pts = timestamp_from_pts_or_dts(read_packet->pts, read_packet->dts);
    av_packet_free(&read_packet);

    if (is_key_frame) {
      if (cur_pts <= pts_to_search) {
        /* We found the I-frame we were looking for! */
        break;
      }
    }

    /* We have hit the beginning of the stream. */
    if (cur_pts <= start_pts) {
      break;
    }

    offset++;
  }

  *requested_pts = current_pts;

  /* Re-seek to timestamp that gave I-frame, so it can be read by decode function. */
  return av_seek_frame(anim->pFormatCtx, anim->videoStream, current_pts, AVSEEK_FLAG_BACKWARD);
}

/* Read packet until timestamp matches `anim->cur_packet`, thus recovering internal `anim` stream
 * position state. */
static void ffmpeg_seek_recover_stream_position(MovieReader *anim)
{
  AVPacket *temp_packet = av_packet_alloc();
  while (ffmpeg_read_video_frame(anim, temp_packet) >= 0) {
    int64_t current_pts = timestamp_from_pts_or_dts(anim->cur_packet->pts, anim->cur_packet->dts);
    int64_t temp_pts = timestamp_from_pts_or_dts(temp_packet->pts, temp_packet->dts);
    av_packet_unref(temp_packet);

    if (current_pts == temp_pts) {
      break;
    }
  }
  av_packet_free(&temp_packet);
}

/* Check if seeking and mainly flushing codec buffers is needed. */
static bool ffmpeg_seek_buffers_need_flushing(MovieReader *anim, int position, int64_t seek_pos)
{
  /* Get timestamp of packet read after seeking. */
  AVPacket *temp_packet = av_packet_alloc();
  ffmpeg_read_video_frame(anim, temp_packet);
  int64_t gop_pts = timestamp_from_pts_or_dts(temp_packet->pts, temp_packet->dts);
  av_packet_unref(temp_packet);
  av_packet_free(&temp_packet);

  /* Seeking gives packet, that is currently read. No seeking was necessary, so buffers don't have
   * to be flushed. */
  if (gop_pts == timestamp_from_pts_or_dts(anim->cur_packet->pts, anim->cur_packet->dts)) {
    return false;
  }

  /* Packet after seeking is same key frame as current, and further in time. No seeking was
   * necessary, so buffers don't have to be flushed. But stream position has to be recovered. */
  if (gop_pts == anim->cur_key_frame_pts && position > anim->cur_position) {
    ffmpeg_seek_recover_stream_position(anim);
    return false;
  }

  /* Seeking was necessary, but we have read packets. Therefore we must seek again. */
  av_seek_frame(anim->pFormatCtx, anim->videoStream, seek_pos, AVSEEK_FLAG_BACKWARD);
  anim->cur_key_frame_pts = gop_pts;
  return true;
}

/* Seek to last necessary key frame. */
static int ffmpeg_seek_to_key_frame(MovieReader *anim,
                                    int position,
                                    const MovieIndex *tc_index,
                                    int64_t pts_to_search)
{
  int64_t seek_pos;
  int ret;

  if (tc_index) {
    /* We can use timestamps generated from our indexer to seek. */
    int new_frame_index = tc_index->get_frame_index(position);

    uint64_t pts = tc_index->get_seek_pos_pts(new_frame_index);
    uint64_t dts = tc_index->get_seek_pos_dts(new_frame_index);

    anim->cur_key_frame_pts = timestamp_from_pts_or_dts(pts, dts);

    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "TC INDEX seek pts = %" PRIu64 "\n", pts);
    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "TC INDEX seek dts = %" PRIu64 "\n", dts);

    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "Using PTS from timecode as seek_pos\n");
    ret = av_seek_frame(anim->pFormatCtx, anim->videoStream, pts, AVSEEK_FLAG_BACKWARD);
  }
  else {
    /* We have to manually seek with ffmpeg to get to the key frame we want to start decoding from.
     */
    seek_pos = ffmpeg_get_seek_pts(anim, pts_to_search);
    av_log(
        anim->pFormatCtx, AV_LOG_DEBUG, "NO INDEX final seek seek_pos = %" PRId64 "\n", seek_pos);

    AVFormatContext *format_ctx = anim->pFormatCtx;

    /* This used to check if the codec implemented "read_seek" or "read_seek2". However this is
     * now hidden from us in FFMPEG 7.0. While not as accurate, usually the AVFMT_TS_DISCONT is
     * set for formats where we need to apply the seek workaround to (like in MPEGTS). */
    if (!(format_ctx->iformat->flags & AVFMT_TS_DISCONT)) {
      ret = av_seek_frame(anim->pFormatCtx, anim->videoStream, seek_pos, AVSEEK_FLAG_BACKWARD);
    }
    else {
      ret = ffmpeg_generic_seek_workaround(anim, &seek_pos, pts_to_search);
      av_log(anim->pFormatCtx,
             AV_LOG_DEBUG,
             "Adjusted final seek seek_pos = %" PRId64 "\n",
             seek_pos);
    }

    if (ret <= 0 && !ffmpeg_seek_buffers_need_flushing(anim, position, seek_pos)) {
      return 0;
    }
  }

  if (ret < 0) {
    av_log(anim->pFormatCtx,
           AV_LOG_ERROR,
           "FETCH: "
           "error while seeking to DTS = %" PRId64 " (frameno = %d, PTS = %" PRId64
           "): errcode = %d\n",
           seek_pos,
           position,
           pts_to_search,
           ret);
  }
  /* Flush the internal buffers of ffmpeg. This needs to be done after seeking to avoid decoding
   * errors. */
  avcodec_flush_buffers(anim->pCodecCtx);
  ffmpeg_double_buffer_backup_frame_clear(anim);

  anim->cur_pts = -1;

  if (anim->cur_packet->stream_index == anim->videoStream) {
    av_packet_unref(anim->cur_packet);
    anim->cur_packet->stream_index = -1;
  }

  return ret;
}

static bool ffmpeg_must_decode(MovieReader *anim, int position)
{
  return !anim->pFrame_complete || anim->cur_position != position;
}

static bool ffmpeg_must_seek(MovieReader *anim, int position)
{
  bool must_seek = position != anim->cur_position + 1 || ffmpeg_is_first_frame_decode(anim);
  anim->seek_before_decode = must_seek;
  return must_seek;
}

static ImBuf *ffmpeg_fetchibuf(MovieReader *anim, int position, IMB_Timecode_Type tc)
{
  if (anim == nullptr) {
    return nullptr;
  }

  av_log(anim->pFormatCtx, AV_LOG_DEBUG, "FETCH: seek_pos=%d\n", position);

  const MovieIndex *tc_index = movie_open_index(anim, tc);
  int64_t pts_to_search = ffmpeg_get_pts_to_search(anim, tc_index, position);
  AVStream *v_st = anim->pFormatCtx->streams[anim->videoStream];
  double frame_rate = av_q2d(v_st->r_frame_rate);
  double pts_time_base = av_q2d(v_st->time_base);
  int64_t start_pts = v_st->start_time;

  if (anim->never_seek_decode_one_frame) {
    /* If we must only ever decode one frame, and never seek, do so here. */
    if (!anim->pFrame_complete) {
      ffmpeg_decode_video_frame(anim);
    }
  }
  else {
    /* For all regular video files, do the seek/decode as needed. */
    av_log(anim->pFormatCtx,
           AV_LOG_DEBUG,
           "FETCH: looking for PTS=%" PRId64 " (pts_timebase=%g, frame_rate=%g, start_pts=%" PRId64
           ")\n",
           int64_t(pts_to_search),
           pts_time_base,
           frame_rate,
           start_pts);

    if (ffmpeg_must_decode(anim, position)) {
      if (ffmpeg_must_seek(anim, position)) {
        ffmpeg_seek_to_key_frame(anim, position, tc_index, pts_to_search);
      }

      ffmpeg_decode_video_frame_scan(anim, pts_to_search);
    }
  }

  /* Update resolution as it can change per-frame with WebM. See #100741 & #100081. */
  anim->x = anim->pCodecCtx->width;
  anim->y = anim->pCodecCtx->height;

  const AVPixFmtDescriptor *pix_fmt_descriptor = av_pix_fmt_desc_get(anim->pCodecCtx->pix_fmt);

  int planes = R_IMF_PLANES_RGBA;
  if ((pix_fmt_descriptor->flags & AV_PIX_FMT_FLAG_ALPHA) == 0) {
    planes = R_IMF_PLANES_RGB;
  }

  ImBuf *cur_frame_final = IMB_allocImBuf(anim->x, anim->y, planes, 0);

  /* Allocate the storage explicitly to ensure the memory is aligned. */
  const size_t align = ffmpeg_get_buffer_alignment();
  const size_t pixel_size = anim->is_float ? 16 : 4;
  uint8_t *buffer_data = static_cast<uint8_t *>(
      MEM_mallocN_aligned(pixel_size * anim->x * anim->y, align, "ffmpeg ibuf"));
  if (anim->is_float) {
    IMB_assign_float_buffer(cur_frame_final, (float *)buffer_data, IB_TAKE_OWNERSHIP);
  }
  else {
    IMB_assign_byte_buffer(cur_frame_final, buffer_data, IB_TAKE_OWNERSHIP);
  }

  AVFrame *final_frame = ffmpeg_frame_by_pts_get(anim, pts_to_search);
  if (final_frame == nullptr) {
    /* No valid frame was decoded for requested PTS, fall back on most recent decoded frame, even
     * if it is incorrect. */
    final_frame = ffmpeg_double_buffer_frame_fallback_get(anim);
  }

  /* Even with the fallback from above it is possible that the current decode frame is nullptr. In
   * this case skip post-processing and return current image buffer. */
  if (final_frame != nullptr) {
    ffmpeg_postprocess(anim, final_frame, cur_frame_final);
  }

  if (anim->is_float) {
    if (anim->keep_original_colorspace) {
      /* Movie has been explicitly requested to keep original colorspace, regardless of the nature
       * of the buffer. */
      cur_frame_final->float_buffer.colorspace = colormanage_colorspace_get_named(
          anim->colorspace);
    }
    else {
      /* Float buffers are expected to be in the scene linear color space.
       * Linearize the buffer if it is in a different space.
       *
       * It might not be the most optimal thing to do from the playback performance in the
       * sequencer perspective, but it ensures that other areas in Blender do not run into obscure
       * color space mismatches. */
      colormanage_imbuf_make_linear(
          cur_frame_final, anim->colorspace, ColorManagedFileOutput::Video);
    }
  }
  else {
    /* Colorspace conversion is lossy for byte buffers, so only assign the colorspace.
     * It is up to artists to ensure operations on byte buffers do not involve mixing different
     * colorspaces. */
    cur_frame_final->byte_buffer.colorspace = colormanage_colorspace_get_named(anim->colorspace);
  }

  anim->cur_position = position;

  return cur_frame_final;
}

static void free_anim_ffmpeg(MovieReader *anim)
{
  if (anim == nullptr) {
    return;
  }

  if (anim->pCodecCtx) {
    avcodec_free_context(&anim->pCodecCtx);
    avformat_close_input(&anim->pFormatCtx);
    av_packet_free(&anim->cur_packet);

    av_frame_free(&anim->pFrame);
    av_frame_free(&anim->pFrame_backup);
    av_frame_free(&anim->pFrameRGB);
    if (anim->pFrameDeinterlaced->data[0] != nullptr) {
      MEM_freeN(anim->pFrameDeinterlaced->data[0]);
    }
    av_frame_free(&anim->pFrameDeinterlaced);
    ffmpeg_sws_release_context(anim->img_convert_ctx);
  }
  anim->duration_in_frames = 0;
}

#endif

/**
 * Try to initialize the #anim struct.
 * Returns true on success.
 */
static bool anim_getnew(MovieReader *anim)
{
  if (anim == nullptr) {
    /* Nothing to initialize. */
    return false;
  }

  BLI_assert(anim->state == MovieReader::State::Uninitialized);

#ifdef WITH_FFMPEG
  free_anim_ffmpeg(anim);
  if (startffmpeg(anim)) {
    anim->state = MovieReader::State::Failed;
    return false;
  }
#endif
  anim->state = MovieReader::State::Valid;
  return true;
}

ImBuf *MOV_decode_preview_frame(MovieReader *anim)
{
  ImBuf *ibuf = nullptr;
  int position = 0;

  ibuf = MOV_decode_frame(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
  if (ibuf) {
    IMB_freeImBuf(ibuf);
    position = anim->duration_in_frames / 2;
    ibuf = MOV_decode_frame(anim, position, IMB_TC_NONE, IMB_PROXY_NONE);

    char value[128];
    IMB_metadata_ensure(&ibuf->metadata);
    SNPRINTF_UTF8(value, "%i", anim->x);
    IMB_metadata_set_field(ibuf->metadata, "Thumb::Video::Width", value);
    SNPRINTF_UTF8(value, "%i", anim->y);
    IMB_metadata_set_field(ibuf->metadata, "Thumb::Video::Height", value);
    SNPRINTF_UTF8(value, "%i", anim->duration_in_frames);
    IMB_metadata_set_field(ibuf->metadata, "Thumb::Video::Frames", value);

#ifdef WITH_FFMPEG
    if (anim->pFormatCtx) {
      AVStream *v_st = anim->pFormatCtx->streams[anim->videoStream];
      AVRational frame_rate = av_guess_frame_rate(anim->pFormatCtx, v_st, nullptr);
      if (frame_rate.num != 0) {
        double duration = anim->duration_in_frames / av_q2d(frame_rate);
        SNPRINTF_UTF8(value, "%g", av_q2d(frame_rate));
        IMB_metadata_set_field(ibuf->metadata, "Thumb::Video::FPS", value);
        SNPRINTF_UTF8(value, "%g", duration);
        IMB_metadata_set_field(ibuf->metadata, "Thumb::Video::Duration", value);
        IMB_metadata_set_field(ibuf->metadata, "Thumb::Video::Codec", anim->pCodec->long_name);
      }
    }
#endif
  }
  return ibuf;
}

ImBuf *MOV_decode_frame(MovieReader *anim,
                        int position,
                        IMB_Timecode_Type tc,
                        IMB_Proxy_Size preview_size)
{
  ImBuf *ibuf = nullptr;
  if (anim == nullptr) {
    return nullptr;
  }

  if (preview_size == IMB_PROXY_NONE) {
    if (anim->state == MovieReader::State::Uninitialized) {
      if (!anim_getnew(anim)) {
        return nullptr;
      }
    }

    if (position < 0) {
      return nullptr;
    }
    if (position >= anim->duration_in_frames) {
      return nullptr;
    }
  }
  else {
    MovieReader *proxy = movie_open_proxy(anim, preview_size);

    if (proxy) {
      position = MOV_calc_frame_index_with_timecode(anim, tc, position);

      return MOV_decode_frame(proxy, position, IMB_TC_NONE, IMB_PROXY_NONE);
    }
  }

#ifdef WITH_FFMPEG
  if (anim->state == MovieReader::State::Valid) {
    ibuf = ffmpeg_fetchibuf(anim, position, tc);
    if (ibuf) {
      anim->cur_position = position;
    }
  }
#endif

  if (ibuf) {
    STRNCPY(ibuf->filepath, anim->filepath);
    ibuf->fileframe = anim->cur_position + 1;
  }
  return ibuf;
}

int MOV_get_duration_frames(MovieReader *anim, IMB_Timecode_Type tc)
{
  if (tc == IMB_TC_NONE) {
    return anim->duration_in_frames;
  }

  const MovieIndex *idx = movie_open_index(anim, tc);
  if (!idx) {
    return anim->duration_in_frames;
  }

  return idx->get_duration();
}

double MOV_get_start_offset_seconds(const MovieReader *anim)
{
  return anim->start_offset;
}

float MOV_get_fps(const MovieReader *anim)
{
  if (anim->frs_sec > 0 && anim->frs_sec_base > 0) {
    return float(double(anim->frs_sec) / anim->frs_sec_base);
  }
  return 0.0f;
}

bool MOV_get_fps_num_denom(const MovieReader *anim, short &r_fps_num, float &r_fps_denom)
{
  if (anim->frs_sec > 0 && anim->frs_sec_base > 0) {
    if (anim->frs_sec > SHRT_MAX) {
      /* If numerator is larger than the max short, we need to approximate. */
      r_fps_num = SHRT_MAX;
      r_fps_denom = float(anim->frs_sec_base * double(SHRT_MAX) / double(anim->frs_sec));
    }
    else {
      r_fps_num = anim->frs_sec;
      r_fps_denom = float(anim->frs_sec_base);
    }
    return true;
  }
  return false;
}

int MOV_get_image_width(const MovieReader *anim)
{
  return ELEM(anim->video_rotation, 90, 270) ? anim->y : anim->x;
}

int MOV_get_image_height(const MovieReader *anim)
{
  return ELEM(anim->video_rotation, 90, 270) ? anim->x : anim->y;
}
