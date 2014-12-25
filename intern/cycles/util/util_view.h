/*
 * Copyright 2011-2013 Blender Foundation
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

#ifndef __UTIL_VIEW_H__
#define __UTIL_VIEW_H__

/* Functions to display a simple OpenGL window using GLUT, simplified to the
 * bare minimum we need to reduce boilerplate code in tests apps. */

CCL_NAMESPACE_BEGIN

typedef void (*ViewInitFunc)(void);
typedef void (*ViewExitFunc)(void);
typedef void (*ViewResizeFunc)(int width, int height);
typedef void (*ViewDisplayFunc)(void);
typedef void (*ViewKeyboardFunc)(unsigned char key);
typedef void (*ViewMotionFunc)(int x, int y, int button);

void view_main_loop(const char *title, int width, int height,
	ViewInitFunc initf, ViewExitFunc exitf,
	ViewResizeFunc resize, ViewDisplayFunc display,
	ViewKeyboardFunc keyboard, ViewMotionFunc motion);

void view_display_info(const char *info);
void view_display_help();
void view_redraw();

CCL_NAMESPACE_END

#endif /*__UTIL_VIEW_H__*/

