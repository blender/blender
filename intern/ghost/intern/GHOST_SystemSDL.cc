/* SPDX-FileCopyrightText: 2011-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <cassert>
#include <stdexcept>

#include "GHOST_ContextSDL.hh"
#include "GHOST_SystemSDL.hh"
#include "GHOST_WindowSDL.hh"

#include "GHOST_WindowManager.hh"

#include "GHOST_EventButton.hh"
#include "GHOST_EventCursor.hh"
#include "GHOST_EventKey.hh"
#include "GHOST_EventWheel.hh"

GHOST_SystemSDL::GHOST_SystemSDL() : GHOST_System()
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    throw std::runtime_error(SDL_GetError());
  }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
}

GHOST_SystemSDL::~GHOST_SystemSDL()
{
  SDL_Quit();
}

GHOST_IWindow *GHOST_SystemSDL::createWindow(const char *title,
                                             int32_t left,
                                             int32_t top,
                                             uint32_t width,
                                             uint32_t height,
                                             GHOST_TWindowState state,
                                             GHOST_GPUSettings gpu_settings,
                                             const bool exclusive,
                                             const bool /*is_dialog*/,
                                             const GHOST_IWindow *parent_window)
{
  GHOST_WindowSDL *window = nullptr;

  const GHOST_ContextParams context_params = GHOST_CONTEXT_PARAMS_FROM_GPU_SETTINGS(gpu_settings);

  window = new GHOST_WindowSDL(this,
                               title,
                               left,
                               top,
                               width,
                               height,
                               state,
                               gpu_settings.context_type,
                               context_params,
                               exclusive,
                               parent_window);

  if (window) {
    if (GHOST_kWindowStateFullScreen == state) {
      SDL_Window *sdl_win = window->getSDLWindow();
      SDL_DisplayMode mode;

      memset(&mode, 0, sizeof(mode));

      SDL_SetWindowDisplayMode(sdl_win, &mode);
      SDL_ShowWindow(sdl_win);
      SDL_SetWindowFullscreen(sdl_win, SDL_TRUE);
    }

    if (window->getValid()) {
      window_manager_->addWindow(window);
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
    }
    else {
      delete window;
      window = nullptr;
    }
  }
  return window;
}

GHOST_TSuccess GHOST_SystemSDL::init()
{
  GHOST_TSuccess success = GHOST_System::init();

  if (success) {
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

/**
 * Returns the dimensions of the main display on this system.
 * \return The dimension of the main display.
 */
void GHOST_SystemSDL::getAllDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  SDL_DisplayMode mode;
  const int display_index = 0; /* NOTE: always 0 display. */
  if (SDL_GetDesktopDisplayMode(display_index, &mode) < 0) {
    return;
  }
  width = mode.w;
  height = mode.h;
}

void GHOST_SystemSDL::getMainDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  SDL_DisplayMode mode;
  const int display_index = 0; /* NOTE: always 0 display. */
  if (SDL_GetCurrentDisplayMode(display_index, &mode) < 0) {
    return;
  }
  width = mode.w;
  height = mode.h;
}

uint8_t GHOST_SystemSDL::getNumDisplays() const
{
  return SDL_GetNumVideoDisplays();
}

GHOST_IContext *GHOST_SystemSDL::createOffscreenContext(GHOST_GPUSettings gpu_settings)
{
  const GHOST_ContextParams context_params_offscreen =
      GHOST_CONTEXT_PARAMS_FROM_GPU_SETTINGS_OFFSCREEN(gpu_settings);

  switch (gpu_settings.context_type) {
#ifdef WITH_OPENGL_BACKEND
    case GHOST_kDrawingContextTypeOpenGL: {
      for (int minor = 6; minor >= 3; --minor) {
        GHOST_Context *context = new GHOST_ContextSDL(
            context_params_offscreen,
            nullptr,
            0, /* Profile bit. */
            4,
            minor,
            GHOST_OPENGL_SDL_CONTEXT_FLAGS,
            GHOST_OPENGL_SDL_RESET_NOTIFICATION_STRATEGY);

        if (context->initializeDrawingContext()) {
          return context;
        }
        delete context;
      }
      return nullptr;
    }
#endif

    default:
      /* Unsupported backend. */
      return nullptr;
  }
}

