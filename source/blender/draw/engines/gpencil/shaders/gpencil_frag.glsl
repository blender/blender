/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_geometry)

#include "draw_colormanagement_lib.glsl"
#include "draw_grease_pencil_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

float3 gpencil_lighting()
{
  float3 light_accum = float3(0.0f);
  for (int i = 0; i < GPENCIL_LIGHT_BUFFER_LEN; i++) {
    if (float3(gp_lights[i]._color).x == -1.0f) {
      break;
    }
    float3 L = gp_lights[i]._position - gp_interp.pos;
    float vis = 1.0f;
    gpLightType type = gpLightType(floatBitsToUint(gp_lights[i]._type));
    /* Spot Attenuation. */
    if (type == GP_LIGHT_TYPE_SPOT) {
      float3x3 rot_scale = float3x3(gp_lights[i]._right, gp_lights[i]._up, gp_lights[i]._forward);
      float3 local_L = rot_scale * L;
      local_L /= abs(local_L.z);
      float ellipse = inversesqrt(length_squared(local_L));
      vis *= smoothstep(
          0.0f, 1.0f, (ellipse - gp_lights[i]._spot_size) / gp_lights[i]._spot_blend);
      /* Also mask +Z cone. */
      vis *= step(0.0f, local_L.z);
    }
    /* Inverse square decay. Skip for suns. */
    float L_len_sqr = length_squared(L);
    if (type < GP_LIGHT_TYPE_SUN) {
      vis /= L_len_sqr;
    }
    else {
      L = gp_lights[i]._forward;
      L_len_sqr = 1.0f;
    }
    /* Lambertian falloff */
    if (type != GP_LIGHT_TYPE_AMBIENT) {
      L /= sqrt(L_len_sqr);
      vis *= clamp(dot(gp_normal, L), 0.0f, 1.0f);
    }
    light_accum += vis * gp_lights[i]._color;
  }
  /* Clamp to avoid NaNs. */
  return clamp(light_accum, 0.0f, 1e10f);
}

void main()
{
  float4 col;
  if (flag_test(gp_interp_flat.mat_flag, GP_STROKE_TEXTURE_USE)) {
    bool premul = flag_test(gp_interp_flat.mat_flag, GP_STROKE_TEXTURE_PREMUL);
    col = texture_read_as_linearrgb(gp_stroke_tx, premul, gp_interp.uv);
  }
  else if (flag_test(gp_interp_flat.mat_flag, GP_FILL_TEXTURE_USE)) {
    bool use_clip = flag_test(gp_interp_flat.mat_flag, GP_FILL_TEXTURE_CLIP);
    float2 uvs = (use_clip) ? clamp(gp_interp.uv, 0.0f, 1.0f) : gp_interp.uv;
    bool premul = flag_test(gp_interp_flat.mat_flag, GP_FILL_TEXTURE_PREMUL);
    col = texture_read_as_linearrgb(gp_fill_tx, premul, uvs);
  }
  else if (flag_test(gp_interp_flat.mat_flag, GP_FILL_GRADIENT_USE)) {
    bool radial = flag_test(gp_interp_flat.mat_flag, GP_FILL_GRADIENT_RADIAL);
    float fac = clamp(radial ? length(gp_interp.uv * 2.0f - 1.0f) : gp_interp.uv.x, 0.0f, 1.0f);
    uint matid = gp_interp_flat.mat_flag >> GPENCIl_MATID_SHIFT;
    col = mix(gp_materials[matid].fill_color, gp_materials[matid].fill_mix_color, fac);
  }
  else /* SOLID */ {
    col = float4(1.0f);
  }
  col.rgb *= col.a;

  /* Composite all other colors on top of texture color.
   * Everything is pre-multiply by `col.a` to have the stencil effect. */
  frag_color = col * gp_interp.color_mul + col.a * gp_interp.color_add;

  frag_color.rgb *= gpencil_lighting();

  frag_color *= gpencil_stroke_mask(gp_interp_flat.sspos.xy,
                                    gp_interp_flat.sspos.zw,
                                    gp_interp_flat.sspos_adj.xy,
                                    gp_interp_flat.sspos_adj.zw,
                                    gp_interp.uv,
                                    gp_interp_flat.mat_flag,
                                    gp_interp_noperspective.thickness.x,
                                    gp_interp_noperspective.hardness,
                                    gp_interp_noperspective.thickness.zw);

  /* To avoid aliasing artifacts, we reduce the opacity of small strokes. */
  frag_color *= smoothstep(0.0f, 1.0f, gp_interp_noperspective.thickness.y);

  /* Holdout materials. */
  if (flag_test(gp_interp_flat.mat_flag, GP_STROKE_HOLDOUT | GP_FILL_HOLDOUT)) {
    revealColor = frag_color.aaaa;
  }
  else {
    /* NOT holdout materials.
     * For compatibility with colored alpha buffer.
     * Note that we are limited to mono-chromatic alpha blending here
     * because of the blend equation and the limit of 1 color target
     * when using custom color blending. */
    revealColor = float4(0.0f, 0.0f, 0.0f, frag_color.a);

    if (frag_color.a < 0.001f) {
      gpu_discard_fragment();
      return;
    }
  }

  float2 fb_size = max(float2(textureSize(gp_scene_depth_tx, 0).xy),
                       float2(textureSize(gp_mask_tx, 0).xy));
  float2 uvs = gl_FragCoord.xy / fb_size;
  /* Manual depth test */
  float scene_depth = texture(gp_scene_depth_tx, uvs).r;
  if (gl_FragCoord.z > scene_depth) {
    gpu_discard_fragment();
    return;
  }

  /* FIXME(fclem): Grrr. This is bad for performance but it's the easiest way to not get
   * depth written where the mask obliterate the layer. */
  float mask = texture(gp_mask_tx, uvs).r;
  if (mask < 0.001f) {
    gpu_discard_fragment();
    return;
  }

  /* We override the fragment depth using the fragment shader to ensure a constant value.
   * This has a cost as the depth test cannot happen early.
   * We could do this in the vertex shader but then perspective interpolation of uvs and
   * fragment clipping gets really complicated. */
  if (gp_interp_flat.depth >= 0.0f) {
    gl_FragDepth = gp_interp_flat.depth;
  }
  else {
    gl_FragDepth = gl_FragCoord.z;
  }
}
