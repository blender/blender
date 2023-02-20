
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec2 uv = gl_PointCoord - vec2(0.5);
  float dist = length(uv);

  if (dist > 0.5) {
    discard;
    return;
  }
  /* Nice sphere falloff. */
  float intensity = sqrt(1.0 - dist * 2.0) * 0.5 + 0.5;
  fragColor = finalColor * vec4(intensity, intensity, intensity, 1.0);

  /* The default value of GL_POINT_SPRITE_COORD_ORIGIN is GL_UPPER_LEFT. Need to reverse the Y. */
  uv.y = -uv.y;
  /* Subtract distance to outer edge of the circle. (0.75 is manually tweaked to look better) */
  vec2 edge_pos = gl_FragCoord.xy - uv * (0.75 / (dist + 1e-9));
  vec2 edge_start = edge_pos + vec2(-uv.y, uv.x);

  lineOutput = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);
}
