
void main()
{
  Surfel surfel = surfels_buf[surfel_index];

  vec3 radiance = vec3(0.0);
  radiance += gl_FrontFacing ? surfel.radiance_direct.front.rgb : surfel.radiance_direct.back.rgb;
  radiance += gl_FrontFacing ? surfel.radiance_indirect[1].front.rgb :
                               surfel.radiance_indirect[1].back.rgb;

  switch (eDebugMode(debug_mode)) {
    default:
    case DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL:
      out_color = vec4(pow(surfel.normal * 0.5 + 0.5, vec3(2.2)), 0.0);
      break;
    case DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE:
      out_color = vec4(radiance, 0.0);
      break;
  }

  /* Display surfels as circles. */
  if (distance(P, surfel.position) > surfel_radius) {
    discard;
    return;
  }
}
