
/* Uniforms to convert smoke grid values into standard range. */
uniform vec3 volumeColor = vec3(1.0);
uniform vec2 volumeTemperature = vec2(0.0);

/* Generic volume attribute. */
void node_attribute_volume(sampler3D tex, mat4 transform, out vec3 outvec)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif

  /* Optional per-grid transform. */
  if (transform[3][3] != 0.0) {
    cos = (transform * vec4(cos, 1.0)).xyz;
  }

  outvec = texture(tex, cos).rgb;
}

/* Special color attribute for smoke. */
void node_attribute_volume_color(sampler3D tex, mat4 transform, out vec3 outvec)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif

  /* Optional per-grid transform. */
  if (transform[3][3] != 0.0) {
    cos = (transform * vec4(cos, 1.0)).xyz;
  }

  /* Density is premultiplied for interpolation, divide it out here. */
  vec4 value = texture(tex, cos).rgba;
  if (value.a > 1e-8) {
    value.rgb /= value.a;
  }

  outvec = value.rgb * volumeColor;
}

/* Special temperature attribute for smoke. */
void node_attribute_volume_temperature(sampler3D tex, mat4 transform, out float outf)
{
#if defined(MESH_SHADER) && defined(VOLUMETRICS)
  vec3 cos = volumeObjectLocalCoord;
#else
  vec3 cos = vec3(0.0);
#endif

  /* Optional per-grid transform. */
  if (transform[3][3] != 0.0) {
    cos = (transform * vec4(cos, 1.0)).xyz;
  }

  float value = texture(tex, cos).r;
  if (volumeTemperature.x < volumeTemperature.y) {
    outf = (value > 0.01) ?
               volumeTemperature.x + value * (volumeTemperature.y - volumeTemperature.x) :
               0.0;
  }
  else {
    outf = value;
  }
}
