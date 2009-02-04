/**
 * $Id:
 *
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_IMAGE_H
#define ED_IMAGE_H

struct SpaceImage;
struct bContext;

/* space_image.c, exported for transform */
struct Image *ED_space_image(struct SpaceImage *sima);
void ED_space_image_size(struct SpaceImage *sima, int *width, int *height);
void ED_space_image_uv_aspect(struct SpaceImage *sima, float *aspx, float *aspy);

/* image_render.c, export for screen_ops.c, render operator */
void ED_space_image_output(struct bContext *C);

#endif /* ED_IMAGE_H */

