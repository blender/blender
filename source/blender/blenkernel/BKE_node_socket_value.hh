/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "DNA_node_types.h"

#include "BLI_any.hh"
#include "BLI_generic_pointer.hh"

#include "BKE_node_socket_value_fwd.hh"

namespace blender::bke {

/**
 * #SocketValueVariant is used by geometry nodes in the lazy-function evaluator to pass data
 * between nodes. Specifically, it is the container type for the following socket types: bool,
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
 */
class SocketValueVariant {
 private:
  /**
   * This allows faster lookup of the correct type in the #Any below. For example, when retrieving
   * the value of an integer socket, we'd usually have to check whether the #Any contains a single
   * `int` or a field. Doing that check by comparing an enum is cheaper.
   *
   * Also, to figure out if we currently store a single value we'd otherwise have to check whether
   * they #Any stored an integer or float or boolean etc.
   */
  enum class Kind {
    /**
     * Used to indicate that there is no value currently. This is used by the default constructor.
     */
    None,
    /**
     * Indicates that there is a single value like `int`, `float` or `std::string` stored.
     */
    Single,
    /**
     * Indicates that there is a `GField` stored.
     */
    Field,
    /**
     * Indicates that there is a `GVolumeGrid` stored.
     */
    Grid,
    /** Indicates that there is a `ListPtr` stored. */
    List,
  };

  /**
   * High level category of the stored type.
   */
  Kind kind_ = Kind::None;
  /**
   * The socket type that corresponds to the stored value type, e.g. `SOCK_INT` for an `int` or
   * integer field.
   */
  eNodeSocketDatatype socket_type_;
  /**
   * Contains the actual socket value. For single values this contains the value directly (e.g.
   * `int` or `float3`). For fields this always contains a #GField and not e.g. #Field<int>. This
   * simplifies generic code.
   *
   * Small types are embedded directly, while larger types are separately allocated.
   */
  Any<void, 24> value_;

 public:
  /**
   * Create an empty variant. This is not valid for any socket type yet.
   */
  SocketValueVariant() = default;
  SocketValueVariant(const SocketValueVariant &other) = default;
  SocketValueVariant(SocketValueVariant &&other) = default;
  SocketValueVariant &operator=(const SocketValueVariant &other) = default;
  SocketValueVariant &operator=(SocketValueVariant &&other) = default;
  ~SocketValueVariant() = default;

  /**
   * Create a variant based on the given value. This works for primitive types. For more complex
   * types use #set explicitly. Alternatively, one can use the #From or #ConstructIn utilities.
   */
  template<typename T,
           /* The enable-if is necessary to avoid overriding the copy/moveconstructors. */
           BLI_ENABLE_IF((std::is_trivial_v<std::decay_t<T>> ||
                          is_same_any_v<std::decay_t<T>, std::string>))>
  explicit SocketValueVariant(T &&value)
  {
    this->set(std::forward<T>(value));
  }

  /** Construct a #SocketValueVariant at the given pointer from the given value. */
  template<typename T> static SocketValueVariant &ConstructIn(void *ptr, T &&value);

  /** Create a new #SocketValueVariant from the given value. */
  template<typename T> static SocketValueVariant From(T &&value);

  /**
   * \return True if the stored value is valid for a specific socket type. This is mainly meant to
   * be used by asserts.
   */
  bool valid_for_socket(eNodeSocketDatatype socket_type) const;

  /**
   * Get the stored value as a specific type. For convenience this allows accessing the stored type
   * as a different type. For example, a stored single `int` can also be accessed as `GField` or
   * `Field<int>` (but not `float` or `Field<float>`).
   *
   * This method may leave the variant empty, in a moved from state or unchanged. Therefore, this
   * should only be called once.
   */
  template<typename T> T extract();

  /**
   * Same as #extract, but always leaves the variant unchanged. So this method can be called
   * multiple times.
   */
  template<typename T> T get() const;

  /**
   * Replaces the stored value with a new value of potentially a different type.
   */
  template<typename T> void set(T &&value);

  /**
   * If true, the stored value cannot be converted to a single value without loss of information.
   */
  bool is_context_dependent_field() const;

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

  /**
   * Convert the stored value into a single value. For simple value access, this is not necessary,
   * because #get does the conversion implicitly. However, it is necessary if one wants to use
   * #get_single_ptr. Context-dependent fields or grids will just result in a fallback value.
   *
   * The caller has to make sure that the stored value is a single value, field or grid.
   */
  void convert_to_single();

  /**
   * Get a pointer to the embedded single value. The caller has to make sure that there actually is
   * a single value stored, e.g. by calling #convert_to_single.
   */
  GPointer get_single_ptr() const;
  GMutablePointer get_single_ptr();

  /**
   * Similar to #get_single_ptr, but returns an untyped pointer. This can only be used if the
   * caller knows for sure which type is contained. In that case, it can be a bit faster though,
   * because the corresponding #CPPType does not have to be looked up based on the socket type.
   */
  const void *get_single_ptr_raw() const;

  /**
   * Replace the stored value with the given single value.
   */
  void store_single(eNodeSocketDatatype socket_type, const void *value);

  /**
   * Replaces the stored value with a new uninitialized single value for the given socket type. The
   * caller is responsible to construct the value in the returned memory before it is used.
   */
  void *allocate_single(eNodeSocketDatatype socket_type);

  friend std::ostream &operator<<(std::ostream &stream, const SocketValueVariant &value_variant);

 private:
  /**
   * This exists so that only one instance of the underlying template has to be instantiated per
   * type. So only `store_impl<int>` is necessary, but not `store_impl<const int &>`.
   */
  template<typename T> void store_impl(T value);
};

template<typename T>
inline SocketValueVariant &SocketValueVariant::ConstructIn(void *ptr, T &&value)
{
  SocketValueVariant *value_variant = new (ptr) SocketValueVariant();
  value_variant->set(std::forward<T>(value));
  return *value_variant;
}

template<typename T> inline SocketValueVariant SocketValueVariant::From(T &&value)
{
  SocketValueVariant value_variant;
  value_variant.set(std::forward<T>(value));
  return value_variant;
}

template<typename T> inline void SocketValueVariant::set(T &&value)
{
  static_assert(!is_same_any_v<std::decay_t<T>, SocketValueVariant, bke::SocketValueVariant *>);
  this->store_impl<std::decay_t<T>>(std::forward<T>(value));
}

inline const void *SocketValueVariant::get_single_ptr_raw() const
{
  BLI_assert(kind_ == Kind::Single);
  return value_.get();
}

}  // namespace blender::bke