GHOST_TSuccess GHOST_SystemSDL::disposeContext(GHOST_IContext *context)
{
  delete context;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemSDL::getModifierKeys(GHOST_ModifierKeys &keys) const
{
  SDL_Keymod mod = SDL_GetModState();

  keys.set(GHOST_kModifierKeyLeftShift, (mod & KMOD_LSHIFT) != 0);
  keys.set(GHOST_kModifierKeyRightShift, (mod & KMOD_RSHIFT) != 0);
  keys.set(GHOST_kModifierKeyLeftControl, (mod & KMOD_LCTRL) != 0);
  keys.set(GHOST_kModifierKeyRightControl, (mod & KMOD_RCTRL) != 0);
  keys.set(GHOST_kModifierKeyLeftAlt, (mod & KMOD_LALT) != 0);
  keys.set(GHOST_kModifierKeyRightAlt, (mod & KMOD_RALT) != 0);
  keys.set(GHOST_kModifierKeyLeftOS, (mod & KMOD_LGUI) != 0);
  keys.set(GHOST_kModifierKeyRightOS, (mod & KMOD_RGUI) != 0);

  return GHOST_kSuccess;
}

#define GXMAP(k, x, y) \
  case x: \
    k = y; \
    break

static GHOST_TKey convertSDLKey(SDL_Scancode key)
{
  GHOST_TKey type;

  if ((key >= SDL_SCANCODE_A) && (key <= SDL_SCANCODE_Z)) {
    type = GHOST_TKey(key - SDL_SCANCODE_A + int(GHOST_kKeyA));
  }
  else if ((key >= SDL_SCANCODE_1) && (key <= SDL_SCANCODE_0)) {
    type = (key == SDL_SCANCODE_0) ? GHOST_kKey0 :
                                     GHOST_TKey(key - SDL_SCANCODE_1 + int(GHOST_kKey1));
  }
  else if ((key >= SDL_SCANCODE_F1) && (key <= SDL_SCANCODE_F12)) {
    type = GHOST_TKey(key - SDL_SCANCODE_F1 + int(GHOST_kKeyF1));
  }
  else if ((key >= SDL_SCANCODE_F13) && (key <= SDL_SCANCODE_F24)) {
    type = GHOST_TKey(key - SDL_SCANCODE_F13 + int(GHOST_kKeyF13));
  }
  else {
    switch (key) {
      GXMAP(type, SDL_SCANCODE_BACKSPACE, GHOST_kKeyBackSpace);
      GXMAP(type, SDL_SCANCODE_TAB, GHOST_kKeyTab);
      GXMAP(type, SDL_SCANCODE_RETURN, GHOST_kKeyEnter);
      GXMAP(type, SDL_SCANCODE_ESCAPE, GHOST_kKeyEsc);
      GXMAP(type, SDL_SCANCODE_SPACE, GHOST_kKeySpace);

      GXMAP(type, SDL_SCANCODE_SEMICOLON, GHOST_kKeySemicolon);
      GXMAP(type, SDL_SCANCODE_PERIOD, GHOST_kKeyPeriod);
      GXMAP(type, SDL_SCANCODE_COMMA, GHOST_kKeyComma);
      GXMAP(type, SDL_SCANCODE_APOSTROPHE, GHOST_kKeyQuote);
      GXMAP(type, SDL_SCANCODE_GRAVE, GHOST_kKeyAccentGrave);
      GXMAP(type, SDL_SCANCODE_MINUS, GHOST_kKeyMinus);
      GXMAP(type, SDL_SCANCODE_EQUALS, GHOST_kKeyEqual);

      GXMAP(type, SDL_SCANCODE_SLASH, GHOST_kKeySlash);
      GXMAP(type, SDL_SCANCODE_BACKSLASH, GHOST_kKeyBackslash);
      GXMAP(type, SDL_SCANCODE_KP_EQUALS, GHOST_kKeyEqual);
      GXMAP(type, SDL_SCANCODE_LEFTBRACKET, GHOST_kKeyLeftBracket);
      GXMAP(type, SDL_SCANCODE_RIGHTBRACKET, GHOST_kKeyRightBracket);
      GXMAP(type, SDL_SCANCODE_PAUSE, GHOST_kKeyPause);

      GXMAP(type, SDL_SCANCODE_LSHIFT, GHOST_kKeyLeftShift);
      GXMAP(type, SDL_SCANCODE_RSHIFT, GHOST_kKeyRightShift);
      GXMAP(type, SDL_SCANCODE_LCTRL, GHOST_kKeyLeftControl);
      GXMAP(type, SDL_SCANCODE_RCTRL, GHOST_kKeyRightControl);
      GXMAP(type, SDL_SCANCODE_LALT, GHOST_kKeyLeftAlt);
      GXMAP(type, SDL_SCANCODE_RALT, GHOST_kKeyRightAlt);
      GXMAP(type, SDL_SCANCODE_LGUI, GHOST_kKeyLeftOS);
      GXMAP(type, SDL_SCANCODE_RGUI, GHOST_kKeyRightOS);
      GXMAP(type, SDL_SCANCODE_APPLICATION, GHOST_kKeyApp);

      GXMAP(type, SDL_SCANCODE_INSERT, GHOST_kKeyInsert);
      GXMAP(type, SDL_SCANCODE_DELETE, GHOST_kKeyDelete);
      GXMAP(type, SDL_SCANCODE_HOME, GHOST_kKeyHome);
      GXMAP(type, SDL_SCANCODE_END, GHOST_kKeyEnd);
      GXMAP(type, SDL_SCANCODE_PAGEUP, GHOST_kKeyUpPage);
      GXMAP(type, SDL_SCANCODE_PAGEDOWN, GHOST_kKeyDownPage);

      GXMAP(type, SDL_SCANCODE_LEFT, GHOST_kKeyLeftArrow);
      GXMAP(type, SDL_SCANCODE_RIGHT, GHOST_kKeyRightArrow);
      GXMAP(type, SDL_SCANCODE_UP, GHOST_kKeyUpArrow);
      GXMAP(type, SDL_SCANCODE_DOWN, GHOST_kKeyDownArrow);

      GXMAP(type, SDL_SCANCODE_CAPSLOCK, GHOST_kKeyCapsLock);
      GXMAP(type, SDL_SCANCODE_SCROLLLOCK, GHOST_kKeyScrollLock);
      GXMAP(type, SDL_SCANCODE_NUMLOCKCLEAR, GHOST_kKeyNumLock);
      GXMAP(type, SDL_SCANCODE_PRINTSCREEN, GHOST_kKeyPrintScreen);

      /* keypad events */

      /* NOTE: SDL defines a bunch of key-pad identifiers that aren't supported by GHOST,
       * such as #SDL_SCANCODE_KP_PERCENT, #SDL_SCANCODE_KP_XOR. */
      GXMAP(type, SDL_SCANCODE_KP_0, GHOST_kKeyNumpad0);
      GXMAP(type, SDL_SCANCODE_KP_1, GHOST_kKeyNumpad1);
      GXMAP(type, SDL_SCANCODE_KP_2, GHOST_kKeyNumpad2);
      GXMAP(type, SDL_SCANCODE_KP_3, GHOST_kKeyNumpad3);
      GXMAP(type, SDL_SCANCODE_KP_4, GHOST_kKeyNumpad4);
      GXMAP(type, SDL_SCANCODE_KP_5, GHOST_kKeyNumpad5);
      GXMAP(type, SDL_SCANCODE_KP_6, GHOST_kKeyNumpad6);
      GXMAP(type, SDL_SCANCODE_KP_7, GHOST_kKeyNumpad7);
      GXMAP(type, SDL_SCANCODE_KP_8, GHOST_kKeyNumpad8);
      GXMAP(type, SDL_SCANCODE_KP_9, GHOST_kKeyNumpad9);
      GXMAP(type, SDL_SCANCODE_KP_PERIOD, GHOST_kKeyNumpadPeriod);

      GXMAP(type, SDL_SCANCODE_KP_ENTER, GHOST_kKeyNumpadEnter);
      GXMAP(type, SDL_SCANCODE_KP_PLUS, GHOST_kKeyNumpadPlus);
      GXMAP(type, SDL_SCANCODE_KP_MINUS, GHOST_kKeyNumpadMinus);
      GXMAP(type, SDL_SCANCODE_KP_MULTIPLY, GHOST_kKeyNumpadAsterisk);
      GXMAP(type, SDL_SCANCODE_KP_DIVIDE, GHOST_kKeyNumpadSlash);

      /* Media keys in some keyboards and laptops with XFree86/XORG. */
      GXMAP(type, SDL_SCANCODE_AUDIOPLAY, GHOST_kKeyMediaPlay);
      GXMAP(type, SDL_SCANCODE_AUDIOSTOP, GHOST_kKeyMediaStop);
      GXMAP(type, SDL_SCANCODE_AUDIOPREV, GHOST_kKeyMediaFirst);
      // GXMAP(type, XF86XK_AudioRewind, GHOST_kKeyMediaFirst);
      GXMAP(type, SDL_SCANCODE_AUDIONEXT, GHOST_kKeyMediaLast);

      /* International Keys. */

      /* This key has multiple purposes,
       * however the only GHOST key that uses the scan-code is GrLess. */
      GXMAP(type, SDL_SCANCODE_NONUSBACKSLASH, GHOST_kKeyGrLess);

      default:
        printf("Unknown\n");
        type = GHOST_kKeyUnknown;
        break;
    }
  }

  return type;
}
#undef GXMAP

static char convert_keyboard_event_to_ascii(const SDL_KeyboardEvent &sdl_sub_evt)
{
  SDL_Keycode sym = sdl_sub_evt.keysym.sym;
  if (sym > 127) {
    switch (sym) {
      case SDLK_KP_DIVIDE:
        sym = '/';
        break;
      case SDLK_KP_MULTIPLY:
        sym = '*';
        break;
      case SDLK_KP_MINUS:
        sym = '-';
        break;
      case SDLK_KP_PLUS:
        sym = '+';
        break;
      case SDLK_KP_1:
        sym = '1';
        break;
      case SDLK_KP_2:
        sym = '2';
        break;
      case SDLK_KP_3:
        sym = '3';
        break;
      case SDLK_KP_4:
        sym = '4';
        break;
      case SDLK_KP_5:
        sym = '5';
        break;
      case SDLK_KP_6:
        sym = '6';
        break;
      case SDLK_KP_7:
        sym = '7';
        break;
      case SDLK_KP_8:
        sym = '8';
        break;
      case SDLK_KP_9:
        sym = '9';
        break;
      case SDLK_KP_0:
        sym = '0';
        break;
      case SDLK_KP_PERIOD:
        sym = '.';
        break;
      default:
        sym = 0;
        break;
    }
  }
  else {
    if (sdl_sub_evt.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) {
      /* Weak US keyboard assumptions. */
      if (sym >= 'a' && sym <= ('a' + 32)) {
        sym -= 32;
      }
      else {
        switch (sym) {
          case '`':
            sym = '~';
            break;
          case '1':
            sym = '!';
            break;
          case '2':
            sym = '@';
            break;
          case '3':
            sym = '#';
            break;
          case '4':
            sym = '$';
            break;
          case '5':
            sym = '%';
            break;
          case '6':
            sym = '^';
            break;
          case '7':
            sym = '&';
            break;
          case '8':
            sym = '*';
            break;
          case '9':
            sym = '(';
            break;
          case '0':
            sym = ')';
            break;
          case '-':
            sym = '_';
            break;
          case '=':
            sym = '+';
            break;
          case '[':
            sym = '{';
            break;
          case ']':
            sym = '}';
            break;
          case '\\':
            sym = '|';
            break;
          case ';':
            sym = ':';
            break;
          case '\'':
            sym = '"';
            break;
          case ',':
            sym = '<';
            break;
          case '.':
            sym = '>';
            break;
          case '/':
            sym = '?';
            break;
          default:
            break;
        }
      }
    }
  }
  return char(sym);
}

/**
 * Events don't always have valid windows,
 * but GHOST needs a window _always_. Fall back to the GL window.
 */
static SDL_Window *SDL_GetWindowFromID_fallback(Uint32 id)
{
  SDL_Window *sdl_win = SDL_GetWindowFromID(id);
  if (sdl_win == nullptr) {
    sdl_win = SDL_GL_GetCurrentWindow();
  }
  return sdl_win;
}

void GHOST_SystemSDL::processEvent(SDL_Event *sdl_event)
{
  GHOST_Event *g_event = nullptr;

  switch (sdl_event->type) {
    case SDL_WINDOWEVENT: {
      const SDL_WindowEvent &sdl_sub_evt = sdl_event->window;
      const uint64_t event_ms = sdl_sub_evt.timestamp;
      GHOST_WindowSDL *window = findGhostWindow(
          SDL_GetWindowFromID_fallback(sdl_sub_evt.windowID));
      /* Can be nullptr on close window. */
#if 0
      assert(window != nullptr);
#endif

      switch (sdl_sub_evt.event) {
        case SDL_WINDOWEVENT_EXPOSED:
          g_event = new GHOST_Event(event_ms, GHOST_kEventWindowUpdate, window);
          break;
        case SDL_WINDOWEVENT_RESIZED:
          g_event = new GHOST_Event(event_ms, GHOST_kEventWindowSize, window);
          break;
        case SDL_WINDOWEVENT_MOVED:
          g_event = new GHOST_Event(event_ms, GHOST_kEventWindowMove, window);
          break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
          g_event = new GHOST_Event(event_ms, GHOST_kEventWindowActivate, window);
          break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
          g_event = new GHOST_Event(event_ms, GHOST_kEventWindowDeactivate, window);
          break;
        case SDL_WINDOWEVENT_CLOSE:
          g_event = new GHOST_Event(event_ms, GHOST_kEventWindowClose, window);
          break;
      }

      break;
    }

    case SDL_QUIT: {
      const SDL_QuitEvent &sdl_sub_evt = sdl_event->quit;
      const uint64_t event_ms = sdl_sub_evt.timestamp;
      GHOST_IWindow *window = window_manager_->getActiveWindow();
      g_event = new GHOST_Event(event_ms, GHOST_kEventQuitRequest, window);
      break;
    }

    case SDL_MOUSEMOTION: {
      const SDL_MouseMotionEvent &sdl_sub_evt = sdl_event->motion;
      const uint64_t event_ms = sdl_sub_evt.timestamp;
      SDL_Window *sdl_win = SDL_GetWindowFromID_fallback(sdl_sub_evt.windowID);
      GHOST_WindowSDL *window = findGhostWindow(sdl_win);
      assert(window != nullptr);

      int x_win, y_win;
      SDL_GetWindowPosition(sdl_win, &x_win, &y_win);

      int32_t x_root = sdl_sub_evt.x + x_win;
      int32_t y_root = sdl_sub_evt.y + y_win;

#if 0
      if (window->getCursorGrabMode() != GHOST_kGrabDisable &&
          window->getCursorGrabMode() != GHOST_kGrabNormal)
      {
        int32_t x_new = x_root;
        int32_t y_new = y_root;
        int32_t x_accum, y_accum;
        GHOST_Rect bounds;

        /* fallback to window bounds */
        if (window->getCursorGrabBounds(bounds) == GHOST_kFailure) {
          window->getClientBounds(bounds);
        }

        /* Could also clamp to screen bounds wrap with a window outside the view will
         * fail at the moment. Use offset of 8 in case the window is at screen bounds. */
        bounds.wrapPoint(x_new, y_new, 8, window->getCursorGrabAxis());
        window->getCursorGrabAccum(x_accum, y_accum);

        /* Can't use #setCursorPosition because the mouse may have no focus! */
        if (x_new != x_root || y_new != y_root) {
          if (1 /* `xme.time > last_warp_` */) {
            /* when wrapping we don't need to add an event because the
             * #setCursorPosition call will cause a new event after */
            SDL_WarpMouseInWindow(sdl_win, x_new - x_win, y_new - y_win); /* wrap */
            window->setCursorGrabAccum(x_accum + (x_root - x_new), y_accum + (y_root - y_new));
            // last_warp_ = lastEventTime(xme.time);
          }
          else {
            // setCursorPosition(x_new, y_new); /* wrap but don't accumulate */
            SDL_WarpMouseInWindow(sdl_win, x_new - x_win, y_new - y_win);
          }

          g_event = new GHOST_EventCursor(
              event_ms, GHOST_kEventCursorMove, window, x_new, y_new, GHOST_TABLET_DATA_NONE);
        }
        else {
          g_event = new GHOST_EventCursor(event_ms,
                                          GHOST_kEventCursorMove,
                                          window,
                                          x_root + x_accum,
                                          y_root + y_accum,
                                          GHOST_TABLET_DATA_NONE);
        }
      }
      else
#endif
      {
        g_event = new GHOST_EventCursor(
            event_ms, GHOST_kEventCursorMove, window, x_root, y_root, GHOST_TABLET_DATA_NONE);
      }
      break;
    }
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEBUTTONDOWN: {
      const SDL_MouseButtonEvent &sdl_sub_evt = sdl_event->button;
      const uint64_t event_ms = sdl_sub_evt.timestamp;
      GHOST_TButton gbmask = GHOST_kButtonMaskLeft;
      GHOST_TEventType type = (sdl_sub_evt.state == SDL_PRESSED) ? GHOST_kEventButtonDown :
                                                                   GHOST_kEventButtonUp;

      GHOST_WindowSDL *window = findGhostWindow(
          SDL_GetWindowFromID_fallback(sdl_sub_evt.windowID));
      assert(window != nullptr);

      /* process rest of normal mouse buttons */
      if (sdl_sub_evt.button == SDL_BUTTON_LEFT) {
        gbmask = GHOST_kButtonMaskLeft;
      }
      else if (sdl_sub_evt.button == SDL_BUTTON_MIDDLE) {
        gbmask = GHOST_kButtonMaskMiddle;
      }
      else if (sdl_sub_evt.button == SDL_BUTTON_RIGHT) {
        gbmask = GHOST_kButtonMaskRight;
        /* these buttons are untested! */
      }
      else if (sdl_sub_evt.button == SDL_BUTTON_X1) {
        gbmask = GHOST_kButtonMaskButton4;
      }
      else if (sdl_sub_evt.button == SDL_BUTTON_X2) {
        gbmask = GHOST_kButtonMaskButton5;
      }
      else {
        break;
      }

      g_event = new GHOST_EventButton(event_ms, type, window, gbmask, GHOST_TABLET_DATA_NONE);
      break;
    }
    case SDL_MOUSEWHEEL: {
      const SDL_MouseWheelEvent &sdl_sub_evt = sdl_event->wheel;
      const uint64_t event_ms = sdl_sub_evt.timestamp;
      GHOST_WindowSDL *window = findGhostWindow(
          SDL_GetWindowFromID_fallback(sdl_sub_evt.windowID));
      assert(window != nullptr);
      if (sdl_sub_evt.x != 0) {
        g_event = new GHOST_EventWheel(
            event_ms, window, GHOST_kEventWheelAxisHorizontal, sdl_sub_evt.x);
      }
      else if (sdl_sub_evt.y != 0) {
        g_event = new GHOST_EventWheel(
            event_ms, window, GHOST_kEventWheelAxisVertical, sdl_sub_evt.y);
      }
      break;
    }
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      const SDL_KeyboardEvent &sdl_sub_evt = sdl_event->key;
      const uint64_t event_ms = sdl_sub_evt.timestamp;
      GHOST_TEventType type = (sdl_sub_evt.state == SDL_PRESSED) ? GHOST_kEventKeyDown :
                                                                   GHOST_kEventKeyUp;
      const bool is_repeat = sdl_sub_evt.repeat != 0;

      GHOST_WindowSDL *window = findGhostWindow(
          SDL_GetWindowFromID_fallback(sdl_sub_evt.windowID));
      assert(window != nullptr);

      GHOST_TKey gkey = convertSDLKey(sdl_sub_evt.keysym.scancode);
      /* NOTE: the `sdl_sub_evt.keysym.sym` is truncated,
       * for unicode support ghost has to be modified. */

      /* TODO(@ideasman42): support full unicode, SDL supports this but it needs to be
       * explicitly enabled via #SDL_StartTextInput which GHOST would have to wrap. */
      char utf8_buf[sizeof(GHOST_TEventKeyData::utf8_buf)] = {'\0'};
      if (type == GHOST_kEventKeyDown) {
        utf8_buf[0] = convert_keyboard_event_to_ascii(sdl_sub_evt);
      }

      g_event = new GHOST_EventKey(event_ms, type, window, gkey, is_repeat, utf8_buf);
      break;
    }
  }

  if (g_event) {
    pushEvent(g_event);
  }
}

