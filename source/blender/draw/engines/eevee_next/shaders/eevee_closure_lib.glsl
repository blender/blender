/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

/* Return the apparent roughness of a closure compared to a GGX lobe. */
float closure_apparent_roughness_get(ClosureUndetermined cl)
{
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return 1.0;
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      return 1.0;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return to_closure_reflection(cl).roughness;
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      /* TODO: This is incorrect. Depends on IOR. */
      return to_closure_refraction(cl).roughness;
    case CLOSURE_NONE_ID:
    default:
      return 0.0;
  }
}
