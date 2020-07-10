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

#ifndef __FN_CPP_TYPE_HH__
#define __FN_CPP_TYPE_HH__

/** \file
 * \ingroup fn
 *
 * The CPPType class is the core of the runtime-type-system used by the functions system. An
 * instance of this class can represent any C++ type, that is default-constructible, destructible,
 * movable and copyable. Therefore it also works for all C types. This restrictions might need to
 * be removed in the future, but for now every required type has these properties.
 *
 * Every type has a size and an alignment. Every function dealing with C++ types in a generic way,
 * has to make sure that alignment rules are followed. The methods provided by a CPPType instance
 * will check for correct alignment as well.
 *
 * Every type has a name that is for debugging purposes only. It should not be used as identifier.
 *
 * To check if two instances of CPPType represent the same type, only their pointers have to be
 * compared. Any C++ type has at most one corresponding CPPType instance.
 *
 * A CPPType instance comes with many methods that allow dealing with types in a generic way. Most
 * methods come in three variants. Using the construct-default methods as example:
 *  - construct_default(void *ptr):
 *      Constructs a single instance of that type at the given pointer.
 *  - construct_default_n(void *ptr, uint n):
 *      Constructs n instances of that type in an array that starts at the given pointer.
 *  - construct_default_indices(void *ptr, IndexMask index_mask):
 *      Constructs multiple instances of that type in an array that starts at the given pointer.
 *      Only the indices referenced by `index_mask` will by constructed.
 *
 * In some cases default-construction does nothing (e.g. for trivial types like int). The
 * `default_value` method provides some default value anyway that can be copied instead. What the
 * default value is, depends on the type. Usually it is something like 0 or an empty string.
 *
 *
 * Implementation Considerations
 * -----------------------------
 *
 * Concepts like inheritance are currently not captured by this system. This is not because it is
 * not possible, but because it was not necessary to add this complexity yet.
 *
 * One could also implement CPPType itself using virtual inheritance. However, I found the approach
 * used now with explicit function pointers to work better. Here are some reasons:
 *  - If CPPType would be inherited once for every used C++ type, we would get a lot of classes
 *    that would only be instanced once each.
 *  - Methods like `construct_default` that operate on a single instance have to be fast. Even this
 *    one necessary indirection using function pointers adds a lot of overhead. If all methods were
 *    virtual, there would be a second level of indirection that increases the overhead even more.
 *  - If it becomes necessary, we could pass the function pointers to C functions more easily than
 *    pointers to virtual member functions.
 */

#include "BLI_hash.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_base.h"
#include "BLI_string_ref.hh"

namespace blender::fn {

class CPPType {
 public:
  using ConstructDefaultF = void (*)(void *ptr);
  using ConstructDefaultNF = void (*)(void *ptr, uint n);
  using ConstructDefaultIndicesF = void (*)(void *ptr, IndexMask index_mask);

  using DestructF = void (*)(void *ptr);
  using DestructNF = void (*)(void *ptr, uint n);
  using DestructIndicesF = void (*)(void *ptr, IndexMask index_mask);

  using CopyToInitializedF = void (*)(const void *src, void *dst);
  using CopyToInitializedNF = void (*)(const void *src, void *dst, uint n);
  using CopyToInitializedIndicesF = void (*)(const void *src, void *dst, IndexMask index_mask);

  using CopyToUninitializedF = void (*)(const void *src, void *dst);
  using CopyToUninitializedNF = void (*)(const void *src, void *dst, uint n);
  using CopyToUninitializedIndicesF = void (*)(const void *src, void *dst, IndexMask index_mask);

  using RelocateToInitializedF = void (*)(void *src, void *dst);
  using RelocateToInitializedNF = void (*)(void *src, void *dst, uint n);
  using RelocateToInitializedIndicesF = void (*)(void *src, void *dst, IndexMask index_mask);

