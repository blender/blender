
/**
 * Virtual shadowmapping: Page Clear.
 *
 * Equivalent to a framebuffer depth clear but only for pages pushed to the clear_page_buf.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

void main()
{
  uvec2 page_co = unpackUvec2x16(clear_page_buf[gl_GlobalInvocationID.z]);
  uvec2 page_texel = page_co * pages_infos_buf.page_size + gl_GlobalInvocationID.xy;

  /* Clear to FLT_MAX instead of 1 so the far plane doesn't cast shadows onto farther objects. */
  imageStore(atlas_img, ivec2(page_texel), uvec4(floatBitsToUint(FLT_MAX)));
}
