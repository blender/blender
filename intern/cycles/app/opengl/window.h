/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Functions to display a simple OpenGL window using SDL, simplified to the
 * bare minimum we need to reduce boilerplate code in tests apps. */

CCL_NAMESPACE_BEGIN

typedef void (*WindowInitFunc)();
typedef void (*WindowExitFunc)();
typedef void (*WindowResizeFunc)(int width, int height);
typedef void (*WindowDisplayFunc)();
typedef void (*WindowKeyboardFunc)(unsigned char key);
typedef void (*WindowMotionFunc)(int x, int y, int button);

void window_main_loop(const char *title,
                      int width,
                      int height,
                      WindowInitFunc initf,
                      WindowExitFunc exitf,
                      WindowResizeFunc resize,
                      WindowDisplayFunc display,
                      WindowKeyboardFunc keyboard,
                      WindowMotionFunc motion);

void window_display_info(const char *info);
void window_display_help();
void window_redraw();

bool window_opengl_context_enable();
void window_opengl_context_disable();

CCL_NAMESPACE_END
