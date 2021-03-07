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

/** \file
 * \ingroup fn
 *
 * The CPPType class is the core of the runtime-type-system used by the functions system. It can
 * represent C++ types that are default-constructible, destructible, movable, copyable,
 * equality comparable and hashable. In the future we might want to make some of these properties
 * optional.
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
 *  - construct_default_n(void *ptr, int64_t n):
 *      Constructs n instances of that type in an array that starts at the given pointer.
 *  - construct_default_indices(void *ptr, IndexMask mask):
 *      Constructs multiple instances of that type in an array that starts at the given pointer.
 *      Only the indices referenced by `mask` will by constructed.
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
#include "BLI_utility_mixins.hh"

namespace blender::fn {

class CPPType : NonCopyable, NonMovable {
 public:
  using ConstructDefaultF = void (*)(void *ptr);
  using ConstructDefaultNF = void (*)(void *ptr, int64_t n);
  using ConstructDefaultIndicesF = void (*)(void *ptr, IndexMask mask);

  using DestructF = void (*)(void *ptr);
  using DestructNF = void (*)(void *ptr, int64_t n);
  using DestructIndicesF = void (*)(void *ptr, IndexMask mask);

  using CopyToInitializedF = void (*)(const void *src, void *dst);
  using CopyToInitializedNF = void (*)(const void *src, void *dst, int64_t n);
  using CopyToInitializedIndicesF = void (*)(const void *src, void *dst, IndexMask mask);

  using CopyToUninitializedF = void (*)(const void *src, void *dst);
  using CopyToUninitializedNF = void (*)(const void *src, void *dst, int64_t n);
  using CopyToUninitializedIndicesF = void (*)(const void *src, void *dst, IndexMask mask);

  using MoveToInitializedF = void (*)(void *src, void *dst);
  using MoveToInitializedNF = void (*)(void *src, void *dst, int64_t n);
  using MoveToInitializedIndicesF = void (*)(void *src, void *dst, IndexMask mask);

  using MoveToUninitializedF = void (*)(void *src, void *dst);
  using MoveToUninitializedNF = void (*)(void *src, void *dst, int64_t n);
  using MoveToUninitializedIndicesF = void (*)(void *src, void *dst, IndexMask mask);

  using RelocateToInitializedF = void (*)(void *src, void *dst);
  using RelocateToInitializedNF = void (*)(void *src, void *dst, int64_t n);
  using RelocateToInitializedIndicesF = void (*)(void *src, void *dst, IndexMask mask);

  using RelocateToUninitializedF = void (*)(void *src, void *dst);
  using RelocateToUninitializedNF = void (*)(void *src, void *dst, int64_t n);
  using RelocateToUninitializedIndicesF = void (*)(void *src, void *dst, IndexMask mask);

  using FillInitializedF = void (*)(const void *value, void *dst, int64_t n);
  using FillInitializedIndicesF = void (*)(const void *value, void *dst, IndexMask mask);

  using FillUninitializedF = void (*)(const void *value, void *dst, int64_t n);
  using FillUninitializedIndicesF = void (*)(const void *value, void *dst, IndexMask mask);

  using DebugPrintF = void (*)(const void *value, std::stringstream &ss);
  using IsEqualF = bool (*)(const void *a, const void *b);
  using HashF = uint64_t (*)(const void *value);

 private:
  int64_t size_;
  int64_t alignment_;
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

  MoveToInitializedF move_to_initialized_;
  MoveToInitializedNF move_to_initialized_n_;
  MoveToInitializedIndicesF move_to_initialized_indices_;

  MoveToUninitializedF move_to_uninitialized_;
  MoveToUninitializedNF move_to_uninitialized_n_;
  MoveToUninitializedIndicesF move_to_uninitialized_indices_;

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
  IsEqualF is_equal_;
  HashF hash_;

  const void *default_value_;
  std::string name_;

 public:
  CPPType(std::string name,
          int64_t size,
          int64_t alignment,
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
          MoveToInitializedF move_to_initialized,
          MoveToInitializedNF move_to_initialized_n,
          MoveToInitializedIndicesF move_to_initialized_indices,
          MoveToUninitializedF move_to_uninitialized,
          MoveToUninitializedNF move_to_uninitialized_n,
          MoveToUninitializedIndicesF move_to_uninitialized_indices,
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
          IsEqualF is_equal,
          HashF hash,
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
        move_to_initialized_(move_to_initialized),
        move_to_initialized_n_(move_to_initialized_n),
        move_to_initialized_indices_(move_to_initialized_indices),
        move_to_uninitialized_(move_to_uninitialized),
        move_to_uninitialized_n_(move_to_uninitialized_n),
        move_to_uninitialized_indices_(move_to_uninitialized_indices),
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
        is_equal_(is_equal),
        hash_(hash),
        default_value_(default_value),
        name_(name)
  {
    BLI_assert(is_power_of_2_i(alignment_));
    alignment_mask_ = (uintptr_t)alignment_ - (uintptr_t)1;
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
  int64_t size() const
  {
    return size_;
  }

  /**
   * Required memory alignment for an instance of this type.
   *
   * C++ equivalent:
   *   alignof(T);
   */
  int64_t alignment() const
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

  void construct_default_n(void *ptr, int64_t n) const
  {
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(ptr));

    construct_default_n_(ptr, n);
  }

  void construct_default_indices(void *ptr, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(ptr));

    construct_default_indices_(ptr, mask);
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

  void destruct_n(void *ptr, int64_t n) const
  {
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(ptr));

    destruct_n_(ptr, n);
  }

  void destruct_indices(void *ptr, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(ptr));

    destruct_indices_(ptr, mask);
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

  void copy_to_initialized_n(const void *src, void *dst, int64_t n) const
  {
    BLI_assert(n == 0 || src != dst);
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(dst));

    copy_to_initialized_n_(src, dst, n);
  }

  void copy_to_initialized_indices(const void *src, void *dst, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    copy_to_initialized_indices_(src, dst, mask);
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

  void copy_to_uninitialized_n(const void *src, void *dst, int64_t n) const
  {
    BLI_assert(n == 0 || src != dst);
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(dst));

    copy_to_uninitialized_n_(src, dst, n);
  }

  void copy_to_uninitialized_indices(const void *src, void *dst, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    copy_to_uninitialized_indices_(src, dst, mask);
  }

  /**
   * Move an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be initialized.
   *
   * C++ equivalent:
   *   dst = std::move(src);
   */
  void move_to_initialized(void *src, void *dst) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    move_to_initialized_(src, dst);
  }

  void move_to_initialized_n(void *src, void *dst, int64_t n) const
  {
    BLI_assert(n == 0 || src != dst);
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(dst));

    move_to_initialized_n_(src, dst, n);
  }

  void move_to_initialized_indices(void *src, void *dst, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    move_to_initialized_indices_(src, dst, mask);
  }

  /**
   * Move an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be uninitialized.
   *
   * C++ equivalent:
   *   new (dst) T(std::move(src));
   */
  void move_to_uninitialized(void *src, void *dst) const
  {
    BLI_assert(src != dst);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    move_to_uninitialized_(src, dst);
  }

  void move_to_uninitialized_n(void *src, void *dst, int64_t n) const
  {
    BLI_assert(n == 0 || src != dst);
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(dst));

    move_to_uninitialized_n_(src, dst, n);
  }

  void move_to_uninitialized_indices(void *src, void *dst, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    move_to_uninitialized_indices_(src, dst, mask);
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

  void relocate_to_initialized_n(void *src, void *dst, int64_t n) const
  {
    BLI_assert(n == 0 || src != dst);
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(dst));

    relocate_to_initialized_n_(src, dst, n);
  }

  void relocate_to_initialized_indices(void *src, void *dst, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    relocate_to_initialized_indices_(src, dst, mask);
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

  void relocate_to_uninitialized_n(void *src, void *dst, int64_t n) const
  {
    BLI_assert(n == 0 || src != dst);
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(dst));

    relocate_to_uninitialized_n_(src, dst, n);
  }

  void relocate_to_uninitialized_indices(void *src, void *dst, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    relocate_to_uninitialized_indices_(src, dst, mask);
  }

  /**
   * Copy the given value to the first n elements in an array starting at dst.
   *
   * Other instances of the same type should live in the array before this method is called.
   */
  void fill_initialized(const void *value, void *dst, int64_t n) const
  {
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(value));
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(dst));

    fill_initialized_(value, dst, n);
  }

  void fill_initialized_indices(const void *value, void *dst, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(value));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    fill_initialized_indices_(value, dst, mask);
  }

  /**
   * Copy the given value to the first n elements in an array starting at dst.
   *
   * The array should be uninitialized before this method is called.
   */
  void fill_uninitialized(const void *value, void *dst, int64_t n) const
  {
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(value));
    BLI_assert(n == 0 || this->pointer_can_point_to_instance(dst));

    fill_uninitialized_(value, dst, n);
  }

  void fill_uninitialized_indices(const void *value, void *dst, IndexMask mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(value));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    fill_uninitialized_indices_(value, dst, mask);
  }

  void debug_print(const void *value, std::stringstream &ss) const
  {
    BLI_assert(this->pointer_can_point_to_instance(value));
    debug_print_(value, ss);
  }

  bool is_equal(const void *a, const void *b) const
  {
    BLI_assert(this->pointer_can_point_to_instance(a));
    BLI_assert(this->pointer_can_point_to_instance(b));
    return is_equal_(a, b);
  }

  uint64_t hash(const void *value) const
  {
    BLI_assert(this->pointer_can_point_to_instance(value));
    return hash_(value);
  }

  /**
   * Get a pointer to a constant value of this type. The specific value depends on the type.
   * It is usually a zero-initialized or default constructed value.
   */
  const void *default_value() const
  {
    return default_value_;
  }

  uint64_t hash() const
  {
    return DefaultHash<const CPPType *>{}(this);
  }

  template<typename T> bool is() const
  {
    return this == &CPPType::get<T>();
  }
};

