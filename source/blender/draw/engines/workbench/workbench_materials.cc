/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_math_color.h"

#include "IMB_colormanagement.hh"

/* get_image */
#include "BKE_node_legacy_types.hh"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "ED_uvedit.hh"
/* get_image */

namespace blender::workbench {

Material::Material(::Object &ob, bool random)
{
  if (random) {
    uint hash = BLI_ghashutil_strhash_p_murmur(ob.id.name);
    if (ob.id.lib) {
      hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob.id.lib->filepath);
    }
    float3 hsv = float3(BLI_hash_int_01(hash), 0.5f, 0.8f);
    hsv_to_rgb_v(hsv, base_color);
    IMB_colormanagement_rec709_to_scene_linear(base_color, base_color);
  }
  else {
    base_color = ob.color;
  }
  packed_data = Material::pack_data(0.0f, 0.4f, ob.color[3]);
}

MaterialTexture::MaterialTexture(Object *ob, int material_index)
{
  const ::bNode *node = nullptr;

  ::Image *image = nullptr;
  ImageUser *user = nullptr;
  ED_object_get_active_image(ob, material_index + 1, &image, &user, &node, nullptr);
  if (!node || !image) {
    return;
  }

  switch (node->type_legacy) {
    case SH_NODE_TEX_IMAGE: {
      const NodeTexImage *storage = static_cast<NodeTexImage *>(node->storage);
      const bool use_filter = (storage->interpolation != SHD_INTERP_CLOSEST);
      sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_LINEAR, use_filter);
      switch (storage->extension) {
        case SHD_IMAGE_EXTENSION_EXTEND:
        default:
          sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_EXTEND;
          sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_EXTEND;
          break;
        case SHD_IMAGE_EXTENSION_REPEAT:
          sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_REPEAT;
          sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_REPEAT;
          break;
        case SHD_IMAGE_EXTENSION_MIRROR:
          sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT;
          sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT;
          break;
        case SHD_IMAGE_EXTENSION_CLIP:
          sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;
          sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;
          break;
      }
      break;
    }
    case SH_NODE_TEX_ENVIRONMENT: {
      const NodeTexEnvironment *storage = static_cast<NodeTexEnvironment *>(node->storage);
      const bool use_filter = (storage->interpolation != SHD_INTERP_CLOSEST);
      sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_LINEAR, use_filter);
      break;
    }
    default:
      BLI_assert_msg(0, "Node type not supported by workbench");
  }

  gpu = BKE_image_get_gpu_material_texture(image, user, true);
  premultiplied = image->alpha_mode == IMA_ALPHA_PREMUL;
  alpha_cutoff = !ELEM(image->alpha_mode, IMA_ALPHA_IGNORE, IMA_ALPHA_CHANNEL_PACKED);
  name = image->id.name;
}

MaterialTexture::MaterialTexture(::Image *image, ImageUser *user /* = nullptr */)
{
  gpu = BKE_image_get_gpu_material_texture(image, user, true);
  premultiplied = image->alpha_mode == IMA_ALPHA_PREMUL;
  alpha_cutoff = !ELEM(image->alpha_mode, IMA_ALPHA_IGNORE, IMA_ALPHA_CHANNEL_PACKED);
  name = image->id.name;
}

}  // namespace blender::workbench
