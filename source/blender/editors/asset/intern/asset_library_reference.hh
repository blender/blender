/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Utility to extend #AssetLibraryReference with C++ functionality (operators, hash function, etc).
 */

#pragma once

#include "BLI_hash.hh"

#include "DNA_asset_types.h"

inline bool operator==(const AssetLibraryReference &a, const AssetLibraryReference &b)
{
  return (a.type == b.type) &&
         ((a.type == ASSET_LIBRARY_CUSTOM) ? (a.custom_library_index == b.custom_library_index) :
                                             true);
}

namespace blender {

template<> struct DefaultHash<AssetLibraryReference> {
  uint64_t operator()(const AssetLibraryReference &value) const
  {
    return get_default_hash(value.type, value.custom_library_index);
  }
};

}  // namespace blender
