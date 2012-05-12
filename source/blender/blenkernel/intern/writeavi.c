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
 * Contributor(s): Robert Wenzlaff
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Functions for writing avi-format files.
 * Added interface for generic movie support (ton)
 */

/** \file blender/blenkernel/intern/writeavi.c
 *  \ingroup bke
 */


#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BKE_writeavi.h"
#include "AVI_avi.h"

/* callbacks */
static int start_avi(Scene *scene, RenderData *rd, int rectx, int recty, ReportList *reports);
static void end_avi(void);
static int append_avi(RenderData *rd, int start_frame, int frame, int *pixels,
                      int rectx, int recty, ReportList *reports);
static void filepath_avi(char *string, RenderData *rd);

/* ********************** general blender movie support ***************************** */

#ifdef WITH_QUICKTIME
#include "quicktime_export.h"
#endif

#ifdef WITH_FFMPEG
#include "BKE_writeffmpeg.h"
#endif

#include "BKE_writeframeserver.h"

bMovieHandle *BKE_movie_handle_get(const char imtype)
{
	static bMovieHandle mh;
	
	/* set the default handle, as builtin */
	mh.start_movie = start_avi;
	mh.append_movie = append_avi;
	mh.end_movie = end_avi;
	mh.get_next_frame = NULL;
	mh.get_movie_path = filepath_avi;
	
	/* do the platform specific handles */
#if defined(_WIN32) && !defined(FREE_WINDOWS)
	if (imtype == R_IMF_IMTYPE_AVICODEC) {		
		//XXX mh.start_movie= start_avi_codec;
		//XXX mh.append_movie= append_avi_codec;
		//XXX mh.end_movie= end_avi_codec;
	}
#endif
#ifdef WITH_QUICKTIME
	if (imtype == R_IMF_IMTYPE_QUICKTIME) {
		mh.start_movie = start_qt;
		mh.append_movie = append_qt;
		mh.end_movie = end_qt;
		mh.get_movie_path = filepath_qt;
	}
#endif
#ifdef WITH_FFMPEG
	if (ELEM4(imtype, R_IMF_IMTYPE_FFMPEG, R_IMF_IMTYPE_H264, R_IMF_IMTYPE_XVID, R_IMF_IMTYPE_THEORA)) {
		mh.start_movie = BKE_ffmpeg_start;
		mh.append_movie = BKE_ffmpeg_append;
		mh.end_movie = BKE_ffmpeg_end;
		mh.get_movie_path = BKE_ffmpeg_filepath_get;
	}
#endif
#ifdef WITH_FRAMESERVER
	if (imtype == R_IMF_IMTYPE_FRAMESERVER) {
		mh.start_movie = BKE_frameserver_start;
		mh.append_movie = BKE_frameserver_append;
		mh.end_movie = BKE_frameserver_end;
		mh.get_next_frame = BKE_frameserver_loop;
	}
#endif

	/* in case all above are disabled */
	(void)imtype;

	return &mh;
}

/* ****************************************************************** */


static AviMovie *avi = NULL;

static void filepath_avi(char *string, RenderData *rd)
{
	if (string == NULL) return;

	strcpy(string, rd->pic);
	BLI_path_abs(string, G.main->name);

	BLI_make_existing_file(string);

	if (!BLI_testextensie(string, ".avi")) {
		BLI_path_frame_range(string, rd->sfra, rd->efra, 4);
		strcat(string, ".avi");
	}
}

static int start_avi(Scene *scene, RenderData *rd, int rectx, int recty, ReportList *reports)
{
	int x, y;
	char name[256];
	AviFormat format;
	int quality;
	double framerate;
	
	(void)scene; /* unused */
	
	filepath_avi(name, rd);

	x = rectx;
	y = recty;

	quality = rd->im_format.quality;
	framerate = (double) rd->frs_sec / (double) rd->frs_sec_base;
	
	avi = MEM_mallocN(sizeof(AviMovie), "avimovie");

	if (rd->im_format.imtype != R_IMF_IMTYPE_AVIJPEG) format = AVI_FORMAT_AVI_RGB;
	else format = AVI_FORMAT_MJPEG;

	if (AVI_open_compress(name, avi, 1, format) != AVI_ERROR_NONE) {
		BKE_report(reports, RPT_ERROR, "Cannot open or start AVI movie file.");
		MEM_freeN(avi);
		avi = NULL;
		return 0;
	}
			
	AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_WIDTH, &x);
	AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_HEIGHT, &y);
	AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_QUALITY, &quality);
	AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_FRAMERATE, &framerate);

	avi->interlace = 0;
	avi->odd_fields = 0;
/*  avi->interlace= rd->mode & R_FIELDS; */
/*  avi->odd_fields= (rd->mode & R_ODDFIELD)?1:0; */
	
	printf("Created avi: %s\n", name);
	return 1;
}

static int append_avi(RenderData *UNUSED(rd), int start_frame, int frame, int *pixels,
                      int rectx, int recty, ReportList *UNUSED(reports))
{
	unsigned int *rt1, *rt2, *rectot;
	int x, y;
	char *cp, rt;
	
	if (avi == NULL)
		return 0;

	/* note that libavi free's the buffer... stupid interface - zr */
	rectot = MEM_mallocN(rectx * recty * sizeof(int), "rectot");
	rt1 = rectot;
	rt2 = (unsigned int *)pixels + (recty - 1) * rectx;
	/* flip y and convert to abgr */
	for (y = 0; y < recty; y++, rt1 += rectx, rt2 -= rectx) {
		memcpy(rt1, rt2, rectx * sizeof(int));
		
		cp = (char *)rt1;
		for (x = rectx; x > 0; x--) {
			rt = cp[0];
			cp[0] = cp[3];
			cp[3] = rt;
			rt = cp[1];
			cp[1] = cp[2];
			cp[2] = rt;
			cp += 4;
		}
	}
	
	AVI_write_frame(avi, (frame - start_frame), AVI_FORMAT_RGB32, rectot, rectx * recty * 4);
//	printf ("added frame %3d (frame %3d in avi): ", frame, frame-start_frame);

	return 1;
}

static void end_avi(void)
{
	if (avi == NULL) return;

	AVI_close_compress(avi);
	MEM_freeN(avi);
	avi = NULL;
}

/* similar to BKE_makepicstring() */
void BKE_movie_filepath_get(char *string, RenderData *rd)
{
	bMovieHandle *mh = BKE_movie_handle_get(rd->im_format.imtype);
	if (mh->get_movie_path)
		mh->get_movie_path(string, rd);
	else
		string[0] = '\0';
}
