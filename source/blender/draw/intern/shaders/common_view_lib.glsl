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

#ifdef COMMON_GLOBALS_LIB
float mul_project_m4_v3_zfac(in vec3 co)
{
  return pixelFac * ((ViewProjectionMatrix[0][3] * co.x) + (ViewProjectionMatrix[1][3] * co.y) +
                     (ViewProjectionMatrix[2][3] * co.z) + ViewProjectionMatrix[3][3]);
}
#endif

/* Not the right place but need to be common to all overlay's.
 * TODO Split to an overlay lib. */
mat4 extract_matrix_packed_data(mat4 mat, out vec4 dataA, out vec4 dataB)
{
  const float div = 1.0 / 255.0;
  int a = int(mat[0][3]);
  int b = int(mat[1][3]);
  int c = int(mat[2][3]);
  int d = int(mat[3][3]);
  dataA = vec4(a & 0xFF, a >> 8, b & 0xFF, b >> 8) * div;
  dataB = vec4(c & 0xFF, c >> 8, d & 0xFF, d >> 8) * div;
  mat[0][3] = mat[1][3] = mat[2][3] = 0.0;
  mat[3][3] = 1.0;
  return mat;
}

/* Same here, Not the right place but need to be common to all overlay's.
 * TODO Split to an overlay lib. */
/* edge_start and edge_pos needs to be in the range [0..sizeViewport]. */
vec4 pack_line_data(vec2 frag_co, vec2 edge_start, vec2 edge_pos)
{
  vec2 edge = edge_start - edge_pos;
  float len = length(edge);
  if (len > 0.0) {
    edge /= len;
    vec2 perp = vec2(-edge.y, edge.x);
    float dist = dot(perp, frag_co - edge_start);
    /* Add 0.1 to diffenrentiate with cleared pixels. */
    return vec4(perp * 0.5 + 0.5, dist * 0.25 + 0.5 + 0.1, 1.0);
  }
  else {
    /* Default line if the origin is perfectly aligned with a pixel. */
    return vec4(1.0, 0.0, 0.5 + 0.1, 1.0);
  }
}

uniform int resourceChunk;

#ifdef GPU_VERTEX_SHADER
#  ifdef GL_ARB_shader_draw_parameters
#    define baseInstance gl_BaseInstanceARB
#  else /* no ARB_shader_draw_parameters */
uniform int baseInstance;
#  endif

#  if defined(IN_PLACE_INSTANCES) || defined(INSTANCED_ATTRIB)
/* When drawing instances of an object at the same position. */
#    define instanceId 0
#  elif defined(GPU_DEPRECATED_AMD_DRIVER)
/* A driver bug make it so that when using an attribute with GL_INT_2_10_10_10_REV as format,
 * the gl_InstanceID is incremented by the 2 bit component of the attrib.
 * Ignore gl_InstanceID then. */
#    define instanceId 0
#  else
#    define instanceId gl_InstanceID
#  endif

#  ifdef UNIFORM_RESOURCE_ID
/* This is in the case we want to do a special instance drawcall but still want to have the
 * right resourceId and all the correct ubo datas. */
uniform int resourceId;
#    define resource_id resourceId
#  else
#    define resource_id (baseInstance + instanceId)
#  endif

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

/* Breaking this across multiple lines causes issues for some older GLSL compilers. */
/* clang-format off */
#if !defined(GPU_INTEL) && !defined(GPU_DEPRECATED_AMD_DRIVER) && !defined(OS_MAC) && !defined(INSTANCED_ATTRIB)
/* clang-format on */
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
/* Note that this is also a workaround of a problem on osx (amd or nvidia)
 * and older amd driver on windows. */
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

#define DRW_BASE_SELECTED (1 << 1)
#define DRW_BASE_FROM_DUPLI (1 << 2)
#define DRW_BASE_FROM_SET (1 << 3)
#define DRW_BASE_ACTIVE (1 << 4)
