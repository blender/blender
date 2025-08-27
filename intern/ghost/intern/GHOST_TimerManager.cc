/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_TimerManager.hh"

#include <algorithm>

#include "GHOST_TimerTask.hh"

GHOST_TimerManager::GHOST_TimerManager() = default;

GHOST_TimerManager::~GHOST_TimerManager()
{
  disposeTimers();
}

uint32_t GHOST_TimerManager::getNumTimers()
{
  return uint32_t(timers_.size());
}

bool GHOST_TimerManager::getTimerFound(GHOST_TimerTask *timer)
{
  TTimerVector::const_iterator iter = std::find(timers_.begin(), timers_.end(), timer);
  return iter != timers_.end();
}

GHOST_TSuccess GHOST_TimerManager::addTimer(GHOST_TimerTask *timer)
{
  GHOST_TSuccess success;
  if (!getTimerFound(timer)) {
    /* Add the timer task. */
    timers_.push_back(timer);
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_TimerManager::removeTimer(GHOST_TimerTask *timer)
{
  GHOST_TSuccess success;
  TTimerVector::iterator iter = std::find(timers_.begin(), timers_.end(), timer);
  if (iter != timers_.end()) {
    /* Remove the timer task. */
    timers_.erase(iter);
    delete timer;
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

uint64_t GHOST_TimerManager::nextFireTime()
{
  uint64_t smallest = GHOST_kFireTimeNever;
  TTimerVector::iterator iter;

  for (iter = timers_.begin(); iter != timers_.end(); ++iter) {
    const uint64_t next = (*iter)->getNext();
    smallest = std::min(next, smallest);
  }

  return smallest;
}

bool GHOST_TimerManager::fireTimers(uint64_t time)
{
  TTimerVector::iterator iter;
  bool anyProcessed = false;

  for (iter = timers_.begin(); iter != timers_.end(); ++iter) {
    if (fireTimer(time, *iter)) {
      anyProcessed = true;
    }
  }

  return anyProcessed;
}

bool GHOST_TimerManager::fireTimer(uint64_t time, GHOST_TimerTask *task)
{
  uint64_t next = task->getNext();

  /* Check if the timer should be fired. */
  if (time > next) {
    /* Fire the timer. */
    GHOST_TimerProcPtr timer_proc = task->getTimerProc();
    uint64_t start = task->getStart();
    timer_proc(task, time - start);

    /* Update the time at which we will fire it again. */
    uint64_t interval = task->getInterval();
    uint64_t numCalls = (next - start) / interval;
    numCalls++;
    next = start + numCalls * interval;
    task->setNext(next);

    return true;
  }
  return false;
}

void GHOST_TimerManager::disposeTimers()
{
  while (timers_.empty() == false) {
    delete timers_[0];
    timers_.erase(timers_.begin());
  }
}
