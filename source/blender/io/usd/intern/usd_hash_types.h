/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_hash.hh"

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/valueTypeName.h>

namespace blender {
template<> struct DefaultHash<pxr::SdfValueTypeName> {
  uint64_t operator()(const pxr::SdfValueTypeName &value) const
  {
    return value.GetHash();
  }
};

template<> struct DefaultHash<pxr::TfToken> {
  uint64_t operator()(const pxr::TfToken &value) const
  {
    return value.Hash();
  }
};
}  // namespace blender
