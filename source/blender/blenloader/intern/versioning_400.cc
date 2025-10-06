/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "ANIM_armature_iter.hh"
#include "ANIM_bone_collections.hh"

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_defaults.h"
#include "DNA_genfile.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_world_types.h"

#undef DNA_GENFILE_VERSIONING_MACROS

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_attribute.hh"
#include "BKE_curve.hh"
#include "BKE_effect.h"
#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_texture.h"
#include "BKE_tracking.h"

#include "SEQ_iterator.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "BLO_read_write.hh"

#include "readfile.hh"

#include "versioning_common.hh"

static void version_composite_nodetree_null_id(bNodeTree *ntree, Scene *scene)
{
  for (bNode *node : ntree->all_nodes()) {
    if (node->id == nullptr && ((node->type_legacy == CMP_NODE_R_LAYERS) ||
                                (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
                                 node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER)))
    {
      node->id = &scene->id;
    }
  }
}

/* Move bone-group color to the individual bones. */
static void version_bonegroup_migrate_color(Main *bmain)
{
  using PoseSet = blender::Set<bPose *>;
  blender::Map<bArmature *, PoseSet> armature_poses;

  /* Gather a mapping from armature to the poses that use it. */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->type != OB_ARMATURE || !ob->pose) {
      continue;
    }

    bArmature *arm = reinterpret_cast<bArmature *>(ob->data);
    BLI_assert_msg(GS(arm->id.name) == ID_AR,
                   "Expected ARMATURE object to have an Armature as data");

    /* There is no guarantee that the current state of poses is in sync with the Armature data.
     *
     * NOTE: No need to handle user reference-counting in readfile code. */
    BKE_pose_ensure(bmain, ob, arm, false);

    PoseSet &pose_set = armature_poses.lookup_or_add_default(arm);
    pose_set.add(ob->pose);
  }

  /* Move colors from the pose's bone-group to either the armature bones or the
   * pose bones, depending on how many poses use the Armature. */
  for (const PoseSet &pose_set : armature_poses.values()) {
    /* If the Armature is shared, the bone group colors might be different, and thus they have to
     * be stored on the pose bones. If the Armature is NOT shared, the bone colors can be stored
     * directly on the Armature bones. */
    const bool store_on_armature = pose_set.size() == 1;

    for (bPose *pose : pose_set) {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
        const bActionGroup *bgrp = (const bActionGroup *)BLI_findlink(&pose->agroups,
                                                                      (pchan->agrp_index - 1));
        if (!bgrp) {
          continue;
        }

        BoneColor &bone_color = store_on_armature ? pchan->bone->color : pchan->color;
        bone_color.palette_index = bgrp->customCol;
        memcpy(&bone_color.custom, &bgrp->cs, sizeof(bone_color.custom));
      }
    }
  }
}

static void version_bonelayers_to_bonecollections(Main *bmain)
{
  char bcoll_name[MAX_NAME];
  char custom_prop_name[MAX_NAME];

  LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
    IDProperty *arm_idprops = IDP_GetProperties(&arm->id);

    BLI_assert_msg(arm->edbo == nullptr, "did not expect an Armature to be saved in edit mode");
    const uint layer_used = arm->layer_used;

    /* Construct a bone collection for each layer that contains at least one bone. */
    blender::Vector<std::pair<uint, BoneCollection *>> layermask_collection;
    for (uint layer = 0; layer < 32; ++layer) {
      const uint layer_mask = 1u << layer;
      if ((layer_used & layer_mask) == 0) {
        /* Layer is empty, so no need to convert to collection. */
        continue;
      }

      /* Construct a suitable name for this bone layer. */
      bcoll_name[0] = '\0';
      if (arm_idprops) {
        /* See if we can use the layer name from the Bone Manager add-on. This is a popular add-on
         * for managing bone layers and giving them names. */
        SNPRINTF_UTF8(custom_prop_name, "layer_name_%u", layer);
        IDProperty *prop = IDP_GetPropertyFromGroup(arm_idprops, custom_prop_name);
        if (prop != nullptr && prop->type == IDP_STRING && IDP_string_get(prop)[0] != '\0') {
          SNPRINTF_UTF8(bcoll_name, "Layer %u - %s", layer + 1, IDP_string_get(prop));
        }
      }
      if (bcoll_name[0] == '\0') {
        /* Either there was no name defined in the custom property, or
         * it was the empty string. */
        SNPRINTF_UTF8(bcoll_name, "Layer %u", layer + 1);
      }

      /* Create a new bone collection for this layer. */
      BoneCollection *bcoll = ANIM_armature_bonecoll_new(arm, bcoll_name);
      layermask_collection.append(std::make_pair(layer_mask, bcoll));

      if ((arm->layer & layer_mask) == 0) {
        ANIM_bonecoll_hide(arm, bcoll);
      }
    }

    /* Iterate over the bones to assign them to their layers. */
    blender::animrig::ANIM_armature_foreach_bone(&arm->bonebase, [&](Bone *bone) {
      for (auto layer_bcoll : layermask_collection) {
        const uint layer_mask = layer_bcoll.first;
        if ((bone->layer & layer_mask) == 0) {
          continue;
        }

        BoneCollection *bcoll = layer_bcoll.second;
        ANIM_armature_bonecoll_assign(bcoll, bone);
      }
    });
  }
}

static void version_bonegroups_to_bonecollections(Main *bmain)
{
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->type != OB_ARMATURE || !ob->pose) {
      continue;
    }

    /* Convert the bone groups on a bone-by-bone basis. */
    bArmature *arm = reinterpret_cast<bArmature *>(ob->data);
    bPose *pose = ob->pose;

    blender::Map<const bActionGroup *, BoneCollection *> collections_by_group;
    /* Convert all bone groups, regardless of whether they contain any bones. */
    LISTBASE_FOREACH (bActionGroup *, bgrp, &pose->agroups) {
      BoneCollection *bcoll = ANIM_armature_bonecoll_new(arm, bgrp->name);
      collections_by_group.add_new(bgrp, bcoll);

      /* Before now, bone visibility was determined by armature layers, and bone
       * groups did not have any impact on this. To retain the behavior, that
       * hiding all layers a bone is on hides the bone, the
       * bone-group-collections should be created hidden. */
      ANIM_bonecoll_hide(arm, bcoll);
    }

    /* Assign the bones to their bone group based collection. */
    LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
      /* Find the bone group of this pose channel. */
      const bActionGroup *bgrp = (const bActionGroup *)BLI_findlink(&pose->agroups,
                                                                    (pchan->agrp_index - 1));
      if (!bgrp) {
        continue;
      }

      /* Assign the bone. */
      BoneCollection *bcoll = collections_by_group.lookup(bgrp);
      ANIM_armature_bonecoll_assign(bcoll, pchan->bone);
    }

    /* The list of bone groups (pose->agroups) is intentionally left alone here. This will allow
     * for older versions of Blender to open the file with bone groups intact. Of course the bone
     * groups will not be updated any more, but this way the data at least survives an accidental
     * save with Blender 4.0. */
  }
}

