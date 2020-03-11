
#pragma BLENDER_REQUIRE(workbench_data_lib.glsl)

vec2 matcap_uv_compute(vec3 I, vec3 N, bool flipped)
{
  /* Quick creation of an orthonormal basis */
  float a = 1.0 / (1.0 + I.z);
  float b = -I.x * I.y * a;
  vec3 b1 = vec3(1.0 - I.x * I.x * a, b, -I.x);
  vec3 b2 = vec3(b, 1.0 - I.y * I.y * a, -I.y);
  vec2 matcap_uv = vec2(dot(b1, N), dot(b2, N));
  if (flipped) {
    matcap_uv.x = -matcap_uv.x;
  }
  return matcap_uv * 0.496 + 0.5;
}

uniform sampler2D matcapDiffuseImage;
uniform sampler2D matcapSpecularImage;

vec3 get_matcap_lighting(vec3 base_color, vec3 N, vec3 I)
{
  bool flipped = world_data.matcap_orientation != 0;
  vec2 uv = matcap_uv_compute(I, N, flipped);

  vec3 diffuse = textureLod(matcapDiffuseImage, uv, 0.0).rgb;
  vec3 specular = textureLod(matcapSpecularImage, uv, 0.0).rgb;

  return diffuse * base_color + specular * float(world_data.use_specular);
}
