/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include <algorithm>
#include <cmath>
#include <string>

#include <fmt/format.h>

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_defaults.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "DNA_defs.h"
#include "DNA_genfile.h"
#include "DNA_particle_types.h"

#undef DNA_GENFILE_VERSIONING_MACROS

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_base.hh"
#include "BLI_math_numbers.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utils.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_attribute.hh"
#include "BKE_collection.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_customdata.hh"
#include "BKE_effect.h"
#include "BKE_fcurve.hh"
#include "BKE_file_handler.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_image_format.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_nla.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_paint.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_texture.h"
#include "BKE_tracking.h"

#include "MOV_enums.hh"

#include "SEQ_iterator.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_armature_iter.hh"
#include "ANIM_bone_collections.hh"
#include "ANIM_versioning.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "BLO_read_write.hh"
#include "BLO_readfile.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// static CLG_LogRef LOG = {"blo.readfile.doversion"};

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

static void version_fcurve_noise_modifier(FCurve &fcurve)
{
  LISTBASE_FOREACH (FModifier *, fcurve_modifier, &fcurve.modifiers) {
    if (fcurve_modifier->type != FMODIFIER_TYPE_NOISE) {
      continue;
    }
    FMod_Noise *data = static_cast<FMod_Noise *>(fcurve_modifier->data);
    data->lacunarity = 2.0f;
    data->roughness = 0.5f;
    data->legacy_noise = true;
  }
}

static void version_fix_fcurve_noise_offset(FCurve &fcurve)
{
  LISTBASE_FOREACH (FModifier *, fcurve_modifier, &fcurve.modifiers) {
    if (fcurve_modifier->type != FMODIFIER_TYPE_NOISE) {
      continue;
    }
    FMod_Noise *data = static_cast<FMod_Noise *>(fcurve_modifier->data);
    if (data->legacy_noise) {
      /* We don't want to modify anything if the noise is set to legacy, because the issue only
       * occurred on the new style noise. */
      continue;
    }
    data->offset *= data->size;
  }
}

static void nlastrips_apply_fcurve_versioning(ListBase &strips)
{
  LISTBASE_FOREACH (NlaStrip *, strip, &strips) {
    LISTBASE_FOREACH (FCurve *, fcurve, &strip->fcurves) {
      version_fix_fcurve_noise_offset(*fcurve);
    }

    /* Check sub-strips (if meta-strips). */
    nlastrips_apply_fcurve_versioning(strip->strips);
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
        SNPRINTF(custom_prop_name, "layer_name_%u", layer);
        IDProperty *prop = IDP_GetPropertyFromGroup(arm_idprops, custom_prop_name);
        if (prop != nullptr && prop->type == IDP_STRING && IDP_String(prop)[0] != '\0') {
          SNPRINTF(bcoll_name, "Layer %u - %s", layer + 1, IDP_String(prop));
        }
      }
      if (bcoll_name[0] == '\0') {
        /* Either there was no name defined in the custom property, or
         * it was the empty string. */
        SNPRINTF(bcoll_name, "Layer %u", layer + 1);
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

/**
 * Change animation/drivers from "collections[..." to "collections_all[..." so
 * they remain stable when the bone collection hierarchy structure changes.
 */
static void version_bonecollection_anim(FCurve *fcurve)
{
  const blender::StringRef rna_path(fcurve->rna_path);
  constexpr char const *rna_path_prefix = "collections[";
  if (!rna_path.startswith(rna_path_prefix)) {
    return;
  }

  const std::string path_remainder(rna_path.drop_known_prefix(rna_path_prefix));
  MEM_freeN(fcurve->rna_path);
  fcurve->rna_path = BLI_sprintfN("collections_all[%s", path_remainder.c_str());
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

static void versioning_eevee_shadow_settings(Object *object)
{
  /** EEVEE no longer uses the Material::blend_shadow property.
   * Instead, it uses Object::visibility_flag for disabling shadow casting
   */

  short *material_len = BKE_object_material_len_p(object);
  if (!material_len) {
    return;
  }

  using namespace blender;
  bool hide_shadows = *material_len > 0;
  for (int i : IndexRange(*material_len)) {
    Material *material = BKE_object_material_get(object, i + 1);
    if (!material || material->blend_shadow != MA_BS_NONE) {
      hide_shadows = false;
    }
  }

  /* Enable the hide_shadow flag only if there's not any shadow casting material. */
  SET_FLAG_FROM_TEST(object->visibility_flag, hide_shadows, OB_HIDE_SHADOW);
}

static void versioning_eevee_material_shadow_none(Material *material)
{
  if (!material->use_nodes || material->nodetree == nullptr) {
    return;
  }
  bNodeTree *ntree = material->nodetree;

  bNode *output_node = version_eevee_output_node_get(ntree, SH_NODE_OUTPUT_MATERIAL);
  bNode *old_output_node = version_eevee_output_node_get(ntree, SH_NODE_OUTPUT_MATERIAL);
  if (output_node == nullptr) {
    return;
  }

  bNodeSocket *existing_out_sock = blender::bke::node_find_socket(output_node, SOCK_IN, "Surface");
  bNodeSocket *volume_sock = blender::bke::node_find_socket(output_node, SOCK_IN, "Volume");
  if (existing_out_sock->link == nullptr && volume_sock->link) {
    /* Don't apply versioning to a material that only has a volumetric input as this makes the
     * object surface opaque to the camera, hiding the volume inside. */
    return;
  }

  if (output_node->custom1 == SHD_OUTPUT_ALL) {
    /* We do not want to affect Cycles. So we split the output into two specific outputs. */
    output_node->custom1 = SHD_OUTPUT_CYCLES;

    bNode *new_output = blender::bke::node_add_node(nullptr, ntree, "ShaderNodeOutputMaterial");
    new_output->custom1 = SHD_OUTPUT_EEVEE;
    new_output->parent = output_node->parent;
    new_output->locx_legacy = output_node->locx_legacy;
    new_output->locy_legacy = output_node->locy_legacy - output_node->height - 120;

    auto copy_link = [&](const char *socket_name) {
      bNodeSocket *sock = blender::bke::node_find_socket(output_node, SOCK_IN, socket_name);
      if (sock && sock->link) {
        bNodeLink *link = sock->link;
        bNodeSocket *to_sock = blender::bke::node_find_socket(new_output, SOCK_IN, socket_name);
        blender::bke::node_add_link(ntree, link->fromnode, link->fromsock, new_output, to_sock);
      }
    };

    /* Don't copy surface as that is handled later */
    copy_link("Volume");
    copy_link("Displacement");
    copy_link("Thickness");

    output_node = new_output;
  }

  bNodeSocket *out_sock = blender::bke::node_find_socket(output_node, SOCK_IN, "Surface");
  bNodeSocket *old_out_sock = blender::bke::node_find_socket(old_output_node, SOCK_IN, "Surface");

  /* Add mix node for mixing between original material, and transparent BSDF for shadows */
  bNode *mix_node = blender::bke::node_add_node(nullptr, ntree, "ShaderNodeMixShader");
  STRNCPY(mix_node->label, "Disable Shadow");
  mix_node->flag |= NODE_HIDDEN;
  mix_node->parent = output_node->parent;
  mix_node->locx_legacy = output_node->locx_legacy;
  mix_node->locy_legacy = output_node->locy_legacy - output_node->height - 120;
  bNodeSocket *mix_fac = static_cast<bNodeSocket *>(BLI_findlink(&mix_node->inputs, 0));
  bNodeSocket *mix_in_1 = static_cast<bNodeSocket *>(BLI_findlink(&mix_node->inputs, 1));
  bNodeSocket *mix_in_2 = static_cast<bNodeSocket *>(BLI_findlink(&mix_node->inputs, 2));
  bNodeSocket *mix_out = static_cast<bNodeSocket *>(BLI_findlink(&mix_node->outputs, 0));
  if (old_out_sock->link != nullptr) {
    blender::bke::node_add_link(
        ntree, old_out_sock->link->fromnode, old_out_sock->link->fromsock, mix_node, mix_in_1);
    if (out_sock->link != nullptr) {
      blender::bke::node_remove_link(ntree, out_sock->link);
    }
  }
  blender::bke::node_add_link(ntree, mix_node, mix_out, output_node, out_sock);

  /* Add light path node to control shadow visibility */
  bNode *lp_node = blender::bke::node_add_node(nullptr, ntree, "ShaderNodeLightPath");
  lp_node->flag |= NODE_HIDDEN;
  lp_node->parent = output_node->parent;
  lp_node->locx_legacy = output_node->locx_legacy;
  lp_node->locy_legacy = mix_node->locy_legacy + 35;
  bNodeSocket *is_shadow = blender::bke::node_find_socket(lp_node, SOCK_OUT, "Is Shadow Ray");
  blender::bke::node_add_link(ntree, lp_node, is_shadow, mix_node, mix_fac);
  /* Hide unconnected sockets for cleaner look. */
  LISTBASE_FOREACH (bNodeSocket *, sock, &lp_node->outputs) {
    if (sock != is_shadow) {
      sock->flag |= SOCK_HIDDEN;
    }
  }

  /* Add transparent BSDF to make shadows transparent. */
  bNode *bsdf_node = blender::bke::node_add_node(nullptr, ntree, "ShaderNodeBsdfTransparent");
  bsdf_node->flag |= NODE_HIDDEN;
  bsdf_node->parent = output_node->parent;
  bsdf_node->locx_legacy = output_node->locx_legacy;
  bsdf_node->locy_legacy = mix_node->locy_legacy - 35;
  bNodeSocket *bsdf_out = blender::bke::node_find_socket(bsdf_node, SOCK_OUT, "BSDF");
  blender::bke::node_add_link(ntree, bsdf_node, bsdf_out, mix_node, mix_in_2);
}

/**
 * Represents a source of transparency inside the closure part of a material node-tree.
 * Sources can be combined together down the tree to figure out where the source of the alpha is.
 * If there is multiple alpha source, we consider the tree as having complex alpha and don't do the
 * versioning.
 */
struct AlphaSource {
  enum AlphaState {
    /* Alpha input is 0. */
    ALPHA_OPAQUE = 0,
    /* Alpha input is 1. */
    ALPHA_FULLY_TRANSPARENT,
    /* Alpha is between 0 and 1, from a graph input or the result of one blending operation. */
    ALPHA_SEMI_TRANSPARENT,
    /* Alpha is unknown and the result of more than one blending operation. */
    ALPHA_COMPLEX_MIX
  };

  /* Socket that is the source of the potential semi-transparency. */
  bNodeSocket *socket = nullptr;
  /* State of the source. */
  AlphaState state;
  /* True if socket is transparency instead of alpha (e.g: `1-alpha`). */
  bool is_transparency = false;

  static AlphaSource alpha_source(bNodeSocket *fac, bool inverted = false)
  {
    return {fac, ALPHA_SEMI_TRANSPARENT, inverted};
  }
  static AlphaSource opaque()
  {
    return {nullptr, ALPHA_OPAQUE, false};
  }
  static AlphaSource fully_transparent(bNodeSocket *socket = nullptr, bool inverted = false)
  {
    return {socket, ALPHA_FULLY_TRANSPARENT, inverted};
  }
  static AlphaSource complex_alpha()
  {
    return {nullptr, ALPHA_COMPLEX_MIX, false};
  }

  bool is_opaque() const
  {
    return state == ALPHA_OPAQUE;
  }
  bool is_fully_transparent() const
  {
    return state == ALPHA_FULLY_TRANSPARENT;
  }
  bool is_transparent() const
  {
    return state != ALPHA_OPAQUE;
  }
  bool is_semi_transparent() const
  {
    return state == ALPHA_SEMI_TRANSPARENT;
  }
  bool is_complex() const
  {
    return state == ALPHA_COMPLEX_MIX;
  }

  /* Combine two source together with a blending parameter. */
  static AlphaSource mix(const AlphaSource &a, const AlphaSource &b, bNodeSocket *fac)
  {
    if (a.is_complex() || b.is_complex()) {
      return complex_alpha();
    }
    if (a.is_semi_transparent() || b.is_semi_transparent()) {
      return complex_alpha();
    }
    if (a.is_fully_transparent() && b.is_fully_transparent()) {
      return fully_transparent();
    }
    if (a.is_opaque() && b.is_opaque()) {
      return opaque();
    }
    /* Only one of them is fully transparent. */
    return alpha_source(fac, !a.is_transparent());
  }

  /* Combine two source together with an additive blending parameter. */
  static AlphaSource add(const AlphaSource &a, const AlphaSource &b)
  {
    if (a.is_complex() || b.is_complex()) {
      return complex_alpha();
    }
    if (a.is_semi_transparent() && b.is_transparent()) {
      return complex_alpha();
    }
    if (a.is_transparent() && b.is_semi_transparent()) {
      return complex_alpha();
    }
    /* Either one of them is opaque or they are both opaque. */
    return a.is_transparent() ? a : b;
  }
};

/**
 * WARNING: recursive.
 */
static AlphaSource versioning_eevee_alpha_source_get(bNodeSocket *socket, int depth = 0)
{
  if (depth > 100) {
    /* Protection against infinite / very long recursion.
     * Also a node-tree with that much depth is likely to not be compatible. */
    return AlphaSource::complex_alpha();
  }

  if (socket->link == nullptr) {
    /* Unconnected closure socket is always opaque black. */
    return AlphaSource::opaque();
  }

  bNode *node = socket->link->fromnode;

  switch (node->type_legacy) {
    case NODE_REROUTE: {
      return versioning_eevee_alpha_source_get(
          static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 0)), depth + 1);
    }

    case NODE_GROUP: {
      return AlphaSource::complex_alpha();
    }

    case SH_NODE_BSDF_TRANSPARENT: {
      bNodeSocket *socket = blender::bke::node_find_socket(node, SOCK_IN, "Color");
      if (socket->link == nullptr) {
        float *socket_color_value = version_cycles_node_socket_rgba_value(socket);
        if ((socket_color_value[0] == 0.0f) && (socket_color_value[1] == 0.0f) &&
            (socket_color_value[2] == 0.0f))
        {
          return AlphaSource::opaque();
        }
        if ((socket_color_value[0] == 1.0f) && (socket_color_value[1] == 1.0f) &&
            (socket_color_value[2] == 1.0f))
        {
          return AlphaSource::fully_transparent(socket, true);
        }
      }
      return AlphaSource::alpha_source(socket, true);
    }

    case SH_NODE_MIX_SHADER: {
      bNodeSocket *socket = blender::bke::node_find_socket(node, SOCK_IN, "Fac");
      AlphaSource src0 = versioning_eevee_alpha_source_get(
          static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 1)), depth + 1);
      AlphaSource src1 = versioning_eevee_alpha_source_get(
          static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 2)), depth + 1);

      if (socket->link == nullptr) {
        float socket_float_value = *version_cycles_node_socket_float_value(socket);
        if (socket_float_value == 0.0f) {
          return src0;
        }
        if (socket_float_value == 1.0f) {
          return src1;
        }
      }
      return AlphaSource::mix(src0, src1, socket);
    }

    case SH_NODE_ADD_SHADER: {
      AlphaSource src0 = versioning_eevee_alpha_source_get(
          static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 0)), depth + 1);
      AlphaSource src1 = versioning_eevee_alpha_source_get(
          static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 1)), depth + 1);
      return AlphaSource::add(src0, src1);
    }

    case SH_NODE_BSDF_PRINCIPLED: {
      bNodeSocket *socket = blender::bke::node_find_socket(node, SOCK_IN, "Alpha");
      if (socket->link == nullptr) {
        float socket_value = *version_cycles_node_socket_float_value(socket);
        if (socket_value == 0.0f) {
          return AlphaSource::fully_transparent(socket);
        }
        if (socket_value == 1.0f) {
          return AlphaSource::opaque();
        }
      }
      return AlphaSource::alpha_source(socket);
    }

    case SH_NODE_EEVEE_SPECULAR: {
      bNodeSocket *socket = blender::bke::node_find_socket(node, SOCK_IN, "Transparency");
      if (socket->link == nullptr) {
        float socket_value = *version_cycles_node_socket_float_value(socket);
        if (socket_value == 0.0f) {
          return AlphaSource::fully_transparent(socket, true);
        }
        if (socket_value == 1.0f) {
          return AlphaSource::opaque();
        }
      }
      return AlphaSource::alpha_source(socket, true);
    }

    default:
      return AlphaSource::opaque();
  }
}

/**
 * This function detect the alpha input of a material node-tree and then convert the input alpha to
 * a step function, either statically or using a math node when there is some value plugged in.
 * If the closure mixture mix some alpha more than once, we cannot convert automatically and keep
 * the same behavior. So we bail out in this case.
 *
 * Only handles the closure tree from the output node.
 */
static bool versioning_eevee_material_blend_mode_settings(bNodeTree *ntree, float threshold)
{
  bNode *output_node = version_eevee_output_node_get(ntree, SH_NODE_OUTPUT_MATERIAL);
  if (output_node == nullptr) {
    return true;
  }
  bNodeSocket *surface_socket = blender::bke::node_find_socket(output_node, SOCK_IN, "Surface");

  AlphaSource alpha = versioning_eevee_alpha_source_get(surface_socket);

  if (alpha.is_complex()) {
    return false;
  }
  if (alpha.socket == nullptr) {
    return true;
  }

  bool is_opaque = (threshold == 2.0f);
  if (is_opaque) {
    if (alpha.socket->link != nullptr) {
      blender::bke::node_remove_link(ntree, alpha.socket->link);
    }

    float value = (alpha.is_transparency) ? 0.0f : 1.0f;
    float values[4] = {value, value, value, 1.0f};

    /* Set default value to opaque. */
    if (alpha.socket->type == SOCK_RGBA) {
      copy_v4_v4(version_cycles_node_socket_rgba_value(alpha.socket), values);
    }
    else {
      *version_cycles_node_socket_float_value(alpha.socket) = value;
    }
  }
  else {
    if (alpha.socket->link != nullptr) {
      /* Insert math node. */
      bNode *to_node = alpha.socket->link->tonode;
      bNode *from_node = alpha.socket->link->fromnode;
      bNodeSocket *to_socket = alpha.socket->link->tosock;
      bNodeSocket *from_socket = alpha.socket->link->fromsock;
      blender::bke::node_remove_link(ntree, alpha.socket->link);

      bNode *math_node = blender::bke::node_add_node(nullptr, ntree, "ShaderNodeMath");
      math_node->custom1 = NODE_MATH_GREATER_THAN;
      math_node->flag |= NODE_HIDDEN;
      math_node->parent = to_node->parent;
      math_node->locx_legacy = to_node->locx_legacy - math_node->width - 30;
      math_node->locy_legacy = min_ff(to_node->locy_legacy, from_node->locy_legacy);

      bNodeSocket *input_1 = static_cast<bNodeSocket *>(BLI_findlink(&math_node->inputs, 0));
      bNodeSocket *input_2 = static_cast<bNodeSocket *>(BLI_findlink(&math_node->inputs, 1));
      bNodeSocket *output = static_cast<bNodeSocket *>(math_node->outputs.first);
      bNodeSocket *alpha_sock = input_1;
      bNodeSocket *threshold_sock = input_2;

      blender::bke::node_add_link(ntree, from_node, from_socket, math_node, alpha_sock);
      blender::bke::node_add_link(ntree, math_node, output, to_node, to_socket);

      *version_cycles_node_socket_float_value(threshold_sock) = alpha.is_transparency ?
                                                                    1.0f - threshold :
                                                                    threshold;
    }
    else {
      /* Modify alpha value directly. */
      if (alpha.socket->type == SOCK_RGBA) {
        float *default_value = version_cycles_node_socket_rgba_value(alpha.socket);
        float sum = default_value[0] + default_value[1] + default_value[2];
        /* Don't do the division if possible to avoid float imprecision. */
        float avg = (sum >= 3.0f) ? 1.0f : (sum / 3.0f);
        float value = float((alpha.is_transparency) ? (avg > 1.0f - threshold) :
                                                      (avg > threshold));
        float values[4] = {value, value, value, 1.0f};
        copy_v4_v4(default_value, values);
      }
      else {
        float *default_value = version_cycles_node_socket_float_value(alpha.socket);
        *default_value = float((alpha.is_transparency) ? (*default_value > 1.0f - threshold) :
                                                         (*default_value > threshold));
      }
    }
  }
  return true;
}

