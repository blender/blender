struct LightData {
  vec4 direction;
  vec4 specular_color;
  vec4 diffuse_color_wrap; /* rgb: diffuse col a: wrapped lighting factor */
};

struct WorldData {
  vec4 viewport_size;
  vec4 object_outline_color;
  vec4 shadow_direction_vs;
  float shadow_focus;
  float shadow_shift;
  float shadow_mul;
  float shadow_add;
  /* - 16 bytes alignment-  */
  LightData lights[4];
  vec4 ambient_color;

  int cavity_sample_start;
  int cavity_sample_end;
  float cavity_sample_count_inv;
  float cavity_jitter_scale;

  float cavity_valley_factor;
  float cavity_ridge_factor;
  float cavity_attenuation;
  float cavity_distance;

  float curvature_ridge;
  float curvature_valley;
  float ui_scale;
  float _pad0;

  int matcap_orientation;
  bool use_specular;
  int _pad1;
  int _pad2;
};

#define viewport_size_inv viewport_size.zw

layout(std140) uniform world_block
{
  WorldData world_data;
};
