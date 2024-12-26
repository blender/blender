/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OSL

#  include <OSL/oslexec.h>

#  include "kernel/osl/compat.h"
#  include "kernel/types.h"

#  include "util/map.h"
#  include "util/param.h"
#  include "util/vector.h"

#  ifndef WIN32
using std::isfinite;
#  endif

CCL_NAMESPACE_BEGIN

class OSLRenderServices;
class ColorSpaceProcessor;

/* OSL Globals
 *
 * Data needed by OSL render services, that is global to a rendering session.
 * This includes all OSL shaders, name to attribute mapping and texture handles.
 */

struct OSLGlobals {
  OSLGlobals()
  {
    ss = nullptr;
    ts = nullptr;
    services = nullptr;
    use = false;
  }

  /* per thread data */
  static void thread_init(struct KernelGlobalsCPU *kg,
                          OSLGlobals *osl_globals,
                          const int thread_index);
  static void thread_free(struct KernelGlobalsCPU *kg);

  bool use;

  /* shading system */
  OSL::ShadingSystem *ss;
  OSL::TextureSystem *ts;
  OSLRenderServices *services;

  /* shader states */
  vector<OSL::ShaderGroupRef> surface_state;
  vector<OSL::ShaderGroupRef> volume_state;
  vector<OSL::ShaderGroupRef> displacement_state;
  vector<OSL::ShaderGroupRef> bump_state;
  OSL::ShaderGroupRef background_state;

  /* attributes */
  using ObjectNameMap = unordered_map<OSLUStringHash, int>;

  ObjectNameMap object_name_map;
  vector<ustring> object_names;
};

/* trace() call result */
struct OSLTraceData {
  Ray ray;
  Intersection isect;
  ShaderData sd;
  bool setup;
  bool init;
  bool hit;
};

/* thread key for thread specific data lookup */
struct OSLThreadData {
  OSL::ShaderGlobals globals;
  OSL::PerThreadInfo *osl_thread_info;
  OSLTraceData tracedata;
  OSL::ShadingContext *context;
  OIIO::TextureSystem::Perthread *oiio_thread_info;
};

CCL_NAMESPACE_END

#endif
