/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include <fmt/format.h>

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_defaults.h"
#include "DNA_light_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_sys_types.h"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_fcurve.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_paint.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_armature_iter.hh"

#include "RNA_access.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// static CLG_LogRef LOG = {"blend.doversion"};

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

/**
 * Fixes situation when `CurvesGeometry` instance has curves with `NURBS_KNOT_MODE_CUSTOM`, but has
 * no custom knots.
 */
static void fix_curve_nurbs_knot_mode_custom(Main *bmain)
{
  auto fix_curves = [](blender::bke::CurvesGeometry &curves) {
    if (curves.custom_knots != nullptr) {
      return;
    }

    int8_t *knot_modes = static_cast<int8_t *>(CustomData_get_layer_named_for_write(
        &curves.curve_data_legacy, CD_PROP_INT8, "knots_mode", curves.curve_num));
    if (knot_modes == nullptr) {
      return;
    }

    for (const int curve : curves.curves_range()) {
      int8_t &knot_mode = knot_modes[curve];
      if (knot_mode == NURBS_KNOT_MODE_CUSTOM) {
        knot_mode = NURBS_KNOT_MODE_NORMAL;
      }
    }
    curves.nurbs_custom_knots_update_size();
  };

  LISTBASE_FOREACH (Curves *, curves_id, &bmain->hair_curves) {
    blender::bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    fix_curves(curves);
  }

  LISTBASE_FOREACH (GreasePencil *, grease_pencil, &bmain->grease_pencils) {
    for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
      if (base->type != GP_DRAWING) {
        continue;
      }
      blender::bke::greasepencil::Drawing &drawing =
          reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
      fix_curves(drawing.strokes_for_write());
    }
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

/* A new Clamp boolean input was added that either enables clamping or disables it. Previously,
 * Clamp was disabled when the maximum was zero. So we enable Clamp for non zero or linked maximum
 * input. */
static void do_version_new_glare_clamp_input(bNodeTree *node_tree)
{
  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    if (node->type_legacy != CMP_NODE_GLARE) {
      continue;
    }

    bNodeSocket *clamp_input = blender::bke::node_find_socket(*node, SOCK_IN, "Clamp Highlights");
    bNodeSocket *maximum_input = blender::bke::node_find_socket(
        *node, SOCK_IN, "Maximum Highlights");

    const float maximum = maximum_input->default_value_typed<bNodeSocketValueFloat>()->value;
    if (version_node_socket_is_used(maximum_input) || maximum != 0.0) {
      clamp_input->default_value_typed<bNodeSocketValueBoolean>()->value = true;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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

  MEM_freeN(storage);
  node->storage = nullptr;
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
    if (!ELEM(link->tonode->type_legacy, CMP_NODE_COMPOSITE_DEPRECATED, CMP_NODE_VIEWER)) {
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
    if (!ELEM(node->type_legacy, CMP_NODE_COMPOSITE_DEPRECATED, CMP_NODE_VIEWER)) {
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

      bNodeSocket *set_alpha_input = blender::bke::node_find_socket(
          *set_alpha_node, SOCK_IN, "Image");
      bNodeSocket *set_alpha_type = blender::bke::node_find_socket(
          *set_alpha_node, SOCK_IN, "Type");
      bNodeSocket *set_alpha_output = blender::bke::node_find_socket(
          *set_alpha_node, SOCK_OUT, "Image");

      set_alpha_type->default_value_typed<bNodeSocketValueMenu>()->value =
          CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA;

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

    bNodeSocket *set_alpha_input = blender::bke::node_find_socket(
        *set_alpha_node, SOCK_IN, "Image");
    bNodeSocket *set_alpha_alpha = blender::bke::node_find_socket(
        *set_alpha_node, SOCK_IN, "Alpha");
    bNodeSocket *set_alpha_type = blender::bke::node_find_socket(*set_alpha_node, SOCK_IN, "Type");
    bNodeSocket *set_alpha_output = blender::bke::node_find_socket(
        *set_alpha_node, SOCK_OUT, "Image");

    set_alpha_type->default_value_typed<bNodeSocketValueMenu>()->value =
        CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA;

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

    bNodeSocket *convert_alpha_input = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_IN, "Image");
    bNodeSocket *convert_alpha_type = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_IN, "Type");
    bNodeSocket *convert_alpha_output = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_OUT, "Image");

    convert_alpha_type->default_value_typed<bNodeSocketValueMenu>()->value =
        CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY;

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

    bNodeSocket *convert_alpha_input = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_IN, "Image");
    bNodeSocket *convert_alpha_type = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_IN, "Type");
    bNodeSocket *convert_alpha_output = blender::bke::node_find_socket(
        *convert_alpha_node, SOCK_OUT, "Image");

    convert_alpha_type->default_value_typed<bNodeSocketValueMenu>()->value =
        CMP_NODE_ALPHA_CONVERT_PREMULTIPLY;

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

    bNodeSocket *to_straight_input = blender::bke::node_find_socket(
        *to_straight_node, SOCK_IN, "Image");
    bNodeSocket *to_straight_type = blender::bke::node_find_socket(
        *to_straight_node, SOCK_IN, "Type");
    bNodeSocket *to_straight_output = blender::bke::node_find_socket(
        *to_straight_node, SOCK_OUT, "Image");

    to_straight_type->default_value_typed<bNodeSocketValueMenu>()->value =
        CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY;

    bNode *to_premultiplied_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_PREMULKEY);
    to_premultiplied_node->parent = link->tonode->parent;
    to_premultiplied_node->location[0] = to_straight_node->location[0] - to_straight_node->width -
                                         20.0f;
    to_premultiplied_node->location[1] = to_straight_node->location[1];

    bNodeSocket *to_premultiplied_input = blender::bke::node_find_socket(
        *to_premultiplied_node, SOCK_IN, "Image");
    bNodeSocket *to_premultiplied_type = blender::bke::node_find_socket(
        *to_premultiplied_node, SOCK_IN, "Type");
    bNodeSocket *to_premultiplied_output = blender::bke::node_find_socket(
        *to_premultiplied_node, SOCK_OUT, "Image");

    to_premultiplied_type->default_value_typed<bNodeSocketValueMenu>()->value =
        CMP_NODE_ALPHA_CONVERT_PREMULTIPLY;

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

  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    if (node->type_legacy == CMP_NODE_ALPHAOVER) {
      NodeTwoFloats *storage = static_cast<NodeTwoFloats *>(node->storage);
      MEM_freeN(storage);
      node->storage = nullptr;
    }
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

