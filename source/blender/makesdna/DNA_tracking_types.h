/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Structs used for camera tracking and the movie-clip editor.
 */

#pragma once

#include "BLI_enum_flags.hh"

#include "DNA_listBase.h"

/* match-moving data */

struct Image;
struct MovieReconstructedCamera;
struct MovieTracking;
struct MovieTrackingCamera;
struct MovieTrackingMarker;
struct MovieTrackingTrack;
struct bGPdata;

enum TrackingDistortionModel {
  TRACKING_DISTORTION_MODEL_POLYNOMIAL = 0,
  TRACKING_DISTORTION_MODEL_DIVISION = 1,
  TRACKING_DISTORTION_MODEL_NUKE = 2,
  TRACKING_DISTORTION_MODEL_BROWN = 3,
};

enum TrackingCameraUnits {
  CAMERA_UNITS_PX = 0,
  CAMERA_UNITS_MM = 1,
};

enum TrackingMarkerFlag {
  MARKER_DISABLED = (1 << 0),
  MARKER_TRACKED = (1 << 1),
  MARKER_GRAPH_SEL_X = (1 << 2),
  MARKER_GRAPH_SEL_Y = (1 << 3),
  MARKER_GRAPH_SEL = (MARKER_GRAPH_SEL_X | MARKER_GRAPH_SEL_Y),
};

enum TrackingTrackFlag {
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

enum TrackingMotionModel {
  TRACK_MOTION_MODEL_TRANSLATION = 0,
  TRACK_MOTION_MODEL_TRANSLATION_ROTATION = 1,
  TRACK_MOTION_MODEL_TRANSLATION_SCALE = 2,
  TRACK_MOTION_MODEL_TRANSLATION_ROTATION_SCALE = 3,
  TRACK_MOTION_MODEL_AFFINE = 4,
  TRACK_MOTION_MODEL_HOMOGRAPHY = 5,
};

enum TrackingAlgorithmFlag {
  TRACK_ALGORITHM_FLAG_USE_BRUTE = (1 << 0),
  TRACK_ALGORITHM_FLAG_USE_NORMALIZATION = (1 << 2),
  TRACK_ALGORITHM_FLAG_USE_MASK = (1 << 3),
};

enum eTrackFrameMatch {
  TRACK_MATCH_KEYFRAME = 0,
  TRACK_MATCH_PREVIOUS_FRAME = 1,
};

enum TrackingMotionFlag {
  TRACKING_MOTION_TRIPOD = (1 << 0),
  TRACKING_MOTION_MODAL = (TRACKING_MOTION_TRIPOD),
};

enum TrackingSpeed {
  TRACKING_SPEED_FASTEST = 0,
  TRACKING_SPEED_REALTIME = 1,
  TRACKING_SPEED_HALF = 2,
  TRACKING_SPEED_QUARTER = 4,
  TRACKING_SPEED_DOUBLE = 5,
};

enum TrackingSettingsReconstructionFlag {
  /* TRACKING_USE_FALLBACK_RECONSTRUCTION = (1 << 0), */ /* DEPRECATED */
  TRACKING_USE_KEYFRAME_SELECTION = (1 << 1),
};

enum TrackingRefineCameraFlag {
  REFINE_NO_INTRINSICS = (0),

