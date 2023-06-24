
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(select_lib.glsl)

void main()
{
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
  fragColor = vec4(finalColor.rgb, finalColor.a * alpha);
  select_id_output(select_id);
}
