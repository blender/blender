/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Camera;
struct ImBuf;
struct ListBase;
struct MovieClipUser;
struct MovieDistortion;
struct MovieReconstructContext;
struct MovieTracking;
struct MovieTrackingCamera;
struct MovieTrackingMarker;
struct MovieTrackingObject;
struct MovieTrackingPlaneMarker;
struct MovieTrackingPlaneTrack;
struct MovieTrackingTrack;
struct Object;
struct Scene;
struct bGPDlayer;
struct rcti;

/* --------------------------------------------------------------------
 * Common types and constants.
 */

typedef enum eTrackArea {
  TRACK_AREA_POINT = (1 << 0),
  TRACK_AREA_PAT = (1 << 1),
  TRACK_AREA_SEARCH = (1 << 2),

  TRACK_AREA_NONE = 0,
  TRACK_AREA_ALL = (TRACK_AREA_POINT | TRACK_AREA_PAT | TRACK_AREA_SEARCH),
} eTrackArea;

/* --------------------------------------------------------------------
 * Common functions.
 */

/**
 * Free tracking structure, only frees structure contents
 * (if structure is allocated in heap, it shall be handled outside).
 *
 * All the pointers inside structure becomes invalid after this call.
 */
void BKE_tracking_free(struct MovieTracking *tracking);
/**
 * Copy tracking structure content.
 */
void BKE_tracking_copy(struct MovieTracking *tracking_dst,
                       const struct MovieTracking *tracking_src,
                       int flag);

/**
 * Initialize motion tracking settings to default values,
 * used when new movie clip data-block is created.
 */
void BKE_tracking_settings_init(struct MovieTracking *tracking);

/* Matrices for constraints and drawing. */

/**
 * Get transformation matrix for a given object which is used
 * for parenting motion tracker reconstruction to 3D world.
 */
void BKE_tracking_get_camera_object_matrix(struct Object *camera_object, float mat[4][4]);
/**
 * Get projection matrix for camera specified by given tracking object
 * and frame number.
 *
 * \note frame number should be in clip space, not scene space.
 */
void BKE_tracking_get_projection_matrix(struct MovieTracking *tracking,
                                        struct MovieTrackingObject *tracking_object,
                                        int framenr,
                                        int winx,
                                        int winy,
                                        float mat[4][4]);

/* --------------------------------------------------------------------
 * Clipboard.
 */

/**
 * Free clipboard by freeing memory used by all tracks in it.
 */
void BKE_tracking_clipboard_free(void);
/**
 * Copy selected tracks from specified object to the clipboard.
 */
void BKE_tracking_clipboard_copy_tracks(struct MovieTracking *tracking,
                                        struct MovieTrackingObject *tracking_object);
/**
 * Check whether there are any tracks in the clipboard.
 */
bool BKE_tracking_clipboard_has_tracks(void);
/**
 * Paste tracks from clipboard to specified object.
 *
 * Names of new tracks in object are guaranteed to be unique here.
 */
void BKE_tracking_clipboard_paste_tracks(struct MovieTracking *tracking,
                                         struct MovieTrackingObject *tracking_object);

/* **** Track **** */

/**
 * Add new empty track to the given list of tracks.
 *
 * It is required that caller will append at least one marker to avoid degenerate tracks.
 */
struct MovieTrackingTrack *BKE_tracking_track_add_empty(struct MovieTracking *tracking,
                                                        struct ListBase *tracks_list);
/**
 * Add new track to a specified tracks base.
 *
 * Coordinates are expected to be in normalized 0..1 space,
 * frame number is expected to be in clip space.
 *
 * Width and height are clip's dimension used to scale track's
 * pattern and search regions.
 */
struct MovieTrackingTrack *BKE_tracking_track_add(struct MovieTracking *tracking,
                                                  struct ListBase *tracksbase,
                                                  float x,
                                                  float y,
                                                  int framenr,
                                                  int width,
                                                  int height);
