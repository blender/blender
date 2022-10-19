/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GHOST_NDOFManager.h"
#include "GHOST_Debug.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventNDOF.h"
#include "GHOST_WindowManager.h"
#include "GHOST_utildefines.h"

/* Logging, use `ghost.ndof.*` prefix. */
#include "CLG_log.h"

#include <climits>
#include <cmath>
#include <cstdio>  /* For error/info reporting. */
#include <cstring> /* For memory functions. */

/* Printable version of each GHOST_TProgress value. */
static const char *progress_string[] = {
    "not started",
    "starting",
    "in progress",
    "finishing",
    "finished",
};

static const char *ndof_button_names[] = {
    /* used internally, never sent */
    "NDOF_BUTTON_NONE",
    /* these two are available from any 3Dconnexion device */
    "NDOF_BUTTON_MENU",
    "NDOF_BUTTON_FIT",
    /* standard views */
    "NDOF_BUTTON_TOP",
    "NDOF_BUTTON_BOTTOM",
    "NDOF_BUTTON_LEFT",
    "NDOF_BUTTON_RIGHT",
    "NDOF_BUTTON_FRONT",
    "NDOF_BUTTON_BACK",
    /* more views */
    "NDOF_BUTTON_ISO1",
    "NDOF_BUTTON_ISO2",
    /* 90 degree rotations */
    "NDOF_BUTTON_ROLL_CW",
    "NDOF_BUTTON_ROLL_CCW",
    "NDOF_BUTTON_SPIN_CW",
    "NDOF_BUTTON_SPIN_CCW",
    "NDOF_BUTTON_TILT_CW",
    "NDOF_BUTTON_TILT_CCW",
    /* device control */
    "NDOF_BUTTON_ROTATE",
    "NDOF_BUTTON_PANZOOM",
    "NDOF_BUTTON_DOMINANT",
    "NDOF_BUTTON_PLUS",
    "NDOF_BUTTON_MINUS",
    /* general-purpose buttons */
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
    /* more general-purpose buttons */
    "NDOF_BUTTON_A",
    "NDOF_BUTTON_B",
    "NDOF_BUTTON_C",
    /* Stored views. */
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

/* Shared by the latest 3Dconnexion hardware
 * SpacePilotPro uses all of these
 * smaller devices use only some, based on button mask. */
static const NDOF_ButtonT Modern3Dx_HID_map[] = {
    NDOF_BUTTON_MENU,     NDOF_BUTTON_FIT,      NDOF_BUTTON_TOP,    NDOF_BUTTON_LEFT,
    NDOF_BUTTON_RIGHT,    NDOF_BUTTON_FRONT,    NDOF_BUTTON_BOTTOM, NDOF_BUTTON_BACK,
    NDOF_BUTTON_ROLL_CW,  NDOF_BUTTON_ROLL_CCW, NDOF_BUTTON_ISO1,   NDOF_BUTTON_ISO2,
    NDOF_BUTTON_1,        NDOF_BUTTON_2,        NDOF_BUTTON_3,      NDOF_BUTTON_4,
    NDOF_BUTTON_5,        NDOF_BUTTON_6,        NDOF_BUTTON_7,      NDOF_BUTTON_8,
    NDOF_BUTTON_9,        NDOF_BUTTON_10,       NDOF_BUTTON_ESC,    NDOF_BUTTON_ALT,
    NDOF_BUTTON_SHIFT,    NDOF_BUTTON_CTRL,     NDOF_BUTTON_ROTATE, NDOF_BUTTON_PANZOOM,
    NDOF_BUTTON_DOMINANT, NDOF_BUTTON_PLUS,     NDOF_BUTTON_MINUS};

static const NDOF_ButtonT SpaceExplorer_HID_map[] = {
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

/* This is the older SpacePilot (sans Pro)
 * thanks to polosson for info about this device. */
static const NDOF_ButtonT SpacePilot_HID_map[] = {
    NDOF_BUTTON_1,     NDOF_BUTTON_2,     NDOF_BUTTON_3,        NDOF_BUTTON_4,
    NDOF_BUTTON_5,     NDOF_BUTTON_6,     NDOF_BUTTON_TOP,      NDOF_BUTTON_LEFT,
    NDOF_BUTTON_RIGHT, NDOF_BUTTON_FRONT, NDOF_BUTTON_ESC,      NDOF_BUTTON_ALT,
    NDOF_BUTTON_SHIFT, NDOF_BUTTON_CTRL,  NDOF_BUTTON_FIT,      NDOF_BUTTON_MENU,
    NDOF_BUTTON_PLUS,  NDOF_BUTTON_MINUS, NDOF_BUTTON_DOMINANT, NDOF_BUTTON_ROTATE,
    NDOF_BUTTON_NONE /* the CONFIG button -- what does it do? */
};

static const NDOF_ButtonT Generic_HID_map[] = {
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
static const NDOF_ButtonT SpaceMouseEnterprise_HID_map[] = {
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

static const int genericButtonCount = ARRAY_SIZE(Generic_HID_map);

GHOST_NDOFManager::GHOST_NDOFManager(GHOST_System &sys)
    : m_system(sys),
      m_deviceType(NDOF_UnknownDevice), /* Each platform has its own device detection code. */
      m_buttonCount(genericButtonCount),
      m_buttonMask(0),
      m_hidMap(Generic_HID_map),
      m_buttons(0),
      m_motionTime(0),
      m_prevMotionTime(0),
      m_motionState(GHOST_kNotStarted),
      m_motionEventPending(false),
      m_deadZone(0.0f)
{
  /* To avoid the rare situation where one triple is updated and
   * the other is not, initialize them both here: */
  memset(m_translation, 0, sizeof(m_translation));
  memset(m_rotation, 0, sizeof(m_rotation));
}

/* -------------------------------------------------------------------- */
/** \name NDOF Device Setup
 * \{ */

static CLG_LogRef LOG_NDOF_DEVICE = {"ghost.ndof.device"};
#define LOG (&LOG_NDOF_DEVICE)

bool GHOST_NDOFManager::setDevice(ushort vendor_id, ushort product_id)
{
  /* Call this function until it returns true
   * it's a good idea to stop calling it after that, as it will "forget"
   * whichever device it already found */

  /* Default to safe generic behavior for "unknown" devices
   * unidentified devices will emit motion events like normal
   * rogue buttons do nothing by default, but can be customized by the user. */

  m_deviceType = NDOF_UnknownDevice;
  m_hidMap = Generic_HID_map;
  m_buttonCount = genericButtonCount;
  m_buttonMask = 0;

  /* "mystery device" owners can help build a HID_map for their hardware
   * A few users have already contributed information about several older devices
   * that I don't have access to. Thanks! */

  switch (vendor_id) {
    case 0x046D: /* Logitech (3Dconnexion was a subsidiary). */
      switch (product_id) {
        /* -- current devices -- */
        case 0xC626: /* full-size SpaceNavigator */
        case 0xC628: /* the "for Notebooks" one */
          puts("ndof: using SpaceNavigator");
          m_deviceType = NDOF_SpaceNavigator;
          m_buttonCount = 2;
          m_hidMap = Modern3Dx_HID_map;
          break;
        case 0xC627:
          puts("ndof: using SpaceExplorer");
          m_deviceType = NDOF_SpaceExplorer;
          m_buttonCount = 15;
          m_hidMap = SpaceExplorer_HID_map;
          break;
        case 0xC629:
          puts("ndof: using SpacePilot Pro");
          m_deviceType = NDOF_SpacePilotPro;
          m_buttonCount = 31;
          m_hidMap = Modern3Dx_HID_map;
          break;
        case 0xC62B:
          puts("ndof: using SpaceMouse Pro");
          m_deviceType = NDOF_SpaceMousePro;
          m_buttonCount = 27;
          /* ^^ actually has 15 buttons, but their HID codes range from 0 to 26 */
          m_buttonMask = 0x07C0F137;
          m_hidMap = Modern3Dx_HID_map;
          break;

        /* -- older devices -- */
        case 0xC625:
          puts("ndof: using SpacePilot");
          m_deviceType = NDOF_SpacePilot;
          m_buttonCount = 21;
          m_hidMap = SpacePilot_HID_map;
          break;
        case 0xC621:
          puts("ndof: using Spaceball 5000");
          m_deviceType = NDOF_Spaceball5000;
          m_buttonCount = 12;
          break;
        case 0xC623:
          puts("ndof: using SpaceTraveler");
          m_deviceType = NDOF_SpaceTraveler;
          m_buttonCount = 8;
          break;

        default:
          printf("ndof: unknown Logitech product %04hx\n", product_id);
      }
      break;
    case 0x256F: /* 3Dconnexion */
      switch (product_id) {
        case 0xC62E: /* Plugged in. */
        case 0xC62F: /* Wireless. */
          puts("ndof: using SpaceMouse Wireless");
          m_deviceType = NDOF_SpaceMouseWireless;
          m_buttonCount = 2;
          m_hidMap = Modern3Dx_HID_map;
          break;
        case 0xC631: /* Plugged in. */
        case 0xC632: /* Wireless. */
          puts("ndof: using SpaceMouse Pro Wireless");
          m_deviceType = NDOF_SpaceMouseProWireless;
          m_buttonCount = 27;
          /* ^^ actually has 15 buttons, but their HID codes range from 0 to 26. */
          m_buttonMask = 0x07C0F137;
          m_hidMap = Modern3Dx_HID_map;
          break;
        case 0xC633:
          puts("ndof: using SpaceMouse Enterprise");
          m_deviceType = NDOF_SpaceMouseEnterprise;
          m_buttonCount = 31;
          m_hidMap = SpaceMouseEnterprise_HID_map;
          break;

        default:
          printf("ndof: unknown 3Dconnexion product %04hx\n", product_id);
      }
      break;
    default:
      printf("ndof: unknown device %04hx:%04hx\n", vendor_id, product_id);
  }

  if (m_buttonMask == 0) {
    m_buttonMask = int(~(UINT_MAX << m_buttonCount));
  }

  CLOG_INFO(LOG, 2, "%d buttons -> hex:%X", m_buttonCount, (uint)m_buttonMask);

  return m_deviceType != NDOF_UnknownDevice;
}

#undef LOG

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Update State
 * \{ */

void GHOST_NDOFManager::updateTranslation(const int t[3], uint64_t time)
{
  memcpy(m_translation, t, sizeof(m_translation));
  m_motionTime = time;
  m_motionEventPending = true;
}

void GHOST_NDOFManager::updateRotation(const int r[3], uint64_t time)
{
  memcpy(m_rotation, r, sizeof(m_rotation));
  m_motionTime = time;
  m_motionEventPending = true;
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

  m_system.pushEvent(event);
}

void GHOST_NDOFManager::sendKeyEvent(GHOST_TKey key,
                                     bool press,
                                     uint64_t time,
                                     GHOST_IWindow *window)
{
  GHOST_TEventType type = press ? GHOST_kEventKeyDown : GHOST_kEventKeyUp;
  GHOST_EventKey *event = new GHOST_EventKey(time, type, window, key, false);

  m_system.pushEvent(event);
}

void GHOST_NDOFManager::updateButton(int button_number, bool press, uint64_t time)
{
  if (button_number >= m_buttonCount) {
    CLOG_INFO(LOG,
              2,
              "button=%d, press=%d (out of range %d, ignoring!)",
              button_number,
              (int)press,
              m_buttonCount);
    return;
  }
  const NDOF_ButtonT button = m_hidMap[button_number];
  if (button == NDOF_BUTTON_NONE) {
    CLOG_INFO(
        LOG, 2, "button=%d, press=%d (mapped to none, ignoring!)", button_number, (int)press);
    return;
  }

  CLOG_INFO(LOG,
            2,
            "button=%d, press=%d, name=%s",
            button_number,
            (int)press,
            ndof_button_names[button]);

  GHOST_IWindow *window = m_system.getWindowManager()->getActiveWindow();
  const GHOST_TKey key = ghost_map_keyboard_from_ndof_buttom(button);
  if (key != GHOST_kKeyUnknown) {
    sendKeyEvent(key, press, time, window);
  }
  else {
    sendButtonEvent(button, press, time, window);
  }

  int mask = 1 << button_number;
  if (press) {
    m_buttons |= mask; /* Set this button's bit. */
  }
  else {
    m_buttons &= ~mask; /* Clear this button's bit. */
  }
}

void GHOST_NDOFManager::updateButtons(int button_bits, uint64_t time)
{
  button_bits &= m_buttonMask; /* Discard any "garbage" bits. */

  int diff = m_buttons ^ button_bits;

  for (int button_number = 0; button_number < m_buttonCount; ++button_number) {
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
  m_deadZone = dz;

  /* Warn the rogue user/developer about high dead-zone, but allow it. */
  CLOG_INFO(LOG, 2, "dead zone set to %.2f%s", dz, (dz > 0.5f) ? " (unexpectedly high)" : "");
}

static bool atHomePosition(GHOST_TEventNDOFMotionData *ndof)
{
#define HOME(foo) (ndof->foo == 0.0f)
  return HOME(tx) && HOME(ty) && HOME(tz) && HOME(rx) && HOME(ry) && HOME(rz);
#undef HOME
}

static bool nearHomePosition(GHOST_TEventNDOFMotionData *ndof, float threshold)
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
  if (!m_motionEventPending) {
    return false;
  }

  m_motionEventPending = false; /* Any pending motion is handled right now. */

  GHOST_IWindow *window = m_system.getWindowManager()->getActiveWindow();

  if (window == nullptr) {
    m_motionState = GHOST_kNotStarted; /* Avoid large `dt` times when changing windows. */
    return false;                      /* Delivery will fail, so don't bother sending. */
  }

  GHOST_EventNDOFMotion *event = new GHOST_EventNDOFMotion(m_motionTime, window);
  GHOST_TEventNDOFMotionData *data = (GHOST_TEventNDOFMotionData *)event->getData();

  /* Scale axis values here to normalize them to around +/- 1
   * they are scaled again for overall sensitivity in the WM based on user preferences. */

  const float scale = 1.0f / 350.0f; /* 3Dconnexion devices send +/- 350 usually */

  data->tx = scale * m_translation[0];
  data->ty = scale * m_translation[1];
  data->tz = scale * m_translation[2];

  data->rx = scale * m_rotation[0];
  data->ry = scale * m_rotation[1];
  data->rz = scale * m_rotation[2];
  data->dt = 0.001f * (m_motionTime - m_prevMotionTime); /* In seconds. */
  m_prevMotionTime = m_motionTime;

  bool weHaveMotion = !nearHomePosition(data, m_deadZone);

  /* Determine what kind of motion event to send `(Starting, InProgress, Finishing)`
   * and where that leaves this NDOF manager `(NotStarted, InProgress, Finished)`. */
  switch (m_motionState) {
    case GHOST_kNotStarted:
    case GHOST_kFinished: {
      if (weHaveMotion) {
        data->progress = GHOST_kStarting;
        m_motionState = GHOST_kInProgress;
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
        m_motionState = GHOST_kFinished;
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
            progress_string[data->progress]);
#else
  /* Raw values, may be useful for debugging. */
  CLOG_INFO(LOG,
            2,
            "motion sent, T=(%d,%d,%d) R=(%d,%d,%d) status=%s",
            m_translation[0],
            m_translation[1],
            m_translation[2],
            m_rotation[0],
            m_rotation[1],
            m_rotation[2],
            progress_string[data->progress]);
#endif
  m_system.pushEvent(event);

  return true;
}

/** \} */
