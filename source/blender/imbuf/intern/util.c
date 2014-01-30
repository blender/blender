/*
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
 */

/** \file blender/imbuf/intern/util.c
 *  \ingroup imbuf
 */


#ifdef _WIN32
#  include <io.h>
#  define open _open
#  define read _read
#  define close _close
#endif

#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_string.h"

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

#include "ffmpeg_compat.h"

#endif

#define UTIL_DEBUG 0

const char *imb_ext_image[] = {
	".png",
	".tga",
	".bmp",
	".jpg", ".jpeg",
	".sgi", ".rgb", ".rgba",
#ifdef WITH_TIFF
	".tif", ".tiff", ".tx",
#endif
#ifdef WITH_OPENJPEG
	".jp2",
	".j2c",
#endif
#ifdef WITH_HDR
	".hdr",
#endif
#ifdef WITH_DDS
	".dds",
#endif
#ifdef WITH_CINEON
	".dpx",
	".cin",
#endif
#ifdef WITH_OPENEXR
	".exr",
#endif
#ifdef WITH_OPENIMAGEIO
	".psd", ".pdd", ".psb",
#endif
	NULL
};

const char *imb_ext_image_filepath_only[] = {
#ifdef WITH_OPENIMAGEIO
	".psd", ".pdd", ".psb",
#endif
	NULL
};

const char *imb_ext_image_qt[] = {
	".gif",
	".psd",
	".pct", ".pict",
	".pntg",
	".qtif",
	NULL
};

const char *imb_ext_movie_qt[] = {
	".avi",   
	".flc",   
	".dv",    
	".r3d",   
	".mov",   
	".movie", 
	".mv",
	NULL
};

const char *imb_ext_movie[] = {
	".avi",
	".flc",
	".mov",
	".movie",
	".mp4",
	".m4v",
	".m2v",
	".m2t",
	".m2ts",
	".mts",
	".ts",
	".mv",
	".avs",
	".wmv",
	".ogv",
	".ogg",
	".r3d",
	".dv",
	".mpeg",
	".mpg",
	".mpg2",
	".vob",
	".mkv",
	".flv",
	".divx",
	".xvid",
	".mxf",
	".webm",
	NULL
};

/* sort of wrong being here... */
const char *imb_ext_audio[] = {
	".wav",
	".ogg",
	".oga",
	".mp3",
	".mp2",
	".ac3",
	".aac",
	".flac",
	".wma",
	".eac3",
	".aif",
	".aiff",
	".m4a",
	NULL
};

static int IMB_ispic_name(const char *name)
{
	/* increased from 32 to 64 because of the bitmaps header size */
#define HEADER_SIZE 64

	unsigned char buf[HEADER_SIZE];
	ImFileType *type;
	struct stat st;
	int fp;

	if (UTIL_DEBUG) printf("IMB_ispic_name: loading %s\n", name);
	
	if (BLI_stat(name, &st) == -1)
		return FALSE;
	if (((st.st_mode) & S_IFMT) != S_IFREG)
		return FALSE;

	if ((fp = BLI_open(name, O_BINARY | O_RDONLY, 0)) < 0)
		return FALSE;

	memset(buf, 0, sizeof(buf));
	if (read(fp, buf, HEADER_SIZE) <= 0) {
		close(fp);
		return FALSE;
	}

	close(fp);

	/* XXX move this exception */
	if ((BIG_LONG(((int *)buf)[0]) & 0xfffffff0) == 0xffd8ffe0)
		return JPG;

	for (type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
		if (type->is_a) {
			if (type->is_a(buf)) {
				return type->filetype;
			}
		}
		else if (type->is_a_filepath) {
			if (type->is_a_filepath(name)) {
				return type->filetype;
			}
		}
	}

	return FALSE;

#undef HEADER_SIZE
}

int IMB_ispic(const char *filename)
{
	if (U.uiflag & USER_FILTERFILEEXTS) {
		if ((BLI_testextensie_array(filename, imb_ext_image)) ||
		    (G.have_quicktime && BLI_testextensie_array(filename, imb_ext_image_qt)))
		{
			return IMB_ispic_name(filename);
		}
		else {
			return FALSE;
		}
	}
	else { /* no FILTERFILEEXTS */
		return IMB_ispic_name(filename);
	}
}



static int isavi(const char *name)
{
#ifdef WITH_AVI
	return AVI_is_avi(name);
#else
	(void)name;
	return FALSE;
#endif
}

#ifdef WITH_QUICKTIME
static int isqtime(const char *name)
{
	return anim_is_quicktime(name);
}
#endif

#ifdef WITH_FFMPEG

#ifdef _MSC_VER
#define va_copy(dst, src) ((dst) = (src))
#endif

/* BLI_vsnprintf in ffmpeg_log_callback() causes invalid warning */
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wmissing-format-attribute"
#endif

static char ffmpeg_last_error[1024];

