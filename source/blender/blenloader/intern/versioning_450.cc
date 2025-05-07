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
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_defaults.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
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
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_customdata.hh"
#include "BKE_effect.h"
#include "BKE_fcurve.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "SEQ_iterator.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_armature_iter.hh"
#include "ANIM_versioning.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "BLO_read_write.hh"
#include "BLO_readfile.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// static CLG_LogRef LOG = {"blo.readfile.doversion"};

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
      blender::bke::node_remove_link(node_tree, *link);
      continue;
    }

    /* Add a hidden dot product node. */
    bNode *dot_product_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_NORMAL);
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
    bNode *multiply_node = blender::bke::node_add_static_node(nullptr, *node_tree, CMP_NODE_MATH);
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

/* The compositor Value, Color Ramp, Mix Color, Map Range, Map Value, Math, Combine XYZ, Separate
 * XYZ, and Vector Curves nodes are now deprecated and should be replaced by their generic Shader
 * node counterpart. */
static void do_version_convert_to_generic_nodes(bNodeTree *node_tree)
{
  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    switch (node->type_legacy) {
      case CMP_NODE_VALUE:
        node->type_legacy = SH_NODE_VALUE;
        STRNCPY(node->idname, "ShaderNodeValue");
        break;
      case CMP_NODE_MATH:
        node->type_legacy = SH_NODE_MATH;
        STRNCPY(node->idname, "ShaderNodeMath");
        break;
      case CMP_NODE_COMBINE_XYZ:
        node->type_legacy = SH_NODE_COMBXYZ;
        STRNCPY(node->idname, "ShaderNodeCombineXYZ");
        break;
      case CMP_NODE_SEPARATE_XYZ:
        node->type_legacy = SH_NODE_SEPXYZ;
        STRNCPY(node->idname, "ShaderNodeSeparateXYZ");
        break;
      case CMP_NODE_CURVE_VEC:
        node->type_legacy = SH_NODE_CURVE_VEC;
        STRNCPY(node->idname, "ShaderNodeVectorCurve");
        break;
      case CMP_NODE_VALTORGB: {
        node->type_legacy = SH_NODE_VALTORGB;
        STRNCPY(node->idname, "ShaderNodeValToRGB");

        /* Compositor node uses "Image" as the output name while the shader node uses "Color" as
         * the output name. */
        bNodeSocket *image_output = blender::bke::node_find_socket(*node, SOCK_OUT, "Image");
        STRNCPY(image_output->identifier, "Color");
        STRNCPY(image_output->name, "Color");

        break;
      }
      case CMP_NODE_MAP_RANGE: {
        node->type_legacy = SH_NODE_MAP_RANGE;
        STRNCPY(node->idname, "ShaderNodeMapRange");

        /* Transfer options from node to NodeMapRange storage. */
        NodeMapRange *data = MEM_callocN<NodeMapRange>(__func__);
        data->clamp = node->custom1;
        data->data_type = CD_PROP_FLOAT;
        data->interpolation_type = NODE_MAP_RANGE_LINEAR;
        node->storage = data;

        /* Compositor node uses "Value" as the output name while the shader node uses "Result" as
         * the output name. */
        bNodeSocket *value_output = blender::bke::node_find_socket(*node, SOCK_OUT, "Value");
        STRNCPY(value_output->identifier, "Result");
        STRNCPY(value_output->name, "Result");

        break;
      }
      case CMP_NODE_MIX_RGB: {
        node->type_legacy = SH_NODE_MIX;
        STRNCPY(node->idname, "ShaderNodeMix");

        /* Transfer options from node to NodeShaderMix storage. */
        NodeShaderMix *data = MEM_callocN<NodeShaderMix>(__func__);
        data->data_type = SOCK_RGBA;
        data->factor_mode = NODE_MIX_MODE_UNIFORM;
        data->clamp_factor = 0;
        data->clamp_result = node->custom2 & SHD_MIXRGB_CLAMP ? 1 : 0;
        data->blend_type = node->custom1;
        node->storage = data;

        /* Compositor node uses "Fac", "Image", and ("Image" "Image_001") as socket names and
         * identifiers while the shader node uses ("Factor", "Factor_Float"), ("A", "A_Color"),
         * ("B", "B_Color"), and ("Result", "Result_Color") as socket names and identifiers. */
        bNodeSocket *factor_input = blender::bke::node_find_socket(*node, SOCK_IN, "Fac");
        STRNCPY(factor_input->identifier, "Factor_Float");
        STRNCPY(factor_input->name, "Factor");
        bNodeSocket *first_input = blender::bke::node_find_socket(*node, SOCK_IN, "Image");
        STRNCPY(first_input->identifier, "A_Color");
        STRNCPY(first_input->name, "A");
        bNodeSocket *second_input = blender::bke::node_find_socket(*node, SOCK_IN, "Image_001");
        STRNCPY(second_input->identifier, "B_Color");
        STRNCPY(second_input->name, "B");
        bNodeSocket *image_output = blender::bke::node_find_socket(*node, SOCK_OUT, "Image");
        STRNCPY(image_output->identifier, "Result_Color");
        STRNCPY(image_output->name, "Result");

        break;
      }
      default:
        break;
    }
  }
}

/* The Use Alpha option is does not exist in the new generic Mix node, it essentially just
 * multiplied the factor by the alpha of the second input. */
static void do_version_mix_color_use_alpha(bNodeTree *node_tree, bNode *node)
{
  if (!(node->custom2 & SHD_MIXRGB_USE_ALPHA)) {
    return;
  }

  bNodeSocket *factor_input = blender::bke::node_find_socket(*node, SOCK_IN, "Factor_Float");
  bNodeSocket *b_input = blender::bke::node_find_socket(*node, SOCK_IN, "B_Color");

  /* Find the links going into the factor and B input of the Mix node. */
  bNodeLink *factor_link = nullptr;
  bNodeLink *b_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
    if (link->tosock == factor_input) {
      factor_link = link;
    }
    else if (link->tosock == b_input) {
      b_link = link;
    }
  }

  /* If neither sockets are connected, just multiply the factor by the alpha of the B input. */
  if (!factor_link && !b_link) {
    static_cast<bNodeSocketValueFloat *>(factor_input->default_value)->value *=
        static_cast<bNodeSocketValueRGBA *>(b_input->default_value)->value[3];
    return;
  }

  /* Otherwise, add a multiply node to do the multiplication. */
  bNode *multiply_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
  multiply_node->parent = node->parent;
  multiply_node->custom1 = NODE_MATH_MULTIPLY;
  multiply_node->location[0] = node->location[0] - node->width - 20.0f;
  multiply_node->location[1] = node->location[1];
  multiply_node->flag |= NODE_HIDDEN;

  bNodeSocket *multiply_input_a = static_cast<bNodeSocket *>(
      BLI_findlink(&multiply_node->inputs, 0));
  bNodeSocket *multiply_input_b = static_cast<bNodeSocket *>(
      BLI_findlink(&multiply_node->inputs, 1));
  bNodeSocket *multiply_output = blender::bke::node_find_socket(*multiply_node, SOCK_OUT, "Value");

  /* Connect the output of the multiply node to the math node. */
  version_node_add_link(*node_tree, *multiply_node, *multiply_output, *node, *factor_input);

  if (factor_link) {
    /* The factor input is linked, so connect its origin to the first input of the multiply and
     * remove the original link. */
    version_node_add_link(*node_tree,
                          *factor_link->fromnode,
                          *factor_link->fromsock,
                          *multiply_node,
                          *multiply_input_a);
    blender::bke::node_remove_link(node_tree, *factor_link);
  }
  else {
    /* Otherwise, the factor is unlinked and we just copy the factor value to the first input in
     * the multiply.*/
    static_cast<bNodeSocketValueFloat *>(multiply_input_a->default_value)->value =
        static_cast<bNodeSocketValueFloat *>(factor_input->default_value)->value;
  }

  if (b_link) {
    /* The B input is linked, so extract the alpha of its origin and connect it to the second input
     * of the multiply and remove the original link. */
    bNode *separate_color_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_SEPARATE_COLOR);
    separate_color_node->parent = node->parent;
    separate_color_node->location[0] = multiply_node->location[0] - multiply_node->width - 20.0f;
    separate_color_node->location[1] = multiply_node->location[1];
    separate_color_node->flag |= NODE_HIDDEN;

    bNodeSocket *image_input = blender::bke::node_find_socket(
        *separate_color_node, SOCK_IN, "Image");
    bNodeSocket *alpha_output = blender::bke::node_find_socket(
        *separate_color_node, SOCK_OUT, "Alpha");

    version_node_add_link(
        *node_tree, *b_link->fromnode, *b_link->fromsock, *separate_color_node, *image_input);
    version_node_add_link(
        *node_tree, *separate_color_node, *alpha_output, *multiply_node, *multiply_input_b);
  }
  else {
    /* Otherwise, the B input is unlinked and we just copy the alpha value to the second input in
     * the multiply.*/
    static_cast<bNodeSocketValueFloat *>(multiply_input_b->default_value)->value =
        static_cast<bNodeSocketValueRGBA *>(b_input->default_value)->value[3];
  }

  version_socket_update_is_used(node_tree);
}

