/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_sys_types.h"

#include "BKE_asset.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_customdata.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_tracking.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"

namespace blender {

// static CLG_LogRef LOG = {"blend.doversion"};

/* The Mix mode of the Mix node previously assumed the alpha of the first input as opposed to
 * mixing the alpha as well. So we add a separate color node to get the alpha of the first input
 * and set it to the result using a set alpha node. */
static void do_version_mix_node_mix_mode_compositor(bNodeTree &node_tree, bNode &node)
{
  const NodeShaderMix *data = reinterpret_cast<NodeShaderMix *>(node.storage);
  if (data->data_type != SOCK_RGBA) {
    return;
  }

  if (data->blend_type != MA_RAMP_BLEND) {
    return;
  }

  bNodeSocket *first_input = bke::node_find_socket(node, SOCK_IN, "A_Color");
  bNodeSocket *output = bke::node_find_socket(node, SOCK_OUT, "Result_Color");

  /* Find the link going into the inputs of the node. */
  bNodeLink *first_link = nullptr;
  for (bNodeLink &link : node_tree.links) {
    if (link.tosock == first_input) {
      first_link = &link;
    }
  }

  bNode &separate_node = version_node_add_empty(node_tree, "CompositorNodeSeparateColor");
  separate_node.parent = node.parent;
  separate_node.location[0] = node.location[0] - 10.0f;
  separate_node.location[1] = node.location[1];
  NodeCMPCombSepColor *storage = MEM_new<NodeCMPCombSepColor>(__func__);
  storage->mode = CMP_NODE_COMBSEP_COLOR_RGB;
  separate_node.storage = storage;

  bNodeSocket &separate_input = version_node_add_socket(
      node_tree, separate_node, SOCK_IN, "NodeSocketColor", "Image");
  bNodeSocket &separate_alpha_output = version_node_add_socket(
      node_tree, separate_node, SOCK_OUT, "NodeSocketFloat", "Alpha");

  copy_v4_v4(separate_input.default_value_typed<bNodeSocketValueRGBA>()->value,
             first_input->default_value_typed<bNodeSocketValueRGBA>()->value);
  if (first_link) {
    version_node_add_link(
        node_tree, *first_link->fromnode, *first_link->fromsock, separate_node, separate_input);
  }

  bNode &set_alpha_node = version_node_add_empty(node_tree, "CompositorNodeSetAlpha");
  set_alpha_node.parent = node.parent;
  set_alpha_node.location[0] = node.location[0] - 10.0f;
  set_alpha_node.location[1] = node.location[1];
  set_alpha_node.storage = MEM_new<NodeCMPCombSepColor>(__func__);

  bNodeSocket &set_alpha_image_input = version_node_add_socket(
      node_tree, set_alpha_node, SOCK_IN, "NodeSocketColor", "Image");
  bNodeSocket &set_alpha_alpha_input = version_node_add_socket(
      node_tree, set_alpha_node, SOCK_IN, "NodeSocketFloat", "Alpha");
  bNodeSocket &set_alpha_type_input = version_node_add_socket(
      node_tree, set_alpha_node, SOCK_IN, "NodeSocketMenu", "Type");
  bNodeSocket &set_alpha_output = version_node_add_socket(
      node_tree, set_alpha_node, SOCK_OUT, "NodeSocketColor", "Image");

  set_alpha_type_input.default_value_typed<bNodeSocketValueMenu>()->value =
      CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA;
  version_node_add_link(node_tree, node, *output, set_alpha_node, set_alpha_image_input);
  version_node_add_link(
      node_tree, separate_node, separate_alpha_output, set_alpha_node, set_alpha_alpha_input);

  for (bNodeLink &link : node_tree.links.items_reversed_mutable()) {
    if (link.fromsock == output && link.tonode != &set_alpha_node) {
      version_node_add_link(
          node_tree, set_alpha_node, set_alpha_output, *link.tonode, *link.tosock);
      bke::node_remove_link(&node_tree, link);
    }
  }
}

/* The Mix mode of the Mix node previously assumed the alpha of the first input as opposed to
 * mixing the alpha as well. So we add a separate color node to get the alpha of the first input
 * and set it to the result using a pair of separate and combine color nodes. */
static void do_version_mix_node_mix_mode_geometry(bNodeTree &node_tree, bNode &node)
{
  const NodeShaderMix *data = reinterpret_cast<NodeShaderMix *>(node.storage);
  if (data->data_type != SOCK_RGBA) {
    return;
  }

  if (data->blend_type != MA_RAMP_BLEND) {
    return;
  }

  bNodeSocket *first_input = bke::node_find_socket(node, SOCK_IN, "A_Color");
  bNodeSocket *output = bke::node_find_socket(node, SOCK_OUT, "Result_Color");

  /* Find the link going into the inputs of the node. */
  bNodeLink *first_link = nullptr;
  for (bNodeLink &link : node_tree.links) {
    if (link.tosock == first_input) {
      first_link = &link;
    }
  }

  bNode &separate_alpha_node = version_node_add_empty(node_tree, "FunctionNodeSeparateColor");
  separate_alpha_node.parent = node.parent;
  separate_alpha_node.location[0] = node.location[0] - 10.0f;
  separate_alpha_node.location[1] = node.location[1];
  NodeCombSepColor *separate_alpha_storage = MEM_new<NodeCombSepColor>(__func__);
  separate_alpha_storage->mode = NODE_COMBSEP_COLOR_RGB;
  separate_alpha_node.storage = separate_alpha_storage;

  bNodeSocket &separate_alpha_input = version_node_add_socket(
      node_tree, separate_alpha_node, SOCK_IN, "NodeSocketColor", "Color");
  bNodeSocket &separate_alpha_output = version_node_add_socket(
      node_tree, separate_alpha_node, SOCK_OUT, "NodeSocketFloat", "Alpha");

  copy_v4_v4(separate_alpha_input.default_value_typed<bNodeSocketValueRGBA>()->value,
             first_input->default_value_typed<bNodeSocketValueRGBA>()->value);
  if (first_link) {
    version_node_add_link(node_tree,
                          *first_link->fromnode,
                          *first_link->fromsock,
                          separate_alpha_node,
                          separate_alpha_input);
  }

  bNode &separate_color_node = version_node_add_empty(node_tree, "FunctionNodeSeparateColor");
  separate_color_node.parent = node.parent;
  separate_color_node.location[0] = node.location[0] - 10.0f;
  separate_color_node.location[1] = node.location[1];
  NodeCombSepColor *separate_color_storage = MEM_new<NodeCombSepColor>(__func__);
  separate_color_storage->mode = NODE_COMBSEP_COLOR_RGB;
  separate_color_node.storage = separate_color_storage;

  bNodeSocket &separate_color_input = version_node_add_socket(
      node_tree, separate_color_node, SOCK_IN, "NodeSocketColor", "Color");
  bNodeSocket &separate_color_red_output = version_node_add_socket(
      node_tree, separate_color_node, SOCK_OUT, "NodeSocketFloat", "Red");
  bNodeSocket &separate_color_green_output = version_node_add_socket(
      node_tree, separate_color_node, SOCK_OUT, "NodeSocketFloat", "Green");
  bNodeSocket &separate_color_blue_output = version_node_add_socket(
      node_tree, separate_color_node, SOCK_OUT, "NodeSocketFloat", "Blue");

  version_node_add_link(node_tree, node, *output, separate_color_node, separate_color_input);

  bNode &combine_color_node = version_node_add_empty(node_tree, "FunctionNodeCombineColor");
  combine_color_node.parent = node.parent;
  combine_color_node.location[0] = node.location[0] - 10.0f;
  combine_color_node.location[1] = node.location[1];
  NodeCombSepColor *combine_color_storage = MEM_new<NodeCombSepColor>(__func__);
  combine_color_storage->mode = NODE_COMBSEP_COLOR_RGB;
  combine_color_node.storage = combine_color_storage;

  bNodeSocket &combine_color_red_input = version_node_add_socket(
      node_tree, combine_color_node, SOCK_IN, "NodeSocketFloat", "Red");
  bNodeSocket &combine_color_green_input = version_node_add_socket(
      node_tree, combine_color_node, SOCK_IN, "NodeSocketFloat", "Green");
  bNodeSocket &combine_color_blue_input = version_node_add_socket(
      node_tree, combine_color_node, SOCK_IN, "NodeSocketFloat", "Blue");
  bNodeSocket &combine_color_alpha_input = version_node_add_socket(
      node_tree, combine_color_node, SOCK_IN, "NodeSocketFloat", "Alpha");
  bNodeSocket &combine_color_output = version_node_add_socket(
      node_tree, combine_color_node, SOCK_OUT, "NodeSocketColor", "Color");

  version_node_add_link(node_tree,
                        separate_color_node,
                        separate_color_red_output,
                        combine_color_node,
                        combine_color_red_input);
  version_node_add_link(node_tree,
                        separate_color_node,
                        separate_color_green_output,
                        combine_color_node,
                        combine_color_green_input);
  version_node_add_link(node_tree,
                        separate_color_node,
                        separate_color_blue_output,
                        combine_color_node,
                        combine_color_blue_input);
  version_node_add_link(node_tree,
                        separate_alpha_node,
                        separate_alpha_output,
                        combine_color_node,
                        combine_color_alpha_input);

  for (bNodeLink &link : node_tree.links.items_reversed_mutable()) {
    if (link.fromsock == output && link.tonode != &separate_color_node) {
      version_node_add_link(
          node_tree, combine_color_node, combine_color_output, *link.tonode, *link.tosock);
      bke::node_remove_link(&node_tree, link);
    }
  }
}

static void init_node_tool_operator_idnames(Main &bmain)
{
  for (bNodeTree &group : bmain.nodetrees) {
    if (group.type != NTREE_GEOMETRY) {
      continue;
    }
    if (!group.geometry_node_asset_traits) {
      continue;
    }
    if (group.geometry_node_asset_traits->node_tool_idname) {
      continue;
    }
    std::string name_str = "geometry.";
    for (char c : StringRef(BKE_id_name(group.id))) {
      c = tolower(c);
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
        name_str.push_back(c);
      }
      else {
        const bool last_is_underscore = name_str[name_str.size() - 1] == '_';
        if (!last_is_underscore) {
          name_str.push_back('_');
        }
      }
    }
    group.geometry_node_asset_traits->node_tool_idname = BLI_strdupn(name_str.c_str(),
                                                                     name_str.size());
    if (group.id.asset_data) {
      auto property = bke::idprop::create(
          "node_tool_idname", StringRefNull(group.geometry_node_asset_traits->node_tool_idname));
      BKE_asset_metadata_idprop_ensure(group.id.asset_data, property.release());
    }
  }
}