static void versioning_replace_splitviewer(bNodeTree *ntree)
{
  /* Split viewer was replaced with a regular split node, so add a viewer node,
   * and link it to the new split node to achieve the same behavior of the split viewer node. */

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != CMP_NODE_SPLITVIEWER__DEPRECATED) {
      continue;
    }

    STRNCPY(node->idname, "CompositorNodeSplit");
    node->type_legacy = CMP_NODE_SPLIT;
    MEM_freeN(node->storage);
    node->storage = nullptr;

    bNode *viewer_node = blender::bke::node_add_static_node(nullptr, ntree, CMP_NODE_VIEWER);
    /* Nodes are created stacked on top of each other, so separate them a bit. */
    viewer_node->locx_legacy = node->locx_legacy + node->width + viewer_node->width / 4.0f;
    viewer_node->locy_legacy = node->locy_legacy;
    viewer_node->flag &= ~NODE_PREVIEW;

    bNodeSocket *split_out_socket = blender::bke::node_add_static_socket(
        ntree, node, SOCK_OUT, SOCK_IMAGE, PROP_NONE, "Image", "Image");
    bNodeSocket *viewer_in_socket = blender::bke::node_find_socket(viewer_node, SOCK_IN, "Image");

    blender::bke::node_add_link(ntree, node, split_out_socket, viewer_node, viewer_in_socket);
  }
}

/**
 * Exit NLA tweakmode when the AnimData struct has insufficient information.
 *
 * When NLA tweakmode is enabled, Blender expects certain pointers to be set up
 * correctly, and if that fails, can crash. This function ensures that
 * everything is consistent, by exiting tweakmode everywhere there's missing
 * pointers.
 *
 * This shouldn't happen, but the example blend file attached to #119615 needs
 * this.
 */
static void version_nla_tweakmode_incomplete(Main *bmain)
{
  bool any_valid_tweakmode_left = false;

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    AnimData *adt = BKE_animdata_from_id(id);
    if (!adt || !(adt->flag & ADT_NLA_EDIT_ON)) {
      continue;
    }

    if (adt->act_track && adt->actstrip) {
      /* Expected case. */
      any_valid_tweakmode_left = true;
      continue;
    }

    /* Not enough info in the blend file to reliably stay in tweak mode. This is the most important
     * part of this versioning code, as it prevents future nullptr access. */
    BKE_nla_tweakmode_exit({*id, *adt});
  }
  FOREACH_MAIN_ID_END;

  if (any_valid_tweakmode_left) {
    /* There are still NLA strips correctly in tweak mode. */
    return;
  }

  /* Nothing is in a valid tweakmode, so just disable the corresponding flags on all scenes. */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    scene->flag &= ~SCE_NLA_EDIT_ON;
  }
}

static bool versioning_convert_strip_speed_factor(Strip *strip, void *user_data)
{
  const Scene *scene = static_cast<Scene *>(user_data);
  const float speed_factor = strip->speed_factor;

  if (speed_factor == 1.0f || !SEQ_retiming_is_allowed(strip) ||
      SEQ_retiming_keys_count(strip) > 0)
  {
    return true;
  }

  SEQ_retiming_data_ensure(strip);
  SeqRetimingKey *last_key = &SEQ_retiming_keys_get(strip)[1];

  last_key->strip_frame_index = (strip->len) / speed_factor;

  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    const int prev_length = strip->len - strip->startofs - strip->endofs;
    const float left_handle = SEQ_time_left_handle_frame_get(scene, strip);
    SEQ_time_right_handle_frame_set(scene, strip, left_handle + prev_length);
  }

  return true;
}

static bool versioning_clear_strip_unused_flag(Strip *strip, void * /*user_data*/)
{
  strip->flag &= ~(1 << 6);
  return true;
}

/* Adjust the values of the given FCurve key frames by applying the given function. The function is
 * expected to get and return a float representing the value of the key frame. The FCurve is
 * potentially changed to have the given property type, if not already the case. */
template<typename Function>
static void adjust_fcurve_key_frame_values(FCurve *fcurve,
                                           const PropertyType property_type,
                                           const Function &function)
{
  /* Adjust key frames. */
  if (fcurve->bezt) {
    for (int i = 0; i < fcurve->totvert; i++) {
      fcurve->bezt[i].vec[0][1] = function(fcurve->bezt[i].vec[0][1]);
      fcurve->bezt[i].vec[1][1] = function(fcurve->bezt[i].vec[1][1]);
      fcurve->bezt[i].vec[2][1] = function(fcurve->bezt[i].vec[2][1]);
    }
  }

  /* Adjust baked key frames. */
  if (fcurve->fpt) {
    for (int i = 0; i < fcurve->totvert; i++) {
      fcurve->fpt[i].vec[1] = function(fcurve->fpt[i].vec[1]);
    }
  }

  /* Setup the flags based on the property type. */
  fcurve->flag &= ~(FCURVE_INT_VALUES | FCURVE_DISCRETE_VALUES);
  switch (property_type) {
    case PROP_FLOAT:
      break;
    case PROP_INT:
      fcurve->flag |= FCURVE_INT_VALUES;
      break;
    default:
      fcurve->flag |= (FCURVE_DISCRETE_VALUES | FCURVE_INT_VALUES);
      break;
  }

  /* Recalculate the automatic handles of the FCurve after adjustments. */
  BKE_fcurve_handles_recalc(fcurve);
}

/* The Threshold, Mix, and Size properties of the node were converted into node inputs, and
 * two new outputs were added.
 *
 * A new Highlights output was added to expose the extracted highlights, this is not relevant for
 * versioning.
 *
 * A new Glare output was added to expose just the generated glare without the input image itself.
 * this relevant for versioning the Mix property as will be shown.
 *
 * The Threshold, Iterations, Fade, Color Modulation, Streaks, and Streaks Angle Offset properties
 * were converted into node inputs, maintaining its type and range, so we just transfer its value
 * as is.
 *
 * The Mix property was converted into a Strength input, but its range changed from [-1, 1] to [0,
 * 1]. For the [-1, 0] sub-range, -1 used to mean zero strength and 0 used to mean full strength,
 * so we can convert between the two ranges by negating the mix factor and subtracting it from 1.
 * The [0, 1] sub-range on the other hand was useless except for the value 1, because it linearly
 * interpolates between Image + Glare and Glare, so it essentially adds an attenuated version of
 * the input image to the glare. When it is 1, only the glare is returned. So we split that range
 * in half as a heuristic and for values in the range [0.5, 1], we just reconnect the output to the
 * newly added Glare output.
 *
 * The Size property was converted into a float node input, and its range was changed from [1, 9]
 * to [0, 1]. For Bloom, the [1, 9] range was related exponentially to the actual size of the
 * glare, that is, 9 meant the glare covers the entire image, 8 meant it covers half, 7 meant it
 * covers quarter and so on. The new range is linear and relative to the image size, that is, 1
 * means the entire image and 0 means nothing. So we can convert from the [1, 9] range to [0, 1]
 * range using the relation 2^(x-9).
 * For Fog Glow, the [1, 9] range was related to the absolute size of the Fog Glow kernel in
 * pixels, where it is 2^size pixels in size. There is no way to version this accurately, since the
 * new size is relative to the input image size, which is runtime information. But we can assume
 * the render size as a guess and compute the size relative to that. */
static void do_version_glare_node_options_to_inputs(const Scene *scene,
                                                    bNodeTree *node_tree,
                                                    bNode *node)
{
  NodeGlare *storage = static_cast<NodeGlare *>(node->storage);
  if (!storage) {
    return;
  }

  /* Get the newly added inputs. */
  bNodeSocket *threshold = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Highlights Threshold", "Threshold");
  bNodeSocket *strength = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Strength", "Strength");
  bNodeSocket *size = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Size", "Size");
  bNodeSocket *streaks = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_INT, PROP_NONE, "Streaks", "Streaks");
  bNodeSocket *streaks_angle = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Streaks Angle", "Streaks Angle");
  bNodeSocket *iterations = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_INT, PROP_NONE, "Iterations", "Iterations");
  bNodeSocket *fade = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Fade", "Fade");
  bNodeSocket *color_modulation = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Color Modulation", "Color Modulation");

  /* Function to remap the Mix property to the range of the new Strength input. See function
   * description. */
  auto mix_to_strength = [](const float mix) {
    return 1.0f - blender::math::clamp(-mix, 0.0f, 1.0f);
  };

  /* Find the render size to guess the Size value. The node tree might not belong to a scene, so we
   * just assume an arbitrary HDTV 1080p render size. */
  blender::int2 render_size;
  if (scene) {
    BKE_render_resolution(&scene->r, true, &render_size.x, &render_size.y);
  }
  else {
    render_size = blender::int2(1920, 1080);
  }

  /* Function to remap the Size property to its new range. See function description. */
  const int max_render_size = blender::math::reduce_max(render_size);
  auto size_to_linear = [&](const int size) {
    if (storage->type == CMP_NODE_GLARE_BLOOM) {
      return blender::math::pow(2.0f, float(size - 9));
    }
    return blender::math::min(1.0f, float((1 << size) + 1) / float(max_render_size));
  };

  /* Assign the inputs the values from the old deprecated properties. */
  threshold->default_value_typed<bNodeSocketValueFloat>()->value = storage->threshold;
  strength->default_value_typed<bNodeSocketValueFloat>()->value = mix_to_strength(storage->mix);
  size->default_value_typed<bNodeSocketValueFloat>()->value = size_to_linear(storage->size);
  streaks->default_value_typed<bNodeSocketValueInt>()->value = storage->streaks;
  streaks_angle->default_value_typed<bNodeSocketValueFloat>()->value = storage->angle_ofs;
  iterations->default_value_typed<bNodeSocketValueInt>()->value = storage->iter;
  fade->default_value_typed<bNodeSocketValueFloat>()->value = storage->fade;
  color_modulation->default_value_typed<bNodeSocketValueFloat>()->value = storage->colmod;

  /* Compute the RNA path of the node. */
  char escaped_node_name[sizeof(node->name) * 2 + 1];
  BLI_str_escape(escaped_node_name, node->name, sizeof(escaped_node_name));
  const std::string node_rna_path = fmt::format("nodes[\"{}\"]", escaped_node_name);

  BKE_fcurves_id_cb(&node_tree->id, [&](ID * /*id*/, FCurve *fcurve) {
    /* The FCurve does not belong to the node since its RNA path doesn't start with the node's RNA
     * path. */
    if (!blender::StringRef(fcurve->rna_path).startswith(node_rna_path)) {
      return;
    }

    /* Change the RNA path of the FCurve from the old properties to the new inputs, adjusting the
     * values of the FCurves frames when needed. */
    char *old_rna_path = fcurve->rna_path;
    if (BLI_str_endswith(fcurve->rna_path, "threshold")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "mix")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
      adjust_fcurve_key_frame_values(
          fcurve, PROP_FLOAT, [&](const float value) { return mix_to_strength(value); });
    }
    else if (BLI_str_endswith(fcurve->rna_path, "size")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
      adjust_fcurve_key_frame_values(
          fcurve, PROP_FLOAT, [&](const float value) { return size_to_linear(value); });
    }
    else if (BLI_str_endswith(fcurve->rna_path, "streaks")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "angle_offset")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "iterations")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[6].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "fade")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[7].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "color_modulation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[8].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });

  /* If the Mix factor is between [0.5, 1], then the user actually wants the Glare output, so
   * reconnect the output to the newly created Glare output. */
  if (storage->mix > 0.5f) {
    bNodeSocket *image_output = version_node_add_socket_if_not_exist(
        node_tree, node, SOCK_OUT, SOCK_RGBA, PROP_NONE, "Image", "Image");
    bNodeSocket *glare_output = version_node_add_socket_if_not_exist(
        node_tree, node, SOCK_OUT, SOCK_RGBA, PROP_NONE, "Glare", "Glare");

    LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
      if (link->fromsock != image_output) {
        continue;
      }

      /* Relink from the Image output to the Glare output. */
      blender::bke::node_add_link(node_tree, node, glare_output, link->tonode, link->tosock);
      blender::bke::node_remove_link(node_tree, link);
    }
  }
}

static void do_version_glare_node_options_to_inputs_recursive(
    const Scene *scene,
    bNodeTree *node_tree,
    blender::Set<bNodeTree *> &node_trees_already_versioned)
{
  if (node_trees_already_versioned.contains(node_tree)) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    if (node->type_legacy == CMP_NODE_GLARE) {
      do_version_glare_node_options_to_inputs(scene, node_tree, node);
    }
    else if (node->is_group()) {
      bNodeTree *child_tree = reinterpret_cast<bNodeTree *>(node->id);
      if (child_tree) {
        do_version_glare_node_options_to_inputs_recursive(
            scene, child_tree, node_trees_already_versioned);
      }
    }
  }

  node_trees_already_versioned.add_new(node_tree);
}

/* The bloom glare is now normalized by its chain length, see the compute_bloom_chain_length method
 * in the glare code. So we need to multiply the strength by the chain length to restore its
 * original value. Since the chain length depend on the input image size, which is runtime
 * information, we assume the render size as a guess. */
static void do_version_glare_node_bloom_strength(const Scene *scene,
                                                 bNodeTree *node_tree,
                                                 bNode *node)
{
  NodeGlare *storage = static_cast<NodeGlare *>(node->storage);
  if (!storage) {
    return;
  }

  if (storage->type != CMP_NODE_GLARE_BLOOM) {
    return;
  }

  /* See the get_quality_factor method in the glare code. */
  const int quality_factor = 1 << storage->quality;

  /* Find the render size to guess the Strength value. The node tree might not belong to a scene,
   * so we just assume an arbitrary HDTV 1080p render size. */
  blender::int2 render_size;
  if (scene) {
    BKE_render_resolution(&scene->r, true, &render_size.x, &render_size.y);
  }
  else {
    render_size = blender::int2(1920, 1080);
  }

  const blender::int2 highlights_size = render_size / quality_factor;

  bNodeSocket *size = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Size", "Size");
  const float size_value = size->default_value_typed<bNodeSocketValueFloat>()->value;

  /* See the compute_bloom_chain_length method in the glare code. */
  const int smaller_dimension = blender::math::reduce_min(highlights_size);
  const float scaled_dimension = smaller_dimension * size_value;
  const int chain_length = int(std::log2(blender::math::max(1.0f, scaled_dimension)));

  auto scale_strength = [chain_length](const float strength) { return strength * chain_length; };

  bNodeSocket *strength_input = version_node_add_socket_if_not_exist(
      node_tree, node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Strength", "Strength");
  strength_input->default_value_typed<bNodeSocketValueFloat>()->value = scale_strength(
      strength_input->default_value_typed<bNodeSocketValueFloat>()->value);

  /* Compute the RNA path of the strength input. */
  char escaped_node_name[sizeof(node->name) * 2 + 1];
  BLI_str_escape(escaped_node_name, node->name, sizeof(escaped_node_name));
  const std::string strength_rna_path = fmt::format("nodes[\"{}\"].inputs[4].default_value",
                                                    escaped_node_name);

  /* Scale F-Curve. */
  BKE_fcurves_id_cb(&node_tree->id, [&](ID * /*id*/, FCurve *fcurve) {
    if (strength_rna_path == fcurve->rna_path) {
      adjust_fcurve_key_frame_values(
          fcurve, PROP_FLOAT, [&](const float value) { return scale_strength(value); });
    }
  });
}

static void do_version_glare_node_bloom_strength_recursive(
    const Scene *scene,
    bNodeTree *node_tree,
    blender::Set<bNodeTree *> &node_trees_already_versioned)
{
  if (node_trees_already_versioned.contains(node_tree)) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    if (node->type_legacy == CMP_NODE_GLARE) {
      do_version_glare_node_bloom_strength(scene, node_tree, node);
    }
    else if (node->is_group()) {
      bNodeTree *child_tree = reinterpret_cast<bNodeTree *>(node->id);
      if (child_tree) {
        do_version_glare_node_bloom_strength_recursive(
            scene, child_tree, node_trees_already_versioned);
      }
    }
  }

  node_trees_already_versioned.add_new(node_tree);
}

/* Previously, color to float implicit conversion happened by taking the average, while now it uses
 * luminance coefficients. So we need to convert all implicit conversions manually by adding a
 * normal node to sum the color components then divide them by an appropriate factor. The normal
 * node compute negative the dot product with its output vector, which is normalized. So if we
 * supply a vector of (-1, -1, -1), we will get the dot product multiplied by 1 / sqrt(3) due to
 * normalization. So if we want the average, we need to multiply by the normalization factor, then
 * divide by 3. */
static void do_version_color_to_float_conversion(bNodeTree *node_tree)
{
  /* Stores a mapping between an output and the final link of the versioning node tree that was
   * added for it, in order to share the same versioning node tree with potentially multiple
   * outgoing links from that same output. */
  blender::Map<bNodeSocket *, bNodeLink *> color_to_float_links;
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (!(link->fromsock->type == SOCK_RGBA && link->tosock->type == SOCK_FLOAT)) {
      continue;
    }

    /* If that output was versioned before, just connect the existing link. */
    bNodeLink *existing_link = color_to_float_links.lookup_default(link->fromsock, nullptr);
    if (existing_link) {
      version_node_add_link(*node_tree,
                            *existing_link->fromnode,
                            *existing_link->fromsock,
                            *link->tonode,
                            *link->tosock);
      blender::bke::node_remove_link(node_tree, link);
      continue;
    }

    /* Add a hidden dot product node. */
    bNode *dot_product_node = blender::bke::node_add_static_node(
        nullptr, node_tree, CMP_NODE_NORMAL);
    dot_product_node->flag |= NODE_HIDDEN;
    dot_product_node->location[0] = link->fromnode->location[0] + link->fromnode->width + 10.0f;
    dot_product_node->location[1] = link->fromnode->location[1];

    /* Link the source socket to the dot product input. */
    bNodeSocket *dot_product_input = version_node_add_socket_if_not_exist(
        node_tree, dot_product_node, SOCK_IN, SOCK_VECTOR, PROP_NONE, "Normal", "Normal");
    version_node_add_link(
        *node_tree, *link->fromnode, *link->fromsock, *dot_product_node, *dot_product_input);

    /* Assign (-1, -1, -1) to the dot product output, which stores the second vector for the
     * dot product. Notice that negative sign, since the node actually returns negative the dot
     * product. */
    bNodeSocket *dot_product_normal_output = version_node_add_socket_if_not_exist(
        node_tree, dot_product_node, SOCK_OUT, SOCK_VECTOR, PROP_NONE, "Normal", "Normal");
    copy_v3_fl(dot_product_normal_output->default_value_typed<bNodeSocketValueVector>()->value,
               -1.0f);

    /* Add a hidden multiply node. */
    bNode *multiply_node = blender::bke::node_add_static_node(nullptr, node_tree, CMP_NODE_MATH);
    multiply_node->custom1 = NODE_MATH_MULTIPLY;
    multiply_node->flag |= NODE_HIDDEN;
    multiply_node->location[0] = dot_product_node->location[0] + dot_product_node->width + 10.0f;
    multiply_node->location[1] = dot_product_node->location[1];

    /* Link the dot product output with the first input of the multiply node. */
    bNodeSocket *dot_product_dot_output = version_node_add_socket_if_not_exist(
        node_tree, dot_product_node, SOCK_OUT, SOCK_FLOAT, PROP_NONE, "Dot", "Dot");
    bNodeSocket *multiply_input_a = static_cast<bNodeSocket *>(
        BLI_findlink(&multiply_node->inputs, 0));
    version_node_add_link(
        *node_tree, *dot_product_node, *dot_product_dot_output, *multiply_node, *multiply_input_a);

    /* Set the second input to  sqrt(3) / 3 as described in the function description. */
    bNodeSocket *multiply_input_b = static_cast<bNodeSocket *>(
        BLI_findlink(&multiply_node->inputs, 1));
    multiply_input_b->default_value_typed<bNodeSocketValueFloat>()->value =
        blender::math::numbers::sqrt3 / 3.0f;

    /* Link the multiply node output to the link target. */
    bNodeSocket *multiply_output = version_node_add_socket_if_not_exist(
        node_tree, multiply_node, SOCK_OUT, SOCK_FLOAT, PROP_NONE, "Value", "Value");
    bNodeLink *final_link = &version_node_add_link(
        *node_tree, *multiply_node, *multiply_output, *link->tonode, *link->tosock);

    /* Add the new link to the cache. */
    color_to_float_links.add_new(link->fromsock, final_link);

    /* Remove the old link. */
    blender::bke::node_remove_link(node_tree, link);
  }
}

