/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual Shadow map tile shader.
 *
 * On TBDR, we can use a three-pass method to perform virtual shadow map updates, leveraging
 * efficient use of tile-based GPUs. Shadow updates rasterize geometry for each view in much the
 * same way as a conventional shadow map render, but for the standard path, there is an additional
 * cost of an atomic-min and store to allow for indirection into the atlas. This setup can lead to
 * excessive overdraw, rasterization and increased complexity in the material depth fragment
 * shader, reducing rendering performance.
 *
 * On a tile-based GPU, as shadow updates are still relative, we can still leverage on-tile depth
 * testing, to avoid atomic-min operations against global memory, and only write out the final
 * depth value stored in each tile. Large memory-less render targets are used to create a virtual
 * render target, where only the updated regions and layers are processed.
 *
 * Firstly, invoke an instance of this shader with PASS_CLEAR to clear the depth values to default
 * for tiles being updated. The first optimization also enables tiles which are not being updated
 * in this pass to be cleared to zero, saving on fragment invocation costs for unused regions of
 * the render target.
 * This allows us to also skip the compute-based tile clear pass.
 *
 * Secondly, eevee_surf_shadow_frag is used to generate depth information which is stored
 * on tile for the closest fragment. The TBDR path uses a simple variant of this shader
 * which just outputs the depth, without any virtual shadow map processing on top.
 *
 * The third pass then runs, writing out only the highest-level final pixel to memory,
 * avoiding the requirement for atomic texture operations.
 *
 * Output shadow atlas page indirection is calculated in the vertex shader, which generates
 * a lattice of quads covering the shadow pages which are to be updated. The quads which
 * belong to shadow pages not being updated in this pass are discarded.
 **/

#include "infos/eevee_shadow_pipeline_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_shadow_page_tile_clear)

#include "eevee_shadow_tilemap_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#if defined(PASS_CLEAR)

void main()
{
  /* The tile clear pass writes out to tile attachment to ensure raster order groups are satisfied,
   * allowing the clear to be guaranteed to happen first, as it is first in submission order. */
  out_tile_depth = FLT_MAX;
}

#elif defined(PASS_DEPTH_STORE)

void main()
{
  /* For storing pass, we store the result from depth in tile memory. */
  uint u_depth = floatBitsToUint(in_tile_depth);

  /* Bias to avoid rounding errors on very large clip values.
   * This can happen easily after the addition of the world volume
   * versioning script in 4.2.
   * +1 should be enough but for some reason, some artifacts
   * are only removed if adding 2 ULP.
   * This is equivalent of calling `next_after`, but without the safety. */
  u_depth += 2;

  /* Write result to atlas. */
#  ifdef GPU_METAL
  /* NOTE: Use the fastest possible write function without any parameter wrapping or conversion. */
  shadow_atlas_img.texture->write(
      u_depth, ushort2(interp_noperspective.out_texel_xy), interp_flat.out_page_z);
#  else
  imageStore(shadow_atlas_img,
             int3(interp_noperspective.out_texel_xy, interp_flat.out_page_z),
             uint4(u_depth));
#  endif
}

#endif
