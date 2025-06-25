/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include <chrono>
#include <optional>

#include "BLI_string_ref.hh"

namespace blender::asset_system {

/**
 * Status information about an externally loaded asset library listing, stored globally.
 *
 * Remote asset library downloading is handled in Python. This API allows storing status
 * information globally per URL. Asset UIs can then query the status and reflect it accordingly.
 *
 * Another important use is coordinating the Python side downloading with the C++ side loading.
 * The C++ asset library loading might have to wait for Python to be done downloading and
 * validating individual asset listing pages, and load in these new pages as they become ready.
 *
 * All functions must be called on the same thread.
 */
class RemoteLibraryLoadingStatus {
 public:
  enum Status {
    Loading,
    Finished,
    Failure,
    Cancelled,
  };
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

 private:
  float timeout_;
  TimePoint last_updated_time_point_;
  /* See #RemoteLibraryLoadingStatus::handle_timeout(). */
  TimePoint last_timeout_handled_time_point_;
  TimePoint last_new_pages_time_point_;

  Status status_;
  std::optional<StringRefNull> failure_message_;
  bool metafiles_in_place_;

 public:
  static void begin_loading(StringRef url, float timeout);
  /** Let the state know that the loading is still ongoing, resetting the timeout. */
  static void ping_still_loading(StringRef url);
  static void ping_new_pages(StringRef url);
  static void ping_metafiles_in_place(StringRef url);
  static void set_finished(StringRef url);
  static void set_failure(StringRef url, std::optional<StringRefNull> failure_message);

  static std::optional<StringRefNull> failure_message(StringRef url);
  static std::optional<RemoteLibraryLoadingStatus::Status> status(StringRef url);
  static std::optional<bool> metafiles_in_place(StringRef url);
  static std::optional<TimePoint> last_new_pages_time(StringRef url);

  /**
   * Checks if the status storage timed out, because it hasn't received status updates for the
   * given timeout duration. Changes the status to failure in that case.
   *
   * Note that this function doesn't do more than check if the timeout is reached, and changing
   * state to failure if so. It's meant to be called in regular, short intervalls to make the whole
   * timeout handling work. Current remote asset library loading takes care of this.
   *
   * \return True if the loading status switched to #Status::Failure due to timing out.
   */
  static bool handle_timeout(StringRef url);

 private:
  /** Update the last update time point, effectively resetting the timout timer. */
  void reset_timeout();
};

}  // namespace blender::asset_system
