
void main()
{
  out_color = finalColor;
  out_color.a *= opacity;
  /* Writing to this second texture is necessary to avoid undefined behavior. */
  lineOutput = vec4(0.0);
}
