/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "ANIM_armature_iter.hh"

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_defaults.h"
#include "DNA_genfile.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"

#undef DNA_GENFILE_VERSIONING_MACROS

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_multi_value_map.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"

#include "BKE_anim_data.hh"
#include "BKE_armature.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_main.hh"
#include "BKE_nla.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "readfile.hh"

#include "versioning_common.hh"

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

void do_versions_after_linking_410(FileData * /*fd*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 401, 23)) {
    version_nla_tweakmode_incomplete(bmain);
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

static void versioning_update_noise_texture_node(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_TEX_NOISE) {
      continue;
    }

    (static_cast<NodeTexNoise *>(node->storage))->type = SHD_NOISE_FBM;

    bNodeSocket *roughness_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Roughness");
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

      bNode *clamp_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_CLAMP);
      clamp_node->parent = node->parent;
      clamp_node->custom1 = NODE_CLAMP_MINMAX;
      clamp_node->locx_legacy = node->locx_legacy;
      clamp_node->locy_legacy = node->locy_legacy - 300.0f;
      clamp_node->flag |= NODE_COLLAPSED;
      bNodeSocket *clamp_socket_value = blender::bke::node_find_socket(
          *clamp_node, SOCK_IN, "Value");
      bNodeSocket *clamp_socket_min = blender::bke::node_find_socket(*clamp_node, SOCK_IN, "Min");
      bNodeSocket *clamp_socket_max = blender::bke::node_find_socket(*clamp_node, SOCK_IN, "Max");
      bNodeSocket *clamp_socket_out = blender::bke::node_find_socket(
          *clamp_node, SOCK_OUT, "Result");

      *version_cycles_node_socket_float_value(clamp_socket_min) = 0.0f;
      *version_cycles_node_socket_float_value(clamp_socket_max) = 1.0f;

      blender::bke::node_remove_link(ntree, *roughness_link);
      blender::bke::node_add_link(
          *ntree, *roughness_from_node, *roughness_from_socket, *clamp_node, *clamp_socket_value);
      blender::bke::node_add_link(
          *ntree, *clamp_node, *clamp_socket_out, *node, *roughness_socket);
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

    STRNCPY_UTF8(node->idname, "ShaderNodeTexNoise");
    node->type_legacy = SH_NODE_TEX_NOISE;
    NodeTexNoise *data = MEM_callocN<NodeTexNoise>(__func__);
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

    bNodeSocket *fac_socket = blender::bke::node_find_socket(*node, SOCK_OUT, "Fac");
    /* Clear label because Musgrave output socket label is set to "Height" instead of "Fac". */
    fac_socket->label[0] = '\0';

    bNodeSocket *detail_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Detail");
    float *detail = version_cycles_node_socket_float_value(detail_socket);

    if (detail_link != nullptr) {
      locy_offset -= 80.0f;

      /* Add Minimum Math node and Subtract Math node before Detail input. */

      bNode *min_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
      min_node->parent = node->parent;
      min_node->custom1 = NODE_MATH_MINIMUM;
      min_node->locx_legacy = node->locx_legacy;
      min_node->locy_legacy = node->locy_legacy - 320.0f;
      min_node->flag |= NODE_COLLAPSED;
      bNodeSocket *min_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&min_node->inputs, 0));
      bNodeSocket *min_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&min_node->inputs, 1));
      bNodeSocket *min_socket_out = blender::bke::node_find_socket(*min_node, SOCK_OUT, "Value");

      bNode *sub1_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
      sub1_node->parent = node->parent;
      sub1_node->custom1 = NODE_MATH_SUBTRACT;
      sub1_node->locx_legacy = node->locx_legacy;
      sub1_node->locy_legacy = node->locy_legacy - 360.0f;
      sub1_node->flag |= NODE_COLLAPSED;
      bNodeSocket *sub1_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&sub1_node->inputs, 0));
      bNodeSocket *sub1_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&sub1_node->inputs, 1));
      bNodeSocket *sub1_socket_out = blender::bke::node_find_socket(*sub1_node, SOCK_OUT, "Value");

      *version_cycles_node_socket_float_value(min_socket_B) = 14.0f;
      *version_cycles_node_socket_float_value(sub1_socket_B) = 1.0f;

      blender::bke::node_remove_link(ntree, *detail_link);
      blender::bke::node_add_link(
          *ntree, *detail_from_node, *detail_from_socket, *sub1_node, *sub1_socket_A);
      blender::bke::node_add_link(*ntree, *sub1_node, *sub1_socket_out, *min_node, *min_socket_A);
      blender::bke::node_add_link(*ntree, *min_node, *min_socket_out, *node, *detail_socket);

      if (ELEM(noise_type, SHD_NOISE_RIDGED_MULTIFRACTAL, SHD_NOISE_HETERO_TERRAIN)) {
        locy_offset -= 40.0f;

        /* Add Greater Than Math node before Subtract Math node. */

        bNode *greater_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
        greater_node->parent = node->parent;
        greater_node->custom1 = NODE_MATH_GREATER_THAN;
        greater_node->locx_legacy = node->locx_legacy;
        greater_node->locy_legacy = node->locy_legacy - 400.0f;
        greater_node->flag |= NODE_COLLAPSED;
        bNodeSocket *greater_socket_A = static_cast<bNodeSocket *>(
            BLI_findlink(&greater_node->inputs, 0));
        bNodeSocket *greater_socket_B = static_cast<bNodeSocket *>(
            BLI_findlink(&greater_node->inputs, 1));
        bNodeSocket *greater_socket_out = blender::bke::node_find_socket(
            *greater_node, SOCK_OUT, "Value");

        *version_cycles_node_socket_float_value(greater_socket_B) = 1.0f;

        blender::bke::node_add_link(
            *ntree, *detail_from_node, *detail_from_socket, *greater_node, *greater_socket_A);
        blender::bke::node_add_link(
            *ntree, *greater_node, *greater_socket_out, *sub1_node, *sub1_socket_B);
      }
      else {
        /* Add Clamp node and Multiply Math node behind Fac output. */

        bNode *clamp_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_CLAMP);
        clamp_node->parent = node->parent;
        clamp_node->custom1 = NODE_CLAMP_MINMAX;
        clamp_node->locx_legacy = node->locx_legacy;
        clamp_node->locy_legacy = node->locy_legacy + 40.0f;
        clamp_node->flag |= NODE_COLLAPSED;
        bNodeSocket *clamp_socket_value = blender::bke::node_find_socket(
            *clamp_node, SOCK_IN, "Value");
        bNodeSocket *clamp_socket_min = blender::bke::node_find_socket(
            *clamp_node, SOCK_IN, "Min");
        bNodeSocket *clamp_socket_max = blender::bke::node_find_socket(
            *clamp_node, SOCK_IN, "Max");
        bNodeSocket *clamp_socket_out = blender::bke::node_find_socket(
            *clamp_node, SOCK_OUT, "Result");

        bNode *mul_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
        mul_node->parent = node->parent;
        mul_node->custom1 = NODE_MATH_MULTIPLY;
        mul_node->locx_legacy = node->locx_legacy;
        mul_node->locy_legacy = node->locy_legacy + 80.0f;
        mul_node->flag |= NODE_COLLAPSED;
        bNodeSocket *mul_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 0));
        bNodeSocket *mul_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 1));
        bNodeSocket *mul_socket_out = blender::bke::node_find_socket(*mul_node, SOCK_OUT, "Value");

        *version_cycles_node_socket_float_value(clamp_socket_min) = 0.0f;
        *version_cycles_node_socket_float_value(clamp_socket_max) = 1.0f;

        if (noise_type == SHD_NOISE_MULTIFRACTAL) {
          /* Add Subtract Math node and Add Math node after Multiply Math node. */

          bNode *sub2_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
          sub2_node->parent = node->parent;
          sub2_node->custom1 = NODE_MATH_SUBTRACT;
          sub2_node->custom2 = SHD_MATH_CLAMP;
          sub2_node->locx_legacy = node->locx_legacy;
          sub2_node->locy_legacy = node->locy_legacy + 120.0f;
          sub2_node->flag |= NODE_COLLAPSED;
          bNodeSocket *sub2_socket_A = static_cast<bNodeSocket *>(
              BLI_findlink(&sub2_node->inputs, 0));
          bNodeSocket *sub2_socket_B = static_cast<bNodeSocket *>(
              BLI_findlink(&sub2_node->inputs, 1));
          bNodeSocket *sub2_socket_out = blender::bke::node_find_socket(
              *sub2_node, SOCK_OUT, "Value");

          bNode *add_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
          add_node->parent = node->parent;
          add_node->custom1 = NODE_MATH_ADD;
          add_node->locx_legacy = node->locx_legacy;
          add_node->locy_legacy = node->locy_legacy + 160.0f;
          add_node->flag |= NODE_COLLAPSED;
          bNodeSocket *add_socket_A = static_cast<bNodeSocket *>(
              BLI_findlink(&add_node->inputs, 0));
          bNodeSocket *add_socket_B = static_cast<bNodeSocket *>(
              BLI_findlink(&add_node->inputs, 1));
          bNodeSocket *add_socket_out = blender::bke::node_find_socket(
              *add_node, SOCK_OUT, "Value");

          *version_cycles_node_socket_float_value(sub2_socket_A) = 1.0f;

          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == fac_socket) {
              blender::bke::node_add_link(
                  *ntree, *add_node, *add_socket_out, *link->tonode, *link->tosock);
              blender::bke::node_remove_link(ntree, *link);
            }
          }

          blender::bke::node_add_link(
              *ntree, *mul_node, *mul_socket_out, *add_node, *add_socket_A);
          blender::bke::node_add_link(
              *ntree, *detail_from_node, *detail_from_socket, *sub2_node, *sub2_socket_B);
          blender::bke::node_add_link(
              *ntree, *sub2_node, *sub2_socket_out, *add_node, *add_socket_B);
        }
        else {
          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == fac_socket) {
              blender::bke::node_add_link(
                  *ntree, *mul_node, *mul_socket_out, *link->tonode, *link->tosock);
              blender::bke::node_remove_link(ntree, *link);
            }
          }
        }

        blender::bke::node_add_link(*ntree, *node, *fac_socket, *mul_node, *mul_socket_A);
        blender::bke::node_add_link(
            *ntree, *detail_from_node, *detail_from_socket, *clamp_node, *clamp_socket_value);
        blender::bke::node_add_link(
            *ntree, *clamp_node, *clamp_socket_out, *mul_node, *mul_socket_B);
      }
    }
    else {
      if (*detail < 1.0f) {
        if (!ELEM(noise_type, SHD_NOISE_RIDGED_MULTIFRACTAL, SHD_NOISE_HETERO_TERRAIN)) {
          /* Add Multiply Math node behind Fac output. */

          bNode *mul_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
          mul_node->parent = node->parent;
          mul_node->custom1 = NODE_MATH_MULTIPLY;
          mul_node->locx_legacy = node->locx_legacy;
          mul_node->locy_legacy = node->locy_legacy + 40.0f;
          mul_node->flag |= NODE_COLLAPSED;
          bNodeSocket *mul_socket_A = static_cast<bNodeSocket *>(
              BLI_findlink(&mul_node->inputs, 0));
          bNodeSocket *mul_socket_B = static_cast<bNodeSocket *>(
              BLI_findlink(&mul_node->inputs, 1));
          bNodeSocket *mul_socket_out = blender::bke::node_find_socket(
              *mul_node, SOCK_OUT, "Value");

          *version_cycles_node_socket_float_value(mul_socket_B) = *detail;

          if (noise_type == SHD_NOISE_MULTIFRACTAL) {
            /* Add an Add Math node after Multiply Math node. */

            bNode *add_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
            add_node->parent = node->parent;
            add_node->custom1 = NODE_MATH_ADD;
            add_node->locx_legacy = node->locx_legacy;
            add_node->locy_legacy = node->locy_legacy + 80.0f;
            add_node->flag |= NODE_COLLAPSED;
            bNodeSocket *add_socket_A = static_cast<bNodeSocket *>(
                BLI_findlink(&add_node->inputs, 0));
            bNodeSocket *add_socket_B = static_cast<bNodeSocket *>(
                BLI_findlink(&add_node->inputs, 1));
            bNodeSocket *add_socket_out = blender::bke::node_find_socket(
                *add_node, SOCK_OUT, "Value");

            *version_cycles_node_socket_float_value(add_socket_B) = 1.0f - *detail;

            LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
              if (link->fromsock == fac_socket) {
                blender::bke::node_add_link(
                    *ntree, *add_node, *add_socket_out, *link->tonode, *link->tosock);
                blender::bke::node_remove_link(ntree, *link);
              }
            }

            blender::bke::node_add_link(
                *ntree, *mul_node, *mul_socket_out, *add_node, *add_socket_A);
          }
          else {
            LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
              if (link->fromsock == fac_socket) {
                blender::bke::node_add_link(
                    *ntree, *mul_node, *mul_socket_out, *link->tonode, *link->tosock);
                blender::bke::node_remove_link(ntree, *link);
              }
            }
          }

          blender::bke::node_add_link(*ntree, *node, *fac_socket, *mul_node, *mul_socket_A);

          *detail = 0.0f;
        }
      }
      else {
        *detail = std::fminf(*detail - 1.0f, 14.0f);
      }
    }

    bNodeSocket *roughness_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Roughness");
    float *roughness = version_cycles_node_socket_float_value(roughness_socket);
    bNodeSocket *lacunarity_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Lacunarity");
    float *lacunarity = version_cycles_node_socket_float_value(lacunarity_socket);

    *roughness = std::fmaxf(*roughness, 1e-5f);
    *lacunarity = std::fmaxf(*lacunarity, 1e-5f);

    if (roughness_link != nullptr) {
      /* Add Maximum Math node after output of roughness_from_node. Add Multiply Math node and
       * Power Math node before Roughness input. */

      bNode *max1_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
      max1_node->parent = node->parent;
      max1_node->custom1 = NODE_MATH_MAXIMUM;
      max1_node->locx_legacy = node->locx_legacy;
      max1_node->locy_legacy = node->locy_legacy - 400.0f + locy_offset;
      max1_node->flag |= NODE_COLLAPSED;
      bNodeSocket *max1_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&max1_node->inputs, 0));
      bNodeSocket *max1_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&max1_node->inputs, 1));
      bNodeSocket *max1_socket_out = blender::bke::node_find_socket(*max1_node, SOCK_OUT, "Value");

      bNode *mul_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
      mul_node->parent = node->parent;
      mul_node->custom1 = NODE_MATH_MULTIPLY;
      mul_node->locx_legacy = node->locx_legacy;
      mul_node->locy_legacy = node->locy_legacy - 360.0f + locy_offset;
      mul_node->flag |= NODE_COLLAPSED;
      bNodeSocket *mul_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 0));
      bNodeSocket *mul_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&mul_node->inputs, 1));
      bNodeSocket *mul_socket_out = blender::bke::node_find_socket(*mul_node, SOCK_OUT, "Value");

      bNode *pow_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
      pow_node->parent = node->parent;
      pow_node->custom1 = NODE_MATH_POWER;
      pow_node->locx_legacy = node->locx_legacy;
      pow_node->locy_legacy = node->locy_legacy - 320.0f + locy_offset;
      pow_node->flag |= NODE_COLLAPSED;
      bNodeSocket *pow_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&pow_node->inputs, 0));
      bNodeSocket *pow_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&pow_node->inputs, 1));
      bNodeSocket *pow_socket_out = blender::bke::node_find_socket(*pow_node, SOCK_OUT, "Value");

      *version_cycles_node_socket_float_value(max1_socket_B) = -1e-5f;
      *version_cycles_node_socket_float_value(mul_socket_B) = -1.0f;
      *version_cycles_node_socket_float_value(pow_socket_A) = *lacunarity;

      blender::bke::node_remove_link(ntree, *roughness_link);
      blender::bke::node_add_link(
          *ntree, *roughness_from_node, *roughness_from_socket, *max1_node, *max1_socket_A);
      blender::bke::node_add_link(*ntree, *max1_node, *max1_socket_out, *mul_node, *mul_socket_A);
      blender::bke::node_add_link(*ntree, *mul_node, *mul_socket_out, *pow_node, *pow_socket_B);
      blender::bke::node_add_link(*ntree, *pow_node, *pow_socket_out, *node, *roughness_socket);

      if (lacunarity_link != nullptr) {
        /* Add Maximum Math node after output of lacunarity_from_node. */

        bNode *max2_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
        max2_node->parent = node->parent;
        max2_node->custom1 = NODE_MATH_MAXIMUM;
        max2_node->locx_legacy = node->locx_legacy;
        max2_node->locy_legacy = node->locy_legacy - 440.0f + locy_offset;
        max2_node->flag |= NODE_COLLAPSED;
        bNodeSocket *max2_socket_A = static_cast<bNodeSocket *>(
            BLI_findlink(&max2_node->inputs, 0));
        bNodeSocket *max2_socket_B = static_cast<bNodeSocket *>(
            BLI_findlink(&max2_node->inputs, 1));
        bNodeSocket *max2_socket_out = blender::bke::node_find_socket(
            *max2_node, SOCK_OUT, "Value");

        *version_cycles_node_socket_float_value(max2_socket_B) = -1e-5f;

        blender::bke::node_remove_link(ntree, *lacunarity_link);
        blender::bke::node_add_link(
            *ntree, *lacunarity_from_node, *lacunarity_from_socket, *max2_node, *max2_socket_A);
        blender::bke::node_add_link(
            *ntree, *max2_node, *max2_socket_out, *pow_node, *pow_socket_A);
        blender::bke::node_add_link(
            *ntree, *max2_node, *max2_socket_out, *node, *lacunarity_socket);
      }
    }
    else if ((lacunarity_link != nullptr) && (roughness_link == nullptr)) {
      /* Add Maximum Math node after output of lacunarity_from_node. Add Power Math node before
       * Roughness input. */

      bNode *max2_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
      max2_node->parent = node->parent;
      max2_node->custom1 = NODE_MATH_MAXIMUM;
      max2_node->locx_legacy = node->locx_legacy;
      max2_node->locy_legacy = node->locy_legacy - 360.0f + locy_offset;
      max2_node->flag |= NODE_COLLAPSED;
      bNodeSocket *max2_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&max2_node->inputs, 0));
      bNodeSocket *max2_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&max2_node->inputs, 1));
      bNodeSocket *max2_socket_out = blender::bke::node_find_socket(*max2_node, SOCK_OUT, "Value");

      bNode *pow_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
      pow_node->parent = node->parent;
      pow_node->custom1 = NODE_MATH_POWER;
      pow_node->locx_legacy = node->locx_legacy;
      pow_node->locy_legacy = node->locy_legacy - 320.0f + locy_offset;
      pow_node->flag |= NODE_COLLAPSED;
      bNodeSocket *pow_socket_A = static_cast<bNodeSocket *>(BLI_findlink(&pow_node->inputs, 0));
      bNodeSocket *pow_socket_B = static_cast<bNodeSocket *>(BLI_findlink(&pow_node->inputs, 1));
      bNodeSocket *pow_socket_out = blender::bke::node_find_socket(*pow_node, SOCK_OUT, "Value");

      *version_cycles_node_socket_float_value(max2_socket_B) = -1e-5f;
      *version_cycles_node_socket_float_value(pow_socket_A) = *lacunarity;
      *version_cycles_node_socket_float_value(pow_socket_B) = -(*roughness);

      blender::bke::node_remove_link(ntree, *lacunarity_link);
      blender::bke::node_add_link(
          *ntree, *lacunarity_from_node, *lacunarity_from_socket, *max2_node, *max2_socket_A);
      blender::bke::node_add_link(*ntree, *max2_node, *max2_socket_out, *pow_node, *pow_socket_A);
      blender::bke::node_add_link(*ntree, *max2_node, *max2_socket_out, *node, *lacunarity_socket);
      blender::bke::node_add_link(*ntree, *pow_node, *pow_socket_out, *node, *roughness_socket);
    }
    else {
      *roughness = std::pow(*lacunarity, -(*roughness));
    }
  }

  version_socket_update_is_used(ntree);
}

