/*
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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#ifndef __TRACKING_OPS_INTERN_H__
#define __TRACKING_OPS_INTERN_H__

struct ListBase;
struct MovieClip;
struct SpaceClip;
struct bContext;

/* tracking_utils.c */

void clip_tracking_clear_invisible_track_selection(struct SpaceClip *sc, struct MovieClip *clip);

void clip_tracking_show_cursor(struct bContext *C);
void clip_tracking_hide_cursor(struct bContext *C);

/* tracking_select.h */

void ed_tracking_deselect_all_tracks(struct ListBase *tracks_base);
void ed_tracking_deselect_all_plane_tracks(struct ListBase *plane_tracks_base);

#endif /* __TRACKING_OPS_INTERN_H__ */
