/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_CallbackEventConsumer class.
 */

#pragma once

#include "GHOST_IEventConsumer.hh"
#include "GHOST_Types.hh"

/**
 * Definition of a callback routine that receives events.
 * \param event: The event received.
 * \param user_data: The callback's user data, supplied to #GHOST_CreateSystem.
 */
using GHOST_EventCallbackProcPtr = bool (*)(const GHOST_IEvent *event,
                                            GHOST_TUserDataPtr user_data);

/**
 * Event consumer that will forward events to a call-back routine.
 * Especially useful for the C-API.
 */
class GHOST_CallbackEventConsumer : public GHOST_IEventConsumer {
 public:
  /**
   * Constructor.
   * \param eventCallback: The call-back routine invoked.
   * \param user_data: The data passed back through the call-back routine.
   */
  GHOST_CallbackEventConsumer(GHOST_EventCallbackProcPtr eventCallback,
                              GHOST_TUserDataPtr user_data);

  /**
   * Destructor.
   */
  ~GHOST_CallbackEventConsumer() override = default;

  /**
   * This method is called by an event producer when an event is available.
   * \param event: The event that can be handled or ignored.
   * \return Indication as to whether the event was handled.
   */
  bool processEvent(const GHOST_IEvent *event) override;

 protected:
  /** The call-back routine invoked. */
  GHOST_EventCallbackProcPtr event_callback_;
  /** The data passed back through the call-back routine. */
  GHOST_TUserDataPtr user_data_;

  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_CallbackEventConsumer")
};
