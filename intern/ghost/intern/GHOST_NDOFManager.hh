/* SPDX-FileCopyrightText: 2002-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifndef WITH_INPUT_NDOF
#  error NDOF code included in non-NDOF-enabled build
#endif

#include "GHOST_System.hh"

#include <array>

enum NDOF_DeviceT {
  NDOF_UnknownDevice = 0,

  /* Current devices. */
  NDOF_SpaceNavigator,
  NDOF_SpaceExplorer,
  NDOF_SpacePilotPro,
  NDOF_SpaceMousePro,
  NDOF_SpaceMouseWireless,
  NDOF_SpaceMouseProWireless,
  NDOF_SpaceMouseEnterprise,
  NDOF_KeyboardPro,
  NDOF_NumpadPro,

  /* Older devices. */
  NDOF_SpacePilot,
  NDOF_Spaceball5000,
  NDOF_SpaceTraveler

};

/**
 * The dummy time delta to use for starting events.
 * 1/80th of a second (12.5ms).
 */
#define NDOF_TIME_DELTA_STARTING 0.0125f

using NDOF_Button_Array = std::array<GHOST_NDOF_ButtonT, 6>;

enum NDOF_Button_Type { ShortButton, LongButton };

class GHOST_NDOFManager {
 public:
  GHOST_NDOFManager(GHOST_System &);
  virtual ~GHOST_NDOFManager() = default;

  /**
   * Whether multi-axis functionality is available (via the OS or driver)
   * does not imply that a device is plugged in or being used.
   */
  virtual bool available() = 0;

  /**
   * Each platform's device detection should call this
   * use standard USB/HID identifiers.
   */
  bool setDevice(unsigned short vendor_id, unsigned short product_id);

  /**
   * Filter out small/accidental/un-calibrated motions by
   * setting up a "dead zone" around home position
   * set to 0 to disable
   * 0.1 is a safe and reasonable value.
   */
  void setDeadZone(float);

  /**
   * The latest raw axis data from the device.
   *
   * \note axis data should be in blender view coordinates
   * - +X is to the right.
   * - +Y is up.
   * - +Z is out of the screen.
   * - for rotations, look from origin to each +axis.
   * - rotations are + when CCW, - when CW.
   * Each platform is responsible for getting axis data into this form
   * these values should not be scaled (just shuffled or flipped).
   */
  void updateTranslation(const int t[3], uint64_t time);
  void updateRotation(const int r[3], uint64_t time);

  /**
   * The latest raw button data from the device
   * use HID button encoding (not #NDOF_ButtonT).
   */
  void updateButtonRAW(int button_number, bool press, uint64_t time);
  /**
   * Add the button event which has already been mapped to #GHOST_NDOF_ButtonT.
   */
  void updateButton(GHOST_NDOF_ButtonT button, bool press, uint64_t time);
  void updateButtonsBitmask(int button_bits, uint64_t time);
  void updateButtonsArray(NDOF_Button_Array buttons, uint64_t time, NDOF_Button_Type type);
  /* #NDOFButton events are sent immediately */

  /**
   * Processes and sends most recent raw data as an #NDOFMotion event
   * returns whether an event was sent.
   */
  bool sendMotionEvent();

 protected:
  GHOST_System &system_;

 private:
  void sendButtonEvent(GHOST_NDOF_ButtonT, bool press, uint64_t time, GHOST_IWindow *);
  void sendKeyEvent(GHOST_TKey, bool press, uint64_t time, GHOST_IWindow *);

  NDOF_DeviceT device_type_;
  int hid_map_button_num_;
  int hid_map_button_mask_;
  const GHOST_NDOF_ButtonT *hid_map_;

  int translation_[3];
  int rotation_[3];

  int button_depressed_; /* Bit field. */
  NDOF_Button_Array pressed_buttons_cache_;
  NDOF_Button_Array pressed_long_buttons_cache_;

  uint64_t motion_time_;      /* In milliseconds. */
  uint64_t motion_time_prev_; /* Time of most recent motion event sent. */

  GHOST_TProgress motion_state_;
  bool motion_event_pending_;
  float motion_dead_zone_; /* Discard motion with each component < this. */

  inline static std::array<NDOF_DeviceT, 9> bitmask_devices_ = {
      NDOF_SpaceNavigator,
      NDOF_SpaceExplorer,
      NDOF_SpacePilotPro,
      NDOF_SpaceMousePro,
      NDOF_SpaceMouseWireless,
      NDOF_SpaceMouseProWireless,
      NDOF_SpacePilot,
      NDOF_Spaceball5000,
      NDOF_SpaceTraveler,
  };
};
