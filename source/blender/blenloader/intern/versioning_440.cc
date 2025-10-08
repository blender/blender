/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include <fmt/format.h>

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_defaults.h"
#include "DNA_genfile.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_workspace_types.h"

#undef DNA_GENFILE_VERSIONING_MACROS

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_anim_data.hh"
#include "BKE_fcurve.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_scene.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "RNA_types.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_versioning.hh"

#include "readfile.hh"

#include "versioning_common.hh"

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
      blender::bke::node_add_link(*node_tree, *node, *glare_output, *link->tonode, *link->tosock);
      blender::bke::node_remove_link(node_tree, *link);
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
 * dot product node that computes the average as before. */
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
      blender::bke::node_remove_link(node_tree, *link);
      continue;
    }

    /* Add a hidden dot product node. */
    bNode *dot_product_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, SH_NODE_VECTOR_MATH);
    dot_product_node->custom1 = NODE_VECTOR_MATH_DOT_PRODUCT;
    dot_product_node->flag |= NODE_COLLAPSED;
    dot_product_node->location[0] = link->fromnode->location[0] + link->fromnode->width + 10.0f;
    dot_product_node->location[1] = link->fromnode->location[1];

    /* Link the source socket to the dot product input. */
    bNodeSocket *dot_product_a_input = blender::bke::node_find_socket(
        *dot_product_node, SOCK_IN, "Vector");
    version_node_add_link(
        *node_tree, *link->fromnode, *link->fromsock, *dot_product_node, *dot_product_a_input);

    /* Set the dot product vector to 1 / 3 to compute the average. */
    bNodeSocket *dot_product_b_input = blender::bke::node_find_socket(
        *dot_product_node, SOCK_IN, "Vector_001");
    copy_v3_fl(dot_product_b_input->default_value_typed<bNodeSocketValueVector>()->value,
               1.0f / 3.0f);

    /* Link the dot product node output to the link target. */
    bNodeSocket *dot_product_output = blender::bke::node_find_socket(
        *dot_product_node, SOCK_OUT, "Value");
    bNodeLink *output_link = &version_node_add_link(
        *node_tree, *dot_product_node, *dot_product_output, *link->tonode, *link->tosock);

    /* Add the new link to the cache. */
    color_to_float_links.add_new(link->fromsock, output_link);

    /* Remove the old link. */
    blender::bke::node_remove_link(node_tree, *link);
  }
}

static void do_version_bump_filter_width(bNodeTree *node_tree)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &node_tree->nodes) {
    if (node->type_legacy != SH_NODE_BUMP) {
      continue;
    }

    bNodeSocket *filter_width_input = blender::bke::node_find_socket(
        *node, SOCK_IN, "Filter Width");
    if (filter_width_input) {
      *version_cycles_node_socket_float_value(filter_width_input) = 1.0f;
    }
  }
}

void do_versions_after_linking_440(FileData *fd, Main *bmain)
{
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
}

static bool versioning_convert_seq_text_anchor(Strip *strip, void * /*user_data*/)
{
  if (strip->type != STRIP_TYPE_TEXT || strip->effectdata == nullptr) {
    return true;
  }

  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  data->anchor_x = data->align;
  data->anchor_y = data->align_y_legacy;
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
    bNodeSocket *selection = bke::node_find_socket(*triangulate, SOCK_IN, "Selection");
    bNodeSocket *min_verts = bke::node_find_socket(*triangulate, SOCK_IN, "Minimum Vertices");
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
    auto *compare_storage = MEM_callocN<NodeFunctionCompare>(__func__);
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
                          *bke::node_find_socket(*&corners_of_face, SOCK_OUT, "Total"),
                          greater_or_equal,
                          *bke::node_find_socket(*&greater_or_equal, SOCK_IN, "A_INT"));
    if (bNodeLink **min_verts_link = input_links.lookup_ptr(min_verts)) {
      (*min_verts_link)->tonode = &greater_or_equal;
      (*min_verts_link)->tosock = bke::node_find_socket(*&greater_or_equal, SOCK_IN, "B_INT");
    }
    else {
      bNodeSocket *new_min_verts = bke::node_find_socket(*&greater_or_equal, SOCK_IN, "B_INT");
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
      (*selection_link)->tosock = bke::node_find_socket(*&boolean_and, SOCK_IN, "Boolean");
      version_node_add_link(*tree,
                            greater_or_equal,
                            *bke::node_find_socket(*&greater_or_equal, SOCK_OUT, "Result"),
                            boolean_and,
                            *bke::node_find_socket(*&boolean_and, SOCK_IN, "Boolean_001"));

      version_node_add_link(*tree,
                            boolean_and,
                            *bke::node_find_socket(*&boolean_and, SOCK_OUT, "Boolean"),
                            *triangulate,
                            *selection);
    }
    else {
      version_node_add_link(*tree,
                            greater_or_equal,
                            *bke::node_find_socket(*&greater_or_equal, SOCK_OUT, "Result"),
                            *triangulate,
                            *selection);
    }

    /* Make versioning idempotent. */
    bke::node_remove_socket(*tree, *triangulate, *min_verts);
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

static bool versioning_clear_strip_unused_flag(Strip *strip, void * /*user_data*/)
{
  strip->flag &= ~(1 << 6);
  return true;
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

void blo_do_versions_440(FileData *fd, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 1)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = blender::seq::editing_get(scene);
      if (ed != nullptr) {
        blender::seq::foreach_strip(&ed->seqbase, versioning_convert_seq_text_anchor, nullptr);
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
          STRNCPY_UTF8(tref->idname, "builtin.brush");
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
      Editing *ed = blender::seq::editing_get(scene);
      if (ed != nullptr) {
        blender::seq::foreach_strip(&ed->seqbase, versioning_clear_strip_unused_flag, scene);
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
                STRNCPY_UTF8(socket->identifier, "Shader_001");
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
      SequencerToolSettings *sequencer_tool_settings = blender::seq::tool_settings_ensure(scene);
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
}
