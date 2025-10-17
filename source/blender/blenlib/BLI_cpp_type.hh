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

#include "BLI_enum_flags.hh"
#include "BLI_hash.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_map.hh"
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
  IdentityDefaultValue = 1 << 3,

  BasicType = Hashable | Printable | EqualityComparable,
};
ENUM_OPERATORS(CPPTypeFlags)

namespace blender {

class CPPType : NonCopyable, NonMovable {
 public:
  /**
   * Required memory in bytes for an instance of this type.
   *
   * C++ equivalent:
   *   `sizeof(T);`
   */
  int64_t size = 0;

  /**
   * Required memory alignment for an instance of this type.
   *
   * C++ equivalent:
   *   `alignof(T);`
   */
  int64_t alignment = 0;

  /**
   * When true, the value is like a normal C type, it can be copied around with #memcpy and does
   * not have to be destructed.
   *
   * C++ equivalent:
   *   `std::is_trivial_v<T>;`
   */
  bool is_trivial = false;

  /**
   * When true, the destructor does not have to be called on this type. This can sometimes be used
   * for optimization purposes.
   *
   * C++ equivalent:
   *   `std::is_trivially_destructible_v<T>;`
   */
  bool is_trivially_destructible = false;

  /**
   * Returns true, when the type has the following functions:
   * - Default constructor.
   * - Copy constructor.
   * - Move constructor.
   * - Copy assignment operator.
   * - Move assignment operator.
   * - Destructor.
   */
  bool has_special_member_functions = false;

  bool is_default_constructible = false;
  bool is_copy_constructible = false;
  bool is_move_constructible = false;
  bool is_destructible = false;
  bool is_copy_assignable = false;
  bool is_move_assignable = false;

 private:
  uintptr_t alignment_mask_ = 0;

  void (*default_construct_)(void *ptr) = nullptr;
  void (*default_construct_n_)(void *ptr, int64_t n) = nullptr;
  void (*default_construct_indices_)(void *ptr, const IndexMask &mask) = nullptr;

  void (*value_initialize_)(void *ptr) = nullptr;
  void (*value_initialize_n_)(void *ptr, int64_t n) = nullptr;
  void (*value_initialize_indices_)(void *ptr, const IndexMask &mask) = nullptr;

  void (*destruct_)(void *ptr) = nullptr;
  void (*destruct_n_)(void *ptr, int64_t n) = nullptr;
  void (*destruct_indices_)(void *ptr, const IndexMask &mask) = nullptr;

  void (*copy_assign_)(const void *src, void *dst) = nullptr;
  void (*copy_assign_n_)(const void *src, void *dst, int64_t n) = nullptr;
  void (*copy_assign_indices_)(const void *src, void *dst, const IndexMask &mask) = nullptr;
  void (*copy_assign_compressed_)(const void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*copy_construct_)(const void *src, void *dst) = nullptr;
  void (*copy_construct_n_)(const void *src, void *dst, int64_t n) = nullptr;
  void (*copy_construct_indices_)(const void *src, void *dst, const IndexMask &mask) = nullptr;
  void (*copy_construct_compressed_)(const void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*move_assign_)(void *src, void *dst) = nullptr;
  void (*move_assign_n_)(void *src, void *dst, int64_t n) = nullptr;
  void (*move_assign_indices_)(void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*move_construct_)(void *src, void *dst) = nullptr;
  void (*move_construct_n_)(void *src, void *dst, int64_t n) = nullptr;
  void (*move_construct_indices_)(void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*relocate_assign_)(void *src, void *dst) = nullptr;
  void (*relocate_assign_n_)(void *src, void *dst, int64_t n) = nullptr;
  void (*relocate_assign_indices_)(void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*relocate_construct_)(void *src, void *dst) = nullptr;
  void (*relocate_construct_n_)(void *src, void *dst, int64_t n) = nullptr;
  void (*relocate_construct_indices_)(void *src, void *dst, const IndexMask &mask) = nullptr;

  void (*fill_assign_n_)(const void *value, void *dst, int64_t n) = nullptr;
  void (*fill_assign_indices_)(const void *value, void *dst, const IndexMask &mask) = nullptr;

  void (*fill_construct_n_)(const void *value, void *dst, int64_t n) = nullptr;
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
   * Get the `CPPType` that corresponds to a specific static type.
   * This only works for types that actually implement the template specialization using
   * `BLI_CPP_TYPE_MAKE`.
   */
  template<typename T> static const CPPType &get();
  template<typename T> static const CPPType &get_impl();

