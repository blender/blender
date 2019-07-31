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
 * Partial Copyright (c) 2006 Peter Schlaile
 */

/** \file
 * \ingroup bke
 */

#ifdef WITH_FFMPEG
#  include <string.h>
#  include <stdio.h>

#  include <stdlib.h>

#  include "MEM_guardedalloc.h"

#  include "DNA_scene_types.h"

#  include "BLI_blenlib.h"

#  ifdef WITH_AUDASPACE
#    include <AUD_Device.h>
#    include <AUD_Special.h>
#  endif

#  include "BLI_math_base.h"
#  include "BLI_utildefines.h"

#  include "BKE_global.h"
#  include "BKE_idprop.h"
#  include "BKE_image.h"
#  include "BKE_library.h"
#  include "BKE_main.h"
#  include "BKE_report.h"
#  include "BKE_sound.h"
#  include "BKE_writeffmpeg.h"

#  include "IMB_imbuf.h"

/* This needs to be included after BLI_math_base.h otherwise it will redefine some math defines
 * like M_SQRT1_2 leading to warnings with MSVC */
#  include <libavformat/avformat.h>
#  include <libavcodec/avcodec.h>
#  include <libavutil/rational.h>
#  include <libavutil/samplefmt.h>
#  include <libswscale/swscale.h>

#  include "ffmpeg_compat.h"

struct StampData;

typedef struct FFMpegContext {
  int ffmpeg_type;
  int ffmpeg_codec;
  int ffmpeg_audio_codec;
  int ffmpeg_video_bitrate;
  int ffmpeg_audio_bitrate;
  int ffmpeg_gop_size;
  int ffmpeg_max_b_frames;
  int ffmpeg_autosplit;
  int ffmpeg_autosplit_count;
  bool ffmpeg_preview;

  int ffmpeg_crf;    /* set to 0 to not use CRF mode; we have another flag for lossless anyway. */
  int ffmpeg_preset; /* see eFFMpegPreset */

  AVFormatContext *outfile;
  AVStream *video_stream;
  AVStream *audio_stream;
  AVFrame *current_frame;
  struct SwsContext *img_convert_ctx;

  uint8_t *audio_input_buffer;
  uint8_t *audio_deinterleave_buffer;
  int audio_input_samples;
#  ifndef FFMPEG_HAVE_ENCODE_AUDIO2
  uint8_t *audio_output_buffer;
  int audio_outbuf_size;
#  endif
  double audio_time;
  bool audio_deinterleave;
  int audio_sample_size;

  struct StampData *stamp_data;

#  ifdef WITH_AUDASPACE
  AUD_Device *audio_mixdown_device;
#  endif
} FFMpegContext;

#  define FFMPEG_AUTOSPLIT_SIZE 2000000000

#  define PRINT \
    if (G.debug & G_DEBUG_FFMPEG) \
    printf

static void ffmpeg_dict_set_int(AVDictionary **dict, const char *key, int value);
static void ffmpeg_dict_set_float(AVDictionary **dict, const char *key, float value);
static void ffmpeg_set_expert_options(RenderData *rd);
static void ffmpeg_filepath_get(
    FFMpegContext *context, char *string, struct RenderData *rd, bool preview, const char *suffix);

/* Delete a picture buffer */

static void delete_picture(AVFrame *f)
{
  if (f) {
    if (f->data[0]) {
      MEM_freeN(f->data[0]);
    }
    av_free(f);
  }
}

static int request_float_audio_buffer(int codec_id)
{
  /* If any of these codecs, we prefer the float sample format (if supported) */
  return codec_id == AV_CODEC_ID_AAC || codec_id == AV_CODEC_ID_AC3 ||
         codec_id == AV_CODEC_ID_VORBIS;
}

#  ifdef WITH_AUDASPACE
static int write_audio_frame(FFMpegContext *context)
{
  AVCodecContext *c = NULL;
  AVPacket pkt;
  AVFrame *frame = NULL;
  int got_output = 0;

  c = context->audio_stream->codec;

  av_init_packet(&pkt);
  pkt.size = 0;
  pkt.data = NULL;

  AUD_Device_read(
      context->audio_mixdown_device, context->audio_input_buffer, context->audio_input_samples);
  context->audio_time += (double)context->audio_input_samples / (double)c->sample_rate;

#    ifdef FFMPEG_HAVE_ENCODE_AUDIO2
  frame = av_frame_alloc();
  av_frame_unref(frame);
  frame->pts = context->audio_time / av_q2d(c->time_base);
  frame->nb_samples = context->audio_input_samples;
  frame->format = c->sample_fmt;
#      ifdef FFMPEG_HAVE_FRAME_CHANNEL_LAYOUT
  frame->channel_layout = c->channel_layout;
#      endif

  if (context->audio_deinterleave) {
    int channel, i;
    uint8_t *temp;

    for (channel = 0; channel < c->channels; channel++) {
      for (i = 0; i < frame->nb_samples; i++) {
        memcpy(context->audio_deinterleave_buffer +
                   (i + channel * frame->nb_samples) * context->audio_sample_size,
               context->audio_input_buffer +
                   (c->channels * i + channel) * context->audio_sample_size,
               context->audio_sample_size);
      }
    }

    temp = context->audio_deinterleave_buffer;
    context->audio_deinterleave_buffer = context->audio_input_buffer;
    context->audio_input_buffer = temp;
  }

  avcodec_fill_audio_frame(frame,
                           c->channels,
                           c->sample_fmt,
                           context->audio_input_buffer,
                           context->audio_input_samples * c->channels * context->audio_sample_size,
                           1);

  if (avcodec_encode_audio2(c, &pkt, frame, &got_output) < 0) {
    // XXX error("Error writing audio packet");
    return -1;
  }

  if (!got_output) {
    av_frame_free(&frame);
    return 0;
  }
#    else
  pkt.size = avcodec_encode_audio(c,
                                  context->audio_output_buffer,
                                  context->audio_outbuf_size,
                                  (short *)context->audio_input_buffer);

  if (pkt.size < 0) {
    // XXX error("Error writing audio packet");
    return -1;
  }

  pkt.data = context->audio_output_buffer;
  got_output = 1;
#    endif

  if (got_output) {
    if (pkt.pts != AV_NOPTS_VALUE) {
      pkt.pts = av_rescale_q(pkt.pts, c->time_base, context->audio_stream->time_base);
    }
    if (pkt.dts != AV_NOPTS_VALUE) {
      pkt.dts = av_rescale_q(pkt.dts, c->time_base, context->audio_stream->time_base);
    }
    if (pkt.duration > 0) {
      pkt.duration = av_rescale_q(pkt.duration, c->time_base, context->audio_stream->time_base);
    }

    pkt.stream_index = context->audio_stream->index;

    pkt.flags |= AV_PKT_FLAG_KEY;

    if (av_interleaved_write_frame(context->outfile, &pkt) != 0) {
      fprintf(stderr, "Error writing audio packet!\n");
      if (frame) {
        av_frame_free(&frame);
      }
      return -1;
    }

    av_free_packet(&pkt);
  }

  if (frame) {
    av_frame_free(&frame);
  }

  return 0;
}
#  endif  // #ifdef WITH_AUDASPACE

/* Allocate a temporary frame */
static AVFrame *alloc_picture(int pix_fmt, int width, int height)
{
  AVFrame *f;
  uint8_t *buf;
  int size;

  /* allocate space for the struct */
  f = av_frame_alloc();
  if (!f) {
    return NULL;
  }
  size = avpicture_get_size(pix_fmt, width, height);
  /* allocate the actual picture buffer */
  buf = MEM_mallocN(size, "AVFrame buffer");
  if (!buf) {
    free(f);
    return NULL;
  }
  avpicture_fill((AVPicture *)f, buf, pix_fmt, width, height);
  return f;
}

