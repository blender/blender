#ifndef VOLUMETRICS
void node_ambient_occlusion(
    vec4 color, float distance, vec3 normal, out vec4 result_color, out float result_ao)
{
  vec3 bent_normal;
  vec4 rand = texelFetch(utilTex, ivec3(ivec2(gl_FragCoord.xy) % LUT_SIZE, 2.0), 0);
  result_ao = occlusion_compute(normalize(normal), viewPosition, 1.0, rand, bent_normal);
  result_color = result_ao * color;
}
#else
/* Stub ambient occlusion because it is not compatible with volumetrics. */
#  define node_ambient_occlusion
#endif
