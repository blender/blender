
/* Required by some nodes. */
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)

#pragma BLENDER_REQUIRE(closure_type_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_diffuse_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_glossy_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_translucent_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_refraction_lib.glsl)

#pragma BLENDER_REQUIRE(surface_lib.glsl)
#pragma BLENDER_REQUIRE(volumetric_lib.glsl)

#ifdef USE_ALPHA_BLEND
/* Use dual source blending to be able to make a whole range of effects. */
layout(location = 0, index = 0) out vec4 outRadiance;
layout(location = 0, index = 1) out vec4 outTransmittance;

#else /* OPAQUE */
layout(location = 0) out vec4 outRadiance;
layout(location = 1) out vec2 ssrNormals;
layout(location = 2) out vec4 ssrData;
#  ifdef USE_SSS
layout(location = 3) out vec3 sssIrradiance;
layout(location = 4) out float sssRadius;
layout(location = 5) out vec3 sssAlbedo;
#  endif

#endif

void main()
{
  Closure cl = nodetree_exec();

  float holdout = saturate(1.0 - cl.holdout);
  float transmit = saturate(avg(cl.transmittance));
  float alpha = 1.0 - transmit;

#ifdef USE_ALPHA_BLEND
  vec2 uvs = gl_FragCoord.xy * volCoordScale.zw;
  vec3 vol_transmit, vol_scatter;
  volumetric_resolve(uvs, gl_FragCoord.z, vol_transmit, vol_scatter);

  /* Removes part of the volume scattering that have
   * already been added to the destination pixels.
   * Since we do that using the blending pipeline we need to account for material transmittance. */
  vol_scatter -= vol_scatter * cl.transmittance;

  cl.radiance = cl.radiance * holdout * vol_transmit + vol_scatter;
  outRadiance = vec4(cl.radiance, alpha * holdout);
  outTransmittance = vec4(cl.transmittance, transmit) * holdout;
#else
  outRadiance = vec4(cl.radiance, holdout);
  ssrNormals = cl.ssr_normal;
  ssrData = cl.ssr_data;
#  ifdef USE_SSS
  sssIrradiance = cl.sss_irradiance;
  sssRadius = cl.sss_radius;
  sssAlbedo = cl.sss_albedo;
#  endif
#endif

  /* For Probe capture */
#ifdef USE_SSS
  float fac = float(!sssToggle);

  /* TODO(fclem): we shouldn't need this.
   * Just disable USE_SSS when USE_REFRACTION is enabled. */
#  ifdef USE_REFRACTION
  /* SSRefraction pass is done after the SSS pass.
   * In order to not lose the diffuse light totally we
   * need to merge the SSS radiance to the main radiance. */
  fac = 1.0;
#  endif

  outRadiance.rgb += cl.sss_irradiance.rgb * cl.sss_albedo.rgb * fac;
#endif

#ifndef USE_ALPHA_BLEND
  float alpha_div = safe_rcp(alpha);
  outRadiance.rgb *= alpha_div;
  ssrData.rgb *= alpha_div;
#  ifdef USE_SSS
  sssAlbedo.rgb *= alpha_div;
#  endif
#endif

#ifdef LOOKDEV
  /* Lookdev spheres are rendered in front. */
  gl_FragDepth = 0.0;
#endif
}
