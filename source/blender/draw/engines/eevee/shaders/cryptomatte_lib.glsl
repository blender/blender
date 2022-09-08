/* NOTE: this lib is included in the cryptomatte vertex shader to work around the issue that eevee
 * cannot use create infos for its static shaders. Keep in sync with draw_shader_shared.h */
#ifdef HAIR_SHADER
/* Define the maximum number of attribute we allow in a curves UBO.
 * This should be kept in sync with `GPU_ATTR_MAX` */
#  define DRW_ATTRIBUTE_PER_CURVES_MAX 15

struct CurvesInfos {
  /* Per attribute scope, follows loading order.
   * NOTE: uint as bool in GLSL is 4 bytes.
   * NOTE: GLSL pad arrays of scalar to 16 bytes (std140). */
  uvec4 is_point_attribute[DRW_ATTRIBUTE_PER_CURVES_MAX];
};
layout(std140) uniform drw_curves
{
  CurvesInfos _drw_curves;
};
#  define drw_curves (_drw_curves)
#endif
