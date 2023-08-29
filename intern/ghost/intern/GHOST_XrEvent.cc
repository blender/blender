/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <iostream>

#include "GHOST_C-api.h"
#include "GHOST_XrContext.hh"
#include "GHOST_Xr_intern.hh"

static bool GHOST_XrEventPollNext(XrInstance instance, XrEventDataBuffer &r_event_data)
{
  /* (Re-)initialize as required by specification. */
  r_event_data.type = XR_TYPE_EVENT_DATA_BUFFER;
  r_event_data.next = nullptr;

  return (xrPollEvent(instance, &r_event_data) == XR_SUCCESS);
}

GHOST_TSuccess GHOST_XrEventsHandle(GHOST_XrContextHandle xr_contexthandle)
{
  if (xr_contexthandle == nullptr) {
    return GHOST_kFailure;
  }

  GHOST_XrContext &xr_context = *(GHOST_XrContext *)xr_contexthandle;
  XrEventDataBuffer event_buffer; /* Structure big enough to hold all possible events. */

  while (GHOST_XrEventPollNext(xr_context.getInstance(), event_buffer)) {
    XrEventDataBaseHeader *event = (XrEventDataBaseHeader *)&event_buffer;

    switch (event->type) {
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        xr_context.handleSessionStateChange((XrEventDataSessionStateChanged &)*event);
        return GHOST_kSuccess;
      case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        GHOST_XrContextDestroy(xr_contexthandle);
        return GHOST_kSuccess;
      default:
        if (xr_context.isDebugMode()) {
          printf("Unhandled event: %i\n", event->type);
        }
        return GHOST_kFailure;
    }
  }

  return GHOST_kFailure;
}
