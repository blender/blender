/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_constraint_types.h"
#include "DNA_defaults.h"
#include "DNA_genfile.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_sequence_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#undef DNA_GENFILE_VERSIONING_MACROS

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_anim_data.hh"
#include "BKE_colortools.hh"
#include "BKE_customdata.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_report.hh"

#include "MOV_enums.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "readfile.hh"

#include "versioning_common.hh"

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
      bNodeSocket *socket = blender::bke::node_find_socket(*node, SOCK_IN, "Color");
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
      bNodeSocket *socket = blender::bke::node_find_socket(*node, SOCK_IN, "Fac");
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
      bNodeSocket *socket = blender::bke::node_find_socket(*node, SOCK_IN, "Alpha");
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
      bNodeSocket *socket = blender::bke::node_find_socket(*node, SOCK_IN, "Transparency");
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
  bNodeSocket *surface_socket = blender::bke::node_find_socket(*output_node, SOCK_IN, "Surface");

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
      blender::bke::node_remove_link(ntree, *alpha.socket->link);
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
      blender::bke::node_remove_link(ntree, *alpha.socket->link);

      bNode *math_node = blender::bke::node_add_node(nullptr, *ntree, "ShaderNodeMath");
      math_node->custom1 = NODE_MATH_GREATER_THAN;
      math_node->flag |= NODE_COLLAPSED;
      math_node->parent = to_node->parent;
      math_node->locx_legacy = to_node->locx_legacy - math_node->width - 30;
      math_node->locy_legacy = min_ff(to_node->locy_legacy, from_node->locy_legacy);

      bNodeSocket *input_1 = static_cast<bNodeSocket *>(BLI_findlink(&math_node->inputs, 0));
      bNodeSocket *input_2 = static_cast<bNodeSocket *>(BLI_findlink(&math_node->inputs, 1));
      bNodeSocket *output = static_cast<bNodeSocket *>(math_node->outputs.first);
      bNodeSocket *alpha_sock = input_1;
      bNodeSocket *threshold_sock = input_2;

      blender::bke::node_add_link(*ntree, *from_node, *from_socket, *math_node, *alpha_sock);
      blender::bke::node_add_link(*ntree, *math_node, *output, *to_node, *to_socket);

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

  bNodeSocket *existing_out_sock = blender::bke::node_find_socket(
      *output_node, SOCK_IN, "Surface");
  bNodeSocket *volume_sock = blender::bke::node_find_socket(*output_node, SOCK_IN, "Volume");
  if (existing_out_sock->link == nullptr && volume_sock->link) {
    /* Don't apply versioning to a material that only has a volumetric input as this makes the
     * object surface opaque to the camera, hiding the volume inside. */
    return;
  }

  if (output_node->custom1 == SHD_OUTPUT_ALL) {
    /* We do not want to affect Cycles. So we split the output into two specific outputs. */
    output_node->custom1 = SHD_OUTPUT_CYCLES;

    bNode *new_output = blender::bke::node_add_node(nullptr, *ntree, "ShaderNodeOutputMaterial");
    new_output->custom1 = SHD_OUTPUT_EEVEE;
    new_output->parent = output_node->parent;
    new_output->locx_legacy = output_node->locx_legacy;
    new_output->locy_legacy = output_node->locy_legacy - output_node->height - 120;

    auto copy_link = [&](const char *socket_name) {
      bNodeSocket *sock = blender::bke::node_find_socket(*output_node, SOCK_IN, socket_name);
      if (sock && sock->link) {
        bNodeLink *link = sock->link;
        bNodeSocket *to_sock = blender::bke::node_find_socket(*new_output, SOCK_IN, socket_name);
        blender::bke::node_add_link(
            *ntree, *link->fromnode, *link->fromsock, *new_output, *to_sock);
      }
    };

    /* Don't copy surface as that is handled later */
    copy_link("Volume");
    copy_link("Displacement");
    copy_link("Thickness");

    output_node = new_output;
  }

  bNodeSocket *out_sock = blender::bke::node_find_socket(*output_node, SOCK_IN, "Surface");
  bNodeSocket *old_out_sock = blender::bke::node_find_socket(*old_output_node, SOCK_IN, "Surface");

  /* Add mix node for mixing between original material, and transparent BSDF for shadows */
  bNode *mix_node = blender::bke::node_add_node(nullptr, *ntree, "ShaderNodeMixShader");
  STRNCPY(mix_node->label, "Disable Shadow");
  mix_node->flag |= NODE_COLLAPSED;
  mix_node->parent = output_node->parent;
  mix_node->locx_legacy = output_node->locx_legacy;
  mix_node->locy_legacy = output_node->locy_legacy - output_node->height - 120;
  bNodeSocket *mix_fac = static_cast<bNodeSocket *>(BLI_findlink(&mix_node->inputs, 0));
  bNodeSocket *mix_in_1 = static_cast<bNodeSocket *>(BLI_findlink(&mix_node->inputs, 1));
  bNodeSocket *mix_in_2 = static_cast<bNodeSocket *>(BLI_findlink(&mix_node->inputs, 2));
  bNodeSocket *mix_out = static_cast<bNodeSocket *>(BLI_findlink(&mix_node->outputs, 0));
  if (old_out_sock->link != nullptr) {
    blender::bke::node_add_link(*ntree,
                                *old_out_sock->link->fromnode,
                                *old_out_sock->link->fromsock,
                                *mix_node,
                                *mix_in_1);
    if (out_sock->link != nullptr) {
      blender::bke::node_remove_link(ntree, *out_sock->link);
    }
  }
  blender::bke::node_add_link(*ntree, *mix_node, *mix_out, *output_node, *out_sock);

  /* Add light path node to control shadow visibility */
  bNode *lp_node = blender::bke::node_add_node(nullptr, *ntree, "ShaderNodeLightPath");
  lp_node->flag |= NODE_COLLAPSED;
  lp_node->parent = output_node->parent;
  lp_node->locx_legacy = output_node->locx_legacy;
  lp_node->locy_legacy = mix_node->locy_legacy + 35;
  bNodeSocket *is_shadow = blender::bke::node_find_socket(*lp_node, SOCK_OUT, "Is Shadow Ray");
  blender::bke::node_add_link(*ntree, *lp_node, *is_shadow, *mix_node, *mix_fac);
  /* Hide unconnected sockets for cleaner look. */
  LISTBASE_FOREACH (bNodeSocket *, sock, &lp_node->outputs) {
    if (sock != is_shadow) {
      sock->flag |= SOCK_HIDDEN;
    }
  }

  /* Add transparent BSDF to make shadows transparent. */
  bNode *bsdf_node = blender::bke::node_add_node(nullptr, *ntree, "ShaderNodeBsdfTransparent");
  bsdf_node->flag |= NODE_COLLAPSED;
  bsdf_node->parent = output_node->parent;
  bsdf_node->locx_legacy = output_node->locx_legacy;
  bsdf_node->locy_legacy = mix_node->locy_legacy - 35;
  bNodeSocket *bsdf_out = blender::bke::node_find_socket(*bsdf_node, SOCK_OUT, "BSDF");
  blender::bke::node_add_link(*ntree, *bsdf_node, *bsdf_out, *mix_node, *mix_in_2);
}

