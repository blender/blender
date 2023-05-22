/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "BLI_listbase.h"

#include "BLO_read_write.h"

#include "DNA_defs.h"
#include "DNA_screen_types.h"

#include "asset_shelf.hh"

#include "ED_asset_shelf.h"

using namespace blender::ed::asset::shelf;

AssetShelfHook *ED_asset_shelf_hook_duplicate(const AssetShelfHook *hook)
{
  static_assert(std::is_trivial_v<AssetShelfHook>,
                "AssetShelfHook needs to be trivial to allow freeing with MEM_freeN()");
  AssetShelfHook *new_hook = MEM_new<AssetShelfHook>(__func__, *hook);

  BLI_listbase_clear(&new_hook->shelves);
  LISTBASE_FOREACH (const AssetShelf *, shelf, &hook->shelves) {
    AssetShelf *new_shelf = MEM_new<AssetShelf>("duplicate asset shelf",
                                                blender::dna::shallow_copy(*shelf));
    BLI_addtail(&new_hook->shelves, new_shelf);
  }

  return new_hook;
}

void ED_asset_shelf_hook_free(AssetShelfHook **hook)
{
  LISTBASE_FOREACH_MUTABLE (AssetShelf *, shelf, &(*hook)->shelves) {
    MEM_delete(shelf);
  }
  MEM_SAFE_FREE(*hook);
}

void ED_asset_shelf_hook_blend_write(BlendWriter *writer, const AssetShelfHook *hook)
{
  BLO_write_struct(writer, AssetShelfHook, hook);
  LISTBASE_FOREACH (const AssetShelf *, shelf, &hook->shelves) {
    BLO_write_struct(writer, AssetShelf, shelf);
    settings_blend_write(writer, shelf->settings);
  }
}

void ED_asset_shelf_hook_blend_read_data(BlendDataReader *reader, AssetShelfHook **hook)
{
  if (!*hook) {
    return;
  }

  BLO_read_data_address(reader, hook);
  if ((*hook)->active_shelf) {
    BLO_read_data_address(reader, &(*hook)->active_shelf);
  }

  BLO_read_list(reader, &(*hook)->shelves);
  LISTBASE_FOREACH (AssetShelf *, shelf, &(*hook)->shelves) {
    shelf->type = nullptr;
    settings_blend_read_data(reader, shelf->settings);
  }
}