/**
 * Duplicate the specified track, result will no belong to any list.
 */
struct MovieTrackingTrack *BKE_tracking_track_duplicate(struct MovieTrackingTrack *track);
/**
 * Ensure specified track has got unique name,
 * if it's not name of specified track will be changed
 * keeping names of all other tracks unchanged.
 */
void BKE_tracking_track_unique_name(struct ListBase *tracksbase, struct MovieTrackingTrack *track);
/**
 * Free specified track, only frees contents of a structure
 * (if track is allocated in heap, it shall be handled outside).
 *
 * All the pointers inside track becomes invalid after this call.
 */
void BKE_tracking_track_free(struct MovieTrackingTrack *track);

/**
 * Get frame numbers of the very first and last markers.
 * There is no check on whether the marker is enabled or not.
 */
void BKE_tracking_track_first_last_frame_get(const struct MovieTrackingTrack *track,
                                             int *r_first_frame,
                                             int *r_last_frame);

/**
 * Find the minimum starting frame and maximum ending frame within given set of tracks.
 */
void BKE_tracking_tracks_first_last_frame_minmax(/*const*/ struct MovieTrackingTrack **tracks,
                                                 int num_tracks,
                                                 int *r_first_frame,
                                                 int *r_last_frame);

int BKE_tracking_count_selected_tracks_in_list(const struct ListBase *tracks_list);
int BKE_tracking_count_selected_tracks_in_active_object(/*const*/ struct MovieTracking *tracking);

/**
 * Get array of selected tracks from the current active object in the tracking structure.
 * If nothing is selected then the result is nullptr and `r_num_tracks` is set to 0.
 */
struct MovieTrackingTrack **BKE_tracking_selected_tracks_in_active_object(
    struct MovieTracking *tracking, int *r_num_tracks);

/**
 * Set flag for all specified track's areas.
 *
 * \param area: which part of marker should be selected. see TRACK_AREA_* constants.
 * \param flag: flag to be set for areas.
 */
void BKE_tracking_track_flag_set(struct MovieTrackingTrack *track, int area, int flag);
/**
 * Clear flag from all specified track's areas.
 *
 * \param area: which part of marker should be selected. see TRACK_AREA_* constants.
 * \param flag: flag to be cleared for areas.
 */
void BKE_tracking_track_flag_clear(struct MovieTrackingTrack *track, int area, int flag);

/**
 * Check whether track has got marker at specified frame.
 *
 * \note frame number should be in clip space, not scene space.
 */
bool BKE_tracking_track_has_marker_at_frame(struct MovieTrackingTrack *track, int framenr);
/**
 * Check whether track has got enabled marker at specified frame.
 *
 * \note frame number should be in clip space, not scene space.
 */
bool BKE_tracking_track_has_enabled_marker_at_frame(struct MovieTrackingTrack *track, int framenr);

/**
 * Clear track's path.
 *
 * \note frame number should be in clip space, not scene space.
 */
typedef enum eTrackClearAction {
  /* Clear path from `ref_frame+1` up to the . */
  TRACK_CLEAR_UPTO,
  /* Clear path from the beginning up to `ref_frame-1`. */
  TRACK_CLEAR_REMAINED,
  /* Only marker at frame `ref_frame` will remain. */
  TRACK_CLEAR_ALL,
} eTrackClearAction;
void BKE_tracking_track_path_clear(struct MovieTrackingTrack *track,
                                   int ref_frame,
                                   eTrackClearAction action);

void BKE_tracking_tracks_join(struct MovieTracking *tracking,
                              struct MovieTrackingTrack *dst_track,
                              struct MovieTrackingTrack *src_track);

void BKE_tracking_tracks_average(struct MovieTrackingTrack *dst_track,
                                 /*const*/ struct MovieTrackingTrack **src_tracks,
                                 int num_src_tracks);

struct MovieTrackingTrack *BKE_tracking_track_get_for_selection_index(
    struct MovieTracking *tracking, int selection_index, struct ListBase **r_tracksbase);

