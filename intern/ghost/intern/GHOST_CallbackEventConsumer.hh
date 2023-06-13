/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_CallbackEventConsumer class.
 */

#pragma once

#include "GHOST_C-api.h"
#include "GHOST_IEventConsumer.hh"

/**
 * Event consumer that will forward events to a call-back routine.
 * Especially useful for the C-API.
 */
class GHOST_CallbackEventConsumer : public GHOST_IEventConsumer {
 public:
  /**
   * Constructor.
   * \param eventCallback: The call-back routine invoked.
   * \param userData: The data passed back through the call-back routine.
   */
  GHOST_CallbackEventConsumer(GHOST_EventCallbackProcPtr eventCallback,
                              GHOST_TUserDataPtr userData);

  /**
   * Destructor.
   */
  ~GHOST_CallbackEventConsumer() {}

  /**
   * This method is called by an event producer when an event is available.
   * \param event: The event that can be handled or ignored.
   * \return Indication as to whether the event was handled.
   */
  bool processEvent(GHOST_IEvent *event);

 protected:
  /** The call-back routine invoked. */
  GHOST_EventCallbackProcPtr m_eventCallback;
  /** The data passed back through the call-back routine. */
  GHOST_TUserDataPtr m_userData;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_CallbackEventConsumer")
#endif
};
