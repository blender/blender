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

#ifndef __FN_SPANS_HH__
#define __FN_SPANS_HH__

/** \file
 * \ingroup fn
 *
 * This file implements multiple variants of a span for different use cases. There are two
 * requirements of the function system that require span implementations other from
 * blender::Span<T>.
 * 1. The function system works with a run-time type system (see `CPPType`). Therefore, it has to
 *   deal with types in a generic way. The type of a Span<T> has to be known at compile time.
 * 2. Span<T> expects an underlying memory buffer that is as large as the span. However, sometimes
 *   we can save some memory and processing when we know that all elements are the same.
 *
 * The first requirement is solved with generic spans, which use the "G" prefix. Those
 * store a CPPType instance to keep track of the type that is currently stored.
 *
 * The second requirement is solved with virtual spans. A virtual span behaves like a normal span,
 * but it might not be backed up by an actual array. Elements in a virtual span are always
 * immutable.
 *
 * Different use cases require different combinations of these properties and therefore use
 * different data structures.
 */

#include "BLI_span.hh"

#include "FN_cpp_type.hh"

namespace blender {
namespace fn {

/**
 * A generic span. It behaves just like a blender::Span<T>, but the type is only known at run-time.
 */
class GSpan {
 private:
  const CPPType *m_type;
  const void *m_buffer;
  uint m_size;

 public:
  GSpan(const CPPType &type, const void *buffer, uint size)
      : m_type(&type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  GSpan(const CPPType &type) : GSpan(type, nullptr, 0)
  {
  }

  template<typename T>
  GSpan(Span<T> array) : GSpan(CPPType::get<T>(), (const void *)array.begin(), array.size())
  {
  }

  const CPPType &type() const
  {
    return *m_type;
  }

  bool is_empty() const
  {
    return m_size == 0;
  }

  uint size() const
  {
    return m_size;
  }

  const void *buffer() const
  {
    return m_buffer;
  }

  const void *operator[](uint index) const
  {
    BLI_assert(index < m_size);
    return POINTER_OFFSET(m_buffer, m_type->size() * index);
  }

  template<typename T> Span<T> typed() const
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    return Span<T>((const T *)m_buffer, m_size);
  }
};

/**
 * A generic mutable span. It behaves just like a blender::MutableSpan<T>, but the type is only
 * known at run-time.
 */
class GMutableSpan {
 private:
  const CPPType *m_type;
  void *m_buffer;
  uint m_size;

 public:
  GMutableSpan(const CPPType &type, void *buffer, uint size)
      : m_type(&type), m_buffer(buffer), m_size(size)
  {
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  GMutableSpan(const CPPType &type) : GMutableSpan(type, nullptr, 0)
  {
  }

  template<typename T>
  GMutableSpan(MutableSpan<T> array)
      : GMutableSpan(CPPType::get<T>(), (void *)array.begin(), array.size())
  {
  }

  operator GSpan() const
  {
    return GSpan(*m_type, m_buffer, m_size);
  }

  const CPPType &type() const
  {
    return *m_type;
  }

  bool is_empty() const
  {
    return m_size == 0;
  }

  uint size() const
  {
    return m_size;
  }

  void *buffer()
  {
    return m_buffer;
  }

  void *operator[](uint index)
  {
    BLI_assert(index < m_size);
    return POINTER_OFFSET(m_buffer, m_type->size() * index);
  }

  template<typename T> MutableSpan<T> typed()
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    return MutableSpan<T>((T *)m_buffer, m_size);
  }
};

/**
 * A virtual span. It behaves like a blender::Span<T>, but might not be backed up by an actual
 * array.
 */
template<typename T> class VSpan {
 private:
  enum Category {
    Single,
    FullArray,
    FullPointerArray,
  };

  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      const T *data;
    } single;
    struct {
      const T *data;
    } full_array;
    struct {
      const T *const *data;
    } full_pointer_array;
  } m_data;

 public:
  VSpan()
  {
    m_virtual_size = 0;
    m_category = FullArray;
    m_data.full_array.data = nullptr;
  }

