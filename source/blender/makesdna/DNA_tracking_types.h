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

/** \file
 * \ingroup DNA
 *
 * Structs used for camera tracking and the movie-clip editor.
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/* match-moving data */

struct Image;
struct MovieReconstructedCamera;
struct MovieTracking;
struct MovieTrackingCamera;
struct MovieTrackingMarker;
struct MovieTrackingTrack;
struct bGPdata;

typedef struct MovieReconstructedCamera {
  int framenr;
  float error;
  float mat[4][4];
} MovieReconstructedCamera;

typedef struct MovieTrackingCamera {
  /** Intrinsics handle. */
  void *intrinsics;

  short distortion_model;
  char _pad[2];

  /** Width of CCD sensor. */
  float sensor_width;
  /** Pixel aspect ratio. */
  float pixel_aspect;
  /** Focal length. */
  float focal;
  /** Units of focal length user is working with. */
  short units;
  char _pad1[2];
  /** Principal point. */
  float principal[2];

  /* Polynomial distortion */
  /** Polynomial radial distortion. */
  float k1, k2, k3;

  /* Division distortion model coefficients */
  float division_k1, division_k2;

  /* Nuke distortion model coefficients */
  float nuke_k1, nuke_k2;

  /* Brown-Conrady distortion model coefficients */
  /** Brown-Conrady radial distortion. */
  float brown_k1, brown_k2, brown_k3, brown_k4;
  /** Brown-Conrady tangential distortion. */
  float brown_p1, brown_p2;
} MovieTrackingCamera;

typedef struct MovieTrackingMarker {
  /** 2d position of marker on frame (in unified 0..1 space). */
  float pos[2];

  /* corners of pattern in the following order:
   *
   *       Y
   *       ^
   *       | (3) --- (2)
   *       |  |       |
   *       |  |       |
   *       |  |       |
   *       | (0) --- (1)
   *       +-------------> X
   *
   * the coordinates are stored relative to pos.
   */
  float pattern_corners[4][2];

  /* positions of left-bottom and right-top corners of search area (in unified 0..1 units,
   * relative to marker->pos
   */
  float search_min[2], search_max[2];

  /** Number of frame marker is associated with. */
  int framenr;
  /** Marker's flag (alive, ...). */
  int flag;
} MovieTrackingMarker;

typedef struct MovieTrackingTrack {
  struct MovieTrackingTrack *next, *prev;

  /** MAX_NAME. */
  char name[64];

  /* ** settings ** */

  /* positions of left-bottom and right-top corners of pattern (in unified 0..1 units,
   * relative to marker->pos)
   * moved to marker's corners since planar tracking implementation
   */
  float pat_min[2] DNA_DEPRECATED, pat_max[2] DNA_DEPRECATED;

  /* positions of left-bottom and right-top corners of search area (in unified 0..1 units,
   * relative to marker->pos
   * moved to marker since affine tracking implementation
   */
  float search_min[2] DNA_DEPRECATED, search_max[2] DNA_DEPRECATED;

  /** Offset to "parenting" point. */
  float offset[2];

  /* ** track ** */
  /** Count of markers in track. */
  int markersnr;
  /** Most recently used marker. */
  int _pad;
  /** Markers in track. */
  MovieTrackingMarker *markers;

  /* ** reconstruction data ** */
  /** Reconstructed position. */
  float bundle_pos[3];
  /** Average track reprojection error. */
  float error;

  /* ** UI editing ** */
  /** Flags (selection, ...). */
  int flag, pat_flag, search_flag;
  /** Custom color for track. */
  float color[3];

  /* ** control how tracking happens */
  /**
   * Number of frames to be tracked during single tracking session
   * (if TRACKING_FRAMES_LIMIT is set).
   */
  short frames_limit;
  /** Margin from frame boundaries. */
  short margin;
  /** Denotes which frame is used for the reference during tracking.
   * An enumerator of `eTrackFrameMatch`. */
  short pattern_match;

  /* tracking parameters */
  /** Model of the motion for this track. */
  short motion_model;
  /** Flags for the tracking algorithm (use brute, use ESM, use pyramid, etc. */
  int algorithm_flag;
  /** Minimal correlation which is still treated as successful tracking. */
  float minimum_correlation;

  /** Grease-pencil data. */
  struct bGPdata *gpd;

  /* Weight of this track.
   *
   * Weight defines how much the track affects on the final reconstruction,
   * usually gets animated in a way so when track has just appeared its
   * weight is zero and then it gets faded up.
   *
   * Used to prevent jumps of the camera when tracks are appearing or
   * disappearing.
   */
  float weight;

  /* track weight especially for 2D stabilization */
  float weight_stab;
} MovieTrackingTrack;