  REFINE_FOCAL_LENGTH = (1 << 0),
  REFINE_PRINCIPAL_POINT = (1 << 1),
  REFINE_RADIAL_DISTORTION = (1 << 2),
  REFINE_TANGENTIAL_DISTORTION = (1 << 3),
};
ENUM_OPERATORS(TrackingRefineCameraFlag);

enum TrackingStabilizationFlag {
  TRACKING_2D_STABILIZATION = (1 << 0),
  TRACKING_AUTOSCALE = (1 << 1),
  TRACKING_STABILIZE_ROTATION = (1 << 2),
  TRACKING_STABILIZE_SCALE = (1 << 3),
  TRACKING_SHOW_STAB_TRACKS = (1 << 5),
};

enum TrackingStabilizationFilter {
  TRACKING_FILTER_NEAREST = 0,
  TRACKING_FILTER_BILINEAR = 1,
  TRACKING_FILTER_BICUBIC = 2,
};

enum TrackingReconstructionFlag {
  TRACKING_RECONSTRUCTED = (1 << 0),
};

enum TrackingObjectFlag {
  TRACKING_OBJECT_CAMERA = (1 << 0),
};

enum TrackingDopesheetSort {
  TRACKING_DOPE_SORT_NAME = 0,
  TRACKING_DOPE_SORT_LONGEST = 1,
  TRACKING_DOPE_SORT_TOTAL = 2,
  TRACKING_DOPE_SORT_AVERAGE_ERROR = 3,
  TRACKING_DOPE_SORT_START = 4,
  TRACKING_DOPE_SORT_END = 5,
};

enum TrackingDopesheetFlag {
  TRACKING_DOPE_SORT_INVERSE = (1 << 0),
  TRACKING_DOPE_SELECTED_ONLY = (1 << 1),
  TRACKING_DOPE_SHOW_HIDDEN = (1 << 2),
};

enum TrackingCoverage {
  TRACKING_COVERAGE_BAD = 0,
  TRACKING_COVERAGE_ACCEPTABLE = 1,
  TRACKING_COVERAGE_OK = 2,
};

enum TrackingPlaneMarkerFlag {
  PLANE_MARKER_DISABLED = (1 << 0),
  PLANE_MARKER_TRACKED = (1 << 1),
};

enum TrackingPlaneTrackFlag {
  PLANE_TRACK_HIDDEN = (1 << 1),
  PLANE_TRACK_LOCKED = (1 << 2),
  PLANE_TRACK_AUTOKEY = (1 << 3),
};

struct MovieReconstructedCamera {
  int framenr = 0;
  float error = 0;
  float mat[4][4] = {};
};

struct MovieTrackingCamera {
  /** Intrinsics handle. */
  void *intrinsics = nullptr;

  short distortion_model = 0; /* TrackingDistortionModel */
  char _pad[2] = {};

  /** Width of CCD sensor. */
  float sensor_width = 0;
  /** Pixel aspect ratio. */
  float pixel_aspect = 0;
  /** Focal length. */
  float focal = 0;
  /** Units of focal length user is working with (#TrackingCameraUnits). */
  short units = 0;
  char _pad1[2] = {};

  /**
   * Principal point (optical center) stored in normalized coordinates.
   *
   * The normalized space stores principal point relative to the frame center which has normalized
   * principal coordinate of (0, 0). The right top corer of the frame corresponds to a normalized
   * principal coordinate of (1, 1), and the left bottom corner corresponds to coordinate of
   * (-1, -1).
   */
  float principal_point[2] = {};

  /** Legacy principal point in pixel space. */
  float principal_legacy[2] = {};

  /* Polynomial distortion */
  /** Polynomial radial distortion. */
  float k1 = 0, k2 = 0, k3 = 0;

  /* Division distortion model coefficients */
  float division_k1 = 0, division_k2 = 0;

  /* Nuke distortion model coefficients */
  float nuke_k1 = 0, nuke_k2 = 0;
  float nuke_p1 = 0, nuke_p2 = 0;

  /* Brown-Conrady distortion model coefficients */
  /** Brown-Conrady radial distortion. */
  float brown_k1 = 0, brown_k2 = 0, brown_k3 = 0, brown_k4 = 0;
  /** Brown-Conrady tangential distortion. */
  float brown_p1 = 0, brown_p2 = 0;
};

struct MovieTrackingMarker {
  /** 2d position of marker on frame (in unified 0..1 space). */
  float pos[2] = {0.0f, 0.0f};

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
  float pattern_corners[4][2] = {};

  /* positions of left-bottom and right-top corners of search area (in unified 0..1 units,
   * relative to marker->pos
   */
  float search_min[2] = {}, search_max[2] = {};

