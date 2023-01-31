
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_matcap_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_world_light_lib.glsl)

/* Special function only to be used with calculate_transparent_weight(). */
float linear_zdepth(float depth, mat4 proj_mat)
{
  if (proj_mat[3][3] == 0.0) {
    float d = 2.0 * depth - 1.0;
    return -proj_mat[3][2] / (d + proj_mat[2][2]);
  }
  else {
    /* Return depth from near plane. */
    float z_delta = -2.0 / proj_mat[2][2];
    return depth * z_delta;
  }
}

/* Based on :
 * McGuire and Bavoil, Weighted Blended Order-Independent Transparency, Journal of
 * Computer Graphics Techniques (JCGT), vol. 2, no. 2, 122–141, 2013
 */
float calculate_transparent_weight(void)
{
  float z = linear_zdepth(gl_FragCoord.z, drw_view.winmat);
#if 0
  /* Eq 10 : Good for surfaces with varying opacity (like particles) */
  float a = min(1.0, alpha * 10.0) + 0.01;
  float b = -gl_FragCoord.z * 0.95 + 1.0;
  float w = a * a * a * 3e2 * b * b * b;
#else
  /* Eq 7 put more emphasis on surfaces closer to the view. */
  // float w = 10.0 / (1e-5 + pow(abs(z) / 5.0, 2.0) + pow(abs(z) / 200.0, 6.0)); /* Eq 7 */
  // float w = 10.0 / (1e-5 + pow(abs(z) / 10.0, 3.0) + pow(abs(z) / 200.0, 6.0)); /* Eq 8 */
  // float w = 10.0 / (1e-5 + pow(abs(z) / 200.0, 4.0)); /* Eq 9 */
  /* Same as eq 7, but optimized. */
  float a = abs(z) / 5.0;
  float b = abs(z) / 200.0;
  b *= b;
  float w = 10.0 / ((1e-5 + a * a) + b * (b * b)); /* Eq 7 */
#endif
  return clamp(w, 1e-2, 3e2);
}

#ifdef WORKBENCH_NEXT

void main()
{
  /* Normal and Incident vector are in viewspace. Lighting is evaluated in viewspace. */
  vec2 uv_viewport = gl_FragCoord.xy * world_data.viewport_size_inv;
  vec3 I = get_view_vector_from_screen_uv(uv_viewport);
  vec3 N = normalize(normal_interp);

  vec3 color = color_interp;

#  ifdef WORKBENCH_COLOR_TEXTURE
  color = workbench_image_color(uv_interp);
#  endif

#  ifdef WORKBENCH_LIGHTING_MATCAP
  vec3 shaded_color = get_matcap_lighting(matcap_tx, color, N, I);
#  endif

#  ifdef WORKBENCH_LIGHTING_STUDIO
  vec3 shaded_color = get_world_lighting(color, _roughness, metallic, N, I);
#  endif

#  ifdef WORKBENCH_LIGHTING_FLAT
  vec3 shaded_color = color;
#  endif

  shaded_color *= get_shadow(N, forceShadowing);

  /* Listing 4 */
  float alpha = alpha_interp * world_data.xray_alpha;
  float weight = calculate_transparent_weight() * alpha;
  out_transparent_accum = vec4(shaded_color * weight, alpha);
  out_revealage_accum = vec4(weight);

  out_object_id = uint(object_id);
}

#else

void main()
{
  /* Normal and Incident vector are in viewspace. Lighting is evaluated in viewspace. */
  vec2 uv_viewport = gl_FragCoord.xy * world_data.viewport_size_inv;
  vec3 I = get_view_vector_from_screen_uv(uv_viewport);
  vec3 N = normalize(normal_interp);

  vec3 color = color_interp;

#  ifdef WORKBENCH_COLOR_TEXTURE
  color = workbench_image_color(uv_interp);
#  endif

#  ifdef WORKBENCH_LIGHTING_MATCAP
  vec3 shaded_color = get_matcap_lighting(matcap_diffuse_tx, matcap_specular_tx, color, N, I);
#  endif

#  ifdef WORKBENCH_LIGHTING_STUDIO
  vec3 shaded_color = get_world_lighting(color, _roughness, metallic, N, I);
#  endif

#  ifdef WORKBENCH_LIGHTING_FLAT
  vec3 shaded_color = color;
#  endif

  shaded_color *= get_shadow(N, forceShadowing);

  /* Listing 4 */
  float weight = calculate_transparent_weight() * alpha_interp;
  out_transparent_accum = vec4(shaded_color * weight, alpha_interp);
  out_revealage_accum = vec4(weight);

  out_object_id = uint(object_id);
}

#endif
