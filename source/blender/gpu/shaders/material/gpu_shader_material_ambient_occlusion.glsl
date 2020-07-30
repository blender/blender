#ifndef VOLUMETRICS
void node_ambient_occlusion(
    vec4 color, float distance, vec3 normal, out vec4 result_color, out float result_ao)
{
  vec3 bent_normal;
  vec4 rand = texelfetch_noise_tex(gl_FragCoord.xy);
  result_ao = occlusion_compute(normalize(normal), viewPosition, 1.0, rand, bent_normal);
  result_color = result_ao * color;
}
#else
/* Stub ambient occlusion because it is not compatible with volumetrics. */
#  define node_ambient_occlusion(a, b, c, d, e) (e = CLOSURE_DEFAULT)
#endif
