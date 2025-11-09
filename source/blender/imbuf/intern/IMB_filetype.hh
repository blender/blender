/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "IMB_imbuf.hh"

struct ImBuf;
struct ImFileColorSpace;

/* -------------------------------------------------------------------- */
/** \name Generic File Type
 * \{ */

#define IM_FTYPE_FLOAT 1

struct ImFileType {
  /** Optional, called once when initializing. */
  void (*init)();
  /** Optional, called once when exiting. */
  void (*exit)();

  /**
   * Check if the data matches this file types 'magic',
   * \note that this may only read in a small part of the files header,
   * see: #IMB_test_image_type for details.
   */
  bool (*is_a)(const unsigned char *buf, size_t size);

  /** Load an image from memory. */
  ImBuf *(*load)(const unsigned char *mem, size_t size, int flags, ImFileColorSpace &r_colorspace);
  /** Load an image from a file. */
  ImBuf *(*load_filepath)(const char *filepath, int flags, ImFileColorSpace &r_colorspace);
  /**
   * Load/Create a thumbnail image from a filepath. `max_thumb_size` is maximum size of either
   * dimension, so can return less on either or both. Should, if possible and performant, return
   * dimensions of the full-size image in r_width & r_height.
   */
  ImBuf *(*load_filepath_thumbnail)(const char *filepath,
                                    int flags,
                                    size_t max_thumb_size,
                                    ImFileColorSpace &r_colorspace,
                                    size_t *r_width,
                                    size_t *r_height);
  /** Save to a file (or memory if #IB_mem is set in `flags` and the format supports it). */
  bool (*save)(ImBuf *ibuf, const char *filepath, int flags);

  int flag;

  /** #eImbFileType */
  int filetype;

  int default_save_role;
};

/** Color space information provided by the file. */
struct ImFileColorSpace {
  /** Color space from metadata. */
  char metadata_colorspace[IM_MAX_SPACE] = "";
  /** Is image HDR with range potentially outside 0..1? */
  bool is_hdr_float = false;
};

extern const ImFileType IMB_FILE_TYPES[];
extern const ImFileType *IMB_FILE_TYPES_LAST;

const ImFileType *IMB_file_type_from_ftype(int ftype);
const ImFileType *IMB_file_type_from_ibuf(const ImBuf *ibuf);

void imb_filetypes_init();
void imb_filetypes_exit();

/** \} */

/* Type Specific Functions */

/* -------------------------------------------------------------------- */
/** \name Format: PNG (#IMB_FTYPE_PNG)
 * \{ */

