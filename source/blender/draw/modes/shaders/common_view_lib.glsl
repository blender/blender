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

  /** Transform shortcuts. */
  /* Rule of thumb: Try to reuse world positions and normals because converting though viewspace
   * will always be decomposed in at least 2 matrix operation. */

  /**
   * Some clarification:
   * Usually Normal matrix is transpose(inverse(ViewMatrix * ModelMatrix))
   *
   * But since it is slow to multiply matrices we decompose it. Decomposing
   * inversion and transposition both invert the product order leaving us with
   * the same original order:
   * transpose(ViewMatrixInverse) * transpose(ModelMatrixInverse)
   *
   * Knowing that the view matrix is orthogonal, the transpose is also the inverse.
   * Note: This is only valid because we are only using the mat3 of the ViewMatrixInverse.
   * ViewMatrix * transpose(ModelMatrixInverse)
   **/
#define normal_object_to_view(nor) (mat3(ViewMatrix) * normal_object_to_world(nor))
#define normal_object_to_world(nor) (transpose(mat3(ModelMatrixInverse)) * nor)
#define normal_world_to_object(nor) (transpose(mat3(ModelMatrix)) * nor)
#define normal_world_to_view(nor) (mat3(ViewMatrix) * nor)

#define point_object_to_ndc(point) (ViewProjectionMatrix * (ModelMatrix * vec4(pt, 1.0)))
#define point_object_to_view(point) ((ViewMatrix * (ModelMatrix * vec4(pt, 1.0))).xyz)
#define point_object_to_world(point) ((ModelMatrix * vec4(point, 1.0)).xyz)
#define point_view_to_ndc(point) (ProjectionMatrix * vec4(point, 1.0))
#define point_view_to_object(point) ((ModelMatrixInverse * (ViewMatrixInverse * vec4(pt, 1.))).xyz)
#define point_view_to_world(point) ((ViewMatrixInverse * vec4(point, 1.0)).xyz)
#define point_world_to_ndc(point) (ViewProjectionMatrix * vec4(point, 1.0))
#define point_world_to_object(point) ((ModelMatrixInverse * vec4(point, 1.0)).xyz)
#define point_world_to_view(point) ((ViewMatrix * vec4(point, 1.0)).xyz)
