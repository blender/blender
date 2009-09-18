/**
 * blenlib/BKE_vfont.h (mar-2001 nzc)
 *	
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
#ifndef BKE_VFONT_H
#define BKE_VFONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <wchar.h>

struct VFont;
struct Scene;
struct Object;
struct Curve;
struct objfnt;
struct TmpFont;
struct CharInfo;

struct chartrans {
	float xof, yof;
	float rot;
	short linenr,charnr;
	char dobreak;
};

typedef struct SelBox {
	float x, y, w, h;
} SelBox;

typedef struct EditFont {	
	wchar_t *copybuf;
	wchar_t *copybufinfo;
	
	wchar_t *textbuf;
	struct CharInfo *textbufinfo;
	wchar_t *oldstr;
	struct CharInfo *oldstrinfo;
	
	float textcurs[4][2];
	
} EditFont;


void BKE_font_register_builtin(void *mem, int size);

void free_vfont(struct VFont *sc); 
void free_ttfont(void);
struct VFont *get_builtin_font(void);
struct VFont *load_vfont(char *name);
struct TmpFont *vfont_find_tmpfont(struct VFont *vfont);

struct chartrans *BKE_text_to_curve(struct Scene *scene, struct Object *ob, int mode);

int BKE_font_getselection(struct Object *ob, int *start, int *end);

void chtoutf8(unsigned long c, char *o);
void wcs2utf8s(char *dst, wchar_t *src);
int wcsleninu8(wchar_t *src);
int utf8towchar(wchar_t *w, char *c);

#ifdef __cplusplus
}
#endif

#endif