static void do_version_bump_filter_width(bNodeTree *node_tree)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &node_tree->nodes) {
    if (node->type_legacy != SH_NODE_BUMP) {
      continue;
    }

    bNodeSocket *filter_width_input = blender::bke::node_find_socket(
        node, SOCK_IN, "Filter Width");
    if (filter_width_input) {
      *version_cycles_node_socket_float_value(filter_width_input) = 1.0f;
    }
  }
}

static void do_version_viewer_shortcut(bNodeTree *node_tree)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &node_tree->nodes) {
    if (node->type_legacy != CMP_NODE_VIEWER) {
      continue;
    }
    /* custom1 was previously used for Tile Order for the Tiled Compositor. */
    node->custom1 = NODE_VIEWER_SHORTCUT_NONE;
  }
}

static bool all_scenes_use(Main *bmain, const blender::Span<const char *> engines)
{
  if (!bmain->scenes.first) {
    return false;
  }

  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    bool match = false;
    for (const char *engine : engines) {
      if (STREQ(scene->r.engine, engine)) {
        match = true;
      }
    }
    if (!match) {
      return false;
    }
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

    /* XXX This was added several years ago in 'lib_link` code of Scene... Should be safe enough
     * here. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->nodetree) {
        version_composite_nodetree_null_id(scene->nodetree, scene);
      }
    }

    /* XXX This was added many years ago (1c19940198) in 'lib_link` code of particles as a bug-fix.
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
      Editing *ed = SEQ_editing_get(scene);
      if (ed != nullptr) {
        SEQ_for_each_callback(&ed->seqbase, versioning_convert_strip_speed_factor, scene);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 34)) {
    BKE_mesh_legacy_face_map_to_generic(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 23)) {
    version_nla_tweakmode_incomplete(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 15)) {
    /* Change drivers and animation on "armature.collections" to
     * ".collections_all", so that they are drawn correctly in the tree view,
     * and keep working when the collection is moved around in the hierarchy. */
    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      AnimData *adt = BKE_animdata_from_id(&arm->id);
      if (!adt) {
        continue;
      }

      LISTBASE_FOREACH (FCurve *, fcurve, &adt->drivers) {
        version_bonecollection_anim(fcurve);
      }
      if (adt->action) {
        LISTBASE_FOREACH (FCurve *, fcurve, &adt->action->curves) {
          version_bonecollection_anim(fcurve);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 23)) {
    /* Shift animation data to accommodate the new Roughness input. */
    version_node_socket_index_animdata(
        bmain, NTREE_SHADER, SH_NODE_SUBSURFACE_SCATTERING, 4, 1, 5);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 50)) {
    if (all_scenes_use(bmain, {RE_engine_id_BLENDER_EEVEE})) {
      LISTBASE_FOREACH (Object *, object, &bmain->objects) {
        versioning_eevee_shadow_settings(object);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 51)) {
    /* Convert blend method to math nodes. */
    if (all_scenes_use(bmain, {RE_engine_id_BLENDER_EEVEE})) {
      LISTBASE_FOREACH (Material *, material, &bmain->materials) {
        if (!material->use_nodes || material->nodetree == nullptr) {
          /* Nothing to version. */
        }
        else if (ELEM(material->blend_method, MA_BM_HASHED, MA_BM_BLEND)) {
          /* Compatible modes. Nothing to change. */
        }
        else if (material->blend_shadow == MA_BS_NONE) {
          /* No need to match the surface since shadows are disabled. */
        }
        else if (material->blend_shadow == MA_BS_SOLID) {
          /* This is already versioned an transferred to `transparent_shadows`. */
        }
        else if ((material->blend_shadow == MA_BS_CLIP && material->blend_method != MA_BM_CLIP) ||
                 (material->blend_shadow == MA_BS_HASHED))
        {
          BLO_reportf_wrap(
              fd->reports,
              RPT_WARNING,
              RPT_("Material %s could not be converted because of different Blend Mode "
                   "and Shadow Mode (need manual adjustment)\n"),
              material->id.name + 2);
        }
        else {
          /* TODO(fclem): Check if threshold is driven or has animation. Bail out if needed? */

          float threshold = (material->blend_method == MA_BM_CLIP) ? material->alpha_threshold :
                                                                     2.0f;

          if (!versioning_eevee_material_blend_mode_settings(material->nodetree, threshold)) {
            BLO_reportf_wrap(fd->reports,
                             RPT_WARNING,
                             RPT_("Material %s could not be converted because of non-trivial "
                                  "alpha blending (need manual adjustment)\n"),
                             material->id.name + 2);
          }
        }

        if (material->blend_shadow == MA_BS_NONE) {
          versioning_eevee_material_shadow_none(material);
        }
        /* Set blend_mode & blend_shadow for forward compatibility. */
        material->blend_method = (material->blend_method != MA_BM_BLEND) ? MA_BM_HASHED :
                                                                           MA_BM_BLEND;
        material->blend_shadow = (material->blend_shadow == MA_BS_SOLID) ? MA_BS_SOLID :
                                                                           MA_BS_HASHED;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 52)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (STREQ(scene->r.engine, RE_engine_id_BLENDER_EEVEE)) {
        STRNCPY(scene->r.engine, RE_engine_id_BLENDER_EEVEE_NEXT);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 6)) {
    /* Shift animation data to accommodate the new Diffuse Roughness input. */
    version_node_socket_index_animdata(bmain, NTREE_SHADER, SH_NODE_BSDF_PRINCIPLED, 7, 1, 30);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 2)) {
    blender::animrig::versioning::convert_legacy_animato_actions(*bmain);
    blender::animrig::versioning::tag_action_users_for_slotted_actions_conversion(*bmain);
    blender::animrig::versioning::convert_legacy_action_assignments(*bmain, fd->reports->reports);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 7)) {
    constexpr char SCE_SNAP_TO_NODE_X = (1 << 0);
    constexpr char SCE_SNAP_TO_NODE_Y = (1 << 1);
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->toolsettings->snap_node_mode & SCE_SNAP_TO_NODE_X ||
          scene->toolsettings->snap_node_mode & SCE_SNAP_TO_NODE_Y)
      {
        scene->toolsettings->snap_node_mode = SCE_SNAP_TO_GRID;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 18)) {
    blender::Set<bNodeTree *> node_trees_already_versioned;
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      bNodeTree *node_tree = scene->nodetree;
      if (!node_tree) {
        continue;
      }
      do_version_glare_node_options_to_inputs_recursive(
          scene, node_tree, node_trees_already_versioned);
    }

    /* The above loop versioned all node trees used in a scene, but other node trees might exist
     * that are not used in a scene. For those, assume the first scene in the file, as this is
     * better than not doing versioning at all. */
    Scene *scene = static_cast<Scene *>(bmain->scenes.first);
    LISTBASE_FOREACH (bNodeTree *, node_tree, &bmain->nodetrees) {
      if (node_trees_already_versioned.contains(node_tree)) {
        continue;
      }

      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_GLARE) {
          do_version_glare_node_options_to_inputs(scene, node_tree, node);
        }
      }
      node_trees_already_versioned.add_new(node_tree);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 19)) {
    /* Two new inputs were added, Saturation and Tint. */
    version_node_socket_index_animdata(bmain, NTREE_COMPOSIT, CMP_NODE_GLARE, 3, 2, 11);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 20)) {
    /* Two new inputs were added, Highlights Smoothness and Highlights suppression. */
    version_node_socket_index_animdata(bmain, NTREE_COMPOSIT, CMP_NODE_GLARE, 2, 2, 13);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 21)) {
    blender::Set<bNodeTree *> node_trees_already_versioned;
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      bNodeTree *node_tree = scene->nodetree;
      if (!node_tree) {
        continue;
      }
      do_version_glare_node_bloom_strength_recursive(
          scene, node_tree, node_trees_already_versioned);
    }

    /* The above loop versioned all node trees used in a scene, but other node trees might exist
     * that are not used in a scene. For those, assume the first scene in the file, as this is
     * better than not doing versioning at all. */
    Scene *scene = static_cast<Scene *>(bmain->scenes.first);
    LISTBASE_FOREACH (bNodeTree *, node_tree, &bmain->nodetrees) {
      if (node_trees_already_versioned.contains(node_tree)) {
        continue;
      }

      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_GLARE) {
          do_version_glare_node_bloom_strength(scene, node_tree, node);
        }
      }
      node_trees_already_versioned.add_new(node_tree);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 25)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (!scene->adt) {
        continue;
      }
      using namespace blender;
      auto replace_rna_path_prefix =
          [](FCurve &fcurve, const StringRef old_prefix, const StringRef new_prefix) {
            const StringRef rna_path = fcurve.rna_path;
            if (!rna_path.startswith(old_prefix)) {
              return;
            }
            const StringRef tail = rna_path.drop_prefix(old_prefix.size());
            char *new_rna_path = BLI_strdupcat(new_prefix.data(), tail.data());
            MEM_freeN(fcurve.rna_path);
            fcurve.rna_path = new_rna_path;
          };
      if (scene->adt->action) {
        animrig::foreach_fcurve_in_action(scene->adt->action->wrap(), [&](FCurve &fcurve) {
          replace_rna_path_prefix(fcurve, "sequence_editor.sequences", "sequence_editor.strips");
        });
      }
      LISTBASE_FOREACH (FCurve *, driver, &scene->adt->drivers) {
        replace_rna_path_prefix(*driver, "sequence_editor.sequences", "sequence_editor.strips");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 27)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_version_color_to_float_conversion(ntree);
      }
      else if (ntree->type == NTREE_SHADER) {
        do_version_bump_filter_width(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  /* For each F-Curve, set the F-Curve flags based on the property type it animates. This is to
   * correct F-Curves created while the bug (#136347) was in active use. Since this bug did not
   * appear before 4.4, and this versioning code has a bit of a performance impact (going over all
   * F-Curves of all Actions, and resolving them all to their RNA properties), it will be skipped
   * if the blend file is old enough to not be affected. */
  if (MAIN_VERSION_FILE_ATLEAST(bmain, 404, 0) && !MAIN_VERSION_FILE_ATLEAST(bmain, 404, 31)) {
    LISTBASE_FOREACH (bAction *, dna_action, &bmain->actions) {
      blender::animrig::Action &action = dna_action->wrap();
      for (const blender::animrig::Slot *slot : action.slots()) {
        blender::Span<ID *> slot_users = slot->users(*bmain);
        if (slot_users.is_empty()) {
          /* If nothing is using this slot, the RNA paths cannot be resolved, and so there
           * is no way to find the animated property type. */
          continue;
        }
        blender::animrig::foreach_fcurve_in_action_slot(action, slot->handle, [&](FCurve &fcurve) {
          /* Loop over all slot users, because when the slot is shared, not all F-Curves may
           * resolve on all users. For example, a custom property might only exist on a subset of
           * the users.*/
          for (ID *slot_user : slot_users) {
            PointerRNA slot_user_ptr = RNA_id_pointer_create(slot_user);
            PointerRNA ptr;
            PropertyRNA *prop;
            if (!RNA_path_resolve_property(&slot_user_ptr, fcurve.rna_path, &ptr, &prop)) {
              continue;
            }

            blender::animrig::update_autoflags_fcurve_direct(&fcurve, RNA_property_type(prop));
            break;
          }
        });
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 32)) {
    LISTBASE_FOREACH (bAction *, dna_action, &bmain->actions) {
      blender::animrig::Action &action = dna_action->wrap();
      blender::animrig::foreach_fcurve_in_action(
          action, [&](FCurve &fcurve) { version_fix_fcurve_noise_offset(fcurve); });
    }
    BKE_animdata_main_cb(bmain, [](ID * /* id */, AnimData *adt) {
      LISTBASE_FOREACH (FCurve *, fcurve, &adt->drivers) {
        version_fix_fcurve_noise_offset(*fcurve);
      }

      LISTBASE_FOREACH (NlaTrack *, track, &adt->nla_tracks) {
        nlastrips_apply_fcurve_versioning(track->strips);
      }
    });
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
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
                     "GeometryNodeInputNamedAttribute"))
        {
          bNodeSocket *socket = blender::bke::node_find_socket(node, SOCK_IN, "Name");
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
    if (node->type_legacy == SH_NODE_BSDF_GLOSSY_LEGACY) {
      STRNCPY(node->idname, "ShaderNodeBsdfAnisotropic");
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
        blender::bke::node_remove_link(ntree, socket->link);
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
    if (link->fromnode->type_legacy == SH_NODE_TEX_COORD &&
        STREQ(link->fromsock->identifier, "Normal"))
    {
      if (geometry_node == nullptr) {
        geometry_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_NEW_GEOMETRY);
        incoming_socket = blender::bke::node_find_socket(geometry_node, SOCK_OUT, "Incoming");

        transform_node = blender::bke::node_add_static_node(
            nullptr, ntree, SH_NODE_VECT_TRANSFORM);
        vec_in_socket = blender::bke::node_find_socket(transform_node, SOCK_IN, "Vector");
        vec_out_socket = blender::bke::node_find_socket(transform_node, SOCK_OUT, "Vector");

        NodeShaderVectTransform *nodeprop = (NodeShaderVectTransform *)transform_node->storage;
        nodeprop->type = SHD_VECT_TRANSFORM_TYPE_NORMAL;

        blender::bke::node_add_link(
            ntree, geometry_node, incoming_socket, transform_node, vec_in_socket);
      }
      blender::bke::node_add_link(
          ntree, transform_node, vec_out_socket, link->tonode, link->tosock);
      blender::bke::node_remove_link(ntree, link);
    }
  }
}

static void version_principled_transmission_roughness(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }
    bNodeSocket *sock = blender::bke::node_find_socket(node, SOCK_IN, "Transmission Roughness");
    if (sock != nullptr) {
      blender::bke::node_remove_socket(ntree, node, sock);
    }
  }
}

/* Convert legacy Velvet BSDF nodes into the new Sheen BSDF node. */
static void version_replace_velvet_sheen_node(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_BSDF_SHEEN) {
      STRNCPY(node->idname, "ShaderNodeBsdfSheen");

      bNodeSocket *sigmaInput = blender::bke::node_find_socket(node, SOCK_IN, "Sigma");
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
    return (node->type_legacy == SH_NODE_BSDF_PRINCIPLED) &&
           (blender::bke::node_find_socket(node, SOCK_IN, "Sheen Roughness") == nullptr);
  };
  auto update_input = [ntree](bNode *node, bNodeSocket *input) {
    /* Change socket type to Color. */
    blender::bke::node_modify_socket_type_static(ntree, node, input, SOCK_RGBA, 0);

    /* Account for the change in intensity between the old and new model.
     * If the Sheen input is set to a fixed value, adjust it and set the tint to white.
     * Otherwise, if it's connected, keep it as-is but set the tint to 0.2 instead. */
    bNodeSocket *sheen = blender::bke::node_find_socket(node, SOCK_IN, "Sheen");
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

/* Convert EEVEE-Legacy refraction depth to EEVEE-Next thickness tree. */
static void version_refraction_depth_to_thickness_value(bNodeTree *ntree, float thickness)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_OUTPUT_MATERIAL) {
      continue;
    }

    bNodeSocket *thickness_socket = blender::bke::node_find_socket(node, SOCK_IN, "Thickness");
    if (thickness_socket == nullptr) {
      continue;
    }

    bool has_link = false;
    LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
      if (link->tosock == thickness_socket) {
        /* Something is already plugged in. Don't modify anything. */
        has_link = true;
      }
    }

    if (has_link) {
      continue;
    }
    bNode *value_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_VALUE);
    value_node->parent = node->parent;
    value_node->locx_legacy = node->locx_legacy;
    value_node->locy_legacy = node->locy_legacy - 160.0f;
    bNodeSocket *socket_value = blender::bke::node_find_socket(value_node, SOCK_OUT, "Value");

    *version_cycles_node_socket_float_value(socket_value) = thickness;

    blender::bke::node_add_link(ntree, value_node, socket_value, node, thickness_socket);
  }

  version_socket_update_is_used(ntree);
}

static void versioning_update_noise_texture_node(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_TEX_NOISE) {
      continue;
    }

    (static_cast<NodeTexNoise *>(node->storage))->type = SHD_NOISE_FBM;

    bNodeSocket *roughness_socket = blender::bke::node_find_socket(node, SOCK_IN, "Roughness");
    if (roughness_socket == nullptr) {
      /* Noise Texture node was created before the Roughness input was added. */
      continue;
    }

    float *roughness = version_cycles_node_socket_float_value(roughness_socket);

    bNodeLink *roughness_link = nullptr;
    bNode *roughness_from_node = nullptr;
    bNodeSocket *roughness_from_socket = nullptr;

    LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
      /* Find links, nodes and sockets. */
      if (link->tosock == roughness_socket) {
        roughness_link = link;
        roughness_from_node = link->fromnode;
        roughness_from_socket = link->fromsock;
      }
    }

    if (roughness_link != nullptr) {
      /* Add Clamp node before Roughness input. */

      bNode *clamp_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_CLAMP);
      clamp_node->parent = node->parent;
      clamp_node->custom1 = NODE_CLAMP_MINMAX;
      clamp_node->locx_legacy = node->locx_legacy;
      clamp_node->locy_legacy = node->locy_legacy - 300.0f;
      clamp_node->flag |= NODE_HIDDEN;
      bNodeSocket *clamp_socket_value = blender::bke::node_find_socket(
          clamp_node, SOCK_IN, "Value");
      bNodeSocket *clamp_socket_min = blender::bke::node_find_socket(clamp_node, SOCK_IN, "Min");
      bNodeSocket *clamp_socket_max = blender::bke::node_find_socket(clamp_node, SOCK_IN, "Max");
      bNodeSocket *clamp_socket_out = blender::bke::node_find_socket(
          clamp_node, SOCK_OUT, "Result");

      *version_cycles_node_socket_float_value(clamp_socket_min) = 0.0f;
      *version_cycles_node_socket_float_value(clamp_socket_max) = 1.0f;

      blender::bke::node_remove_link(ntree, roughness_link);
      blender::bke::node_add_link(
          ntree, roughness_from_node, roughness_from_socket, clamp_node, clamp_socket_value);
      blender::bke::node_add_link(ntree, clamp_node, clamp_socket_out, node, roughness_socket);
    }
    else {
      *roughness = std::clamp(*roughness, 0.0f, 1.0f);
    }
  }

  version_socket_update_is_used(ntree);
}

