/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <atomic>
#include <mutex>

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

namespace blender {

static RawVector<RawArray<int64_t, 0>> arrays;
static int64_t current_array_size = 0;
static int64_t *current_array = nullptr;
static std::mutex current_array_mutex;

Span<int64_t> IndexRange::as_span() const
{
  int64_t min_required_size = start_ + size_;

  if (min_required_size <= current_array_size) {
    return Span<int64_t>(current_array + start_, size_);
  }

  std::lock_guard<std::mutex> lock(current_array_mutex);

  if (min_required_size <= current_array_size) {
    return Span<int64_t>(current_array + start_, size_);
  }

  int64_t new_size = std::max<int64_t>(1000, power_of_2_max_u(min_required_size));
  RawArray<int64_t, 0> new_array(new_size);
  for (int64_t i = 0; i < new_size; i++) {
    new_array[i] = i;
  }
  arrays.append(std::move(new_array));

  current_array = arrays.last().data();
  std::atomic_thread_fence(std::memory_order_seq_cst);
  current_array_size = new_size;

  return Span<int64_t>(current_array + start_, size_);
}

}  // namespace blender
