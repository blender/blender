/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_TouchWin32.hh"

#include <cmath>
#include <memory>

#include "GHOST_EventButton.hh"
#include "GHOST_EventCursor.hh"
#include "GHOST_EventTrackpad.hh"
#include "GHOST_SystemWin32.hh"
#include "GHOST_WindowWin32.hh"

namespace {

constexpr int TOUCH_TAP_DISTANCE_THRESHOLD_PX = 10;
constexpr int TOUCH_TAP_DISTANCE_THRESHOLD_PX_SQ = TOUCH_TAP_DISTANCE_THRESHOLD_PX *
                                                   TOUCH_TAP_DISTANCE_THRESHOLD_PX;
constexpr uint64_t TOUCH_TAP_TIME_THRESHOLD_MS = 220;
constexpr float TOUCH_PINCH_SCALE_FACTOR = 125.0f;

}  // namespace

GHOST_TouchWin32::GHOST_TouchWin32(GHOST_SystemWin32 &system) : system_(system) {}

bool GHOST_TouchWin32::processPointerEvent(
    UINT type, GHOST_WindowWin32 *window, WPARAM wParam, LPARAM /*lParam*/)
{
  POINTER_INFO pointer_info;
  if (!pointerInfoFromWParam(wParam, pointer_info)) {
    return false;
  }

  switch (type) {
    case WM_POINTERDOWN: {
      processPointerDown(window, pointer_info);
      break;
    }
    case WM_POINTERUPDATE: {
      processPointerUpdate(window, pointer_info);
      break;
    }
    case WM_POINTERUP: {
      processPointerUp(window, pointer_info);
      break;
    }
    default: {
      break;
    }
  }

  return true;
}

void GHOST_TouchWin32::processPointerLeave(WPARAM wParam)
{
  const UINT32 pointer_id = GET_POINTERID_WPARAM(wParam);
  if (contacts_.erase(pointer_id) != 0) {
    resetGestureBaseline();
    if (contacts_.empty()) {
      sequence_had_multitouch_ = false;
    }
  }
}

void GHOST_TouchWin32::reset()
{
  contacts_.clear();
  sequence_had_multitouch_ = false;
  resetGestureBaseline();
}

bool GHOST_TouchWin32::pointerInfoFromWParam(WPARAM wParam, POINTER_INFO &r_pointer_info) const
{
  const UINT32 pointer_id = GET_POINTERID_WPARAM(wParam);
  POINTER_INPUT_TYPE pointer_type;
  if (!GetPointerType(pointer_id, &pointer_type) || pointer_type != PT_TOUCH) {
    return false;
  }
  if (!GetPointerInfo(pointer_id, &r_pointer_info)) {
    return false;
  }
  return r_pointer_info.pointerType == PT_TOUCH;
}

uint64_t GHOST_TouchWin32::pointerTimeMs(const POINTER_INFO &pointer_info) const
{
  return system_.performanceCounterToMillis(pointer_info.PerformanceCount);
}

void GHOST_TouchWin32::processPointerDown(GHOST_WindowWin32 * /*window*/,
                                          const POINTER_INFO &pointer_info)
{
  if (contacts_.empty()) {
    sequence_had_multitouch_ = false;
  }

  TouchContact &contact = contacts_[pointer_info.pointerId];
  contact.screen_pos = pointer_info.ptPixelLocation;
  contact.down_screen_pos = pointer_info.ptPixelLocation;
  contact.down_time_ms = pointerTimeMs(pointer_info);
  contact.moved = false;

  if (contacts_.size() > 1) {
    sequence_had_multitouch_ = true;
  }

  resetGestureBaseline();
}

void GHOST_TouchWin32::processPointerUpdate(GHOST_WindowWin32 *window,
                                            const POINTER_INFO &pointer_info)
{
  TouchContact &contact = contacts_[pointer_info.pointerId];
  if (contact.down_time_ms == 0) {
    contact.down_screen_pos = pointer_info.ptPixelLocation;
    contact.down_time_ms = pointerTimeMs(pointer_info);
  }

  contact.screen_pos = pointer_info.ptPixelLocation;
  if (distanceSquared(contact.screen_pos, contact.down_screen_pos) > TOUCH_TAP_DISTANCE_THRESHOLD_PX_SQ)
  {
    contact.moved = true;
  }

  if (contacts_.size() > 1) {
    sequence_had_multitouch_ = true;
  }

  emitGestureEvents(window, pointerTimeMs(pointer_info));
}

void GHOST_TouchWin32::processPointerUp(GHOST_WindowWin32 *window, const POINTER_INFO &pointer_info)
{
  auto contact_it = contacts_.find(pointer_info.pointerId);
  if (contact_it == contacts_.end()) {
    return;
  }

  contact_it->second.screen_pos = pointer_info.ptPixelLocation;
  if (distanceSquared(contact_it->second.screen_pos, contact_it->second.down_screen_pos) >
      TOUCH_TAP_DISTANCE_THRESHOLD_PX_SQ)
  {
    contact_it->second.moved = true;
  }

  const bool allow_tap = !sequence_had_multitouch_ && (contacts_.size() == 1) &&
                         !(pointer_info.pointerFlags & POINTER_FLAG_CANCELED) &&
                         shouldSynthesizeTap(contact_it->second, pointerTimeMs(pointer_info));

  const POINT up_screen_pos = contact_it->second.screen_pos;
  contacts_.erase(contact_it);

  resetGestureBaseline();
  if (contacts_.empty()) {
    sequence_had_multitouch_ = false;
  }

  if (allow_tap) {
    synthesizeTap(window, up_screen_pos, pointerTimeMs(pointer_info));
  }
}

