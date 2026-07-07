/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* Kernel Feature Flags
 *
 * These are set from the host side depending on which features are used by the scene,
 * either for runtime checks or specialization.
 *
 * NOTE: Keep kernel features as defines as they are used by the preprocessor to do
 * compile time optimization while using adaptive kernel compilation. */

/* Shader nodes. */
#define KERNEL_FEATURE_NODE_BSDF (1U << 0U)
#define KERNEL_FEATURE_NODE_EMISSION (1U << 1U)
#define KERNEL_FEATURE_NODE_VOLUME (1U << 2U)
#define KERNEL_FEATURE_NODE_BUMP (1U << 3U)
#define KERNEL_FEATURE_NODE_BUMP_STATE (1U << 4U)
#define KERNEL_FEATURE_NODE_VORONOI_EXTRA (1U << 5U)
#define KERNEL_FEATURE_NODE_RAYTRACE (1U << 6U)
#define KERNEL_FEATURE_NODE_AOV (1U << 7U)
#define KERNEL_FEATURE_NODE_LIGHT_PATH (1U << 8U)
#define KERNEL_FEATURE_NODE_PRINCIPLED_HAIR (1U << 9U)
#define KERNEL_FEATURE_NODE_PORTAL (1U << 10U)

/* Use path tracing kernels. */
#define KERNEL_FEATURE_PATH_TRACING (1U << 11U)

/* BVH/sampling kernel features. */
#define KERNEL_FEATURE_POINTCLOUD (1U << 12U)
#define KERNEL_FEATURE_HAIR_RIBBON (1U << 13U)
#define KERNEL_FEATURE_HAIR_THICK (1U << 14U)
#define KERNEL_FEATURE_HAIR (KERNEL_FEATURE_HAIR_RIBBON | KERNEL_FEATURE_HAIR_THICK)
#define KERNEL_FEATURE_OBJECT_MOTION (1U << 15U)

/* Denotes whether baking functionality is needed. */
#define KERNEL_FEATURE_BAKING (1U << 16U)

/* Use subsurface scattering materials. */
#define KERNEL_FEATURE_SUBSURFACE (1U << 17U)

/* Use volume materials. */
#define KERNEL_FEATURE_VOLUME (1U << 18U)

/* Use Transparent shadows */
#define KERNEL_FEATURE_TRANSPARENT (1U << 19U)

/* Use shadow catcher. */
#define KERNEL_FEATURE_SHADOW_CATCHER (1U << 20U)

/* Light render passes. */
#define KERNEL_FEATURE_LIGHT_PASSES (1U << 21U)

/* AO. */
#define KERNEL_FEATURE_AO_PASS (1U << 22U)
#define KERNEL_FEATURE_AO_ADDITIVE (1U << 23U)
#define KERNEL_FEATURE_AO (KERNEL_FEATURE_AO_PASS | KERNEL_FEATURE_AO_ADDITIVE)

/* MNEE. */
#define KERNEL_FEATURE_MNEE (1U << 24U)

/* Path guiding. */
#define KERNEL_FEATURE_PATH_GUIDING (1U << 25U)

/* OSL. */
#define KERNEL_FEATURE_OSL_SHADING (1U << 26U)
#define KERNEL_FEATURE_OSL_CAMERA (1U << 27U)

/* Light and shadow linking. */
#define KERNEL_FEATURE_LIGHT_LINKING (1U << 28U)
#define KERNEL_FEATURE_SHADOW_LINKING (1U << 29U)

/* Use denoising kernels and output denoising passes. */
#define KERNEL_FEATURE_DENOISING (1U << 30U)

/* Light tree. */
#define KERNEL_FEATURE_LIGHT_TREE (1U << 31U)

/* Shader node feature mask, to specialize shader evaluation for kernels. */

#define KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT \
  (KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_VORONOI_EXTRA | \
   KERNEL_FEATURE_NODE_LIGHT_PATH | KERNEL_FEATURE_NODE_PORTAL)
#define KERNEL_FEATURE_NODE_MASK_SURFACE_BACKGROUND \
  (KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT | KERNEL_FEATURE_NODE_AOV)
#define KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW \
  (KERNEL_FEATURE_NODE_BSDF | KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_BUMP | \
   KERNEL_FEATURE_NODE_BUMP_STATE | KERNEL_FEATURE_NODE_VORONOI_EXTRA | \
   KERNEL_FEATURE_NODE_LIGHT_PATH | KERNEL_FEATURE_NODE_PRINCIPLED_HAIR | \
   KERNEL_FEATURE_NODE_PORTAL)
#define KERNEL_FEATURE_NODE_MASK_SURFACE \
  (KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW | KERNEL_FEATURE_NODE_RAYTRACE | \
   KERNEL_FEATURE_NODE_AOV | KERNEL_FEATURE_NODE_LIGHT_PATH)
