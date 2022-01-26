/* Temporary until we fully make the switch. */
#ifndef DRW_SHADER_SHARED_H

#  define DRW_RESOURCE_CHUNK_LEN 512

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

  /* View frustum corners [NDC(-1.0, -1.0, -1.0) & NDC(1.0, 1.0, 1.0)].
   * Fourth components are near and far values. */
  vec4 ViewVecs[2];

  /* TODO: move it elsewhere. */
  vec4 CameraTexCoFactors;
};

#endif /* DRW_SHADER_SHARED_H */

#define ViewNear (ViewVecs[0].w)
#define ViewFar (ViewVecs[1].w)

#define cameraForward ViewMatrixInverse[2].xyz
#define cameraPos ViewMatrixInverse[3].xyz
#define cameraVec(P) ((ProjectionMatrix[3][3] == 0.0) ? normalize(cameraPos - P) : cameraForward)
#define viewCameraVec(vP) ((ProjectionMatrix[3][3] == 0.0) ? normalize(-vP) : vec3(0.0, 0.0, 1.0))

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
 * TODO: Split to an overlay lib. */
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
 * TODO: Split to an overlay lib. */
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

/* Temporary until we fully make the switch. */
#ifndef DRW_SHADER_SHARED_H
uniform int drw_resourceChunk;
#endif /* DRW_SHADER_SHARED_H */

#ifdef GPU_VERTEX_SHADER

/* Temporary until we fully make the switch. */
#  ifndef DRW_SHADER_SHARED_H

/* clang-format off */
#    if defined(IN_PLACE_INSTANCES) || defined(INSTANCED_ATTR) || defined(DRW_LEGACY_MODEL_MATRIX) || defined(GPU_DEPRECATED_AMD_DRIVER)
/* clang-format on */
/* When drawing instances of an object at the same position. */
#      define instanceId 0
#    else
#      define instanceId gl_InstanceID
#    endif

#    if defined(UNIFORM_RESOURCE_ID)
/* This is in the case we want to do a special instance drawcall for one object but still want to
 * have the right resourceId and all the correct ubo datas. */
uniform int drw_resourceId;
#      define resource_id drw_resourceId
#    else
#      define resource_id (gpu_BaseInstance + instanceId)
#    endif

/* Use this to declare and pass the value if
 * the fragment shader uses the resource_id. */
#    ifdef USE_GEOMETRY_SHADER
#      define RESOURCE_ID_VARYING flat out int resourceIDGeom;
#      define PASS_RESOURCE_ID resourceIDGeom = resource_id;
#    else
#      define RESOURCE_ID_VARYING flat out int resourceIDFrag;
#      define PASS_RESOURCE_ID resourceIDFrag = resource_id;
#    endif

#  endif /* DRW_SHADER_SHARED_H */

#endif /* GPU_VERTEX_SHADER */

/* Temporary until we fully make the switch. */
#ifdef DRW_SHADER_SHARED_H
/* TODO(fclem): Rename PASS_RESOURCE_ID to DRW_RESOURCE_ID_VARYING_SET */
#  if defined(UNIFORM_RESOURCE_ID)
#    define resource_id drw_ResourceID
#    define PASS_RESOURCE_ID

#  elif defined(GPU_VERTEX_SHADER)
#    define resource_id gpu_InstanceIndex
#    define PASS_RESOURCE_ID drw_ResourceID_iface.resource_index = resource_id;

#  elif defined(GPU_GEOMETRY_SHADER)
#    define resource_id drw_ResourceID_iface_in[0].index
#    define PASS_RESOURCE_ID drw_ResourceID_iface_out.resource_index = resource_id;

#  elif defined(GPU_FRAGMENT_SHADER)
#    define resource_id drw_ResourceID_iface.resource_index
#  endif

/* TODO(fclem): Remove. */
#  define RESOURCE_ID_VARYING

#else
/* If used in a fragment / geometry shader, we pass
 * resource_id as varying. */
#  ifdef GPU_GEOMETRY_SHADER
#    define RESOURCE_ID_VARYING \
      flat out int resourceIDFrag; \
      flat in int resourceIDGeom[];

#    define resource_id resourceIDGeom
#    define PASS_RESOURCE_ID resourceIDFrag = resource_id[0];
#  endif

#  ifdef GPU_FRAGMENT_SHADER
flat in int resourceIDFrag;
#    define resource_id resourceIDFrag
#  endif
#endif