/* The Map Value node is now deprecated and should be replaced by other nodes. The node essentially
 * just computes (value + offset) * size and clamps based on min and max. */
static void do_version_map_value_node(bNodeTree *node_tree, bNode *node)
{
  const TexMapping &texture_mapping = *static_cast<TexMapping *>(node->storage);
  const bool use_min = texture_mapping.flag & TEXMAP_CLIP_MIN;
  const bool use_max = texture_mapping.flag & TEXMAP_CLIP_MAX;
  const float offset = texture_mapping.loc[0];
  const float size = texture_mapping.size[0];
  const float min = texture_mapping.min[0];
  const float max = texture_mapping.max[0];

  bNodeSocket *value_input = blender::bke::node_find_socket(*node, SOCK_IN, "Value");

  /* Find the link going into the value input Map Value node. */
  bNodeLink *value_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
    if (link->tosock == value_input) {
      value_link = link;
    }
  }

  /* If the value input is not connected, add a value node with the computed value. */
  if (!value_link) {
    const float value = static_cast<bNodeSocketValueFloat *>(value_input->default_value)->value;
    const float mapped_value = (value + offset) * size;
    const float min_clamped_value = use_min ? blender::math::max(mapped_value, min) : mapped_value;
    const float clamped_value = use_max ? blender::math::min(min_clamped_value, max) :
                                          min_clamped_value;

    bNode *value_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_VALUE);
    value_node->parent = node->parent;
    value_node->location[0] = node->location[0];
    value_node->location[1] = node->location[1];

    bNodeSocket *value_output = blender::bke::node_find_socket(*value_node, SOCK_OUT, "Value");
    static_cast<bNodeSocketValueFloat *>(value_output->default_value)->value = clamped_value;

    /* Relink from the Map Value node to the value node. */
    LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
      if (link->fromnode != node) {
        continue;
      }

      version_node_add_link(*node_tree, *value_node, *value_output, *link->tonode, *link->tosock);
      blender::bke::node_remove_link(node_tree, *link);
    }

    blender::bke::node_remove_node(nullptr, *node_tree, *node, false);

    version_socket_update_is_used(node_tree);
    return;
  }

  /* Otherwise, add math nodes to do the computation, starting with an add node to add the offset
   * of the range. */
  bNode *add_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
  add_node->parent = node->parent;
  add_node->custom1 = NODE_MATH_ADD;
  add_node->location[0] = node->location[0];
  add_node->location[1] = node->location[1];
  add_node->flag |= NODE_HIDDEN;

  bNodeSocket *add_input_a = static_cast<bNodeSocket *>(BLI_findlink(&add_node->inputs, 0));
  bNodeSocket *add_input_b = static_cast<bNodeSocket *>(BLI_findlink(&add_node->inputs, 1));
  bNodeSocket *add_output = blender::bke::node_find_socket(*add_node, SOCK_OUT, "Value");

  /* Connect the origin of the node to the first input of the add node and remove the original
   * link. */
  version_node_add_link(
      *node_tree, *value_link->fromnode, *value_link->fromsock, *add_node, *add_input_a);
  blender::bke::node_remove_link(node_tree, *value_link);

  /* Set the offset to the second input of the add node. */
  static_cast<bNodeSocketValueFloat *>(add_input_b->default_value)->value = offset;

  /* Add a multiply node to multiply by the size. */
  bNode *multiply_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
  multiply_node->parent = node->parent;
  multiply_node->custom1 = NODE_MATH_MULTIPLY;
  multiply_node->location[0] = add_node->location[0];
  multiply_node->location[1] = add_node->location[1] - 40.0f;
  multiply_node->flag |= NODE_HIDDEN;

  bNodeSocket *multiply_input_a = static_cast<bNodeSocket *>(
      BLI_findlink(&multiply_node->inputs, 0));
  bNodeSocket *multiply_input_b = static_cast<bNodeSocket *>(
      BLI_findlink(&multiply_node->inputs, 1));
  bNodeSocket *multiply_output = blender::bke::node_find_socket(*multiply_node, SOCK_OUT, "Value");

  /* Connect the output of the add node to the first input of the multiply node. */
  version_node_add_link(*node_tree, *add_node, *add_output, *multiply_node, *multiply_input_a);

  /* Set the size to the second input of the multiply node. */
  static_cast<bNodeSocketValueFloat *>(multiply_input_b->default_value)->value = size;

  bNode *final_node = multiply_node;
  bNodeSocket *final_output = multiply_output;

  if (use_min) {
    /* Add a maximum node to clamp by the minimum. */
    bNode *max_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
    max_node->parent = node->parent;
    max_node->custom1 = NODE_MATH_MAXIMUM;
    max_node->location[0] = final_node->location[0];
    max_node->location[1] = final_node->location[1] - 40.0f;
    max_node->flag |= NODE_HIDDEN;

    bNodeSocket *max_input_a = static_cast<bNodeSocket *>(BLI_findlink(&max_node->inputs, 0));
    bNodeSocket *max_input_b = static_cast<bNodeSocket *>(BLI_findlink(&max_node->inputs, 1));
    bNodeSocket *max_output = blender::bke::node_find_socket(*max_node, SOCK_OUT, "Value");

    /* Connect the output of the final node to the first input of the maximum node. */
    version_node_add_link(*node_tree, *final_node, *final_output, *max_node, *max_input_a);

    /* Set the minimum to the second input of the maximum node. */
    static_cast<bNodeSocketValueFloat *>(max_input_b->default_value)->value = min;

    final_node = max_node;
    final_output = max_output;
  }

  if (use_max) {
    /* Add a minimum node to clamp by the maximum. */
    bNode *min_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
    min_node->parent = node->parent;
    min_node->custom1 = NODE_MATH_MINIMUM;
    min_node->location[0] = final_node->location[0];
    min_node->location[1] = final_node->location[1] - 40.0f;
    min_node->flag |= NODE_HIDDEN;

    bNodeSocket *min_input_a = static_cast<bNodeSocket *>(BLI_findlink(&min_node->inputs, 0));
    bNodeSocket *min_input_b = static_cast<bNodeSocket *>(BLI_findlink(&min_node->inputs, 1));
    bNodeSocket *min_output = blender::bke::node_find_socket(*min_node, SOCK_OUT, "Value");

    /* Connect the output of the final node to the first input of the minimum node. */
    version_node_add_link(*node_tree, *final_node, *final_output, *min_node, *min_input_a);

    /* Set the maximum to the second input of the minimum node. */
    static_cast<bNodeSocketValueFloat *>(min_input_b->default_value)->value = max;

    final_node = min_node;
    final_output = min_output;
  }

  /* Relink from the Map Value node to the final node. */
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (link->fromnode != node) {
      continue;
    }

    version_node_add_link(*node_tree, *final_node, *final_output, *link->tonode, *link->tosock);
    blender::bke::node_remove_link(node_tree, *link);
  }

  blender::bke::node_remove_node(nullptr, *node_tree, *node, false);

  version_socket_update_is_used(node_tree);
}

/* Equivalent to do_version_convert_to_generic_nodes but performed after linking for handing things
 * like animation or node construction. */
static void do_version_convert_to_generic_nodes_after_linking(Main *bmain,
                                                              bNodeTree *node_tree,
                                                              ID *id)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &node_tree->nodes) {
    char escaped_node_name[sizeof(node->name) * 2 + 1];
    BLI_str_escape(escaped_node_name, node->name, sizeof(escaped_node_name));
    const std::string rna_path_prefix = fmt::format("nodes[\"{}\"].inputs", escaped_node_name);

    switch (node->type_legacy) {
      /* Notice that we use the shader type because the node is already converted in versioning
       * before linking. */
      case SH_NODE_CURVE_VEC: {
        /* The node gained a new Factor input as a first socket, so the vector socket moved to be
         * the second socket and we need to transfer its animation as well. */
        BKE_animdata_fix_paths_rename_all_ex(
            bmain, id, rna_path_prefix.c_str(), nullptr, nullptr, 0, 1, false);
        break;
      }
      /* Notice that we use the shader type because the node is already converted in versioning
       * before linking. */
      case SH_NODE_MIX: {
        /* The node gained multiple new sockets after the factor socket, so the second and third
         * sockets moved to be the 7th and 8th sockets. */
        BKE_animdata_fix_paths_rename_all_ex(
            bmain, id, rna_path_prefix.c_str(), nullptr, nullptr, 1, 6, false);
        BKE_animdata_fix_paths_rename_all_ex(
            bmain, id, rna_path_prefix.c_str(), nullptr, nullptr, 2, 7, false);

        do_version_mix_color_use_alpha(node_tree, node);

        break;
      }
      case CMP_NODE_MAP_VALUE: {
        do_version_map_value_node(node_tree, node);
        break;
      }
      default:
        break;
    }
  }
}

/* A new suppress boolean input was added that either enables suppression or disabled it.
 * Previously, suppression was disabled when the maximum was zero. So we enable suppression for non
 * zero or linked maximum input. */