static void versioning_replace_musgrave_texture_node(bNodeTree *ntree)
{
  version_node_input_socket_name(ntree, SH_NODE_TEX_MUSGRAVE_DEPRECATED, "Dimension", "Roughness");
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_TEX_MUSGRAVE_DEPRECATED) {
      continue;
    }

    STRNCPY(node->idname, "ShaderNodeTexNoise");
    node->type_legacy = SH_NODE_TEX_NOISE;
    NodeTexNoise *data = MEM_cnew<NodeTexNoise>(__func__);
    data->base = (static_cast<NodeTexMusgrave *>(node->storage))->base;
    data->dimensions = (static_cast<NodeTexMusgrave *>(node->storage))->dimensions;
    data->normalize = false;
    data->type = (static_cast<NodeTexMusgrave *>(node->storage))->musgrave_type;
    MEM_freeN(node->storage);
    node->storage = data;

    bNodeLink *detail_link = nullptr;
    bNode *detail_from_node = nullptr;
    bNodeSocket *detail_from_socket = nullptr;

    bNodeLink *roughness_link = nullptr;
    bNode *roughness_from_node = nullptr;
    bNodeSocket *roughness_from_socket = nullptr;

    bNodeLink *lacunarity_link = nullptr;
    bNode *lacunarity_from_node = nullptr;
    bNodeSocket *lacunarity_from_socket = nullptr;

    LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
      /* Find links, nodes and sockets. */
      if (link->tonode == node) {
        if (STREQ(link->tosock->identifier, "Detail")) {
          detail_link = link;
          detail_from_node = link->fromnode;
          detail_from_socket = link->fromsock;
        }
        if (STREQ(link->tosock->identifier, "Roughness")) {
          roughness_link = link;
          roughness_from_node = link->fromnode;
          roughness_from_socket = link->fromsock;
        }
        if (STREQ(link->tosock->identifier, "Lacunarity")) {
          lacunarity_link = link;
          lacunarity_from_node = link->fromnode;
          lacunarity_from_socket = link->fromsock;
        }
      }
    }

    uint8_t noise_type = (static_cast<NodeTexNoise *>(node->storage))->type;
    float locy_offset = 0.0f;

    bNodeSocket *fac_socket = blender::bke::node_find_socket(node, SOCK_OUT, "Fac");
    /* Clear label because Musgrave output socket label is set to "Height" instead of "Fac". */
    fac_socket->label[0] = '\0';

    bNodeSocket *detail_socket = blender::bke::node_find_socket(node, SOCK_IN, "Detail");
    float *detail = version_cycles_node_socket_float_value(detail_socket);

    if (detail_link != nullptr) {
      locy_offset -= 80.0f;

      /* Add Minimum Math node and Subtract Math node before Detail input. */

      bNode *min_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
      min_node->parent = node->parent;
      min_node->custom1 = NODE_MATH_MINIMUM;
      min_node->locx_legacy = node->locx_legacy;
      min_node->locy_legacy = node->locy_legacy - 320.0f;
      min_node->flag |= NODE_HIDDEN;
      bNodeSocket *min_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&min_node->inputs, 0));
      bNodeSocket *min_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&min_node->inputs, 1));
      bNodeSocket *min_socket_out = blender::bke::node_find_socket(min_node, SOCK_OUT, "Value");

      bNode *sub1_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
      sub1_node->parent = node->parent;
      sub1_node->custom1 = NODE_MATH_SUBTRACT;
      sub1_node->locx_legacy = node->locx_legacy;
      sub1_node->locy_legacy = node->locy_legacy - 360.0f;
      sub1_node->flag |= NODE_HIDDEN;
      bNodeSocket *sub1_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&sub1_node->inputs, 0));
      bNodeSocket *sub1_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&sub1_node->inputs, 1));
      bNodeSocket *sub1_socket_out = blender::bke::node_find_socket(sub1_node, SOCK_OUT, "Value");

      *version_cycles_node_socket_float_value(min_socket_B) = 14.0f;
      *version_cycles_node_socket_float_value(sub1_socket_B) = 1.0f;

      blender::bke::node_remove_link(ntree, detail_link);
      blender::bke::node_add_link(
          ntree, detail_from_node, detail_from_socket, sub1_node, sub1_socket_A);
      blender::bke::node_add_link(ntree, sub1_node, sub1_socket_out, min_node, min_socket_A);
      blender::bke::node_add_link(ntree, min_node, min_socket_out, node, detail_socket);

      if (ELEM(noise_type, SHD_NOISE_RIDGED_MULTIFRACTAL, SHD_NOISE_HETERO_TERRAIN)) {
        locy_offset -= 40.0f;

        /* Add Greater Than Math node before Subtract Math node. */

        bNode *greater_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
        greater_node->parent = node->parent;
        greater_node->custom1 = NODE_MATH_GREATER_THAN;
        greater_node->locx_legacy = node->locx_legacy;
        greater_node->locy_legacy = node->locy_legacy - 400.0f;
        greater_node->flag |= NODE_HIDDEN;
        bNodeSocket *greater_socket_A = static_cast<bNodeSocket *>(
            BLI_findlink(&greater_node->inputs, 0));
        bNodeSocket *greater_socket_B = static_cast<bNodeSocket *>(
            BLI_findlink(&greater_node->inputs, 1));
        bNodeSocket *greater_socket_out = blender::bke::node_find_socket(
            greater_node, SOCK_OUT, "Value");

        *version_cycles_node_socket_float_value(greater_socket_B) = 1.0f;

        blender::bke::node_add_link(
            ntree, detail_from_node, detail_from_socket, greater_node, greater_socket_A);
        blender::bke::node_add_link(
            ntree, greater_node, greater_socket_out, sub1_node, sub1_socket_B);
      }
      else {
        /* Add Clamp node and Multiply Math node behind Fac output. */

        bNode *clamp_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_CLAMP);
        clamp_node->parent = node->parent;
        clamp_node->custom1 = NODE_CLAMP_MINMAX;
        clamp_node->locx_legacy = node->locx_legacy;
        clamp_node->locy_legacy = node->locy_legacy + 40.0f;
        clamp_node->flag |= NODE_HIDDEN;
        bNodeSocket *clamp_socket_value = blender::bke::node_find_socket(
            clamp_node, SOCK_IN, "Value");
        bNodeSocket *clamp_socket_min = blender::bke::node_find_socket(clamp_node, SOCK_IN, "Min");
        bNodeSocket *clamp_socket_max = blender::bke::node_find_socket(clamp_node, SOCK_IN, "Max");
        bNodeSocket *clamp_socket_out = blender::bke::node_find_socket(
            clamp_node, SOCK_OUT, "Result");

        bNode *mul_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
        mul_node->parent = node->parent;
        mul_node->custom1 = NODE_MATH_MULTIPLY;
        mul_node->locx_legacy = node->locx_legacy;
        mul_node->locy_legacy = node->locy_legacy + 80.0f;
        mul_node->flag |= NODE_HIDDEN;
        bNodeSocket *mul_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 0));
        bNodeSocket *mul_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 1));
        bNodeSocket *mul_socket_out = blender::bke::node_find_socket(mul_node, SOCK_OUT, "Value");

        *version_cycles_node_socket_float_value(clamp_socket_min) = 0.0f;
        *version_cycles_node_socket_float_value(clamp_socket_max) = 1.0f;

        if (noise_type == SHD_NOISE_MULTIFRACTAL) {
          /* Add Subtract Math node and Add Math node after Multiply Math node. */

          bNode *sub2_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
          sub2_node->parent = node->parent;
          sub2_node->custom1 = NODE_MATH_SUBTRACT;
          sub2_node->custom2 = SHD_MATH_CLAMP;
          sub2_node->locx_legacy = node->locx_legacy;
          sub2_node->locy_legacy = node->locy_legacy + 120.0f;
          sub2_node->flag |= NODE_HIDDEN;
          bNodeSocket *sub2_socket_A = static_cast<bNodeSocket *>(
              BLI_findlink(&sub2_node->inputs, 0));
          bNodeSocket *sub2_socket_B = static_cast<bNodeSocket *>(
              BLI_findlink(&sub2_node->inputs, 1));
          bNodeSocket *sub2_socket_out = blender::bke::node_find_socket(
              sub2_node, SOCK_OUT, "Value");

          bNode *add_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
          add_node->parent = node->parent;
          add_node->custom1 = NODE_MATH_ADD;
          add_node->locx_legacy = node->locx_legacy;
          add_node->locy_legacy = node->locy_legacy + 160.0f;
          add_node->flag |= NODE_HIDDEN;
          bNodeSocket *add_socket_A = static_cast<bNodeSocket *>(
              BLI_findlink(&add_node->inputs, 0));
          bNodeSocket *add_socket_B = static_cast<bNodeSocket *>(
              BLI_findlink(&add_node->inputs, 1));
          bNodeSocket *add_socket_out = blender::bke::node_find_socket(
              add_node, SOCK_OUT, "Value");

          *version_cycles_node_socket_float_value(sub2_socket_A) = 1.0f;

          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == fac_socket) {
              blender::bke::node_add_link(
                  ntree, add_node, add_socket_out, link->tonode, link->tosock);
              blender::bke::node_remove_link(ntree, link);
            }
          }

          blender::bke::node_add_link(ntree, mul_node, mul_socket_out, add_node, add_socket_A);
          blender::bke::node_add_link(
              ntree, detail_from_node, detail_from_socket, sub2_node, sub2_socket_B);
          blender::bke::node_add_link(ntree, sub2_node, sub2_socket_out, add_node, add_socket_B);
        }
        else {
          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == fac_socket) {
              blender::bke::node_add_link(
                  ntree, mul_node, mul_socket_out, link->tonode, link->tosock);
              blender::bke::node_remove_link(ntree, link);
            }
          }
        }

        blender::bke::node_add_link(ntree, node, fac_socket, mul_node, mul_socket_A);
        blender::bke::node_add_link(
            ntree, detail_from_node, detail_from_socket, clamp_node, clamp_socket_value);
        blender::bke::node_add_link(ntree, clamp_node, clamp_socket_out, mul_node, mul_socket_B);
      }
    }
    else {
      if (*detail < 1.0f) {
        if (!ELEM(noise_type, SHD_NOISE_RIDGED_MULTIFRACTAL, SHD_NOISE_HETERO_TERRAIN)) {
          /* Add Multiply Math node behind Fac output. */

          bNode *mul_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
          mul_node->parent = node->parent;
          mul_node->custom1 = NODE_MATH_MULTIPLY;
          mul_node->locx_legacy = node->locx_legacy;
          mul_node->locy_legacy = node->locy_legacy + 40.0f;
          mul_node->flag |= NODE_HIDDEN;
          bNodeSocket *mul_socket_A = static_cast<bNodeSocket *>(
              BLI_findlink(&mul_node->inputs, 0));
          bNodeSocket *mul_socket_B = static_cast<bNodeSocket *>(
              BLI_findlink(&mul_node->inputs, 1));
          bNodeSocket *mul_socket_out = blender::bke::node_find_socket(
              mul_node, SOCK_OUT, "Value");

          *version_cycles_node_socket_float_value(mul_socket_B) = *detail;

          if (noise_type == SHD_NOISE_MULTIFRACTAL) {
            /* Add an Add Math node after Multiply Math node. */

            bNode *add_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
            add_node->parent = node->parent;
            add_node->custom1 = NODE_MATH_ADD;
            add_node->locx_legacy = node->locx_legacy;
            add_node->locy_legacy = node->locy_legacy + 80.0f;
            add_node->flag |= NODE_HIDDEN;
            bNodeSocket *add_socket_A = static_cast<bNodeSocket *>(
                BLI_findlink(&add_node->inputs, 0));
            bNodeSocket *add_socket_B = static_cast<bNodeSocket *>(
                BLI_findlink(&add_node->inputs, 1));
            bNodeSocket *add_socket_out = blender::bke::node_find_socket(
                add_node, SOCK_OUT, "Value");

            *version_cycles_node_socket_float_value(add_socket_B) = 1.0f - *detail;

            LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
              if (link->fromsock == fac_socket) {
                blender::bke::node_add_link(
                    ntree, add_node, add_socket_out, link->tonode, link->tosock);
                blender::bke::node_remove_link(ntree, link);
              }
            }

            blender::bke::node_add_link(ntree, mul_node, mul_socket_out, add_node, add_socket_A);
          }
          else {
            LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
              if (link->fromsock == fac_socket) {
                blender::bke::node_add_link(
                    ntree, mul_node, mul_socket_out, link->tonode, link->tosock);
                blender::bke::node_remove_link(ntree, link);
              }
            }
          }

          blender::bke::node_add_link(ntree, node, fac_socket, mul_node, mul_socket_A);

          *detail = 0.0f;
        }
      }
      else {
        *detail = std::fminf(*detail - 1.0f, 14.0f);
      }
    }

    bNodeSocket *roughness_socket = blender::bke::node_find_socket(node, SOCK_IN, "Roughness");
    float *roughness = version_cycles_node_socket_float_value(roughness_socket);
    bNodeSocket *lacunarity_socket = blender::bke::node_find_socket(node, SOCK_IN, "Lacunarity");
    float *lacunarity = version_cycles_node_socket_float_value(lacunarity_socket);

    *roughness = std::fmaxf(*roughness, 1e-5f);
    *lacunarity = std::fmaxf(*lacunarity, 1e-5f);

    if (roughness_link != nullptr) {
      /* Add Maximum Math node after output of roughness_from_node. Add Multiply Math node and
       * Power Math node before Roughness input. */

      bNode *max1_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
      max1_node->parent = node->parent;
      max1_node->custom1 = NODE_MATH_MAXIMUM;
      max1_node->locx_legacy = node->locx_legacy;
      max1_node->locy_legacy = node->locy_legacy - 400.0f + locy_offset;
      max1_node->flag |= NODE_HIDDEN;
      bNodeSocket *max1_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&max1_node->inputs, 0));
      bNodeSocket *max1_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&max1_node->inputs, 1));
      bNodeSocket *max1_socket_out = blender::bke::node_find_socket(max1_node, SOCK_OUT, "Value");

      bNode *mul_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
      mul_node->parent = node->parent;
      mul_node->custom1 = NODE_MATH_MULTIPLY;
      mul_node->locx_legacy = node->locx_legacy;
      mul_node->locy_legacy = node->locy_legacy - 360.0f + locy_offset;
      mul_node->flag |= NODE_HIDDEN;
      bNodeSocket *mul_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 0));
      bNodeSocket *mul_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 1));
      bNodeSocket *mul_socket_out = blender::bke::node_find_socket(mul_node, SOCK_OUT, "Value");

      bNode *pow_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
      pow_node->parent = node->parent;
      pow_node->custom1 = NODE_MATH_POWER;
      pow_node->locx_legacy = node->locx_legacy;
      pow_node->locy_legacy = node->locy_legacy - 320.0f + locy_offset;
      pow_node->flag |= NODE_HIDDEN;
      bNodeSocket *pow_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&pow_node->inputs, 0));
      bNodeSocket *pow_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&pow_node->inputs, 1));
      bNodeSocket *pow_socket_out = blender::bke::node_find_socket(pow_node, SOCK_OUT, "Value");

      *version_cycles_node_socket_float_value(max1_socket_B) = -1e-5f;
      *version_cycles_node_socket_float_value(mul_socket_B) = -1.0f;
      *version_cycles_node_socket_float_value(pow_socket_A) = *lacunarity;

      blender::bke::node_remove_link(ntree, roughness_link);
      blender::bke::node_add_link(
          ntree, roughness_from_node, roughness_from_socket, max1_node, max1_socket_A);
      blender::bke::node_add_link(ntree, max1_node, max1_socket_out, mul_node, mul_socket_A);
      blender::bke::node_add_link(ntree, mul_node, mul_socket_out, pow_node, pow_socket_B);
      blender::bke::node_add_link(ntree, pow_node, pow_socket_out, node, roughness_socket);

      if (lacunarity_link != nullptr) {
        /* Add Maximum Math node after output of lacunarity_from_node. */

        bNode *max2_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
        max2_node->parent = node->parent;
        max2_node->custom1 = NODE_MATH_MAXIMUM;
        max2_node->locx_legacy = node->locx_legacy;
        max2_node->locy_legacy = node->locy_legacy - 440.0f + locy_offset;
        max2_node->flag |= NODE_HIDDEN;
        bNodeSocket *max2_socket_A = static_cast<bNodeSocket *>(
            BLI_findlink(&max2_node->inputs, 0));
        bNodeSocket *max2_socket_B = static_cast<bNodeSocket *>(
            BLI_findlink(&max2_node->inputs, 1));
        bNodeSocket *max2_socket_out = blender::bke::node_find_socket(
            max2_node, SOCK_OUT, "Value");

        *version_cycles_node_socket_float_value(max2_socket_B) = -1e-5f;

        blender::bke::node_remove_link(ntree, lacunarity_link);
        blender::bke::node_add_link(
            ntree, lacunarity_from_node, lacunarity_from_socket, max2_node, max2_socket_A);
        blender::bke::node_add_link(ntree, max2_node, max2_socket_out, pow_node, pow_socket_A);
        blender::bke::node_add_link(ntree, max2_node, max2_socket_out, node, lacunarity_socket);
      }
    }
    else if ((lacunarity_link != nullptr) && (roughness_link == nullptr)) {
      /* Add Maximum Math node after output of lacunarity_from_node. Add Power Math node before
       * Roughness input. */

      bNode *max2_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
      max2_node->parent = node->parent;
      max2_node->custom1 = NODE_MATH_MAXIMUM;
      max2_node->locx_legacy = node->locx_legacy;
      max2_node->locy_legacy = node->locy_legacy - 360.0f + locy_offset;
      max2_node->flag |= NODE_HIDDEN;
      bNodeSocket *max2_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&max2_node->inputs, 0));
      bNodeSocket *max2_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&max2_node->inputs, 1));
      bNodeSocket *max2_socket_out = blender::bke::node_find_socket(max2_node, SOCK_OUT, "Value");

      bNode *pow_node = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MATH);
      pow_node->parent = node->parent;
      pow_node->custom1 = NODE_MATH_POWER;
      pow_node->locx_legacy = node->locx_legacy;
      pow_node->locy_legacy = node->locy_legacy - 320.0f + locy_offset;
      pow_node->flag |= NODE_HIDDEN;
      bNodeSocket *pow_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&pow_node->inputs, 0));
      bNodeSocket *pow_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&pow_node->inputs, 1));
      bNodeSocket *pow_socket_out = blender::bke::node_find_socket(pow_node, SOCK_OUT, "Value");

      *version_cycles_node_socket_float_value(max2_socket_B) = -1e-5f;
      *version_cycles_node_socket_float_value(pow_socket_A) = *lacunarity;
      *version_cycles_node_socket_float_value(pow_socket_B) = -(*roughness);

      blender::bke::node_remove_link(ntree, lacunarity_link);
      blender::bke::node_add_link(
          ntree, lacunarity_from_node, lacunarity_from_socket, max2_node, max2_socket_A);
      blender::bke::node_add_link(ntree, max2_node, max2_socket_out, pow_node, pow_socket_A);
      blender::bke::node_add_link(ntree, max2_node, max2_socket_out, node, lacunarity_socket);
      blender::bke::node_add_link(ntree, pow_node, pow_socket_out, node, roughness_socket);
    }
    else {
      *roughness = std::pow(*lacunarity, -(*roughness));
    }
  }

  version_socket_update_is_used(ntree);
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
    if (blender::bke::node_find_socket(node, SOCK_IN, "Subsurface Scale")) {
      /* Node is already updated. */
      continue;
    }

    /* Add Scale input */
    bNodeSocket *scale_in = blender::bke::node_add_static_socket(
        ntree, node, SOCK_IN, SOCK_FLOAT, PROP_DISTANCE, "Subsurface Scale", "Subsurface Scale");

    bNodeSocket *subsurf = blender::bke::node_find_socket(node, SOCK_IN, "Subsurface");
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
    bNodeSocket *base_col = blender::bke::node_find_socket(node, SOCK_IN, "Base Color");
    bNodeSocket *subsurf_col = blender::bke::node_find_socket(node, SOCK_IN, "Subsurface Color");
    float *base_col_val = version_cycles_node_socket_rgba_value(base_col);
    float *subsurf_col_val = version_cycles_node_socket_rgba_value(subsurf_col);
    /* If any of the three inputs is dynamic, we need a Mix node. */
    if (subsurf->link || subsurf_col->link || base_col->link) {
      bNode *mix = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MIX);
      static_cast<NodeShaderMix *>(mix->storage)->data_type = SOCK_RGBA;
      mix->locx_legacy = node->locx_legacy - 170;
      mix->locy_legacy = node->locy_legacy - 120;

      bNodeSocket *a_in = blender::bke::node_find_socket(mix, SOCK_IN, "A_Color");
      bNodeSocket *b_in = blender::bke::node_find_socket(mix, SOCK_IN, "B_Color");
      bNodeSocket *fac_in = blender::bke::node_find_socket(mix, SOCK_IN, "Factor_Float");
      bNodeSocket *result_out = blender::bke::node_find_socket(mix, SOCK_OUT, "Result_Color");

      copy_v4_v4(version_cycles_node_socket_rgba_value(a_in), base_col_val);
      copy_v4_v4(version_cycles_node_socket_rgba_value(b_in), subsurf_col_val);
      *version_cycles_node_socket_float_value(fac_in) = *subsurf_val;

      if (base_col->link) {
        blender::bke::node_add_link(
            ntree, base_col->link->fromnode, base_col->link->fromsock, mix, a_in);
        blender::bke::node_remove_link(ntree, base_col->link);
      }
      if (subsurf_col->link) {
        blender::bke::node_add_link(
            ntree, subsurf_col->link->fromnode, subsurf_col->link->fromsock, mix, b_in);
        blender::bke::node_remove_link(ntree, subsurf_col->link);
      }
      if (subsurf->link) {
        blender::bke::node_add_link(
            ntree, subsurf->link->fromnode, subsurf->link->fromsock, mix, fac_in);
        blender::bke::node_add_link(
            ntree, subsurf->link->fromnode, subsurf->link->fromsock, node, scale_in);
        blender::bke::node_remove_link(ntree, subsurf->link);
      }
      blender::bke::node_add_link(ntree, mix, result_out, node, base_col);
    }
    /* Mix the fixed values. */
    interp_v4_v4v4(base_col_val, base_col_val, subsurf_col_val, *subsurf_val);

    /* Set node to 100% subsurface, 0% diffuse. */
    *subsurf_val = 1.0f;

    /* Delete Subsurface Color input */
    blender::bke::node_remove_socket(ntree, node, subsurf_col);
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
    if (!blender::bke::node_find_socket(node, SOCK_IN, "Emission")) {
      /* Old enough to have neither, new defaults are fine. */
      continue;
    }
    if (blender::bke::node_find_socket(node, SOCK_IN, "Emission Strength")) {
      /* New enough to have both, no need to do anything. */
      continue;
    }
    bNodeSocket *sock = blender::bke::node_add_static_socket(
        ntree, node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Emission Strength", "Emission Strength");
    *version_cycles_node_socket_float_value(sock) = 1.0f;
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

/* Replace old Principled Hair BSDF as a variant in the new Principled Hair BSDF. */
static void version_replace_principled_hair_model(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_HAIR_PRINCIPLED) {
      continue;
    }
    NodeShaderHairPrincipled *data = MEM_cnew<NodeShaderHairPrincipled>(__func__);
    data->model = SHD_PRINCIPLED_HAIR_CHIANG;
    data->parametrization = node->custom1;

    node->storage = data;
  }
}

