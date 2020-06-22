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
 * Depending on the use case, the referenced data might have a different structure. More
 * categories can be added when necessary.
 */
enum class VArraySpanCategory {
  SingleArray,
  StartsAndSizes,
};

template<typename T> class VArraySpanBase {
 protected:
  uint m_virtual_size;
  VArraySpanCategory m_category;

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
  bool is_single_array() const
  {
    switch (m_category) {
      case VArraySpanCategory::SingleArray:
        return true;
      case VArraySpanCategory::StartsAndSizes:
        return m_virtual_size == 1;
    }
    BLI_assert(false);
    return false;
  }

  bool is_empty() const
  {
    return this->m_virtual_size == 0;
  }

  uint size() const
  {
    return this->m_virtual_size;
  }
};

/**
 * A virtual array span. Every element of this span contains a virtual span. So it behaves like
 * a blender::Span, but might not be backed up by an actual array.
 */
template<typename T> class VArraySpan : public VArraySpanBase<T> {
 private:
  friend class GVArraySpan;

  VArraySpan(const VArraySpanBase<void> &other)
  {
    memcpy(this, &other, sizeof(VArraySpanBase<void>));
  }

 public:
  VArraySpan()
  {
    this->m_virtual_size = 0;
    this->m_category = VArraySpanCategory::StartsAndSizes;
    this->m_data.starts_and_sizes.starts = nullptr;
    this->m_data.starts_and_sizes.sizes = nullptr;
  }

  VArraySpan(Span<T> span, uint virtual_size)
  {
    this->m_virtual_size = virtual_size;
    this->m_category = VArraySpanCategory::SingleArray;
    this->m_data.single_array.start = span.data();
    this->m_data.single_array.size = span.size();
  }

  VArraySpan(Span<const T *> starts, Span<uint> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    this->m_virtual_size = starts.size();
    this->m_category = VArraySpanCategory::StartsAndSizes;
    this->m_data.starts_and_sizes.starts = starts.begin();
    this->m_data.starts_and_sizes.sizes = sizes.begin();
  }

  VSpan<T> operator[](uint index) const
  {
    BLI_assert(index < this->m_virtual_size);
    switch (this->m_category) {
      case VArraySpanCategory::SingleArray:
        return VSpan<T>(Span<T>(this->m_data.single_array.start, this->m_data.single_array.size));
      case VArraySpanCategory::StartsAndSizes:
        return VSpan<T>(Span<T>(this->m_data.starts_and_sizes.starts[index],
                                this->m_data.starts_and_sizes.sizes[index]));
    }
    BLI_assert(false);
    return {};
  }
};

/**
 * A generic virtual array span. It's just like a VArraySpan, but the type is only known at
 * run-time.
 */
class GVArraySpan : public VArraySpanBase<void> {
 private:
  const CPPType *m_type;

  GVArraySpan() = default;

 public:
  GVArraySpan(const CPPType &type)
  {
    this->m_type = &type;
    this->m_virtual_size = 0;
    this->m_category = VArraySpanCategory::StartsAndSizes;
    this->m_data.starts_and_sizes.starts = nullptr;
    this->m_data.starts_and_sizes.sizes = nullptr;
  }

  GVArraySpan(GSpan array, uint virtual_size)
  {
    this->m_type = &array.type();
    this->m_virtual_size = virtual_size;
    this->m_category = VArraySpanCategory::SingleArray;
    this->m_data.single_array.start = array.buffer();
    this->m_data.single_array.size = array.size();
  }

  GVArraySpan(const CPPType &type, Span<const void *> starts, Span<uint> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    this->m_type = &type;
    this->m_virtual_size = starts.size();
    this->m_category = VArraySpanCategory::StartsAndSizes;
    this->m_data.starts_and_sizes.starts = (void **)starts.begin();
    this->m_data.starts_and_sizes.sizes = sizes.begin();
  }

  template<typename T> GVArraySpan(VArraySpan<T> other)
  {
    this->m_type = &CPPType::get<T>();
    memcpy(this, &other, sizeof(VArraySpanBase<void>));
  }

  const CPPType &type() const
  {
    return *this->m_type;
  }

  template<typename T> VArraySpan<T> typed() const
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    return VArraySpan<T>(*this);
  }

  GVSpan operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case VArraySpanCategory::SingleArray:
        return GVSpan(GSpan(*m_type, m_data.single_array.start, m_data.single_array.size));
      case VArraySpanCategory::StartsAndSizes:
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
