#ifndef VOLUMETRICS
void node_ambient_occlusion(vec4 color,
                            float dist,
                            vec3 normal,
                            const float inverted,
                            const float sample_count,
                            out vec4 result_color,
                            out float result_ao)
{
  vec3 bent_normal;
  vec4 rand = texelfetch_noise_tex(gl_FragCoord.xy);
  OcclusionData data = occlusion_search(viewPosition, maxzBuffer, dist, inverted, sample_count);

  vec3 V = cameraVec(worldPosition);
  vec3 N = normalize(normal);
  vec3 Ng = safe_normalize(cross(dFdx(worldPosition), dFdy(worldPosition)));

  vec3 unused;
  occlusion_eval(data, V, N, Ng, inverted, result_ao, unused);
  result_color = result_ao * color;
}
#else
/* Stub ambient occlusion because it is not compatible with volumetrics. */
#  define node_ambient_occlusion(a, b, c, d, e, f) (e = vec4(0); f = 0.0)
#endif