static void change_input_socket_to_rotation_type(bNodeTree &ntree,
                                                 bNode &node,
                                                 bNodeSocket &socket)
{
  if (socket.type == SOCK_ROTATION) {
    return;
  }
  socket.type = SOCK_ROTATION;
  STRNCPY(socket.idname, "NodeSocketRotation");
  auto *old_value = static_cast<bNodeSocketValueVector *>(socket.default_value);
  auto *new_value = MEM_cnew<bNodeSocketValueRotation>(__func__);
  copy_v3_v3(new_value->value_euler, old_value->value);
  socket.default_value = new_value;
  MEM_freeN(old_value);
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    if (link->tosock != &socket) {
      continue;
    }
    if (ELEM(link->fromsock->type, SOCK_ROTATION, SOCK_VECTOR, SOCK_FLOAT) &&
        !link->fromnode->is_reroute())
    {
      /* No need to add the conversion node when implicit conversions will work. */
      continue;
    }
    if (STREQ(link->fromnode->idname, "FunctionNodeEulerToRotation")) {
      /* Make versioning idempotent. */
      continue;
    }
    bNode *convert = blender::bke::node_add_node(nullptr, &ntree, "FunctionNodeEulerToRotation");
    convert->parent = node.parent;
    convert->locx_legacy = node.locx_legacy - 40;
    convert->locy_legacy = node.locy_legacy;
    link->tonode = convert;
    link->tosock = blender::bke::node_find_socket(convert, SOCK_IN, "Euler");

    blender::bke::node_add_link(&ntree,
                                convert,
                                blender::bke::node_find_socket(convert, SOCK_OUT, "Rotation"),
                                &node,
                                &socket);
  }
}

static void change_output_socket_to_rotation_type(bNodeTree &ntree,
                                                  bNode &node,
                                                  bNodeSocket &socket)
{
  /* Rely on generic node declaration update to change the socket type. */
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    if (link->fromsock != &socket) {
      continue;
    }
    if (ELEM(link->tosock->type, SOCK_ROTATION, SOCK_VECTOR) && !link->tonode->is_reroute()) {
      /* No need to add the conversion node when implicit conversions will work. */
      continue;
    }
    if (STREQ(link->tonode->idname, "FunctionNodeRotationToEuler"))
    { /* Make versioning idempotent. */
      continue;
    }
    bNode *convert = blender::bke::node_add_node(nullptr, &ntree, "FunctionNodeRotationToEuler");
    convert->parent = node.parent;
    convert->locx_legacy = node.locx_legacy + 40;
    convert->locy_legacy = node.locy_legacy;
    link->fromnode = convert;
    link->fromsock = blender::bke::node_find_socket(convert, SOCK_OUT, "Euler");

    blender::bke::node_add_link(&ntree,
                                &node,
                                &socket,
                                convert,
                                blender::bke::node_find_socket(convert, SOCK_IN, "Rotation"));
  }
}

static void version_geometry_nodes_use_rotation_socket(bNodeTree &ntree)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree.nodes) {
    if (STR_ELEM(node->idname,
                 "GeometryNodeInstanceOnPoints",
                 "GeometryNodeRotateInstances",
                 "GeometryNodeTransform"))
    {
      bNodeSocket *socket = blender::bke::node_find_socket(node, SOCK_IN, "Rotation");
      change_input_socket_to_rotation_type(ntree, *node, *socket);
    }
    if (STR_ELEM(node->idname,
                 "GeometryNodeDistributePointsOnFaces",
                 "GeometryNodeObjectInfo",
                 "GeometryNodeInputInstanceRotation"))
    {
      bNodeSocket *socket = blender::bke::node_find_socket(node, SOCK_OUT, "Rotation");
      change_output_socket_to_rotation_type(ntree, *node, *socket);
    }
  }
}

/* Find the base socket name for an idname that may include a subtype. */
static blender::StringRef legacy_socket_idname_to_socket_type(blender::StringRef idname)
{
  using string_pair = std::pair<const char *, const char *>;
  static const string_pair subtypes_map[] = {{"NodeSocketFloatUnsigned", "NodeSocketFloat"},
                                             {"NodeSocketFloatPercentage", "NodeSocketFloat"},
                                             {"NodeSocketFloatFactor", "NodeSocketFloat"},
                                             {"NodeSocketFloatAngle", "NodeSocketFloat"},
                                             {"NodeSocketFloatTime", "NodeSocketFloat"},
                                             {"NodeSocketFloatTimeAbsolute", "NodeSocketFloat"},
                                             {"NodeSocketFloatDistance", "NodeSocketFloat"},
                                             {"NodeSocketIntUnsigned", "NodeSocketInt"},
                                             {"NodeSocketIntPercentage", "NodeSocketInt"},
                                             {"NodeSocketIntFactor", "NodeSocketInt"},
                                             {"NodeSocketVectorTranslation", "NodeSocketVector"},
                                             {"NodeSocketVectorDirection", "NodeSocketVector"},
                                             {"NodeSocketVectorVelocity", "NodeSocketVector"},
                                             {"NodeSocketVectorAcceleration", "NodeSocketVector"},
                                             {"NodeSocketVectorEuler", "NodeSocketVector"},
                                             {"NodeSocketVectorXYZ", "NodeSocketVector"}};
  for (const string_pair &pair : subtypes_map) {
    if (pair.first == idname) {
      return pair.second;
    }
  }
  /* Unchanged socket idname. */
  return idname;
}

static bNodeTreeInterfaceItem *legacy_socket_move_to_interface(bNodeSocket &legacy_socket,
                                                               const eNodeSocketInOut in_out)
{
  bNodeTreeInterfaceSocket *new_socket = MEM_cnew<bNodeTreeInterfaceSocket>(__func__);
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
  tree_interface.root_panel.items_array = static_cast<bNodeTreeInterfaceItem **>(MEM_malloc_arrayN(
      tree_interface.root_panel.items_num, sizeof(bNodeTreeInterfaceItem *), __func__));

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

/**
 * Original node tree interface conversion in did not convert socket idnames with subtype suffixes
 * to correct socket base types (see #versioning_convert_node_tree_socket_lists_to_interface).
 */
static void versioning_fix_socket_subtype_idnames(bNodeTree *ntree)
{
  bNodeTreeInterface &tree_interface = ntree->tree_interface;

  tree_interface.foreach_item([](bNodeTreeInterfaceItem &item) -> bool {
    if (item.item_type == NODE_INTERFACE_SOCKET) {
      bNodeTreeInterfaceSocket &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);
      blender::StringRef corrected_socket_type = legacy_socket_idname_to_socket_type(
          socket.socket_type);
      if (socket.socket_type != corrected_socket_type) {
        MEM_freeN(socket.socket_type);
        socket.socket_type = BLI_strdup(corrected_socket_type.data());
      }
    }
    return true;
  });
}

/* Convert coat inputs on the Principled BSDF. */
static void version_principled_bsdf_coat(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_BSDF_PRINCIPLED) {
      continue;
    }
    if (blender::bke::node_find_socket(node, SOCK_IN, "Coat IOR") != nullptr) {
      continue;
    }
    bNodeSocket *coat_ior_input = blender::bke::node_add_static_socket(
        ntree, node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Coat IOR", "Coat IOR");

    /* Adjust for 4x change in intensity. */
    bNodeSocket *coat_input = blender::bke::node_find_socket(node, SOCK_IN, "Clearcoat");
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

static void remove_triangulate_node_min_size_input(bNodeTree *tree)
{
  using namespace blender;
  Set<bNode *> triangulate_nodes;
  LISTBASE_FOREACH (bNode *, node, &tree->nodes) {
    if (node->type_legacy == GEO_NODE_TRIANGULATE) {
      triangulate_nodes.add(node);
    }
  }

  Map<bNodeSocket *, bNodeLink *> input_links;
  LISTBASE_FOREACH (bNodeLink *, link, &tree->links) {
    if (triangulate_nodes.contains(link->tonode)) {
      input_links.add_new(link->tosock, link);
    }
  }

  for (bNode *triangulate : triangulate_nodes) {
    bNodeSocket *selection = bke::node_find_socket(triangulate, SOCK_IN, "Selection");
    bNodeSocket *min_verts = bke::node_find_socket(triangulate, SOCK_IN, "Minimum Vertices");
    if (!min_verts) {
      /* Make versioning idempotent. */
      continue;
    }
    const int old_min_verts = static_cast<bNodeSocketValueInt *>(min_verts->default_value)->value;
    if (!input_links.contains(min_verts) && old_min_verts <= 4) {
      continue;
    }
    bNode &corners_of_face = version_node_add_empty(*tree, "GeometryNodeCornersOfFace");
    version_node_add_socket_if_not_exist(
        tree, &corners_of_face, SOCK_IN, SOCK_INT, PROP_NONE, "Face Index", "Face Index");
    version_node_add_socket_if_not_exist(
        tree, &corners_of_face, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Weights", "Weights");
    version_node_add_socket_if_not_exist(
        tree, &corners_of_face, SOCK_IN, SOCK_INT, PROP_NONE, "Sort Index", "Sort Index");
    version_node_add_socket_if_not_exist(
        tree, &corners_of_face, SOCK_OUT, SOCK_INT, PROP_NONE, "Corner Index", "Corner Index");
    version_node_add_socket_if_not_exist(
        tree, &corners_of_face, SOCK_OUT, SOCK_INT, PROP_NONE, "Total", "Total");
    corners_of_face.locx_legacy = triangulate->locx_legacy - 200;
    corners_of_face.locy_legacy = triangulate->locy_legacy - 50;
    corners_of_face.parent = triangulate->parent;
    LISTBASE_FOREACH (bNodeSocket *, socket, &corners_of_face.inputs) {
      socket->flag |= SOCK_HIDDEN;
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &corners_of_face.outputs) {
      if (!STREQ(socket->identifier, "Total")) {
        socket->flag |= SOCK_HIDDEN;
      }
    }

    bNode &greater_or_equal = version_node_add_empty(*tree, "FunctionNodeCompare");
    auto *compare_storage = MEM_cnew<NodeFunctionCompare>(__func__);
    compare_storage->operation = NODE_COMPARE_GREATER_EQUAL;
    compare_storage->data_type = SOCK_INT;
    greater_or_equal.storage = compare_storage;
    version_node_add_socket_if_not_exist(
        tree, &greater_or_equal, SOCK_IN, SOCK_INT, PROP_NONE, "A_INT", "A");
    version_node_add_socket_if_not_exist(
        tree, &greater_or_equal, SOCK_IN, SOCK_INT, PROP_NONE, "B_INT", "B");
    version_node_add_socket_if_not_exist(
        tree, &greater_or_equal, SOCK_OUT, SOCK_BOOLEAN, PROP_NONE, "Result", "Result");
    greater_or_equal.locx_legacy = triangulate->locx_legacy - 100;
    greater_or_equal.locy_legacy = triangulate->locy_legacy - 50;
    greater_or_equal.parent = triangulate->parent;
    greater_or_equal.flag &= ~NODE_OPTIONS;
    version_node_add_link(*tree,
                          corners_of_face,
                          *bke::node_find_socket(&corners_of_face, SOCK_OUT, "Total"),
                          greater_or_equal,
                          *bke::node_find_socket(&greater_or_equal, SOCK_IN, "A_INT"));
    if (bNodeLink **min_verts_link = input_links.lookup_ptr(min_verts)) {
      (*min_verts_link)->tonode = &greater_or_equal;
      (*min_verts_link)->tosock = bke::node_find_socket(&greater_or_equal, SOCK_IN, "B_INT");
    }
    else {
      bNodeSocket *new_min_verts = bke::node_find_socket(&greater_or_equal, SOCK_IN, "B_INT");
      static_cast<bNodeSocketValueInt *>(new_min_verts->default_value)->value = old_min_verts;
    }

    if (bNodeLink **selection_link = input_links.lookup_ptr(selection)) {
      bNode &boolean_and = version_node_add_empty(*tree, "FunctionNodeBooleanMath");
      version_node_add_socket_if_not_exist(
          tree, &boolean_and, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Boolean", "Boolean");
      version_node_add_socket_if_not_exist(
          tree, &boolean_and, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Boolean_001", "Boolean");
      version_node_add_socket_if_not_exist(
          tree, &boolean_and, SOCK_OUT, SOCK_BOOLEAN, PROP_NONE, "Boolean", "Boolean");
      boolean_and.locx_legacy = triangulate->locx_legacy - 75;
      boolean_and.locy_legacy = triangulate->locy_legacy - 50;
      boolean_and.parent = triangulate->parent;
      boolean_and.flag &= ~NODE_OPTIONS;
      boolean_and.custom1 = NODE_BOOLEAN_MATH_AND;

      (*selection_link)->tonode = &boolean_and;
      (*selection_link)->tosock = bke::node_find_socket(&boolean_and, SOCK_IN, "Boolean");
      version_node_add_link(*tree,
                            greater_or_equal,
                            *bke::node_find_socket(&greater_or_equal, SOCK_OUT, "Result"),
                            boolean_and,
                            *bke::node_find_socket(&boolean_and, SOCK_IN, "Boolean_001"));

      version_node_add_link(*tree,
                            boolean_and,
                            *bke::node_find_socket(&boolean_and, SOCK_OUT, "Boolean"),
                            *triangulate,
                            *selection);
    }
    else {
      version_node_add_link(*tree,
                            greater_or_equal,
                            *bke::node_find_socket(&greater_or_equal, SOCK_OUT, "Result"),
                            *triangulate,
                            *selection);
    }

    /* Make versioning idempotent. */
    bke::node_remove_socket(tree, triangulate, min_verts);
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
        node, SOCK_IN, "Specular Tint");
    if (specular_tint_sock->type == SOCK_RGBA) {
      /* Node is already updated. */
      continue;
    }

    bNodeSocket *base_color_sock = blender::bke::node_find_socket(node, SOCK_IN, "Base Color");
    bNodeSocket *metallic_sock = blender::bke::node_find_socket(node, SOCK_IN, "Metallic");
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
      bNode *mix = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MIX);
      static_cast<NodeShaderMix *>(mix->storage)->data_type = SOCK_RGBA;
      mix->locx_legacy = node->locx_legacy - 270;
      mix->locy_legacy = node->locy_legacy - 120;

      bNodeSocket *a_in = blender::bke::node_find_socket(mix, SOCK_IN, "A_Color");
      bNodeSocket *b_in = blender::bke::node_find_socket(mix, SOCK_IN, "B_Color");
      bNodeSocket *fac_in = blender::bke::node_find_socket(mix, SOCK_IN, "Factor_Float");
      metallic_mix_out = blender::bke::node_find_socket(mix, SOCK_OUT, "Result_Color");
      metallic_mix_node = mix;

      copy_v4_v4(version_cycles_node_socket_rgba_value(a_in), base_color);
      if (base_color_sock->link) {
        blender::bke::node_add_link(
            ntree, base_color_sock->link->fromnode, base_color_sock->link->fromsock, mix, a_in);
      }
      copy_v4_v4(version_cycles_node_socket_rgba_value(b_in), one);
      *version_cycles_node_socket_float_value(fac_in) = metallic;
      if (metallic_sock->link) {
        blender::bke::node_add_link(
            ntree, metallic_sock->link->fromnode, metallic_sock->link->fromsock, mix, fac_in);
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
      bNode *mix = blender::bke::node_add_static_node(nullptr, ntree, SH_NODE_MIX);
      static_cast<NodeShaderMix *>(mix->storage)->data_type = SOCK_RGBA;
      mix->locx_legacy = node->locx_legacy - 170;
      mix->locy_legacy = node->locy_legacy - 120;

      bNodeSocket *a_in = blender::bke::node_find_socket(mix, SOCK_IN, "A_Color");
      bNodeSocket *b_in = blender::bke::node_find_socket(mix, SOCK_IN, "B_Color");
      bNodeSocket *fac_in = blender::bke::node_find_socket(mix, SOCK_IN, "Factor_Float");
      bNodeSocket *result_out = blender::bke::node_find_socket(mix, SOCK_OUT, "Result_Color");

      copy_v4_v4(version_cycles_node_socket_rgba_value(a_in), one);
      copy_v4_v4(version_cycles_node_socket_rgba_value(b_in), metallic_mix);
      if (metallic_mix_out) {
        blender::bke::node_add_link(ntree, metallic_mix_node, metallic_mix_out, mix, b_in);
      }
      *version_cycles_node_socket_float_value(fac_in) = specular_tint_old;
      if (specular_tint_sock->link) {
        blender::bke::node_add_link(ntree,
                                    specular_tint_sock->link->fromnode,
                                    specular_tint_sock->link->fromsock,
                                    mix,
                                    fac_in);
        blender::bke::node_remove_link(ntree, specular_tint_sock->link);
      }
      blender::bke::node_add_link(ntree, mix, result_out, node, specular_tint_sock);
    }
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
  parent.items_array = MEM_cnew_array<bNodeTreeInterfaceItem *>(parent.items_num, __func__);
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
        group->geometry_node_asset_traits = MEM_cnew<GeometryNodeAssetTraits>(__func__);
      }
      group->geometry_node_asset_traits->flag |= GEO_NODE_ASSET_MODIFIER;
      return false;
    });
  }
}

static void version_socket_identifier_suffixes_for_dynamic_types(
    ListBase sockets, const char *separator, const std::optional<int> total = std::nullopt)
{
  int index = 0;
  LISTBASE_FOREACH (bNodeSocket *, socket, &sockets) {
    if (socket->is_available()) {
      if (char *pos = strstr(socket->identifier, separator)) {
        /* End the identifier at the separator so that the old suffix is ignored. */
        *pos = '\0';

        if (total.has_value()) {
          index++;
          if (index == *total) {
            return;
          }
        }
      }
    }
    else {
      /* Rename existing identifiers so that they don't conflict with the renamed one. Those will
       * be removed after versioning code. */
      BLI_strncat(socket->identifier, "_deprecated", sizeof(socket->identifier));
    }
  }
}

