#pragma BLENDER_REQUIRE(select_lib.glsl)

void main()
{
  /* No color output, only depth (line below is implicit). */
  // gl_FragDepth = gl_FragCoord.z;

  /* This is optimized to noop in the non select case. */
  select_id_output(select_id);
}
