/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Utility to extend #AssetLibraryReference with C++ functionality (operators, hash function, etc).
 */

#pragma once

#include <cstdint>

#include "DNA_asset_types.h"

namespace blender::ed::asset {

/**
 * Wrapper to add logic to the AssetLibraryReference DNA struct.
 */
class AssetLibraryReferenceWrapper : public AssetLibraryReference {
 public:
  /* Intentionally not `explicit`, allow implicit conversion for convenience. Might have to be
   * NOLINT */
  AssetLibraryReferenceWrapper(const AssetLibraryReference &reference);
  ~AssetLibraryReferenceWrapper() = default;

  friend bool operator==(const AssetLibraryReferenceWrapper &a,
                         const AssetLibraryReferenceWrapper &b);
  uint64_t hash() const;
};

}  // namespace blender::ed::asset