static void version_principled_bsdf_update_animdata(ID *owner_id, bNodeTree *ntree)
{
  ID *id = &ntree->id;
  AnimData *adt = BKE_animdata_from_id(id);

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }

    char node_name_escaped[MAX_NAME * 2];
    BLI_str_escape(node_name_escaped, node->name, sizeof(node_name_escaped));
    std::string prefix = "nodes[\"" + std::string(node_name_escaped) + "\"].inputs";

    /* Remove animdata for inputs 18 (Transmission Roughness) and 3 (Subsurface Color). */
    BKE_animdata_fix_paths_remove(id, (prefix + "[18]").c_str());
    BKE_animdata_fix_paths_remove(id, (prefix + "[3]").c_str());

    /* Order is important here: If we e.g. want to change A->B and B->C, but perform A->B first,
     * then later we don't know whether a B entry is an original B (and therefore should be
     * changed to C) or used to be A and was already handled.
     * In practice, going reverse mostly works, the two notable dependency chains are:
     * - 8->13, then 2->8, then 9->2 (13 was changed before)
     * - 1->9, then 6->1 (9 was changed before)
     * - 4->10, then 21->4 (10 was changed before)
     *
     * 0 (Base Color) and 17 (Transmission) are fine as-is. */
    std::pair<int, int> remap_table[] = {
        {20, 27}, /* Emission Strength */
        {19, 26}, /* Emission */
        {16, 3},  /* IOR */
        {15, 19}, /* Clearcoat Roughness */
        {14, 18}, /* Clearcoat */
        {13, 25}, /* Sheen Tint */
        {12, 23}, /* Sheen */
        {11, 15}, /* Anisotropic Rotation */
        {10, 14}, /* Anisotropic */
        {8, 13},  /* Specular Tint */
        {2, 8},   /* Subsurface Radius */
        {9, 2},   /* Roughness */
        {7, 12},  /* Specular */
        {1, 9},   /* Subsurface Scale */
        {6, 1},   /* Metallic */
        {5, 11},  /* Subsurface Anisotropy */
        {4, 10},  /* Subsurface IOR */
        {21, 4}   /* Alpha */
    };
    for (const auto &entry : remap_table) {
      BKE_animdata_fix_paths_rename(
          id, adt, owner_id, prefix.c_str(), nullptr, nullptr, entry.first, entry.second, false);
    }
  }
}

static bool versioning_convert_strip_speed_factor(Strip *strip, void *user_data)
{
  const Scene *scene = static_cast<Scene *>(user_data);
  const float speed_factor = strip->speed_factor;

  if (speed_factor == 1.0f || !blender::seq::retiming_is_allowed(strip) ||
      blender::seq::retiming_keys_count(strip) > 0)
  {
    return true;
  }

  blender::seq::retiming_data_ensure(strip);
  SeqRetimingKey *last_key = &blender::seq::retiming_keys_get(strip)[1];

  last_key->strip_frame_index = (strip->len) / speed_factor;

  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    const int prev_length = strip->len - strip->startofs - strip->endofs;
    const float left_handle = blender::seq::time_left_handle_frame_get(scene, strip);
    blender::seq::time_right_handle_frame_set(scene, strip, left_handle + prev_length);
  }

  return true;
}

void do_versions_after_linking_400(FileData *fd, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 9)) {
    /* Fix area light scaling. */
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      light->energy = light->energy_deprecated;
      if (light->type == LA_AREA) {
        light->energy *= M_PI_4;
      }
    }

    /* XXX This was added several years ago in `lib_link` code of Scene... Should be safe enough
     * here. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->nodetree) {
        version_composite_nodetree_null_id(scene->nodetree, scene);
      }
    }

    /* XXX This was added many years ago (1c19940198) in `lib_link` code of particles as a bug-fix.
     * But this is actually versioning. Should be safe enough here. */
    LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
      if (!part->effector_weights) {
        part->effector_weights = BKE_effector_add_weights(part->force_group);
      }
    }

    /* Object proxies have been deprecated sine 3.x era, so their update & sanity check can now
     * happen in do_versions code. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->proxy) {
        /* Paranoia check, actually a proxy_from pointer should never be written... */
        if (!ID_IS_LINKED(ob->proxy)) {
          ob->proxy->proxy_from = nullptr;
          ob->proxy = nullptr;

          if (ob->id.lib) {
            BLO_reportf_wrap(fd->reports,
                             RPT_INFO,
                             RPT_("Proxy lost from object %s lib %s\n"),
                             ob->id.name + 2,
                             ob->id.lib->filepath);
          }
          else {
            BLO_reportf_wrap(fd->reports,
                             RPT_INFO,
                             RPT_("Proxy lost from object %s lib <NONE>\n"),
                             ob->id.name + 2);
          }
          fd->reports->count.missing_obproxies++;
        }
        else {
          /* This triggers object_update to always use a copy. */
          ob->proxy->proxy_from = ob;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 21)) {
    if (!DNA_struct_member_exists(fd->filesdna, "bPoseChannel", "BoneColor", "color")) {
      version_bonegroup_migrate_color(bmain);
    }

    if (!DNA_struct_member_exists(fd->filesdna, "bArmature", "ListBase", "collections")) {
      version_bonelayers_to_bonecollections(bmain);
      version_bonegroups_to_bonecollections(bmain);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 24)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        /* Convert animdata on the Principled BSDF sockets. */
        version_principled_bsdf_update_animdata(id, ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 27)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = blender::seq::editing_get(scene);
      if (ed != nullptr) {
        blender::seq::foreach_strip(&ed->seqbase, versioning_convert_strip_speed_factor, scene);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 34)) {
    BKE_mesh_legacy_face_map_to_generic(bmain);
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
  tracking.reconstruction_legacy = MovieTrackingReconstruction{};
}

static void version_movieclips_legacy_camera_object(Main *bmain)
{
  LISTBASE_FOREACH (MovieClip *, movieclip, &bmain->movieclips) {
    version_motion_tracking_legacy_camera_object(*movieclip);
  }
}

static void versioning_replace_legacy_glossy_node(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_BSDF_GLOSSY_LEGACY) {
      STRNCPY_UTF8(node->idname, "ShaderNodeBsdfAnisotropic");
      node->type_legacy = SH_NODE_BSDF_GLOSSY;
    }
  }
}

