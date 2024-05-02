/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "BLI_listbase.h"

#include "BLO_read_write.hh"

#include "DNA_defs.h"
#include "DNA_screen_types.h"

#include "asset_shelf.hh"

RegionAssetShelf *RegionAssetShelf::get_from_asset_shelf_region(const ARegion &region)
{
  if (region.regiontype != RGN_TYPE_ASSET_SHELF) {
    /* Should only be called on main asset shelf region. */
    BLI_assert_unreachable();
    return nullptr;
  }
  return static_cast<RegionAssetShelf *>(region.regiondata);
}

RegionAssetShelf *RegionAssetShelf::ensure_from_asset_shelf_region(ARegion &region)
{
  if (region.regiontype != RGN_TYPE_ASSET_SHELF) {
    /* Should only be called on main asset shelf region. */
    BLI_assert_unreachable();
    return nullptr;
  }
  if (!region.regiondata) {
    region.regiondata = MEM_cnew<RegionAssetShelf>("RegionAssetShelf");
  }
  return static_cast<RegionAssetShelf *>(region.regiondata);
}

namespace blender::ed::asset::shelf {

RegionAssetShelf *regiondata_duplicate(const RegionAssetShelf *shelf_regiondata)
{
  static_assert(std::is_trivial_v<RegionAssetShelf>,
                "RegionAssetShelf needs to be trivial to allow freeing with MEM_freeN()");
  RegionAssetShelf *new_shelf_regiondata = MEM_new<RegionAssetShelf>(__func__, *shelf_regiondata);

  BLI_listbase_clear(&new_shelf_regiondata->shelves);
  LISTBASE_FOREACH (const AssetShelf *, shelf, &shelf_regiondata->shelves) {
    AssetShelf *new_shelf = MEM_new<AssetShelf>("duplicate asset shelf",
                                                blender::dna::shallow_copy(*shelf));
    new_shelf->settings = shelf->settings;
    BLI_addtail(&new_shelf_regiondata->shelves, new_shelf);
    if (shelf_regiondata->active_shelf == shelf) {
      new_shelf_regiondata->active_shelf = new_shelf;
    }
  }

  return new_shelf_regiondata;
}

void regiondata_free(RegionAssetShelf *shelf_regiondata)
{
  LISTBASE_FOREACH_MUTABLE (AssetShelf *, shelf, &shelf_regiondata->shelves) {
    MEM_delete(shelf);
  }
  MEM_freeN(shelf_regiondata);
}

void regiondata_blend_write(BlendWriter *writer, const RegionAssetShelf *shelf_regiondata)
{
  BLO_write_struct(writer, RegionAssetShelf, shelf_regiondata);
  LISTBASE_FOREACH (const AssetShelf *, shelf, &shelf_regiondata->shelves) {
    BLO_write_struct(writer, AssetShelf, shelf);
    settings_blend_write(writer, shelf->settings);
  }
}

void regiondata_blend_read_data(BlendDataReader *reader, RegionAssetShelf **shelf_regiondata)
{
  if (!*shelf_regiondata) {
    return;
  }

  BLO_read_struct(reader, RegionAssetShelf, shelf_regiondata);
  if ((*shelf_regiondata)->active_shelf) {
    BLO_read_struct(reader, AssetShelf, &(*shelf_regiondata)->active_shelf);
  }

  BLO_read_struct_list(reader, AssetShelf, &(*shelf_regiondata)->shelves);
  LISTBASE_FOREACH (AssetShelf *, shelf, &(*shelf_regiondata)->shelves) {
    shelf->type = nullptr;
    settings_blend_read_data(reader, shelf->settings);
  }
}

}  // namespace blender::ed::asset::shelf
