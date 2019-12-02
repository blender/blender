
in vec3 pos;
in float weight;

uniform sampler1D weightTex;

out vec4 weightColor;

vec3 weight_to_rgb(float t)
{
  if (t < 0.0) {
    /* Minimum color, grey */
    return vec3(0.25, 0.25, 0.25);
  }
  else if (t > 1.0) {
    /* Error color */
    return vec3(1.0, 0.0, 1.0);
  }
  else {
    return texture(weightTex, t).rgb;
  }
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  weightColor = vec4(weight_to_rgb(weight), 1.0);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