/* The options were converted into inputs. */
static void do_version_bokeh_blur_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Extend Bounds")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Extend Bounds", "Extend Bounds");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1);
  }
}

/* The options were converted into inputs. */
static void do_version_bokeh_blur_node_options_to_inputs_animation(bNodeTree *node_tree,
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
    if (BLI_str_endswith(fcurve->rna_path, "use_extended_bounds")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The XY Offset option was removed. If enabled, the image is translated in relative space using X
 * and Y, so add a Translate node to achieve the same function. */
static void do_version_scale_node_remove_translate(bNodeTree *node_tree)
{
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (link->fromnode->type_legacy != CMP_NODE_SCALE) {
      continue;
    }

    if (link->fromnode->custom1 != CMP_NODE_SCALE_RENDER_SIZE) {
      continue;
    }

    const float x = link->fromnode->custom3;
    const float y = link->fromnode->custom4;
    if (x == 0.0f && y == 0.0f) {
      continue;
    }

    bNode *translate_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_TRANSLATE);
    translate_node->parent = link->fromnode->parent;
    translate_node->location[0] = link->fromnode->location[0] + link->fromnode->width + 20.0f;
    translate_node->location[1] = link->fromnode->location[1];
    static_cast<NodeTranslateData *>(translate_node->storage)->interpolation =
        static_cast<NodeScaleData *>(link->fromnode->storage)->interpolation;
    static_cast<NodeTranslateData *>(translate_node->storage)->relative = true;

    bNodeSocket *translate_image_input = blender::bke::node_find_socket(
        *translate_node, SOCK_IN, "Image");
    bNodeSocket *translate_x_input = blender::bke::node_find_socket(*translate_node, SOCK_IN, "X");
    bNodeSocket *translate_y_input = blender::bke::node_find_socket(*translate_node, SOCK_IN, "Y");
    bNodeSocket *translate_image_output = blender::bke::node_find_socket(
        *translate_node, SOCK_OUT, "Image");

    translate_x_input->default_value_typed<bNodeSocketValueFloat>()->value = x;
    translate_y_input->default_value_typed<bNodeSocketValueFloat>()->value = y;

    version_node_add_link(
        *node_tree, *link->fromnode, *link->fromsock, *translate_node, *translate_image_input);
    version_node_add_link(
        *node_tree, *translate_node, *translate_image_output, *link->tonode, *link->tosock);

    blender::bke::node_remove_link(node_tree, *link);
  }
}

/**
 * Turns all instances of `{` and `}` in a string into `{{` and `}}`, escaping
 * them for strings that are processed with templates so that they don't
 * erroneously get interpreted as template expressions.
 */
static void version_escape_curly_braces(char string[], const int string_array_length)
{
  int bytes_processed = 0;
  while (bytes_processed < string_array_length && string[bytes_processed] != '\0') {
    if (string[bytes_processed] == '{') {
      BLI_string_replace_range(
          string, string_array_length, bytes_processed, bytes_processed + 1, "{{");
      bytes_processed += 2;
      continue;
    }
    if (string[bytes_processed] == '}') {
      BLI_string_replace_range(
          string, string_array_length, bytes_processed, bytes_processed + 1, "}}");
      bytes_processed += 2;
      continue;
    }
    bytes_processed++;
  }
}

/* The Gamma option was removed. If enabled, a Gamma node will be added before and after
 * the node to perform the adjustment in sRGB space. */
static void do_version_blur_defocus_nodes_remove_gamma(bNodeTree *node_tree)
{
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (!ELEM(link->tonode->type_legacy, CMP_NODE_BLUR, CMP_NODE_DEFOCUS)) {
      continue;
    }

    if (link->tonode->type_legacy == CMP_NODE_BLUR &&
        !bool(static_cast<NodeBlurData *>(link->tonode->storage)->gamma))
    {
      continue;
    }

    if (link->tonode->type_legacy == CMP_NODE_DEFOCUS &&
        !bool(static_cast<NodeDefocus *>(link->tonode->storage)->gamco))
    {
      continue;
    }

    if (blender::StringRef(link->tosock->identifier) != "Image") {
      continue;
    }

    bNode *gamma_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_GAMMA);
    gamma_node->parent = link->tonode->parent;
    gamma_node->location[0] = link->tonode->location[0] - link->tonode->width - 20.0f;
    gamma_node->location[1] = link->tonode->location[1];

    bNodeSocket *color_input = blender::bke::node_find_socket(*gamma_node, SOCK_IN, "Color");
    bNodeSocket *color_output = blender::bke::node_find_socket(*gamma_node, SOCK_OUT, "Color");

    bNodeSocket *gamma_input = blender::bke::node_find_socket(*gamma_node, SOCK_IN, "Gamma");
    gamma_input->default_value_typed<bNodeSocketValueFloat>()->value = 2.0f;

    version_node_add_link(*node_tree, *link->fromnode, *link->fromsock, *gamma_node, *color_input);
    version_node_add_link(*node_tree, *gamma_node, *color_output, *link->tonode, *link->tosock);

    blender::bke::node_remove_link(node_tree, *link);
  }

  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (!ELEM(link->fromnode->type_legacy, CMP_NODE_BLUR, CMP_NODE_DEFOCUS)) {
      continue;
    }

    if (link->fromnode->type_legacy == CMP_NODE_BLUR &&
        !bool(static_cast<NodeBlurData *>(link->fromnode->storage)->gamma))
    {
      continue;
    }

    if (link->fromnode->type_legacy == CMP_NODE_DEFOCUS &&
        !bool(static_cast<NodeDefocus *>(link->fromnode->storage)->gamco))
    {
      continue;
    }

    bNode *gamma_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_GAMMA);
    gamma_node->parent = link->fromnode->parent;
    gamma_node->location[0] = link->fromnode->location[0] + link->fromnode->width + 20.0f;
    gamma_node->location[1] = link->fromnode->location[1];

    bNodeSocket *color_input = blender::bke::node_find_socket(*gamma_node, SOCK_IN, "Color");
    bNodeSocket *color_output = blender::bke::node_find_socket(*gamma_node, SOCK_OUT, "Color");

    bNodeSocket *gamma_input = blender::bke::node_find_socket(*gamma_node, SOCK_IN, "Gamma");
    gamma_input->default_value_typed<bNodeSocketValueFloat>()->value = 0.5f;

    version_node_add_link(*node_tree, *link->fromnode, *link->fromsock, *gamma_node, *color_input);
    version_node_add_link(*node_tree, *gamma_node, *color_output, *link->tonode, *link->tosock);

    blender::bke::node_remove_link(node_tree, *link);
  }
}

