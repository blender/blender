
void main()
{
  vec2 phase = mod(gl_FragCoord.xy, (size * 2));

  if ((phase.x > size && phase.y < size) || (phase.x < size && phase.y > size)) {
    fragColor = color1;
  }
  else {
    fragColor = color2;
  }
}