  using RelocateToUninitializedF = void (*)(void *src, void *dst);
  using RelocateToUninitializedNF = void (*)(void *src, void *dst, uint n);
  using RelocateToUninitializedIndicesF = void (*)(void *src, void *dst, IndexMask index_mask);

  using FillInitializedF = void (*)(const void *value, void *dst, uint n);
  using FillInitializedIndicesF = void (*)(const void *value, void *dst, IndexMask index_mask);

  using FillUninitializedF = void (*)(const void *value, void *dst, uint n);
  using FillUninitializedIndicesF = void (*)(const void *value, void *dst, IndexMask index_mask);

  using DebugPrintF = void (*)(const void *value, std::stringstream &ss);

  CPPType(std::string name,
          uint size,
          uint alignment,
          bool is_trivially_destructible,
          ConstructDefaultF construct_default,
          ConstructDefaultNF construct_default_n,
          ConstructDefaultIndicesF construct_default_indices,
          DestructF destruct,
          DestructNF destruct_n,
          DestructIndicesF destruct_indices,
          CopyToInitializedF copy_to_initialized,
          CopyToInitializedNF copy_to_initialized_n,
          CopyToInitializedIndicesF copy_to_initialized_indices,
          CopyToUninitializedF copy_to_uninitialized,
          CopyToUninitializedNF copy_to_uninitialized_n,
          CopyToUninitializedIndicesF copy_to_uninitialized_indices,
          RelocateToInitializedF relocate_to_initialized,
          RelocateToInitializedNF relocate_to_initialized_n,
          RelocateToInitializedIndicesF relocate_to_initialized_indices,
          RelocateToUninitializedF relocate_to_uninitialized,
          RelocateToUninitializedNF relocate_to_uninitialized_n,
          RelocateToUninitializedIndicesF relocate_to_uninitialized_indices,
          FillInitializedF fill_initialized,
          FillInitializedIndicesF fill_initialized_indices,
          FillUninitializedF fill_uninitialized,
          FillUninitializedIndicesF fill_uninitialized_indices,
          DebugPrintF debug_print,
          const void *default_value)
      : size_(size),
        alignment_(alignment),
        is_trivially_destructible_(is_trivially_destructible),
        construct_default_(construct_default),
        construct_default_n_(construct_default_n),
        construct_default_indices_(construct_default_indices),
        destruct_(destruct),
        destruct_n_(destruct_n),
        destruct_indices_(destruct_indices),
        copy_to_initialized_(copy_to_initialized),
        copy_to_initialized_n_(copy_to_initialized_n),
        copy_to_initialized_indices_(copy_to_initialized_indices),
        copy_to_uninitialized_(copy_to_uninitialized),
        copy_to_uninitialized_n_(copy_to_uninitialized_n),
        copy_to_uninitialized_indices_(copy_to_uninitialized_indices),
        relocate_to_initialized_(relocate_to_initialized),
        relocate_to_initialized_n_(relocate_to_initialized_n),
        relocate_to_initialized_indices_(relocate_to_initialized_indices),
        relocate_to_uninitialized_(relocate_to_uninitialized),
        relocate_to_uninitialized_n_(relocate_to_uninitialized_n),
        relocate_to_uninitialized_indices_(relocate_to_uninitialized_indices),
        fill_initialized_(fill_initialized),
        fill_initialized_indices_(fill_initialized_indices),
        fill_uninitialized_(fill_uninitialized),
        fill_uninitialized_indices_(fill_uninitialized_indices),
        debug_print_(debug_print),
        default_value_(default_value),
        name_(name)
  {
    BLI_assert(is_power_of_2_i(alignment_));
    alignment_mask_ = (uintptr_t)alignment_ - (uintptr_t)1;
  }

  /**
   * Returns the name of the type for debugging purposes. This name should not be used as
   * identifier.
   */
  StringRefNull name() const
  {
    return name_;
  }

  /**
   * Required memory in bytes for an instance of this type.
   *
   * C++ equivalent:
   *   sizeof(T);
   */
  uint size() const
  {
    return size_;
  }

