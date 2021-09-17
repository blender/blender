/*
 * Copyright 2011-2022 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
