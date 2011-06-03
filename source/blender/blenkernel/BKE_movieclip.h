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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_MOVIECLIP_H
#define BKE_MOVIECLIP_H

/** \file BKE_movieclip.h
 *  \ingroup bke
 *  \author Sergey Sharybin
 */

struct ImBuf;
struct Main;
struct MovieClip;
struct MovieClipUser;
struct MovieTrackingMarker;

void free_movieclip(struct MovieClip *clip);
void unlink_movieclip(struct Main *bmain, struct MovieClip *clip);

struct MovieClip *BKE_add_movieclip_file(const char *name);
void BKE_movieclip_reload(struct MovieClip *clip);

struct ImBuf *BKE_movieclip_acquire_ibuf(struct MovieClip *clip, struct MovieClipUser *user);
int BKE_movieclip_has_frame(struct MovieClip *clip, struct MovieClipUser *user);
void BKE_movieclip_user_set_frame(struct MovieClipUser *user, int framenr);

void BKE_movieclip_select_marker(struct MovieClip *clip, struct MovieTrackingMarker *marker, int area, int extend);
void BKE_movieclip_deselect_marker(struct MovieClip *clip, struct MovieTrackingMarker *marker, int area);
void BKE_movieclip_set_selection(struct MovieClip *clip, int type, void *sel);
void BKE_movieclip_last_selection(struct MovieClip *clip, int *type, void **sel);

void BKE_movieclip_get_cache_segments(struct MovieClip *clip, int *totseg_r, int **points_r);

#endif
