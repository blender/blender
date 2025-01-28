/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_attribute_filter.hh"

#include "BLI_set.hh"

namespace blender::bke {

/**
 * Utility to create an #AttributeFilter from a lambda.
 */
template<typename Fn> struct AttributeFilterFromFunc : public AttributeFilter {
 private:
  Fn fn_;

  static_assert(std::is_invocable_r_v<Result, Fn, StringRef>);

 public:
  constexpr AttributeFilterFromFunc(Fn fn) : fn_(std::move(fn)) {}

  Result filter(const StringRef name) const override
  {
    return fn_(name);
  }
};

/**
 * Combines an existing #AttributeFilter and tags a few additional attributes that can/should be
 * skipped.
 */
inline auto attribute_filter_with_skip_ref(AttributeFilter filter, const Span<StringRef> skip)
{
  return AttributeFilterFromFunc([filter, skip](const StringRef name) {
    if (skip.contains(name)) {
      return AttributeFilter::Result::AllowSkip;
    }
    return filter.filter(name);
  });
}

/** Same as above but with a #Set. */
template<typename StringT>
inline auto attribute_filter_with_skip_ref(AttributeFilter filter, const Set<StringT> &skip)
{
  return AttributeFilterFromFunc([filter, &skip](const StringRef name) {
    if (skip.contains_as(name)) {
      return AttributeFilter::Result::AllowSkip;
    }
    return filter.filter(name);
  });
}

/**
 * Creates a simple #AttributeFilter that skips allows the given attributes to be skipped, while
 * all others should be processed.
 */
inline auto attribute_filter_from_skip_ref(const Span<StringRef> skip)
{
  return AttributeFilterFromFunc([skip](const StringRef name) {
    if (skip.contains(name)) {
      return AttributeFilter::Result::AllowSkip;
    }
    return AttributeFilter::Result::Process;
  });
}

/** Same as above but with a #Set. */
template<typename StringT> inline auto attribute_filter_from_skip_ref(const Set<StringT> &skip)
{
  return AttributeFilterFromFunc([&skip](const StringRef name) {
    if (skip.contains_as(name)) {
      return AttributeFilter::Result::AllowSkip;
    }
    return AttributeFilter::Result::Process;
  });
}

}  // namespace blender::bke
