/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#ifdef WITH_GHOST_DBUS

#  include "GHOST_SystemDBusUnix.hh"

#  include "GHOST_Debug.hh"

#  ifdef WITH_GHOST_DBUS_DYNLOAD
#    include "dbus_dynload.h"
#    include "dbus_dynload_API.h"
#  else
#    include <dbus/dbus.h>
#  endif

#  include <array>
#  include <cerrno>
#  include <chrono>
#  include <condition_variable>
#  include <cstdlib>
#  include <mutex>
#  include <utility>
#  include <vector>

#  include <fcntl.h>
#  include <poll.h>
#  include <sys/stat.h>
#  include <unistd.h>

static constexpr const char *PORTAL_SERVICE = "org.freedesktop.portal.Desktop";
static constexpr const char *PORTAL_PATH = "/org/freedesktop/portal/desktop";
static constexpr const char *PORTAL_SETTINGS_IFACE = "org.freedesktop.portal.Settings";
static constexpr const char *PORTAL_SETTINGS_CHANGED = "SettingChanged";

/**
 * Timeout for the initial read of each setting. The `libdbus` default (25 seconds) is too long to
 * leave the thread waiting. The portal is DBUS activated so the first call may have to start the
 * service, keep this generous enough for that.
 */
static constexpr int SETTING_READ_TIMEOUT_MS = 5000;

/**
 * Owns the pair of descriptors behind #Data::stop_pipe, closing whichever are still open when
 * #Data is released. Since #Data is shared between this object and its thread and outlives
 * both, neither has to work out which of them is responsible for tidying up.
 */
class StopPipe {
  int fds_[2] = {-1, -1};

 public:
  StopPipe() = default;
  ~StopPipe()
  {
    this->close_write();
    if (fds_[0] != -1) {
      close(fds_[0]);
      fds_[0] = -1;
    }
  }

  StopPipe(const StopPipe &) = delete;
  StopPipe &operator=(const StopPipe &) = delete;

  bool open()
  {
    if (pipe2(fds_, O_CLOEXEC) != 0) {
      fds_[0] = fds_[1] = -1;
      return false;
    }
    return true;
  }

  /** The end #poll waits on, -1 when #open was never called or failed (#poll ignores it). */
  int read_fd() const
  {
    return fds_[0];
  }

  /** Wakes the thread: a closed write end reports #POLLHUP on #read_fd. */
  void close_write()
  {
    if (fds_[1] != -1) {
      close(fds_[1]);
      fds_[1] = -1;
    }
  }
};

struct GHOST_SystemDBusUnix::Data {
  struct Setting {
    std::string name_space;
    std::string key;
    SettingChangedFn on_change;
  };

  /** Filled in before the thread starts, read-only afterwards. */
  std::vector<Setting> settings;

  /**
   * Guards everything below as well as calls to #Setting::on_change, so the destructor can rely
   * on no callback running (or starting) once it has set #stop.
   */
  std::mutex mutex;
  /** Signalled once #initial_done is set, see #GHOST_SystemDBusUnix::wait_initial. */
  std::condition_variable initial_cond;
  bool stop = false;
  /**
   * Whether the initial read of every setting has been delivered to its callback. Also set when
   * there is nothing to deliver (the bus couldn't be reached, the thread was stopped), so a
   * waiter is never left hanging.
   */
  bool initial_done = false;

  /**
   * Pipe used to wake the thread out of #poll when the destructor wants it to stop, mirroring
   * `GWL_Display::events_pipe`.
   *
   * A pipe is used because closing the DBUS connection does *not* interrupt a #poll another
   * thread is already blocked in, so there is no way to signal through `libdbus` itself.
   */
  StopPipe stop_pipe;

  /**
   * Values received from the bus but not passed to their callback yet, as indices into
   * #settings. Only ever touched by the background thread, see #settings_changed_flush.
   */
  std::vector<std::pair<size_t, GHOST_DBusValue>> pending;
};

using WatcherData = GHOST_SystemDBusUnix::Data;