typedef struct MovieTrackingPlaneMarker {
  /* Corners of the plane in the following order:
   *
   *       Y
   *       ^
   *       | (3) --- (2)
   *       |  |       |
   *       |  |       |
   *       |  |       |
   *       | (0) --- (1)
   *       +-------------> X
   *
   * The coordinates are stored in frame normalized coordinates.
   */
  float corners[4][2];

  /** Number of frame plane marker is associated with. */
  int framenr;
  /** Marker's flag (alive, ...). */
  int flag;
} MovieTrackingPlaneMarker;

typedef struct MovieTrackingPlaneTrack {
  struct MovieTrackingPlaneTrack *next, *prev;

  /** MAX_NAME. */
  char name[64];

  /**
   * Array of point tracks used to define this plane.
   * Each element is a pointer to MovieTrackingTrack.
   */
  MovieTrackingTrack **point_tracks;
  /** Number of tracks in point_tracks array. */
  int point_tracksnr;
  char _pad[4];

  /** Markers in the plane track. */
  MovieTrackingPlaneMarker *markers;
  /** Count of markers in track (size of markers array). */
  int markersnr;

  /** Flags (selection, ...). */
  int flag;

  /** Image displaying during editing. */
  struct Image *image;
  /** Opacity of the image. */
  float image_opacity;

  /* Runtime data */
  /** Most recently used marker. */
  int last_marker;
} MovieTrackingPlaneTrack;

typedef struct MovieTrackingSettings {
  /* ** default tracker settings */
  /** Model of the motion for this track. */
  short default_motion_model;
  /** Flags for the tracking algorithm (use brute, use ESM, use pyramid, etc. */
  short default_algorithm_flag;
  /** Minimal correlation which is still treated as successful tracking. */
  float default_minimum_correlation;
  /** Size of pattern area for new tracks, measured in pixels. */
  short default_pattern_size;
  /** Size of search area for new tracks, measured in pixels. */
  short default_search_size;
  /** Number of frames to be tracked during single tracking session
   * (if TRACKING_FRAMES_LIMIT is set). */
  short default_frames_limit;
  /** Margin from frame boundaries. */
  short default_margin;
  /** Denotes which frame is used for the reference during tracking.
   * An enumerator of `eTrackFrameMatch`. */
  short default_pattern_match;
  /** Default flags like color channels used by default. */
  short default_flag;
  /** Default weight of the track. */
  float default_weight;

  /** Flags describes motion type. */
  short motion_flag;

  /* ** common tracker settings ** */
  /** Speed of tracking. */
  short speed;

  /* ** reconstruction settings ** */
  /* two keyframes for reconstruction initialization
   * were moved to per-tracking object settings
   */
  int keyframe1 DNA_DEPRECATED;
  int keyframe2 DNA_DEPRECATED;

  int reconstruction_flag;

  /* which camera intrinsics to refine. uses on the REFINE_* flags */
  int refine_camera_intrinsics;

  /* ** tool settings ** */

  /* set scale */
  /** Distance between two bundles used for scene scaling. */
  float dist;

  /* cleanup */
  int clean_frames, clean_action;
  float clean_error;

  /* set object scale */
  /** Distance between two bundles used for object scaling. */
  float object_distance;
} MovieTrackingSettings;

typedef struct MovieTrackingStabilization {
  int flag;
  /** Total number of translation tracks and index of active track in list. */
  int tot_track, act_track;
  /** Total number of rotation tracks and index of active track in list. */
  int tot_rot_track, act_rot_track;

  /* 2d stabilization */
  /** Max auto-scale factor. */
  float maxscale;
  /** Use TRACK_USE_2D_STAB_ROT on individual tracks instead. */
  MovieTrackingTrack *rot_track DNA_DEPRECATED;

  /** Reference point to anchor stabilization offset. */
  int anchor_frame;
  /** Expected target position of frame after raw stabilization, will be subtracted. */
  float target_pos[2];
  /** Expected target rotation of frame after raw stabilization, will be compensated. */
  float target_rot;
  /** Zoom factor known to be present on original footage. Also used for auto-scale. */
  float scale;

  /** Influence on location, scale and rotation. */
  float locinf, scaleinf, rotinf;

  /** Filter used for pixel interpolation. */
  int filter;

  /* initialization and run-time data */
  /** Without effect now, we initialize on every frame.
   * Formerly used for caching of init values. */
  int ok DNA_DEPRECATED;
} MovieTrackingStabilization;

typedef struct MovieTrackingReconstruction {
  int flag;

  /** Average error of reconstruction. */
  float error;

  /** Most recently used camera. */
  int last_camera;
  /** Number of reconstructed cameras. */
  int camnr;
  /** Reconstructed cameras. */
  struct MovieReconstructedCamera *cameras;
} MovieTrackingReconstruction;

