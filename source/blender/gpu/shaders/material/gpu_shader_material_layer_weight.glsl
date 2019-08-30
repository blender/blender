void node_layer_weight(float blend, vec3 N, vec3 I, out float fresnel, out float facing)
{
  N = normalize(N);

  /* fresnel */
  float eta = max(1.0 - blend, 0.00001);
  vec3 I_view = (ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);

  fresnel = fresnel_dielectric(I_view, N, (gl_FrontFacing) ? 1.0 / eta : eta);

  /* facing */
  facing = abs(dot(I_view, N));
  if (blend != 0.5) {
    blend = clamp(blend, 0.0, 0.99999);
    blend = (blend < 0.5) ? 2.0 * blend : 0.5 / (1.0 - blend);
    facing = pow(facing, blend);
  }
  facing = 1.0 - facing;
}
