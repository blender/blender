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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup imbuf
 */

#ifdef _WIN32
#  define INC_OLE2
#  include <windows.h>
#  include <windowsx.h>
#  include <mmsystem.h>
#  include <memory.h>
#  include <commdlg.h>
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

#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#ifndef _WIN32
#  include <dirent.h>
#else
#  include <io.h>
#endif

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_path_util.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_AVI
#  include "AVI_avi.h"
#endif

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

#include "IMB_anim.h"
#include "IMB_indexer.h"
#include "IMB_metadata.h"

#ifdef WITH_FFMPEG
#  include "BKE_global.h" /* ENDIAN_ORDER */

#  include <libavformat/avformat.h>
#  include <libavcodec/avcodec.h>
#  include <libavutil/rational.h>
#  include <libswscale/swscale.h>

#  include "ffmpeg_compat.h"
#endif  // WITH_FFMPEG

int ismovie(const char *UNUSED(filepath))
{
  return 0;
}

/* never called, just keep the linker happy */
static int startmovie(struct anim *UNUSED(anim))
{
  return 1;
}
static ImBuf *movie_fetchibuf(struct anim *UNUSED(anim), int UNUSED(position))
{
  return NULL;
}
static void free_anim_movie(struct anim *UNUSED(anim))
{
  /* pass */
}

#if defined(_WIN32)
#  define PATHSEPARATOR '\\'
#else
#  define PATHSEPARATOR '/'
#endif

static int an_stringdec(const char *string, char *head, char *tail, unsigned short *numlen)
{
  unsigned short len, nume, nums = 0;
  short i;
  bool found = false;

  len = strlen(string);
  nume = len;

  for (i = len - 1; i >= 0; i--) {
    if (string[i] == PATHSEPARATOR) {
      break;
    }
    if (isdigit(string[i])) {
      if (found) {
        nums = i;
      }
      else {
        nume = i;
        nums = i;
        found = true;
      }
    }
    else {
      if (found) {
        break;
      }
    }
  }
  if (found) {
    strcpy(tail, &string[nume + 1]);
    strcpy(head, string);
    head[nums] = '\0';
    *numlen = nume - nums + 1;
    return ((int)atoi(&(string[nums])));
  }
  tail[0] = '\0';
  strcpy(head, string);
  *numlen = 0;
  return true;
}

static void an_stringenc(
    char *string, const char *head, const char *tail, unsigned short numlen, int pic)
{
  BLI_stringenc(string, head, tail, numlen, pic);
}

