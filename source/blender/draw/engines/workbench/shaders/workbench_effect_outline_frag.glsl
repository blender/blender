
void main()
{
  vec3 offset = vec3(world_data.viewport_size_inv, 0.0) * world_data.ui_scale;
  vec2 uv = uvcoordsvar.st;

  uint center_id = texture(objectIdBuffer, uv).r;
  uvec4 adjacent_ids = uvec4(texture(objectIdBuffer, uv + offset.zy).r,
                             texture(objectIdBuffer, uv - offset.zy).r,
                             texture(objectIdBuffer, uv + offset.xz).r,
                             texture(objectIdBuffer, uv - offset.xz).r);

  float outline_opacity = 1.0 - dot(vec4(equal(uvec4(center_id), adjacent_ids)), vec4(0.25));

  fragColor = world_data.object_outline_color * outline_opacity;
}