/* Get the correct file extensions for the requested format,
 * first is always desired guess_format parameter */
static const char **get_file_extensions(int format)
{
  switch (format) {
    case FFMPEG_DV: {
      static const char *rv[] = {".dv", NULL};
      return rv;
    }
    case FFMPEG_MPEG1: {
      static const char *rv[] = {".mpg", ".mpeg", NULL};
      return rv;
    }
    case FFMPEG_MPEG2: {
      static const char *rv[] = {".dvd", ".vob", ".mpg", ".mpeg", NULL};
      return rv;
    }
    case FFMPEG_MPEG4: {
      static const char *rv[] = {".mp4", ".mpg", ".mpeg", NULL};
      return rv;
    }
    case FFMPEG_AVI: {
      static const char *rv[] = {".avi", NULL};
      return rv;
    }
    case FFMPEG_MOV: {
      static const char *rv[] = {".mov", NULL};
      return rv;
    }
    case FFMPEG_H264: {
      /* FIXME: avi for now... */
      static const char *rv[] = {".avi", NULL};
      return rv;
    }

    case FFMPEG_XVID: {
      /* FIXME: avi for now... */
      static const char *rv[] = {".avi", NULL};
      return rv;
    }
    case FFMPEG_FLV: {
      static const char *rv[] = {".flv", NULL};
      return rv;
    }
    case FFMPEG_MKV: {
      static const char *rv[] = {".mkv", NULL};
      return rv;
    }
    case FFMPEG_OGG: {
      static const char *rv[] = {".ogv", ".ogg", NULL};
      return rv;
    }
    default:
      return NULL;
  }
}

/* Write a frame to the output file */
static int write_video_frame(
    FFMpegContext *context, RenderData *rd, int cfra, AVFrame *frame, ReportList *reports)
{
  int got_output;
  int ret, success = 1;
  AVCodecContext *c = context->video_stream->codec;
  AVPacket packet = {0};

  av_init_packet(&packet);

  frame->pts = cfra;

  ret = avcodec_encode_video2(c, &packet, frame, &got_output);

  if (ret >= 0 && got_output) {
    if (packet.pts != AV_NOPTS_VALUE) {
      packet.pts = av_rescale_q(packet.pts, c->time_base, context->video_stream->time_base);
      PRINT("Video Frame PTS: %d\n", (int)packet.pts);
    }
    else {
      PRINT("Video Frame PTS: not set\n");
    }
    if (packet.dts != AV_NOPTS_VALUE) {
      packet.dts = av_rescale_q(packet.dts, c->time_base, context->video_stream->time_base);
      PRINT("Video Frame DTS: %d\n", (int)packet.dts);
    }
    else {
      PRINT("Video Frame DTS: not set\n");
    }

    packet.stream_index = context->video_stream->index;
    ret = av_interleaved_write_frame(context->outfile, &packet);
    success = (ret == 0);
  }
  else if (ret < 0) {
    success = 0;
  }

  if (!success) {
    BKE_report(reports, RPT_ERROR, "Error writing frame");
  }

  return success;
}

/* read and encode a frame of audio from the buffer */
static AVFrame *generate_video_frame(FFMpegContext *context, uint8_t *pixels, ReportList *reports)
{
  uint8_t *rendered_frame;

  AVCodecContext *c = context->video_stream->codec;
  int width = c->width;
  int height = c->height;
  AVFrame *rgb_frame;

  if (c->pix_fmt != AV_PIX_FMT_BGR32) {
    rgb_frame = alloc_picture(AV_PIX_FMT_BGR32, width, height);
    if (!rgb_frame) {
      BKE_report(reports, RPT_ERROR, "Could not allocate temporary frame");
      return NULL;
    }
  }
  else {
    rgb_frame = context->current_frame;
  }

  rendered_frame = pixels;

  /* Do RGBA-conversion and flipping in one step depending
   * on CPU-Endianess */

  if (ENDIAN_ORDER == L_ENDIAN) {
    int y;
    for (y = 0; y < height; y++) {
      uint8_t *target = rgb_frame->data[0] + width * 4 * (height - y - 1);
      uint8_t *src = rendered_frame + width * 4 * y;
      uint8_t *end = src + width * 4;
      while (src != end) {
        target[3] = src[3];
        target[2] = src[2];
        target[1] = src[1];
        target[0] = src[0];

        target += 4;
        src += 4;
      }
    }
  }
  else {
    int y;
    for (y = 0; y < height; y++) {
      uint8_t *target = rgb_frame->data[0] + width * 4 * (height - y - 1);
      uint8_t *src = rendered_frame + width * 4 * y;
      uint8_t *end = src + width * 4;
      while (src != end) {
        target[3] = src[0];
        target[2] = src[1];
        target[1] = src[2];
        target[0] = src[3];

        target += 4;
        src += 4;
      }
    }
  }

  if (c->pix_fmt != AV_PIX_FMT_BGR32) {
    sws_scale(context->img_convert_ctx,
              (const uint8_t *const *)rgb_frame->data,
              rgb_frame->linesize,
              0,
              c->height,
              context->current_frame->data,
              context->current_frame->linesize);
    delete_picture(rgb_frame);
  }

  context->current_frame->format = AV_PIX_FMT_BGR32;
  context->current_frame->width = width;
  context->current_frame->height = height;

  return context->current_frame;
}

static void set_ffmpeg_property_option(AVCodecContext *c,
                                       IDProperty *prop,
                                       AVDictionary **dictionary)
{
  char name[128];
  char *param;

  PRINT("FFMPEG expert option: %s: ", prop->name);

  BLI_strncpy(name, prop->name, sizeof(name));

  param = strchr(name, ':');

  if (param) {
    *param++ = '\0';
  }

  switch (prop->type) {
    case IDP_STRING:
      PRINT("%s.\n", IDP_String(prop));
      av_dict_set(dictionary, name, IDP_String(prop), 0);
      break;
    case IDP_FLOAT:
      PRINT("%g.\n", IDP_Float(prop));
      ffmpeg_dict_set_float(dictionary, prop->name, IDP_Float(prop));
      break;
    case IDP_INT:
      PRINT("%d.\n", IDP_Int(prop));

      if (param) {
        if (IDP_Int(prop)) {
          av_dict_set(dictionary, name, param, 0);
        }
        else {
          return;
        }
      }
      else {
        ffmpeg_dict_set_int(dictionary, prop->name, IDP_Int(prop));
      }
      break;
  }
}

static int ffmpeg_proprty_valid(AVCodecContext *c, const char *prop_name, IDProperty *curr)
{
  int valid = 1;

  if (STREQ(prop_name, "video")) {
    if (STREQ(curr->name, "bf")) {
      /* flash codec doesn't support b frames */
      valid &= c->codec_id != AV_CODEC_ID_FLV1;
    }
  }

  return valid;
}

static void set_ffmpeg_properties(RenderData *rd,
                                  AVCodecContext *c,
                                  const char *prop_name,
                                  AVDictionary **dictionary)
{
  IDProperty *prop;
  IDProperty *curr;

  /* TODO(sergey): This is actually rather stupid, because changing
   * codec settings in render panel would also set expert options.
   *
   * But we need ti here in order to get rid of deprecated settings
   * when opening old files in new blender.
   *
   * For as long we don't allow editing properties in the interface
   * it's all good. bug if we allow editing them, we'll need to
   * replace it with some smarter code which would port settings
   * from deprecated to new one.
   */
  ffmpeg_set_expert_options(rd);

  if (!rd->ffcodecdata.properties) {
    return;
  }

  prop = IDP_GetPropertyFromGroup(rd->ffcodecdata.properties, prop_name);
  if (!prop) {
    return;
  }

  for (curr = prop->data.group.first; curr; curr = curr->next) {
    if (ffmpeg_proprty_valid(c, prop_name, curr)) {
      set_ffmpeg_property_option(c, curr, dictionary);
    }
  }
}

