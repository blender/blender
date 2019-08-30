float fresnel_dielectric_cos(float cosi, float eta)
{
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  float c = abs(cosi);
  float g = eta * eta - 1.0 + c * c;
  float result;

  if (g > 0.0) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1.0) / (c * (g - c) + 1.0);
    result = 0.5 * A * A * (1.0 + B * B);
  }
  else {
    result = 1.0; /* TIR (no refracted component) */
  }

  return result;
}

float fresnel_dielectric(vec3 Incoming, vec3 Normal, float eta)
{
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  return fresnel_dielectric_cos(dot(Incoming, Normal), eta);
}

void node_fresnel(float ior, vec3 N, vec3 I, out float result)
{
  N = normalize(N);
  /* handle perspective/orthographic */
  vec3 I_view = (ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);

  float eta = max(ior, 0.00001);
  result = fresnel_dielectric(I_view, N, (gl_FrontFacing) ? eta : 1.0 / eta);
}
