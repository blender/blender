/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BLI_utildefines.h"
#include "BLI_vector.hh"

struct ID;
struct bConstraint;
struct bPoseChannel;
struct ModifierData;

namespace blender::deg {

/* This class keeps track of the builder calls nesting, allowing to unroll them back and provide a
 * clue about how the builder made it to its current state.
 *
 * The tracing is based on the builder giving a trace clues to the stack. Typical usage is:
 *
 *   void DepsgraphRelationBuilder::my_id_builder(ID *id)
 *   {
 *     if (built_map_.checkIsBuiltAndTag(id)) {
 *       return;
 *     }
 *
 *     const BuilderStack::ScopedEntry stack_entry = stack_.trace(*id);
 *
 *     ...
 *   }
 */
class BuilderStack {
 public:
  /* Entry of the backtrace.
   * A cheap-to-construct wrapper which allows to gather a proper string representation whenever
   * the stack is printed. */
  class Entry {
   public:
    explicit Entry(const ID &id) : id_(&id) {}

    explicit Entry(const bConstraint &constraint) : constraint_(&constraint) {}

    explicit Entry(const bPoseChannel &pchan) : pchan_(&pchan) {}

    explicit Entry(const ModifierData &modifier_data) : modifier_data_(&modifier_data) {}

   private:
    friend class BuilderStack;

    const ID *id_ = nullptr;
    const bConstraint *constraint_ = nullptr;
    const ModifierData *modifier_data_ = nullptr;
    const bPoseChannel *pchan_ = nullptr;
  };

  using Stack = Vector<Entry>;

  /* A helper class to provide a RAII style of tracing. It is constructed by the
   * `BuilderStack::trace` (which pushes entry to the stack), and upon destruction of this object
   * the corresponding entry is popped from the stack.
   *
   * The goal of this `ScopedEntry` is to free developers from worrying about removing entries from
   * the stack whenever leaving a builder step scope. */
  class ScopedEntry {
   public:
    /* Delete copy constructor and operator: scoped entries are only supposed to be constructed
     * once and never copied. */
    ScopedEntry(const ScopedEntry &other) = delete;
    ScopedEntry &operator=(const ScopedEntry &other) = delete;

    /* Move semantic. */
    ScopedEntry(ScopedEntry &&other) noexcept : stack_(other.stack_)
    {
      other.stack_ = nullptr;
    }
    ScopedEntry &operator=(ScopedEntry &&other)
    {
      if (this == &other) {
        return *this;
      }

      stack_ = other.stack_;
      other.stack_ = nullptr;

      return *this;
    }

    ~ScopedEntry()
    {
      /* Stack will become nullptr when the entry was moved somewhere else. */
      if (stack_ != nullptr) {
        BLI_assert(!stack_->is_empty());
        stack_->pop_last();
      }
    }

   private:
    friend BuilderStack;

    explicit ScopedEntry(Stack &stack) : stack_(&stack) {}

    Stack *stack_;
  };

  BuilderStack() = default;
  ~BuilderStack() = default;

  bool is_empty() const
  {
    return stack_.is_empty();
  }

  void print_backtrace(std::ostream &stream);

  template<class... Args> ScopedEntry trace(const Args &...args)
  {
    stack_.append_as(args...);

    return ScopedEntry(stack_);
  }

 private:
  Stack stack_;
};

}  // namespace blender::deg
