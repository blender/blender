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
  StringRefNull description_;
  bool is_hdr_ = false;
  Gamut gamut_ = Gamut::Unknown;
  TransferFunction transfer_function_ = TransferFunction::Unknown;

 public:
  LibOCIOView(const int index,
              const StringRefNull name,
              const StringRefNull description,
              const bool is_hdr,
              const Gamut gamut,
              const TransferFunction transfer_function)
      : name_(name),
        description_(description),
        is_hdr_(is_hdr),
        gamut_(gamut),
        transfer_function_(transfer_function)
  {
    this->index = index;
  }

  StringRefNull name() const override
  {
    return name_;
  }

  StringRefNull description() const override
  {
    return description_;
  }

  bool is_hdr() const override
  {
    return is_hdr_;
  }

  Gamut gamut() const override
  {
    return gamut_;
  }

  TransferFunction transfer_function() const override
  {
    return transfer_function_;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOView");
};

}  // namespace blender::ocio

#endif
