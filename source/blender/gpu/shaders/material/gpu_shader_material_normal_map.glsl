void node_normal_map(vec4 info, vec4 tangent, vec3 normal, vec3 texnormal, out vec3 outnormal)
{
  if (all(equal(tangent, vec4(0.0, 0.0, 0.0, 1.0)))) {
    outnormal = normal;
    return;
  }
  tangent *= (gl_FrontFacing ? 1.0 : -1.0);
  vec3 B = tangent.w * cross(normal, tangent.xyz) * sign(info.w);

  outnormal = texnormal.x * tangent.xyz + texnormal.y * B + texnormal.z * normal;
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

void node_normal_map_mix(float strength, vec3 newnormal, vec3 oldnormal, out vec3 outnormal)
{
  outnormal = normalize(mix(oldnormal, newnormal, max(strength, 0.0)));
}
