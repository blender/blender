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
 */

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"

#include "DNA_ID.h"

#include "UI_interface_icons.h"

#include "RNA_access.h"

#include "ED_asset.h"

bool ED_asset_mark_id(const bContext *C, ID *id)
{
  if (id->asset_data) {
    return false;
  }
  if (!BKE_id_can_be_asset(id)) {
    return false;
  }

  id_fake_user_set(id);

  id->asset_data = BKE_asset_metadata_create();

  UI_icon_render_id(C, nullptr, id, ICON_SIZE_PREVIEW, true);

  return true;
}

bool ED_asset_clear_id(ID *id)
{
  if (!id->asset_data) {
    return false;
  }
  BKE_asset_metadata_free(&id->asset_data);
  /* Don't clear fake user here, there's no guarantee that it was actually set by
   * #ED_asset_mark_id(), it might have been something/someone else. */

  return true;
}

bool ED_asset_can_make_single_from_context(const bContext *C)
{
  /* Context needs a "id" pointer to be set for #ASSET_OT_mark()/#ASSET_OT_clear() to use. */
  return CTX_data_pointer_get_type_silent(C, "id", &RNA_ID).data != nullptr;
}
