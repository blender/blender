/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_action.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"

#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"

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

namespace {

/* Allocate fcu->bezt, and also return a unique_ptr to it for easily freeing the memory. */
std::unique_ptr<BezTriple[]> allocate_keyframes(FCurve *fcu, const size_t num_keyframes)
{
  auto bezt_uptr = std::make_unique<BezTriple[]>(num_keyframes);
  fcu->bezt = bezt_uptr.get();
  return bezt_uptr;
}

/* Append keyframe, assumes that fcu->bezt is allocated and has enough space. */
void add_keyframe(FCurve *fcu, float x, float y)
{
  /* The insert_keyframe functions are in the editors, so we cannot link to those here. */
  BezTriple the_keyframe;
  memset(&the_keyframe, 0, sizeof(the_keyframe));

  /* Copied from insert_vert_fcurve() in keyframing.c. */
  the_keyframe.vec[0][0] = x - 1.0f;
  the_keyframe.vec[0][1] = y;
  the_keyframe.vec[1][0] = x;
  the_keyframe.vec[1][1] = y;
  the_keyframe.vec[2][0] = x + 1.0f;
  the_keyframe.vec[2][1] = y;

  memcpy(&fcu->bezt[fcu->totvert], &the_keyframe, sizeof(the_keyframe));
  fcu->totvert++;
}

}  // namespace

TEST(action_assets, BKE_action_has_single_frame)
{
  /* NULL action. */
  EXPECT_FALSE(BKE_action_has_single_frame(nullptr)) << "NULL Action cannot have a single frame.";

  /* No FCurves. */
  {
    const bAction empty = {{nullptr}};
    EXPECT_FALSE(BKE_action_has_single_frame(&empty))
        << "Action without FCurves cannot have a single frame.";
  }

  /* One curve with one key. */
  {
    FCurve fcu = {nullptr};
    std::unique_ptr<BezTriple[]> bezt = allocate_keyframes(&fcu, 1);
    add_keyframe(&fcu, 1.0f, 2.0f);

    bAction action = {{nullptr}};
    BLI_addtail(&action.curves, &fcu);

    EXPECT_TRUE(BKE_action_has_single_frame(&action))
        << "Action with one FCurve and one key should have single frame.";
  }

  /* Two curves with one key each. */
  {
    FCurve fcu1 = {nullptr};
    FCurve fcu2 = {nullptr};
    std::unique_ptr<BezTriple[]> bezt1 = allocate_keyframes(&fcu1, 1);
    std::unique_ptr<BezTriple[]> bezt2 = allocate_keyframes(&fcu2, 1);
    add_keyframe(&fcu1, 1.0f, 327.0f);
    add_keyframe(&fcu2, 1.0f, 47.0f); /* Same X-coordinate as the other one. */

    bAction action = {{nullptr}};
    BLI_addtail(&action.curves, &fcu1);
    BLI_addtail(&action.curves, &fcu2);

    EXPECT_TRUE(BKE_action_has_single_frame(&action))
        << "Two FCurves with keys on the same frame should have single frame.";

    /* Modify the 2nd curve so it's keyed on a different frame. */
    fcu2.bezt[0].vec[1][0] = 2.0f;
    EXPECT_FALSE(BKE_action_has_single_frame(&action))
        << "Two FCurves with keys on different frames should have animation.";
  }

  /* One curve with two keys. */
  {
    FCurve fcu = {nullptr};
    std::unique_ptr<BezTriple[]> bezt = allocate_keyframes(&fcu, 2);
    add_keyframe(&fcu, 1.0f, 2.0f);
    add_keyframe(&fcu, 2.0f, 2.5f);

    bAction action = {{nullptr}};
    BLI_addtail(&action.curves, &fcu);

    EXPECT_FALSE(BKE_action_has_single_frame(&action))
        << "Action with one FCurve and two keys must have animation.";
  }
}

}  // namespace blender::bke::tests
