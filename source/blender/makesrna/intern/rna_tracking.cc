/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <climits>
#include <cstdlib>

#include "BLT_translation.hh"

#include "RNA_define.hh"

#include "rna_internal.hh"

#include "DNA_object_types.h" /* SELECT */
#include "DNA_scene_types.h"

#include "WM_types.hh"

#ifdef RNA_RUNTIME

#  include "DNA_anim_types.h"
#  include "DNA_defaults.h"
#  include "DNA_movieclip_types.h"

#  include "BLI_math_vector.h"

#  include "BKE_anim_data.hh"
#  include "BKE_animsys.h"
#  include "BKE_movieclip.h"
#  include "BKE_node.hh"
#  include "BKE_node_tree_update.hh"
#  include "BKE_report.hh"
#  include "BKE_tracking.h"

#  include "DEG_depsgraph.hh"

#  include "IMB_imbuf.hh"

#  include "WM_api.hh"

static std::optional<std::string> rna_tracking_path(const PointerRNA * /*ptr*/)
{
  return "tracking";
}

static std::optional<std::string> rna_trackingSettings_path(const PointerRNA * /*ptr*/)
{
  return "tracking.settings";
}

static void rna_tracking_defaultSettings_patternUpdate(Main * /*bmain*/,
                                                       Scene * /*scene*/,
                                                       PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingSettings *settings = &tracking->settings;

  if (settings->default_search_size < settings->default_pattern_size) {
    settings->default_search_size = settings->default_pattern_size;
  }
}

static void rna_tracking_defaultSettings_searchUpdate(Main * /*bmain*/,
                                                      Scene * /*scene*/,
                                                      PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingSettings *settings = &tracking->settings;

  if (settings->default_pattern_size > settings->default_search_size) {
    settings->default_pattern_size = settings->default_search_size;
  }
}

static std::optional<std::string> rna_trackingTrack_path(const PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;
  /* Escaped object name, escaped track name, rest of the path. */
  char rna_path[MAX_NAME * 4 + 64];
  BKE_tracking_get_rna_path_for_track(&clip->tracking, track, rna_path, sizeof(rna_path));
  return rna_path;
}

static void rna_trackingTracks_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&clip->tracking);

  rna_iterator_listbase_begin(iter, ptr, &tracking_camera_object->tracks, nullptr);
}

static void rna_trackingPlaneTracks_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&clip->tracking);

  rna_iterator_listbase_begin(iter, ptr, &tracking_camera_object->plane_tracks, nullptr);
}

static PointerRNA rna_trackingReconstruction_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&clip->tracking);

  return RNA_pointer_create_with_parent(
      *ptr, &RNA_MovieTrackingReconstruction, &tracking_camera_object->reconstruction);
}

static void rna_trackingObjects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;

  rna_iterator_listbase_begin(iter, ptr, &clip->tracking.objects, nullptr);
}

static int rna_tracking_active_object_index_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;

  return clip->tracking.objectnr;
}

static void rna_tracking_active_object_index_set(PointerRNA *ptr, int value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;

  clip->tracking.objectnr = value;
  BKE_tracking_dopesheet_tag_update(&clip->tracking);
}

static void rna_tracking_active_object_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;

  *min = 0;
  *max = max_ii(0, clip->tracking.tot_object - 1);
}

static PointerRNA rna_tracking_active_track_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  return RNA_pointer_create_with_parent(
      *ptr, &RNA_MovieTrackingTrack, tracking_object->active_track);
}

static void rna_tracking_active_track_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingTrack *track = (MovieTrackingTrack *)value.data;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  int index = BLI_findindex(&tracking_object->tracks, track);

  if (index != -1) {
    tracking_object->active_track = track;
  }
  else {
    BKE_reportf(reports,
                RPT_ERROR,
                "Track '%s' is not found in the tracking object %s",
                track->name,
                tracking_object->name);
  }
}

static PointerRNA rna_tracking_active_plane_track_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  return RNA_pointer_create_with_parent(
      *ptr, &RNA_MovieTrackingPlaneTrack, tracking_object->active_plane_track);
}

static void rna_tracking_active_plane_track_set(PointerRNA *ptr,
                                                PointerRNA value,
                                                ReportList *reports)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingPlaneTrack *plane_track = (MovieTrackingPlaneTrack *)value.data;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  int index = BLI_findindex(&tracking_object->plane_tracks, plane_track);

  if (index != -1) {
    tracking_object->active_plane_track = plane_track;
  }
  else {
    BKE_reportf(reports,
                RPT_ERROR,
                "Plane track '%s' is not found in the tracking object %s",
                plane_track->name,
                tracking_object->name);
  }
}

static PointerRNA rna_tracking_object_active_track_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  return RNA_pointer_create_with_parent(
      *ptr, &RNA_MovieTrackingTrack, tracking_object->active_track);
}

static void rna_tracking_object_active_track_set(PointerRNA *ptr,
                                                 PointerRNA value,
                                                 ReportList *reports)
{
  MovieTrackingTrack *track = (MovieTrackingTrack *)value.data;
  MovieTrackingObject *tracking_object = (MovieTrackingObject *)ptr->data;
  int index = BLI_findindex(&tracking_object->tracks, track);

  if (index != -1) {
    tracking_object->active_track = track;
  }
  else {
    BKE_reportf(reports,
                RPT_ERROR,
                "Track '%s' is not found in the tracking object %s",
                track->name,
                tracking_object->name);
  }
}

static PointerRNA rna_tracking_object_active_plane_track_get(PointerRNA *ptr)
{
  MovieTrackingObject *tracking_object = (MovieTrackingObject *)ptr->data;

  return RNA_pointer_create_with_parent(
      *ptr, &RNA_MovieTrackingPlaneTrack, tracking_object->active_plane_track);
}

static void rna_tracking_object_active_plane_track_set(PointerRNA *ptr,
                                                       PointerRNA value,
                                                       ReportList *reports)
{
  MovieTrackingPlaneTrack *plane_track = (MovieTrackingPlaneTrack *)value.data;
  MovieTrackingObject *tracking_object = (MovieTrackingObject *)ptr->data;
  int index = BLI_findindex(&tracking_object->plane_tracks, plane_track);

  if (index != -1) {
    tracking_object->active_plane_track = plane_track;
  }
  else {
    BKE_reportf(reports,
                RPT_ERROR,
                "Plane track '%s' is not found in the tracking object %s",
                plane_track->name,
                tracking_object->name);
  }
}

static void rna_trackingTrack_name_set(PointerRNA *ptr, const char *value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;
  MovieTrackingObject *tracking_object = BKE_tracking_find_object_for_track(&clip->tracking,
                                                                            track);
  /* Store old name, for the animation fix later. */
  char old_name[sizeof(track->name)];
  STRNCPY_UTF8(old_name, track->name);
  /* Update the name, */
  STRNCPY_UTF8(track->name, value);
  BKE_tracking_track_unique_name(&tracking_object->tracks, track);
  /* Fix animation paths. */
  AnimData *adt = BKE_animdata_from_id(&clip->id);
  if (adt != nullptr) {
    char rna_path_prefix[MAX_NAME * 2 + 64];
    BKE_tracking_get_rna_path_prefix_for_track(
        &clip->tracking, track, rna_path_prefix, sizeof(rna_path_prefix));
    BKE_animdata_fix_paths_rename(
        &clip->id, adt, nullptr, rna_path_prefix, old_name, track->name, 0, 0, 1);
  }
}

static bool rna_trackingTrack_select_get(PointerRNA *ptr)
{
  MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;

  return TRACK_SELECTED(track);
}

static void rna_trackingTrack_select_set(PointerRNA *ptr, bool value)
{
  MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;

  if (value) {
    track->flag |= SELECT;
    track->pat_flag |= SELECT;
    track->search_flag |= SELECT;
  }
  else {
    track->flag &= ~SELECT;
    track->pat_flag &= ~SELECT;
    track->search_flag &= ~SELECT;
  }
}

static void rna_trackingPlaneMarker_frame_set(PointerRNA *ptr, int value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingPlaneMarker *plane_marker = (MovieTrackingPlaneMarker *)ptr->data;
  MovieTrackingPlaneTrack *plane_track_of_marker = nullptr;

  LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
    LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &tracking_object->plane_tracks) {
      if (plane_marker >= plane_track->markers &&
          plane_marker < plane_track->markers + plane_track->markersnr)
      {
        plane_track_of_marker = plane_track;
        break;
      }
    }

    if (plane_track_of_marker) {
      break;
    }
  }

  if (plane_track_of_marker) {
    MovieTrackingPlaneMarker new_plane_marker = *plane_marker;
    new_plane_marker.framenr = value;

    BKE_tracking_plane_marker_delete(plane_track_of_marker, plane_marker->framenr);
    BKE_tracking_plane_marker_insert(plane_track_of_marker, &new_plane_marker);
  }
}

static std::optional<std::string> rna_trackingPlaneTrack_path(const PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingPlaneTrack *plane_track = (MovieTrackingPlaneTrack *)ptr->data;
  /* Escaped object name, escaped track name, rest of the path. */
  char rna_path[MAX_NAME * 4 + 64];
  BKE_tracking_get_rna_path_for_plane_track(
      &clip->tracking, plane_track, rna_path, sizeof(rna_path));
  return rna_path;
}

