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

#ifndef __FN_ATTRIBUTES_REF_HH__
#define __FN_ATTRIBUTES_REF_HH__

/** \file
 * \ingroup fn
 *
 * An AttributesRef references multiple arrays of equal length. Each array has a corresponding name
 * and index.
 */

#include "FN_spans.hh"

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector_set.hh"

namespace blender {
namespace fn {

class AttributesInfo;

class AttributesInfoBuilder : NonCopyable, NonMovable {
 private:
  LinearAllocator<> m_allocator;
  VectorSet<std::string> m_names;
  Vector<const CPPType *> m_types;
  Vector<void *> m_defaults;

  friend AttributesInfo;

 public:
  AttributesInfoBuilder() = default;
  ~AttributesInfoBuilder();

  template<typename T> void add(StringRef name, const T &default_value)
  {
    this->add(name, CPPType::get<T>(), (const void *)&default_value);
  }

  void add(StringRef name, const CPPType &type, const void *default_value = nullptr);
};

/**
 * Stores which attributes are in an AttributesRef. Every attribute has a unique index, a unique
 * name, a type and a default value.
 */
class AttributesInfo : NonCopyable, NonMovable {
 private:
  LinearAllocator<> m_allocator;
  Map<StringRefNull, uint> m_index_by_name;
  Vector<StringRefNull> m_name_by_index;
  Vector<const CPPType *> m_type_by_index;
  Vector<void *> m_defaults;

 public:
  AttributesInfo() = default;
  AttributesInfo(const AttributesInfoBuilder &builder);
  ~AttributesInfo();

  uint size() const
  {
    return m_name_by_index.size();
  }

  IndexRange index_range() const
  {
    return m_name_by_index.index_range();
  }

  StringRefNull name_of(uint index) const
  {
    return m_name_by_index[index];
  }

  uint index_of(StringRef name) const
  {
    return m_index_by_name.lookup_as(name);
  }

  const void *default_of(uint index) const
  {
    return m_defaults[index];
  }

  const void *default_of(StringRef name) const
  {
    return this->default_of(this->index_of(name));
  }

  template<typename T> const T &default_of(uint index) const
  {
    BLI_assert(m_type_by_index[index]->is<T>());
    return *(T *)m_defaults[index];
  }

  template<typename T> const T &default_of(StringRef name) const
  {
    return this->default_of<T>(this->index_of(name));
  }

  const CPPType &type_of(uint index) const
  {
    return *m_type_by_index[index];
  }

  const CPPType &type_of(StringRef name) const
  {
    return this->type_of(this->index_of(name));
  }

  bool has_attribute(StringRef name, const CPPType &type) const
  {
    return this->try_index_of(name, type) >= 0;
  }

  int try_index_of(StringRef name) const
  {
    return (int)m_index_by_name.lookup_default_as(name, -1);
  }

  int try_index_of(StringRef name, const CPPType &type) const
  {
    int index = this->try_index_of(name);
    if (index == -1) {
      return -1;
    }
    else if (this->type_of((uint)index) == type) {
      return index;
    }
    else {
      return -1;
    }
  }
};

/**
 * References multiple arrays that match the description of an AttributesInfo instance. This class
 * is supposed to be relatively cheap to copy. It does not own any of the arrays itself.
 */
class MutableAttributesRef {
 private:
  const AttributesInfo *m_info;
  Span<void *> m_buffers;
  IndexRange m_range;

 public:
  MutableAttributesRef(const AttributesInfo &info, Span<void *> buffers, uint size)
      : MutableAttributesRef(info, buffers, IndexRange(size))
  {
  }

  MutableAttributesRef(const AttributesInfo &info, Span<void *> buffers, IndexRange range)
      : m_info(&info), m_buffers(buffers), m_range(range)
  {
  }

  uint size() const
  {
    return m_range.size();
  }

  const AttributesInfo &info() const
  {
    return *m_info;
  }

  GMutableSpan get(uint index) const
  {
    const CPPType &type = m_info->type_of(index);
    void *ptr = POINTER_OFFSET(m_buffers[index], type.size() * m_range.start());
    return GMutableSpan(type, ptr, m_range.size());
  }

  GMutableSpan get(StringRef name) const
  {
    return this->get(m_info->index_of(name));
  }

  template<typename T> MutableSpan<T> get(uint index) const
  {
    BLI_assert(m_info->type_of(index).is<T>());
    return MutableSpan<T>((T *)m_buffers[index] + m_range.start(), m_range.size());
  }

  template<typename T> MutableSpan<T> get(StringRef name) const
  {
    return this->get<T>(m_info->index_of(name));
  }

  Optional<GMutableSpan> try_get(StringRef name, const CPPType &type) const
  {
    int index = m_info->try_index_of(name, type);
    if (index == -1) {
      return {};
    }
    else {
      return this->get((uint)index);
    }
  }

  template<typename T> Optional<MutableSpan<T>> try_get(StringRef name) const
  {
    int index = m_info->try_index_of(name);
    if (index == -1) {
      return {};
    }
    else if (m_info->type_of((uint)index).is<T>()) {
      return this->get<T>((uint)index);
    }
    else {
      return {};
    }
  }

  MutableAttributesRef slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  MutableAttributesRef slice(uint start, uint size) const
  {
    return MutableAttributesRef(*m_info, m_buffers, m_range.slice(start, size));
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_ATTRIBUTES_REF_HH__ */