static void versioning_replace_splitviewer(bNodeTree *ntree)
{
  /* Split viewer was replaced with a regular split node, so add a viewer node,
   * and link it to the new split node to achieve the same behavior of the split viewer node. */

  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != CMP_NODE_SPLITVIEWER__DEPRECATED) {
      continue;
    }

    STRNCPY_UTF8(node->idname, "CompositorNodeSplit");
    node->type_legacy = CMP_NODE_SPLIT;
    MEM_freeN(node->storage);
    node->storage = nullptr;

    bNode *viewer_node = blender::bke::node_add_static_node(nullptr, *ntree, CMP_NODE_VIEWER);
    /* Nodes are created stacked on top of each other, so separate them a bit. */
    viewer_node->locx_legacy = node->locx_legacy + node->width + viewer_node->width / 4.0f;
    viewer_node->locy_legacy = node->locy_legacy;
    viewer_node->flag &= ~NODE_PREVIEW;

    bNodeSocket *split_out_socket = blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_OUT, SOCK_IMAGE, PROP_NONE, "Image", "Image");
    bNodeSocket *viewer_in_socket = blender::bke::node_find_socket(*viewer_node, SOCK_IN, "Image");

    blender::bke::node_add_link(*ntree, *node, *split_out_socket, *viewer_node, *viewer_in_socket);
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

