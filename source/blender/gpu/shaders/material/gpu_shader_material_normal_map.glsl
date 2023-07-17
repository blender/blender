
#ifdef OBINFO_LIB
void node_normal_map(vec4 tangent, float strength, vec3 texnormal, out vec3 outnormal)
{
  if (all(equal(tangent, vec4(0.0, 0.0, 0.0, 1.0)))) {
    outnormal = g_data.Ni;
    return;
  }
  tangent *= (FrontFacing ? 1.0 : -1.0);
  vec3 B = tangent.w * cross(g_data.Ni, tangent.xyz) * sign(ObjectInfo.w);

  /* Apply strength here instead of in node_normal_map_mix for tangent space. */
  texnormal.xy *= strength;

  outnormal = texnormal.x * tangent.xyz + texnormal.y * B + texnormal.z * g_data.Ni;
  outnormal = normalize(outnormal);
}
#endif

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
  outnormal = normalize(mix(g_data.N, newnormal, max(0.0, strength)));
}