/* --------------------------------------------------------------------
 * Utility for creating CPPType instances for C++ types.
 */

namespace cpp_type_util {

template<typename T> void construct_default_cb(void *ptr)
{
  new (ptr) T;
}
template<typename T> void construct_default_n_cb(void *ptr, int64_t n)
{
  blender::default_construct_n(static_cast<T *>(ptr), n);
}
template<typename T> void construct_default_indices_cb(void *ptr, IndexMask mask)
{
  mask.foreach_index([&](int64_t i) { new (static_cast<T *>(ptr) + i) T; });
}

template<typename T> void destruct_cb(void *ptr)
{
  (static_cast<T *>(ptr))->~T();
}
template<typename T> void destruct_n_cb(void *ptr, int64_t n)
{
  blender::destruct_n(static_cast<T *>(ptr), n);
}
template<typename T> void destruct_indices_cb(void *ptr, IndexMask mask)
{
  T *ptr_ = static_cast<T *>(ptr);
  mask.foreach_index([&](int64_t i) { ptr_[i].~T(); });
}

template<typename T> void copy_to_initialized_cb(const void *src, void *dst)
{
  *static_cast<T *>(dst) = *static_cast<const T *>(src);
}
template<typename T> void copy_to_initialized_n_cb(const void *src, void *dst, int64_t n)
{
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    dst_[i] = src_[i];
  }
}
template<typename T>
void copy_to_initialized_indices_cb(const void *src, void *dst, IndexMask mask)
{
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = src_[i]; });
}

