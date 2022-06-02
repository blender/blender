
void main()
{
  fragColor = gl_FrontFacing ? colorFaceFront : colorFaceBack;
}