  /**
   * Required memory alignment for an instance of this type.
   *
   * C++ equivalent:
   *   alignof(T);
   */
  uint alignment() const
  {
    return alignment_;
  }

  /**
   * When true, the destructor does not have to be called on this type. This can sometimes be used
   * for optimization purposes.
   *
   * C++ equivalent:
   *   std::is_trivially_destructible_v<T>;
   */
  bool is_trivially_destructible() const
  {
    return is_trivially_destructible_;
  }

  /**
   * Returns true, when the given pointer fulfills the alignment requirement of this type.
   */
  bool pointer_has_valid_alignment(const void *ptr) const
  {
    return ((uintptr_t)ptr & alignment_mask_) == 0;
  }

  bool pointer_can_point_to_instance(const void *ptr) const
  {
    return ptr != nullptr && pointer_has_valid_alignment(ptr);
  }

  /**
   * Call the default constructor at the given memory location.
   * The memory should be uninitialized before this method is called.
   * For some trivial types (like int), this method does nothing.
   *
   * C++ equivalent:
   *   new (ptr) T;
   */
  void construct_default(void *ptr) const
  {
    BLI_assert(this->pointer_can_point_to_instance(ptr));

    construct_default_(ptr);
  }

  void construct_default_n(void *ptr, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    construct_default_n_(ptr, n);
  }

  void construct_default_indices(void *ptr, IndexMask index_mask) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    construct_default_indices_(ptr, index_mask);
  }

  /**
   * Call the destructor on the given instance of this type. The pointer must not be nullptr.
   *
   * For some trivial types, this does nothing.
   *
   * C++ equivalent:
   *   ptr->~T();
   */
  void destruct(void *ptr) const
  {
    BLI_assert(this->pointer_can_point_to_instance(ptr));

    destruct_(ptr);
  }

