/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstddef>
#include <functional>

CCL_NAMESPACE_BEGIN

/* Task Limiter
 *
 * Limit total memory usage of parallel memory intensive tasks.
 *
 * NOTE: Task limiters should not be nested, this will deadlock. */

class TaskLimiter {
 public:
  /* Execute task that uses approximately #memory_size, potentially
   * with some delay waiting for other tasks.
   *
   * Returns false if the task was not completed when the cancel() callback
   * returned true. */
  static bool execute(size_t memory_size,
                      const std::function<void()> &task,
                      const std::function<bool()> &cancel = nullptr);

  /* Set memory limit, for testing only. */
  static size_t set_memory_limit(size_t memory_limit);

 private:
  static size_t memory_limit_;
};

CCL_NAMESPACE_END
