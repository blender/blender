
void main()
{
  float phase = mod((gl_FragCoord.x + gl_FragCoord.y), (size1 + size2));

  if (phase < size1) {
    fragColor = color1;
  }
  else {
    fragColor = color2;
  }
}
