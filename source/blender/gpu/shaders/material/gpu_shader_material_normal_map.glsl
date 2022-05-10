void node_normal_map(vec4 tangent, vec3 texnormal, out vec3 outnormal)
{
  if (all(equal(tangent, vec4(0.0, 0.0, 0.0, 1.0)))) {
    outnormal = g_data.N;
    return;
  }
  tangent *= (FrontFacing ? 1.0 : -1.0);
  vec3 B = tangent.w * cross(g_data.N, tangent.xyz) * sign(ObjectInfo.w);

  outnormal = texnormal.x * tangent.xyz + texnormal.y * B + texnormal.z * g_data.N;
  outnormal = normalize(outnormal);
}

void color_to_normal_new_shading(vec3 color, out vec3 normal)
{
  normal = vec3(2.0) * color - vec3(1.0);
}

void color_to_blender_normal_new_shading(vec3 color, out vec3 normal)
{
  normal = vec3(2.0, -2.0, -2.0) * color - vec3(1.0);
}

void node_normal_map_mix(float strength, vec3 newnormal, out vec3 outnormal)
{
  outnormal = normalize(mix(g_data.N, newnormal, max(strength, 0.0)));
}
