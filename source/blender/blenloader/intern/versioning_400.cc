/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include <cmath>

#include "BLI_sys_types.h"

#include "CLG_log.h"

#include "DNA_brush_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "DNA_defaults.h"
#include "DNA_genfile.h"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_main.h"
#include "BKE_mesh_legacy_convert.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_scene.h"
#include "BKE_tracking.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "versioning_common.h"

// static CLG_LogRef LOG = {"blo.readfile.doversion"};

void do_versions_after_linking_400(FileData * /*fd*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 9)) {
    /* Fix area light scaling. */
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      light->energy = light->energy_deprecated;
      if (light->type == LA_AREA) {
        light->energy *= M_PI_4;
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #blo_do_versions_400 in this file.
   * - `versioning_userdef.cc`, #blo_do_versions_userdef
   * - `versioning_userdef.cc`, #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
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

/* Version VertexWeightEdit modifier to make existing weights exclusive of the threshold. */
static void version_vertex_weight_edit_preserve_threshold_exclusivity(Main *bmain)
{
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->type != OB_MESH) {
      continue;
    }

    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_WeightVGEdit) {
        WeightVGEditModifierData *wmd = reinterpret_cast<WeightVGEditModifierData *>(md);
        wmd->add_threshold = nexttoward(wmd->add_threshold, 2.0);
        wmd->rem_threshold = nexttoward(wmd->rem_threshold, -1.0);
      }
    }
  }
}

static void version_mesh_crease_generic(Main &bmain)
{
  LISTBASE_FOREACH (Mesh *, mesh, &bmain.meshes) {
    BKE_mesh_legacy_crease_to_generic(mesh);
  }

  LISTBASE_FOREACH (bNodeTree *, ntree, &bmain.nodetrees) {
    if (ntree->type == NTREE_GEOMETRY) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (STR_ELEM(node->idname,
                     "GeometryNodeStoreNamedAttribute",
                     "GeometryNodeInputNamedAttribute")) {
          bNodeSocket *socket = nodeFindSocket(node, SOCK_IN, "Name");
          if (STREQ(socket->default_value_typed<bNodeSocketValueString>()->value, "crease")) {
            STRNCPY(socket->default_value_typed<bNodeSocketValueString>()->value, "crease_edge");
          }
        }
      }
    }
  }

  LISTBASE_FOREACH (Object *, object, &bmain.objects) {
    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      if (md->type != eModifierType_Nodes) {
        continue;
      }
      if (IDProperty *settings = reinterpret_cast<NodesModifierData *>(md)->settings.properties) {
        LISTBASE_FOREACH (IDProperty *, prop, &settings->data.group) {
          if (blender::StringRef(prop->name).endswith("_attribute_name")) {
            if (STREQ(IDP_String(prop), "crease")) {
              IDP_AssignString(prop, "crease_edge");
            }
          }
        }
      }
    }
  }
}

static void versioning_replace_legacy_glossy_node(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == SH_NODE_BSDF_GLOSSY_LEGACY) {
      STRNCPY(node->idname, "ShaderNodeBsdfAnisotropic");
      node->type = SH_NODE_BSDF_GLOSSY;
    }
  }
}

static void versioning_remove_microfacet_sharp_distribution(bNodeTree *ntree)
{
  /* Find all glossy, glass and refraction BSDF nodes that have their distribution
   * set to SHARP and set them to GGX, disconnect any link to the Roughness input
   * and set its value to zero. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (!ELEM(node->type, SH_NODE_BSDF_GLOSSY, SH_NODE_BSDF_GLASS, SH_NODE_BSDF_REFRACTION)) {
      continue;
    }
    if (node->custom1 != SHD_GLOSSY_SHARP_DEPRECATED) {
      continue;
    }

    node->custom1 = SHD_GLOSSY_GGX;
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      if (!STREQ(socket->identifier, "Roughness")) {
        continue;
      }

      if (socket->link != nullptr) {
        nodeRemLink(ntree, socket->link);
      }
      bNodeSocketValueFloat *socket_value = (bNodeSocketValueFloat *)socket->default_value;
      socket_value->value = 0.0f;

      break;
    }
  }
}

static void version_replace_texcoord_normal_socket(bNodeTree *ntree)
{
  /* The normal of a spot light was set to the incoming light direction, replace with the
   * `Incoming` socket from the Geometry shader node. */
  bNode *geometry_node = nullptr;
  bNode *transform_node = nullptr;
  bNodeSocket *incoming_socket = nullptr;
  bNodeSocket *vec_in_socket = nullptr;
  bNodeSocket *vec_out_socket = nullptr;

  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->fromnode->type == SH_NODE_TEX_COORD && STREQ(link->fromsock->identifier, "Normal")) {
      if (geometry_node == nullptr) {
        geometry_node = nodeAddStaticNode(nullptr, ntree, SH_NODE_NEW_GEOMETRY);
        incoming_socket = nodeFindSocket(geometry_node, SOCK_OUT, "Incoming");

        transform_node = nodeAddStaticNode(nullptr, ntree, SH_NODE_VECT_TRANSFORM);
        vec_in_socket = nodeFindSocket(transform_node, SOCK_IN, "Vector");
        vec_out_socket = nodeFindSocket(transform_node, SOCK_OUT, "Vector");

        NodeShaderVectTransform *nodeprop = (NodeShaderVectTransform *)transform_node->storage;
        nodeprop->type = SHD_VECT_TRANSFORM_TYPE_NORMAL;

        nodeAddLink(ntree, geometry_node, incoming_socket, transform_node, vec_in_socket);
      }
      nodeAddLink(ntree, transform_node, vec_out_socket, link->tonode, link->tosock);
      nodeRemLink(ntree, link);
    }
  }
}

