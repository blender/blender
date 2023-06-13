/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_stack.hh"
#include "BLI_string_ref.hh"

namespace blender {

/**
 * A hash that uniquely identifies a specific (non-fixed-size) compute context. The hash has to
 * have enough bits to make collisions practically impossible.
 */
struct ComputeContextHash {
  static constexpr int64_t HashSizeInBytes = 16;
  uint64_t v1 = 0;
  uint64_t v2 = 0;

  uint64_t hash() const
  {
    return v1;
  }

  friend bool operator==(const ComputeContextHash &a, const ComputeContextHash &b)
  {
    return a.v1 == b.v1 && a.v2 == b.v2;
  }

  friend bool operator!=(const ComputeContextHash &a, const ComputeContextHash &b)
  {
    return !(a == b);
  }

  void mix_in(const void *data, int64_t len);

  friend std::ostream &operator<<(std::ostream &stream, const ComputeContextHash &hash);
};

static_assert(sizeof(ComputeContextHash) == ComputeContextHash::HashSizeInBytes);

/**
 * Identifies the context in which a computation happens. This context can be used to identify
 * values logged during the computation. For more details, see the comment at the top of the file.
 *
 * This class should be subclassed to implement specific contexts.
 */
class ComputeContext {
 private:
  /**
   * Only used for debugging currently.
   */
  const char *static_type_;
  /**
   * Pointer to the context that this context is child of. That allows nesting compute contexts.
   */
  const ComputeContext *parent_ = nullptr;

 protected:
  /**
   * The hash that uniquely identifies this context. It's a combined hash of this context as well
   * as all the parent contexts.
   */
  ComputeContextHash hash_;

 public:
  ComputeContext(const char *static_type, const ComputeContext *parent)
      : static_type_(static_type), parent_(parent)
  {
    if (parent != nullptr) {
      hash_ = parent_->hash_;
    }
  }
  virtual ~ComputeContext() = default;

  const ComputeContextHash &hash() const
  {
    return hash_;
  }

  const char *static_type() const
  {
    return static_type_;
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
};

/**
 * Utility class to build a context stack in one place. This is typically used to get the hash that
 * corresponds to a specific nested compute context, in order to look up corresponding logged
 * values.
 */
class ComputeContextBuilder {
 private:
  LinearAllocator<> allocator_;
  Stack<destruct_ptr<ComputeContext>> contexts_;

 public:
  bool is_empty() const
  {
    return contexts_.is_empty();
  }

  const ComputeContext *current() const
  {
    if (contexts_.is_empty()) {
      return nullptr;
    }
    return contexts_.peek().get();
  }

  const ComputeContextHash hash() const
  {
    BLI_assert(!contexts_.is_empty());
    return this->current()->hash();
  }

  template<typename T, typename... Args> void push(Args &&...args)
  {
    const ComputeContext *current = this->current();
    destruct_ptr<T> context = allocator_.construct<T>(current, std::forward<Args>(args)...);
    contexts_.push(std::move(context));
  }

  void pop()
  {
    contexts_.pop();
  }
};

}  // namespace blender