  void destruct_n(void *ptr, uint n) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    destruct_n_(ptr, n);
  }

  void destruct_indices(void *ptr, IndexMask index_mask) const
  {
    BLI_assert(this->pointer_has_valid_alignment(ptr));

    destruct_indices_(ptr, index_mask);
  }

  DestructF destruct_cb() const
  {
    return destruct_;
  }

  /**
   * Copy an instance of this type from src to dst.
   *
   * C++ equivalent:
   *   dst = src;
   */
  void copy_to_initialized(const void *src, void *dst) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    copy_to_initialized_(src, dst);
  }

  void copy_to_initialized_n(const void *src, void *dst, uint n) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    copy_to_initialized_n_(src, dst, n);
  }

  void copy_to_initialized_indices(const void *src, void *dst, IndexMask index_mask) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    copy_to_initialized_indices_(src, dst, index_mask);
  }

  /**
   * Copy an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be uninitialized.
   *
   * C++ equivalent:
   *   new (dst) T(src);
   */
  void copy_to_uninitialized(const void *src, void *dst) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    copy_to_uninitialized_(src, dst);
  }

  void copy_to_uninitialized_n(const void *src, void *dst, uint n) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    copy_to_uninitialized_n_(src, dst, n);
  }

  void copy_to_uninitialized_indices(const void *src, void *dst, IndexMask index_mask) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    copy_to_uninitialized_indices_(src, dst, index_mask);
  }

  /**
   * Relocates an instance of this type from src to dst. src will point to uninitialized memory
   * afterwards.
   *
   * C++ equivalent:
   *   dst = std::move(src);
   *   src->~T();
   */
  void relocate_to_initialized(void *src, void *dst) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    relocate_to_initialized_(src, dst);
  }

  void relocate_to_initialized_n(void *src, void *dst, uint n) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    relocate_to_initialized_n_(src, dst, n);
  }

  void relocate_to_initialized_indices(void *src, void *dst, IndexMask index_mask) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    relocate_to_initialized_indices_(src, dst, index_mask);
  }

  /**
   * Relocates an instance of this type from src to dst. src will point to uninitialized memory
   * afterwards.
   *
   * C++ equivalent:
   *   new (dst) T(std::move(src))
   *   src->~T();
   */
  void relocate_to_uninitialized(void *src, void *dst) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    relocate_to_uninitialized_(src, dst);
  }

  void relocate_to_uninitialized_n(void *src, void *dst, uint n) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    relocate_to_uninitialized_n_(src, dst, n);
  }

  void relocate_to_uninitialized_indices(void *src, void *dst, IndexMask index_mask) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_has_valid_alignment(src));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    relocate_to_uninitialized_indices_(src, dst, index_mask);
  }

  /**
   * Copy the given value to the first n elements in an array starting at dst.
   *
   * Other instances of the same type should live in the array before this method is called.
   */
  void fill_initialized(const void *value, void *dst, uint n) const
  {
    BLI_assert(this->pointer_can_point_to_instance(value));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    fill_initialized_(value, dst, n);
  }

  void fill_initialized_indices(const void *value, void *dst, IndexMask index_mask) const
  {
    BLI_assert(this->pointer_has_valid_alignment(value));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    fill_initialized_indices_(value, dst, index_mask);
  }

  /**
   * Copy the given value to the first n elements in an array starting at dst.
   *
   * The array should be uninitialized before this method is called.
   */
  void fill_uninitialized(const void *value, void *dst, uint n) const
  {
    BLI_assert(this->pointer_can_point_to_instance(value));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    fill_uninitialized_(value, dst, n);
  }

  void fill_uninitialized_indices(const void *value, void *dst, IndexMask index_mask) const
  {
    BLI_assert(this->pointer_has_valid_alignment(value));
    BLI_assert(this->pointer_has_valid_alignment(dst));

    fill_uninitialized_indices_(value, dst, index_mask);
  }

  void debug_print(const void *value, std::stringstream &ss) const
  {
    BLI_assert(this->pointer_can_point_to_instance(value));
    debug_print_(value, ss);
  }

  /**
   * Get a pointer to a constant value of this type. The specific value depends on the type.
   * It is usually a zero-initialized or default constructed value.
   */
  const void *default_value() const
  {
    return default_value_;
  }

  uint32_t hash() const
  {
    return DefaultHash<const CPPType *>{}(this);
  }

  /**
   * Two types only compare equal when their pointer is equal. No two instances of CPPType for the
   * same C++ type should be created.
   */
  friend bool operator==(const CPPType &a, const CPPType &b)
  {
    return &a == &b;
  }

  friend bool operator!=(const CPPType &a, const CPPType &b)
  {
    return !(&a == &b);
  }

  template<typename T> static const CPPType &get();

  template<typename T> bool is() const
  {
    return this == &CPPType::get<T>();
  }

 private:
  uint size_;
  uint alignment_;
  uintptr_t alignment_mask_;
  bool is_trivially_destructible_;

  ConstructDefaultF construct_default_;
  ConstructDefaultNF construct_default_n_;
  ConstructDefaultIndicesF construct_default_indices_;

  DestructF destruct_;
  DestructNF destruct_n_;
  DestructIndicesF destruct_indices_;

  CopyToInitializedF copy_to_initialized_;
  CopyToInitializedNF copy_to_initialized_n_;
  CopyToInitializedIndicesF copy_to_initialized_indices_;

  CopyToUninitializedF copy_to_uninitialized_;
  CopyToUninitializedNF copy_to_uninitialized_n_;
  CopyToUninitializedIndicesF copy_to_uninitialized_indices_;

  RelocateToInitializedF relocate_to_initialized_;
  RelocateToInitializedNF relocate_to_initialized_n_;
  RelocateToInitializedIndicesF relocate_to_initialized_indices_;

  RelocateToUninitializedF relocate_to_uninitialized_;
  RelocateToUninitializedNF relocate_to_uninitialized_n_;
  RelocateToUninitializedIndicesF relocate_to_uninitialized_indices_;

  FillInitializedF fill_initialized_;
  FillInitializedIndicesF fill_initialized_indices_;

  FillUninitializedF fill_uninitialized_;
  FillUninitializedIndicesF fill_uninitialized_indices_;

  DebugPrintF debug_print_;

  const void *default_value_;
  std::string name_;
};