static void version_principled_transmission_roughness(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }
    bNodeSocket *sock = nodeFindSocket(node, SOCK_IN, "Transmission Roughness");
    if (sock != nullptr) {
      nodeRemoveSocket(ntree, node, sock);
    }
  }
}

/* Convert legacy Velvet BSDF nodes into the new Sheen BSDF node. */
static void version_replace_velvet_sheen_node(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == SH_NODE_BSDF_SHEEN) {
      STRNCPY(node->idname, "ShaderNodeBsdfSheen");

      bNodeSocket *sigmaInput = nodeFindSocket(node, SOCK_IN, "Sigma");
      if (sigmaInput != nullptr) {
        node->custom1 = SHD_SHEEN_ASHIKHMIN;
        STRNCPY(sigmaInput->identifier, "Roughness");
        STRNCPY(sigmaInput->name, "Roughness");
      }
    }
  }
}

/* Convert sheen inputs on the Principled BSDF. */
static void version_principled_bsdf_sheen(bNodeTree *ntree)
{
  auto check_node = [](const bNode *node) {
    return (node->type == SH_NODE_BSDF_PRINCIPLED) &&
           (nodeFindSocket(node, SOCK_IN, "Sheen Roughness") == nullptr);
  };
  auto update_input = [ntree](bNode *node, bNodeSocket *input) {
    /* Change socket type to Color. */
    nodeModifySocketTypeStatic(ntree, node, input, SOCK_RGBA, 0);

    /* Account for the change in intensity between the old and new model.
     * If the Sheen input is set to a fixed value, adjust it and set the tint to white.
     * Otherwise, if it's connected, keep it as-is but set the tint to 0.2 instead. */
    bNodeSocket *sheen = nodeFindSocket(node, SOCK_IN, "Sheen");
    if (sheen != nullptr && sheen->link == nullptr) {
      *version_cycles_node_socket_float_value(sheen) *= 0.2f;

      static float default_value[] = {1.0f, 1.0f, 1.0f, 1.0f};
      copy_v4_v4(version_cycles_node_socket_rgba_value(input), default_value);
    }
    else {
      static float default_value[] = {0.2f, 0.2f, 0.2f, 1.0f};
      copy_v4_v4(version_cycles_node_socket_rgba_value(input), default_value);
    }
  };
  auto update_input_link = [](bNode *, bNodeSocket *, bNode *, bNodeSocket *) {
    /* Don't replace the link here, tint works differently enough now to make conversion
     * impractical. */
  };

  version_update_node_input(ntree, check_node, "Sheen Tint", update_input, update_input_link);

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (check_node(node)) {
      bNodeSocket *input = nodeAddStaticSocket(
          ntree, node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Sheen Roughness", "Sheen Roughness");
      *version_cycles_node_socket_float_value(input) = 0.5f;
    }
  }
}

void blo_do_versions_400(FileData *fd, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 1)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      version_mesh_legacy_to_struct_of_array_format(*mesh);
    }
    version_movieclips_legacy_camera_object(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 2)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      BKE_mesh_legacy_bevel_weight_to_generic(mesh);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 3)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_geometry_nodes_add_realize_instance_nodes(ntree);
      }
    }
  }

  /* 400 4 did not require any do_version here. */

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 5)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      if (ts->snap_mode_tools != SCE_SNAP_TO_NONE) {
        ts->snap_mode_tools = SCE_SNAP_TO_GEOM;
      }

#define SCE_SNAP_PROJECT (1 << 3)
      if (ts->snap_flag & SCE_SNAP_PROJECT) {
        ts->snap_mode &= ~SCE_SNAP_TO_FACE;
        ts->snap_mode |= SCE_SNAP_INDIVIDUAL_PROJECT;
      }