/**
 * Escapes all instances of `{` and `}` in the paths in a compositor node tree's
 * File Output nodes.
 *
 * If the passed node tree is not a compositor node tree, does nothing.
 */
static void version_escape_curly_braces_in_compositor_file_output_nodes(bNodeTree &nodetree)
{
  if (nodetree.type != NTREE_COMPOSIT) {
    return;
  }

  LISTBASE_FOREACH (bNode *, node, &nodetree.nodes) {
    if (!STREQ(node->idname, "CompositorNodeOutputFile")) {
      continue;
    }

    NodeCompositorFileOutput *node_data = static_cast<NodeCompositorFileOutput *>(node->storage);
    version_escape_curly_braces(node_data->directory, FILE_MAX);

    LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
      NodeImageMultiFileSocket *socket_data = static_cast<NodeImageMultiFileSocket *>(
          sock->storage);
      version_escape_curly_braces(socket_data->path, FILE_MAX);
    }
  }
}

/* The Relative option was removed. Insert Relative To Pixel nodes for the X and Y inputs to
 * convert relative values to pixel values. */
static void do_version_translate_node_remove_relative(bNodeTree *node_tree)
{
  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    if (!STREQ(node->idname, "CompositorNodeTranslate")) {
      continue;
    }

    const NodeTranslateData *data = static_cast<NodeTranslateData *>(node->storage);
    if (!data) {
      continue;
    }

    if (!bool(data->relative)) {
      continue;
    }

    /* Find links going into the node. */
    bNodeLink *image_link = nullptr;
    bNodeLink *x_link = nullptr;
    bNodeLink *y_link = nullptr;
    LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
      if (link->tonode != node) {
        continue;
      }

      if (blender::StringRef(link->tosock->identifier) == "Image") {
        image_link = link;
      }

      if (blender::StringRef(link->tosock->identifier) == "X") {
        x_link = link;
      }

      if (blender::StringRef(link->tosock->identifier) == "Y") {
        y_link = link;
      }
    }

    /* Image input is unlinked, so the node does nothing. */
    if (!image_link) {
      continue;
    }

    /* Add a Relative To Pixel node, assign it the input of the X translation and connect it to the
     * X translation input. */
    bNode *x_relative_to_pixel_node = blender::bke::node_add_node(
        nullptr, *node_tree, "CompositorNodeRelativeToPixel");
    x_relative_to_pixel_node->parent = node->parent;
    x_relative_to_pixel_node->location[0] = node->location[0] - node->width - 20.0f;
    x_relative_to_pixel_node->location[1] = node->location[1];

    x_relative_to_pixel_node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT;
    x_relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X;

    bNodeSocket *x_image_input = blender::bke::node_find_socket(
        *x_relative_to_pixel_node, SOCK_IN, "Image");
    bNodeSocket *x_value_input = blender::bke::node_find_socket(
        *x_relative_to_pixel_node, SOCK_IN, "Float Value");
    bNodeSocket *x_value_output = blender::bke::node_find_socket(
        *x_relative_to_pixel_node, SOCK_OUT, "Float Value");

    bNodeSocket *x_input = blender::bke::node_find_socket(*node, SOCK_IN, "X");
    x_value_input->default_value_typed<bNodeSocketValueFloat>()->value =
        x_input->default_value_typed<bNodeSocketValueFloat>()->value;

    version_node_add_link(*node_tree, *x_relative_to_pixel_node, *x_value_output, *node, *x_input);
    version_node_add_link(*node_tree,
                          *image_link->fromnode,
                          *image_link->fromsock,
                          *x_relative_to_pixel_node,
                          *x_image_input);

    if (x_link) {
      version_node_add_link(*node_tree,
                            *x_link->fromnode,
                            *x_link->fromsock,
                            *x_relative_to_pixel_node,
                            *x_value_input);
      blender::bke::node_remove_link(node_tree, *x_link);
    }

    /* Add a Relative To Pixel node, assign it the input of the Y translation and connect it to the
     * Y translation input. */
    bNode *y_relative_to_pixel_node = blender::bke::node_add_node(
        nullptr, *node_tree, "CompositorNodeRelativeToPixel");
    y_relative_to_pixel_node->parent = node->parent;
    y_relative_to_pixel_node->location[0] = node->location[0] - node->width - 20.0f;
    y_relative_to_pixel_node->location[1] = node->location[1] - 20.0f;

    y_relative_to_pixel_node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT;
    y_relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_Y;

    bNodeSocket *y_image_input = blender::bke::node_find_socket(
        *y_relative_to_pixel_node, SOCK_IN, "Image");
    bNodeSocket *y_value_input = blender::bke::node_find_socket(
        *y_relative_to_pixel_node, SOCK_IN, "Float Value");
    bNodeSocket *y_value_output = blender::bke::node_find_socket(
        *y_relative_to_pixel_node, SOCK_OUT, "Float Value");

    bNodeSocket *y_input = blender::bke::node_find_socket(*node, SOCK_IN, "Y");
    y_value_input->default_value_typed<bNodeSocketValueFloat>()->value =
        y_input->default_value_typed<bNodeSocketValueFloat>()->value;

    version_node_add_link(*node_tree, *y_relative_to_pixel_node, *y_value_output, *node, *y_input);
    version_node_add_link(*node_tree,
                          *image_link->fromnode,
                          *image_link->fromsock,
                          *y_relative_to_pixel_node,
                          *y_image_input);

    if (y_link) {
      version_node_add_link(*node_tree,
                            *y_link->fromnode,
                            *y_link->fromsock,
                            *y_relative_to_pixel_node,
                            *y_value_input);
      blender::bke::node_remove_link(node_tree, *y_link);
    }
  }
}

/* The options were converted into inputs, but the Relative option was removed. If relative is
 * enabled, we add Relative To Pixel nodes to convert the relative values to pixels. */
