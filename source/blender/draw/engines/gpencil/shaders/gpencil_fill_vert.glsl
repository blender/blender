
in vec3 pos;
in vec4 color;
in vec2 texCoord;

out vec4 finalColor;
out vec2 texCoord_interp;

void main(void)
{
  gl_Position = point_object_to_ndc(pos);
  finalColor = color;
  texCoord_interp = texCoord;
}