static void versioning_nodes_dynamic_sockets(bNodeTree &ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    switch (node->type_legacy) {
      case GEO_NODE_ACCUMULATE_FIELD:
        /* This node requires the extra `total` parameter, because the `Group Index` identifier
         * also has a space in the name, that should not be treated as separator. */
        version_socket_identifier_suffixes_for_dynamic_types(node->inputs, " ", 1);
        version_socket_identifier_suffixes_for_dynamic_types(node->outputs, " ", 3);
        break;
      case GEO_NODE_CAPTURE_ATTRIBUTE:
      case GEO_NODE_ATTRIBUTE_STATISTIC:
      case GEO_NODE_BLUR_ATTRIBUTE:
      case GEO_NODE_EVALUATE_AT_INDEX:
      case GEO_NODE_EVALUATE_ON_DOMAIN:
      case GEO_NODE_INPUT_NAMED_ATTRIBUTE:
      case GEO_NODE_RAYCAST:
      case GEO_NODE_SAMPLE_INDEX:
      case GEO_NODE_SAMPLE_NEAREST_SURFACE:
      case GEO_NODE_SAMPLE_UV_SURFACE:
      case GEO_NODE_STORE_NAMED_ATTRIBUTE:
      case GEO_NODE_VIEWER:
        version_socket_identifier_suffixes_for_dynamic_types(node->inputs, "_");
        version_socket_identifier_suffixes_for_dynamic_types(node->outputs, "_");
        break;
    }
  }
}

static void versioning_nodes_dynamic_sockets_2(bNodeTree &ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (!ELEM(node->type_legacy, GEO_NODE_SWITCH, GEO_NODE_SAMPLE_CURVE)) {
      continue;
    }
    version_socket_identifier_suffixes_for_dynamic_types(node->inputs, "_");
    version_socket_identifier_suffixes_for_dynamic_types(node->outputs, "_");
  }
}

static void convert_grease_pencil_stroke_hardness_to_softness(GreasePencil *grease_pencil)
{
  using namespace blender;
  for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    bke::greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    const int layer_index = CustomData_get_named_layer_index(
        &drawing.geometry.curve_data, CD_PROP_FLOAT, "hardness");
    if (layer_index == -1) {
      continue;
    }
    float *data = static_cast<float *>(CustomData_get_layer_named_for_write(
        &drawing.geometry.curve_data, CD_PROP_FLOAT, "hardness", drawing.geometry.curve_num));
    for (const int i : IndexRange(drawing.geometry.curve_num)) {
      data[i] = 1.0f - data[i];
    }
    /* Rename the layer. */
    STRNCPY(drawing.geometry.curve_data.layers[layer_index].name, "softness");
  }
}

static void versioning_grease_pencil_stroke_radii_scaling(GreasePencil *grease_pencil)
{
  using namespace blender;
  for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    bke::greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    MutableSpan<float> radii = drawing.radii_for_write();
    threading::parallel_for(radii.index_range(), 8192, [&](const IndexRange range) {
      for (const int i : range) {
        radii[i] *= bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;
      }
    });
  }
}

static void fix_geometry_nodes_object_info_scale(bNodeTree &ntree)
{
  using namespace blender;
  MultiValueMap<bNodeSocket *, bNodeLink *> out_links_per_socket;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    if (link->fromnode->type_legacy == GEO_NODE_OBJECT_INFO) {
      out_links_per_socket.add(link->fromsock, link);
    }
  }

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree.nodes) {
    if (node->type_legacy != GEO_NODE_OBJECT_INFO) {
      continue;
    }
    bNodeSocket *scale = blender::bke::node_find_socket(node, SOCK_OUT, "Scale");
    const Span<bNodeLink *> links = out_links_per_socket.lookup(scale);
    if (links.is_empty()) {
      continue;
    }
    bNode *absolute_value = blender::bke::node_add_node(nullptr, &ntree, "ShaderNodeVectorMath");
    absolute_value->custom1 = NODE_VECTOR_MATH_ABSOLUTE;
    absolute_value->parent = node->parent;
    absolute_value->locx_legacy = node->locx_legacy + 100;
    absolute_value->locy_legacy = node->locy_legacy - 50;
    blender::bke::node_add_link(&ntree,
                                node,
                                scale,
                                absolute_value,
                                static_cast<bNodeSocket *>(absolute_value->inputs.first));
    for (bNodeLink *link : links) {
      link->fromnode = absolute_value;
      link->fromsock = static_cast<bNodeSocket *>(absolute_value->outputs.first);
    }
  }
}

static bool strip_filter_bilinear_to_auto(Strip *strip, void * /*user_data*/)
{
  StripTransform *transform = strip->data->transform;
  if (transform != nullptr && transform->filter == SEQ_TRANSFORM_FILTER_BILINEAR) {
    transform->filter = SEQ_TRANSFORM_FILTER_AUTO;
  }
  return true;
}

static void update_paint_modes_for_brush_assets(Main &bmain)
{
  /* Replace paint brushes with a reference to the default brush asset for that mode. */
  LISTBASE_FOREACH (Scene *, scene, &bmain.scenes) {
    BKE_paint_brushes_set_default_references(scene->toolsettings);
  }

  /* Replace persistent tool references with the new single builtin brush tool. */
  LISTBASE_FOREACH (WorkSpace *, workspace, &bmain.workspaces) {
    LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
      if (tref->space_type == SPACE_IMAGE && tref->mode == SI_MODE_PAINT) {
        STRNCPY(tref->idname, "builtin.brush");
        continue;
      }
      if (tref->space_type != SPACE_VIEW3D) {
        continue;
      }
      if (!ELEM(tref->mode,
                CTX_MODE_SCULPT,
                CTX_MODE_PAINT_VERTEX,
                CTX_MODE_PAINT_WEIGHT,
                CTX_MODE_PAINT_TEXTURE,
                CTX_MODE_PAINT_GPENCIL_LEGACY,
                CTX_MODE_PAINT_GREASE_PENCIL,
                CTX_MODE_SCULPT_GPENCIL_LEGACY,
                CTX_MODE_SCULPT_GREASE_PENCIL,
                CTX_MODE_WEIGHT_GPENCIL_LEGACY,
                CTX_MODE_WEIGHT_GREASE_PENCIL,
                CTX_MODE_VERTEX_GREASE_PENCIL,
                CTX_MODE_VERTEX_GPENCIL_LEGACY,
                CTX_MODE_SCULPT_CURVES))
      {
        continue;
      }
      STRNCPY(tref->idname, "builtin.brush");
    }
  }
}

static void image_settings_avi_to_ffmpeg(Scene *scene)
{
  if (ELEM(scene->r.im_format.imtype, R_IMF_IMTYPE_AVIRAW, R_IMF_IMTYPE_AVIJPEG)) {
    scene->r.im_format.imtype = R_IMF_IMTYPE_FFMPEG;
  }
}

/* The Hue Correct curve now wraps around by specifying CUMA_USE_WRAPPING, which means it no longer
 * makes sense to have curve maps outside of the [0, 1] range, so enable clipping and reset the
 * clip and view ranges. */
static void hue_correct_set_wrapping(CurveMapping *curve_mapping)
{
  curve_mapping->flag |= CUMA_DO_CLIP;
  curve_mapping->flag |= CUMA_USE_WRAPPING;

  curve_mapping->clipr.xmin = 0.0f;
  curve_mapping->clipr.xmax = 1.0f;
  curve_mapping->clipr.ymin = 0.0f;
  curve_mapping->clipr.ymax = 1.0f;

  curve_mapping->curr.xmin = 0.0f;
  curve_mapping->curr.xmax = 1.0f;
  curve_mapping->curr.ymin = 0.0f;
  curve_mapping->curr.ymax = 1.0f;
}

static bool strip_hue_correct_set_wrapping(Strip *strip, void * /*user_data*/)
{
  LISTBASE_FOREACH (SequenceModifierData *, smd, &strip->modifiers) {
    if (smd->type == seqModifierType_HueCorrect) {
      HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;
      CurveMapping *cumap = (CurveMapping *)&hcmd->curve_mapping;
      hue_correct_set_wrapping(cumap);
    }
  }
  return true;
}

static void versioning_update_timecode(short int *tc)
{
  /* 2 = IMB_TC_FREE_RUN, 4 = IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN. */
  if (ELEM(*tc, 2, 4)) {
    *tc = IMB_TC_RECORD_RUN;
  }
}

static bool strip_proxies_timecode_update(Strip *strip, void * /*user_data*/)
{
  if (strip->data == nullptr || strip->data->proxy == nullptr) {
    return true;
  }
  StripProxy *proxy = strip->data->proxy;
  versioning_update_timecode(&proxy->tc);
  return true;
}

static bool strip_text_data_update(Strip *strip, void * /*user_data*/)
{
  if (strip->type != STRIP_TYPE_TEXT || strip->effectdata == nullptr) {
    return true;
  }

  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  if (data->shadow_angle == 0.0f) {
    data->shadow_angle = DEG2RADF(65.0f);
    data->shadow_offset = 0.04f;
    data->shadow_blur = 0.0f;
  }
  if (data->outline_width == 0.0f) {
    data->outline_color[3] = 0.7f;
    data->outline_width = 0.05f;
  }
  return true;
}

static void versioning_node_hue_correct_set_wrappng(bNodeTree *ntree)
{
  if (ntree->type == NTREE_COMPOSIT) {
    LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {

      if (node->type_legacy == CMP_NODE_HUECORRECT) {
        CurveMapping *cumap = (CurveMapping *)node->storage;
        hue_correct_set_wrapping(cumap);
      }
    }
  }
}

static void add_image_editor_asset_shelf(Main &bmain)
{
  LISTBASE_FOREACH (bScreen *, screen, &bmain.screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype != SPACE_IMAGE) {
          continue;
        }

        ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;

        if (ARegion *new_shelf_region = do_versions_add_region_if_not_found(
                regionbase, RGN_TYPE_ASSET_SHELF, __func__, RGN_TYPE_TOOL_HEADER))
        {
          new_shelf_region->regiondata = MEM_cnew<RegionAssetShelf>(__func__);
          new_shelf_region->alignment = RGN_ALIGN_BOTTOM;
          new_shelf_region->flag |= RGN_FLAG_HIDDEN;
        }
        if (ARegion *new_shelf_header = do_versions_add_region_if_not_found(
                regionbase, RGN_TYPE_ASSET_SHELF_HEADER, __func__, RGN_TYPE_ASSET_SHELF))
        {
          new_shelf_header->alignment = RGN_ALIGN_BOTTOM | RGN_ALIGN_HIDE_WITH_PREV;
        }
      }
    }
  }
}

static void node_reroute_add_storage(bNodeTree &tree)
{
  for (bNode *node : tree.all_nodes()) {
    if (node->is_reroute()) {
      if (node->storage != nullptr) {
        continue;
      }

      bNodeSocket &input = *static_cast<bNodeSocket *>(node->inputs.first);
      bNodeSocket &output = *static_cast<bNodeSocket *>(node->outputs.first);

      /* Use uniform identifier for sockets. In old Blender versions (<=2021, up to af0b7925), the
       * identifiers were sometimes all lower case. Fixing those wrong socket identifiers is
       * important because otherwise they loose links now that the reroute node also uses node
       * declarations. */
      STRNCPY(input.identifier, "Input");
      STRNCPY(output.identifier, "Output");

      NodeReroute *data = MEM_cnew<NodeReroute>(__func__);
      STRNCPY(data->type_idname, input.idname);
      node->storage = data;
    }
  }
}

/**
 * It was possible that curve attributes were initialized to 0 even if that is not allowed for some
 * attributes.
 */
static void fix_built_in_curve_attribute_defaults(Main *bmain)
{
  LISTBASE_FOREACH (Curves *, curves, &bmain->hair_curves) {
    const int curves_num = curves->geometry.curve_num;
    if (int *resolutions = static_cast<int *>(CustomData_get_layer_named_for_write(
            &curves->geometry.curve_data, CD_PROP_INT32, "resolution", curves_num)))
    {
      for (int &resolution : blender::MutableSpan{resolutions, curves_num}) {
        resolution = std::max(resolution, 1);
      }
    }
    if (int8_t *nurb_orders = static_cast<int8_t *>(CustomData_get_layer_named_for_write(
            &curves->geometry.curve_data, CD_PROP_INT8, "nurbs_order", curves_num)))
    {
      for (int8_t &nurbs_order : blender::MutableSpan{nurb_orders, curves_num}) {
        nurbs_order = std::max<int8_t>(nurbs_order, 1);
      }
    }
  }
}

static void add_bevel_modifier_attribute_name_defaults(Main &bmain)
{
  LISTBASE_FOREACH (Object *, ob, &bmain.objects) {
    if (ob->type != OB_MESH) {
      continue;
    }
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_Bevel) {
        BevelModifierData *bmd = reinterpret_cast<BevelModifierData *>(md);
        if (bmd->vertex_weight_name[0] == '\0') {
          STRNCPY(bmd->vertex_weight_name, "bevel_weight_vert");
        }
        if (bmd->edge_weight_name[0] == '\0') {
          STRNCPY(bmd->edge_weight_name, "bevel_weight_edge");
        }
      }
    }
  }
}

static void hide_simulation_node_skip_socket_value(Main &bmain)
{
  LISTBASE_FOREACH (bNodeTree *, tree, &bmain.nodetrees) {
    LISTBASE_FOREACH (bNode *, node, &tree->nodes) {
      if (node->type_legacy != GEO_NODE_SIMULATION_OUTPUT) {
        continue;
      }
      bNodeSocket *skip_input = static_cast<bNodeSocket *>(node->inputs.first);
      if (!skip_input || !STREQ(skip_input->identifier, "Skip")) {
        continue;
      }
      auto *default_value = static_cast<bNodeSocketValueBoolean *>(skip_input->default_value);
      if (!default_value->value) {
        continue;
      }
      bool is_linked = false;
      LISTBASE_FOREACH (bNodeLink *, link, &tree->links) {
        if (link->tosock == skip_input) {
          is_linked = true;
        }
      }
      if (is_linked) {
        continue;
      }

      bNode &input_node = version_node_add_empty(*tree, "FunctionNodeInputBool");
      input_node.parent = node->parent;
      input_node.locx_legacy = node->locx_legacy - 25;
      input_node.locy_legacy = node->locy_legacy;

      NodeInputBool *input_node_storage = MEM_cnew<NodeInputBool>(__func__);
      input_node.storage = input_node_storage;
      input_node_storage->boolean = true;

      bNodeSocket &input_node_socket = version_node_add_socket(
          *tree, input_node, SOCK_OUT, "NodeSocketBool", "Boolean");

      version_node_add_link(*tree, input_node, input_node_socket, *node, *skip_input);

      /* Change the old socket value so that the versioning code is not run again. */
      default_value->value = false;
    }
  }
}

static bool versioning_convert_seq_text_anchor(Strip *strip, void * /*user_data*/)
{
  if (strip->type != STRIP_TYPE_TEXT || strip->effectdata == nullptr) {
    return true;
  }

  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  data->anchor_x = data->align;
  data->anchor_y = data->align_y;
  data->align = SEQ_TEXT_ALIGN_X_LEFT;

  return true;
}

static void add_subsurf_node_limit_surface_option(Main &bmain)
{
  LISTBASE_FOREACH (bNodeTree *, ntree, &bmain.nodetrees) {
    if (ntree->type == NTREE_GEOMETRY) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy == GEO_NODE_SUBDIVISION_SURFACE) {
          bNodeSocket *socket = version_node_add_socket_if_not_exist(
              ntree, node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Limit Surface", "Limit Surface");
          static_cast<bNodeSocketValueBoolean *>(socket->default_value)->value = false;
        }
      }
    }
  }
}

static void version_node_locations_to_global(bNodeTree &ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    node->location[0] = node->locx_legacy;
    node->location[1] = node->locy_legacy;
    for (const bNode *parent = node->parent; parent; parent = parent->parent) {
      node->location[0] += parent->locx_legacy;
      node->location[1] += parent->locy_legacy;
    }

    node->location[0] += node->offsetx_legacy;
    node->location[1] += node->offsety_legacy;
    node->offsetx_legacy = 0.0f;
    node->offsety_legacy = 0.0f;
  }
}

static CustomDataLayer *find_old_seam_layer(CustomData &custom_data, const blender::StringRef name)
{
  for (CustomDataLayer &layer : blender::MutableSpan(custom_data.layers, custom_data.totlayer)) {
    if (layer.name == name) {
      return &layer;
    }
  }
  return nullptr;
}

