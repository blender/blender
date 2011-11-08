/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

void view_main_loop(const char *title, int width, int height,
	ViewInitFunc initf, ViewExitFunc exitf,
	ViewResizeFunc resize, ViewDisplayFunc display,
	ViewKeyboardFunc keyboard);

void view_display_info(const char *info);
void view_redraw();

CCL_NAMESPACE_END

#endif /*__UTIL_VIEW_H__*/

