
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)

void main()
{
  normalData = workbench_normal_encode(gl_FrontFacing, normal_interp);

  materialData = vec4(color_interp, workbench_float_pair_encode(roughness, metallic));

  objectId = uint(object_id);

  if (useMatcap) {
    /* For matcaps, save front facing in alpha channel. */
    materialData.a = float(gl_FrontFacing);
  }

#ifdef V3D_SHADING_TEXTURE_COLOR
  materialData.rgb = workbench_image_color(uv_interp);
#endif
}
