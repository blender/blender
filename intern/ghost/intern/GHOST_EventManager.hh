/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventManager class.
 */

#pragma once

#include <deque>
#include <vector>

#include "GHOST_IEventConsumer.hh"

/**
 * Manages an event stack and a list of event consumers.
 * The stack works on a FIFO (First In First Out) basis.
 * Events are pushed on the front of the stack and retrieved from the back.
 * Ownership of the event is transferred to the event manager as soon as an event is pushed.
 * Ownership of the event is transferred from the event manager as soon as an event is popped.
 * Events can be dispatched to the event consumers.
 */
class GHOST_EventManager {
 public:
  /**
   * Constructor.
   */
  GHOST_EventManager();

  /**
   * Destructor.
   */
  ~GHOST_EventManager();

  /**
   * Returns the number of events currently on the stack.
   * \return The number of events on the stack.
   */
  uint32_t getNumEvents();

  /**
   * Returns the number of events of a certain type currently on the stack.
   * \param type: The type of events to be counted.
   * \return The number of events on the stack of this type.
   */
  uint32_t getNumEvents(GHOST_TEventType type);

  /**
   * Pushes an event on the stack.
   * To dispatch it, call dispatchEvent() or dispatchEvents().
   * Do not delete the event!
   * \param event: The event to push on the stack.
   */
  GHOST_TSuccess pushEvent(const GHOST_IEvent *event);

  /**
   * Dispatches the given event directly, bypassing the event stack.
   */
  void dispatchEvent(const GHOST_IEvent *event);

  /**
   * Dispatches the event at the back of the stack.
   * The event will be removed from the stack.
   */
  void dispatchEvent();

  /**
   * Dispatches all the events on the stack.
   * The event stack will be empty afterwards.
   */
  void dispatchEvents();

  /**
   * Adds a consumer to the list of event consumers.
   * \param consumer: The consumer added to the list.
   * \return Indication as to whether addition has succeeded.
   */
  GHOST_TSuccess addConsumer(GHOST_IEventConsumer *consumer);

  /**
   * Removes a consumer from the list of event consumers.
   * \param consumer: The consumer removed from the list.
   * \return Indication as to whether removal has succeeded.
   */
  GHOST_TSuccess removeConsumer(GHOST_IEventConsumer *consumer);

  /**
   * Removes all events for a window from the stack.
   * \param window: The window to remove events for.
   */
  void removeWindowEvents(const GHOST_IWindow *window);

  /**
   * Removes all events of a certain type from the stack.
   * The window parameter is optional.
   * If non-null, the routine will remove events only associated with that window.
   * \param type: The type of events to be removed.
   * \param window: The window to remove the events for.
   */
  void removeTypeEvents(GHOST_TEventType type, const GHOST_IWindow *window = nullptr);

 protected:
  /**
   * Removes all events from the stack.
   */
  void disposeEvents();

  /** A stack with events. */
  using TEventStack = std::deque<const GHOST_IEvent *>;

  /** The event stack. */
  std::deque<const GHOST_IEvent *> m_events;
  std::deque<const GHOST_IEvent *> m_handled_events;

  /** A vector with event consumers. */
  using TConsumerVector = std::vector<GHOST_IEventConsumer *>;

  /** The list with event consumers. */
  TConsumerVector m_consumers;

  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_EventManager")
};
