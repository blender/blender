/*
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

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "IMB_imbuf.h"

/* Generic File Type */

struct ImBuf;

#define IM_FTYPE_FLOAT 1

typedef struct ImFileType {
  /** Optional, called once when initializing. */
  void (*init)(void);
  /** Optional, called once when exiting. */
  void (*exit)(void);

  /**
   * Check if the data matches this file types 'magic',
   * \note that this may only read in a small part of the files header,
   * see: #IMB_ispic_type for details.
   */
  bool (*is_a)(const unsigned char *buf, const size_t size);

  /** Load an image from memory. */
  struct ImBuf *(*load)(const unsigned char *mem,
                        size_t size,
                        int flags,
                        char colorspace[IM_MAX_SPACE]);
  /** Load an image from a file. */
  struct ImBuf *(*load_filepath)(const char *filepath, int flags, char colorspace[IM_MAX_SPACE]);
  /** Save to a file (or memory if #IB_mem is set in `flags` and the format supports it). */
  bool (*save)(struct ImBuf *ibuf, const char *filepath, int flags);
  void (*load_tile)(struct ImBuf *ibuf,
                    const unsigned char *mem,
                    size_t size,
                    int tx,
                    int ty,
                    unsigned int *rect);

  int flag;

  /** #eImbFileType */
  int filetype;

  int default_save_role;
} ImFileType;

extern const ImFileType IMB_FILE_TYPES[];
extern const ImFileType *IMB_FILE_TYPES_LAST;

const ImFileType *IMB_file_type_from_ftype(int ftype);
const ImFileType *IMB_file_type_from_ibuf(const struct ImBuf *ibuf);

void imb_filetypes_init(void);
void imb_filetypes_exit(void);

void imb_tile_cache_init(void);
void imb_tile_cache_exit(void);

void imb_loadtile(struct ImBuf *ibuf, int tx, int ty, unsigned int *rect);
void imb_tile_cache_tile_free(struct ImBuf *ibuf, int tx, int ty);

/* Type Specific Functions */

/* png */
bool imb_is_a_png(const unsigned char *mem, const size_t size);
struct ImBuf *imb_loadpng(const unsigned char *mem,
                          size_t size,
                          int flags,
                          char colorspace[IM_MAX_SPACE]);
bool imb_savepng(struct ImBuf *ibuf, const char *filepath, int flags);

/* targa */
bool imb_is_a_targa(const unsigned char *buf, const size_t size);
struct ImBuf *imb_loadtarga(const unsigned char *mem,
                            size_t size,
                            int flags,
                            char colorspace[IM_MAX_SPACE]);
bool imb_savetarga(struct ImBuf *ibuf, const char *filepath, int flags);

/* iris */
bool imb_is_a_iris(const unsigned char *mem, const size_t size);
struct ImBuf *imb_loadiris(const unsigned char *mem,
                           size_t size,
                           int flags,
                           char colorspace[IM_MAX_SPACE]);
bool imb_saveiris(struct ImBuf *ibuf, const char *filepath, int flags);

/* jp2 */
bool imb_is_a_jp2(const unsigned char *buf, const size_t size);
struct ImBuf *imb_load_jp2(const unsigned char *mem,
                           size_t size,
                           int flags,
                           char colorspace[IM_MAX_SPACE]);
struct ImBuf *imb_load_jp2_filepath(const char *filepath,
                                    int flags,
                                    char colorspace[IM_MAX_SPACE]);
bool imb_save_jp2(struct ImBuf *ibuf, const char *filepath, int flags);

/* jpeg */
bool imb_is_a_jpeg(const unsigned char *mem, const size_t size);
bool imb_savejpeg(struct ImBuf *ibuf, const char *filepath, int flags);
struct ImBuf *imb_load_jpeg(const unsigned char *buffer,
                            size_t size,
                            int flags,
                            char colorspace[IM_MAX_SPACE]);

/* bmp */
bool imb_is_a_bmp(const unsigned char *buf, const size_t size);
struct ImBuf *imb_bmp_decode(const unsigned char *mem,
                             size_t size,
                             int flags,
                             char colorspace[IM_MAX_SPACE]);
bool imb_savebmp(struct ImBuf *ibuf, const char *filepath, int flags);

/* cineon */
bool imb_is_a_cineon(const unsigned char *buf, const size_t size);
bool imb_save_cineon(struct ImBuf *buf, const char *filepath, int flags);
struct ImBuf *imb_load_cineon(const unsigned char *mem,
                              size_t size,
                              int flags,
                              char colorspace[IM_MAX_SPACE]);

/* dpx */
bool imb_is_a_dpx(const unsigned char *buf, const size_t size);
bool imb_save_dpx(struct ImBuf *buf, const char *filepath, int flags);
struct ImBuf *imb_load_dpx(const unsigned char *mem,
                           size_t size,
                           int flags,
                           char colorspace[IM_MAX_SPACE]);

/* hdr */
bool imb_is_a_hdr(const unsigned char *buf, const size_t size);
struct ImBuf *imb_loadhdr(const unsigned char *mem,
                          size_t size,
                          int flags,
                          char colorspace[IM_MAX_SPACE]);
bool imb_savehdr(struct ImBuf *ibuf, const char *filepath, int flags);

/* tiff */
void imb_inittiff(void);
bool imb_is_a_tiff(const unsigned char *buf, const size_t size);
struct ImBuf *imb_loadtiff(const unsigned char *mem,
                           size_t size,
                           int flags,
                           char colorspace[IM_MAX_SPACE]);
void imb_loadtiletiff(
    struct ImBuf *ibuf, const unsigned char *mem, size_t size, int tx, int ty, unsigned int *rect);
bool imb_savetiff(struct ImBuf *ibuf, const char *filepath, int flags);