#undef SCE_SNAP_PROJECT
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 6)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      BKE_mesh_legacy_face_map_to_generic(mesh);
    }
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      versioning_replace_legacy_glossy_node(ntree);
      versioning_remove_microfacet_sharp_distribution(ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 7)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      version_mesh_crease_generic(*bmain);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 8)) {
    LISTBASE_FOREACH (bAction *, act, &bmain->actions) {
      act->frame_start = max_ff(act->frame_start, MINAFRAMEF);
      act->frame_end = min_ff(act->frame_end, MAXFRAMEF);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 9)) {
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      if (light->type == LA_SPOT && light->nodetree) {
        version_replace_texcoord_normal_socket(light->nodetree);
      }
    }
  }

  /* Fix brush->tip_scale_x which should never be zero. */
  LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
    if (brush->tip_scale_x == 0.0f) {
      brush->tip_scale_x = 1.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 10)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_NODE) {
            SpaceNode *snode = reinterpret_cast<SpaceNode *>(space);
            snode->overlay.flag |= SN_OVERLAY_SHOW_PREVIEWS;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 11)) {
    version_vertex_weight_edit_preserve_threshold_exclusivity(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 12)) {
    if (!DNA_struct_elem_find(fd->filesdna, "LightProbe", "int", "grid_bake_samples")) {
      LISTBASE_FOREACH (LightProbe *, lightprobe, &bmain->lightprobes) {
        lightprobe->grid_bake_samples = 2048;
        lightprobe->surfel_density = 1.0f;
        lightprobe->grid_normal_bias = 0.3f;
        lightprobe->grid_view_bias = 0.0f;
        lightprobe->grid_facing_bias = 0.5f;
      }
    }

    /* Set default bake resolution. */
    if (!DNA_struct_elem_find(fd->filesdna, "LightProbe", "int", "resolution")) {
      LISTBASE_FOREACH (LightProbe *, lightprobe, &bmain->lightprobes) {
        lightprobe->resolution = LIGHT_PROBE_RESOLUTION_1024;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "World", "int", "probe_resolution")) {
      LISTBASE_FOREACH (World *, world, &bmain->worlds) {
        world->probe_resolution = LIGHT_PROBE_RESOLUTION_1024;
      }
    }

    /* Clear removed "Z Buffer" flag. */
    {
      const int R_IMF_FLAG_ZBUF_LEGACY = 1 << 0;
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->r.im_format.flag &= ~R_IMF_FLAG_ZBUF_LEGACY;
      }
    }

    /* Reset the layer opacity for all layers to 1. */
    LISTBASE_FOREACH (GreasePencil *, grease_pencil, &bmain->grease_pencils) {
      for (blender::bke::greasepencil::Layer *layer : grease_pencil->layers_for_write()) {
        layer->opacity = 1.0f;
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        /* Remove Transmission Roughness from Principled BSDF. */
        version_principled_transmission_roughness(ntree);
        /* Convert legacy Velvet BSDF nodes into the new Sheen BSDF node. */
        version_replace_velvet_sheen_node(ntree);
        /* Convert sheen inputs on the Principled BSDF. */
        version_principled_bsdf_sheen(ntree);
      }
    }
    FOREACH_NODETREE_END;

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;

          /* Layout based regions used to also disallow resizing, now these are separate flags.
           * Make sure they are set together for old regions. */
          LISTBASE_FOREACH (ARegion *, region, regionbase) {
            if (region->flag & RGN_FLAG_DYNAMIC_SIZE) {
              region->flag |= RGN_FLAG_NO_USER_RESIZE;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 13)) {
    /* For the scenes configured to use the "None" display disable the color management
     * again. This will handle situation when the "None" display is removed and is replaced with
     * a "Raw" view instead.
     *
     * Note that this versioning will do nothing if the "None" display exists in the OCIO
     * configuration. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      const ColorManagedDisplaySettings &display_settings = scene->display_settings;
      if (STREQ(display_settings.display_device, "None")) {
        BKE_scene_disable_color_management(scene);
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #do_versions_after_linking_400 in this file.
   * - `versioning_userdef.cc`, #blo_do_versions_userdef
   * - `versioning_userdef.cc`, #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }

  if (!DNA_struct_find(fd->filesdna, "RegionAssetShelf")) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_VIEW3D) {
            continue;
          }

          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;

          if (ARegion *new_shelf_region = do_versions_add_region_if_not_found(
                  regionbase,
                  RGN_TYPE_ASSET_SHELF,
                  "asset shelf for view3d (versioning)",
                  RGN_TYPE_TOOL_HEADER))
          {
            new_shelf_region->alignment = RGN_ALIGN_BOTTOM;
          }
          if (ARegion *new_shelf_header = do_versions_add_region_if_not_found(
                  regionbase,
                  RGN_TYPE_ASSET_SHELF_HEADER,
                  "asset shelf header for view3d (versioning)",
                  RGN_TYPE_ASSET_SHELF))
          {
            new_shelf_header->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
          }
        }
      }
    }
  }
}
