/*
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_intern.h
 *  \ingroup spclip
 */


#ifndef ED_CLIP_INTERN_H
#define ED_CLIP_INTERN_H

struct ARegion;
struct Scene;
struct SpaceClip;
struct wmOperatorType;

/* internal exports only */

/* clip_ops.c */
void CLIP_OT_open(struct wmOperatorType *ot);
void CLIP_OT_reload(struct wmOperatorType *ot);
void CLIP_OT_tools(struct wmOperatorType *ot);
// void CLIP_OT_properties(struct wmOperatorType *ot);
// void CLIP_OT_unlink(struct wmOperatorType *ot);
void CLIP_OT_view_pan(struct wmOperatorType *ot);
void CLIP_OT_view_zoom(wmOperatorType *ot);
void CLIP_OT_view_zoom_in(struct wmOperatorType *ot);
void CLIP_OT_view_zoom_out(struct wmOperatorType *ot);
void CLIP_OT_view_zoom_ratio(struct wmOperatorType *ot);
void CLIP_OT_view_all(struct wmOperatorType *ot);

/* tracking_ops.c */
void CLIP_OT_select(struct wmOperatorType *ot);
void CLIP_OT_select_all(struct wmOperatorType *ot);
void CLIP_OT_select_border(struct wmOperatorType *ot);
void CLIP_OT_select_circle(struct wmOperatorType *ot);

void CLIP_OT_add_marker(struct wmOperatorType *ot);
void CLIP_OT_delete(struct wmOperatorType *ot);

void CLIP_OT_track_markers(struct wmOperatorType *ot);

/* clip_draw.c */
void draw_clip_main(struct SpaceClip *sc, struct ARegion *ar, struct Scene *scene);

/* clip_buttons.c */
void ED_clip_buttons_register(struct ARegionType *art);

#endif /* ED_CLIP_INTERN_H */