static void versioning_remove_microfacet_sharp_distribution(bNodeTree *ntree)
{
  /* Find all glossy, glass and refraction BSDF nodes that have their distribution
   * set to SHARP and set them to GGX, disconnect any link to the Roughness input
   * and set its value to zero. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (!ELEM(node->type_legacy, SH_NODE_BSDF_GLOSSY, SH_NODE_BSDF_GLASS, SH_NODE_BSDF_REFRACTION))
    {
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
        blender::bke::node_remove_link(ntree, *socket->link);
      }
      bNodeSocketValueFloat *socket_value = (bNodeSocketValueFloat *)socket->default_value;
      socket_value->value = 0.0f;

      break;
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
                     "GeometryNodeInputNamedAttribute"))
        {
          bNodeSocket *socket = blender::bke::node_find_socket(*node, SOCK_IN, "Name");
          if (STREQ(socket->default_value_typed<bNodeSocketValueString>()->value, "crease")) {
            STRNCPY_UTF8(socket->default_value_typed<bNodeSocketValueString>()->value,
                         "crease_edge");
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
            if (STREQ(IDP_string_get(prop), "crease")) {
              IDP_AssignString(prop, "crease_edge");
            }
          }
        }
      }
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
    if (link->fromnode->type_legacy == SH_NODE_TEX_COORD &&
        STREQ(link->fromsock->identifier, "Normal"))
    {
      if (geometry_node == nullptr) {
        geometry_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_NEW_GEOMETRY);
        incoming_socket = blender::bke::node_find_socket(*geometry_node, SOCK_OUT, "Incoming");

        transform_node = blender::bke::node_add_static_node(
            nullptr, *ntree, SH_NODE_VECT_TRANSFORM);
        vec_in_socket = blender::bke::node_find_socket(*transform_node, SOCK_IN, "Vector");
        vec_out_socket = blender::bke::node_find_socket(*transform_node, SOCK_OUT, "Vector");

        NodeShaderVectTransform *nodeprop = (NodeShaderVectTransform *)transform_node->storage;
        nodeprop->type = SHD_VECT_TRANSFORM_TYPE_NORMAL;

        blender::bke::node_add_link(
            *ntree, *geometry_node, *incoming_socket, *transform_node, *vec_in_socket);
      }
      blender::bke::node_add_link(
          *ntree, *transform_node, *vec_out_socket, *link->tonode, *link->tosock);
      blender::bke::node_remove_link(ntree, *link);
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

static void version_principled_transmission_roughness(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }
    bNodeSocket *sock = blender::bke::node_find_socket(*node, SOCK_IN, "Transmission Roughness");
    if (sock != nullptr) {
      blender::bke::node_remove_socket(*ntree, *node, *sock);
    }
  }
}

/* Convert legacy Velvet BSDF nodes into the new Sheen BSDF node. */
static void version_replace_velvet_sheen_node(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_BSDF_SHEEN) {
      STRNCPY_UTF8(node->idname, "ShaderNodeBsdfSheen");

      bNodeSocket *sigmaInput = blender::bke::node_find_socket(*node, SOCK_IN, "Sigma");
      if (sigmaInput != nullptr) {
        node->custom1 = SHD_SHEEN_ASHIKHMIN;
        STRNCPY_UTF8(sigmaInput->identifier, "Roughness");
        STRNCPY_UTF8(sigmaInput->name, "Roughness");
      }
    }
  }
}

/* Convert sheen inputs on the Principled BSDF. */
static void version_principled_bsdf_sheen(bNodeTree *ntree)
{
  auto check_node = [](const bNode *node) {
    return (node->type_legacy == SH_NODE_BSDF_PRINCIPLED) &&
           (blender::bke::node_find_socket(*node, SOCK_IN, "Sheen Roughness") == nullptr);
  };
  auto update_input = [ntree](bNode *node, bNodeSocket *input) {
    /* Change socket type to Color. */
    blender::bke::node_modify_socket_type_static(ntree, node, input, SOCK_RGBA, 0);

    /* Account for the change in intensity between the old and new model.
     * If the Sheen input is set to a fixed value, adjust it and set the tint to white.
     * Otherwise, if it's connected, keep it as-is but set the tint to 0.2 instead. */
    bNodeSocket *sheen = blender::bke::node_find_socket(*node, SOCK_IN, "Sheen");
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
}

/* Replace old Principled Hair BSDF as a variant in the new Principled Hair BSDF. */
static void version_replace_principled_hair_model(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_HAIR_PRINCIPLED) {
      continue;
    }
    NodeShaderHairPrincipled *data = MEM_callocN<NodeShaderHairPrincipled>(__func__);
    data->model = SHD_PRINCIPLED_HAIR_CHIANG;
    data->parametrization = node->custom1;

    node->storage = data;
  }
}

static bNodeTreeInterfaceItem *legacy_socket_move_to_interface(bNodeSocket &legacy_socket,
                                                               const eNodeSocketInOut in_out)
{
  bNodeTreeInterfaceSocket *new_socket = MEM_callocN<bNodeTreeInterfaceSocket>(__func__);
  new_socket->item.item_type = NODE_INTERFACE_SOCKET;

  /* Move reusable data. */
  new_socket->name = BLI_strdup(legacy_socket.name);
  new_socket->identifier = BLI_strdup(legacy_socket.identifier);
  new_socket->description = BLI_strdup(legacy_socket.description);
  /* If the socket idname includes a subtype (e.g. "NodeSocketFloatFactor") this will convert it to
   * the base type name ("NodeSocketFloat"). */
  new_socket->socket_type = BLI_strdup(
      legacy_socket_idname_to_socket_type(legacy_socket.idname).data());
  new_socket->flag = (in_out == SOCK_IN ? NODE_INTERFACE_SOCKET_INPUT :
                                          NODE_INTERFACE_SOCKET_OUTPUT);
  SET_FLAG_FROM_TEST(
      new_socket->flag, legacy_socket.flag & SOCK_HIDE_VALUE, NODE_INTERFACE_SOCKET_HIDE_VALUE);
  SET_FLAG_FROM_TEST(new_socket->flag,
                     legacy_socket.flag & SOCK_HIDE_IN_MODIFIER,
                     NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER);
  new_socket->attribute_domain = legacy_socket.attribute_domain;

  /* The following data are stolen from the old data, the ownership of their memory is directly
   * transferred to the new data. */
  new_socket->default_attribute_name = legacy_socket.default_attribute_name;
  legacy_socket.default_attribute_name = nullptr;
  new_socket->socket_data = legacy_socket.default_value;
  legacy_socket.default_value = nullptr;
  new_socket->properties = legacy_socket.prop;
  legacy_socket.prop = nullptr;

  /* Unused data. */
  MEM_delete(legacy_socket.runtime);
  legacy_socket.runtime = nullptr;

  return &new_socket->item;
}

static void versioning_convert_node_tree_socket_lists_to_interface(bNodeTree *ntree)
{
  bNodeTreeInterface &tree_interface = ntree->tree_interface;

  const int num_inputs = BLI_listbase_count(&ntree->inputs_legacy);
  const int num_outputs = BLI_listbase_count(&ntree->outputs_legacy);
  tree_interface.root_panel.items_num = num_inputs + num_outputs;
  tree_interface.root_panel.items_array = MEM_malloc_arrayN<bNodeTreeInterfaceItem *>(
      size_t(tree_interface.root_panel.items_num), __func__);

  /* Convert outputs first to retain old outputs/inputs ordering. */
  int index;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &ntree->outputs_legacy, index) {
    tree_interface.root_panel.items_array[index] = legacy_socket_move_to_interface(*socket,
                                                                                   SOCK_OUT);
  }
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &ntree->inputs_legacy, index) {
    tree_interface.root_panel.items_array[num_outputs + index] = legacy_socket_move_to_interface(
        *socket, SOCK_IN);
  }
}

