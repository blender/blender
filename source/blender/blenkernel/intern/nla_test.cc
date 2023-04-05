/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

#include "BLI_listbase.h"

#include "BKE_nla.h"

#include "DNA_anim_types.h"
#include "DNA_nla_types.h"

#include "MEM_guardedalloc.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(nla_strip, BKE_nlastrip_recalculate_blend)
{
  NlaStrip strip{};
  strip.blendin = 4.0;
  strip.blendout = 5.0;
  strip.start = 1;
  strip.end = 10;

  /* Scaling a strip up doesn't affect the blend in/out value. */
  strip.end = 20;
  BKE_nlastrip_recalculate_blend(&strip);
  EXPECT_FLOAT_EQ(strip.blendin, 4.0);
  EXPECT_FLOAT_EQ(strip.blendout, 5.0);

  /* Scaling a strip down affects the blend-in value before the blend-out value. */
  strip.end = 7;
  BKE_nlastrip_recalculate_blend(&strip);
  EXPECT_FLOAT_EQ(strip.blendin, 1.0);
  EXPECT_FLOAT_EQ(strip.blendout, 5.0);

  /* Scaling a strip down to nothing updates the blend in/out values accordingly. */
  strip.end = 1.1;
  BKE_nlastrip_recalculate_blend(&strip);
  EXPECT_FLOAT_EQ(strip.blendin, 0.0);
  EXPECT_FLOAT_EQ(strip.blendout, 0.1);
}

TEST(nla_strip, BKE_nlastrips_add_strip)
{
  ListBase strips{};
  NlaStrip strip1{};
  strip1.start = 0;
  strip1.end = 10;
  strips.first = &strip1;

  NlaStrip strip2{};
  strip2.start = 5;
  strip2.end = 10;

  /* Can't add a null NLA strip to an NLA Track. */
  EXPECT_FALSE(BKE_nlastrips_add_strip(&strips, nullptr));

  /* Can't add an NLA strip to an NLA Track that overlaps another NLA strip. */
  EXPECT_FALSE(BKE_nlastrips_add_strip(&strips, &strip2));

  strip2.start = 15;
  strip2.end = 20;
  /* Can add an NLA strip to an NLA Track that doesn't overlaps another NLA strip. */
  EXPECT_TRUE(BKE_nlastrips_add_strip(&strips, &strip2));
}

TEST(nla_track, BKE_nlatrack_remove_strip)
{
  NlaTrack track{};
  ListBase strips{};
  NlaStrip strip1{};
  strip1.start = 0;
  strip1.end = 10;

  NlaStrip strip2{};
  strip2.start = 11;
  strip2.end = 20;

  /* Add NLA strips to the NLATrack. */
  BKE_nlastrips_add_strip(&strips, &strip1);
  BKE_nlastrips_add_strip(&strips, &strip2);
  track.strips = strips;

  /* Ensure we have 2 strips in the track. */
  EXPECT_EQ(2, BLI_listbase_count(&track.strips));

  BKE_nlatrack_remove_strip(&track, &strip2);
  EXPECT_EQ(1, BLI_listbase_count(&track.strips));
  /* Ensure the correct strip was removed. */
  EXPECT_EQ(-1, BLI_findindex(&track.strips, &strip2));
}

TEST(nla_track, BKE_nlatrack_remove_and_free)
{
  AnimData adt{};

  /* Add NLA tracks to the Animation Data. */
  NlaTrack *track1 = BKE_nlatrack_new_tail(&adt.nla_tracks, false);
  NlaTrack *track2 = BKE_nlatrack_new_tail(&adt.nla_tracks, false);

  /* Ensure we have 2 tracks in the track. */
  EXPECT_EQ(2, BLI_listbase_count(&adt.nla_tracks));

  BKE_nlatrack_remove_and_free(&adt.nla_tracks, track2, false);
  EXPECT_EQ(1, BLI_listbase_count(&adt.nla_tracks));

  /* Ensure the correct track was removed. */
  EXPECT_EQ(-1, BLI_findindex(&adt.nla_tracks, track2));

  /* Free the rest of the tracks, and ensure they are removed. */
  BKE_nlatrack_remove_and_free(&adt.nla_tracks, track1, false);
  EXPECT_EQ(0, BLI_listbase_count(&adt.nla_tracks));
  EXPECT_EQ(-1, BLI_findindex(&adt.nla_tracks, track1));
}

TEST(nla_track, BKE_nlatrack_new_tail)
{
  AnimData adt{};
  NlaTrack *trackB = BKE_nlatrack_new_tail(&adt.nla_tracks, false);
  NlaTrack *trackA = BKE_nlatrack_new_tail(&adt.nla_tracks, false);

  /* Expect that Track B was added before track A. */
  EXPECT_EQ(1, BLI_findindex(&adt.nla_tracks, trackA));
  EXPECT_EQ(0, BLI_findindex(&adt.nla_tracks, trackB));

  /* Free the tracks. */
  BKE_nlatrack_remove_and_free(&adt.nla_tracks, trackA, false);
  BKE_nlatrack_remove_and_free(&adt.nla_tracks, trackB, false);
}

TEST(nla_track, BKE_nlatrack_new_head)
{
  AnimData adt{};
  NlaTrack *trackB = BKE_nlatrack_new_head(&adt.nla_tracks, false);
  NlaTrack *trackA = BKE_nlatrack_new_head(&adt.nla_tracks, false);

  /* Expect that Track A was added before track B. */
  EXPECT_EQ(0, BLI_findindex(&adt.nla_tracks, trackA));
  EXPECT_EQ(1, BLI_findindex(&adt.nla_tracks, trackB));

  /* Free the tracks. */
  BKE_nlatrack_remove_and_free(&adt.nla_tracks, trackA, false);
  BKE_nlatrack_remove_and_free(&adt.nla_tracks, trackB, false);
}

}  // namespace blender::bke::tests
