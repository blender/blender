/* SPDX-FileCopyrightText: 2007-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GHOST_NDOFManager.hh"
#include "GHOST_Debug.hh"
#include "GHOST_EventKey.hh"
#include "GHOST_EventNDOF.hh"
#include "GHOST_WindowManager.hh"
#include "GHOST_utildefines.hh"

/* Logging, use `ghost.ndof.*` prefix. */
#include "CLG_log.h"

#include <climits>
#include <cmath>
#include <cstring> /* For memory functions. */

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
static const char *ndof_button_names[] = {
    /* Exclude `NDOF_BUTTON_NONE` (-1). */
    "NDOF_BUTTON_MENU",
    "NDOF_BUTTON_FIT",
    "NDOF_BUTTON_TOP",
    "NDOF_BUTTON_BOTTOM",
    "NDOF_BUTTON_LEFT",
    "NDOF_BUTTON_RIGHT",
    "NDOF_BUTTON_FRONT",
    "NDOF_BUTTON_BACK",
    "NDOF_BUTTON_ISO1",
    "NDOF_BUTTON_ISO2",
    "NDOF_BUTTON_ROLL_CW",
    "NDOF_BUTTON_ROLL_CCW",
    "NDOF_BUTTON_SPIN_CW",
    "NDOF_BUTTON_SPIN_CCW",
    "NDOF_BUTTON_TILT_CW",
    "NDOF_BUTTON_TILT_CCW",
    "NDOF_BUTTON_ROTATE",
    "NDOF_BUTTON_PANZOOM",
    "NDOF_BUTTON_DOMINANT",
    "NDOF_BUTTON_PLUS",
    "NDOF_BUTTON_MINUS",
    "NDOF_BUTTON_1",
    "NDOF_BUTTON_2",
    "NDOF_BUTTON_3",
    "NDOF_BUTTON_4",
    "NDOF_BUTTON_5",
    "NDOF_BUTTON_6",
    "NDOF_BUTTON_7",
    "NDOF_BUTTON_8",
    "NDOF_BUTTON_9",
    "NDOF_BUTTON_10",
    "NDOF_BUTTON_A",
    "NDOF_BUTTON_B",
    "NDOF_BUTTON_C",
    "NDOF_BUTTON_V1",
    "NDOF_BUTTON_V2",
    "NDOF_BUTTON_V3",
    /* Keyboard emulation. */
    "NDOF_BUTTON_ESC",
    "NDOF_BUTTON_ENTER",
    "NDOF_BUTTON_DELETE",
    "NDOF_BUTTON_TAB",
    "NDOF_BUTTON_SPACE",
    "NDOF_BUTTON_ALT",
    "NDOF_BUTTON_SHIFT",
    "NDOF_BUTTON_CTRL",
};

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
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Button Maps
 * \{ */

/* Shared by the latest 3Dconnexion hardware
 * SpacePilotPro uses all of these
 * smaller devices use only some, based on button mask. */
static const NDOF_ButtonT ndof_HID_map_Modern3Dx[] = {
    NDOF_BUTTON_MENU,     NDOF_BUTTON_FIT,      NDOF_BUTTON_TOP,    NDOF_BUTTON_LEFT,
    NDOF_BUTTON_RIGHT,    NDOF_BUTTON_FRONT,    NDOF_BUTTON_BOTTOM, NDOF_BUTTON_BACK,
    NDOF_BUTTON_ROLL_CW,  NDOF_BUTTON_ROLL_CCW, NDOF_BUTTON_ISO1,   NDOF_BUTTON_ISO2,
    NDOF_BUTTON_1,        NDOF_BUTTON_2,        NDOF_BUTTON_3,      NDOF_BUTTON_4,
    NDOF_BUTTON_5,        NDOF_BUTTON_6,        NDOF_BUTTON_7,      NDOF_BUTTON_8,
    NDOF_BUTTON_9,        NDOF_BUTTON_10,       NDOF_BUTTON_ESC,    NDOF_BUTTON_ALT,
    NDOF_BUTTON_SHIFT,    NDOF_BUTTON_CTRL,     NDOF_BUTTON_ROTATE, NDOF_BUTTON_PANZOOM,
    NDOF_BUTTON_DOMINANT, NDOF_BUTTON_PLUS,     NDOF_BUTTON_MINUS};

static const NDOF_ButtonT ndof_HID_map_SpaceExplorer[] = {
    NDOF_BUTTON_1,
    NDOF_BUTTON_2,
    NDOF_BUTTON_TOP,
    NDOF_BUTTON_LEFT,
    NDOF_BUTTON_RIGHT,
    NDOF_BUTTON_FRONT,
    NDOF_BUTTON_ESC,
    NDOF_BUTTON_ALT,
    NDOF_BUTTON_SHIFT,
    NDOF_BUTTON_CTRL,
    NDOF_BUTTON_FIT,
    NDOF_BUTTON_MENU,
    NDOF_BUTTON_PLUS,
    NDOF_BUTTON_MINUS,
    NDOF_BUTTON_ROTATE,
};

/* This is the older SpacePilot (sans Pro). */
static const NDOF_ButtonT ndof_HID_map_SpacePilot[] = {
    NDOF_BUTTON_1,     NDOF_BUTTON_2,     NDOF_BUTTON_3,        NDOF_BUTTON_4,
    NDOF_BUTTON_5,     NDOF_BUTTON_6,     NDOF_BUTTON_TOP,      NDOF_BUTTON_LEFT,
    NDOF_BUTTON_RIGHT, NDOF_BUTTON_FRONT, NDOF_BUTTON_ESC,      NDOF_BUTTON_ALT,
    NDOF_BUTTON_SHIFT, NDOF_BUTTON_CTRL,  NDOF_BUTTON_FIT,      NDOF_BUTTON_MENU,
    NDOF_BUTTON_PLUS,  NDOF_BUTTON_MINUS, NDOF_BUTTON_DOMINANT, NDOF_BUTTON_ROTATE,
    NDOF_BUTTON_NONE /* the CONFIG button -- what does it do? */
};

static const NDOF_ButtonT ndof_HID_map_Generic[] = {
    NDOF_BUTTON_1,
    NDOF_BUTTON_2,
    NDOF_BUTTON_3,
    NDOF_BUTTON_4,
    NDOF_BUTTON_5,
    NDOF_BUTTON_6,
    NDOF_BUTTON_7,
    NDOF_BUTTON_8,
    NDOF_BUTTON_9,
    NDOF_BUTTON_A,
    NDOF_BUTTON_B,
    NDOF_BUTTON_C,
};

/* Values taken from: https://github.com/FreeSpacenav/spacenavd/wiki/Device-button-names */
static const NDOF_ButtonT ndof_HID_map_SpaceMouseEnterprise[] = {
    NDOF_BUTTON_1,       /* (0) */
    NDOF_BUTTON_2,       /* (1) */
    NDOF_BUTTON_3,       /* (2) */
    NDOF_BUTTON_4,       /* (3) */
    NDOF_BUTTON_5,       /* (4) */
    NDOF_BUTTON_6,       /* (5) */
    NDOF_BUTTON_7,       /* (6) */
    NDOF_BUTTON_8,       /* (7) */
    NDOF_BUTTON_9,       /* (8) */
    NDOF_BUTTON_A,       /* Labeled "10" (9). */
    NDOF_BUTTON_B,       /* Labeled "11" (10). */
    NDOF_BUTTON_C,       /* Labeled "12" (11). */
    NDOF_BUTTON_MENU,    /* (12). */
    NDOF_BUTTON_FIT,     /* (13). */
    NDOF_BUTTON_TOP,     /* (14). */
    NDOF_BUTTON_RIGHT,   /* (15). */
    NDOF_BUTTON_FRONT,   /* (16). */
    NDOF_BUTTON_ROLL_CW, /* (17). */
    NDOF_BUTTON_ESC,     /* (18). */
    NDOF_BUTTON_ALT,     /* (19). */
    NDOF_BUTTON_SHIFT,   /* (20). */
    NDOF_BUTTON_CTRL,    /* (21). */
    NDOF_BUTTON_ROTATE,  /* Labeled "Lock Rotate" (22). */
    NDOF_BUTTON_ENTER,   /* Labeled "Enter" (23). */
    NDOF_BUTTON_DELETE,  /* (24). */
    NDOF_BUTTON_TAB,     /* (25). */
    NDOF_BUTTON_SPACE,   /* (26). */
    NDOF_BUTTON_V1,      /* Labeled "V1" (27). */
    NDOF_BUTTON_V2,      /* Labeled "V2" (28). */
    NDOF_BUTTON_V3,      /* Labeled "V3" (29). */
    NDOF_BUTTON_ISO1,    /* Labeled "ISO1" (30). */
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

static CLG_LogRef LOG_NDOF_DEVICE = {"ghost.ndof.device"};
#define LOG (&LOG_NDOF_DEVICE)

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
          hid_map_ = ndof_HID_map_Modern3Dx;
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
          hid_map_ = ndof_HID_map_Modern3Dx;
          break;
        }
        case 0xC62B: {
          device_type_ = NDOF_SpaceMousePro;
          hid_map_button_num_ = 27; /* 15 physical buttons, but HID codes range from 0 to 26. */
          hid_map_button_mask_ = 0x07C0F137;
          hid_map_ = ndof_HID_map_Modern3Dx;
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
          CLOG_INFO(LOG, 2, "unknown Logitech product %04hx", product_id);
        }
      }
      break;
    case 0x256F: /* 3Dconnexion. */
      switch (product_id) {
        case 0xC62E: /* Plugged in. */
        case 0xC62F: /* Wireless. */
        case 0xC658: /* Wireless (3DConnexion Universal Wireless Receiver in WIN32), see #82412. */
        {
          device_type_ = NDOF_SpaceMouseWireless;
          hid_map_button_num_ = 2;
          hid_map_ = ndof_HID_map_Modern3Dx;
          break;
        }
        case 0xC631: /* Plugged in. */
        case 0xC632: /* Wireless. */
        {
          device_type_ = NDOF_SpaceMouseProWireless;
          hid_map_button_num_ = 27; /* 15 physical buttons, but HID codes range from 0 to 26. */
          hid_map_button_mask_ = 0x07C0F137;
          hid_map_ = ndof_HID_map_Modern3Dx;
          break;
        }
        case 0xC633: {
          device_type_ = NDOF_SpaceMouseEnterprise;
          hid_map_button_num_ = 31;
          hid_map_ = ndof_HID_map_SpaceMouseEnterprise;
          break;
        }
        default: {
          CLOG_INFO(LOG, 2, "unknown 3Dconnexion product %04hx", product_id);
        }
      }
      break;
    default:
      CLOG_INFO(LOG, 2, "unknown device %04hx:%04hx", vendor_id, product_id);
  }

  if (device_type_ != NDOF_UnknownDevice) {
    CLOG_INFO(LOG, 2, "using %s", ndof_device_names[device_type_]);
  }

  if (hid_map_button_mask_ == 0) {
    hid_map_button_mask_ = int(~(UINT_MAX << hid_map_button_num_));
  }

  CLOG_INFO(LOG, 2, "%d buttons -> hex:%X", hid_map_button_num_, uint(hid_map_button_mask_));

  return device_type_ != NDOF_UnknownDevice;
}

