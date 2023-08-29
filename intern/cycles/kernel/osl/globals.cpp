/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <OSL/oslexec.h>

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/types.h"

#include "kernel/osl/globals.h"
#include "kernel/osl/services.h"

CCL_NAMESPACE_BEGIN

void OSLGlobals::thread_init(KernelGlobalsCPU *kg, OSLGlobals *osl_globals)
{
  /* no osl used? */
  if (!osl_globals->use) {
    kg->osl = NULL;
    return;
  }

  /* Per thread kernel data init. */
  kg->osl = osl_globals;

  OSL::ShadingSystem *ss = kg->osl->ss;
  OSLThreadData *tdata = new OSLThreadData();

  memset((void *)&tdata->globals, 0, sizeof(OSL::ShaderGlobals));
  tdata->globals.tracedata = &tdata->tracedata;
  tdata->osl_thread_info = ss->create_thread_info();
  tdata->context = ss->get_context(tdata->osl_thread_info);

  tdata->oiio_thread_info = osl_globals->ts->get_perthread_info();

  kg->osl_ss = (OSLShadingSystem *)ss;
  kg->osl_tdata = tdata;
}

void OSLGlobals::thread_free(KernelGlobalsCPU *kg)
{
  if (!kg->osl)
    return;

  OSL::ShadingSystem *ss = (OSL::ShadingSystem *)kg->osl_ss;
  OSLThreadData *tdata = kg->osl_tdata;
  ss->release_context(tdata->context);

  ss->destroy_thread_info(tdata->osl_thread_info);

  delete tdata;

  kg->osl = NULL;
  kg->osl_ss = NULL;
  kg->osl_tdata = NULL;
}

CCL_NAMESPACE_END
