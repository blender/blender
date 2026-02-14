/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <utility>

CCL_NAMESPACE_BEGIN

/* Execute a callable when the scope exits. */
template<typename F> class ScopedDefer {
 public:
  explicit ScopedDefer(F &&func) : func_(std::forward<F>(func)) {}

  ~ScopedDefer()
  {
    func_();
  }

  ScopedDefer(const ScopedDefer &) = delete;
  ScopedDefer &operator=(const ScopedDefer &) = delete;

 private:
  F func_;
};

/* Helper to deduce template argument. */
template<typename F> ScopedDefer<F> make_scoped_defer(F &&func)
{
  return ScopedDefer<F>(std::forward<F>(func));
}

#define SCOPED_DEFER_NAME2(a, b) a##b
#define SCOPED_DEFER_NAME(a, b) SCOPED_DEFER_NAME2(a, b)
#define SCOPED_DEFER(code) \
  auto SCOPED_DEFER_NAME(_scoped_defer_, __LINE__) = make_scoped_defer([&]() { code; })

CCL_NAMESPACE_END