static bool value_from_iter(DBusMessageIter *iter, GHOST_DBusValue &r_value)
{
  switch (dbus_message_iter_get_arg_type(iter)) {
    case DBUS_TYPE_BOOLEAN: {
      dbus_bool_t value = FALSE;
      dbus_message_iter_get_basic(iter, &value);
      r_value = bool(value);
      return true;
    }
    case DBUS_TYPE_INT32: {
      dbus_int32_t value = 0;
      dbus_message_iter_get_basic(iter, &value);
      r_value = int32_t(value);
      return true;
    }
    case DBUS_TYPE_UINT32: {
      dbus_uint32_t value = 0;
      dbus_message_iter_get_basic(iter, &value);
      r_value = uint32_t(value);
      return true;
    }
    case DBUS_TYPE_DOUBLE: {
      double value = 0.0;
      dbus_message_iter_get_basic(iter, &value);
      r_value = value;
      return true;
    }
    case DBUS_TYPE_STRING: {
      const char *value = nullptr;
      dbus_message_iter_get_basic(iter, &value);
      r_value = std::string(value ? value : "");
      return true;
    }
  }
  return false;
}

/**
 * Read value from the message, stepping into any variants on the way.
 */
static bool value_from_variant(DBusMessageIter *iter, GHOST_DBusValue &r_value)
{
  if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_VARIANT) {
    DBusMessageIter iter_inner;
    dbus_message_iter_recurse(iter, &iter_inner);
    return value_from_variant(&iter_inner, r_value);
  }
  return value_from_iter(iter, r_value);
}

/**
 * Whether connecting to a session bus can be attempted without `libdbus` falling back to
 * auto-starting one. Auto-launch spawns a `dbus-daemon` (and involves X11): on a machine with
 * no desktop session that's both pointless and a known way to hang, so don't go near it.
 *
 * DBUS has not necessarily been dynamically loaded at this point, but that should be fine.
 */
static bool session_bus_is_available()
{
  const char *session_bus_address = getenv("DBUS_SESSION_BUS_ADDRESS");
  if (session_bus_address && session_bus_address[0] != '\0') {
    return true;
  }
  /* `libdbus` checks this well known location before resorting to auto-launch. */
  const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir || runtime_dir[0] == '\0') {
    return false;
  }
  const std::string path = std::string(runtime_dir) + "/bus";
  struct stat statbuf;
  return (stat(path.c_str(), &statbuf) == 0) && S_ISSOCK(statbuf.st_mode);
}

/**
 * Read a setting's current value with `org.freedesktop.portal.Settings.Read`.
 *
 * This is blocking, which is only okay because it runs on the watcher's thread. The blocking call
 * is not interrupted, so a connection that stops answering keeps the thread here for up to
 * #SETTING_READ_TIMEOUT_MS, which is part of the reason the destructor doesn't wait for it.
 */
static bool setting_read(DBusConnection *connection,
                         const WatcherData::Setting &setting,
                         GHOST_DBusValue &r_value)
{
  DBusMessage *message = dbus_message_new_method_call(
      PORTAL_SERVICE, PORTAL_PATH, PORTAL_SETTINGS_IFACE, "Read");
  if (!message) {
    return false;
  }

  const char *name_space = setting.name_space.c_str();
  const char *key = setting.key.c_str();
  if (!dbus_message_append_args(
          message, DBUS_TYPE_STRING, &name_space, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID))
  {
    dbus_message_unref(message);
    return false;
  }

  DBusError error;
  dbus_error_init(&error);
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(
      connection, message, SETTING_READ_TIMEOUT_MS, &error);
  dbus_message_unref(message);

  if (dbus_error_is_set(&error)) {
    dbus_error_free(&error);
  }
  if (!reply) {
    return false;
  }

  DBusMessageIter iter;
  const bool ok = dbus_message_iter_init(reply, &iter) && value_from_variant(&iter, r_value);
  dbus_message_unref(reply);
  return ok;
}

/**
 * Handle `SettingChanged` signals, queuing the value for #settings_changed_flush.
 */
