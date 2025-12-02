/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_enum_flags.hh"
#include "DNA_movieclip_types.h"

struct Depsgraph;
struct ImBuf;
struct Main;
struct MovieDistortion;

namespace blender::gpu {
class Texture;
}  // namespace blender::gpu

enum class MovieClipCacheFlag {
  None = 0,
  SkipCache = 1 << 0,
};
ENUM_OPERATORS(MovieClipCacheFlag);

/* Note: do not change values; DNA data #SpaceClip.postproc_flag uses this. */
enum class MovieClipPostprocFlag {
  None = 0,
  DisableRed = (1 << 0),
  DisableGreen = (1 << 1),
  DisableBlue = (1 << 2),
  PreviewGray = (1 << 3),
};
ENUM_OPERATORS(MovieClipPostprocFlag);

/**
 * Checks if image was already loaded, then returns same image otherwise creates new.
 * does not load ibuf itself pass on optional frame for `filepath` images.
 */
MovieClip *BKE_movieclip_file_add(Main *bmain, const char *filepath);
MovieClip *BKE_movieclip_file_add_exists_ex(Main *bmain, const char *filepath, bool *r_exists);
MovieClip *BKE_movieclip_file_add_exists(Main *bmain, const char *filepath);
void BKE_movieclip_reload(Main *bmain, MovieClip *clip);
void BKE_movieclip_clear_cache(MovieClip *clip);
void BKE_movieclip_clear_proxy_cache(MovieClip *clip);

/**
 * Will try to make image buffer usable when originating from the multi-layer source.
 * Internally finds a first combined pass and uses that as a buffer.
 * Not ideal, but is better than a complete empty buffer.
 */
void BKE_movieclip_convert_multilayer_ibuf(ImBuf *ibuf);

ImBuf *BKE_movieclip_get_ibuf(MovieClip *clip, const MovieClipUser *user);
ImBuf *BKE_movieclip_get_postprocessed_ibuf(MovieClip *clip,
                                            const MovieClipUser *user,
                                            MovieClipPostprocFlag postprocess_flag);
ImBuf *BKE_movieclip_get_stable_ibuf(MovieClip *clip,
                                     const MovieClipUser *user,
                                     MovieClipPostprocFlag postprocess_flag,
                                     float r_loc[2],
                                     float *r_scale,
                                     float *r_angle);
ImBuf *BKE_movieclip_get_ibuf_flag(MovieClip *clip,
                                   const MovieClipUser *user,
                                   MovieClipFlag flag,
                                   MovieClipCacheFlag cache_flag);
void BKE_movieclip_get_size(MovieClip *clip,
                            const MovieClipUser *user,
                            int *r_width,
                            int *r_height);
void BKE_movieclip_get_size_fl(MovieClip *clip, const MovieClipUser *user, float r_size[2]);
int BKE_movieclip_get_duration(MovieClip *clip);
float BKE_movieclip_get_fps(MovieClip *clip);
void BKE_movieclip_get_aspect(MovieClip *clip, float *aspx, float *aspy);
bool BKE_movieclip_has_frame(MovieClip *clip, const MovieClipUser *user);
void BKE_movieclip_user_set_frame(MovieClipUser *user, int framenr);

void BKE_movieclip_update_scopes(MovieClip *clip,
                                 const MovieClipUser *user,
                                 MovieClipScopes *scopes);

/**
 * Get segments of cached frames. useful for debugging cache policies.
 */
void BKE_movieclip_get_cache_segments(MovieClip *clip,
                                      const MovieClipUser *user,
                                      int *r_totseg,
                                      int **r_points);

/**
 * \note currently used by proxy job for movies, threading happens within single frame
 * (meaning scaling shall be threaded).
 */
void BKE_movieclip_build_proxy_frame(MovieClip *clip,
                                     MovieClipFlag clip_flag,
                                     MovieDistortion *distortion,
                                     int cfra,
                                     const int *build_sizes,
                                     int build_count,
                                     bool undistorted);

/**
 * \note currently used by proxy job for sequences, threading happens within sequence
 * (different threads handles different frames, no threading within frame is needed)
 */
void BKE_movieclip_build_proxy_frame_for_ibuf(MovieClip *clip,
                                              ImBuf *ibuf,
                                              MovieDistortion *distortion,
                                              int cfra,
                                              const int *build_sizes,
                                              int build_count,
                                              bool undistorted);
bool BKE_movieclip_proxy_enabled(MovieClip *clip);

float BKE_movieclip_remap_scene_to_clip_frame(const MovieClip *clip, float framenr);
float BKE_movieclip_remap_clip_to_scene_frame(const MovieClip *clip, float framenr);

void BKE_movieclip_filepath_for_frame(MovieClip *clip, const MovieClipUser *user, char *filepath);

/**
 * Read image buffer from the given movie clip without acquiring the #LOCK_MOVIECLIP lock.
 * Used by a prefetch job which takes care of creating a local copy of the clip.
 */
ImBuf *BKE_movieclip_anim_ibuf_for_frame_no_lock(MovieClip *clip, const MovieClipUser *user);

bool BKE_movieclip_has_cached_frame(MovieClip *clip, const MovieClipUser *user);
bool BKE_movieclip_put_frame_if_possible(MovieClip *clip, const MovieClipUser *user, ImBuf *ibuf);

blender::gpu::Texture *BKE_movieclip_get_gpu_texture(MovieClip *clip, MovieClipUser *cuser);

void BKE_movieclip_free_gputexture(MovieClip *clip);

/* Dependency graph evaluation. */

void BKE_movieclip_eval_update(Depsgraph *depsgraph, Main *bmain, MovieClip *clip);