/* Breaking this across multiple lines causes issues for some older GLSL compilers. */
/* clang-format off */
#if !defined(GPU_INTEL) && !defined(GPU_DEPRECATED_AMD_DRIVER) && !defined(OS_MAC) && !defined(INSTANCED_ATTR) && !defined(DRW_LEGACY_MODEL_MATRIX)
/* clang-format on */

/* Temporary until we fully make the switch. */
#  ifndef DRW_SHADER_SHARED_H

struct ObjectMatrices {
  mat4 drw_modelMatrix;
  mat4 drw_modelMatrixInverse;
};

layout(std140) uniform modelBlock
{
  ObjectMatrices drw_matrices[DRW_RESOURCE_CHUNK_LEN];
};

#    define ModelMatrix (drw_matrices[resource_id].drw_modelMatrix)
#    define ModelMatrixInverse (drw_matrices[resource_id].drw_modelMatrixInverse)
#  endif /* DRW_SHADER_SHARED_H */

#else /* GPU_INTEL */

/* Temporary until we fully make the switch. */
#  ifndef DRW_SHADER_SHARED_H
/* Intel GPU seems to suffer performance impact when the model matrix is in UBO storage.
 * So for now we just force using the legacy path. */
/* Note that this is also a workaround of a problem on osx (amd or nvidia)
 * and older amd driver on windows. */
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;
#  endif /* DRW_SHADER_SHARED_H */

#endif

/* Temporary until we fully make the switch. */
#ifndef DRW_SHADER_SHARED_H
#  define resource_handle (drw_resourceChunk * DRW_RESOURCE_CHUNK_LEN + resource_id)
#endif

/** Transform shortcuts. */
/* Rule of thumb: Try to reuse world positions and normals because converting through viewspace
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
 * NOTE: This is only valid because we are only using the mat3 of the ViewMatrixInverse.
 * ViewMatrix * transpose(ModelMatrixInverse)
 */
#define NormalMatrix transpose(mat3(ModelMatrixInverse))
#define NormalMatrixInverse transpose(mat3(ModelMatrix))

#define normal_object_to_view(n) (mat3(ViewMatrix) * (NormalMatrix * n))
#define normal_object_to_world(n) (NormalMatrix * n)
#define normal_world_to_object(n) (NormalMatrixInverse * n)
#define normal_world_to_view(n) (mat3(ViewMatrix) * n)
#define normal_view_to_world(n) (mat3(ViewMatrixInverse) * n)

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

/* ---- Opengl Depth conversion ---- */

float linear_depth(bool is_persp, float z, float zf, float zn)
{
  if (is_persp) {
    return (zn * zf) / (z * (zn - zf) + zf);
  }
  else {
    return (z * 2.0 - 1.0) * zf;
  }
}

float buffer_depth(bool is_persp, float z, float zf, float zn)
{
  if (is_persp) {
    return (zf * (zn - z)) / (z * (zn - zf));
  }
  else {
    return (z / (zf * 2.0)) + 0.5;
  }
}

float get_view_z_from_depth(float depth)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    float d = 2.0 * depth - 1.0;
    return -ProjectionMatrix[3][2] / (d + ProjectionMatrix[2][2]);
  }
  else {
    return ViewVecs[0].z + depth * ViewVecs[1].z;
  }
}

float get_depth_from_view_z(float z)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    float d = (-ProjectionMatrix[3][2] / z) - ProjectionMatrix[2][2];
    return d * 0.5 + 0.5;
  }
  else {
    return (z - ViewVecs[0].z) / ViewVecs[1].z;
  }
}

vec2 get_uvs_from_view(vec3 view)
{
  vec4 ndc = ProjectionMatrix * vec4(view, 1.0);
  return (ndc.xy / ndc.w) * 0.5 + 0.5;
}

vec3 get_view_space_from_depth(vec2 uvcoords, float depth)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    return vec3(ViewVecs[0].xy + uvcoords * ViewVecs[1].xy, 1.0) * get_view_z_from_depth(depth);
  }
  else {
    return ViewVecs[0].xyz + vec3(uvcoords, depth) * ViewVecs[1].xyz;
  }
}

vec3 get_world_space_from_depth(vec2 uvcoords, float depth)
{
  return (ViewMatrixInverse * vec4(get_view_space_from_depth(uvcoords, depth), 1.0)).xyz;
}

vec3 get_view_vector_from_screen_uv(vec2 uv)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    return normalize(vec3(ViewVecs[0].xy + uv * ViewVecs[1].xy, 1.0));
  }
  else {
    return vec3(0.0, 0.0, 1.0);
  }
}
