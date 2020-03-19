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
 */

#include <iostream>

#include "GHOST_C-api.h"
#include "GHOST_XrContext.h"
#include "GHOST_Xr_intern.h"

static bool GHOST_XrEventPollNext(XrInstance instance, XrEventDataBuffer &r_event_data)
{
  /* (Re-)initialize as required by specification. */
  r_event_data.type = XR_TYPE_EVENT_DATA_BUFFER;
  r_event_data.next = nullptr;

  return (xrPollEvent(instance, &r_event_data) == XR_SUCCESS);
}

GHOST_TSuccess GHOST_XrEventsHandle(GHOST_XrContextHandle xr_contexthandle)
{
  GHOST_XrContext *xr_context = (GHOST_XrContext *)xr_contexthandle;
  XrEventDataBuffer event_buffer; /* Structure big enough to hold all possible events. */

  if (xr_context == NULL) {
    return GHOST_kFailure;
  }

  while (GHOST_XrEventPollNext(xr_context->getInstance(), event_buffer)) {
    XrEventDataBaseHeader *event = (XrEventDataBaseHeader *)&event_buffer;

    switch (event->type) {
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        xr_context->handleSessionStateChange((XrEventDataSessionStateChanged *)event);
        return GHOST_kSuccess;
      case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        GHOST_XrContextDestroy(xr_contexthandle);
        return GHOST_kSuccess;
      default:
        if (xr_context->isDebugMode()) {
          printf("Unhandled event: %i\n", event->type);
        }
        return GHOST_kFailure;
    }
  }

  return GHOST_kFailure;
}
