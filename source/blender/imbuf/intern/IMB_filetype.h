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

	int (*is_a)(unsigned char *buf);
	int (*is_a_filepath)(const char *name);
	int (*ftype)(struct ImFileType *type, struct ImBuf *ibuf);
	struct ImBuf *(*load)(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
	struct ImBuf *(*load_filepath)(const char *name, int flags, char colorspace[IM_MAX_SPACE]);
	int (*save)(struct ImBuf *ibuf, const char *name, int flags);
	void (*load_tile)(struct ImBuf *ibuf, unsigned char *mem, size_t size, int tx, int ty, unsigned int *rect);

	int flag;
	int filetype;
	int default_save_role;
} ImFileType;

extern ImFileType IMB_FILE_TYPES[];
extern ImFileType *IMB_FILE_TYPES_LAST;

void imb_filetypes_init(void);
void imb_filetypes_exit(void);

void imb_tile_cache_init(void);
void imb_tile_cache_exit(void);

void imb_loadtile(struct ImBuf *ibuf, int tx, int ty, unsigned int *rect);
void imb_tile_cache_tile_free(struct ImBuf *ibuf, int tx, int ty);

/* Type Specific Functions */

/* png */
int imb_is_a_png(unsigned char *buf);
struct ImBuf *imb_loadpng(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savepng(struct ImBuf *ibuf, const char *name, int flags);

/* targa */
int imb_is_a_targa(unsigned char *buf);
struct ImBuf *imb_loadtarga(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savetarga(struct ImBuf *ibuf, const char *name, int flags);

/* iris */
int imb_is_a_iris(unsigned char *mem);
struct ImBuf *imb_loadiris(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_saveiris(struct ImBuf *ibuf, const char *name, int flags);

/* jp2 */
int imb_is_a_jp2(unsigned char *buf);
struct ImBuf *imb_jp2_decode(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savejp2(struct ImBuf *ibuf, const char *name, int flags);

/* jpeg */
int imb_is_a_jpeg(unsigned char *mem);
int imb_savejpeg(struct ImBuf *ibuf, const char *name, int flags);
struct ImBuf *imb_load_jpeg (unsigned char *buffer, size_t size, int flags, char colorspace[IM_MAX_SPACE]);

/* bmp */
int imb_is_a_bmp(unsigned char *buf);
struct ImBuf *imb_bmp_decode(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savebmp(struct ImBuf *ibuf, const char *name, int flags);

/* cocoa */
struct ImBuf *imb_cocoaLoadImage(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);

/* cineon */
int imb_save_cineon(struct ImBuf *buf, const char *name, int flags);
struct ImBuf *imb_load_cineon(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_is_cineon(unsigned char *buf);

/* dpx */
int imb_save_dpx(struct ImBuf *buf, const char *name, int flags);
struct ImBuf *imb_load_dpx(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_is_dpx(unsigned char *buf);

/* hdr */
int imb_is_a_hdr(unsigned char *buf);
struct ImBuf *imb_loadhdr(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
int imb_savehdr(struct ImBuf *ibuf, const char *name, int flags);

/* tiff */
void imb_inittiff(void);
int imb_is_a_tiff(unsigned char *buf);
struct ImBuf *imb_loadtiff(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE]);
void imb_loadtiletiff(struct ImBuf *ibuf, unsigned char *mem, size_t size,
	int tx, int ty, unsigned int *rect);
int imb_savetiff(struct ImBuf *ibuf, const char *name, int flags);

#endif	/* __IMB_FILETYPE_H__ */