static void change_input_socket_to_rotation_type(bNodeTree &ntree,
                                                 bNode &node,
                                                 bNodeSocket &socket)
{
  if (socket.type == SOCK_ROTATION) {
    return;
  }
  socket.type = SOCK_ROTATION;
  STRNCPY_UTF8(socket.idname, "NodeSocketRotation");
  auto *old_value = static_cast<bNodeSocketValueVector *>(socket.default_value);
  auto *new_value = MEM_callocN<bNodeSocketValueRotation>(__func__);
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
    bNode *convert = blender::bke::node_add_node(nullptr, ntree, "FunctionNodeEulerToRotation");
    convert->parent = node.parent;
    convert->locx_legacy = node.locx_legacy - 40;
    convert->locy_legacy = node.locy_legacy;
    link->tonode = convert;
    link->tosock = blender::bke::node_find_socket(*convert, SOCK_IN, "Euler");

    blender::bke::node_add_link(ntree,
                                *convert,
                                *blender::bke::node_find_socket(*convert, SOCK_OUT, "Rotation"),
                                node,
                                socket);
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
    bNode *convert = blender::bke::node_add_node(nullptr, ntree, "FunctionNodeRotationToEuler");
    convert->parent = node.parent;
    convert->locx_legacy = node.locx_legacy + 40;
    convert->locy_legacy = node.locy_legacy;
    link->fromnode = convert;
    link->fromsock = blender::bke::node_find_socket(*convert, SOCK_OUT, "Euler");

    blender::bke::node_add_link(ntree,
                                node,
                                socket,
                                *convert,
                                *blender::bke::node_find_socket(*convert, SOCK_IN, "Rotation"));
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
      bNodeSocket *socket = blender::bke::node_find_socket(*node, SOCK_IN, "Rotation");
      change_input_socket_to_rotation_type(ntree, *node, *socket);
    }
    if (STR_ELEM(node->idname,
                 "GeometryNodeDistributePointsOnFaces",
                 "GeometryNodeObjectInfo",
                 "GeometryNodeInputInstanceRotation"))
    {
      bNodeSocket *socket = blender::bke::node_find_socket(*node, SOCK_OUT, "Rotation");
      change_output_socket_to_rotation_type(ntree, *node, *socket);
    }
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
    bNodeSocket *scale = blender::bke::node_find_socket(*node, SOCK_OUT, "Scale");
    const Span<bNodeLink *> links = out_links_per_socket.lookup(scale);
    if (links.is_empty()) {
      continue;
    }
    bNode *absolute_value = blender::bke::node_add_node(nullptr, ntree, "ShaderNodeVectorMath");
    absolute_value->custom1 = NODE_VECTOR_MATH_ABSOLUTE;
    absolute_value->parent = node->parent;
    absolute_value->locx_legacy = node->locx_legacy + 100;
    absolute_value->locy_legacy = node->locy_legacy - 50;
    blender::bke::node_add_link(*&ntree,
                                *node,
                                *scale,
                                *absolute_value,
                                *static_cast<bNodeSocket *>(absolute_value->inputs.first));
    for (bNodeLink *link : links) {
      link->fromnode = absolute_value;
      link->fromsock = static_cast<bNodeSocket *>(absolute_value->outputs.first);
    }
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

static bool strip_filter_bilinear_to_auto(Strip *strip, void * /*user_data*/)
{
  StripTransform *transform = strip->data->transform;
  if (transform != nullptr && transform->filter == SEQ_TRANSFORM_FILTER_BILINEAR) {
    transform->filter = SEQ_TRANSFORM_FILTER_AUTO;
  }
  return true;
}

void blo_do_versions_410(FileData *fd, Library * /*lib*/, Main *bmain)
{
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
        scene->eevee.fast_gi_bias = default_eevee.fast_gi_bias;
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
            node->custom2 = CMP_NODE_INTERPOLATION_ANISOTROPIC;
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
        Sculpt default_sculpt = blender::dna::shallow_copy(*DNA_struct_default_get(Sculpt));
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
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_filter_bilinear_to_auto, nullptr);
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
}
