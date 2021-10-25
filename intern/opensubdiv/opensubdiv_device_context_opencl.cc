/*
 * Adopted from OpenSubdiv with the following license:
 *
 *   Copyright 2015 Pixar
 *
 *   Licensed under the Apache License, Version 2.0 (the "Apache License")
 *   with the following modification; you may not use this file except in
 *   compliance with the Apache License and the following modification to it:
 *   Section 6. Trademarks. is deleted and replaced with:
 *
 *   6. Trademarks. This License does not grant permission to use the trade
 *      names, trademarks, service marks, or product names of the Licensor
 *      and its affiliates, except as required to comply with Section 4(c) of
 *      the License and to reproduce the content of the NOTICE file.
 *
 *   You may obtain a copy of the Apache License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the Apache License with the above modification is
 *   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *   KIND, either express or implied. See the Apache License for the specific
 *   language governing permissions and limitations under the Apache License.
 *
 */

#ifdef OPENSUBDIV_HAS_OPENCL

#ifdef _MSC_VER
#  include "iso646.h"
#endif

#include "opensubdiv_device_context_opencl.h"

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <OpenGL/OpenGL.h>
#else
#  include <GL/glx.h>
#endif

#include <cstdio>
#include <cstring>
#include <string>

#define message(...)    // fprintf(stderr, __VA_ARGS__)
#define error(...)  fprintf(stderr, __VA_ARGS__)

/* Returns the first found platform. */
static cl_platform_id findPlatform() {
	cl_uint numPlatforms;
	cl_int ciErrNum = clGetPlatformIDs(0, NULL, &numPlatforms);
	if (ciErrNum != CL_SUCCESS) {
		error("Error %d in clGetPlatformIDs call.\n", ciErrNum);
		return NULL;
	}
	if (numPlatforms == 0) {
		error("No OpenCL platform found.\n");
		return NULL;
	}
	cl_platform_id *clPlatformIDs = new cl_platform_id[numPlatforms];
	ciErrNum = clGetPlatformIDs(numPlatforms, clPlatformIDs, NULL);
	char chBuffer[1024];
	for (cl_uint i = 0; i < numPlatforms; ++i) {
		ciErrNum = clGetPlatformInfo(clPlatformIDs[i], CL_PLATFORM_NAME,
		                             1024, chBuffer,NULL);
		if (ciErrNum == CL_SUCCESS) {
			cl_platform_id platformId = clPlatformIDs[i];
			delete[] clPlatformIDs;
			return platformId;
		}
	}
	delete[] clPlatformIDs;
	return NULL;
}

/* Return. the device in clDevices which supports the extension. */
static int findExtensionSupportedDevice(cl_device_id *clDevices,
                                        int numDevices,
                                        const char *extensionName) {
	/* Find a device that supports sharing with GL/D3D11
	 * (SLI / X-fire configurations)
	 */
	cl_int ciErrNum;
	for (int i = 0; i < numDevices; ++i) {
		/* Get extensions string size. */
		size_t extensionSize;
		ciErrNum = clGetDeviceInfo(clDevices[i],
		                           CL_DEVICE_EXTENSIONS, 0, NULL,
		                           &extensionSize);
		if (ciErrNum != CL_SUCCESS) {
			error("Error %d in clGetDeviceInfo\n", ciErrNum);
			return -1;
		}
		if (extensionSize > 0) {
			/* Get extensions string. */
			char *extensions = new char[extensionSize];
			ciErrNum = clGetDeviceInfo(clDevices[i], CL_DEVICE_EXTENSIONS,
			                           extensionSize, extensions,
			                           &extensionSize);
			if (ciErrNum != CL_SUCCESS) {
				error("Error %d in clGetDeviceInfo\n", ciErrNum);
				delete[] extensions;
				continue;
			}
			std::string extString(extensions);
			delete[] extensions;
			/* Parse string. This is bit deficient since the extentions
			 * is space separated.
			 *
			 * The actual string would be "cl_khr_d3d11_sharing"
			 *                         or "cl_nv_d3d11_sharing"
			 */
			if (extString.find(extensionName) != std::string::npos) {
				return i;
			}
		}
	}
	return -1;
}

