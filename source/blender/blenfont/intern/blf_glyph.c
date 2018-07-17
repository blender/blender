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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/intern/blf_glyph.c
 *  \ingroup blf
 *
 * Glyph rendering, texturing and caching. Wraps Freetype and OpenGL functions.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_threads.h"

#include "BIF_gl.h"
#include "BLF_api.h"

#ifndef BLF_STANDALONE
#  include "GPU_immediate.h"
#  include "GPU_extensions.h"
#endif

#include "blf_internal_types.h"
#include "blf_internal.h"

#include "BLI_strict_flags.h"
#include "BLI_math_vector.h"

KerningCacheBLF *blf_kerning_cache_find(FontBLF *font)
{
	KerningCacheBLF *p;

	p = (KerningCacheBLF *)font->kerning_caches.first;
	while (p) {
		if (p->mode == font->kerning_mode)
			return p;
		p = p->next;
	}
	return NULL;
}

/* Create a new glyph cache for the current kerning mode. */
KerningCacheBLF *blf_kerning_cache_new(FontBLF *font)
{
	KerningCacheBLF *kc;

	kc = (KerningCacheBLF *)MEM_callocN(sizeof(KerningCacheBLF), "blf_kerning_cache_new");
	kc->next = NULL;
	kc->prev = NULL;
	kc->mode = font->kerning_mode;

	unsigned int i, j;
	for (i = 0; i < 0x80; i++) {
		for (j = 0; j < 0x80; j++) {
			GlyphBLF *g = blf_glyph_search(font->glyph_cache, i);
			if (!g) {
				FT_UInt glyph_index = FT_Get_Char_Index(font->face, i);
				g = blf_glyph_add(font, glyph_index, i);
			}
			/* Cannot fail since it has been added just before. */
			GlyphBLF *g_prev = blf_glyph_search(font->glyph_cache, j);

			FT_Vector delta = {.x = 0, .y = 0};
			if (FT_Get_Kerning(font->face, g_prev->idx, g->idx, kc->mode, &delta) == 0) {
				kc->table[i][j] = (int)delta.x >> 6;
			}
			else {
				kc->table[i][j] = 0;
			}
		}
	}

	BLI_addhead(&font->kerning_caches, kc);
	return kc;
}

void blf_kerning_cache_clear(FontBLF *font)
{
	font->kerning_cache = NULL;
	BLI_freelistN(&font->kerning_caches);
}

GlyphCacheBLF *blf_glyph_cache_find(FontBLF *font, unsigned int size, unsigned int dpi)
{
	GlyphCacheBLF *p;

	p = (GlyphCacheBLF *)font->cache.first;
	while (p) {
		if (p->size == size && p->dpi == dpi)
			return p;
		p = p->next;
	}
	return NULL;
}

/* Create a new glyph cache for the current size and dpi. */
GlyphCacheBLF *blf_glyph_cache_new(FontBLF *font)
{
	GlyphCacheBLF *gc;

	gc = (GlyphCacheBLF *)MEM_callocN(sizeof(GlyphCacheBLF), "blf_glyph_cache_new");
	gc->next = NULL;
	gc->prev = NULL;
	gc->size = font->size;
	gc->dpi = font->dpi;

	memset(gc->glyph_ascii_table, 0, sizeof(gc->glyph_ascii_table));
	memset(gc->bucket, 0, sizeof(gc->bucket));

	gc->textures = (GPUTexture **)MEM_callocN(sizeof(GPUTexture *) * 256, __func__);
	gc->textures_len = 256;
	gc->texture_current = BLF_TEXTURE_UNSET;
	gc->offset_x = 3; /* enough padding for blur */
	gc->offset_y = 3; /* enough padding for blur */
	gc->pad = 6;

	gc->glyphs_len_max = (int)font->face->num_glyphs;
	gc->glyphs_len_free = (int)font->face->num_glyphs;
	gc->ascender = ((float)font->face->size->metrics.ascender) / 64.0f;
	gc->descender = ((float)font->face->size->metrics.descender) / 64.0f;

	if (FT_IS_SCALABLE(font->face)) {
		gc->glyph_width_max = (int)((float)(font->face->bbox.xMax - font->face->bbox.xMin) *
		                            (((float)font->face->size->metrics.x_ppem) /
		                             ((float)font->face->units_per_EM)));

		gc->glyph_height_max = (int)((float)(font->face->bbox.yMax - font->face->bbox.yMin) *
		                             (((float)font->face->size->metrics.y_ppem) /
		                              ((float)font->face->units_per_EM)));
	}
	else {
		gc->glyph_width_max = (int)(((float)font->face->size->metrics.max_advance) / 64.0f);
		gc->glyph_height_max = (int)(((float)font->face->size->metrics.height) / 64.0f);
	}

	/* can happen with size 1 fonts */
	CLAMP_MIN(gc->glyph_width_max, 1);
	CLAMP_MIN(gc->glyph_height_max, 1);

	gc->p2_width = 0;
	gc->p2_height = 0;

	BLI_addhead(&font->cache, gc);
	return gc;
}