float *BKE_tracking_track_get_mask(int frame_width,
                                   int frame_height,
                                   const struct MovieTrackingTrack *track,
                                   const struct MovieTrackingMarker *marker);

float BKE_tracking_track_get_weight_for_marker(struct MovieClip *clip,
                                               struct MovieTrackingTrack *track,
                                               struct MovieTrackingMarker *marker);

/* --------------------------------------------------------------------
 * Selection.
 */

/**
 * \param area: which part of marker should be selected. see TRACK_AREA_* constants.
 */
void BKE_tracking_track_select(struct ListBase *tracksbase,
                               struct MovieTrackingTrack *track,
                               int area,
                               bool extend);
void BKE_tracking_track_deselect(struct MovieTrackingTrack *track, int area);
void BKE_tracking_tracks_deselect_all(struct ListBase *tracksbase);

/* --------------------------------------------------------------------
 * Marker.
 */

struct MovieTrackingMarker *BKE_tracking_marker_insert(struct MovieTrackingTrack *track,
                                                       struct MovieTrackingMarker *marker);
void BKE_tracking_marker_delete(struct MovieTrackingTrack *track, int framenr);

/**
 * If the pattern area is outside of the search area its position will be modified in a way that it
 * is within the pattern is within the search area.
 *
 * If the pattern area is already within the search area nothing happens.
 *
 * If the pattern area is bigger than the search area the behavior is undefined.
 *
 * Search area is never modified.
 */
void BKE_tracking_marker_clamp_pattern_position(struct MovieTrackingMarker *marker);

/**
 * If the search size is such that pattern area is (partially) outside of the search area make the
 * search area bigger so that the pattern is within the search area.
 *
 * Pattern area is never modified.
 */
void BKE_tracking_marker_clamp_search_size(struct MovieTrackingMarker *marker);

/**
 * If the search position is such that pattern area is (partially) outside of the search area move
 * the search area so that the pattern is within the search area.
 *
 * If the search area is smaller than the pattern the behavior is undefined.
 *
 * Pattern area is never modified.
 */
void BKE_tracking_marker_clamp_search_position(struct MovieTrackingMarker *marker);

/**
 * Get marker closest to the given frame number.
 *
 * If there is maker with exact frame number it returned.
 * Otherwise, marker with highest frame number but lower than the requested
 * frame is returned if such marker exists. Otherwise, the marker with lowest
 * frame number greater than the requested frame number is returned.
 *
 * This function has complexity of `O(log number_of_markers)`.
 */
struct MovieTrackingMarker *BKE_tracking_marker_get(struct MovieTrackingTrack *track, int framenr);
struct MovieTrackingMarker *BKE_tracking_marker_get_exact(struct MovieTrackingTrack *track,
                                                          int framenr);
struct MovieTrackingMarker *BKE_tracking_marker_ensure(struct MovieTrackingTrack *track,
                                                       int framenr);

/**
 * Get marker position, possibly interpolating gap between key-framed/tracked markers.
 *
 * The result marker frame number is set to the requested frame number. Its flags are 0 if the
 * marker is interpolated, and is set to original marker flag if there were no interpolation
 * involved.
 *
 * \returns truth if the result is usable.
 */
bool BKE_tracking_marker_get_interpolated(struct MovieTrackingTrack *track,
                                          int framenr,
                                          struct MovieTrackingMarker *r_marker);

void BKE_tracking_marker_pattern_minmax(const struct MovieTrackingMarker *marker,
                                        float min[2],
                                        float max[2]);

void BKE_tracking_marker_get_subframe_position(struct MovieTrackingTrack *track,
                                               float framenr,
                                               float pos[2]);

/* --------------------------------------------------------------------
 * Plane track.
 */

/**
 * Creates new plane track out of selected point tracks.
 */
struct MovieTrackingPlaneTrack *BKE_tracking_plane_track_add(struct MovieTracking *tracking,
                                                             struct ListBase *plane_tracks_base,
                                                             struct ListBase *tracks,
                                                             int framenr);
