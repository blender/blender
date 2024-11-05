/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * NPR Evaluation.
 */

#include "common_hair_lib.glsl"
#include "draw_view_lib.glsl"
#include "eevee_ambient_occlusion_lib.glsl"
#include "eevee_deferred_combine_lib.glsl"
#include "eevee_light_eval_lib.glsl"
#include "eevee_nodetree_lib.glsl"
#include "eevee_renderpass_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_surf_lib.glsl"

#define TEX_HANDLE_NULL 0u
#define TEX_HANDLE_RP_COLOR 1u
#define TEX_HANDLE_RP_VALUE 2u
#define TEX_HANDLE_COMBINED_COLOR 10u
#define TEX_HANDLE_DIFFUSE_COLOR 11u
#define TEX_HANDLE_DIFFUSE_DIRECT 12u
#define TEX_HANDLE_DIFFUSE_INDIRECT 13u
#define TEX_HANDLE_SPECULAR_COLOR 14u
#define TEX_HANDLE_SPECULAR_DIRECT 15u
#define TEX_HANDLE_SPECULAR_INDIRECT 16u
#define TEX_HANDLE_POSITION 17u
#define TEX_HANDLE_NORMAL 18u
#define TEX_HANDLE_BACK_COMBINED_COLOR 19u
#define TEX_HANDLE_BACK_POSITION 20u

vec4 closure_to_rgba(Closure cl)
{
  return vec4(0.0);
}

void npr_input_impl(out TextureHandle combined_color,
                    out TextureHandle diffuse_color,
                    out TextureHandle diffuse_direct,
                    out TextureHandle diffuse_indirect,
                    out TextureHandle specular_color,
                    out TextureHandle specular_direct,
                    out TextureHandle specular_indirect,
                    out TextureHandle position,
                    out TextureHandle normal)
{
  combined_color = TextureHandle(TEX_HANDLE_COMBINED_COLOR, 0);
  diffuse_color = TextureHandle(TEX_HANDLE_DIFFUSE_COLOR, 0);
  diffuse_direct = TextureHandle(TEX_HANDLE_DIFFUSE_DIRECT, 0);
  diffuse_indirect = TextureHandle(TEX_HANDLE_DIFFUSE_INDIRECT, 0);
  specular_color = TextureHandle(TEX_HANDLE_SPECULAR_COLOR, 0);
  specular_direct = TextureHandle(TEX_HANDLE_SPECULAR_DIRECT, 0);
  specular_indirect = TextureHandle(TEX_HANDLE_SPECULAR_INDIRECT, 0);
  position = TextureHandle(TEX_HANDLE_POSITION, 0);
  normal = TextureHandle(TEX_HANDLE_NORMAL, 0);
}

void npr_refraction_impl(out TextureHandle combined_color, out TextureHandle position)
{
  combined_color = TextureHandle(TEX_HANDLE_BACK_COMBINED_COLOR, 0);
  position = TextureHandle(TEX_HANDLE_BACK_POSITION, 0);
}

void input_aov_impl(uint hash, out TextureHandle color, out TextureHandle value)
{
  /* Don't use TEX_HANDLE_NULL for the type even when the aov index is not found.
   * Eases the static compilation of the TextureHandle_eval_impl switch.  */
  color = TextureHandle(TEX_HANDLE_RP_COLOR, aov_color_index(hash));
  color.index += color.index >= 0 ? uniform_buf.render_pass.color_len : 0;
  value = TextureHandle(TEX_HANDLE_RP_VALUE, aov_value_index(hash));
  value.index += value.index >= 0 ? uniform_buf.render_pass.value_len : 0;
}

float4 swap_alpha(vec4 v)
{
  v.a = 1.0 - saturate(v.a);
  return v;
}

