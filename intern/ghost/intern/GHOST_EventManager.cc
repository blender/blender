/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_EventManager.hh"
#include "GHOST_Debug.hh"
#include <algorithm>

GHOST_EventManager::GHOST_EventManager() {}

GHOST_EventManager::~GHOST_EventManager()
{
  disposeEvents();

  TConsumerVector::iterator iter = m_consumers.begin();
  while (iter != m_consumers.end()) {
    GHOST_IEventConsumer *consumer = *iter;
    delete consumer;
    iter = m_consumers.erase(iter);
  }
}

uint32_t GHOST_EventManager::getNumEvents()
{
  return uint32_t(m_events.size());
}

uint32_t GHOST_EventManager::getNumEvents(GHOST_TEventType type)
{
  uint32_t numEvents = 0;
  TEventStack::iterator p;
  for (p = m_events.begin(); p != m_events.end(); ++p) {
    if ((*p)->getType() == type) {
      numEvents++;
    }
  }
  return numEvents;
}

GHOST_TSuccess GHOST_EventManager::pushEvent(GHOST_IEvent *event)
{
  GHOST_TSuccess success;
  GHOST_ASSERT(event, "invalid event");
  if (m_events.size() < m_events.max_size()) {
    m_events.push_front(event);
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

void GHOST_EventManager::dispatchEvent(GHOST_IEvent *event)
{
  TConsumerVector::iterator iter;

  for (iter = m_consumers.begin(); iter != m_consumers.end(); ++iter) {
    (*iter)->processEvent(event);
  }
}

void GHOST_EventManager::dispatchEvent()
{
  GHOST_IEvent *event = m_events.back();
  m_events.pop_back();
  m_handled_events.push_back(event);

  dispatchEvent(event);
}

void GHOST_EventManager::dispatchEvents()
{
  while (!m_events.empty()) {
    dispatchEvent();
  }

  disposeEvents();
}

GHOST_TSuccess GHOST_EventManager::addConsumer(GHOST_IEventConsumer *consumer)
{
  GHOST_TSuccess success;
  GHOST_ASSERT(consumer, "invalid consumer");

  /* Check to see whether the consumer is already in our list. */
  TConsumerVector::const_iterator iter = std::find(
      m_consumers.begin(), m_consumers.end(), consumer);

  if (iter == m_consumers.end()) {
    /* Add the consumer. */
    m_consumers.push_back(consumer);
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
  TConsumerVector::iterator iter = std::find(m_consumers.begin(), m_consumers.end(), consumer);

  if (iter != m_consumers.end()) {
    /* Remove the consumer. */
    m_consumers.erase(iter);
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

void GHOST_EventManager::removeWindowEvents(GHOST_IWindow *window)
{
  TEventStack::iterator iter;
  iter = m_events.begin();
  while (iter != m_events.end()) {
    GHOST_IEvent *event = *iter;
    if (event->getWindow() == window) {
      GHOST_PRINT("GHOST_EventManager::removeWindowEvents(): removing event\n");
      /*
       * Found an event for this window, remove it.
       * The iterator will become invalid.
       */
      delete event;
      m_events.erase(iter);
      iter = m_events.begin();
    }
    else {
      ++iter;
    }
  }
}

void GHOST_EventManager::removeTypeEvents(GHOST_TEventType type, GHOST_IWindow *window)
{
  TEventStack::iterator iter;
  iter = m_events.begin();
  while (iter != m_events.end()) {
    GHOST_IEvent *event = *iter;
    if ((event->getType() == type) && (!window || (event->getWindow() == window))) {
      GHOST_PRINT("GHOST_EventManager::removeTypeEvents(): removing event\n");
      /*
       * Found an event of this type for the window, remove it.
       * The iterator will become invalid.
       */
      delete event;
      m_events.erase(iter);
      iter = m_events.begin();
    }
    else {
      ++iter;
    }
  }
}

void GHOST_EventManager::disposeEvents()
{
  while (m_handled_events.empty() == false) {
    GHOST_ASSERT(m_handled_events[0], "invalid event");
    delete m_handled_events[0];
    m_handled_events.pop_front();
  }

  while (m_events.empty() == false) {
    GHOST_ASSERT(m_events[0], "invalid event");
    delete m_events[0];
    m_events.pop_front();
  }
}
