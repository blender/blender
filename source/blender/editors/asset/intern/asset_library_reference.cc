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

#include "BLI_hash.hh"

#include "asset_library_reference.hh"

namespace blender::ed::asset {

AssetLibraryReferenceWrapper::AssetLibraryReferenceWrapper(const AssetLibraryReference &reference)
    : AssetLibraryReference(reference)
{
}

bool operator==(const AssetLibraryReferenceWrapper &a, const AssetLibraryReferenceWrapper &b)
{
  return (a.type == b.type) && (a.type == ASSET_LIBRARY_CUSTOM) ?
             (a.custom_library_index == b.custom_library_index) :
             true;
}

uint64_t AssetLibraryReferenceWrapper::hash() const
{
  uint64_t hash1 = DefaultHash<decltype(type)>{}(type);
  if (type != ASSET_LIBRARY_CUSTOM) {
    return hash1;
  }

  uint64_t hash2 = DefaultHash<decltype(custom_library_index)>{}(custom_library_index);
  return hash1 ^ (hash2 * 33); /* Copied from DefaultHash for std::pair. */
}

}  // namespace blender::ed::asset
