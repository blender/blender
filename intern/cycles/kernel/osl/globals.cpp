/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <OSL/oslexec.h>

#include "kernel/osl/globals.h"

CCL_NAMESPACE_BEGIN

OSLThreadData::OSLThreadData(OSLGlobals *osl_globals, const int thread_index)
    : globals(osl_globals), thread_index(thread_index)
{
  if (globals == nullptr || globals->use == false) {
    return;
  }

  ss = globals->ss;

  memset((void *)&shader_globals, 0, sizeof(shader_globals));
  shader_globals.tracedata = &tracedata;

  osl_thread_info = ss->create_thread_info();
  context = ss->get_context(osl_thread_info);
  oiio_thread_info = globals->ts->get_perthread_info();
}

OSLThreadData::~OSLThreadData()
{
  if (context) {
    ss->release_context(context);
  }
  if (osl_thread_info) {
    ss->destroy_thread_info(osl_thread_info);
  }
}

OSLThreadData::OSLThreadData(OSLThreadData &&other) noexcept
    : globals(other.globals),
      ss(other.ss),
      thread_index(other.thread_index),
      shader_globals(other.shader_globals),
      tracedata(other.tracedata),
      osl_thread_info(other.osl_thread_info),
      context(other.context),
      oiio_thread_info(other.oiio_thread_info)
{
  shader_globals.tracedata = &tracedata;

  memset((void *)&other.shader_globals, 0, sizeof(other.shader_globals));
  memset((void *)&other.tracedata, 0, sizeof(other.tracedata));
  other.thread_index = -1;
  other.context = nullptr;
  other.osl_thread_info = nullptr;
  other.oiio_thread_info = nullptr;
}

CCL_NAMESPACE_END