/* prepare a video stream for the output file */

static AVStream *alloc_video_stream(FFMpegContext *context,
                                    RenderData *rd,
                                    int codec_id,
                                    AVFormatContext *of,
                                    int rectx,
                                    int recty,
                                    char *error,
                                    int error_size)
{
  AVStream *st;
  AVCodecContext *c;
  AVCodec *codec;
  AVDictionary *opts = NULL;

  error[0] = '\0';

  st = avformat_new_stream(of, NULL);
  if (!st) {
    return NULL;
  }
  st->id = 0;

  /* Set up the codec context */

  c = st->codec;
  c->thread_count = 0;
  c->thread_type = FF_THREAD_FRAME;

  c->codec_id = codec_id;
  c->codec_type = AVMEDIA_TYPE_VIDEO;

  /* Get some values from the current render settings */

  c->width = rectx;
  c->height = recty;

  /* FIXME: Really bad hack (tm) for NTSC support */
  if (context->ffmpeg_type == FFMPEG_DV && rd->frs_sec != 25) {
    c->time_base.den = 2997;
    c->time_base.num = 100;
  }
  else if ((float)((int)rd->frs_sec_base) == rd->frs_sec_base) {
    c->time_base.den = rd->frs_sec;
    c->time_base.num = (int)rd->frs_sec_base;
  }
  else if (compare_ff(rd->frs_sec_base, 1.001f, 0.000001f)) {
    /* This converts xx/1.001 (which is used in presets) to xx000/1001 (which is used in the rest
     * of the world, including FFmpeg). */
    c->time_base.den = (int)(rd->frs_sec * 1000);
    c->time_base.num = (int)(rd->frs_sec_base * 1000);
  }
  else {
    /* This calculates a fraction (DENUM_MAX / num) which approximates the scene frame rate
     * (frs_sec / frs_sec_base). It uses the maximum denominator allowed by FFmpeg.
     */
    const double DENUM_MAX = (codec_id == AV_CODEC_ID_MPEG4) ? (1UL << 16) - 1 : (1UL << 31) - 1;
    const double num = (DENUM_MAX / (double)rd->frs_sec) * rd->frs_sec_base;

    c->time_base.den = (int)DENUM_MAX;
    c->time_base.num = (int)num;
  }

  c->gop_size = context->ffmpeg_gop_size;
  c->max_b_frames = context->ffmpeg_max_b_frames;

  if (context->ffmpeg_crf >= 0) {
    ffmpeg_dict_set_int(&opts, "crf", context->ffmpeg_crf);
  }
  else {
    c->bit_rate = context->ffmpeg_video_bitrate * 1000;
    c->rc_max_rate = rd->ffcodecdata.rc_max_rate * 1000;
    c->rc_min_rate = rd->ffcodecdata.rc_min_rate * 1000;
    c->rc_buffer_size = rd->ffcodecdata.rc_buffer_size * 1024;
  }

  if (context->ffmpeg_preset) {
    /* 'preset' is used by h.264, 'deadline' is used by webm/vp9. I'm not
     * setting those properties conditionally based on the video codec,
     * as the FFmpeg encoder simply ignores unknown settings anyway. */
    char const *preset_name = NULL;   /* used by h.264 */
    char const *deadline_name = NULL; /* used by webm/vp9 */
    switch (context->ffmpeg_preset) {
      case FFM_PRESET_GOOD:
        preset_name = "medium";
        deadline_name = "good";
        break;
      case FFM_PRESET_BEST:
        preset_name = "slower";
        deadline_name = "best";
        break;
      case FFM_PRESET_REALTIME:
        preset_name = "superfast";
        deadline_name = "realtime";
        break;
      default:
        printf("Unknown preset number %i, ignoring.\n", context->ffmpeg_preset);
    }
    if (preset_name != NULL) {
      av_dict_set(&opts, "preset", preset_name, 0);
    }
    if (deadline_name != NULL) {
      av_dict_set(&opts, "deadline", deadline_name, 0);
    }
  }

  /* Deprecated and not doing anything since July 2015, deleted in recent ffmpeg */
  // c->me_method = ME_EPZS;

  codec = avcodec_find_encoder(c->codec_id);
  if (!codec) {
    return NULL;
  }

  /* Be sure to use the correct pixel format(e.g. RGB, YUV) */

  if (codec->pix_fmts) {
    c->pix_fmt = codec->pix_fmts[0];
  }
  else {
    /* makes HuffYUV happy ... */
    c->pix_fmt = AV_PIX_FMT_YUV422P;
  }

  if (context->ffmpeg_type == FFMPEG_XVID) {
    /* arghhhh ... */
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->codec_tag = (('D' << 24) + ('I' << 16) + ('V' << 8) + 'X');
  }

  if (codec_id == AV_CODEC_ID_H264) {
    /* correct wrong default ffmpeg param which crash x264 */
    c->qmin = 10;
    c->qmax = 51;
  }

  /* Keep lossless encodes in the RGB domain. */
  if (codec_id == AV_CODEC_ID_HUFFYUV) {
    if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_BGRA;
    }
    else {
      c->pix_fmt = AV_PIX_FMT_RGB32;
    }
  }

  if (codec_id == AV_CODEC_ID_FFV1) {
    c->pix_fmt = AV_PIX_FMT_RGB32;
  }

  if (codec_id == AV_CODEC_ID_QTRLE) {
    /* Always write to ARGB. The default pixel format of QTRLE is RGB24, which uses 3 bytes per
     * pixels, which breaks the export. */
    c->pix_fmt = AV_PIX_FMT_ARGB;
  }

  if (codec_id == AV_CODEC_ID_VP9) {
    if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_YUVA420P;
    }
  }

  if (codec_id == AV_CODEC_ID_PNG) {
    if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_RGBA;
    }
  }

  if ((of->oformat->flags & AVFMT_GLOBALHEADER)) {
    PRINT("Using global header\n");
    c->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  /* xasp & yasp got float lately... */

  st->sample_aspect_ratio = c->sample_aspect_ratio = av_d2q(((double)rd->xasp / (double)rd->yasp),
                                                            255);
  st->avg_frame_rate = av_inv_q(c->time_base);

  set_ffmpeg_properties(rd, c, "video", &opts);

  if (avcodec_open2(c, codec, &opts) < 0) {
    BLI_strncpy(error, IMB_ffmpeg_last_error(), error_size);
    av_dict_free(&opts);
    return NULL;
  }
  av_dict_free(&opts);

  context->current_frame = alloc_picture(c->pix_fmt, c->width, c->height);

  context->img_convert_ctx = sws_getContext(c->width,
                                            c->height,
                                            AV_PIX_FMT_BGR32,
                                            c->width,
                                            c->height,
                                            c->pix_fmt,
                                            SWS_BICUBIC,
                                            NULL,
                                            NULL,
                                            NULL);
  return st;
}

