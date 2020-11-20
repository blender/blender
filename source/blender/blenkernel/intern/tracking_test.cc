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