template<typename T> void copy_to_uninitialized_cb(const void *src, void *dst)
{
  blender::uninitialized_copy_n(static_cast<const T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void copy_to_uninitialized_n_cb(const void *src, void *dst, int64_t n)
{
  blender::uninitialized_copy_n(static_cast<const T *>(src), n, static_cast<T *>(dst));
}
template<typename T>
void copy_to_uninitialized_indices_cb(const void *src, void *dst, IndexMask mask)
{
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(src_[i]); });
}

template<typename T> void move_to_initialized_cb(void *src, void *dst)
{
  blender::initialized_move_n(static_cast<T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void move_to_initialized_n_cb(void *src, void *dst, int64_t n)
{
  blender::initialized_move_n(static_cast<T *>(src), n, static_cast<T *>(dst));
}
template<typename T> void move_to_initialized_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = std::move(src_[i]); });
}

template<typename T> void move_to_uninitialized_cb(void *src, void *dst)
{
  blender::uninitialized_move_n(static_cast<T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void move_to_uninitialized_n_cb(void *src, void *dst, int64_t n)
{
  blender::uninitialized_move_n(static_cast<T *>(src), n, static_cast<T *>(dst));
}
template<typename T> void move_to_uninitialized_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(std::move(src_[i])); });
}

template<typename T> void relocate_to_initialized_cb(void *src, void *dst)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  *dst_ = std::move(*src_);
  src_->~T();
}
template<typename T> void relocate_to_initialized_n_cb(void *src, void *dst, int64_t n)
{
  blender::initialized_relocate_n(static_cast<T *>(src), n, static_cast<T *>(dst));
}
template<typename T> void relocate_to_initialized_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) {
    dst_[i] = std::move(src_[i]);
    src_[i].~T();
  });
}

