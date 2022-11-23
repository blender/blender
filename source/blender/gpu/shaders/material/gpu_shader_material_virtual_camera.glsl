void node_virtual_camera_empty(vec3 co, out vec4 color, out float alpha)
{
  color = vec4(0.0);
  alpha = 0.0;
}

void node_virtual_camera(vec3 co, sampler2D ima, out vec4 color, out float alpha)
{
  color = texture(ima, co.xy);
  alpha = color.a;
}
