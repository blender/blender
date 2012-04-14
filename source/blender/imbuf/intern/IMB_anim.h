/*
 * allocimbuf.h
 *
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/IMB_anim.h
 *  \ingroup imbuf
 */


#ifndef __IMB_ANIM_H__
#define __IMB_ANIM_H__

#ifdef _WIN32
#  define INC_OLE2
#  include <windows.h>
#  include <windowsx.h>
#  include <mmsystem.h>
#  include <memory.h>
#  include <commdlg.h>

#  ifndef FREE_WINDOWS
#    include <vfw.h>
#  endif

#  undef AVIIF_KEYFRAME // redefined in AVI_avi.h
#  undef AVIIF_LIST // redefined in AVI_avi.h
#endif /* _WIN32 */

#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#  include <io.h>
#else
#  include <dirent.h>
#endif

#include "imbuf.h"

#include "AVI_avi.h"

#ifdef WITH_QUICKTIME
#  if defined(_WIN32) || defined(__APPLE__)
#    include "quicktime_import.h"
#  endif /* _WIN32 || __APPLE__ */
#endif /* WITH_QUICKTIME */

#ifdef WITH_FFMPEG
#  include <libavformat/avformat.h>
#  include <libavcodec/avcodec.h>
#  include <libswscale/swscale.h>
#endif

#ifdef WITH_REDCODE
#  ifdef _WIN32 /* on windows we use the one in extern instead */
#    include "libredcode/format.h"
#  else
#    include "libredcode/format.h"
#  endif
#endif

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"



/* actually hard coded endianness */
#define GET_BIG_LONG(x) (((uchar *) (x))[0] << 24 | ((uchar *) (x))[1] << 16 | ((uchar *) (x))[2] << 8 | ((uchar *) (x))[3])
#define GET_LITTLE_LONG(x) (((uchar *) (x))[3] << 24 | ((uchar *) (x))[2] << 16 | ((uchar *) (x))[1] << 8 | ((uchar *) (x))[0])
#define SWAP_L(x) (((x << 24) & 0xff000000) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) | ((x >> 24) & 0xff))
#define SWAP_S(x) (((x << 8) & 0xff00) | ((x >> 8) & 0xff))

/* more endianness... should move to a separate file... */
#ifdef __BIG_ENDIAN__
#  define GET_ID GET_BIG_LONG
#  define LITTLE_LONG SWAP_LONG
#else
#  define GET_ID GET_LITTLE_LONG
#  define LITTLE_LONG ENDIAN_NOP
#endif

/* anim.curtype, runtime only */
#define ANIM_NONE		0
#define ANIM_SEQUENCE	(1 << 0)
#define ANIM_MOVIE		(1 << 4)
#define ANIM_AVI		(1 << 6)
#define ANIM_QTIME		(1 << 7)
#define ANIM_FFMPEG     (1 << 8)
#define ANIM_REDCODE    (1 << 9)

#define MAXNUMSTREAMS		50

struct _AviMovie;
struct anim_index;

struct anim {
	int ib_flags;
	int curtype;
	int curposition;	/* index  0 = 1e,  1 = 2e, enz. */
	int duration;
	short frs_sec;
	float frs_sec_base;
	int x, y;
	
		/* voor op nummer */
	char name[1024];
		/* voor sequence */
	char first[1024];

		/* movie */
	void *movie;
	void *track;
	void *params;
	int orientation; 
	size_t framesize;
	int interlacing;
	int preseek;
	int streamindex;
	
		/* avi */
	struct _AviMovie *avi;

#if defined(_WIN32) && !defined(FREE_WINDOWS)
		/* windows avi */
	int avistreams;
	int firstvideo;
	int pfileopen;
	PAVIFILE	pfile;
	PAVISTREAM  pavi[MAXNUMSTREAMS];	// the current streams
	PGETFRAME	  pgf;
#endif

#ifdef WITH_QUICKTIME
		/* quicktime */
	struct _QuicktimeMovie *qtime;
#endif /* WITH_QUICKTIME */

#ifdef WITH_FFMPEG
	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	AVFrame *pFrame;
	int pFrameComplete;
	AVFrame *pFrameRGB;
	AVFrame *pFrameDeinterlaced;
	struct SwsContext *img_convert_ctx;
	int videoStream;

	struct ImBuf * last_frame;
	int64_t last_pts;
	int64_t next_pts;
	AVPacket next_packet;
#endif

#ifdef WITH_REDCODE
	struct redcode_handle * redcodeCtx;
#endif

	char index_dir[768];

	int proxies_tried;
	int indices_tried;
	
	struct anim * proxy_anim[IMB_PROXY_MAX_SLOT];
	struct anim_index * curr_idx[IMB_TC_MAX_SLOT];

};

#endif