  /**
   * Returns the name of the type for debugging purposes. This name should not be used as
   * identifier.
   */
  StringRefNull name() const;

  bool is_printable() const;
  bool is_equality_comparable() const;
  bool is_hashable() const;

  /**
   * Returns true, when the given pointer fulfills the alignment requirement of this type.
   */
  bool pointer_has_valid_alignment(const void *ptr) const;
  bool pointer_can_point_to_instance(const void *ptr) const;

  /**
   * Call the default constructor at the given memory location.
   * The memory should be uninitialized before this method is called.
   * For some trivial types (like int), this method does nothing.
   *
   * C++ equivalent:
   *   `new (ptr) T;`
   */
  void default_construct(void *ptr) const;
  void default_construct_n(void *ptr, int64_t n) const;
  void default_construct_indices(void *ptr, const IndexMask &mask) const;

  /**
   * Same as #default_construct, but does zero initialization for trivial types.
   *
   * C++ equivalent:
   *   `new (ptr) T();`
   */
  void value_initialize(void *ptr) const;
  void value_initialize_n(void *ptr, int64_t n) const;
  void value_initialize_indices(void *ptr, const IndexMask &mask) const;

  /**
   * Call the destructor on the given instance of this type. The pointer must not be nullptr.
   *
   * For some trivial types, this does nothing.
   *
   * C++ equivalent:
   *   `ptr->~T();`
   */
  void destruct(void *ptr) const;
  void destruct_n(void *ptr, int64_t n) const;
  void destruct_indices(void *ptr, const IndexMask &mask) const;

  /**
   * Copy an instance of this type from src to dst.
   *
   * C++ equivalent:
   *   `dst = src;`
   */
  void copy_assign(const void *src, void *dst) const;
  void copy_assign_n(const void *src, void *dst, int64_t n) const;
  void copy_assign_indices(const void *src, void *dst, const IndexMask &mask) const;

  /**
   * Similar to #copy_assign_indices, but does not leave gaps in the #dst array.
   */
  void copy_assign_compressed(const void *src, void *dst, const IndexMask &mask) const;

  /**
   * Copy an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be uninitialized.
   *
   * C++ equivalent:
   *   `new (dst) T(src);`
   */
  void copy_construct(const void *src, void *dst) const;
  void copy_construct_n(const void *src, void *dst, int64_t n) const;
  void copy_construct_indices(const void *src, void *dst, const IndexMask &mask) const;

  /**
   * Similar to #copy_construct_indices, but does not leave gaps in the #dst array.
   */
  void copy_construct_compressed(const void *src, void *dst, const IndexMask &mask) const;

  /**
   * Move an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be initialized.
   *
   * C++ equivalent:
   *   `dst = std::move(src);`
   */
  void move_assign(void *src, void *dst) const;
  void move_assign_n(void *src, void *dst, int64_t n) const;
  void move_assign_indices(void *src, void *dst, const IndexMask &mask) const;

  /**
   * Move an instance of this type from src to dst.
   *
   * The memory pointed to by dst should be uninitialized.
   *
   * C++ equivalent:
   *   `new (dst) T(std::move(src));`
   */
  void move_construct(void *src, void *dst) const;
  void move_construct_n(void *src, void *dst, int64_t n) const;
  void move_construct_indices(void *src, void *dst, const IndexMask &mask) const;

  /**
   * Relocates an instance of this type from src to dst. src will point to uninitialized memory
   * afterwards.
   *
   * C++ equivalent:
   *   `dst = std::move(src);`
   *   `src->~T();`
   */
  void relocate_assign(void *src, void *dst) const;
  void relocate_assign_n(void *src, void *dst, int64_t n) const;
  void relocate_assign_indices(void *src, void *dst, const IndexMask &mask) const;

  /**
   * Relocates an instance of this type from src to dst. src will point to uninitialized memory
   * afterwards.
   *
   * C++ equivalent:
   *   `new (dst) T(std::move(src))`
   *   `src->~T();`
   */
  void relocate_construct(void *src, void *dst) const;
  void relocate_construct_n(void *src, void *dst, int64_t n) const;
  void relocate_construct_indices(void *src, void *dst, const IndexMask &mask) const;

  /**
   * Copy the given value to the first n elements in an array starting at dst.
   *
   * Other instances of the same type should live in the array before this method is called.
   */
  void fill_assign_n(const void *value, void *dst, int64_t n) const;
  void fill_assign_indices(const void *value, void *dst, const IndexMask &mask) const;

