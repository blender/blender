/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <type_traits>
#include <utility>

#include "BLI_any.hh"
#include "BLI_cpp_type.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_memory_counter_fwd.hh"

#include "BKE_node_socket_value_fwd.hh"

namespace blender {

namespace fn {
class GField;
}
namespace nodes {
class GListPtr;
}
namespace bke::volume_grid {
class GVolumeGrid;
}

namespace bke {

class SocketValueVariant;

namespace detail {

enum class SocketValueStorageKind : uint8_t {
  Single,
  Field,
  List,
  Grid,
};

template<typename T>
inline constexpr SocketValueStorageKind socket_value_storage_kind =
    std::is_same_v<T, fn::GField>      ? SocketValueStorageKind::Field :
    std::is_same_v<T, nodes::GListPtr> ? SocketValueStorageKind::List :
#ifdef WITH_OPENVDB
    std::is_same_v<T, volume_grid::GVolumeGrid> ? SocketValueStorageKind::Grid :
#endif
                                                  SocketValueStorageKind::Single;

struct SocketValueVariantTypeInfo;
using SocketValueVariantAny = Any<SocketValueVariantTypeInfo, 32, 16>;

struct SocketValueVariantTypeInfo {
  const CPPType &type;
  bool (*convert_to)(const CPPType &dst_type, SocketValueVariantAny &value);
  bool (*is_interpretable_as)(const CPPType &dst_type, const SocketValueVariantAny &value);
  SocketValueStorageKind storage_kind;

  template<typename T> static SocketValueVariantTypeInfo get()
  {
    return SocketValueVariantTypeInfo{
        .type = *CPPType::get_pre_register<T>(),
        .convert_to = try_convert_fn<T>,
        .is_interpretable_as = is_interpretable_as_fn<T>,
        .storage_kind = socket_value_storage_kind<T>,
    };
  }

  template<typename T>
  static bool try_convert_fn(const CPPType &dst_type, SocketValueVariantAny &value);

  template<typename T>
  static bool is_interpretable_as_fn(const CPPType &dst_type, const SocketValueVariantAny &value);
};

template<typename T>
concept has_generic_type = requires { typename T::generic_type; };
template<typename T> struct storage_type {
  using type = T;
};
template<has_generic_type T> struct storage_type<T> {
  using type = typename T::generic_type;
};

inline const CPPType &to_storage_type(const CPPType &type)
{
  if (type.generic_type) {
    return *type.generic_type;
  }
  return type;  // NOLINT
}

};  // namespace detail

/**
 * #SocketValueVariant is used to pass data between nodes, e.g. by geometry nodes in the lazy
 * function evaluator. Specifically, it is the container type for the following socket types: bool,
 * float, integer, vector, rotation, color and string.
 *
 * The data passed through e.g. an integer socket can be a single value, a field or a grid (and in
 * the lists and images). Each of those is stored differently, but this container can store them
 * all.
 *
 * A key requirement for this container is that it is type-erased, i.e. not all code that uses it
 * has to include all the headers required to process the other storage types. This is achieved by
 * using the #Any type and by providing templated accessors that are implemented outside of a
 * header.
 *
 * This container is also able to do implicit conversions between the different types, with the
 * #ensure_type() methods.
 */
class SocketValueVariant {
 private:
  using Info = detail::SocketValueVariantTypeInfo;

  detail::SocketValueVariantAny value_;

  /**
   * Some types have a generic and compile-time version. For example, there is #GField and
   * #Field<T>. Those are expected to have the same memory layout so that references can be cast
   * between them. The #Any always stores the generic version if it exists.
   */
  template<typename T> using to_storage_type = typename detail::storage_type<T>::type;

 public:
  /**
   * Create an empty variant. This is not valid for any socket type yet.
   */
  SocketValueVariant() = default;

  /**
   * Create a variant based on the given value. This works for primitive types. For more complex
   * types, one can use the #from or #construct_in utilities.
   */
  template<typename T>
  explicit SocketValueVariant(T &&value)
      /* Required to avoid overriding the copy/move-constructors. */
    requires(std::is_trivial_v<std::decay_t<T>> || is_same_any_v<std::decay_t<T>, std::string>);