void BKE_tracking_plane_track_unique_name(struct ListBase *plane_tracks_base,
                                          struct MovieTrackingPlaneTrack *plane_track);
/**
 * Free specified plane track, only frees contents of a structure
 * (if track is allocated in heap, it shall be handled outside).
 *
 * All the pointers inside track becomes invalid after this call.
 */
void BKE_tracking_plane_track_free(struct MovieTrackingPlaneTrack *plane_track);

bool BKE_tracking_plane_track_has_marker_at_frame(struct MovieTrackingPlaneTrack *plane_track,
                                                  int framenr);
bool BKE_tracking_plane_track_has_enabled_marker_at_frame(
    struct MovieTrackingPlaneTrack *plane_track, int framenr);

void BKE_tracking_plane_tracks_deselect_all(struct ListBase *plane_tracks_base);

bool BKE_tracking_plane_track_has_point_track(struct MovieTrackingPlaneTrack *plane_track,
                                              struct MovieTrackingTrack *track);
bool BKE_tracking_plane_track_remove_point_track(struct MovieTrackingPlaneTrack *plane_track,
                                                 struct MovieTrackingTrack *track);

void BKE_tracking_plane_tracks_remove_point_track(struct MovieTracking *tracking,
                                                  struct MovieTrackingTrack *track);

void BKE_tracking_plane_track_replace_point_track(struct MovieTrackingPlaneTrack *plane_track,
                                                  struct MovieTrackingTrack *old_track,
                                                  struct MovieTrackingTrack *new_track);
void BKE_tracking_plane_tracks_replace_point_track(struct MovieTracking *tracking,
                                                   struct MovieTrackingTrack *old_track,
                                                   struct MovieTrackingTrack *new_track);

/* --------------------------------------------------------------------
 * Plane marker.
 */

struct MovieTrackingPlaneMarker *BKE_tracking_plane_marker_insert(
    struct MovieTrackingPlaneTrack *plane_track, struct MovieTrackingPlaneMarker *plane_marker);
void BKE_tracking_plane_marker_delete(struct MovieTrackingPlaneTrack *plane_track, int framenr);

/**
 * Get a plane marker at given frame,
 * If there's no such marker, closest one from the left side will be returned.
 */
struct MovieTrackingPlaneMarker *BKE_tracking_plane_marker_get(
    struct MovieTrackingPlaneTrack *plane_track, int framenr);
/**
 * Get a plane marker at exact given frame, if there's no marker at the frame,
 * NULL will be returned.
 */
struct MovieTrackingPlaneMarker *BKE_tracking_plane_marker_get_exact(
    struct MovieTrackingPlaneTrack *plane_track, int framenr);
/**
 * Ensure there's a marker for the given frame.
 */
struct MovieTrackingPlaneMarker *BKE_tracking_plane_marker_ensure(
    struct MovieTrackingPlaneTrack *plane_track, int framenr);
void BKE_tracking_plane_marker_get_subframe_corners(struct MovieTrackingPlaneTrack *plane_track,
                                                    float framenr,
                                                    float corners[4][2]);

/* --------------------------------------------------------------------
 * Object.
 */

struct MovieTrackingObject *BKE_tracking_object_add(struct MovieTracking *tracking,
                                                    const char *name);
bool BKE_tracking_object_delete(struct MovieTracking *tracking,
                                struct MovieTrackingObject *tracking_object);

void BKE_tracking_object_unique_name(struct MovieTracking *tracking,
                                     struct MovieTrackingObject *tracking_object);

struct MovieTrackingObject *BKE_tracking_object_get_named(struct MovieTracking *tracking,
                                                          const char *name);

struct MovieTrackingObject *BKE_tracking_object_get_active(const struct MovieTracking *tracking);
struct MovieTrackingObject *BKE_tracking_object_get_camera(const struct MovieTracking *tracking);

