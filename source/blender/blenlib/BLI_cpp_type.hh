/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * The `CPPType` class allows working with arbitrary C++ types in a generic way. An instance of
 * #CPPType wraps exactly one type like `int` or `std::string`.
 *
 * With #CPPType one can write generic data structures and algorithms. That is similar to what C++
 * templates allow. The difference is that when using templates, the types have to be known at
 * compile time and the code has to be instantiated multiple times. On the other hand, when using
 * #CPPType, the data type only has to be known at run-time, and the code only has to be compiled
 * once. Whether #CPPType or classic c++ templates should be used depends on the context:
 * - If the data type is not known at run-time, #CPPType should be used.
 * - If the data type is known to be one of a few, it depends on how performance sensitive the code
 *   is.
 *   - If it it's a small hot loop, a template can be used to optimize for every type (at the
 *     cost of longer compile time, a larger binary and the complexity that comes from using
 *     templates).
 *   - If the code is not performance sensitive, it usually makes sense to use #CPPType instead.
 * - Sometimes a combination can make sense. Optimized code can be generated at compile-time for
 *   some types, while there is a fallback code path using #CPPType for all other types.
 *   #CPPType::to_static_type allows dispatching between both versions based on the type.
 *
 * Under some circumstances, #CPPType serves a similar role as #std::type_info. However, #CPPType
 * has much more utility because it contains methods for actually working with instances of the
 * type.
 *
 * Every type has a size and an alignment. Every function dealing with C++ types in a generic way,
 * has to make sure that alignment rules are followed. The methods provided by a #CPPType instance
 * will check for correct alignment as well.
 *
 * Every type has a name that is for debugging purposes only. It should not be used as identifier.
 *
 * To check if two instances of #CPPType represent the same type, only their pointers have to be
 * compared. Any C++ type has at most one corresponding #CPPType instance.
 *
 * A #CPPType instance comes with many methods that allow dealing with types in a generic way. Most
 * methods come in three variants. Using the default-construct methods as an example:
 *  - `default_construct(void *ptr)`:
 *      Constructs a single instance of that type at the given pointer.
 *  - `default_construct_n(void *ptr, int64_t n)`:
 *      Constructs n instances of that type in an array that starts at the given pointer.
 *  - `default_construct_indices(void *ptr, const IndexMask &mask)`:
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
 * One could also implement CPPType itself using virtual methods and a child class for every
 * wrapped type. However, the approach used now with explicit function pointers to works better.
 * Here are some reasons:
 *  - If CPPType would be inherited once for every used C++ type, we would get a lot of classes
 *    that would only be instanced once each.
 *  - Methods like `default_construct` that operate on a single instance have to be fast. Even this
 *    one necessary indirection using function pointers adds a lot of overhead. If all methods were
 *    virtual, there would be a second level of indirection that increases the overhead even more.
 *  - If it becomes necessary, we could pass the function pointers to C functions more easily than
 *    pointers to virtual member functions.
 */

#include "BLI_hash.hh"
#include "BLI_index_mask.hh"
#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_parameter_pack_utils.hh"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"

/**
 * Different types support different features. Features like copy constructability can be detected
 * automatically easily. For some features this is harder as of C++17. Those have flags in this
 * enum and need to be determined by the programmer.
 */
enum class CPPTypeFlags {
  None = 0,
  Hashable = 1 << 0,
  Printable = 1 << 1,
  EqualityComparable = 1 << 2,

  BasicType = Hashable | Printable | EqualityComparable,
};
ENUM_OPERATORS(CPPTypeFlags, CPPTypeFlags::EqualityComparable)

namespace blender {

class CPPType : NonCopyable, NonMovable {
 private:
  int64_t size_ = 0;
  int64_t alignment_ = 0;
  uintptr_t alignment_mask_ = 0;
  bool is_trivial_ = false;
  bool is_trivially_destructible_ = false;
  bool has_special_member_functions_ = false;

  void (*default_construct_)(void *ptr) = nullptr;
  void (*default_construct_indices_)(void *ptr, const IndexMask &mask) = nullptr;

  void (*value_initialize_)(void *ptr) = nullptr;
  void (*value_initialize_indices_)(void *ptr, const IndexMask &mask) = nullptr;

  void (*destruct_)(void *ptr) = nullptr;
  void (*destruct_indices_)(void *ptr, const IndexMask &mask) = nullptr;

