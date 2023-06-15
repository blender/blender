/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_ITimerTask interface class.
 */

#pragma once

#include "GHOST_Types.h"

/**
 * Interface for a timer task.
 * Timer tasks are created by the system and can be installed by the system.
 * After installation, the timer callback-procedure or "timerProc" will be called
 * periodically. You should not need to inherit this class. It is passed to the
 * application in the timer-callback.<br>
 * <br>
 * Note that GHOST processes timers in the UI thread. You should ask GHOST
 * process messages in order for the timer-callbacks to be called.
 * \see GHOST_ISystem#installTimer
 * \see GHOST_TimerProcPtr
 */
class GHOST_ITimerTask {
 public:
  /**
   * Destructor.
   */
  virtual ~GHOST_ITimerTask() {}

  /**
   * Returns the timer callback.
   * \return The timer callback.
   */
  virtual GHOST_TimerProcPtr getTimerProc() const = 0;

  /**
   * Changes the timer callback.
   * \param timerProc: The timer callback.
   */
  virtual void setTimerProc(const GHOST_TimerProcPtr timerProc) = 0;

  /**
   * Returns the timer user data.
   * \return The timer user data.
   */
  virtual GHOST_TUserDataPtr getUserData() const = 0;

  /**
   * Changes the time user data.
   * \param userData: The timer user data.
   */
  virtual void setUserData(const GHOST_TUserDataPtr userData) = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_ITimerTask")
#endif
};
