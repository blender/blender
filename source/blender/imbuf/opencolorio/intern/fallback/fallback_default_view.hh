/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "OCIO_view.hh"

namespace blender::ocio {

class ColorSpace;

class FallbackDefaultView : public View {
 protected:
  const ColorSpace *display_colorspace_ = nullptr;

 public:
  FallbackDefaultView(const ColorSpace *display_colorspace)
      : display_colorspace_(display_colorspace)
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

  bool support_emulation() const override
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

  const ColorSpace *display_colorspace() const override
  {
    return display_colorspace_;
  }
};

}  // namespace blender::ocio
