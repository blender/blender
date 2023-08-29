/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Abstraction for XR (VR, AR, MR, ..) access via OpenXR.
 */

#include <cassert>
#include <string>

#include "GHOST_C-api.h"

#include "GHOST_XrContext.hh"
#include "GHOST_XrException.hh"
#include "GHOST_Xr_intern.hh"

GHOST_XrContextHandle GHOST_XrContextCreate(const GHOST_XrContextCreateInfo *create_info)
{
  auto xr_context = std::make_unique<GHOST_XrContext>(create_info);

  /* TODO GHOST_XrContext's should probably be owned by the GHOST_System, which will handle context
   * creation and destruction. Try-catch logic can be moved to C-API then. */
  try {
    xr_context->initialize(create_info);
  }
  catch (GHOST_XrException &e) {
    xr_context->dispatchErrorMessage(&e);
    return nullptr;
  }

  /* Give ownership to the caller. */
  return (GHOST_XrContextHandle)xr_context.release();
}

void GHOST_XrContextDestroy(GHOST_XrContextHandle xr_contexthandle)
{
  delete (GHOST_XrContext *)xr_contexthandle;
}

void GHOST_XrErrorHandler(GHOST_XrErrorHandlerFn handler_fn, void *customdata)
{
  GHOST_XrContext::setErrorHandler(handler_fn, customdata);
}
