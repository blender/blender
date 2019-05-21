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

uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;

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
#define normal_object_to_view(n) (mat3(ViewMatrix) * (transpose(mat3(ModelMatrixInverse)) * n))
#define normal_object_to_world(n) (transpose(mat3(ModelMatrixInverse)) * n)
#define normal_world_to_object(n) (transpose(mat3(ModelMatrix)) * n)
#define normal_world_to_view(n) (mat3(ViewMatrix) * n)

#define point_object_to_ndc(p) (ViewProjectionMatrix * vec4((ModelMatrix * vec4(p, 1.0)).xyz, 1.0))
#define point_object_to_view(p) ((ViewMatrix * vec4((ModelMatrix * vec4(p, 1.0)).xyz, 1.0)).xyz)
#define point_object_to_world(p) ((ModelMatrix * vec4(p, 1.0)).xyz)
#define point_view_to_ndc(p) (ProjectionMatrix * vec4(p, 1.0))
#define point_view_to_object(p) ((ModelMatrixInverse * (ViewMatrixInverse * vec4(p, 1.0))).xyz)
#define point_view_to_world(p) ((ViewMatrixInverse * vec4(p, 1.0)).xyz)
#define point_world_to_ndc(p) (ViewProjectionMatrix * vec4(p, 1.0))
#define point_world_to_object(p) ((ModelMatrixInverse * vec4(p, 1.0)).xyz)
#define point_world_to_view(p) ((ViewMatrix * vec4(p, 1.0)).xyz)

/* Due to some shader compiler bug, we somewhat need to access gl_VertexID
 * to make vertex shaders work. even if it's actually dead code. */
#ifdef GPU_INTEL
#  define GPU_INTEL_VERTEX_SHADER_WORKAROUND gl_Position.x = float(gl_VertexID);
#else
#  define GPU_INTEL_VERTEX_SHADER_WORKAROUND
#endif