/* Find point track with the given name in the tracking object.
 * If such track does not exist NULL is returned. */
struct MovieTrackingTrack *BKE_tracking_object_find_track_with_name(
    struct MovieTrackingObject *tracking_object, const char *name);

/* Find plane track with the given name in the tracking object.
 * If such track does not exist NULL is returned. */
struct MovieTrackingPlaneTrack *BKE_tracking_object_find_plane_track_with_name(
    struct MovieTrackingObject *tracking_object, const char *name);

/* --------------------------------------------------------------------
 * Camera.
 */

/**
 * Converts principal offset from center to offset of blender's camera.
 */
void BKE_tracking_camera_shift_get(
    struct MovieTracking *tracking, int winx, int winy, float *shiftx, float *shifty);
void BKE_tracking_camera_to_blender(struct MovieTracking *tracking,
                                    struct Scene *scene,
                                    struct Camera *camera,
                                    int width,
                                    int height);

struct MovieReconstructedCamera *BKE_tracking_camera_get_reconstructed(
    struct MovieTracking *tracking, struct MovieTrackingObject *tracking_object, int framenr);
void BKE_tracking_camera_get_reconstructed_interpolate(struct MovieTracking *tracking,
                                                       struct MovieTrackingObject *tracking_object,
                                                       float framenr,
                                                       float mat[4][4]);

/* Access the principal point in pixels space. */
void BKE_tracking_camera_principal_point_pixel_get(struct MovieClip *clip,
                                                   float r_principal_point_pixel[2]);
void BKE_tracking_camera_principal_point_pixel_set(struct MovieClip *clip,
                                                   const float principal_point_pixel[2]);

/* Compares distortion related parameters of camera. Ideally, this implementation will be
 * abstracted away in the future, but for now, one needs to be careful about it and handle any
 * extra parameters of distortions models. */
bool BKE_tracking_camera_distortion_equal(const struct MovieTrackingCamera *a,
                                          const struct MovieTrackingCamera *b);
/* Hashes distortion related paramaters of camera. Ideally, this implementation will be
 * abstracted away in the future, but for now, one needs to be careful about it and handle any
 * extra parameters of distortions models. */
uint64_t BKE_tracking_camera_distortion_hash(const struct MovieTrackingCamera *camera);

/* --------------------------------------------------------------------
 * (Un)distortion.
 */

struct MovieDistortion *BKE_tracking_distortion_new(struct MovieTracking *tracking,
                                                    int calibration_width,
                                                    int calibration_height);
void BKE_tracking_distortion_update(struct MovieDistortion *distortion,
                                    struct MovieTracking *tracking,
                                    int calibration_width,
                                    int calibration_height);
void BKE_tracking_distortion_set_threads(struct MovieDistortion *distortion, int threads);
struct MovieDistortion *BKE_tracking_distortion_copy(struct MovieDistortion *distortion);
struct ImBuf *BKE_tracking_distortion_exec(struct MovieDistortion *distortion,
                                           struct MovieTracking *tracking,
                                           struct ImBuf *ibuf,
                                           int width,
                                           int height,
                                           float overscan,
                                           bool undistort);
void BKE_tracking_distortion_distort_v2(struct MovieDistortion *distortion,
                                        const float co[2],
                                        float r_co[2]);
void BKE_tracking_distortion_undistort_v2(struct MovieDistortion *distortion,
                                          const float co[2],
                                          float r_co[2]);
void BKE_tracking_distortion_free(struct MovieDistortion *distortion);

void BKE_tracking_distort_v2(struct MovieTracking *tracking,
                             int image_width,
                             int image_height,
                             const float co[2],
                             float r_co[2]);
void BKE_tracking_undistort_v2(struct MovieTracking *tracking,
                               int image_width,
                               int image_height,
                               const float co[2],
                               float r_co[2]);

struct ImBuf *BKE_tracking_undistort_frame(struct MovieTracking *tracking,
                                           struct ImBuf *ibuf,
                                           int calibration_width,
                                           int calibration_height,
                                           float overscan);
