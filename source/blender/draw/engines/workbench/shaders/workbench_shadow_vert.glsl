
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vData.pos = pos;
  vData.frontPosition = point_object_to_ndc(pos);
  vData.backPosition = point_object_to_ndc(pos + lightDirection * lightDistance);
}