static void version_realize_instances_to_curve_domain(Main &bmain)
{
  for (bNodeTree &node_tree : bmain.nodetrees) {
    if (node_tree.type != NTREE_GEOMETRY) {
      continue;
    }
    for (bNode &node : node_tree.nodes) {
      if (node.type_legacy != GEO_NODE_REALIZE_INSTANCES) {
        continue;
      }
      node.custom1 |= GEO_NODE_REALIZE_TO_POINT_DOMAIN;
    }
  }
}

static void version_mesh_uv_map_strings(Main &bmain)
{
  for (Mesh &mesh : bmain.meshes) {
    const CustomData *data = &mesh.corner_data;
    if (!mesh.active_uv_map_attribute) {
      if (const char *name = CustomData_get_active_layer_name(data, CD_PROP_FLOAT2)) {
        mesh.active_uv_map_attribute = BLI_strdup(name);
      }
    }
    if (!mesh.default_uv_map_attribute) {
      if (const char *name = CustomData_get_render_layer_name(data, CD_PROP_FLOAT2)) {
        mesh.default_uv_map_attribute = BLI_strdup(name);
      }
    }
  }
}

static void version_clear_unused_strip_flags(Main &bmain)
{
  for (Scene &scene : bmain.scenes) {
    Editing *ed = seq::editing_get(&scene);
    if (ed != nullptr) {
      seq::foreach_strip(&ed->seqbase, [&](Strip *strip) {
        constexpr int flag_overlap = 1 << 3;
        constexpr int flag_ipo_frame_locked = 1 << 8;
        constexpr int flag_effect_not_loaded = 1 << 9;
        constexpr int flag_delete = 1 << 10;
        constexpr int flag_ignore_channel_lock = 1 << 16;
        constexpr int flag_show_offsets = 1 << 20;
        strip->flag &= ~(flag_overlap | flag_ipo_frame_locked | flag_effect_not_loaded |
                         flag_delete | flag_ignore_channel_lock | flag_show_offsets);
        return true;
      });
    }
  }
}

