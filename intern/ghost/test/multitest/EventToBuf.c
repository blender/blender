/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "EventToBuf.h"
#include "GHOST_C-api.h"

char *eventtype_to_string(GHOST_TEventType type)
{
  switch (type) {
    case GHOST_kEventCursorMove:
      return "CursorMove";
    case GHOST_kEventButtonDown:
      return "ButtonDown";
    case GHOST_kEventButtonUp:
      return "ButtonUp";

    case GHOST_kEventKeyDown:
      return "KeyDown";
    case GHOST_kEventKeyUp:
      return "KeyUp";

    case GHOST_kEventQuitRequest:
      return "Quit";

    case GHOST_kEventWindowClose:
      return "WindowClose";
    case GHOST_kEventWindowActivate:
      return "WindowActivate";
    case GHOST_kEventWindowDeactivate:
      return "WindowDeactivate";
    case GHOST_kEventWindowUpdate:
      return "WindowUpdate";
    case GHOST_kEventWindowSize:
      return "WindowSize";
    default:
      return "<invalid>";
  }
}

static char *keytype_to_string(GHOST_TKey key)
{
#define K(key) \
  case GHOST_k##key: \
    return #key;
  switch (key) {
    K(KeyBackSpace);
    K(KeyTab);
    K(KeyLinefeed);
    K(KeyClear);
    K(KeyEnter);

    K(KeyEsc);
    K(KeySpace);
    K(KeyQuote);
    K(KeyComma);
    K(KeyMinus);
    K(KeyPeriod);
    K(KeySlash);

    K(Key0);
    K(Key1);
    K(Key2);
    K(Key3);
    K(Key4);
    K(Key5);
    K(Key6);
    K(Key7);
    K(Key8);
    K(Key9);

    K(KeySemicolon);
    K(KeyEqual);

    K(KeyA);
    K(KeyB);
    K(KeyC);
    K(KeyD);
    K(KeyE);
    K(KeyF);
    K(KeyG);
    K(KeyH);
    K(KeyI);
    K(KeyJ);
    K(KeyK);
    K(KeyL);
    K(KeyM);
    K(KeyN);
    K(KeyO);
    K(KeyP);
    K(KeyQ);
    K(KeyR);
    K(KeyS);
    K(KeyT);
    K(KeyU);
    K(KeyV);
    K(KeyW);
    K(KeyX);
    K(KeyY);
    K(KeyZ);

    K(KeyLeftBracket);
    K(KeyRightBracket);
    K(KeyBackslash);
    K(KeyAccentGrave);

    K(KeyLeftShift);
    K(KeyRightShift);
    K(KeyLeftControl);
    K(KeyRightControl);
    K(KeyLeftAlt);
    K(KeyRightAlt);
    K(KeyOS);

    K(KeyCapsLock);
    K(KeyNumLock);
    K(KeyScrollLock);

    K(KeyLeftArrow);
    K(KeyRightArrow);
    K(KeyUpArrow);
    K(KeyDownArrow);

    K(KeyPrintScreen);
    K(KeyPause);

    K(KeyInsert);
    K(KeyDelete);
    K(KeyHome);
    K(KeyEnd);
    K(KeyUpPage);
    K(KeyDownPage);

    K(KeyNumpad0);
    K(KeyNumpad1);
    K(KeyNumpad2);
    K(KeyNumpad3);
    K(KeyNumpad4);
    K(KeyNumpad5);
    K(KeyNumpad6);
    K(KeyNumpad7);
    K(KeyNumpad8);
    K(KeyNumpad9);
    K(KeyNumpadPeriod);
    K(KeyNumpadEnter);
    K(KeyNumpadPlus);
    K(KeyNumpadMinus);
    K(KeyNumpadAsterisk);
    K(KeyNumpadSlash);

    K(KeyF1);
    K(KeyF2);
    K(KeyF3);
    K(KeyF4);
    K(KeyF5);
    K(KeyF6);
    K(KeyF7);
    K(KeyF8);
    K(KeyF9);
    K(KeyF10);
    K(KeyF11);
    K(KeyF12);
    K(KeyF13);
    K(KeyF14);
    K(KeyF15);
    K(KeyF16);
    K(KeyF17);
    K(KeyF18);
    K(KeyF19);
    K(KeyF20);
    K(KeyF21);
    K(KeyF22);
    K(KeyF23);
    K(KeyF24);

    default:
      return "KeyUnknown";
  }
#undef K
}

void event_to_buf(GHOST_EventHandle evt, char buf[128])
{
  GHOST_TEventType type = GHOST_GetEventType(evt);
  double time = (double)((int64_t)GHOST_GetEventTime(evt)) / 1000;
  GHOST_WindowHandle win = GHOST_GetEventWindow(evt);
  void *data = GHOST_GetEventData(evt);
  char *pos = buf;

  pos += sprintf(pos, "event: %6.2f, %16s", time, eventtype_to_string(type));
  if (win) {
    char *s = GHOST_GetTitle(win);
    pos += sprintf(pos, " - win: %s", s);
    free(s);
  }
  else {
    pos += sprintf(pos, " - sys evt");
  }
  switch (type) {
    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cd = data;
      pos += sprintf(pos, " - pos: (%d, %d)", cd->x, cd->y);
      break;
    }
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *bd = data;
      pos += sprintf(pos, " - but: %d", bd->button);
      break;
    }

    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *kd = data;
      pos += sprintf(pos, " - key: %s (%d)", keytype_to_string(kd->key), kd->key);
      /* TODO: ideally this would print the unicode character. */
      if (kd->utf8_buf[0]) {
        pos += sprintf(pos, " ascii: '%c' (%d)", kd->utf8_buf[0], kd->utf8_buf[0]);
      }
      break;
    }
  }
}