bool GHOST_TouchWin32::shouldSynthesizeTap(const TouchContact &contact, uint64_t time_ms) const
{
  if (contact.moved) {
    return false;
  }
  if ((time_ms - contact.down_time_ms) > TOUCH_TAP_TIME_THRESHOLD_MS) {
    return false;
  }
  return distanceSquared(contact.screen_pos, contact.down_screen_pos) <=
         TOUCH_TAP_DISTANCE_THRESHOLD_PX_SQ;
}

void GHOST_TouchWin32::synthesizeTap(GHOST_WindowWin32 *window,
                                     const POINT &screen_pos,
                                     uint64_t time_ms)
{
  system_.pushEvent(std::make_unique<GHOST_EventCursor>(
      time_ms, GHOST_kEventCursorMove, window, screen_pos.x, screen_pos.y, GHOST_TABLET_DATA_NONE));
  system_.pushEvent(std::make_unique<GHOST_EventButton>(
      time_ms, GHOST_kEventButtonDown, window, GHOST_kButtonMaskLeft, GHOST_TABLET_DATA_NONE));
  system_.pushEvent(std::make_unique<GHOST_EventButton>(
      time_ms, GHOST_kEventButtonUp, window, GHOST_kButtonMaskLeft, GHOST_TABLET_DATA_NONE));
}

GHOST_TouchWin32::TouchFrame GHOST_TouchWin32::frameFromContacts() const
{
  TouchFrame frame;
  frame.finger_count = int(contacts_.size());

  if (frame.finger_count == 0) {
    return frame;
  }

  int64_t sum_x = 0;
  int64_t sum_y = 0;
  for (const auto &item : contacts_) {
    sum_x += item.second.screen_pos.x;
    sum_y += item.second.screen_pos.y;
  }
  frame.centroid.x = int(sum_x / frame.finger_count);
  frame.centroid.y = int(sum_y / frame.finger_count);

  if (frame.finger_count == 2) {
    auto item_it = contacts_.begin();
    const POINT p0 = item_it->second.screen_pos;
    ++item_it;
    const POINT p1 = item_it->second.screen_pos;
    const float dx = float(p1.x - p0.x);
    const float dy = float(p1.y - p0.y);
    frame.finger_distance = std::sqrt((dx * dx) + (dy * dy));
  }

  return frame;
}

void GHOST_TouchWin32::resetGestureBaseline()
{
  gesture_baseline_valid_ = false;
  gesture_baseline_fingers_ = 0;
  gesture_baseline_centroid_ = {};
  gesture_baseline_distance_ = 0.0f;
}

void GHOST_TouchWin32::setGestureBaseline(const TouchFrame &frame)
{
  gesture_baseline_valid_ = true;
  gesture_baseline_fingers_ = frame.finger_count;
  gesture_baseline_centroid_ = frame.centroid;
  gesture_baseline_distance_ = frame.finger_distance;
}

void GHOST_TouchWin32::emitGestureEvents(GHOST_WindowWin32 *window, uint64_t time_ms)
{
  const TouchFrame frame = frameFromContacts();
  if (frame.finger_count != 1 && frame.finger_count != 2) {
    resetGestureBaseline();
    return;
  }

  if (!gesture_baseline_valid_ || (gesture_baseline_fingers_ != frame.finger_count)) {
    setGestureBaseline(frame);
    return;
  }

  if (frame.finger_count == 1) {
    bool has_motion = false;
    for (const auto &item : contacts_) {
      if (item.second.moved) {
        has_motion = true;
        break;
      }
    }
    if (!has_motion) {
      setGestureBaseline(frame);
      return;
    }
  }

  const int delta_x = frame.centroid.x - gesture_baseline_centroid_.x;
  const int delta_y = frame.centroid.y - gesture_baseline_centroid_.y;
  if (delta_x != 0 || delta_y != 0) {
    system_.pushEvent(std::make_unique<GHOST_EventTrackpad>(time_ms,
                                                            window,
                                                            GHOST_kTrackpadEventScroll,
                                                            frame.centroid.x,
                                                            frame.centroid.y,
                                                            delta_x,
                                                            delta_y,
                                                            false,
                                                            GHOST_kTrackpadSourceTouchscreen,
                                                            frame.finger_count));
  }

  if (frame.finger_count == 2 && gesture_baseline_distance_ > 1.0f) {
    const float zoom_ratio = frame.finger_distance / gesture_baseline_distance_;
    const int zoom_delta = int(std::lround((zoom_ratio - 1.0f) * TOUCH_PINCH_SCALE_FACTOR));
    if (zoom_delta != 0) {
      system_.pushEvent(std::make_unique<GHOST_EventTrackpad>(time_ms,
                                                              window,
                                                              GHOST_kTrackpadEventMagnify,
                                                              frame.centroid.x,
                                                              frame.centroid.y,
                                                              zoom_delta,
                                                              0,
                                                              false,
                                                              GHOST_kTrackpadSourceTouchscreen,
                                                              frame.finger_count));
    }
  }

  setGestureBaseline(frame);
}

int GHOST_TouchWin32::distanceSquared(const POINT &a, const POINT &b)
{
  const int dx = a.x - b.x;
  const int dy = a.y - b.y;
  return (dx * dx) + (dy * dy);
}