static void rna_trackingPlaneTrack_name_set(PointerRNA *ptr, const char *value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingPlaneTrack *plane_track = (MovieTrackingPlaneTrack *)ptr->data;
  MovieTrackingObject *tracking_object = BKE_tracking_find_object_for_plane_track(&clip->tracking,
                                                                                  plane_track);
  /* Store old name, for the animation fix later. */
  char old_name[sizeof(plane_track->name)];
  STRNCPY(old_name, plane_track->name);
  /* Update the name, */
  STRNCPY(plane_track->name, value);
  BKE_tracking_plane_track_unique_name(&tracking_object->plane_tracks, plane_track);
  /* Fix animation paths. */
  AnimData *adt = BKE_animdata_from_id(&clip->id);
  if (adt != nullptr) {
    char rna_path[MAX_NAME * 2 + 64];
    BKE_tracking_get_rna_path_prefix_for_plane_track(
        &clip->tracking, plane_track, rna_path, sizeof(rna_path));
    BKE_animdata_fix_paths_rename(
        &clip->id, adt, nullptr, rna_path, old_name, plane_track->name, 0, 0, 1);
  }
}

static std::optional<std::string> rna_trackingCamera_path(const PointerRNA * /*ptr*/)
{
  return "tracking.camera";
}

static float rna_trackingCamera_focal_mm_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingCamera *camera = &clip->tracking.camera;
  float val = camera->focal;

  if (clip->lastsize[0]) {
    val = val * camera->sensor_width / float(clip->lastsize[0]);
  }

  return val;
}

static void rna_trackingCamera_focal_mm_set(PointerRNA *ptr, float value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingCamera *camera = &clip->tracking.camera;

  if (clip->lastsize[0]) {
    value = clip->lastsize[0] * value / camera->sensor_width;
  }

  if (value >= 0.0001f) {
    camera->focal = value;
  }
}

static void rna_trackingCamera_principal_point_pixels_get(PointerRNA *ptr,
                                                          float *r_principal_point_pixels)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  BKE_tracking_camera_principal_point_pixel_get(clip, r_principal_point_pixels);
}

static void rna_trackingCamera_principal_point_pixels_set(PointerRNA *ptr,
                                                          const float *principal_point_pixels)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  BKE_tracking_camera_principal_point_pixel_set(clip, principal_point_pixels);
}

static std::optional<std::string> rna_trackingStabilization_path(const PointerRNA * /*ptr*/)
{
  return "tracking.stabilization";
}

static bool rna_track_2d_stabilization(CollectionPropertyIterator * /*iter*/, void *data)
{
  MovieTrackingTrack *track = (MovieTrackingTrack *)data;

  if ((track->flag & TRACK_USE_2D_STAB) == 0) {
    return true;
  }

  return false;
}

static bool rna_track_2d_stabilization_rotation(CollectionPropertyIterator * /*iter*/, void *data)
{
  MovieTrackingTrack *track = (MovieTrackingTrack *)data;

  if ((track->flag & TRACK_USE_2D_STAB_ROT) == 0) {
    return true;
  }

  return false;
}

static void rna_tracking_stabTracks_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&clip->tracking);
  rna_iterator_listbase_begin(
      iter, ptr, &tracking_camera_object->tracks, rna_track_2d_stabilization);
}

static int rna_tracking_stabTracks_active_index_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  return clip->tracking.stabilization.act_track;
}

static void rna_tracking_stabTracks_active_index_set(PointerRNA *ptr, int value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  clip->tracking.stabilization.act_track = value;
}

static void rna_tracking_stabTracks_active_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;

  *min = 0;
  *max = max_ii(0, clip->tracking.stabilization.tot_track - 1);
}

static void rna_tracking_stabRotTracks_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&clip->tracking);
  rna_iterator_listbase_begin(
      iter, ptr, &tracking_camera_object->tracks, rna_track_2d_stabilization_rotation);
}

static int rna_tracking_stabRotTracks_active_index_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  return clip->tracking.stabilization.act_rot_track;
}

static void rna_tracking_stabRotTracks_active_index_set(PointerRNA *ptr, int value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  clip->tracking.stabilization.act_rot_track = value;
}

static void rna_tracking_stabRotTracks_active_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;

  *min = 0;
  *max = max_ii(0, clip->tracking.stabilization.tot_rot_track - 1);
}

static void rna_tracking_flushUpdate(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;

  BKE_ntree_update_tag_id_changed(bmain, &clip->id);
  BKE_ntree_update(*bmain);

  WM_main_add_notifier(NC_SCENE | ND_NODES, nullptr);
  WM_main_add_notifier(NC_SCENE, nullptr);
  DEG_id_tag_update(&clip->id, 0);
}

static void rna_tracking_resetIntrinsics(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTracking *tracking = &clip->tracking;

  if (tracking->camera.intrinsics) {
    BKE_tracking_distortion_free(static_cast<MovieDistortion *>(tracking->camera.intrinsics));
    tracking->camera.intrinsics = nullptr;
  }
}

static void rna_trackingObject_tracks_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  MovieTrackingObject *tracking_object = (MovieTrackingObject *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &tracking_object->tracks, nullptr);
}

static void rna_trackingObject_plane_tracks_begin(CollectionPropertyIterator *iter,
                                                  PointerRNA *ptr)
{
  MovieTrackingObject *tracking_object = (MovieTrackingObject *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &tracking_object->plane_tracks, nullptr);
}

static PointerRNA rna_trackingObject_reconstruction_get(PointerRNA *ptr)
{
  MovieTrackingObject *tracking_object = (MovieTrackingObject *)ptr->data;
  return RNA_pointer_create_with_parent(
      *ptr, &RNA_MovieTrackingReconstruction, &tracking_object->reconstruction);
}

static PointerRNA rna_tracking_active_object_get(PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingObject *tracking_object = static_cast<MovieTrackingObject *>(
      BLI_findlink(&clip->tracking.objects, clip->tracking.objectnr));

  return RNA_pointer_create_with_parent(*ptr, &RNA_MovieTrackingObject, tracking_object);
}

static void rna_tracking_active_object_set(PointerRNA *ptr,
                                           PointerRNA value,
                                           ReportList * /*reports*/)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingObject *tracking_object = (MovieTrackingObject *)value.data;
  const int index = BLI_findindex(&clip->tracking.objects, tracking_object);

  if (index != -1) {
    clip->tracking.objectnr = index;
  }
  else {
    clip->tracking.objectnr = 0;
  }
}

static void rna_trackingObject_name_set(PointerRNA *ptr, const char *value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingObject *tracking_object = (MovieTrackingObject *)ptr->data;

  STRNCPY_UTF8(tracking_object->name, value);

  BKE_tracking_object_unique_name(&clip->tracking, tracking_object);
}

static void rna_trackingObject_flushUpdate(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;

  WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, nullptr);
  DEG_id_tag_update(&clip->id, 0);
}

static void rna_trackingMarker_frame_set(PointerRNA *ptr, int value)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingMarker *marker = (MovieTrackingMarker *)ptr->data;
  MovieTrackingTrack *track_of_marker = nullptr;

  LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      if (marker >= track->markers && marker < track->markers + track->markersnr) {
        track_of_marker = track;
        break;
      }
    }

    if (track_of_marker) {
      break;
    }
  }

  if (track_of_marker) {
    MovieTrackingMarker new_marker = *marker;
    new_marker.framenr = value;

    BKE_tracking_marker_delete(track_of_marker, marker->framenr);
    BKE_tracking_marker_insert(track_of_marker, &new_marker);
  }
}

static void rna_tracking_markerPattern_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  MovieTrackingMarker *marker = (MovieTrackingMarker *)ptr->data;

  BKE_tracking_marker_clamp_search_size(marker);
}

static void rna_tracking_markerSearch_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  MovieTrackingMarker *marker = (MovieTrackingMarker *)ptr->data;

  BKE_tracking_marker_clamp_search_size(marker);
}

static void rna_tracking_markerPattern_boundbox_get(PointerRNA *ptr, float *values)
{
  MovieTrackingMarker *marker = (MovieTrackingMarker *)ptr->data;
  float min[2], max[2];

  BKE_tracking_marker_pattern_minmax(marker, min, max);

  copy_v2_v2(values, min);
  copy_v2_v2(values + 2, max);
}

static std::optional<std::string> rna_trackingDopesheet_path(const PointerRNA * /*ptr*/)
{
  return "tracking.dopesheet";
}

static void rna_trackingDopesheet_tagUpdate(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  MovieClip *clip = (MovieClip *)ptr->owner_id;
  MovieTrackingDopesheet *dopesheet = &clip->tracking.dopesheet;

  dopesheet->ok = 0;
}

/* API */

