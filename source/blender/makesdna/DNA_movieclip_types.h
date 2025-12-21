/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_color_types.h"
#include "DNA_tracking_types.h"

struct AnimData;
struct ImBuf;
struct MovieReader;
struct MovieClipProxy;
struct MovieTrackingMarker;
struct MovieTrackingTrack;
struct bGPdata;
#ifdef __cplusplus
namespace blender::gpu {
class Texture;
}  // namespace blender::gpu
using GPUTexture = blender::gpu::Texture;
#else
struct GPUTexture;
#endif

/** #MovieClipProxy.build_size_flag
 * NOTE: Keep in sync with #IMB_Proxy_Size. */
enum {
  MCLIP_PROXY_SIZE_25 = (1 << 0),
  MCLIP_PROXY_SIZE_50 = (1 << 1),
  MCLIP_PROXY_SIZE_75 = (1 << 2),
  MCLIP_PROXY_SIZE_100 = (1 << 3),
  MCLIP_PROXY_UNDISTORTED_SIZE_25 = (1 << 4),
  MCLIP_PROXY_UNDISTORTED_SIZE_50 = (1 << 5),
  MCLIP_PROXY_UNDISTORTED_SIZE_75 = (1 << 6),
  MCLIP_PROXY_UNDISTORTED_SIZE_100 = (1 << 7),
};

/** #MovieClipProxy.build_tc_flag
 * NOTE: Keep in sync with #IMB_Timecode_Type. */
enum {
  MCLIP_TC_RECORD_RUN = 1,
  MCLIP_TC_RECORD_RUN_NO_GAPS = 8,
};

enum MovieClipSource {
  MCLIP_SRC_SEQUENCE = 1,
  MCLIP_SRC_MOVIE = 2,
};

enum MovieClipFlag {
  MCLIP_USE_PROXY = (1 << 0),
  MCLIP_USE_PROXY_CUSTOM_DIR = (1 << 1),
  /* MCLIP_CUSTOM_START_FRAME    = (1 << 2), */ /* UNUSED */
  MCLIP_DATA_EXPAND = (1 << 3),

  MCLIP_TIMECODE_FLAGS = (MCLIP_USE_PROXY | MCLIP_USE_PROXY_CUSTOM_DIR),
};

/** #MovieClip.render_size */
enum {
  MCLIP_PROXY_RENDER_SIZE_FULL = 0,
  MCLIP_PROXY_RENDER_SIZE_25 = 1,
  MCLIP_PROXY_RENDER_SIZE_50 = 2,
  MCLIP_PROXY_RENDER_SIZE_75 = 3,
  MCLIP_PROXY_RENDER_SIZE_100 = 4,
};

/** #MovieClip.render_flag */
enum {
  MCLIP_PROXY_RENDER_UNDISTORT = 1,
  /** Use original, if proxy is not found. */
  MCLIP_PROXY_RENDER_USE_FALLBACK_RENDER = 2,
};

struct MovieClipUser {
  /** Current frame number. */
  int framenr = 1;
  /** Proxy render size. */
  short render_size = MCLIP_PROXY_RENDER_SIZE_FULL, render_flag = 0;
};

struct MovieClipProxy {
  /** Custom directory for index and proxy files (defaults to "BL_proxy"). */
  char dir[/*FILE_MAXDIR*/ 768] = "";

  /** Time code in use. */
  short tc = 0;
  /** Proxy build quality. */
  short quality = 50;
  /** Size flags (see below) of all proxies to build. */
  short build_size_flag = MCLIP_PROXY_SIZE_25;
  /** Time code flags (see below) of all tc indices to build. */
  short build_tc_flag = MCLIP_TC_RECORD_RUN | MCLIP_TC_RECORD_RUN_NO_GAPS;
};

struct MovieClip_RuntimeGPUTexture {
  void *next = nullptr, *prev = nullptr;
  MovieClipUser user;
  /** Not written in file. */
  GPUTexture *gputexture[/*TEXTARGET_COUNT*/ 3];
};

struct MovieClip_Runtime {
  ListBaseT<MovieClip_RuntimeGPUTexture> gputextures = {nullptr, nullptr};
  /* The Depsgraph::update_count when this ID was last updated. Covers any IDRecalcFlag. */
  uint64_t last_update = 0;
};

struct MovieClip {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_MC;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt = nullptr;

  char filepath[/*FILE_MAX*/ 1024] = "";

  int source = 0; /* MovieClipSource */
  int _pad = {};
  /** Size of last accessed frame. */
  int lastsize[2] = {};

  /** Display aspect. */
  float aspx = 1.0f, aspy = 1.0f;

  /** Movie source data. */
  struct MovieReader *anim = nullptr;
  /** Cache for different stuff, not in file. */
  struct MovieClipCache *cache = nullptr;
  /** Grease pencil data. */
  struct bGPdata *gpd = nullptr;

  /** Data for SfM tracking. */
  struct MovieTracking tracking;
  /**
   * Context of tracking job used to synchronize data
   * like frame-number in SpaceClip clip user.
   */
  void *tracking_context = nullptr;

  /** Proxy to clip data. */
  struct MovieClipProxy proxy;
  int flag = 0; /* MovieClipFlag */

  /** Length of movie. */
  int len = 0;

  /**
   * Scene frame number footage starts playing at affects all data
   * which is associated with a clip such as motion tracking,
   * camera Reconstruction and so.
   */
  int start_frame = 1;
  /**
   * Offset which is adding to a file number when reading frame from a file.
   * affects only a way how scene frame is mapping to a file name and not
   * touches other data associated with a clip. */
  int frame_offset = 0;

  /* color management */
  ColorManagedColorspaceSettings colorspace_settings;

  struct MovieClip_Runtime runtime;
};

struct MovieClipScopes {
  /** 1 means scopes are ok and recalculation is unneeded. */
  short ok = 0;
  /** Whether track's mask should be applied on preview. */
  short use_track_mask = 0;
  /** Height of track preview widget. */
  int track_preview_height = 120;
  /** Width and height of frame for which scopes are calculated. */
  int frame_width = 0, frame_height = 0;
  /** Undistorted position of marker used for pattern sampling. */
  struct MovieTrackingMarker undist_marker;
  /** Search area of a track. */
  struct ImBuf *track_search = nullptr;
  /** #ImBuf displayed in track preview. */
  struct ImBuf *track_preview = nullptr;
  /** Sub-pixel position of marker in track ImBuf. */
  float track_pos[2] = {0, 0};
  /** Active track is disabled, special notifier should be drawn. */
  short track_disabled = 0;
  /** Active track is locked, no transformation should be allowed. */
  short track_locked = 0;
  /** Frame number scopes are created for (measured in scene frames). */
  int scene_framenr = 0;
  /** Track scopes are created for. */
  struct MovieTrackingTrack *track = nullptr;
  /** Marker scopes are created for. */
  struct MovieTrackingMarker *marker = nullptr;
  /** Scale used for sliding from preview area. */
  float slide_scale[2] = {0.0f, 0.0f};
};
