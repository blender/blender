/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef BKE_WRITEFFMPEG_H
#define BKE_WRITEFFMPEG_H

#ifdef WITH_FFMPEG

#ifdef __cplusplus
extern "C" {
#endif

#define FFMPEG_MPEG1 	0
#define FFMPEG_MPEG2 	1
#define FFMPEG_MPEG4 	2
#define FFMPEG_AVI	3
#define FFMPEG_MOV	4
#define FFMPEG_DV	5
#define FFMPEG_H264     6
#define FFMPEG_XVID     7
#define FFMPEG_FLV      8
#define FFMPEG_MKV      9
#define FFMPEG_OGG      10
#define FFMPEG_WAV      11
#define FFMPEG_MP3      12

#define FFMPEG_PRESET_NONE		0
#define FFMPEG_PRESET_DVD		1
#define FFMPEG_PRESET_SVCD		2
#define FFMPEG_PRESET_VCD 		3
#define FFMPEG_PRESET_DV		4
#define FFMPEG_PRESET_H264		5
#define FFMPEG_PRESET_THEORA	6
#define FFMPEG_PRESET_XVID		7

struct IDProperty;
struct RenderData;	
struct ReportList;
struct Scene;

extern int start_ffmpeg(struct Scene *scene, struct RenderData *rd, int rectx, int recty, struct ReportList *reports);
extern void end_ffmpeg(void);
extern int append_ffmpeg(struct RenderData *rd, int frame, int *pixels, int rectx, int recty, struct ReportList *reports);
void filepath_ffmpeg(char* string, struct RenderData* rd);

extern void ffmpeg_set_preset(struct RenderData *rd, int preset);
extern void ffmpeg_verify_image_type(struct RenderData *rd);

extern struct IDProperty *ffmpeg_property_add(struct RenderData *Rd, char *type, int opt_index, int parent_index);
extern int ffmpeg_property_add_string(struct RenderData *rd, const char *type, const char *str);
extern void ffmpeg_property_del(struct RenderData *rd, void *type, void *prop_);

#ifdef __cplusplus
}
#endif

#endif

#endif

