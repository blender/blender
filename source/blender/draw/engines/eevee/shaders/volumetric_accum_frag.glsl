
/* This shader is used to add default values to the volume accum textures.
 * so it looks similar (transmittance = 1, scattering = 0) */
void main()
{
  FragColor0 = vec4(0.0);
  FragColor1 = vec4(1.0);
}
