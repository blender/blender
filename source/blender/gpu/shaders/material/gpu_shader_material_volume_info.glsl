void node_attribute_volume_density(sampler3D tex, out vec4 outcol, out vec3 outvec, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif
  outf = texture(tex, cos).r;
  outvec = vec3(outf, outf, outf);
  outcol = vec4(outf, outf, outf, 1.0);
}

uniform vec3 volumeColor = vec3(1.0);

void node_attribute_volume_color(sampler3D tex, out vec4 outcol, out vec3 outvec, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif

  vec4 value = texture(tex, cos).rgba;
  /* Density is premultiplied for interpolation, divide it out here. */
  if (value.a > 1e-8) {
    value.rgb /= value.a;
  }

  outvec = value.rgb * volumeColor;
  outcol = vec4(outvec, 1.0);
  outf = avg(outvec);
}

void node_attribute_volume_flame(sampler3D tex, out vec4 outcol, out vec3 outvec, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif
  outf = texture(tex, cos).r;
  outvec = vec3(outf, outf, outf);
  outcol = vec4(outf, outf, outf, 1.0);
}

void node_attribute_volume_temperature(
    sampler3D tex, vec2 temperature, out vec4 outcol, out vec3 outvec, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif
  float flame = texture(tex, cos).r;

  outf = (flame > 0.01) ? temperature.x + flame * (temperature.y - temperature.x) : 0.0;
  outvec = vec3(outf, outf, outf);
  outcol = vec4(outf, outf, outf, 1.0);
}

void node_volume_info(sampler3D densitySampler,
                      sampler3D colorSampler,
                      sampler3D flameSampler,
                      vec2 temperature,
                      out vec4 outColor,
                      out float outDensity,
                      out float outFlame,
                      out float outTemprature)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 p = volumeObjectLocalCoord;
#else
  vec3 p = vec3(0.0);
#endif

  outDensity = texture(densitySampler, p).r;

  /* Color is premultiplied for interpolation, divide it out here. */
  vec4 color = texture(colorSampler, p);
  if (color.a > 1e-8) {
    color.rgb /= color.a;
  }
  outColor = vec4(color.rgb * volumeColor, 1.0);

  float flame = texture(flameSampler, p).r;
  outFlame = flame;

  outTemprature = (flame > 0.01) ? temperature.x + flame * (temperature.y - temperature.x) : 0.0;
}
