
#ifndef HAIR_SHADER
in vec3 pos;
in vec3 nor;
in vec2 u; /* active texture layer */
#  ifdef V3D_SHADING_VERTEX_COLOR
in vec3 c; /* active color */
#  endif
#  define uv u
#else /* HAIR_SHADER */
#  ifdef V3D_SHADING_TEXTURE_COLOR
uniform samplerBuffer u; /* active texture layer */
#  endif
flat out float hair_rand;
#endif /* HAIR_SHADER */

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
out vec3 normal_viewport;
#endif

#ifdef V3D_SHADING_TEXTURE_COLOR
out vec2 uv_interp;
#endif
#ifdef V3D_SHADING_VERTEX_COLOR
out vec3 vertexColor;
#endif

/* From http://libnoise.sourceforge.net/noisegen/index.html */
float integer_noise(int n)
{
  n = (n >> 13) ^ n;
  int nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
  return (float(nn) / 1073741824.0);
}

#ifdef V3D_SHADING_VERTEX_COLOR
vec3 srgb_to_linear_attr(vec3 c)
{
  c = max(c, vec3(0.0));
  vec3 c1 = c * (1.0 / 12.92);
  vec3 c2 = pow((c + 0.055) * (1.0 / 1.055), vec3(2.4));
  return mix(c1, c2, step(vec3(0.04045), c));
}
#endif

vec3 workbench_hair_hair_normal(vec3 tan, vec3 binor, float rand)
{
  /* To "simulate" anisotropic shading, randomize hair normal per strand. */
  vec3 nor = cross(tan, binor);
  nor = normalize(mix(nor, -tan, rand * 0.1));
  float cos_theta = (rand * 2.0 - 1.0) * 0.2;
  float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
  nor = nor * sin_theta + binor * cos_theta;
  return nor;
}

void main()
{
#ifdef HAIR_SHADER
#  ifdef V3D_SHADING_TEXTURE_COLOR
  vec2 uv = hair_get_customdata_vec2(u);
#  endif
  float time, thick_time, thickness;
  vec3 world_pos, tan, binor;
  hair_get_pos_tan_binor_time((ProjectionMatrix[3][3] == 0.0),
                              ModelMatrixInverse,
                              ViewMatrixInverse[3].xyz,
                              ViewMatrixInverse[2].xyz,
                              world_pos,
                              tan,
                              binor,
                              time,
                              thickness,
                              thick_time);

  hair_rand = integer_noise(hair_get_strand_id());
  vec3 nor = workbench_hair_hair_normal(tan, binor, hair_rand);
#else
  vec3 world_pos = point_object_to_world(pos);
#endif
  gl_Position = point_world_to_ndc(world_pos);

#ifdef V3D_SHADING_TEXTURE_COLOR
  uv_interp = uv;
#endif

#ifdef V3D_SHADING_VERTEX_COLOR
#  ifndef HAIR_SHADER
  vertexColor = srgb_to_linear_attr(c);
#  endif
#endif

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
  normal_viewport = normal_object_to_view(nor);
#  ifndef HAIR_SHADER
  normal_viewport = normalize(normal_viewport);
#  endif
#endif

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