void blf_glyph_cache_clear(FontBLF *font)
{
	GlyphCacheBLF *gc;

	while ((gc = BLI_pophead(&font->cache))) {
		blf_glyph_cache_free(gc);
	}
	font->glyph_cache = NULL;
}

void blf_glyph_cache_free(GlyphCacheBLF *gc)
{
	GlyphBLF *g;
	unsigned int i;

	for (i = 0; i < 257; i++) {
		while ((g = BLI_pophead(&gc->bucket[i]))) {
			blf_glyph_free(g);
		}
	}
	for (i = 0; i < gc->textures_len; i++) {
		if (gc->textures[i]) {
			GPU_texture_free(gc->textures[i]);
		}
	}
	MEM_freeN(gc->textures);
	MEM_freeN(gc);
}

static void blf_glyph_cache_texture(FontBLF *font, GlyphCacheBLF *gc)
{
	int i;
	char error[256];

	/* move the index. */
	gc->texture_current++;

	if (UNLIKELY(gc->texture_current >= gc->textures_len)) {
		gc->textures_len *= 2;
		gc->textures = MEM_recallocN((void *)gc->textures, sizeof(GPUTexture *) * gc->textures_len);
	}

	gc->p2_width = (int)blf_next_p2((unsigned int)((gc->glyphs_len_free * gc->glyph_width_max) + (gc->pad * 2)));
	if (gc->p2_width > font->tex_size_max) {
		gc->p2_width = font->tex_size_max;
	}

	i = (int)((gc->p2_width - (gc->pad * 2)) / gc->glyph_width_max);
	gc->p2_height = (int)blf_next_p2((unsigned int)(((gc->glyphs_len_max / i) + 1) * gc->glyph_height_max + (gc->pad * 2)));

	if (gc->p2_height > font->tex_size_max) {
		gc->p2_height = font->tex_size_max;
	}

	unsigned char *pixels = MEM_callocN((size_t)gc->p2_width * (size_t)gc->p2_height, "BLF texture init");
	GPUTexture *tex = GPU_texture_create_nD(gc->p2_width, gc->p2_height, 0, 2, pixels, GPU_R8, GPU_DATA_UNSIGNED_BYTE, 0, false, error);
	MEM_freeN(pixels);
	gc->textures[gc->texture_current] = tex;
	GPU_texture_bind(tex, 0);
	GPU_texture_wrap_mode(tex, false);
	GPU_texture_filters(tex, GPU_NEAREST, GPU_LINEAR);
	GPU_texture_unbind(tex);
}

GlyphBLF *blf_glyph_search(GlyphCacheBLF *gc, unsigned int c)
{
	GlyphBLF *p;
	unsigned int key;

	key = blf_hash(c);
	p = gc->bucket[key].first;
	while (p) {
		if (p->c == c)
			return p;
		p = p->next;
	}
	return NULL;
}

