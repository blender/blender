/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** This describe the entire interface of the shader. */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/* Global interface for SSR.
 * SSR will set these global variables itself.
 * Also make false positive compiler warnings disappear by setting values. */
#define SSR_INTERFACE \
  vec3 worldPosition = vec3(0); \
  vec3 viewPosition = vec3(0); \
  vec3 worldNormal = vec3(0); \
  vec3 viewNormal = vec3(0);

/* Skip interface declaration when using create-info. */
#ifndef USE_GPU_SHADER_CREATE_INFO

#  define SURFACE_INTERFACE \
    vec3 worldPosition; \
    vec3 viewPosition; \
    vec3 worldNormal; \
    vec3 viewNormal;

#  ifndef IN_OUT
#    if defined(GPU_VERTEX_SHADER)
#      define IN_OUT out
#    elif defined(GPU_FRAGMENT_SHADER)
#      define IN_OUT in
#    endif
#  endif

#  ifndef EEVEE_GENERATED_INTERFACE
#    if defined(STEP_RESOLVE) || defined(STEP_RAYTRACE)

SSR_INTERFACE

#    elif defined(GPU_GEOMETRY_SHADER)
in ShaderStageInterface{SURFACE_INTERFACE} dataIn[];

out ShaderStageInterface{SURFACE_INTERFACE} dataOut;

#      define PASS_SURFACE_INTERFACE(vert) \
        dataOut.worldPosition = dataIn[vert].worldPosition; \
        dataOut.viewPosition = dataIn[vert].viewPosition; \
        dataOut.worldNormal = dataIn[vert].worldNormal; \
        dataOut.viewNormal = dataIn[vert].viewNormal;

#    else /* GPU_VERTEX_SHADER || GPU_FRAGMENT_SHADER */

IN_OUT ShaderStageInterface{SURFACE_INTERFACE};

#    endif
#  endif /* EEVEE_GENERATED_INTERFACE */

#  ifdef HAIR_SHADER
IN_OUT ShaderHairInterface
{
  /* world space */
  vec3 hairTangent;
  float hairThickTime;
  float hairThickness;
  float hairTime;
  flat int hairStrandID;
  vec2 hairBary;
};
#  endif

#  ifdef POINTCLOUD_SHADER
IN_OUT ShaderPointCloudInterface
{
  /* world space */
  float pointRadius;
  float pointPosition;
  flat int pointID;
};
#  endif

#else
/* Checks to ensure create-info is setup correctly. */
#  ifdef HAIR_SHADER
#    ifndef USE_SURFACE_LIB_HAIR
#      error Ensure CreateInfo eevee_legacy_surface_lib_hair is included if using surface library with a hair shader.
#    endif
#  endif

#  ifdef POINTCLOUD_SHADER
#    ifndef USE_SURFACE_LIB_POINTCLOUD
#      error Ensure CreateInfo eevee_legacy_surface_lib_pointcloud is included if using surface library with a hair shader.
#    endif
#  endif

/* SSR Global Interface. */
#  if defined(STEP_RESOLVE) || defined(STEP_RAYTRACE)
SSR_INTERFACE
#  endif

#endif /* USE_GPU_SHADER_CREATE_INFO */

#if defined(GPU_FRAGMENT_SHADER) && defined(CODEGEN_LIB)

#  if defined(USE_BARYCENTRICS) && !defined(HAIR_SHADER)
vec3 barycentric_distances_get()
{
#    if defined(GPU_OPENGL)
  /* NOTE: No need to undo perspective divide since it is not applied yet. */
  vec3 pos0 = (ProjectionMatrixInverse * gpu_position_at_vertex(0)).xyz;
  vec3 pos1 = (ProjectionMatrixInverse * gpu_position_at_vertex(1)).xyz;
  vec3 pos2 = (ProjectionMatrixInverse * gpu_position_at_vertex(2)).xyz;
  vec3 edge21 = pos2 - pos1;
  vec3 edge10 = pos1 - pos0;
  vec3 edge02 = pos0 - pos2;
  vec3 d21 = safe_normalize(edge21);
  vec3 d10 = safe_normalize(edge10);
  vec3 d02 = safe_normalize(edge02);
  vec3 dists;
  float d = dot(d21, edge02);
  dists.x = sqrt(dot(edge02, edge02) - d * d);
  d = dot(d02, edge10);
  dists.y = sqrt(dot(edge10, edge10) - d * d);
  d = dot(d10, edge21);
  dists.z = sqrt(dot(edge21, edge21) - d * d);
  return dists.xyz;
#    elif defined(GPU_METAL)
  /* Calculate Barycentric distances from available parameters in Metal. */
  float3 wp_delta = (length(dfdx(worldPosition.xyz)) + length(dfdy(worldPosition.xyz)));
  float3 bc_delta = (length(dfdx(gpu_BaryCoord)) + length(dfdy(gpu_BaryCoord)));
  float3 rate_of_change = wp_delta / bc_delta;
  vec3 dists;
  dists.x = length(rate_of_change * (1.0 - gpu_BaryCoord.x));
  dists.y = length(rate_of_change * (1.0 - gpu_BaryCoord.y));
  dists.z = length(rate_of_change * (1.0 - gpu_BaryCoord.z));
  return dists.xyz;
#    endif
}
#  endif

