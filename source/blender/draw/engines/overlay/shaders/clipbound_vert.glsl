
uniform vec3 boundbox[8];

void main()
{
  vec3 world_pos = boundbox[gl_VertexID];
  gl_Position = point_world_to_ndc(world_pos);

  /* Result in a position at 1.0 (far plane). Small epsilon to avoid precision issue.
   * This mimics the effect of infinite projection matrix
   * (see http://www.terathon.com/gdc07_lengyel.pdf). */
  gl_Position.z = gl_Position.w - 2.4e-7;
}
