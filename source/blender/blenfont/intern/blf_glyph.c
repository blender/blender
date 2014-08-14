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

#include "blf_internal_types.h"
#include "blf_internal.h"

#include "BLI_strict_flags.h"

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

	gc->textures = (GLuint *)MEM_mallocN(sizeof(GLuint) * 256, __func__);
	gc->ntex = 256;
	gc->cur_tex = BLF_CURTEX_UNSET;
	gc->x_offs = 0;
	gc->y_offs = 0;
	gc->pad = 3;

	gc->num_glyphs = (int)font->face->num_glyphs;
	gc->rem_glyphs = (int)font->face->num_glyphs;
	gc->ascender = ((float)font->face->size->metrics.ascender) / 64.0f;
	gc->descender = ((float)font->face->size->metrics.descender) / 64.0f;

	if (FT_IS_SCALABLE(font->face)) {
		gc->max_glyph_width = (int)((float)(font->face->bbox.xMax - font->face->bbox.xMin) *
		                            (((float)font->face->size->metrics.x_ppem) /
		                             ((float)font->face->units_per_EM)));

		gc->max_glyph_height = (int)((float)(font->face->bbox.yMax - font->face->bbox.yMin) *
		                             (((float)font->face->size->metrics.y_ppem) /
		                              ((float)font->face->units_per_EM)));
	}
	else {
		gc->max_glyph_width = (int)(((float)font->face->size->metrics.max_advance) / 64.0f);
		gc->max_glyph_height = (int)(((float)font->face->size->metrics.height) / 64.0f);
	}

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
}

void blf_glyph_cache_free(GlyphCacheBLF *gc)
{
	GlyphBLF *g;
	int i;

	for (i = 0; i < 257; i++) {
		while ((g = BLI_pophead(&gc->bucket[i]))) {
			blf_glyph_free(g);
		}
	}

	if (gc->cur_tex != BLF_CURTEX_UNSET)
		glDeleteTextures((int)gc->cur_tex + 1, gc->textures);
	MEM_freeN((void *)gc->textures);
	MEM_freeN(gc);
}

