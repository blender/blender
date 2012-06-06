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

#ifndef __BKE_MOVIECLIP_H__
#define __BKE_MOVIECLIP_H__

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

void BKE_movieclip_free(struct MovieClip *clip);
void BKE_movieclip_unlink(struct Main *bmain, struct MovieClip *clip);

struct MovieClip *BKE_movieclip_file_add(const char *name);
void BKE_movieclip_reload(struct MovieClip *clip);

struct ImBuf *BKE_movieclip_get_ibuf(struct MovieClip *clip, struct MovieClipUser *user);
struct ImBuf *BKE_movieclip_get_postprocessed_ibuf(struct MovieClip *clip, struct MovieClipUser *user, int postprocess_flag);
struct ImBuf *BKE_movieclip_get_stable_ibuf(struct MovieClip *clip, struct MovieClipUser *user, float loc[2], float *scale, float *angle, int postprocess_flag);
struct ImBuf *BKE_movieclip_get_ibuf_flag(struct MovieClip *clip, struct MovieClipUser *user, int flag, int cache_flag);
void BKE_movieclip_get_size(struct MovieClip *clip, struct MovieClipUser *user, int *width, int *height);
int BKE_movieclip_get_duration(struct MovieClip *clip);
void BKE_movieclip_aspect(struct MovieClip *clip, float *aspx, float *aspy);
int BKE_movieclip_has_frame(struct MovieClip *clip, struct MovieClipUser *user);
void BKE_movieclip_user_set_frame(struct MovieClipUser *user, int framenr);

void BKE_movieclip_update_scopes(struct MovieClip *clip, struct MovieClipUser *user, struct MovieClipScopes *scopes);

void BKE_movieclip_get_cache_segments(struct MovieClip *clip, struct MovieClipUser *user, int *totseg_r, int **points_r);

void BKE_movieclip_build_proxy_frame(struct MovieClip *clip, int clip_flag, struct MovieDistortion *distortion,
                                     int cfra, int *build_sizes, int build_count, int undistorted);

int BKE_movieclip_remap_scene_to_clip_frame(struct MovieClip *clip, int framenr);
int BKE_movieclip_remap_clip_to_scene_frame(struct MovieClip *clip, int framenr);

/* cacheing flags */
#define MOVIECLIP_CACHE_SKIP        (1 << 0)

/* postprocessing flags */
#define MOVIECLIP_DISABLE_RED       (1 << 0)
#define MOVIECLIP_DISABLE_GREEN     (1 << 1)
#define MOVIECLIP_DISABLE_BLUE      (1 << 2)
#define MOVIECLIP_PREVIEW_GRAYSCALE (1 << 3)

#endif