/* Convert coat inputs on the Principled BSDF. */
static void version_principled_bsdf_coat(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }
    if (blender::bke::node_find_socket(*node, SOCK_IN, "Coat IOR") != nullptr) {
      continue;
    }
    bNodeSocket *coat_ior_input = blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Coat IOR", "Coat IOR");

    /* Adjust for 4x change in intensity. */
    bNodeSocket *coat_input = blender::bke::node_find_socket(*node, SOCK_IN, "Clearcoat");
    *version_cycles_node_socket_float_value(coat_input) *= 0.25f;
    /* When the coat input is dynamic, instead of inserting a *0.25 math node, set the Coat IOR
     * to 1.2 instead - this also roughly quarters reflectivity compared to the 1.5 default. */
    *version_cycles_node_socket_float_value(coat_ior_input) = (coat_input->link) ? 1.2f : 1.5f;
  }

  /* Rename sockets. */
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Clearcoat", "Coat");
  version_node_input_socket_name(
      ntree, SH_NODE_BSDF_PRINCIPLED, "Clearcoat Roughness", "Coat Roughness");
  version_node_input_socket_name(
      ntree, SH_NODE_BSDF_PRINCIPLED, "Clearcoat Normal", "Coat Normal");
}

/* Convert subsurface inputs on the Principled BSDF. */
static void version_principled_bsdf_subsurface(bNodeTree *ntree)
{
  /* - Create Subsurface Scale input
   * - If a node's Subsurface input was connected or nonzero:
   *   - Make the Base Color a mix of old Base Color and Subsurface Color,
   *     using Subsurface as the mix factor
   *   - Move Subsurface link and default value to the new Subsurface Scale input
   *   - Set the Subsurface input to 1.0
   * - Remove Subsurface Color input
   */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }
    if (blender::bke::node_find_socket(*node, SOCK_IN, "Subsurface Scale")) {
      /* Node is already updated. */
      continue;
    }

    /* Add Scale input */
    bNodeSocket *scale_in = blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_IN, SOCK_FLOAT, PROP_DISTANCE, "Subsurface Scale", "Subsurface Scale");

    bNodeSocket *subsurf = blender::bke::node_find_socket(*node, SOCK_IN, "Subsurface");
    float *subsurf_val = version_cycles_node_socket_float_value(subsurf);

    if (!subsurf->link && *subsurf_val == 0.0f) {
      *version_cycles_node_socket_float_value(scale_in) = 0.05f;
    }
    else {
      *version_cycles_node_socket_float_value(scale_in) = *subsurf_val;
    }

    if (subsurf->link == nullptr && *subsurf_val == 0.0f) {
      /* Node doesn't use Subsurf, we're done here. */
      continue;
    }

    /* Fix up Subsurface Color input */
    bNodeSocket *base_col = blender::bke::node_find_socket(*node, SOCK_IN, "Base Color");
    bNodeSocket *subsurf_col = blender::bke::node_find_socket(*node, SOCK_IN, "Subsurface Color");
    float *base_col_val = version_cycles_node_socket_rgba_value(base_col);
    float *subsurf_col_val = version_cycles_node_socket_rgba_value(subsurf_col);
    /* If any of the three inputs is dynamic, we need a Mix node. */
    if (subsurf->link || subsurf_col->link || base_col->link) {
      bNode *mix = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MIX);
      static_cast<NodeShaderMix *>(mix->storage)->data_type = SOCK_RGBA;
      mix->locx_legacy = node->locx_legacy - 170;
      mix->locy_legacy = node->locy_legacy - 120;

      bNodeSocket *a_in = blender::bke::node_find_socket(*mix, SOCK_IN, "A_Color");
      bNodeSocket *b_in = blender::bke::node_find_socket(*mix, SOCK_IN, "B_Color");
      bNodeSocket *fac_in = blender::bke::node_find_socket(*mix, SOCK_IN, "Factor_Float");
      bNodeSocket *result_out = blender::bke::node_find_socket(*mix, SOCK_OUT, "Result_Color");

      copy_v4_v4(version_cycles_node_socket_rgba_value(a_in), base_col_val);
      copy_v4_v4(version_cycles_node_socket_rgba_value(b_in), subsurf_col_val);
      *version_cycles_node_socket_float_value(fac_in) = *subsurf_val;

      if (base_col->link) {
        blender::bke::node_add_link(
            *ntree, *base_col->link->fromnode, *base_col->link->fromsock, *mix, *a_in);
        blender::bke::node_remove_link(ntree, *base_col->link);
      }
      if (subsurf_col->link) {
        blender::bke::node_add_link(
            *ntree, *subsurf_col->link->fromnode, *subsurf_col->link->fromsock, *mix, *b_in);
        blender::bke::node_remove_link(ntree, *subsurf_col->link);
      }
      if (subsurf->link) {
        blender::bke::node_add_link(
            *ntree, *subsurf->link->fromnode, *subsurf->link->fromsock, *mix, *fac_in);
        blender::bke::node_add_link(
            *ntree, *subsurf->link->fromnode, *subsurf->link->fromsock, *node, *scale_in);
        blender::bke::node_remove_link(ntree, *subsurf->link);
      }
      blender::bke::node_add_link(*ntree, *mix, *result_out, *node, *base_col);
    }
    /* Mix the fixed values. */
    interp_v4_v4v4(base_col_val, base_col_val, subsurf_col_val, *subsurf_val);

    /* Set node to 100% subsurface, 0% diffuse. */
    *subsurf_val = 1.0f;

    /* Delete Subsurface Color input */
    blender::bke::node_remove_socket(*ntree, *node, *subsurf_col);
  }
}

/* Convert emission inputs on the Principled BSDF. */
static void version_principled_bsdf_emission(bNodeTree *ntree)
{
  /* Blender 3.x and before would default to Emission = 0.0, Emission Strength = 1.0.
   * Now we default the other way around (1.0 and 0.0), but because the Strength input was added
   * a bit later, a file that only has the Emission socket would now end up as (1.0, 0.0) instead
   * of (1.0, 1.0).
   * Therefore, set strength to 1.0 for those files.
   */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }
    if (!blender::bke::node_find_socket(*node, SOCK_IN, "Emission")) {
      /* Old enough to have neither, new defaults are fine. */
      continue;
    }
    if (blender::bke::node_find_socket(*node, SOCK_IN, "Emission Strength")) {
      /* New enough to have both, no need to do anything. */
      continue;
    }
    bNodeSocket *sock = blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Emission Strength", "Emission Strength");
    *version_cycles_node_socket_float_value(sock) = 1.0f;
  }
}

static void version_copy_socket(bNodeTreeInterfaceSocket &dst,
                                const bNodeTreeInterfaceSocket &src,
                                char *identifier)
{
  /* Node socket copy function based on bNodeTreeInterface::item_copy to avoid using blenkernel. */
  dst.name = BLI_strdup_null(src.name);
  dst.description = BLI_strdup_null(src.description);
  dst.socket_type = BLI_strdup(src.socket_type);
  dst.default_attribute_name = BLI_strdup_null(src.default_attribute_name);
  dst.identifier = identifier;
  if (src.properties) {
    dst.properties = IDP_CopyProperty_ex(src.properties, 0);
  }
  if (src.socket_data != nullptr) {
    dst.socket_data = MEM_dupallocN(src.socket_data);
    /* No user count increment needed, gets reset after versioning. */
  }
}