static AVStream *alloc_audio_stream(FFMpegContext *context,
                                    RenderData *rd,
                                    int codec_id,
                                    AVFormatContext *of,
                                    char *error,
                                    int error_size)
{
  AVStream *st;
  AVCodecContext *c;
  AVCodec *codec;
  AVDictionary *opts = NULL;

  error[0] = '\0';

  st = avformat_new_stream(of, NULL);
  if (!st) {
    return NULL;
  }
  st->id = 1;

  c = st->codec;
  c->thread_count = 0;
  c->thread_type = FF_THREAD_FRAME;

  c->codec_id = codec_id;
  c->codec_type = AVMEDIA_TYPE_AUDIO;

  c->sample_rate = rd->ffcodecdata.audio_mixrate;
  c->bit_rate = context->ffmpeg_audio_bitrate * 1000;
  c->sample_fmt = AV_SAMPLE_FMT_S16;
  c->channels = rd->ffcodecdata.audio_channels;

#  ifdef FFMPEG_HAVE_FRAME_CHANNEL_LAYOUT
  switch (rd->ffcodecdata.audio_channels) {
    case FFM_CHANNELS_MONO:
      c->channel_layout = AV_CH_LAYOUT_MONO;
      break;
    case FFM_CHANNELS_STEREO:
      c->channel_layout = AV_CH_LAYOUT_STEREO;
      break;
    case FFM_CHANNELS_SURROUND4:
      c->channel_layout = AV_CH_LAYOUT_QUAD;
      break;
    case FFM_CHANNELS_SURROUND51:
      c->channel_layout = AV_CH_LAYOUT_5POINT1_BACK;
      break;
    case FFM_CHANNELS_SURROUND71:
      c->channel_layout = AV_CH_LAYOUT_7POINT1;
      break;
  }
#  endif

  if (request_float_audio_buffer(codec_id)) {
    /* mainly for AAC codec which is experimental */
    c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    c->sample_fmt = AV_SAMPLE_FMT_FLT;
  }

  codec = avcodec_find_encoder(c->codec_id);
  if (!codec) {
    // XXX error("Couldn't find a valid audio codec");
    return NULL;
  }

  if (codec->sample_fmts) {
    /* Check if the preferred sample format for this codec is supported.
     * this is because, depending on the version of libav,
     * and with the whole ffmpeg/libav fork situation,
     * you have various implementations around.
     * Float samples in particular are not always supported. */
    const enum AVSampleFormat *p = codec->sample_fmts;
    for (; *p != -1; p++) {
      if (*p == st->codec->sample_fmt) {
        break;
      }
    }
    if (*p == -1) {
      /* sample format incompatible with codec. Defaulting to a format known to work */
      st->codec->sample_fmt = codec->sample_fmts[0];
    }
  }

  if (codec->supported_samplerates) {
    const int *p = codec->supported_samplerates;
    int best = 0;
    int best_dist = INT_MAX;
    for (; *p; p++) {
      int dist = abs(st->codec->sample_rate - *p);
      if (dist < best_dist) {
        best_dist = dist;
        best = *p;
      }
    }
    /* best is the closest supported sample rate (same as selected if best_dist == 0) */
    st->codec->sample_rate = best;
  }

  if (of->oformat->flags & AVFMT_GLOBALHEADER) {
    c->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  set_ffmpeg_properties(rd, c, "audio", &opts);

  if (avcodec_open2(c, codec, &opts) < 0) {
    // XXX error("Couldn't initialize audio codec");
    BLI_strncpy(error, IMB_ffmpeg_last_error(), error_size);
    av_dict_free(&opts);
    return NULL;
  }
  av_dict_free(&opts);

  /* need to prevent floating point exception when using vorbis audio codec,
   * initialize this value in the same way as it's done in FFmpeg itself (sergey) */
  st->codec->time_base.num = 1;
  st->codec->time_base.den = st->codec->sample_rate;

#  ifndef FFMPEG_HAVE_ENCODE_AUDIO2
  context->audio_outbuf_size = FF_MIN_BUFFER_SIZE;
#  endif

  if (c->frame_size == 0) {
    // used to be if ((c->codec_id >= CODEC_ID_PCM_S16LE) && (c->codec_id <= CODEC_ID_PCM_DVD))
    // not sure if that is needed anymore, so let's try out if there are any
    // complaints regarding some ffmpeg versions users might have
    context->audio_input_samples = FF_MIN_BUFFER_SIZE * 8 / c->bits_per_coded_sample / c->channels;
  }
  else {
    context->audio_input_samples = c->frame_size;
#  ifndef FFMPEG_HAVE_ENCODE_AUDIO2
    if (c->frame_size * c->channels * sizeof(int16_t) * 4 > context->audio_outbuf_size) {
      context->audio_outbuf_size = c->frame_size * c->channels * sizeof(int16_t) * 4;
    }
#  endif
  }

  context->audio_deinterleave = av_sample_fmt_is_planar(c->sample_fmt);

  context->audio_sample_size = av_get_bytes_per_sample(c->sample_fmt);

  context->audio_input_buffer = (uint8_t *)av_malloc(context->audio_input_samples * c->channels *
                                                     context->audio_sample_size);
#  ifndef FFMPEG_HAVE_ENCODE_AUDIO2
  context->audio_output_buffer = (uint8_t *)av_malloc(context->audio_outbuf_size);
#  endif

  if (context->audio_deinterleave) {
    context->audio_deinterleave_buffer = (uint8_t *)av_malloc(
        context->audio_input_samples * c->channels * context->audio_sample_size);
  }

  context->audio_time = 0.0f;

  return st;
}
/* essential functions -- start, append, end */

static void ffmpeg_dict_set_int(AVDictionary **dict, const char *key, int value)
{
  char buffer[32];

  BLI_snprintf(buffer, sizeof(buffer), "%d", value);

  av_dict_set(dict, key, buffer, 0);
}

static void ffmpeg_dict_set_float(AVDictionary **dict, const char *key, float value)
{
  char buffer[32];

  BLI_snprintf(buffer, sizeof(buffer), "%.8f", value);

  av_dict_set(dict, key, buffer, 0);
}

static void ffmpeg_add_metadata_callback(void *data,
                                         const char *propname,
                                         char *propvalue,
                                         int len)
{
  AVDictionary **metadata = (AVDictionary **)data;
  av_dict_set(metadata, propname, propvalue, 0);
}

static int start_ffmpeg_impl(FFMpegContext *context,
                             struct RenderData *rd,
                             int rectx,
                             int recty,
                             const char *suffix,
                             ReportList *reports)
{
  /* Handle to the output file */
  AVFormatContext *of;
  AVOutputFormat *fmt;
  AVDictionary *opts = NULL;
  char name[FILE_MAX], error[1024];
  const char **exts;

  context->ffmpeg_type = rd->ffcodecdata.type;
  context->ffmpeg_codec = rd->ffcodecdata.codec;
  context->ffmpeg_audio_codec = rd->ffcodecdata.audio_codec;
  context->ffmpeg_video_bitrate = rd->ffcodecdata.video_bitrate;
  context->ffmpeg_audio_bitrate = rd->ffcodecdata.audio_bitrate;
  context->ffmpeg_gop_size = rd->ffcodecdata.gop_size;
  context->ffmpeg_autosplit = rd->ffcodecdata.flags & FFMPEG_AUTOSPLIT_OUTPUT;
  context->ffmpeg_crf = rd->ffcodecdata.constant_rate_factor;
  context->ffmpeg_preset = rd->ffcodecdata.ffmpeg_preset;

  if ((rd->ffcodecdata.flags & FFMPEG_USE_MAX_B_FRAMES) != 0) {
    context->ffmpeg_max_b_frames = rd->ffcodecdata.max_b_frames;
  }

  /* Determine the correct filename */
  ffmpeg_filepath_get(context, name, rd, context->ffmpeg_preview, suffix);
  PRINT(
      "Starting output to %s(ffmpeg)...\n"
      "  Using type=%d, codec=%d, audio_codec=%d,\n"
      "  video_bitrate=%d, audio_bitrate=%d,\n"
      "  gop_size=%d, autosplit=%d\n"
      "  render width=%d, render height=%d\n",
      name,
      context->ffmpeg_type,
      context->ffmpeg_codec,
      context->ffmpeg_audio_codec,
      context->ffmpeg_video_bitrate,
      context->ffmpeg_audio_bitrate,
      context->ffmpeg_gop_size,
      context->ffmpeg_autosplit,
      rectx,
      recty);

  exts = get_file_extensions(context->ffmpeg_type);
  if (!exts) {
    BKE_report(reports, RPT_ERROR, "No valid formats found");
    return 0;
  }
  fmt = av_guess_format(NULL, exts[0], NULL);
  if (!fmt) {
    BKE_report(reports, RPT_ERROR, "No valid formats found");
    return 0;
  }

  of = avformat_alloc_context();
  if (!of) {
    BKE_report(reports, RPT_ERROR, "Error opening output file");
    return 0;
  }

  /* Returns after this must 'goto fail;' */

  of->oformat = fmt;

  /* Only bother with setting packet size & mux rate when CRF is not used. */
  if (context->ffmpeg_crf == 0) {
    of->packet_size = rd->ffcodecdata.mux_packet_size;
    if (context->ffmpeg_audio_codec != AV_CODEC_ID_NONE) {
      ffmpeg_dict_set_int(&opts, "muxrate", rd->ffcodecdata.mux_rate);
    }
    else {
      av_dict_set(&opts, "muxrate", "0", 0);
    }
  }

  ffmpeg_dict_set_int(&opts, "preload", (int)(0.5 * AV_TIME_BASE));

  of->max_delay = (int)(0.7 * AV_TIME_BASE);

  fmt->audio_codec = context->ffmpeg_audio_codec;

  BLI_strncpy(of->filename, name, sizeof(of->filename));
  /* set the codec to the user's selection */
  switch (context->ffmpeg_type) {
    case FFMPEG_AVI:
    case FFMPEG_MOV:
    case FFMPEG_MKV:
      fmt->video_codec = context->ffmpeg_codec;
      break;
    case FFMPEG_OGG:
      fmt->video_codec = AV_CODEC_ID_THEORA;
      break;
    case FFMPEG_DV:
      fmt->video_codec = AV_CODEC_ID_DVVIDEO;
      break;
    case FFMPEG_MPEG1:
      fmt->video_codec = AV_CODEC_ID_MPEG1VIDEO;
      break;
    case FFMPEG_MPEG2:
      fmt->video_codec = AV_CODEC_ID_MPEG2VIDEO;
      break;
    case FFMPEG_H264:
      fmt->video_codec = AV_CODEC_ID_H264;
      break;
    case FFMPEG_XVID:
      fmt->video_codec = AV_CODEC_ID_MPEG4;
      break;
    case FFMPEG_FLV:
      fmt->video_codec = AV_CODEC_ID_FLV1;
      break;
    case FFMPEG_MPEG4:
    default:
      fmt->video_codec = context->ffmpeg_codec;
      break;
  }
  if (fmt->video_codec == AV_CODEC_ID_DVVIDEO) {
    if (rectx != 720) {
      BKE_report(reports, RPT_ERROR, "Render width has to be 720 pixels for DV!");
      goto fail;
    }
    if (rd->frs_sec != 25 && recty != 480) {
      BKE_report(reports, RPT_ERROR, "Render height has to be 480 pixels for DV-NTSC!");
      goto fail;
    }
    if (rd->frs_sec == 25 && recty != 576) {
      BKE_report(reports, RPT_ERROR, "Render height has to be 576 pixels for DV-PAL!");
      goto fail;
    }
  }

  if (context->ffmpeg_type == FFMPEG_DV) {
    fmt->audio_codec = AV_CODEC_ID_PCM_S16LE;
    if (context->ffmpeg_audio_codec != AV_CODEC_ID_NONE &&
        rd->ffcodecdata.audio_mixrate != 48000 && rd->ffcodecdata.audio_channels != 2) {
      BKE_report(reports, RPT_ERROR, "FFMPEG only supports 48khz / stereo audio for DV!");
      goto fail;
    }
  }

  if (fmt->video_codec != AV_CODEC_ID_NONE) {
    context->video_stream = alloc_video_stream(
        context, rd, fmt->video_codec, of, rectx, recty, error, sizeof(error));
    PRINT("alloc video stream %p\n", context->video_stream);
    if (!context->video_stream) {
      if (error[0]) {
        BKE_report(reports, RPT_ERROR, error);
      }
      else {
        BKE_report(reports, RPT_ERROR, "Error initializing video stream");
      }
      goto fail;
    }
  }

  if (context->ffmpeg_audio_codec != AV_CODEC_ID_NONE) {
    context->audio_stream = alloc_audio_stream(
        context, rd, fmt->audio_codec, of, error, sizeof(error));
    if (!context->audio_stream) {
      if (error[0]) {
        BKE_report(reports, RPT_ERROR, error);
      }
      else {
        BKE_report(reports, RPT_ERROR, "Error initializing audio stream");
      }
      goto fail;
    }
  }
  if (!(fmt->flags & AVFMT_NOFILE)) {
    if (avio_open(&of->pb, name, AVIO_FLAG_WRITE) < 0) {
      BKE_report(reports, RPT_ERROR, "Could not open file for writing");
      goto fail;
    }
  }

  if (context->stamp_data != NULL) {
    BKE_stamp_info_callback(
        &of->metadata, context->stamp_data, ffmpeg_add_metadata_callback, false);
  }

  if (avformat_write_header(of, NULL) < 0) {
    BKE_report(reports,
               RPT_ERROR,
               "Could not initialize streams, probably unsupported codec combination");
    goto fail;
  }

  context->outfile = of;
  av_dump_format(of, 0, name, 1);
  av_dict_free(&opts);

  return 1;

fail:
  if (of->pb) {
    avio_close(of->pb);
  }

  if (context->video_stream && context->video_stream->codec) {
    avcodec_close(context->video_stream->codec);
    context->video_stream = NULL;
  }

  if (context->audio_stream && context->audio_stream->codec) {
    avcodec_close(context->audio_stream->codec);
    context->audio_stream = NULL;
  }

  av_dict_free(&opts);
  avformat_free_context(of);
  return 0;
}

/**
 * Writes any delayed frames in the encoder. This function is called before
 * closing the encoder.
 *
 * <p>
 * Since an encoder may use both past and future frames to predict
 * inter-frames (H.264 B-frames, for example), it can output the frames
 * in a different order from the one it was given.
 * For example, when sending frames 1, 2, 3, 4 to the encoder, it may write
 * them in the order 1, 4, 2, 3 - first the two frames used for prediction,
 * and then the bidirectionally-predicted frames. What this means in practice
 * is that the encoder may not immediately produce one output frame for each
 * input frame. These delayed frames must be flushed before we close the
 * stream. We do this by calling avcodec_encode_video with NULL for the last
 * parameter.
 * </p>
 */
static void flush_ffmpeg(FFMpegContext *context)
{
  int ret = 0;

  AVCodecContext *c = context->video_stream->codec;
  /* get the delayed frames */
  while (1) {
    int got_output;
    AVPacket packet = {0};
    av_init_packet(&packet);

    ret = avcodec_encode_video2(c, &packet, NULL, &got_output);
    if (ret < 0) {
      fprintf(stderr, "Error encoding delayed frame %d\n", ret);
      break;
    }
    if (!got_output) {
      break;
    }
    if (packet.pts != AV_NOPTS_VALUE) {
      packet.pts = av_rescale_q(packet.pts, c->time_base, context->video_stream->time_base);
      PRINT("Video Frame PTS: %d\n", (int)packet.pts);
    }
    else {
      PRINT("Video Frame PTS: not set\n");
    }
    if (packet.dts != AV_NOPTS_VALUE) {
      packet.dts = av_rescale_q(packet.dts, c->time_base, context->video_stream->time_base);
      PRINT("Video Frame DTS: %d\n", (int)packet.dts);
    }
    else {
      PRINT("Video Frame DTS: not set\n");
    }

    packet.stream_index = context->video_stream->index;
    ret = av_interleaved_write_frame(context->outfile, &packet);
    if (ret != 0) {
      fprintf(stderr, "Error writing delayed frame %d\n", ret);
      break;
    }
  }
  avcodec_flush_buffers(context->video_stream->codec);
}

/* **********************************************************************
 * * public interface
 * ********************************************************************** */

/* Get the output filename-- similar to the other output formats */
static void ffmpeg_filepath_get(
    FFMpegContext *context, char *string, RenderData *rd, bool preview, const char *suffix)
{
  char autosplit[20];

  const char **exts = get_file_extensions(rd->ffcodecdata.type);
  const char **fe = exts;
  int sfra, efra;

  if (!string || !exts) {
    return;
  }

  if (preview) {
    sfra = rd->psfra;
    efra = rd->pefra;
  }
  else {
    sfra = rd->sfra;
    efra = rd->efra;
  }

  strcpy(string, rd->pic);
  BLI_path_abs(string, BKE_main_blendfile_path_from_global());

  BLI_make_existing_file(string);

  autosplit[0] = '\0';

  if ((rd->ffcodecdata.flags & FFMPEG_AUTOSPLIT_OUTPUT) != 0) {
    if (context) {
      sprintf(autosplit, "_%03d", context->ffmpeg_autosplit_count);
    }
  }

  if (rd->scemode & R_EXTENSION) {
    while (*fe) {
      if (BLI_strcasecmp(string + strlen(string) - strlen(*fe), *fe) == 0) {
        break;
      }
      fe++;
    }

    if (*fe == NULL) {
      strcat(string, autosplit);

      BLI_path_frame_range(string, sfra, efra, 4);
      strcat(string, *exts);
    }
    else {
      *(string + strlen(string) - strlen(*fe)) = '\0';
      strcat(string, autosplit);
      strcat(string, *fe);
    }
  }
  else {
    if (BLI_path_frame_check_chars(string)) {
      BLI_path_frame_range(string, sfra, efra, 4);
    }

    strcat(string, autosplit);
  }

  BLI_path_suffix(string, FILE_MAX, suffix, "");
}

void BKE_ffmpeg_filepath_get(char *string, RenderData *rd, bool preview, const char *suffix)
{
  ffmpeg_filepath_get(NULL, string, rd, preview, suffix);
}

int BKE_ffmpeg_start(void *context_v,
                     struct Scene *scene,
                     RenderData *rd,
                     int rectx,
                     int recty,
                     ReportList *reports,
                     bool preview,
                     const char *suffix)
{
  int success;
  FFMpegContext *context = context_v;

  context->ffmpeg_autosplit_count = 0;
  context->ffmpeg_preview = preview;
  context->stamp_data = BKE_stamp_info_from_scene_static(scene);

  success = start_ffmpeg_impl(context, rd, rectx, recty, suffix, reports);
#  ifdef WITH_AUDASPACE
  if (context->audio_stream) {
    AVCodecContext *c = context->audio_stream->codec;
    AUD_DeviceSpecs specs;
    specs.channels = c->channels;

    switch (av_get_packed_sample_fmt(c->sample_fmt)) {
      case AV_SAMPLE_FMT_U8:
        specs.format = AUD_FORMAT_U8;
        break;
      case AV_SAMPLE_FMT_S16:
        specs.format = AUD_FORMAT_S16;
        break;
      case AV_SAMPLE_FMT_S32:
        specs.format = AUD_FORMAT_S32;
        break;
      case AV_SAMPLE_FMT_FLT:
        specs.format = AUD_FORMAT_FLOAT32;
        break;
      case AV_SAMPLE_FMT_DBL:
        specs.format = AUD_FORMAT_FLOAT64;
        break;
      default:
        return -31415;
    }

    specs.rate = rd->ffcodecdata.audio_mixrate;
    context->audio_mixdown_device = BKE_sound_mixdown(
        scene, specs, preview ? rd->psfra : rd->sfra, rd->ffcodecdata.audio_volume);
#    ifdef FFMPEG_CODEC_TIME_BASE
    c->time_base.den = specs.rate;
    c->time_base.num = 1;
#    endif
  }
#  endif
  return success;
}

static void end_ffmpeg_impl(FFMpegContext *context, int is_autosplit);

#  ifdef WITH_AUDASPACE
static void write_audio_frames(FFMpegContext *context, double to_pts)
{
  int finished = 0;

  while (context->audio_stream && !finished) {
    if ((context->audio_time >= to_pts) || (write_audio_frame(context))) {
      finished = 1;
    }
  }
}
#  endif

int BKE_ffmpeg_append(void *context_v,
                      RenderData *rd,
                      int start_frame,
                      int frame,
                      int *pixels,
                      int rectx,
                      int recty,
                      const char *suffix,
                      ReportList *reports)
{
  FFMpegContext *context = context_v;
  AVFrame *avframe;
  int success = 1;

  PRINT("Writing frame %i, render width=%d, render height=%d\n", frame, rectx, recty);

  /* why is this done before writing the video frame and again at end_ffmpeg? */
  //  write_audio_frames(frame / (((double)rd->frs_sec) / rd->frs_sec_base));

  if (context->video_stream) {
    avframe = generate_video_frame(context, (unsigned char *)pixels, reports);
    success = (avframe && write_video_frame(context, rd, frame - start_frame, avframe, reports));

    if (context->ffmpeg_autosplit) {
      if (avio_tell(context->outfile->pb) > FFMPEG_AUTOSPLIT_SIZE) {
        end_ffmpeg_impl(context, true);
        context->ffmpeg_autosplit_count++;
        success &= start_ffmpeg_impl(context, rd, rectx, recty, suffix, reports);
      }
    }
  }

#  ifdef WITH_AUDASPACE
  write_audio_frames(context,
                     (frame - start_frame) / (((double)rd->frs_sec) / (double)rd->frs_sec_base));
#  endif
  return success;
}

static void end_ffmpeg_impl(FFMpegContext *context, int is_autosplit)
{
  PRINT("Closing ffmpeg...\n");

#  ifdef WITH_AUDASPACE
  if (is_autosplit == false) {
    if (context->audio_mixdown_device) {
      AUD_Device_free(context->audio_mixdown_device);
      context->audio_mixdown_device = NULL;
    }
  }
#  endif

  if (context->video_stream && context->video_stream->codec) {
    PRINT("Flushing delayed frames...\n");
    flush_ffmpeg(context);
  }

  if (context->outfile) {
    av_write_trailer(context->outfile);
  }

  /* Close the video codec */

  if (context->video_stream != NULL && context->video_stream->codec != NULL) {
    avcodec_close(context->video_stream->codec);
    PRINT("zero video stream %p\n", context->video_stream);
    context->video_stream = NULL;
  }

  if (context->audio_stream != NULL && context->audio_stream->codec != NULL) {
    avcodec_close(context->audio_stream->codec);
    context->audio_stream = NULL;
  }

  /* free the temp buffer */
  if (context->current_frame != NULL) {
    delete_picture(context->current_frame);
    context->current_frame = NULL;
  }
  if (context->outfile != NULL && context->outfile->oformat) {
    if (!(context->outfile->oformat->flags & AVFMT_NOFILE)) {
      avio_close(context->outfile->pb);
    }
  }
  if (context->outfile != NULL) {
    avformat_free_context(context->outfile);
    context->outfile = NULL;
  }
  if (context->audio_input_buffer != NULL) {
    av_free(context->audio_input_buffer);
    context->audio_input_buffer = NULL;
  }
#  ifndef FFMPEG_HAVE_ENCODE_AUDIO2
  if (context->audio_output_buffer != NULL) {
    av_free(context->audio_output_buffer);
    context->audio_output_buffer = NULL;
  }
#  endif

  if (context->audio_deinterleave_buffer != NULL) {
    av_free(context->audio_deinterleave_buffer);
    context->audio_deinterleave_buffer = NULL;
  }

  if (context->img_convert_ctx != NULL) {
    sws_freeContext(context->img_convert_ctx);
    context->img_convert_ctx = NULL;
  }
}

void BKE_ffmpeg_end(void *context_v)
{
  FFMpegContext *context = context_v;
  end_ffmpeg_impl(context, false);
}

/* properties */

void BKE_ffmpeg_property_del(RenderData *rd, void *type, void *prop_)
{
  struct IDProperty *prop = (struct IDProperty *)prop_;
  IDProperty *group;

  if (!rd->ffcodecdata.properties) {
    return;
  }

  group = IDP_GetPropertyFromGroup(rd->ffcodecdata.properties, type);
  if (group && prop) {
    IDP_FreeFromGroup(group, prop);
  }
}

static IDProperty *BKE_ffmpeg_property_add(RenderData *rd,
                                           const char *type,
                                           const AVOption *o,
                                           const AVOption *parent)
{
  AVCodecContext c;
  IDProperty *group;
  IDProperty *prop;
  IDPropertyTemplate val;
  int idp_type;
  char name[256];

  val.i = 0;

  avcodec_get_context_defaults3(&c, NULL);

  if (!rd->ffcodecdata.properties) {
    rd->ffcodecdata.properties = IDP_New(IDP_GROUP, &val, "ffmpeg");
  }

  group = IDP_GetPropertyFromGroup(rd->ffcodecdata.properties, type);

  if (!group) {
    group = IDP_New(IDP_GROUP, &val, type);
    IDP_AddToGroup(rd->ffcodecdata.properties, group);
  }

  if (parent) {
    BLI_snprintf(name, sizeof(name), "%s:%s", parent->name, o->name);
  }
  else {
    BLI_strncpy(name, o->name, sizeof(name));
  }

  PRINT("ffmpeg_property_add: %s %s\n", type, name);

  prop = IDP_GetPropertyFromGroup(group, name);
  if (prop) {
    return prop;
  }

  switch (o->type) {
    case AV_OPT_TYPE_INT:
    case AV_OPT_TYPE_INT64:
      val.i = FFMPEG_DEF_OPT_VAL_INT(o);
      idp_type = IDP_INT;
      break;
    case AV_OPT_TYPE_DOUBLE:
    case AV_OPT_TYPE_FLOAT:
      val.f = FFMPEG_DEF_OPT_VAL_DOUBLE(o);
      idp_type = IDP_FLOAT;
      break;
    case AV_OPT_TYPE_STRING:
      val.string.str =
          (char
               *)"                                                                               ";
      val.string.len = 80;
      idp_type = IDP_STRING;
      break;
    case AV_OPT_TYPE_CONST:
      val.i = 1;
      idp_type = IDP_INT;
      break;
    default:
      return NULL;
  }
  prop = IDP_New(idp_type, &val, name);
  IDP_AddToGroup(group, prop);
  return prop;
}

/* not all versions of ffmpeg include that, so here we go ... */

int BKE_ffmpeg_property_add_string(RenderData *rd, const char *type, const char *str)
{
  AVCodecContext c;
  const AVOption *o = NULL;
  const AVOption *p = NULL;
  char name_[128];
  char *name;
  char *param;
  IDProperty *prop = NULL;

  avcodec_get_context_defaults3(&c, NULL);

  BLI_strncpy(name_, str, sizeof(name_));

  name = name_;
  while (*name == ' ') {
    name++;
  }

  param = strchr(name, ':');

  if (!param) {
    param = strchr(name, ' ');
  }
  if (param) {
    *param++ = '\0';
    while (*param == ' ') {
      param++;
    }
  }

  o = av_opt_find(&c, name, NULL, 0, AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ);
  if (!o) {
    PRINT("Ignoring unknown expert option %s\n", str);
    return 0;
  }
  if (param && o->type == AV_OPT_TYPE_CONST) {
    return 0;
  }
  if (param && o->type != AV_OPT_TYPE_CONST && o->unit) {
    p = av_opt_find(&c, param, o->unit, 0, AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ);
    if (p) {
      prop = BKE_ffmpeg_property_add(rd, (char *)type, p, o);
    }
    else {
      PRINT("Ignoring unknown expert option %s\n", str);
    }
  }
  else {
    prop = BKE_ffmpeg_property_add(rd, (char *)type, o, NULL);
  }

  if (!prop) {
    return 0;
  }

  if (param && !p) {
    switch (prop->type) {
      case IDP_INT:
        IDP_Int(prop) = atoi(param);
        break;
      case IDP_FLOAT:
        IDP_Float(prop) = atof(param);
        break;
      case IDP_STRING:
        strncpy(IDP_String(prop), param, prop->len);
        break;
    }
  }
  return 1;
}

static void ffmpeg_set_expert_options(RenderData *rd)
{
  int codec_id = rd->ffcodecdata.codec;

  if (rd->ffcodecdata.properties) {
    IDP_FreePropertyContent(rd->ffcodecdata.properties);
  }

  if (codec_id == AV_CODEC_ID_H264) {
    /*
     * All options here are for x264, but must be set via ffmpeg.
     * The names are therefore different - Search for "x264 to FFmpeg option mapping"
     * to get a list.
     */

    /*
     * Use CABAC coder. Using "coder:1", which should be equivalent,
     * crashes Blender for some reason. Either way - this is no big deal.
     */
    BKE_ffmpeg_property_add_string(rd, "video", "coder:vlc");

    /*
     * The other options were taken from the libx264-default.preset
     * included in the ffmpeg distribution.
     */

    /* This breaks compatibility for QT. */
    // BKE_ffmpeg_property_add_string(rd, "video", "flags:loop");
    BKE_ffmpeg_property_add_string(rd, "video", "cmp:chroma");
    BKE_ffmpeg_property_add_string(rd, "video", "partitions:parti4x4");  // Deprecated.
    BKE_ffmpeg_property_add_string(rd, "video", "partitions:partp8x8");  // Deprecated.
    BKE_ffmpeg_property_add_string(rd, "video", "partitions:partb8x8");  // Deprecated.
    BKE_ffmpeg_property_add_string(rd, "video", "me:hex");
    BKE_ffmpeg_property_add_string(rd, "video", "subq:6");
    BKE_ffmpeg_property_add_string(rd, "video", "me_range:16");
    BKE_ffmpeg_property_add_string(rd, "video", "qdiff:4");
    BKE_ffmpeg_property_add_string(rd, "video", "keyint_min:25");
    BKE_ffmpeg_property_add_string(rd, "video", "sc_threshold:40");
    BKE_ffmpeg_property_add_string(rd, "video", "i_qfactor:0.71");
    BKE_ffmpeg_property_add_string(rd, "video", "b_strategy:1");
    BKE_ffmpeg_property_add_string(rd, "video", "bf:3");
    BKE_ffmpeg_property_add_string(rd, "video", "refs:2");
    BKE_ffmpeg_property_add_string(rd, "video", "qcomp:0.6");

    BKE_ffmpeg_property_add_string(rd, "video", "trellis:0");
    BKE_ffmpeg_property_add_string(rd, "video", "weightb:1");
#  ifdef FFMPEG_HAVE_DEPRECATED_FLAGS2
    BKE_ffmpeg_property_add_string(rd, "video", "flags2:dct8x8");
    BKE_ffmpeg_property_add_string(rd, "video", "directpred:3");
    BKE_ffmpeg_property_add_string(rd, "video", "flags2:fastpskip");
    BKE_ffmpeg_property_add_string(rd, "video", "flags2:wpred");
#  else
    BKE_ffmpeg_property_add_string(rd, "video", "8x8dct:1");
    BKE_ffmpeg_property_add_string(rd, "video", "fast-pskip:1");
    BKE_ffmpeg_property_add_string(rd, "video", "wpredp:2");
#  endif
  }
  else if (codec_id == AV_CODEC_ID_DNXHD) {
    if (rd->ffcodecdata.flags & FFMPEG_LOSSLESS_OUTPUT) {
      BKE_ffmpeg_property_add_string(rd, "video", "mbd:rd");
    }
  }
}

void BKE_ffmpeg_preset_set(RenderData *rd, int preset)
{
  int isntsc = (rd->frs_sec != 25);

  if (rd->ffcodecdata.properties) {
    IDP_FreePropertyContent(rd->ffcodecdata.properties);
  }

  switch (preset) {
    case FFMPEG_PRESET_VCD:
      rd->ffcodecdata.type = FFMPEG_MPEG1;
      rd->ffcodecdata.video_bitrate = 1150;
      rd->xsch = 352;
      rd->ysch = isntsc ? 240 : 288;
      rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
      rd->ffcodecdata.rc_max_rate = 1150;
      rd->ffcodecdata.rc_min_rate = 1150;
      rd->ffcodecdata.rc_buffer_size = 40 * 8;
      rd->ffcodecdata.mux_packet_size = 2324;
      rd->ffcodecdata.mux_rate = 2352 * 75 * 8;
      break;

    case FFMPEG_PRESET_SVCD:
      rd->ffcodecdata.type = FFMPEG_MPEG2;
      rd->ffcodecdata.video_bitrate = 2040;
      rd->xsch = 480;
      rd->ysch = isntsc ? 480 : 576;
      rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
      rd->ffcodecdata.rc_max_rate = 2516;
      rd->ffcodecdata.rc_min_rate = 0;
      rd->ffcodecdata.rc_buffer_size = 224 * 8;
      rd->ffcodecdata.mux_packet_size = 2324;
      rd->ffcodecdata.mux_rate = 0;
      break;

    case FFMPEG_PRESET_DVD:
      rd->ffcodecdata.type = FFMPEG_MPEG2;
      rd->ffcodecdata.video_bitrate = 6000;

      /* Don't set resolution, see [#21351]
       * rd->xsch = 720;
       * rd->ysch = isntsc ? 480 : 576; */

      rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
      rd->ffcodecdata.rc_max_rate = 9000;
      rd->ffcodecdata.rc_min_rate = 0;
      rd->ffcodecdata.rc_buffer_size = 224 * 8;
      rd->ffcodecdata.mux_packet_size = 2048;
      rd->ffcodecdata.mux_rate = 10080000;
      break;

    case FFMPEG_PRESET_DV:
      rd->ffcodecdata.type = FFMPEG_DV;
      rd->xsch = 720;
      rd->ysch = isntsc ? 480 : 576;
      break;

    case FFMPEG_PRESET_H264:
      rd->ffcodecdata.type = FFMPEG_AVI;
      rd->ffcodecdata.codec = AV_CODEC_ID_H264;
      rd->ffcodecdata.video_bitrate = 6000;
      rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
      rd->ffcodecdata.rc_max_rate = 9000;
      rd->ffcodecdata.rc_min_rate = 0;
      rd->ffcodecdata.rc_buffer_size = 224 * 8;
      rd->ffcodecdata.mux_packet_size = 2048;
      rd->ffcodecdata.mux_rate = 10080000;

      break;

    case FFMPEG_PRESET_THEORA:
    case FFMPEG_PRESET_XVID:
      if (preset == FFMPEG_PRESET_XVID) {
        rd->ffcodecdata.type = FFMPEG_AVI;
        rd->ffcodecdata.codec = AV_CODEC_ID_MPEG4;
      }
      else if (preset == FFMPEG_PRESET_THEORA) {
        rd->ffcodecdata.type = FFMPEG_OGG;  // XXX broken
        rd->ffcodecdata.codec = AV_CODEC_ID_THEORA;
      }

      rd->ffcodecdata.video_bitrate = 6000;
      rd->ffcodecdata.gop_size = isntsc ? 18 : 15;
      rd->ffcodecdata.rc_max_rate = 9000;
      rd->ffcodecdata.rc_min_rate = 0;
      rd->ffcodecdata.rc_buffer_size = 224 * 8;
      rd->ffcodecdata.mux_packet_size = 2048;
      rd->ffcodecdata.mux_rate = 10080000;
      break;
  }

  ffmpeg_set_expert_options(rd);
}

void BKE_ffmpeg_image_type_verify(RenderData *rd, ImageFormatData *imf)
{
  int audio = 0;

  if (imf->imtype == R_IMF_IMTYPE_FFMPEG) {
    if (rd->ffcodecdata.type <= 0 || rd->ffcodecdata.codec <= 0 ||
        rd->ffcodecdata.audio_codec <= 0 || rd->ffcodecdata.video_bitrate <= 1) {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_H264);
      rd->ffcodecdata.constant_rate_factor = FFM_CRF_MEDIUM;
      rd->ffcodecdata.ffmpeg_preset = FFM_PRESET_GOOD;
      rd->ffcodecdata.type = FFMPEG_MKV;
    }
    if (rd->ffcodecdata.type == FFMPEG_OGG) {
      rd->ffcodecdata.type = FFMPEG_MPEG2;
    }

    audio = 1;
  }
  else if (imf->imtype == R_IMF_IMTYPE_H264) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_H264) {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_H264);
      audio = 1;
    }
  }
  else if (imf->imtype == R_IMF_IMTYPE_XVID) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_MPEG4) {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_XVID);
      audio = 1;
    }
  }
  else if (imf->imtype == R_IMF_IMTYPE_THEORA) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_THEORA) {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_THEORA);
      audio = 1;
    }
  }

  if (audio && rd->ffcodecdata.audio_codec < 0) {
    rd->ffcodecdata.audio_codec = AV_CODEC_ID_NONE;
    rd->ffcodecdata.audio_bitrate = 128;
  }
}

