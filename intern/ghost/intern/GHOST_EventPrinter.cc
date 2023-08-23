/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventPrinter class.
 */

#include "GHOST_EventPrinter.hh"
#include "GHOST_Debug.hh"
#include "GHOST_EventDragnDrop.hh"
#include "GHOST_EventKey.hh"

#include <iomanip>
#include <iostream>

#include <cstdio>

static const char *getButtonActionString(const GHOST_TButtonAction action)
{
  switch (action) {
    case GHOST_kPress:
      return "Press";
    case GHOST_kRelease:
      return "Release";
  }
  return "Unknown";
}

bool GHOST_EventPrinter::processEvent(GHOST_IEvent *event)
{
  bool handled = false;

  GHOST_ASSERT(event, "event==0");

  if (event->getType() == GHOST_kEventWindowUpdate) {
    return false;
  }
  std::cout << "GHOST_EventPrinter::processEvent, time: " << int32_t(event->getTime())
            << ", type: ";

#define CASE_TYPE(ty) \
  case ty: { \
    std::cout << #ty; \
    handled = true; \
    break; \
  } \
    ((void)0)

  const GHOST_TEventType event_type = event->getType();
  switch (event_type) {
    case GHOST_kEventUnknown: {
      std::cout << "GHOST_kEventUnknown";
      handled = false;
      break;
    }
    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cursorData =
          (GHOST_TEventCursorData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventCursorMove, (x,y): (" << cursorData->x << "," << cursorData->y
                << ")";
      handled = true;
      break;
    }
    case GHOST_kEventButtonDown: {
      GHOST_TEventButtonData *buttonData =
          (GHOST_TEventButtonData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventButtonDown, button: " << buttonData->button;
      handled = true;
      break;
    }
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *buttonData =
          (GHOST_TEventButtonData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventCursorButtonUp, button: " << buttonData->button;
      handled = true;
      break;
    }
    case GHOST_kEventWheel: {
      GHOST_TEventWheelData *wheelData =
          (GHOST_TEventWheelData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventWheel, z: " << wheelData->z;
      handled = true;
      break;
    }

      CASE_TYPE(GHOST_kEventTrackpad);

#ifdef WITH_INPUT_NDOF
    case GHOST_kEventNDOFMotion: {
      const GHOST_TEventNDOFMotionData *ndof_motion =
          (GHOST_TEventNDOFMotionData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventNDOFMotion: ";
      std::cout << std::fixed << std::setprecision(2) <<
          /* Translation. */
          "tx=" << ndof_motion->tx << " ty=" << ndof_motion->tx << " tz=" << ndof_motion->tz <<
          /* Rotation. */
          "rx=" << ndof_motion->tx << " ry=" << ndof_motion->rx << " rz=" << ndof_motion->rz;
      std::cout << std::fixed << std::setprecision(4) << " dt=" << ndof_motion->dt;
      handled = true;
      break;
    }

    case GHOST_kEventNDOFButton: {
      const GHOST_TEventNDOFButtonData *ndof_button =
          (GHOST_TEventNDOFButtonData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventNDOFButton: " << getButtonActionString(ndof_button->action)
                << " button=" << ndof_button->button;
      handled = true;
      break;
    }
#endif

    case GHOST_kEventKeyDown: {
      GHOST_TEventKeyData *keyData = (GHOST_TEventKeyData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventKeyDown, key: " << getKeyString(keyData->key);
      handled = true;
      break;
    }
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *keyData = (GHOST_TEventKeyData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventKeyUp, key: " << getKeyString(keyData->key);
      handled = true;
      break;
    }

      CASE_TYPE(GHOST_kEventQuitRequest);
      CASE_TYPE(GHOST_kEventWindowClose);
      CASE_TYPE(GHOST_kEventWindowActivate);
      CASE_TYPE(GHOST_kEventWindowDeactivate);
      CASE_TYPE(GHOST_kEventWindowUpdate);
      CASE_TYPE(GHOST_kEventWindowUpdateDecor);
      CASE_TYPE(GHOST_kEventWindowSize);
      CASE_TYPE(GHOST_kEventWindowMove);
      CASE_TYPE(GHOST_kEventWindowDPIHintChanged);

    case GHOST_kEventDraggingEntered: {
      GHOST_TEventDragnDropData *dragnDropData =
          (GHOST_TEventDragnDropData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventDraggingEntered, dragged object type : "
                << dragnDropData->dataType;
      std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
      handled = true;
      break;
    }
    case GHOST_kEventDraggingUpdated: {
      GHOST_TEventDragnDropData *dragnDropData =
          (GHOST_TEventDragnDropData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventDraggingUpdated, dragged object type : "
                << dragnDropData->dataType;
      std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
      handled = true;
      break;
    }
    case GHOST_kEventDraggingExited: {
      GHOST_TEventDragnDropData *dragnDropData =
          (GHOST_TEventDragnDropData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventDraggingExited, dragged object type : " << dragnDropData->dataType;
      handled = true;
      break;
    }
    case GHOST_kEventDraggingDropDone: {
      GHOST_TEventDragnDropData *dragnDropData =
          (GHOST_TEventDragnDropData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventDraggingDropDone,";
      std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
      switch (dragnDropData->dataType) {
        case GHOST_kDragnDropTypeString: {
          std::cout << " type : GHOST_kDragnDropTypeString,";
          std::cout << "\n  String received = " << (char *)dragnDropData->data;
          break;
        }
        case GHOST_kDragnDropTypeFilenames: {
          GHOST_TStringArray *strArray = (GHOST_TStringArray *)dragnDropData->data;
          int i;
          std::cout << " type : GHOST_kDragnDropTypeFilenames,";
          std::cout << "\n  Received " << strArray->count << " filename"
                    << (strArray->count > 1 ? "s:" : ":");
          for (i = 0; i < strArray->count; i++) {
            std::cout << "\n    File[" << i << "] : " << strArray->strings[i];
          }
          break;
        }
        default: {
          break;
        }
      }
      handled = true;
      break;
    }
    case GHOST_kEventOpenMainFile: {
      GHOST_TEventDataPtr eventData = ((GHOST_IEvent *)event)->getData();

      if (eventData) {
        std::cout << "GHOST_kEventOpenMainFile for path : " << (char *)eventData;
      }
      else {
        std::cout << "GHOST_kEventOpenMainFile with no path specified!!";
      }
      handled = true;
      break;
    }

      CASE_TYPE(GHOST_kEventNativeResolutionChange);

      CASE_TYPE(GHOST_kEventTimer);

      CASE_TYPE(GHOST_kEventImeCompositionStart);
      CASE_TYPE(GHOST_kEventImeComposition);
      CASE_TYPE(GHOST_kEventImeCompositionEnd);
  }

#undef CASE_TYPE

  if ((handled == false) && event_type != GHOST_kEventUnknown) {
    std::cout << "not found";
  }

  std::cout << std::endl;

  std::cout.flush();

  return handled;
}

const char *GHOST_EventPrinter::getKeyString(const GHOST_TKey key) const
{
  const char *tstr = nullptr;

#define CASE_KEY(k, v) \
  case k: { \
    tstr = v; \
    break; \
  }

  switch (key) {
    CASE_KEY(GHOST_kKeyBackSpace, "BackSpace");
    CASE_KEY(GHOST_kKeyTab, "Tab");
    CASE_KEY(GHOST_kKeyLinefeed, "Linefeed");
    CASE_KEY(GHOST_kKeyClear, "Clear");
    CASE_KEY(GHOST_kKeyEnter, "Enter");
    CASE_KEY(GHOST_kKeyEsc, "Esc");
    CASE_KEY(GHOST_kKeySpace, "Space");
    CASE_KEY(GHOST_kKeyQuote, "Quote");
    CASE_KEY(GHOST_kKeyBackslash, "\\");
    CASE_KEY(GHOST_kKeyAccentGrave, "`");
    CASE_KEY(GHOST_kKeyLeftShift, "LeftShift");
    CASE_KEY(GHOST_kKeyRightShift, "RightShift");
    CASE_KEY(GHOST_kKeyLeftControl, "LeftControl");
    CASE_KEY(GHOST_kKeyRightControl, "RightControl");
    CASE_KEY(GHOST_kKeyLeftAlt, "LeftAlt");
    CASE_KEY(GHOST_kKeyRightAlt, "RightAlt");
    CASE_KEY(GHOST_kKeyLeftOS, "LeftOS");
    CASE_KEY(GHOST_kKeyRightOS, "RightOS");
    CASE_KEY(GHOST_kKeyApp, "App");
    CASE_KEY(GHOST_kKeyGrLess, "GrLess");
    CASE_KEY(GHOST_kKeyCapsLock, "CapsLock");
    CASE_KEY(GHOST_kKeyNumLock, "NumLock");
    CASE_KEY(GHOST_kKeyScrollLock, "ScrollLock");
    CASE_KEY(GHOST_kKeyLeftArrow, "LeftArrow");
    CASE_KEY(GHOST_kKeyRightArrow, "RightArrow");
    CASE_KEY(GHOST_kKeyUpArrow, "UpArrow");
    CASE_KEY(GHOST_kKeyDownArrow, "DownArrow");
    CASE_KEY(GHOST_kKeyPrintScreen, "PrintScreen");
    CASE_KEY(GHOST_kKeyPause, "Pause");
    CASE_KEY(GHOST_kKeyInsert, "Insert");
    CASE_KEY(GHOST_kKeyDelete, "Delete");
    CASE_KEY(GHOST_kKeyHome, "Home");
    CASE_KEY(GHOST_kKeyEnd, "End");
    CASE_KEY(GHOST_kKeyUpPage, "UpPage");
    CASE_KEY(GHOST_kKeyDownPage, "DownPage");
    CASE_KEY(GHOST_kKeyNumpadPeriod, "NumpadPeriod");
    CASE_KEY(GHOST_kKeyNumpadEnter, "NumpadEnter");
    CASE_KEY(GHOST_kKeyNumpadPlus, "NumpadPlus");
    CASE_KEY(GHOST_kKeyNumpadMinus, "NumpadMinus");
    CASE_KEY(GHOST_kKeyNumpadAsterisk, "NumpadAsterisk");
    CASE_KEY(GHOST_kKeyNumpadSlash, "NumpadSlash");
    CASE_KEY(GHOST_kKeyMediaPlay, "MediaPlayPause");
    CASE_KEY(GHOST_kKeyMediaStop, "MediaStop");
    CASE_KEY(GHOST_kKeyMediaFirst, "MediaFirst");
    CASE_KEY(GHOST_kKeyMediaLast, "MediaLast");
    CASE_KEY(GHOST_kKeyNumpad0, "Numpad 0");
    CASE_KEY(GHOST_kKeyNumpad1, "Numpad 1");
    CASE_KEY(GHOST_kKeyNumpad2, "Numpad 2");
    CASE_KEY(GHOST_kKeyNumpad3, "Numpad 3");
    CASE_KEY(GHOST_kKeyNumpad4, "Numpad 4");
    CASE_KEY(GHOST_kKeyNumpad5, "Numpad 5");
    CASE_KEY(GHOST_kKeyNumpad6, "Numpad 6");
    CASE_KEY(GHOST_kKeyNumpad7, "Numpad 7");
    CASE_KEY(GHOST_kKeyNumpad8, "Numpad 8");
    CASE_KEY(GHOST_kKeyNumpad9, "Numpad 9");

    CASE_KEY(GHOST_kKeyF1, "F1");
    CASE_KEY(GHOST_kKeyF2, "F2");
    CASE_KEY(GHOST_kKeyF3, "F3");
    CASE_KEY(GHOST_kKeyF4, "F4");
    CASE_KEY(GHOST_kKeyF5, "F5");
    CASE_KEY(GHOST_kKeyF6, "F6");
    CASE_KEY(GHOST_kKeyF7, "F7");
    CASE_KEY(GHOST_kKeyF8, "F8");
    CASE_KEY(GHOST_kKeyF9, "F9");
    CASE_KEY(GHOST_kKeyF10, "F10");
    CASE_KEY(GHOST_kKeyF11, "F11");
    CASE_KEY(GHOST_kKeyF12, "F12");
    CASE_KEY(GHOST_kKeyF13, "F13");
    CASE_KEY(GHOST_kKeyF14, "F14");
    CASE_KEY(GHOST_kKeyF15, "F15");
    CASE_KEY(GHOST_kKeyF16, "F16");
    CASE_KEY(GHOST_kKeyF17, "F17");
    CASE_KEY(GHOST_kKeyF18, "F18");
    CASE_KEY(GHOST_kKeyF19, "F19");
    CASE_KEY(GHOST_kKeyF20, "F20");
    CASE_KEY(GHOST_kKeyF21, "F21");
    CASE_KEY(GHOST_kKeyF22, "F22");
    CASE_KEY(GHOST_kKeyF23, "F23");
    CASE_KEY(GHOST_kKeyF24, "F24");

    CASE_KEY(GHOST_kKeyUnknown, "Unknown");

    CASE_KEY(GHOST_kKeyComma, ",");
    CASE_KEY(GHOST_kKeyMinus, "-");
    CASE_KEY(GHOST_kKeyPlus, "=");
    CASE_KEY(GHOST_kKeyPeriod, ".");
    CASE_KEY(GHOST_kKeySlash, "/");
    CASE_KEY(GHOST_kKey0, "0");
    CASE_KEY(GHOST_kKey1, "1");
    CASE_KEY(GHOST_kKey2, "2");
    CASE_KEY(GHOST_kKey3, "3");
    CASE_KEY(GHOST_kKey4, "4");
    CASE_KEY(GHOST_kKey5, "5");
    CASE_KEY(GHOST_kKey6, "6");
    CASE_KEY(GHOST_kKey7, "7");
    CASE_KEY(GHOST_kKey8, "8");
    CASE_KEY(GHOST_kKey9, "9");
    CASE_KEY(GHOST_kKeySemicolon, ";");
    CASE_KEY(GHOST_kKeyEqual, "=");
    CASE_KEY(GHOST_kKeyA, "A");
    CASE_KEY(GHOST_kKeyB, "B");
    CASE_KEY(GHOST_kKeyC, "C");
    CASE_KEY(GHOST_kKeyD, "D");
    CASE_KEY(GHOST_kKeyE, "E");
    CASE_KEY(GHOST_kKeyF, "F");
    CASE_KEY(GHOST_kKeyG, "G");
    CASE_KEY(GHOST_kKeyH, "H");
    CASE_KEY(GHOST_kKeyI, "I");
    CASE_KEY(GHOST_kKeyJ, "J");
    CASE_KEY(GHOST_kKeyK, "K");
    CASE_KEY(GHOST_kKeyL, "L");
    CASE_KEY(GHOST_kKeyM, "M");
    CASE_KEY(GHOST_kKeyN, "N");
    CASE_KEY(GHOST_kKeyO, "O");
    CASE_KEY(GHOST_kKeyP, "P");
    CASE_KEY(GHOST_kKeyQ, "Q");
    CASE_KEY(GHOST_kKeyR, "R");
    CASE_KEY(GHOST_kKeyS, "S");
    CASE_KEY(GHOST_kKeyT, "T");
    CASE_KEY(GHOST_kKeyU, "U");
    CASE_KEY(GHOST_kKeyV, "V");
    CASE_KEY(GHOST_kKeyW, "W");
    CASE_KEY(GHOST_kKeyX, "X");
    CASE_KEY(GHOST_kKeyY, "Y");
    CASE_KEY(GHOST_kKeyZ, "Z");
    CASE_KEY(GHOST_kKeyLeftBracket, "[");
    CASE_KEY(GHOST_kKeyRightBracket, "]");
  }

#undef CASE_KEY

  /* Shouldn't happen (the value is not known to #GHOST_TKey). */
  if (tstr == nullptr) {
    tstr = "Invalid";
  }
  return tstr;
}
