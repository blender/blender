/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BIF_PREVIEW_ICONS_H
#define BIF_PREVIEW_ICONS_H

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
#define PREVIEW_DEFAULT_HEIGHT 96

/*
 Resizable Icons for Blender
*/
void BIF_icons_init(int first_dyn_id);
int BIF_icon_get_width(int icon_id);
int BIF_icon_get_height(int icon_id);

void BIF_icon_draw(float x, float y, int icon_id);
void BIF_icon_draw_preview(float x, float y, int icon_id, int nocreate);

void BIF_icon_draw_aspect(float x, float y, int icon_id, float aspect);
void BIF_icon_draw_aspect_blended(float x, float y, int icon_id, float aspect, int shade);
void BIF_icons_free();
void BIF_icons_free_drawinfo(void *drawinfo);

struct ListBase *BIF_iconfile_list(void);
int BIF_iconfile_get_index(char *filename);


#endif /*  BIF_ICONS_H */
