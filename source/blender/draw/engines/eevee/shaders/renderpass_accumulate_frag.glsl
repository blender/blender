
/** Stupidly simple shader to allow alpha blended accumulation. */

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  fragColor = texelFetch(inputBuffer, texel, 0);
}
