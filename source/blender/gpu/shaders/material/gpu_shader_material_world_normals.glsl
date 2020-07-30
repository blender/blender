/* TODO : clean this ifdef mess */
void world_normals_get(out vec3 N)
{
#ifndef VOLUMETRICS
#  ifdef HAIR_SHADER
  vec3 B = normalize(cross(worldNormal, hairTangent));
  float cos_theta;
  if (hairThicknessRes == 1) {
    vec4 rand = texelfetch_noise_tex(gl_FragCoord.xy);
    /* Random cosine normal distribution on the hair surface. */
    cos_theta = rand.x * 2.0 - 1.0;
  }
  else {
    /* Shade as a cylinder. */
    cos_theta = hairThickTime / hairThickness;
  }
  float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
  N = normalize(worldNormal * sin_theta + B * cos_theta);
#  else
  N = gl_FrontFacing ? worldNormal : -worldNormal;
#  endif
#else
  generated_from_orco(vec3(0.0), N);
#endif
}
