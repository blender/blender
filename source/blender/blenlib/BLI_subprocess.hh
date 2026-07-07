/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * API for subprocess creation and inter-process communication.
 * NOTE: The use of subprocesses is generally discouraged.
 * It should only be used for parallelizing workloads that can only happen on a per-process level
 * due to OS or driver limitations.
 * WARNING: The Subprocess API is only supported on Windows and Linux.
 * Its use should always be inside `#if BLI_SUBPROCESS_SUPPORT` preprocessor directives.
 */

#if defined(_WIN32) || defined(__linux__)
#  define BLI_SUBPROCESS_SUPPORT 1
#else
#  define BLI_SUBPROCESS_SUPPORT 0
#endif

#if BLI_SUBPROCESS_SUPPORT

#  include "BLI_span.hh"
#  include "BLI_string_ref.hh"
#  include "BLI_sys_types.h"
#  include "BLI_utility_mixins.hh"
#  include <string>

#  ifdef _WIN32
typedef void *HANDLE;
#  else
#    include <semaphore.h>
#  endif

namespace blender {

/**
 * Creates a subprocess of the current Blender executable.
 * WARNING: This class doesn't handle subprocess destruction.
 * On Windows, subprocesses are closed automatically when the parent process finishes.
 * On Linux, subprocesses become children of init or SYSTEMD when the parent process finishes.
 */
class BlenderSubprocess : NonCopyable {
 private:
#  ifdef _WIN32
  HANDLE handle_ = nullptr;
#  else
  pid_t pid_ = 0;
#  endif
 public:
  ~BlenderSubprocess();

  /**
   * Create a subprocess and pass the arguments to the main function.
   * NOTE: The subprocess path is not passed as `argv[0]`.
   * `args` only support alphanumeric characters, underscores and hyphen-minus as a safety
   * measure.
   * WARNING: This function shouldn't be called again after it succeeds.
   */
  bool create(Span<StringRefNull> args);
  /**
   * Checks if the subprocess is still running.
   * It always returns false if creation failed.
   * It doesn't detects hanged subprocesses.
   */
  bool is_running();
};

/**
 * Creates or gets access to a block of memory that can be read and written by more than once
 * process.
 * WARNING: It doesn't have any built-in safety measure to prevent concurrent writes or
 * read/writes. Synchronization should be handled with SharedSemaphores.
 */
class SharedMemory : NonCopyable {
 private:
  std::string name_;
#  ifdef _WIN32
  HANDLE handle_;
#  else
  int handle_;
#  endif
  void *data_;
  size_t data_size_;
  bool is_owner_;

 public:
  /**
   * WARNING: The name should be unique a unique identifier across all processes (including
   * multiple Blender instances). You should include the PID of the "owner" process in the name to
   * prevent name collisions.
   * `is_owner` should only be true for the first process that creates a SharedMemory with a given
   * name.
   * On Linux, the memory will become invalid across all processes after the owner destructor has
   * run (Windows uses reference counting).
   */
  SharedMemory(std::string name, size_t size, bool is_owner);
  ~SharedMemory();

  /**
   * Get a pointer to the shared memory block.
   * WARNING: It can be null if creation failed, or invalid if the owner destructor has run.
   * */
  void *get_data()
  {
    return data_;
  }

  size_t get_size()
  {
    return data_size_;
  }
};

/**
 * Creates or get access to a semaphore that can be used across multiple processes.
 */
class SharedSemaphore : NonCopyable {
 private:
  std::string name_;
#  if defined(_WIN32)
  HANDLE handle_;
#  else
  sem_t *handle_;
#  endif
  bool is_owner_;

 public:
  /**
   * WARNING: The name should be unique a unique identifier across all processes (including
   * multiple Blender instances). You should include the PID of the "owner" process in the name to
   * prevent name collisions.
   * `is_owner` should only be true for the last process that needs to read it (It's ok if the
   * creator is not the owner).
   * On Linux, the semaphore will become invalid across all processes after the owner destructor
   * has run (Windows uses reference counting).
   */
  SharedSemaphore(std::string name, bool is_owner);
  ~SharedSemaphore();

  /* Increment the semaphore value. */
  void increment();
  /* Decrement the semaphore value (Blocks until the semaphore value is greater than 0). */
  void decrement();
  /**
   * Try to decrement the semaphore value. Returns true on success.
   * (Blocks until the semaphore value is greater than 0 or the wait time runs out).
   */
  bool try_decrement(int wait_ms = 0);
};

}  // namespace blender

#endif
