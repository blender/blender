/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifdef WITH_GHOST_DBUS

#  include <cstdint>
#  include <functional>
#  include <memory>
#  include <string>
#  include <thread>
#  include <variant>

/**
 * The value of a desktop setting, see #GHOST_SystemDBusUnix::setting_add.
 */
using GHOST_DBusValue = std::variant<bool, int32_t, uint32_t, double, std::string>;

/**
 * Watches settings exposed by the XDG Desktop Portal's `org.freedesktop.portal.Settings`
 * interface over DBUS, for example the `org.freedesktop.appearance` `color-scheme` light/dark
 * preference or the desktop's cursor size.
 *
 * Any number of settings can be registered with #setting_add, they all share this object's
 * single connection and background thread. That thread blocks on the connection so changes are
 * delivered as they happen instead of by polling, independent of WAYLAND's own event handling
 * thread and its locking.
 *
 * Nothing here runs on the calling thread: connecting to the session bus and the initial read of
 * each setting happen on the background thread, so a slow (or unresponsive) bus doesn't delay
 * startup.
 */
class GHOST_SystemDBusUnix {
 public:
  /** Shared with the background thread, see the note on detaching in the destructor. */
  struct Data;

 private:
  std::shared_ptr<Data> data_;
  std::thread thread_;

 public:
  using SettingChangedFn = std::function<void(const GHOST_DBusValue &value)>;

  GHOST_SystemDBusUnix();
  ~GHOST_SystemDBusUnix();

  GHOST_SystemDBusUnix(const GHOST_SystemDBusUnix &) = delete;
  GHOST_SystemDBusUnix &operator=(const GHOST_SystemDBusUnix &) = delete;

  /**
   * Register a portal setting to watch, must be called before #start.
   *
   * \param on_change: Called from the background thread, both for the setting's initial value
   * and whenever it changes afterwards. It's never called once this object's destructor has
   * begun, so it may safely reference the owner of this watcher.
   */
  void setting_add(const char *name_space, const char *key, SettingChangedFn on_change);

  /**
   * Start watching the settings registered with #setting_add.
   *
   * Returns false when DBUS isn't usable at all (the library isn't installed, or there is no
   * session bus - as is typical on a render-farm node), in which case no thread is created and
   * no callback will ever run. There is no error otherwise: a portal that doesn't answer or
   * doesn't know a setting simply leaves that setting's callback un-called.
   */
  bool start();

  /**
   * Block until every setting's initial value has been passed to its callback, or until
   * `timeout_ms` has elapsed, returning whether the values arrived in time.
   *
   * Reading is otherwise entirely asynchronous. This exists for values a caller can't draw
   * correctly without (the light/dark preference decides the window decoration colors, which
   * are chosen once when a window is created). Prefer waiting as late as possible: #start is
   * called early in start-up, so by the time anything needs a value it has normally arrived
   * and this returns immediately.
   *
   * Returns false without waiting when #start didn't start a thread, and stops waiting early
   * when the bus turned out to be unreachable.
   */
  bool wait_initial(int timeout_ms);
};

#endif /* WITH_GHOST_DBUS */