static int version_nodes_find_valid_insert_position_for_item(const bNodeTreeInterfacePanel &panel,
                                                             const bNodeTreeInterfaceItem &item,
                                                             const int initial_pos)
{
  const bool sockets_above_panels = !(panel.flag &
                                      NODE_INTERFACE_PANEL_ALLOW_SOCKETS_AFTER_PANELS);
  const blender::Span<const bNodeTreeInterfaceItem *> items = {panel.items_array, panel.items_num};

  int pos = initial_pos;

  if (sockets_above_panels) {
    if (item.item_type == NODE_INTERFACE_PANEL) {
      /* Find the closest valid position from the end, only panels at or after #position. */
      for (int test_pos = items.size() - 1; test_pos >= initial_pos; test_pos--) {
        if (test_pos < 0) {
          /* Initial position is out of range but valid. */
          break;
        }
        if (items[test_pos]->item_type != NODE_INTERFACE_PANEL) {
          /* Found valid position, insert after the last socket item. */
          pos = test_pos + 1;
          break;
        }
      }
    }
    else {
      /* Find the closest valid position from the start, no panels at or after #position. */
      for (int test_pos = 0; test_pos <= initial_pos; test_pos++) {
        if (test_pos >= items.size()) {
          /* Initial position is out of range but valid. */
          break;
        }
        if (items[test_pos]->item_type == NODE_INTERFACE_PANEL) {
          /* Found valid position, inserting moves the first panel. */
          pos = test_pos;
          break;
        }
      }
    }
  }

  return pos;
}

static void version_nodes_insert_item(bNodeTreeInterfacePanel &parent,
                                      bNodeTreeInterfaceSocket &socket,
                                      int position)
{
  /* Apply any constraints on the item positions. */
  position = version_nodes_find_valid_insert_position_for_item(parent, socket.item, position);
  position = std::min(std::max(position, 0), parent.items_num);

  blender::MutableSpan<bNodeTreeInterfaceItem *> old_items = {parent.items_array,
                                                              parent.items_num};
  parent.items_num++;
  parent.items_array = MEM_calloc_arrayN<bNodeTreeInterfaceItem *>(parent.items_num, __func__);
  parent.items().take_front(position).copy_from(old_items.take_front(position));
  parent.items().drop_front(position + 1).copy_from(old_items.drop_front(position));
  parent.items()[position] = &socket.item;

  if (old_items.data()) {
    MEM_freeN(old_items.data());
  }
}

/* Node group interface copy function based on bNodeTreeInterface::insert_item_copy. */
static void version_node_group_split_socket(bNodeTreeInterface &tree_interface,
                                            bNodeTreeInterfaceSocket &socket,
                                            bNodeTreeInterfacePanel *parent,
                                            int position)
{
  if (parent == nullptr) {
    parent = &tree_interface.root_panel;
  }

  bNodeTreeInterfaceSocket *csocket = static_cast<bNodeTreeInterfaceSocket *>(
      MEM_dupallocN(&socket));
  /* Generate a new unique identifier.
   * This might break existing links, but the identifiers were duplicate anyway. */
  char *dst_identifier = BLI_sprintfN("Socket_%d", tree_interface.next_uid++);
  version_copy_socket(*csocket, socket, dst_identifier);

  version_nodes_insert_item(*parent, *csocket, position);

  /* Original socket becomes output. */
  socket.flag &= ~NODE_INTERFACE_SOCKET_INPUT;
  /* Copied socket becomes input. */
  csocket->flag &= ~NODE_INTERFACE_SOCKET_OUTPUT;
}

static void versioning_node_group_sort_sockets_recursive(bNodeTreeInterfacePanel &panel)
{
  /* True if item a should be above item b. */
  auto item_compare = [](const bNodeTreeInterfaceItem *a,
                         const bNodeTreeInterfaceItem *b) -> bool {
    if (a->item_type != b->item_type) {
      /* Keep sockets above panels. */
      return a->item_type == NODE_INTERFACE_SOCKET;
    }
    /* Keep outputs above inputs. */
    if (a->item_type == NODE_INTERFACE_SOCKET) {
      const bNodeTreeInterfaceSocket *sa = reinterpret_cast<const bNodeTreeInterfaceSocket *>(a);
      const bNodeTreeInterfaceSocket *sb = reinterpret_cast<const bNodeTreeInterfaceSocket *>(b);
      const bool is_output_a = sa->flag & NODE_INTERFACE_SOCKET_OUTPUT;
      const bool is_output_b = sb->flag & NODE_INTERFACE_SOCKET_OUTPUT;
      if (is_output_a != is_output_b) {
        return is_output_a;
      }
    }

    return false;
  };

  /* Sort panel content. */
  std::stable_sort(panel.items().begin(), panel.items().end(), item_compare);

  /* Sort any child panels too. */
  for (bNodeTreeInterfaceItem *item : panel.items()) {
    if (item->item_type == NODE_INTERFACE_PANEL) {
      versioning_node_group_sort_sockets_recursive(
          *reinterpret_cast<bNodeTreeInterfacePanel *>(item));
    }
  }
}

