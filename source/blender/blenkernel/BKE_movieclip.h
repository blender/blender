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
  * The Original Code is Copyright (C) 2011 Blender Foundation.
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
struct MovieClipScopes;
struct MovieClipUser;
struct MovieTrackingTrack;
struct MovieDistortion;

void free_movieclip(struct MovieClip *clip);
void unlink_movieclip(struct Main *bmain, struct MovieClip *clip);

struct MovieClip *BKE_add_movieclip_file(const char *name);
void BKE_movieclip_reload(struct MovieClip *clip);

struct ImBuf *BKE_movieclip_get_ibuf(struct MovieClip *clip, struct MovieClipUser *user);
struct ImBuf *BKE_movieclip_get_stable_ibuf(struct MovieClip *clip, struct MovieClipUser *user, float loc[2], float *scale, float *angle);
struct ImBuf *BKE_movieclip_get_ibuf_flag(struct MovieClip *clip, struct MovieClipUser *user, int flag);
void BKE_movieclip_get_size(struct MovieClip *clip, struct MovieClipUser *user, int *width, int *height);
void BKE_movieclip_aspect(struct MovieClip *clip, float *aspx, float *aspy);
int BKE_movieclip_has_frame(struct MovieClip *clip, struct MovieClipUser *user);
void BKE_movieclip_user_set_frame(struct MovieClipUser *user, int framenr);

void BKE_movieclip_select_track(struct MovieClip *clip, struct MovieTrackingTrack *track, int area, int extend);

void BKE_movieclip_update_scopes(struct MovieClip *clip, struct MovieClipUser *user, struct MovieClipScopes *scopes);

void BKE_movieclip_get_cache_segments(struct MovieClip *clip, struct MovieClipUser *user, int *totseg_r, int **points_r);

void BKE_movieclip_build_proxy_frame(struct MovieClip *clip, int clip_flag, struct MovieDistortion *distortion,
			int cfra, int *build_sizes, int build_count, int undistorted);

#define TRACK_CLEAR_UPTO		0
#define TRACK_CLEAR_REMAINED	1
#define TRACK_CLEAR_ALL			2

#endif