template<typename T> void relocate_to_uninitialized_cb(void *src, void *dst)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  new (dst_) T(std::move(*src_));
  src_->~T();
}
template<typename T> void relocate_to_uninitialized_n_cb(void *src, void *dst, int64_t n)
{
  blender::uninitialized_relocate_n(static_cast<T *>(src), n, static_cast<T *>(dst));
}
template<typename T>
void relocate_to_uninitialized_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) {
    new (dst_ + i) T(std::move(src_[i]));
    src_[i].~T();
  });
}

template<typename T> void fill_initialized_cb(const void *value, void *dst, int64_t n)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    dst_[i] = value_;
  }
}
template<typename T> void fill_initialized_indices_cb(const void *value, void *dst, IndexMask mask)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = value_; });
}

template<typename T> void fill_uninitialized_cb(const void *value, void *dst, int64_t n)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    new (dst_ + i) T(value_);
  }
}
template<typename T>
void fill_uninitialized_indices_cb(const void *value, void *dst, IndexMask mask)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(value_); });
}

template<typename T> void debug_print_cb(const void *value, std::stringstream &ss)
{
  const T &value_ = *static_cast<const T *>(value);
  ss << value_;
}

template<typename T> bool is_equal_cb(const void *a, const void *b)
{
  const T &a_ = *static_cast<const T *>(a);
  const T &b_ = *static_cast<const T *>(b);
  return a_ == b_;
}

template<typename T> uint64_t hash_cb(const void *value)
{
  const T &value_ = *static_cast<const T *>(value);
  return DefaultHash<T>{}(value_);
}

}  // namespace cpp_type_util

template<typename T>
inline std::unique_ptr<const CPPType> create_cpp_type(StringRef name, const T &default_value)
{
  using namespace cpp_type_util;
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
                                    move_to_initialized_cb<T>,
                                    move_to_initialized_n_cb<T>,
                                    move_to_initialized_indices_cb<T>,
                                    move_to_uninitialized_cb<T>,
                                    move_to_uninitialized_n_cb<T>,
                                    move_to_uninitialized_indices_cb<T>,
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
                                    is_equal_cb<T>,
                                    hash_cb<T>,
                                    static_cast<const void *>(&default_value));
  return std::unique_ptr<const CPPType>(type);
}

}  // namespace blender::fn

#define MAKE_CPP_TYPE(IDENTIFIER, TYPE_NAME) \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get<TYPE_NAME>() \
  { \
    static TYPE_NAME default_value; \
    static std::unique_ptr<const CPPType> cpp_type = blender::fn::create_cpp_type<TYPE_NAME>( \
        STRINGIFY(IDENTIFIER), default_value); \
    return *cpp_type; \
  } \
  /* Support using `CPPType::get<const T>()`. Otherwise the caller would have to remove const. */ \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get<const TYPE_NAME>() \
  { \
    return blender::fn::CPPType::get<TYPE_NAME>(); \
  }

/* Utility for allocating an uninitialized buffer for a single value of the given #CPPType. */
#define BUFFER_FOR_CPP_TYPE_VALUE(type, variable_name) \
  blender::DynamicStackBuffer<64, 64> stack_buffer_for_##variable_name(type.size(), \
                                                                       type.alignment()); \
  void *variable_name = stack_buffer_for_##variable_name.buffer();