typedef struct MovieTrackingObject {
  struct MovieTrackingObject *next, *prev;

  /** Name of tracking object, MAX_NAME. */
  char name[64];
  int flag;
  /** Scale of object solution in camera space. */
  float scale;

  /** List of tracks use to tracking this object. */
  ListBase tracks;
  /** List of plane tracks used by this object. */
  ListBase plane_tracks;
  /** Reconstruction data for this object. */
  MovieTrackingReconstruction reconstruction;

  /* reconstruction options */
  /** Two keyframes for reconstruction initialization. */
  int keyframe1, keyframe2;
} MovieTrackingObject;

typedef struct MovieTrackingStats {
  char message[256];
} MovieTrackingStats;

typedef struct MovieTrackingDopesheetChannel {
  struct MovieTrackingDopesheetChannel *next, *prev;

  /** Motion track for which channel is created. */
  MovieTrackingTrack *track;
  char _pad[4];

  /** Name of channel. */
  char name[64];

  /** Total number of segments. */
  int tot_segment;
  /** Tracked segments. */
  int *segments;
  /** Longest segment length and total number of tracked frames. */
  int max_segment, total_frames;
  /** These numbers are valid only if tot_segment > 0. */
  int first_not_disabled_marker_framenr, last_not_disabled_marker_framenr;
} MovieTrackingDopesheetChannel;

typedef struct MovieTrackingDopesheetCoverageSegment {
  struct MovieTrackingDopesheetCoverageSegment *next, *prev;

  int coverage;
  int start_frame;
  int end_frame;

  char _pad[4];
} MovieTrackingDopesheetCoverageSegment;

typedef struct MovieTrackingDopesheet {
  /** Flag if dopesheet information is still relevant. */
  int ok;

  /** Method to be used to sort tracks. */
  short sort_method;
  /** Dopesheet building flag such as inverted order of sort. */
  short flag;

  /* ** runtime stuff ** */

  /* summary */
  ListBase coverage_segments;

  /* detailed */
  ListBase channels;
  int tot_channel;

  char _pad[4];
} MovieTrackingDopesheet;

typedef struct MovieTracking {
  /** Different tracking-related settings. */
  MovieTrackingSettings settings;
  /** Camera intrinsics. */
  MovieTrackingCamera camera;
  /** List of tracks used for camera object. */
  ListBase tracks;
  /** List of plane tracks used by camera object. */
  ListBase plane_tracks;
  /** Reconstruction data for camera object. */
  MovieTrackingReconstruction reconstruction;
  /** Stabilization data. */
  MovieTrackingStabilization stabilization;
  /** Active track. */
  MovieTrackingTrack *act_track;
  /** Active plane track. */
  MovieTrackingPlaneTrack *act_plane_track;

  ListBase objects;
  /** Index of active object and total number of objects. */
  int objectnr, tot_object;

  /** Statistics displaying in clip editor. */
  MovieTrackingStats *stats;

  /** Dopesheet data. */
  MovieTrackingDopesheet dopesheet;
} MovieTracking;

/* MovieTrackingCamera->distortion_model */
enum {
  TRACKING_DISTORTION_MODEL_POLYNOMIAL = 0,
  TRACKING_DISTORTION_MODEL_DIVISION = 1,
  TRACKING_DISTORTION_MODEL_NUKE = 2,
  TRACKING_DISTORTION_MODEL_BROWN = 3,
};

/* MovieTrackingCamera->units */
enum {
  CAMERA_UNITS_PX = 0,
  CAMERA_UNITS_MM = 1,
};

/* MovieTrackingMarker->flag */
enum {
  MARKER_DISABLED = (1 << 0),
  MARKER_TRACKED = (1 << 1),
  MARKER_GRAPH_SEL_X = (1 << 2),
  MARKER_GRAPH_SEL_Y = (1 << 3),
  MARKER_GRAPH_SEL = (MARKER_GRAPH_SEL_X | MARKER_GRAPH_SEL_Y),
};

/* MovieTrackingTrack->flag */
enum {
  TRACK_HAS_BUNDLE = (1 << 1),
  TRACK_DISABLE_RED = (1 << 2),
  TRACK_DISABLE_GREEN = (1 << 3),
  TRACK_DISABLE_BLUE = (1 << 4),
  TRACK_HIDDEN = (1 << 5),
  TRACK_LOCKED = (1 << 6),
  TRACK_CUSTOMCOLOR = (1 << 7),
  TRACK_USE_2D_STAB = (1 << 8),
  TRACK_PREVIEW_GRAYSCALE = (1 << 9),
  TRACK_DOPE_SEL = (1 << 10),
  TRACK_PREVIEW_ALPHA = (1 << 11),
  TRACK_USE_2D_STAB_ROT = (1 << 12),
};