struct ImBuf *BKE_tracking_distort_frame(struct MovieTracking *tracking,
                                         struct ImBuf *ibuf,
                                         int calibration_width,
                                         int calibration_height,
                                         float overscan);

void BKE_tracking_max_distortion_delta_across_bound(struct MovieTracking *tracking,
                                                    int image_width,
                                                    int image_height,
                                                    struct rcti *rect,
                                                    bool undistort,
                                                    float delta[2]);

/* --------------------------------------------------------------------
 * Image sampling.
 */

struct ImBuf *BKE_tracking_sample_pattern(int frame_width,
                                          int frame_height,
                                          const struct ImBuf *search_ib,
                                          const struct MovieTrackingTrack *track,
                                          const struct MovieTrackingMarker *marker,
                                          bool from_anchor,
                                          bool use_mask,
                                          int num_samples_x,
                                          int num_samples_y,
                                          float pos[2]);
struct ImBuf *BKE_tracking_get_pattern_imbuf(const struct ImBuf *ibuf,
                                             const struct MovieTrackingTrack *track,
                                             const struct MovieTrackingMarker *marker,
                                             bool anchored,
                                             bool disable_channels);
struct ImBuf *BKE_tracking_get_search_imbuf(const struct ImBuf *ibuf,
                                            const struct MovieTrackingTrack *track,
                                            const struct MovieTrackingMarker *marker,
                                            bool anchored,
                                            bool disable_channels);

/* Create a new image buffer which consists of pixels which the plane marker "sees".
 * The function will choose best image resolution based on the plane marker size. */
struct ImBuf *BKE_tracking_get_plane_imbuf(const struct ImBuf *frame_ibuf,
                                           const struct MovieTrackingPlaneMarker *plane_marker);

/**
 * Zap channels from the imbuf that are disabled by the user. this can lead to
 * better tracks sometimes. however, instead of simply zeroing the channels
 * out, do a partial gray-scale conversion so the display is better.
 */
void BKE_tracking_disable_channels(
    struct ImBuf *ibuf, bool disable_red, bool disable_green, bool disable_blue, bool grayscale);

/* --------------------------------------------------------------------
 * 2D tracking.
 */

/**
 * Refine marker's position using previously known keyframe.
 * Direction of searching for a keyframe depends on backwards flag,
 * which means if backwards is false, previous keyframe will be as reference.
 */
void BKE_tracking_refine_marker(struct MovieClip *clip,
                                struct MovieTrackingTrack *track,
                                struct MovieTrackingMarker *marker,
                                bool backwards);

/* --------------------------------------------------------------------
 * 2D tracking using auto-track pipeline.
 */

struct AutoTrackContext *BKE_autotrack_context_new(struct MovieClip *clip,
                                                   struct MovieClipUser *user,
                                                   bool is_backwards);
void BKE_autotrack_context_start(struct AutoTrackContext *context);
bool BKE_autotrack_context_step(struct AutoTrackContext *context);
void BKE_autotrack_context_sync(struct AutoTrackContext *context);
void BKE_autotrack_context_sync_user(struct AutoTrackContext *context, struct MovieClipUser *user);
void BKE_autotrack_context_finish(struct AutoTrackContext *context);
void BKE_autotrack_context_free(struct AutoTrackContext *context);

/* --------------------------------------------------------------------
 * Plane tracking.
 */

/**
 * \note frame number should be in clip space, not scene space.
 */
void BKE_tracking_track_plane_from_existing_motion(struct MovieTrackingPlaneTrack *plane_track,
                                                   int start_frame);
void BKE_tracking_retrack_plane_from_existing_motion_at_segment(
    struct MovieTrackingPlaneTrack *plane_track, int start_frame);
void BKE_tracking_homography_between_two_quads(/*const*/ float reference_corners[4][2],
                                               /*const*/ float corners[4][2],
                                               float H[3][3]);

