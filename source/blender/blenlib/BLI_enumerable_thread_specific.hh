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
 * along with this program; if not, write to the Free Software  Foundation,
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

namespace blender {

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
  T &local()
  {
    return values_.local();
  }

#else /* WITH_TBB */

 private:
  std::mutex mutex_;
  /* Maps thread ids to their corresponding values. The values are not embedded in the map, so that
   * their addresses do not change when the map grows. */
  Map<int, std::unique_ptr<T>> values_;

 public:
  T &local()
  {
    const int thread_id = enumerable_thread_specific_utils::thread_id;
    std::lock_guard lock{mutex_};
    return *values_.lookup_or_add_cb(thread_id, []() { return std::make_unique<T>(); });
  }

#endif /* WITH_TBB */
};

}  // namespace blender
