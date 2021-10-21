
uniform bool depthSet;
uniform bool isCameraBackground;

in vec3 pos;

out vec2 uvs;

void main()
{
  if (isCameraBackground) {
    vec3 vP = (ModelMatrix * vec4(pos, 1.0)).xyz;
    gl_Position = point_view_to_ndc(vP);
  }
  else {
    vec3 world_pos = point_object_to_world(pos);
    gl_Position = point_world_to_ndc(world_pos);
  }

  if (depthSet) {
    /* Result in a position at 1.0 (far plane). Small epsilon to avoid precision issue.
     * This mimics the effect of infinite projection matrix
     * (see http://www.terathon.com/gdc07_lengyel.pdf). */
    gl_Position.z = gl_Position.w - 2.4e-7;
  }

  uvs = pos.xy * 0.5 + 0.5;
}
