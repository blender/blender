// Copyright 2020 Blender Foundation. All rights reserved.
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
//
// Author: Sergey Sharybin

#include "internal/device/device_context_glsl_compute.h"

#include <GL/glew.h>

namespace blender {
namespace opensubdiv {

bool GLSLComputeDeviceContext::isSupported()
{
  return GLEW_VERSION_4_3 || GLEW_ARB_compute_shader;
}

GLSLComputeDeviceContext::GLSLComputeDeviceContext()
{
}

GLSLComputeDeviceContext::~GLSLComputeDeviceContext()
{
}

}  // namespace opensubdiv
}  // namespace blender