  /** Number of frame marker is associated with. */
  int framenr = 0;
  /** Marker's flag (alive, ...), #TrackingMarkerFlag. */
  int flag = 0;
};

struct MovieTrackingTrack {
  struct MovieTrackingTrack *next = nullptr, *prev = nullptr;

  char name[/*MAX_NAME*/ 64] = "";

  /* ** settings ** */

  /* positions of left-bottom and right-top corners of pattern (in unified 0..1 units,
   * relative to marker->pos)
   * moved to marker's corners since planar tracking implementation
   */
  float pat_min_legacy[2] = {}, pat_max_legacy[2] = {};

  /* positions of left-bottom and right-top corners of search area (in unified 0..1 units,
   * relative to marker->pos
   * moved to marker since affine tracking implementation
   */
  float search_min_legacy[2] = {}, search_max_legacy[2] = {};

  /** Offset to "parenting" point. */
  float offset[2] = {};

  /* ** track ** */
  /** Count of markers in track. */
  int markersnr = 0;
  /** Most recently used marker. */
  int _pad = {};
  /** Markers in track. */
  MovieTrackingMarker *markers = nullptr;

  /* ** reconstruction data ** */
  /** Reconstructed position. */
  float bundle_pos[3] = {};
  /** Average track reprojection error. */
  float error = 0;

  /* ** UI editing ** */
  /** Flags (selection, ...), #TrackingTrackFlag. */
  int flag = 0, pat_flag = 0, search_flag = 0;
  /** Custom color for track. */
  float color[3] = {};

  /* ** control how tracking happens */
  /**
   * Number of frames to be tracked during single tracking session
   * (if TRACKING_FRAMES_LIMIT is set).
   */
  short frames_limit = 0;
  /** Margin from frame boundaries. */
  short margin = 0;
  /** Denotes which frame is used for the reference during tracking, #eTrackFrameMatch. */
  short pattern_match = 0;

  /* tracking parameters */
  /** Model of the motion for this track, #TrackingMotionModel. */
  short motion_model = 0;
  /** Flags for the tracking algorithm (use brute, use ESM, use pyramid, etc),
   * #TrackingAlgorithmFlag. */
  int algorithm_flag = 0;
  /** Minimal correlation which is still treated as successful tracking. */
  float minimum_correlation = 0;

  /** Grease-pencil data. */
  struct bGPdata *gpd = nullptr;

  /* Weight of this track.
   *
   * Weight defines how much the track affects on the final reconstruction,
   * usually gets animated in a way so when track has just appeared its
   * weight is zero and then it gets faded up.
   *
   * Used to prevent jumps of the camera when tracks are appearing or
   * disappearing.
   */
  float weight = 0;

  /* track weight especially for 2D stabilization */
  float weight_stab = 0;
};

struct MovieTrackingPlaneMarker {
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
  float corners[4][2] = {};

  /** Number of frame plane marker is associated with. */
  int framenr = 0;
  /** Marker's flag (alive, ...), #TrackingPlaneMarkerFlag. */
  int flag = 0;
};

struct MovieTrackingPlaneTrack {
  struct MovieTrackingPlaneTrack *next = nullptr, *prev = nullptr;

  char name[/*MAX_NAME*/ 64] = "";

  /**
   * Array of point tracks used to define this plane.
   * Each element is a pointer to MovieTrackingTrack.
   */
  MovieTrackingTrack **point_tracks = nullptr;
  /** Number of tracks in point_tracks array. */
  int point_tracksnr = 0;
  char _pad[4] = {};

  /** Markers in the plane track. */
  MovieTrackingPlaneMarker *markers = nullptr;
  /** Count of markers in track (size of markers array). */
  int markersnr = 0;

  /** Flags (selection, ...), #TrackingPlaneTrackFlag. */
  int flag = 0;

