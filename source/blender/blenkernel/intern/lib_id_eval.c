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
 * \ingroup bke
 *
 * Contains management of ID's and libraries
 * allocate and free of all library data
 */

#include "DNA_ID.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"

#include "BKE_lib_id.h"
#include "BKE_mesh.h"

/**
 * Copy relatives parameters, from `id` to `id_cow`.
 * Use handle the #ID_RECALC_PARAMETERS tag.
 * \note Keep in sync with #ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW.
 */
void BKE_id_eval_properties_copy(ID *id_cow, ID *id)
{
  const ID_Type id_type = GS(id->name);
  BLI_assert((id_cow->tag & LIB_TAG_COPIED_ON_WRITE) && !(id->tag & LIB_TAG_COPIED_ON_WRITE));
  BLI_assert(ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW(id_type));
  if (id_type == ID_ME) {
    BKE_mesh_copy_parameters((Mesh *)id_cow, (const Mesh *)id);
  }
  else {
    BLI_assert_unreachable();
  }
}
