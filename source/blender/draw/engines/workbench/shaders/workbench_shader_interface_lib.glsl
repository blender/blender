
#ifndef WORKBENCH_SHADER_SHARED_H
IN_OUT ShaderStageInterface
{
  vec3 normal_interp;
  vec3 color_interp;
  float alpha_interp;
  vec2 uv_interp;
#  ifdef TRANSPARENT_MATERIAL
  flat float roughness;
  flat float metallic;
#  else
  flat float packed_rough_metal;
#  endif
  flat int object_id;
};
#endif
