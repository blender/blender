/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pxr/pxr.h>

#define CCL_NS ccl
#define CCL_NAMESPACE_USING_DIRECTIVE using namespace CCL_NS;

#define HD_CYCLES_NS HdCycles
#define HDCYCLES_NAMESPACE_OPEN_SCOPE \
  namespace HD_CYCLES_NS { \
  CCL_NAMESPACE_USING_DIRECTIVE; \
  PXR_NAMESPACE_USING_DIRECTIVE;
#define HDCYCLES_NAMESPACE_CLOSE_SCOPE }

namespace HD_CYCLES_NS {
class HdCyclesCamera;
class HdCyclesDelegate;
class HdCyclesSession;
class HdCyclesRenderBuffer;
}  // namespace HD_CYCLES_NS

namespace CCL_NS {
class AttributeSet;
class BufferParams;
class Camera;
class Geometry;
class Hair;
class Light;
class Mesh;
class Object;
class ParticleSystem;
class Pass;
class PointCloud;
class Scene;
class Session;
class SessionParams;
class Shader;
class ShaderGraph;
class ShaderNode;
class Volume;
}  // namespace CCL_NS
