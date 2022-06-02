
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
  fragColor = vec4(finalColor.rgb, finalColor.a * alpha);
}
