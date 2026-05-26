/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual Shadow map output.
 *
 * Meshes are rasterize onto an empty frame-buffer. Each generated fragment then checks which
 * virtual page it is supposed to go and load the physical page address.
 * If a physical page exists, we then use atomicMin to mimic a less-than depth test and write to
 * the destination texel.
 */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_nodetree)
FRAGMENT_SHADER_CREATE_INFO(eevee_geom_iface_info)

#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_shadow_shared.hh"
#include "eevee_shadow_tilemap_lib.bsl.hh"
#include "eevee_surf_common.bsl.hh"

float4 closure_to_rgba_shadow(Closure /*cl*/)
{
  return float4(0.0f);
}

namespace eevee {

struct SurfShadow {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;

  [[storage(SHADOW_RENDER_MAP_BUF_SLOT,
            read)]] const uint (&render_map_buf)[SHADOW_RENDER_MAP_SIZE];

  [[image(SHADOW_ATLAS_IMG_SLOT, read_write, UINT_32)]] uimage2DArrayAtomic shadow_atlas_img;
};

[[fragment]] [[texture_atomic]]
void surf_shadow([[resource_table]] PipelineConstants &pipe,
                 [[resource_table]] SurfShadow &srt,
                 [[resource_table]] const Sampling &sampling,
                 [[front_facing]] const bool front_face,
                 [[frag_coord]] const float4 frag_co)
{
  auto &shadow_iface = interface_get(eevee_shadow_iface_info, shadow_iface);
  auto &shadow_clip = interface_get(eevee_shadow_iface_info, shadow_clip);

  float linear_depth = length(shadow_clip.position);

  /* Clip to light shape. */
  if (dot(shadow_clip.vector, shadow_clip.vector) < 1.0f) {
    gpu_discard_fragment();
    return;
  }

  if (pipe.use_transparency) [[static_branch]] {
    init_globals(front_face);

    nodetree_surface(0.0f);

    float noise_offset = sampling.rng_1D_get(SAMPLING_TRANSPARENCY);
    float random_threshold = pcg4d(float4(g_data.P, noise_offset)).x;

    float transparency = average(g_transmittance);
    if (transparency > random_threshold) {
      gpu_discard_fragment();
      return;
    }
  }

  int2 texel_co = int2(frag_co.xy);

  /* Using bitwise ops is way faster than integer ops. */
  constexpr int page_shift = SHADOW_PAGE_LOD;
  constexpr int page_mask = ~(0xFFFFFFFF << SHADOW_PAGE_LOD);

  int2 tile_co = texel_co >> page_shift;
  int2 texel_page = texel_co & page_mask;

  int view_index = shadow_iface.shadow_view_id;

  int render_page_index = shadow_render_page_index_get(view_index, tile_co);
  uint page_packed = srt.render_map_buf[render_page_index];

  int3 page = int3(shadow_page_unpack(page_packed));
  /* If the page index is invalid this page shouldn't be rendered,
   * however shadow_page_unpack clamps the result to a valid page.
   * Instead of doing an early return (and introducing branching),
   * we simply ensure the page layer is out-of-bounds. */
  page.z = page_packed < SHADOW_MAX_PAGE ? page.z : -1;

  int3 out_texel = int3((page.xy << page_shift) | texel_page, page.z);

  uint u_depth = floatBitsToUint(linear_depth);

  /* Bias to avoid rounding errors on very large clip values.
   * This can happen easily after the addition of the world volume
   * versioning script in 4.2.
   * +1 should be enough but for some reason, some artifacts
   * are only removed if adding 2 ULP.
   * This is equivalent of calling `next_after`, but without the safety. */
  u_depth += 2;

  if (uniform_buf.shadow.use_debug_cost) {
    imageAtomicAdd(srt.shadow_atlas_img, out_texel, 1u);
  }
  else {
    imageAtomicMin(srt.shadow_atlas_img, out_texel, u_depth);
  }
}

}  // namespace eevee