static MovieTrackingTrack *add_track_to_base(
    MovieClip *clip, MovieTracking *tracking, ListBase *tracksbase, const char *name, int frame)
{
  int width, height;
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
  MovieTrackingTrack *track;

  user.framenr = 1;

  BKE_movieclip_get_size(clip, &user, &width, &height);

  track = BKE_tracking_track_add(tracking, tracksbase, 0, 0, frame, width, height);

  if (name && name[0]) {
    STRNCPY_UTF8(track->name, name);
    BKE_tracking_track_unique_name(tracksbase, track);
  }

  return track;
}

static MovieTrackingTrack *rna_trackingTracks_new(ID *id,
                                                  MovieTracking *tracking,
                                                  const char *name,
                                                  int frame)
{
  MovieClip *clip = (MovieClip *)id;
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&clip->tracking);
  MovieTrackingTrack *track = add_track_to_base(
      clip, tracking, &tracking_camera_object->tracks, name, frame);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, clip);

  return track;
}

static MovieTrackingTrack *rna_trackingObject_tracks_new(ID *id,
                                                         MovieTrackingObject *tracking_object,
                                                         const char *name,
                                                         int frame)
{
  MovieClip *clip = (MovieClip *)id;
  MovieTrackingTrack *track = add_track_to_base(
      clip, &clip->tracking, &tracking_object->tracks, name, frame);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, nullptr);

  return track;
}

static MovieTrackingObject *rna_trackingObject_new(MovieTracking *tracking, const char *name)
{
  MovieTrackingObject *tracking_object = BKE_tracking_object_add(tracking, name);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, nullptr);

  return tracking_object;
}

static void rna_trackingObject_remove(MovieTracking *tracking,
                                      ReportList *reports,
                                      PointerRNA *object_ptr)
{
  MovieTrackingObject *tracking_object = static_cast<MovieTrackingObject *>(object_ptr->data);
  if (BKE_tracking_object_delete(tracking, tracking_object) == false) {
    BKE_reportf(reports, RPT_ERROR, "MovieTracking '%s' cannot be removed", tracking_object->name);
    return;
  }

  object_ptr->invalidate();

  WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, nullptr);
}

static MovieTrackingMarker *rna_trackingMarkers_find_frame(MovieTrackingTrack *track,
                                                           int framenr,
                                                           bool exact)
{
  if (exact) {
    return BKE_tracking_marker_get_exact(track, framenr);
  }
  else {
    return BKE_tracking_marker_get(track, framenr);
  }
}

static MovieTrackingMarker *rna_trackingMarkers_insert_frame(MovieTrackingTrack *track,
                                                             int framenr,
                                                             const float co[2])
{
  MovieTrackingMarker marker = {}, *new_marker;

  marker.framenr = framenr;
  copy_v2_v2(marker.pos, co);

  /* a bit arbitrary, but better than creating markers with zero pattern
   * which is forbidden actually
   */
  copy_v2_v2(marker.pattern_corners[0], track->markers[0].pattern_corners[0]);
  copy_v2_v2(marker.pattern_corners[1], track->markers[0].pattern_corners[1]);
  copy_v2_v2(marker.pattern_corners[2], track->markers[0].pattern_corners[2]);
  copy_v2_v2(marker.pattern_corners[3], track->markers[0].pattern_corners[3]);

  new_marker = BKE_tracking_marker_insert(track, &marker);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, nullptr);

  return new_marker;
}

static void rna_trackingMarkers_delete_frame(MovieTrackingTrack *track, int framenr)
{
  if (track->markersnr == 1) {
    return;
  }

  BKE_tracking_marker_delete(track, framenr);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, nullptr);
}

static MovieTrackingPlaneMarker *rna_trackingPlaneMarkers_find_frame(
    MovieTrackingPlaneTrack *plane_track, int framenr, bool exact)
{
  if (exact) {
    return BKE_tracking_plane_marker_get_exact(plane_track, framenr);
  }
  else {
    return BKE_tracking_plane_marker_get(plane_track, framenr);
  }
}

static MovieTrackingPlaneMarker *rna_trackingPlaneMarkers_insert_frame(
    MovieTrackingPlaneTrack *plane_track, int framenr)
{
  MovieTrackingPlaneMarker plane_marker = {}, *new_plane_marker;

  plane_marker.framenr = framenr;

  /* a bit arbitrary, but better than creating zero markers */
  copy_v2_v2(plane_marker.corners[0], plane_track->markers[0].corners[0]);
  copy_v2_v2(plane_marker.corners[1], plane_track->markers[0].corners[1]);
  copy_v2_v2(plane_marker.corners[2], plane_track->markers[0].corners[2]);
  copy_v2_v2(plane_marker.corners[3], plane_track->markers[0].corners[3]);

  new_plane_marker = BKE_tracking_plane_marker_insert(plane_track, &plane_marker);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, nullptr);

  return new_plane_marker;
}

static void rna_trackingPlaneMarkers_delete_frame(MovieTrackingPlaneTrack *plane_track,
                                                  int framenr)
{
  if (plane_track->markersnr == 1) {
    return;
  }

  BKE_tracking_plane_marker_delete(plane_track, framenr);

  WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, nullptr);
}

static MovieTrackingObject *find_object_for_reconstruction(
    MovieTracking *tracking, MovieTrackingReconstruction *reconstruction)
{
  LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
    if (&tracking_object->reconstruction == reconstruction) {
      return tracking_object;
    }
  }

  return nullptr;
}

static MovieReconstructedCamera *rna_trackingCameras_find_frame(
    ID *id, MovieTrackingReconstruction *reconstruction, int framenr)
{
  MovieClip *clip = (MovieClip *)id;
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = find_object_for_reconstruction(tracking, reconstruction);
  return BKE_tracking_camera_get_reconstructed(tracking, tracking_object, framenr);
}

static void rna_trackingCameras_matrix_from_frame(ID *id,
                                                  MovieTrackingReconstruction *reconstruction,
                                                  int framenr,
                                                  float matrix[16])
{
  float mat[4][4];

  MovieClip *clip = (MovieClip *)id;
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = find_object_for_reconstruction(tracking, reconstruction);
  BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object, framenr, mat);

  memcpy(matrix, mat, sizeof(float[4][4]));
}

#else

