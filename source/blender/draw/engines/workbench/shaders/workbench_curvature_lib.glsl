
float curvature_soft_clamp(float curvature, float control)
{
  if (curvature < 0.5 / control) {
    return curvature * (1.0 - curvature * control);
  }
  return 0.25 / control;
}

void curvature_compute(vec2 uv,
                       usampler2D objectIdBuffer,
                       sampler2D normalBuffer,
                       out float curvature)
{
  curvature = 0.0;

  vec3 offset = vec3(world_data.viewport_size_inv, 0.0) * world_data.ui_scale;
  uint object_up = texture(objectIdBuffer, uv + offset.zy).r;
  uint object_down = texture(objectIdBuffer, uv - offset.zy).r;
  uint object_right = texture(objectIdBuffer, uv + offset.xz).r;
  uint object_left = texture(objectIdBuffer, uv - offset.xz).r;

  /* Remove object outlines. */
  if ((object_up != object_down) || (object_right != object_left)) {
    return;
  }
  /* Avoid shading background pixels. */
  if ((object_up == object_right) && (object_right == 0u)) {
    return;
  }

  float normal_up = workbench_normal_decode(texture(normalBuffer, uv + offset.zy)).g;
  float normal_down = workbench_normal_decode(texture(normalBuffer, uv - offset.zy)).g;
  float normal_right = workbench_normal_decode(texture(normalBuffer, uv + offset.xz)).r;
  float normal_left = workbench_normal_decode(texture(normalBuffer, uv - offset.xz)).r;

  float normal_diff = (normal_up - normal_down) + (normal_right - normal_left);

  if (normal_diff < 0) {
    curvature = -2.0 * curvature_soft_clamp(-normal_diff, world_data.curvature_valley);
  }
  else {
    curvature = 2.0 * curvature_soft_clamp(normal_diff, world_data.curvature_ridge);
  }
}