  /** Image displaying during editing. */
  struct Image *image = nullptr;
  /** Opacity of the image. */
  float image_opacity = 0;

  /* Runtime data */
  /** Most recently used marker. */
  int last_marker = 0;
};

struct MovieTrackingSettings {
  /* ** default tracker settings */
  /** Model of the motion for this track, #TrackingMotionModel. */
  short default_motion_model = 0;
  /** Flags for the tracking algorithm (use brute, use ESM, use pyramid, etc.),
   * #TrackingAlgorithmFlag. */
  short default_algorithm_flag = 0;
  /** Minimal correlation which is still treated as successful tracking. */
  float default_minimum_correlation = 0;
  /** Size of pattern area for new tracks, measured in pixels. */
  short default_pattern_size = 0;
  /** Size of search area for new tracks, measured in pixels. */
  short default_search_size = 0;
  /** Number of frames to be tracked during single tracking session
   * (if TRACKING_FRAMES_LIMIT is set). */
  short default_frames_limit = 0;
  /** Margin from frame boundaries. */
  short default_margin = 0;
  /** Denotes which frame is used for the reference during tracking, #eTrackFrameMatch. */
  short default_pattern_match = 0;
  /** Default flags like color channels used by default. */
  short default_flag = 0;
  /** Default weight of the track. */
  float default_weight = 0;

  /** Flags describes motion type, #TrackingMotionFlag. */
  short motion_flag = 0;

  /* ** common tracker settings ** */
  /** Speed of tracking, #TrackingSpeed. */
  short speed = 0;

  /* ** reconstruction settings ** */
  /* two keyframes for reconstruction initialization
   * were moved to per-tracking object settings
   */
  int keyframe1_legacy = 0;
  int keyframe2_legacy = 0;

  int reconstruction_flag = 0; /* TrackingSettingsReconstructionFlag */

  /* Which camera intrinsics to refine, #TrackingRefineCameraFlag. */
  int refine_camera_intrinsics = 0;

  /* ** tool settings ** */

  /* set scale */
  /** Distance between two bundles used for scene scaling. */
  float dist = 0;

  /* cleanup */
  int clean_frames = 0, clean_action = 0;
  float clean_error = 0;

  /* set object scale */
  /** Distance between two bundles used for object scaling. */
  float object_distance = 0;
};

struct MovieTrackingStabilization {
  int flag = 0; /* TrackingStabilizationFlag */
  /** Total number of translation tracks and index of active track in list. */
  int tot_track = 0, act_track = 0;
  /** Total number of rotation tracks and index of active track in list. */
  int tot_rot_track = 0, act_rot_track = 0;

  /* 2d stabilization */
  /** Max auto-scale factor. */
  float maxscale = 0;
  /** Use TRACK_USE_2D_STAB_ROT on individual tracks instead. */
  MovieTrackingTrack *rot_track_legacy = nullptr;

  /** Reference point to anchor stabilization offset. */
  int anchor_frame = 0;
  /** Expected target position of frame after raw stabilization, will be subtracted. */
  float target_pos[2] = {};
  /** Expected target rotation of frame after raw stabilization, will be compensated. */
  float target_rot = 0;
  /** Zoom factor known to be present on original footage. Also used for auto-scale. */
  float scale = 0;

  /** Influence on location, scale and rotation. */
  float locinf = 0, scaleinf = 0, rotinf = 0;

  /** Filter used for pixel interpolation, #TrackingStabilizationFilter. */
  int filter = 0;

  int _pad = {};
};

struct MovieTrackingReconstruction {
  int flag = 0; /* TrackingReconstructionFlag */

  /** Average error of reconstruction. */
  float error = 0;

  /** Most recently used camera. */
  int last_camera = 0;
  /** Number of reconstructed cameras. */
  int camnr = 0;
  /** Reconstructed cameras. */
  struct MovieReconstructedCamera *cameras = nullptr;
};

struct MovieTrackingObject {
  struct MovieTrackingObject *next = nullptr, *prev = nullptr;

