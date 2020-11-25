/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "DNA_tracking_types.h"

#include "BKE_tracking.h"

namespace {

class TrackingTest : public ::testing::Test {
 protected:
  MovieTrackingMarker *addMarkerToTrack(MovieTrackingTrack *track, int frame_number)
  {
    MovieTrackingMarker marker = {{0.0f}};
    marker.framenr = frame_number;
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
