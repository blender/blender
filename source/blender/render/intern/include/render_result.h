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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/render_result.h
 *  \ingroup render
 */

#ifndef __RENDER_RESULT_H__
#define __RENDER_RESULT_H__

#define PASS_VECTOR_MAX	10000.0f

#define RR_USE_MEM		0
#define RR_USE_EXR		1

#define RR_ALL_LAYERS	NULL

struct ImBuf;
struct ListBase;
struct Render;
struct RenderData;
struct RenderLayer;
struct RenderResult;
struct Scene;
struct rcti;
struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;

/* New */

struct RenderResult *render_result_new(struct Render *re,
	struct rcti *partrct, int crop, int savebuffers, const char *layername);
struct RenderResult *render_result_new_full_sample(struct Render *re,
	struct ListBase *lb, struct rcti *partrct, int crop, int savebuffers);

struct RenderResult *render_result_new_from_exr(void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty);

/* Merge */

void render_result_merge(struct RenderResult *rr, struct RenderResult *rrpart);

/* Free */

void render_result_free(struct RenderResult *rr);
void render_result_free_list(struct ListBase *lb, struct RenderResult *rr);

/* Single Layer Render */

void render_result_single_layer_begin(struct Render *re);
void render_result_single_layer_end(struct Render *re);

/* EXR Tile File Render */

void render_result_exr_file_begin(struct Render *re);
void render_result_exr_file_end(struct Render *re);

void render_result_exr_file_merge(struct RenderResult *rr, struct RenderResult *rrpart);

void render_result_exr_file_path(struct Scene *scene, const char *layname, int sample, char *filepath);
int render_result_exr_file_read(struct Render *re, int sample);
int render_result_exr_file_read_path(struct RenderResult *rr, struct RenderLayer *rl_single, const char *filepath);

/* Combined Pixel Rect */

struct ImBuf *render_result_rect_to_ibuf(struct RenderResult *rr, struct RenderData *rd);
void render_result_rect_from_ibuf(struct RenderResult *rr, struct RenderData *rd,
	struct ImBuf *ibuf);

void render_result_rect_fill_zero(struct RenderResult *rr);
void render_result_rect_get_pixels(struct RenderResult *rr,
	unsigned int *rect, int rectx, int recty,
	const struct ColorManagedViewSettings *view_settings,
	const struct ColorManagedDisplaySettings *display_settings);

#endif /* __RENDER_RESULT_H__ */