static const EnumPropertyItem tracker_motion_model[] = {
    {TRACK_MOTION_MODEL_HOMOGRAPHY,
     "Perspective",
     0,
     "Perspective",
     "Search for markers that are perspectively deformed (homography) between frames"},
    {TRACK_MOTION_MODEL_AFFINE,
     "Affine",
     0,
     "Affine",
     "Search for markers that are affine-deformed (t, r, k, and skew) between frames"},
    {TRACK_MOTION_MODEL_TRANSLATION_ROTATION_SCALE,
     "LocRotScale",
     0,
     "Location, Rotation & Scale",
     "Search for markers that are translated, rotated, and scaled between frames"},
    {TRACK_MOTION_MODEL_TRANSLATION_SCALE,
     "LocScale",
     0,
     "Location & Scale",
     "Search for markers that are translated and scaled between frames"},
    {TRACK_MOTION_MODEL_TRANSLATION_ROTATION,
     "LocRot",
     0,
     "Location & Rotation",
     "Search for markers that are translated and rotated between frames"},
    {TRACK_MOTION_MODEL_TRANSLATION,
     "Loc",
     0,
     "Location",
     "Search for markers that are translated between frames"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem pattern_match_items[] = {
    {TRACK_MATCH_KEYFRAME, "KEYFRAME", 0, "Keyframe", "Track pattern from keyframe to next frame"},
    {TRACK_MATCH_PREVIOUS_FRAME,
     "PREV_FRAME",
     0,
     "Previous frame",
     "Track pattern from current frame to next frame"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_trackingSettings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem speed_items[] = {
      {0, "FASTEST", 0, "Fastest", "Track as fast as possible"},
      {TRACKING_SPEED_DOUBLE, "DOUBLE", 0, "Double", "Track with double speed"},
      {TRACKING_SPEED_REALTIME, "REALTIME", 0, "Realtime", "Track with realtime speed"},
      {TRACKING_SPEED_HALF, "HALF", 0, "Half", "Track with half of realtime speed"},
      {TRACKING_SPEED_QUARTER, "QUARTER", 0, "Quarter", "Track with quarter of realtime speed"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem cleanup_items[] = {
      {TRACKING_CLEAN_SELECT, "SELECT", 0, "Select", "Select unclean tracks"},
      {TRACKING_CLEAN_DELETE_TRACK, "DELETE_TRACK", 0, "Delete Track", "Delete unclean tracks"},
      {TRACKING_CLEAN_DELETE_SEGMENT,
       "DELETE_SEGMENTS",
       0,
       "Delete Segments",
       "Delete unclean segments of tracks"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MovieTrackingSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_trackingSettings_path");
  RNA_def_struct_ui_text(srna, "Movie tracking settings", "Match moving settings");

  /* speed */
  prop = RNA_def_property(srna, "speed", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, speed_items);
  RNA_def_property_ui_text(prop,
                           "Speed",
                           "Limit speed of tracking to make visual feedback easier "
                           "(this does not affect the tracking quality)");

  /* use keyframe selection */
  prop = RNA_def_property(srna, "use_keyframe_selection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "reconstruction_flag", TRACKING_USE_KEYFRAME_SELECTION);
  RNA_def_property_ui_text(prop,
                           "Keyframe Selection",
                           "Automatically select keyframes when solving camera/object motion");

  /* intrinsics refinement during bundle adjustment */

  prop = RNA_def_property(srna, "refine_intrinsics_focal_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "refine_camera_intrinsics", REFINE_FOCAL_LENGTH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Refine Focal Length", "Refine focal length during camera solving");

  prop = RNA_def_property(srna, "refine_intrinsics_principal_point", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "refine_camera_intrinsics", REFINE_PRINCIPAL_POINT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Refine Principal Point", "Refine principal point during camera solving");

  prop = RNA_def_property(srna, "refine_intrinsics_radial_distortion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "refine_camera_intrinsics", REFINE_RADIAL_DISTORTION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Refine Radial",
                           "Refine radial coefficients of distortion model during camera solving");

  prop = RNA_def_property(
      srna, "refine_intrinsics_tangential_distortion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "refine_camera_intrinsics", REFINE_TANGENTIAL_DISTORTION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Refine Tangential",
      "Refine tangential coefficients of distortion model during camera solving");

  /* tool settings */

  /* distance */
  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "dist");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(
      prop, "Distance", "Distance between two bundles used for scene scaling");

  /* frames count */
  prop = RNA_def_property(srna, "clean_frames", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "clean_frames");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Tracked Frames",
      "Effect on tracks which are tracked less than the specified amount of frames");

  /* re-projection error */
  prop = RNA_def_property(srna, "clean_error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "clean_error");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Reprojection Error", "Effect on tracks which have a larger re-projection error");

  /* cleanup action */
  prop = RNA_def_property(srna, "clean_action", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "clean_action");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, cleanup_items);
  RNA_def_property_ui_text(prop, "Action", "Cleanup action to execute");

  /* solver settings */
  prop = RNA_def_property(srna, "use_tripod_solver", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "motion_flag", TRACKING_MOTION_TRIPOD);
  RNA_def_property_ui_text(
      prop,
      "Tripod Motion",
      "Use special solver to track a stable camera position, such as a tripod");

  /* default_limit_frames */
  prop = RNA_def_property(srna, "default_frames_limit", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "default_frames_limit");
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Frames Limit", "Every tracking cycle, this number of frames are tracked");

  /* default_pattern_match */
  prop = RNA_def_property(srna, "default_pattern_match", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_sdna(prop, nullptr, "default_pattern_match");
  RNA_def_property_enum_items(prop, pattern_match_items);
  RNA_def_property_ui_text(
      prop, "Pattern Match", "Track pattern from given frame when tracking marker to next frame");

  /* default_margin */
  prop = RNA_def_property(srna, "default_margin", PROP_INT, PROP_PIXEL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "default_margin");
  RNA_def_property_range(prop, 0, 300);
  RNA_def_property_ui_text(
      prop, "Margin", "Default distance from image boundary at which marker stops tracking");

  /* default_tracking_motion_model */
  prop = RNA_def_property(srna, "default_motion_model", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, tracker_motion_model);
  RNA_def_property_ui_text(prop, "Motion Model", "Default motion model to use for tracking");

  /* default_use_brute */
  prop = RNA_def_property(srna, "use_default_brute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "default_algorithm_flag", TRACK_ALGORITHM_FLAG_USE_BRUTE);
  RNA_def_property_ui_text(
      prop, "Prepass", "Use a brute-force translation-only initialization when tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* default_use_brute */
  prop = RNA_def_property(srna, "use_default_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "default_algorithm_flag", TRACK_ALGORITHM_FLAG_USE_MASK);
  RNA_def_property_ui_text(
      prop,
      "Use Mask",
      "Use a Grease Pencil data-block as a mask to use only specified areas of pattern "
      "when tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* default_use_normalization */
  prop = RNA_def_property(srna, "use_default_normalization", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "default_algorithm_flag", TRACK_ALGORITHM_FLAG_USE_NORMALIZATION);
  RNA_def_property_ui_text(
      prop, "Normalize", "Normalize light intensities while tracking (slower)");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* default minimal correlation */
  prop = RNA_def_property(srna, "default_correlation_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "default_minimum_correlation");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.05, 3);
  RNA_def_property_ui_text(
      prop,
      "Correlation",
      "Default minimum value of correlation between matched pattern and reference "
      "that is still treated as successful tracking");

  /* default pattern size */
  prop = RNA_def_property(srna, "default_pattern_size", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "default_pattern_size");
  RNA_def_property_range(prop, 5, 1000);
  RNA_def_property_update(prop, 0, "rna_tracking_defaultSettings_patternUpdate");
  RNA_def_property_ui_text(prop, "Pattern Size", "Size of pattern area for newly created tracks");

  /* default search size */
  prop = RNA_def_property(srna, "default_search_size", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "default_search_size");
  RNA_def_property_range(prop, 5, 1000);
  RNA_def_property_update(prop, 0, "rna_tracking_defaultSettings_searchUpdate");
  RNA_def_property_ui_text(prop, "Search Size", "Size of search area for newly created tracks");

  /* default use_red_channel */
  prop = RNA_def_property(srna, "use_default_red_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "default_flag", TRACK_DISABLE_RED);
  RNA_def_property_ui_text(prop, "Use Red Channel", "Use red channel from footage for tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* default_use_green_channel */
  prop = RNA_def_property(srna, "use_default_green_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "default_flag", TRACK_DISABLE_GREEN);
  RNA_def_property_ui_text(
      prop, "Use Green Channel", "Use green channel from footage for tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* default_use_blue_channel */
  prop = RNA_def_property(srna, "use_default_blue_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "default_flag", TRACK_DISABLE_BLUE);
  RNA_def_property_ui_text(prop, "Use Blue Channel", "Use blue channel from footage for tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  prop = RNA_def_property(srna, "default_weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Weight", "Influence of newly created track on a final solution");

  /* ** object tracking ** */

  /* object distance */
  prop = RNA_def_property(srna, "object_distance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "object_distance");
  RNA_def_property_ui_text(
      prop, "Distance", "Distance between two bundles used for object scaling");
  RNA_def_property_range(prop, 0.001, 10000);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_range(prop, 0.001, 10000.0, 1, 3);
}

static void rna_def_trackingCamera(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem distortion_model_items[] = {
      {TRACKING_DISTORTION_MODEL_POLYNOMIAL,
       "POLYNOMIAL",
       0,
       "Polynomial",
       "Radial distortion model which fits common cameras"},
      {TRACKING_DISTORTION_MODEL_DIVISION,
       "DIVISION",
       0,
       "Divisions",
       "Division distortion model which "
       "better represents wide-angle cameras"},
      {TRACKING_DISTORTION_MODEL_NUKE, "NUKE", 0, "Nuke", "Nuke distortion model"},
      {TRACKING_DISTORTION_MODEL_BROWN, "BROWN", 0, "Brown", "Brown-Conrady distortion model"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem camera_units_items[] = {
      {CAMERA_UNITS_PX, "PIXELS", 0, "px", "Use pixels for units of focal length"},
      {CAMERA_UNITS_MM, "MILLIMETERS", 0, "mm", "Use millimeters for units of focal length"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MovieTrackingCamera", nullptr);
  RNA_def_struct_path_func(srna, "rna_trackingCamera_path");
  RNA_def_struct_ui_text(
      srna, "Movie tracking camera data", "Match-moving camera data for tracking");

  /* Distortion model */
  prop = RNA_def_property(srna, "distortion_model", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, distortion_model_items);
  RNA_def_property_ui_text(prop, "Distortion Model", "Distortion model used for camera lenses");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_resetIntrinsics");

  /* Sensor */
  prop = RNA_def_property(srna, "sensor_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sensor_width");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 500.0f);
  RNA_def_property_ui_text(prop, "Sensor", "Width of CCD sensor in millimeters");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* Focal Length */
  prop = RNA_def_property(srna, "focal_length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "focal");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0001f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0001f, 5000.0f, 1, 2);
  RNA_def_property_float_funcs(
      prop, "rna_trackingCamera_focal_mm_get", "rna_trackingCamera_focal_mm_set", nullptr);
  RNA_def_property_ui_text(prop, "Focal Length", "Camera's focal length");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* Focal Length in pixels */
  prop = RNA_def_property(srna, "focal_length_pixels", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "focal");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 5000.0f, 1, 2);
  RNA_def_property_ui_text(prop, "Focal Length", "Camera's focal length");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* Units */
  prop = RNA_def_property(srna, "units", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "units");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, camera_units_items);
  RNA_def_property_ui_text(prop, "Units", "Units used for camera focal length");

  /* Principal Point */
  prop = RNA_def_property(srna, "principal_point", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_sdna(prop, nullptr, "principal_point");
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_range(prop, -1, 1, 0.1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Principal Point", "Optical center of lens");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* Principal Point, in pixels */
  prop = RNA_def_property(srna, "principal_point_pixels", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_array(prop, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_funcs(prop,
                               "rna_trackingCamera_principal_point_pixels_get",
                               "rna_trackingCamera_principal_point_pixels_set",
                               nullptr);
  RNA_def_property_ui_text(prop, "Principal Point", "Optical center of lens in pixels");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* Radial distortion parameters */
  prop = RNA_def_property(srna, "k1", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "k1");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "K1", "First coefficient of third order polynomial radial distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "k2", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "k2");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "K2", "Second coefficient of third order polynomial radial distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "k3", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "k3");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "K3", "Third coefficient of third order polynomial radial distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  /* Division distortion parameters */
  prop = RNA_def_property(srna, "division_k1", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(prop, "K1", "First coefficient of second order division distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "division_k2", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(prop, "K2", "Second coefficient of second order division distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  /* Nuke distortion parameters */
  prop = RNA_def_property(srna, "nuke_k1", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(prop, "K1", "First coefficient of second order Nuke distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "nuke_k2", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(prop, "K2", "Second coefficient of second order Nuke distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "nuke_p1", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(prop, "P1", "First coefficient of tangential Nuke distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "nuke_p2", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(prop, "P2", "Second coefficient of tangential Nuke distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  /* Brown-Conrady distortion parameters */
  prop = RNA_def_property(srna, "brown_k1", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "K1", "First coefficient of fourth order Brown-Conrady radial distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "brown_k2", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "K2", "Second coefficient of fourth order Brown-Conrady radial distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "brown_k3", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "K3", "Third coefficient of fourth order Brown-Conrady radial distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "brown_k4", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "K4", "Fourth coefficient of fourth order Brown-Conrady radial distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "brown_p1", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "P1", "First coefficient of second order Brown-Conrady tangential distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  prop = RNA_def_property(srna, "brown_p2", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, -10, 10, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "P2", "Second coefficient of second order Brown-Conrady tangential distortion");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_flushUpdate");

  /* pixel aspect */
  prop = RNA_def_property(srna, "pixel_aspect", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "pixel_aspect");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.1f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.1f, 5000.0f, 1, 2);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Pixel Aspect Ratio", "Pixel aspect ratio");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");
}

static void rna_def_trackingMarker(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static int boundbox_dimsize[] = {2, 2};

  srna = RNA_def_struct(brna, "MovieTrackingMarker", nullptr);
  RNA_def_struct_ui_text(
      srna, "Movie tracking marker data", "Match-moving marker data for tracking");

  /* position */
  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_float_sdna(prop, nullptr, "pos");
  RNA_def_property_ui_text(prop, "Position", "Marker position at frame in normalized coordinates");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* frame */
  prop = RNA_def_property(srna, "frame", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "framenr");
  RNA_def_property_ui_text(prop, "Frame", "Frame number marker is keyframed on");
  RNA_def_property_int_funcs(prop, nullptr, "rna_trackingMarker_frame_set", nullptr);
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* enable */
  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MARKER_DISABLED);
  RNA_def_property_ui_text(prop, "Mode", "Is marker muted for current frame");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* pattern */
  prop = RNA_def_property(srna, "pattern_corners", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "pattern_corners");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x2);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop,
                           "Pattern Corners",
                           "Array of coordinates which represents pattern's corners in "
                           "normalized coordinates relative to marker position");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_markerPattern_update");

  prop = RNA_def_property(srna, "pattern_bound_box", PROP_FLOAT, PROP_NONE);
  RNA_def_property_multi_array(prop, 2, boundbox_dimsize);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_tracking_markerPattern_boundbox_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Pattern Bounding Box", "Pattern area bounding box in normalized coordinates");

  /* search */
  prop = RNA_def_property(srna, "search_min", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_float_sdna(prop, nullptr, "search_min");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Search Min",
                           "Left-bottom corner of search area in normalized coordinates relative "
                           "to marker position");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_markerSearch_update");

  prop = RNA_def_property(srna, "search_max", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_float_sdna(prop, nullptr, "search_max");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Search Max",
                           "Right-bottom corner of search area in normalized coordinates relative "
                           "to marker position");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_tracking_markerSearch_update");

  /* is marker keyframed */
  prop = RNA_def_property(srna, "is_keyed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", MARKER_TRACKED);
  RNA_def_property_ui_text(
      prop, "Keyframed", "Whether the position of the marker is keyframed or tracked");
}

static void rna_def_trackingMarkers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MovieTrackingMarkers");
  srna = RNA_def_struct(brna, "MovieTrackingMarkers", nullptr);
  RNA_def_struct_sdna(srna, "MovieTrackingTrack");
  RNA_def_struct_ui_text(
      srna, "Movie Tracking Markers", "Collection of markers for movie tracking track");

  func = RNA_def_function(srna, "find_frame", "rna_trackingMarkers_find_frame");
  RNA_def_function_ui_description(func, "Get marker for specified frame");
  parm = RNA_def_int(func,
                     "frame",
                     1,
                     MINFRAME,
                     MAXFRAME,
                     "Frame",
                     "Frame number to find marker for",
                     MINFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func,
                  "exact",
                  true,
                  "Exact",
                  "Get marker at exact frame number rather than get estimated marker");
  parm = RNA_def_pointer(func, "marker", "MovieTrackingMarker", "", "Marker for specified frame");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "insert_frame", "rna_trackingMarkers_insert_frame");
  RNA_def_function_ui_description(func, "Insert a new marker at the specified frame");
  parm = RNA_def_int(func,
                     "frame",
                     1,
                     MINFRAME,
                     MAXFRAME,
                     "Frame",
                     "Frame number to insert marker to",
                     MINFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_float_vector(
      func,
      "co",
      2,
      nullptr,
      -1.0,
      1.0,
      "Coordinate",
      "Place new marker at the given frame using specified in normalized space coordinates",
      -1.0,
      1.0);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "marker", "MovieTrackingMarker", "", "Newly created marker");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "delete_frame", "rna_trackingMarkers_delete_frame");
  RNA_def_function_ui_description(func, "Delete marker at specified frame");
  parm = RNA_def_int(func,
                     "frame",
                     1,
                     MINFRAME,
                     MAXFRAME,
                     "Frame",
                     "Frame number to delete marker from",
                     MINFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_trackingTrack(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_trackingMarker(brna);

  srna = RNA_def_struct(brna, "MovieTrackingTrack", nullptr);
  RNA_def_struct_path_func(srna, "rna_trackingTrack_path");
  RNA_def_struct_ui_text(
      srna, "Movie tracking track data", "Match-moving track data for tracking");
  RNA_def_struct_ui_icon(srna, ICON_ANIM_DATA);

  /* name */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Unique name of track");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_trackingTrack_name_set");
  RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);
  RNA_def_struct_name_property(srna, prop);

  /* limit frames */
  prop = RNA_def_property(srna, "frames_limit", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "frames_limit");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Frames Limit", "Every tracking cycle, this number of frames are tracked");

  /* pattern match */
  prop = RNA_def_property(srna, "pattern_match", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_sdna(prop, nullptr, "pattern_match");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, pattern_match_items);
  RNA_def_property_ui_text(
      prop, "Pattern Match", "Track pattern from given frame when tracking marker to next frame");

  /* margin */
  prop = RNA_def_property(srna, "margin", PROP_INT, PROP_PIXEL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "margin");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 300);
  RNA_def_property_ui_text(
      prop, "Margin", "Distance from image boundary at which marker stops tracking");

  /* tracking motion model */
  prop = RNA_def_property(srna, "motion_model", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, tracker_motion_model);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Motion Model", "Default motion model to use for tracking");

  /* minimum correlation */
  prop = RNA_def_property(srna, "correlation_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "minimum_correlation");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.05, 3);
  RNA_def_property_ui_text(prop,
                           "Correlation",
                           "Minimal value of correlation between matched pattern and reference "
                           "that is still treated as successful tracking");

  /* use_brute */
  prop = RNA_def_property(srna, "use_brute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "algorithm_flag", TRACK_ALGORITHM_FLAG_USE_BRUTE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Prepass", "Use a brute-force translation only pre-track before refinement");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* use_brute */
  prop = RNA_def_property(srna, "use_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "algorithm_flag", TRACK_ALGORITHM_FLAG_USE_MASK);
  RNA_def_property_ui_text(
      prop,
      "Use Mask",
      "Use a Grease Pencil data-block as a mask to use only specified areas of pattern "
      "when tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* use_normalization */
  prop = RNA_def_property(srna, "use_normalization", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "algorithm_flag", TRACK_ALGORITHM_FLAG_USE_NORMALIZATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Normalize", "Normalize light intensities while tracking (slower)");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* markers */
  prop = RNA_def_property(srna, "markers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingMarker");
  RNA_def_property_collection_sdna(prop, nullptr, "markers", "markersnr");
  RNA_def_property_ui_text(prop, "Markers", "Collection of markers in track");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  rna_def_trackingMarkers(brna, prop);

  /* ** channels ** */

  /* use_red_channel */
  prop = RNA_def_property(srna, "use_red_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", TRACK_DISABLE_RED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Use Red Channel", "Use red channel from footage for tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* use_green_channel */
  prop = RNA_def_property(srna, "use_green_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", TRACK_DISABLE_GREEN);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Use Green Channel", "Use green channel from footage for tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* use_blue_channel */
  prop = RNA_def_property(srna, "use_blue_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", TRACK_DISABLE_BLUE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Use Blue Channel", "Use blue channel from footage for tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* preview_grayscale */
  prop = RNA_def_property(srna, "use_grayscale_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACK_PREVIEW_GRAYSCALE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Grayscale", "Display what the tracking algorithm sees in the preview");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* preview_alpha */
  prop = RNA_def_property(srna, "use_alpha_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACK_PREVIEW_ALPHA);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Alpha", "Apply track's mask on displaying preview");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* has bundle */
  prop = RNA_def_property(srna, "has_bundle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACK_HAS_BUNDLE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Has Bundle", "True if track has a valid bundle");

  /* bundle position */
  prop = RNA_def_property(srna, "bundle", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "bundle_pos");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Bundle", "Position of bundle reconstructed from this track");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

  /* hide */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACK_HIDDEN);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Hide", "Track is hidden");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* select */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_trackingTrack_select_get", "rna_trackingTrack_select_set");
  RNA_def_property_ui_text(prop, "Select", "Track is selected");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* select_anchor */
  prop = RNA_def_property(srna, "select_anchor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SELECT);
  RNA_def_property_ui_text(prop, "Select Anchor", "Track's anchor point is selected");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* select_pattern */
  prop = RNA_def_property(srna, "select_pattern", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pat_flag", SELECT);
  RNA_def_property_ui_text(prop, "Select Pattern", "Track's pattern area is selected");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* select_search */
  prop = RNA_def_property(srna, "select_search", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "search_flag", SELECT);
  RNA_def_property_ui_text(prop, "Select Search", "Track's search area is selected");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* locked */
  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACK_LOCKED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Lock", "Track is locked and all changes to it are disabled");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* custom color */
  prop = RNA_def_property(srna, "use_custom_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACK_CUSTOMCOLOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Custom Color", "Use custom color instead of theme-defined");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* color */
  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Color",
      "Color of the track in the Movie Clip Editor and the 3D viewport after a solve");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* average error */
  prop = RNA_def_property(srna, "average_error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "error");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Average Error", "Average error of re-projection");

  /* Annotations */
  prop = RNA_def_property(srna, "annotation", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gpd");
  RNA_def_property_struct_type(prop, "Annotation");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Annotation", "Annotation data for this track");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* weight */
  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "weight");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Weight", "Influence of this track on a final solution");

  /* weight_stab */
  prop = RNA_def_property(srna, "weight_stab", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "weight_stab");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Stab Weight", "Influence of this track on 2D stabilization");

  /* offset */
  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_float_sdna(prop, nullptr, "offset");
  RNA_def_property_ui_text(prop, "Offset", "Offset of track from the parenting point");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);
}

static void rna_def_trackingPlaneMarker(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MovieTrackingPlaneMarker", nullptr);
  RNA_def_struct_ui_text(
      srna, "Movie Tracking Plane Marker Data", "Match-moving plane marker data for tracking");

  /* frame */
  prop = RNA_def_property(srna, "frame", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "framenr");
  RNA_def_property_ui_text(prop, "Frame", "Frame number marker is keyframed on");
  RNA_def_property_int_funcs(prop, nullptr, "rna_trackingPlaneMarker_frame_set", nullptr);
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* Corners */
  prop = RNA_def_property(srna, "corners", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "corners");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x2);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop,
                           "Corners",
                           "Array of coordinates which represents UI rectangle corners in "
                           "frame normalized coordinates");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);

  /* enable */
  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PLANE_MARKER_DISABLED);
  RNA_def_property_ui_text(prop, "Mode", "Is marker muted for current frame");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);
}

static void rna_def_trackingPlaneMarkers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MovieTrackingPlaneMarkers");
  srna = RNA_def_struct(brna, "MovieTrackingPlaneMarkers", nullptr);
  RNA_def_struct_sdna(srna, "MovieTrackingPlaneTrack");
  RNA_def_struct_ui_text(srna,
                         "Movie Tracking Plane Markers",
                         "Collection of markers for movie tracking plane track");

  func = RNA_def_function(srna, "find_frame", "rna_trackingPlaneMarkers_find_frame");
  RNA_def_function_ui_description(func, "Get plane marker for specified frame");
  parm = RNA_def_int(func,
                     "frame",
                     1,
                     MINFRAME,
                     MAXFRAME,
                     "Frame",
                     "Frame number to find marker for",
                     MINFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func,
                  "exact",
                  true,
                  "Exact",
                  "Get plane marker at exact frame number rather than get estimated marker");
  parm = RNA_def_pointer(
      func, "plane_marker", "MovieTrackingPlaneMarker", "", "Plane marker for specified frame");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "insert_frame", "rna_trackingPlaneMarkers_insert_frame");
  RNA_def_function_ui_description(func, "Insert a new plane marker at the specified frame");
  parm = RNA_def_int(func,
                     "frame",
                     1,
                     MINFRAME,
                     MAXFRAME,
                     "Frame",
                     "Frame number to insert marker to",
                     MINFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "plane_marker", "MovieTrackingPlaneMarker", "", "Newly created plane marker");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "delete_frame", "rna_trackingPlaneMarkers_delete_frame");
  RNA_def_function_ui_description(func, "Delete plane marker at specified frame");
  parm = RNA_def_int(func,
                     "frame",
                     1,
                     MINFRAME,
                     MAXFRAME,
                     "Frame",
                     "Frame number to delete plane marker from",
                     MINFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_trackingPlaneTrack(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_trackingPlaneMarker(brna);

  srna = RNA_def_struct(brna, "MovieTrackingPlaneTrack", nullptr);
  RNA_def_struct_path_func(srna, "rna_trackingPlaneTrack_path");
  RNA_def_struct_ui_text(
      srna, "Movie tracking plane track data", "Match-moving plane track data for tracking");
  RNA_def_struct_ui_icon(srna, ICON_ANIM_DATA);

  /* name */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Unique name of track");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_trackingPlaneTrack_name_set");
  RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);
  RNA_def_struct_name_property(srna, prop);

  /* markers */
  prop = RNA_def_property(srna, "markers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingPlaneMarker");
  RNA_def_property_collection_sdna(prop, nullptr, "markers", "markersnr");
  RNA_def_property_ui_text(prop, "Markers", "Collection of markers in track");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  rna_def_trackingPlaneMarkers(brna, prop);

  /* select */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SELECT);
  RNA_def_property_ui_text(prop, "Select", "Plane track is selected");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* auto keyframing */
  prop = RNA_def_property(srna, "use_auto_keying", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PLANE_TRACK_AUTOKEY);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Auto Keyframe", "Automatic keyframe insertion when moving plane corners");
  RNA_def_property_ui_icon(prop, ICON_REC, 0);

  /* image */
  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Image", "Image displayed in the track during editing in clip editor");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* image opacity */
  prop = RNA_def_property(srna, "image_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Image Opacity", "Opacity of the image");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);
}

static void rna_def_trackingStabilization(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem filter_items[] = {
      {TRACKING_FILTER_NEAREST,
       "NEAREST",
       0,
       "Nearest",
       "No interpolation, use nearest neighbor pixel"},
      {TRACKING_FILTER_BILINEAR,
       "BILINEAR",
       0,
       "Bilinear",
       "Simple interpolation between adjacent pixels"},
      {TRACKING_FILTER_BICUBIC, "BICUBIC", 0, "Bicubic", "High quality pixel interpolation"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MovieTrackingStabilization", nullptr);
  RNA_def_struct_path_func(srna, "rna_trackingStabilization_path");
  RNA_def_struct_ui_text(
      srna, "Movie tracking stabilization data", "2D stabilization based on tracking markers");

  /* 2d stabilization */
  prop = RNA_def_property(srna, "use_2d_stabilization", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_2D_STABILIZATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Use 2D Stabilization", "Use 2D stabilization for footage");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* use_stabilize_rotation */
  prop = RNA_def_property(srna, "use_stabilize_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_STABILIZE_ROTATION);
  RNA_def_property_ui_text(
      prop, "Stabilize Rotation", "Stabilize detected rotation around center of frame");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* use_stabilize_scale */
  prop = RNA_def_property(srna, "use_stabilize_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_STABILIZE_SCALE);
  RNA_def_property_ui_text(
      prop, "Stabilize Scale", "Compensate any scale changes relative to center of rotation");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* tracks */
  prop = RNA_def_property(srna, "tracks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_tracking_stabTracks_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MovieTrackingTrack");
  RNA_def_property_ui_text(
      prop, "Translation Tracks", "Collection of tracks used for 2D stabilization (translation)");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* active track index */
  prop = RNA_def_property(srna, "active_track_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "act_track");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_tracking_stabTracks_active_index_get",
                             "rna_tracking_stabTracks_active_index_set",
                             "rna_tracking_stabTracks_active_index_range");
  RNA_def_property_ui_text(prop,
                           "Active Track Index",
                           "Index of active track in translation stabilization tracks list");

  /* tracks used for rotation stabilization */
  prop = RNA_def_property(srna, "rotation_tracks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_tracking_stabRotTracks_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MovieTrackingTrack");
  RNA_def_property_ui_text(
      prop, "Rotation Tracks", "Collection of tracks used for 2D stabilization (translation)");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* active rotation track index */
  prop = RNA_def_property(srna, "active_rotation_track_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "act_rot_track");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_tracking_stabRotTracks_active_index_get",
                             "rna_tracking_stabRotTracks_active_index_set",
                             "rna_tracking_stabRotTracks_active_index_range");
  RNA_def_property_ui_text(prop,
                           "Active Rotation Track Index",
                           "Index of active track in rotation stabilization tracks list");

  /* anchor frame */
  prop = RNA_def_property(srna, "anchor_frame", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "anchor_frame");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop,
                           "Anchor Frame",
                           "Reference point to anchor stabilization "
                           "(other frames will be adjusted relative to this frame's position)");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* target position */
  prop = RNA_def_property(srna, "target_position", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_range(
      prop, -FLT_MAX, FLT_MAX, 1, 3); /* increment in steps of 0.01 and show 3 digit after point */
  RNA_def_property_float_sdna(prop, nullptr, "target_pos");
  RNA_def_property_ui_text(prop,
                           "Expected Position",
                           "Known relative offset of original shot, will be subtracted "
                           "(e.g. for panning shot, can be animated)");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* target rotation */
  prop = RNA_def_property(srna, "target_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "target_rot");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 10.0f, 3);
  RNA_def_property_ui_text(
      prop,
      "Expected Rotation",
      "Rotation present on original shot, will be compensated (e.g. for deliberate tilting)");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* target scale */
  prop = RNA_def_property(srna, "target_scale", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_range(prop, FLT_EPSILON, FLT_MAX);
  RNA_def_property_ui_range(
      prop, 0.01f, 10.0f, 0.001f, 3); /* increment in steps of 0.001. Show 3 digit after point */
  RNA_def_property_ui_text(prop,
                           "Expected Scale",
                           "Explicitly scale resulting frame to compensate zoom of original shot");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* Auto-scale. */
  prop = RNA_def_property(srna, "use_autoscale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_AUTOSCALE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Autoscale", "Automatically scale footage to cover unfilled areas when stabilizing");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* max scale */
  prop = RNA_def_property(srna, "scale_max", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "maxscale");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "Maximal Scale", "Limit the amount of automatic scaling");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* influence_location */
  prop = RNA_def_property(srna, "influence_location", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "locinf");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Location Influence", "Influence of stabilization algorithm on footage location");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* influence_scale */
  prop = RNA_def_property(srna, "influence_scale", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "scaleinf");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Scale Influence", "Influence of stabilization algorithm on footage scale");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* influence_rotation */
  prop = RNA_def_property(srna, "influence_rotation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "rotinf");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Rotation Influence", "Influence of stabilization algorithm on footage rotation");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* filter */
  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "filter");
  RNA_def_property_enum_items(prop, filter_items);
  RNA_def_property_ui_text(
      prop,
      "Interpolate",
      "Interpolation to use for sub-pixel shifts and rotations due to stabilization");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, "rna_tracking_flushUpdate");

  /* UI display : show participating tracks */
  prop = RNA_def_property(srna, "show_tracks_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_SHOW_STAB_TRACKS);
  RNA_def_property_ui_text(
      prop, "Show Tracks", "Show UI list of tracks participating in stabilization");
  RNA_def_property_ui_icon(prop, ICON_RIGHTARROW, 1);
}

static void rna_def_reconstructedCamera(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MovieReconstructedCamera", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Movie tracking reconstructed camera data",
                         "Match-moving reconstructed camera data from tracker");

  /* frame */
  prop = RNA_def_property(srna, "frame", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_sdna(prop, nullptr, "framenr");
  RNA_def_property_ui_text(prop, "Frame", "Frame number marker is keyframed on");

  /* matrix */
  prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "mat");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Matrix", "Worldspace transformation matrix");

  /* average_error */
  prop = RNA_def_property(srna, "average_error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "error");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Average Error", "Average error of reconstruction");
}

static void rna_def_trackingReconstructedCameras(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "MovieTrackingReconstructedCameras", nullptr);
  RNA_def_struct_sdna(srna, "MovieTrackingReconstruction");
  RNA_def_struct_ui_text(srna, "Reconstructed Cameras", "Collection of solved cameras");

  func = RNA_def_function(srna, "find_frame", "rna_trackingCameras_find_frame");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Find a reconstructed camera for a give frame number");
  RNA_def_int(func,
              "frame",
              1,
              MINFRAME,
              MAXFRAME,
              "Frame",
              "Frame number to find camera for",
              MINFRAME,
              MAXFRAME);
  parm = RNA_def_pointer(
      func, "camera", "MovieReconstructedCamera", "", "Camera for a given frame");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "matrix_from_frame", "rna_trackingCameras_matrix_from_frame");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Return interpolated camera matrix for a given frame");
  RNA_def_int(func,
              "frame",
              1,
              MINFRAME,
              MAXFRAME,
              "Frame",
              "Frame number to find camera for",
              MINFRAME,
              MAXFRAME);
  parm = RNA_def_float_matrix(func,
                              "matrix",
                              4,
                              4,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Matrix",
                              "Interpolated camera matrix for a given frame",
                              -FLT_MAX,
                              FLT_MAX);
  RNA_def_parameter_flags(
      parm, PROP_THICK_WRAP, ParameterFlag(0)); /* needed for string return value */
  RNA_def_function_output(func, parm);
}

static void rna_def_trackingReconstruction(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_reconstructedCamera(brna);

  srna = RNA_def_struct(brna, "MovieTrackingReconstruction", nullptr);
  RNA_def_struct_ui_text(
      srna, "Movie tracking reconstruction data", "Match-moving reconstruction data from tracker");

  /* is_valid */
  prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_RECONSTRUCTED);
  RNA_def_property_ui_text(prop,
                           "Reconstructed",
                           "Whether the tracking data contains valid reconstruction information");

  /* average_error */
  prop = RNA_def_property(srna, "average_error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "error");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Average Error", "Average error of reconstruction");

  /* cameras */
  prop = RNA_def_property(srna, "cameras", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieReconstructedCamera");
  RNA_def_property_collection_sdna(prop, nullptr, "cameras", "camnr");
  RNA_def_property_ui_text(prop, "Cameras", "Collection of solved cameras");
  RNA_def_property_srna(prop, "MovieTrackingReconstructedCameras");
}

static void rna_def_trackingTracks(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "MovieTrackingTracks", nullptr);
  RNA_def_struct_sdna(srna, "MovieTracking");
  RNA_def_struct_ui_text(srna, "Movie Tracks", "Collection of movie tracking tracks");

  func = RNA_def_function(srna, "new", "rna_trackingTracks_new");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Create new motion track in this movie clip");
  RNA_def_string(func, "name", nullptr, 0, "", "Name of new track");
  RNA_def_int(func,
              "frame",
              1,
              MINFRAME,
              MAXFRAME,
              "Frame",
              "Frame number to add track on",
              MINFRAME,
              MAXFRAME);
  parm = RNA_def_pointer(func, "track", "MovieTrackingTrack", "", "Newly created track");
  RNA_def_function_return(func, parm);

  /* active track */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingTrack");
  RNA_def_property_pointer_funcs(
      prop, "rna_tracking_active_track_get", "rna_tracking_active_track_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop,
                           "Active Track",
                           "Active track in this tracking data object. "
                           "Deprecated, use objects[name].tracks.active");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_SELECT, nullptr);
}

static void rna_def_trackingPlaneTracks(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MovieTrackingPlaneTracks", nullptr);
  RNA_def_struct_sdna(srna, "MovieTracking");
  RNA_def_struct_ui_text(srna, "Movie Plane Tracks", "Collection of movie tracking plane tracks");

  /* TODO(sergey): Add API to create new plane tracks */

  /* active plane track */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingPlaneTrack");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_tracking_active_plane_track_get",
                                 "rna_tracking_active_plane_track_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop,
                           "Active Plane Track",
                           "Active plane track in this tracking data object. "
                           "Deprecated, use objects[name].plane_tracks.active");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_SELECT, nullptr);
}

static void rna_def_trackingObjectTracks(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "MovieTrackingObjectTracks", nullptr);
  RNA_def_struct_sdna(srna, "MovieTrackingObject");
  RNA_def_struct_ui_text(srna, "Movie Tracks", "Collection of movie tracking tracks");

  func = RNA_def_function(srna, "new", "rna_trackingObject_tracks_new");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "create new motion track in this movie clip");
  RNA_def_string(func, "name", nullptr, 0, "", "Name of new track");
  RNA_def_int(func,
              "frame",
              1,
              MINFRAME,
              MAXFRAME,
              "Frame",
              "Frame number to add tracks on",
              MINFRAME,
              MAXFRAME);
  parm = RNA_def_pointer(func, "track", "MovieTrackingTrack", "", "Newly created track");
  RNA_def_function_return(func, parm);

  /* active track */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingTrack");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_tracking_object_active_track_get",
                                 "rna_tracking_object_active_track_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Track", "Active track in this tracking data object");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_SELECT, nullptr);
}

static void rna_def_trackingObjectPlaneTracks(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MovieTrackingObjectPlaneTracks", nullptr);
  RNA_def_struct_sdna(srna, "MovieTrackingObject");
  RNA_def_struct_ui_text(srna, "Plane Tracks", "Collection of tracking plane tracks");

  /* active track */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingTrack");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_tracking_object_active_plane_track_get",
                                 "rna_tracking_object_active_plane_track_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Track", "Active track in this tracking data object");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_SELECT, nullptr);
}

static void rna_def_trackingObject(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MovieTrackingObject", nullptr);
  RNA_def_struct_ui_text(
      srna, "Movie tracking object data", "Match-moving object tracking and reconstruction data");

  /* name */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Unique name of object");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_trackingObject_name_set");
  RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, nullptr);
  RNA_def_struct_name_property(srna, prop);

  /* is_camera */
  prop = RNA_def_property(srna, "is_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_OBJECT_CAMERA);
  RNA_def_property_ui_text(prop, "Camera", "Object is used for camera tracking");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* tracks */
  prop = RNA_def_property(srna, "tracks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_trackingObject_tracks_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MovieTrackingTrack");
  RNA_def_property_ui_text(prop, "Tracks", "Collection of tracks in this tracking data object");
  RNA_def_property_srna(prop, "MovieTrackingObjectTracks");

  /* plane tracks */
  prop = RNA_def_property(srna, "plane_tracks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_trackingObject_plane_tracks_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MovieTrackingPlaneTrack");
  RNA_def_property_ui_text(
      prop, "Plane Tracks", "Collection of plane tracks in this tracking data object");
  RNA_def_property_srna(prop, "MovieTrackingObjectPlaneTracks");

  /* reconstruction */
  prop = RNA_def_property(srna, "reconstruction", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingReconstruction");
  RNA_def_property_pointer_funcs(
      prop, "rna_trackingObject_reconstruction_get", nullptr, nullptr, nullptr);

  /* scale */
  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_range(prop, 0.0001f, 10000.0f);
  RNA_def_property_ui_range(prop, 0.0001f, 10000.0, 1, 4);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Scale", "Scale of object solution in camera space");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_trackingObject_flushUpdate");

  /* keyframe_a */
  prop = RNA_def_property(srna, "keyframe_a", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "keyframe1");
  RNA_def_property_ui_text(
      prop, "Keyframe A", "First keyframe used for reconstruction initialization");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* keyframe_b */
  prop = RNA_def_property(srna, "keyframe_b", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "keyframe2");
  RNA_def_property_ui_text(
      prop, "Keyframe B", "Second keyframe used for reconstruction initialization");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);
}

static void rna_def_trackingObjects(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MovieTrackingObjects");
  srna = RNA_def_struct(brna, "MovieTrackingObjects", nullptr);
  RNA_def_struct_sdna(srna, "MovieTracking");
  RNA_def_struct_ui_text(srna, "Movie Objects", "Collection of movie tracking objects");

  func = RNA_def_function(srna, "new", "rna_trackingObject_new");
  RNA_def_function_ui_description(func, "Add tracking object to this movie clip");
  parm = RNA_def_string(func, "name", nullptr, 0, "", "Name of new object");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "object", "MovieTrackingObject", "", "New motion tracking object");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_trackingObject_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove tracking object from this movie clip");
  parm = RNA_def_pointer(
      func, "object", "MovieTrackingObject", "", "Motion tracking object to be removed");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* active object */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingObject");
  RNA_def_property_pointer_funcs(
      prop, "rna_tracking_active_object_get", "rna_tracking_active_object_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Object", "Active object in this tracking data object");
}

static void rna_def_trackingDopesheet(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem sort_items[] = {
      {TRACKING_DOPE_SORT_NAME, "NAME", 0, "Name", "Sort channels by their names"},
      {TRACKING_DOPE_SORT_LONGEST,
       "LONGEST",
       0,
       "Longest",
       "Sort channels by longest tracked segment"},
      {TRACKING_DOPE_SORT_TOTAL,
       "TOTAL",
       0,
       "Total",
       "Sort channels by overall amount of tracked segments"},
      {TRACKING_DOPE_SORT_AVERAGE_ERROR,
       "AVERAGE_ERROR",
       0,
       "Average Error",
       "Sort channels by average reprojection error of tracks after solve"},
      {TRACKING_DOPE_SORT_START, "START", 0, "Start Frame", "Sort channels by first frame number"},
      {TRACKING_DOPE_SORT_END, "END", 0, "End Frame", "Sort channels by last frame number"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MovieTrackingDopesheet", nullptr);
  RNA_def_struct_path_func(srna, "rna_trackingDopesheet_path");
  RNA_def_struct_ui_text(srna, "Movie Tracking Dopesheet", "Match-moving dopesheet data");

  /* dopesheet sort */
  prop = RNA_def_property(srna, "sort_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "sort_method");
  RNA_def_property_enum_items(prop, sort_items);
  RNA_def_property_ui_text(
      prop, "Dopesheet Sort Field", "Method to be used to sort channels in dopesheet view");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_trackingDopesheet_tagUpdate");

  /* invert_dopesheet_sort */
  prop = RNA_def_property(srna, "use_invert_sort", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_DOPE_SORT_INVERSE);
  RNA_def_property_ui_text(
      prop, "Invert Dopesheet Sort", "Invert sort order of dopesheet channels");
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_trackingDopesheet_tagUpdate");

  /* show_only_selected */
  prop = RNA_def_property(srna, "show_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_DOPE_SELECTED_ONLY);
  RNA_def_property_ui_text(
      prop, "Only Show Selected", "Only include channels relating to selected objects and data");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 0);
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_trackingDopesheet_tagUpdate");

  /* show_hidden */
  prop = RNA_def_property(srna, "show_hidden", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRACKING_DOPE_SHOW_HIDDEN);
  RNA_def_property_ui_text(
      prop, "Display Hidden", "Include channels from objects/bone that are not visible");
  RNA_def_property_ui_icon(prop, ICON_GHOST_ENABLED, 0);
  RNA_def_property_update(prop, NC_MOVIECLIP | NA_EDITED, "rna_trackingDopesheet_tagUpdate");
}

static void rna_def_tracking(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_trackingSettings(brna);
  rna_def_trackingCamera(brna);
  rna_def_trackingTrack(brna);
  rna_def_trackingPlaneTrack(brna);
  rna_def_trackingTracks(brna);
  rna_def_trackingPlaneTracks(brna);
  rna_def_trackingObjectTracks(brna);
  rna_def_trackingObjectPlaneTracks(brna);
  rna_def_trackingStabilization(brna);
  rna_def_trackingReconstructedCameras(brna);
  rna_def_trackingReconstruction(brna);
  rna_def_trackingObject(brna);
  rna_def_trackingDopesheet(brna);

  srna = RNA_def_struct(brna, "MovieTracking", nullptr);
  RNA_def_struct_path_func(srna, "rna_tracking_path");
  RNA_def_struct_ui_text(srna, "Movie tracking data", "Match-moving data for tracking");

  /* settings */
  prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingSettings");

  /* camera properties */
  prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingCamera");

  /* tracks */
  prop = RNA_def_property(srna, "tracks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_trackingTracks_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MovieTrackingTrack");
  RNA_def_property_ui_text(prop,
                           "Tracks",
                           "Collection of tracks in this tracking data object. "
                           "Deprecated, use objects[name].tracks");
  RNA_def_property_srna(prop, "MovieTrackingTracks");

  /* tracks */
  prop = RNA_def_property(srna, "plane_tracks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_trackingPlaneTracks_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MovieTrackingPlaneTrack");
  RNA_def_property_ui_text(prop,
                           "Plane Tracks",
                           "Collection of plane tracks in this tracking data object. "
                           "Deprecated, use objects[name].plane_tracks");
  RNA_def_property_srna(prop, "MovieTrackingPlaneTracks");

  /* stabilization */
  prop = RNA_def_property(srna, "stabilization", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingStabilization");

  /* reconstruction */
  prop = RNA_def_property(srna, "reconstruction", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "reconstruction_legacy");
  RNA_def_property_pointer_funcs(
      prop, "rna_trackingReconstruction_get", nullptr, nullptr, nullptr);
  RNA_def_property_struct_type(prop, "MovieTrackingReconstruction");

  /* objects */
  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_trackingObjects_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "MovieTrackingObject");
  RNA_def_property_ui_text(prop, "Objects", "Collection of objects in this tracking data object");
  rna_def_trackingObjects(brna, prop);

  /* active object index */
  prop = RNA_def_property(srna, "active_object_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "objectnr");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_tracking_active_object_index_get",
                             "rna_tracking_active_object_index_set",
                             "rna_tracking_active_object_index_range");
  RNA_def_property_ui_text(prop, "Active Object Index", "Index of active object");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* dopesheet */
  prop = RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MovieTrackingDopesheet");
}

void RNA_def_tracking(BlenderRNA *brna)
{
  rna_def_tracking(brna);
}

#endif