static void do_version_crop_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeTwoXYs *storage = static_cast<NodeTwoXYs *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "X")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "X", "X");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->x1;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Y")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Y", "Y");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->y2;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Width")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Width", "Width");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->x2 - storage->x1;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Height")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_INT, PROP_NONE, "Height", "Height");
    input->default_value_typed<bNodeSocketValueInt>()->value = storage->y1 - storage->y2;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Alpha Crop")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Alpha Crop", "Alpha Crop");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = !bool(node->custom1);
  }

  /* Find links going into the node. */
  bNodeLink *image_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
    if (link->tonode != node) {
      continue;
    }

    if (blender::StringRef(link->tosock->identifier) == "Image") {
      image_link = link;
    }
  }

  /* If Relative is not enabled or no image is connected, nothing else to do. */
  if (!bool(node->custom2) || !image_link) {
    MEM_freeN(storage);
    node->storage = nullptr;
    return;
  }

  bNode *x_relative_to_pixel_node = blender::bke::node_add_node(
      nullptr, *node_tree, "CompositorNodeRelativeToPixel");
  x_relative_to_pixel_node->parent = node->parent;
  x_relative_to_pixel_node->location[0] = node->location[0] - node->width - 20.0f;
  x_relative_to_pixel_node->location[1] = node->location[1];

  x_relative_to_pixel_node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT;
  x_relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X;

  bNodeSocket *x_image_input = blender::bke::node_find_socket(
      *x_relative_to_pixel_node, SOCK_IN, "Image");
  bNodeSocket *x_value_input = blender::bke::node_find_socket(
      *x_relative_to_pixel_node, SOCK_IN, "Float Value");
  bNodeSocket *x_value_output = blender::bke::node_find_socket(
      *x_relative_to_pixel_node, SOCK_OUT, "Float Value");

  x_value_input->default_value_typed<bNodeSocketValueFloat>()->value = storage->fac_x1;

  bNodeSocket *x_input = blender::bke::node_find_socket(*node, SOCK_IN, "X");
  version_node_add_link(*node_tree, *x_relative_to_pixel_node, *x_value_output, *node, *x_input);
  version_node_add_link(*node_tree,
                        *image_link->fromnode,
                        *image_link->fromsock,
                        *x_relative_to_pixel_node,
                        *x_image_input);

  bNode *y_relative_to_pixel_node = blender::bke::node_add_node(
      nullptr, *node_tree, "CompositorNodeRelativeToPixel");
  y_relative_to_pixel_node->parent = node->parent;
  y_relative_to_pixel_node->location[0] = node->location[0] - node->width - 20.0f;
  y_relative_to_pixel_node->location[1] = node->location[1] - 10;

  y_relative_to_pixel_node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT;
  y_relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_Y;

  bNodeSocket *y_image_input = blender::bke::node_find_socket(
      *y_relative_to_pixel_node, SOCK_IN, "Image");
  bNodeSocket *y_value_input = blender::bke::node_find_socket(
      *y_relative_to_pixel_node, SOCK_IN, "Float Value");
  bNodeSocket *y_value_output = blender::bke::node_find_socket(
      *y_relative_to_pixel_node, SOCK_OUT, "Float Value");

  bNodeSocket *y_input = blender::bke::node_find_socket(*node, SOCK_IN, "Y");
  y_value_input->default_value_typed<bNodeSocketValueFloat>()->value = storage->fac_y2;

  version_node_add_link(*node_tree, *y_relative_to_pixel_node, *y_value_output, *node, *y_input);
  version_node_add_link(*node_tree,
                        *image_link->fromnode,
                        *image_link->fromsock,
                        *y_relative_to_pixel_node,
                        *y_image_input);

  bNode *width_relative_to_pixel_node = blender::bke::node_add_node(
      nullptr, *node_tree, "CompositorNodeRelativeToPixel");
  width_relative_to_pixel_node->parent = node->parent;
  width_relative_to_pixel_node->location[0] = node->location[0] - node->width - 20.0f;
  width_relative_to_pixel_node->location[1] = node->location[1] - 20;

  width_relative_to_pixel_node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT;
  width_relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X;

  bNodeSocket *width_image_input = blender::bke::node_find_socket(
      *width_relative_to_pixel_node, SOCK_IN, "Image");
  bNodeSocket *width_value_input = blender::bke::node_find_socket(
      *width_relative_to_pixel_node, SOCK_IN, "Float Value");
  bNodeSocket *width_value_output = blender::bke::node_find_socket(
      *width_relative_to_pixel_node, SOCK_OUT, "Float Value");

  bNodeSocket *width_input = blender::bke::node_find_socket(*node, SOCK_IN, "Width");
  width_value_input->default_value_typed<bNodeSocketValueFloat>()->value = storage->fac_x2 -
                                                                           storage->fac_x1;

  version_node_add_link(
      *node_tree, *width_relative_to_pixel_node, *width_value_output, *node, *width_input);
  version_node_add_link(*node_tree,
                        *image_link->fromnode,
                        *image_link->fromsock,
                        *width_relative_to_pixel_node,
                        *width_image_input);

  bNode *height_relative_to_pixel_node = blender::bke::node_add_node(
      nullptr, *node_tree, "CompositorNodeRelativeToPixel");
  height_relative_to_pixel_node->parent = node->parent;
  height_relative_to_pixel_node->location[0] = node->location[0] - node->width - 20.0f;
  height_relative_to_pixel_node->location[1] = node->location[1] - 30;

  height_relative_to_pixel_node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT;
  height_relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_Y;

  bNodeSocket *height_image_input = blender::bke::node_find_socket(
      *height_relative_to_pixel_node, SOCK_IN, "Image");
  bNodeSocket *height_value_input = blender::bke::node_find_socket(
      *height_relative_to_pixel_node, SOCK_IN, "Float Value");
  bNodeSocket *height_value_output = blender::bke::node_find_socket(
      *height_relative_to_pixel_node, SOCK_OUT, "Float Value");

  bNodeSocket *height_input = blender::bke::node_find_socket(*node, SOCK_IN, "Height");
  height_value_input->default_value_typed<bNodeSocketValueFloat>()->value = storage->fac_y1 -
                                                                            storage->fac_y2;

  version_node_add_link(
      *node_tree, *height_relative_to_pixel_node, *height_value_output, *node, *height_input);
  version_node_add_link(*node_tree,
                        *image_link->fromnode,
                        *image_link->fromsock,
                        *height_relative_to_pixel_node,
                        *height_image_input);

  MEM_freeN(storage);
  node->storage = nullptr;
}

