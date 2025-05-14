/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_linear_allocator.hh"

namespace blender::fn {

/**
 * Extension of #UserData that is thread-local. This avoids accessing e.g.
 * `EnumerableThreadSpecific.local()` in every nested lazy-function because the thread local
 * data is passed in by the caller.
 */
class LocalUserData {
 public:
  virtual ~LocalUserData() = default;
};

/**
 * This allows passing arbitrary data into a function. For that, #UserData has to be subclassed.
 * This mainly exists because it's more type safe than passing a `void *` with no type information
 * attached.
 *
 * Some lazy-functions may expect to find a certain type of user data when executed.
 */
class UserData {
 public:
  virtual ~UserData() = default;

  /**
   * Get thread local data for this user-data and the current thread.
   */
  virtual destruct_ptr<LocalUserData> get_local(LinearAllocator<> &allocator);
};

}  // namespace blender::fn