void BKE_ffmpeg_codec_settings_verify(RenderData *rd)
{
  ffmpeg_set_expert_options(rd);
}

bool BKE_ffmpeg_alpha_channel_is_supported(RenderData *rd)
{
  int codec = rd->ffcodecdata.codec;
  return ELEM(codec,
              AV_CODEC_ID_QTRLE,
              AV_CODEC_ID_PNG,
              AV_CODEC_ID_VP9,
#  ifdef FFMPEG_FFV1_ALPHA_SUPPORTED
              AV_CODEC_ID_FFV1,
#  endif
              AV_CODEC_ID_HUFFYUV);
}

void *BKE_ffmpeg_context_create(void)
{
  FFMpegContext *context;

  /* new ffmpeg data struct */
  context = MEM_callocN(sizeof(FFMpegContext), "new ffmpeg context");

  context->ffmpeg_codec = AV_CODEC_ID_MPEG4;
  context->ffmpeg_audio_codec = AV_CODEC_ID_NONE;
  context->ffmpeg_video_bitrate = 1150;
  context->ffmpeg_audio_bitrate = 128;
  context->ffmpeg_gop_size = 12;
  context->ffmpeg_autosplit = 0;
  context->ffmpeg_autosplit_count = 0;
  context->ffmpeg_preview = false;
  context->stamp_data = NULL;

  return context;
}

void BKE_ffmpeg_context_free(void *context_v)
{
  FFMpegContext *context = context_v;
  if (context == NULL) {
    return;
  }
  if (context->stamp_data) {
    MEM_freeN(context->stamp_data);
  }
  MEM_freeN(context);
}

#endif /* WITH_FFMPEG */
