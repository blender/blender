
void main()
{
  float fac = smoothstep(1.0, 0.2, colorFac);
  fragColor.rgb = mix(finalInnerColor.rgb, finalWireColor.rgb, fac);
  fragColor.a = alpha;
  lineOutput = vec4(0.0);
}
