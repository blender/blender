/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <optional>

#include "BLI_index_range.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

namespace blender::threading {

/**
 * Specifies how large the individual tasks are relative to each other. It's common that all tasks
 * have a very similar size in which case one can just ignore this. However, sometimes tasks have
 * very different sizes and it makes sense for the scheduler to group fewer big tasks and many
 * small tasks together.
 */
class TaskSizeHints {
 public:
  enum class Type {
    /** All tasks have the same size. */
    Static,
    /** All tasks can have different sizes and one has to look up the sizes one by one. */
    IndividualLookup,
    /**
     * All tasks can have different sizes but one can efficiently determine the size of a
     * consecutive range of tasks.
     */
    AccumulatedLookup,
  };

  Type type;

 protected:
  TaskSizeHints(const Type type) : type(type) {}
};

namespace detail {

class TaskSizeHints_Static : public TaskSizeHints {
 public:
  int64_t size;

  TaskSizeHints_Static(const int64_t size) : TaskSizeHints(Type::Static), size(size) {}
};

class TaskSizeHints_IndividualLookup : public TaskSizeHints {
 public:
  std::optional<int64_t> full_size;

  TaskSizeHints_IndividualLookup(std::optional<int64_t> full_size)
      : TaskSizeHints(Type::IndividualLookup), full_size(full_size)
  {
  }

  /** Get the individual size of all tasks in the range. */
  virtual void lookup_individual_sizes(IndexRange /*range*/,
                                       MutableSpan<int64_t> r_sizes) const = 0;
};

class TaskSizeHints_AccumulatedLookup : public TaskSizeHints {
 public:
  TaskSizeHints_AccumulatedLookup() : TaskSizeHints(Type::AccumulatedLookup) {}

  /** Get the accumulated size of a range of tasks. */
  virtual int64_t lookup_accumulated_size(IndexRange range) const = 0;
};

template<typename Fn>
class TaskSizeHints_IndividualLookupFn : public TaskSizeHints_IndividualLookup {
 private:
  Fn fn_;

 public:
  TaskSizeHints_IndividualLookupFn(Fn fn, const std::optional<int64_t> full_size)
      : TaskSizeHints_IndividualLookup(full_size), fn_(std::move(fn))
  {
  }

  void lookup_individual_sizes(const IndexRange range, MutableSpan<int64_t> r_sizes) const override
  {
    fn_(range, r_sizes);
  }
};

template<typename Fn>
class TaskSizeHints_AccumulatedLookupFn : public TaskSizeHints_AccumulatedLookup {
 private:
  Fn fn_;

 public:
  TaskSizeHints_AccumulatedLookupFn(Fn fn) : TaskSizeHints_AccumulatedLookup(), fn_(std::move(fn))
  {
  }

  int64_t lookup_accumulated_size(const IndexRange range) const override
  {
    return fn_(range);
  }
};

}  // namespace detail

inline bool use_single_thread(const TaskSizeHints &size_hints,
                              const IndexRange range,
                              const int64_t threshold)
{
#ifdef __GNUC__ /* False positive warning with GCC. */
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif
  switch (size_hints.type) {
    case TaskSizeHints::Type::Static: {
      const int64_t size = static_cast<const detail::TaskSizeHints_Static &>(size_hints).size;
      return size * range.size() <= threshold;
    }
    case TaskSizeHints::Type::IndividualLookup: {
      const std::optional<int64_t> &full_size =
          static_cast<const detail::TaskSizeHints_IndividualLookup &>(size_hints).full_size;
      if (full_size.has_value()) {
        if (*full_size <= threshold) {
          return true;
        }
      }
      return false;
    }
    case TaskSizeHints::Type::AccumulatedLookup: {
      const int64_t accumulated_size =
          static_cast<const detail::TaskSizeHints_AccumulatedLookup &>(size_hints)
              .lookup_accumulated_size(range);
      return accumulated_size <= threshold;
    }
  }
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
  BLI_assert_unreachable();
  return true;
}

/**
 * Specify how large the task at each index is with a callback. This is especially useful if the
 * size of each individual task can be very different. Specifying the size allows the scheduler to
 * distribute the work across threads more equally.
 *
 * \param fn: A function that returns the size for a single task: `(int64_t index) -> int64_t`.
 * \param full_size: The (approximate) accumulated size of all tasks. This is optional and should
 *   only be passed in if it is trivially accessible already.
 */
template<typename Fn>
inline auto individual_task_sizes(Fn &&fn, const std::optional<int64_t> full_size = std::nullopt)
{
  auto array_fn = [fn = std::forward<Fn>(fn)](const IndexRange range,
                                              MutableSpan<int64_t> r_sizes) {
    for (const int64_t i : range.index_range()) {
      r_sizes[i] = fn(range[i]);
    }
  };
  return detail::TaskSizeHints_IndividualLookupFn<decltype(array_fn)>(std::move(array_fn),
                                                                      full_size);
}

/**
 * Very similar to #individual_task_sizes, but should be used if one can very efficiently compute
 * the accumulated task size (in O(1) time). This is often the case when e.g. working with
 * #OffsetIndices.
 *
 * \param fn: A function that returns the accumulated size for a range of tasks:
 * `(IndexRange indices) -> int64_t`.
 */
template<typename Fn> inline auto accumulated_task_sizes(Fn &&fn)
{
  return detail::TaskSizeHints_AccumulatedLookupFn<decltype(fn)>(std::forward<Fn>(fn));
}

}  // namespace blender::threading