/* Convert specular tint in Principled BSDF. */
static void version_principled_bsdf_specular_tint(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }
    bNodeSocket *specular_tint_sock = blender::bke::node_find_socket(
        *node, SOCK_IN, "Specular Tint");
    if (specular_tint_sock->type == SOCK_RGBA) {
      /* Node is already updated. */
      continue;
    }

    bNodeSocket *base_color_sock = blender::bke::node_find_socket(*node, SOCK_IN, "Base Color");
    bNodeSocket *metallic_sock = blender::bke::node_find_socket(*node, SOCK_IN, "Metallic");
    float specular_tint_old = *version_cycles_node_socket_float_value(specular_tint_sock);
    float *base_color = version_cycles_node_socket_rgba_value(base_color_sock);
    float metallic = *version_cycles_node_socket_float_value(metallic_sock);

    /* Change socket type to Color. */
    blender::bke::node_modify_socket_type_static(ntree, node, specular_tint_sock, SOCK_RGBA, 0);
    float *specular_tint = version_cycles_node_socket_rgba_value(specular_tint_sock);

    /* The conversion logic here is that the new Specular Tint should be
     * mix(one, mix(base_color, one, metallic), old_specular_tint).
     * This needs to be handled both for the fixed values, as well as for any potential connected
     * inputs. */

    static float one[] = {1.0f, 1.0f, 1.0f, 1.0f};

    /* Mix the fixed values. */
    float metallic_mix[4];
    interp_v4_v4v4(metallic_mix, base_color, one, metallic);
    interp_v4_v4v4(specular_tint, one, metallic_mix, specular_tint_old);

    if (specular_tint_sock->link == nullptr && specular_tint_old <= 0.0f) {
      /* Specular Tint was fixed at zero, we don't need any conversion node setup. */
      continue;
    }

    /* If the Metallic input is dynamic, or fixed > 0 and base color is dynamic,
     * we need to insert a node to compute the metallic_mix.
     * Otherwise, use whatever is connected to the base color, or the static value
     * if it's unconnected. */
    bNodeSocket *metallic_mix_out = nullptr;
    bNode *metallic_mix_node = nullptr;
    if (metallic_sock->link || (base_color_sock->link && metallic > 0.0f)) {
      /* Metallic Mix needs to be dynamically mixed. */
      bNode *mix = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MIX);
      static_cast<NodeShaderMix *>(mix->storage)->data_type = SOCK_RGBA;
      mix->locx_legacy = node->locx_legacy - 270;
      mix->locy_legacy = node->locy_legacy - 120;

      bNodeSocket *a_in = blender::bke::node_find_socket(*mix, SOCK_IN, "A_Color");
      bNodeSocket *b_in = blender::bke::node_find_socket(*mix, SOCK_IN, "B_Color");
      bNodeSocket *fac_in = blender::bke::node_find_socket(*mix, SOCK_IN, "Factor_Float");
      metallic_mix_out = blender::bke::node_find_socket(*mix, SOCK_OUT, "Result_Color");
      metallic_mix_node = mix;

      copy_v4_v4(version_cycles_node_socket_rgba_value(a_in), base_color);
      if (base_color_sock->link) {
        blender::bke::node_add_link(*ntree,
                                    *base_color_sock->link->fromnode,
                                    *base_color_sock->link->fromsock,
                                    *mix,
                                    *a_in);
      }
      copy_v4_v4(version_cycles_node_socket_rgba_value(b_in), one);
      *version_cycles_node_socket_float_value(fac_in) = metallic;
      if (metallic_sock->link) {
        blender::bke::node_add_link(
            *ntree, *metallic_sock->link->fromnode, *metallic_sock->link->fromsock, *mix, *fac_in);
      }
    }
    else if (base_color_sock->link) {
      /* Metallic Mix is a no-op and equivalent to Base Color. */
      metallic_mix_out = base_color_sock->link->fromsock;
      metallic_mix_node = base_color_sock->link->fromnode;
    }

    /* Similar to above, if the Specular Tint input is dynamic, or fixed > 0 and metallic mix
     * is dynamic, we need to insert a node to compute the new specular tint. */
    if (specular_tint_sock->link || (metallic_mix_out && specular_tint_old > 0.0f)) {
      bNode *mix = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MIX);
      static_cast<NodeShaderMix *>(mix->storage)->data_type = SOCK_RGBA;
      mix->locx_legacy = node->locx_legacy - 170;
      mix->locy_legacy = node->locy_legacy - 120;

      bNodeSocket *a_in = blender::bke::node_find_socket(*mix, SOCK_IN, "A_Color");
      bNodeSocket *b_in = blender::bke::node_find_socket(*mix, SOCK_IN, "B_Color");
      bNodeSocket *fac_in = blender::bke::node_find_socket(*mix, SOCK_IN, "Factor_Float");
      bNodeSocket *result_out = blender::bke::node_find_socket(*mix, SOCK_OUT, "Result_Color");

      copy_v4_v4(version_cycles_node_socket_rgba_value(a_in), one);
      copy_v4_v4(version_cycles_node_socket_rgba_value(b_in), metallic_mix);
      if (metallic_mix_out) {
        blender::bke::node_add_link(*ntree, *metallic_mix_node, *metallic_mix_out, *mix, *b_in);
      }
      *version_cycles_node_socket_float_value(fac_in) = specular_tint_old;
      if (specular_tint_sock->link) {
        blender::bke::node_add_link(*ntree,
                                    *specular_tint_sock->link->fromnode,
                                    *specular_tint_sock->link->fromsock,
                                    *mix,
                                    *fac_in);
        blender::bke::node_remove_link(ntree, *specular_tint_sock->link);
      }
      blender::bke::node_add_link(*ntree, *mix, *result_out, *node, *specular_tint_sock);
    }
  }
}

/* Rename various Principled BSDF sockets. */
static void version_principled_bsdf_rename_sockets(bNodeTree *ntree)
{
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Emission", "Emission Color");
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Specular", "Specular IOR Level");
  version_node_input_socket_name(
      ntree, SH_NODE_BSDF_PRINCIPLED, "Subsurface", "Subsurface Weight");
  version_node_input_socket_name(
      ntree, SH_NODE_BSDF_PRINCIPLED, "Transmission", "Transmission Weight");
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Coat", "Coat Weight");
  version_node_input_socket_name(ntree, SH_NODE_BSDF_PRINCIPLED, "Sheen", "Sheen Weight");
}

