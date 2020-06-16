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

#ifndef __FN_ARRAY_SPANS_HH__
#define __FN_ARRAY_SPANS_HH__

/** \file
 * \ingroup fn
 *
 * An ArraySpan is a span where every element contains an array (instead of a single element as is
 * the case in a normal span). It's main use case is to reference many small arrays.
 */

#include "FN_spans.hh"

namespace blender {
namespace fn {

/**
 * A virtual array span. Every element of this span contains a virtual span. So it behaves like a
 * blender::Span, but might not be backed up by an actual array.
 */
template<typename T> class VArraySpan {
 private:
  /**
   * Depending on the use case, the referenced data might have a different structure. More
   * categories can be added when necessary.
   */
  enum Category {
    SingleArray,
    StartsAndSizes,
  };

  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      const T *start;
      uint size;
    } single_array;
    struct {
      const T *const *starts;
      const uint *sizes;
    } starts_and_sizes;
  } m_data;

 public:
  VArraySpan()
  {
    m_virtual_size = 0;
    m_category = StartsAndSizes;
    m_data.starts_and_sizes.starts = nullptr;
    m_data.starts_and_sizes.sizes = nullptr;
  }

  VArraySpan(Span<T> span, uint virtual_size)
  {
    m_virtual_size = virtual_size;
    m_category = SingleArray;
    m_data.single_array.start = span.data();
    m_data.single_array.size = span.size();
  }

  VArraySpan(Span<const T *> starts, Span<uint> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    m_virtual_size = starts.size();
    m_category = StartsAndSizes;
    m_data.starts_and_sizes.starts = starts.begin();
    m_data.starts_and_sizes.sizes = sizes.begin();
  }

  bool is_empty() const
  {
    return m_virtual_size == 0;
  }

  uint size() const
  {
    return m_virtual_size;
  }

  VSpan<T> operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case SingleArray:
        return VSpan<T>(Span<T>(m_data.single_array.start, m_data.single_array.size));
      case StartsAndSizes:
        return VSpan<T>(
            Span<T>(m_data.starts_and_sizes.starts[index], m_data.starts_and_sizes.sizes[index]));
    }
    BLI_assert(false);
    return {};
  }
};

/**
 * A generic virtual array span. It's just like a VArraySpan, but the type is only known at
 * run-time.
 */
class GVArraySpan {
 private:
  /**
   * Depending on the use case, the referenced data might have a different structure. More
   * categories can be added when necessary.
   */
  enum Category {
    SingleArray,
    StartsAndSizes,
  };

  const CPPType *m_type;
  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      const void *values;
      uint size;
    } single_array;
    struct {
      const void *const *starts;
      const uint *sizes;
    } starts_and_sizes;
  } m_data;

  GVArraySpan() = default;

 public:
  GVArraySpan(const CPPType &type)
  {
    m_type = &type;
    m_virtual_size = 0;
    m_category = StartsAndSizes;
    m_data.starts_and_sizes.starts = nullptr;
    m_data.starts_and_sizes.sizes = nullptr;
  }

  GVArraySpan(GSpan array, uint virtual_size)
  {
    m_type = &array.type();
    m_virtual_size = virtual_size;
    m_category = SingleArray;
    m_data.single_array.values = array.buffer();
    m_data.single_array.size = array.size();
  }

  GVArraySpan(const CPPType &type, Span<const void *> starts, Span<uint> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    m_type = &type;
    m_virtual_size = starts.size();
    m_category = StartsAndSizes;
    m_data.starts_and_sizes.starts = starts.begin();
    m_data.starts_and_sizes.sizes = sizes.begin();
  }

  bool is_empty() const
  {
    return m_virtual_size == 0;
  }

  uint size() const
  {
    return m_virtual_size;
  }

  const CPPType &type() const
  {
    return *m_type;
  }

  template<typename T> VArraySpan<T> typed() const
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    switch (m_category) {
      case SingleArray:
        return VArraySpan<T>(
            Span<T>((const T *)m_data.single_array.values, m_data.single_array.size));
      case StartsAndSizes:
        return VArraySpan<T>(
            Span<const T *>((const T *const *)m_data.starts_and_sizes.starts, m_virtual_size),
            Span<uint>(m_data.starts_and_sizes.sizes, m_virtual_size));
    }
  }

  GVSpan operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case SingleArray:
        return GVSpan(GSpan(*m_type, m_data.single_array.values, m_data.single_array.size));
      case StartsAndSizes:
        return GVSpan(GSpan(
            *m_type, m_data.starts_and_sizes.starts[index], m_data.starts_and_sizes.sizes[index]));
    }
    BLI_assert(false);
    return GVSpan(*m_type);
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_ARRAY_SPANS_HH__ */
