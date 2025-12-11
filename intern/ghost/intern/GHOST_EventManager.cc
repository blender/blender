/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_EventManager.hh"
#include "GHOST_Debug.hh"
#include <algorithm>

GHOST_EventManager::GHOST_EventManager() = default;

GHOST_EventManager::~GHOST_EventManager()
{
  disposeEvents();

  TConsumerVector::iterator iter = consumers_.begin();
  while (iter != consumers_.end()) {
    GHOST_IEventConsumer *consumer = *iter;
    delete consumer;
    iter = consumers_.erase(iter);
  }
}

uint32_t GHOST_EventManager::getNumEvents()
{
  return uint32_t(events_.size());
}

uint32_t GHOST_EventManager::getNumEvents(GHOST_TEventType type)
{
  uint32_t numEvents = 0;
  TEventStack::iterator p;
  for (p = events_.begin(); p != events_.end(); ++p) {
    if ((*p)->getType() == type) {
      numEvents++;
    }
  }
  return numEvents;
}

GHOST_TSuccess GHOST_EventManager::pushEvent(std::unique_ptr<const GHOST_IEvent> event)
{
  GHOST_TSuccess success;
  GHOST_ASSERT(event, "invalid event");
  if (events_.size() < events_.max_size()) {
    events_.push_front(std::move(event));
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

void GHOST_EventManager::dispatchEvent(const GHOST_IEvent *event)
{
  TConsumerVector::iterator iter;

  for (iter = consumers_.begin(); iter != consumers_.end(); ++iter) {
    (*iter)->processEvent(event);
  }
}

void GHOST_EventManager::dispatchEvent()
{
  std::unique_ptr<const GHOST_IEvent> event = std::move(events_.back());
  events_.pop_back();
  const GHOST_IEvent *event_ptr = event.get();
  handled_events_.push_back(std::move(event));

  dispatchEvent(event_ptr);
}

void GHOST_EventManager::dispatchEvents()
{
  while (!events_.empty()) {
    dispatchEvent();
  }

  disposeEvents();
}

GHOST_TSuccess GHOST_EventManager::addConsumer(GHOST_IEventConsumer *consumer)
{
  GHOST_TSuccess success;
  GHOST_ASSERT(consumer, "invalid consumer");

  /* Check to see whether the consumer is already in our list. */
  TConsumerVector::const_iterator iter = std::find(consumers_.begin(), consumers_.end(), consumer);

  if (iter == consumers_.end()) {
    /* Add the consumer. */
    consumers_.push_back(consumer);
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_TSuccess GHOST_EventManager::removeConsumer(GHOST_IEventConsumer *consumer)
{
  GHOST_TSuccess success;
  GHOST_ASSERT(consumer, "invalid consumer");

  /* Check to see whether the consumer is in our list. */
  TConsumerVector::iterator iter = std::find(consumers_.begin(), consumers_.end(), consumer);

  if (iter != consumers_.end()) {
    /* Remove the consumer. */
    consumers_.erase(iter);
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

void GHOST_EventManager::removeWindowEvents(const GHOST_IWindow *window)
{
  TEventStack::iterator iter;
  iter = events_.begin();
  while (iter != events_.end()) {
    std::unique_ptr<const GHOST_IEvent> &event = *iter;
    if (event->getWindow() == window) {
      GHOST_PRINT("GHOST_EventManager::removeWindowEvents(): removing event\n");
      /*
       * Found an event for this window, remove it.
       * The iterator will become invalid.
       */
      event.reset();
      events_.erase(iter);
      iter = events_.begin();
    }
    else {
      ++iter;
    }
  }
}

void GHOST_EventManager::removeTypeEvents(GHOST_TEventType type, const GHOST_IWindow *window)
{
  TEventStack::iterator iter;
  iter = events_.begin();
  while (iter != events_.end()) {
    std::unique_ptr<const GHOST_IEvent> &event = *iter;
    if ((event->getType() == type) && (!window || (event->getWindow() == window))) {
      GHOST_PRINT("GHOST_EventManager::removeTypeEvents(): removing event\n");
      /*
       * Found an event of this type for the window, remove it.
       * The iterator will become invalid.
       */
      event.reset();
      events_.erase(iter);
      iter = events_.begin();
    }
    else {
      ++iter;
    }
  }
}

void GHOST_EventManager::disposeEvents()
{
  while (handled_events_.empty() == false) {
    GHOST_ASSERT(handled_events_[0], "invalid event");
    handled_events_[0].reset();
    handled_events_.pop_front();
  }

  while (events_.empty() == false) {
    GHOST_ASSERT(events_[0], "invalid event");
    events_[0].reset();
    events_.pop_front();
  }
}
