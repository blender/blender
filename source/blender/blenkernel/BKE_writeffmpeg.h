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
 */

#ifndef __BKE_WRITEFFMPEG_H__
#define __BKE_WRITEFFMPEG_H__

/** \file BKE_writeffmpeg.h
 *  \ingroup bke
 */

#ifdef WITH_FFMPEG

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FFMPEG_MPEG1    = 0,
	FFMPEG_MPEG2    = 1,
	FFMPEG_MPEG4    = 2,
	FFMPEG_AVI      = 3,
	FFMPEG_MOV      = 4,
	FFMPEG_DV       = 5,
	FFMPEG_H264     = 6,
	FFMPEG_XVID     = 7,
	FFMPEG_FLV      = 8,
	FFMPEG_MKV      = 9,
	FFMPEG_OGG      = 10,
	FFMPEG_INVALID  = 11,
};

enum {
	FFMPEG_PRESET_NONE      = 0,
	FFMPEG_PRESET_DVD       = 1,
	FFMPEG_PRESET_SVCD      = 2,
	FFMPEG_PRESET_VCD       = 3,
	FFMPEG_PRESET_DV        = 4,
	FFMPEG_PRESET_H264      = 5,
	FFMPEG_PRESET_THEORA    = 6,
	FFMPEG_PRESET_XVID      = 7,
};

struct IDProperty;
struct RenderData;
struct ReportList;
struct Scene;

int BKE_ffmpeg_start(struct Scene *scene, struct RenderData *rd, int rectx, int recty, struct ReportList *reports);
void BKE_ffmpeg_end(void);
int BKE_ffmpeg_append(struct RenderData *rd, int start_frame, int frame, int *pixels,
                      int rectx, int recty, struct ReportList *reports);
void BKE_ffmpeg_filepath_get(char *string, struct RenderData *rd);

void BKE_ffmpeg_preset_set(struct RenderData *rd, int preset);
void BKE_ffmpeg_image_type_verify(struct RenderData *rd, struct ImageFormatData *imf);
void BKE_ffmpeg_codec_settings_verify(struct RenderData *rd);
bool BKE_ffmpeg_alpha_channel_is_supported(struct RenderData *rd);

struct IDProperty *BKE_ffmpeg_property_add(struct RenderData *Rd, const char *type, int opt_index, int parent_index);
int BKE_ffmpeg_property_add_string(struct RenderData *rd, const char *type, const char *str);
void BKE_ffmpeg_property_del(struct RenderData *rd, void *type, void *prop_);

#ifdef __cplusplus
}
#endif

#endif

#endif

