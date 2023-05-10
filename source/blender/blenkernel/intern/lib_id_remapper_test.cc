/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

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
  STRNCPY(id1.name, "OB1");
  STRNCPY(id2.name, "OB2");

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
  STRNCPY(id1.name, "OB2");

  IDRemapper *remapper = BKE_id_remapper_create();
  BKE_id_remapper_add(remapper, &id1, nullptr);
  IDRemapperApplyResult result = BKE_id_remapper_apply(remapper, &idp, ID_REMAP_APPLY_DEFAULT);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNASSIGNED);
  EXPECT_EQ(idp, nullptr);

  BKE_id_remapper_free(remapper);
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
  IDRemapper *remapper = BKE_id_remapper_create();
  BKE_id_remapper_add(remapper, &id1, &id2);
  IDRemapperApplyResult result = BKE_id_remapper_apply_ex(
      remapper, &idp, ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF, &id_self);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_REMAPPED);
  EXPECT_EQ(idp, &id2);

  /* Default mapping behavior. Should unassign. */
  idp = &id1;
  BKE_id_remapper_clear(remapper);
  BKE_id_remapper_add(remapper, &id1, nullptr);
  result = BKE_id_remapper_apply_ex(
      remapper, &idp, ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF, &id_self);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNASSIGNED);
  EXPECT_EQ(idp, nullptr);

  /* Unmap when remapping to self behavior. Should unassign. */
  idp = &id1;
  BKE_id_remapper_clear(remapper);
  BKE_id_remapper_add(remapper, &id1, &id_self);
  result = BKE_id_remapper_apply_ex(
      remapper, &idp, ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF, &id_self);
  EXPECT_EQ(result, ID_REMAP_RESULT_SOURCE_UNASSIGNED);
  EXPECT_EQ(idp, nullptr);
  BKE_id_remapper_free(remapper);
}

}  // namespace blender::bke::id::remapper::tests
