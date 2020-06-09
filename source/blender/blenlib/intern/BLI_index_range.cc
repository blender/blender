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

#include <atomic>
#include <mutex>

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

namespace blender {

static Vector<Array<uint, 0, RawAllocator>, 1, RawAllocator> arrays;
static uint current_array_size = 0;
static uint *current_array = nullptr;
static std::mutex current_array_mutex;

Span<uint> IndexRange::as_span() const
{
  uint min_required_size = m_start + m_size;

  if (min_required_size <= current_array_size) {
    return Span<uint>(current_array + m_start, m_size);
  }

  std::lock_guard<std::mutex> lock(current_array_mutex);

  if (min_required_size <= current_array_size) {
    return Span<uint>(current_array + m_start, m_size);
  }

  uint new_size = std::max<uint>(1000, power_of_2_max_u(min_required_size));
  Array<uint, 0, RawAllocator> new_array(new_size);
  for (uint i = 0; i < new_size; i++) {
    new_array[i] = i;
  }
  arrays.append(std::move(new_array));

  current_array = arrays.last().data();
  std::atomic_thread_fence(std::memory_order_seq_cst);
  current_array_size = new_size;

  return Span<uint>(current_array + m_start, m_size);
}

}  // namespace blender