GlyphBLF *blf_glyph_add(FontBLF *font, unsigned int index, unsigned int c)
{
	FT_GlyphSlot slot;
	GlyphBLF *g;
	FT_Error err;
	FT_Bitmap bitmap, tempbitmap;
	const bool is_sharp = !BLF_antialias_get();
	int flags = FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP;
	FT_BBox bbox;
	unsigned int key;

	g = blf_glyph_search(font->glyph_cache, c);
	if (g)
		return g;

	/* glyphs are dynamically created as needed by font rendering. this means that
	 * to make font rendering thread safe we have to do locking here. note that this
	 * must be a lock for the whole library and not just per font, because the font
	 * renderer uses a shared buffer internally */
	BLI_spin_lock(font->ft_lib_mutex);

	/* search again after locking */
	g = blf_glyph_search(font->glyph_cache, c);
	if (g) {
		BLI_spin_unlock(font->ft_lib_mutex);
		return g;
	}

	if (font->flags & BLF_HINTING)
		flags &= ~FT_LOAD_NO_HINTING;

	if (is_sharp)
		err = FT_Load_Glyph(font->face, (FT_UInt)index, FT_LOAD_TARGET_MONO);
	else
		err = FT_Load_Glyph(font->face, (FT_UInt)index, flags);

	if (err) {
		BLI_spin_unlock(font->ft_lib_mutex);
		return NULL;
	}

	/* get the glyph. */
	slot = font->face->glyph;

	if (is_sharp) {
		err = FT_Render_Glyph(slot, FT_RENDER_MODE_MONO);

		/* Convert result from 1 bit per pixel to 8 bit per pixel */
		/* Accum errors for later, fine if not interested beyond "ok vs any error" */
		FT_Bitmap_New(&tempbitmap);
		err += FT_Bitmap_Convert(font->ft_lib, &slot->bitmap, &tempbitmap, 1); /* Does Blender use Pitch 1 always? It works so far */
		err += FT_Bitmap_Copy(font->ft_lib, &tempbitmap, &slot->bitmap);
		err += FT_Bitmap_Done(font->ft_lib, &tempbitmap);
	}
	else {
		err = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	}

	if (err || slot->format != FT_GLYPH_FORMAT_BITMAP) {
		BLI_spin_unlock(font->ft_lib_mutex);
		return NULL;
	}

	g = (GlyphBLF *)MEM_callocN(sizeof(GlyphBLF), "blf_glyph_add");
	g->c = c;
	g->idx = (FT_UInt)index;
	g->offset_x = -1;
	g->offset_y = -1;
	bitmap = slot->bitmap;
	g->width = (int)bitmap.width;
	g->height = (int)bitmap.rows;

	if (g->width && g->height) {
		if (is_sharp) {
			/* Font buffer uses only 0 or 1 values, Blender expects full 0..255 range */
			int i;
			for (i = 0; i < (g->width * g->height); i++) {
				bitmap.buffer[i] = bitmap.buffer[i] ? 255 : 0;
			}
		}

		g->bitmap = (unsigned char *)MEM_mallocN((size_t)g->width * (size_t)g->height, "glyph bitmap");
		memcpy((void *)g->bitmap, (void *)bitmap.buffer, (size_t)g->width * (size_t)g->height);
	}

	g->advance = ((float)slot->advance.x) / 64.0f;
	g->advance_i = (int)g->advance;
	g->pos_x = (float)slot->bitmap_left;
	g->pos_y = (float)slot->bitmap_top;
	g->pitch = slot->bitmap.pitch;

	FT_Outline_Get_CBox(&(slot->outline), &bbox);
	g->box.xmin = ((float)bbox.xMin) / 64.0f;
	g->box.xmax = ((float)bbox.xMax) / 64.0f;
	g->box.ymin = ((float)bbox.yMin) / 64.0f;
	g->box.ymax = ((float)bbox.yMax) / 64.0f;

	key = blf_hash(g->c);
	BLI_addhead(&(font->glyph_cache->bucket[key]), g);

	BLI_spin_unlock(font->ft_lib_mutex);

	return g;
}