static void version_string_to_curves_node_inputs(bNodeTree &tree, bNode &node)
{
  if (!node.storage) {
    return;
  }
  auto &storage = *reinterpret_cast<NodeGeometryStringToCurves *>(node.storage);
  if (!blender::bke::node_find_socket(node, SOCK_IN, "Font")) {
    bNodeSocket &socket = version_node_add_socket(tree, node, SOCK_IN, "NodeSocketFont", "Font");
    socket.default_value_typed<bNodeSocketValueFont>()->value = reinterpret_cast<VFont *>(node.id);
    node.id = nullptr;
  }
  if (!blender::bke::node_find_socket(node, SOCK_IN, "Overflow")) {
    bNodeSocket &socket = version_node_add_socket(
        tree, node, SOCK_IN, "NodeSocketMenu", "Overflow");
    socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.overflow;
  }
  if (!blender::bke::node_find_socket(node, SOCK_IN, "Align X")) {
    bNodeSocket &socket = version_node_add_socket(
        tree, node, SOCK_IN, "NodeSocketMenu", "Align X");
    socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.align_x;
  }
  if (!blender::bke::node_find_socket(node, SOCK_IN, "Align Y")) {
    bNodeSocket &socket = version_node_add_socket(
        tree, node, SOCK_IN, "NodeSocketMenu", "Align Y");
    socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.align_y;
  }
  if (!blender::bke::node_find_socket(node, SOCK_IN, "Pivot Point")) {
    bNodeSocket &socket = version_node_add_socket(
        tree, node, SOCK_IN, "NodeSocketMenu", "Pivot Point");
    socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.pivot_mode;
  }
}

