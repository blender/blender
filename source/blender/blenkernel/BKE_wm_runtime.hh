/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

struct UndoStack;
struct wmMsgBus;
struct wmKeyConfig;
struct wmEvent;
struct wmWindow;
struct wmIMEData;
struct wmGesture;
struct wmJob;
struct wmDrag;
struct wmPaintCursor;

#include "BKE_report.hh"

#include "DNA_windowmanager_types.h"

#include "BLI_set.hh"

namespace blender::bke {

struct wmNotifierHashForQueue {
  uint64_t operator()(const wmNotifier *note) const;
};
struct wmNotifierEqForQueue {
  bool operator()(const wmNotifier *a, const wmNotifier *b) const;
};
using wmNotifierQueueSet = Set<const wmNotifier *,
                               4,
                               DefaultProbingStrategy,
                               wmNotifierHashForQueue,
                               wmNotifierEqForQueue>;

struct WindowManagerRuntime {
  /** Separate active from drawable. */
  wmWindow *windrawable = nullptr;
  /**
   * \note `CTX_wm_window(C)` is usually preferred.
   * Avoid relying on this where possible as this may become NULL during when handling
   * events that close or replace windows (e.g. opening a file).
   * While this happens rarely in practice, it can cause difficult to reproduce bugs.
   */
  wmWindow *winactive = nullptr;

  /** Indicates whether interface is locked for user interaction. */
  bool is_interface_locked = false;

  /** Information and error reports. */
  ReportList reports;

  /**
   * Refresh/redraw #wmNotifier structs.
   * \note Once in the queue, notifiers should be considered read-only.
   * With the exception of clearing notifiers for data which has been removed,
   * see: #NOTE_CATEGORY_TAG_CLEARED.
   */
  ListBaseT<wmNotifier> notifier_queue = {nullptr, nullptr};
  /**
   * For duplicate detection.
   * \note keep in sync with `notifier_queue` adding/removing elements must also update this set.
   */
  wmNotifierQueueSet notifier_queue_set;

  /** The current notifier in the `notifier_queue` being handled (clear instead of freeing). */
  const wmNotifier *notifier_current = nullptr;

  /** Operator registry. */
  ListBaseT<wmOperator> operators = {nullptr, nullptr};

  /** Extra overlay cursors to draw, like circles. */
  ListBaseT<wmPaintCursor> paintcursors = {nullptr, nullptr};

  /**
   * Known key configurations.
   * This includes all the #wmKeyConfig members (`defaultconf`, `addonconf`, etc).
   */
  ListBaseT<wmKeyConfig> keyconfigs = {nullptr, nullptr};

  /** Active timers. */
  ListBaseT<wmTimer> timers = {nullptr, nullptr};

  /** Threaded jobs manager. */
  ListBaseT<wmJob> jobs = {nullptr, nullptr};

  /** Active dragged items. */
  ListBaseT<wmDrag> drags = {nullptr, nullptr};

  /** Default configuration. */
  wmKeyConfig *defaultconf = nullptr;

  /** Addon configuration. */
  wmKeyConfig *addonconf = nullptr;

  /** User configuration. */
  wmKeyConfig *userconf = nullptr;

  /** All undo history. */
  UndoStack *undo_stack = nullptr;

  wmMsgBus *message_bus = nullptr;

  WindowManagerRuntime();
  ~WindowManagerRuntime();
};

struct WindowRuntime {
  /** All events #wmEvent (ghost level events were handled). */
  ListBaseT<wmEvent> event_queue = {nullptr, nullptr};

  /**
   * Input Method Editor data - complex character input (especially for Asian character input)
   * Only used when `WITH_INPUT_IME` is defined.
   */
  wmIMEData *ime_data = nullptr;
  bool ime_data_is_composing = false;

  /** Don't want to include ghost.h stuff. */
  void *ghostwin = nullptr;

  /** Don't want to include gpu stuff. */
  void *gpuctx = nullptr;

  /** Window+screen handlers, handled last. */
  ListBaseT<wmEventHandler> handlers = {nullptr, nullptr};

  /** Priority handlers, handled first. */
  ListBaseT<wmEventHandler> modalhandlers = {nullptr, nullptr};

  /** Custom drawing callbacks. */
  ListBaseT<struct WindowDrawCB> drawcalls = {nullptr, nullptr};

  /** Gesture stuff. */
  ListBaseT<wmGesture> gesture = {nullptr, nullptr};

  /**
   * Keep the last handled event in `event_queue` here (owned and must be freed).
   *
   * \warning This must only to be used for event queue logic.
   * User interactions should use `eventstate` instead (if the event isn't passed to the function).
   */
  wmEvent *event_last_handled = nullptr;

  /**
   * Storage for event system.
   *
   * For the most part this is storage for `wmEvent.xy` & `wmEvent.modifiers`.
   * newly added key/button events copy the cursor location and modifier state stored here.
   *
   * It's also convenient at times to be able to pass this as if it's a regular event.
   *
   * - This is not simply the current event being handled.
   *   The type and value is always set to the last press/release events
   *   otherwise cursor motion would always clear these values.
   *
   * - The value of `eventstate->modifiers` is set from the last pressed/released modifier key.
   *   This has the down side that the modifier value will be incorrect if users hold both
   *   left/right modifiers then release one. See note in #wm_event_add_ghostevent for details.
   */
  wmEvent *eventstate = nullptr;

  /**
   * The time when the key is pressed in milliseconds (see #GHOST_GetEventTime).
   * Used to detect double-click events.
   */
  uint64_t eventstate_prev_press_time_ms = 0;

  /** Private runtime info to show text in the status bar. */
  void *cursor_keymap_status = nullptr;

  WindowRuntime() = default;
  ~WindowRuntime();
};

}  // namespace blender::bke