GlobalData init_globals(void)
{
  GlobalData surf;

#  if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  surf.P = transform_direction(ViewMatrixInverse, -viewCameraVec(viewPosition));
  surf.N = surf.Ng = surf.Ni = -surf.P;
  surf.ray_length = 0.0;
#  else
  surf.P = worldPosition;
  surf.Ni = worldNormal;
  surf.N = safe_normalize(worldNormal);
  surf.Ng = safe_normalize(cross(dFdx(surf.P), dFdy(surf.P)));
  surf.ray_length = distance(surf.P, cameraPos);
#  endif
  surf.barycentric_coords = vec2(0.0);
  surf.barycentric_dists = vec3(0.0);
  surf.N = (FrontFacing) ? surf.N : -surf.N;
  surf.Ni = (FrontFacing) ? surf.Ni : -surf.Ni;
#  ifdef HAIR_SHADER
  vec3 V = cameraVec(surf.P);
  /* Shade as a cylinder. */
  vec3 B = normalize(cross(worldNormal, hairTangent));
  float cos_theta;
  if (hairThicknessRes == 1) {
    /* Random cosine normal distribution on the hair surface. */
    cos_theta = texelfetch_noise_tex(gl_FragCoord.xy).x * 2.0 - 1.0;
  }
  else {
    /* Shade as a cylinder. */
    cos_theta = hairThickTime / hairThickness;
  }
  float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
  surf.N = surf.Ni = safe_normalize(worldNormal * sin_theta + B * cos_theta);
  surf.curve_T = -hairTangent;
  /* Costly, but follows cycles per pixel tangent space (not following curve shape). */
  surf.curve_B = cross(V, surf.curve_T);
  surf.curve_N = safe_normalize(cross(surf.curve_T, surf.curve_B));
  surf.is_strand = true;
  surf.hair_time = hairTime;
  surf.hair_thickness = hairThickness;
  surf.hair_strand_id = hairStrandID;
#    ifdef USE_BARYCENTRICS
  surf.barycentric_coords = hair_resolve_barycentric(hairBary);
#    endif
#  else
  surf.curve_T = surf.curve_B = surf.curve_N = vec3(0.0);
  surf.is_strand = false;
  surf.hair_time = 0.0;
  surf.hair_thickness = 0.0;
  surf.hair_strand_id = 0;
#    ifdef USE_BARYCENTRICS
  surf.barycentric_coords = gpu_BaryCoord.xy;
  surf.barycentric_dists = barycentric_distances_get();
#    endif
#  endif
  surf.ray_type = rayType;
  surf.ray_depth = 0.0;
  return surf;
}
#endif

vec3 coordinate_camera(vec3 P)
{
  vec3 vP;
#if defined(PROBE_CAPTURE)
  /* Unsupported. It would make the probe camera-dependent. */
  vP = P;
#elif defined(WORLD_BACKGROUND)
  vP = transform_direction(ViewMatrix, P);
#else
  vP = transform_point(ViewMatrix, P);
#endif
  vP.z = -vP.z;
  return vP;
}

vec3 coordinate_screen(vec3 P)
{
  vec3 window = vec3(0.0);
#if defined(PROBE_CAPTURE)
  /* Unsupported. It would make the probe camera-dependent. */
  window.xy = vec2(0.5);

#elif defined(WORLD_BACKGROUND) && defined(COMMON_UNIFORMS_LIB)
  window.xy = project_point(ProjectionMatrix, viewPosition).xy * 0.5 + 0.5;
  window.xy = window.xy * cameraUvScaleBias.xy + cameraUvScaleBias.zw;

#elif defined(COMMON_UNIFORMS_LIB) /* MESH */
  window.xy = project_point(ProjectionMatrix, transform_point(ViewMatrix, P)).xy * 0.5 + 0.5;
  window.xy = window.xy * cameraUvScaleBias.xy + cameraUvScaleBias.zw;
#endif
  return window;
}

vec3 coordinate_reflect(vec3 P, vec3 N)
{
#if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  return N;
#else
  return -reflect(cameraVec(P), N);
#endif
}

vec3 coordinate_incoming(vec3 P)
{
#if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  return -P;
#else
  return cameraVec(P);
#endif
}
