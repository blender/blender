
uniform float normalSize;
uniform sampler2D depthTex;
uniform float alpha = 1.0;

in vec3 pos;
in vec4 lnor;
in vec4 vnor;
in vec4 norAndFlag;

flat out vec4 finalColor;

bool test_occlusion()
{
  vec3 ndc = (gl_Position.xyz / gl_Position.w) * 0.5 + 0.5;
  return (ndc.z - 0.00035) > texture(depthTex, ndc.xy).r;
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 nor;
  /* Select the right normal by checking if the generic attribute is used. */
  if (!all(equal(lnor.xyz, vec3(0)))) {
    if (lnor.w < 0.0) {
      finalColor = vec4(0.0);
      return;
    }
    nor = lnor.xyz;
    finalColor = colorLNormal;
  }
  else if (!all(equal(vnor.xyz, vec3(0)))) {
    if (vnor.w < 0.0) {
      finalColor = vec4(0.0);
      return;
    }
    nor = vnor.xyz;
    finalColor = colorVNormal;
  }
  else {
    nor = norAndFlag.xyz;
    if (all(equal(nor, vec3(0)))) {
      finalColor = vec4(0.0);
      return;
    }
    finalColor = colorNormal;
  }

  vec3 n = normalize(normal_object_to_world(nor));

  vec3 world_pos = point_object_to_world(pos);

  if (gl_VertexID == 0) {
    world_pos += n * normalSize;
  }

  gl_Position = point_world_to_ndc(world_pos);

  finalColor.a *= (test_occlusion()) ? alpha : 1.0;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
