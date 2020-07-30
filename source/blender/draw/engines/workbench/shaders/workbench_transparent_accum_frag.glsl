
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_shader_interface_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_matcap_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_world_light_lib.glsl)

/* Revealage is actually stored in transparentAccum alpha channel.
 * This is a workaround to older hardware not having separate blend equation per render target. */
layout(location = 0) out vec4 transparentAccum;
layout(location = 1) out vec4 revealageAccum;

/* Note: Blending will be skipped on objectId because output is a non-normalized integer buffer. */
layout(location = 2) out uint objectId;

/* Special function only to be used with calculate_transparent_weight(). */
float linear_zdepth(float depth, vec4 viewvecs[2], mat4 proj_mat)
{
  if (proj_mat[3][3] == 0.0) {
    float d = 2.0 * depth - 1.0;
    return -proj_mat[3][2] / (d + proj_mat[2][2]);
  }
  else {
    /* Return depth from near plane. */
    return depth * viewvecs[1].z;
  }
}

/* Based on :
 * McGuire and Bavoil, Weighted Blended Order-Independent Transparency, Journal of
 * Computer Graphics Techniques (JCGT), vol. 2, no. 2, 122–141, 2013
 */
float calculate_transparent_weight(void)
{
  float z = linear_zdepth(gl_FragCoord.z, ViewVecs, ProjectionMatrix);
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

void main()
{
  /* Normal and Incident vector are in viewspace. Lighting is evaluated in viewspace. */
  vec2 uv_viewport = gl_FragCoord.xy * world_data.viewport_size_inv;
  vec3 I = get_view_vector_from_screen_uv(uv_viewport);
  vec3 N = normalize(normal_interp);

  vec3 color = color_interp;

#ifdef V3D_SHADING_TEXTURE_COLOR
  color = workbench_image_color(uv_interp);
#endif

#ifdef V3D_LIGHTING_MATCAP
  vec3 shaded_color = get_matcap_lighting(color, N, I);
#endif

#ifdef V3D_LIGHTING_STUDIO
  vec3 shaded_color = get_world_lighting(color, roughness, metallic, N, I);
#endif

#ifdef V3D_LIGHTING_FLAT
  vec3 shaded_color = color;
#endif

  shaded_color *= get_shadow(N);

  /* Listing 4 */
  float weight = calculate_transparent_weight() * alpha_interp;
  transparentAccum = vec4(shaded_color * weight, alpha_interp);
  revealageAccum = vec4(weight);

  objectId = uint(object_id);
}
