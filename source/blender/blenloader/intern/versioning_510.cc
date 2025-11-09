/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_sys_types.h"

#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"
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

  bNodeSocket *first_input = blender::bke::node_find_socket(node, SOCK_IN, "A_Color");
  bNodeSocket *output = blender::bke::node_find_socket(node, SOCK_OUT, "Result_Color");

  /* Find the link going into the inputs of the node. */
  bNodeLink *first_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree.links) {
    if (link->tosock == first_input) {
      first_link = link;
    }
  }

  bNode &separate_node = version_node_add_empty(node_tree, "CompositorNodeSeparateColor");
  separate_node.parent = node.parent;
  separate_node.location[0] = node.location[0] - 10.0f;
  separate_node.location[1] = node.location[1];
  NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
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
  set_alpha_node.storage = MEM_callocN<NodeCMPCombSepColor>(__func__);

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

  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree.links) {
    if (link->fromsock == output && link->tonode != &set_alpha_node) {
      version_node_add_link(
          node_tree, set_alpha_node, set_alpha_output, *link->tonode, *link->tosock);
      blender::bke::node_remove_link(&node_tree, *link);
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

  bNodeSocket *first_input = blender::bke::node_find_socket(node, SOCK_IN, "A_Color");
  bNodeSocket *output = blender::bke::node_find_socket(node, SOCK_OUT, "Result_Color");

  /* Find the link going into the inputs of the node. */
  bNodeLink *first_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree.links) {
    if (link->tosock == first_input) {
      first_link = link;
    }
  }

  bNode &separate_alpha_node = version_node_add_empty(node_tree, "FunctionNodeSeparateColor");
  separate_alpha_node.parent = node.parent;
  separate_alpha_node.location[0] = node.location[0] - 10.0f;
  separate_alpha_node.location[1] = node.location[1];
  NodeCombSepColor *separate_alpha_storage = MEM_callocN<NodeCombSepColor>(__func__);
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
  NodeCombSepColor *separate_color_storage = MEM_callocN<NodeCombSepColor>(__func__);
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
  NodeCombSepColor *combine_color_storage = MEM_callocN<NodeCombSepColor>(__func__);
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

  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree.links) {
    if (link->fromsock == output && link->tonode != &separate_color_node) {
      version_node_add_link(
          node_tree, combine_color_node, combine_color_output, *link->tonode, *link->tosock);
      blender::bke::node_remove_link(&node_tree, *link);
    }
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
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_NODE) {
            SpaceNode *space_node = reinterpret_cast<SpaceNode *>(space);
            bNodeTreePath *path = static_cast<bNodeTreePath *>(space_node->treepath.last);
            if (space_node->nodetree && path) {
              space_node->nodetree->active_viewer_key = path->parent_key;
            }
          }
        }
      }
    }
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
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == SH_NODE_MIX) {
            do_version_mix_node_mix_mode_compositor(*node_tree, *node);
          }
        }
      }
      else if (node_tree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == SH_NODE_MIX) {
            do_version_mix_node_mix_mode_geometry(*node_tree, *node);
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