  VSpan(Span<T> values)
  {
    m_virtual_size = values.size();
    m_category = FullArray;
    m_data.full_array.data = values.begin();
  }

  VSpan(MutableSpan<T> values) : VSpan(Span<T>(values))
  {
  }

  VSpan(Span<const T *> values)
  {
    m_virtual_size = values.size();
    m_category = FullPointerArray;
    m_data.full_pointer_array.data = values.begin();
  }

  static VSpan FromSingle(const T *value, uint virtual_size)
  {
    VSpan ref;
    ref.m_virtual_size = virtual_size;
    ref.m_category = Single;
    ref.m_data.single.data = value;
    return ref;
  }

  const T &operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case Single:
        return *m_data.single.data;
      case FullArray:
        return m_data.full_array.data[index];
      case FullPointerArray:
        return *m_data.full_pointer_array.data[index];
    }
    BLI_assert(false);
    return *m_data.single.data;
  }

  bool is_empty() const
  {
    return m_virtual_size == 0;
  }

  uint size() const
  {
    return m_virtual_size;
  }
};

/**
 * A generic virtual span. It behaves like a blender::Span<T>, but the type is only known at
 * run-time and it might not be backed up by an actual array.
 */
class GVSpan {
 private:
  enum Category {
    Single,
    FullArray,
    FullPointerArray,
  };

  const CPPType *m_type;
  uint m_virtual_size;
  Category m_category;

  union {
    struct {
      const void *data;
    } single;
    struct {
      const void *data;
    } full_array;
    struct {
      const void *const *data;
    } full_pointer_array;
  } m_data;

  GVSpan() = default;

 public:
  GVSpan(const CPPType &type)
  {
    m_type = &type;
    m_virtual_size = 0;
    m_category = FullArray;
    m_data.full_array.data = nullptr;
  }

  GVSpan(GSpan values)
  {
    m_type = &values.type();
    m_virtual_size = values.size();
    m_category = FullArray;
    m_data.full_array.data = values.buffer();
  }

  GVSpan(GMutableSpan values) : GVSpan(GSpan(values))
  {
  }

  template<typename T> GVSpan(Span<T> values) : GVSpan(GSpan(values))
  {
  }

  template<typename T> GVSpan(MutableSpan<T> values) : GVSpan(GSpan(values))
  {
  }

  static GVSpan FromSingle(const CPPType &type, const void *value, uint virtual_size)
  {
    GVSpan ref;
    ref.m_type = &type;
    ref.m_virtual_size = virtual_size;
    ref.m_category = Single;
    ref.m_data.single.data = value;
    return ref;
  }

  static GVSpan FromFullPointerArray(const CPPType &type, const void *const *values, uint size)
  {
    GVSpan ref;
    ref.m_type = &type;
    ref.m_virtual_size = size;
    ref.m_category = FullPointerArray;
    ref.m_data.full_pointer_array.data = values;
    return ref;
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

  const void *operator[](uint index) const
  {
    BLI_assert(index < m_virtual_size);
    switch (m_category) {
      case Single:
        return m_data.single.data;
      case FullArray:
        return POINTER_OFFSET(m_data.full_array.data, index * m_type->size());
      case FullPointerArray:
        return m_data.full_pointer_array.data[index];
    }
    BLI_assert(false);
    return m_data.single.data;
  }

  template<typename T> VSpan<T> typed() const
  {
    BLI_assert(CPPType::get<T>() == *m_type);
    switch (m_category) {
      case Single:
        return VSpan<T>::FromSingle((const T *)m_data.single.data, m_virtual_size);
      case FullArray:
        return VSpan<T>(Span<T>((const T *)m_data.full_array.data, m_virtual_size));
      case FullPointerArray:
        return VSpan<T>(
            Span<const T *>((const T *const *)m_data.full_pointer_array.data, m_virtual_size));
    }
    BLI_assert(false);
    return {};
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_SPANS_HH__ */
