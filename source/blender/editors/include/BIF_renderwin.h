/**
 * $Id: BIF_renderwin.h 9376 2006-12-20 17:57:56Z ton $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

struct Render;
struct ScrArea;
struct RenderStats;

void calc_renderwin_rectangle(int rectx, int recty, int posmask, int renderpos_r[2], int rendersize_r[2]);

void BIF_close_render_display(void);

void BIF_do_render(int anim);

/**
 * @param v3d The View3D space to render.
 */
void BIF_do_ogl_render(struct View3D *v3d, int anim);

void BIF_renderwin_set_for_ogl_render(void);
void BIF_renderwin_set_custom_cursor(unsigned char mask[16][2], unsigned char bitmap[16][2]);

void BIF_redraw_render_rect(void);
void BIF_swap_render_rects(void);
void BIF_store_spare(void);
void BIF_toggle_render_display(void);

void BIF_init_render_callbacks(struct Render *re, int do_display);
void BIF_end_render_callbacks(void);

/* should not be here, ~WIP~ */
void make_renderinfo_string(struct RenderStats *rs, char *str);

/* space for info text */
#define RW_HEADERY		18

/* header print for window */
#define RW_MAXTEXT		512

