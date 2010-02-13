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
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_targa.h"
#include "IMB_png.h"

#ifdef WITH_DDS
#include "dds/dds_api.h"
#endif

#include "IMB_bmp.h"
#include "IMB_tiff.h"
#include "IMB_radiance_hdr.h"
#include "IMB_dpxcineon.h"

#include "IMB_anim.h"

#ifdef WITH_OPENEXR
#include "openexr/openexr_api.h"
#endif

#ifdef WITH_QUICKTIME
#include "quicktime_import.h"
#endif

#ifdef WITH_OPENJPEG
#include "IMB_jp2.h"
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

/* from misc_util: flip the bytes from x  */
#define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1])

/* this one is only def-ed once, strangely... */
#define GSS(x) (((uchar *)(x))[1] << 8 | ((uchar *)(x))[0])

static int IMB_ispic_name(char *name)
{
	struct stat st;
	int fp, buf[10];
	int ofs = 0;

	if(UTIL_DEBUG) printf("IMB_ispic_name: loading %s\n", name);
	
	if (ib_stat(name,&st) == -1) return(0);
	if (((st.st_mode) & S_IFMT) == S_IFREG){
		if ((fp = open(name,O_BINARY|O_RDONLY)) >= 0){
			if (read(fp,buf,32)==32){
				close(fp);
				if (buf[ofs] == CAT) ofs += 3;
				if (buf[ofs] == FORM){
					if (buf[ofs + 2] == ILBM) return(AMI);
					if (buf[ofs + 2] == ANIM){
						if (buf[ofs + 3] == FORM){
							return(ANIM);
						}else{
							return(Anim);
						}
					}
				} else {
					if (GS(buf) == IMAGIC) return(IMAGIC);
					if (GSS(buf) == IMAGIC) return(IMAGIC);
					if ((BIG_LONG(buf[0]) & 0xfffffff0) == 0xffd8ffe0) return(JPG);

					/* at windows there are ".ffl" files with the same magic numnber... 
					   besides that,  tim images are not really important anymore! */
					/* if ((BIG_LONG(buf[0]) == 0x10000000) && ((BIG_LONG(buf[1]) & 0xf0ffffff) == 0)) return(TIM); */

				}
				if (imb_is_a_png(buf)) return(PNG);
#ifdef WITH_DDS
				if (imb_is_a_dds((uchar *)buf)) return(DDS);
#endif
				if (imb_is_a_targa(buf)) return(TGA);
#ifdef WITH_OPENEXR
				if (imb_is_a_openexr((uchar *)buf)) return(OPENEXR);
#endif
				if (imb_is_a_tiff(buf)) return(TIF);
				if (imb_is_dpx(buf)) return (DPX);
				if (imb_is_cineon(buf)) return(CINEON);
				/* radhdr: check if hdr format */
				if (imb_is_a_hdr(buf)) return(RADHDR);

/*
				if (imb_is_a_bmp(buf)) return(BMP);
*/
				
#ifdef WITH_OPENJPEG
				if (imb_is_a_jp2(buf)) return(JP2);
#endif
				
#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__)
				if(G.have_quicktime) {
					if (imb_is_a_quicktime(name)) return(QUICKTIME);
				}
#endif
#endif

				return(FALSE);
			}
			close(fp);
		}
	}
	return(FALSE);
}



int IMB_ispic(char *filename)
{
	if(U.uiflag & USER_FILTERFILEEXTS) {
		if (G.have_libtiff && (BLI_testextensie(filename, ".tif")
				||	BLI_testextensie(filename, ".tiff"))) {
				return IMB_ispic_name(filename);
		}
		if (G.have_quicktime){
			if(		BLI_testextensie(filename, ".jpg")
				||	BLI_testextensie(filename, ".jpeg")
				||	BLI_testextensie(filename, ".tif")
				||	BLI_testextensie(filename, ".tiff")
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
		} else { /* no quicktime or libtiff */
			if(		BLI_testextensie(filename, ".jpg")
				||	BLI_testextensie(filename, ".jpeg")
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
	if (ib_stat(name,&st) == -1) return(0);
	if (((st.st_mode) & S_IFMT) != S_IFREG) return(0);

	if (isavi(name)) return (ANIM_AVI);

	if (ismovie(name)) return (ANIM_MOVIE);
#else
	if (ib_stat(name,&st) == -1) return(0);
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
	if (type == ANIM) return (ANIM_ANIM5);
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