/* --------------------------------------------------------------------
 * Camera solving.
 */

/**
 * Perform early check on whether everything is fine to start reconstruction.
 */
bool BKE_tracking_reconstruction_check(struct MovieTracking *tracking,
                                       struct MovieTrackingObject *tracking_object,
                                       char *error_msg,
                                       int error_size);

/**
 * Create context for camera/object motion reconstruction.
 * Copies all data needed for reconstruction from movie clip datablock,
 * so editing this clip is safe during reconstruction job is in progress.
 */
struct MovieReconstructContext *BKE_tracking_reconstruction_context_new(
    struct MovieClip *clip,
    struct MovieTrackingObject *tracking_object,
    int keyframe1,
    int keyframe2,
    int width,
    int height);
/**
 * Free memory used by a reconstruction process.
 */
void BKE_tracking_reconstruction_context_free(struct MovieReconstructContext *context);
/**
 * Solve camera/object motion and reconstruct 3D markers position
 * from a prepared reconstruction context.
 *
 * stop is not actually used at this moment, so reconstruction
 * job could not be stopped.
 *
 * do_update, progress and stat_message are set by reconstruction
 * callback in libmv side and passing to an interface.
 */
void BKE_tracking_reconstruction_solve(struct MovieReconstructContext *context,
                                       bool *stop,
                                       bool *do_update,
                                       float *progress,
                                       char *stats_message,
                                       int message_size);
/**
 * Finish reconstruction process by copying reconstructed data to an actual movie clip data-block.
 */
bool BKE_tracking_reconstruction_finish(struct MovieReconstructContext *context,
                                        struct MovieTracking *tracking);

void BKE_tracking_reconstruction_report_error_message(struct MovieReconstructContext *context,
                                                      const char *error_message);

const char *BKE_tracking_reconstruction_error_message_get(
    const struct MovieReconstructContext *context);

/**
 * Apply scale on all reconstructed cameras and bundles, used by camera scale apply operator.
 */
void BKE_tracking_reconstruction_scale(struct MovieTracking *tracking, float scale[3]);

/* **** Feature detection **** */

/**
 * Detect features using FAST detector.
 */
void BKE_tracking_detect_fast(struct MovieTracking *tracking,
                              struct ListBase *tracksbase,
                              struct ImBuf *ibuf,
                              int framenr,
                              int margin,
                              int min_trackness,
                              int min_distance,
                              struct bGPDlayer *layer,
                              bool place_outside_layer);

/**
 * Detect features using Harris detector.
 */
void BKE_tracking_detect_harris(struct MovieTracking *tracking,
                                struct ListBase *tracksbase,
                                struct ImBuf *ibuf,
                                int framenr,
                                int margin,
                                float threshold,
                                int min_distance,
                                struct bGPDlayer *layer,
                                bool place_outside_layer);

/* --------------------------------------------------------------------
 * 2D stabilization.
 */

/**
 * Get stabilization data (translation, scaling and angle) for a given frame.
 * Returned data describes how to compensate the detected movement, but with any
 * chosen scale factor already applied and any target frame position already compensated.
 * In case stabilization fails or is disabled, neutral values are returned.
 *
 * \param framenr: is a frame number, relative to the clip (not relative to the scene timeline).
 * \param width: is an effective width of the canvas (square pixels), used to scale the
 * determined translation.
 *
 * Outputs:
 * \param translation: of the lateral shift, absolute canvas coordinates (square pixels).
 * \param scale: of the scaling to apply.
 * \param angle: of the rotation angle, relative to the frame center.
 *
 * TODO(sergey): Use `r_` prefix for output parameters here.
 */
void BKE_tracking_stabilization_data_get(struct MovieClip *clip,
                                         int framenr,
                                         int width,
                                         int height,
                                         float translation[2],
                                         float *scale,
                                         float *angle);
/**
 * Stabilize given image buffer using stabilization data for a specified frame number.
 *
 * \note frame number should be in clip space, not scene space.
 *
 * TODO(sergey): Use `r_` prefix for output parameters here.
 */