  /** Name of tracking object. */
  char name[/*MAX_NAME*/ 64] = "";
  int flag = 0; /* TrackingObjectFlag */
  /** Scale of object solution in camera space. */
  float scale = 0;

  /** Lists of point and plane tracks use to tracking this object. */
  ListBaseT<MovieTrackingTrack> tracks = {nullptr, nullptr};
  ListBaseT<MovieTrackingPlaneTrack> plane_tracks = {nullptr, nullptr};

  /** Active point and plane tracks. */
  MovieTrackingTrack *active_track = nullptr;
  MovieTrackingPlaneTrack *active_plane_track = nullptr;

  /** Reconstruction data for this object. */
  MovieTrackingReconstruction reconstruction;

  /* reconstruction options */
  /** Two keyframes for reconstruction initialization. */
  int keyframe1 = 0, keyframe2 = 0;
};

struct MovieTrackingStats {
  char message[256] = "";
};

struct MovieTrackingDopesheetChannel {
  struct MovieTrackingDopesheetChannel *next = nullptr, *prev = nullptr;

  /** Motion track for which channel is created. */
  MovieTrackingTrack *track = nullptr;
  char _pad[4] = {};

  /** Name of channel. */
  char name[64] = "";

  /** Total number of segments. */
  int tot_segment = 0;
  /** Tracked segments. */
  int *segments = nullptr;
  /** Longest segment length and total number of tracked frames. */
  int max_segment = 0, total_frames = 0;
  /** These numbers are valid only if tot_segment > 0. */
  int first_not_disabled_marker_framenr = 0, last_not_disabled_marker_framenr = 0;
};

struct MovieTrackingDopesheetCoverageSegment {
  struct MovieTrackingDopesheetCoverageSegment *next = nullptr, *prev = nullptr;

  int coverage = 0; /* TrackingCoverage */
  int start_frame = 0;
  int end_frame = 0;

  char _pad[4] = {};
};

struct MovieTrackingDopesheet {
  /** Flag if dopesheet information is still relevant. */
  int ok = 0;

  /** Method to be used to sort tracks, #TrackingDopesheetSort. */
  short sort_method = 0;
  /** Dope-sheet building flag such as inverted order of sort, #TrackingDopesheetFlag. */
  short flag = 0;

  /* ** runtime stuff ** */

  /* summary */
  ListBaseT<MovieTrackingDopesheetCoverageSegment> coverage_segments = {nullptr, nullptr};

  /* detailed */
  ListBaseT<MovieTrackingDopesheetChannel> channels = {nullptr, nullptr};
  int tot_channel = 0;

  char _pad[4] = {};
};

struct MovieTracking {
  /** Different tracking-related settings. */
  MovieTrackingSettings settings;
  /** Camera intrinsics. */
  MovieTrackingCamera camera;
  /** Lists of point and plane tracks used for camera object.
   * NOTE: Only left for the versioning purposes. */
  ListBaseT<MovieTrackingTrack> tracks_legacy = {nullptr, nullptr};
  ListBaseT<MovieTrackingPlaneTrack> plane_tracks_legacy = {nullptr, nullptr};

  /** Reconstruction data for camera object.
   * NOTE: Only left for the versioning purposes. */
  MovieTrackingReconstruction reconstruction_legacy;

  /** Stabilization data. */
  MovieTrackingStabilization stabilization;

  /** Active point and plane tracks.
   * NOTE: Only left for the versioning purposes. */
  MovieTrackingTrack *act_track_legacy = nullptr;
  MovieTrackingPlaneTrack *act_plane_track_legacy = nullptr;

  ListBaseT<MovieTrackingObject> objects = {nullptr, nullptr};
  /** Index of active object and total number of objects. */
  int objectnr = 0, tot_object = 0;

  /** Statistics displaying in clip editor. */
  MovieTrackingStats *stats = nullptr;

  /** Dope-sheet data. */
  MovieTrackingDopesheet dopesheet;
};
