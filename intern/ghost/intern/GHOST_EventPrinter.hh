/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventPrinter class.
 */

#pragma once

#include "GHOST_IEventConsumer.hh"

/**
 * An Event consumer that prints all the events to standard out.
 * Really useful when debugging.
 */
class GHOST_EventPrinter : public GHOST_IEventConsumer {
 public:
  /**
   * Prints all the events received to std out.
   * \param event: The event that can be handled or not.
   * \return Indication as to whether the event was handled.
   */
  bool processEvent(GHOST_IEvent *event);

 protected:
  /**
   * Converts GHOST key code to a readable string.
   * \param key: The GHOST key code to convert.
   * \param str: The GHOST key code converted to a readable string.
   */
  void getKeyString(GHOST_TKey key, char str[32]) const;
};
