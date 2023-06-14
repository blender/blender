/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_TimerManager class.
 */

#pragma once

#include <vector>

#include "GHOST_Types.h"

class GHOST_TimerTask;

/**
 * Manages a list of timer tasks.
 * Timer tasks added are owned by the manager.
 * Don't delete timer task objects.
 */
class GHOST_TimerManager {
 public:
  /**
   * Constructor.
   */
  GHOST_TimerManager();

  /**
   * Destructor.
   */
  ~GHOST_TimerManager();

  /**
   * Returns the number of timer tasks.
   * \return The number of events on the stack.
   */
  uint32_t getNumTimers();

  /**
   * Returns whether this timer task ins in our list.
   * \return Indication of presence.
   */
  bool getTimerFound(GHOST_TimerTask *timer);

  /**
   * Adds a timer task to the list.
   * It is only added when it not already present in the list.
   * \param timer: The timer task added to the list.
   * \return Indication as to whether addition has succeeded.
   */
  GHOST_TSuccess addTimer(GHOST_TimerTask *timer);

  /**
   * Removes a timer task from the list.
   * It is only removed when it is found in the list.
   * \param timer: The timer task to be removed from the list.
   * \return Indication as to whether removal has succeeded.
   */
  GHOST_TSuccess removeTimer(GHOST_TimerTask *timer);

  /**
   * Finds the soonest time the next timer would fire.
   * \return The soonest time the next timer would fire,
   * or GHOST_kFireTimeNever if no timers exist.
   */
  uint64_t nextFireTime();

  /**
   * Checks all timer tasks to see if they are expired and fires them if needed.
   * \param time: The current time.
   * \return True if any timers were fired.
   */
  bool fireTimers(uint64_t time);

  /**
   * Checks this timer task to see if they are expired and fires them if needed.
   * \param time: The current time.
   * \param task: The timer task to check and optionally fire.
   * \return True if the timer fired.
   */
  bool fireTimer(uint64_t time, GHOST_TimerTask *task);

 protected:
  /**
   * Deletes all timers.
   */
  void disposeTimers();

  using TTimerVector = std::vector<GHOST_TimerTask *>;
  /** The list with event consumers. */
  TTimerVector m_timers;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_TimerManager")
#endif
};
