/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "BLI_sys_types.h"

#include "CLG_log.h"

#include "DNA_genfile.h"
#include "DNA_movieclip_types.h"
#include "DNA_workspace_types.h"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_set.hh"

#include "BKE_main.h"
#include "BKE_mesh_legacy_convert.h"
#include "BKE_node.hh"
#include "BKE_screen.h"
#include "BKE_tracking.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "versioning_common.h"

// static CLG_LogRef LOG = {"blo.readfile.doversion"};

void do_versions_after_linking_400(FileData * /*fd*/, Main *bmain)
{
  UNUSED_VARS(bmain);
}

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
}

static void version_motion_tracking_legacy_camera_object(MovieClip &movieclip)
{
  MovieTracking &tracking = movieclip.tracking;
  MovieTrackingObject *active_tracking_object = BKE_tracking_object_get_active(&tracking);
  MovieTrackingObject *tracking_camera_object = BKE_tracking_object_get_camera(&tracking);

  BLI_assert(tracking_camera_object != nullptr);

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

static void version_geometry_nodes_add_realize_instance_nodes(bNodeTree *ntree)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    if (STREQ(node->idname, "GeometryNodeMeshBoolean")) {
      add_realize_instances_before_socket(ntree, node, nodeFindSocket(node, SOCK_IN, "Mesh 2"));
    }
  }
}

void blo_do_versions_400(FileData *fd, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_ATLEAST(bmain, 400, 1)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      version_mesh_legacy_to_struct_of_array_format(*mesh);
    }
    version_movieclips_legacy_camera_object(bmain);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 400, 2)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      BKE_mesh_legacy_bevel_weight_to_generic(mesh);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 400, 3)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_geometry_nodes_add_realize_instance_nodes(ntree);
      }
    }
  }

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

  if (!DNA_struct_find(fd->filesdna, "AssetShelfHook")) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_VIEW3D) {
            continue;
          }
          View3D *v3d = reinterpret_cast<View3D *>(sl);

          v3d->asset_shelf_hook = MEM_cnew<AssetShelfHook>("Versioning AssetShelfHook");

          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;

          /* TODO for old files saved with the branch only. */
          {
            SpaceType *space_type = BKE_spacetype_from_id(sl->spacetype);

            if (ARegion *asset_shelf = BKE_region_find_in_listbase_by_type(regionbase,
                                                                           RGN_TYPE_ASSET_SHELF)) {
              BLI_remlink(regionbase, asset_shelf);
              BKE_area_region_free(space_type, asset_shelf);
              MEM_freeN(asset_shelf);
            }

            if (ARegion *asset_shelf_footer = BKE_region_find_in_listbase_by_type(
                    regionbase, RGN_TYPE_ASSET_SHELF_FOOTER))
            {
              BLI_remlink(regionbase, asset_shelf_footer);
              BKE_area_region_free(space_type, asset_shelf_footer);
              MEM_freeN(asset_shelf_footer);
            }
          }

          {
            ARegion *new_asset_shelf_footer = do_versions_add_region_if_not_found(
                regionbase,
                RGN_TYPE_ASSET_SHELF_FOOTER,
                "asset shelf footer for view3d (versioning)",
                RGN_TYPE_UI);
            if (new_asset_shelf_footer != nullptr) {
              new_asset_shelf_footer->alignment = RGN_ALIGN_BOTTOM;
            }
          }
          {
            /* TODO for old files saved with the branch only. */
            ARegion *new_asset_shelf = do_versions_add_region_if_not_found(
                regionbase,
                RGN_TYPE_ASSET_SHELF,
                "asset shelf for view3d (versioning)",
                RGN_TYPE_ASSET_SHELF_FOOTER);
            new_asset_shelf->alignment = RGN_ALIGN_BOTTOM;
          }
        }
      }
    }

    /* Should we really use the "All" library by default? Consider loading time and memory usage.
     */
    LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
      workspace->asset_library_ref.type = ASSET_LIBRARY_ALL;
      workspace->asset_library_ref.custom_library_index = -1;
    }
  }
}
