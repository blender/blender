/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_TimerTask class.
 */

#pragma once

#include "GHOST_ITimerTask.hh"

/**
 * Implementation of a timer task.
 */
class GHOST_TimerTask : public GHOST_ITimerTask {
 public:
  /**
   * Constructor.
   * \param start: The timer start time.
   * \param interval: The interval between calls to the #timer_proc.
   * \param timer_proc: The callback invoked when the interval expires.
   * \param user_data: The timer user data.
   */
  GHOST_TimerTask(uint64_t start,
                  uint64_t interval,
                  GHOST_TimerProcPtr timer_proc,
                  GHOST_TUserDataPtr user_data = nullptr)
      : start_(start),
        interval_(interval),
        next_(start),
        timer_proc_(timer_proc),
        user_data_(user_data)
  {
  }

  /**
   * Returns the timer start time.
   * \return The timer start time.
   */
  uint64_t getStart() const
  {
    return start_;
  }

  /**
   * Changes the timer start time.
   * \param start: The timer start time.
   */
  void setStart(uint64_t start)
  {
    start_ = start;
  }

  /**
   * Returns the timer interval.
   * \return The timer interval.
   */
  uint64_t getInterval() const
  {
    return interval_;
  }

  /**
   * Changes the timer interval.
   * \param interval: The timer interval.
   */
  void setInterval(uint64_t interval)
  {
    interval_ = interval;
  }

  /**
   * Returns the time the timer_proc will be called.
   * \return The time the timer_proc will be called.
   */
  uint64_t getNext() const
  {
    return next_;
  }

  /**
   * Changes the time the timer_proc will be called.
   * \param next: The time the timer_proc will be called.
   */
  void setNext(uint64_t next)
  {
    next_ = next;
  }

  /** \copydoc #GHOST_ITimerTask::getTimerProc */
  GHOST_TimerProcPtr getTimerProc() const override
  {
    return timer_proc_;
  }

  /** \copydoc #GHOST_ITimerTask::setTimerProc */
  void setTimerProc(const GHOST_TimerProcPtr timer_proc) override
  {
    timer_proc_ = timer_proc;
  }

  /** \copydoc #GHOST_ITimerTask::getUserData */
  GHOST_TUserDataPtr getUserData() const override
  {
    return user_data_;
  }

  /** \copydoc #GHOST_ITimerTask::setUserData */
  void setUserData(const GHOST_TUserDataPtr user_data) override
  {
    user_data_ = user_data;
  }

  /**
   * Returns the auxiliary storage room.
   * \return The auxiliary storage room.
   */
  uint32_t getAuxData() const
  {
    return aux_data_;
  }

  /**
   * Changes the auxiliary storage room.
   * \param auxData: The auxiliary storage room.
   */
  void setAuxData(uint32_t auxData)
  {
    aux_data_ = auxData;
  }

 protected:
  /** The time the timer task was started. */
  uint64_t start_;

  /** The interval between calls. */
  uint64_t interval_;

  /** The time the timer_proc will be called. */
  uint64_t next_;

  /** The callback invoked when the timer expires. */
  GHOST_TimerProcPtr timer_proc_;

  /** The timer task user data. */
  GHOST_TUserDataPtr user_data_;

  /** Auxiliary storage room. */
  uint32_t aux_data_ = 0;
};
