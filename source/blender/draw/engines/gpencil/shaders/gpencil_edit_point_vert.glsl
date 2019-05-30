
uniform mat4 gpModelMatrix;

in vec3 pos;
in vec4 color;
in float size;

out vec4 finalColor;
out float finalThickness;

void main()
{
  gl_Position = point_world_to_ndc((gpModelMatrix * vec4(pos, 1.0)).xyz);
  finalColor = color;
  finalThickness = size;
}
