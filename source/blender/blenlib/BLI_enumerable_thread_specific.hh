/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#ifdef WITH_TBB
#  include <tbb/enumerable_thread_specific.h>
#endif

#include <atomic>
#include <mutex>

#include "BLI_map.hh"
#include "BLI_utility_mixins.hh"

namespace blender::threading {

namespace enumerable_thread_specific_utils {
inline std::atomic<int> next_id = 0;
inline thread_local int thread_id = next_id.fetch_add(1, std::memory_order_relaxed);
}  // namespace enumerable_thread_specific_utils

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

  template<typename F> EnumerableThreadSpecific(F initializer) : values_(std::move(initializer))
  {
  }

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

  EnumerableThreadSpecific() : initializer_([](void *buffer) { new (buffer) T(); })
  {
  }

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
