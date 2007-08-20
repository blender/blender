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

#ifndef BIF_LANGUAGE_H
#define BIF_LANGUAGE_H

#include "DNA_vec_types.h"

struct BMF_Font;

int  read_languagefile(void);		/* usiblender.c */
void free_languagemenu(void);		/* usiblender.c */

void set_interface_font(char *str); /* headerbuttons.c */
void start_interface_font(void);	/* headerbuttons.c */
void lang_setlanguage(void);		/* usiblender.c */

char *language_pup(void);
char *fontsize_pup(void);

int BIF_DrawString(struct BMF_Font* font, char *str, int translate);
float BIF_GetStringWidth(struct BMF_Font* font, char *str, int translate);
void BIF_GetBoundingBox(struct BMF_Font* font, char* str, int translate, rctf* bbox);

void BIF_RasterPos(float x, float y);
void BIF_SetScale(float aspect);
void refresh_interface_font(void);

struct LANGMenuEntry {
	struct LANGMenuEntry *next;
	char *line;
	char *language;
	char *code;
	int id;
};

struct LANGMenuEntry *find_language(short langid);

#endif /* BIF_LANGUAGE_H */

