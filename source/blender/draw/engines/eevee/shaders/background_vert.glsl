
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(surface_lib.glsl)

in vec2 pos;

RESOURCE_ID_VARYING

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  PASS_RESOURCE_ID

  gl_Position = vec4(pos, 1.0, 1.0);
  viewPosition = vec3(pos, -1.0);

#ifndef VOLUMETRICS
  /* Not used in practice but needed to avoid compilation errors. */
  worldPosition = viewPosition;
  worldNormal = viewNormal = normalize(-viewPosition);
#endif

#ifdef USE_ATTR
  pass_attr(viewPosition, NormalMatrix, ModelMatrixInverse);
#endif
}