static void do_version_new_glare_suppress_input(bNodeTree *node_tree)
{
  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    if (node->type_legacy != CMP_NODE_GLARE) {
      continue;
    }

    bNodeSocket *suppress_input = blender::bke::node_find_socket(
        *node, SOCK_IN, "Suppress Highlights");
    bNodeSocket *maximum_input = blender::bke::node_find_socket(
        *node, SOCK_IN, "Maximum Highlights");

    const float maximum = maximum_input->default_value_typed<bNodeSocketValueFloat>()->value;
    if (version_node_socket_is_used(maximum_input) || maximum != 0.0) {
      suppress_input->default_value_typed<bNodeSocketValueBoolean>()->value = true;
    }
  }
}

/* The Rotate Star 45 option was converted into a Diagonal Star input. */
static void do_version_glare_node_star_45_option_to_input(bNodeTree *node_tree, bNode *node)
{
  NodeGlare *storage = static_cast<NodeGlare *>(node->storage);
  if (!storage) {
    return;
  }

  /* Input already exists, was already versioned. */
  if (blender::bke::node_find_socket(*node, SOCK_IN, "Diagonal Star")) {
    return;
  }

  bNodeSocket *diagonal_star_input = blender::bke::node_add_static_socket(
      *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Diagonal Star", "Diagonal");
  diagonal_star_input->default_value_typed<bNodeSocketValueBoolean>()->value = storage->star_45;
}

/* The Rotate Star 45 option was converted into a Diagonal Star input. */
static void do_version_glare_node_star_45_option_to_input_animation(bNodeTree *node_tree,
                                                                    bNode *node)
{
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

    /* Change the RNA path of the FCurve from the old property to the new input. */
    if (BLI_str_endswith(fcurve->rna_path, "use_rotate_45")) {
      MEM_freeN(fcurve->rna_path);
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[14].default_value");
    }
  });
}

/* The options were converted into inputs. */
static void do_version_bokeh_image_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeBokehImage *storage = static_cast<NodeBokehImage *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Flaps")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Flaps", "Flaps");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->flaps;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Angle")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Angle", "Angle");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->angle;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Roundness")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Roundness", "Roundness");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->rounding;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Catadioptric Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Catadioptric Size",
                                                              "Catadioptric Size");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->catadioptric;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Color Shift")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Color Shift", "Color Shift");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->lensshift;
  }
}

/* The options were converted into inputs. */
static void do_version_bokeh_image_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                    bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "flaps")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[0].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "angle")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "rounding")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "catadioptric")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "shift")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_time_curve_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Start Frame")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Start Frame", "Start Frame");
    input->default_value_typed<bNodeSocketValueInt>()->value = node->custom1;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "End Frame")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "End Frame", "End Frame");
    input->default_value_typed<bNodeSocketValueInt>()->value = node->custom2;
  }
}

/* The options were converted into inputs. */
static void do_version_time_curve_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                   bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "frame_start")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[0].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "frame_end")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_mask_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeMask *storage = static_cast<NodeMask *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Size X")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Size X", "Size X");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->size_x;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Size Y")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Size Y", "Size Y");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->size_y;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Feather")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Feather", "Feather");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = !(
        node->custom1 & CMP_NODE_MASK_FLAG_NO_FEATHER);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Motion Blur")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Motion Blur", "Motion Blur");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(
        node->custom1 & CMP_NODE_MASK_FLAG_MOTION_BLUR);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Motion Blur Samples")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Motion Blur Samples", "Samples");
    input->default_value_typed<bNodeSocketValueInt>()->value = node->custom2;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Motion Blur Shutter")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Motion Blur Shutter", "Shutter");
    input->default_value_typed<bNodeSocketValueFloat>()->value = node->custom3;
  }
}

/* The options were converted into inputs. */
static void do_version_mask_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "size_x")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[0].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "size_y")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_feather")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_motion_blur")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "motion_blur_samples")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "motion_blur_shutter")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_switch_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Switch")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Switch", "Switch");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1);
  }
}

/* The options were converted into inputs. */
static void do_version_switch_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "check")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[0].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "inputs[0].default_value")) {
      /* The new input was added at the start, so offset the animation indices by 1. */
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "inputs[1].default_value")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_split_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Factor")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Factor", "Factor");
    input->default_value_typed<bNodeSocketValueFloat>()->value = node->custom1 / 100.0f;
  }
}

/* The options were converted into inputs. */
static void do_version_split_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "factor")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[0].default_value");
      adjust_fcurve_key_frame_values(
          fcurve, PROP_FLOAT, [&](const float value) { return value / 100.0f; });
    }
    else if (BLI_str_endswith(fcurve->rna_path, "inputs[0].default_value")) {
      /* The new input was added at the start, so offset the animation indices by 1. */
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "inputs[1].default_value")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_invert_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Invert Color")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Invert Color", "Invert Color");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1 &
                                                                        CMP_CHAN_RGB);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Invert Alpha")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Invert Alpha", "Invert Alpha");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1 &
                                                                        CMP_CHAN_A);
  }
}

/* The options were converted into inputs. */
static void do_version_invert_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "invert_rgb")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "invert_alpha")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_z_combine_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Use Alpha")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Use Alpha", "Use Alpha");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Anti-Alias")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Anti-Alias", "Anti-Alias");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = !bool(node->custom2);
  }
}

/* The options were converted into inputs. */
static void do_version_z_combine_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                  bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "use_alpha")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_antialias_z")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_tone_map_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeTonemap *storage = static_cast<NodeTonemap *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Key")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Key", "Key");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->key;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Balance")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Balance", "Balance");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->offset;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Gamma")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Gamma", "Gamma");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->gamma;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Intensity")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Intensity", "Intensity");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->f;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Contrast")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Contrast", "Contrast");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->m;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Light Adaptation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Light Adaptation",
                                                              "Light Adaptation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->a;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Chromatic Adaptation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Chromatic Adaptation",
                                                              "Chromatic Adaptation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->c;
  }
}

/* The options were converted into inputs. */
static void do_version_tone_map_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "key")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "offset")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "gamma")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "intensity")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "contrast")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "adaptation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[6].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "correction")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[7].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_dilate_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Size", "Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = node->custom2;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Falloff Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Falloff Size", "Falloff Size");
    input->default_value_typed<bNodeSocketValueFloat>()->value = node->custom3;
  }
}

/* The options were converted into inputs. */
static void do_version_dilate_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "distance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "edge")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_inpaint_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Size", "Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = node->custom2;
  }
}

/* The options were converted into inputs. */
static void do_version_inpaint_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "distance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_pixelate_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Size", "Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = node->custom1;
  }
}

/* The options were converted into inputs. */
static void do_version_pixelate_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "pixel_size")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_kuwahara_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeKuwaharaData *storage = static_cast<NodeKuwaharaData *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Uniformity")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Uniformity", "Uniformity");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->uniformity;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Sharpness")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Sharpness", "Sharpness");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->sharpness;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Eccentricity")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Eccentricity", "Eccentricity");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->eccentricity;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "High Precision")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "High Precision", "High Precision");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = storage->high_precision;
  }
}

/* The options were converted into inputs. */
static void do_version_kuwahara_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "uniformity")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "sharpness")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "eccentricity")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "high_precision")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_despeckle_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Color Threshold")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Color Threshold", "Color Threshold");
    input->default_value_typed<bNodeSocketValueFloat>()->value = node->custom3;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Neighbor Threshold")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Neighbor Threshold",
                                                              "Neighbor Threshold");
    input->default_value_typed<bNodeSocketValueFloat>()->value = node->custom4;
  }
}

/* The options were converted into inputs. */
static void do_version_despeckle_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                  bNode *node)
{
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
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "threshold_neighbor")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_denoise_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeDenoise *storage = static_cast<NodeDenoise *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "HDR")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "HDR", "HDR");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = storage->hdr;
  }
}

/* The options were converted into inputs. */
static void do_version_denoise_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "use_hdr")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_anti_alias_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeAntiAliasingData *storage = static_cast<NodeAntiAliasingData *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Threshold")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Threshold", "Threshold");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->threshold;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Contrast Limit")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Contrast Limit", "Contrast Limit");
    /* Contrast limit was previously divided by 10. */
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->contrast_limit * 10.0f;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Corner Rounding")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Corner Rounding", "Corner Rounding");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->corner_rounding;
  }
}

/* The options were converted into inputs. */
static void do_version_anti_alias_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                   bNode *node)
{
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
    else if (BLI_str_endswith(fcurve->rna_path, "contrast_limit")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
      /* Contrast limit was previously divided by 10. */
      adjust_fcurve_key_frame_values(
          fcurve, PROP_FLOAT, [&](const float value) { return value * 10.0f; });
    }
    else if (BLI_str_endswith(fcurve->rna_path, "corner_rounding")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_vector_blur_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeBlurData *storage = static_cast<NodeBlurData *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Samples")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Samples", "Samples");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->samples;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Shutter")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Shutter", "Shutter");
    /* Shutter was previously divided by 2. */
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->fac * 2.0f;
  }
}