static void blf_glyph_cache_texture(FontBLF *font, GlyphCacheBLF *gc)
{
	int i;

	/* move the index. */
	gc->cur_tex++;

	if (UNLIKELY(gc->cur_tex >= gc->ntex)) {
		gc->ntex *= 2;
		gc->textures = (GLuint *)MEM_reallocN((void *)gc->textures, sizeof(GLuint) * gc->ntex);
	}

	gc->p2_width = (int)blf_next_p2((unsigned int)((gc->rem_glyphs * gc->max_glyph_width) + (gc->pad * 2)));
	if (gc->p2_width > font->max_tex_size)
		gc->p2_width = font->max_tex_size;

	i = (int)((gc->p2_width - (gc->pad * 2)) / gc->max_glyph_width);
	gc->p2_height = (int)blf_next_p2((unsigned int)(((gc->num_glyphs / i) + 1) * gc->max_glyph_height));

	if (gc->p2_height > font->max_tex_size)
		gc->p2_height = font->max_tex_size;

	glGenTextures(1, &gc->textures[gc->cur_tex]);
	glBindTexture(GL_TEXTURE_2D, (font->tex_bind_state = gc->textures[gc->cur_tex]));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, gc->p2_width, gc->p2_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL);
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
	int sharp = (U.text_render & USER_TEXT_DISABLE_AA);
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
	
	if (sharp)
		err = FT_Load_Glyph(font->face, (FT_UInt)index, FT_LOAD_TARGET_MONO);
	else
		err = FT_Load_Glyph(font->face, (FT_UInt)index, flags);  

	if (err) {
		BLI_spin_unlock(font->ft_lib_mutex);
		return NULL;
	}

	/* get the glyph. */
	slot = font->face->glyph;

	if (sharp) {
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
	g->xoff = -1;
	g->yoff = -1;
	bitmap = slot->bitmap;
	g->width = bitmap.width;
	g->height = bitmap.rows;

	if (g->width && g->height) {
		if (sharp) {
			/* Font buffer uses only 0 or 1 values, Blender expects full 0..255 range */
			int i;
			for (i = 0; i < (g->width * g->height); i++) {
				bitmap.buffer[i] = bitmap.buffer[i] ? 255 : 0;
			}
		}

		g->bitmap = (unsigned char *)MEM_mallocN((size_t)(g->width * g->height), "glyph bitmap");
		memcpy((void *)g->bitmap, (void *)bitmap.buffer, (size_t)(g->width * g->height));
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

static void blf_texture_draw(float uv[2][2], float dx, float y1, float dx1, float y2)
{
	glBegin(GL_QUADS);
	glTexCoord2f(uv[0][0], uv[0][1]);
	glVertex2f(dx, y1);
	
	glTexCoord2f(uv[0][0], uv[1][1]);
	glVertex2f(dx, y2);
	
	glTexCoord2f(uv[1][0], uv[1][1]);
	glVertex2f(dx1, y2);
	
	glTexCoord2f(uv[1][0], uv[0][1]);
	glVertex2f(dx1, y1);
	glEnd();
}

static void blf_texture5_draw(const float shadow_col[4], float uv[2][2], float x1, float y1, float x2, float y2)
{
	const float soft[25] = {1 / 60.0f, 1 / 60.0f, 2 / 60.0f, 1 / 60.0f, 1 / 60.0f,
	                        1 / 60.0f, 3 / 60.0f, 5 / 60.0f, 3 / 60.0f, 1 / 60.0f,
	                        2 / 60.0f, 5 / 60.0f, 8 / 60.0f, 5 / 60.0f, 2 / 60.0f,
	                        1 / 60.0f, 3 / 60.0f, 5 / 60.0f, 3 / 60.0f, 1 / 60.0f,
	                        1 / 60.0f, 1 / 60.0f, 2 / 60.0f, 1 / 60.0f, 1 / 60.0f};
	
	const float *fp = soft;
	float color[4];
	float dx, dy;

	color[0] = shadow_col[0];
	color[1] = shadow_col[1];
	color[2] = shadow_col[2];
	
	for (dx = -2; dx < 3; dx++) {
		for (dy = -2; dy < 3; dy++, fp++) {
			color[3] = *(fp) * shadow_col[3];
			glColor4fv(color);
			blf_texture_draw(uv, x1 + dx, y1 + dy, x2 + dx, y2 + dy);
		}
	}
	
	glColor4fv(color);
}

static void blf_texture3_draw(const float shadow_col[4], float uv[2][2], float x1, float y1, float x2, float y2)
{
	const float soft[9] = {1 / 16.0f, 2 / 16.0f, 1 / 16.0f,
	                       2 / 16.0f, 4 / 16.0f, 2 / 16.0f,
	                       1 / 16.0f, 2 / 16.0f, 1 / 16.0f};

	const float *fp = soft;
	float color[4];
	float dx, dy;

	color[0] = shadow_col[0];
	color[1] = shadow_col[1];
	color[2] = shadow_col[2];

	for (dx = -1; dx < 2; dx++) {
		for (dy = -1; dy < 2; dy++, fp++) {
			color[3] = *(fp) * shadow_col[3];
			glColor4fv(color);
			blf_texture_draw(uv, x1 + dx, y1 + dy, x2 + dx, y2 + dy);
		}
	}
	
	glColor4fv(color);
}

static void blf_glyph_calc_rect(rctf *rect, GlyphBLF *g, float x, float y)
{
	rect->xmin = (float)floor(x + g->pos_x);
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

		if (font->max_tex_size == -1)
			glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&font->max_tex_size);

		if (gc->cur_tex == BLF_CURTEX_UNSET) {
			blf_glyph_cache_texture(font, gc);
			gc->x_offs = gc->pad;
			gc->y_offs = 0;
		}

		if (gc->x_offs > (gc->p2_width - gc->max_glyph_width)) {
			gc->x_offs = gc->pad;
			gc->y_offs += gc->max_glyph_height;

			if (gc->y_offs > (gc->p2_height - gc->max_glyph_height)) {
				gc->y_offs = 0;
				blf_glyph_cache_texture(font, gc);
			}
		}

		g->tex = gc->textures[gc->cur_tex];
		g->xoff = gc->x_offs;
		g->yoff = gc->y_offs;

		/* prevent glTexSubImage2D from failing if the character
		 * asks for pixels out of bounds, this tends only to happen
		 * with very small sizes (5px high or less) */
		if (UNLIKELY((g->xoff + g->width)  > gc->p2_width)) {
			g->width  -= (g->xoff + g->width)  - gc->p2_width;
			BLI_assert(g->width > 0);
		}
		if (UNLIKELY((g->yoff + g->height) > gc->p2_height)) {
			g->height -= (g->yoff + g->height) - gc->p2_height;
			BLI_assert(g->height > 0);
		}


		glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
		glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glBindTexture(GL_TEXTURE_2D, g->tex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, g->xoff, g->yoff, g->width, g->height, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap);
		glPopClientAttrib();

		g->uv[0][0] = ((float)g->xoff) / ((float)gc->p2_width);
		g->uv[0][1] = ((float)g->yoff) / ((float)gc->p2_height);
		g->uv[1][0] = ((float)(g->xoff + g->width)) / ((float)gc->p2_width);
		g->uv[1][1] = ((float)(g->yoff + g->height)) / ((float)gc->p2_height);

		/* update the x offset for the next glyph. */
		gc->x_offs += (int)BLI_rctf_size_x(&g->box) + gc->pad;

		gc->rem_glyphs--;
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
		glBindTexture(GL_TEXTURE_2D, (font->tex_bind_state = g->tex));
	}

	if (font->flags & BLF_SHADOW) {
		rctf rect_ofs;
		blf_glyph_calc_rect(&rect_ofs, g,
		                    x + (float)font->shadow_x,
		                    y + (float)font->shadow_y);

		switch (font->shadow) {
			case 3:
				blf_texture3_draw(font->shadow_col, g->uv, rect_ofs.xmin, rect_ofs.ymin, rect_ofs.xmax, rect_ofs.ymax);
				break;
			case 5:
				blf_texture5_draw(font->shadow_col, g->uv, rect_ofs.xmin, rect_ofs.ymin, rect_ofs.xmax, rect_ofs.ymax);
				break;
			default:
				glColor4fv(font->shadow_col);
				blf_texture_draw(g->uv, rect_ofs.xmin, rect_ofs.ymin, rect_ofs.xmax, rect_ofs.ymax);
				break;
		}

		glColor4fv(font->orig_col);
	}

	switch (font->blur) {
		case 3:
			blf_texture3_draw(font->orig_col, g->uv, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
			break;
		case 5:
			blf_texture5_draw(font->orig_col, g->uv, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
			break;
		default:
			blf_texture_draw(g->uv, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
			break;
	}

	return;
}