/* The options were converted into inputs. */
static void do_version_crop_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
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
    if (BLI_str_endswith(fcurve->rna_path, "min_x")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "max_y")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "max_x")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "min_y")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[4].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_crop_size")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
      adjust_fcurve_key_frame_values(
          fcurve, PROP_BOOLEAN, [&](const float value) { return 1.0f - value; });
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The options were converted into inputs. */
static void do_version_color_balance_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeColorBalance *storage = static_cast<NodeColorBalance *>(node->storage);
  if (!storage) {
    return;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Color Lift")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Color Lift", "Lift");
    copy_v3_v3(input->default_value_typed<bNodeSocketValueRGBA>()->value, storage->lift);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Color Gamma")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Color Gamma", "Gamma");
    copy_v3_v3(input->default_value_typed<bNodeSocketValueRGBA>()->value, storage->gamma);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Color Gain")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Color Gain", "Gain");
    copy_v3_v3(input->default_value_typed<bNodeSocketValueRGBA>()->value, storage->gain);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Color Offset")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Color Offset", "Offset");
    copy_v3_v3(input->default_value_typed<bNodeSocketValueRGBA>()->value, storage->offset);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Color Power")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Color Power", "Power");
    copy_v3_v3(input->default_value_typed<bNodeSocketValueRGBA>()->value, storage->power);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Color Slope")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "Color Slope", "Slope");
    copy_v3_v3(input->default_value_typed<bNodeSocketValueRGBA>()->value, storage->slope);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Base Offset")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Base Offset", "Offset");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->offset_basis;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Input Temperature")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_COLOR_TEMPERATURE,
                                                              "Input Temperature",
                                                              "Temperature");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->input_temperature;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Input Tint")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Input Tint", "Tint");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->input_tint;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Output Temperature")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(*node_tree,
                                                              *node,
                                                              SOCK_IN,
                                                              SOCK_FLOAT,
                                                              PROP_COLOR_TEMPERATURE,
                                                              "Output Temperature",
                                                              "Temperature");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->output_temperature;
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Output Tint")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Output Tint", "Tint");
    input->default_value_typed<bNodeSocketValueFloat>()->value = storage->output_tint;
  }

  MEM_freeN(storage);
  node->storage = nullptr;
}

/* The options were converted into inputs. */
static void do_version_color_balance_node_options_to_inputs_animation(bNodeTree *node_tree,
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
    if (BLI_str_endswith(fcurve->rna_path, "lift")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "gamma")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[5].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "gain")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[7].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "offset_basis")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[8].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "offset")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[9].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "power")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[11].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "slope")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[13].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "input_temperature")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[14].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "input_tint")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[15].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "output_temperature")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[16].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "output_tint")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[17].default_value");
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* The Coordinates outputs were moved into their own Texture Coordinate node. If used, add a
 * Texture Coordinates node and use it instead. */
static void do_version_replace_image_info_node_coordinates(bNodeTree *node_tree)
{
  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    if (!STREQ(node->idname, "CompositorNodeImageInfo")) {
      continue;
    }

    bNodeLink *input_link = nullptr;
    bNodeLink *output_texture_link = nullptr;
    bNodeLink *output_pixel_link = nullptr;
    LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
      if (link->tonode == node) {
        input_link = link;
      }

      if (link->fromnode == node &&
          blender::StringRef(link->fromsock->identifier) == "Texture Coordinates")
      {
        output_texture_link = link;
      }

      if (link->fromnode == node &&
          blender::StringRef(link->fromsock->identifier) == "Pixel Coordinates")
      {
        output_pixel_link = link;
      }
    }

    if (!output_texture_link && !output_pixel_link) {
      continue;
    }

    bNode *image_coordinates_node = blender::bke::node_add_node(
        nullptr, *node_tree, "CompositorNodeImageCoordinates");
    image_coordinates_node->parent = node->parent;
    image_coordinates_node->location[0] = node->location[0];
    image_coordinates_node->location[1] = node->location[1] - node->height - 10.0f;

    if (input_link) {
      bNodeSocket *image_input = blender::bke::node_find_socket(
          *image_coordinates_node, SOCK_IN, "Image");
      version_node_add_link(*node_tree,
                            *input_link->fromnode,
                            *input_link->fromsock,
                            *image_coordinates_node,
                            *image_input);
    }

    if (output_texture_link) {
      bNodeSocket *uniform_output = blender::bke::node_find_socket(
          *image_coordinates_node, SOCK_OUT, "Uniform");
      version_node_add_link(*node_tree,
                            *image_coordinates_node,
                            *uniform_output,
                            *output_texture_link->tonode,
                            *output_texture_link->tosock);
      blender::bke::node_remove_link(node_tree, *output_texture_link);
    }

    if (output_pixel_link) {
      bNodeSocket *pixel_output = blender::bke::node_find_socket(
          *image_coordinates_node, SOCK_OUT, "Pixel");
      version_node_add_link(*node_tree,
                            *image_coordinates_node,
                            *pixel_output,
                            *output_pixel_link->tonode,
                            *output_pixel_link->tosock);
      blender::bke::node_remove_link(node_tree, *output_pixel_link);
    }
  }
}

/**
 * Vector sockets can now have different dimensions,
 * so set the dimensions for existing sockets to 3.
 */
static void do_version_vector_sockets_dimensions(bNodeTree *node_tree)
{
  node_tree->tree_interface.foreach_item([&](bNodeTreeInterfaceItem &item) {
    if (item.item_type != NODE_INTERFACE_SOCKET) {
      return true;
    }

    bNodeTreeInterfaceSocket &interface_socket =
        blender::bke::node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
    blender::bke::bNodeSocketType *base_typeinfo = blender::bke::node_socket_type_find(
        interface_socket.socket_type);

    if (base_typeinfo->type == SOCK_VECTOR) {
      blender::bke::node_interface::get_socket_data_as<bNodeSocketValueVector>(interface_socket)
          .dimensions = 3;
    }
    return true;
  });

  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      if (socket->type == SOCK_VECTOR) {
        socket->default_value_typed<bNodeSocketValueVector>()->dimensions = 3;
      }
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
      if (socket->type == SOCK_VECTOR) {
        socket->default_value_typed<bNodeSocketValueVector>()->dimensions = 3;
      }
    }
  }
}

/* The options were converted into inputs, but the Relative option was removed. If relative is
 * enabled, we add Relative To Pixel nodes to convert the relative values to pixels. */
