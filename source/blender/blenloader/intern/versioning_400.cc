/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "CLG_log.h"

#include "DNA_movieclip_types.h"

#include "BLI_assert.h"
#include "BLI_listbase.h"

#include "BKE_main.h"
#include "BKE_mesh_legacy_convert.h"
#include "BKE_tracking.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "versioning_common.h"

// static CLG_LogRef LOG = {"blo.readfile.doversion"};

static void version_mesh_legacy_to_struct_of_array_format(Mesh &mesh)
{
  BKE_mesh_legacy_convert_flags_to_selection_layers(&mesh);
  BKE_mesh_legacy_convert_flags_to_hide_layers(&mesh);
  BKE_mesh_legacy_convert_uvs_to_generic(&mesh);
  BKE_mesh_legacy_convert_mpoly_to_material_indices(&mesh);
  BKE_mesh_legacy_bevel_weight_to_layers(&mesh);
  BKE_mesh_legacy_face_set_to_generic(&mesh);
  BKE_mesh_legacy_edge_crease_to_layers(&mesh);
  BKE_mesh_legacy_convert_verts_to_positions(&mesh);
  BKE_mesh_legacy_attribute_flags_to_strings(&mesh);
}

static void version_motion_tracking_legacy_camera_object(MovieClip &movieclip)
{
  MovieTracking &tracking = movieclip.tracking;
  MovieTrackingObject *active_tracking_object = BKE_tracking_object_get_active(&tracking);
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&tracking);

  /* Sanity check.
   * The camera tracking object is not supposed to have tracking and reconstruction read into it
   * yet. */

  BLI_assert(tracking_camera_object != nullptr);
  BLI_assert(BLI_listbase_is_empty(&tracking_camera_object->tracks));
  BLI_assert(BLI_listbase_is_empty(&tracking_camera_object->plane_tracks));
  BLI_assert(tracking_camera_object->reconstruction.cameras == nullptr);

  /* Move storage from tracking to the actual tracking object. */

  tracking_camera_object->tracks = tracking.tracks_legacy;
  tracking_camera_object->plane_tracks = tracking.plane_tracks_legacy;

  tracking_camera_object->reconstruction = tracking.reconstruction_legacy;
  memset(&tracking.reconstruction_legacy, 0, sizeof(tracking.reconstruction_legacy));

  /* The active track in the tracking structure used to be shared across all tracking objects. */
  active_tracking_object->active_track = tracking.act_track_legacy;
  active_tracking_object->active_plane_track = tracking.act_plane_track_legacy;

  /* Clear pointers in the legacy storage. */
  BLI_listbase_clear(&tracking.tracks_legacy);
  BLI_listbase_clear(&tracking.plane_tracks_legacy);
  tracking.act_track_legacy = nullptr;
  tracking.act_plane_track_legacy = nullptr;
}

static void version_movieclips_legacy_camera_object(Main *bmain)
{
  LISTBASE_FOREACH (MovieClip *, movieclip, &bmain->movieclips) {
    version_motion_tracking_legacy_camera_object(*movieclip);
  }
}

void blo_do_versions_400(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  // if (!MAIN_VERSION_ATLEAST(bmain, 400, 0)) {
  /* This is done here because we will continue to write with the old format until 4.0, so we need
   * to convert even "current" files. Keep the check commented out for now so the versioning isn't
   * turned off right after the 4.0 bump. */
  LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
    version_mesh_legacy_to_struct_of_array_format(*mesh);
  }
  version_movieclips_legacy_camera_object(bmain);
  // }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}
