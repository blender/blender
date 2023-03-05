/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "DNA_tracking_types.h"

#include "BKE_tracking.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

namespace blender {

namespace {

class TrackingTest : public ::testing::Test {
 protected:
  MovieTrackingMarker *addMarkerToTrack(MovieTrackingTrack *track,
                                        int frame_number,
                                        const float2 &position = float2(0.0f, 0.0f),
                                        int flag = 0)
  {
    MovieTrackingMarker marker = {{0.0f}};
    copy_v2_v2(marker.pos, position);
    marker.framenr = frame_number;
    marker.flag = flag;
    return BKE_tracking_marker_insert(track, &marker);
  }
};

}  // namespace

TEST_F(TrackingTest, BKE_tracking_marker_get)
{
  {
    MovieTrackingTrack track = {nullptr};

    addMarkerToTrack(&track, 10);

    EXPECT_EQ(BKE_tracking_marker_get(&track, 0), &track.markers[0]);
    EXPECT_EQ(BKE_tracking_marker_get(&track, 10), &track.markers[0]);
    EXPECT_EQ(BKE_tracking_marker_get(&track, 20), &track.markers[0]);

    BKE_tracking_track_free(&track);
  }

  {
    MovieTrackingTrack track = {nullptr};

    addMarkerToTrack(&track, 1);
    addMarkerToTrack(&track, 10);

    {
      const MovieTrackingMarker *marker = BKE_tracking_marker_get(&track, 1);
      EXPECT_NE(marker, nullptr);
      EXPECT_EQ(marker->framenr, 1);
    }

    {
      const MovieTrackingMarker *marker = BKE_tracking_marker_get(&track, 5);
      EXPECT_NE(marker, nullptr);
      EXPECT_EQ(marker->framenr, 1);
    }

    BKE_tracking_track_free(&track);
  }

  {
    {
      MovieTrackingTrack track = {nullptr};

      addMarkerToTrack(&track, 1);
      addMarkerToTrack(&track, 2);
      addMarkerToTrack(&track, 10);

      EXPECT_EQ(BKE_tracking_marker_get(&track, 0), &track.markers[0]);
      EXPECT_EQ(BKE_tracking_marker_get(&track, 1), &track.markers[0]);
      EXPECT_EQ(BKE_tracking_marker_get(&track, 2), &track.markers[1]);
      EXPECT_EQ(BKE_tracking_marker_get(&track, 3), &track.markers[1]);
      EXPECT_EQ(BKE_tracking_marker_get(&track, 9), &track.markers[1]);
      EXPECT_EQ(BKE_tracking_marker_get(&track, 10), &track.markers[2]);
      EXPECT_EQ(BKE_tracking_marker_get(&track, 11), &track.markers[2]);

      BKE_tracking_track_free(&track);
    }
  }
}

TEST_F(TrackingTest, BKE_tracking_marker_get_exact)
{
  MovieTrackingTrack track = {nullptr};

  addMarkerToTrack(&track, 1);
  addMarkerToTrack(&track, 10);

  {
    const MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(&track, 1);
    EXPECT_NE(marker, nullptr);
    EXPECT_EQ(marker->framenr, 1);
  }

  {
    const MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(&track, 5);
    EXPECT_EQ(marker, nullptr);
  }

  BKE_tracking_track_free(&track);
}

TEST_F(TrackingTest, BKE_tracking_marker_get_interpolated)
{
  /* Simple case, no disabled markers in a way. */
  {
    MovieTrackingTrack track = {nullptr};

    addMarkerToTrack(&track, 1, float2(1.0f, 5.0f));
    addMarkerToTrack(&track, 10, float2(2.0f, 1.0f));

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 1, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.framenr, 1);
      EXPECT_V2_NEAR(interpolated_marker.pos, float2(1.0f, 5.0f), 1e-6f);
    }

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 10, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.framenr, 10);
      EXPECT_V2_NEAR(interpolated_marker.pos, float2(2.0f, 1.0f), 1e-6f);
    }

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 4, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.framenr, 4);
      EXPECT_V2_NEAR(interpolated_marker.pos, float2(1.3333333f, 3.6666666f), 1e-6f);
    }

    BKE_tracking_track_free(&track);
  }

  /* More comprehensive test, which resembles real-life tracking scenario better. */
  {
    MovieTrackingTrack track = {nullptr};

    addMarkerToTrack(&track, 1, float2(1.0f, 5.0f));
    addMarkerToTrack(&track, 2, float2(0.0f, 0.0f), MARKER_DISABLED);
    addMarkerToTrack(&track, 9, float2(0.0f, 0.0f), MARKER_DISABLED);
    addMarkerToTrack(&track, 10, float2(2.0f, 1.0f));

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 1, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.framenr, 1);
      EXPECT_V2_NEAR(interpolated_marker.pos, float2(1.0f, 5.0f), 1e-6f);
    }

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 10, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.framenr, 10);
      EXPECT_V2_NEAR(interpolated_marker.pos, float2(2.0f, 1.0f), 1e-6f);
    }

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 4, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.framenr, 4);
      EXPECT_V2_NEAR(interpolated_marker.pos, float2(1.3333333f, 3.6666666f), 1e-6f);
    }

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 9, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.framenr, 9);
      EXPECT_V2_NEAR(interpolated_marker.pos, float2(1.888888f, 1.4444444f), 1e-6f);
    }

    BKE_tracking_track_free(&track);
  }

  /* Tracked/keyframed flag check. */
  {
    MovieTrackingTrack track = {nullptr};

    addMarkerToTrack(&track, 1, float2(1.0f, 5.0f), MARKER_TRACKED);
    addMarkerToTrack(&track, 10, float2(2.0f, 1.0f), MARKER_TRACKED);

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 1, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.flag, MARKER_TRACKED);
    }

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 10, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.flag, MARKER_TRACKED);
    }

    {
      MovieTrackingMarker interpolated_marker;
      EXPECT_TRUE(BKE_tracking_marker_get_interpolated(&track, 4, &interpolated_marker));
      EXPECT_EQ(interpolated_marker.flag, 0);
    }

    BKE_tracking_track_free(&track);
  }
}

}  // namespace blender
