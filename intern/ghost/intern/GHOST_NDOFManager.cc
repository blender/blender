/* SPDX-FileCopyrightText: 2007-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GHOST_NDOFManager.hh"
#include "GHOST_Debug.hh"
#include "GHOST_EventKey.hh"
#include "GHOST_EventNDOF.hh"
#include "GHOST_Types.h"
#include "GHOST_WindowManager.hh"
#include "GHOST_utildefines.hh"

#include "CLG_log.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring> /* For memory functions. */
#include <map>

static CLG_LogRef LOG = {"ghost.ndof"};

/**
 * 3Dconnexion keyboards and keypads use specific keys that have no standard equivalent.
 * These could be supported as generic "custom" keys, see !124155 review for details.
 */
// #define USE_3DCONNEXION_NONSTANDARD_KEYS

/* -------------------------------------------------------------------- */
/** \name NDOF Enum Strings
 * \{ */

/* Printable values for #GHOST_TProgress enum (keep aligned). */
static const char *ndof_progress_string[] = {
    "not started",
    "starting",
    "in progress",
    "finishing",
    "finished",
};

/* Printable values for #NDOF_ButtonT enum (keep aligned) */
#define MAP_ENTRY(button) {GHOST_##button, #button}
static const std::map<GHOST_NDOF_ButtonT, const char *> ndof_button_names = {
    /* Disable wrapping, it makes it difficult to read. */
    /* clang-format off */
    MAP_ENTRY(NDOF_BUTTON_INVALID),
    MAP_ENTRY(NDOF_BUTTON_MENU),
    MAP_ENTRY(NDOF_BUTTON_FIT),
    MAP_ENTRY(NDOF_BUTTON_TOP),
    MAP_ENTRY(NDOF_BUTTON_LEFT),
    MAP_ENTRY(NDOF_BUTTON_RIGHT),
    MAP_ENTRY(NDOF_BUTTON_FRONT),
    MAP_ENTRY(NDOF_BUTTON_BOTTOM),
    MAP_ENTRY(NDOF_BUTTON_BACK),
    MAP_ENTRY(NDOF_BUTTON_ROLL_CW),
    MAP_ENTRY(NDOF_BUTTON_ROLL_CCW),
    MAP_ENTRY(NDOF_BUTTON_SPIN_CW),
    MAP_ENTRY(NDOF_BUTTON_SPIN_CCW),
    MAP_ENTRY(NDOF_BUTTON_TILT_CW),
    MAP_ENTRY(NDOF_BUTTON_TILT_CCW),
    MAP_ENTRY(NDOF_BUTTON_ISO1),
    MAP_ENTRY(NDOF_BUTTON_ISO2),
    MAP_ENTRY(NDOF_BUTTON_1),
    MAP_ENTRY(NDOF_BUTTON_2),
    MAP_ENTRY(NDOF_BUTTON_3),
    MAP_ENTRY(NDOF_BUTTON_4),
    MAP_ENTRY(NDOF_BUTTON_5),
    MAP_ENTRY(NDOF_BUTTON_6),
    MAP_ENTRY(NDOF_BUTTON_7),
    MAP_ENTRY(NDOF_BUTTON_8),
    MAP_ENTRY(NDOF_BUTTON_9),
    MAP_ENTRY(NDOF_BUTTON_10),
    MAP_ENTRY(NDOF_BUTTON_11),
    MAP_ENTRY(NDOF_BUTTON_12),
    MAP_ENTRY(NDOF_BUTTON_ESC),
    MAP_ENTRY(NDOF_BUTTON_ALT),
    MAP_ENTRY(NDOF_BUTTON_SHIFT),
    MAP_ENTRY(NDOF_BUTTON_CTRL),
    MAP_ENTRY(NDOF_BUTTON_ENTER),
    MAP_ENTRY(NDOF_BUTTON_DELETE),
    MAP_ENTRY(NDOF_BUTTON_TAB),
    MAP_ENTRY(NDOF_BUTTON_SPACE),
    MAP_ENTRY(NDOF_BUTTON_ROTATE),
    MAP_ENTRY(NDOF_BUTTON_PANZOOM),
    MAP_ENTRY(NDOF_BUTTON_DOMINANT),
    MAP_ENTRY(NDOF_BUTTON_PLUS),
    MAP_ENTRY(NDOF_BUTTON_MINUS),
    MAP_ENTRY(NDOF_BUTTON_V1),
    MAP_ENTRY(NDOF_BUTTON_V2),
    MAP_ENTRY(NDOF_BUTTON_V3),
    MAP_ENTRY(NDOF_BUTTON_SAVE_V1),
    MAP_ENTRY(NDOF_BUTTON_SAVE_V2),
    MAP_ENTRY(NDOF_BUTTON_SAVE_V3),
    MAP_ENTRY(NDOF_BUTTON_KBP_F1),
    MAP_ENTRY(NDOF_BUTTON_KBP_F2),
    MAP_ENTRY(NDOF_BUTTON_KBP_F3),
    MAP_ENTRY(NDOF_BUTTON_KBP_F4),
    MAP_ENTRY(NDOF_BUTTON_KBP_F5),
    MAP_ENTRY(NDOF_BUTTON_KBP_F6),
    MAP_ENTRY(NDOF_BUTTON_KBP_F7),
    MAP_ENTRY(NDOF_BUTTON_KBP_F8),
    MAP_ENTRY(NDOF_BUTTON_KBP_F9),
    MAP_ENTRY(NDOF_BUTTON_KBP_F10),
    MAP_ENTRY(NDOF_BUTTON_KBP_F11),
    MAP_ENTRY(NDOF_BUTTON_KBP_F12),
    MAP_ENTRY(NDOF_BUTTON_NP_F1),
    MAP_ENTRY(NDOF_BUTTON_NP_F2),
    MAP_ENTRY(NDOF_BUTTON_NP_F3),
    MAP_ENTRY(NDOF_BUTTON_NP_F4),
    /* clang-format on */
};
#undef MAP_ENTRY

