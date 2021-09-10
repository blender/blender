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
 * The Original Code is Copyright (C) 2021 Blender Foundation
 * All rights reserved.
 */

#include "BKE_action.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"

#include "BLI_listbase.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(action_groups, ReconstructGroupsWithReordering)
{
  /* Construct an Action with three groups. */
  bAction action = {{nullptr}};
  FCurve groupAcurve1 = {nullptr};
  FCurve groupAcurve2 = {nullptr};
  FCurve groupBcurve1 = {nullptr};
  FCurve groupBcurve2 = {nullptr};
  FCurve groupBcurve3 = {nullptr};
  /* Group C has no curves intentionally. */
  FCurve groupDcurve1 = {nullptr};
  FCurve groupDcurve2 = {nullptr};

  groupAcurve1.rna_path = (char *)"groupAcurve1";
  groupAcurve2.rna_path = (char *)"groupAcurve2";
  groupBcurve1.rna_path = (char *)"groupBcurve1";
  groupBcurve2.rna_path = (char *)"groupBcurve2";
  groupDcurve1.rna_path = (char *)"groupDcurve1";
  groupBcurve3.rna_path = (char *)"groupBcurve3";
  groupDcurve2.rna_path = (char *)"groupDcurve2";

  BLI_addtail(&action.curves, &groupAcurve1);
  BLI_addtail(&action.curves, &groupAcurve2);
  BLI_addtail(&action.curves, &groupBcurve1);
  BLI_addtail(&action.curves, &groupBcurve2);
  BLI_addtail(&action.curves, &groupDcurve1);
  BLI_addtail(&action.curves, &groupBcurve3); /* <-- The error that should be corrected. */
  BLI_addtail(&action.curves, &groupDcurve2);

  /* Introduce another error type, by changing some `prev` pointers. */
  groupBcurve1.prev = nullptr;
  groupBcurve3.prev = &groupBcurve2;
  groupDcurve1.prev = &groupBcurve3;

  bActionGroup groupA = {nullptr};
  bActionGroup groupB = {nullptr};
  bActionGroup groupC = {nullptr};
  bActionGroup groupD = {nullptr};
  strcpy(groupA.name, "groupA");
  strcpy(groupB.name, "groupB");
  strcpy(groupC.name, "groupC");
  strcpy(groupD.name, "groupD");

  BLI_addtail(&action.groups, &groupA);
  BLI_addtail(&action.groups, &groupB);
  BLI_addtail(&action.groups, &groupC);
  BLI_addtail(&action.groups, &groupD);

  groupAcurve1.grp = &groupA;
  groupAcurve2.grp = &groupA;
  groupBcurve1.grp = &groupB;
  groupBcurve2.grp = &groupB;
  groupBcurve3.grp = &groupB;
  groupDcurve1.grp = &groupD;
  groupDcurve2.grp = &groupD;

  groupA.channels.first = &groupAcurve1;
  groupA.channels.last = &groupAcurve2;
  groupB.channels.first = &groupBcurve1;
  groupB.channels.last = &groupBcurve3; /* The last channel in group B, after group C curve 1. */
  groupD.channels.first = &groupDcurve1;
  groupD.channels.last = &groupDcurve2;

  EXPECT_EQ(groupA.channels.first, &groupAcurve1);
  EXPECT_EQ(groupA.channels.last, &groupAcurve2);
  EXPECT_EQ(groupB.channels.first, &groupBcurve1);
  EXPECT_EQ(groupB.channels.last, &groupBcurve3);
  EXPECT_EQ(groupC.channels.first, nullptr);
  EXPECT_EQ(groupC.channels.last, nullptr);
  EXPECT_EQ(groupD.channels.first, &groupDcurve1);
  EXPECT_EQ(groupD.channels.last, &groupDcurve2);

  BKE_action_groups_reconstruct(&action);

  EXPECT_EQ(action.curves.first, &groupAcurve1);
  EXPECT_EQ(action.curves.last, &groupDcurve2);

  EXPECT_EQ(groupA.prev, nullptr);
  EXPECT_EQ(groupB.prev, &groupA);
  EXPECT_EQ(groupC.prev, &groupB);
  EXPECT_EQ(groupD.prev, &groupC);

  EXPECT_EQ(groupA.next, &groupB);
  EXPECT_EQ(groupB.next, &groupC);
  EXPECT_EQ(groupC.next, &groupD);
  EXPECT_EQ(groupD.next, nullptr);

  EXPECT_EQ(groupA.channels.first, &groupAcurve1);
  EXPECT_EQ(groupA.channels.last, &groupAcurve2);
  EXPECT_EQ(groupB.channels.first, &groupBcurve1);
  EXPECT_EQ(groupB.channels.last, &groupBcurve3);
  EXPECT_EQ(groupC.channels.first, nullptr);
  EXPECT_EQ(groupC.channels.last, nullptr);
  EXPECT_EQ(groupD.channels.first, &groupDcurve1);
  EXPECT_EQ(groupD.channels.last, &groupDcurve2);

  EXPECT_EQ(groupAcurve1.prev, nullptr);
  EXPECT_EQ(groupAcurve2.prev, &groupAcurve1);
  EXPECT_EQ(groupBcurve1.prev, &groupAcurve2);
  EXPECT_EQ(groupBcurve2.prev, &groupBcurve1);
  EXPECT_EQ(groupBcurve3.prev, &groupBcurve2);
  EXPECT_EQ(groupDcurve1.prev, &groupBcurve3);
  EXPECT_EQ(groupDcurve2.prev, &groupDcurve1);

  EXPECT_EQ(groupAcurve1.next, &groupAcurve2);
  EXPECT_EQ(groupAcurve2.next, &groupBcurve1);
  EXPECT_EQ(groupBcurve1.next, &groupBcurve2);
  EXPECT_EQ(groupBcurve2.next, &groupBcurve3);
  EXPECT_EQ(groupBcurve3.next, &groupDcurve1);
  EXPECT_EQ(groupDcurve1.next, &groupDcurve2);
  EXPECT_EQ(groupDcurve2.next, nullptr);
}

}  // namespace blender::bke::tests
