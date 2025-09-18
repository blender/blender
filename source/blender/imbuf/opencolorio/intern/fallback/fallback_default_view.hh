/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "OCIO_view.hh"

namespace blender::ocio {

class FallbackDefaultView : public View {
 public:
  FallbackDefaultView()
  {
    this->index = 0;
  }

  StringRefNull name() const override
  {
    return "Standard";
  }

  StringRefNull description() const override
  {
    return "";
  }

  bool is_hdr() const override
  {
    return false;
  }

  Gamut gamut() const override
  {
    return Gamut::Rec709;
  }

  TransferFunction transfer_function() const override
  {
    return TransferFunction::sRGB;
  }
};

}  // namespace blender::ocio