GHOST_TSuccess GHOST_SystemSDL::getCursorPosition(int32_t &x, int32_t &y) const
{
  int x_win, y_win;
  SDL_Window *win = SDL_GetMouseFocus();
  SDL_GetWindowPosition(win, &x_win, &y_win);

  int xi, yi;
  SDL_GetMouseState(&xi, &yi);
  x = xi + x_win;
  y = yi + x_win;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemSDL::setCursorPosition(int32_t x, int32_t y)
{
  int x_win, y_win;
  SDL_Window *win = SDL_GetMouseFocus();
  SDL_GetWindowPosition(win, &x_win, &y_win);

  SDL_WarpMouseInWindow(win, x - x_win, y - y_win);
  return GHOST_kSuccess;
}

bool GHOST_SystemSDL::generateWindowExposeEvents()
{
  std::vector<GHOST_WindowSDL *>::iterator w_start = dirty_windows_.begin();
  std::vector<GHOST_WindowSDL *>::const_iterator w_end = dirty_windows_.end();
  bool anyProcessed = false;

  for (; w_start != w_end; ++w_start) {
    /* The caller doesn't have a time-stamp. */
    const uint64_t event_ms = getMilliSeconds();
    GHOST_Event *g_event = new GHOST_Event(event_ms, GHOST_kEventWindowUpdate, *w_start);

    (*w_start)->validate();

    if (g_event) {
      // printf("Expose events pushed\n");
      pushEvent(g_event);
      anyProcessed = true;
    }
  }

  dirty_windows_.clear();
  return anyProcessed;
}

bool GHOST_SystemSDL::processEvents(bool waitForEvent)
{
  /* Get all the current events - translate them into
   * ghost events and call base class #pushEvent() method. */

  bool anyProcessed = false;

  do {
    GHOST_TimerManager *timerMgr = getTimerManager();

    if (waitForEvent && dirty_windows_.empty() && !SDL_HasEvents(SDL_FIRSTEVENT, SDL_LASTEVENT)) {
      uint64_t next = timerMgr->nextFireTime();

      if (next == GHOST_kFireTimeNever) {
        SDL_WaitEventTimeout(nullptr, -1);
        // SleepTillEvent(display_, -1);
      }
      else {
        int64_t maxSleep = next - getMilliSeconds();

        if (maxSleep >= 0) {
          SDL_WaitEventTimeout(nullptr, next - getMilliSeconds());
          // SleepTillEvent(display_, next - getMilliSeconds()); /* X11. */
        }
      }
    }

    if (timerMgr->fireTimers(getMilliSeconds())) {
      anyProcessed = true;
    }

    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event)) {
      processEvent(&sdl_event);
      anyProcessed = true;
    }

    if (generateWindowExposeEvents()) {
      anyProcessed = true;
    }
  } while (waitForEvent && !anyProcessed);

  return anyProcessed;
}

