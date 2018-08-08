/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation 2010.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/IMB_filetype.h
 *  \ingroup imbuf
 */


#ifndef __IMB_FILETYPE_H__
#define __IMB_FILETYPE_H__

/* Generic File Type */

struct ImBuf;

#define IM_FTYPE_FLOAT	1

typedef struct ImFileType {
	void (*init)(void);
	void (*exit)(void);

	int (*is_a)(const unsigned char *buf);
	int (*is_a_filepath)(const char *name);
	int (*ftype)(const struct ImFileType *type, struct ImBuf *ibuf);
	struct ImBuf *(*load)(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
	struct ImBuf *(*load_filepath)(const char *name, int flags, char colorspace[IM_MAX_SPACE]);
	int (*save)(struct ImBuf *ibuf, const char *name, int flags);
	void (*load_tile)(struct ImBuf *ibuf, const unsigned char *mem, size_t size, int tx, int ty, unsigned int *rect);

	int flag;
	int filetype;
	int default_save_role;
} ImFileType;

extern const ImFileType IMB_FILE_TYPES[];
extern const ImFileType *IMB_FILE_TYPES_LAST;

void imb_filetypes_init(void);
void imb_filetypes_exit(void);

void imb_tile_cache_init(void);
void imb_tile_cache_exit(void);

void imb_loadtile(struct ImBuf *ibuf, int tx, int ty, unsigned int *rect);
void imb_tile_cache_tile_free(struct ImBuf *ibuf, int tx, int ty);

/* Type Specific Functions */

/* png */
int imb_is_a_png(const unsigned char *buf);
struct ImBuf *imb_loadpng(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savepng(struct ImBuf *ibuf, const char *name, int flags);

/* targa */
int imb_is_a_targa(const unsigned char *buf);
struct ImBuf *imb_loadtarga(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savetarga(struct ImBuf *ibuf, const char *name, int flags);

/* iris */
int imb_is_a_iris(const unsigned char *mem);
struct ImBuf *imb_loadiris(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_saveiris(struct ImBuf *ibuf, const char *name, int flags);

/* jp2 */
int imb_is_a_jp2(const unsigned char *buf);
struct ImBuf *imb_load_jp2(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
struct ImBuf *imb_load_jp2_filepath(const char *name, int flags, char colorspace[IM_MAX_SPACE]);
int imb_save_jp2(struct ImBuf *ibuf, const char *name, int flags);

/* jpeg */
int imb_is_a_jpeg(const unsigned char *mem);
int imb_savejpeg(struct ImBuf *ibuf, const char *name, int flags);
struct ImBuf *imb_load_jpeg(const unsigned char *buffer, size_t size, int flags, char colorspace[IM_MAX_SPACE]);

/* bmp */
int imb_is_a_bmp(const unsigned char *buf);
struct ImBuf *imb_bmp_decode(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savebmp(struct ImBuf *ibuf, const char *name, int flags);

/* cineon */
int imb_is_cineon(const unsigned char *buf);
int imb_save_cineon(struct ImBuf *buf, const char *name, int flags);
struct ImBuf *imb_load_cineon(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);

/* dpx */
int imb_is_dpx(const unsigned char *buf);
int imb_save_dpx(struct ImBuf *buf, const char *name, int flags);
struct ImBuf *imb_load_dpx(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);

/* hdr */
int imb_is_a_hdr(const unsigned char *buf);
struct ImBuf *imb_loadhdr(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savehdr(struct ImBuf *ibuf, const char *name, int flags);

/* tiff */
void imb_inittiff(void);
int imb_is_a_tiff(const unsigned char *buf);
struct ImBuf *imb_loadtiff(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
void imb_loadtiletiff(struct ImBuf *ibuf, const unsigned char *mem, size_t size,
	int tx, int ty, unsigned int *rect);
int imb_savetiff(struct ImBuf *ibuf, const char *name, int flags);

#endif	/* __IMB_FILETYPE_H__ */