#define KERNEL_FEATURE_NODE_MASK_VOLUME \
  (KERNEL_FEATURE_NODE_EMISSION | KERNEL_FEATURE_NODE_VOLUME | \
   KERNEL_FEATURE_NODE_VORONOI_EXTRA | KERNEL_FEATURE_NODE_LIGHT_PATH | \
   KERNEL_FEATURE_NODE_PORTAL)
#define KERNEL_FEATURE_NODE_MASK_DISPLACEMENT \
  (KERNEL_FEATURE_NODE_VORONOI_EXTRA | KERNEL_FEATURE_NODE_BUMP | \
   KERNEL_FEATURE_NODE_BUMP_STATE | KERNEL_FEATURE_NODE_PORTAL)
#define KERNEL_FEATURE_NODE_MASK_BUMP KERNEL_FEATURE_NODE_MASK_DISPLACEMENT

#define IF_KERNEL_FEATURE(feature) \
  if constexpr ((node_feature_mask & (KERNEL_FEATURE_##feature)) != 0U)
#define IF_KERNEL_NODES_FEATURE(feature) \
  if constexpr ((node_feature_mask & (KERNEL_FEATURE_NODE_##feature)) != 0U)

/* Kernel Feature Guards
 *
 * These are used throughout the code to disable code for certain features entirely.
 * They can be commented out for faster builds, or to measure the performance impact
 * of the existence of the extra code in the kernel.
 *
 * Commenting out the first set of feature guards should work well to reduce kernel
 * compile time while still working for many scenes. */

#define __AO__
#define __LIGHT_LINKING__
#define __MNEE__
#define __OBJECT_MOTION__
#define __POINTCLOUD__
#define __PRINCIPLED_HAIR__
#define __SHADER_RAYTRACE__
#define __SHADOW_CATCHER__
#define __SHADOW_LINKING__
#define __SUBSURFACE__
#define __TRANSPARENT_SHADOWS__
#define __VOLUME__

#define __CAUSTICS_TRICKS__
#define __CLAMP_SAMPLE__
#define __DENOISING_FEATURES__
#define __DPDU__
#define __HAIR__
#define __LIGHT_TREE__
#define __PASSES__
#define __RAY_DIFFERENTIALS__
#define __VISIBILITY_FLAG__
#define __SVM__

/* Device specific features */

#ifdef WITH_OSL
#  define __OSL__
#  ifdef __KERNEL_OPTIX__
/* Kernels with OSL support are built separately in OptiX and don't need SVM. */
#    undef __SVM__
#  endif
#endif
#ifndef __KERNEL_GPU__
#  if defined(WITH_PATH_GUIDING)
#    define __PATH_GUIDING__
#  endif
#  define __VOLUME_RECORD_ALL__
#endif /* !__KERNEL_GPU__ */

/* MNEE caused "Compute function exceeds available temporary registers" in macOS < 13 due to a bug
 * in spill buffer allocation sizing. */
#if defined(__KERNEL_METAL__) && (__KERNEL_METAL_MACOS__ < 13)
#  undef __MNEE__
#endif

/* Scene-based selective features compilation. */

#ifdef __KERNEL_FEATURES__
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_OBJECT_MOTION)
#    undef __OBJECT_MOTION__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_HAIR)
#    undef __HAIR__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_POINTCLOUD)
#    undef __POINTCLOUD__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_VOLUME)
#    undef __VOLUME__
#    if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_TRANSPARENT)
#      undef __TRANSPARENT_SHADOWS__
#    endif
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_SUBSURFACE)
#    undef __SUBSURFACE__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_SHADOW_CATCHER)
#    undef __SHADOW_CATCHER__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_DENOISING)
#    undef __DENOISING_FEATURES__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_AO)
#    undef __AO__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_MNEE)
#    undef __MNEE__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_PATH_GUIDING)
#    undef __PATH_GUIDING__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_NODE_PRINCIPLED_HAIR)
#    undef __PRINCIPLED_HAIR__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_LIGHT_LINKING)
#    undef __LIGHT_LINKING__
#  endif
#  if !(__KERNEL_FEATURES__ & KERNEL_FEATURE_SHADOW_LINKING)
#    undef __SHADOW_LINKING__
#  endif
#endif

#ifdef WITH_CYCLES_DEBUG_NAN
#  define __KERNEL_DEBUG_NAN__
#endif

/* Features that enable others */

#if defined(__SUBSURFACE__) || defined(__SHADER_RAYTRACE__)
#  define __BVH_LOCAL__
#endif

/* Feature mask for integrator states, to allocate state memory only when
 * certain features are on or off. */

struct KernelFeatureRequest {
  uint32_t on;
  uint32_t off;

  explicit KernelFeatureRequest(uint32_t on) : on(on), off(0) {}
  KernelFeatureRequest(uint32_t on, uint32_t off) : on(on), off(off) {}

  bool test(uint32_t features) const
  {
    return (features & on) && !(features & off);
  }
};

CCL_NAMESPACE_END
