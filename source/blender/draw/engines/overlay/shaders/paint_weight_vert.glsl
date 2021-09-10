#ifdef FAKE_SHADING
uniform vec3 light_dir;
#endif

in float weight;
in vec3 pos;
in vec3 nor;

out vec2 weight_interp; /* (weight, alert) */
out float color_fac;

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  /* Separate actual weight and alerts for independent interpolation */
  weight_interp = max(vec2(weight, -weight), 0.0);

  /* Saturate the weight to give a hint of the geometry behind the weights. */
#ifdef FAKE_SHADING
  vec3 view_normal = normalize(normal_object_to_view(nor));
  color_fac = abs(dot(view_normal, light_dir));
  color_fac = color_fac * 0.9 + 0.1;

#else
  color_fac = 1.0;
#endif

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
