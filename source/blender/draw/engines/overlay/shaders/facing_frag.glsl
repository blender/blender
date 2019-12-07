out vec4 fragColor;

void main()
{
  fragColor = gl_FrontFacing ? colorFaceFront : colorFaceBack;
}
