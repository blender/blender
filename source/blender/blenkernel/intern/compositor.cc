/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string>

#include <fmt/format.h>

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

#include "BKE_compositor.hh"
#include "BKE_context.hh"
#include "BKE_cryptomatte.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"

#include "WM_api.hh"

#include "DNA_layer_types.h"
#include "DNA_node_types.h"
#include "DNA_object_enums.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

namespace blender::bke::compositor {

/* Adds the pass names of the passes used by the given Render Layer node to the given used passes.
 * This essentially adds the pass names of the outputs that are logically linked. */
static void add_passes_used_by_render_layer_node(const bNode *node, Set<std::string> &used_passes)
{
  for (const bNodeSocket *output : node->output_sockets()) {
    if (output->is_logically_linked()) {
      if (output->identifier == StringRef("Alpha")) {
        used_passes.add(RE_PASSNAME_COMBINED);
      }
      else {
        used_passes.add(output->identifier);
      }
    }
  }
}

/* Adds the pass names of the passes used by the given Group Input node to the given used passes.
 * This essentially adds the names of the outputs that are logically linked, since the names of the
 * outputs represent the pass names. This should only be called on group inputs of root node trees.
 * Note however that the Image output is actually the Combined pass, but named as Image for
 * compatibility reasons. */
static void add_passes_used_by_group_input_node(const bNode *node, Set<std::string> &used_passes)
{
  for (const bNodeSocket *output : node->output_sockets()) {
    if (output->is_logically_linked()) {
      if (StringRef(output->identifier) == "Image") {
        used_passes.add(RE_PASSNAME_COMBINED);
      }
      else {
        used_passes.add(output->name);
      }
    }
  }
}

/* Adds the pass names of all Cryptomatte layers needed by the given node to the given used passes.
 * Only passes in the given viewer layers are added. */
static void add_passes_used_by_cryptomatte_node(const bNode *node,
                                                const ViewLayer *view_layer,
                                                Set<std::string> &used_passes)
{
  if (node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) {
    return;
  }

  Scene *scene = reinterpret_cast<Scene *>(node->id);
  if (!scene) {
    return;
  }

  cryptomatte::CryptomatteSessionPtr session = cryptomatte::CryptomatteSessionPtr(
      BKE_cryptomatte_init_from_scene(scene, false));

  const Vector<std::string> &layer_names = cryptomatte::BKE_cryptomatte_layer_names_get(*session);
  if (layer_names.is_empty()) {
    return;
  }

  /* If the stored layer name doesn't corresponds to an existing Cryptomatte layer, fall back to
   * the name of the first layer. */
  const NodeCryptomatte *data = static_cast<NodeCryptomatte *>(node->storage);
  const std::string layer_name = layer_names.contains(data->layer_name) ? data->layer_name :
                                                                          layer_names[0];

  /* Does not use passes from the given view layer, so no need to add anything. */
  if (!StringRef(layer_name).startswith(view_layer->name)) {
    return;
  }

  /* Find out which type of Cryptomatte layers the node needs. Also ensure the type is enabled in
   * the view layer, because the node can use one of the types as a placeholder. */
  const char *cryptomatte_type_name = nullptr;
  if (StringRef(layer_name).endswith(RE_PASSNAME_CRYPTOMATTE_OBJECT)) {
    if (view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_OBJECT) {
      cryptomatte_type_name = RE_PASSNAME_CRYPTOMATTE_OBJECT;
    }
  }
  else if (StringRef(layer_name).endswith(RE_PASSNAME_CRYPTOMATTE_ASSET)) {
    if (view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_ASSET) {
      cryptomatte_type_name = RE_PASSNAME_CRYPTOMATTE_ASSET;
    }
  }
  else if (StringRef(layer_name).endswith(RE_PASSNAME_CRYPTOMATTE_MATERIAL)) {
    if (view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_MATERIAL) {
      cryptomatte_type_name = RE_PASSNAME_CRYPTOMATTE_MATERIAL;
    }
  }

  if (!cryptomatte_type_name) {
    return;
  }

  /* Each layer stores two ranks/levels, so do ceiling division by two. */
  const int cryptomatte_layers_count = int(math::ceil(view_layer->cryptomatte_levels / 2.0f));
  for (const int i : IndexRange(cryptomatte_layers_count)) {
    used_passes.add(fmt::format("{}{:02}", cryptomatte_type_name, i));
  }
}

/* Adds the pass names of the passes used by the given compositor node tree to the given used
 * passes. This is called recursively for node groups. */
static void add_used_passes_recursive(const bNodeTree *node_tree,
                                      const ViewLayer *view_layer,
                                      const bool is_root_tree,
                                      Set<const bNodeTree *> &node_trees_already_searched,
                                      Set<std::string> &used_passes)
{
  if (node_tree == nullptr) {
    return;
  }

  node_tree->ensure_topology_cache();
  for (const bNode *node : node_tree->all_nodes()) {
    if (node->is_muted()) {
      continue;
    }

    switch (node->type_legacy) {
      case NODE_GROUP:
      case NODE_CUSTOM_GROUP: {
        const bNodeTree *node_group_tree = reinterpret_cast<const bNodeTree *>(node->id);
        if (node_trees_already_searched.add(node_group_tree)) {
          add_used_passes_recursive(
              node_group_tree, view_layer, false, node_trees_already_searched, used_passes);
        }
        break;
      }
      case CMP_NODE_R_LAYERS:
        add_passes_used_by_render_layer_node(node, used_passes);
        break;
      case NODE_GROUP_INPUT:
        if (is_root_tree) {
          add_passes_used_by_group_input_node(node, used_passes);
        }
        break;
      case CMP_NODE_CRYPTOMATTE:
        add_passes_used_by_cryptomatte_node(node, view_layer, used_passes);
        break;
      default:
        break;
    }
  }
}

Set<std::string> get_used_passes(const Scene &scene, const ViewLayer *view_layer)
{
  Set<std::string> used_passes;
  Set<const bNodeTree *> node_trees_already_searched;
  add_used_passes_recursive(
      scene.compositing_node_group, view_layer, true, node_trees_already_searched, used_passes);
  return used_passes;
}

bool is_viewport_compositor_used(const bContext &context)
{
  const Scene *scene = CTX_data_scene(&context);
  if (!scene->compositing_node_group) {
    return false;
  }

  wmWindowManager *window_manager = CTX_wm_manager(&context);
  for (const wmWindow &window : window_manager->windows) {
    const bScreen *screen = WM_window_get_active_screen(&window);
    for (const ScrArea &area : screen->areabase) {
      const SpaceLink &space = *static_cast<const SpaceLink *>(area.spacedata.first);
      if (space.spacetype == SPACE_VIEW3D) {
        const View3D &view_3d = reinterpret_cast<const View3D &>(space);

        if (view_3d.shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_DISABLED) {
          continue;
        }

        if (!(view_3d.shading.type >= OB_MATERIAL)) {
          continue;
        }

        return true;
      }
    }
  }

  return false;
}

}  // namespace blender::bke::compositor