static const char *legacy_pass_name_to_new_name(const char *name)
{
  if (STREQ(name, "DiffDir")) {
    return "Diffuse Direct";
  }
  if (STREQ(name, "DiffInd")) {
    return "Diffuse Indirect";
  }
  if (STREQ(name, "DiffCol")) {
    return "Diffuse Color";
  }
  if (STREQ(name, "GlossDir")) {
    return "Glossy Direct";
  }
  if (STREQ(name, "GlossInd")) {
    return "Glossy Indirect";
  }
  if (STREQ(name, "GlossCol")) {
    return "Glossy Color";
  }
  if (STREQ(name, "TransDir")) {
    return "Transmission Direct";
  }
  if (STREQ(name, "TransInd")) {
    return "Transmission Indirect";
  }
  if (STREQ(name, "TransCol")) {
    return "Transmission Color";
  }
  if (STREQ(name, "VolumeDir")) {
    return "Volume Direct";
  }
  if (STREQ(name, "VolumeInd")) {
    return "Volume Indirect";
  }
  if (STREQ(name, "VolumeCol")) {
    return "Volume Color";
  }
  if (STREQ(name, "AO")) {
    return "Ambient Occlusion";
  }
  if (STREQ(name, "Env")) {
    return "Environment";
  }
  if (STREQ(name, "IndexMA")) {
    return "Material Index";
  }
  if (STREQ(name, "IndexOB")) {
    return "Object Index";
  }
  if (STREQ(name, "GreasePencil")) {
    return "Grease Pencil";
  }
  if (STREQ(name, "Emit")) {
    return "Emission";
  }
  if (STREQ(name, "Z")) {
    return "Depth";
  }
  if (STREQ(name, "Speed")) {
    return "Vector";
  }

  return name;
}

