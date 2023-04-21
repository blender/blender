
void main()
{
  DebugSurfel surfel = surfels_buf[surfel_index];
  out_color = surfel.color;

  /* Display surfels as circles. */
  if (distance(P, surfel.position) > surfel_radius) {
    discard;
    return;
  }

  /* Display backfacing surfels with a transparent checkerboard grid. */
  if (!gl_FrontFacing) {
    ivec2 grid_uv = ivec2(gl_FragCoord.xy) / 5;
    if ((grid_uv.x + grid_uv.y) % 2 == 0) {
      discard;
      return;
    }
  }
}
