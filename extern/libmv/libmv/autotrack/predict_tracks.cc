// Copyright (c) 2014 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Author: mierle@gmail.com (Keir Mierle)

#include "libmv/autotrack/marker.h"
#include "libmv/autotrack/predict_tracks.h"
#include "libmv/autotrack/tracks.h"
#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/tracking/kalman_filter.h"

namespace mv {

namespace {

using libmv::vector;
using libmv::Vec2;

// Implied time delta between steps. Set empirically by tweaking and seeing
// what numbers did best at prediction.
const double dt = 3.8;

// State transition matrix.

// The states for predicting a track are as follows:
//
//   0 - X position
//   1 - X velocity
//   2 - X acceleration
//   3 - Y position
//   4 - Y velocity
//   5 - Y acceleration
//
// Note that in the velocity-only state transition matrix, the acceleration
// component is ignored; so technically the system could be modelled with only
// 4 states instead of 6. For ease of implementation, this keeps order 6.

// Choose one or the other model from below (velocity or acceleration).

// For a typical system having constant velocity. This gives smooth-appearing
// predictions, but they are not always as accurate.
const double velocity_state_transition_data[] = {
  1, dt,       0,  0,  0,        0,
  0,  1,       0,  0,  0,        0,
  0,  0,       1,  0,  0,        0,
  0,  0,       0,  1, dt,        0,
  0,  0,       0,  0,  1,        0,
  0,  0,       0,  0,  0,        1
};

// This 3rd-order system also models acceleration. This makes for "jerky"
// predictions, but that tend to be more accurate.
const double acceleration_state_transition_data[] = {
  1, dt, dt*dt/2,  0,  0,        0,
  0,  1,      dt,  0,  0,        0,
  0,  0,       1,  0,  0,        0,
  0,  0,       0,  1, dt,  dt*dt/2,
  0,  0,       0,  0,  1,       dt,
  0,  0,       0,  0,  0,        1
};

// This system (attempts) to add an angular velocity component. However, it's
// total junk.
const double angular_state_transition_data[] = {
  1, dt,     -dt,  0,  0,        0,   // Position x
  0,  1,       0,  0,  0,        0,   // Velocity x
  0,  0,       1,  0,  0,        0,   // Angular momentum
  0,  0,      dt,  1, dt,        0,   // Position y
  0,  0,       0,  0,  1,        0,   // Velocity y
  0,  0,       0,  0,  0,        1    // Ignored
};

const double* state_transition_data = velocity_state_transition_data;

// Observation matrix.
const double observation_data[] = {
  1., 0., 0., 0., 0., 0.,
  0., 0., 0., 1., 0., 0.
};

// Process covariance.
const double process_covariance_data[] = {
 35,  0,  0,  0,  0,  0,
  0,  5,  0,  0,  0,  0,
  0,  0,  5,  0,  0,  0,
  0,  0,  0, 35,  0,  0,
  0,  0,  0,  0,  5,  0,
  0,  0,  0,  0,  0,  5
};

// Process covariance.
const double measurement_covariance_data[] = {
  0.01,  0.00,
  0.00,  0.01,
};

// Initial covariance.
const double initial_covariance_data[] = {
 10,  0,  0,  0,  0,  0,
  0,  1,  0,  0,  0,  0,
  0,  0,  1,  0,  0,  0,
  0,  0,  0, 10,  0,  0,
  0,  0,  0,  0,  1,  0,
  0,  0,  0,  0,  0,  1
};

typedef mv::KalmanFilter<double, 6, 2> TrackerKalman;

TrackerKalman filter(state_transition_data,
                     observation_data,
                     process_covariance_data,
                     measurement_covariance_data);

bool OrderByFrameLessThan(const Marker* a, const Marker* b) {
  if (a->frame == b->frame) {
    if (a->clip == b->clip) {
      return a->track < b->track;
    }
    return a->clip < b->clip;
  }
  return a->frame < b-> frame;
}

// Predicted must be after the previous markers (in the frame numbering sense).
void RunPrediction(const vector<Marker*> previous_markers,
                   Marker* predicted_marker) {
  TrackerKalman::State state;
  state.mean << previous_markers[0]->center.x(), 0, 0,
                previous_markers[0]->center.y(), 0, 0;
  state.covariance = Eigen::Matrix<double, 6, 6, Eigen::RowMajor>(
      initial_covariance_data);

  int current_frame = previous_markers[0]->frame;
  int target_frame = predicted_marker->frame;

  bool predict_forward = current_frame < target_frame;
  int frame_delta = predict_forward ? 1 : -1;

  for (int i = 1; i < previous_markers.size(); ++i) {
    // Step forward predicting the state until it is on the current marker.
    int predictions = 0;
    for (;
         current_frame != previous_markers[i]->frame;
         current_frame += frame_delta) {
      filter.Step(&state);
      predictions++;
      LG << "Predicted point (frame " << current_frame << "): "
         << state.mean(0) << ", " << state.mean(3);
    }
    // Log the error -- not actually used, but interesting.
    Vec2 error = previous_markers[i]->center.cast<double>() -
                 Vec2(state.mean(0), state.mean(3));
    LG << "Prediction error for " << predictions << " steps: ("
       << error.x() << ", " << error.y() << "); norm: " << error.norm();
    // Now that the state is predicted in the current frame, update the state
    // based on the measurement from the current frame.
    filter.Update(previous_markers[i]->center.cast<double>(),
                  Eigen::Matrix<double, 2, 2, Eigen::RowMajor>(
                      measurement_covariance_data),
                  &state);
    LG << "Updated point: " << state.mean(0) << ", " << state.mean(3);
  }
  // At this point as all the prediction that's possible is done. Finally
  // predict until the target frame.
  for (; current_frame != target_frame; current_frame += frame_delta) {
    filter.Step(&state);
    LG << "Final predicted point (frame " << current_frame << "): "
       << state.mean(0) << ", " << state.mean(3);
  }

  // The x and y positions are at 0 and 3; ignore acceleration and velocity.
  predicted_marker->center.x() = state.mean(0);
  predicted_marker->center.y() = state.mean(3);

  // Take the patch from the last marker then shift it to match the prediction.
  const Marker& last_marker = *previous_markers[previous_markers.size() - 1];
  predicted_marker->patch = last_marker.patch;
  Vec2f delta = predicted_marker->center - last_marker.center;
  for (int i = 0; i < 4; ++i) {
    predicted_marker->patch.coordinates.row(i) += delta;
  }

  // Alter the search area as well so it always corresponds to the center.
  predicted_marker->search_region = last_marker.search_region;
  predicted_marker->search_region.Offset(delta);
}

}  // namespace

bool PredictMarkerPosition(const Tracks& tracks, Marker* marker) {
  // Get all markers for this clip and track.
  vector<Marker> markers;
  tracks.GetMarkersForTrackInClip(marker->clip, marker->track, &markers);

  if (markers.empty()) {
    LG << "No markers to predict from for " << *marker;
    return false;
  }

  // Order the markers by frame within the clip.
  vector<Marker*> boxed_markers(markers.size());
  for (int i = 0; i < markers.size(); ++i) {
    boxed_markers[i] = &markers[i];
  }
  std::sort(boxed_markers.begin(), boxed_markers.end(), OrderByFrameLessThan);

  // Find the insertion point for this marker among the returned ones.
  int insert_at = -1;      // If we find the exact frame
  int insert_before = -1;  // Otherwise...
  for (int i = 0; i < boxed_markers.size(); ++i) {
    if (boxed_markers[i]->frame == marker->frame) {
      insert_at = i;
      break;
    }
    if (boxed_markers[i]->frame > marker->frame) {
      insert_before = i;
      break;
    }
  }

  // Forward starts at the marker or insertion point, and goes forward.
  int forward_scan_begin, forward_scan_end;

  // Backward scan starts at the marker or insertion point, and goes backward.
  int backward_scan_begin, backward_scan_end;

  // Determine the scanning ranges.
  if (insert_at == -1 && insert_before == -1) {
    // Didn't find an insertion point except the end.
    forward_scan_begin = forward_scan_end = 0;
    backward_scan_begin = markers.size() - 1;
    backward_scan_end = 0;
  } else if (insert_at != -1) {
    // Found existing marker; scan before and after it.
    forward_scan_begin = insert_at + 1;
    forward_scan_end = markers.size() - 1;;
    backward_scan_begin = insert_at - 1;
    backward_scan_end = 0;
  } else {
    // Didn't find existing marker but found an insertion point.
    forward_scan_begin = insert_before;
    forward_scan_end = markers.size() - 1;;
    backward_scan_begin = insert_before - 1;
    backward_scan_end = 0;
  }

  const int num_consecutive_needed = 2;

  if (forward_scan_begin <= forward_scan_end &&
      forward_scan_end - forward_scan_begin > num_consecutive_needed) {
    // TODO(keir): Finish this.
  }

  bool predict_forward = false;
  if (backward_scan_end <= backward_scan_begin) {
    // TODO(keir): Add smarter handling and detecting of consecutive frames!
    predict_forward = true;
  }

  const int max_frames_to_predict_from = 20;
  if (predict_forward) {
    if (backward_scan_begin - backward_scan_end < num_consecutive_needed) {
      // Not enough information to do a prediction.
      LG << "Predicting forward impossible, not enough information";
      return false;
    }
    LG << "Predicting forward";
    int predict_begin =
        std::max(backward_scan_begin - max_frames_to_predict_from, 0);
    int predict_end = backward_scan_begin;
    vector<Marker*> previous_markers;
    for (int i = predict_begin; i <= predict_end; ++i) {
      previous_markers.push_back(boxed_markers[i]);
    }
    RunPrediction(previous_markers, marker);
    return true;
  } else {
    if (forward_scan_end - forward_scan_begin < num_consecutive_needed) {
      // Not enough information to do a prediction.
      LG << "Predicting backward impossible, not enough information";
      return false;
    }
    LG << "Predicting backward";
    int predict_begin =
        std::min(forward_scan_begin + max_frames_to_predict_from,
                 forward_scan_end);
    int predict_end = forward_scan_begin;
    vector<Marker*> previous_markers;
    for (int i = predict_begin; i >= predict_end; --i) {
      previous_markers.push_back(boxed_markers[i]);
    }
    RunPrediction(previous_markers, marker);
    return false;
  }

}

}  // namespace mv