GHOST_WindowSDL *GHOST_SystemSDL::findGhostWindow(SDL_Window *sdl_win)
{
  if (sdl_win == nullptr) {
    return nullptr;
  }
  /* It is not entirely safe to do this as the back-pointer may point
   * to a window that has recently been removed.
   * We should always check the window manager's list of windows
   * and only process events on these windows. */

  const std::vector<GHOST_IWindow *> &win_vec = window_manager_->getWindows();

  std::vector<GHOST_IWindow *>::const_iterator win_it = win_vec.begin();
  std::vector<GHOST_IWindow *>::const_iterator win_end = win_vec.end();

  for (; win_it != win_end; ++win_it) {
    GHOST_WindowSDL *window = static_cast<GHOST_WindowSDL *>(*win_it);
    if (window->getSDLWindow() == sdl_win) {
      return window;
    }
  }
  return nullptr;
}

void GHOST_SystemSDL::addDirtyWindow(GHOST_WindowSDL *bad_wind)
{
  GHOST_ASSERT((bad_wind != nullptr), "addDirtyWindow() nullptr ptr trapped (window)");

  dirty_windows_.push_back(bad_wind);
}

GHOST_TSuccess GHOST_SystemSDL::getButtons(GHOST_Buttons &buttons) const
{
  Uint8 state = SDL_GetMouseState(nullptr, nullptr);
  buttons.set(GHOST_kButtonMaskLeft, (state & SDL_BUTTON_LMASK) != 0);
  buttons.set(GHOST_kButtonMaskMiddle, (state & SDL_BUTTON_MMASK) != 0);
  buttons.set(GHOST_kButtonMaskRight, (state & SDL_BUTTON_RMASK) != 0);

  return GHOST_kSuccess;
}