vec4 TextureHandle_eval_impl(TextureHandle tex, vec2 offset, bool texel_offset)
{
  if (tex.type == TEX_HANDLE_NULL) {
    return vec4(0.0);
  }

  ivec2 texel = ivec2(gl_FragCoord.xy);
  if (texel_offset) {
    texel += ivec2(offset);
  }
  else {
    /* View-space offset. */
    vec3 vP = drw_point_world_to_view(g_data.P);
    vec2 uv = drw_point_view_to_screen(vP + vec3(offset, 0.0)).xy;
    texel = ivec2(uv * uniform_buf.film.render_extent);
  }

  texel = clamp(texel, ivec2(0), uniform_buf.film.render_extent - ivec2(1));

  float depth = texelFetch(hiz_tx, texel, 0).r;
  vec2 screen_uv = vec2(texel) / uniform_buf.film.render_extent;

  switch (tex.type) {
    case TEX_HANDLE_RP_COLOR:
      return swap_alpha(imageLoad(rp_color_img, ivec3(texel, tex.index)));
    case TEX_HANDLE_RP_VALUE:
      return vec4(imageLoad(rp_value_img, ivec3(texel, tex.index)).rrr, 0.0);
    case TEX_HANDLE_COMBINED_COLOR:
      return texelFetch(radiance_tx, texel, 0);
    case TEX_HANDLE_BACK_COMBINED_COLOR:
      return texelFetch(radiance_back_tx, texel, 0);
    case TEX_HANDLE_POSITION: {
      vec3 position = drw_point_screen_to_world(vec3(screen_uv, depth));
      return vec4(position, 0.0);
    }
    case TEX_HANDLE_BACK_POSITION: {
      float back_depth = texelFetch(hiz_back_tx, texel, 0).r;
      vec3 back_position = drw_point_screen_to_world(vec3(screen_uv, back_depth));
      return vec4(back_position, 0.0);
    }
    default: {
      if (depth == 1.0) {
        switch (tex.type) {
          case TEX_HANDLE_NORMAL: {
            vec3 position = drw_point_screen_to_world(vec3(screen_uv, depth));
            vec3 normal = drw_world_incident_vector(position);
            return vec4(normal, 0.0);
          }
          default:
            return vec4(0.0);
        }
      }
      /* TODO(NPR): This could be further optimized for each case. */
      DeferredCombine dc = deferred_combine(texel);
      deferred_combine_clamp(dc);
      switch (tex.type) {
        case TEX_HANDLE_DIFFUSE_COLOR:
          return vec4(dc.diffuse_color, 0.0);
        case TEX_HANDLE_DIFFUSE_DIRECT:
          return vec4(dc.diffuse_direct, 0.0);
        case TEX_HANDLE_DIFFUSE_INDIRECT:
          return vec4(dc.diffuse_indirect, 0.0);
        case TEX_HANDLE_SPECULAR_COLOR:
          return vec4(dc.specular_color, 0.0);
        case TEX_HANDLE_SPECULAR_DIRECT:
          return vec4(dc.specular_direct, 0.0);
        case TEX_HANDLE_SPECULAR_INDIRECT:
          return vec4(dc.specular_indirect, 0.0);
        case TEX_HANDLE_NORMAL:
          return vec4(dc.average_normal, 0.0);
        default:
          return vec4(0.0);
      }
    }
  }
}

vec4 TextureHandle_eval(TextureHandle tex, vec2 offset, bool texel_offset)
{
  return swap_alpha(TextureHandle_eval_impl(tex, offset, texel_offset));
}

vec4 TextureHandle_eval(TextureHandle tex)
{
  return TextureHandle_eval(tex, vec2(0.0), true);
}

bool light_loop_setup(uint l_idx,
                      bool is_directional,
                      vec3 N,
                      out vec4 out_color,
                      out vec3 out_vector,
                      out float out_distance,
                      out float out_attenuation,
                      out float out_shadow_mask)
{
  LightData light = light_buf[l_idx];
  if (light.color == vec3(0.0)) {
    return false;
  }

  ObjectInfos object_infos = drw_infos[resource_id];
  uchar receiver_light_set = receiver_light_set_get(object_infos);
  if (!light_linking_affects_receiver(light.light_set_membership, receiver_light_set)) {
    return false;
  }

  LightVector lv = light_vector_get(light, is_directional, g_data.P);

  float attenuation = light_attenuation_volume(light, is_directional, lv);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return false;
  }
  vec3 V = drw_world_incident_vector(g_data.P);
  /* TODO(NPR): specular ? */
  vec4 ltc_mat = vec4(1.0, 0.0, 0.0, 1.0); /* No transform, just plain cosine distribution. */
  /* Make the normal point into the light direction, so the diffuse term can be customized. */
  float ltc = light_ltc(utility_tx, light, lv.L, V, lv, ltc_mat);
  attenuation *= ltc * light_power_get(light, LIGHT_DIFFUSE);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return false;
  }

  float shadow_mask = 1.0;
  if (light.tilemap_index != LIGHT_NO_SHADOW) {
    int ray_count = uniform_buf.shadow.ray_count;
    int ray_step_count = uniform_buf.shadow.step_count;
    shadow_mask = shadow_eval(
        light, is_directional, false, false, 0.0, g_data.P, N, ray_count, ray_step_count);
    shadow_mask *= dot(N, lv.L) > 0.0 ? 1.0 : 0.0;
  }

  out_color = vec4(light.color, 1.0);
  out_vector = lv.L;
  out_distance = lv.dist;
  out_attenuation = attenuation;
  out_shadow_mask = shadow_mask;

  return true;
}

#define LIGHT_LOOP_BEGIN( \
    N, out_color, out_vector, out_distance, out_attenuation, out_shadow_mask) \
  LIGHT_FOREACH_ALL_BEGIN(light_cull_buf, \
                          light_zbin_buf, \
                          light_tile_buf, \
                          gl_FragCoord.xy, \
                          drw_point_world_to_view(g_data.P).z, \
                          l_idx, \
                          is_local) \
  if (!light_loop_setup(l_idx, \
                        !is_local, \
                        N, \
                        out_color, \
                        out_vector, \
                        out_distance, \
                        out_attenuation, \
                        out_shadow_mask)) \
  { \
    continue; \
  }

#define LIGHT_LOOP_END() LIGHT_FOREACH_ALL_END()

void main()
{
  init_globals();

  vec4 result = nodetree_npr();
  out_radiance = vec4(result.rgb * result.a, 0.0);
  out_transmittance = vec4(1.0 - result.a);

  /* For AOVs */
  /* TODO(NPR): Move AOV codegen to nodetree_npr. */
  nodetree_surface(0.0);
}
