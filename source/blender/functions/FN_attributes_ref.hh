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

#include <optional>

#include "FN_spans.hh"

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector_set.hh"

namespace blender::fn {

class AttributesInfo;

class AttributesInfoBuilder : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  VectorSet<std::string> names_;
  Vector<const CPPType *> types_;
  Vector<void *> defaults_;

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
  LinearAllocator<> allocator_;
  Map<StringRefNull, uint> index_by_name_;
  Vector<StringRefNull> name_by_index_;
  Vector<const CPPType *> type_by_index_;
  Vector<void *> defaults_;

 public:
  AttributesInfo() = default;
  AttributesInfo(const AttributesInfoBuilder &builder);
  ~AttributesInfo();

  uint size() const
  {
    return name_by_index_.size();
  }

  IndexRange index_range() const
  {
    return name_by_index_.index_range();
  }

  StringRefNull name_of(uint index) const
  {
    return name_by_index_[index];
  }

  uint index_of(StringRef name) const
  {
    return index_by_name_.lookup_as(name);
  }

  const void *default_of(uint index) const
  {
    return defaults_[index];
  }

  const void *default_of(StringRef name) const
  {
    return this->default_of(this->index_of(name));
  }

  template<typename T> const T &default_of(uint index) const
  {
    BLI_assert(type_by_index_[index]->is<T>());
    return *(T *)defaults_[index];
  }

  template<typename T> const T &default_of(StringRef name) const
  {
    return this->default_of<T>(this->index_of(name));
  }

  const CPPType &type_of(uint index) const
  {
    return *type_by_index_[index];
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
    return (int)index_by_name_.lookup_default_as(name, -1);
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
  const AttributesInfo *info_;
  Span<void *> buffers_;
  IndexRange range_;

  friend class AttributesRef;

 public:
  MutableAttributesRef(const AttributesInfo &info, Span<void *> buffers, uint size)
      : MutableAttributesRef(info, buffers, IndexRange(size))
  {
  }

  MutableAttributesRef(const AttributesInfo &info, Span<void *> buffers, IndexRange range)
      : info_(&info), buffers_(buffers), range_(range)
  {
  }

  uint size() const
  {
    return range_.size();
  }

  const AttributesInfo &info() const
  {
    return *info_;
  }

  GMutableSpan get(uint index) const
  {
    const CPPType &type = info_->type_of(index);
    void *ptr = POINTER_OFFSET(buffers_[index], type.size() * range_.start());
    return GMutableSpan(type, ptr, range_.size());
  }

  GMutableSpan get(StringRef name) const
  {
    return this->get(info_->index_of(name));
  }

  template<typename T> MutableSpan<T> get(uint index) const
  {
    BLI_assert(info_->type_of(index).is<T>());
    return MutableSpan<T>((T *)buffers_[index] + range_.start(), range_.size());
  }

  template<typename T> MutableSpan<T> get(StringRef name) const
  {
    return this->get<T>(info_->index_of(name));
  }

  std::optional<GMutableSpan> try_get(StringRef name, const CPPType &type) const
  {
    int index = info_->try_index_of(name, type);
    if (index == -1) {
      return {};
    }
    else {
      return this->get((uint)index);
    }
  }

  template<typename T> std::optional<MutableSpan<T>> try_get(StringRef name) const
  {
    int index = info_->try_index_of(name);
    if (index == -1) {
      return {};
    }
    else if (info_->type_of((uint)index).is<T>()) {
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
    return MutableAttributesRef(*info_, buffers_, range_.slice(start, size));
  }
};

class AttributesRef {
 private:
  const AttributesInfo *info_;
  Span<const void *> buffers_;
  IndexRange range_;

 public:
  AttributesRef(const AttributesInfo &info, Span<const void *> buffers, uint size)
      : AttributesRef(info, buffers, IndexRange(size))
  {
  }

  AttributesRef(const AttributesInfo &info, Span<const void *> buffers, IndexRange range)
      : info_(&info), buffers_(buffers), range_(range)
  {
  }

  AttributesRef(MutableAttributesRef attributes)
      : info_(attributes.info_), buffers_(attributes.buffers_), range_(attributes.range_)
  {
  }

  uint size() const
  {
    return range_.size();
  }

  const AttributesInfo &info() const
  {
    return *info_;
  }

  GSpan get(uint index) const
  {
    const CPPType &type = info_->type_of(index);
    const void *ptr = POINTER_OFFSET(buffers_[index], type.size() * range_.start());
    return GSpan(type, ptr, range_.size());
  }

  GSpan get(StringRef name) const
  {
    return this->get(info_->index_of(name));
  }

  template<typename T> Span<T> get(uint index) const
  {
    BLI_assert(info_->type_of(index).is<T>());
    return Span<T>((T *)buffers_[index] + range_.start(), range_.size());
  }

  template<typename T> Span<T> get(StringRef name) const
  {
    return this->get<T>(info_->index_of(name));
  }

  std::optional<GSpan> try_get(StringRef name, const CPPType &type) const
  {
    int index = info_->try_index_of(name, type);
    if (index == -1) {
      return {};
    }
    else {
      return this->get((uint)index);
    }
  }

  template<typename T> std::optional<Span<T>> try_get(StringRef name) const
  {
    int index = info_->try_index_of(name);
    if (index == -1) {
      return {};
    }
    else if (info_->type_of((uint)index).is<T>()) {
      return this->get<T>((uint)index);
    }
    else {
      return {};
    }
  }

  AttributesRef slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  AttributesRef slice(uint start, uint size) const
  {
    return AttributesRef(*info_, buffers_, range_.slice(start, size));
  }
};

}  // namespace blender::fn

#endif /* __FN_ATTRIBUTES_REF_HH__ */