  /** Construct a #SocketValueVariant at the given pointer from the given value. */
  template<typename T> static SocketValueVariant &construct_in(void *ptr, T &&value);

  /** Create a new #SocketValueVariant from the given value. */
  template<typename T> static SocketValueVariant from(T &&value);

  /** Replace the stored value by constructing the given type in the storage. */
  template<typename T, typename... Args> T &emplace(Args &&...args);

  /**
   * Try to convert the stored value to the given type, returning null if the implicit conversion
   * does not exist.
   */
  template<typename T> T *try_convert();
  void *try_convert(const CPPType &type);

  /**
   * Try to convert the stored value to the given type, creating the type's default value if the
   * conversion does not exist.
   */
  template<typename T> T &ensure_type();
  void *ensure_type(const CPPType &type);

  /** Equivalent to calling #extract<T>() on a copy of this value. */
  template<typename T> T copy_as() const;

  /**
   * Get the stored value as a specific type. For convenience this allows accessing the stored type
   * as a different type, using implicit conversions as possible.
   *
   * This method may leave the variant empty, in a moved from state or unchanged. Therefore, this
   * should only be called once.
   */
  template<typename T> T extract();

  /**
   * Return a pointer to the stored value if it is of the given type. Otherwise return null.
   */
  template<typename T> const T *get_if() const;
  template<typename T> T *get_if();
  const void *get_if(const CPPType &type) const;
  void *get_if(const CPPType &type);

  /** Get the stored value as a #GPointer. */
  GPointer get() const;
  GMutablePointer get();

  /**
   * If true, the stored value cannot be converted to a single value without loss of information.
   */
  bool is_context_dependent_field() const;

  /**
   * If true, the value is stored as a #GField.
   */
  bool is_field() const;

  /**
   * The stored value is a volume grid.
   */
  bool is_volume_grid() const;

  /**
   * The stored value is a single value.
   */
  bool is_single() const;

  /**
   * The stored value is a list.
   */
  bool is_list() const;

  /** Also see GeomtrySet::ensure_owns_direct_data. */
  void ensure_owns_direct_data();
  bool owns_direct_data() const;

  /**
   * Replaces the stored value with a new uninitialized single value for the given socket type. The
   * caller is responsible to construct the value in the returned memory before it is used.
   */
  void *allocate_single(const CPPType &type);

  void count_memory(MemoryCounter &memory) const;

 private:
  template<typename T> T &init_default();
  void *init_default(const CPPType &type);