#undef LOG

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

static CLG_LogRef LOG_NDOF_BUTTONS = {"ghost.ndof.buttons"};
#define LOG (&LOG_NDOF_BUTTONS)

static GHOST_TKey ghost_map_keyboard_from_ndof_buttom(const NDOF_ButtonT button)
{
  switch (button) {
    case NDOF_BUTTON_ESC: {
      return GHOST_kKeyEsc;
    }
    case NDOF_BUTTON_ENTER: {
      return GHOST_kKeyEnter;
    }
    case NDOF_BUTTON_DELETE: {
      return GHOST_kKeyDelete;
    }
    case NDOF_BUTTON_TAB: {
      return GHOST_kKeyTab;
    }
    case NDOF_BUTTON_SPACE: {
      return GHOST_kKeySpace;
    }
    case NDOF_BUTTON_ALT: {
      return GHOST_kKeyLeftAlt;
    }
    case NDOF_BUTTON_SHIFT: {
      return GHOST_kKeyLeftShift;
    }
    case NDOF_BUTTON_CTRL: {
      return GHOST_kKeyLeftControl;
    }
    default: {
      return GHOST_kKeyUnknown;
    }
  }
}

void GHOST_NDOFManager::sendButtonEvent(NDOF_ButtonT button,
                                        bool press,
                                        uint64_t time,
                                        GHOST_IWindow *window)
{
  GHOST_ASSERT(button > NDOF_BUTTON_NONE && button < NDOF_BUTTON_NUM,
               "rogue button trying to escape NDOF manager");

  GHOST_EventNDOFButton *event = new GHOST_EventNDOFButton(time, window);
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
  GHOST_EventKey *event = new GHOST_EventKey(time, type, window, key, false);

  system_.pushEvent(event);
}