/* --------------------------------------------------------------------
 * Utility for creating CPPType instances for C++ types.
 */

namespace CPPTypeUtil {

template<typename T> void construct_default_cb(void *ptr)
{
  new (ptr) T;
}
template<typename T> void construct_default_n_cb(void *ptr, uint n)
{
  blender::default_construct_n((T *)ptr, n);
}
template<typename T> void construct_default_indices_cb(void *ptr, IndexMask index_mask)
{
  index_mask.foreach_index([&](uint i) { new ((T *)ptr + i) T; });
}

template<typename T> void destruct_cb(void *ptr)
{
  ((T *)ptr)->~T();
}
template<typename T> void destruct_n_cb(void *ptr, uint n)
{
  blender::destruct_n((T *)ptr, n);
}
template<typename T> void destruct_indices_cb(void *ptr, IndexMask index_mask)
{
  T *ptr_ = (T *)ptr;
  index_mask.foreach_index([&](uint i) { ptr_[i].~T(); });
}

template<typename T> void copy_to_initialized_cb(const void *src, void *dst)
{
  *(T *)dst = *(T *)src;
}
template<typename T> void copy_to_initialized_n_cb(const void *src, void *dst, uint n)
{
  const T *src_ = (const T *)src;
  T *dst_ = (T *)dst;

  for (uint i = 0; i < n; i++) {
    dst_[i] = src_[i];
  }
}
template<typename T>
void copy_to_initialized_indices_cb(const void *src, void *dst, IndexMask index_mask)
{
  const T *src_ = (const T *)src;
  T *dst_ = (T *)dst;

  index_mask.foreach_index([&](uint i) { dst_[i] = src_[i]; });
}

template<typename T> void copy_to_uninitialized_cb(const void *src, void *dst)
{
  blender::uninitialized_copy_n((T *)src, 1, (T *)dst);
}
template<typename T> void copy_to_uninitialized_n_cb(const void *src, void *dst, uint n)
{
  blender::uninitialized_copy_n((T *)src, n, (T *)dst);
}
template<typename T>
void copy_to_uninitialized_indices_cb(const void *src, void *dst, IndexMask index_mask)
{
  const T *src_ = (const T *)src;
  T *dst_ = (T *)dst;

  index_mask.foreach_index([&](uint i) { new (dst_ + i) T(src_[i]); });
}

template<typename T> void relocate_to_initialized_cb(void *src, void *dst)
{
  T *src_ = (T *)src;
  T *dst_ = (T *)dst;

  *dst_ = std::move(*src_);
  src_->~T();
}
template<typename T> void relocate_to_initialized_n_cb(void *src, void *dst, uint n)
{
  blender::initialized_relocate_n((T *)src, n, (T *)dst);
}
template<typename T>
void relocate_to_initialized_indices_cb(void *src, void *dst, IndexMask index_mask)
{
  T *src_ = (T *)src;
  T *dst_ = (T *)dst;

  index_mask.foreach_index([&](uint i) {
    dst_[i] = std::move(src_[i]);
    src_[i].~T();
  });
}

template<typename T> void relocate_to_uninitialized_cb(void *src, void *dst)
{
  T *src_ = (T *)src;
  T *dst_ = (T *)dst;

  new (dst_) T(std::move(*src_));
  src_->~T();
}
template<typename T> void relocate_to_uninitialized_n_cb(void *src, void *dst, uint n)
{
  blender::uninitialized_relocate_n((T *)src, n, (T *)dst);
}
template<typename T>
void relocate_to_uninitialized_indices_cb(void *src, void *dst, IndexMask index_mask)
{
  T *src_ = (T *)src;
  T *dst_ = (T *)dst;

  index_mask.foreach_index([&](uint i) {
    new (dst_ + i) T(std::move(src_[i]));
    src_[i].~T();
  });
}

template<typename T> void fill_initialized_cb(const void *value, void *dst, uint n)
{
  const T &value_ = *(const T *)value;
  T *dst_ = (T *)dst;

  for (uint i = 0; i < n; i++) {
    dst_[i] = value_;
  }
}
template<typename T>
void fill_initialized_indices_cb(const void *value, void *dst, IndexMask index_mask)
{
  const T &value_ = *(const T *)value;
  T *dst_ = (T *)dst;

  index_mask.foreach_index([&](uint i) { dst_[i] = value_; });
}

template<typename T> void fill_uninitialized_cb(const void *value, void *dst, uint n)
{
  const T &value_ = *(const T *)value;
  T *dst_ = (T *)dst;

  for (uint i = 0; i < n; i++) {
    new (dst_ + i) T(value_);
  }
}
template<typename T>
void fill_uninitialized_indices_cb(const void *value, void *dst, IndexMask index_mask)
{
  const T &value_ = *(const T *)value;
  T *dst_ = (T *)dst;

  index_mask.foreach_index([&](uint i) { new (dst_ + i) T(value_); });
}

template<typename T> void debug_print_cb(const void *value, std::stringstream &ss)
{
  const T &value_ = *(const T *)value;
  ss << value_;
}

}  // namespace CPPTypeUtil

