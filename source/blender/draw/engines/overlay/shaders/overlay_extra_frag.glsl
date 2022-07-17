
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  fragColor = finalColor;
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
}
