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
  BKE_mesh_legacy_sharp_faces_from_flags(&mesh);
  BKE_mesh_legacy_bevel_weight_to_layers(&mesh);
  BKE_mesh_legacy_sharp_edges_from_flags(&mesh);
  BKE_mesh_legacy_face_set_to_generic(&mesh);
  BKE_mesh_legacy_edge_crease_to_layers(&mesh);
  BKE_mesh_legacy_uv_seam_from_flags(&mesh);
  BKE_mesh_legacy_convert_verts_to_positions(&mesh);
  BKE_mesh_legacy_attribute_flags_to_strings(&mesh);
  BKE_mesh_legacy_convert_loops_to_corners(&mesh);
  BKE_mesh_legacy_convert_polys_to_offsets(&mesh);
  BKE_mesh_legacy_convert_edges_to_generic(&mesh);
  BKE_mesh_bevel_weight_layers_from_future(&mesh);
  BKE_mesh_crease_layers_from_future(&mesh);
}

static void version_motion_tracking_legacy_camera_object(MovieClip &movieclip)
{
  MovieTracking &tracking = movieclip.tracking;
  MovieTrackingObject *active_tracking_object = BKE_tracking_object_get_active(&tracking);
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&tracking);

  BLI_assert(tracking_camera_object != nullptr);

  /* NOTE: The regular .blend file saving converts the new format to the legacy format, but the
   * auto-save one does not do this. Likely, the regular saving clears the new storage before
   * write, so it can be used to make a decision here.
   *
   * The idea is basically to not override the new storage if it exists. This is only supposed to
   * happen for auto-save files. */

  if (BLI_listbase_is_empty(&tracking_camera_object->tracks)) {
    tracking_camera_object->tracks = tracking.tracks_legacy;
    active_tracking_object->active_track = tracking.act_track_legacy;
  }

  if (BLI_listbase_is_empty(&tracking_camera_object->plane_tracks)) {
    tracking_camera_object->plane_tracks = tracking.plane_tracks_legacy;
    active_tracking_object->active_plane_track = tracking.act_plane_track_legacy;
  }

  if (tracking_camera_object->reconstruction.cameras == nullptr) {
    tracking_camera_object->reconstruction = tracking.reconstruction_legacy;
  }

  /* Clear pointers in the legacy storage.
   * Always do it, in the case something got missed in the logic above, so that the legacy storage
   * is always ensured to be empty after load. */
  BLI_listbase_clear(&tracking.tracks_legacy);
  BLI_listbase_clear(&tracking.plane_tracks_legacy);
  tracking.act_track_legacy = nullptr;
  tracking.act_plane_track_legacy = nullptr;
  memset(&tracking.reconstruction_legacy, 0, sizeof(tracking.reconstruction_legacy));
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