  /**
   * Copy the given value to the first n elements in an array starting at dst.
   *
   * The array should be uninitialized before this method is called.
   */
  void fill_construct_n(const void *value, void *dst, int64_t n) const;
  void fill_construct_indices(const void *value, void *dst, const IndexMask &mask) const;

  bool can_exist_in_buffer(const int64_t buffer_size, const int64_t buffer_alignment) const;

  void print(const void *value, std::stringstream &ss) const;
  std::string to_string(const void *value) const;
  void print_or_default(const void *value, std::stringstream &ss, StringRef default_value) const;

  bool is_equal(const void *a, const void *b) const;
  bool is_equal_or_false(const void *a, const void *b) const;

  uint64_t hash(const void *value) const;
  uint64_t hash_or_fallback(const void *value, uint64_t fallback_hash) const;

  /**
   * Get a pointer to a constant value of this type. The specific value depends on the type.
   * It is usually a zero-initialized or default constructed value.
   */
  const void *default_value() const;

  uint64_t hash() const;

  void (*destruct_fn() const)(void *);

  template<typename T> bool is() const;

  template<typename... T> bool is_any() const;

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
  template<typename... Types, typename Fn> void to_static_type(const Fn &fn) const;

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
  blender::DynamicStackBuffer<64, 64> stack_buffer_for_##variable_name((type).size, \
                                                                       (type).alignment); \
  void *variable_name = stack_buffer_for_##variable_name.buffer();

namespace blender {

/* Give a compile error instead of a link error when type information is missing. */
template<> const CPPType &CPPType::get_impl<void>() = delete;

/**
 * Two types only compare equal when their pointer is equal. No two instances of CPPType for the
 * same C++ type should be created.
 */
inline bool operator==(const CPPType &a, const CPPType &b)
{
  return &a == &b;
}

inline bool operator!=(const CPPType &a, const CPPType &b)
{
  return !(&a == &b);
}

template<typename T> inline const CPPType &CPPType::get()
{
  /* Store the #CPPType locally to avoid making the function call in most cases. */
  static const CPPType &type = CPPType::get_impl<std::decay_t<T>>();
  return type;
}

inline StringRefNull CPPType::name() const
{
  return debug_name_;
}

inline bool CPPType::is_printable() const
{
  return print_ != nullptr;
}

inline bool CPPType::is_equality_comparable() const
{
  return is_equal_ != nullptr;
}

inline bool CPPType::is_hashable() const
{
  return hash_ != nullptr;
}

inline bool CPPType::pointer_has_valid_alignment(const void *ptr) const
{
  return (uintptr_t(ptr) & alignment_mask_) == 0;
}

inline bool CPPType::pointer_can_point_to_instance(const void *ptr) const
{
  return ptr != nullptr && pointer_has_valid_alignment(ptr);
}

inline void CPPType::default_construct(void *ptr) const
{
  default_construct_(ptr);
}

inline void CPPType::default_construct_n(void *ptr, int64_t n) const
{
  default_construct_n_(ptr, n);
}

inline void CPPType::default_construct_indices(void *ptr, const IndexMask &mask) const
{
  default_construct_indices_(ptr, mask);
}

inline void CPPType::value_initialize(void *ptr) const
{
  value_initialize_(ptr);
}

inline void CPPType::value_initialize_n(void *ptr, int64_t n) const
{
  value_initialize_n_(ptr, n);
}

inline void CPPType::value_initialize_indices(void *ptr, const IndexMask &mask) const
{
  value_initialize_indices_(ptr, mask);
}

inline void CPPType::destruct(void *ptr) const
{
  destruct_(ptr);
}

inline void CPPType::destruct_n(void *ptr, int64_t n) const
{
  destruct_n_(ptr, n);
}

inline void CPPType::destruct_indices(void *ptr, const IndexMask &mask) const
{
  destruct_indices_(ptr, mask);
}

inline void CPPType::copy_assign(const void *src, void *dst) const
{
  copy_assign_(src, dst);
}

inline void CPPType::copy_assign_n(const void *src, void *dst, int64_t n) const
{
  copy_assign_n_(src, dst, n);
}

inline void CPPType::copy_assign_indices(const void *src, void *dst, const IndexMask &mask) const
{
  copy_assign_indices_(src, dst, mask);
}

inline void CPPType::copy_assign_compressed(const void *src,
                                            void *dst,
                                            const IndexMask &mask) const
{
  copy_assign_compressed_(src, dst, mask);
}

inline void CPPType::copy_construct(const void *src, void *dst) const
{
  copy_construct_(src, dst);
}

inline void CPPType::copy_construct_n(const void *src, void *dst, int64_t n) const
{
  copy_construct_n_(src, dst, n);
}

inline void CPPType::copy_construct_indices(const void *src,
                                            void *dst,
                                            const IndexMask &mask) const
{
  copy_construct_indices_(src, dst, mask);
}

inline void CPPType::copy_construct_compressed(const void *src,
                                               void *dst,
                                               const IndexMask &mask) const
{
  copy_construct_compressed_(src, dst, mask);
}

inline void CPPType::move_assign(void *src, void *dst) const
{
  move_assign_(src, dst);
}

inline void CPPType::move_assign_n(void *src, void *dst, int64_t n) const
{
  move_assign_n_(src, dst, n);
}

inline void CPPType::move_assign_indices(void *src, void *dst, const IndexMask &mask) const
{
  move_assign_indices_(src, dst, mask);
}

inline void CPPType::move_construct(void *src, void *dst) const
{
  move_construct_(src, dst);
}

inline void CPPType::move_construct_n(void *src, void *dst, int64_t n) const
{
  move_construct_n_(src, dst, n);
}

inline void CPPType::move_construct_indices(void *src, void *dst, const IndexMask &mask) const
{
  move_construct_indices_(src, dst, mask);
}

inline void CPPType::relocate_assign(void *src, void *dst) const
{
  relocate_assign_(src, dst);
}

inline void CPPType::relocate_assign_n(void *src, void *dst, int64_t n) const
{
  relocate_assign_n_(src, dst, n);
}

inline void CPPType::relocate_assign_indices(void *src, void *dst, const IndexMask &mask) const
{
  relocate_assign_indices_(src, dst, mask);
}

inline void CPPType::relocate_construct(void *src, void *dst) const
{
  relocate_construct_(src, dst);
}

inline void CPPType::relocate_construct_n(void *src, void *dst, int64_t n) const
{
  relocate_construct_n_(src, dst, n);
}

inline void CPPType::relocate_construct_indices(void *src, void *dst, const IndexMask &mask) const
{
  relocate_construct_indices_(src, dst, mask);
}

inline void CPPType::fill_assign_n(const void *value, void *dst, int64_t n) const
{
  fill_assign_n_(value, dst, n);
}

inline void CPPType::fill_assign_indices(const void *value, void *dst, const IndexMask &mask) const
{
  fill_assign_indices_(value, dst, mask);
}

inline void CPPType::fill_construct_n(const void *value, void *dst, int64_t n) const
{
  fill_construct_n_(value, dst, n);
}

inline void CPPType::fill_construct_indices(const void *value,
                                            void *dst,
                                            const IndexMask &mask) const
{
  fill_construct_indices_(value, dst, mask);
}

inline bool CPPType::can_exist_in_buffer(const int64_t buffer_size,
                                         const int64_t buffer_alignment) const
{
  return this->size <= buffer_size && this->alignment <= buffer_alignment;
}

inline void CPPType::print(const void *value, std::stringstream &ss) const
{
  BLI_assert(this->pointer_can_point_to_instance(value));
  print_(value, ss);
}

inline bool CPPType::is_equal(const void *a, const void *b) const
{
  return is_equal_(a, b);
}

inline bool CPPType::is_equal_or_false(const void *a, const void *b) const
{
  if (this->is_equality_comparable()) {
    return this->is_equal(a, b);
  }
  return false;
}

inline uint64_t CPPType::hash(const void *value) const
{
  return hash_(value);
}

inline uint64_t CPPType::hash_or_fallback(const void *value, uint64_t fallback_hash) const
{
  if (this->is_hashable()) {
    return this->hash(value);
  }
  return fallback_hash;
}

inline const void *CPPType::default_value() const
{
  return default_value_;
}

inline uint64_t CPPType::hash() const
{
  return get_default_hash(this);
}

inline void (*CPPType::destruct_fn() const)(void *)
{
  return destruct_;
}

template<typename T> inline bool CPPType::is() const
{
  return this == &CPPType::get<std::decay_t<T>>();
}

template<typename... T> inline bool CPPType::is_any() const
{
  return (this->is<T>() || ...);
}

template<typename... Types, typename Fn> inline void CPPType::to_static_type(const Fn &fn) const
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

}  // namespace blender
