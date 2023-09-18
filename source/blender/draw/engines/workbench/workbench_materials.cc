/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

#include "BLI_hash.h"
#include "BLI_math_color.h"
/* get_image */
#include "BKE_node.hh"
#include "DNA_node_types.h"
#include "ED_uvedit.hh"
/* get_image */

namespace blender::workbench {

Material::Material() = default;

Material::Material(float3 color)
{
  base_color = color;
  packed_data = Material::pack_data(0.0f, 0.4f, 1.0f);
}

Material::Material(::Object &ob, bool random)
{
  if (random) {
    uint hash = BLI_ghashutil_strhash_p_murmur(ob.id.name);
    if (ob.id.lib) {
      hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob.id.lib->filepath);
    }
    float3 hsv = float3(BLI_hash_int_01(hash), 0.5f, 0.8f);
    hsv_to_rgb_v(hsv, base_color);
  }
  else {
    base_color = ob.color;
  }
  packed_data = Material::pack_data(0.0f, 0.4f, ob.color[3]);
}

Material::Material(::Material &mat)
{
  base_color = &mat.r;
  packed_data = Material::pack_data(mat.metallic, mat.roughness, mat.a);
}

bool Material::is_transparent()
{
  uint32_t full_alpha_ref = 0x00ff0000;
  return (packed_data & full_alpha_ref) != full_alpha_ref;
}

uint32_t Material::pack_data(float metallic, float roughness, float alpha)
{
  /* Remap to Disney roughness. */
  roughness = sqrtf(roughness);
  uint32_t packed_roughness = unit_float_to_uchar_clamp(roughness);
  uint32_t packed_metallic = unit_float_to_uchar_clamp(metallic);
  uint32_t packed_alpha = unit_float_to_uchar_clamp(alpha);
  return (packed_alpha << 16u) | (packed_roughness << 8u) | packed_metallic;
}

void get_material_image(Object *ob,
                        int material_slot,
                        ::Image *&image,
                        ImageUser *&iuser,
                        GPUSamplerState &sampler_state)
{
  const ::bNode *node = nullptr;

  ED_object_get_active_image(ob, material_slot + 1, &image, &iuser, &node, nullptr);
  if (node && image) {
    switch (node->type) {
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
  }
}

}  // namespace blender::workbench
