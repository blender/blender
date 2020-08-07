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

#include "BLI_array.hh"
#include "BLI_vector.hh"

#include "FN_attributes_ref.hh"

#include <atomic>
#include <mutex>

namespace blender::sim {

class AttributesAllocator : NonCopyable, NonMovable {
 private:
  struct AttributesBlock {
    Array<void *> buffers;
    int size;
  };

  const fn::AttributesInfo &attributes_info_;
  Vector<std::unique_ptr<AttributesBlock>> allocated_blocks_;
  Vector<fn::MutableAttributesRef> allocated_attributes_;
  int total_allocated_ = 0;
  std::mutex mutex_;

 public:
  AttributesAllocator(const fn::AttributesInfo &attributes_info)
      : attributes_info_(attributes_info)
  {
  }

  ~AttributesAllocator();

  Span<fn::MutableAttributesRef> get_allocations() const
  {
    return allocated_attributes_;
  }

  int total_allocated() const
  {
    return total_allocated_;
  }

  const fn::AttributesInfo &attributes_info() const
  {
    return attributes_info_;
  }

  fn::MutableAttributesRef allocate_uninitialized(int size);
};

class ParticleAllocator : NonCopyable, NonMovable {
 private:
  AttributesAllocator attributes_allocator_;
  std::atomic<int> next_id_;
  uint32_t hash_seed_;

 public:
  ParticleAllocator(const fn::AttributesInfo &attributes_info, int next_id, uint32_t hash_seed)
      : attributes_allocator_(attributes_info), next_id_(next_id), hash_seed_(hash_seed)
  {
  }

  const fn::AttributesInfo &attributes_info() const
  {
    return attributes_allocator_.attributes_info();
  }

  Span<fn::MutableAttributesRef> get_allocations() const
  {
    return attributes_allocator_.get_allocations();
  }

  int total_allocated() const
  {
    return attributes_allocator_.total_allocated();
  }

  fn::MutableAttributesRef allocate(int size);
};

}  // namespace blender::sim