#ifdef WITH_AVI
static void free_anim_avi(struct anim *anim)
{
#  if defined(_WIN32)
  int i;
#  endif

  if (anim == NULL) {
    return;
  }
  if (anim->avi == NULL) {
    return;
  }

  AVI_close(anim->avi);
  MEM_freeN(anim->avi);
  anim->avi = NULL;

#  if defined(_WIN32)

  if (anim->pgf) {
    AVIStreamGetFrameClose(anim->pgf);
    anim->pgf = NULL;
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

  anim->duration = 0;
}
#endif /* WITH_AVI */

#ifdef WITH_FFMPEG
static void free_anim_ffmpeg(struct anim *anim);
#endif

void IMB_free_anim(struct anim *anim)
{
  if (anim == NULL) {
    printf("free anim, anim == NULL\n");
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

void IMB_close_anim(struct anim *anim)
{
  if (anim == NULL) {
    return;
  }

  IMB_free_anim(anim);
}

void IMB_close_anim_proxies(struct anim *anim)
{
  if (anim == NULL) {
    return;
  }

  IMB_free_indices(anim);
}

struct IDProperty *IMB_anim_load_metadata(struct anim *anim)
{
  switch (anim->curtype) {
    case ANIM_FFMPEG: {
#ifdef WITH_FFMPEG
      AVDictionaryEntry *entry = NULL;

      BLI_assert(anim->pFormatCtx != NULL);
      av_log(anim->pFormatCtx, AV_LOG_DEBUG, "METADATA FETCH\n");

      while (true) {
        entry = av_dict_get(anim->pFormatCtx->metadata, "", entry, AV_DICT_IGNORE_SUFFIX);
        if (entry == NULL) {
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

struct anim *IMB_open_anim(const char *name,
                           int ib_flags,
                           int streamindex,
                           char colorspace[IM_MAX_SPACE])
{
  struct anim *anim;

  BLI_assert(!BLI_path_is_rel(name));

  anim = (struct anim *)MEM_callocN(sizeof(struct anim), "anim struct");
  if (anim != NULL) {
    if (colorspace) {
      colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);
      BLI_strncpy(anim->colorspace, colorspace, sizeof(anim->colorspace));
    }
    else {
      colorspace_set_default_role(
          anim->colorspace, sizeof(anim->colorspace), COLOR_ROLE_DEFAULT_BYTE);
    }

    BLI_strncpy(anim->name, name, sizeof(anim->name));
    anim->ib_flags = ib_flags;
    anim->streamindex = streamindex;
  }
  return (anim);
}

void IMB_suffix_anim(struct anim *anim, const char *suffix)
{
  BLI_strncpy(anim->suffix, suffix, sizeof(anim->suffix));
}

#ifdef WITH_AVI
static int startavi(struct anim *anim)
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

  anim->avi = MEM_callocN(sizeof(AviMovie), "animavi");

  if (anim->avi == NULL) {
    printf("Can't open avi: %s\n", anim->name);
    return -1;
  }

  avierror = AVI_open_movie(anim->name, anim->avi);

#  if defined(_WIN32)
  if (avierror == AVI_ERROR_COMPRESSION) {
    AVIFileInit();
    hr = AVIFileOpen(&anim->pfile, anim->name, OF_READ, 0L);
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
          anim->pgf = AVIStreamGetFrameOpen(anim->pavi[i], NULL);
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
    printf("Error loading avi: %s\n", anim->name);
    free_anim_avi(anim);
    return -1;
  }

  anim->duration = anim->avi->header->TotalFrames;
  anim->params = NULL;

  anim->x = anim->avi->header->Width;
  anim->y = anim->avi->header->Height;
  anim->interlacing = 0;
  anim->orientation = 0;
  anim->framesize = anim->x * anim->y * 4;

  anim->curposition = 0;
  anim->preseek = 0;

#  if 0
  printf("x:%d y:%d size:%d interl:%d dur:%d\n",
         anim->x,
         anim->y,
         anim->framesize,
         anim->interlacing,
         anim->duration);
#  endif

  return 0;
}
#endif /* WITH_AVI */

#ifdef WITH_AVI
static ImBuf *avi_fetchibuf(struct anim *anim, int position)
{
  ImBuf *ibuf = NULL;
  int *tmp;
  int y;

  if (anim == NULL) {
    return NULL;
  }

#  if defined(_WIN32)
  if (anim->avistreams) {
    LPBITMAPINFOHEADER lpbi;

    if (anim->pgf) {
      lpbi = AVIStreamGetFrame(anim->pgf, position + AVIStreamStart(anim->pavi[anim->firstvideo]));
      if (lpbi) {
        ibuf = IMB_ibImageFromMemory(
            (const unsigned char *)lpbi, 100, IB_rect, anim->colorspace, "<avi_fetchibuf>");
        // Oh brother...
      }
    }
  }
  else
#  endif
  {
    ibuf = IMB_allocImBuf(anim->x, anim->y, 24, IB_rect);

    tmp = AVI_read_frame(
        anim->avi, AVI_FORMAT_RGB32, position, AVI_get_stream(anim->avi, AVIST_VIDEO, 0));

    if (tmp == NULL) {
      printf("Error reading frame from AVI: '%s'\n", anim->name);
      IMB_freeImBuf(ibuf);
      return NULL;
    }

    for (y = 0; y < anim->y; y++) {
      memcpy(&(ibuf->rect)[((anim->y - y) - 1) * anim->x], &tmp[y * anim->x], anim->x * 4);
    }

    MEM_freeN(tmp);
  }

  ibuf->rect_colorspace = colormanage_colorspace_get_named(anim->colorspace);

  return ibuf;
}
#endif /* WITH_AVI */

#ifdef WITH_FFMPEG

BLI_INLINE bool need_aligned_ffmpeg_buffer(struct anim *anim)
{
  return (anim->x & 31) != 0;
}

static int startffmpeg(struct anim *anim)
{
  int i, videoStream;

  AVCodec *pCodec;
  AVFormatContext *pFormatCtx = NULL;
  AVCodecContext *pCodecCtx;
  AVRational frame_rate;
  int frs_num;
  double frs_den;
  int streamcount;

#  ifdef FFMPEG_SWSCALE_COLOR_SPACE_SUPPORT
  /* The following for color space determination */
  int srcRange, dstRange, brightness, contrast, saturation;
  int *table;
  const int *inv_table;
#  endif

  if (anim == NULL) {
    return (-1);
  }

  streamcount = anim->streamindex;

  if (avformat_open_input(&pFormatCtx, anim->name, NULL, NULL) != 0) {
    return -1;
  }

  if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  av_dump_format(pFormatCtx, 0, anim->name, 0);

  /* Find the video stream */
  videoStream = -1;

  for (i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (streamcount > 0) {
        streamcount--;
        continue;
      }
      videoStream = i;
      break;
    }
  }

  if (videoStream == -1) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  pCodecCtx = pFormatCtx->streams[videoStream]->codec;

  /* Find the decoder for the video stream */
  pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
  if (pCodec == NULL) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  pCodecCtx->workaround_bugs = 1;

  if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
    avformat_close_input(&pFormatCtx);
    return -1;
  }
  if (pCodecCtx->pix_fmt == AV_PIX_FMT_NONE) {
    avcodec_close(anim->pCodecCtx);
    avformat_close_input(&pFormatCtx);
    return -1;
  }

  frame_rate = av_get_r_frame_rate_compat(pFormatCtx, pFormatCtx->streams[videoStream]);
  if (pFormatCtx->streams[videoStream]->nb_frames != 0) {
    anim->duration = pFormatCtx->streams[videoStream]->nb_frames;
  }
  else {
    anim->duration = (int)(pFormatCtx->duration * av_q2d(frame_rate) / AV_TIME_BASE + 0.5f);
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

  anim->params = 0;

  anim->x = pCodecCtx->width;
  anim->y = av_get_cropped_height_from_codec(pCodecCtx);

  anim->pFormatCtx = pFormatCtx;
  anim->pCodecCtx = pCodecCtx;
  anim->pCodec = pCodec;
  anim->videoStream = videoStream;

  anim->interlacing = 0;
  anim->orientation = 0;
  anim->framesize = anim->x * anim->y * 4;

  anim->curposition = -1;
  anim->last_frame = 0;
  anim->last_pts = -1;
  anim->next_pts = -1;
  anim->next_packet.stream_index = -1;

  anim->pFrame = av_frame_alloc();
  anim->pFrameComplete = false;
  anim->pFrameDeinterlaced = av_frame_alloc();
  anim->pFrameRGB = av_frame_alloc();

  if (need_aligned_ffmpeg_buffer(anim)) {
    anim->pFrameRGB->format = AV_PIX_FMT_RGBA;
    anim->pFrameRGB->width = anim->x;
    anim->pFrameRGB->height = anim->y;

    if (av_frame_get_buffer(anim->pFrameRGB, 32) < 0) {
      fprintf(stderr, "Could not allocate frame data.\n");
      avcodec_close(anim->pCodecCtx);
      avformat_close_input(&anim->pFormatCtx);
      av_frame_free(&anim->pFrameRGB);
      av_frame_free(&anim->pFrameDeinterlaced);
      av_frame_free(&anim->pFrame);
      anim->pCodecCtx = NULL;
      return -1;
    }
  }

  if (avpicture_get_size(AV_PIX_FMT_RGBA, anim->x, anim->y) != anim->x * anim->y * 4) {
    fprintf(stderr, "ffmpeg has changed alloc scheme ... ARGHHH!\n");
    avcodec_close(anim->pCodecCtx);
    avformat_close_input(&anim->pFormatCtx);
    av_frame_free(&anim->pFrameRGB);
    av_frame_free(&anim->pFrameDeinterlaced);
    av_frame_free(&anim->pFrame);
    anim->pCodecCtx = NULL;
    return -1;
  }

  if (anim->ib_flags & IB_animdeinterlace) {
    avpicture_fill((AVPicture *)anim->pFrameDeinterlaced,
                   MEM_callocN(avpicture_get_size(anim->pCodecCtx->pix_fmt,
                                                  anim->pCodecCtx->width,
                                                  anim->pCodecCtx->height),
                               "ffmpeg deinterlace"),
                   anim->pCodecCtx->pix_fmt,
                   anim->pCodecCtx->width,
                   anim->pCodecCtx->height);
  }

  if (pCodecCtx->has_b_frames) {
    anim->preseek = 25; /* FIXME: detect gopsize ... */
  }
  else {
    anim->preseek = 0;
  }

  anim->img_convert_ctx = sws_getContext(anim->x,
                                         anim->y,
                                         anim->pCodecCtx->pix_fmt,
                                         anim->x,
                                         anim->y,
                                         AV_PIX_FMT_RGBA,
                                         SWS_FAST_BILINEAR | SWS_PRINT_INFO | SWS_FULL_CHR_H_INT,
                                         NULL,
                                         NULL,
                                         NULL);

  if (!anim->img_convert_ctx) {
    fprintf(stderr, "Can't transform color space??? Bailing out...\n");
    avcodec_close(anim->pCodecCtx);
    avformat_close_input(&anim->pFormatCtx);
    av_frame_free(&anim->pFrameRGB);
    av_frame_free(&anim->pFrameDeinterlaced);
    av_frame_free(&anim->pFrame);
    anim->pCodecCtx = NULL;
    return -1;
  }

#  ifdef FFMPEG_SWSCALE_COLOR_SPACE_SUPPORT
  /* Try do detect if input has 0-255 YCbCR range (JFIF Jpeg MotionJpeg) */
  if (!sws_getColorspaceDetails(anim->img_convert_ctx,
                                (int **)&inv_table,
                                &srcRange,
                                &table,
                                &dstRange,
                                &brightness,
                                &contrast,
                                &saturation)) {
    srcRange = srcRange || anim->pCodecCtx->color_range == AVCOL_RANGE_JPEG;
    inv_table = sws_getCoefficients(anim->pCodecCtx->colorspace);

    if (sws_setColorspaceDetails(anim->img_convert_ctx,
                                 (int *)inv_table,
                                 srcRange,
                                 table,
                                 dstRange,
                                 brightness,
                                 contrast,
                                 saturation)) {
      fprintf(stderr, "Warning: Could not set libswscale colorspace details.\n");
    }
  }
  else {
    fprintf(stderr, "Warning: Could not set libswscale colorspace details.\n");
  }
#  endif

  return (0);
}

/* postprocess the image in anim->pFrame and do color conversion
 * and deinterlacing stuff.
 *
 * Output is anim->last_frame
 */

static void ffmpeg_postprocess(struct anim *anim)
{
  AVFrame *input = anim->pFrame;
  ImBuf *ibuf = anim->last_frame;
  int filter_y = 0;

  if (!anim->pFrameComplete) {
    return;
  }

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
         "  POSTPROC: anim->pFrame planes: %p %p %p %p\n",
         input->data[0],
         input->data[1],
         input->data[2],
         input->data[3]);

  if (anim->ib_flags & IB_animdeinterlace) {
    if (avpicture_deinterlace((AVPicture *)anim->pFrameDeinterlaced,
                              (const AVPicture *)anim->pFrame,
                              anim->pCodecCtx->pix_fmt,
                              anim->pCodecCtx->width,
                              anim->pCodecCtx->height) < 0) {
      filter_y = true;
    }
    else {
      input = anim->pFrameDeinterlaced;
    }
  }

  if (!need_aligned_ffmpeg_buffer(anim)) {
    avpicture_fill((AVPicture *)anim->pFrameRGB,
                   (unsigned char *)ibuf->rect,
                   AV_PIX_FMT_RGBA,
                   anim->x,
                   anim->y);
  }

  if (ENDIAN_ORDER == B_ENDIAN) {
    int *dstStride = anim->pFrameRGB->linesize;
    uint8_t **dst = anim->pFrameRGB->data;
    int dstStride2[4] = {dstStride[0], 0, 0, 0};
    uint8_t *dst2[4] = {dst[0], 0, 0, 0};
    int x, y, h, w;
    unsigned char *bottom;
    unsigned char *top;

    sws_scale(anim->img_convert_ctx,
              (const uint8_t *const *)input->data,
              input->linesize,
              0,
              anim->y,
              dst2,
              dstStride2);

    bottom = (unsigned char *)ibuf->rect;
    top = bottom + ibuf->x * (ibuf->y - 1) * 4;

    h = (ibuf->y + 1) / 2;
    w = ibuf->x;

    for (y = 0; y < h; y++) {
      unsigned char tmp[4];
      unsigned int *tmp_l = (unsigned int *)tmp;

      for (x = 0; x < w; x++) {
        tmp[0] = bottom[0];
        tmp[1] = bottom[1];
        tmp[2] = bottom[2];
        tmp[3] = bottom[3];

        bottom[0] = top[0];
        bottom[1] = top[1];
        bottom[2] = top[2];
        bottom[3] = top[3];

        *(unsigned int *)top = *tmp_l;

        bottom += 4;
        top += 4;
      }
      top -= 8 * w;
    }
  }
  else {
    int *dstStride = anim->pFrameRGB->linesize;
    uint8_t **dst = anim->pFrameRGB->data;
    int dstStride2[4] = {-dstStride[0], 0, 0, 0};
    uint8_t *dst2[4] = {dst[0] + (anim->y - 1) * dstStride[0], 0, 0, 0};

    sws_scale(anim->img_convert_ctx,
              (const uint8_t *const *)input->data,
              input->linesize,
              0,
              anim->y,
              dst2,
              dstStride2);
  }

  if (need_aligned_ffmpeg_buffer(anim)) {
    uint8_t *src = anim->pFrameRGB->data[0];
    uint8_t *dst = (uint8_t *)ibuf->rect;
    for (int y = 0; y < anim->y; y++) {
      memcpy(dst, src, anim->x * 4);
      dst += anim->x * 4;
      src += anim->pFrameRGB->linesize[0];
    }
  }

  if (filter_y) {
    IMB_filtery(ibuf);
  }
}

/* decode one video frame also considering the packet read into next_packet */

static int ffmpeg_decode_video_frame(struct anim *anim)
{
  int rval = 0;

  av_log(anim->pFormatCtx, AV_LOG_DEBUG, "  DECODE VIDEO FRAME\n");

  if (anim->next_packet.stream_index == anim->videoStream) {
    av_free_packet(&anim->next_packet);
    anim->next_packet.stream_index = -1;
  }

  while ((rval = av_read_frame(anim->pFormatCtx, &anim->next_packet)) >= 0) {
    av_log(anim->pFormatCtx,
           AV_LOG_DEBUG,
           "%sREAD: strID=%d (VID: %d) dts=%lld pts=%lld "
           "%s\n",
           (anim->next_packet.stream_index == anim->videoStream) ? "->" : "  ",
           anim->next_packet.stream_index,
           anim->videoStream,
           (anim->next_packet.dts == AV_NOPTS_VALUE) ? -1 : (long long int)anim->next_packet.dts,
           (anim->next_packet.pts == AV_NOPTS_VALUE) ? -1 : (long long int)anim->next_packet.pts,
           (anim->next_packet.flags & AV_PKT_FLAG_KEY) ? " KEY" : "");
    if (anim->next_packet.stream_index == anim->videoStream) {
      anim->pFrameComplete = 0;

      avcodec_decode_video2(
          anim->pCodecCtx, anim->pFrame, &anim->pFrameComplete, &anim->next_packet);

      if (anim->pFrameComplete) {
        anim->next_pts = av_get_pts_from_frame(anim->pFormatCtx, anim->pFrame);

        av_log(anim->pFormatCtx,
               AV_LOG_DEBUG,
               "  FRAME DONE: next_pts=%lld "
               "pkt_pts=%lld, guessed_pts=%lld\n",
               (anim->pFrame->pts == AV_NOPTS_VALUE) ? -1 : (long long int)anim->pFrame->pts,
               (anim->pFrame->pkt_pts == AV_NOPTS_VALUE) ? -1 :
                                                           (long long int)anim->pFrame->pkt_pts,
               (long long int)anim->next_pts);
        break;
      }
    }
    av_free_packet(&anim->next_packet);
    anim->next_packet.stream_index = -1;
  }

  if (rval == AVERROR_EOF) {
    /* this sets size and data fields to zero,
     * which is necessary to decode the remaining data
     * in the decoder engine after EOF. It also prevents a memory
     * leak, since av_read_frame spills out a full size packet even
     * on EOF... (and: it's safe to call on NULL packets) */

    av_free_packet(&anim->next_packet);

    anim->next_packet.size = 0;
    anim->next_packet.data = 0;

    anim->pFrameComplete = 0;

    avcodec_decode_video2(
        anim->pCodecCtx, anim->pFrame, &anim->pFrameComplete, &anim->next_packet);

    if (anim->pFrameComplete) {
      anim->next_pts = av_get_pts_from_frame(anim->pFormatCtx, anim->pFrame);

      av_log(anim->pFormatCtx,
             AV_LOG_DEBUG,
             "  FRAME DONE (after EOF): next_pts=%lld "
             "pkt_pts=%lld, guessed_pts=%lld\n",
             (anim->pFrame->pts == AV_NOPTS_VALUE) ? -1 : (long long int)anim->pFrame->pts,
             (anim->pFrame->pkt_pts == AV_NOPTS_VALUE) ? -1 : (long long int)anim->pFrame->pkt_pts,
             (long long int)anim->next_pts);
      rval = 0;
    }
  }

  if (rval < 0) {
    anim->next_packet.stream_index = -1;

    av_log(anim->pFormatCtx,
           AV_LOG_ERROR,
           "  DECODE READ FAILED: av_read_frame() "
           "returned error: %d\n",
           rval);
  }

  return (rval >= 0);
}

static void ffmpeg_decode_video_frame_scan(struct anim *anim, int64_t pts_to_search)
{
  /* there seem to exist *very* silly GOP lengths out in the wild... */
  int count = 1000;

  av_log(anim->pFormatCtx,
         AV_LOG_DEBUG,
         "SCAN start: considering pts=%lld in search of %lld\n",
         (long long int)anim->next_pts,
         (long long int)pts_to_search);

  while (count > 0 && anim->next_pts < pts_to_search) {
    av_log(anim->pFormatCtx,
           AV_LOG_DEBUG,
           "  WHILE: pts=%lld in search of %lld\n",
           (long long int)anim->next_pts,
           (long long int)pts_to_search);
    if (!ffmpeg_decode_video_frame(anim)) {
      break;
    }
    count--;
  }
  if (count == 0) {
    av_log(anim->pFormatCtx,
           AV_LOG_ERROR,
           "SCAN failed: completely lost in stream, "
           "bailing out at PTS=%lld, searching for PTS=%lld\n",
           (long long int)anim->next_pts,
           (long long int)pts_to_search);
  }
  if (anim->next_pts == pts_to_search) {
    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "SCAN HAPPY: we found our PTS!\n");
  }
  else {
    av_log(anim->pFormatCtx, AV_LOG_ERROR, "SCAN UNHAPPY: PTS not matched!\n");
  }
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

static ImBuf *ffmpeg_fetchibuf(struct anim *anim, int position, IMB_Timecode_Type tc)
{
  int64_t pts_to_search = 0;
  double frame_rate;
  double pts_time_base;
  long long st_time;
  struct anim_index *tc_index = 0;
  AVStream *v_st;
  int new_frame_index = 0; /* To quiet gcc barking... */
  int old_frame_index = 0; /* To quiet gcc barking... */

  if (anim == NULL) {
    return (0);
  }

  av_log(anim->pFormatCtx, AV_LOG_DEBUG, "FETCH: pos=%d\n", position);

  if (tc != IMB_TC_NONE) {
    tc_index = IMB_anim_open_index(anim, tc);
  }

  v_st = anim->pFormatCtx->streams[anim->videoStream];

  frame_rate = av_q2d(av_get_r_frame_rate_compat(anim->pFormatCtx, v_st));

  st_time = anim->pFormatCtx->start_time;
  pts_time_base = av_q2d(v_st->time_base);

  if (tc_index) {
    new_frame_index = IMB_indexer_get_frame_index(tc_index, position);
    old_frame_index = IMB_indexer_get_frame_index(tc_index, anim->curposition);
    pts_to_search = IMB_indexer_get_pts(tc_index, new_frame_index);
  }
  else {
    pts_to_search = (long long)floor(((double)position) / pts_time_base / frame_rate + 0.5);

    if (st_time != AV_NOPTS_VALUE) {
      pts_to_search += st_time / pts_time_base / AV_TIME_BASE;
    }
  }

  av_log(anim->pFormatCtx,
         AV_LOG_DEBUG,
         "FETCH: looking for PTS=%lld "
         "(pts_timebase=%g, frame_rate=%g, st_time=%lld)\n",
         (long long int)pts_to_search,
         pts_time_base,
         frame_rate,
         st_time);

  if (anim->last_frame && anim->last_pts <= pts_to_search && anim->next_pts > pts_to_search) {
    av_log(anim->pFormatCtx,
           AV_LOG_DEBUG,
           "FETCH: frame repeat: last: %lld next: %lld\n",
           (long long int)anim->last_pts,
           (long long int)anim->next_pts);
    IMB_refImBuf(anim->last_frame);
    anim->curposition = position;
    return anim->last_frame;
  }

  if (position > anim->curposition + 1 && anim->preseek && !tc_index &&
      position - (anim->curposition + 1) < anim->preseek) {
    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "FETCH: within preseek interval (no index)\n");

    ffmpeg_decode_video_frame_scan(anim, pts_to_search);
  }
  else if (tc_index && IMB_indexer_can_scan(tc_index, old_frame_index, new_frame_index)) {
    av_log(anim->pFormatCtx,
           AV_LOG_DEBUG,
           "FETCH: within preseek interval "
           "(index tells us)\n");

    ffmpeg_decode_video_frame_scan(anim, pts_to_search);
  }
  else if (position != anim->curposition + 1) {
    long long pos;
    int ret;

    if (tc_index) {
      unsigned long long dts;

      pos = IMB_indexer_get_seek_pos(tc_index, new_frame_index);
      dts = IMB_indexer_get_seek_pos_dts(tc_index, new_frame_index);

      av_log(anim->pFormatCtx, AV_LOG_DEBUG, "TC INDEX seek pos = %lld\n", pos);
      av_log(anim->pFormatCtx, AV_LOG_DEBUG, "TC INDEX seek dts = %llu\n", dts);

      if (ffmpeg_seek_by_byte(anim->pFormatCtx)) {
        av_log(anim->pFormatCtx, AV_LOG_DEBUG, "... using BYTE pos\n");

        ret = av_seek_frame(anim->pFormatCtx, -1, pos, AVSEEK_FLAG_BYTE);
        av_update_cur_dts(anim->pFormatCtx, v_st, dts);
      }
      else {
        av_log(anim->pFormatCtx, AV_LOG_DEBUG, "... using DTS pos\n");
        ret = av_seek_frame(anim->pFormatCtx, anim->videoStream, dts, AVSEEK_FLAG_BACKWARD);
      }
    }
    else {
      pos = (long long)(position - anim->preseek) * AV_TIME_BASE / frame_rate;

      av_log(anim->pFormatCtx,
             AV_LOG_DEBUG,
             "NO INDEX seek pos = %lld, st_time = %lld\n",
             pos,
             (st_time != AV_NOPTS_VALUE) ? st_time : 0);

      if (pos < 0) {
        pos = 0;
      }

      if (st_time != AV_NOPTS_VALUE) {
        pos += st_time;
      }

      av_log(anim->pFormatCtx, AV_LOG_DEBUG, "NO INDEX final seek pos = %lld\n", pos);

      ret = av_seek_frame(anim->pFormatCtx, -1, pos, AVSEEK_FLAG_BACKWARD);
    }

    if (ret < 0) {
      av_log(anim->pFormatCtx,
             AV_LOG_ERROR,
             "FETCH: "
             "error while seeking to DTS = %lld "
             "(frameno = %d, PTS = %lld): errcode = %d\n",
             pos,
             position,
             (long long int)pts_to_search,
             ret);
    }

    avcodec_flush_buffers(anim->pCodecCtx);

    anim->next_pts = -1;

    if (anim->next_packet.stream_index == anim->videoStream) {
      av_free_packet(&anim->next_packet);
      anim->next_packet.stream_index = -1;
    }

    /* memset(anim->pFrame, ...) ?? */

    if (ret >= 0) {
      ffmpeg_decode_video_frame_scan(anim, pts_to_search);
    }
  }
  else if (position == 0 && anim->curposition == -1) {
    /* first frame without seeking special case... */
    ffmpeg_decode_video_frame(anim);
  }
  else {
    av_log(anim->pFormatCtx, AV_LOG_DEBUG, "FETCH: no seek necessary, just continue...\n");
  }

  IMB_freeImBuf(anim->last_frame);
  anim->last_frame = IMB_allocImBuf(anim->x, anim->y, 32, IB_rect);
  anim->last_frame->rect_colorspace = colormanage_colorspace_get_named(anim->colorspace);

  ffmpeg_postprocess(anim);

  anim->last_pts = anim->next_pts;

  ffmpeg_decode_video_frame(anim);

  anim->curposition = position;

  IMB_refImBuf(anim->last_frame);

  return anim->last_frame;
}

static void free_anim_ffmpeg(struct anim *anim)
{
  if (anim == NULL) {
    return;
  }

  if (anim->pCodecCtx) {
    avcodec_close(anim->pCodecCtx);
    avformat_close_input(&anim->pFormatCtx);

    /* Special case here: pFrame could share pointers with codec,
     * so in order to avoid double-free we don't use av_frame_free()
     * to free the frame.
     *
     * Could it be a bug in FFmpeg?
     */
    av_free(anim->pFrame);

    if (!need_aligned_ffmpeg_buffer(anim)) {
      /* If there's no need for own aligned buffer it means that FFmpeg's
       * frame shares the same buffer as temporary ImBuf. In this case we
       * should not free the buffer when freeing the FFmpeg buffer.
       */
      avpicture_fill((AVPicture *)anim->pFrameRGB, NULL, AV_PIX_FMT_RGBA, anim->x, anim->y);
    }
    av_frame_free(&anim->pFrameRGB);
    av_frame_free(&anim->pFrameDeinterlaced);

    sws_freeContext(anim->img_convert_ctx);
    IMB_freeImBuf(anim->last_frame);
    if (anim->next_packet.stream_index != -1) {
      av_free_packet(&anim->next_packet);
    }
  }
  anim->duration = 0;
}

#endif

/* Try next picture to read */
/* No picture, try to open next animation */
/* Succeed, remove first image from animation */

static ImBuf *anim_getnew(struct anim *anim)
{
  struct ImBuf *ibuf = NULL;

  if (anim == NULL) {
    return (NULL);
  }

  free_anim_movie(anim);

#ifdef WITH_AVI
  free_anim_avi(anim);
#endif

#ifdef WITH_FFMPEG
  free_anim_ffmpeg(anim);
#endif

  if (anim->curtype != 0) {
    return (NULL);
  }
  anim->curtype = imb_get_anim_type(anim->name);

  switch (anim->curtype) {
    case ANIM_SEQUENCE:
      ibuf = IMB_loadiffname(anim->name, anim->ib_flags, anim->colorspace);
      if (ibuf) {
        BLI_strncpy(anim->first, anim->name, sizeof(anim->first));
        anim->duration = 1;
      }
      break;
    case ANIM_MOVIE:
      if (startmovie(anim)) {
        return (NULL);
      }
      ibuf = IMB_allocImBuf(anim->x, anim->y, 24, 0); /* fake */
      break;
#ifdef WITH_AVI
    case ANIM_AVI:
      if (startavi(anim)) {
        printf("couldn't start avi\n");
        return (NULL);
      }
      ibuf = IMB_allocImBuf(anim->x, anim->y, 24, 0);
      break;
#endif
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      if (startffmpeg(anim)) {
        return (0);
      }
      ibuf = IMB_allocImBuf(anim->x, anim->y, 24, 0);
      break;
#endif
  }
  return (ibuf);
}

struct ImBuf *IMB_anim_previewframe(struct anim *anim)
{
  struct ImBuf *ibuf = NULL;
  int position = 0;

  ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
  if (ibuf) {
    IMB_freeImBuf(ibuf);
    position = anim->duration / 2;
    ibuf = IMB_anim_absolute(anim, position, IMB_TC_NONE, IMB_PROXY_NONE);
  }
  return ibuf;
}

struct ImBuf *IMB_anim_absolute(struct anim *anim,
                                int position,
                                IMB_Timecode_Type tc,
                                IMB_Proxy_Size preview_size)
{
  struct ImBuf *ibuf = NULL;
  char head[256], tail[256];
  unsigned short digits;
  int pic;
  int filter_y;
  if (anim == NULL) {
    return (NULL);
  }

  filter_y = (anim->ib_flags & IB_animdeinterlace);

  if (preview_size == IMB_PROXY_NONE) {
    if (anim->curtype == 0) {
      ibuf = anim_getnew(anim);
      if (ibuf == NULL) {
        return (NULL);
      }

      IMB_freeImBuf(ibuf); /* ???? */
      ibuf = NULL;
    }

    if (position < 0) {
      return (NULL);
    }
    if (position >= anim->duration) {
      return (NULL);
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
    case ANIM_SEQUENCE:
      pic = an_stringdec(anim->first, head, tail, &digits);
      pic += position;
      an_stringenc(anim->name, head, tail, digits, pic);
      ibuf = IMB_loadiffname(anim->name, IB_rect, anim->colorspace);
      if (ibuf) {
        anim->curposition = position;
      }
      break;
    case ANIM_MOVIE:
      ibuf = movie_fetchibuf(anim, position);
      if (ibuf) {
        anim->curposition = position;
        IMB_convert_rgba_to_abgr(ibuf);
      }
      break;
#ifdef WITH_AVI
    case ANIM_AVI:
      ibuf = avi_fetchibuf(anim, position);
      if (ibuf) {
        anim->curposition = position;
      }
      break;
#endif
#ifdef WITH_FFMPEG
    case ANIM_FFMPEG:
      ibuf = ffmpeg_fetchibuf(anim, position, tc);
      if (ibuf) {
        anim->curposition = position;
      }
      filter_y = 0; /* done internally */
      break;
#endif
  }

  if (ibuf) {
    if (filter_y) {
      IMB_filtery(ibuf);
    }
    BLI_snprintf(ibuf->name, sizeof(ibuf->name), "%s.%04d", anim->name, anim->curposition + 1);
  }
  return (ibuf);
}

/***/

int IMB_anim_get_duration(struct anim *anim, IMB_Timecode_Type tc)
{
  struct anim_index *idx;
  if (tc == IMB_TC_NONE) {
    return anim->duration;
  }

  idx = IMB_anim_open_index(anim, tc);
  if (!idx) {
    return anim->duration;
  }

  return IMB_indexer_get_duration(idx);
}

bool IMB_anim_get_fps(struct anim *anim, short *frs_sec, float *frs_sec_base, bool no_av_base)
{
  double frs_sec_base_double;
  if (anim->frs_sec) {
    if (anim->frs_sec > SHRT_MAX) {
      /* We cannot store original rational in our short/float format,
       * we need to approximate it as best as we can... */
      *frs_sec = SHRT_MAX;
      frs_sec_base_double = anim->frs_sec_base * (double)SHRT_MAX / (double)anim->frs_sec;
    }
    else {
      *frs_sec = anim->frs_sec;
      frs_sec_base_double = anim->frs_sec_base;
    }
#ifdef WITH_FFMPEG
    if (no_av_base) {
      *frs_sec_base = (float)(frs_sec_base_double / AV_TIME_BASE);
    }
    else {
      *frs_sec_base = (float)frs_sec_base_double;
    }
#else
    UNUSED_VARS(no_av_base);
    *frs_sec_base = (float)frs_sec_base_double;
#endif
    BLI_assert(*frs_sec > 0);
    BLI_assert(*frs_sec_base > 0.0f);

    return true;
  }
  return false;
}

void IMB_anim_set_preseek(struct anim *anim, int preseek)
{
  anim->preseek = preseek;
}

int IMB_anim_get_preseek(struct anim *anim)
{
  return anim->preseek;
}