void GHOST_NDOFManager::updateButton(int button_number, bool press, uint64_t time)
{
  if (button_number >= hid_map_button_num_) {
    CLOG_INFO(LOG,
              2,
              "button=%d, press=%d (out of range %d, ignoring!)",
              button_number,
              int(press),
              hid_map_button_num_);
    return;
  }
  const NDOF_ButtonT button = hid_map_[button_number];
  if (button == NDOF_BUTTON_NONE) {
    CLOG_INFO(
        LOG, 2, "button=%d, press=%d (mapped to none, ignoring!)", button_number, int(press));
    return;
  }

  CLOG_INFO(LOG,
            2,
            "button=%d, press=%d, name=%s",
            button_number,
            int(press),
            ndof_button_names[button]);

  GHOST_IWindow *window = system_.getWindowManager()->getActiveWindow();

  /* Delivery will fail, so don't bother sending.
   * Do, however update the buttons internal depressed state. */
  if (window != nullptr) {
    const GHOST_TKey key = ghost_map_keyboard_from_ndof_buttom(button);
    if (key != GHOST_kKeyUnknown) {
      sendKeyEvent(key, press, time, window);
    }
    else {
      sendButtonEvent(button, press, time, window);
    }
  }

  int mask = 1 << button_number;
  if (press) {
    button_depressed_ |= mask; /* Set this button's bit. */
  }
  else {
    button_depressed_ &= ~mask; /* Clear this button's bit. */
  }
}

