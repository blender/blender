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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

#ifndef __BKE_MOVIECLIP_H__
#define __BKE_MOVIECLIP_H__

/** \file
 * \ingroup bke
 */

struct Depsgraph;
struct ImBuf;
struct Main;
struct MovieClip;
struct MovieClipScopes;
struct MovieClipUser;
struct MovieDistortion;

void BKE_movieclip_free(struct MovieClip *clip);

void BKE_movieclip_copy_data(struct Main *bmain,
                             struct MovieClip *clip_dst,
                             const struct MovieClip *clip_src,
                             const int flag);
struct MovieClip *BKE_movieclip_copy(struct Main *bmain, const struct MovieClip *clip);
void BKE_movieclip_make_local(struct Main *bmain, struct MovieClip *clip, const bool lib_local);

struct MovieClip *BKE_movieclip_file_add(struct Main *bmain, const char *name);
struct MovieClip *BKE_movieclip_file_add_exists_ex(struct Main *bmain,
                                                   const char *name,
                                                   bool *r_exists);
struct MovieClip *BKE_movieclip_file_add_exists(struct Main *bmain, const char *name);
void BKE_movieclip_reload(struct Main *bmain, struct MovieClip *clip);
void BKE_movieclip_clear_cache(struct MovieClip *clip);
void BKE_movieclip_clear_proxy_cache(struct MovieClip *clip);

void BKE_movieclip_convert_multilayer_ibuf(struct ImBuf *ibuf);

struct ImBuf *BKE_movieclip_get_ibuf(struct MovieClip *clip, struct MovieClipUser *user);
struct ImBuf *BKE_movieclip_get_postprocessed_ibuf(struct MovieClip *clip,
                                                   struct MovieClipUser *user,
                                                   int postprocess_flag);
struct ImBuf *BKE_movieclip_get_stable_ibuf(struct MovieClip *clip,
                                            struct MovieClipUser *user,
                                            float loc[2],
                                            float *scale,
                                            float *angle,
                                            int postprocess_flag);
struct ImBuf *BKE_movieclip_get_ibuf_flag(struct MovieClip *clip,
                                          struct MovieClipUser *user,
                                          int flag,
                                          int cache_flag);
void BKE_movieclip_get_size(struct MovieClip *clip,
                            struct MovieClipUser *user,
                            int *width,
                            int *height);
void BKE_movieclip_get_size_fl(struct MovieClip *clip, struct MovieClipUser *user, float size[2]);
int BKE_movieclip_get_duration(struct MovieClip *clip);
float BKE_movieclip_get_fps(struct MovieClip *clip);
void BKE_movieclip_get_aspect(struct MovieClip *clip, float *aspx, float *aspy);
bool BKE_movieclip_has_frame(struct MovieClip *clip, struct MovieClipUser *user);
void BKE_movieclip_user_set_frame(struct MovieClipUser *user, int framenr);

void BKE_movieclip_update_scopes(struct MovieClip *clip,
                                 struct MovieClipUser *user,
                                 struct MovieClipScopes *scopes);

void BKE_movieclip_get_cache_segments(struct MovieClip *clip,
                                      struct MovieClipUser *user,
                                      int *r_totseg,
                                      int **r_points);

void BKE_movieclip_build_proxy_frame(struct MovieClip *clip,
                                     int clip_flag,
                                     struct MovieDistortion *distortion,
                                     int cfra,
                                     int *build_sizes,
                                     int build_count,
                                     bool undistorted);

void BKE_movieclip_build_proxy_frame_for_ibuf(struct MovieClip *clip,
                                              struct ImBuf *ibuf,
                                              struct MovieDistortion *distortion,
                                              int cfra,
                                              int *build_sizes,
                                              int build_count,
                                              bool undistorted);

float BKE_movieclip_remap_scene_to_clip_frame(const struct MovieClip *clip, float framenr);
float BKE_movieclip_remap_clip_to_scene_frame(const struct MovieClip *clip, float framenr);

void BKE_movieclip_filename_for_frame(struct MovieClip *clip,
                                      struct MovieClipUser *user,
                                      char *name);
struct ImBuf *BKE_movieclip_anim_ibuf_for_frame(struct MovieClip *clip,
                                                struct MovieClipUser *user);

bool BKE_movieclip_has_cached_frame(struct MovieClip *clip, struct MovieClipUser *user);
bool BKE_movieclip_put_frame_if_possible(struct MovieClip *clip,
                                         struct MovieClipUser *user,
                                         struct ImBuf *ibuf);

/* Evaluation. */
void BKE_movieclip_eval_update(struct Depsgraph *depsgraph, struct MovieClip *clip);
void BKE_movieclip_eval_selection_update(struct Depsgraph *depsgraph, struct MovieClip *clip);

/* caching flags */
#define MOVIECLIP_CACHE_SKIP (1 << 0)

/* postprocessing flags */
#define MOVIECLIP_DISABLE_RED (1 << 0)
#define MOVIECLIP_DISABLE_GREEN (1 << 1)
#define MOVIECLIP_DISABLE_BLUE (1 << 2)
#define MOVIECLIP_PREVIEW_GRAYSCALE (1 << 3)

#endif