/* The options were converted into inputs. */
static void do_version_vector_blur_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                    bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "samples")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "factor")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
      /* Shutter was previously divided by 2. */
      adjust_fcurve_key_frame_values(
          fcurve, PROP_FLOAT, [&](const float value) { return value * 2.0f; });
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_channel_matte_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeChroma *storage = static_cast<NodeChroma *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Minimum")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Minimum", "Minimum");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t2;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Maximum")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Maximum", "Maximum");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t1;
  }
}

/* The options were converted into inputs. */
static void do_version_channel_matte_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                      bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "limit_min")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "limit_max")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_chroma_matte_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeChroma *storage = static_cast<NodeChroma *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Minimum")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Minimum", "Minimum");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t2;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Maximum")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Maximum", "Maximum");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t1;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Falloff")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Falloff", "Falloff");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->fstrength;
  }
}

/* The options were converted into inputs. */
static void do_version_chroma_matte_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                     bNode *node)
{
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
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "tolerance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "gain")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_color_matte_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeChroma *storage = static_cast<NodeChroma *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Hue")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Hue", "Hue");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t1;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Saturation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Saturation", "Saturation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t2;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Value")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Value", "Value");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t3;
  }
}

/* The options were converted into inputs. */
static void do_version_color_matte_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                    bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "color_hue")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "color_saturation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "color_value")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_difference_matte_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeChroma *storage = static_cast<NodeChroma *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Tolerance")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Tolerance", "Tolerance");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t1;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Falloff")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Falloff", "Falloff");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t2;
  }
}

/* The options were converted into inputs. */
static void do_version_difference_matte_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                         bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "tolerance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "falloff")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_distance_matte_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeChroma *storage = static_cast<NodeChroma *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Tolerance")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Tolerance", "Tolerance");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t1;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Falloff")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Falloff", "Falloff");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t2;
  }
}

/* The options were converted into inputs. */
static void do_version_distance_matte_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                       bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "tolerance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "falloff")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_luminance_matte_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeChroma *storage = static_cast<NodeChroma *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Minimum")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Minimum", "Minimum");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t2;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Maximum")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Maximum", "Maximum");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->t1;
  }
}

/* The options were converted into inputs. */
static void do_version_luminance_matte_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                        bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "limit_min")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "limit_max")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_color_spill_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeColorspill *storage = static_cast<NodeColorspill *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Limit Strength")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Limit Strength", "Limit Strength");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->limscale;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Use Spill Strength")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_BOOLEAN,
                                                              PROP_NONE,
                                                              "Use Spill Strength",
                                                              "Use Spill Strength");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(storage->unspill);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Spill Strength")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Spill Strength", "Spill Strength");
    input->default_value_typed<bNodeSocketValueRGBA>()->value[0] = storage->uspillr;
    input->default_value_typed<bNodeSocketValueRGBA>()->value[1] = storage->uspillg;
    input->default_value_typed<bNodeSocketValueRGBA>()->value[2] = storage->uspillb;
  }
}

/* The options were converted into inputs. */
static void do_version_color_spill_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                    bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "ratio")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_unspill")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "unspill_red")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
      fcurve->array_index = 0;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "unspill_green")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
      fcurve->array_index = 1;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "unspill_blue")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
      fcurve->array_index = 2;
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_keying_screen_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeKeyingScreenData *storage = static_cast<NodeKeyingScreenData *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Smoothness")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Smoothness", "Smoothness");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->smoothness;
  }
}

/* The options were converted into inputs. */
static void do_version_keying_screen_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                      bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "smoothness")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[0].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_keying_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeKeyingData *storage = static_cast<NodeKeyingData *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Preprocess Blur Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_INT,
                                                              PROP_NONE,
                                                              "Preprocess Blur Size",
                                                              "Preprocess Blur Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->blur_pre;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Key Balance")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Key Balance", "Key Balance");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->screen_balance;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Edge Search Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Edge Search Size", "Edge Search Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->edge_kernel_radius;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Edge Tolerance")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Edge Tolerance", "Edge Tolerance");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->edge_kernel_tolerance;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Black Level")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Black Level", "Black Level");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->clip_black;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "White Level")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "White Level", "White Level");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->clip_white;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Postprocess Blur Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_INT,
                                                              PROP_NONE,
                                                              "Postprocess Blur Size",
                                                              "Postprocess Blur Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->blur_post;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Postprocess Dilate Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_INT,
                                                              PROP_NONE,
                                                              "Postprocess Dilate Size",
                                                              "Postprocess Dilate Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->dilate_distance;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Postprocess Feather Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_INT,
                                                              PROP_NONE,
                                                              "Postprocess Feather Size",
                                                              "Postprocess Feather Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->feather_distance;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Despill Strength")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Despill Strength",
                                                              "Despill Strength");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->despill_factor;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Despill Balance")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Despill Balance", "Despill Balance");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->despill_balance;
  }
}

/* The options were converted into inputs. */
static void do_version_keying_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "blur_pre")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "screen_balance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "clip_black")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "clip_white")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "edge_kernel_radius")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[6].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "edge_kernel_tolerance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[7].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "blur_post")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[10].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "dilate_distance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[11].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "feather_distance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[12].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "despill_factor")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[13].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "despill_balance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[14].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_id_mask_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Index")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Index", "Index");
    input->default_value_typed<bNodeSocketValueInt>()->value = node->custom1;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Anti-Alias")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Anti-Alias", "Anti-Alias");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom2);
  }
}

/* The options were converted into inputs. */
static void do_version_id_mask_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "index")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_antialiasing")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_stabilize_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Invert")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Invert", "Invert");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom2);
  }
}

/* The options were converted into inputs. */
static void do_version_stabilize_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                  bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "invert")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_plane_track_deform_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodePlaneTrackDeformData *storage = static_cast<NodePlaneTrackDeformData *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Motion Blur")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Motion Blur", "Motion Blur");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(storage->flag);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Motion Blur Samples")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Motion Blur Samples", "Samples");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->motion_blur_samples;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Motion Blur Shutter")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Motion Blur Shutter", "Shutter");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->motion_blur_shutter;
  }
}

/* The options were converted into inputs. */
static void do_version_plane_track_deform_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                           bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "use_motion_blur")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "motion_blur_samples")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "motion_blur_shutter")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_color_correction_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeColorCorrection *storage = static_cast<NodeColorCorrection *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Master Saturation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Master Saturation",
                                                              "Master Saturation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->master.saturation;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Master Contrast")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Master Contrast", "Master Contrast");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->master.contrast;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Master Gamma")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Master Gamma", "Master Gamma");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->master.gamma;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Master Gain")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Master Gain", "Master Gain");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->master.gain;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Master Lift")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Master Lift", "Master Lift");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->master.lift;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Shadows Saturation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Shadows Saturation",
                                                              "Shadows Saturation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->shadows.saturation;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Shadows Contrast")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Shadows Contrast",
                                                              "Shadows Contrast");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->shadows.contrast;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Shadows Gamma")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Shadows Gamma", "Shadows Gamma");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->shadows.gamma;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Shadows Gain")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Shadows Gain", "Shadows Gain");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->shadows.gain;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Shadows Lift")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Shadows Lift", "Shadows Lift");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->shadows.lift;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Midtones Saturation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Midtones Saturation",
                                                              "Midtones Saturation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->midtones.saturation;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Midtones Contrast")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Midtones Contrast",
                                                              "Midtones Contrast");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->midtones.contrast;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Midtones Gamma")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Midtones Gamma", "Midtones Gamma");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->midtones.gamma;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Midtones Gain")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Midtones Gain", "Midtones Gain");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->midtones.gain;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Midtones Lift")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Midtones Lift", "Midtones Lift");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->midtones.lift;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Highlights Saturation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Highlights Saturation",
                                                              "Highlights Saturation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->highlights.saturation;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Highlights Contrast")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Highlights Contrast",
                                                              "Highlights Contrast");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->highlights.contrast;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Highlights Gamma")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_FACTOR,
                                                              "Highlights Gamma",
                                                              "Highlights Gamma");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->highlights.gamma;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Highlights Gain")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Highlights Gain", "Highlights Gain");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->highlights.gain;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Highlights Lift")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Highlights Lift", "Highlights Lift");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->highlights.lift;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Midtones Start")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Midtones Start", "Midtones Start");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->startmidtones;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Midtones End")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Midtones End", "Midtones End");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->endmidtones;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Apply On Red")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Apply On Red", "Apply On Red");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1 & (1 << 0));
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Apply On Green")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Apply On Green", "Apply On Green");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1 & (1 << 1));
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Apply On Blue")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Apply On Blue", "Apply On Blue");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1 & (1 << 2));
  }
}