bool imb_is_a_png(const unsigned char *mem, size_t size);
ImBuf *imb_load_png(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);
bool imb_save_png(ImBuf *ibuf, const char *filepath, int flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: TARGA (#IMB_FTYPE_TGA)
 * \{ */

bool imb_is_a_tga(const unsigned char *mem, size_t size);
ImBuf *imb_load_tga(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);
bool imb_save_tga(ImBuf *ibuf, const char *filepath, int flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: IRIS (#IMB_FTYPE_IRIS)
 * \{ */

bool imb_is_a_iris(const unsigned char *mem, size_t size);
/**
 * Read in a B/W RGB or RGBA iris image file and return an image buffer.
 */
ImBuf *imb_loadiris(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);
bool imb_saveiris(ImBuf *ibuf, const char *filepath, int flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: JP2 (#IMB_FTYPE_JP2)
 * \{ */

bool imb_is_a_jp2(const unsigned char *buf, size_t size);
ImBuf *imb_load_jp2(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);
ImBuf *imb_load_jp2_filepath(const char *filepath, int flags, ImFileColorSpace &r_colorspace);
bool imb_save_jp2(ImBuf *ibuf, const char *filepath, int flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: JPEG (#IMB_FTYPE_JPG)
 * \{ */

bool imb_is_a_jpeg(const unsigned char *mem, size_t size);
bool imb_savejpeg(ImBuf *ibuf, const char *filepath, int flags);
ImBuf *imb_load_jpeg(const unsigned char *buffer,
                     size_t size,
                     int flags,
                     ImFileColorSpace &r_colorspace);
ImBuf *imb_thumbnail_jpeg(const char *filepath,
                          int flags,
                          size_t max_thumb_size,
                          ImFileColorSpace &r_colorspace,
                          size_t *r_width,
                          size_t *r_height);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: BMP (#IMB_FTYPE_BMP)
 * \{ */

bool imb_is_a_bmp(const unsigned char *mem, size_t size);
ImBuf *imb_load_bmp(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);
/* Found write info at http://users.ece.gatech.edu/~slabaugh/personal/c/bitmapUnix.c */
bool imb_save_bmp(ImBuf *ibuf, const char *filepath, int flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: CINEON (#IMB_FTYPE_CINEON)
 * \{ */

bool imb_is_a_cineon(const unsigned char *mem, size_t size);
bool imb_save_cineon(ImBuf *buf, const char *filepath, int flags);
ImBuf *imb_load_cineon(const unsigned char *mem,
                       size_t size,
                       int flags,
                       ImFileColorSpace &r_colorspace);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: DPX (#IMB_FTYPE_DPX)
 * \{ */

bool imb_is_a_dpx(const unsigned char *mem, size_t size);
bool imb_save_dpx(ImBuf *ibuf, const char *filepath, int flags);
ImBuf *imb_load_dpx(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: HDR (#IMB_FTYPE_RADHDR)
 * \{ */

bool imb_is_a_hdr(const unsigned char *mem, size_t size);
ImBuf *imb_load_hdr(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);
bool imb_save_hdr(ImBuf *ibuf, const char *filepath, int flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: TIFF (#IMB_FTYPE_TIF)
 * \{ */

bool imb_is_a_tiff(const unsigned char *mem, size_t size);
/**
 * Loads a TIFF file.
 * \param mem: Memory containing the TIFF file.
 * \param size: Size of the mem buffer.
 * \param flags: If flags has IB_test set then the file is not actually loaded,
 * but all other operations take place.
 *
 * \return A newly allocated #ImBuf structure if successful, otherwise NULL.
 */
ImBuf *imb_load_tiff(const unsigned char *mem,
                     size_t size,
                     int flags,
                     ImFileColorSpace &r_colorspace);
/**
 * Saves a TIFF file.
 *
 * #ImBuf structures with 1, 3 or 4 bytes per pixel (GRAY, RGB, RGBA respectively)
 * are accepted, and interpreted correctly. Note that the TIFF convention is to use
 * pre-multiplied alpha, which can be achieved within Blender by setting `premul` alpha handling.
 * Other alpha conventions are not strictly correct, but are permitted anyhow.
 *
 * \param ibuf: Image buffer.
 * \param filepath: Name of the TIFF file to create.
 * \param flags: Currently largely ignored.
 *
 * \return 1 if the function is successful, 0 on failure.
 */
bool imb_save_tiff(ImBuf *ibuf, const char *filepath, int flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: WEBP (#IMB_FTYPE_WEBP)
 * \{ */

bool imb_is_a_webp(const unsigned char *mem, size_t size);
ImBuf *imb_loadwebp(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);
ImBuf *imb_load_filepath_thumbnail_webp(const char *filepath,
                                        const int flags,
                                        const size_t max_thumb_size,
                                        ImFileColorSpace &r_colorspace,
                                        size_t *r_width,
                                        size_t *r_height);
bool imb_savewebp(ImBuf *ibuf, const char *filepath, int flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: DDS (#IMB_FTYPE_DDS)
 * \{ */

void imb_init_dds();

bool imb_is_a_dds(const unsigned char *mem, size_t size);

ImBuf *imb_load_dds(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: PSD (#IMB_FTYPE_PSD)
 * \{ */

bool imb_is_a_psd(const unsigned char *mem, size_t size);

ImBuf *imb_load_psd(const unsigned char *mem,
                    size_t size,
                    int flags,
                    ImFileColorSpace &r_colorspace);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format: SVG - Only for thumbnails.
 * \{ */

ImBuf *imb_load_filepath_thumbnail_svg(const char *filepath,
                                       const int flags,
                                       const size_t max_thumb_size,
                                       ImFileColorSpace &r_colorspace,
                                       size_t *r_width,
                                       size_t *r_height);

/** \} */
