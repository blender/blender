/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#ifdef _WIN32
#  include "BLI_winstuff.h"
#  include <vfw.h>

#  undef AVIIF_KEYFRAME /* redefined in AVI_avi.h */
#  undef AVIIF_LIST     /* redefined in AVI_avi.h */

#  define FIXCC(fcc) \
    { \
      if (fcc == 0) { \
        fcc = mmioFOURCC('N', 'o', 'n', 'e'); \
      } \
      if (fcc == BI_RLE8) { \
        fcc = mmioFOURCC('R', 'l', 'e', '8'); \
      } \
    } \
    (void)0

#endif

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifndef _WIN32
#  include <dirent.h>
#else
#  include <io.h>
#endif

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_AVI
#  include "AVI_avi.h"
#endif

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

#include "IMB_anim.h"
#include "IMB_indexer.h"
#include "IMB_metadata.h"

#ifdef WITH_FFMPEG
#  include "BKE_global.h" /* ENDIAN_ORDER */

extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libavutil/imgutils.h>
#  include <libavutil/rational.h>
#  include <libswscale/swscale.h>

#  include "ffmpeg_compat.h"
}

#endif /* WITH_FFMPEG */

int ismovie(const char * /*filepath*/)
{
  return 0;
}

/* never called, just keep the linker happy */
static int startmovie(anim * /*anim*/)
{
  return 1;
}
static ImBuf *movie_fetchibuf(anim * /*anim*/, int /*position*/)
{
  return nullptr;
}
static void free_anim_movie(anim * /*anim*/)
{
  /* pass */
}

#ifdef WITH_AVI
static void free_anim_avi(anim *anim)
{
#  if defined(_WIN32)
  int i;
#  endif

  if (anim == nullptr) {
    return;
  }
  if (anim->avi == nullptr) {
    return;
  }

  AVI_close(anim->avi);
  MEM_freeN(anim->avi);
  anim->avi = nullptr;

#  if defined(_WIN32)

  if (anim->pgf) {
    AVIStreamGetFrameClose(anim->pgf);
    anim->pgf = nullptr;
  }

  for (i = 0; i < anim->avistreams; i++) {
    AVIStreamRelease(anim->pavi[i]);
  }
  anim->avistreams = 0;

  if (anim->pfileopen) {
    AVIFileRelease(anim->pfile);
    anim->pfileopen = 0;
    AVIFileExit();
  }
#  endif

  anim->duration_in_frames = 0;
}
#endif /* WITH_AVI */

#ifdef WITH_FFMPEG
static void free_anim_ffmpeg(anim *anim);
#endif

void IMB_free_anim(anim *anim)
{
  if (anim == nullptr) {
    printf("free anim, anim == nullptr\n");
    return;
  }

  free_anim_movie(anim);

#ifdef WITH_AVI
  free_anim_avi(anim);
#endif

#ifdef WITH_FFMPEG
  free_anim_ffmpeg(anim);
#endif
  IMB_free_indices(anim);
  IMB_metadata_free(anim->metadata);

  MEM_freeN(anim);
}

void IMB_close_anim(anim *anim)
{
  if (anim == nullptr) {
    return;
  }

  IMB_free_anim(anim);
}

void IMB_close_anim_proxies(anim *anim)
{
  if (anim == nullptr) {
    return;
  }

  IMB_free_indices(anim);
}

IDProperty *IMB_anim_load_metadata(anim *anim)
{
  switch (anim->curtype) {
    case ANIM_FFMPEG: {
#ifdef WITH_FFMPEG
      AVDictionaryEntry *entry = nullptr;

      BLI_assert(anim->pFormatCtx != nullptr);
      av_log(anim->pFormatCtx, AV_LOG_DEBUG, "METADATA FETCH\n");

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
      break;
    }
    case ANIM_SEQUENCE:
    case ANIM_AVI:
    case ANIM_MOVIE:
      /* TODO */
      break;
    case ANIM_NONE:
    default:
      break;
  }
  return anim->metadata;
}

anim *IMB_open_anim(const char *filepath,
                    int ib_flags,
                    int streamindex,
                    char colorspace[IM_MAX_SPACE])
{
  anim *anim;

  BLI_assert(!BLI_path_is_rel(filepath));

  anim = (struct anim *)MEM_callocN(sizeof(struct anim), "anim struct");
  if (anim != nullptr) {
    if (colorspace) {
      colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);
      STRNCPY(anim->colorspace, colorspace);
    }
    else {
      colorspace_set_default_role(
          anim->colorspace, sizeof(anim->colorspace), COLOR_ROLE_DEFAULT_BYTE);
    }

    STRNCPY(anim->filepath, filepath);
    anim->ib_flags = ib_flags;
    anim->streamindex = streamindex;
  }
  return anim;
}

bool IMB_anim_can_produce_frames(const anim *anim)
{
#if !(defined(WITH_AVI) || defined(WITH_FFMPEG))
  UNUSED_VARS(anim);
#endif

#ifdef WITH_AVI
  if (anim->avi != nullptr) {
    return true;
  }
#endif
#ifdef WITH_FFMPEG
  if (anim->pCodecCtx != nullptr) {
    return true;
  }
#endif
  return false;
}