 public:
  template<typename T> static T &init_default(detail::SocketValueVariantAny &value);
  static void *init_default(const CPPType &type, detail::SocketValueVariantAny &value);
  static void *allocate(const CPPType &type, detail::SocketValueVariantAny &value);
};

template<typename T>
inline SocketValueVariant::SocketValueVariant(T &&value)
  requires(std::is_trivial_v<std::decay_t<T>> || is_same_any_v<std::decay_t<T>, std::string>)
    : value_(std::in_place_type<to_storage_type<std::decay_t<T>>>, std::forward<T>(value))
{
}

template<typename T>
inline SocketValueVariant &SocketValueVariant::construct_in(void *ptr, T &&value)
{
  SocketValueVariant *value_variant = new (ptr) SocketValueVariant();
  value_variant->emplace<std::decay_t<T>>(std::forward<T>(value));
  return *value_variant;
}

template<typename T> inline SocketValueVariant SocketValueVariant::from(T &&value)
{
  SocketValueVariant value_variant;
  value_variant.emplace<std::decay_t<T>>(std::forward<T>(value));
  return value_variant;
}

template<typename T, typename... Args> inline T &SocketValueVariant::emplace(Args &&...args)
{
  using StorageT = to_storage_type<T>;
  StorageT &value = value_.emplace<StorageT>(T(std::forward<Args>(args)...));
  static_assert(sizeof(T) == sizeof(StorageT));
  return reinterpret_cast<T &>(value);
}

template<typename T> inline T *SocketValueVariant::try_convert()
{
  using StorageT = to_storage_type<T>;
  if (!value_.has_value()) {
    return nullptr;
  }
  if (value_.is<StorageT>()) {
    if constexpr (detail::has_generic_type<T>) {
      /* Same generic storage (e.g. #GField), but the base type may still differ. */
      if (value_.extra_info().is_interpretable_as(CPPType::get<T>(), value_)) {
        return &reinterpret_cast<T &>(value_.get<StorageT>());
      }
    }
    else {
      return &reinterpret_cast<T &>(value_.get<StorageT>());
    }
  }
  const Info &info = value_.extra_info();
  const CPPType &requested_type = CPPType::get<T>();
  if (!info.convert_to(requested_type, value_)) {
    return nullptr;
  }
  BLI_assert(value_.extra_info().is_interpretable_as(requested_type, value_));
  return &reinterpret_cast<T &>(value_.get<StorageT>());
}

inline void *SocketValueVariant::try_convert(const CPPType &type)
{
  if (!value_.has_value()) {
    return nullptr;
  }
  const Info &info = value_.extra_info();
  if (info.type == type) {
    return value_.get();
  }
  if (!info.convert_to(type, value_)) {
    return nullptr;
  }
  return value_.get();
}

template<typename T> inline T &SocketValueVariant::ensure_type()
{
  if (T *ptr = this->try_convert<T>()) {
    return *ptr;
  }
  return this->init_default<T>();
}

inline void *SocketValueVariant::ensure_type(const CPPType &type)
{
  if (void *ptr = this->try_convert(type)) {
    return ptr;
  }
  return this->init_default(type);
}

template<typename T> inline T SocketValueVariant::copy_as() const
{
  if (const T *value = this->get_if<T>()) {
    return *value;
  }
  SocketValueVariant copy(*this);
  return copy.extract<T>();
}

template<typename T> inline const T *SocketValueVariant::get_if() const
{
  using StorageT = to_storage_type<T>;
  if (!value_.has_value()) {
    return nullptr;
  }
  /* Compare #Any type-info pointers directly instead of going through #CPPType. */
  if (!value_.is<StorageT>()) {
    return nullptr;
  }
  if constexpr (detail::has_generic_type<T>) {
    const Info &info = value_.extra_info();
    if (!info.is_interpretable_as(CPPType::get<T>(), value_)) {
      return nullptr;
    }
    return reinterpret_cast<const T *>(&value_.get<StorageT>());
  }
  else {
    return &value_.get<StorageT>();
  }
}

template<typename T> inline T *SocketValueVariant::get_if()
{
  return const_cast<T *>(std::as_const(*this).get_if<T>());
}

inline const void *SocketValueVariant::get_if(const CPPType &type) const
{
  if (!value_) {
    return nullptr;
  }
  const Info &info = value_.extra_info();
  if (info.type == type) {
    return value_.get();
  }
  if (info.is_interpretable_as(type, value_)) {
    return value_.get();
  }
  return nullptr;
}

inline void *SocketValueVariant::get_if(const CPPType &type)
{
  return const_cast<void *>(std::as_const(*this).get_if(type));
}

inline GPointer SocketValueVariant::get() const
{
  if (!value_) {
    return {};
  }
  const Info &info = value_.extra_info();
  return {info.type, value_.get()};
}

inline GMutablePointer SocketValueVariant::get()
{
  if (!value_) {
    return {};
  }
  const Info &info = value_.extra_info();
  return {info.type, value_.get()};
}

inline bool SocketValueVariant::is_single() const
{
  return value_.has_value() &&
         value_.extra_info().storage_kind == detail::SocketValueStorageKind::Single;
}

inline bool SocketValueVariant::is_field() const
{
  return value_.has_value() &&
         value_.extra_info().storage_kind == detail::SocketValueStorageKind::Field;
}

inline bool SocketValueVariant::is_list() const
{
  return value_.has_value() &&
         value_.extra_info().storage_kind == detail::SocketValueStorageKind::List;
}

inline bool SocketValueVariant::is_volume_grid() const
{
  return value_.has_value() &&
         value_.extra_info().storage_kind == detail::SocketValueStorageKind::Grid;
}

template<typename T> inline T &SocketValueVariant::init_default()
{
  return SocketValueVariant::init_default<T>(value_);
}

inline void *SocketValueVariant::init_default(const CPPType &type)
{
  return SocketValueVariant::init_default(type, value_);
}

template<typename T> inline T SocketValueVariant::extract()
{
  return std::move(this->ensure_type<T>());
}

}  // namespace bke
}  // namespace blender