/* The options were converted into inputs. */
static void do_version_color_correction_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                         bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "use_motion_blur")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "master_saturation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "master_contrast")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "master_gamma")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "master_gain")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "master_lift")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[6].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "highlights_saturation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[7].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "highlights_contrast")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[8].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "highlights_gamma")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[9].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "highlights_gain")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[10].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "highlights_lift")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[11].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "midtones_saturation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[12].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "midtones_contrast")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[13].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "midtones_gamma")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[14].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "midtones_gain")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[15].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "midtones_lift")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[16].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "shadows_saturation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[17].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "shadows_contrast")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[18].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "shadows_gamma")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[19].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "shadows_gain")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[20].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "shadows_lift")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[21].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "midtones_start")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[22].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "midtones_end")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[23].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "red")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[24].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "green")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[25].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "blue")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[26].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_lens_distortion_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeLensDist *storage = static_cast<NodeLensDist *>(node->storage);
  if (!storage) {
    return;
  }

  /* Use Projector boolean option is now an enum between two types. */
  storage->distortion_type = storage->proj ? CMP_NODE_LENS_DISTORTION_HORIZONTAL :
                                             CMP_NODE_LENS_DISTORTION_RADIAL;

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Jitter")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Jitter", "Jitter");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(storage->jit);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Fit")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Fit", "Fit");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(storage->fit);
  }
}

/* The options were converted into inputs. */
static void do_version_lens_distortion_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                        bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "use_jitter")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_fit")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_projector")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "distortion_type");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_box_mask_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeBoxMask *storage = static_cast<NodeBoxMask *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Position")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_VECTOR, PROP_FACTOR, "Position", "Position");
    input->default_value_typed<bNodeSocketValueVector>()->value[0] = storage->x;
    input->default_value_typed<bNodeSocketValueVector>()->value[1] = storage->y;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_VECTOR, PROP_FACTOR, "Size", "Size");
    input->default_value_typed<bNodeSocketValueVector>()->value[0] = storage->width;
    input->default_value_typed<bNodeSocketValueVector>()->value[1] = storage->height;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Rotation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Rotation", "Rotation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->rotation;
  }
}

/* The options were converted into inputs. */
static void do_version_box_mask_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "x")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
      fcurve->array_index = 0;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "y")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
      fcurve->array_index = 1;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "mask_width")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
      fcurve->array_index = 0;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "mask_height")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
      fcurve->array_index = 1;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "rotation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_ellipse_mask_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeEllipseMask *storage = static_cast<NodeEllipseMask *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Position")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_VECTOR, PROP_FACTOR, "Position", "Position");
    input->default_value_typed<bNodeSocketValueVector>()->value[0] = storage->x;
    input->default_value_typed<bNodeSocketValueVector>()->value[1] = storage->y;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_VECTOR, PROP_FACTOR, "Size", "Size");
    input->default_value_typed<bNodeSocketValueVector>()->value[0] = storage->width;
    input->default_value_typed<bNodeSocketValueVector>()->value[1] = storage->height;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Rotation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Rotation", "Rotation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->rotation;
  }
}

/* The options were converted into inputs. */
static void do_version_ellipse_mask_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                     bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "x")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
      fcurve->array_index = 0;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "y")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
      fcurve->array_index = 1;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "mask_width")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
      fcurve->array_index = 0;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "mask_height")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
      fcurve->array_index = 1;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "rotation")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_sun_beams_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeSunBeams *storage = static_cast<NodeSunBeams *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Source")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_VECTOR, PROP_FACTOR, "Source", "Source");
    input->default_value_typed<bNodeSocketValueVector>()->value[0] = storage->source[0];
    input->default_value_typed<bNodeSocketValueVector>()->value[1] = storage->source[1];
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Length")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Length", "Length");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->ray_length;
  }
}

/* The options were converted into inputs. */
static void do_version_sun_beams_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                  bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "source")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "ray_length")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_directional_blur_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeDBlurData *storage = static_cast<NodeDBlurData *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Samples")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Samples", "Samples");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->iter;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Center")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_VECTOR, PROP_FACTOR, "Center", "Center");
    input->default_value_typed<bNodeSocketValueVector>()->value[0] = storage->center_x;
    input->default_value_typed<bNodeSocketValueVector>()->value[1] = storage->center_y;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Translation Amount")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Translation Amount", "Amount");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->distance;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Translation Direction")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Translation Direction", "Direction");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->angle;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Rotation")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Rotation", "Rotation");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->spin;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Scale")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Scale", "Scale");
    /* Scale was previously minus 1. */
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->zoom + 1.0f;
  }
}

/* The options were converted into inputs. */
static void do_version_directional_blur_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                         bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "iterations")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "center_x")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
      fcurve->array_index = 0;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "center_y")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
      fcurve->array_index = 1;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "spin")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "zoom")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
      /* Scale was previously minus 1. */
      adjust_fcurve_key_frame_values(
          fcurve, PROP_FLOAT, [&](const float value) { return value + 1.0f; });
    }
    else if (BLI_str_endswith(fcurve->rna_path, "distance")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "angle")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[6].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_bilateral_blur_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeBilateralBlurData *storage = static_cast<NodeBilateralBlurData *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Size")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Size", "Size");
    input->default_value_typed<bNodeSocketValueInt>()->value = std::ceil(storage->iter +
                                                                         storage->sigma_space);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Threshold")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Threshold", "Threshold");
    /* Threshold was previously multiplied by 3. */
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->sigma_color / 3.0f;
  }
}

/* The options were converted into inputs. */
static void do_version_bilateral_blur_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                       bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "iterations")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "sigma_color")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The Use Alpha option and Alpha input were removed. If Use Alpha was disabled, set the input
 * alpha to 1 using a Set Alpha node, otherwise, if the Alpha input is linked, set it to the image
 * using a Set Alpha node. */
static void do_version_composite_viewer_remove_alpha(bNodeTree *node_tree)
{
  /* Maps the names of the viewer and composite nodes to the links going into their image and alpha
   * inputs. */
  blender::Map<std::string, bNodeLink *> node_to_image_link_map;
  blender::Map<std::string, bNodeLink *> node_to_alpha_link_map;

  /* Find links going into the composite and viewer nodes. */
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
    if (!ELEM(link->tonode->type_legacy, CMP_NODE_COMPOSITE, CMP_NODE_VIEWER)) {
      continue;
    }

    if (blender::StringRef(link->tosock->identifier) == "Image") {
      node_to_image_link_map.add_new(link->tonode->name, link);
    }
    else if (blender::StringRef(link->tosock->identifier) == "Alpha") {
      node_to_alpha_link_map.add_new(link->tonode->name, link);
    }
  }

  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    if (!ELEM(node->type_legacy, CMP_NODE_COMPOSITE, CMP_NODE_VIEWER)) {
      continue;
    }

    bNodeSocket *image_input = blender::bke::node_find_socket(*node, SOCK_IN, "Image");

    /* Use Alpha is disabled, so we need to set the alpha to 1. */
    if (node->custom2 & CMP_NODE_OUTPUT_IGNORE_ALPHA) {
      /* Nothing is connected to the image, just set the default value alpha to 1. */
      if (!node_to_image_link_map.contains(node->name)) {
        image_input->default_value_typed<bNodeSocketValueRGBA>()->value[3] = 1.0f;
        continue;
      }

      bNodeLink *image_link = node_to_image_link_map.lookup(node->name);

      /* Add a set alpha node and make the necessary connections. */
      bNode *set_alpha_node = blender::bke::node_add_static_node(
          nullptr, *node_tree, CMP_NODE_SETALPHA);
      set_alpha_node->parent = node->parent;
      set_alpha_node->location[0] = node->location[0] - node->width - 20.0f;
      set_alpha_node->location[1] = node->location[1];

      NodeSetAlpha *data = static_cast<NodeSetAlpha *>(set_alpha_node->storage);
      data->mode = CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA;

      bNodeSocket *set_alpha_input = blender::bke::node_find_socket(
          *set_alpha_node, SOCK_IN, "Image");
      bNodeSocket *set_alpha_output = blender::bke::node_find_socket(
          *set_alpha_node, SOCK_OUT, "Image");

      version_node_add_link(*node_tree,
                            *image_link->fromnode,
                            *image_link->fromsock,
                            *set_alpha_node,
                            *set_alpha_input);
      version_node_add_link(*node_tree, *set_alpha_node, *set_alpha_output, *node, *image_input);

      blender::bke::node_remove_link(node_tree, *image_link);
      continue;
    }

    /* If we don't continue, the alpha input is connected and Use Alpha is enabled, so we need to
     * set the alpha using a Set Alpha node. */
    if (!node_to_alpha_link_map.contains(node->name)) {
      continue;
    }

    bNodeLink *alpha_link = node_to_alpha_link_map.lookup(node->name);

    /* Add a set alpha node and make the necessary connections. */
    bNode *set_alpha_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_SETALPHA);
    set_alpha_node->parent = node->parent;
    set_alpha_node->location[0] = node->location[0] - node->width - 20.0f;
    set_alpha_node->location[1] = node->location[1];

    NodeSetAlpha *data = static_cast<NodeSetAlpha *>(set_alpha_node->storage);
    data->mode = CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA;

    bNodeSocket *set_alpha_input = blender::bke::node_find_socket(
        *set_alpha_node, SOCK_IN, "Image");
    bNodeSocket *set_alpha_alpha = blender::bke::node_find_socket(
        *set_alpha_node, SOCK_IN, "Alpha");
    bNodeSocket *set_alpha_output = blender::bke::node_find_socket(
        *set_alpha_node, SOCK_OUT, "Image");

    version_node_add_link(*node_tree,
                          *alpha_link->fromnode,
                          *alpha_link->fromsock,
                          *set_alpha_node,
                          *set_alpha_alpha);
    version_node_add_link(*node_tree, *set_alpha_node, *set_alpha_output, *node, *image_input);
    blender::bke::node_remove_link(node_tree, *alpha_link);

    if (!node_to_image_link_map.contains(node->name)) {
      copy_v4_v4(set_alpha_input->default_value_typed<bNodeSocketValueRGBA>()->value,
                 image_input->default_value_typed<bNodeSocketValueRGBA>()->value);
    }
    else {
      bNodeLink *image_link = node_to_image_link_map.lookup(node->name);
      version_node_add_link(*node_tree,
                            *image_link->fromnode,
                            *image_link->fromsock,
                            *set_alpha_node,
                            *set_alpha_input);
      blender::bke::node_remove_link(node_tree, *image_link);
    }
  }
}