  void (*copy_assign_)(const void *src, void *dst) = nullptr;
  void (*copy_assign_indices_)(const void *src, void *dst, const IndexMask &mask) = nullptr;
  void (*copy_assign_compressed_)(const void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*copy_construct_)(const void *src, void *dst) = nullptr;
  void (*copy_construct_indices_)(const void *src, void *dst, const IndexMask &mask) = nullptr;
  void (*copy_construct_compressed_)(const void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*move_assign_)(void *src, void *dst) = nullptr;
  void (*move_assign_indices_)(void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*move_construct_)(void *src, void *dst) = nullptr;
  void (*move_construct_indices_)(void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*relocate_assign_)(void *src, void *dst) = nullptr;
  void (*relocate_assign_indices_)(void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*relocate_construct_)(void *src, void *dst) = nullptr;
  void (*relocate_construct_indices_)(void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*fill_assign_indices_)(const void *value, void *dst, const IndexMask &mask) = nullptr;

  void (*fill_construct_indices_)(const void *value, void *dst, const IndexMask &mask) = nullptr;

  void (*print_)(const void *value, std::stringstream &ss) = nullptr;
  bool (*is_equal_)(const void *a, const void *b) = nullptr;
  uint64_t (*hash_)(const void *value) = nullptr;

  const void *default_value_ = nullptr;
  std::string debug_name_;

 public:
  template<typename T, CPPTypeFlags Flags>
  CPPType(TypeTag<T> /*type*/, TypeForValue<CPPTypeFlags, Flags> /*flags*/, StringRef debug_name);
  virtual ~CPPType() = default;

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

  /**
   * Get the `CPPType` that corresponds to a specific static type.
   * This only works for types that actually implement the template specialization using
   * `BLI_CPP_TYPE_MAKE`.
   */
  template<typename T> static const CPPType &get()
  {
    /* Store the #CPPType locally to avoid making the function call in most cases. */
    static const CPPType &type = CPPType::get_impl<std::decay_t<T>>();
    return type;
  }
  template<typename T> static const CPPType &get_impl();

  /**
   * Returns the name of the type for debugging purposes. This name should not be used as
   * identifier.
   */
  StringRefNull name() const
  {
    return debug_name_;
  }

  /**
   * Required memory in bytes for an instance of this type.
   *
   * C++ equivalent:
   *   `sizeof(T);`
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
   * When true, the value is like a normal C type, it can be copied around with #memcpy and does
   * not have to be destructed.
   *
   * C++ equivalent:
   *   std::is_trivial_v<T>;
   */
  bool is_trivial() const
  {
    return is_trivial_;
  }

  bool is_default_constructible() const
  {
    return default_construct_ != nullptr;
  }

  bool is_copy_constructible() const
  {
    return copy_assign_ != nullptr;
  }

  bool is_move_constructible() const
  {
    return move_assign_ != nullptr;
  }

  bool is_destructible() const
  {
    return destruct_ != nullptr;
  }

  bool is_copy_assignable() const
  {
    return copy_assign_ != nullptr;
  }

  bool is_move_assignable() const
  {
    return copy_construct_ != nullptr;
  }

  bool is_printable() const
  {
    return print_ != nullptr;
  }

  bool is_equality_comparable() const
  {
    return is_equal_ != nullptr;
  }

  bool is_hashable() const
  {
    return hash_ != nullptr;
  }

  /**
   * Returns true, when the type has the following functions:
   * - Default constructor.
   * - Copy constructor.
   * - Move constructor.
   * - Copy assignment operator.
   * - Move assignment operator.
   * - Destructor.
   */
  bool has_special_member_functions() const
  {
    return has_special_member_functions_;
  }

  /**
   * Returns true, when the given pointer fulfills the alignment requirement of this type.
   */
  bool pointer_has_valid_alignment(const void *ptr) const
  {
    return (uintptr_t(ptr) & alignment_mask_) == 0;
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
  void default_construct(void *ptr) const
  {
    BLI_assert(this->pointer_can_point_to_instance(ptr));

    default_construct_(ptr);
  }

  void default_construct_n(void *ptr, int64_t n) const
  {
    this->default_construct_indices(ptr, IndexMask(n));
  }

  void default_construct_indices(void *ptr, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(ptr));

    default_construct_indices_(ptr, mask);
  }

  /**
   * Same as #default_construct, but does zero initialization for trivial types.
   *
   * C++ equivalent:
   *   new (ptr) T();
   */
  void value_initialize(void *ptr) const
  {
    BLI_assert(this->pointer_can_point_to_instance(ptr));

    value_initialize_(ptr);
  }

  void value_initialize_n(void *ptr, int64_t n) const
  {
    this->value_initialize_indices(ptr, IndexMask(n));
  }

