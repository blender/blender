
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_matcap_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_world_light_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_cavity_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_curvature_lib.glsl)

void main()
{
  vec2 uv = uvcoordsvar.st;
  /* Normal and Incident vector are in viewspace. Lighting is evaluated in viewspace. */
  vec3 V = get_view_vector_from_screen_uv(uv);
  vec3 N = workbench_normal_decode(texture(normal_tx, uv));
  vec4 mat_data = texture(material_tx, uv);
  float depth = texture(depth_tx, uv).r;

  vec3 base_color = mat_data.rgb;
  vec4 color = world_data.background_color;

  /* Background pixels. */
  if (depth != 1.0) {
#ifdef WORKBENCH_LIGHTING_MATCAP
    /* When using matcaps, mat_data.a is the back-face sign. */
    N = (mat_data.a > 0.0) ? N : -N;
    color.rgb = get_matcap_lighting(matcap_tx, base_color, N, V);
#endif

#ifdef WORKBENCH_LIGHTING_STUDIO
    float roughness, metallic;
    workbench_float_pair_decode(mat_data.a, roughness, metallic);
    color.rgb = get_world_lighting(base_color, roughness, metallic, N, V);
#endif

#ifdef WORKBENCH_LIGHTING_FLAT
    color.rgb = base_color;
#endif

#if defined(WORKBENCH_CAVITY) || defined(WORKBENCH_CURVATURE)
    float cavity = 0.0, edges = 0.0, curvature = 0.0;

#  ifdef WORKBENCH_CAVITY
    cavity_compute(uv, depth_tx, normal_tx, cavity, edges);
#  endif

#  ifdef WORKBENCH_CURVATURE
    curvature_compute(uv, object_id_tx, normal_tx, curvature);
#  endif

    float final_cavity_factor = clamp(
        (1.0 - cavity) * (1.0 + edges) * (1.0 + curvature), 0.0, 4.0);

    color.rgb *= final_cavity_factor;
#endif

    bool shadow = texture(stencil_tx, uv).r != 0;
    color.rgb *= get_shadow(N, shadow);

    color.a = 1.0f;
  }

#ifdef WORKBENCH_OUTLINE
  vec3 offset = vec3(world_data.viewport_size_inv, 0.0) * world_data.ui_scale;

  uint center_id = texture(object_id_tx, uv).r;
  uvec4 adjacent_ids = uvec4(texture(object_id_tx, uv + offset.zy).r,
                             texture(object_id_tx, uv - offset.zy).r,
                             texture(object_id_tx, uv + offset.xz).r,
                             texture(object_id_tx, uv - offset.xz).r);

  float outline_opacity = 1.0 - dot(vec4(equal(uvec4(center_id), adjacent_ids)), vec4(0.25));
  color = mix(color, world_data.object_outline_color, outline_opacity);
#endif

  if (all(equal(color, world_data.background_color))) {
    discard;
  }
  else {
    fragColor = color;
  }
}
