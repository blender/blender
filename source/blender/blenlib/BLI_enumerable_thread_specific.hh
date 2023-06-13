/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_TBB
/* Quiet top level deprecation message, unrelated to API usage here. */
#  if defined(WIN32) && !defined(NOMINMAX)
/* TBB includes Windows.h which will define min/max macros causing issues
 * when we try to use std::min and std::max later on. */
#    define NOMINMAX
#    define TBB_MIN_MAX_CLEANUP
#  endif
#  include <tbb/enumerable_thread_specific.h>
#  ifdef WIN32
/* We cannot keep this defined, since other parts of the code deal with this on their own, leading
 * to multiple define warnings unless we un-define this, however we can only undefine this if we
 * were the ones that made the definition earlier. */
#    ifdef TBB_MIN_MAX_CLEANUP
#      undef NOMINMAX
#    endif
#  endif
#else
#  include <atomic>
#  include <mutex>

#  include "BLI_map.hh"
#endif

#include "BLI_utility_mixins.hh"

namespace blender::threading {

#ifndef WITH_TBB
namespace enumerable_thread_specific_utils {
inline std::atomic<int> next_id = 0;
inline thread_local int thread_id = next_id.fetch_add(1, std::memory_order_relaxed);
}  // namespace enumerable_thread_specific_utils
#endif

/**
 * This is mainly a wrapper for `tbb::enumerable_thread_specific`. The wrapper is needed because we
 * want to be able to build without tbb.
 *
 * More features of the tbb version can be wrapped when they are used.
 */
template<typename T> class EnumerableThreadSpecific : NonCopyable, NonMovable {
#ifdef WITH_TBB

 private:
  tbb::enumerable_thread_specific<T> values_;

 public:
  using iterator = typename tbb::enumerable_thread_specific<T>::iterator;

  EnumerableThreadSpecific() = default;

  template<typename F> EnumerableThreadSpecific(F initializer) : values_(std::move(initializer)) {}

  T &local()
  {
    return values_.local();
  }

  iterator begin()
  {
    return values_.begin();
  }

  iterator end()
  {
    return values_.end();
  }

#else /* WITH_TBB */

 private:
  std::mutex mutex_;
  /* Maps thread ids to their corresponding values. The values are not embedded in the map, so that
   * their addresses do not change when the map grows. */
  Map<int, std::reference_wrapper<T>> values_;
  Vector<std::unique_ptr<T>> owned_values_;
  std::function<void(void *)> initializer_;

 public:
  using iterator = typename Map<int, std::reference_wrapper<T>>::MutableValueIterator;

  EnumerableThreadSpecific() : initializer_([](void *buffer) { new (buffer) T(); }) {}

  template<typename F>
  EnumerableThreadSpecific(F initializer)
      : initializer_([=](void *buffer) { new (buffer) T(initializer()); })
  {
  }

  T &local()
  {
    const int thread_id = enumerable_thread_specific_utils::thread_id;
    std::lock_guard lock{mutex_};
    return values_.lookup_or_add_cb(thread_id, [&]() {
      T *value = (T *)::operator new(sizeof(T));
      initializer_(value);
      owned_values_.append(std::unique_ptr<T>{value});
      return std::reference_wrapper<T>{*value};
    });
  }

  iterator begin()
  {
    return values_.values().begin();
  }

  iterator end()
  {
    return values_.values().end();
  }

#endif /* WITH_TBB */
};

}  // namespace blender::threading
