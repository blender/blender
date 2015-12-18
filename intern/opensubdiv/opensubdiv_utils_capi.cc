/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *                 Brecht van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "opensubdiv_capi.h"

#include <cstring>
#include <GL/glew.h>

#ifdef _MSC_VER
#  include "iso646.h"
#endif

#ifdef OPENSUBDIV_HAS_OPENCL
#  include "opensubdiv_device_context_opencl.h"
#endif  /* OPENSUBDIV_HAS_OPENCL */

#ifdef OPENSUBDIV_HAS_CUDA
#  include "opensubdiv_device_context_cuda.h"
#endif  /* OPENSUBDIV_HAS_CUDA */

static bool gpu_legacy_support_global = false;

int openSubdiv_getAvailableEvaluators(void)
{
	if (!openSubdiv_supportGPUDisplay()) {
		return 0;
	}

	int flags = OPENSUBDIV_EVALUATOR_CPU;

#ifdef OPENSUBDIV_HAS_OPENMP
	flags |= OPENSUBDIV_EVALUATOR_OPENMP;
#endif  /* OPENSUBDIV_HAS_OPENMP */

#ifdef OPENSUBDIV_HAS_OPENCL
	if (CLDeviceContext::HAS_CL_VERSION_1_1()) {
		flags |= OPENSUBDIV_EVALUATOR_OPENCL;
	}
#endif  /* OPENSUBDIV_HAS_OPENCL */

#ifdef OPENSUBDIV_HAS_CUDA
	if (CudaDeviceContext::HAS_CUDA_VERSION_4_0()) {
		flags |= OPENSUBDIV_EVALUATOR_CUDA;
	}
#endif  /* OPENSUBDIV_HAS_OPENCL */

#ifdef OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK
	if (GLEW_VERSION_4_1) {
		flags |= OPENSUBDIV_EVALUATOR_GLSL_TRANSFORM_FEEDBACK;
	}
#endif  /* OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK */

#ifdef OPENSUBDIV_HAS_GLSL_COMPUTE
	if (GLEW_VERSION_4_3 || GLEW_ARB_compute_shader) {
		flags |= OPENSUBDIV_EVALUATOR_GLSL_COMPUTE;
	}
#endif  /* OPENSUBDIV_HAS_GLSL_COMPUTE */

	return flags;
}

void openSubdiv_init(bool gpu_legacy_support)
{
	/* Ensure all OpenGL strings are cached. */
	(void)openSubdiv_getAvailableEvaluators();
	gpu_legacy_support_global = gpu_legacy_support;
}

void openSubdiv_cleanup(void)
{
	openSubdiv_osdGLDisplayDeinit();
}

bool openSubdiv_gpu_legacy_support(void)
{
	return gpu_legacy_support_global;
}
