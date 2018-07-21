// Adopted from OpenSubdiv with the following license:
//
//   Copyright 2015 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.

#include "opensubdiv_device_context_opencl.h"

#ifdef OPENSUBDIV_HAS_OPENCL

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
#include <vector>

#define message(...)  // fprintf(stderr, __VA_ARGS__)
#define error(...) fprintf(stderr, __VA_ARGS__)

namespace {

// Returns the first found platform.
cl_platform_id findPlatform() {
  cl_uint num_platforms;
  cl_int ci_error_number = clGetPlatformIDs(0, NULL, &num_platforms);
  if (ci_error_number != CL_SUCCESS) {
    error("Error %d in clGetPlatformIDs call.\n", ci_error_number);
    return NULL;
  }
  if (num_platforms == 0) {
    error("No OpenCL platform found.\n");
    return NULL;
  }
  std::vector<cl_platform_id> cl_platform_ids(num_platforms);
  ci_error_number = clGetPlatformIDs(num_platforms, &cl_platform_ids[0], NULL);
  char ch_buffer[1024];
  for (cl_uint i = 0; i < num_platforms; ++i) {
    ci_error_number = clGetPlatformInfo(cl_platform_ids[i],
                                        CL_PLATFORM_NAME,
                                        sizeof(ch_buffer),
                                        ch_buffer,
                                        NULL);
    if (ci_error_number == CL_SUCCESS) {
      cl_platform_id platform_id = cl_platform_ids[i];
      return platform_id;
    }
  }
  return NULL;
}

// Return the device in cl_devices which supports the extension.
int findExtensionSupportedDevice(cl_device_id* cl_devices,
                                 int num_devices,
                                 const char* extension_name) {
  // Find a device that supports sharing with GL/D3D11
  // (SLI / X-fire configurations)
  cl_int cl_error_number;
  for (int i = 0; i < num_devices; ++i) {
    // Get extensions string size.
    size_t extensions_size;
    cl_error_number = clGetDeviceInfo(cl_devices[i],
                                      CL_DEVICE_EXTENSIONS,
                                      0,
                                      NULL,
                                      &extensions_size);
    if (cl_error_number != CL_SUCCESS) {
      error("Error %d in clGetDeviceInfo\n", cl_error_number);
      return -1;
    }
    if (extensions_size > 0) {
      // Get extensions string.
      std::string extensions('\0', extensions_size);
      cl_error_number = clGetDeviceInfo(cl_devices[i],
                                        CL_DEVICE_EXTENSIONS,
                                        extensions_size,
                                        &extensions[0],
                                        &extensions_size);
      if (cl_error_number != CL_SUCCESS) {
        error("Error %d in clGetDeviceInfo\n", cl_error_number);
        continue;
      }
      // Parse string. This is bit deficient since the extentions
      // is space separated.
      //
      // The actual string would be "cl_khr_d3d11_sharing"
      //                         or "cl_nv_d3d11_sharing"
      if (extensions.find(extension_name) != std::string::npos) {
        return i;
      }
    }
  }
  return -1;
}

}  // namespace

CLDeviceContext::CLDeviceContext()
    : cl_context_(NULL),
      cl_command_queue_(NULL) {
}

CLDeviceContext::~CLDeviceContext() {
  if (cl_command_queue_) {
    clReleaseCommandQueue(cl_command_queue_);
  }
  if (cl_context_) {
    clReleaseContext(cl_context_);
  }
}

bool CLDeviceContext::HAS_CL_VERSION_1_1() {
#ifdef OPENSUBDIV_HAS_CLEW
  static bool clew_initialized = false;
  static bool clew_load_success;
  if (!clew_initialized) {
    clew_initialized = true;
    clew_load_success = clewInit() == CLEW_SUCCESS;
    if (!clew_load_success) {
      error("Loading OpenCL failed.\n");
    }
  }
  return clew_load_success;
#endif
  return true;
}