static void do_version_blur_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  NodeBlurData *storage = static_cast<NodeBlurData *>(node->storage);
  if (!storage) {
    return;
  }

  bNodeSocket *size_input = blender::bke::node_find_socket(*node, SOCK_IN, "Size");
  const float old_size = size_input->default_value_typed<bNodeSocketValueFloat>()->value;

  blender::bke::node_modify_socket_type_static(
      node_tree, node, size_input, SOCK_VECTOR, PROP_NONE);
  size_input->default_value_typed<bNodeSocketValueVector>()->value[0] = old_size * storage->sizex;
  size_input->default_value_typed<bNodeSocketValueVector>()->value[1] = old_size * storage->sizey;

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Extend Bounds")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Extend Bounds", "Extend Bounds");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = bool(node->custom1 & (1 << 1));
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Separable")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Separable", "Separable");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = !bool(storage->bokeh);
  }

  /* Find links going into the node. */
  bNodeLink *image_link = nullptr;
  bNodeLink *size_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
    if (link->tonode != node) {
      continue;
    }

    if (blender::StringRef(link->tosock->identifier) == "Image") {
      image_link = link;
    }

    if (blender::StringRef(link->tosock->identifier) == "Size") {
      size_link = link;
    }
  }

  if (size_link) {
    bNode *multiply_node = blender::bke::node_add_node(
        nullptr, *node_tree, "ShaderNodeVectorMath");
    multiply_node->parent = node->parent;
    multiply_node->location[0] = node->location[0] - node->width - 40.0f;
    multiply_node->location[1] = node->location[1];

    multiply_node->custom1 = NODE_VECTOR_MATH_SCALE;

    bNodeSocket *vector_input = blender::bke::node_find_socket(*multiply_node, SOCK_IN, "Vector");
    bNodeSocket *scale_input = blender::bke::node_find_socket(*multiply_node, SOCK_IN, "Scale");
    bNodeSocket *vector_output = blender::bke::node_find_socket(
        *multiply_node, SOCK_OUT, "Vector");

    if (storage->relative) {
      vector_input->default_value_typed<bNodeSocketValueVector>()->value[0] = storage->percentx /
                                                                              100.0f;
      vector_input->default_value_typed<bNodeSocketValueVector>()->value[1] = storage->percenty /
                                                                              100.0f;
    }
    else {
      vector_input->default_value_typed<bNodeSocketValueVector>()->value[0] = storage->sizex;
      vector_input->default_value_typed<bNodeSocketValueVector>()->value[1] = storage->sizey;
    }

    version_node_add_link(
        *node_tree, *size_link->fromnode, *size_link->fromsock, *multiply_node, *scale_input);
    bNodeLink &new_link = version_node_add_link(
        *node_tree, *multiply_node, *vector_output, *node, *size_input);
    blender::bke::node_remove_link(node_tree, *size_link);
    size_link = &new_link;
  }

  /* If Relative is not enabled or no image is connected, nothing else to do. */
  if (!bool(storage->relative) || !image_link) {
    return;
  }

  bNode *relative_to_pixel_node = blender::bke::node_add_node(
      nullptr, *node_tree, "CompositorNodeRelativeToPixel");
  relative_to_pixel_node->parent = node->parent;
  relative_to_pixel_node->location[0] = node->location[0] - node->width - 20.0f;
  relative_to_pixel_node->location[1] = node->location[1];

  relative_to_pixel_node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_VECTOR;
  switch (storage->aspect) {
    case CMP_NODE_BLUR_ASPECT_Y:
      relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_Y;
      break;
    case CMP_NODE_BLUR_ASPECT_X:
      relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_X;
      break;
    case CMP_NODE_BLUR_ASPECT_NONE:
      relative_to_pixel_node->custom2 =
          CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_PER_DIMENSION;
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  bNodeSocket *image_input = blender::bke::node_find_socket(
      *relative_to_pixel_node, SOCK_IN, "Image");
  bNodeSocket *vector_input = blender::bke::node_find_socket(
      *relative_to_pixel_node, SOCK_IN, "Vector Value");
  bNodeSocket *vector_output = blender::bke::node_find_socket(
      *relative_to_pixel_node, SOCK_OUT, "Vector Value");

  version_node_add_link(*node_tree,
                        *image_link->fromnode,
                        *image_link->fromsock,
                        *relative_to_pixel_node,
                        *image_input);
  if (size_link) {
    version_node_add_link(*node_tree,
                          *size_link->fromnode,
                          *size_link->fromsock,
                          *relative_to_pixel_node,
                          *vector_input);
    blender::bke::node_remove_link(node_tree, *size_link);
  }
  else {
    vector_input->default_value_typed<bNodeSocketValueVector>()->value[0] = (storage->percentx /
                                                                             100.0f) *
                                                                            old_size;
    vector_input->default_value_typed<bNodeSocketValueVector>()->value[1] = (storage->percenty /
                                                                             100.0f) *
                                                                            old_size;
  }
  version_node_add_link(*node_tree, *relative_to_pixel_node, *vector_output, *node, *size_input);
}

/* The options were converted into inputs. */
static void do_version_blur_node_options_to_inputs_animation(bNodeTree *node_tree, bNode *node)
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
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
      fcurve->array_index = 0;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "size_y")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[1].default_value");
      fcurve->array_index = 1;
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_extended_bounds")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[2].default_value");
    }
    else if (BLI_str_endswith(fcurve->rna_path, "use_bokeh")) {
      fcurve->rna_path = BLI_sprintfN("%s.%s", node_rna_path.c_str(), "inputs[3].default_value");
      adjust_fcurve_key_frame_values(
          fcurve, PROP_BOOLEAN, [&](const float value) { return 1.0f - value; });
    }

    /* The RNA path was changed, free the old path. */
    if (fcurve->rna_path != old_rna_path) {
      MEM_freeN(old_rna_path);
    }
  });
}

/* Unified paint settings need a default curve for the color jitter options. */
static void do_init_default_jitter_curves_in_unified_paint_settings(ToolSettings *ts)
{
  if (ts->unified_paint_settings.curve_rand_hue == nullptr) {
    ts->unified_paint_settings.curve_rand_hue = BKE_paint_default_curve();
  }
  if (ts->unified_paint_settings.curve_rand_saturation == nullptr) {
    ts->unified_paint_settings.curve_rand_saturation = BKE_paint_default_curve();
  }
  if (ts->unified_paint_settings.curve_rand_value == nullptr) {
    ts->unified_paint_settings.curve_rand_value = BKE_paint_default_curve();
  }
}

