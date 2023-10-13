/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_reconstruction_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ambient_occlusion_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 extent = imageSize(in_normal_img).xy;
  if (any(greaterThanEqual(texel, extent))) {
    return;
  }

  SurfaceReconstructResult surf = view_reconstruct_from_depth(hiz_tx, extent, texel);
  if (surf.is_background) {
    /* Do not trace for background */
    imageStore(out_ao_img, ivec3(texel, out_ao_img_layer_index), vec4(0.0));
    return;
  }

  vec3 P = drw_point_view_to_world(surf.vP);
  vec3 V = drw_world_incident_vector(P);
  vec3 Ng = drw_normal_view_to_world(surf.vNg);
  vec3 N = imageLoad(in_normal_img, ivec3(texel, in_normal_img_layer_index)).xyz;

  OcclusionData data = ambient_occlusion_search(
      surf.vP, hiz_tx, texel, uniform_buf.ao.distance, 0.0, 8.0);

  float visibility;
  float unused_visibility_error_out;
  vec3 unused_bent_normal_out;
  ambient_occlusion_eval(
      data, texel, V, N, Ng, 0.0, visibility, unused_visibility_error_out, unused_bent_normal_out);

  imageStore(out_ao_img, ivec3(texel, out_ao_img_layer_index), vec4(saturate(visibility)));
}