/* The Convert Premultiplied option was removed. If enabled, a convert alpha node will be added
 * before and after the node to perform the adjustment in straight alpha. */
static void do_version_bright_contrast_remove_premultiplied(bNodeTree *node_tree)
{
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (link->tonode->type_legacy != CMP_NODE_BRIGHTCONTRAST) {
      continue;
    }

    if (!bool(link->tonode->custom1)) {
      continue;
    }

    if (blender::StringRef(link->tosock->identifier) != "Image") {
      continue;
    }

    bNode *convert_alpha_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_PREMULKEY);
    convert_alpha_node->parent = link->tonode->parent;
    convert_alpha_node->location[0] = link->tonode->location[0] - link->tonode->width - 20.0f;
    convert_alpha_node->location[1] = link->tonode->location[1];
    convert_alpha_node->custom1 = CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY;

    bNodeSocket *convert_alpha_input = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_IN, "Image");
    bNodeSocket *convert_alpha_output = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_OUT, "Image");

    version_node_add_link(
        *node_tree, *link->fromnode, *link->fromsock, *convert_alpha_node, *convert_alpha_input);
    version_node_add_link(
        *node_tree, *convert_alpha_node, *convert_alpha_output, *link->tonode, *link->tosock);

    blender::bke::node_remove_link(node_tree, *link);
  }

  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (link->fromnode->type_legacy != CMP_NODE_BRIGHTCONTRAST) {
      continue;
    }

    if (!bool(link->fromnode->custom1)) {
      continue;
    }

    bNode *convert_alpha_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_PREMULKEY);
    convert_alpha_node->parent = link->fromnode->parent;
    convert_alpha_node->location[0] = link->fromnode->location[0] + link->fromnode->width + 20.0f;
    convert_alpha_node->location[1] = link->fromnode->location[1];
    convert_alpha_node->custom1 = CMP_NODE_ALPHA_CONVERT_PREMULTIPLY;

    bNodeSocket *convert_alpha_input = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_IN, "Image");
    bNodeSocket *convert_alpha_output = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_OUT, "Image");

    version_node_add_link(
        *node_tree, *link->fromnode, *link->fromsock, *convert_alpha_node, *convert_alpha_input);
    version_node_add_link(
        *node_tree, *convert_alpha_node, *convert_alpha_output, *link->tonode, *link->tosock);

    blender::bke::node_remove_link(node_tree, *link);
  }
}

/* The Premultiply Mix option was removed. If enabled, the image is converted to premultiplied then
 * to straight, and both are mixed using a mix node. */
static void do_version_alpha_over_remove_premultiply(bNodeTree *node_tree)
{
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (link->tonode->type_legacy != CMP_NODE_ALPHAOVER) {
      continue;
    }

    const float mix_factor = static_cast<NodeTwoFloats *>(link->tonode->storage)->x;
    if (mix_factor == 0.0f) {
      continue;
    }

    if (blender::StringRef(link->tosock->identifier) != "Image_001") {
      continue;
    }

    /* Disable Convert Premultiplied option, since this will be done manually. */
    link->tonode->custom1 = false;

    bNode *mix_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MIX);
    mix_node->parent = link->tonode->parent;
    mix_node->location[0] = link->tonode->location[0] - link->tonode->width - 20.0f;
    mix_node->location[1] = link->tonode->location[1];
    static_cast<NodeShaderMix *>(mix_node->storage)->data_type = SOCK_RGBA;

    bNodeSocket *mix_a_input = blender::bke::node_find_socket(*mix_node, SOCK_IN, "A_Color");
    bNodeSocket *mix_b_input = blender::bke::node_find_socket(*mix_node, SOCK_IN, "B_Color");
    bNodeSocket *mix_factor_input = blender::bke::node_find_socket(
        *mix_node, SOCK_IN, "Factor_Float");
    bNodeSocket *mix_output = blender::bke::node_find_socket(*mix_node, SOCK_OUT, "Result_Color");

    mix_factor_input->default_value_typed<bNodeSocketValueFloat>()->value = mix_factor;

    bNode *to_straight_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_PREMULKEY);
    to_straight_node->parent = link->tonode->parent;
    to_straight_node->location[0] = mix_node->location[0] - mix_node->width - 20.0f;
    to_straight_node->location[1] = mix_node->location[1];
    to_straight_node->custom1 = CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY;

    bNodeSocket *to_straight_input = blender::bke::node_find_socket(
        *to_straight_node, SOCK_IN, "Image");
    bNodeSocket *to_straight_output = blender::bke::node_find_socket(
        *to_straight_node, SOCK_OUT, "Image");

    bNode *to_premultiplied_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_PREMULKEY);
    to_premultiplied_node->parent = link->tonode->parent;
    to_premultiplied_node->location[0] = to_straight_node->location[0] - to_straight_node->width -
                                         20.0f;
    to_premultiplied_node->location[1] = to_straight_node->location[1];
    to_premultiplied_node->custom1 = CMP_NODE_ALPHA_CONVERT_PREMULTIPLY;

    bNodeSocket *to_premultiplied_input = blender::bke::node_find_socket(
        *to_premultiplied_node, SOCK_IN, "Image");
    bNodeSocket *to_premultiplied_output = blender::bke::node_find_socket(
        *to_premultiplied_node, SOCK_OUT, "Image");

    version_node_add_link(*node_tree,
                          *link->fromnode,
                          *link->fromsock,
                          *to_premultiplied_node,
                          *to_premultiplied_input);
    version_node_add_link(*node_tree,
                          *to_premultiplied_node,
                          *to_premultiplied_output,
                          *to_straight_node,
                          *to_straight_input);
    version_node_add_link(
        *node_tree, *to_premultiplied_node, *to_premultiplied_output, *mix_node, *mix_b_input);
    version_node_add_link(
        *node_tree, *to_straight_node, *to_straight_output, *mix_node, *mix_a_input);
    version_node_add_link(*node_tree, *mix_node, *mix_output, *link->tonode, *link->tosock);

    blender::bke::node_remove_link(node_tree, *link);
  }
}

/* The options were converted into inputs. */
static void do_version_alpha_over_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Straight Alpha")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Straight Alpha", "Straight Alpha");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1);
  }
}

