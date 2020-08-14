/*
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
 */

/** \file
 * \ingroup GHOST
 *
 * Abstraction for XR (VR, AR, MR, ..) access via OpenXR.
 */

#include <cassert>
#include <string>

#include "GHOST_C-api.h"

#include "GHOST_XrContext.h"
#include "GHOST_XrException.h"
#include "GHOST_Xr_intern.h"

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
