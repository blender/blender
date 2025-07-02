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
   * \param interval: The interval between calls to the #timerProc.
   * \param timerProc: The callback invoked when the interval expires.
   * \param userData: The timer user data.
   */
  GHOST_TimerTask(uint64_t start,
                  uint64_t interval,
                  GHOST_TimerProcPtr timerProc,
                  GHOST_TUserDataPtr userData = nullptr)
      : m_start(start),
        m_interval(interval),
        m_next(start),
        m_timerProc(timerProc),
        m_userData(userData)
  {
  }

  /**
   * Returns the timer start time.
   * \return The timer start time.
   */
  uint64_t getStart() const
  {
    return m_start;
  }

  /**
   * Changes the timer start time.
   * \param start: The timer start time.
   */
  void setStart(uint64_t start)
  {
    m_start = start;
  }

  /**
   * Returns the timer interval.
   * \return The timer interval.
   */
  uint64_t getInterval() const
  {
    return m_interval;
  }

  /**
   * Changes the timer interval.
   * \param interval: The timer interval.
   */
  void setInterval(uint64_t interval)
  {
    m_interval = interval;
  }

  /**
   * Returns the time the timerProc will be called.
   * \return The time the timerProc will be called.
   */
  uint64_t getNext() const
  {
    return m_next;
  }

  /**
   * Changes the time the timerProc will be called.
   * \param next: The time the timerProc will be called.
   */
  void setNext(uint64_t next)
  {
    m_next = next;
  }

  /** \copydoc #GHOST_ITimerTask::getTimerProc */
  GHOST_TimerProcPtr getTimerProc() const override
  {
    return m_timerProc;
  }

  /** \copydoc #GHOST_ITimerTask::setTimerProc */
  void setTimerProc(const GHOST_TimerProcPtr timerProc) override
  {
    m_timerProc = timerProc;
  }

  /** \copydoc #GHOST_ITimerTask::getUserData */
  GHOST_TUserDataPtr getUserData() const override
  {
    return m_userData;
  }

  /** \copydoc #GHOST_ITimerTask::setUserData */
  void setUserData(const GHOST_TUserDataPtr userData) override
  {
    m_userData = userData;
  }

  /**
   * Returns the auxiliary storage room.
   * \return The auxiliary storage room.
   */
  uint32_t getAuxData() const
  {
    return m_auxData;
  }

  /**
   * Changes the auxiliary storage room.
   * \param auxData: The auxiliary storage room.
   */
  void setAuxData(uint32_t auxData)
  {
    m_auxData = auxData;
  }

 protected:
  /** The time the timer task was started. */
  uint64_t m_start;

  /** The interval between calls. */
  uint64_t m_interval;

  /** The time the timerProc will be called. */
  uint64_t m_next;

  /** The callback invoked when the timer expires. */
  GHOST_TimerProcPtr m_timerProc;

  /** The timer task user data. */
  GHOST_TUserDataPtr m_userData;

  /** Auxiliary storage room. */
  uint32_t m_auxData = 0;
};