/* The options were converted into inputs. */
static void do_version_alpha_over_node_options_to_inputs_animation(bNodeTree *node_tree,
                                                                   bNode *node)
{
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
    if (BLI_str_endswith(fcurve->rna_path, "use_premultiply")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
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

void do_versions_after_linking_450(FileData *fd, Main *bmain)
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 8)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_version_convert_to_generic_nodes_after_linking(bmain, ntree, id);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 12)) {
    version_node_socket_index_animdata(bmain, NTREE_COMPOSIT, CMP_NODE_GLARE, 3, 1, 14);
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_version_new_glare_suppress_input(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  /* For each F-Curve, set the F-Curve flags based on the property type it animates. This is to
   * correct F-Curves created while the bug (#136347) was in active use. Since this bug did not
   * appear before 4.4, and this versioning code has a bit of a performance impact (going over all
   * F-Curves of all Actions, and resolving them all to their RNA properties), it will be skipped
   * if the blend file is old enough to not be affected. */
  if (MAIN_VERSION_FILE_ATLEAST(bmain, 404, 0) && !MAIN_VERSION_FILE_ATLEAST(bmain, 405, 13)) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 14)) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 20)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_GLARE) {
            do_version_glare_node_star_45_option_to_input_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 22)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_BOKEHIMAGE) {
            do_version_bokeh_image_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 23)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_TIME) {
            do_version_time_curve_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 24)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_MASK) {
            do_version_mask_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 25)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_SWITCH) {
            do_version_switch_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 26)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_SPLIT) {
            do_version_split_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 27)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_INVERT) {
            do_version_invert_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 28)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_ZCOMBINE) {
            do_version_z_combine_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 29)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_TONEMAP) {
            do_version_tone_map_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 30)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DILATEERODE) {
            do_version_dilate_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 31)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_INPAINT) {
            do_version_inpaint_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 32)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_PIXELATE) {
            do_version_pixelate_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 33)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_KUWAHARA) {
            do_version_kuwahara_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 34)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DESPECKLE) {
            do_version_despeckle_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 35)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DENOISE) {
            do_version_denoise_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 36)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_ANTIALIASING) {
            do_version_anti_alias_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 37)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_VECBLUR) {
            do_version_vector_blur_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 38)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_CHANNEL_MATTE) {
            do_version_channel_matte_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 39)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_CHROMA_MATTE) {
            do_version_chroma_matte_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 40)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLOR_MATTE) {
            do_version_color_matte_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 41)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DIFF_MATTE) {
            do_version_difference_matte_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 42)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DIST_MATTE) {
            do_version_distance_matte_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 43)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_LUMA_MATTE) {
            do_version_luminance_matte_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 44)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLOR_SPILL) {
            do_version_color_spill_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 45)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_KEYINGSCREEN) {
            do_version_keying_screen_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 47)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_KEYING) {
            do_version_keying_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 48)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_ID_MASK) {
            do_version_id_mask_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 49)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_STABILIZE2D) {
            do_version_stabilize_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 50)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_PLANETRACKDEFORM) {
            do_version_plane_track_deform_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 52)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLORCORRECTION) {
            do_version_color_correction_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 53)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_LENSDIST) {
            do_version_lens_distortion_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 54)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_MASK_BOX) {
            do_version_box_mask_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 55)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_MASK_ELLIPSE) {
            do_version_ellipse_mask_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 58)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_SUNBEAMS) {
            do_version_sun_beams_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 59)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DBLUR) {
            do_version_directional_blur_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 60)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_BILATERALBLUR) {
            do_version_bilateral_blur_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 64)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_ALPHAOVER) {
            do_version_alpha_over_node_options_to_inputs_animation(node_tree, node);
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

static void do_version_node_curve_to_mesh_scale_input(bNodeTree *tree)
{
  using namespace blender;
  Set<bNode *> curve_to_mesh_nodes;
  LISTBASE_FOREACH (bNode *, node, &tree->nodes) {
    if (STREQ(node->idname, "GeometryNodeCurveToMesh")) {
      curve_to_mesh_nodes.add(node);
    }
  }

  for (bNode *curve_to_mesh : curve_to_mesh_nodes) {
    if (bke::node_find_socket(*curve_to_mesh, SOCK_IN, "Scale")) {
      /* Make versioning idempotent. */
      continue;
    }
    version_node_add_socket_if_not_exist(
        tree, curve_to_mesh, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Scale", "Scale");

    bNode &named_attribute = version_node_add_empty(*tree, "GeometryNodeInputNamedAttribute");
    NodeGeometryInputNamedAttribute *named_attribute_storage =
        MEM_callocN<NodeGeometryInputNamedAttribute>(__func__);
    named_attribute_storage->data_type = CD_PROP_FLOAT;
    named_attribute.storage = named_attribute_storage;
    named_attribute.parent = curve_to_mesh->parent;
    named_attribute.location[0] = curve_to_mesh->location[0] - 25;
    named_attribute.location[1] = curve_to_mesh->location[1];
    named_attribute.flag &= ~NODE_SELECT;

    bNodeSocket *name_input = version_node_add_socket_if_not_exist(
        tree, &named_attribute, SOCK_IN, SOCK_STRING, PROP_NONE, "Name", "Name");
    STRNCPY(name_input->default_value_typed<bNodeSocketValueString>()->value, "radius");

    version_node_add_socket_if_not_exist(
        tree, &named_attribute, SOCK_OUT, SOCK_BOOLEAN, PROP_NONE, "Exists", "Exists");
    version_node_add_socket_if_not_exist(
        tree, &named_attribute, SOCK_OUT, SOCK_FLOAT, PROP_NONE, "Attribute", "Attribute");

    bNode &switch_node = version_node_add_empty(*tree, "GeometryNodeSwitch");
    NodeSwitch *switch_storage = MEM_callocN<NodeSwitch>(__func__);
    switch_storage->input_type = SOCK_FLOAT;
    switch_node.storage = switch_storage;
    switch_node.parent = curve_to_mesh->parent;
    switch_node.location[0] = curve_to_mesh->location[0] - 25;
    switch_node.location[1] = curve_to_mesh->location[1];
    switch_node.flag &= ~NODE_SELECT;

    version_node_add_socket_if_not_exist(
        tree, &switch_node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Switch", "Switch");
    bNodeSocket *false_input = version_node_add_socket_if_not_exist(
        tree, &switch_node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "False", "False");
    false_input->default_value_typed<bNodeSocketValueFloat>()->value = 1.0f;

    version_node_add_socket_if_not_exist(
        tree, &switch_node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "True", "True");

    version_node_add_link(*tree,
                          named_attribute,
                          *bke::node_find_socket(named_attribute, SOCK_OUT, "Exists"),
                          switch_node,
                          *bke::node_find_socket(switch_node, SOCK_IN, "Switch"));
    version_node_add_link(*tree,
                          named_attribute,
                          *bke::node_find_socket(named_attribute, SOCK_OUT, "Attribute"),
                          switch_node,
                          *bke::node_find_socket(switch_node, SOCK_IN, "True"));

    version_node_add_socket_if_not_exist(
        tree, &switch_node, SOCK_OUT, SOCK_FLOAT, PROP_NONE, "Output", "Output");

    version_node_add_link(*tree,
                          switch_node,
                          *bke::node_find_socket(switch_node, SOCK_OUT, "Output"),
                          *curve_to_mesh,
                          *bke::node_find_socket(*curve_to_mesh, SOCK_IN, "Scale"));
  }
}

static bool strip_effect_overdrop_to_alphaover(Strip *strip, void * /*user_data*/)
{
  if (strip->type == STRIP_TYPE_OVERDROP_REMOVED) {
    strip->type = STRIP_TYPE_ALPHAOVER;
  }
  if (strip->blend_mode == STRIP_TYPE_OVERDROP_REMOVED) {
    strip->blend_mode = STRIP_TYPE_ALPHAOVER;
  }
  return true;
}

static void version_sequencer_update_overdrop(Main *bmain)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->ed != nullptr) {
      blender::seq::for_each_callback(
          &scene->ed->seqbase, strip_effect_overdrop_to_alphaover, nullptr);
    }
  }
}

static void asset_browser_add_list_view(Main *bmain)
{
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype != SPACE_FILE) {
          continue;
        }
        SpaceFile *sfile = reinterpret_cast<SpaceFile *>(sl);
        if (sfile->params) {
          if (sfile->params->list_thumbnail_size == 0) {
            sfile->params->list_thumbnail_size = 16;
          }
          if (sfile->params->list_column_size == 0) {
            sfile->params->list_column_size = 500;
          }
        }
        if (sfile->asset_params) {
          if (sfile->asset_params->base_params.list_thumbnail_size == 0) {
            sfile->asset_params->base_params.list_thumbnail_size = 32;
          }
          if (sfile->asset_params->base_params.list_column_size == 0) {
            sfile->asset_params->base_params.list_column_size = 220;
          }
          sfile->asset_params->base_params.details_flags = 0;
        }
      }
    }
  }
}

static void version_show_texpaint_to_show_uv(Main *bmain)
{
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == SPACE_IMAGE) {
          SpaceImage *sima = reinterpret_cast<SpaceImage *>(sl);
          if (sima->flag & SI_NO_DRAW_TEXPAINT) {
            sima->flag |= SI_NO_DRAW_UV_GUIDE;
          }
        }
      }
    }
  }
}

static void version_set_uv_face_overlay_defaults(Main *bmain)
{
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == SPACE_IMAGE) {
          SpaceImage *sima = reinterpret_cast<SpaceImage *>(sl);
          /* Remove ID Code from screen name */
          const char *workspace_name = screen->id.name + 2;
          /* Don't set uv_face_opacity for Texture Paint or Shading since these are workspaces
           * where it's important to have unobstructed view of the Image Editor to see Image
           * Textures. UV Editing is the only other default workspace with an Image Editor.*/
          if (STREQ(workspace_name, "UV Editing")) {
            sima->uv_face_opacity = 1.0f;
          }
        }
      }
    }
  }
}

