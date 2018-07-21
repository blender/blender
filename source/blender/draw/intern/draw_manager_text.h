/*
 * Copyright 2016, Blender Foundation.
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
 */

/** \file blender/draw/intern/draw_manager_text.h
 *  \ingroup draw
 */

#ifndef __DRAW_MANAGER_TEXT_H__
#define __DRAW_MANAGER_TEXT_H__

struct DRWTextStore;

struct DRWTextStore *DRW_text_cache_create(void);
void DRW_text_cache_destroy(struct DRWTextStore *dt);

void DRW_text_cache_add(
        struct DRWTextStore *dt,
        const float co[3],
        const char *str, const int str_len,
        short xoffs, short flag,
        const uchar col[4]);

void DRW_text_cache_draw(struct DRWTextStore *dt, struct ARegion *ar);

enum {
	DRW_TEXT_CACHE_ASCII        = (1 << 0),
	DRW_TEXT_CACHE_GLOBALSPACE  = (1 << 1),
	DRW_TEXT_CACHE_LOCALCLIP    = (1 << 2),
	/* reference the string by pointer */
	DRW_TEXT_CACHE_STRING_PTR   = (1 << 3),
};

/* draw_manager.c */
struct DRWTextStore *DRW_text_cache_ensure(void);

#endif /* __DRAW_MANAGER_TEXT_H__ */
