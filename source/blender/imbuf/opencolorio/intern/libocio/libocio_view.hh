/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "MEM_guardedalloc.h"

#  include "OCIO_view.hh"

#  include "BLI_string_ref.hh"

namespace blender::ocio {

class LibOCIOView : public View {
  StringRefNull name_;
  bool is_hdr_ = false;
  bool is_wide_gamut_ = false;
  bool is_srgb_ = false;
  bool is_extended_ = false;

 public:
  LibOCIOView(const int index,
              const StringRefNull name,
              const bool is_hdr,
              const bool is_wide_gamut,
              const bool is_srgb,
              const bool is_extended)
      : name_(name),
        is_hdr_(is_hdr),
        is_wide_gamut_(is_wide_gamut),
        is_srgb_(is_srgb),
        is_extended_(is_extended)
  {
    this->index = index;
  }

  StringRefNull name() const override
  {
    return name_;
  }

  bool is_hdr() const override
  {
    return is_hdr_;
  }

  bool is_wide_gamut() const override
  {
    return is_wide_gamut_;
  }

  /* Display space is exactly Rec.709 + sRGB piecewise transfer function. */
  bool is_srgb() const
  {
    return is_srgb_;
  }

  /* Display space has values outside of 0..1 range. */
  bool is_extended() const
  {
    return is_extended_;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOView");
};

}  // namespace blender::ocio

#endif
