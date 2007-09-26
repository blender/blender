/**
 * $Id$
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

#ifndef BIF_DRAWIMAGE_H
#define BIF_DRAWIMAGE_H

struct ScrArea;
struct SpaceImage;
struct Render;
struct Image;
struct ImBuf;
struct uiBlock;

void do_imagebuts(unsigned short event);
void calc_image_view(struct SpaceImage *sima, char mode);
void drawimagespace(struct ScrArea *sa, void *spacedata);
void image_changed(struct SpaceImage *sima, struct Image *image);
void draw_uvs_sima(void);
void image_set_tile(struct SpaceImage *sima, int dotile);
void image_home(void);
void image_viewmove(int mode);
void image_viewzoom(unsigned short event, int invert);
void image_viewcenter(void);
void uvco_to_areaco(float *vec, short *mval);
void uvco_to_areaco_noclip(float *vec, int *mval);
void what_image(struct SpaceImage *sima);
void image_preview_event(int event);

void image_info(struct Image *ima, struct ImBuf *ibuf, char *str);
void imagespace_composite_flipbook(struct ScrArea *sa);

void imagewindow_render_callbacks(struct Render *re);
void imagewindow_toggle_render(void);
void imagewindow_swap_render_rects(void);
void imagewin_store_spare(void);
struct ImBuf *imagewindow_get_ibuf(struct SpaceImage *sima);

void image_editvertex_buts(struct uiBlock *block);
void image_editcursor_buts(struct uiBlock *block);

#endif