void IMB_suffix_anim(anim *anim, const char *suffix)
{
  STRNCPY(anim->suffix, suffix);
}

#ifdef WITH_AVI
static int startavi(anim *anim)
{

  AviError avierror;
#  if defined(_WIN32)
  HRESULT hr;
  int i, firstvideo = -1;
  int streamcount;
  BYTE abFormat[1024];
  LONG l;
  LPBITMAPINFOHEADER lpbi;
  AVISTREAMINFO avis;

  streamcount = anim->streamindex;
#  endif

  anim->avi = MEM_cnew<AviMovie>("animavi");

  if (anim->avi == nullptr) {
    printf("Can't open avi: %s\n", anim->filepath);
    return -1;
  }

  avierror = AVI_open_movie(anim->filepath, anim->avi);

#  if defined(_WIN32)
  if (avierror == AVI_ERROR_COMPRESSION) {
    AVIFileInit();
    hr = AVIFileOpen(&anim->pfile, anim->filepath, OF_READ, 0L);
    if (hr == 0) {
      anim->pfileopen = 1;
      for (i = 0; i < MAXNUMSTREAMS; i++) {
        if (AVIFileGetStream(anim->pfile, &anim->pavi[i], 0L, i) != AVIERR_OK) {
          break;
        }

        AVIStreamInfo(anim->pavi[i], &avis, sizeof(avis));
        if ((avis.fccType == streamtypeVIDEO) && (firstvideo == -1)) {
          if (streamcount > 0) {
            streamcount--;
            continue;
          }
          anim->pgf = AVIStreamGetFrameOpen(anim->pavi[i], nullptr);
          if (anim->pgf) {
            firstvideo = i;

            /* get stream length */
            anim->avi->header->TotalFrames = AVIStreamLength(anim->pavi[i]);

            /* get information about images inside the stream */
            l = sizeof(abFormat);
            AVIStreamReadFormat(anim->pavi[i], 0, &abFormat, &l);
            lpbi = (LPBITMAPINFOHEADER)abFormat;
            anim->avi->header->Height = lpbi->biHeight;
            anim->avi->header->Width = lpbi->biWidth;
          }
          else {
            FIXCC(avis.fccHandler);
            FIXCC(avis.fccType);
            printf("Can't find AVI decoder for type : %4.4hs/%4.4hs\n",
                   (LPSTR)&avis.fccType,
                   (LPSTR)&avis.fccHandler);
          }
        }
      }

      /* register number of opened avistreams */
      anim->avistreams = i;

      /*
       * Couldn't get any video streams out of this file
       */
      if ((anim->avistreams == 0) || (firstvideo == -1)) {
        avierror = AVI_ERROR_FORMAT;
      }
      else {
        avierror = AVI_ERROR_NONE;
        anim->firstvideo = firstvideo;
      }
    }
    else {
      AVIFileExit();
    }
  }
#  endif

  if (avierror != AVI_ERROR_NONE) {
    AVI_print_error(avierror);
    printf("Error loading avi: %s\n", anim->filepath);
    free_anim_avi(anim);
    return -1;
  }

  anim->duration_in_frames = anim->avi->header->TotalFrames;
  anim->start_offset = 0.0f;
  anim->params = nullptr;

  anim->x = anim->avi->header->Width;
  anim->y = anim->avi->header->Height;
  anim->interlacing = 0;
  anim->orientation = 0;
  anim->framesize = anim->x * anim->y * 4;

  anim->cur_position = 0;

#  if 0
  printf("x:%d y:%d size:%d interlace:%d dur:%d\n",
         anim->x,
         anim->y,
         anim->framesize,
         anim->interlacing,
         anim->duration_in_frames);
#  endif

  return 0;
}
#endif /* WITH_AVI */

#ifdef WITH_AVI
static ImBuf *avi_fetchibuf(anim *anim, int position)
{
  ImBuf *ibuf = nullptr;
  int *tmp;
  int y;

  if (anim == nullptr) {
    return nullptr;
  }

#  if defined(_WIN32)
  if (anim->avistreams) {
    LPBITMAPINFOHEADER lpbi;

    if (anim->pgf) {
      lpbi = static_cast<LPBITMAPINFOHEADER>(
          AVIStreamGetFrame(anim->pgf, position + AVIStreamStart(anim->pavi[anim->firstvideo])));
      if (lpbi) {
        ibuf = IMB_ibImageFromMemory(
            (const uchar *)lpbi, 100, IB_rect, anim->colorspace, "<avi_fetchibuf>");
        /* Oh brother... */
      }
    }
  }
  else
#  endif
  {
    ibuf = IMB_allocImBuf(anim->x, anim->y, 24, IB_rect);

    tmp = static_cast<int *>(AVI_read_frame(
        anim->avi, AVI_FORMAT_RGB32, position, AVI_get_stream(anim->avi, AVIST_VIDEO, 0)));

    if (tmp == nullptr) {
      printf("Error reading frame from AVI: '%s'\n", anim->filepath);
      IMB_freeImBuf(ibuf);
      return nullptr;
    }

    for (y = 0; y < anim->y; y++) {
      memcpy(&(ibuf->byte_buffer.data)[((anim->y - y) - 1) * anim->x],
             &tmp[y * anim->x],
             anim->x * 4);
    }

    MEM_freeN(tmp);
  }

  ibuf->byte_buffer.colorspace = colormanage_colorspace_get_named(anim->colorspace);

  return ibuf;
}
#endif /* WITH_AVI */

