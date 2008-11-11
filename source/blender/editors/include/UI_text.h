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

#ifndef UI_TEXT_H
#define UI_TEXT_H

struct BMF_Font;

int  read_languagefile(void);		/* usiblender.c */
void free_languagemenu(void);		/* usiblender.c */

void set_interface_font(char *str); /* headerbuttons.c */
void start_interface_font(void);	/* headerbuttons.c */
void lang_setlanguage(void);		/* usiblender.c */

char *language_pup(void);
char *fontsize_pup(void);

int UI_DrawString(struct BMF_Font* font, char *str, int translate);
float UI_GetStringWidth(struct BMF_Font* font, char *str, int translate);
void UI_GetBoundingBox(struct BMF_Font* font, char* str, int translate, rctf* bbox);

void UI_set_international(int international);
int UI_get_international(void);

void UI_RasterPos(float x, float y);
void UI_SetScale(float aspect);
void ui_text_init_userdef(void);

struct LANGMenuEntry {
	struct LANGMenuEntry *next;
	char *line;
	char *language;
	char *code;
	int id;
};

struct LANGMenuEntry *find_language(short langid);

#endif /* UI_TEXT_H */

