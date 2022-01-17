
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_shader_interface_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)

#ifndef WORKBENCH_SHADER_SHARED_H
layout(location = 0) out vec4 materialData;
layout(location = 1) out vec2 normalData;
layout(location = 2) out uint objectId;
#endif

uniform bool useMatcap = false;

void main()
{
  normalData = workbench_normal_encode(gl_FrontFacing, normal_interp);

  materialData = vec4(color_interp, packed_rough_metal);

  objectId = uint(object_id);

  if (useMatcap) {
    /* For matcaps, save front facing in alpha channel. */
    materialData.a = float(gl_FrontFacing);
  }

#ifdef V3D_SHADING_TEXTURE_COLOR
  materialData.rgb = workbench_image_color(uv_interp);
#endif
}