bool CLDeviceContext::Initialize() {
#ifdef OPENSUBDIV_HAS_CLEW
  if (!clGetPlatformIDs) {
    error("Error clGetPlatformIDs function not bound.\n");
    return false;
  }
#endif
  cl_int cl_error_number;
  cl_platform_id cp_platform = findPlatform();

#if defined(_WIN32)
  cl_context_properties props[] = {
      CL_GL_CONTEXT_KHR,
      (cl_context_properties)wglGetCurrentContext(),
      CL_WGL_HDC_KHR,
      (cl_context_properties)wglGetCurrentDC(),
      CL_CONTEXT_PLATFORM,
      (cl_context_properties)cp_platform,
      0};
#elif defined(__APPLE__)
  CGLContextObj kCGLContext = CGLGetCurrentContext();
  CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
  cl_context_properties props[] = {CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
                                   (cl_context_properties)kCGLShareGroup,
                                   0};
#else
  cl_context_properties props[] = {
      CL_GL_CONTEXT_KHR,
      (cl_context_properties)glXGetCurrentContext(),
      CL_GLX_DISPLAY_KHR,
      (cl_context_properties)glXGetCurrentDisplay(),
      CL_CONTEXT_PLATFORM,
      (cl_context_properties)cp_platform,
      0};
#endif

#if defined(__APPLE__)
  _clContext = clCreateContext(props, 0, NULL, clLogMessagesToStdoutAPPLE, NULL,
                               &cl_error_number);
  if (cl_error_number != CL_SUCCESS) {
    error("Error %d in clCreateContext\n", cl_error_number);
    return false;
  }
  size_t devices_size = 0;
  clGetGLContextInfoAPPLE(_clContext, kCGLContext,
                          CL_CGL_DEVICES_FOR_SUPPORTED_VIRTUAL_SCREENS_APPLE,
                          0,
                          NULL,
                          &devices_size);
  const int num_devices = devices_size / sizeof(cl_device_id);
  if (num_devices == 0) {
    error("No sharable devices.\n");
    return false;
  }
  std::vector<cl_device_id> cl_devices(num_devices);
  clGetGLContextInfoAPPLE(_clContext, kCGLContext,
                          CL_CGL_DEVICES_FOR_SUPPORTED_VIRTUAL_SCREENS_APPLE,
                          num_devices * sizeof(cl_device_id),
                          &cl_devices[0],
                          NULL);
  int cl_device_used = 0;
#else   // not __APPLE__
  // Get the number of GPU devices available to the platform.
  cl_uint num_devices = 0;
  clGetDeviceIDs(cp_platform, CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);
  if (num_devices == 0) {
    error("No CL GPU device found.\n");
    return false;
  }
  // Create the device list.
  std::vector<cl_device_id> cl_devices(num_devices);
  clGetDeviceIDs(cp_platform,
                 CL_DEVICE_TYPE_GPU,
                 num_devices,
                 &cl_devices[0],
                 NULL);
  const char* extension = "cl_khr_gl_sharing";
  int cl_device_used = findExtensionSupportedDevice(&cl_devices[0],
                                                    num_devices,
                                                    extension);
  if (cl_device_used < 0) {
    error("No device found that supports CL/GL context sharing\n");
    return false;
  }
  cl_context_ = clCreateContext(props,
                                1,
                                &cl_devices[cl_device_used],
                                NULL, NULL,
                                &cl_error_number);
#endif  // not __APPLE__
  if (cl_error_number != CL_SUCCESS) {
    error("Error %d in clCreateContext\n", cl_error_number);
    return false;
  }
  cl_command_queue_ = clCreateCommandQueue(cl_context_,
                                           cl_devices[cl_device_used],
                                           0,
                                           &cl_error_number);
  if (cl_error_number != CL_SUCCESS) {
    error("Error %d in clCreateCommandQueue\n", cl_error_number);
    return false;
  }
  return true;
}

bool CLDeviceContext::IsInitialized() const {
  return (cl_context_ != NULL);
}

cl_context CLDeviceContext::GetContext() const {
  return cl_context_;
}

cl_command_queue CLDeviceContext::GetCommandQueue() const {
  return cl_command_queue_;
}

#endif  // OPENSUBDIV_HAS_OPENCL
