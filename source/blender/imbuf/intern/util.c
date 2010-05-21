/**
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
 * util.c
 *
 * $Id$
 */

#ifdef _WIN32
#include <io.h>
#define open _open
#define read _read
#define close _close
#endif

#include "BLI_blenlib.h"

#include "DNA_userdef_types.h"
#include "BKE_global.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

#include "IMB_anim.h"

#ifdef WITH_QUICKTIME
#include "quicktime_import.h"
#endif

#ifdef WITH_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>

#if LIBAVFORMAT_VERSION_INT < (49 << 16)
#define FFMPEG_OLD_FRAME_RATE 1
#else
#define FFMPEG_CODEC_IS_POINTER 1
#endif

#endif

#define UTIL_DEBUG 0

static int IMB_ispic_name(char *name)
{
	ImFileType *type;
	struct stat st;
	int fp, buf[10];

	if(UTIL_DEBUG) printf("IMB_ispic_name: loading %s\n", name);
	
	if(stat(name,&st) == -1)
		return FALSE;
	if(((st.st_mode) & S_IFMT) != S_IFREG)
		return FALSE;

	if((fp = open(name,O_BINARY|O_RDONLY)) < 0)
		return FALSE;

	if(read(fp, buf, 32) != 32) {
		close(fp);
		return FALSE;
	}

	close(fp);

	/* XXX move this exception */
	if((BIG_LONG(buf[0]) & 0xfffffff0) == 0xffd8ffe0)
		return JPG;

	for(type=IMB_FILE_TYPES; type->is_a; type++)
		if(type->is_a((uchar*)buf))
			return type->filetype;

	return FALSE;
}

int IMB_ispic(char *filename)
{
	if(U.uiflag & USER_FILTERFILEEXTS) {
		if (BLI_testextensie(filename, ".tif")
				||	BLI_testextensie(filename, ".tiff")
				||	BLI_testextensie(filename, ".tx")) {
				return IMB_ispic_name(filename);
		}
		if (G.have_quicktime){
			if(		BLI_testextensie(filename, ".jpg")
				||	BLI_testextensie(filename, ".jpeg")
#ifdef WITH_TIFF
				||	BLI_testextensie(filename, ".tif")
				||	BLI_testextensie(filename, ".tiff")
				||	BLI_testextensie(filename, ".tx")
#endif
				||	BLI_testextensie(filename, ".hdr")
				||	BLI_testextensie(filename, ".tga")
				||	BLI_testextensie(filename, ".rgb")
				||	BLI_testextensie(filename, ".bmp")
				||	BLI_testextensie(filename, ".png")
#ifdef WITH_DDS
				||	BLI_testextensie(filename, ".dds")
#endif
				||	BLI_testextensie(filename, ".iff")
				||	BLI_testextensie(filename, ".lbm")
				||	BLI_testextensie(filename, ".gif")
				||	BLI_testextensie(filename, ".psd")
				||	BLI_testextensie(filename, ".pct")
				||	BLI_testextensie(filename, ".pict")
				||	BLI_testextensie(filename, ".pntg") //macpaint
				||	BLI_testextensie(filename, ".qtif")
				||	BLI_testextensie(filename, ".cin")
#ifdef WITH_BF_OPENEXR
				||	BLI_testextensie(filename, ".exr")
#endif
#ifdef WITH_BF_OPENJPEG
				||	BLI_testextensie(filename, ".jp2")
#endif
				||	BLI_testextensie(filename, ".sgi")) {
				return IMB_ispic_name(filename);
			} else {
				return(FALSE);			
			}
		} else { /* no quicktime */
			if(		BLI_testextensie(filename, ".jpg")
				||	BLI_testextensie(filename, ".jpeg")
#ifdef WITH_TIFF
				||	BLI_testextensie(filename, ".tif")
				||	BLI_testextensie(filename, ".tiff")
				||	BLI_testextensie(filename, ".tx")
#endif
				||	BLI_testextensie(filename, ".hdr")
				||	BLI_testextensie(filename, ".tga")
				||	BLI_testextensie(filename, ".rgb")
				||	BLI_testextensie(filename, ".bmp")
				||	BLI_testextensie(filename, ".png")
				||	BLI_testextensie(filename, ".cin")
#ifdef WITH_DDS
				||	BLI_testextensie(filename, ".dds")
#endif
#ifdef WITH_BF_OPENEXR
				||	BLI_testextensie(filename, ".exr")
#endif
#ifdef WITH_BF_OPENJPEG
				||	BLI_testextensie(filename, ".jp2")
#endif
				||	BLI_testextensie(filename, ".iff")
				||	BLI_testextensie(filename, ".lbm")
				||	BLI_testextensie(filename, ".sgi")) {
				return IMB_ispic_name(filename);
			}
			else  {
				return(FALSE);
			}
		}
	} else { /* no FILTERFILEEXTS */
		return IMB_ispic_name(filename);
	}
}



static int isavi (char *name) {
	return AVI_is_avi (name);
}

#ifdef WITH_QUICKTIME
static int isqtime (char *name) {
	return anim_is_quicktime (name);
}
#endif

#ifdef WITH_FFMPEG

void silence_log_ffmpeg(int quiet)
{
	if (quiet)
	{
		av_log_set_level(AV_LOG_QUIET);
	}
	else
	{
		av_log_set_level(AV_LOG_INFO);
	}
}