/* GP_BRUSH_* settings in gpencil_settings->flag2 were deprecated and replaced with
 * brush->color_jitter_flag. */
static void do_convert_gp_jitter_flags(Brush *brush)
{
  BrushGpencilSettings *settings = brush->gpencil_settings;
  if (settings->flag2 & GP_BRUSH_USE_HUE_AT_STROKE) {
    brush->color_jitter_flag |= BRUSH_COLOR_JITTER_USE_HUE_AT_STROKE;
  }
  if (settings->flag2 & GP_BRUSH_USE_SAT_AT_STROKE) {
    brush->color_jitter_flag |= BRUSH_COLOR_JITTER_USE_SAT_AT_STROKE;
  }
  if (settings->flag2 & GP_BRUSH_USE_VAL_AT_STROKE) {
    brush->color_jitter_flag |= BRUSH_COLOR_JITTER_USE_VAL_AT_STROKE;
  }
  if (settings->flag2 & GP_BRUSH_USE_HUE_RAND_PRESS) {
    brush->color_jitter_flag |= BRUSH_COLOR_JITTER_USE_HUE_RAND_PRESS;
  }
  if (settings->flag2 & GP_BRUSH_USE_SAT_RAND_PRESS) {
    brush->color_jitter_flag |= BRUSH_COLOR_JITTER_USE_SAT_RAND_PRESS;
  }
  if (settings->flag2 & GP_BRUSH_USE_VAL_RAND_PRESS) {
    brush->color_jitter_flag |= BRUSH_COLOR_JITTER_USE_VAL_RAND_PRESS;
  }
}

/* The options were converted into inputs. */
static void do_version_flip_node_options_to_inputs(bNodeTree *node_tree, bNode *node)
{
  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Flip X")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Flip X", "Flip X");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = ELEM(node->custom1, 0, 2);
  }

  if (!blender::bke::node_find_socket(*node, SOCK_IN, "Flip Y")) {
    bNodeSocket *input = blender::bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_BOOLEAN, PROP_NONE, "Flip Y", "Flip Y");
    input->default_value_typed<bNodeSocketValueBoolean>()->value = ELEM(node->custom1, 1, 2);
  }
}

static void clamp_subdivision_node_level_input(bNodeTree &tree)
{
  blender::Map<bNodeSocket *, bNodeLink *> links_to_level_and_max_inputs;
  LISTBASE_FOREACH (bNodeLink *, link, &tree.links) {
    if (link->tosock) {
      if (ELEM(blender::StringRef(link->tosock->identifier), "Level", "Max")) {
        links_to_level_and_max_inputs.add(link->tosock, link);
      }
    }
  }
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &tree.nodes) {
    if (!ELEM(node->type_legacy, GEO_NODE_SUBDIVISION_SURFACE, GEO_NODE_SUBDIVIDE_MESH)) {
      continue;
    }
    bNodeSocket *level_input = blender::bke::node_find_socket(*node, SOCK_IN, "Level");
    if (!level_input || level_input->type != SOCK_INT) {
      continue;
    }
    bNodeLink *link = links_to_level_and_max_inputs.lookup_default(level_input, nullptr);
    if (link) {
      bNode *origin_node = link->fromnode;
      if (origin_node->type_legacy == SH_NODE_CLAMP) {
        bNodeSocket *max_input_socket = blender::bke::node_find_socket(
            *origin_node, SOCK_IN, "Max");
        if (max_input_socket->type == SOCK_FLOAT &&
            !links_to_level_and_max_inputs.contains(max_input_socket))
        {
          if (max_input_socket->default_value_typed<bNodeSocketValueFloat>()->value <= 11.0f) {
            /* There is already a clamp node, so no need to add another one. */
            continue;
          }
        }
      }
      /* Insert clamp node. */
      bNode &clamp_node = version_node_add_empty(tree, "ShaderNodeClamp");
      clamp_node.parent = node->parent;
      clamp_node.location[0] = node->location[0] - 25;
      clamp_node.location[1] = node->location[1];
      bNodeSocket &clamp_value_input = version_node_add_socket(
          tree, clamp_node, SOCK_IN, "NodeSocketFloat", "Value");
      bNodeSocket &clamp_min_input = version_node_add_socket(
          tree, clamp_node, SOCK_IN, "NodeSocketFloat", "Min");
      bNodeSocket &clamp_max_input = version_node_add_socket(
          tree, clamp_node, SOCK_IN, "NodeSocketFloat", "Max");
      bNodeSocket &clamp_value_output = version_node_add_socket(
          tree, clamp_node, SOCK_OUT, "NodeSocketFloat", "Result");

      static_cast<bNodeSocketValueFloat *>(clamp_min_input.default_value)->value = 0.0f;
      static_cast<bNodeSocketValueFloat *>(clamp_max_input.default_value)->value = 11.0f;

      link->tosock = &clamp_value_input;
      version_node_add_link(tree, clamp_node, clamp_value_output, *node, *level_input);
    }
    else {
      /* Clamp value directly. */
      bNodeSocketValueInt *value = level_input->default_value_typed<bNodeSocketValueInt>();
      value->value = std::clamp(value->value, 0, 11);
    }
  }

  version_socket_update_is_used(&tree);
}

void do_versions_after_linking_450(FileData * /*fd*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 12)) {
    version_node_socket_index_animdata(bmain, NTREE_COMPOSIT, CMP_NODE_GLARE, 3, 1, 14);
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_version_new_glare_clamp_input(ntree);
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
           * the users. */
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

  /* Because this was backported to 4.4 (f1e829a459) we need to exclude anything that was already
   * saved with that version otherwise we would apply the fix twice. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 404, 32) ||
      (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 14) && bmain->versionfile >= 405))
  {
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
          if (node->type_legacy == CMP_NODE_SUNBEAMS_DEPRECATED) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 69)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_BOKEHBLUR) {
            do_version_bokeh_blur_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 75)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_CROP) {
            do_version_crop_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 76)) {
    ToolSettings toolsettings_default = blender::dna::shallow_copy(
        *DNA_struct_default_get(ToolSettings));
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->toolsettings->snap_playhead_mode = toolsettings_default.snap_playhead_mode;
      scene->toolsettings->snap_step_frames = toolsettings_default.snap_step_frames;
      scene->toolsettings->snap_step_seconds = toolsettings_default.snap_step_seconds;
      scene->toolsettings->playhead_snap_distance = toolsettings_default.playhead_snap_distance;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 77)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLORBALANCE) {
            do_version_color_balance_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 80)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_BLUR) {
            do_version_blur_node_options_to_inputs_animation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 84)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      do_init_default_jitter_curves_in_unified_paint_settings(scene->toolsettings);
    }

    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->gpencil_settings) {
        do_convert_gp_jitter_flags(brush);
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
    if (!version_node_socket_is_used(
            bke::node_find_socket(*curve_to_mesh, SOCK_IN, "Profile Curve")))
    {
      /* No additional versioning is needed when the profile curve input is unused. */
      continue;
    }

    if (bke::node_find_socket(*curve_to_mesh, SOCK_IN, "Scale")) {
      /* Make versioning idempotent. */
      continue;
    }
    bNodeSocket *scale_socket = version_node_add_socket_if_not_exist(
        tree, curve_to_mesh, SOCK_IN, SOCK_FLOAT, PROP_NONE, "Scale", "Scale");
    /* Use a default scale value of 1. */
    scale_socket->default_value_typed<bNodeSocketValueFloat>()->value = 1.0f;

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

  version_socket_update_is_used(tree);
}

