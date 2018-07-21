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

/* ********************** general blender movie support ***************************** */

static int start_stub(void *UNUSED(context_v), Scene *UNUSED(scene), RenderData *UNUSED(rd), int UNUSED(rectx), int UNUSED(recty),
                      ReportList *UNUSED(reports), bool UNUSED(preview), const char *UNUSED(suffix))
{ return 0; }

static void end_stub(void *UNUSED(context_v))
{}

static int append_stub(void *UNUSED(context_v), RenderData *UNUSED(rd), int UNUSED(start_frame), int UNUSED(frame), int *UNUSED(pixels),
                       int UNUSED(rectx), int UNUSED(recty), const char *UNUSED(suffix), ReportList *UNUSED(reports))
{ return 0; }

static void *context_create_stub(void)
{ return NULL; }

static void context_free_stub(void *UNUSED(context_v))
{}

#ifdef WITH_AVI
#  include "AVI_avi.h"

/* callbacks */
static int start_avi(void *context_v, Scene *scene, RenderData *rd, int rectx, int recty, ReportList *reports, bool preview, const char *suffix);
static void end_avi(void *context_v);
static int append_avi(void *context_v, RenderData *rd, int start_frame, int frame, int *pixels,
                      int rectx, int recty, const char *suffix, ReportList *reports);
static void filepath_avi(char *string, RenderData *rd, bool preview, const char *suffix);
static void *context_create_avi(void);
static void context_free_avi(void *context_v);
#endif  /* WITH_AVI */

#ifdef WITH_FFMPEG
#  include "BKE_writeffmpeg.h"
#endif

bMovieHandle *BKE_movie_handle_get(const char imtype)
{
	static bMovieHandle mh = {NULL};
	/* stub callbacks in case none of the movie formats is supported */
	mh.start_movie = start_stub;
	mh.append_movie = append_stub;
	mh.end_movie = end_stub;
	mh.get_next_frame = NULL;
	mh.get_movie_path = NULL;
	mh.context_create = context_create_stub;
	mh.context_free = context_free_stub;

	/* set the default handle, as builtin */
#ifdef WITH_AVI
	mh.start_movie = start_avi;
	mh.append_movie = append_avi;
	mh.end_movie = end_avi;
	mh.get_movie_path = filepath_avi;
	mh.context_create = context_create_avi;
	mh.context_free = context_free_avi;
#endif

	/* do the platform specific handles */
#ifdef WITH_FFMPEG
	if (ELEM(imtype, R_IMF_IMTYPE_FFMPEG, R_IMF_IMTYPE_H264, R_IMF_IMTYPE_XVID, R_IMF_IMTYPE_THEORA)) {
		mh.start_movie = BKE_ffmpeg_start;
		mh.append_movie = BKE_ffmpeg_append;
		mh.end_movie = BKE_ffmpeg_end;
		mh.get_movie_path = BKE_ffmpeg_filepath_get;
		mh.context_create = BKE_ffmpeg_context_create;
		mh.context_free = BKE_ffmpeg_context_free;
	}
#endif

	/* in case all above are disabled */
	(void)imtype;

	return (mh.append_movie != append_stub) ? &mh : NULL;
}

/* ****************************************************************** */


#ifdef WITH_AVI

static void filepath_avi(char *string, RenderData *rd, bool preview, const char *suffix)
{
	int sfra, efra;

	if (string == NULL) return;

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

	if (rd->scemode & R_EXTENSION) {
		if (!BLI_path_extension_check(string, ".avi")) {
			BLI_path_frame_range(string, sfra, efra, 4);
			strcat(string, ".avi");
		}
	}
	else {
		if (BLI_path_frame_check_chars(string)) {
			BLI_path_frame_range(string, sfra, efra, 4);
		}
	}

	BLI_path_suffix(string, FILE_MAX, suffix, "");
}

static int start_avi(void *context_v, Scene *UNUSED(scene), RenderData *rd, int rectx, int recty,
                     ReportList *reports, bool preview, const char *suffix)
{
	int x, y;
	char name[256];
	AviFormat format;
	int quality;
	double framerate;
	AviMovie *avi = context_v;

	filepath_avi(name, rd, preview, suffix);

	x = rectx;
	y = recty;

	quality = rd->im_format.quality;
	framerate = (double) rd->frs_sec / (double) rd->frs_sec_base;

	if (rd->im_format.imtype != R_IMF_IMTYPE_AVIJPEG) format = AVI_FORMAT_AVI_RGB;
	else format = AVI_FORMAT_MJPEG;

	if (AVI_open_compress(name, avi, 1, format) != AVI_ERROR_NONE) {
		BKE_report(reports, RPT_ERROR, "Cannot open or start AVI movie file");
		return 0;
	}

	AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_WIDTH, &x);
	AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_HEIGHT, &y);
	AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_QUALITY, &quality);
	AVI_set_compress_option(avi, AVI_OPTION_TYPE_MAIN, 0, AVI_OPTION_FRAMERATE, &framerate);

	avi->interlace = 0;
	avi->odd_fields = 0;

	printf("Created avi: %s\n", name);
	return 1;
}

static int append_avi(void *context_v, RenderData *UNUSED(rd), int start_frame, int frame, int *pixels,
                      int rectx, int recty, const char *UNUSED(suffix), ReportList *UNUSED(reports))
{
	unsigned int *rt1, *rt2, *rectot;
	int x, y;
	char *cp, rt;
	AviMovie *avi = context_v;

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
//	printf("added frame %3d (frame %3d in avi): ", frame, frame-start_frame);

	return 1;
}

static void end_avi(void *context_v)
{
	AviMovie *avi = context_v;

	if (avi == NULL) return;

	AVI_close_compress(avi);
}

static void *context_create_avi(void)
{
	AviMovie *avi = MEM_mallocN(sizeof(AviMovie), "avimovie");
	return avi;
}

static void context_free_avi(void *context_v)
{
	AviMovie *avi = context_v;
	if (avi) {
		MEM_freeN(avi);
	}
}

#endif  /* WITH_AVI */

/* similar to BKE_image_path_from_imformat() */
void BKE_movie_filepath_get(char *string, RenderData *rd, bool preview, const char *suffix)
{
	bMovieHandle *mh = BKE_movie_handle_get(rd->im_format.imtype);
	if (mh && mh->get_movie_path) {
		mh->get_movie_path(string, rd, preview, suffix);
	}
	else {
		string[0] = '\0';
	}
}
