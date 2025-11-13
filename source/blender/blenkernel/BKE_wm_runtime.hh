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
struct wmWindow;
#ifdef WITH_INPUT_IME
struct wmIMEData;
#endif

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
  ListBase notifier_queue = {nullptr, nullptr};
  /**
   * For duplicate detection.
   * \note keep in sync with `notifier_queue` adding/removing elements must also update this set.
   */
  wmNotifierQueueSet notifier_queue_set;

  /** The current notifier in the `notifier_queue` being handled (clear instead of freeing). */
  const wmNotifier *notifier_current = nullptr;

  /** Operator registry. */
  ListBase operators = {nullptr, nullptr};

  /** Extra overlay cursors to draw, like circles. */
  ListBase paintcursors = {nullptr, nullptr};

  /**
   * Known key configurations.
   * This includes all the #wmKeyConfig members (`defaultconf`, `addonconf`, etc).
   */
  ListBase keyconfigs = {nullptr, nullptr};

  /** Active timers. */
  ListBase timers = {nullptr, nullptr};

  /** Threaded jobs manager. */
  ListBase jobs = {nullptr, nullptr};

  /** Active dragged items. */
  ListBase drags = {nullptr, nullptr};

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
  ListBase event_queue = {nullptr, nullptr};

#ifdef WITH_INPUT_IME
  /**
   * Input Method Editor data - complex character input (especially for Asian character input)
   * Only used when `WITH_INPUT_IME` is defined.
   */
  wmIMEData *ime_data = nullptr;
  bool ime_data_is_composing = false;
#endif

  WindowRuntime() = default;
  ~WindowRuntime();
};

}  // namespace blender::bke
