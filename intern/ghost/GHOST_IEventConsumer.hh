/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_IEventConsumer interface class.
 */

#pragma once

#include "GHOST_IEvent.hh"

/**
 * Interface class for objects interested in receiving events.
 * Objects interested in events should inherit this class and implement the
 * processEvent() method. They should then be registered with the system that
 * they want to receive events. The system will call the processEvent() method
 * for every installed event consumer to pass events.
 * \see GHOST_ISystem#addEventConsumer
 */
class GHOST_IEventConsumer {
 public:
  /**
   * Destructor.
   */
  virtual ~GHOST_IEventConsumer() {}

  /**
   * This method is called by the system when it has events to dispatch.
   * \see GHOST_ISystem#dispatchEvents
   * \param event: The event that can be handled or ignored.
   * \return Indication as to whether the event was handled.
   */
  virtual bool processEvent(GHOST_IEvent *event) = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_IEventConsumer")
#endif
};