/* MovieTrackingTrack->motion_model */
enum {
  TRACK_MOTION_MODEL_TRANSLATION = 0,
  TRACK_MOTION_MODEL_TRANSLATION_ROTATION = 1,
  TRACK_MOTION_MODEL_TRANSLATION_SCALE = 2,
  TRACK_MOTION_MODEL_TRANSLATION_ROTATION_SCALE = 3,
  TRACK_MOTION_MODEL_AFFINE = 4,
  TRACK_MOTION_MODEL_HOMOGRAPHY = 5,
};

/* MovieTrackingTrack->algorithm_flag */
enum {
  TRACK_ALGORITHM_FLAG_USE_BRUTE = (1 << 0),
  TRACK_ALGORITHM_FLAG_USE_NORMALIZATION = (1 << 2),
  TRACK_ALGORITHM_FLAG_USE_MASK = (1 << 3),
};

/* MovieTrackingTrack->pattern_match */
typedef enum eTrackFrameMatch {
  TRACK_MATCH_KEYFRAME = 0,
  TRACK_MATCH_PREVIOS_FRAME = 1,
} eTrackFrameMatch;

/* MovieTrackingSettings->motion_flag */
enum {
  TRACKING_MOTION_TRIPOD = (1 << 0),

  TRACKING_MOTION_MODAL = (TRACKING_MOTION_TRIPOD),
};

/* MovieTrackingSettings->speed */
enum {
  TRACKING_SPEED_FASTEST = 0,
  TRACKING_SPEED_REALTIME = 1,
  TRACKING_SPEED_HALF = 2,
  TRACKING_SPEED_QUARTER = 4,
  TRACKING_SPEED_DOUBLE = 5,
};

/* MovieTrackingSettings->reconstruction_flag */
enum {
  /* TRACKING_USE_FALLBACK_RECONSTRUCTION = (1 << 0), */ /* DEPRECATED */
  TRACKING_USE_KEYFRAME_SELECTION = (1 << 1),
};

/* MovieTrackingSettings->refine_camera_intrinsics */
enum {
  REFINE_NO_INTRINSICS = (0),

  REFINE_FOCAL_LENGTH = (1 << 0),
  REFINE_PRINCIPAL_POINT = (1 << 1),
  REFINE_RADIAL_DISTORTION = (1 << 2),
  REFINE_TANGENTIAL_DISTORTION = (1 << 3),
};

/* MovieTrackingStabilization->flag */
enum {
  TRACKING_2D_STABILIZATION = (1 << 0),
  TRACKING_AUTOSCALE = (1 << 1),
  TRACKING_STABILIZE_ROTATION = (1 << 2),
  TRACKING_STABILIZE_SCALE = (1 << 3),
  TRACKING_SHOW_STAB_TRACKS = (1 << 5),
};

/* MovieTrackingStabilization->filter */
enum {
  TRACKING_FILTER_NEAREST = 0,
  TRACKING_FILTER_BILINEAR = 1,
  TRACKING_FILTER_BICUBIC = 2,
};

/* MovieTrackingReconstruction->flag */
enum {
  TRACKING_RECONSTRUCTED = (1 << 0),
};

/* MovieTrackingObject->flag */
enum {
  TRACKING_OBJECT_CAMERA = (1 << 0),
};

enum {
  TRACKING_CLEAN_SELECT = 0,
  TRACKING_CLEAN_DELETE_TRACK = 1,
  TRACKING_CLEAN_DELETE_SEGMENT = 2,
};

/* MovieTrackingDopesheet->sort_method */
enum {
  TRACKING_DOPE_SORT_NAME = 0,
  TRACKING_DOPE_SORT_LONGEST = 1,
  TRACKING_DOPE_SORT_TOTAL = 2,
  TRACKING_DOPE_SORT_AVERAGE_ERROR = 3,
  TRACKING_DOPE_SORT_START = 4,
  TRACKING_DOPE_SORT_END = 5,
};

/* MovieTrackingDopesheet->flag */
enum {
  TRACKING_DOPE_SORT_INVERSE = (1 << 0),
  TRACKING_DOPE_SELECTED_ONLY = (1 << 1),
  TRACKING_DOPE_SHOW_HIDDEN = (1 << 2),
};

/* MovieTrackingDopesheetCoverageSegment->trackness */
enum {
  TRACKING_COVERAGE_BAD = 0,
  TRACKING_COVERAGE_ACCEPTABLE = 1,
  TRACKING_COVERAGE_OK = 2,
};

/* MovieTrackingPlaneMarker->flag */
enum {
  PLANE_MARKER_DISABLED = (1 << 0),
  PLANE_MARKER_TRACKED = (1 << 1),
};

/* MovieTrackingPlaneTrack->flag */
enum {
  PLANE_TRACK_HIDDEN = (1 << 1),
  PLANE_TRACK_LOCKED = (1 << 2),
  PLANE_TRACK_AUTOKEY = (1 << 3),
};

#ifdef __cplusplus
}
#endif
