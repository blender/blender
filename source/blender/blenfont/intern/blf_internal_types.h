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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLF_INTERNAL_TYPES_H
#define BLF_INTERNAL_TYPES_H

typedef struct GlyphCacheBLF {
	struct GlyphCacheBLF *next;
	struct GlyphCacheBLF *prev;

	/* font size. */
	int size;

	/* and dpi. */
	int dpi;

	/* and the glyphs. */
	ListBase bucket[257];

	/* texture array, to draw the glyphs. */
	GLuint *textures;

	/* size of the array. */
	int ntex;

	/* and the last texture, aka. the current texture. */
	int cur_tex;

	/* like bftgl, we draw every glyph in a big texture, so this is the
	 * current position inside the texture.
	 */
	int x_offs;
	int y_offs;

	/* and the space from one to other. */
	unsigned int pad;

	/* and the bigger glyph in the font. */
	int max_glyph_width;
	int max_glyph_height;

	/* next two integer power of two, to build the texture. */
	int p2_width;
	int p2_height;

	/* number of glyphs in the font. */
	int num_glyphs;

	/* number of glyphs that we load here. */
	int rem_glyphs;

	/* ascender and descender value. */
	float ascender;
	float descender;
} GlyphCacheBLF;

typedef struct GlyphBLF {
	struct GlyphBLF *next;
	struct GlyphBLF *prev;

	/* and the character, as UTF8 */
	unsigned int c;

	/* texture id where this glyph is store. */
	GLuint tex;

	/* position inside the texture where this glyph is store. */
	int xoff;
	int yoff;

	/* glyph width and height. */
	int width;
	int height;

	/* glyph bounding box. */
	rctf box;

	/* uv coords. */
	float uv[2][2];

	/* advance value. */
	float advance;

	/* X and Y bearing of the glyph.
	 * The X bearing is from the origin to the glyph left bbox edge.
	 * The Y bearing is from the baseline to the top of the glyph edge.
	 */
	float pos_x;
	float pos_y;
} GlyphBLF;

typedef struct FontBLF {
	/* font name. */
	char *name;

	/* filename or NULL. */
	char *filename;

	/* font type, can be freetype2 or internal. */
	int type;

	/* reference count. */
	int ref;

	/* aspect ratio or scale. */
	float aspect;

	/* initial position for draw the text. */
	float pos[3];

	/* angle in degrees. */
	float angle;

	/* this is the matrix that we load before rotate/scale/translate. */
	float mat[4][4];

	/* clipping rectangle. */
	rctf clip_rec;

	/* font dpi (default 72). */
	int dpi;

	/* font size. */
	int size;

	/* max texture size. */
	int max_tex_size;

	/* font options. */
	int flags;

	/* list of glyph cache for this font. */
	ListBase cache;

	/* current glyph cache, size and dpi. */
	GlyphCacheBLF *glyph_cache;

	/* engine data. */
	void *engine;

	/* engine functions. */
	void (*size_set)(struct FontBLF *, int, int);
	void (*draw)(struct FontBLF *, char *);
	void (*boundbox_get)(struct FontBLF *, char *, rctf *);
	float (*width_get)(struct FontBLF *, char *);
	float (*height_get)(struct FontBLF *, char *);
	void (*free)(struct FontBLF *);
} FontBLF;

typedef struct DirBLF {
	struct DirBLF *next;
	struct DirBLF *prev;

	/* full path where search fonts. */
	char *path;
} DirBLF;

typedef struct LangBLF {
	struct LangBLF *next;
	struct LangBLF *prev;

	char *line;
	char *language;
	char *code;
	int id;
} LangBLF;

#define BLF_LANG_FIND_BY_LINE 0
#define BLF_LANG_FIND_BY_LANGUAGE 1
#define BLF_LANG_FIND_BY_CODE 2

/* font->clip_mode */
#define BLF_CLIP_DISABLE 0
#define BLF_CLIP_OUT 1

/* font->type */
#define BLF_FONT_FREETYPE2 0
#define BLF_FONT_INTERNAL 1

#endif /* BLF_INTERNAL_TYPES_H */
