#define COMMON_VIEW_LIB
#define DRW_RESOURCE_CHUNK_LEN 512

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

  vec4 clipPlanes[6];

  /* TODO move it elsewhere. */
  vec4 CameraTexCoFactors;
};

#ifdef world_clip_planes_calc_clip_distance
#  undef world_clip_planes_calc_clip_distance
#  define world_clip_planes_calc_clip_distance(p) \
    _world_clip_planes_calc_clip_distance(p, clipPlanes)
#endif

uniform int resourceChunk;

#ifdef GPU_VERTEX_SHADER
#  ifdef GL_ARB_shader_draw_parameters
#    define baseInstance gl_BaseInstanceARB
#  else /* no ARB_shader_draw_parameters */
uniform int baseInstance;
#  endif

#  ifdef IN_PLACE_INSTANCES
/* When drawing instances of an object at the same position. */
#    define instanceId 0
#  elif defined(GPU_CRAPPY_AMD_DRIVER)
/* NOTE: This does contain the baseInstance ofset */
in int _instanceId;
#    define instanceId (_instanceId - baseInstance)
#  else
#    define instanceId gl_InstanceID
#  endif

#  define resource_id (baseInstance + instanceId)

/* Use this to declare and pass the value if
 * the fragment shader uses the resource_id. */
#  define RESOURCE_ID_VARYING flat out int resourceIDFrag;
#  define RESOURCE_ID_VARYING_GEOM flat out int resourceIDGeom;
#  define PASS_RESOURCE_ID resourceIDFrag = resource_id;
#  define PASS_RESOURCE_ID_GEOM resourceIDGeom = resource_id;
#endif

/* If used in a fragment / geometry shader, we pass
 * resource_id as varying. */
#ifdef GPU_GEOMETRY_SHADER
#  define RESOURCE_ID_VARYING \
    flat out int resourceIDFrag; \
    flat in int resourceIDGeom[];

#  define resource_id resourceIDGeom
#  define PASS_RESOURCE_ID(i) resourceIDFrag = resource_id[i];
#endif

#ifdef GPU_FRAGMENT_SHADER
flat in int resourceIDFrag;
#  define resource_id resourceIDFrag
#endif

#ifndef GPU_INTEL
struct ObjectMatrices {
  mat4 drw_modelMatrix;
  mat4 drw_modelMatrixInverse;
};

layout(std140) uniform modelBlock
{
  ObjectMatrices drw_matrices[DRW_RESOURCE_CHUNK_LEN];
};

#  define ModelMatrix (drw_matrices[resource_id].drw_modelMatrix)
#  define ModelMatrixInverse (drw_matrices[resource_id].drw_modelMatrixInverse)

#else /* GPU_INTEL */
/* Intel GPU seems to suffer performance impact when the model matrix is in UBO storage.
 * So for now we just force using the legacy path. */
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;
#endif

#define resource_handle (resourceChunk * DRW_RESOURCE_CHUNK_LEN + resource_id)

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
