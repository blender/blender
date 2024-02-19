/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BKE_lib_remap.hh"

#include "BLI_string.h"

#include "DNA_ID.h"

using namespace blender::bke::id;

namespace blender::bke::id::tests {

TEST(lib_id_remapper, unavailable)
{
  ID id1;
  ID *idp = &id1;

  IDRemapper remapper;
  IDRemapperApplyResult result = remapper.apply(&idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNAVAILABLE);
}

TEST(lib_id_remapper, not_mappable)
{
  ID *idp = nullptr;

  IDRemapper remapper;
  IDRemapperApplyResult result = remapper.apply(&idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE);
}

TEST(lib_id_remapper, mapped)
{
  ID id1;
  ID id2;
  ID *idp = &id1;
  STRNCPY(id1.name, "OB1");
  STRNCPY(id2.name, "OB2");

  IDRemapper remapper;
  remapper.add(&id1, &id2);
  IDRemapperApplyResult result = remapper.apply(&idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_REMAPPED);
  EXPECT_EQ(idp, &id2);
}

TEST(lib_id_remapper, unassigned)
{
  ID id1;
  ID *idp = &id1;
  STRNCPY(id1.name, "OB2");

  IDRemapper remapper;
  remapper.add(&id1, nullptr);
  IDRemapperApplyResult result = remapper.apply(&idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNASSIGNED);
  EXPECT_EQ(idp, nullptr);
}

TEST(lib_id_remapper, unassign_when_mapped_to_self)
{
  ID id_self;
  ID id1;
  ID id2;
  ID *idp;

  STRNCPY(id_self.name, "OBSelf");
  STRNCPY(id1.name, "OB1");
  STRNCPY(id2.name, "OB2");

  /* Default mapping behavior. Should just remap to id2. */
  idp = &id1;
  IDRemapper remapper;
  remapper.add(&id1, &id2);
  IDRemapperApplyResult result = remapper.apply(
      &idp, ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF, &id_self);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_REMAPPED);
  EXPECT_EQ(idp, &id2);

  /* Default mapping behavior. Should unassign. */
  idp = &id1;
  remapper.clear();
  remapper.add(&id1, nullptr);
  result = remapper.apply(&idp, ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF, &id_self);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNASSIGNED);
  EXPECT_EQ(idp, nullptr);

  /* Unmap when remapping to self behavior. Should unassign. */
  idp = &id1;
  remapper.clear();
  remapper.add(&id1, &id_self);
  result = remapper.apply(&idp, ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF, &id_self);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNASSIGNED);
  EXPECT_EQ(idp, nullptr);
}

}  // namespace blender::bke::id::tests
