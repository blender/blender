
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

RESOURCE_ID_VARYING

void main()
{
#ifdef VOLUME_SLICE
  if (sliceAxis == 0) {
    localPos = vec3(slicePosition * 2.0 - 1.0, pos.xy);
  }
  else if (sliceAxis == 1) {
    localPos = vec3(pos.x, slicePosition * 2.0 - 1.0, pos.y);
  }
  else {
    localPos = vec3(pos.xy, slicePosition * 2.0 - 1.0);
  }
  vec3 final_pos = localPos;
#else
  vec3 final_pos = pos;
#endif

#ifdef VOLUME_SMOKE
  final_pos = ((final_pos * 0.5 + 0.5) - OrcoTexCoFactors[0].xyz) / OrcoTexCoFactors[1].xyz;
#else
  final_pos = (volumeTextureToObject * vec4(final_pos * 0.5 + 0.5, 1.0)).xyz;
#endif
  gl_Position = point_object_to_ndc(final_pos);

  PASS_RESOURCE_ID
}
