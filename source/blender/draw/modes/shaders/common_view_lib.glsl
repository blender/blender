/* keep in sync with DRWManager.view_data */
layout(std140) uniform viewBlock
{
  /* Same order as DRWViewportMatrixType */
  mat4 ViewProjectionMatrix;
  mat4 ViewProjectionMatrixInverse;
  mat4 ViewMatrix;
  mat4 ViewMatrixInverse;
  mat4 ProjectionMatrix;
  mat4 ProjectionMatrixInverse;

  vec4 CameraTexCoFactors;

  vec4 clipPlanes[2];
};

/* Transform shortcuts. */
#define transform_normal_object_to_world(nor) (transpose(mat3(ModelMatrixInverse)) * nor)
#define transform_normal_world_to_object(nor) (transpose(mat3(ModelMatrix)) * nor)
#define transform_point_view_to_object(point) \
  ((ModelMatrixInverse * (ViewMatrixInverse * vec4(point, 1.0))).xyz)