#ifdef WITH_FFMPEG

static int startffmpeg(anim *anim)
{
  int i, video_stream_index;

  const AVCodec *pCodec;
  AVFormatContext *pFormatCtx = nullptr;
  AVCodecContext *pCodecCtx;
  AVRational frame_rate;
  AVStream *video_stream;
  int frs_num;
  double frs_den;
  int streamcount;

  /* The following for color space determination */
  int srcRange, dstRange, brightness, contrast, saturation;
  int *table;
  const int *inv_table;

  if (anim == nullptr) {
    return (-1);
  }

  streamcount = anim->streamindex;

  if (avformat_open_input(&pFormatCtx, anim->filepath, nullptr, nullptr) != 0) {
    return -1;
  }

  if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  av_dump_format(pFormatCtx, 0, anim->filepath, 0);

  /* Find the video stream */
  video_stream_index = -1;

  for (i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (streamcount > 0) {
        streamcount--;
        continue;
      }
      video_stream_index = i;
      break;
    }
  }

  if (video_stream_index == -1) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  video_stream = pFormatCtx->streams[video_stream_index];

  /* Find the decoder for the video stream */
  pCodec = avcodec_find_decoder(video_stream->codecpar->codec_id);
  if (pCodec == nullptr) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  pCodecCtx = avcodec_alloc_context3(nullptr);
  avcodec_parameters_to_context(pCodecCtx, video_stream->codecpar);
  pCodecCtx->workaround_bugs = FF_BUG_AUTODETECT;

  if (pCodec->capabilities & AV_CODEC_CAP_OTHER_THREADS) {
    pCodecCtx->thread_count = 0;
  }
  else {
    pCodecCtx->thread_count = BLI_system_thread_count();
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

  double video_start = 0;
  double pts_time_base = av_q2d(video_stream->time_base);

  if (video_stream->start_time != AV_NOPTS_VALUE) {
    video_start = video_stream->start_time * pts_time_base;
  }

  frame_rate = av_guess_frame_rate(pFormatCtx, video_stream, nullptr);
  anim->duration_in_frames = 0;

  /* Take from the stream if we can. */
  if (video_stream->nb_frames != 0) {
    anim->duration_in_frames = video_stream->nb_frames;

    /* Sanity check on the detected duration. This is to work around corruption like reported in
     * #68091. */
    if (frame_rate.den != 0 && pFormatCtx->duration > 0) {
      double stream_sec = anim->duration_in_frames * av_q2d(frame_rate);
      double container_sec = pFormatCtx->duration / double(AV_TIME_BASE);
      if (stream_sec > 4.0 * container_sec) {
        /* The stream is significantly longer than the container duration, which is
         * suspicious. */
        anim->duration_in_frames = 0;
      }
    }
  }
  /* Fall back to manually estimating the video stream duration.
   * This is because the video stream duration can be shorter than the pFormatCtx->duration.
   */
  if (anim->duration_in_frames == 0) {
    double stream_dur;

    if (video_stream->duration != AV_NOPTS_VALUE) {
      stream_dur = video_stream->duration * pts_time_base;
    }
    else {
      double audio_start = 0;

      /* Find audio stream to guess the duration of the video.
       * Sometimes the audio AND the video stream have a start offset.
       * The difference between these is the offset we want to use to
       * calculate the video duration.
       */
      for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
          AVStream *audio_stream = pFormatCtx->streams[i];
          if (audio_stream->start_time != AV_NOPTS_VALUE) {
            audio_start = audio_stream->start_time * av_q2d(audio_stream->time_base);
          }
          break;
        }
      }

      if (video_start > audio_start) {
        stream_dur = double(pFormatCtx->duration) / AV_TIME_BASE - (video_start - audio_start);
      }
      else {
        /* The video stream starts before or at the same time as the audio stream!
         * We have to assume that the video stream is as long as the full pFormatCtx->duration.
         */
        stream_dur = double(pFormatCtx->duration) / AV_TIME_BASE;
      }
    }
    anim->duration_in_frames = int(stream_dur * av_q2d(frame_rate) + 0.5f);
  }

  frs_num = frame_rate.num;
  frs_den = frame_rate.den;

  frs_den *= AV_TIME_BASE;

  while (frs_num % 10 == 0 && frs_den >= 2.0 && frs_num > 10) {
    frs_num /= 10;
    frs_den /= 10;
  }

  anim->frs_sec = frs_num;
  anim->frs_sec_base = frs_den;
  /* Save the relative start time for the video. IE the start time in relation to where playback
   * starts. */
  anim->start_offset = video_start;

  anim->params = 0;

  anim->x = pCodecCtx->width;
  anim->y = pCodecCtx->height;

  anim->pFormatCtx = pFormatCtx;
  anim->pCodecCtx = pCodecCtx;
  anim->pCodec = pCodec;
  anim->videoStream = video_stream_index;

  anim->interlacing = 0;
  anim->orientation = 0;
  anim->framesize = anim->x * anim->y * 4;

  anim->cur_position = 0;
  anim->cur_frame_final = 0;
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
  anim->pFrameRGB->format = AV_PIX_FMT_RGBA;
  anim->pFrameRGB->width = anim->x;
  anim->pFrameRGB->height = anim->y;

  if (av_frame_get_buffer(anim->pFrameRGB, 0) < 0) {
    fprintf(stderr, "Could not allocate frame data.\n");
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

  if (av_image_get_buffer_size(AV_PIX_FMT_RGBA, anim->x, anim->y, 1) != anim->x * anim->y * 4) {
    fprintf(stderr, "ffmpeg has changed alloc scheme ... ARGHHH!\n");
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
    av_image_fill_arrays(
        anim->pFrameDeinterlaced->data,
        anim->pFrameDeinterlaced->linesize,
        static_cast<const uint8_t *>(MEM_callocN(
            av_image_get_buffer_size(
                anim->pCodecCtx->pix_fmt, anim->pCodecCtx->width, anim->pCodecCtx->height, 1),
            "ffmpeg deinterlace")),
        anim->pCodecCtx->pix_fmt,
        anim->pCodecCtx->width,
        anim->pCodecCtx->height,
        1);
  }

  anim->img_convert_ctx = sws_getContext(anim->x,
                                         anim->y,
                                         anim->pCodecCtx->pix_fmt,
                                         anim->x,
                                         anim->y,
                                         AV_PIX_FMT_RGBA,
                                         SWS_BILINEAR | SWS_PRINT_INFO | SWS_FULL_CHR_H_INT,
                                         nullptr,
                                         nullptr,
                                         nullptr);

  if (!anim->img_convert_ctx) {
    fprintf(stderr, "Can't transform color space??? Bailing out...\n");
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

  /* Try do detect if input has 0-255 YCbCR range (JFIF, JPEG, Motion-JPEG). */
  if (!sws_getColorspaceDetails(anim->img_convert_ctx,
                                (int **)&inv_table,
                                &srcRange,
                                &table,
                                &dstRange,
                                &brightness,
                                &contrast,
                                &saturation))
  {
    srcRange = srcRange || anim->pCodecCtx->color_range == AVCOL_RANGE_JPEG;
    inv_table = sws_getCoefficients(anim->pCodecCtx->colorspace);

    if (sws_setColorspaceDetails(anim->img_convert_ctx,
                                 (int *)inv_table,
                                 srcRange,
                                 table,
                                 dstRange,
                                 brightness,
                                 contrast,
                                 saturation))
    {
      fprintf(stderr, "Warning: Could not set libswscale colorspace details.\n");
    }
  }
  else {
    fprintf(stderr, "Warning: Could not set libswscale colorspace details.\n");
  }

  return 0;
}

static double ffmpeg_steps_per_frame_get(anim *anim)
{
  AVStream *v_st = anim->pFormatCtx->streams[anim->videoStream];
  AVRational time_base = v_st->time_base;
  AVRational frame_rate = av_guess_frame_rate(anim->pFormatCtx, v_st, nullptr);
  return av_q2d(av_inv_q(av_mul_q(frame_rate, time_base)));
}

/* Store backup frame.
 * With VFR movies, if PTS is not matched perfectly, scanning continues to look for next PTS.
 * It is likely to overshoot and scanning stops. Having previous frame backed up, it is possible
 * to use it when overshoot happens.
 */
static void ffmpeg_double_buffer_backup_frame_store(anim *anim, int64_t pts_to_search)
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
static void ffmpeg_double_buffer_backup_frame_clear(anim *anim)
{
  if (anim->pFrame_backup_complete) {
    av_frame_unref(anim->pFrame_backup);
  }
  anim->pFrame_backup_complete = false;
}

/* Return recently decoded frame. If it does not exist, return frame from backup buffer. */
static AVFrame *ffmpeg_double_buffer_frame_fallback_get(anim *anim)
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

/**
 * Postprocess the image in anim->pFrame and do color conversion and de-interlacing stuff.
 *
 * Output is `anim->cur_frame_final`.
 */
static void ffmpeg_postprocess(anim *anim, AVFrame *input)
{
  ImBuf *ibuf = anim->cur_frame_final;
  int filter_y = 0;

  /* This means the data wasn't read properly,
   * this check stops crashing */
  if (input->data[0] == 0 && input->data[1] == 0 && input->data[2] == 0 && input->data[3] == 0) {
    fprintf(stderr,
            "ffmpeg_fetchibuf: "
            "data not read properly...\n");
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
    if (av_image_deinterlace(anim->pFrameDeinterlaced,
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

  sws_scale(anim->img_convert_ctx,
            (const uint8_t *const *)input->data,
            input->linesize,
            0,
            anim->y,
            anim->pFrameRGB->data,
            anim->pFrameRGB->linesize);

  /* Copy the valid bytes from the aligned buffer vertically flipped into ImBuf */
  int aligned_stride = anim->pFrameRGB->linesize[0];
  const uint8_t *const src[4] = {
      anim->pFrameRGB->data[0] + (anim->y - 1) * aligned_stride, 0, 0, 0};
  /* NOTE: Negative linesize is used to copy and flip image at once with function
   * `av_image_copy_to_buffer`. This could cause issues in future and image may need to be flipped
   * explicitly. */
  const int src_linesize[4] = {-anim->pFrameRGB->linesize[0], 0, 0, 0};
  int dst_size = av_image_get_buffer_size(
      AVPixelFormat(anim->pFrameRGB->format), anim->pFrameRGB->width, anim->pFrameRGB->height, 1);
  av_image_copy_to_buffer((uint8_t *)ibuf->byte_buffer.data,
                          dst_size,
                          src,
                          src_linesize,
                          AV_PIX_FMT_RGBA,
                          anim->x,
                          anim->y,
                          1);
  if (filter_y) {
    IMB_filtery(ibuf);
  }
}

static void final_frame_log(anim *anim,
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
static AVFrame *ffmpeg_frame_by_pts_get(anim *anim, int64_t pts_to_search)
{
  /* NOTE: `frame->pts + frame->pkt_duration` does not always match pts of next frame.
   * See footage from #86361. Here it is OK to use, because PTS must match current or backup frame.
   * If there is no current frame, return nullptr.
   */
  if (!anim->pFrame_complete) {
    return nullptr;
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

static void ffmpeg_decode_store_frame_pts(anim *anim)
{
  anim->cur_pts = av_get_pts_from_frame(anim->pFrame);

  if (anim->pFrame->key_frame) {
    anim->cur_key_frame_pts = anim->cur_pts;
  }

  av_log(anim->pFormatCtx,
         AV_LOG_DEBUG,
         "  FRAME DONE: cur_pts=%" PRId64 ", guessed_pts=%" PRId64 "\n",
         av_get_pts_from_frame(anim->pFrame),
         int64_t(anim->cur_pts));
}

static int ffmpeg_read_video_frame(anim *anim, AVPacket *packet)
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
static int ffmpeg_decode_video_frame(anim *anim)
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

static int match_format(const char *name, AVFormatContext *pFormatCtx)
{
  const char *p;
  int len, namelen;

  const char *names = pFormatCtx->iformat->name;

  if (!name || !names) {
    return 0;
  }

  namelen = strlen(name);
  while ((p = strchr(names, ','))) {
    len = MAX2(p - names, namelen);
    if (!BLI_strncasecmp(name, names, len)) {
      return 1;
    }
    names = p + 1;
  }
  return !BLI_strcasecmp(name, names);
}

static int ffmpeg_seek_by_byte(AVFormatContext *pFormatCtx)
{
  static const char *byte_seek_list[] = {"mpegts", 0};
  const char **p;

  if (pFormatCtx->iformat->flags & AVFMT_TS_DISCONT) {
    return true;
  }

  p = byte_seek_list;

  while (*p) {
    if (match_format(*p++, pFormatCtx)) {
      return true;
    }
  }

  return false;
}

static int64_t ffmpeg_get_seek_pts(anim *anim, int64_t pts_to_search)
{
  /* FFmpeg seeks internally using DTS values instead of PTS. In some files DTS and PTS values are
   * offset and sometimes ffmpeg fails to take this into account when seeking.
   * Therefore we need to seek backwards a certain offset to make sure the frame we want is in
   * front of us. It is not possible to determine the exact needed offset, this value is determined
   * experimentally. Note: Too big offset can impact performance. Current 3 frame offset has no
   * measurable impact.
   */
  int64_t seek_pts = pts_to_search - (ffmpeg_steps_per_frame_get(anim) * 3);

  if (seek_pts < 0) {
    seek_pts = 0;
  }
  return seek_pts;
}

/* This gives us an estimate of which pts our requested frame will have.
 * Note that this might be off a bit in certain video files, but it should still be close enough.
 */
static int64_t ffmpeg_get_pts_to_search(anim *anim, anim_index *tc_index, int position)
{
  int64_t pts_to_search;

  if (tc_index) {
    int new_frame_index = IMB_indexer_get_frame_index(tc_index, position);
    pts_to_search = IMB_indexer_get_pts(tc_index, new_frame_index);
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

static bool ffmpeg_is_first_frame_decode(anim *anim)
{
  return anim->pFrame_complete == false;
}

static void ffmpeg_scan_log(anim *anim, int64_t pts_to_search)
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
static void ffmpeg_decode_video_frame_scan(anim *anim, int64_t pts_to_search)
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
static int ffmpeg_generic_seek_workaround(anim *anim,
                                          int64_t *requested_pts,
                                          int64_t pts_to_search)
{
  int64_t current_pts = *requested_pts;
  int64_t offset = 0;

  int64_t cur_pts, prev_pts = -1;

  /* Step backward frame by frame until we find the key frame we are looking for. */
  while (current_pts != 0) {
    current_pts = *requested_pts - int64_t(round(offset * ffmpeg_steps_per_frame_get(anim)));
    current_pts = MAX2(current_pts, 0);

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
    bool is_key_frame = read_packet->flags & AV_PKT_FLAG_KEY;
    /* We need to check the packet timestamp as the key frame could be for a GOP forward in the
     * video stream. So if it has a larger timestamp than the frame we want, ignore it.
     */
    cur_pts = timestamp_from_pts_or_dts(read_packet->pts, read_packet->dts);
    av_packet_free(&read_packet);

    if (is_key_frame) {
      if (cur_pts <= pts_to_search) {
        /* We found the I-frame we were looking for! */
        break;
      }
    }

    if (cur_pts == prev_pts) {
      /* We got the same key frame packet twice.
       * This probably means that we have hit the beginning of the stream. */
      break;
    }

    prev_pts = cur_pts;
    offset++;
  }

  *requested_pts = current_pts;

  /* Re-seek to timestamp that gave I-frame, so it can be read by decode function. */
  return av_seek_frame(anim->pFormatCtx, anim->videoStream, current_pts, AVSEEK_FLAG_BACKWARD);
}

/* Read packet until timestamp matches `anim->cur_packet`, thus recovering internal `anim` stream
 * position state. */
static void ffmpeg_seek_recover_stream_position(anim *anim)
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
static bool ffmpeg_seek_buffers_need_flushing(anim *anim, int position, int64_t seek_pos)
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
static int ffmpeg_seek_to_key_frame(anim *anim,
                                    int position,
                                    anim_index *tc_index,
                                    int64_t pts_to_search)
{
  int64_t seek_pos;
  int ret;

  if (tc_index) {
    /* We can use timestamps generated from our indexer to seek. */
    int new_frame_index = IMB_indexer_get_frame_index(tc_index, position);
    int old_frame_index = IMB_indexer_get_frame_index(tc_index, anim->cur_position);

    if (IMB_indexer_can_scan(tc_index, old_frame_index, new_frame_index)) {
      /* No need to seek, return early. */
      return 0;
    }
    uint64_t pts;
    uint64_t dts;

    seek_pos = IMB_indexer_get_seek_pos(tc_index, new_frame_index);
    pts = IMB_indexer_get_seek_pos_pts(tc_index, new_frame_index);
    dts = IMB_indexer_get_seek_pos_dts(tc_index, new_frame_index);

    anim->cur_key_frame_pts = timestamp_from_pts_or_dts(pts, dts);

    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "TC INDEX seek seek_pos = %" PRId64 "\n", seek_pos);
    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "TC INDEX seek pts = %" PRIu64 "\n", pts);
    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "TC INDEX seek dts = %" PRIu64 "\n", dts);

    if (ffmpeg_seek_by_byte(anim->pFormatCtx)) {
      av_log(anim->pFormatCtx, AV_LOG_DEBUG, "... using BYTE seek_pos\n");

      ret = av_seek_frame(anim->pFormatCtx, -1, seek_pos, AVSEEK_FLAG_BYTE);
    }
    else {
      av_log(anim->pFormatCtx, AV_LOG_DEBUG, "... using PTS seek_pos\n");
      ret = av_seek_frame(
          anim->pFormatCtx, anim->videoStream, anim->cur_key_frame_pts, AVSEEK_FLAG_BACKWARD);
    }
  }
  else {
    /* We have to manually seek with ffmpeg to get to the key frame we want to start decoding from.
     */
    seek_pos = ffmpeg_get_seek_pts(anim, pts_to_search);
    av_log(
        anim->pFormatCtx, AV_LOG_DEBUG, "NO INDEX final seek seek_pos = %" PRId64 "\n", seek_pos);

    AVFormatContext *format_ctx = anim->pFormatCtx;

    if (format_ctx->iformat->read_seek2 || format_ctx->iformat->read_seek) {
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

static bool ffmpeg_must_seek(anim *anim, int position)
{
  bool must_seek = position != anim->cur_position + 1 || ffmpeg_is_first_frame_decode(anim);
  anim->seek_before_decode = must_seek;
  return must_seek;
}

static ImBuf *ffmpeg_fetchibuf(anim *anim, int position, IMB_Timecode_Type tc)
{
  if (anim == nullptr) {
    return nullptr;
  }

  av_log(anim->pFormatCtx, AV_LOG_DEBUG, "FETCH: seek_pos=%d\n", position);

  anim_index *tc_index = IMB_anim_open_index(anim, tc);
  int64_t pts_to_search = ffmpeg_get_pts_to_search(anim, tc_index, position);
  AVStream *v_st = anim->pFormatCtx->streams[anim->videoStream];
  double frame_rate = av_q2d(v_st->r_frame_rate);
  double pts_time_base = av_q2d(v_st->time_base);
  int64_t start_pts = v_st->start_time;

  av_log(anim->pFormatCtx,
         AV_LOG_DEBUG,
         "FETCH: looking for PTS=%" PRId64 " (pts_timebase=%g, frame_rate=%g, start_pts=%" PRId64
         ")\n",
         int64_t(pts_to_search),
         pts_time_base,
         frame_rate,
         start_pts);

  if (ffmpeg_must_seek(anim, position)) {
    ffmpeg_seek_to_key_frame(anim, position, tc_index, pts_to_search);
  }

  ffmpeg_decode_video_frame_scan(anim, pts_to_search);

  /* Update resolution as it can change per-frame with WebM. See #100741 & #100081. */
  anim->x = anim->pCodecCtx->width;
  anim->y = anim->pCodecCtx->height;

  IMB_freeImBuf(anim->cur_frame_final);

  /* Certain versions of FFmpeg have a bug in libswscale which ends up in crash
   * when destination buffer is not properly aligned. For example, this happens
   * in FFmpeg 4.3.1. It got fixed later on, but for compatibility reasons is
   * still best to avoid crash.
   *
   * This is achieved by using own allocation call rather than relying on
   * IMB_allocImBuf() to do so since the IMB_allocImBuf() is not guaranteed
   * to perform aligned allocation.
   *
   * In theory this could give better performance, since SIMD operations on
   * aligned data are usually faster.
   *
   * Note that even though sometimes vertical flip is required it does not
   * affect on alignment of data passed to sws_scale because if the X dimension
   * is not 32 byte aligned special intermediate buffer is allocated.
   *
   * The issue was reported to FFmpeg under ticket #8747 in the FFmpeg tracker
   * and is fixed in the newer versions than 4.3.1. */

  const AVPixFmtDescriptor *pix_fmt_descriptor = av_pix_fmt_desc_get(anim->pCodecCtx->pix_fmt);

  int planes = R_IMF_PLANES_RGBA;
  if ((pix_fmt_descriptor->flags & AV_PIX_FMT_FLAG_ALPHA) == 0) {
    planes = R_IMF_PLANES_RGB;
  }

  anim->cur_frame_final = IMB_allocImBuf(anim->x, anim->y, planes, 0);

  /* Allocate the storage explicitly to ensure the memory is aligned. */
  uint8_t *buffer_data = static_cast<uint8_t *>(
      MEM_mallocN_aligned(size_t(4) * anim->x * anim->y, 32, "ffmpeg ibuf"));
  IMB_assign_byte_buffer(anim->cur_frame_final, buffer_data, IB_TAKE_OWNERSHIP);

  anim->cur_frame_final->byte_buffer.colorspace = colormanage_colorspace_get_named(
      anim->colorspace);

  AVFrame *final_frame = ffmpeg_frame_by_pts_get(anim, pts_to_search);
  if (final_frame == nullptr) {
    /* No valid frame was decoded for requested PTS, fall back on most recent decoded frame, even
     * if it is incorrect. */
    final_frame = ffmpeg_double_buffer_frame_fallback_get(anim);
  }

  /* Even with the fallback from above it is possible that the current decode frame is nullptr. In
   * this case skip post-processing and return current image buffer. */
  if (final_frame != nullptr) {
    ffmpeg_postprocess(anim, final_frame);
  }

  anim->cur_position = position;

  IMB_refImBuf(anim->cur_frame_final);

  return anim->cur_frame_final;
}

static void free_anim_ffmpeg(anim *anim)
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
    av_frame_free(&anim->pFrameDeinterlaced);

    sws_freeContext(anim->img_convert_ctx);
    IMB_freeImBuf(anim->cur_frame_final);
  }
  anim->duration_in_frames = 0;
}

#endif

/**
 * Try to initialize the #anim struct.
 * Returns true on success.
 */
static bool anim_getnew(anim *anim)
{
  BLI_assert(anim->curtype == ANIM_NONE);
  if (anim == nullptr) {
    /* Nothing to initialize. */
    return false;
  }

  free_anim_movie(anim);

#ifdef WITH_AVI
  free_anim_avi(anim);
#endif

#ifdef WITH_FFMPEG
  free_anim_ffmpeg(anim);
#endif

  anim->curtype = imb_get_anim_type(anim->filepath);

  switch (anim->curtype) {
    case ANIM_SEQUENCE: {
      ImBuf *ibuf = IMB_loadiffname(anim->filepath, anim->ib_flags, anim->colorspace);
      if (ibuf) {
        STRNCPY(anim->filepath_first, anim->filepath);
        anim->duration_in_frames = 1;
        IMB_freeImBuf(ibuf);
      }
      else {
        return false;
      }
      break;
    }
    case ANIM_MOVIE:
      if (startmovie(anim)) {
        return false;
      }
      break;
#ifdef WITH_AVI
    case ANIM_AVI:
      if (startavi(anim)) {
        printf("couldn't start avi\n");
        return false;
      }
      break;
#endif
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      if (startffmpeg(anim)) {
        return false;
      }
      break;
#endif
  }
  return true;
}

ImBuf *IMB_anim_previewframe(anim *anim)
{
  ImBuf *ibuf = nullptr;
  int position = 0;

  ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
  if (ibuf) {
    IMB_freeImBuf(ibuf);
    position = anim->duration_in_frames / 2;
    ibuf = IMB_anim_absolute(anim, position, IMB_TC_NONE, IMB_PROXY_NONE);
  }
  return ibuf;
}

ImBuf *IMB_anim_absolute(anim *anim,
                         int position,
                         IMB_Timecode_Type tc,
                         IMB_Proxy_Size preview_size)
{
  ImBuf *ibuf = nullptr;
  int filter_y;
  if (anim == nullptr) {
    return nullptr;
  }

  filter_y = (anim->ib_flags & IB_animdeinterlace);

  if (preview_size == IMB_PROXY_NONE) {
    if (anim->curtype == ANIM_NONE) {
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
    struct anim *proxy = IMB_anim_open_proxy(anim, preview_size);

    if (proxy) {
      position = IMB_anim_index_get_frame_index(anim, tc, position);

      return IMB_anim_absolute(proxy, position, IMB_TC_NONE, IMB_PROXY_NONE);
    }
  }

  switch (anim->curtype) {
    case ANIM_SEQUENCE: {
      constexpr size_t filepath_size = BOUNDED_ARRAY_TYPE_SIZE<decltype(anim->filepath_first)>();
      char head[filepath_size], tail[filepath_size];
      ushort digits;
      const int pic = BLI_path_sequence_decode(
                          anim->filepath_first, head, sizeof(head), tail, sizeof(tail), &digits) +
                      position;
      BLI_path_sequence_encode(anim->filepath, sizeof(anim->filepath), head, tail, digits, pic);
      ibuf = IMB_loadiffname(anim->filepath, IB_rect, anim->colorspace);
      if (ibuf) {
        anim->cur_position = position;
      }
      break;
    }
    case ANIM_MOVIE:
      ibuf = movie_fetchibuf(anim, position);
      if (ibuf) {
        anim->cur_position = position;
        IMB_convert_rgba_to_abgr(ibuf);
      }
      break;
#ifdef WITH_AVI
    case ANIM_AVI:
      ibuf = avi_fetchibuf(anim, position);
      if (ibuf) {
        anim->cur_position = position;
      }
      break;
#endif
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      ibuf = ffmpeg_fetchibuf(anim, position, tc);
      if (ibuf) {
        anim->cur_position = position;
      }
      filter_y = 0; /* done internally */
      break;
#endif
  }

  if (ibuf) {
    if (filter_y) {
      IMB_filtery(ibuf);
    }
    SNPRINTF(ibuf->filepath, "%s.%04d", anim->filepath, anim->cur_position + 1);
  }
  return ibuf;
}

/***/

int IMB_anim_get_duration(anim *anim, IMB_Timecode_Type tc)
{
  anim_index *idx;
  if (tc == IMB_TC_NONE) {
    return anim->duration_in_frames;
  }

  idx = IMB_anim_open_index(anim, tc);
  if (!idx) {
    return anim->duration_in_frames;
  }

  return IMB_indexer_get_duration(idx);
}

double IMD_anim_get_offset(anim *anim)
{
  return anim->start_offset;
}

bool IMB_anim_get_fps(anim *anim, short *frs_sec, float *frs_sec_base, bool no_av_base)
{
  double frs_sec_base_double;
  if (anim->frs_sec) {
    if (anim->frs_sec > SHRT_MAX) {
      /* We cannot store original rational in our short/float format,
       * we need to approximate it as best as we can... */
      *frs_sec = SHRT_MAX;
      frs_sec_base_double = anim->frs_sec_base * double(SHRT_MAX) / double(anim->frs_sec);
    }
    else {
      *frs_sec = anim->frs_sec;
      frs_sec_base_double = anim->frs_sec_base;
    }
#ifdef WITH_FFMPEG
    if (no_av_base) {
      *frs_sec_base = float(frs_sec_base_double / AV_TIME_BASE);
    }
    else {
      *frs_sec_base = float(frs_sec_base_double);
    }
#else
    UNUSED_VARS(no_av_base);
    *frs_sec_base = float(frs_sec_base_double);
#endif
    BLI_assert(*frs_sec > 0);
    BLI_assert(*frs_sec_base > 0.0f);

    return true;
  }
  return false;
}

int IMB_anim_get_image_width(anim *anim)
{
  return anim->x;
}

int IMB_anim_get_image_height(anim *anim)
{
  return anim->y;
}