extern void do_init_ffmpeg();
void do_init_ffmpeg()
{
	static int ffmpeg_init = 0;
	if (!ffmpeg_init) {
		ffmpeg_init = 1;
		av_register_all();
		avdevice_register_all();
		
		if ((G.f & G_DEBUG) == 0)
		{
			silence_log_ffmpeg(1);
		}
	}
}

#ifdef FFMPEG_CODEC_IS_POINTER
static AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return stream->codec;
}
#else
static AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return &stream->codec;
}
#endif


static int isffmpeg (char *filename) {
	AVFormatContext *pFormatCtx;
	unsigned int i;
	int videoStream;
	AVCodec *pCodec;
	AVCodecContext *pCodecCtx;

	do_init_ffmpeg();

	if( BLI_testextensie(filename, ".swf") ||
		BLI_testextensie(filename, ".jpg") ||
		BLI_testextensie(filename, ".png") ||
		BLI_testextensie(filename, ".dds") ||
		BLI_testextensie(filename, ".tga") ||
		BLI_testextensie(filename, ".bmp") ||
		BLI_testextensie(filename, ".exr") ||
		BLI_testextensie(filename, ".cin") ||
		BLI_testextensie(filename, ".wav")) return 0;

	if(av_open_input_file(&pFormatCtx, filename, NULL, 0, NULL)!=0) {
		if(UTIL_DEBUG) fprintf(stderr, "isffmpeg: av_open_input_file failed\n");
		return 0;
	}

	if(av_find_stream_info(pFormatCtx)<0) {
		if(UTIL_DEBUG) fprintf(stderr, "isffmpeg: av_find_stream_info failed\n");
		av_close_input_file(pFormatCtx);
		return 0;
	}

	if(UTIL_DEBUG) dump_format(pFormatCtx, 0, filename, 0);


		/* Find the first video stream */
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i] &&
		   get_codec_from_stream(pFormatCtx->streams[i]) && 
		  (get_codec_from_stream(pFormatCtx->streams[i])->codec_type==CODEC_TYPE_VIDEO))
		{
			videoStream=i;
			break;
		}

	if(videoStream==-1) {
		av_close_input_file(pFormatCtx);
		return 0;
	}

	pCodecCtx = get_codec_from_stream(pFormatCtx->streams[videoStream]);

		/* Find the decoder for the video stream */
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		av_close_input_file(pFormatCtx);
		return 0;
	}

	if(avcodec_open(pCodecCtx, pCodec)<0) {
		av_close_input_file(pFormatCtx);
		return 0;
	}

	avcodec_close(pCodecCtx);
	av_close_input_file(pFormatCtx);

	return 1;
}
#endif

#ifdef WITH_REDCODE
static int isredcode(char * filename)
{
	struct redcode_handle * h = redcode_open(filename);
	if (!h) {
		return 0;
	}
	redcode_close(h);
	return 1;
}

#endif

int imb_get_anim_type(char * name) {
	int type;
	struct stat st;

	if(UTIL_DEBUG) printf("in getanimtype: %s\n", name);

#ifndef _WIN32
#	ifdef WITH_QUICKTIME
	if (isqtime(name)) return (ANIM_QTIME);
#	endif
#	ifdef WITH_FFMPEG
	/* stat test below fails on large files > 4GB */
	if (isffmpeg(name)) return (ANIM_FFMPEG);
#	endif
	if (stat(name,&st) == -1) return(0);
	if (((st.st_mode) & S_IFMT) != S_IFREG) return(0);

	if (isavi(name)) return (ANIM_AVI);

	if (ismovie(name)) return (ANIM_MOVIE);
#else
	if (stat(name,&st) == -1) return(0);
	if (((st.st_mode) & S_IFMT) != S_IFREG) return(0);

	if (ismovie(name)) return (ANIM_MOVIE);
#	ifdef WITH_QUICKTIME
	if (isqtime(name)) return (ANIM_QTIME);
#	endif
#	ifdef WITH_FFMPEG
	if (isffmpeg(name)) return (ANIM_FFMPEG);
#	endif
	if (isavi(name)) return (ANIM_AVI);
#endif
#ifdef WITH_REDCODE
	if (isredcode(name)) return (ANIM_REDCODE);
#endif
	type = IMB_ispic(name);
	if (type) return(ANIM_SEQUENCE);
	return(0);
}
 
int IMB_isanim(char *filename) {
	int type;
	
	if(U.uiflag & USER_FILTERFILEEXTS) {
		if (G.have_quicktime){
			if(		BLI_testextensie(filename, ".avi")
				||	BLI_testextensie(filename, ".flc")
				||	BLI_testextensie(filename, ".dv")
				||	BLI_testextensie(filename, ".r3d")
				||	BLI_testextensie(filename, ".mov")
				||	BLI_testextensie(filename, ".movie")
				||	BLI_testextensie(filename, ".mv")) {
				type = imb_get_anim_type(filename);
			} else {
				return(FALSE);			
			}
		} else { // no quicktime
			if(		BLI_testextensie(filename, ".avi")
				||	BLI_testextensie(filename, ".dv")
				||	BLI_testextensie(filename, ".r3d")
				||	BLI_testextensie(filename, ".mv")) {
				type = imb_get_anim_type(filename);
			}
			else  {
				return(FALSE);
			}
		}
	} else { // no FILTERFILEEXTS
		type = imb_get_anim_type(filename);
	}
	
	return (type && type!=ANIM_SEQUENCE);
}
