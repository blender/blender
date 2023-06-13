/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#ifdef _WIN32
#  define INC_OLE2
#  include <commdlg.h>
#  include <memory.h>
#  include <mmsystem.h>
#  include <vfw.h>
#  include <windows.h>
#  include <windowsx.h>

#  undef AVIIF_KEYFRAME /* redefined in AVI_avi.h */
#  undef AVIIF_LIST     /* redefined in AVI_avi.h */
#endif                  /* _WIN32 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <io.h>
#else
#  include <dirent.h>
#endif

#include "imbuf.h"

#ifdef WITH_AVI
#  include "AVI_avi.h"
#endif

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_allocimbuf.h"

#ifdef WITH_FFMPEG
extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libswscale/swscale.h>
}
#endif

/* more endianness... should move to a separate file... */
#ifdef __BIG_ENDIAN__
#  define LITTLE_LONG SWAP_LONG
#else
#  define LITTLE_LONG ENDIAN_NOP
#endif

/* anim.curtype, runtime only */
#define ANIM_NONE 0
#define ANIM_SEQUENCE (1 << 0)
#define ANIM_MOVIE (1 << 4)
#define ANIM_AVI (1 << 6)
#define ANIM_FFMPEG (1 << 8)

#define MAXNUMSTREAMS 50

struct IDProperty;
struct _AviMovie;
struct anim_index;

struct anim {
  int ib_flags;
  int curtype;
  int cur_position; /* index  0 = 1e,  1 = 2e, enz. */
  int duration_in_frames;
  int frs_sec;
  double frs_sec_base;
  double start_offset;
  int x, y;

  /* for number */
  char filepath[1024];
  /* for sequence */
  char filepath_first[1024];

  /* movie */
  void *movie;
  void *track;
  void *params;
  int orientation;
  size_t framesize;
  int interlacing;
  int streamindex;

  /* avi */
  struct _AviMovie *avi;

#if defined(_WIN32)
  /* windows avi */
  int avistreams;
  int firstvideo;
  int pfileopen;
  PAVIFILE pfile;
  PAVISTREAM pavi[MAXNUMSTREAMS]; /* the current streams */
  PGETFRAME pgf;
#endif

#ifdef WITH_FFMPEG
  AVFormatContext *pFormatCtx;
  AVCodecContext *pCodecCtx;
  const AVCodec *pCodec;
  AVFrame *pFrameRGB;
  AVFrame *pFrameDeinterlaced;
  struct SwsContext *img_convert_ctx;
  int videoStream;

  AVFrame *pFrame;
  bool pFrame_complete;
  AVFrame *pFrame_backup;
  bool pFrame_backup_complete;

  struct ImBuf *cur_frame_final;
  int64_t cur_pts;
  int64_t cur_key_frame_pts;
  AVPacket *cur_packet;

  bool seek_before_decode;
#endif

  char index_dir[768];

  int proxies_tried;
  int indices_tried;

  struct anim *proxy_anim[IMB_PROXY_MAX_SLOT];
  struct anim_index *curr_idx[IMB_TC_MAX_SLOT];

  char colorspace[64];
  char suffix[64]; /* MAX_NAME - multiview */

  struct IDProperty *metadata;
};
