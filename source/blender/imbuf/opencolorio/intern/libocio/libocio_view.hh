/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_string_ref.hh"

#include "OCIO_scope.hh"
#include "OCIO_view.hh"

#include "libocio_colorspace.hh"

namespace blender::ocio {

struct ScopeInfo;

class LibOCIOView : public View {
  StringRefNull name_;
  StringRefNull description_;
  bool is_hdr_ = false;
  bool is_data_ = false;
  bool support_emulation_ = false;
  Gamut gamut_ = Gamut::Unknown;
  TransferFunction transfer_function_ = TransferFunction::Unknown;
  const LibOCIOColorSpace *display_colorspace_ = nullptr;
  mutable std::unique_ptr<ScopeInfo> scope_info_;

 public:
  LibOCIOView(const int index,
              const StringRefNull name,
              const StringRefNull description,
              const bool is_hdr,
              const bool is_data,
              const bool support_emulation,
              const Gamut gamut,
              const TransferFunction transfer_function,
              const LibOCIOColorSpace *display_colorspace)
      : name_(name),
        description_(description),
        is_hdr_(is_hdr),
        is_data_(is_data),
        support_emulation_(support_emulation),
        gamut_(gamut),
        transfer_function_(transfer_function),
        display_colorspace_(display_colorspace)
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

  bool support_emulation() const override
  {
    return support_emulation_;
  }

  Gamut gamut() const override
  {
    return gamut_;
  }

  TransferFunction transfer_function() const override
  {
    return transfer_function_;
  }

  const ColorSpace *display_colorspace() const override
  {
    return display_colorspace_;
  }

  /** Max luminance of the view transform, or 0 if no maximum found. */
  int max_nits() const override;

  /**
   * Get scope display info for this view transform.
   */
  const ScopeInfo &scope_info() const override;

  /**
   * Append transforms to convert from the display's colorspace to normalized space
   * suitable for plotting scopes.
   */
  void append_scope_space_transforms(OCIO_NAMESPACE::GroupTransformRcPtr &group,
                                     StringRefNull lin_cie_xyz_d65_name) const;

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOView");
};

}  // namespace blender::ocio