void GHOST_NDOFManager::updateButtons(int button_bits, uint64_t time)
{
  button_bits &= hid_map_button_mask_; /* Discard any "garbage" bits. */

  int diff = button_depressed_ ^ button_bits;

  for (int button_number = 0; button_number < hid_map_button_num_; ++button_number) {
    int mask = 1 << button_number;

    if (diff & mask) {
      bool press = button_bits & mask;
      updateButton(button_number, press, time);
    }
  }
}

#undef LOG

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Motion
 * \{ */

static CLG_LogRef LOG_NDOF_MOTION = {"ghost.ndof.motion"};
#define LOG (&LOG_NDOF_MOTION)

void GHOST_NDOFManager::setDeadZone(float dz)
{
  if (dz < 0.0f) {
    /* Negative values don't make sense, so clamp at zero. */
    dz = 0.0f;
  }
  motion_dead_zone_ = dz;

  /* Warn the rogue user/developer about high dead-zone, but allow it. */
  CLOG_INFO(LOG, 2, "dead zone set to %.2f%s", dz, (dz > 0.5f) ? " (unexpectedly high)" : "");
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

  GHOST_EventNDOFMotion *event = new GHOST_EventNDOFMotion(motion_time_, window);
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
        data->dt = 0.0125f;
      }
      else {
        /* Send no event and keep current state. */
        CLOG_INFO(LOG, 2, "motion ignored");
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
  CLOG_INFO(LOG,
            2,
            "motion sent, T=(%.2f,%.2f,%.2f), R=(%.2f,%.2f,%.2f) dt=%.3f, status=%s",
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
  CLOG_INFO(LOG,
            2,
            "motion sent, T=(%d,%d,%d) R=(%d,%d,%d) status=%s",
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