static DBusHandlerResult filter_func(DBusConnection * /*connection*/,
                                     DBusMessage *message,
                                     void *user_data)
{
  WatcherData *data = static_cast<WatcherData *>(user_data);

  if (!dbus_message_is_signal(message, PORTAL_SETTINGS_IFACE, PORTAL_SETTINGS_CHANGED)) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  /* Signal arguments are `(s name_space, s key, v value)`. */
  DBusMessageIter iter;
  const char *name_space = nullptr;
  const char *key = nullptr;
  if (!dbus_message_iter_init(message, &iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
  {
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  dbus_message_iter_get_basic(&iter, &name_space);
  if (!dbus_message_iter_next(&iter) || dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
  {
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  dbus_message_iter_get_basic(&iter, &key);
  if (!dbus_message_iter_next(&iter)) {
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  for (size_t i = 0; i < data->settings.size(); i++) {
    const WatcherData::Setting &setting = data->settings[i];
    if ((setting.name_space != name_space) || (setting.key != key)) {
      continue;
    }
    GHOST_DBusValue value;
    if (value_from_variant(&iter, value)) {
      data->pending.emplace_back(i, std::move(value));
    }
    break;
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

/**
 * Pass the values collected by #filter_func to their callbacks.
 *
 * Callbacks intentionally run here instead of from #filter_func, so that no DBUS lock is held
 * while calling code that knows nothing about this connection and takes locks of its own.
 */
static void settings_changed_flush(WatcherData *data)
{
  if (data->pending.empty()) {
    return;
  }
  {
    std::lock_guard lock{data->mutex};
    /* Skipped once the destructor has run: `on_change` may reference the watcher's owner. */
    if (!data->stop) {
      for (const std::pair<size_t, GHOST_DBusValue> &item : data->pending) {
        data->settings[item.first].on_change(item.second);
      }
    }
  }
  data->pending.clear();
}

/** Open a private connection to the session bus, null when it can't be reached. */
static DBusConnection *bus_connect()
{
  DBusError error;
  dbus_error_init(&error);
  /* A private connection: the shared one from `dbus_bus_get` is used by other libraries in the
   * process, dispatching it here would steal their messages. */
  DBusConnection *connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
  if (dbus_error_is_set(&error)) {
    dbus_error_free(&error);
  }
  if (!connection) {
    return nullptr;
  }
  dbus_connection_set_exit_on_disconnect(connection, false);
  return connection;
}

static bool settings_subscribe(WatcherData *data, DBusConnection *connection)
{
  for (const WatcherData::Setting &setting : data->settings) {
    const std::string match_rule = std::string("type='signal',interface='") +
                                   PORTAL_SETTINGS_IFACE + "',member='" + PORTAL_SETTINGS_CHANGED +
                                   "',arg0='" + setting.name_space + "',arg1='" + setting.key +
                                   "'";
    DBusError error;
    dbus_error_init(&error);
    dbus_bus_add_match(connection, match_rule.c_str(), &error);
    if (dbus_error_is_set(&error)) {
      dbus_error_free(&error);
      return false;
    }
  }

  if (!dbus_connection_add_filter(connection, filter_func, data, nullptr)) {
    return false;
  }
  dbus_connection_flush(connection);
  return true;
}

/** Release anyone waiting in #GHOST_SystemDBusUnix::wait_initial. */
static void initial_done_set(WatcherData *data)
{
  {
    std::lock_guard lock{data->mutex};
    if (data->initial_done) {
      return;
    }
    data->initial_done = true;
  }
  data->initial_cond.notify_all();
}

static bool thread_should_stop(WatcherData *data)
{
  std::lock_guard lock{data->mutex};
  return data->stop;
}

/**
 * Dispatch signals until the destructor asks to stop or the connection goes away.
 *
 * #poll is used directly rather than letting `dbus_connection_read_write_dispatch` block:
 * it can then watch #Data::stop_pipe alongside the bus, which is the only way to wake this
 * thread promptly.
 */
static void thread_dispatch_loop(WatcherData *data, DBusConnection *connection)
{
  int connection_fd = -1;
  if (!dbus_connection_get_unix_fd(connection, &connection_fd)) {
    return;
  }

  for (;;) {
    /* Process everything already queued. A single read can yield several messages, and the
     * extra ones don't make the socket readable again, so they have to be drained before
     * blocking or they would remain unprocessed until the next change arrived. */
    do {
      if (!dbus_connection_read_write_dispatch(connection, 0)) {
        return;
      }
      settings_changed_flush(data);
    } while (dbus_connection_get_dispatch_status(connection) == DBUS_DISPATCH_DATA_REMAINS);

    if (thread_should_stop(data)) {
      return;
    }

    /* Wait for something to do. This call makes watching cheap; the thread spends virtually its
     * whole life waiting here.
     *
     * Two descriptors are watched, with either ending the wait:
     * 1. The bus, once a signal has arrived for the drain above to dispatch.
     * 2. #Data::stop_pipe. The destructor closing this shows as #POLLHUP.
     *
     * The -1 timeout blocks indefinitely. */
    std::array<pollfd, 2> fds = {{
        {connection_fd, POLLIN | POLLPRI, 0},
        {data->stop_pipe.read_fd(), POLLIN, 0},
    }};
    if (poll(fds.data(), fds.size(), -1) == -1) {
      /* Interrupted by a signal before anything was ready, simply wait again. */
      if (errno == EINTR) {
        continue;
      }
      return;
    }
    if (fds[1].revents != 0) {
      /* The destructor asked this thread to stop. */
      return;
    }
    if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
      /* The bus is no longer available. */
      return;
    }
  }
}

/**
 * Read each setting's current value once, then watch for changes until asked to stop.
 * The caller owns `connection`, see #thread_run.
 */
static void thread_watch(WatcherData *data, DBusConnection *connection)
{
  /* Subscribe before reading, otherwise a change made in between would be missed. */
  if (!settings_subscribe(data, connection)) {
    return;
  }

  for (size_t i = 0; i < data->settings.size(); i++) {
    if (thread_should_stop(data)) {
      return;
    }
    GHOST_DBusValue value;
    if (setting_read(connection, data->settings[i], value)) {
      data->pending.emplace_back(i, std::move(value));
    }
  }
  settings_changed_flush(data);
  initial_done_set(data);

  thread_dispatch_loop(data, connection);
}

static void thread_run(std::shared_ptr<GHOST_SystemDBusUnix::Data> data_ptr)
{
  WatcherData *data = data_ptr.get();

  DBusConnection *connection = bus_connect();
  if (!connection) {
    initial_done_set(data);
    return;
  }

  thread_watch(data, connection);

  /* A private connection must be closed before its last reference is dropped. */
  dbus_connection_close(connection);
  dbus_connection_unref(connection);

  initial_done_set(data);
}

GHOST_SystemDBusUnix::GHOST_SystemDBusUnix() : data_(std::make_shared<Data>()) {}

GHOST_SystemDBusUnix::~GHOST_SystemDBusUnix()
{
  if (!thread_.joinable()) {
    return;
  }

  {
    std::lock_guard lock{data_->mutex};
    data_->stop = true;
    /* Wakes the thread's #poll, see #Data::stop_pipe. */
    data_->stop_pipe.close_write();
  }

  /* The connection is deliberately left for the thread to close. Doing it from here could end the
   * initial #setting_read sooner, but tearing a connection down from a thread other than the one
   * dispatching it is reported by ThreadSanitizer as a potential deadlock. Since this doesn't
   * wait for the thread anyway, there is nothing to gain from it. */

  /* Detached rather than joined: the thread stops on its own moments after the wake above, but
   * neither `dbus_bus_get_private` nor a blocking #setting_read can be interrupted from here,
   * so a dbus session that goes quiet mid-exchange would hold up Blender's exit for as long as
   * `libdbus` waits on it, which is unnecessary: `data_` is shared with the thread and outlives
   * this object, and `stop` (checked under `mutex` before every callback) keeps it from reaching
   * into memory owned by this object. */
  thread_.detach();
}

bool GHOST_SystemDBusUnix::wait_initial(const int timeout_ms)
{
  if (!thread_.joinable()) {
    return false;
  }
  std::unique_lock lock{data_->mutex};
  return data_->initial_cond.wait_for(
      lock, std::chrono::milliseconds(timeout_ms), [this] { return data_->initial_done; });
}

void GHOST_SystemDBusUnix::setting_add(const char *name_space,
                                       const char *key,
                                       SettingChangedFn on_change)
{
  GHOST_ASSERT(!thread_.joinable(), "Settings must be added before starting the watcher");
  Data::Setting &setting = data_->settings.emplace_back();
  setting.name_space = name_space;
  setting.key = key;
  setting.on_change = std::move(on_change);
}

bool GHOST_SystemDBusUnix::start()
{
  if (data_->settings.empty()) {
    return false;
  }

  if (!session_bus_is_available()) {
    return false;
  }

#  ifdef WITH_GHOST_DBUS_DYNLOAD
  if (!dbus_dynload_init(false)) {
    return false;
  }
#  endif

  /* Created here rather than on the thread so the destructor can always rely on it, even if it
   * runs before the thread starts. */
  if (!data_->stop_pipe.open()) {
    return false;
  }

  thread_ = std::thread(thread_run, data_);
  return true;
}

#endif /* WITH_GHOST_DBUS */
