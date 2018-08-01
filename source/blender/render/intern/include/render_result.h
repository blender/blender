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
#define RR_ALL_VIEWS	NULL

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
	struct rcti *partrct, int crop, int savebuffers, const char *layername, const char *viewname);

struct RenderResult *render_result_new_from_exr(void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty);

void render_result_view_new(struct RenderResult *rr, const char *viewname);
void render_result_views_new(struct RenderResult *rr, struct RenderData *rd);

/* Merge */

void render_result_merge(struct RenderResult *rr, struct RenderResult *rrpart);

/* Add Passes */

void render_result_clone_passes(struct Render *re, struct RenderResult *rr, const char *viewname);
void render_result_add_pass(struct RenderResult *rr, const char *name, int channels, const char *chan_id, const char *layername, const char *viewname);

/* Free */

void render_result_free(struct RenderResult *rr);
void render_result_free_list(struct ListBase *lb, struct RenderResult *rr);

/* Single Layer Render */

void render_result_single_layer_begin(struct Render *re);
void render_result_single_layer_end(struct Render *re);

/* EXR Tile File Render */

void render_result_save_empty_result_tiles(struct Render *re);
void render_result_exr_file_begin(struct Render *re);
void render_result_exr_file_end(struct Render *re);

/* render pass wrapper for gpencil */
struct RenderPass *gp_add_pass(struct RenderResult *rr, struct RenderLayer *rl, int channels, const char *name, const char *viewname);

void render_result_exr_file_merge(struct RenderResult *rr, struct RenderResult *rrpart, const char *viewname);

void render_result_exr_file_path(struct Scene *scene, const char *layname, int sample, char *filepath);
int render_result_exr_file_read_sample(struct Render *re, int sample);
int render_result_exr_file_read_path(struct RenderResult *rr, struct RenderLayer *rl_single, const char *filepath);

/* EXR cache */

void render_result_exr_file_cache_write(struct Render *re);
bool render_result_exr_file_cache_read(struct Render *re);

/* Combined Pixel Rect */

struct ImBuf *render_result_rect_to_ibuf(struct RenderResult *rr, struct RenderData *rd, const int view_id);

void render_result_rect_fill_zero(struct RenderResult *rr, const int view_id);
void render_result_rect_get_pixels(struct RenderResult *rr,
	unsigned int *rect, int rectx, int recty,
	const struct ColorManagedViewSettings *view_settings,
	const struct ColorManagedDisplaySettings *display_settings,
	const int view_id);

void render_result_views_shallowcopy(struct RenderResult *dst, struct RenderResult *src);
void render_result_views_shallowdelete(struct RenderResult *rr);
bool render_result_has_views(struct RenderResult *rr);

#define FOREACH_VIEW_LAYER_TO_RENDER_BEGIN(re_, iter_)    \
{                                                         \
	int nr_;                                              \
	ViewLayer *iter_;                                     \
	for (nr_ = 0, iter_ = (re_)->view_layers.first;       \
	     iter_ != NULL;                                   \
	    iter_ = iter_->next, nr_++)                       \
	{                                                     \
		if (!G.background &&  (re_)->r.scemode & R_SINGLE_LAYER) {  \
			if (nr_ != re->active_view_layer) {           \
				continue;                                 \
			}                                             \
		}                                                 \
		else {                                            \
			if ((iter_->flag & VIEW_LAYER_RENDER) == 0) { \
				continue;                                 \
			}                                             \
		}

#define FOREACH_VIEW_LAYER_TO_RENDER_END                  \
	}                                                     \
} ((void)0)

#endif /* __RENDER_RESULT_H__ */