CLDeviceContext::CLDeviceContext()
    : _clContext(NULL),
      _clCommandQueue(NULL) {
}

CLDeviceContext::~CLDeviceContext() {
	if (_clCommandQueue)
		clReleaseCommandQueue(_clCommandQueue);
	if (_clContext)
		clReleaseContext(_clContext);
}

bool CLDeviceContext::HAS_CL_VERSION_1_1()
{
#ifdef OPENSUBDIV_HAS_CLEW
	static bool clewInitialized = false;
	static bool clewLoadSuccess;
	if (not clewInitialized) {
		clewInitialized = true;
		clewLoadSuccess = clewInit() == CLEW_SUCCESS;
		if (!clewLoadSuccess) {
			error("Loading OpenCL failed.\n");
		}
	}
	return clewLoadSuccess;
#endif
	return true;
}

bool CLDeviceContext::Initialize()
{
#ifdef OPENSUBDIV_HAS_CLEW
	if (!clGetPlatformIDs) {
		error("Error clGetPlatformIDs function not bound.\n");
		return false;
	}
#endif
	cl_int ciErrNum;
	cl_platform_id cpPlatform = findPlatform();

#if defined(_WIN32)
	cl_context_properties props[] = {
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)cpPlatform,
		0
	};
#elif defined(__APPLE__)
	CGLContextObj kCGLContext = CGLGetCurrentContext();
	CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
	cl_context_properties props[] = {
		CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties)kCGLShareGroup,
		0
	};
#else
	cl_context_properties props[] = {
		CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
		CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)cpPlatform,
		0
	};
#endif

#if defined(__APPLE__)
	_clContext = clCreateContext(props, 0, NULL, clLogMessagesToStdoutAPPLE,
	                             NULL, &ciErrNum);
	if (ciErrNum != CL_SUCCESS) {
		error("Error %d in clCreateContext\n", ciErrNum);
		return false;
	}

	size_t devicesSize = 0;
	clGetGLContextInfoAPPLE(_clContext, kCGLContext,
	                        CL_CGL_DEVICES_FOR_SUPPORTED_VIRTUAL_SCREENS_APPLE,
	                        0, NULL, &devicesSize);
	int numDevices = int(devicesSize / sizeof(cl_device_id));
	if (numDevices == 0) {
		error("No sharable devices.\n");
		return false;
	}
	cl_device_id *clDevices = new cl_device_id[numDevices];
	clGetGLContextInfoAPPLE(_clContext, kCGLContext,
	                        CL_CGL_DEVICES_FOR_SUPPORTED_VIRTUAL_SCREENS_APPLE,
	                        numDevices * sizeof(cl_device_id), clDevices, NULL);
	int clDeviceUsed = 0;

#else   // not __APPLE__
	/* Get the number of GPU devices available to the platform. */
	cl_uint numDevices = 0;
	clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 0, NULL, &numDevices);
	if (numDevices == 0) {
		error("No CL GPU device found.\n");
		return false;
	}

	/* Create the device list. */
	cl_device_id *clDevices = new cl_device_id[numDevices];
	clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, numDevices, clDevices, NULL);

	const char *extension = "cl_khr_gl_sharing";
	int clDeviceUsed = findExtensionSupportedDevice(clDevices, numDevices,
	                                                extension);

	if (clDeviceUsed < 0) {
		error("No device found that supports CL/GL context sharing\n");
		delete[] clDevices;
		return false;
	}

	_clContext = clCreateContext(props, 1, &clDevices[clDeviceUsed],
	                             NULL, NULL, &ciErrNum);
#endif   // not __APPLE__
	if (ciErrNum != CL_SUCCESS) {
		error("Error %d in clCreateContext\n", ciErrNum);
		delete[] clDevices;
		return false;
	}
	_clCommandQueue = clCreateCommandQueue(_clContext, clDevices[clDeviceUsed],
	                                       0, &ciErrNum);
	delete[] clDevices;
	if (ciErrNum != CL_SUCCESS) {
		error("Error %d in clCreateCommandQueue\n", ciErrNum);
		return false;
	}
	return true;
}

#endif  /* OPENSUBDIV_HAS_OPENCL */
