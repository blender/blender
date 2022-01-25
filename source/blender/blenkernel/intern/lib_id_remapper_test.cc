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
 *
 * The Original Code is Copyright (C) 2022 by Blender Foundation.
 */

#include "testing/testing.h"

#include "BKE_lib_remap.h"

#include "BLI_string.h"

#include "DNA_ID.h"

namespace blender::bke::id::remapper::tests {

TEST(lib_id_remapper, unavailable)
{
  ID id1;
  ID *idp = &id1;

  IDRemapper *remapper = BKE_id_remapper_create();
  IDRemapperApplyResult result = BKE_id_remapper_apply(remapper, &idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNAVAILABLE);

  BKE_id_remapper_free(remapper);
}

TEST(lib_id_remapper, not_mappable)
{
  ID *idp = nullptr;

  IDRemapper *remapper = BKE_id_remapper_create();
  IDRemapperApplyResult result = BKE_id_remapper_apply(remapper, &idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE);

  BKE_id_remapper_free(remapper);
}

TEST(lib_id_remapper, mapped)
{
  ID id1;
  ID id2;
  ID *idp = &id1;
  BLI_strncpy(id1.name, "OB1", sizeof(id1.name));
  BLI_strncpy(id2.name, "OB2", sizeof(id2.name));

  IDRemapper *remapper = BKE_id_remapper_create();
  BKE_id_remapper_add(remapper, &id1, &id2);
  IDRemapperApplyResult result = BKE_id_remapper_apply(remapper, &idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_REMAPPED);
  EXPECT_EQ(idp, &id2);

  BKE_id_remapper_free(remapper);
}

TEST(lib_id_remapper, unassigned)
{
  ID id1;
  ID *idp = &id1;

  IDRemapper *remapper = BKE_id_remapper_create();
  BKE_id_remapper_add(remapper, &id1, nullptr);
  IDRemapperApplyResult result = BKE_id_remapper_apply(remapper, &idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNASSIGNED);
  EXPECT_EQ(idp, nullptr);

  BKE_id_remapper_free(remapper);
}

}  // namespace blender::bke::id::remapper::tests
