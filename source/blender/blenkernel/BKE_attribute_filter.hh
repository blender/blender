/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_string_ref.hh"

namespace blender::bke {

/**
 * Many geometry algorithms need to deal with attributes. For example, the Subdivide Curves code
 * has to properly interpolate attributes to the intermediate points. However, sometimes certain
 * attributes are not necessary after the operation anymore, so they can just be skipped for
 * optimization purposes. This is where #AttributeFilter comes in. It allows the caller to specify
 * which attributes should be processed and which should be ignored.
 *
 * \note It depends on the algorithm whether the output of the filter is followed exactly. For
 * example, some algorithm might not be able to propagate attributes on some domain, even if the
 * filter says that the attribute should be propagated.
 */
struct AttributeFilter {
 public:
  enum class Result {
    /** The algorithm is allowed to skip processing the attribute. */
    AllowSkip,
    /** The attribute should be processed/propagated if at all possible. */
    Process,
  };

  virtual ~AttributeFilter() = default;

  /**
   * This function has different implementations in each derived class. By default, all attributes
   * should be processed.
   */
  virtual Result filter(const StringRef /*name*/) const
  {
    return Result::Process;
  }

  /**
   * Utility to simplify the check for whether some attribute can be skipped.
   */
  bool allow_skip(const StringRef name) const
  {
    return this->filter(name) == Result::AllowSkip;
  }

  static const AttributeFilter &default_filter()
  {
    static AttributeFilter filter;
    return filter;
  }
};

}  // namespace blender::bke
