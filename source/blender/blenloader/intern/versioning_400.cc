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
#include "BKE_node.h"
#include "BKE_tracking.h"
#include "BLI_math_base_safe.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "versioning_common.h"

// static CLG_LogRef LOG = {"blo.readfile.doversion"};

float *version_cycles_node_socket_float_value(bNodeSocket *socket)
{
  bNodeSocketValueFloat *socket_data = static_cast<bNodeSocketValueFloat *>(socket->default_value);
  return &socket_data->value;
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

static void version_principled_bsdf_rename_sockets(bNodeTree *ntree)
{
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Emission Color", "Emission");
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Specular IOR Level", "Specular");
  version_node_input_socket_name(
      ntree, SH_NODE_BSDF_PRINCIPLED, "Subsurface Weight", "Subsurface");
  version_node_input_socket_name(
      ntree, SH_NODE_BSDF_PRINCIPLED, "Transmission Weight", "Transmission");
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Coat Weight", "Coat");
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Sheen Weight", "Sheen");
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Coat", "Clearcoat");
  version_node_input_socket_name(
      ntree, SH_NODE_BSDF_PRINCIPLED, "Coat Roughness", "Clearcoat Roughness");
  version_node_input_socket_name(
      ntree, SH_NODE_BSDF_PRINCIPLED, "Coat Normal", "Clearcoat Normal");
}

static void versioning_convert_combined_noise_texture_node(bNodeTree *ntree)
{
  /* Convert future combined Noise Texture back to Musgrave and Noise Texture nodes. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type != SH_NODE_TEX_NOISE) {
      continue;
    }

    const NodeTexNoise *noise_data = static_cast<const NodeTexNoise *>(node->storage);
    if (!noise_data || (noise_data->type == SHD_NOISE_FBM && noise_data->normalize)) {
      continue;
    }

    /* Does the node have the expected new sockets? */
    bNodeSocket *height_socket = nodeFindSocket(node, SOCK_OUT, "Fac");
    bNodeSocket *roughness_socket = nodeFindSocket(node, SOCK_IN, "Roughness");
    bNodeSocket *detail_socket = nodeFindSocket(node, SOCK_IN, "Detail");
    bNodeSocket *lacunarity_socket = nodeFindSocket(node, SOCK_IN, "Lacunarity");

    if (!(height_socket && roughness_socket && detail_socket && lacunarity_socket)) {
      continue;
    }

    /* Delete links to sockets that don't exist in Blender 4.0. */
    LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
      if (((link->tonode == node) && STREQ(link->tosock->identifier, "Distortion")) ||
          ((link->fromnode == node) && STREQ(link->fromsock->identifier, "Color")))
      {
        nodeRemLink(ntree, link);
      }
    }

    /* Change node idname and storage to Musgrave. */
    STRNCPY(node->idname, "ShaderNodeTexMusgrave");
    node->type = SH_NODE_TEX_MUSGRAVE;
    NodeTexMusgrave *data = MEM_cnew<NodeTexMusgrave>(__func__);
    data->base = noise_data->base;
    data->musgrave_type = noise_data->type;
    data->dimensions = noise_data->dimensions;
    MEM_freeN(node->storage);
    node->storage = data;

    /* Convert socket names and labels. */
    bNodeSocket *dimension_socket = roughness_socket;
    STRNCPY(dimension_socket->identifier, "Dimension");
    STRNCPY(dimension_socket->name, "Dimension");

    STRNCPY(height_socket->label, "Height");

    /* Find links to convert. */
    bNodeLink *detail_link = nullptr;
    bNode *detail_from_node = nullptr;
    bNodeSocket *detail_from_socket = nullptr;
    float *detail = version_cycles_node_socket_float_value(detail_socket);

    bNodeLink *dimension_link = nullptr;
    bNode *dimension_from_node = nullptr;
    bNodeSocket *dimension_from_socket = nullptr;
    float *dimension = version_cycles_node_socket_float_value(dimension_socket);

    bNodeLink *lacunarity_link = nullptr;
    bNode *lacunarity_from_node = nullptr;
    bNodeSocket *lacunarity_from_socket = nullptr;
    float *lacunarity = version_cycles_node_socket_float_value(lacunarity_socket);

    LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
      /* Find links, nodes and sockets. */
      if (link->tonode == node) {
        if (link->tosock == detail_socket) {
          detail_link = link;
          detail_from_node = link->fromnode;
          detail_from_socket = link->fromsock;
        }
        if (link->tosock == dimension_socket) {
          dimension_link = link;
          dimension_from_node = link->fromnode;
          dimension_from_socket = link->fromsock;
        }
        if (link->tosock == lacunarity_socket) {
          lacunarity_link = link;
          lacunarity_from_node = link->fromnode;
          lacunarity_from_socket = link->fromsock;
        }
      }
    }

    float locy_offset = 0.0f;

    if (detail_link != nullptr) {
      locy_offset -= 40.0f;

      /* Add Add Math node before Detail input. */

      bNode *add_node = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
      add_node->parent = node->parent;
      add_node->custom1 = NODE_MATH_ADD;
      add_node->locx = node->locx;
      add_node->locy = node->locy - 280.0f;
      add_node->flag |= NODE_HIDDEN;
      bNodeSocket *add_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&add_node->inputs, 0));
      bNodeSocket *add_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&add_node->inputs, 1));
      bNodeSocket *add_socket_out = nodeFindSocket(add_node, SOCK_OUT, "Value");

      *version_cycles_node_socket_float_value(add_socket_B) = 1.0f;

      nodeRemLink(ntree, detail_link);
      nodeAddLink(ntree, detail_from_node, detail_from_socket, add_node, add_socket_A);
      nodeAddLink(ntree, add_node, add_socket_out, node, detail_socket);
    }
    else {
      *detail = std::clamp(*detail + 1.0f, 0.0f, 15.0f);
    }

    if ((dimension_link != nullptr) || (lacunarity_link != nullptr)) {
      /* Add Logarithm Math node and Multiply Math node before Roughness input. */

      bNode *log_node = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
      log_node->parent = node->parent;
      log_node->custom1 = NODE_MATH_LOGARITHM;
      log_node->locx = node->locx;
      log_node->locy = node->locy - 320.0f + locy_offset;
      log_node->flag |= NODE_HIDDEN;
      bNodeSocket *log_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&log_node->inputs, 0));
      bNodeSocket *log_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&log_node->inputs, 1));
      bNodeSocket *log_socket_out = nodeFindSocket(log_node, SOCK_OUT, "Value");

      bNode *mul_node = nodeAddStaticNode(nullptr, ntree, SH_NODE_MATH);
      mul_node->parent = node->parent;
      mul_node->custom1 = NODE_MATH_MULTIPLY;
      mul_node->locx = node->locx;
      mul_node->locy = node->locy - 280.0f + locy_offset;
      mul_node->flag |= NODE_HIDDEN;
      bNodeSocket *mul_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 0));
      bNodeSocket *mul_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 1));
      bNodeSocket *mul_socket_out = nodeFindSocket(mul_node, SOCK_OUT, "Value");

      *version_cycles_node_socket_float_value(log_socket_A) = *dimension;
      *version_cycles_node_socket_float_value(log_socket_B) = *lacunarity;
      *version_cycles_node_socket_float_value(mul_socket_B) = -1.0f;

      if (dimension_link) {
        nodeRemLink(ntree, dimension_link);
        nodeAddLink(ntree, dimension_from_node, dimension_from_socket, log_node, log_socket_A);
      }
      nodeAddLink(ntree, log_node, log_socket_out, mul_node, mul_socket_A);
      nodeAddLink(ntree, mul_node, mul_socket_out, node, dimension_socket);

      if (lacunarity_link != nullptr) {
        nodeAddLink(ntree, lacunarity_from_node, lacunarity_from_socket, log_node, log_socket_B);
      }
    }
    else {
      *dimension = -safe_logf(*dimension, *lacunarity);
    }
  }

  version_socket_update_is_used(ntree);
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

  if (MAIN_VERSION_ATLEAST(bmain, 400, 0)) {
    /* Forward compatibility with renamed sockets. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        version_principled_bsdf_rename_sockets(ntree);
      }
    }
    FOREACH_NODETREE_END;

    /* Forward compatibility with repurposed #RGN_FLAG_PREFSIZE_OR_HIDDEN (in 4.0:
     * #RGN_FLAG_NO_USER_RESIZE), which is set for more regions now. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          LISTBASE_FOREACH (ARegion *, region, regionbase) {
            /* The Properties editor navigation region is the only one using the
             * #RGN_FLAG_PREFSIZE_OR_HIDDEN flag in 3.6. Unset it for others. */
            if (!((sl->spacetype == SPACE_PROPERTIES) && (region->regiontype == RGN_TYPE_NAV_BAR)))
            {
              region->flag &= ~RGN_FLAG_PREFSIZE_OR_HIDDEN;
            }
          }
        }
      }
    }
  }

  if (MAIN_VERSION_ATLEAST(bmain, 400, 30)) {
    /* Forward compatibility for #ts->snap_mode. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      enum { IS_DEFAULT = 0, IS_UV, IS_NODE };
      auto versioning_snap_to = [](short snap_to_old, int type) {
        eSnapMode snap_to_new = SCE_SNAP_MODE_NONE;
        if (snap_to_old & (1 << 1)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_MODE_NODE_Y :
                         type == IS_UV   ? SCE_SNAP_MODE_VERTEX :
                                           SCE_SNAP_MODE_EDGE_MIDPOINT;
        }
        if (snap_to_old & (1 << 2)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_MODE_NODE_Y : SCE_SNAP_MODE_VERTEX;
        }
        if (snap_to_old & (1 << 3)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_MODE_NODE_Y :
                         type == IS_UV   ? SCE_SNAP_MODE_VERTEX :
                                           SCE_SNAP_MODE_EDGE_PERPENDICULAR;
        }
        if (snap_to_old & (1 << 4)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_MODE_NODE_Y :
                         type == IS_UV   ? SCE_SNAP_MODE_VERTEX :
                                           SCE_SNAP_MODE_EDGE;
        }
        if (snap_to_old & (1 << 5)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_MODE_NODE_Y :
                         type == IS_UV   ? SCE_SNAP_MODE_VERTEX :
                                           SCE_SNAP_MODE_FACE_RAYCAST;
        }
        if (snap_to_old & (1 << 6)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_MODE_NODE_Y :
                         type == IS_UV   ? SCE_SNAP_MODE_VERTEX :
                                           SCE_SNAP_MODE_VOLUME;
        }
        if (snap_to_old & (1 << 7)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_MODE_NODE_Y :
                         type == IS_UV   ? SCE_SNAP_MODE_VERTEX :
                                           SCE_SNAP_MODE_GRID;
        }
        if (snap_to_old & (1 << 8)) {
          snap_to_new |= SCE_SNAP_MODE_INCREMENT;
        }
        if (snap_to_old & (1 << 9)) {
          snap_to_new |= SCE_SNAP_MODE_FACE_NEAREST;
        }
        if (snap_to_old & (1 << 10)) {
          snap_to_new |= SCE_SNAP_MODE_FACE_RAYCAST;
        }

        if (!snap_to_new) {
          snap_to_new = eSnapMode(1 << 0);
        }

        return snap_to_new;
      };

      ts->snap_mode = versioning_snap_to(ts->snap_mode, IS_DEFAULT);
      ts->snap_node_mode = versioning_snap_to(ts->snap_node_mode, IS_NODE);
      ts->snap_uv_mode = versioning_snap_to(ts->snap_uv_mode, IS_UV);
    }
  }

  if (MAIN_VERSION_ATLEAST(bmain, 401, 4)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_CUSTOM) {
        versioning_convert_combined_noise_texture_node(ntree);
      }
    }
    FOREACH_NODETREE_END;
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
}
