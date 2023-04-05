/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventPrinter class.
 */

#include "GHOST_EventPrinter.hh"
#include "GHOST_Debug.hh"
#include "GHOST_EventDragnDrop.hh"
#include "GHOST_EventKey.hh"
#include <iostream>

#include <cstdio>

bool GHOST_EventPrinter::processEvent(GHOST_IEvent *event)
{
  bool handled = true;

  GHOST_ASSERT(event, "event==0");

  if (event->getType() == GHOST_kEventWindowUpdate) {
    return false;
  }
  std::cout << "GHOST_EventPrinter::processEvent, time: " << (int32_t)event->getTime()
            << ", type: ";
  switch (event->getType()) {
    case GHOST_kEventUnknown:
      std::cout << "GHOST_kEventUnknown";
      handled = false;
      break;

    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *buttonData =
          (GHOST_TEventButtonData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventCursorButtonUp, button: " << buttonData->button;
    } break;
    case GHOST_kEventButtonDown: {
      GHOST_TEventButtonData *buttonData =
          (GHOST_TEventButtonData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventButtonDown, button: " << buttonData->button;
    } break;

    case GHOST_kEventWheel: {
      GHOST_TEventWheelData *wheelData =
          (GHOST_TEventWheelData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventWheel, z: " << wheelData->z;
    } break;

    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cursorData =
          (GHOST_TEventCursorData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventCursorMove, (x,y): (" << cursorData->x << "," << cursorData->y
                << ")";
    } break;

    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *keyData = (GHOST_TEventKeyData *)((GHOST_IEvent *)event)->getData();
      char str[32] = {'\0'};
      getKeyString(keyData->key, str);
      std::cout << "GHOST_kEventKeyUp, key: " << str;
    } break;
    case GHOST_kEventKeyDown: {
      GHOST_TEventKeyData *keyData = (GHOST_TEventKeyData *)((GHOST_IEvent *)event)->getData();
      char str[32] = {'\0'};
      getKeyString(keyData->key, str);
      std::cout << "GHOST_kEventKeyDown, key: " << str;
    } break;

    case GHOST_kEventDraggingEntered: {
      GHOST_TEventDragnDropData *dragnDropData =
          (GHOST_TEventDragnDropData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventDraggingEntered, dragged object type : "
                << dragnDropData->dataType;
      std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
    } break;

    case GHOST_kEventDraggingUpdated: {
      GHOST_TEventDragnDropData *dragnDropData =
          (GHOST_TEventDragnDropData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventDraggingUpdated, dragged object type : "
                << dragnDropData->dataType;
      std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
    } break;

    case GHOST_kEventDraggingExited: {
      GHOST_TEventDragnDropData *dragnDropData =
          (GHOST_TEventDragnDropData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventDraggingExited, dragged object type : " << dragnDropData->dataType;
    } break;

    case GHOST_kEventDraggingDropDone: {
      GHOST_TEventDragnDropData *dragnDropData =
          (GHOST_TEventDragnDropData *)((GHOST_IEvent *)event)->getData();
      std::cout << "GHOST_kEventDraggingDropDone,";
      std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
      switch (dragnDropData->dataType) {
        case GHOST_kDragnDropTypeString:
          std::cout << " type : GHOST_kDragnDropTypeString,";
          std::cout << "\n  String received = " << (char *)dragnDropData->data;
          break;
        case GHOST_kDragnDropTypeFilenames: {
          GHOST_TStringArray *strArray = (GHOST_TStringArray *)dragnDropData->data;
          int i;
          std::cout << " type : GHOST_kDragnDropTypeFilenames,";
          std::cout << "\n  Received " << strArray->count << " filename"
                    << (strArray->count > 1 ? "s:" : ":");
          for (i = 0; i < strArray->count; i++) {
            std::cout << "\n    File[" << i << "] : " << strArray->strings[i];
          }
        } break;
        default:
          break;
      }
    } break;

    case GHOST_kEventOpenMainFile: {
      GHOST_TEventDataPtr eventData = ((GHOST_IEvent *)event)->getData();

      if (eventData) {
        std::cout << "GHOST_kEventOpenMainFile for path : " << (char *)eventData;
      }
      else {
        std::cout << "GHOST_kEventOpenMainFile with no path specified!!";
      }
    } break;

    case GHOST_kEventQuitRequest:
      std::cout << "GHOST_kEventQuitRequest";
      break;
    case GHOST_kEventWindowClose:
      std::cout << "GHOST_kEventWindowClose";
      break;
    case GHOST_kEventWindowActivate:
      std::cout << "GHOST_kEventWindowActivate";
      break;
    case GHOST_kEventWindowDeactivate:
      std::cout << "GHOST_kEventWindowDeactivate";
      break;
    case GHOST_kEventWindowUpdate:
      std::cout << "GHOST_kEventWindowUpdate";
      break;
    case GHOST_kEventWindowSize:
      std::cout << "GHOST_kEventWindowSize";
      break;

    default:
      std::cout << "not found";
      handled = false;
      break;
  }

  std::cout << std::endl;

  std::cout.flush();

  return handled;
}

void GHOST_EventPrinter::getKeyString(GHOST_TKey key, char str[32]) const
{
  if ((key >= GHOST_kKeyComma) && (key <= GHOST_kKeyRightBracket)) {
    sprintf(str, "%c", char(key));
  }
  else if ((key >= GHOST_kKeyNumpad0) && (key <= GHOST_kKeyNumpad9)) {
    sprintf(str, "Numpad %d", (key - GHOST_kKeyNumpad0));
  }
  else if ((key >= GHOST_kKeyF1) && (key <= GHOST_kKeyF24)) {
    sprintf(str, "F%d", key - GHOST_kKeyF1 + 1);
  }
  else {
    const char *tstr = nullptr;
    switch (key) {
      case GHOST_kKeyBackSpace:
        tstr = "BackSpace";
        break;
      case GHOST_kKeyTab:
        tstr = "Tab";
        break;
      case GHOST_kKeyLinefeed:
        tstr = "Linefeed";
        break;
      case GHOST_kKeyClear:
        tstr = "Clear";
        break;
      case GHOST_kKeyEnter:
        tstr = "Enter";
        break;
      case GHOST_kKeyEsc:
        tstr = "Esc";
        break;
      case GHOST_kKeySpace:
        tstr = "Space";
        break;
      case GHOST_kKeyQuote:
        tstr = "Quote";
        break;
      case GHOST_kKeyBackslash:
        tstr = "\\";
        break;
      case GHOST_kKeyAccentGrave:
        tstr = "`";
        break;
      case GHOST_kKeyLeftShift:
        tstr = "LeftShift";
        break;
      case GHOST_kKeyRightShift:
        tstr = "RightShift";
        break;
      case GHOST_kKeyLeftControl:
        tstr = "LeftControl";
        break;
      case GHOST_kKeyRightControl:
        tstr = "RightControl";
        break;
      case GHOST_kKeyLeftAlt:
        tstr = "LeftAlt";
        break;
      case GHOST_kKeyRightAlt:
        tstr = "RightAlt";
        break;
      case GHOST_kKeyLeftOS:
        tstr = "LeftOS";
        break;
      case GHOST_kKeyRightOS:
        tstr = "RightOS";
        break;
      case GHOST_kKeyApp:
        tstr = "App";
        break;
      case GHOST_kKeyGrLess:
        // PC german!
        tstr = "GrLess";
        break;
      case GHOST_kKeyCapsLock:
        tstr = "CapsLock";
        break;
      case GHOST_kKeyNumLock:
        tstr = "NumLock";
        break;
      case GHOST_kKeyScrollLock:
        tstr = "ScrollLock";
        break;
      case GHOST_kKeyLeftArrow:
        tstr = "LeftArrow";
        break;
      case GHOST_kKeyRightArrow:
        tstr = "RightArrow";
        break;
      case GHOST_kKeyUpArrow:
        tstr = "UpArrow";
        break;
      case GHOST_kKeyDownArrow:
        tstr = "DownArrow";
        break;
      case GHOST_kKeyPrintScreen:
        tstr = "PrintScreen";
        break;
      case GHOST_kKeyPause:
        tstr = "Pause";
        break;
      case GHOST_kKeyInsert:
        tstr = "Insert";
        break;
      case GHOST_kKeyDelete:
        tstr = "Delete";
        break;
      case GHOST_kKeyHome:
        tstr = "Home";
        break;
      case GHOST_kKeyEnd:
        tstr = "End";
        break;
      case GHOST_kKeyUpPage:
        tstr = "UpPage";
        break;
      case GHOST_kKeyDownPage:
        tstr = "DownPage";
        break;
      case GHOST_kKeyNumpadPeriod:
        tstr = "NumpadPeriod";
        break;
      case GHOST_kKeyNumpadEnter:
        tstr = "NumpadEnter";
        break;
      case GHOST_kKeyNumpadPlus:
        tstr = "NumpadPlus";
        break;
      case GHOST_kKeyNumpadMinus:
        tstr = "NumpadMinus";
        break;
      case GHOST_kKeyNumpadAsterisk:
        tstr = "NumpadAsterisk";
        break;
      case GHOST_kKeyNumpadSlash:
        tstr = "NumpadSlash";
        break;
      case GHOST_kKeyMediaPlay:
        tstr = "MediaPlayPause";
        break;
      case GHOST_kKeyMediaStop:
        tstr = "MediaStop";
        break;
      case GHOST_kKeyMediaFirst:
        tstr = "MediaFirst";
        break;
      case GHOST_kKeyMediaLast:
        tstr = "MediaLast";
        break;
      default:
        tstr = "unknown";
        break;
    }

    sprintf(str, "%s", tstr);
  }
}