static void ffmpeg_log_callback(void *ptr, int level, const char *format, va_list arg)
{
	if (ELEM(level, AV_LOG_FATAL, AV_LOG_ERROR)) {
		size_t n;
		va_list args_cpy;

		va_copy(args_cpy, arg);
		n = BLI_vsnprintf(ffmpeg_last_error, sizeof(ffmpeg_last_error), format, args_cpy);
		va_end(args_cpy);

		/* strip trailing \n */
		ffmpeg_last_error[n - 1] = '\0';
	}

	if (G.debug & G_DEBUG_FFMPEG) {
		/* call default logger to print all message to console */
		av_log_default_callback(ptr, level, format, arg);
	}
}

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

void IMB_ffmpeg_init(void)
{
	av_register_all();
	avdevice_register_all();

	ffmpeg_last_error[0] = '\0';

	if (G.debug & G_DEBUG_FFMPEG)
		av_log_set_level(AV_LOG_DEBUG);

	/* set own callback which could store last error to report to UI */
	av_log_set_callback(ffmpeg_log_callback);
}

const char *IMB_ffmpeg_last_error(void)
{
	return ffmpeg_last_error;
}

static int isffmpeg(const char *filename)
{
	AVFormatContext *pFormatCtx = NULL;
	unsigned int i;
	int videoStream;
	AVCodec *pCodec;
	AVCodecContext *pCodecCtx;

	if (BLI_testextensie_n(
	        filename,
	        ".swf", ".jpg", ".png", ".dds", ".tga", ".bmp", ".tif", ".exr", ".cin", ".wav", NULL))
	{
		return 0;
	}

	if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0) {
		if (UTIL_DEBUG) fprintf(stderr, "isffmpeg: av_open_input_file failed\n");
		return 0;
	}

	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		if (UTIL_DEBUG) fprintf(stderr, "isffmpeg: avformat_find_stream_info failed\n");
		avformat_close_input(&pFormatCtx);
		return 0;
	}

	if (UTIL_DEBUG) av_dump_format(pFormatCtx, 0, filename, 0);


	/* Find the first video stream */
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i] &&
		    pFormatCtx->streams[i]->codec &&
		    (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO))
		{
			videoStream = i;
			break;
		}

	if (videoStream == -1) {
		avformat_close_input(&pFormatCtx);
		return 0;
	}

	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

	/* Find the decoder for the video stream */
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		avformat_close_input(&pFormatCtx);
		return 0;
	}

	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		avformat_close_input(&pFormatCtx);
		return 0;
	}

	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 1;
}
#endif

#ifdef WITH_REDCODE
static int isredcode(const char *filename)
{
	struct redcode_handle *h = redcode_open(filename);
	if (!h) {
		return 0;
	}
	redcode_close(h);
	return 1;
}

#endif

int imb_get_anim_type(const char *name)
{
	int type;
	struct stat st;

	if (UTIL_DEBUG) printf("in getanimtype: %s\n", name);

#ifndef _WIN32
#   ifdef WITH_QUICKTIME
	if (isqtime(name)) return (ANIM_QTIME);
#   endif
#   ifdef WITH_FFMPEG
	/* stat test below fails on large files > 4GB */
	if (isffmpeg(name)) return (ANIM_FFMPEG);
#   endif
	if (BLI_stat(name, &st) == -1) return(0);
	if (((st.st_mode) & S_IFMT) != S_IFREG) return(0);

	if (isavi(name)) return (ANIM_AVI);

	if (ismovie(name)) return (ANIM_MOVIE);
#else
	if (BLI_stat(name, &st) == -1) return(0);
	if (((st.st_mode) & S_IFMT) != S_IFREG) return(0);

	if (ismovie(name)) return (ANIM_MOVIE);
#   ifdef WITH_QUICKTIME
	if (isqtime(name)) return (ANIM_QTIME);
#   endif
#   ifdef WITH_FFMPEG
	if (isffmpeg(name)) return (ANIM_FFMPEG);
#   endif


	if (isavi(name)) return (ANIM_AVI);
#endif
#ifdef WITH_REDCODE
	if (isredcode(name)) return (ANIM_REDCODE);
#endif
	type = IMB_ispic(name);
	if (type) {
		return ANIM_SEQUENCE;
	}

	return ANIM_NONE;
}
 
int IMB_isanim(const char *filename)
{
	int type;
	
	if (U.uiflag & USER_FILTERFILEEXTS) {
		if (G.have_quicktime) {
			if (BLI_testextensie_array(filename, imb_ext_movie_qt)) {	
				type = imb_get_anim_type(filename);
			}
			else {
				return(FALSE);
			}
		}
		else { /* no quicktime */
			if (BLI_testextensie_array(filename, imb_ext_movie)) {
				type = imb_get_anim_type(filename);
			}
			else {
				return(FALSE);
			}
		}
	}
	else { /* no FILTERFILEEXTS */
		type = imb_get_anim_type(filename);
	}
	
	return (type && type != ANIM_SEQUENCE);
}
