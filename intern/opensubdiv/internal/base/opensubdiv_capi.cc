// Copyright 2013 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#include "opensubdiv_capi.h"

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include "internal/base/util.h"
#include "internal/device/device_context_cuda.h"
#include "internal/device/device_context_glsl_compute.h"
#include "internal/device/device_context_glsl_transform_feedback.h"
#include "internal/device/device_context_opencl.h"
#include "internal/device/device_context_openmp.h"

using blender::opensubdiv::CUDADeviceContext;
using blender::opensubdiv::GLSLComputeDeviceContext;
using blender::opensubdiv::GLSLTransformFeedbackDeviceContext;
using blender::opensubdiv::OpenCLDeviceContext;
using blender::opensubdiv::OpenMPDeviceContext;

void openSubdiv_init(void)
{
  // Ensure all OpenGL strings are cached.
  openSubdiv_getAvailableEvaluators();
}

void openSubdiv_cleanup(void)
{
}

int openSubdiv_getAvailableEvaluators(void)
{
  int flags = OPENSUBDIV_EVALUATOR_CPU;

  if (OpenMPDeviceContext::isSupported()) {
    flags |= OPENSUBDIV_EVALUATOR_OPENMP;
  }

  if (OpenCLDeviceContext::isSupported()) {
    flags |= OPENSUBDIV_EVALUATOR_OPENCL;
  }

  if (CUDADeviceContext::isSupported()) {
    flags |= OPENSUBDIV_EVALUATOR_CUDA;
  }

  if (GLSLTransformFeedbackDeviceContext::isSupported()) {
    flags |= OPENSUBDIV_EVALUATOR_GLSL_TRANSFORM_FEEDBACK;
  }

  if (GLSLComputeDeviceContext::isSupported()) {
    flags |= OPENSUBDIV_EVALUATOR_GLSL_COMPUTE;
  }

  return flags;
}

int openSubdiv_getVersionHex(void)
{
#if defined(OPENSUBDIV_VERSION_NUMBER)
  return OPENSUBDIV_VERSION_NUMBER;
#elif defined(OPENSUBDIV_VERSION_MAJOR)
  return OPENSUBDIV_VERSION_MAJOR * 10000 + OPENSUBDIV_VERSION_MINOR * 100 +
         OPENSUBDIV_VERSION_PATCH;
#elif defined(OPENSUBDIV_VERSION)
  const char *version = STRINGIFY(OPENSUBDIV_VERSION);
  if (version[0] == 'v') {
    version += 1;
  }
  int major = 0, minor = 0, patch = 0;
  vector<string> tokens;
  blender::opensubdiv::stringSplit(&tokens, version, "_", true);
  if (tokens.size() == 3) {
    major = atoi(tokens[0].c_str());
    minor = atoi(tokens[1].c_str());
    patch = atoi(tokens[2].c_str());
  }
  return major * 10000 + minor * 100 + patch;
#else
  return 0;
#endif
}