static void rename_mesh_uv_seam_attribute(Mesh &mesh)
{
  using namespace blender;
  CustomDataLayer *old_seam_layer = find_old_seam_layer(mesh.edge_data, ".uv_seam");
  if (!old_seam_layer) {
    return;
  }
  Set<StringRef> names;
  for (const CustomDataLayer &layer : Span(mesh.vert_data.layers, mesh.vert_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.edge_data.layers, mesh.edge_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.face_data.layers, mesh.face_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.corner_data.layers, mesh.corner_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  LISTBASE_FOREACH (const bDeformGroup *, vertex_group, &mesh.vertex_group_names) {
    names.add(vertex_group->name);
  }

  /* If the new UV name is already taken, still rename the attribute so it becomes visible in the
   * list. Then the user can deal with the name conflict themselves. */
  const std::string new_name = BLI_uniquename_cb(
      [&](const StringRef name) { return names.contains(name); }, '.', "uv_seam");
  STRNCPY(old_seam_layer->name, new_name.c_str());
}

/**
 * Clear unnecessary pointers to data blocks on output sockets group input nodes.
 * These values should never have been set in the first place. They are not harmful on their own,
 * but can pull in additional data-blocks when the node group is linked/appended.
 */
static void version_group_input_socket_data_block_reference(bNodeTree &ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (!node->is_group_input()) {
      continue;
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
      switch (socket->type) {
        case SOCK_OBJECT:
          socket->default_value_typed<bNodeSocketValueObject>()->value = nullptr;
          break;
        case SOCK_IMAGE:
          socket->default_value_typed<bNodeSocketValueImage>()->value = nullptr;
          break;
        case SOCK_COLLECTION:
          socket->default_value_typed<bNodeSocketValueCollection>()->value = nullptr;
          break;
        case SOCK_TEXTURE:
          socket->default_value_typed<bNodeSocketValueTexture>()->value = nullptr;
          break;
        case SOCK_MATERIAL:
          socket->default_value_typed<bNodeSocketValueMaterial>()->value = nullptr;
          break;
      }
    }
  }
}

static void version_geometry_normal_input_node(bNodeTree &ntree)
{
  if (ntree.type == NTREE_GEOMETRY) {
    LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
      if (STREQ(node->idname, "GeometryNodeInputNormal")) {
        node->custom1 = 1;
      }
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

  /* 400 4 did not require any do_version here. */

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
              NodeTexNoise *tex = MEM_cnew<NodeTexNoise>(__func__);
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
      const int curvetype = BKE_curve_type_get(curve);
      if (curvetype == OB_FONT) {
        CharInfo *info = curve->strinfo;
        if (info != nullptr) {
          for (int i = curve->len_char32 - 1; i >= 0; i--, info++) {
            if (info->mat_nr > 0) {
              /** CharInfo mat_nr used to start at 1, unlike mesh & nurbs, now zero-based. */
              info->mat_nr--;
            }
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 1)) {
    LISTBASE_FOREACH (GreasePencil *, grease_pencil, &bmain->grease_pencils) {
      versioning_grease_pencil_stroke_radii_scaling(grease_pencil);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 4)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_CUSTOM) {
        /* versioning_update_noise_texture_node must be done before
         * versioning_replace_musgrave_texture_node. */
        versioning_update_noise_texture_node(ntree);

        /* Convert Musgrave Texture nodes to Noise Texture nodes. */
        versioning_replace_musgrave_texture_node(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 5)) {
    /* Unify Material::blend_shadow and Cycles.use_transparent_shadows into the
     * Material::blend_flag. */
    bool is_eevee = all_scenes_use(bmain,
                                   {RE_engine_id_BLENDER_EEVEE, RE_engine_id_BLENDER_EEVEE_NEXT});
    LISTBASE_FOREACH (Material *, material, &bmain->materials) {
      bool transparent_shadows = true;
      if (is_eevee) {
        transparent_shadows = material->blend_shadow != MA_BS_SOLID;
      }
      else if (IDProperty *cmat = version_cycles_properties_from_ID(&material->id)) {
        transparent_shadows = version_cycles_property_boolean(
            cmat, "use_transparent_shadow", true);
      }
      SET_FLAG_FROM_TEST(material->blend_flag, transparent_shadows, MA_BL_TRANSPARENT_SHADOW);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 5)) {
    /** NOTE: This versioning code didn't update the subversion number. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        versioning_replace_splitviewer(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  /* 401 6 did not require any do_version here. */

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 7)) {
    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "int", "volumetric_ray_depth")) {
      SceneEEVEE default_eevee = *DNA_struct_default_get(SceneEEVEE);
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.volumetric_ray_depth = default_eevee.volumetric_ray_depth;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "Material", "char", "surface_render_method")) {
      LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
        mat->surface_render_method = (mat->blend_method == MA_BM_BLEND) ?
                                         MA_SURFACE_METHOD_FORWARD :
                                         MA_SURFACE_METHOD_DEFERRED;
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          const ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                       &sl->regionbase;
          LISTBASE_FOREACH (ARegion *, region, regionbase) {
            if (region->regiontype != RGN_TYPE_ASSET_SHELF_HEADER) {
              continue;
            }
            region->alignment &= ~RGN_SPLIT_PREV;
            region->alignment |= RGN_ALIGN_HIDE_WITH_PREV;
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "float", "gtao_thickness")) {
      SceneEEVEE default_eevee = *DNA_struct_default_get(SceneEEVEE);
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.gtao_thickness = default_eevee.gtao_thickness;
        scene->eevee.gtao_focus = default_eevee.gtao_focus;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "LightProbe", "float", "data_display_size")) {
      LightProbe default_probe = *DNA_struct_default_get(LightProbe);
      LISTBASE_FOREACH (LightProbe *, probe, &bmain->lightprobes) {
        probe->data_display_size = default_probe.data_display_size;
      }
    }

    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      mesh->flag &= ~ME_NO_OVERLAPPING_TOPOLOGY;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 8)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      versioning_nodes_dynamic_sockets(*ntree);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 9)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Material", "char", "displacement_method")) {
      /* Replace Cycles.displacement_method by Material::displacement_method. */
      LISTBASE_FOREACH (Material *, material, &bmain->materials) {
        int displacement_method = MA_DISPLACEMENT_BUMP;
        if (IDProperty *cmat = version_cycles_properties_from_ID(&material->id)) {
          displacement_method = version_cycles_property_int(
              cmat, "displacement_method", MA_DISPLACEMENT_BUMP);
        }
        material->displacement_method = displacement_method;
      }
    }

    /* Prevent custom bone colors from having alpha zero.
     * Part of the fix for issue #115434. */
    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      blender::animrig::ANIM_armature_foreach_bone(&arm->bonebase, [](Bone *bone) {
        bone->color.custom.solid[3] = 255;
        bone->color.custom.select[3] = 255;
        bone->color.custom.active[3] = 255;
      });
      if (arm->edbo) {
        LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
          ebone->color.custom.solid[3] = 255;
          ebone->color.custom.select[3] = 255;
          ebone->color.custom.active[3] = 255;
        }
      }
    }
    LISTBASE_FOREACH (Object *, obj, &bmain->objects) {
      if (obj->pose == nullptr) {
        continue;
      }
      LISTBASE_FOREACH (bPoseChannel *, pchan, &obj->pose->chanbase) {
        pchan->color.custom.solid[3] = 255;
        pchan->color.custom.select[3] = 255;
        pchan->color.custom.active[3] = 255;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 10)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "SceneEEVEE", "RaytraceEEVEE", "ray_tracing_options"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.ray_tracing_options.flag = RAYTRACE_EEVEE_USE_DENOISE;
        scene->eevee.ray_tracing_options.denoise_stages = RAYTRACE_EEVEE_DENOISE_SPATIAL |
                                                          RAYTRACE_EEVEE_DENOISE_TEMPORAL |
                                                          RAYTRACE_EEVEE_DENOISE_BILATERAL;
        scene->eevee.ray_tracing_options.screen_trace_quality = 0.25f;
        scene->eevee.ray_tracing_options.screen_trace_thickness = 0.2f;
        scene->eevee.ray_tracing_options.trace_max_roughness = 0.5f;
        scene->eevee.ray_tracing_options.resolution_scale = 2;
      }
    }

    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_geometry_nodes_use_rotation_socket(*ntree);
        versioning_nodes_dynamic_sockets_2(*ntree);
        fix_geometry_nodes_object_info_scale(*ntree);
      }
    }
  }

  if (MAIN_VERSION_FILE_ATLEAST(bmain, 400, 20) && !MAIN_VERSION_FILE_ATLEAST(bmain, 401, 11)) {
    /* Convert old socket lists into new interface items. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      versioning_fix_socket_subtype_idnames(ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 12)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == CMP_NODE_PIXELATE) {
            node->custom1 = 1;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 13)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == CMP_NODE_MAP_UV) {
            node->custom2 = CMP_NODE_MAP_UV_FILTERING_ANISOTROPIC;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 14)) {
    const Brush *default_brush = DNA_struct_default_get(Brush);
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      brush->automasking_start_normal_limit = default_brush->automasking_start_normal_limit;
      brush->automasking_start_normal_falloff = default_brush->automasking_start_normal_falloff;

      brush->automasking_view_normal_limit = default_brush->automasking_view_normal_limit;
      brush->automasking_view_normal_falloff = default_brush->automasking_view_normal_falloff;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 15)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == CMP_NODE_KEYING) {
            NodeKeyingData &keying_data = *static_cast<NodeKeyingData *>(node->storage);
            keying_data.edge_kernel_radius = max_ii(keying_data.edge_kernel_radius - 1, 0);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 16)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Sculpt *sculpt = scene->toolsettings->sculpt;
      if (sculpt != nullptr) {
        Sculpt default_sculpt = *DNA_struct_default_get(Sculpt);
        sculpt->automasking_boundary_edges_propagation_steps =
            default_sculpt.automasking_boundary_edges_propagation_steps;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 17)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      int input_sample_values[9];

      input_sample_values[0] = ts->imapaint.paint.num_input_samples_deprecated;
      input_sample_values[1] = ts->sculpt != nullptr ?
                                   ts->sculpt->paint.num_input_samples_deprecated :
                                   1;
      input_sample_values[2] = ts->curves_sculpt != nullptr ?
                                   ts->curves_sculpt->paint.num_input_samples_deprecated :
                                   1;

      input_sample_values[3] = ts->gp_paint != nullptr ?
                                   ts->gp_paint->paint.num_input_samples_deprecated :
                                   1;
      input_sample_values[4] = ts->gp_vertexpaint != nullptr ?
                                   ts->gp_vertexpaint->paint.num_input_samples_deprecated :
                                   1;
      input_sample_values[5] = ts->gp_sculptpaint != nullptr ?
                                   ts->gp_sculptpaint->paint.num_input_samples_deprecated :
                                   1;
      input_sample_values[6] = ts->gp_weightpaint != nullptr ?
                                   ts->gp_weightpaint->paint.num_input_samples_deprecated :
                                   1;

      input_sample_values[7] = ts->vpaint != nullptr ?
                                   ts->vpaint->paint.num_input_samples_deprecated :
                                   1;
      input_sample_values[8] = ts->wpaint != nullptr ?
                                   ts->wpaint->paint.num_input_samples_deprecated :
                                   1;

      int unified_value = 1;
      for (int i = 0; i < 9; i++) {
        if (input_sample_values[i] != 1) {
          if (unified_value == 1) {
            unified_value = input_sample_values[i];
          }
          else {
            /* In the case of a user having multiple tools with different num_input_value values
             * set we cannot support this in the single UnifiedPaintSettings value, so fallback
             * to 1 instead of deciding that one value is more canonical than the other.
             */
            break;
          }
        }
      }

      ts->unified_paint_settings.input_samples = unified_value;
    }
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      brush->input_samples = 1;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 18)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        SEQ_for_each_callback(&scene->ed->seqbase, strip_filter_bilinear_to_auto, nullptr);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 19)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_socket_name(ntree, FN_NODE_ROTATE_ROTATION, "Rotation 1", "Rotation");
        version_node_socket_name(ntree, FN_NODE_ROTATE_ROTATION, "Rotation 2", "Rotate By");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 20)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      int uid = 1;
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        /* These identifiers are not necessarily stable for linked data. If the linked data has a
         * new modifier inserted, the identifiers of other modifiers can change. */
        md->persistent_uid = uid++;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 21)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      /* The `sculpt_flag` was used to store the `BRUSH_DIR_IN`
       * With the fix for #115313 this is now just using the `brush->flag`. */
      if (brush->gpencil_settings && (brush->gpencil_settings->sculpt_flag & BRUSH_DIR_IN) != 0) {
        brush->flag |= BRUSH_DIR_IN;
      }
    }
  }

  /* Keep point/spot light soft falloff for files created before 4.0. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 400, 0)) {
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      if (ELEM(light->type, LA_LOCAL, LA_SPOT)) {
        light->mode |= LA_USE_SOFT_FALLOFF;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 1)) {
    using namespace blender::bke::greasepencil;
    /* Initialize newly added scale layer transform to one. */
    LISTBASE_FOREACH (GreasePencil *, grease_pencil, &bmain->grease_pencils) {
      for (Layer *layer : grease_pencil->layers_for_write()) {
        copy_v3_fl(layer->scale, 1.0f);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 2)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      bool is_cycles = scene && STREQ(scene->r.engine, RE_engine_id_CYCLES);
      if (is_cycles) {
        if (IDProperty *cscene = version_cycles_properties_from_ID(&scene->id)) {
          int cposition = version_cycles_property_int(cscene, "motion_blur_position", 1);
          BLI_assert(cposition >= 0 && cposition < 3);
          int order_conversion[3] = {SCE_MB_START, SCE_MB_CENTER, SCE_MB_END};
          scene->r.motion_blur_position = order_conversion[std::clamp(cposition, 0, 2)];
        }
      }
      else {
        SET_FLAG_FROM_TEST(
            scene->r.mode, scene->eevee.flag & SCE_EEVEE_MOTION_BLUR_ENABLED_DEPRECATED, R_MBLUR);
        scene->r.motion_blur_position = scene->eevee.motion_blur_position_deprecated;
        scene->r.motion_blur_shutter = scene->eevee.motion_blur_shutter_deprecated;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 3)) {
    constexpr int NTREE_EXECUTION_MODE_CPU = 0;
    constexpr int NTREE_EXECUTION_MODE_FULL_FRAME = 1;

    constexpr int NTREE_COM_GROUPNODE_BUFFER = 1 << 3;
    constexpr int NTREE_COM_OPENCL = 1 << 1;

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }

      ntree->flag &= ~(NTREE_COM_GROUPNODE_BUFFER | NTREE_COM_OPENCL);

      if (ntree->execution_mode == NTREE_EXECUTION_MODE_FULL_FRAME) {
        ntree->execution_mode = NTREE_EXECUTION_MODE_CPU;
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 4)) {
    if (!DNA_struct_member_exists(fd->filesdna, "SpaceImage", "float", "stretch_opacity")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_IMAGE) {
              SpaceImage *sima = reinterpret_cast<SpaceImage *>(sl);
              sima->stretch_opacity = 0.9f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 5)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      image_settings_avi_to_ffmpeg(scene);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 6)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (BrushCurvesSculptSettings *settings = brush->curves_sculpt_settings) {
        settings->flag |= BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_RADIUS;
        settings->curve_radius = 0.01f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 8)) {
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      light->shadow_filter_radius = 1.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 9)) {
    const float default_snap_angle_increment = DEG2RADF(5.0f);
    const float default_snap_angle_increment_precision = DEG2RADF(1.0f);
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->toolsettings->snap_angle_increment_2d = default_snap_angle_increment;
      scene->toolsettings->snap_angle_increment_3d = default_snap_angle_increment;
      scene->toolsettings->snap_angle_increment_2d_precision =
          default_snap_angle_increment_precision;
      scene->toolsettings->snap_angle_increment_3d_precision =
          default_snap_angle_increment_precision;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 10)) {
    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "int", "gtao_resolution")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.gtao_resolution = 2;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 12)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      versioning_node_hue_correct_set_wrappng(ntree);
    }
    FOREACH_NODETREE_END;

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        SEQ_for_each_callback(&scene->ed->seqbase, strip_hue_correct_set_wrapping, nullptr);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 14)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (bMotionPath *mpath = ob->mpath) {
        mpath->color_post[0] = 0.1f;
        mpath->color_post[1] = 1.0f;
        mpath->color_post[2] = 0.1f;
      }
      if (!ob->pose) {
        continue;
      }
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        if (bMotionPath *mpath = pchan->mpath) {
          mpath->color_post[0] = 0.1f;
          mpath->color_post[1] = 1.0f;
          mpath->color_post[2] = 0.1f;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 18)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Light", "float", "transmission_fac")) {
      LISTBASE_FOREACH (Light *, light, &bmain->lights) {
        /* Refracted light was not supported in legacy EEVEE. Set it to zero for compatibility with
         * older files. */
        light->transmission_fac = 0.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 19)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Keep legacy EEVEE old behavior. */
      scene->eevee.flag |= SCE_EEVEE_VOLUME_CUSTOM_RANGE;
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->eevee.clamp_surface_indirect = 10.0f;
      /* Make contribution of indirect lighting very small (but non-null) to avoid world lighting
       * and volume lightprobe changing the appearance of volume objects. */
      scene->eevee.clamp_volume_indirect = 1e-8f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 20)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      SequencerToolSettings *sequencer_tool_settings = SEQ_tool_settings_ensure(scene);
      sequencer_tool_settings->snap_mode |= SEQ_SNAP_TO_MARKERS;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 21)) {
    add_image_editor_asset_shelf(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 22)) {
    /* Display missing media in sequencer by default. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        scene->ed->show_missing_media_flag |= SEQ_EDIT_SHOW_MISSING_MEDIA;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 23)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      if (!ts->uvsculpt.strength_curve) {
        ts->uvsculpt.size = 50;
        ts->uvsculpt.strength = 1.0f;
        ts->uvsculpt.curve_preset = BRUSH_CURVE_SMOOTH;
        ts->uvsculpt.strength_curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 24)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Material", "char", "thickness_mode")) {
      LISTBASE_FOREACH (Material *, material, &bmain->materials) {
        if (material->blend_flag & MA_BL_TRANSLUCENCY) {
          /* EEVEE Legacy used thickness from shadow map when translucency was on. */
          material->blend_flag |= MA_BL_THICKNESS_FROM_SHADOW;
        }
        if ((material->blend_flag & MA_BL_SS_REFRACTION) && material->use_nodes &&
            material->nodetree)
        {
          /* EEVEE Legacy used slab assumption. */
          material->thickness_mode = MA_THICKNESS_SLAB;
          version_refraction_depth_to_thickness_value(material->nodetree, material->refract_depth);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 25)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_BLUR) {
          continue;
        }

        NodeBlurData &blur_data = *static_cast<NodeBlurData *>(node->storage);

        if (blur_data.filtertype != R_FILTER_FAST_GAUSS) {
          continue;
        }

        /* The size of the Fast Gaussian mode of blur decreased by the following factor to match
         * other blur sizes. So increase it back. */
        const float size_factor = 3.0f / 2.0f;
        blur_data.sizex *= size_factor;
        blur_data.sizey *= size_factor;
        blur_data.percentx *= size_factor;
        blur_data.percenty *= size_factor;
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 26)) {
    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "float", "shadow_resolution_scale"))
    {
      SceneEEVEE default_scene_eevee = *DNA_struct_default_get(SceneEEVEE);
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.shadow_resolution_scale = default_scene_eevee.shadow_resolution_scale;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 27)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        scene->ed->cache_flag &= ~(SEQ_CACHE_UNUSED_5 | SEQ_CACHE_UNUSED_6 | SEQ_CACHE_UNUSED_7 |
                                   SEQ_CACHE_UNUSED_8 | SEQ_CACHE_UNUSED_9);
      }
    }
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->cache_overlay.flag |= SEQ_CACHE_SHOW_FINAL_OUT;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 28)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        SEQ_for_each_callback(&scene->ed->seqbase, strip_proxies_timecode_update, nullptr);
      }
    }

    LISTBASE_FOREACH (MovieClip *, clip, &bmain->movieclips) {
      MovieClipProxy proxy = clip->proxy;
      versioning_update_timecode(&proxy.tc);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 29)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed) {
        SEQ_for_each_callback(&scene->ed->seqbase, strip_text_data_update, nullptr);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 30)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->nodetree) {
        scene->nodetree->flag &= ~NTREE_UNUSED_2;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 31)) {
    LISTBASE_FOREACH (LightProbe *, lightprobe, &bmain->lightprobes) {
      /* Guess a somewhat correct density given the resolution. But very low resolution need
       * a decent enough density to work. */
      lightprobe->grid_surfel_density = max_ii(20,
                                               2 * max_iii(lightprobe->grid_resolution_x,
                                                           lightprobe->grid_resolution_y,
                                                           lightprobe->grid_resolution_z));
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 31)) {
    bool only_uses_eevee_legacy_or_workbench = true;
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (!STR_ELEM(scene->r.engine, RE_engine_id_BLENDER_EEVEE, RE_engine_id_BLENDER_WORKBENCH)) {
        only_uses_eevee_legacy_or_workbench = false;
      }
    }
    /* Mark old EEVEE world volumes for showing conversion operator. */
    LISTBASE_FOREACH (World *, world, &bmain->worlds) {
      if (world->nodetree) {
        bNode *output_node = version_eevee_output_node_get(world->nodetree, SH_NODE_OUTPUT_WORLD);
        if (output_node) {
          bNodeSocket *volume_input_socket = static_cast<bNodeSocket *>(
              BLI_findlink(&output_node->inputs, 1));
          if (volume_input_socket) {
            LISTBASE_FOREACH (bNodeLink *, node_link, &world->nodetree->links) {
              if (node_link->tonode == output_node && node_link->tosock == volume_input_socket) {
                world->flag |= WO_USE_EEVEE_FINITE_VOLUME;
                /* Only display a warning message if we are sure this can be used by EEVEE. */
                if (only_uses_eevee_legacy_or_workbench) {
                  BLO_reportf_wrap(fd->reports,
                                   RPT_WARNING,
                                   RPT_("%s contains a volume shader that might need to be "
                                        "converted to object (see world volume panel)\n"),
                                   world->id.name + 2);
                }
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 33)) {
    constexpr int NTREE_EXECUTION_MODE_GPU = 2;

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->nodetree) {
        if (scene->nodetree->execution_mode == NTREE_EXECUTION_MODE_GPU) {
          scene->r.compositor_device = SCE_COMPOSITOR_DEVICE_GPU;
        }
        scene->r.compositor_precision = scene->nodetree->precision;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 34)) {
    float shadow_max_res_sun = 0.001f;
    float shadow_max_res_local = 0.001f;
    bool shadow_resolution_absolute = false;
    /* Try to get default resolution from scene setting. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      shadow_max_res_local = (2.0f * M_SQRT2) / scene->eevee.shadow_cube_size_deprecated;
      /* Round to avoid weird numbers in the UI. */
      shadow_max_res_local = ceil(shadow_max_res_local * 1000.0f) / 1000.0f;
      shadow_resolution_absolute = true;
      break;
    }

    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      if (light->type == LA_SUN) {
        /* Sun are too complex to convert. Need user interaction. */
        light->shadow_maximum_resolution = shadow_max_res_sun;
        SET_FLAG_FROM_TEST(light->mode, false, LA_SHAD_RES_ABSOLUTE);
      }
      else {
        light->shadow_maximum_resolution = shadow_max_res_local;
        SET_FLAG_FROM_TEST(light->mode, shadow_resolution_absolute, LA_SHAD_RES_ABSOLUTE);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 36)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      /* Only for grease pencil brushes. */
      if (brush->gpencil_settings) {
        /* Use the `Scene` radius unit by default (confusingly named `BRUSH_LOCK_SIZE`).
         * Convert the radius to be the same visual size as in GPv2. */
        brush->flag |= BRUSH_LOCK_SIZE;
        brush->unprojected_radius = brush->size *
                                    blender::bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 37)) {
    const World *default_world = DNA_struct_default_get(World);
    LISTBASE_FOREACH (World *, world, &bmain->worlds) {
      world->sun_threshold = default_world->sun_threshold;
      world->sun_angle = default_world->sun_angle;
      world->sun_shadow_maximum_resolution = default_world->sun_shadow_maximum_resolution;
      /* Having the sun extracted is mandatory to keep the same look and avoid too much light
       * leaking compared to EEVEE-Legacy. But adding shadows might create performance overhead and
       * change the result in a very different way. So we disable shadows in older file. */
      world->flag &= ~WO_USE_SUN_SHADOW;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 38)) {
    LISTBASE_FOREACH (GreasePencil *, grease_pencil, &bmain->grease_pencils) {
      convert_grease_pencil_stroke_hardness_to_softness(grease_pencil);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 39)) {
    /* Unify cast shadow property with Cycles. */
    if (!all_scenes_use(bmain, {RE_engine_id_BLENDER_EEVEE})) {
      const Light *default_light = DNA_struct_default_get(Light);
      LISTBASE_FOREACH (Light *, light, &bmain->lights) {
        IDProperty *clight = version_cycles_properties_from_ID(&light->id);
        if (clight) {
          bool value = version_cycles_property_boolean(
              clight, "cast_shadow", default_light->mode & LA_SHADOW);
          SET_FLAG_FROM_TEST(light->mode, value, LA_SHADOW);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 40)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      version_node_input_socket_name(ntree, FN_NODE_COMBINE_TRANSFORM, "Location", "Translation");
      version_node_output_socket_name(
          ntree, FN_NODE_SEPARATE_TRANSFORM, "Location", "Translation");
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 41)) {
    const Light *default_light = DNA_struct_default_get(Light);
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      light->shadow_jitter_overblur = default_light->shadow_jitter_overblur;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 43)) {
    const World *default_world = DNA_struct_default_get(World);
    LISTBASE_FOREACH (World *, world, &bmain->worlds) {
      world->sun_shadow_maximum_resolution = default_world->sun_shadow_maximum_resolution;
      world->sun_shadow_filter_radius = default_world->sun_shadow_filter_radius;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 44)) {
    const Scene *default_scene = DNA_struct_default_get(Scene);
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->eevee.fast_gi_step_count = default_scene->eevee.fast_gi_step_count;
      scene->eevee.fast_gi_ray_count = default_scene->eevee.fast_gi_ray_count;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 45)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = reinterpret_cast<View3D *>(sl);
            v3d->flag2 |= V3D_SHOW_CAMERA_GUIDES;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 46)) {
    const Scene *default_scene = DNA_struct_default_get(Scene);
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->eevee.fast_gi_thickness_near = default_scene->eevee.fast_gi_thickness_near;
      scene->eevee.fast_gi_thickness_far = default_scene->eevee.fast_gi_thickness_far;
    }
  }
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 48)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (!ob->pose) {
        continue;
      }
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        pchan->custom_shape_wire_width = 1.0;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 49)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = reinterpret_cast<View3D *>(sl);
            v3d->flag2 |= V3D_SHOW_CAMERA_PASSEPARTOUT;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 50)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != GEO_NODE_CAPTURE_ATTRIBUTE) {
          continue;
        }
        NodeGeometryAttributeCapture *storage = static_cast<NodeGeometryAttributeCapture *>(
            node->storage);
        if (storage->next_identifier > 0) {
          continue;
        }
        storage->capture_items_num = 1;
        storage->capture_items = MEM_cnew_array<NodeGeometryAttributeCaptureItem>(
            storage->capture_items_num, __func__);
        NodeGeometryAttributeCaptureItem &item = storage->capture_items[0];
        item.data_type = storage->data_type_legacy;
        item.identifier = storage->next_identifier++;
        item.name = BLI_strdup("Value");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 53)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_NODE) {
            SpaceNode *snode = reinterpret_cast<SpaceNode *>(sl);
            snode->overlay.flag |= SN_OVERLAY_SHOW_REROUTE_AUTO_LABELS;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 55)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_CURVE_RGB) {
          continue;
        }

        CurveMapping &curve_mapping = *static_cast<CurveMapping *>(node->storage);

        /* Film-like tone only works with the combined curve, which is the fourth curve, so make
         * the combined curve current, as we now hide the rest of the curves since they no longer
         * have an effect. */
        if (curve_mapping.tone == CURVE_TONE_FILMLIKE) {
          curve_mapping.cur = 3;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 2)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space_link, &area->spacedata) {
          if (space_link->spacetype == SPACE_NODE) {
            SpaceNode *space_node = reinterpret_cast<SpaceNode *>(space_link);
            space_node->flag &= ~SNODE_FLAG_UNUSED_5;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 60) ||
      (bmain->versionfile == 403 && !MAIN_VERSION_FILE_ATLEAST(bmain, 403, 3)))
  {
    /* Limit Rotation constraints from old files should use the legacy Limit
     * Rotation behavior. */
    LISTBASE_FOREACH (Object *, obj, &bmain->objects) {
      LISTBASE_FOREACH (bConstraint *, constraint, &obj->constraints) {
        if (constraint->type != CONSTRAINT_TYPE_ROTLIMIT) {
          continue;
        }
        static_cast<bRotLimitConstraint *>(constraint->data)->flag |= LIMIT_ROT_LEGACY_BEHAVIOR;
      }

      if (!obj->pose) {
        continue;
      }
      LISTBASE_FOREACH (bPoseChannel *, pbone, &obj->pose->chanbase) {
        LISTBASE_FOREACH (bConstraint *, constraint, &pbone->constraints) {
          if (constraint->type != CONSTRAINT_TYPE_ROTLIMIT) {
            continue;
          }
          static_cast<bRotLimitConstraint *>(constraint->data)->flag |= LIMIT_ROT_LEGACY_BEHAVIOR;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 61)) {
    /* LIGHT_PROBE_RESOLUTION_64 has been removed in EEVEE-Next as the tedrahedral mapping is to
     * low res to be usable. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->eevee.gi_cubemap_resolution = std::max(scene->eevee.gi_cubemap_resolution, 128);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 64)) {
    if (all_scenes_use(bmain, {RE_engine_id_BLENDER_EEVEE})) {
      /* Re-apply versioning made for EEVEE-Next in 4.1 before it got delayed. */
      LISTBASE_FOREACH (Material *, material, &bmain->materials) {
        bool transparent_shadows = material->blend_shadow != MA_BS_SOLID;
        SET_FLAG_FROM_TEST(material->blend_flag, transparent_shadows, MA_BL_TRANSPARENT_SHADOW);
      }
      LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
        mat->surface_render_method = (mat->blend_method == MA_BM_BLEND) ?
                                         MA_SURFACE_METHOD_FORWARD :
                                         MA_SURFACE_METHOD_DEFERRED;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 3)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (BrushGpencilSettings *settings = brush->gpencil_settings) {
        /* Copy the `draw_strength` value to the `alpha` value. */
        brush->alpha = settings->draw_strength;

        /* We approximate the simplify pixel threshold by taking the previous threshold (world
         * space) and dividing by the legacy radius conversion factor. This should generally give
         * reasonable "pixel" threshold values, at least for previous GPv2 defaults. */
        settings->simplify_px = settings->simplify_f /
                                blender::bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR * 0.1f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 4)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->view_settings.temperature = 6500.0f;
      scene->view_settings.tint = 10.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 7)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      SequencerToolSettings *sequencer_tool_settings = SEQ_tool_settings_ensure(scene);
      sequencer_tool_settings->snap_mode |= SEQ_SNAP_TO_PREVIEW_BORDERS |
                                            SEQ_SNAP_TO_PREVIEW_CENTER |
                                            SEQ_SNAP_TO_STRIPS_PREVIEW;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 8)) {
    update_paint_modes_for_brush_assets(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 9)) {
    fix_built_in_curve_attribute_defaults(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 10)) {
    /* Initialize Color Balance node white point settings. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_CUSTOM) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == CMP_NODE_COLORBALANCE) {
            NodeColorBalance *n = static_cast<NodeColorBalance *>(node->storage);
            n->input_temperature = n->output_temperature = 6500.0f;
            n->input_tint = n->output_tint = 10.0f;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 11)) {
    LISTBASE_FOREACH (Curves *, curves, &bmain->hair_curves) {
      curves->geometry.attributes_active_index = curves->attributes_active_index_legacy;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 13)) {
    Camera default_cam = *DNA_struct_default_get(Camera);
    LISTBASE_FOREACH (Camera *, camera, &bmain->cameras) {
      camera->central_cylindrical_range_u_min = default_cam.central_cylindrical_range_u_min;
      camera->central_cylindrical_range_u_max = default_cam.central_cylindrical_range_u_max;
      camera->central_cylindrical_range_v_min = default_cam.central_cylindrical_range_v_min;
      camera->central_cylindrical_range_v_max = default_cam.central_cylindrical_range_v_max;
      camera->central_cylindrical_radius = default_cam.central_cylindrical_radius;
    }
  }

  /* The File Output node now uses the linear color space setting of its stored image formats. So
   * we need to ensure the color space value is initialized to some sane default based on the image
   * type. Furthermore, the node now gained a new Save As Render option that is global to the node,
   * which will be used if Use Node Format is enabled for each input, so we potentially need to
   * disable Use Node Format in case inputs had different Save As render options. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 14)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }

      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_OUTPUT_FILE) {
          continue;
        }

        /* Initialize node format color space if it is not set. */
        NodeImageMultiFile *storage = static_cast<NodeImageMultiFile *>(node->storage);
        if (storage->format.linear_colorspace_settings.name[0] == '\0') {
          BKE_image_format_update_color_space_for_type(&storage->format);
        }

        if (BLI_listbase_is_empty(&node->inputs)) {
          continue;
        }

        /* Initialize input formats color space if it is not set. */
        LISTBASE_FOREACH (const bNodeSocket *, input, &node->inputs) {
          NodeImageMultiFileSocket *input_storage = static_cast<NodeImageMultiFileSocket *>(
              input->storage);
          if (input_storage->format.linear_colorspace_settings.name[0] == '\0') {
            BKE_image_format_update_color_space_for_type(&input_storage->format);
          }
        }

        /* EXR images don't use Save As Render. */
        if (ELEM(storage->format.imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
          continue;
        }

        /* Find out if all inputs have the same Save As Render option. */
        const bNodeSocket *first_input = static_cast<bNodeSocket *>(node->inputs.first);
        const NodeImageMultiFileSocket *first_input_storage =
            static_cast<NodeImageMultiFileSocket *>(first_input->storage);
        const bool first_save_as_render = first_input_storage->save_as_render;
        bool all_inputs_have_same_save_as_render = true;
        LISTBASE_FOREACH (const bNodeSocket *, input, &node->inputs) {
          const NodeImageMultiFileSocket *input_storage = static_cast<NodeImageMultiFileSocket *>(
              input->storage);
          if (bool(input_storage->save_as_render) != first_save_as_render) {
            all_inputs_have_same_save_as_render = false;
            break;
          }
        }

        /* All inputs have the same save as render option, so we set the node Save As Render option
         * to that value, and we leave inputs as is. */
        if (all_inputs_have_same_save_as_render) {
          storage->save_as_render = first_save_as_render;
          continue;
        }

        /* For inputs that have Use Node Format enabled, we need to disabled it because otherwise
         * they will use the node's Save As Render option. It follows that we need to copy the
         * node's format to the input format. */
        LISTBASE_FOREACH (const bNodeSocket *, input, &node->inputs) {
          NodeImageMultiFileSocket *input_storage = static_cast<NodeImageMultiFileSocket *>(
              input->storage);

          if (!input_storage->use_node_format) {
            continue;
          }

          input_storage->use_node_format = false;
          input_storage->format = storage->format;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 15)) {
    using namespace blender;

    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      const ListBase *exporters = &collection->exporters;
      LISTBASE_FOREACH (CollectionExport *, data, exporters) {
        /* The name field should be empty at this point. */
        BLI_assert(data->name[0] == '\0');

        bke::FileHandlerType *fh = bke::file_handler_find(data->fh_idname);
        BKE_collection_exporter_name_set(exporters, data, fh ? fh->label : DATA_("Undefined"));
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 16)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->eevee.flag |= SCE_EEVEE_FAST_GI_ENABLED;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 17)) {
    FOREACH_NODETREE_BEGIN (bmain, tree, id) {
      if (tree->default_group_node_width == 0) {
        tree->default_group_node_width = GROUP_NODE_DEFAULT_WIDTH;
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 20)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_TOOLS);
            if (region != nullptr) {
              region->flag &= ~RGN_FLAG_HIDDEN;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 21)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_CLIP) {
            ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
            if (region != nullptr) {
              View2D *v2d = &region->v2d;
              v2d->flag &= ~V2D_VIEWSYNC_SCREEN_TIME;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 22)) {
    add_bevel_modifier_attribute_name_defaults(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 23)) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type != eModifierType_Nodes) {
          continue;
        }
        NodesModifierData &nmd = *reinterpret_cast<NodesModifierData *>(md);
        if (nmd.bake_target == NODES_MODIFIER_BAKE_TARGET_INHERIT) {
          /* Use disk target for existing modifiers to avoid changing behavior. */
          nmd.bake_target = NODES_MODIFIER_BAKE_TARGET_DISK;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 24)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      node_reroute_add_storage(*ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 26)) {
    hide_simulation_node_skip_socket_value(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 28)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = reinterpret_cast<View3D *>(sl);
            copy_v3_fl(v3d->overlay.gpencil_grid_color, 0.5f);
            copy_v2_fl(v3d->overlay.gpencil_grid_scale, 1.0f);
            copy_v2_fl(v3d->overlay.gpencil_grid_offset, 0.0f);
            v3d->overlay.gpencil_grid_subdivisions = 4;
          }
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
        if (ELEM(node->type_legacy, CMP_NODE_VIEWER, CMP_NODE_COMPOSITE)) {
          node->flag &= ~NODE_PREVIEW;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 29)) {
    /* Open warnings panel by default. */
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Nodes) {
          md->layout_panel_open_flag |= 1 << NODES_MODIFIER_PANEL_WARNINGS;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 31)) {
    LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
      LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
        if (tref->space_type != SPACE_SEQ) {
          continue;
        }
        STRNCPY(tref->idname, "builtin.select_box");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 1)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = SEQ_editing_get(scene);
      if (ed != nullptr) {
        SEQ_for_each_callback(&ed->seqbase, versioning_convert_seq_text_anchor, nullptr);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 4)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_FILE) {
            continue;
          }
          SpaceFile *sfile = reinterpret_cast<SpaceFile *>(sl);
          if (sfile->asset_params) {
            sfile->asset_params->base_params.sort = FILE_SORT_ASSET_CATALOG;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 6)) {
    add_subsurf_node_limit_surface_option(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 8)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        remove_triangulate_node_min_size_input(ntree);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 10)) {
    LISTBASE_FOREACH (bAction *, dna_action, &bmain->actions) {
      blender::animrig::Action &action = dna_action->wrap();
      blender::animrig::foreach_fcurve_in_action(
          action, [&](FCurve &fcurve) { version_fcurve_noise_modifier(fcurve); });
    }

    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      AnimData *adt = BKE_animdata_from_id(id);
      if (!adt) {
        continue;
      }

      LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
        version_fcurve_noise_modifier(*fcu);
      }
    }
    FOREACH_MAIN_ID_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 11)) {
    /* #update_paint_modes_for_brush_assets() didn't handle image editor tools for some time. 4.3
     * files saved during that period could have invalid tool references stored. */
    LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
      LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
        if (tref->space_type == SPACE_IMAGE && tref->mode == SI_MODE_PAINT) {
          STRNCPY(tref->idname, "builtin.brush");
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 12)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      version_node_locations_to_global(*ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 13)) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, modifier, &object->modifiers) {
        if (modifier->type != eModifierType_Nodes) {
          continue;
        }
        NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(modifier);
        if (!nmd->settings.properties) {
          continue;
        }
        LISTBASE_FOREACH (IDProperty *, idprop, &nmd->settings.properties->data.group) {
          if (idprop->type != IDP_STRING) {
            continue;
          }
          blender::StringRef prop_name(idprop->name);
          if (prop_name.endswith("_attribute_name") || prop_name.endswith("_use_attribute")) {
            idprop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY | IDP_FLAG_STATIC_TYPE;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 14)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      version_group_input_socket_data_block_reference(*ntree);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 15)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = SEQ_editing_get(scene);
      if (ed != nullptr) {
        SEQ_for_each_callback(&ed->seqbase, versioning_clear_strip_unused_flag, scene);
      }
    }
  }

  /* Fix incorrect identifier in the shader mix node. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 16)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == SH_NODE_MIX_SHADER) {
            LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
              if (STREQ(socket->identifier, "Shader.001")) {
                STRNCPY(socket->identifier, "Shader_001");
              }
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 17)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "RenderData", "RenderSettings", "compositor_denoise_preview_quality"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->r.compositor_denoise_preview_quality = SCE_COMPOSITOR_DENOISE_BALANCED;
      }
    }
    if (!DNA_struct_member_exists(
            fd->filesdna, "RenderData", "RenderSettings", "compositor_denoise_final_quality"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->r.compositor_denoise_final_quality = SCE_COMPOSITOR_DENOISE_HIGH;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 22)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      IDProperty *cscene = version_cycles_properties_from_ID(&scene->id);
      if (cscene) {
        if (version_cycles_property_int(cscene, "sample_offset", 0) > 0) {
          version_cycles_property_boolean_set(cscene, "use_sample_subset", true);
          version_cycles_property_int_set(cscene, "sample_subset_length", (1 << 24));
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 23)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Curves", "float", "surface_collision_distance")) {
      LISTBASE_FOREACH (Curves *, curves, &bmain->hair_curves) {
        curves->surface_collision_distance = 0.005f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 24)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      version_geometry_normal_input_node(*ntree);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 26)) {
    const Brush *default_brush = DNA_struct_default_get(Brush);
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if ((brush->mask_stencil_dimension[0] == 0) && (brush->mask_stencil_dimension[1] == 0)) {
        brush->mask_stencil_dimension[0] = default_brush->mask_stencil_dimension[0];
        brush->mask_stencil_dimension[1] = default_brush->mask_stencil_dimension[1];
      }
      if ((brush->mask_stencil_pos[0] == 0) && (brush->mask_stencil_pos[1] == 0)) {
        brush->mask_stencil_pos[0] = default_brush->mask_stencil_pos[0];
        brush->mask_stencil_pos[1] = default_brush->mask_stencil_pos[1];
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 27)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_version_viewer_shortcut(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 28)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      SequencerToolSettings *sequencer_tool_settings = SEQ_tool_settings_ensure(scene);
      sequencer_tool_settings->snap_mode |= SEQ_SNAP_TO_RETIMING;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 29)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      ts->imapaint.clone_alpha = 0.5f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 30)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (ELEM(sl->spacetype, SPACE_ACTION, SPACE_INFO, SPACE_CONSOLE)) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_WINDOW) {
                region->v2d.scroll |= V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
              }
            }
          }
        }
      }
    }
  }

  /* Always run this versioning; meshes are written with the legacy format which always needs to
   * be converted to the new format on file load. Can be moved to a subversion check in a larger
   * breaking release. */
  LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
    blender::bke::mesh_sculpt_mask_to_generic(*mesh);
    blender::bke::mesh_custom_normals_to_generic(*mesh);
    rename_mesh_uv_seam_attribute(*mesh);
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}