static const char *ndof_device_names[] = {
    "UnknownDevice",
    "SpaceNavigator",
    "SpaceExplorer",
    "SpacePilotPro",
    "SpaceMousePro",
    "SpaceMouseWireless",
    "SpaceMouseProWireless",
    "SpaceMouseEnterprise",
    "SpacePilot",
    "Spaceball5000",
    "SpaceTraveler",
    "Keyboard Pro",
    "Numpad Pro",
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Button Maps
 * \{ */

/**
 * Shared by the some 3Dconnexion hardware
 * SpacePilotPro uses all of these
 * SpaceMouse Pro and SpaceNavigator use only some, based on button mask.
 */
static const GHOST_NDOF_ButtonT ndof_HID_map_Shared3Dx[] = {
    GHOST_NDOF_BUTTON_MENU,     GHOST_NDOF_BUTTON_FIT,      GHOST_NDOF_BUTTON_TOP,
    GHOST_NDOF_BUTTON_LEFT,     GHOST_NDOF_BUTTON_RIGHT,    GHOST_NDOF_BUTTON_FRONT,
    GHOST_NDOF_BUTTON_BOTTOM,   GHOST_NDOF_BUTTON_BACK,     GHOST_NDOF_BUTTON_ROLL_CW,
    GHOST_NDOF_BUTTON_ROLL_CCW, GHOST_NDOF_BUTTON_ISO1,     GHOST_NDOF_BUTTON_ISO2,
    GHOST_NDOF_BUTTON_1,        GHOST_NDOF_BUTTON_2,        GHOST_NDOF_BUTTON_3,
    GHOST_NDOF_BUTTON_4,        GHOST_NDOF_BUTTON_5,        GHOST_NDOF_BUTTON_6,
    GHOST_NDOF_BUTTON_7,        GHOST_NDOF_BUTTON_8,        GHOST_NDOF_BUTTON_9,
    GHOST_NDOF_BUTTON_10,       GHOST_NDOF_BUTTON_ESC,      GHOST_NDOF_BUTTON_ALT,
    GHOST_NDOF_BUTTON_SHIFT,    GHOST_NDOF_BUTTON_CTRL,     GHOST_NDOF_BUTTON_ROTATE,
    GHOST_NDOF_BUTTON_PANZOOM,  GHOST_NDOF_BUTTON_DOMINANT, GHOST_NDOF_BUTTON_PLUS,
    GHOST_NDOF_BUTTON_MINUS,
};

static const GHOST_NDOF_ButtonT ndof_HID_map_SpaceExplorer[] = {
    GHOST_NDOF_BUTTON_1,
    GHOST_NDOF_BUTTON_2,
    GHOST_NDOF_BUTTON_TOP,
    GHOST_NDOF_BUTTON_LEFT,
    GHOST_NDOF_BUTTON_RIGHT,
    GHOST_NDOF_BUTTON_FRONT,
    GHOST_NDOF_BUTTON_ESC,
    GHOST_NDOF_BUTTON_ALT,
    GHOST_NDOF_BUTTON_SHIFT,
    GHOST_NDOF_BUTTON_CTRL,
    GHOST_NDOF_BUTTON_FIT,
    GHOST_NDOF_BUTTON_MENU,
    GHOST_NDOF_BUTTON_PLUS,
    GHOST_NDOF_BUTTON_MINUS,
    GHOST_NDOF_BUTTON_ROTATE,
};

/* This is the older SpacePilot (sans Pro). */
static const GHOST_NDOF_ButtonT ndof_HID_map_SpacePilot[] = {
    GHOST_NDOF_BUTTON_1,        GHOST_NDOF_BUTTON_2,
    GHOST_NDOF_BUTTON_3,        GHOST_NDOF_BUTTON_4,
    GHOST_NDOF_BUTTON_5,        GHOST_NDOF_BUTTON_6,
    GHOST_NDOF_BUTTON_TOP,      GHOST_NDOF_BUTTON_LEFT,
    GHOST_NDOF_BUTTON_RIGHT,    GHOST_NDOF_BUTTON_FRONT,
    GHOST_NDOF_BUTTON_ESC,      GHOST_NDOF_BUTTON_ALT,
    GHOST_NDOF_BUTTON_SHIFT,    GHOST_NDOF_BUTTON_CTRL,
    GHOST_NDOF_BUTTON_FIT,      GHOST_NDOF_BUTTON_MENU,
    GHOST_NDOF_BUTTON_PLUS,     GHOST_NDOF_BUTTON_MINUS,
    GHOST_NDOF_BUTTON_DOMINANT, GHOST_NDOF_BUTTON_ROTATE,
    GHOST_NDOF_BUTTON_INVALID /* the CONFIG button -- what does it do? */
};

static const GHOST_NDOF_ButtonT ndof_HID_map_Generic[] = {
    GHOST_NDOF_BUTTON_1,
    GHOST_NDOF_BUTTON_2,
    GHOST_NDOF_BUTTON_3,
    GHOST_NDOF_BUTTON_4,
    GHOST_NDOF_BUTTON_5,
    GHOST_NDOF_BUTTON_6,
    GHOST_NDOF_BUTTON_7,
    GHOST_NDOF_BUTTON_8,
    GHOST_NDOF_BUTTON_9,
};
/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Manager Class
 * \{ */

static const int genericButtonCount = ARRAY_SIZE(ndof_HID_map_Generic);

GHOST_NDOFManager::GHOST_NDOFManager(GHOST_System &sys)
    : system_(sys),
      device_type_(NDOF_UnknownDevice), /* Each platform has its own device detection code. */
      hid_map_button_num_(genericButtonCount),
      hid_map_button_mask_(0),
      hid_map_(ndof_HID_map_Generic),
      button_depressed_(0),
      pressed_buttons_cache_(),
      pressed_long_buttons_cache_(),
      motion_time_(0),
      motion_time_prev_(0),
      motion_state_(GHOST_kNotStarted),
      motion_event_pending_(false),
      motion_dead_zone_(0.0f)
{
  /* To avoid the rare situation where one triple is updated and
   * the other is not, initialize them both here: */
  memset(translation_, 0, sizeof(translation_));
  memset(rotation_, 0, sizeof(rotation_));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Device Setup
 * \{ */

bool GHOST_NDOFManager::setDevice(ushort vendor_id, ushort product_id)
{
  /* Call this function until it returns true
   * it's a good idea to stop calling it after that, as it will "forget"
   * whichever device it already found. */

  /* Default to safe generic behavior for "unknown" devices
   * unidentified devices will emit motion events like normal
   * rogue buttons do nothing by default, but can be customized by the user. */

  device_type_ = NDOF_UnknownDevice;
  hid_map_ = ndof_HID_map_Generic;
  hid_map_button_num_ = genericButtonCount;
  hid_map_button_mask_ = 0;

  /* "mystery device" owners can help build a HID_map for their hardware
   * A few users have already contributed information about several older devices
   * that I don't have access to. Thanks! */

  switch (vendor_id) {
    case 0x046D: /* Logitech (3Dconnexion was a subsidiary). */
      switch (product_id) {
        /* -- current devices -- */
        case 0xC626: /* Full-size SpaceNavigator. */
        case 0xC628: /* The "for Notebooks" one. */
        {
          device_type_ = NDOF_SpaceNavigator;
          hid_map_button_num_ = 2;
          hid_map_ = ndof_HID_map_Shared3Dx;
          break;
        }
        case 0xC627: {
          device_type_ = NDOF_SpaceExplorer;
          hid_map_button_num_ = 15;
          hid_map_ = ndof_HID_map_SpaceExplorer;
          break;
        }
        case 0xC629: {
          device_type_ = NDOF_SpacePilotPro;
          hid_map_button_num_ = 31;
          hid_map_ = ndof_HID_map_Shared3Dx;
          break;
        }
        case 0xC62B: {
          device_type_ = NDOF_SpaceMousePro;
          hid_map_button_num_ = 27; /* 15 physical buttons, but HID codes range from 0 to 26. */
          hid_map_button_mask_ = 0x07C0F137;
          hid_map_ = ndof_HID_map_Shared3Dx;
          break;
        }

        /* -- older devices -- */
        case 0xC625: {
          device_type_ = NDOF_SpacePilot;
          hid_map_button_num_ = 21;
          hid_map_ = ndof_HID_map_SpacePilot;
          break;
        }
        case 0xC621: {
          device_type_ = NDOF_Spaceball5000;
          hid_map_button_num_ = 12;
          break;
        }
        case 0xC623: {
          device_type_ = NDOF_SpaceTraveler;
          hid_map_button_num_ = 8;
          break;
        }
        default: {
          CLOG_INFO(&LOG, "Unknown Logitech product %04hx", product_id);
        }
      }
      break;
    case 0x256F: /* 3Dconnexion. */
      switch (product_id) {
        case 0xC62E: /* SpaceMouse Wireless (cabled). */
        case 0xC62F: /* SpaceMouse Wireless Receiver. */
        case 0xC658: /* Wireless (3DConnexion Universal Wireless Receiver in WIN32), see #82412. */
        {
          device_type_ = NDOF_SpaceMouseWireless;
          hid_map_button_num_ = 2;
          hid_map_ = ndof_HID_map_Shared3Dx;
          break;
        }
        case 0xC631: /* SpaceMouse Pro Wireless (cabled). */
        case 0xC632: /* SpaceMouse Pro Wireless Receiver. */
        case 0xC638: /* SpaceMouse Pro Wireless BT (cabled), see #116393.
                      * 3Dconnexion docs describe this as "Wireless BT", but it is cabled. */
        case 0xC652: /* Universal Receiver. */
        {
          device_type_ = NDOF_SpaceMouseProWireless;
          hid_map_button_num_ = 27; /* 15 physical buttons, but HID codes range from 0 to 26. */
          hid_map_button_mask_ = 0x07C0F137;
          hid_map_ = ndof_HID_map_Shared3Dx;
          break;
        }
        case 0xC633: /* Newer devices don't need to use button mappings. */
        {
          device_type_ = NDOF_SpaceMouseEnterprise;
          break;
        }
        case 0xC664:
        case 0xC668: {
          device_type_ = NDOF_KeyboardPro;
          break;
        }
        case 0xC665: {
          device_type_ = NDOF_NumpadPro;
          break;
        }
        default: {
          CLOG_INFO(&LOG, "Unknown 3Dconnexion product %04hx", product_id);
        }
      }
      break;
    default:
      CLOG_INFO(&LOG, "Unknown device %04hx:%04hx", vendor_id, product_id);
  }

  if (device_type_ != NDOF_UnknownDevice) {
    CLOG_INFO(&LOG, "Using %s", ndof_device_names[device_type_]);
  }

  if (hid_map_button_mask_ == 0) {
    hid_map_button_mask_ = int(~(UINT_MAX << hid_map_button_num_));
  }

  CLOG_DEBUG(&LOG, "Device %d buttons -> hex:%X", hid_map_button_num_, uint(hid_map_button_mask_));

  return device_type_ != NDOF_UnknownDevice;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Update State
 * \{ */

void GHOST_NDOFManager::updateTranslation(const int t[3], uint64_t time)
{
  memcpy(translation_, t, sizeof(translation_));
  motion_time_ = time;
  motion_event_pending_ = true;
}

void GHOST_NDOFManager::updateRotation(const int r[3], uint64_t time)
{
  memcpy(rotation_, r, sizeof(rotation_));
  motion_time_ = time;
  motion_event_pending_ = true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Buttons
 * \{ */

static GHOST_TKey ghost_map_keyboard_from_ndof_button(const GHOST_NDOF_ButtonT button)
{
  switch (button) {
    case GHOST_NDOF_BUTTON_ESC: {
      return GHOST_kKeyEsc;
    }
    case GHOST_NDOF_BUTTON_ENTER: {
      return GHOST_kKeyEnter;
    }
    case GHOST_NDOF_BUTTON_DELETE: {
      return GHOST_kKeyDelete;
    }
    case GHOST_NDOF_BUTTON_TAB: {
      return GHOST_kKeyTab;
    }
    case GHOST_NDOF_BUTTON_SPACE: {
      return GHOST_kKeySpace;
    }
    case GHOST_NDOF_BUTTON_ALT: {
      return GHOST_kKeyLeftAlt;
    }
    case GHOST_NDOF_BUTTON_SHIFT: {
      return GHOST_kKeyLeftShift;
    }
    case GHOST_NDOF_BUTTON_CTRL: {
      return GHOST_kKeyLeftControl;
    }

#ifdef USE_3DCONNEXION_NONSTANDARD_KEYS
    case GHOST_NDOF_BUTTON_NP_F1:
    case GHOST_NDOF_BUTTON_KBP_F1: {
      return GHOST_kKeyF1;
    }
    case GHOST_NDOF_BUTTON_NP_F2:
    case GHOST_NDOF_BUTTON_KBP_F2: {
      return GHOST_kKeyF2;
    }
    case GHOST_NDOF_BUTTON_NP_F3:
    case GHOST_NDOF_BUTTON_KBP_F3: {
      return GHOST_kKeyF3;
    }
    case GHOST_NDOF_BUTTON_NP_F4:
    case GHOST_NDOF_BUTTON_KBP_F4: {
      return GHOST_kKeyF4;
    }
    case GHOST_NDOF_BUTTON_KBP_F5: {
      return GHOST_kKeyF5;
    }
    case GHOST_NDOF_BUTTON_KBP_F6: {
      return GHOST_kKeyF6;
    }
    case GHOST_NDOF_BUTTON_KBP_F7: {
      return GHOST_kKeyF7;
    }
    case GHOST_NDOF_BUTTON_KBP_F8: {
      return GHOST_kKeyF8;
    }
    case GHOST_NDOF_BUTTON_KBP_F9: {
      return GHOST_kKeyF9;
    }
    case GHOST_NDOF_BUTTON_KBP_F10: {
      return GHOST_kKeyF10;
    }
    case GHOST_NDOF_BUTTON_KBP_F11: {
      return GHOST_kKeyF11;
    }
    case GHOST_NDOF_BUTTON_KBP_F12: {
      return GHOST_kKeyF12;
    }
#endif /* !USE_3DCONNEXION_NONSTANDARD_KEYS */

    default: {
      return GHOST_kKeyUnknown;
    }
  }
}

void GHOST_NDOFManager::sendButtonEvent(GHOST_NDOF_ButtonT button,
                                        bool press,
                                        uint64_t time,
                                        GHOST_IWindow *window)
{
  GHOST_ASSERT(button > GHOST_NDOF_BUTTON_NONE && button < GHOST_NDOF_BUTTON_USER,
               "rogue button trying to escape GHOST_NDOF manager");

  const GHOST_EventNDOFButton *event = new GHOST_EventNDOFButton(time, window);
  GHOST_TEventNDOFButtonData *data = (GHOST_TEventNDOFButtonData *)event->getData();

  data->action = press ? GHOST_kPress : GHOST_kRelease;
  data->button = button;

  system_.pushEvent(event);
}

void GHOST_NDOFManager::sendKeyEvent(GHOST_TKey key,
                                     bool press,
                                     uint64_t time,
                                     GHOST_IWindow *window)
{
  GHOST_TEventType type = press ? GHOST_kEventKeyDown : GHOST_kEventKeyUp;
  const GHOST_EventKey *event = new GHOST_EventKey(time, type, window, key, false);

  system_.pushEvent(event);
}

void GHOST_NDOFManager::updateButton(GHOST_NDOF_ButtonT button, bool press, uint64_t time)
{
  if (button == GHOST_NDOF_BUTTON_INVALID) {
    CLOG_DEBUG(
        &LOG, "Update button=%d, press=%d (mapped to none, ignoring!)", int(button), int(press));
    return;
  }

  CLOG_DEBUG(&LOG,
             "Update button=%d, press=%d, name=%s",
             button,
             int(press),
             ndof_button_names.at(button));

#ifndef USE_3DCONNEXION_NONSTANDARD_KEYS
  if (((button >= GHOST_NDOF_BUTTON_KBP_F1) && (button <= GHOST_NDOF_BUTTON_KBP_F12)) ||
      ((button >= GHOST_NDOF_BUTTON_NP_F1) && (button <= GHOST_NDOF_BUTTON_NP_F4)))
  {
    return;
  }
#endif /* !USE_3DCONNEXION_NONSTANDARD_KEYS */

  GHOST_IWindow *window = system_.getWindowManager()->getActiveWindow();

  /* Delivery will fail, so don't bother sending. */
  if (window != nullptr) {
    const GHOST_TKey key = ghost_map_keyboard_from_ndof_button(button);
    if (key != GHOST_kKeyUnknown) {
      sendKeyEvent(key, press, time, window);
    }
    else {
      sendButtonEvent(button, press, time, window);
    }
  }
}

void GHOST_NDOFManager::updateButtonRAW(int button_number, bool press, uint64_t time)
{
  GHOST_NDOF_ButtonT button;

  /* For bit-mask devices button mapping isn't unified, therefore check the button map. */
  if (std::find(bitmask_devices_.begin(), bitmask_devices_.end(), device_type_) !=
      bitmask_devices_.end())
  {
    if (button_number >= hid_map_button_num_) {
      CLOG_DEBUG(
          &LOG, "Update button=%d, press=%d (out of range, ignoring!)", button_number, int(press));
      return;
    }
    button = hid_map_[button_number];
  }
  else {
    button = static_cast<GHOST_NDOF_ButtonT>(button_number);
  }

  GHOST_NDOFManager::updateButton(button, press, time);
}

void GHOST_NDOFManager::updateButtonsBitmask(int button_bits, uint64_t time)
{
  /* Some devices send two data packets: bitmask and number array.
   * In this case, packet has to be ignored if it came from such a device. */
  if (std::find(bitmask_devices_.begin(), bitmask_devices_.end(), device_type_) ==
      bitmask_devices_.end())
  {
    return;
  }

  button_bits &= hid_map_button_mask_; /* Discard any "garbage" bits. */

  int diff = button_depressed_ ^ button_bits;

  for (int button_number = 0; button_number < hid_map_button_num_; ++button_number) {
    int mask = 1 << button_number;

    if (diff & mask) {
      bool press = button_bits & mask;

      if (press) {
        button_depressed_ |= mask; /* Set this button's bit. */
      }
      else {
        button_depressed_ &= ~mask; /* Clear this button's bit. */
      }

      /* Bitmask devices don't have unified keymaps, so button numbers needs to be looked up in the
       * map. */
      const GHOST_NDOF_ButtonT button = hid_map_[button_number];
      updateButton(button, press, time);
    }
  }
}

void GHOST_NDOFManager::updateButtonsArray(NDOF_Button_Array buttons,
                                           uint64_t time,
                                           NDOF_Button_Type type)
{
  NDOF_Button_Array &cache = (type == NDOF_Button_Type::LongButton) ? pressed_long_buttons_cache_ :
                                                                      pressed_buttons_cache_;

  /* Find released buttons */
  for (const auto &cached_button : cache) {
    bool found = false;
    for (const auto &button : buttons) {
      if (button == cached_button) {
        found = true;
        break;
      }
    }

    if (!found) {
      updateButton(cached_button, false, time);
    }
  }

  /* Find pressed buttons */
  for (const auto &button : buttons) {
    bool found = false;
    for (const auto &cached_button : cache) {
      if (button == cached_button) {
        found = true;
        break;
      }
    }

    if (!found) {
      updateButton(button, true, time);
    }
  }
  cache = buttons;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Motion
 * \{ */

void GHOST_NDOFManager::setDeadZone(float dz)
{
  /* Negative values don't make sense, so clamp at zero. */
  dz = std::max(dz, 0.0f);
  motion_dead_zone_ = dz;

  /* Warn the rogue user/developer about high dead-zone, but allow it. */
  CLOG_INFO(&LOG, "Dead zone set to %.2f%s", dz, (dz > 0.5f) ? " (unexpectedly high)" : "");
}

static bool atHomePosition(const GHOST_TEventNDOFMotionData *ndof)
{
#define HOME(foo) (ndof->foo == 0.0f)
  return HOME(tx) && HOME(ty) && HOME(tz) && HOME(rx) && HOME(ry) && HOME(rz);
#undef HOME
}

static bool nearHomePosition(const GHOST_TEventNDOFMotionData *ndof, float threshold)
{
  if (threshold == 0.0f) {
    return atHomePosition(ndof);
  }
#define HOME(foo) (fabsf(ndof->foo) < threshold)
  return HOME(tx) && HOME(ty) && HOME(tz) && HOME(rx) && HOME(ry) && HOME(rz);
#undef HOME
}

bool GHOST_NDOFManager::sendMotionEvent()
{
  if (!motion_event_pending_) {
    if (motion_state_ != GHOST_kNotStarted) {
      /* Detect window de-activation and change the `motion_state_` even when no motion is pending.
       * Without this check it's possible the window is de-activated before the NDOF
       * motion callbacks have run, while the `motion_state_` is active.
       * In this case, activating the window again would create an event
       * with a large time-delta, see: #134733. */
      if (system_.getWindowManager()->getActiveWindow() == nullptr) {
        /* Avoid large `dt` times when changing windows. */
        motion_state_ = GHOST_kNotStarted;
      }
    }
    return false;
  }

  motion_event_pending_ = false; /* Any pending motion is handled right now. */

  GHOST_IWindow *window = system_.getWindowManager()->getActiveWindow();

  /* Delivery will fail, so don't bother sending. */
  if (window == nullptr) {
    /* Avoid large `dt` times when changing windows. */
    motion_state_ = GHOST_kNotStarted;
    return false;
  }

  const GHOST_EventNDOFMotion *event = new GHOST_EventNDOFMotion(motion_time_, window);
  GHOST_TEventNDOFMotionData *data = (GHOST_TEventNDOFMotionData *)event->getData();

  /* Scale axis values here to normalize them to around +/- 1
   * they are scaled again for overall sensitivity in the WM based on user preferences. */

  const float scale = 1.0f / 350.0f; /* 3Dconnexion devices send +/- 350 usually */

  data->tx = scale * translation_[0];
  data->ty = scale * translation_[1];
  data->tz = scale * translation_[2];

  data->rx = scale * rotation_[0];
  data->ry = scale * rotation_[1];
  data->rz = scale * rotation_[2];
  data->dt = 0.001f * (motion_time_ - motion_time_prev_); /* In seconds. */
  motion_time_prev_ = motion_time_;

  bool weHaveMotion = !nearHomePosition(data, motion_dead_zone_);

  /* Determine what kind of motion event to send `(Starting, InProgress, Finishing)`
   * and where that leaves this NDOF manager `(NotStarted, InProgress, Finished)`. */
  switch (motion_state_) {
    case GHOST_kNotStarted:
    case GHOST_kFinished: {
      if (weHaveMotion) {
        data->progress = GHOST_kStarting;
        motion_state_ = GHOST_kInProgress;
        /* Previous motion time will be ancient, so just make up a reasonable time delta. */
        data->dt = NDOF_TIME_DELTA_STARTING;
      }
      else {
        /* Send no event and keep current state. */
        CLOG_DEBUG(&LOG, "Motion ignored");
        delete event;
        return false;
      }
      break;
    }
    case GHOST_kInProgress: {
      if (weHaveMotion) {
        data->progress = GHOST_kInProgress;
        /* Remain 'InProgress'. */
      }
      else {
        data->progress = GHOST_kFinishing;
        motion_state_ = GHOST_kFinished;
      }
      break;
    }
    default: {
      /* Will always be one of the above. */
      break;
    }
  }

#if 1
  CLOG_DEBUG(&LOG,
             "Motion sent, T=(%.2f,%.2f,%.2f), R=(%.2f,%.2f,%.2f) dt=%.3f, status=%s",
             data->tx,
             data->ty,
             data->tz,
             data->rx,
             data->ry,
             data->rz,
             data->dt,
             ndof_progress_string[data->progress]);
#else
  /* Raw values, may be useful for debugging. */
  CLOG_DEBUG(&LOG,
             "Motion sent, T=(%d,%d,%d) R=(%d,%d,%d) status=%s",
             translation_[0],
             translation_[1],
             translation_[2],
             rotation_[0],
             rotation_[1],
             rotation_[2],
             ndof_progress_string[data->progress]);
#endif
  system_.pushEvent(event);

  return true;
}

/** \} */