void blf_glyph_free(GlyphBLF *g)
{
	/* don't need free the texture, the GlyphCache already
	 * have a list of all the texture and free it.
	 */
	if (g->bitmap)
		MEM_freeN(g->bitmap);
	MEM_freeN(g);
}

static void blf_texture_draw(const unsigned char color[4], const float uv[2][2], float x1, float y1, float x2, float y2)
{
	/* Only one vertex per glyph, geometry shader expand it into a quad. */
	/* TODO Get rid of Geom Shader because it's not optimal AT ALL for the GPU */
	copy_v4_fl4(GPU_vertbuf_raw_step(&g_batch.pos_step), x1 + g_batch.ofs[0], y1 + g_batch.ofs[1],
	                                                     x2 + g_batch.ofs[0], y2 + g_batch.ofs[1]);
	copy_v4_v4(GPU_vertbuf_raw_step(&g_batch.tex_step), (float *)uv);
	copy_v4_v4_uchar(GPU_vertbuf_raw_step(&g_batch.col_step), color);
	g_batch.glyph_len++;
	/* Flush cache if it's full. */
	if (g_batch.glyph_len == BLF_BATCH_DRAW_LEN_MAX) {
		blf_batch_draw();
	}
}

static void blf_texture5_draw(const unsigned char color_in[4], int tex_w, int tex_h, const float uv[2][2],
                              float x1, float y1, float x2, float y2)
{
	float ofs[2] = { 2 / (float)tex_w, 2 / (float)tex_h };
	float uv_flag[2][2];
	copy_v4_v4((float *)uv_flag, (float *)uv);
	/* flag the x and y component signs for 5x5 bluring */
	uv_flag[0][0] = -(uv_flag[0][0] - ofs[0]);
	uv_flag[0][1] = -(uv_flag[0][1] - ofs[1]);
	uv_flag[1][0] = -(uv_flag[1][0] + ofs[0]);
	uv_flag[1][1] = -(uv_flag[1][1] + ofs[1]);

	blf_texture_draw(color_in, uv_flag, x1 - 2, y1 + 2, x2 + 2, y2 - 2);
}

static void blf_texture3_draw(const unsigned char color_in[4], int tex_w, int tex_h, const float uv[2][2],
                              float x1, float y1, float x2, float y2)
{
	float ofs[2] = { 1 / (float)tex_w, 1 / (float)tex_h };
	float uv_flag[2][2];
	copy_v4_v4((float *)uv_flag, (float *)uv);
	/* flag the x component sign for 3x3 bluring */
	uv_flag[0][0] = -(uv_flag[0][0] - ofs[0]);
	uv_flag[0][1] =  (uv_flag[0][1] - ofs[1]);
	uv_flag[1][0] = -(uv_flag[1][0] + ofs[0]);
	uv_flag[1][1] =  (uv_flag[1][1] + ofs[1]);

	blf_texture_draw(color_in, uv_flag, x1 - 1, y1 + 1, x2 + 1, y2 - 1);
}

static void blf_glyph_calc_rect(rctf *rect, GlyphBLF *g, float x, float y)
{
	rect->xmin = floorf(x + g->pos_x);
	rect->xmax = rect->xmin + (float)g->width;
	rect->ymin = y + g->pos_y;
	rect->ymax = y + g->pos_y - (float)g->height;
}

