/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifndef WITH_INPUT_NDOF
#  error NDOF code included in non-NDOF-enabled build
#endif

#include "GHOST_System.hh"

typedef enum {
  NDOF_UnknownDevice = 0,

  /* Current devices. */
  NDOF_SpaceNavigator,
  NDOF_SpaceExplorer,
  NDOF_SpacePilotPro,
  NDOF_SpaceMousePro,
  NDOF_SpaceMouseWireless,
  NDOF_SpaceMouseProWireless,
  NDOF_SpaceMouseEnterprise,

  /* Older devices. */
  NDOF_SpacePilot,
  NDOF_Spaceball5000,
  NDOF_SpaceTraveler

} NDOF_DeviceT;

/**
 * NDOF device button event types.
 *
 * \note Button values are stored in DNA as part of key-map items.
 * Existing values should not be changed. Otherwise, a mapping must be used,
 * see #NDOF_BUTTON_INDEX_AS_EVENT.
 */
typedef enum {
  /* Used internally, never sent or used as an index. */
  NDOF_BUTTON_NONE = -1,
  /* These two are available from any 3Dconnexion device. */
  NDOF_BUTTON_MENU,
  NDOF_BUTTON_FIT,
  /* Standard views. */
  NDOF_BUTTON_TOP,
  NDOF_BUTTON_BOTTOM,
  NDOF_BUTTON_LEFT,
  NDOF_BUTTON_RIGHT,
  NDOF_BUTTON_FRONT,
  NDOF_BUTTON_BACK,
  /* More views. */
  NDOF_BUTTON_ISO1,
  NDOF_BUTTON_ISO2,
  /* 90 degree rotations.
   * These don't all correspond to physical buttons. */
  NDOF_BUTTON_ROLL_CW,
  NDOF_BUTTON_ROLL_CCW,
  NDOF_BUTTON_SPIN_CW,
  NDOF_BUTTON_SPIN_CCW,
  NDOF_BUTTON_TILT_CW,
  NDOF_BUTTON_TILT_CCW,
  /* Device control. */
  NDOF_BUTTON_ROTATE,
  NDOF_BUTTON_PANZOOM,
  NDOF_BUTTON_DOMINANT,
  NDOF_BUTTON_PLUS,
  NDOF_BUTTON_MINUS,
  /* Store Views. */
  NDOF_BUTTON_V1,
  NDOF_BUTTON_V2,
  NDOF_BUTTON_V3,
  _NDOF_UNUSED_0,
  /* General-purpose buttons.
   * Users can assign functions via keymap editor. */
  NDOF_BUTTON_1,
  NDOF_BUTTON_2,
  NDOF_BUTTON_3,
  NDOF_BUTTON_4,
  NDOF_BUTTON_5,
  NDOF_BUTTON_6,
  NDOF_BUTTON_7,
  NDOF_BUTTON_8,
  NDOF_BUTTON_9,
  NDOF_BUTTON_10,
  /* More general-purpose buttons. */
  NDOF_BUTTON_A,
  NDOF_BUTTON_B,
  NDOF_BUTTON_C,

  /* Keyboard emulation (keep last as they are mapped to regular keyboard events). */
  NDOF_BUTTON_ESC,
  NDOF_BUTTON_ENTER,
  NDOF_BUTTON_DELETE,
  NDOF_BUTTON_TAB,
  NDOF_BUTTON_SPACE,
  NDOF_BUTTON_ALT,
  NDOF_BUTTON_SHIFT,
  NDOF_BUTTON_CTRL,
#define NDOF_BUTTON_NUM (NDOF_BUTTON_CTRL + 1)
} NDOF_ButtonT;

class GHOST_NDOFManager {
 public:
  GHOST_NDOFManager(GHOST_System &);
  virtual ~GHOST_NDOFManager() {}

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
  void updateButton(int button_number, bool press, uint64_t time);
  void updateButtons(int button_bits, uint64_t time);
  /* #NDOFButton events are sent immediately */

  /**
   * Processes and sends most recent raw data as an #NDOFMotion event
   * returns whether an event was sent.
   */
  bool sendMotionEvent();

 protected:
  GHOST_System &system_;

 private:
  void sendButtonEvent(NDOF_ButtonT, bool press, uint64_t time, GHOST_IWindow *);
  void sendKeyEvent(GHOST_TKey, bool press, uint64_t time, GHOST_IWindow *);

  NDOF_DeviceT device_type_;
  int hid_map_button_num_;
  int hid_map_button_mask_;
  const NDOF_ButtonT *hid_map_;

  int translation_[3];
  int rotation_[3];
  int button_depressed_; /* Bit field. */

  uint64_t motion_time_;      /* In milliseconds. */
  uint64_t motion_time_prev_; /* Time of most recent motion event sent. */

  GHOST_TProgress motion_state_;
  bool motion_event_pending_;
  float motion_dead_zone_; /* Discard motion with each component < this. */
};