static void do_version_light_remove_use_nodes(Main *bmain, Light *light)
{
  if (light->use_nodes) {
    return;
  }

  /* Users defined a light node tree, but deactivated it by disabling "Use Nodes". So we
   * simulate the same effect by creating a new Light Output node and setting it to active. */
  bNodeTree *ntree = light->nodetree;
  if (ntree == nullptr) {
    /* In case the light was defined through Python API it might have been missing a node tree. */
    ntree = bke::node_tree_add_tree_embedded(
        bmain, &light->id, "Light Node Tree Versioning", "ShaderNodeTree");
  }

  bNode *old_output = nullptr;
  for (bNode &node : ntree->nodes) {
    if (STREQ(node.idname, "ShaderNodeOutputLight") && (node.flag & NODE_DO_OUTPUT)) {
      old_output = &node;
      old_output->flag &= ~NODE_DO_OUTPUT;
    }
  }

  bNode &new_output = version_node_add_empty(*ntree, "ShaderNodeOutputLight");
  bNodeSocket &output_surface_input = version_node_add_socket(
      *ntree, new_output, SOCK_IN, "NodeSocketShader", "Surface");
  new_output.flag |= NODE_DO_OUTPUT;

  bNode &emission = version_node_add_empty(*ntree, "ShaderNodeEmission");
  bNodeSocket &emission_color_input = version_node_add_socket(
      *ntree, emission, SOCK_IN, "NodeSocketColor", "Color");
  bNodeSocket &emission_strength_input = version_node_add_socket(
      *ntree, emission, SOCK_IN, "NodeSocketFloat", "Strength");
  bNodeSocket &emission_output = version_node_add_socket(
      *ntree, emission, SOCK_OUT, "NodeSocketShader", "Emission");

  version_node_add_link(*ntree, emission, emission_output, new_output, output_surface_input);

  bNodeSocketValueRGBA *rgba = emission_color_input.default_value_typed<bNodeSocketValueRGBA>();
  rgba->value[0] = 1.0f;
  rgba->value[1] = 1.0f;
  rgba->value[2] = 1.0f;
  rgba->value[3] = 1.0f;
  emission_strength_input.default_value_typed<bNodeSocketValueFloat>()->value = 1.0f;

  if (old_output != nullptr) {
    /* Position the newly created node after the old output. Assume the old output node is at
     * the far right of the node tree. */
    emission.location[0] = old_output->location[0] + 1.5f * old_output->width;
    emission.location[1] = old_output->location[1];
  }
  else {
    /* Use default position, see #node_tree_shader_default() */
    emission.location[0] = -200.0f;
    emission.location[1] = 100.0f;
  }

  new_output.location[0] = emission.location[0] + 2.0f * emission.width;
  new_output.location[1] = emission.location[1];
}

/* For cycles, the Denoising Albedo render pass is now registered after the Denoising Normal pass
 * to match the compositor Denoise node. So we swap the order of Denoising Albedo and Denoising
 * Normal sockets in the Render Layers node that has been saved with the old order. */
static void do_version_render_layers_node_albedo_normal_swap(bNode &node)
{
  bNodeSocket *socket_denoise_normal = nullptr;
  bNodeSocket *socket_denoise_albedo = nullptr;
  for (bNodeSocket &socket : node.outputs) {
    if (STREQ(socket.identifier, "Denoising Normal")) {
      socket_denoise_normal = &socket;
    }
    if (STREQ(socket.identifier, "Denoising Albedo")) {
      socket_denoise_albedo = &socket;
    }
  }
  if (socket_denoise_albedo && socket_denoise_normal) {
    BLI_listbase_swaplinks(&node.outputs, socket_denoise_normal, socket_denoise_albedo);
  }
}