void blf_glyph_render(FontBLF *font, GlyphBLF *g, float x, float y)
{
	rctf rect;

	if ((!g->width) || (!g->height))
		return;

	if (g->build_tex == 0) {
		GlyphCacheBLF *gc = font->glyph_cache;

		if (font->tex_size_max == -1)
			font->tex_size_max = GPU_max_texture_size();

		if (gc->texture_current == BLF_TEXTURE_UNSET) {
			blf_glyph_cache_texture(font, gc);
			gc->offset_x = gc->pad;
			gc->offset_y = 3; /* enough padding for blur */
		}

		if (gc->offset_x > (gc->p2_width - gc->glyph_width_max)) {
			gc->offset_x = gc->pad;
			gc->offset_y += gc->glyph_height_max;

			if (gc->offset_y > (gc->p2_height - gc->glyph_height_max)) {
				gc->offset_y = 3; /* enough padding for blur */
				blf_glyph_cache_texture(font, gc);
			}
		}

		g->tex = gc->textures[gc->texture_current];
		g->offset_x = gc->offset_x;
		g->offset_y = gc->offset_y;

		/* prevent glTexSubImage2D from failing if the character
		 * asks for pixels out of bounds, this tends only to happen
		 * with very small sizes (5px high or less) */
		if (UNLIKELY((g->offset_x + g->width)  > gc->p2_width)) {
			g->width  -= (g->offset_x + g->width)  - gc->p2_width;
			BLI_assert(g->width > 0);
		}
		if (UNLIKELY((g->offset_y + g->height) > gc->p2_height)) {
			g->height -= (g->offset_y + g->height) - gc->p2_height;
			BLI_assert(g->height > 0);
		}

		GPU_texture_update_sub(g->tex, GPU_DATA_UNSIGNED_BYTE, g->bitmap, g->offset_x, g->offset_y, 0, g->width, g->height, 0);

		g->uv[0][0] = ((float)g->offset_x) / ((float)gc->p2_width);
		g->uv[0][1] = ((float)g->offset_y) / ((float)gc->p2_height);
		g->uv[1][0] = ((float)(g->offset_x + g->width)) / ((float)gc->p2_width);
		g->uv[1][1] = ((float)(g->offset_y + g->height)) / ((float)gc->p2_height);

		/* update the x offset for the next glyph. */
		gc->offset_x += (int)BLI_rctf_size_x(&g->box) + gc->pad;

		gc->glyphs_len_free--;
		g->build_tex = 1;
	}

	blf_glyph_calc_rect(&rect, g, x, y);

	if (font->flags & BLF_CLIPPING) {
		/* intentionally check clipping without shadow offset */
		rctf rect_test = rect;
		BLI_rctf_translate(&rect_test, font->pos[0], font->pos[1]);

		if (!BLI_rctf_inside_rctf(&font->clip_rec, &rect_test)) {
			return;
		}
	}

	if (font->tex_bind_state != g->tex) {
		blf_batch_draw();
		font->tex_bind_state = g->tex;
		GPU_texture_bind(font->tex_bind_state, 0);
	}

	g_batch.tex_bind_state = g->tex;

	if (font->flags & BLF_SHADOW) {
		rctf rect_ofs;
		blf_glyph_calc_rect(&rect_ofs, g,
		                    x + (float)font->shadow_x,
		                    y + (float)font->shadow_y);

		if (font->shadow == 0) {
			blf_texture_draw(font->shadow_color, g->uv, rect_ofs.xmin, rect_ofs.ymin, rect_ofs.xmax, rect_ofs.ymax);
		}
		else if (font->shadow <= 4) {
			blf_texture3_draw(font->shadow_color, font->glyph_cache->p2_width, font->glyph_cache->p2_height, g->uv,
			                  rect_ofs.xmin, rect_ofs.ymin, rect_ofs.xmax, rect_ofs.ymax);
		}
		else {
			blf_texture5_draw(font->shadow_color, font->glyph_cache->p2_width, font->glyph_cache->p2_height, g->uv,
			                  rect_ofs.xmin, rect_ofs.ymin, rect_ofs.xmax, rect_ofs.ymax);
		}
	}

#if BLF_BLUR_ENABLE
	switch (font->blur) {
		case 3:
			blf_texture3_draw(font->color, font->glyph_cache->p2_width, font->glyph_cache->p2_height, g->uv,
			                  rect.xmin, rect.ymin, rect.xmax, rect.ymax);
			break;
		case 5:
			blf_texture5_draw(font->color, font->glyph_cache->p2_width, font->glyph_cache->p2_height, g->uv,
			                  rect.xmin, rect.ymin, rect.xmax, rect.ymax);
			break;
		default:
			blf_texture_draw(font->color, g->uv, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	}
#else
	blf_texture_draw(font->color, g->uv, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
#endif
}