static void version_convert_sculpt_planar_brushes(Main *bmain)
{
  LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
    if (ELEM(brush->sculpt_brush_type,
             SCULPT_BRUSH_TYPE_FLATTEN,
             SCULPT_BRUSH_TYPE_FILL,
             SCULPT_BRUSH_TYPE_SCRAPE))
    {
      if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_FLATTEN) {
        brush->plane_height = 1.0f;
        brush->plane_depth = 1.0f;
        brush->area_radius_factor = 1.0f;
        brush->plane_inversion_mode = BRUSH_PLANE_INVERT_DISPLACEMENT;
      }

      if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_FILL) {
        brush->plane_height = 0.0f;
        brush->plane_depth = 1.0f;
        brush->plane_inversion_mode = brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL ?
                                          BRUSH_PLANE_SWAP_HEIGHT_AND_DEPTH :
                                          BRUSH_PLANE_INVERT_DISPLACEMENT;
      }

      if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_SCRAPE) {
        brush->plane_height = 1.0f;
        brush->plane_depth = 0.0f;
        brush->plane_inversion_mode = brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL ?
                                          BRUSH_PLANE_SWAP_HEIGHT_AND_DEPTH :
                                          BRUSH_PLANE_INVERT_DISPLACEMENT;
      }

      if (brush->flag & BRUSH_PLANE_TRIM) {
        brush->plane_height *= brush->plane_trim;
        brush->plane_depth *= brush->plane_trim;
      }

      brush->stabilize_normal = (brush->flag & BRUSH_ORIGINAL_NORMAL) ? 1.0f : 0.0f;
      brush->stabilize_plane = (brush->flag & BRUSH_ORIGINAL_PLANE) ? 1.0f : 0.0f;
      brush->flag &= ~BRUSH_ORIGINAL_NORMAL;
      brush->flag &= ~BRUSH_ORIGINAL_PLANE;

      brush->sculpt_brush_type = SCULPT_BRUSH_TYPE_PLANE;
    }
  }
}

void blo_do_versions_450(FileData *fd, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 1)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = blender::seq::editing_get(scene);
      if (ed != nullptr) {
        blender::seq::for_each_callback(&ed->seqbase, versioning_convert_seq_text_anchor, nullptr);
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
      Editing *ed = blender::seq::editing_get(scene);
      if (ed != nullptr) {
        blender::seq::for_each_callback(&ed->seqbase, versioning_clear_strip_unused_flag, scene);
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 2)) {
    version_sequencer_update_overdrop(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 4)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        do_version_node_curve_to_mesh_scale_input(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 5)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      tool_settings->snap_flag_seq |= SCE_SNAP;

      SequencerToolSettings *sequencer_tool_settings = blender::seq::tool_settings_ensure(scene);
      sequencer_tool_settings->snap_mode |= SEQ_SNAP_TO_FRAME_RANGE;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 6)) {
    asset_browser_add_list_view(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 7)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (STREQ(node->idname, "GeometryNodeStoreNamedGrid")) {
            switch (node->custom1) {
              case CD_PROP_FLOAT:
                node->custom1 = VOLUME_GRID_FLOAT;
                break;
              case CD_PROP_FLOAT2:
              case CD_PROP_FLOAT3:
                node->custom1 = VOLUME_GRID_VECTOR_FLOAT;
                break;
              default:
                node->custom1 = VOLUME_GRID_FLOAT;
                break;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 8)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_version_convert_to_generic_nodes(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 9)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_FILE) {
            continue;
          }
          SpaceFile *sfile = reinterpret_cast<SpaceFile *>(sl);
          if (sfile->asset_params) {
            sfile->asset_params->import_flags |= FILE_ASSET_IMPORT_INSTANCE_COLLECTIONS_ON_LINK;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 15)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_SCALE) {
          continue;
        }
        if (node->storage != nullptr) {
          continue;
        }
        NodeScaleData *data = MEM_callocN<NodeScaleData>(__func__);
        data->interpolation = CMP_NODE_INTERPOLATION_BILINEAR;
        node->storage = data;
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 16)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->grease_pencil_settings.smaa_threshold_render =
          scene->grease_pencil_settings.smaa_threshold;
      scene->grease_pencil_settings.aa_samples = 1;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 17)) {
    version_show_texpaint_to_show_uv(bmain);
    version_set_uv_face_overlay_defaults(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 18)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == CMP_NODE_CORNERPIN) {
            node->custom1 = CMP_NODE_CORNER_PIN_INTERPOLATION_ANISOTROPIC;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 19)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_PROPERTIES) {
            SpaceProperties *sbuts = reinterpret_cast<SpaceProperties *>(sl);
            /* Translates to 0xFFFFFFFF, so other tabs can be added without versioning. */
            sbuts->visible_tabs = uint(-1);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 20)) {
    /* Older files uses non-UTF8 aware string copy, ensure names are valid UTF8.
     * The slot names are not unique so no further changes are needed. */
    LISTBASE_FOREACH (Image *, image, &bmain->images) {
      LISTBASE_FOREACH (RenderSlot *, slot, &image->renderslots) {
        if (slot->name[0]) {
          BLI_str_utf8_invalid_strip(slot->name, STRNLEN(slot->name));
        }
      }
    }
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.ppm_factor = 72.0f;
      scene->r.ppm_base = 0.0254f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 21)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_GLARE) {
            do_version_glare_node_star_45_option_to_input(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 22)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_BOKEHIMAGE) {
            do_version_bokeh_image_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 23)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_TIME) {
            do_version_time_curve_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 24)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_MASK) {
            do_version_mask_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 25)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_SWITCH) {
            do_version_switch_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 26)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_SPLIT) {
            do_version_split_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 27)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_INVERT) {
            do_version_invert_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 28)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_ZCOMBINE) {
            do_version_z_combine_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 29)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_TONEMAP) {
            do_version_tone_map_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 30)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DILATEERODE) {
            do_version_dilate_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 31)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_INPAINT) {
            do_version_inpaint_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 32)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_PIXELATE) {
            do_version_pixelate_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 33)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_KUWAHARA) {
            do_version_kuwahara_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 34)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DESPECKLE) {
            do_version_despeckle_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 35)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DENOISE) {
            do_version_denoise_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 36)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_ANTIALIASING) {
            do_version_anti_alias_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 37)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_VECBLUR) {
            do_version_vector_blur_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 38)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_CHANNEL_MATTE) {
            do_version_channel_matte_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 39)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_CHROMA_MATTE) {
            do_version_chroma_matte_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 40)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLOR_MATTE) {
            do_version_color_matte_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 41)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DIFF_MATTE) {
            do_version_difference_matte_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 42)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DIST_MATTE) {
            do_version_distance_matte_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 43)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_LUMA_MATTE) {
            do_version_luminance_matte_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 44)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLOR_SPILL) {
            do_version_color_spill_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 45)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_KEYINGSCREEN) {
            do_version_keying_screen_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 46)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_WINDOW) {
                region->v2d.keepzoom |= V2D_KEEPZOOM;
                region->v2d.keepofs |= V2D_KEEPOFS_X | V2D_KEEPOFS_Y;
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 47)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_KEYING) {
            do_version_keying_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 48)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_ID_MASK) {
            do_version_id_mask_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 49)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_STABILIZE2D) {
            do_version_stabilize_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 50)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_PLANETRACKDEFORM) {
            do_version_plane_track_deform_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 51)) {
    const Object *dob = DNA_struct_default_get(Object);
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      object->shadow_terminator_normal_offset = dob->shadow_terminator_normal_offset;
      object->shadow_terminator_geometry_offset = dob->shadow_terminator_geometry_offset;
      object->shadow_terminator_shading_offset = dob->shadow_terminator_shading_offset;
      /* Copy Cycles' property into Blender Object. */
      IDProperty *cob = version_cycles_properties_from_ID(&object->id);
      if (cob) {
        object->shadow_terminator_geometry_offset = version_cycles_property_float(
            cob, "shadow_terminator_geometry_offset", dob->shadow_terminator_geometry_offset);
        object->shadow_terminator_shading_offset = version_cycles_property_float(
            cob, "shadow_terminator_offset", dob->shadow_terminator_shading_offset);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 52)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLORCORRECTION) {
            do_version_color_correction_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 53)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_LENSDIST) {
            do_version_lens_distortion_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 54)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_MASK_BOX) {
            do_version_box_mask_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 55)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_MASK_ELLIPSE) {
            do_version_ellipse_mask_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 56)) {
    version_convert_sculpt_planar_brushes(bmain);
  }

  /* Enforce that bone envelope radii match for parent and connected children. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 57)) {
    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      blender::animrig::ANIM_armature_foreach_bone(&arm->bonebase, [](Bone *bone) {
        if (bone->parent && (bone->flag & BONE_CONNECTED)) {
          bone->rad_head = bone->parent->rad_tail;
        }
      });
      if (arm->edbo) {
        LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
          if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
            ebone->rad_head = ebone->parent->rad_tail;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 58)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_SUNBEAMS) {
            do_version_sun_beams_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 59)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_DBLUR) {
            do_version_directional_blur_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 60)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_BILATERALBLUR) {
            do_version_bilateral_blur_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 61)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        do_version_composite_viewer_remove_alpha(node_tree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 62)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        do_version_bright_contrast_remove_premultiplied(node_tree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 63)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        do_version_alpha_over_remove_premultiply(node_tree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 64)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_ALPHAOVER) {
            do_version_alpha_over_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 65)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_WINDOW) {
                region->v2d.flag |= V2D_ZOOM_IGNORE_KEEPOFS;
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 66)) {
    /* Clear unused draw flag (used to be SEQ_DRAW_BACKDROP). */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *space_sequencer = (SpaceSeq *)sl;
            space_sequencer->draw_flag &= ~SEQ_DRAW_UNUSED_0;
          }
        }
      }
    }
  }

  /* Always run this versioning (keep at the bottom of the function). Meshes are written with the
   * legacy format which always needs to be converted to the new format on file load. To be moved
   * to a subversion check in 5.0. */
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
