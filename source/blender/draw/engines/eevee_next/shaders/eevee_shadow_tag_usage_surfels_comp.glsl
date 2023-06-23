
/**
 * Virtual shadowmapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This pass iterates the surfels buffer and tag all tiles that are needed for light shadowing as
 * needed.
 */

#pragma BLENDER_REQUIRE(eevee_shadow_tag_usage_lib.glsl)

void main()
{
  int index = int(gl_GlobalInvocationID.x);
  if (index >= capture_info_buf.surfel_len) {
    return;
  }

  Surfel surfel = surfel_buf[index];
  shadow_tag_usage_surfel(surfel, directional_level);
}