GHOST_TCapabilityFlag GHOST_SystemSDL::getCapabilities() const
{
  return GHOST_TCapabilityFlag(
      GHOST_CAPABILITY_FLAG_ALL &
      /* NOTE: order the following flags as they they're declared in the source. */
      ~(
          /* This SDL back-end has not yet implemented primary clipboard. */
          GHOST_kCapabilityClipboardPrimary |
          /* This SDL back-end has not yet implemented image copy/paste. */
          GHOST_kCapabilityClipboardImage |
          /* This SDL back-end has not yet implemented color sampling the desktop. */
          GHOST_kCapabilityDesktopSample |
          /* No support yet for IME input methods. */
          GHOST_kCapabilityInputIME |
          /* No support for window decoration styles. */
          GHOST_kCapabilityWindowDecorationStyles |
          /* No support for precisely placing windows on multiple monitors. */
          GHOST_kCapabilityMultiMonitorPlacement |
          /* No support for a Hyper modifier key. */
          GHOST_kCapabilityKeyboardHyperKey |
          /* No support yet for RGBA mouse cursors. */
          GHOST_kCapabilityCursorRGBA |
          /* No support yet for dynamic cursor generation. */
          GHOST_kCapabilityCursorGenerator |
          /* No support for window path meta-data. */
          GHOST_kCapabilityWindowPath));
}

char *GHOST_SystemSDL::getClipboard(bool /*selection*/) const
{
  return (char *)SDL_GetClipboardText();
}

void GHOST_SystemSDL::putClipboard(const char *buffer, bool /*selection*/) const
{
  SDL_SetClipboardText(buffer);
}

uint64_t GHOST_SystemSDL::getMilliSeconds() const
{
  return SDL_GetTicks64();
}
