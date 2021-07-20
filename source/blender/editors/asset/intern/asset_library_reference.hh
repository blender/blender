/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
