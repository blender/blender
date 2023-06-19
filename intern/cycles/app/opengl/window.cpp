/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

#include "app/opengl/window.h"

#include "util/string.h"
#include "util/thread.h"
#include "util/time.h"
#include "util/version.h"

#include <SDL.h>
#include <epoxy/gl.h>

CCL_NAMESPACE_BEGIN

/* structs */

struct Window {
  WindowInitFunc initf = nullptr;
  WindowExitFunc exitf = nullptr;
  WindowResizeFunc resize = nullptr;
  WindowDisplayFunc display = nullptr;
  WindowKeyboardFunc keyboard = nullptr;
  WindowMotionFunc motion = nullptr;

  bool first_display = true;
  bool redraw = false;

  int mouseX = 0, mouseY = 0;
  int mouseBut0 = 0, mouseBut2 = 0;

  int width = 0, height = 0;

  SDL_Window *window = nullptr;
  SDL_GLContext gl_context = nullptr;
  thread_mutex gl_context_mutex;
} V;

/* public */

static void window_display_text(int x, int y, const char *text)
{
/* Not currently supported, need to add text rendering support. */
#if 0
  const char *c;

  glRasterPos3f(x, y, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  printf("display %s\n", text);

  for (c = text; *c != '\0'; c++) {
    const uint8_t *bitmap = helvetica10_character_map[*c];
    glBitmap(bitmap[0],
             helvetica10_height,
             helvetica10_x_offset,
             helvetica10_y_offset,
             bitmap[0],
             0.0f,
             bitmap + 1);
  }
#else
  static string last_text = "";

  if (text != last_text) {
    printf("%s\n", text);
    last_text = text;
  }
#endif
}

void window_display_info(const char *info)
{
  const int height = 20;

#if 0
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(0.1f, 0.1f, 0.1f, 0.8f);
  glRectf(0.0f, V.height - height, V.width, V.height);
  glDisable(GL_BLEND);

  glColor3f(0.5f, 0.5f, 0.5f);
#endif

  window_display_text(10, 7 + V.height - height, info);

#if 0
  glColor3f(1.0f, 1.0f, 1.0f);
#endif
}

void window_display_help()
{
  const int w = (int)((float)V.width / 1.15f);
  const int h = (int)((float)V.height / 1.15f);

  const int x1 = (V.width - w) / 2;
#if 0
  const int x2 = x1 + w;
#endif

  const int y1 = (V.height - h) / 2;
  const int y2 = y1 + h;

#if 0
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(0.5f, 0.5f, 0.5f, 0.8f);
  glRectf(x1, y1, x2, y2);
  glDisable(GL_BLEND);

  glColor3f(0.8f, 0.8f, 0.8f);
#endif

  string info = string("Cycles Renderer ") + CYCLES_VERSION_STRING;

  window_display_text(x1 + 20, y2 - 20, info.c_str());
  window_display_text(x1 + 20, y2 - 40, "(C) 2011-2016 Blender Foundation");
  window_display_text(x1 + 20, y2 - 80, "Controls:");
  window_display_text(x1 + 20, y2 - 100, "h:  Info/Help");
  window_display_text(x1 + 20, y2 - 120, "r:  Reset");
  window_display_text(x1 + 20, y2 - 140, "p:  Pause");
  window_display_text(x1 + 20, y2 - 160, "esc:  Cancel");
  window_display_text(x1 + 20, y2 - 180, "q:  Quit program");

  window_display_text(x1 + 20, y2 - 210, "i:  Interactive mode");
  window_display_text(x1 + 20, y2 - 230, "Left mouse:  Move camera");
  window_display_text(x1 + 20, y2 - 250, "Right mouse:  Rotate camera");
  window_display_text(x1 + 20, y2 - 270, "W/A/S/D:  Move camera");
  window_display_text(x1 + 20, y2 - 290, "0/1/2/3:  Set max bounces");

#if 0
  glColor3f(1.0f, 1.0f, 1.0f);
#endif
}

static void window_display()
{
  if (V.first_display) {
    if (V.initf) {
      V.initf();
    }
    if (V.exitf) {
      atexit(V.exitf);
    }

    V.first_display = false;
  }

  window_opengl_context_enable();

  glViewport(0, 0, V.width, V.height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.05f, 0.05f, 0.05f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, V.width, 0, V.height, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glRasterPos3f(0, 0, 0);

  if (V.display)
    V.display();

  SDL_GL_SwapWindow(V.window);
  window_opengl_context_disable();
}

static void window_reshape(int width, int height)
{
  if (V.width != width || V.height != height) {
    if (V.resize) {
      V.resize(width, height);
    }
  }

  V.width = width;
  V.height = height;
}

static bool window_keyboard(unsigned char key)
{
  if (V.keyboard)
    V.keyboard(key);

  if (key == 'q') {
    if (V.exitf)
      V.exitf();
    return true;
  }

  return false;
}

static void window_mouse(int button, int state, int x, int y)
{
  if (button == SDL_BUTTON_LEFT) {
    if (state == SDL_MOUSEBUTTONDOWN) {
      V.mouseX = x;
      V.mouseY = y;
      V.mouseBut0 = 1;
    }
    else if (state == SDL_MOUSEBUTTONUP) {
      V.mouseBut0 = 0;
    }
  }
  else if (button == SDL_BUTTON_RIGHT) {
    if (state == SDL_MOUSEBUTTONDOWN) {
      V.mouseX = x;
      V.mouseY = y;
      V.mouseBut2 = 1;
    }
    else if (state == SDL_MOUSEBUTTONUP) {
      V.mouseBut2 = 0;
    }
  }
}

static void window_motion(int x, int y)
{
  const int but = V.mouseBut0 ? 0 : 2;
  const int distX = x - V.mouseX;
  const int distY = y - V.mouseY;

  if (V.motion)
    V.motion(distX, distY, but);

  V.mouseX = x;
  V.mouseY = y;
}

bool window_opengl_context_enable()
{
  V.gl_context_mutex.lock();
  SDL_GL_MakeCurrent(V.window, V.gl_context);
  return true;
}

void window_opengl_context_disable()
{
  SDL_GL_MakeCurrent(V.window, nullptr);
  V.gl_context_mutex.unlock();
}

void window_main_loop(const char *title,
                      int width,
                      int height,
                      WindowInitFunc initf,
                      WindowExitFunc exitf,
                      WindowResizeFunc resize,
                      WindowDisplayFunc display,
                      WindowKeyboardFunc keyboard,
                      WindowMotionFunc motion)
{
  V.width = width;
  V.height = height;
  V.first_display = true;
  V.redraw = false;
  V.initf = initf;
  V.exitf = exitf;
  V.resize = resize;
  V.display = display;
  V.keyboard = keyboard;
  V.motion = motion;

  SDL_Init(SDL_INIT_VIDEO);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
  V.window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              width,
                              height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  if (V.window == nullptr) {
    fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    return;
  }

  SDL_RaiseWindow(V.window);

  V.gl_context = SDL_GL_CreateContext(V.window);
  SDL_GL_MakeCurrent(V.window, nullptr);

  window_reshape(width, height);
  window_display();

  while (true) {
    bool quit = false;
    SDL_Event event;
    while (!quit && SDL_PollEvent(&event)) {
      if (event.type == SDL_TEXTINPUT) {
        quit = window_keyboard(event.text.text[0]);
      }
      else if (event.type == SDL_MOUSEMOTION) {
        window_motion(event.motion.x, event.motion.y);
      }
      else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        window_mouse(event.button.button, event.button.state, event.button.x, event.button.y);
      }
      else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
            event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
          window_reshape(event.window.data1, event.window.data2);
        }
      }
      else if (event.type == SDL_QUIT) {
        if (V.exitf) {
          V.exitf();
        }
        quit = true;
      }
    }

    if (quit) {
      break;
    }

    if (V.redraw) {
      V.redraw = false;
      window_display();
    }

    SDL_WaitEventTimeout(NULL, 100);
  }

  SDL_GL_DeleteContext(V.gl_context);
  SDL_DestroyWindow(V.window);
  SDL_Quit();
}

void window_redraw()
{
  V.redraw = true;
}

CCL_NAMESPACE_END