  void value_initialize_indices(void *ptr, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(ptr));

    value_initialize_indices_(ptr, mask);
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
    this->destruct_indices(ptr, IndexMask(n));
  }

  void destruct_indices(void *ptr, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(ptr));

    destruct_indices_(ptr, mask);
  }

  /**
   * Copy an instance of this type from src to dst.
   *
   * C++ equivalent:
   *   dst = src;
   */
  void copy_assign(const void *src, void *dst) const
  {
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    copy_assign_(src, dst);
  }

  void copy_assign_n(const void *src, void *dst, int64_t n) const
  {
    this->copy_assign_indices(src, dst, IndexMask(n));
  }

  void copy_assign_indices(const void *src, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    copy_assign_indices_(src, dst, mask);
  }

  /**
   * Similar to #copy_assign_indices, but does not leave gaps in the #dst array.
   */
  void copy_assign_compressed(const void *src, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    copy_assign_compressed_(src, dst, mask);
  }

  /**
   * Copy an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be uninitialized.
   *
   * C++ equivalent:
   *   new (dst) T(src);
   */
  void copy_construct(const void *src, void *dst) const
  {
    BLI_assert(src != dst || is_trivial_);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    copy_construct_(src, dst);
  }

  void copy_construct_n(const void *src, void *dst, int64_t n) const
  {
    this->copy_construct_indices(src, dst, IndexMask(n));
  }

  void copy_construct_indices(const void *src, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    copy_construct_indices_(src, dst, mask);
  }

  /**
   * Similar to #copy_construct_indices, but does not leave gaps in the #dst array.
   */
  void copy_construct_compressed(const void *src, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    copy_construct_compressed_(src, dst, mask);
  }

  /**
   * Move an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be initialized.
   *
   * C++ equivalent:
   *   dst = std::move(src);
   */
  void move_assign(void *src, void *dst) const
  {
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    move_assign_(src, dst);
  }

  void move_assign_n(void *src, void *dst, int64_t n) const
  {
    this->move_assign_indices(src, dst, IndexMask(n));
  }

  void move_assign_indices(void *src, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    move_assign_indices_(src, dst, mask);
  }

  /**
   * Move an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be uninitialized.
   *
   * C++ equivalent:
   *   new (dst) T(std::move(src));
   */
  void move_construct(void *src, void *dst) const
  {
    BLI_assert(src != dst || is_trivial_);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    move_construct_(src, dst);
  }

  void move_construct_n(void *src, void *dst, int64_t n) const
  {
    this->move_construct_indices(src, dst, IndexMask(n));
  }

  void move_construct_indices(void *src, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    move_construct_indices_(src, dst, mask);
  }

  /**
   * Relocates an instance of this type from src to dst. src will point to uninitialized memory
   * afterwards.
   *
   * C++ equivalent:
   *   dst = std::move(src);
   *   src->~T();
   */
  void relocate_assign(void *src, void *dst) const
  {
    BLI_assert(src != dst || is_trivial_);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    relocate_assign_(src, dst);
  }

  void relocate_assign_n(void *src, void *dst, int64_t n) const
  {
    this->relocate_assign_indices(src, dst, IndexMask(n));
  }

  void relocate_assign_indices(void *src, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    relocate_assign_indices_(src, dst, mask);
  }

  /**
   * Relocates an instance of this type from src to dst. src will point to uninitialized memory
   * afterwards.
   *
   * C++ equivalent:
   *   new (dst) T(std::move(src))
   *   src->~T();
   */
  void relocate_construct(void *src, void *dst) const
  {
    BLI_assert(src != dst || is_trivial_);
    BLI_assert(this->pointer_can_point_to_instance(src));
    BLI_assert(this->pointer_can_point_to_instance(dst));

    relocate_construct_(src, dst);
  }

  void relocate_construct_n(void *src, void *dst, int64_t n) const
  {
    this->relocate_construct_indices(src, dst, IndexMask(n));
  }

  void relocate_construct_indices(void *src, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || src != dst);
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(src));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    relocate_construct_indices_(src, dst, mask);
  }

  /**
   * Copy the given value to the first n elements in an array starting at dst.
   *
   * Other instances of the same type should live in the array before this method is called.
   */
  void fill_assign_n(const void *value, void *dst, int64_t n) const
  {
    this->fill_assign_indices(value, dst, IndexMask(n));
  }

  void fill_assign_indices(const void *value, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(value));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    fill_assign_indices_(value, dst, mask);
  }

  /**
   * Copy the given value to the first n elements in an array starting at dst.
   *
   * The array should be uninitialized before this method is called.
   */
  void fill_construct_n(const void *value, void *dst, int64_t n) const
  {
    this->fill_construct_indices(value, dst, IndexMask(n));
  }

  void fill_construct_indices(const void *value, void *dst, const IndexMask &mask) const
  {
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(value));
    BLI_assert(mask.size() == 0 || this->pointer_can_point_to_instance(dst));

    fill_construct_indices_(value, dst, mask);
  }

  bool can_exist_in_buffer(const int64_t buffer_size, const int64_t buffer_alignment) const
  {
    return size_ <= buffer_size && alignment_ <= buffer_alignment;
  }

  void print(const void *value, std::stringstream &ss) const
  {
    BLI_assert(this->pointer_can_point_to_instance(value));
    print_(value, ss);
  }

  std::string to_string(const void *value) const;

  void print_or_default(const void *value, std::stringstream &ss, StringRef default_value) const;

  bool is_equal(const void *a, const void *b) const
  {
    BLI_assert(this->pointer_can_point_to_instance(a));
    BLI_assert(this->pointer_can_point_to_instance(b));
    return is_equal_(a, b);
  }

  bool is_equal_or_false(const void *a, const void *b) const
  {
    if (this->is_equality_comparable()) {
      return this->is_equal(a, b);
    }
    return false;
  }

  uint64_t hash(const void *value) const
  {
    BLI_assert(this->pointer_can_point_to_instance(value));
    return hash_(value);
  }

  uint64_t hash_or_fallback(const void *value, uint64_t fallback_hash) const
  {
    if (this->is_hashable()) {
      return this->hash(value);
    }
    return fallback_hash;
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
    return get_default_hash(this);
  }

  void (*destruct_fn() const)(void *)
  {
    return destruct_;
  }

  template<typename T> bool is() const
  {
    return this == &CPPType::get<std::decay_t<T>>();
  }

  template<typename... T> bool is_any() const
  {
    return (this->is<T>() || ...);
  }

  /**
   * Convert a #CPPType that is only known at run-time, to a static type that is known at
   * compile-time. This allows the compiler to optimize a function for specific types, while all
   * other types can still use a generic fallback function.
   *
   * \param Types: The types that code should be generated for.
   * \param fn: The function object to call. This is expected to have a templated `operator()` and
   * a non-templated `operator()`. The templated version will be called if the current #CPPType
   *   matches any of the given types. Otherwise, the non-templated function is called.
   */
  template<typename... Types, typename Fn> void to_static_type(const Fn &fn) const
  {
    using Callback = void (*)(const Fn &fn);

    /* Build a lookup table to avoid having to compare the current #CPPType with every type in
     * #Types one after another. */
    static const Map<const CPPType *, Callback> callback_map = []() {
      Map<const CPPType *, Callback> callback_map;
      /* This adds an entry in the map for every type in #Types. */
      (callback_map.add_new(&CPPType::get<Types>(),
                            [](const Fn &fn) {
                              /* Call the templated `operator()` of the given function object. */
                              fn.template operator()<Types>();
                            }),
       ...);
      return callback_map;
    }();

    const Callback callback = callback_map.lookup_default(this, nullptr);
    if (callback != nullptr) {
      callback(fn);
    }
    else {
      /* Call the non-templated `operator()` of the given function object. */
      fn();
    }
  }

 private:
  template<typename Fn> struct TypeTagExecutor {
    const Fn &fn;

    template<typename T> void operator()() const
    {
      fn(TypeTag<T>{});
    }

    void operator()() const
    {
      fn(TypeTag<void>{});
    }
  };

 public:
  /**
   * Similar to #to_static_type but is easier to use with a lambda function. The function is
   * expected to take a single `auto TypeTag` parameter. To extract the static type, use:
   * `using T = typename decltype(TypeTag)::type;`
   *
   * If the current #CPPType is not in #Types, the type tag is `void`.
   */
  template<typename... Types, typename Fn> void to_static_type_tag(const Fn &fn) const
  {
    TypeTagExecutor<Fn> executor{fn};
    this->to_static_type<Types...>(executor);
  }
};

/**
 * Initialize and register basic cpp types.
 */
void register_cpp_types();

}  // namespace blender

/* Utility for allocating an uninitialized buffer for a single value of the given #CPPType. */
#define BUFFER_FOR_CPP_TYPE_VALUE(type, variable_name) \
  blender::DynamicStackBuffer<64, 64> stack_buffer_for_##variable_name((type).size(), \
                                                                       (type).alignment()); \
  void *variable_name = stack_buffer_for_##variable_name.buffer();
