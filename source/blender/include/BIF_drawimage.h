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

void calc_image_view(struct SpaceImage *sima, char mode);
void drawimagespace(struct ScrArea *sa, void *spacedata);
void draw_tfaces(void);
void image_changed(struct SpaceImage *sima, int dotile);
void image_home(void);
void image_viewmove(void);
void image_viewzoom(unsigned short event);
void rectwrite_part(int winxmin, int winymin, int winxmax, int winymax, int x1, int y1, int xim, int yim, float zoomx, float zoomy, unsigned int *rect);
void uvco_to_areaco(float *vec, short *mval);
void uvco_to_areaco_noclip(float *vec, short *mval);
void what_image(struct SpaceImage *sima);

#endif

