
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vData.pos = pos;
  vData.frontPosition = point_object_to_ndc(pos);
  vec3 back_pos = pos + lightDirection * lightDistance;
  vData.backPosition = point_object_to_ndc(back_pos);
}