template<typename T>
static std::unique_ptr<const CPPType> create_cpp_type(StringRef name, const T &default_value)
{
  using namespace CPPTypeUtil;
  const CPPType *type = new CPPType(name,
                                    sizeof(T),
                                    alignof(T),
                                    std::is_trivially_destructible_v<T>,
                                    construct_default_cb<T>,
                                    construct_default_n_cb<T>,
                                    construct_default_indices_cb<T>,
                                    destruct_cb<T>,
                                    destruct_n_cb<T>,
                                    destruct_indices_cb<T>,
                                    copy_to_initialized_cb<T>,
                                    copy_to_initialized_n_cb<T>,
                                    copy_to_initialized_indices_cb<T>,
                                    copy_to_uninitialized_cb<T>,
                                    copy_to_uninitialized_n_cb<T>,
                                    copy_to_uninitialized_indices_cb<T>,
                                    relocate_to_initialized_cb<T>,
                                    relocate_to_initialized_n_cb<T>,
                                    relocate_to_initialized_indices_cb<T>,
                                    relocate_to_uninitialized_cb<T>,
                                    relocate_to_uninitialized_n_cb<T>,
                                    relocate_to_uninitialized_indices_cb<T>,
                                    fill_initialized_cb<T>,
                                    fill_initialized_indices_cb<T>,
                                    fill_uninitialized_cb<T>,
                                    fill_uninitialized_indices_cb<T>,
                                    debug_print_cb<T>,
                                    (const void *)&default_value);
  return std::unique_ptr<const CPPType>(type);
}

}  // namespace blender::fn

#define MAKE_CPP_TYPE(IDENTIFIER, TYPE_NAME) \
  static TYPE_NAME default_value_##IDENTIFIER; \
  static std::unique_ptr<const blender::fn::CPPType> CPPTYPE_##IDENTIFIER##_owner = \
      blender::fn::create_cpp_type<TYPE_NAME>(STRINGIFY(IDENTIFIER), default_value_##IDENTIFIER); \
  const blender::fn::CPPType &CPPType_##IDENTIFIER = *CPPTYPE_##IDENTIFIER##_owner; \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get<TYPE_NAME>() \
  { \
    /* This can happen when trying to access a CPPType during static storage initialization. */ \
    BLI_assert(CPPTYPE_##IDENTIFIER##_owner.get() != nullptr); \
    return CPPType_##IDENTIFIER; \
  }

#endif /* __FN_CPP_TYPE_HH__ */