static void enable_geometry_nodes_is_modifier(Main &bmain)
{
  /* Any node group with a first socket geometry output can potentially be a modifier. Previously
   * this wasn't an explicit option, so better to enable too many groups rather than too few. */
  LISTBASE_FOREACH (bNodeTree *, group, &bmain.nodetrees) {
    if (group->type != NTREE_GEOMETRY) {
      continue;
    }
    group->tree_interface.foreach_item([&](const bNodeTreeInterfaceItem &item) {
      if (item.item_type != NODE_INTERFACE_SOCKET) {
        return true;
      }
      const auto &socket = reinterpret_cast<const bNodeTreeInterfaceSocket &>(item);
      if ((socket.flag & NODE_INTERFACE_SOCKET_OUTPUT) == 0) {
        return true;
      }
      if (!STREQ(socket.socket_type, "NodeSocketGeometry")) {
        return true;
      }
      if (!group->geometry_node_asset_traits) {
        group->geometry_node_asset_traits = MEM_callocN<GeometryNodeAssetTraits>(__func__);
      }
      group->geometry_node_asset_traits->flag |= GEO_NODE_ASSET_MODIFIER;
      return false;
    });
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 5)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      if (ts->snap_mode_tools != SCE_SNAP_TO_NONE) {
        ts->snap_mode_tools = SCE_SNAP_TO_GEOM;
      }

#define SCE_SNAP_PROJECT (1 << 3)
      if (ts->snap_flag & SCE_SNAP_PROJECT) {
        ts->snap_mode &= ~(1 << 2); /* SCE_SNAP_TO_FACE */
        ts->snap_mode |= (1 << 8);  /* SCE_SNAP_INDIVIDUAL_PROJECT */
      }
#undef SCE_SNAP_PROJECT
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 6)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      versioning_replace_legacy_glossy_node(ntree);
      versioning_remove_microfacet_sharp_distribution(ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 7)) {
    version_mesh_crease_generic(*bmain);
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
    if (!DNA_struct_member_exists(fd->filesdna, "LightProbe", "int", "grid_bake_samples")) {
      LISTBASE_FOREACH (LightProbe *, lightprobe, &bmain->lightprobes) {
        lightprobe->grid_bake_samples = 2048;
        lightprobe->grid_normal_bias = 0.3f;
        lightprobe->grid_view_bias = 0.0f;
        lightprobe->grid_facing_bias = 0.5f;
        lightprobe->grid_dilation_threshold = 0.5f;
        lightprobe->grid_dilation_radius = 1.0f;
      }
    }

    /* Set default bake resolution. */
    if (!DNA_struct_member_exists(fd->filesdna, "World", "int", "probe_resolution")) {
      LISTBASE_FOREACH (World *, world, &bmain->worlds) {
        world->probe_resolution = LIGHT_PROBE_RESOLUTION_1024;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "LightProbe", "float", "grid_surface_bias")) {
      LISTBASE_FOREACH (LightProbe *, lightprobe, &bmain->lightprobes) {
        lightprobe->grid_surface_bias = 0.05f;
        lightprobe->grid_escape_bias = 0.1f;
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 14)) {
    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "int", "ray_tracing_method")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.ray_tracing_method = RAYTRACE_EEVEE_METHOD_SCREEN;
      }
    }

    if (!DNA_struct_exists(fd->filesdna, "RegionAssetShelf")) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 16)) {
    /* Set Normalize property of Noise Texture node to true. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_CUSTOM) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == SH_NODE_TEX_NOISE) {
            if (!node->storage) {
              NodeTexNoise *tex = MEM_callocN<NodeTexNoise>(__func__);
              BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
              BKE_texture_colormapping_default(&tex->base.color_mapping);
              tex->dimensions = 3;
              tex->type = SHD_NOISE_FBM;
              node->storage = tex;
            }
            ((NodeTexNoise *)node->storage)->normalize = true;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 17)) {
    if (!DNA_struct_exists(fd->filesdna, "NodeShaderHairPrincipled")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          version_replace_principled_hair_model(ntree);
        }
      }
      FOREACH_NODETREE_END;
    }

    /* Panorama properties shared with Eevee. */
    if (!DNA_struct_member_exists(fd->filesdna, "Camera", "float", "fisheye_fov")) {
      Camera default_cam = *DNA_struct_default_get(Camera);
      LISTBASE_FOREACH (Camera *, camera, &bmain->cameras) {
        IDProperty *ccam = version_cycles_properties_from_ID(&camera->id);
        if (ccam) {
          camera->panorama_type = version_cycles_property_int(
              ccam, "panorama_type", default_cam.panorama_type);
          camera->fisheye_fov = version_cycles_property_float(
              ccam, "fisheye_fov", default_cam.fisheye_fov);
          camera->fisheye_lens = version_cycles_property_float(
              ccam, "fisheye_lens", default_cam.fisheye_lens);
          camera->latitude_min = version_cycles_property_float(
              ccam, "latitude_min", default_cam.latitude_min);
          camera->latitude_max = version_cycles_property_float(
              ccam, "latitude_max", default_cam.latitude_max);
          camera->longitude_min = version_cycles_property_float(
              ccam, "longitude_min", default_cam.longitude_min);
          camera->longitude_max = version_cycles_property_float(
              ccam, "longitude_max", default_cam.longitude_max);
          /* Fit to match default projective camera with focal_length 50 and sensor_width 36. */
          camera->fisheye_polynomial_k0 = version_cycles_property_float(
              ccam, "fisheye_polynomial_k0", default_cam.fisheye_polynomial_k0);
          camera->fisheye_polynomial_k1 = version_cycles_property_float(
              ccam, "fisheye_polynomial_k1", default_cam.fisheye_polynomial_k1);
          camera->fisheye_polynomial_k2 = version_cycles_property_float(
              ccam, "fisheye_polynomial_k2", default_cam.fisheye_polynomial_k2);
          camera->fisheye_polynomial_k3 = version_cycles_property_float(
              ccam, "fisheye_polynomial_k3", default_cam.fisheye_polynomial_k3);
          camera->fisheye_polynomial_k4 = version_cycles_property_float(
              ccam, "fisheye_polynomial_k4", default_cam.fisheye_polynomial_k4);
        }
        else {
          camera->panorama_type = default_cam.panorama_type;
          camera->fisheye_fov = default_cam.fisheye_fov;
          camera->fisheye_lens = default_cam.fisheye_lens;
          camera->latitude_min = default_cam.latitude_min;
          camera->latitude_max = default_cam.latitude_max;
          camera->longitude_min = default_cam.longitude_min;
          camera->longitude_max = default_cam.longitude_max;
          /* Fit to match default projective camera with focal_length 50 and sensor_width 36. */
          camera->fisheye_polynomial_k0 = default_cam.fisheye_polynomial_k0;
          camera->fisheye_polynomial_k1 = default_cam.fisheye_polynomial_k1;
          camera->fisheye_polynomial_k2 = default_cam.fisheye_polynomial_k2;
          camera->fisheye_polynomial_k3 = default_cam.fisheye_polynomial_k3;
          camera->fisheye_polynomial_k4 = default_cam.fisheye_polynomial_k4;
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "LightProbe", "float", "grid_flag")) {
      LISTBASE_FOREACH (LightProbe *, lightprobe, &bmain->lightprobes) {
        /* Keep old behavior of baking the whole lighting. */
        lightprobe->grid_flag = LIGHTPROBE_GRID_CAPTURE_WORLD | LIGHTPROBE_GRID_CAPTURE_INDIRECT |
                                LIGHTPROBE_GRID_CAPTURE_EMISSION;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "int", "gi_irradiance_pool_size")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.gi_irradiance_pool_size = 16;
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->toolsettings->snap_flag_anim |= SCE_SNAP;
      scene->toolsettings->snap_anim_mode |= (1 << 10); /* SCE_SNAP_TO_FRAME */
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 20)) {
    /* Convert old socket lists into new interface items. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      versioning_convert_node_tree_socket_lists_to_interface(ntree);
      /* Clear legacy sockets after conversion.
       * Internal data pointers have been moved or freed already. */
      BLI_freelistN(&ntree->inputs_legacy);
      BLI_freelistN(&ntree->outputs_legacy);
    }
    FOREACH_NODETREE_END;
  }
  else {
    /* Legacy node tree sockets are created for forward compatibility,
     * but have to be freed after loading and versioning. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      LISTBASE_FOREACH_MUTABLE (bNodeSocket *, legacy_socket, &ntree->inputs_legacy) {
        MEM_SAFE_FREE(legacy_socket->default_attribute_name);
        MEM_SAFE_FREE(legacy_socket->default_value);
        if (legacy_socket->prop) {
          IDP_FreeProperty(legacy_socket->prop);
        }
        MEM_delete(legacy_socket->runtime);
        MEM_freeN(legacy_socket);
      }
      LISTBASE_FOREACH_MUTABLE (bNodeSocket *, legacy_socket, &ntree->outputs_legacy) {
        MEM_SAFE_FREE(legacy_socket->default_attribute_name);
        MEM_SAFE_FREE(legacy_socket->default_value);
        if (legacy_socket->prop) {
          IDP_FreeProperty(legacy_socket->prop);
        }
        MEM_delete(legacy_socket->runtime);
        MEM_freeN(legacy_socket);
      }
      BLI_listbase_clear(&ntree->inputs_legacy);
      BLI_listbase_clear(&ntree->outputs_legacy);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 22)) {
    /* Initialize root panel flags in files created before these flags were added. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      ntree->tree_interface.root_panel.flag |= NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS_LEGACY;
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 23)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == GEO_NODE_SET_SHADE_SMOOTH) {
            node->custom1 = int8_t(blender::bke::AttrDomain::Face);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 24)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        /* Convert coat inputs on the Principled BSDF. */
        version_principled_bsdf_coat(ntree);
        /* Convert subsurface inputs on the Principled BSDF. */
        version_principled_bsdf_subsurface(ntree);
        /* Convert emission on the Principled BSDF. */
        version_principled_bsdf_emission(ntree);
      }
    }
    FOREACH_NODETREE_END;

    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            const ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                         &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype != RGN_TYPE_ASSET_SHELF) {
                continue;
              }

              RegionAssetShelf *shelf_data = static_cast<RegionAssetShelf *>(region->regiondata);
              if (shelf_data && shelf_data->active_shelf &&
                  (shelf_data->active_shelf->preferred_row_count == 0))
              {
                shelf_data->active_shelf->preferred_row_count = 1;
              }
            }
          }
        }
      }
    }

    /* Convert sockets with both input and output flag into two separate sockets. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      blender::Vector<bNodeTreeInterfaceSocket *> sockets_to_split;
      ntree->tree_interface.foreach_item([&](bNodeTreeInterfaceItem &item) {
        if (item.item_type == NODE_INTERFACE_SOCKET) {
          bNodeTreeInterfaceSocket &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);
          if ((socket.flag & NODE_INTERFACE_SOCKET_INPUT) &&
              (socket.flag & NODE_INTERFACE_SOCKET_OUTPUT))
          {
            sockets_to_split.append(&socket);
          }
        }
        return true;
      });

      for (bNodeTreeInterfaceSocket *socket : sockets_to_split) {
        const int position = ntree->tree_interface.find_item_position(socket->item);
        bNodeTreeInterfacePanel *parent = ntree->tree_interface.find_item_parent(socket->item);
        version_node_group_split_socket(ntree->tree_interface, *socket, parent, position + 1);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 25)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        /* Convert specular tint on the Principled BSDF. */
        version_principled_bsdf_specular_tint(ntree);
        /* Rename some sockets. */
        version_principled_bsdf_rename_sockets(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 26)) {
    enable_geometry_nodes_is_modifier(*bmain);

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->simulation_frame_start = scene->r.sfra;
      scene->simulation_frame_end = scene->r.efra;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 27)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->timeline_overlay.flag |= SEQ_TIMELINE_SHOW_STRIP_RETIMING;
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "int", "shadow_step_count")) {
      SceneEEVEE default_scene_eevee = *DNA_struct_default_get(SceneEEVEE);
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.shadow_ray_count = default_scene_eevee.shadow_ray_count;
        scene->eevee.shadow_step_count = default_scene_eevee.shadow_step_count;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 28)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          const ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                       &sl->regionbase;
          LISTBASE_FOREACH (ARegion *, region, regionbase) {
            if (region->regiontype != RGN_TYPE_ASSET_SHELF) {
              continue;
            }

            RegionAssetShelf *shelf_data = static_cast<RegionAssetShelf *>(region->regiondata);
            if (shelf_data && shelf_data->active_shelf) {
              AssetShelfSettings &settings = shelf_data->active_shelf->settings;
              settings.asset_library_reference.custom_library_index = -1;
              settings.asset_library_reference.type = ASSET_LIBRARY_ALL;
            }

            region->flag |= RGN_FLAG_HIDDEN;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 29)) {
    /* Unhide all Reroute nodes. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->is_reroute()) {
          static_cast<bNodeSocket *>(node->inputs.first)->flag &= ~SOCK_HIDDEN;
          static_cast<bNodeSocket *>(node->outputs.first)->flag &= ~SOCK_HIDDEN;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 30)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      enum { IS_DEFAULT = 0, IS_UV, IS_NODE, IS_ANIM };
      auto versioning_snap_to = [](short snap_to_old, int type) {
        eSnapMode snap_to_new = SCE_SNAP_TO_NONE;
        if (snap_to_old & (1 << 0)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_TO_NONE :
                         type == IS_ANIM ? SCE_SNAP_TO_FRAME :
                                           SCE_SNAP_TO_VERTEX;
        }
        if (snap_to_old & (1 << 1)) {
          snap_to_new |= type == IS_NODE ? SCE_SNAP_TO_NONE :
                         type == IS_ANIM ? SCE_SNAP_TO_SECOND :
                                           SCE_SNAP_TO_EDGE;
        }
        if (ELEM(type, IS_DEFAULT, IS_ANIM) && snap_to_old & (1 << 2)) {
          snap_to_new |= type == IS_DEFAULT ? SCE_SNAP_TO_FACE : SCE_SNAP_TO_MARKERS;
        }
        if (type == IS_DEFAULT && snap_to_old & (1 << 3)) {
          snap_to_new |= SCE_SNAP_TO_VOLUME;
        }
        if (type == IS_DEFAULT && snap_to_old & (1 << 4)) {
          snap_to_new |= SCE_SNAP_TO_EDGE_MIDPOINT;
        }
        if (type == IS_DEFAULT && snap_to_old & (1 << 5)) {
          snap_to_new |= SCE_SNAP_TO_EDGE_PERPENDICULAR;
        }
        if (ELEM(type, IS_DEFAULT, IS_UV, IS_NODE) && snap_to_old & (1 << 6)) {
          snap_to_new |= SCE_SNAP_TO_INCREMENT;
        }
        if (ELEM(type, IS_DEFAULT, IS_UV, IS_NODE) && snap_to_old & (1 << 7)) {
          snap_to_new |= SCE_SNAP_TO_GRID;
        }
        if (type == IS_DEFAULT && snap_to_old & (1 << 8)) {
          snap_to_new |= SCE_SNAP_INDIVIDUAL_NEAREST;
        }
        if (type == IS_DEFAULT && snap_to_old & (1 << 9)) {
          snap_to_new |= SCE_SNAP_INDIVIDUAL_PROJECT;
        }
        if (snap_to_old & (1 << 10)) {
          snap_to_new |= SCE_SNAP_TO_FRAME;
        }
        if (snap_to_old & (1 << 11)) {
          snap_to_new |= SCE_SNAP_TO_SECOND;
        }
        if (snap_to_old & (1 << 12)) {
          snap_to_new |= SCE_SNAP_TO_MARKERS;
        }

        if (!snap_to_new) {
          snap_to_new = eSnapMode(1 << 0);
        }

        return snap_to_new;
      };

      ts->snap_mode = versioning_snap_to(ts->snap_mode, IS_DEFAULT);
      ts->snap_uv_mode = versioning_snap_to(ts->snap_uv_mode, IS_UV);
      ts->snap_node_mode = versioning_snap_to(ts->snap_node_mode, IS_NODE);
      ts->snap_anim_mode = versioning_snap_to(ts->snap_anim_mode, IS_ANIM);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 31)) {
    LISTBASE_FOREACH (Curve *, curve, &bmain->curves) {
      /* No need to check the curves type, this will be null for non text curves. */
      if (CharInfo *info = curve->strinfo) {
        for (int i = curve->len_char32 - 1; i >= 0; i--, info++) {
          if (info->mat_nr > 0) {
            /** CharInfo mat_nr used to start at 1, unlike mesh & nurbs, now zero-based. */
            info->mat_nr--;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 33)) {
    /* Fix node group socket order by sorting outputs and inputs. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      versioning_node_group_sort_sockets_recursive(ntree->tree_interface.root_panel);
    }
  }
}