static bool strip_effect_overdrop_to_alphaover(Strip *strip, void * /*user_data*/)
{
  if (strip->type == STRIP_TYPE_OVERDROP_REMOVED) {
    strip->type = STRIP_TYPE_ALPHAOVER;
  }
  if (strip->blend_mode == STRIP_BLEND_OVERDROP_REMOVED) {
    strip->blend_mode = STRIP_BLEND_ALPHAOVER;
  }
  return true;
}

static void version_sequencer_update_overdrop(Main *bmain)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->ed != nullptr) {
      blender::seq::foreach_strip(
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
           * Textures. UV Editing is the only other default workspace with an Image Editor. */
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

        /* Note, this fix was committed after some users had already run the versioning after
         * 4.5 was released. Since 4.5 is an LTS and will be used for the foreseeable future to
         * transition between 4.x and 5.x the fix has been added here, even though that does
         * not fix the issue for some users with custom brush assets who have started using 4.5
         * already.
         *
         * Since the `sculpt_brush_type` field changed from 'SCULPT_BRUSH_TYPE_SCRAPE' to
         * 'SCULPT_BRUSH_TYPE_PLANE', we do not have a value that can be used to definitively apply
         * a corrective versioning step along with a subversion bump without potentially affecting
         * some false positives.
         *
         * See #142151 for more details. */
        brush->plane_offset *= -1.0f;
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

static void node_interface_single_value_to_structure_type(bNodeTreeInterfaceItem &item)
{
  if (item.item_type == eNodeTreeInterfaceItemType::NODE_INTERFACE_SOCKET) {
    auto &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);
    if (socket.flag & NODE_INTERFACE_SOCKET_SINGLE_VALUE_ONLY_LEGACY) {
      socket.structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE;
    }
    else {
      socket.structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO;
    }
  }
  else {
    auto &panel = reinterpret_cast<bNodeTreeInterfacePanel &>(item);
    for (bNodeTreeInterfaceItem *item : blender::Span(panel.items_array, panel.items_num)) {
      node_interface_single_value_to_structure_type(*item);
    }
  }
}

static void version_set_default_bone_drawtype(Main *bmain)
{
  LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
    blender::animrig::ANIM_armature_foreach_bone(
        &arm->bonebase, [](Bone *bone) { bone->drawtype = ARM_DRAW_TYPE_ARMATURE_DEFINED; });
    BLI_assert_msg(!arm->edbo, "Armatures should not be saved in edit mode");
  }
}

void blo_do_versions_450(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{

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
            node->custom1 = CMP_NODE_INTERPOLATION_ANISOTROPIC;
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
          if (node->type_legacy == CMP_NODE_SUNBEAMS_DEPRECATED) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 67)) {
    /* Version render output paths (both primary on scene as well as those in
     * the File Output compositor node) to escape curly braces. */
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        version_escape_curly_braces(scene->r.pic, FILE_MAX);
        if (scene->nodetree) {
          version_escape_curly_braces_in_compositor_file_output_nodes(*scene->nodetree);
        }
      }

      LISTBASE_FOREACH (bNodeTree *, nodetree, &bmain->nodetrees) {
        version_escape_curly_braces_in_compositor_file_output_nodes(*nodetree);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 68)) {
    /* Fix brush->tip_scale_x which should never be zero. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->tip_scale_x == 0.0f) {
        brush->tip_scale_x = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 69)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_BOKEHBLUR) {
            do_version_bokeh_blur_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 70)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        do_version_scale_node_remove_translate(node_tree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 71)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        do_version_blur_defocus_nodes_remove_gamma(node_tree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 72)) {
    version_set_default_bone_drawtype(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 73)) {
    /* Make #Curve::type the source of truth for the curve type.
     * Previously #Curve::vfont was checked which is error prone
     * since the member can become null at run-time, see: #139133. */
    LISTBASE_FOREACH (Curve *, cu, &bmain->curves) {
      if (ELEM(cu->ob_type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
        continue;
      }
      short ob_type = OB_CURVES_LEGACY;
      if (cu->vfont) {
        ob_type = OB_FONT;
      }
      else {
        LISTBASE_FOREACH (const Nurb *, nu, &cu->nurb) {
          if (nu->pntsv > 1) {
            ob_type = OB_SURF;
            break;
          }
        }
      }
      cu->ob_type = ob_type;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 74)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        do_version_translate_node_remove_relative(node_tree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 75)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_CROP) {
            do_version_crop_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 76)) {
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      if (light->temperature == 0.0f) {
        light->temperature = 6500.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 77)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLORBALANCE) {
            do_version_color_balance_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 78)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        do_version_replace_image_info_node_coordinates(node_tree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 79)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      do_version_vector_sockets_dimensions(node_tree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 80)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_BLUR) {
            do_version_blur_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 81)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        node_interface_single_value_to_structure_type(ntree->tree_interface.root_panel.item);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 83)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->soft) {
        ob->soft->fuzzyness = std::max<int>(1, ob->soft->fuzzyness);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 85)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_FLIP) {
            do_version_flip_node_options_to_inputs(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 86)) {
    fix_curve_nurbs_knot_mode_custom(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 87)) {
    FOREACH_NODETREE_BEGIN (bmain, tree, id) {
      if (tree->type == NTREE_GEOMETRY) {
        clamp_subdivision_node_level_input(*tree);
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
