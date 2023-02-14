
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  if (isCameraBackground) {
    /* Model matrix converts to view position to avoid jittering (see #91398). */
    gl_Position = point_view_to_ndc(world_pos);
    /* Camera background images are not really part of the 3D space.
     * It makes no sense to apply clipping on them. */
    view_clipping_distances_bypass();
  }
  else {
    gl_Position = point_world_to_ndc(world_pos);
    view_clipping_distances(world_pos);
  }

  if (depthSet) {
    /* Result in a position at 1.0 (far plane). Small epsilon to avoid precision issue.
     * This mimics the effect of infinite projection matrix
     * (see http://www.terathon.com/gdc07_lengyel.pdf). */
    gl_Position.z = gl_Position.w - 2.4e-7;
    view_clipping_distances_bypass();
  }

  uvs = pos.xy * 0.5 + 0.5;
}
