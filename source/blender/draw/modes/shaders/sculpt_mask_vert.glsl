
uniform mat4 ModelMatrix;

uniform float maskOpacity;

in vec3 pos;
in float msk;

out vec4 finalColor;

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  float mask = 1.0 - (msk * maskOpacity);
  finalColor = vec4(mask, mask, mask, 1.0);
}
