uniform mat4 ModelMatrix;

in vec3 pos;
in vec4 color;
in float size;

out vec4 finalColor;
out float finalThickness;

void main()
{
  gl_Position = point_object_to_ndc(pos);
  finalColor = color;
  finalThickness = size;
}
