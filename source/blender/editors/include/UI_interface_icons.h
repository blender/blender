/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 * 
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef UI_INTERFACE_ICONS_H
#define UI_INTERFACE_ICONS_H

struct Image;
struct ImBuf;
struct World;
struct Tex;
struct Lamp;
struct Material;

typedef struct IconFile {
	struct IconFile *next, *prev;
	char filename[80];	// FILE_MAXFILE size
	int index;
} IconFile;

#define ICON_DEFAULT_HEIGHT 16
#define ICON_DEFAULT_WIDTH	16
#define PREVIEW_DEFAULT_HEIGHT 96

/*
 Resizable Icons for Blender
*/
void UI_icons_init(int first_dyn_id);
int UI_icon_get_width(int icon_id);
int UI_icon_get_height(int icon_id);

void UI_icon_draw(float x, float y, int icon_id);
void UI_icon_draw_preview(float x, float y, int icon_id, int nocreate);

void UI_icon_draw_aspect(float x, float y, int icon_id, float aspect, float alpha);
void UI_icon_draw_aspect_color(float x, float y, int icon_id, float aspect, float *rgb);
void UI_icon_draw_size(float x, float y, int size, int icon_id, float alpha);
void UI_icons_free();
void UI_icons_free_drawinfo(void *drawinfo);

struct ListBase *UI_iconfile_list(void);
int UI_iconfile_get_index(char *filename);


#endif /*  UI_INTERFACE_ICONS_H */
