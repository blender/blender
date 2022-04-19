
void node_attribute_color(vec4 attr, out vec4 out_attr)
{
  out_attr = attr_load_color_post(attr);
}

void node_attribute_temperature(vec4 attr, out vec4 out_attr)
{
  out_attr.x = attr_load_temperature_post(attr.x);
  out_attr.y = 0.0;
  out_attr.z = 0.0;
  out_attr.w = 1.0;
}

void node_attribute(
    vec4 attr, out vec4 outcol, out vec3 outvec, out float outf, out float outalpha)
{
  outcol = vec4(attr.xyz, 1.0);
  outvec = attr.xyz;
  outf = avg(attr.xyz);
  outalpha = attr.w;
}