struct ImBuf *BKE_tracking_stabilize_frame(struct MovieClip *clip,
                                           int framenr,
                                           struct ImBuf *ibuf,
                                           float translation[2],
                                           float *scale,
                                           float *angle);
/**
 * Build a 4x4 transformation matrix based on the given 2D stabilization data.
 * mat is a 4x4 matrix in homogeneous coordinates, adapted to the
 *     final image buffer size and compensated for pixel aspect ratio,
 *     ready for direct OpenGL drawing.
 *
 * TODO(sergey): The signature of this function should be changed. we actually
 *               don't need the dimensions of the image buffer. Instead we
 *               should consider to provide the pivot point of the rotation as a
 *               further stabilization data parameter.
 */
void BKE_tracking_stabilization_data_to_mat4(int width,
                                             int height,
                                             float aspect,
                                             float translation[2],
                                             float scale,
                                             float angle,
                                             float mat[4][4]);

/* Dope-sheet */

/**
 * Tag dope-sheet for update, actual update will happen later when it'll be actually needed.
 */
void BKE_tracking_dopesheet_tag_update(struct MovieTracking *tracking);
/**
 * Do dope-sheet update, if update is not needed nothing will happen.
 */
void BKE_tracking_dopesheet_update(struct MovieTracking *tracking);

/* --------------------------------------------------------------------
 * Query and search.
 */

/**
 * \note Returns NULL if the track comes from camera object,.
 */
struct MovieTrackingObject *BKE_tracking_find_object_for_track(
    const struct MovieTracking *tracking, const struct MovieTrackingTrack *track);
struct MovieTrackingObject *BKE_tracking_find_object_for_plane_track(
    const struct MovieTracking *tracking, const struct MovieTrackingPlaneTrack *plane_track);

void BKE_tracking_get_rna_path_for_track(const struct MovieTracking *tracking,
                                         const struct MovieTrackingTrack *track,
                                         char *rna_path,
                                         size_t rna_path_maxncpy);
void BKE_tracking_get_rna_path_prefix_for_track(const struct MovieTracking *tracking,
                                                const struct MovieTrackingTrack *track,
                                                char *rna_path,
                                                size_t rna_path_maxncpy);
void BKE_tracking_get_rna_path_for_plane_track(const struct MovieTracking *tracking,
                                               const struct MovieTrackingPlaneTrack *plane_track,
                                               char *rna_path,
                                               size_t rna_path_len);
void BKE_tracking_get_rna_path_prefix_for_plane_track(
    const struct MovieTracking *tracking,
    const struct MovieTrackingPlaneTrack *plane_track,
    char *rna_path,
    size_t rna_path_maxncpy);

/* --------------------------------------------------------------------
 * Utility macros.
 */

#define TRACK_SELECTED(track) \
  ((track)->flag & SELECT || (track)->pat_flag & SELECT || (track)->search_flag & SELECT)

#define TRACK_AREA_SELECTED(track, area) \
  ((area) == TRACK_AREA_POINT ? \
       (track)->flag & SELECT : \
       ((area) == TRACK_AREA_PAT ? (track)->pat_flag & SELECT : (track)->search_flag & SELECT))

#define TRACK_VIEW_SELECTED(sc, track) \
  ((((track)->flag & TRACK_HIDDEN) == 0) && \
   (TRACK_AREA_SELECTED(track, TRACK_AREA_POINT) || \
    (((sc)->flag & SC_SHOW_MARKER_PATTERN) && TRACK_AREA_SELECTED(track, TRACK_AREA_PAT)) || \
    (((sc)->flag & SC_SHOW_MARKER_SEARCH) && TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH))))

#define PLANE_TRACK_VIEW_SELECTED(plane_track) \
  ((((plane_track)->flag & PLANE_TRACK_HIDDEN) == 0) && ((plane_track)->flag & SELECT))

#ifdef __cplusplus
}
#endif
