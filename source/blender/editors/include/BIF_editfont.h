/**
 * $Id: BIF_editfont.h 5286 2005-09-15 17:32:24Z ton $
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

#include <wchar.h>

#ifndef BIF_EDITFONT_H
#define BIF_EDITFONT_H

struct Text;

extern char *BIF_lorem;
extern wchar_t *copybuf;
extern wchar_t *copybufinfo;

typedef struct unicodect
{
	char *name;
	char *longname;
	int   start;
	int   end;
} unicodect;

void do_textedit(unsigned short event, short val, unsigned long _ascii);
void make_editText(void);
void load_editText(void);
void remake_editText(void);
void free_editText(void);
void paste_editText(void);
void txt_export_to_object(struct Text *text);
void txt_export_to_objects(struct Text *text);
void undo_push_font(char *);
void load_3dtext_fs(char *);
void add_lorem(void);
void paste_unicodeText(char *filename);

/**
 * @attention The argument is discarded. It is there for
 * compatibility.
 */
void add_primitiveFont(int);
void to_upper(void);

#endif

