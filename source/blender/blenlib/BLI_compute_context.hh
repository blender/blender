/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * When logging computed values, we generally want to know where the value was computed. For
 * example, geometry nodes logs socket values so that they can be displayed in the ui. For that we
 * can combine the logged value with a `ComputeContext`, which identifies the place where the value
 * was computed.
 *
 * This is not a trivial problem because e.g. just storing a pointer to the socket a value
 * belongs to is not enough. That's because the same socket may correspond to many different values
 * when the socket is used in a node group that is used multiple times. In this case, not only does
 * the socket have to be stored but also the entire nested node group path that led to the
 * evaluation of the socket.
 *
 * Storing the entire "context path" for every logged value is not feasible, because that path can
 * become quite long. So that would need much more memory, more compute overhead and makes it
 * complicated to compare if two contexts are the same. If the identifier for a compute context
 * would have a variable size, it would also be much harder to create a map from context to values.
 *
 * The solution implemented below uses the following key ideas:
 * - Every compute context can be hashed to a unique fixed size value (`ComputeContextHash`). While
 *   technically there could be hash collisions, the hashing algorithm has to be chosen to make
 *   that practically impossible. This way an entire context path, possibly consisting of many
 *   nested contexts, is represented by a single value that can be stored easily.
 * - A nested compute context is build as singly linked list, where every compute context has a
 *   pointer to the parent compute context. Note that a link in the other direction is not possible
 *   because the same parent compute context may be used by many different children which possibly
 *   run on different threads.
 */

#include "BLI_cache_mutex.hh"
#include "BLI_string_ref.hh"
#include "BLI_struct_equality_utils.hh"

namespace blender {

class ComputeContext;

/**
 * A hash that uniquely identifies a specific (non-fixed-size) compute context. The hash has to
 * have enough bits to make collisions practically impossible.
 */
struct ComputeContextHash {
  uint64_t v1 = 0;
  uint64_t v2 = 0;

  uint64_t hash() const
  {
    return v1;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_2(ComputeContextHash, v1, v2)

  /**
   * Standard way to create a compute context hash.
   * \param parent: The optional parent context.
   * \param type_str: A string literal that identifies the context type. This is used to avoid hash
   * collisions between different context types.
   * \param args: Additional arguments that affect the hash. Note that only the shallow bytes of
   * these types are used. So they generally should not contain any padding.
   */
  template<size_t N, typename... Args>
  static ComputeContextHash from(const ComputeContext *parent,
                                 const char (&type_str)[N],
                                 Args &&...args);

  friend std::ostream &operator<<(std::ostream &stream, const ComputeContextHash &hash);

 private:
  /**
   * Compute a context hash by packing all the arguments into a contiguous buffer and hashing
   * that.
   */
  template<typename... Args> static ComputeContextHash from_shallow_bytes(Args &&...args);

  /** Compute a context hash from a contiguous buffer. */
  static ComputeContextHash from_bytes(const void *data, int64_t len);
};

/**
 * Identifies the context in which a computation happens. This context can be used to identify
 * values logged during the computation. For more details, see the comment at the top of the file.
 *
 * This class should be subclassed to implement specific contexts.
 */
class ComputeContext {
 protected:
  /**
   * Pointer to the context that this context is child of. That allows nesting compute
   * contexts.
   */
  const ComputeContext *parent_ = nullptr;

  /**
   * The hash that uniquely identifies this context. It's a combined hash of this context as well
   * as all the parent contexts. It's computed lazily to keep initial construction of compute
   * contexts very cheap.
   */
  mutable ComputeContextHash hash_;

 private:
  mutable CacheMutex hash_mutex_;

 public:
  ComputeContext(const ComputeContext *parent) : parent_(parent) {}
  virtual ~ComputeContext() = default;

  const ComputeContextHash &hash() const
  {
    hash_mutex_.ensure([&]() { hash_ = this->compute_hash(); });
    return hash_;
  }

  const ComputeContext *parent() const
  {
    return parent_;
  }

  /**
   * Print the entire nested context stack.
   */
  void print_stack(std::ostream &stream, StringRef name) const;

  /**
   * Print information about this specific context. This has to be implemented by each subclass.
   */
  virtual void print_current_in_line(std::ostream &stream) const = 0;

  friend std::ostream &operator<<(std::ostream &stream, const ComputeContext &compute_context);

 private:
  /** Compute the hash of this context, usually using #ComputeContextHash::from. */
  virtual ComputeContextHash compute_hash() const = 0;
};

template<size_t N, typename... Args>
inline ComputeContextHash ComputeContextHash::from(const ComputeContext *parent,
                                                   const char (&type_str)[N],
                                                   Args &&...args)
{
  return ComputeContextHash::from_shallow_bytes(
      parent ? parent->hash() : ComputeContextHash{0, 0}, type_str, args...);
}

template<typename... Args>
inline ComputeContextHash ComputeContextHash::from_shallow_bytes(Args &&...args)
{
  /* Copy all values into a contiguous buffer. Intentionally don't use std::tuple to avoid any
   * potential padding. */
  constexpr int64_t size_sum = (sizeof(args) + ...);
  char buffer[size_sum];
  int64_t offset = 0;
  (
      [&] {
        using Arg = std::remove_reference_t<std::remove_cv_t<Args>>;
        static_assert(std::has_unique_object_representations_v<Arg>);
        const Arg &arg = args;
        memcpy(buffer + offset, &arg, sizeof(Arg));
        offset += sizeof(Arg);
      }(),
      ...);
  /* Compute the hash of that buffer. */
  return ComputeContextHash::from_bytes(buffer, offset);
}

}  // namespace blender
