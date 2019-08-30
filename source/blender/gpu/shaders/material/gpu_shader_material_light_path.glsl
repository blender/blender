void node_light_path(out float is_camera_ray,
                     out float is_shadow_ray,
                     out float is_diffuse_ray,
                     out float is_glossy_ray,
                     out float is_singular_ray,
                     out float is_reflection_ray,
                     out float is_transmission_ray,
                     out float ray_length,
                     out float ray_depth,
                     out float diffuse_depth,
                     out float glossy_depth,
                     out float transparent_depth,
                     out float transmission_depth)
{
  /* Supported. */
  is_camera_ray = (rayType == EEVEE_RAY_CAMERA) ? 1.0 : 0.0;
  is_shadow_ray = (rayType == EEVEE_RAY_SHADOW) ? 1.0 : 0.0;
  is_diffuse_ray = (rayType == EEVEE_RAY_DIFFUSE) ? 1.0 : 0.0;
  is_glossy_ray = (rayType == EEVEE_RAY_GLOSSY) ? 1.0 : 0.0;
  /* Kind of supported. */
  is_singular_ray = is_glossy_ray;
  is_reflection_ray = is_glossy_ray;
  is_transmission_ray = is_glossy_ray;
  ray_depth = rayDepth;
  diffuse_depth = (is_diffuse_ray == 1.0) ? rayDepth : 0.0;
  glossy_depth = (is_glossy_ray == 1.0) ? rayDepth : 0.0;
  transmission_depth = (is_transmission_ray == 1.0) ? glossy_depth : 0.0;
  /* Not supported. */
  ray_length = 1.0;
  transparent_depth = 0.0;
}