void do_versions_after_linking_420(FileData *fd, Main *bmain)
{
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
}

static void image_settings_avi_to_ffmpeg(Scene *scene)
{
  /* R_IMF_IMTYPE_AVIRAW and R_IMF_IMTYPE_AVIJPEG. */
  constexpr char deprecated_avi_raw_imtype = 15;
  constexpr char deprecated_avi_jpeg_imtype = 16;
  if (ELEM(scene->r.im_format.imtype, deprecated_avi_raw_imtype, deprecated_avi_jpeg_imtype)) {
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
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    if (smd->type == eSeqModifierType_HueCorrect) {
      HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;
      CurveMapping *cumap = (CurveMapping *)&hcmd->curve_mapping;
      hue_correct_set_wrapping(cumap);
    }
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
          new_shelf_region->regiondata = MEM_callocN<RegionAssetShelf>(__func__);
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

/* Convert EEVEE-Legacy refraction depth to EEVEE-Next thickness tree. */
static void version_refraction_depth_to_thickness_value(bNodeTree *ntree, float thickness)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != SH_NODE_OUTPUT_MATERIAL) {
      continue;
    }

    bNodeSocket *thickness_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Thickness");
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
    bNode *value_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_VALUE);
    value_node->parent = node->parent;
    value_node->locx_legacy = node->locx_legacy;
    value_node->locy_legacy = node->locy_legacy - 160.0f;
    bNodeSocket *socket_value = blender::bke::node_find_socket(*value_node, SOCK_OUT, "Value");

    *version_cycles_node_socket_float_value(socket_value) = thickness;

    blender::bke::node_add_link(*ntree, *value_node, *socket_value, *node, *thickness_socket);
  }

  version_socket_update_is_used(ntree);
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

static void convert_grease_pencil_stroke_hardness_to_softness(GreasePencil *grease_pencil)
{
  using namespace blender;
  for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    bke::greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    const int layer_index = CustomData_get_named_layer_index(
        &drawing.geometry.curve_data_legacy, CD_PROP_FLOAT, "hardness");
    if (layer_index == -1) {
      continue;
    }
    float *data = static_cast<float *>(
        CustomData_get_layer_named_for_write(&drawing.geometry.curve_data_legacy,
                                             CD_PROP_FLOAT,
                                             "hardness",
                                             drawing.geometry.curve_num));
    for (const int i : IndexRange(drawing.geometry.curve_num)) {
      data[i] = 1.0f - data[i];
    }
    /* Rename the layer. */
    STRNCPY_UTF8(drawing.geometry.curve_data_legacy.layers[layer_index].name, "softness");
  }
}

void blo_do_versions_420(FileData *fd, Library * /*lib*/, Main *bmain)
{
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
        scene->eevee.fast_gi_resolution = 2;
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
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_hue_correct_set_wrapping, nullptr);
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
      SequencerToolSettings *sequencer_tool_settings = blender::seq::tool_settings_ensure(scene);
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
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_proxies_timecode_update, nullptr);
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
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_text_data_update, nullptr);
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
        brush->unprojected_size = brush->size *
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
        storage->capture_items = MEM_calloc_arrayN<NodeGeometryAttributeCaptureItem>(
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 402, 65)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_DENOISE) {
          if (node->storage == nullptr) {
            /* Some known files were saved without a valid storage. These are likely corrupt files
             * that have been produced by a non official blender release. The node type will be set
             * to Undefined during linking, see #ntree_set_typeinfo. However, a valid storage might
             * be needed for future versioning (before linking), see
             * #do_version_denoise_menus_to_inputs so we set a valid storage at this stage such
             * that the node becomes well defined. */
            NodeDenoise *ndg = MEM_callocN<NodeDenoise>(__func__);
            ndg->hdr = true;
            ndg->prefilter = CMP_NODE_DENOISE_PREFILTER_ACCURATE;
            node->storage = ndg;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }
}