void do_versions_after_linking_510(FileData * /*fd*/, Main *bmain)
{
  /* Some blend files were saved with an invalid active viewer key, possibly due to a bug that was
   * fixed already in c8cb24121f, but blend files were never updated. So starting in 5.1, we fix
   * those files by essentially doing what ED_node_set_active_viewer_key is supposed to do at load
   * time during versioning. Note that the invalid active viewer will just cause a harmless assert,
   * so this does not need to exist in previous releases. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 0)) {
    for (bScreen &screen : bmain->screens) {
      for (ScrArea &area : screen.areabase) {
        for (SpaceLink &space : area.spacedata) {
          if (space.spacetype == SPACE_NODE) {
            SpaceNode *space_node = reinterpret_cast<SpaceNode *>(&space);
            bNodeTreePath *path = static_cast<bNodeTreePath *>(space_node->treepath.last);
            if (space_node->nodetree && path) {
              space_node->nodetree->active_viewer_key = path->parent_key;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 0)) {
    version_clear_unused_strip_flags(*bmain);
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

void blo_do_versions_510(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 1)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        for (bNode &node : node_tree->nodes) {
          if (node.type_legacy == SH_NODE_MIX) {
            do_version_mix_node_mix_mode_compositor(*node_tree, node);
          }
        }
      }
      else if (node_tree->type == NTREE_GEOMETRY) {
        for (bNode &node : node_tree->nodes) {
          if (node.type_legacy == SH_NODE_MIX) {
            do_version_mix_node_mix_mode_geometry(*node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 5)) {
    version_realize_instances_to_curve_domain(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 7)) {
    version_mesh_uv_map_strings(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 8)) {
    for (Object &obj : bmain->objects) {
      if (!obj.pose) {
        continue;
      }
      for (bPoseChannel &pose_bone : obj.pose->chanbase) {
        /* Those flags were previously unused, so to be safe we clear them. */
        pose_bone.flag &= ~(POSE_SELECTED_ROOT | POSE_SELECTED_TIP);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 9)) {
    init_node_tool_operator_idnames(*bmain);

    for (Scene &scene : bmain->scenes) {
      scene.r.ffcodecdata.custom_constant_rate_factor = 23;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 10)) {
    for (wmWindowManager &wm : bmain->wm) {
      wm.xr.session_settings.view_scale = 1.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 12)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        version_node_input_socket_name(node_tree, CMP_NODE_CRYPTOMATTE_LEGACY, "image", "Image");
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 13)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        for (bNode &node : node_tree->nodes) {
          if (node.type_legacy == CMP_NODE_R_LAYERS) {
            for (bNodeSocket &socket : node.outputs) {
              const char *new_pass_name = legacy_pass_name_to_new_name(socket.name);
              STRNCPY(socket.name, new_pass_name);
              const char *new_pass_identifier = legacy_pass_name_to_new_name(socket.identifier);
              STRNCPY(socket.identifier, new_pass_identifier);
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 14)) {
    for (bScreen &screen : bmain->screens) {
      for (ScrArea &area : screen.areabase) {
        for (SpaceLink &sl : area.spacedata) {
          if (sl.spacetype == SPACE_IMAGE) {
            SpaceImage *sima = reinterpret_cast<SpaceImage *>(&sl);
            sima->uv_edge_opacity = sima->uv_opacity;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 16)) {
    for (Scene &scene : bmain->scenes) {
      if (scene.toolsettings) {
        scene.toolsettings->anim_mirror_object = nullptr;
        scene.toolsettings->anim_relative_object = nullptr;
        scene.toolsettings->anim_mirror_bone[0] = '\0';
      }
    }
  }

  /* This has no version check and always runs for all versions because there is forward
   * compatibility code at write time that reallocates the storage, so we need to free it
   * regardless of the version. */
  FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
    if (node_tree->type == NTREE_COMPOSIT) {
      for (bNode &node : node_tree->nodes) {
        if (ELEM(node.type_legacy, CMP_NODE_IMAGE, CMP_NODE_R_LAYERS)) {
          for (bNodeSocket &socket : node.outputs) {
            if (socket.storage) {
              MEM_delete_void(socket.storage);
              socket.storage = nullptr;
            }
          }
        }
      }
    }
  }
  FOREACH_NODETREE_END;

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 15)) {
    for (Light &light : bmain->lights) {
      do_version_light_remove_use_nodes(bmain, &light);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 17)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        for (bNode &node : node_tree->nodes) {
          if (node.type_legacy == CMP_NODE_MOVIEDISTORTION) {
            if (node.storage) {
              BKE_tracking_distortion_free(static_cast<MovieDistortion *>(node.storage));
            }
            node.storage = nullptr;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 18)) {
    FOREACH_NODETREE_BEGIN (bmain, tree, id) {
      if (tree->type == NTREE_GEOMETRY) {
        for (bNode &node : tree->nodes) {
          if (node.type_legacy == GEO_NODE_STRING_TO_CURVES) {
            version_string_to_curves_node_inputs(*tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 19)) {
    for (Mesh &mesh : bmain->meshes) {
      bke::mesh_convert_customdata_to_storage(mesh);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 20)) {
    for (Scene &scene : bmain->scenes) {
      SequencerToolSettings *seq_ts = seq::tool_settings_ensure(&scene);
      constexpr short SEQ_SNAP_TO_FRAME_RANGE_OLD = (1 << 8);
      /* Snap to frame range was bit 8, now bit 9, to make room for snap to increment in bit 8. */
      if (seq_ts->snap_mode & SEQ_SNAP_TO_FRAME_RANGE_OLD) {
        seq_ts->snap_mode &= ~SEQ_SNAP_TO_FRAME_RANGE_OLD;
        seq_ts->snap_mode |= SEQ_SNAP_TO_FRAME_RANGE;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 21)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        for (bNode &node : node_tree->nodes) {
          if (node.type_legacy == CMP_NODE_R_LAYERS) {
            do_version_render_layers_node_albedo_normal_swap(node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

}  // namespace blender
