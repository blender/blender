/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_color_types.h"    /* for color management */
#include "DNA_tracking_types.h" /* for #MovieTracking */

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
typedef struct GPUTexture GPUTexture;
#endif

typedef struct MovieClipUser {
  /** Current frame number. */
  int framenr;
  /** Proxy render size. */
  short render_size, render_flag;
} MovieClipUser;

typedef struct MovieClipProxy {
  /** Custom directory for index and proxy files (defaults to "BL_proxy"). */
  char dir[/*FILE_MAXDIR*/ 768];

  /** Time code in use. */
  short tc;
  /** Proxy build quality. */
  short quality;
  /** Size flags (see below) of all proxies to build. */
  short build_size_flag;
  /** Time code flags (see below) of all tc indices to build. */
  short build_tc_flag;
} MovieClipProxy;

typedef struct MovieClip_RuntimeGPUTexture {
  void *next, *prev;
  MovieClipUser user;
  /** Not written in file. */
  GPUTexture *gputexture[/*TEXTARGET_COUNT*/ 3];
} MovieClip_RuntimeGPUTexture;

typedef struct MovieClip_Runtime {
  struct ListBase gputextures;
  /* The Depsgraph::update_count when this ID was last updated. Covers any IDRecalcFlag. */
  uint64_t last_update;
} MovieClip_Runtime;

typedef struct MovieClip {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_MC;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  char filepath[/*FILE_MAX*/ 1024];

  /** Sequence or movie. */
  int source;
  int _pad;
  /** Size of last accessed frame. */
  int lastsize[2];

  /** Display aspect. */
  float aspx, aspy;

  /** Movie source data. */
  struct MovieReader *anim;
  /** Cache for different stuff, not in file. */
  struct MovieClipCache *cache;
  /** Grease pencil data. */
  struct bGPdata *gpd;

  /** Data for SfM tracking. */
  struct MovieTracking tracking;
  /**
   * Context of tracking job used to synchronize data
   * like frame-number in SpaceClip clip user.
   */
  void *tracking_context;

  /** Proxy to clip data. */
  struct MovieClipProxy proxy;
  int flag;

  /** Length of movie. */
  int len;

  /**
   * Scene frame number footage starts playing at affects all data
   * which is associated with a clip such as motion tracking,
   * camera Reconstruction and so.
   */
  int start_frame;
  /**
   * Offset which is adding to a file number when reading frame from a file.
   * affects only a way how scene frame is mapping to a file name and not
   * touches other data associated with a clip. */
  int frame_offset;

  /* color management */
  ColorManagedColorspaceSettings colorspace_settings;

  struct MovieClip_Runtime runtime;
} MovieClip;

typedef struct MovieClipScopes {
  /** 1 means scopes are ok and recalculation is unneeded. */
  short ok;
  /** Whether track's mask should be applied on preview. */
  short use_track_mask;
  /** Height of track preview widget. */
  int track_preview_height;
  /** Width and height of frame for which scopes are calculated. */
  int frame_width, frame_height;
  /** Undistorted position of marker used for pattern sampling. */
  struct MovieTrackingMarker undist_marker;
  /** Search area of a track. */
  struct ImBuf *track_search;
  /** #ImBuf displayed in track preview. */
  struct ImBuf *track_preview;
  /** Sub-pixel position of marker in track ImBuf. */
  float track_pos[2];
  /** Active track is disabled, special notifier should be drawn. */
  short track_disabled;
  /** Active track is locked, no transformation should be allowed. */
  short track_locked;
  /** Frame number scopes are created for (measured in scene frames). */
  int scene_framenr;
  /** Track scopes are created for. */
  struct MovieTrackingTrack *track;
  /** Marker scopes are created for. */
  struct MovieTrackingMarker *marker;
  /** Scale used for sliding from preview area. */
  float slide_scale[2];
} MovieClipScopes;

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

/** #MovieClip.source */
enum {
  MCLIP_SRC_SEQUENCE = 1,
  MCLIP_SRC_MOVIE = 2,
};

/** #MovieClip.flag */
enum {
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
