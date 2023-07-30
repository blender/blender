/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup datatoc
 */

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* for bool */
#include "../blenlib/BLI_sys_types.h"

/* for DIR */
#if !defined(WIN32) || defined(FREEWINDOWS)
#  include <dirent.h>
#endif

#include <png.h>

/* for Win32 DIR functions */
#ifdef WIN32
#  include "../blenlib/BLI_winstuff.h"
#endif

#ifdef WIN32
#  define SEP '\\'
#else
#  define SEP '/'
#endif

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static bool path_test_extension(const char *filepath, const char *ext)
{
  const size_t a = strlen(filepath);
  const size_t b = strlen(ext);
  return !(a == 0 || b == 0 || b >= a) && (strcmp(ext, filepath + a - b) == 0);
}

static void endian_switch_uint32(uint *val)
{
  uint tval = *val;
  *val = (tval >> 24) | ((tval << 8) & 0x00ff0000) | ((tval >> 8) & 0x0000ff00) | (tval << 24);
}

static const char *path_slash_rfind(const char *path)
{
  const char *const lfslash = strrchr(path, '/');
  const char *const lbslash = strrchr(path, '\\');

  if (!lfslash) {
    return lbslash;
  }
  if (!lbslash) {
    return lfslash;
  }

  return (lfslash > lbslash) ? lfslash : lbslash;
}

static const char *path_basename(const char *path)
{
  const char *const filename = path_slash_rfind(path);
  return filename ? filename + 1 : path;
}

static bool path_join(char *filepath,
                      size_t filepath_maxncpy,
                      const char *dirpath,
                      const char *filename)
{
  int dirpath_len = strlen(dirpath);
  if (dirpath_len && dirpath[dirpath_len - 1] == SEP) {
    dirpath_len--;
  }
  const int filename_len = strlen(filename);
  if (dirpath_len + 1 + filename_len + 1 > filepath_maxncpy) {
    return false;
  }
  memcpy(filepath, dirpath, dirpath_len);
  filepath[dirpath_len] = SEP;
  memcpy(filepath + dirpath_len + 1, filename, filename_len + 1);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Write a PNG from RGBA Pixels
 * \{ */

static bool write_png(const char *filepath, const uint *pixels, const int width, const int height)
{
  png_structp png_ptr;
  png_infop info_ptr;
  png_bytepp row_pointers = nullptr;

  FILE *fp;

  const int bytesperpixel = 4;
  const int compression = 9;
  int i;

  fp = fopen(filepath, "wb");
  if (fp == nullptr) {
    printf("%s: Cannot open file for writing '%s'\n", __func__, filepath);
    return false;
  }

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (png_ptr == nullptr) {
    printf("%s: Cannot png_create_write_struct for file: '%s'\n", __func__, filepath);
    fclose(fp);
    return false;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == nullptr) {
    png_destroy_write_struct(&png_ptr, (png_infopp) nullptr);
    printf("%s: Cannot png_create_info_struct for file: '%s'\n", __func__, filepath);
    fclose(fp);
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    printf("%s: Cannot setjmp for file: '%s'\n", __func__, filepath);
    fclose(fp);
    return false;
  }

  /* write the file */
  png_init_io(png_ptr, fp);

  png_set_compression_level(png_ptr, compression);

  /* png image settings */
  png_set_IHDR(png_ptr,
               info_ptr,
               width,
               height,
               8,
               PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  /* write the file header information */
  png_write_info(png_ptr, info_ptr);

#ifdef __LITTLE_ENDIAN__
  png_set_swap(png_ptr);
#endif

  /* allocate memory for an array of row-pointers */
  row_pointers = (png_bytepp)malloc(height * sizeof(png_bytep));
  if (row_pointers == nullptr) {
    printf("%s: Cannot allocate row-pointers array for file '%s'\n", __func__, filepath);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    if (fp) {
      fclose(fp);
    }
    return false;
  }

  /* set the individual row-pointers to point at the correct offsets */
  for (i = 0; i < height; i++) {
    row_pointers[height - 1 - i] = (png_bytep)(((const uchar *)pixels) +
                                               (i * width) * bytesperpixel * sizeof(uchar));
  }

  /* write out the entire image data in one call */
  png_write_image(png_ptr, row_pointers);

  /* write the additional chunks to the PNG file (not really needed) */
  png_write_end(png_ptr, info_ptr);

  /* clean up */
  free(row_pointers);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  fflush(fp);
  fclose(fp);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Merge Icon-Data from Files
 * \{ */

struct IconHead {
  uint icon_w, icon_h;
  uint orig_x, orig_y;
  uint canvas_w, canvas_h;
};

struct IconInfo {
  IconHead head;
  char *file_name;
};

struct IconMergeContext {
  /* Information about all icons read from disk.
   * Is used for sanity checks like prevention of two files defining icon for
   * the same position on canvas. */
  int num_read_icons;
  IconInfo *read_icons;
};

static void icon_merge_context_init(IconMergeContext *context)
{
  context->num_read_icons = 0;
  context->read_icons = nullptr;
}

/* Get icon information from the context which matches given icon head.
 * Is used to check whether icon is re-defined, and to provide useful information about which
 * files are conflicting. */
static IconInfo *icon_merge_context_info_for_icon_head(IconMergeContext *context,
                                                       IconHead *icon_head)
{
  if (context->read_icons == nullptr) {
    return nullptr;
  }

  for (int i = 0; i < context->num_read_icons; i++) {
    IconInfo *read_icon_info = &context->read_icons[i];
    const IconHead *read_icon_head = &read_icon_info->head;
    if (read_icon_head->orig_x == icon_head->orig_x && read_icon_head->orig_y == icon_head->orig_y)
    {
      return read_icon_info;
    }
  }

  return nullptr;
}

static void icon_merge_context_register_icon(IconMergeContext *context,
                                             const char *file_name,
                                             const IconHead *icon_head)
{
  context->read_icons = static_cast<IconInfo *>(
      realloc(context->read_icons, sizeof(IconInfo) * (context->num_read_icons + 1)));

  IconInfo *icon_info = &context->read_icons[context->num_read_icons];
  icon_info->head = *icon_head;
  icon_info->file_name = strdup(path_basename(file_name));

  context->num_read_icons++;
}

static void icon_merge_context_free(IconMergeContext *context)
{
  if (context->read_icons != nullptr) {
    for (int i = 0; i < context->num_read_icons; i++) {
      free(context->read_icons[i].file_name);
    }
    free(context->read_icons);
  }
}

static bool icon_decode_head(FILE *f_src, IconHead *r_head)
{
  if (fread(r_head, 1, sizeof(*r_head), f_src) == sizeof(*r_head)) {
#ifndef __LITTLE_ENDIAN__
    endian_switch_uint32(&r_head->icon_w);
    endian_switch_uint32(&r_head->icon_h);
    endian_switch_uint32(&r_head->orig_x);
    endian_switch_uint32(&r_head->orig_y);
    endian_switch_uint32(&r_head->canvas_w);
    endian_switch_uint32(&r_head->canvas_h);
#endif
    return true;
  }

  /* quiet warning */
  (void)endian_switch_uint32;

  return false;
}

static bool icon_decode(FILE *f_src, IconHead *r_head, uint **r_pixels)
{
  uint *pixels;
  uint pixels_size;

  if (!icon_decode_head(f_src, r_head)) {
    printf("%s: failed to read header\n", __func__);
    return false;
  }

  pixels_size = sizeof(char[4]) * r_head->icon_w * r_head->icon_h;
  pixels = static_cast<uint *>(malloc(pixels_size));
  if (pixels == nullptr) {
    printf("%s: failed to allocate pixels\n", __func__);
    return false;
  }

  if (fread(pixels, 1, pixels_size, f_src) != pixels_size) {
    printf("%s: failed to read pixels\n", __func__);
    free(pixels);
    return false;
  }

  *r_pixels = pixels;
  return true;
}

static bool icon_read(const char *file_src, IconHead *r_head, uint **r_pixels)
{
  FILE *f_src;
  bool success;

  f_src = fopen(file_src, "rb");
  if (f_src == nullptr) {
    printf("%s: failed to open '%s'\n", __func__, file_src);
    return false;
  }

  success = icon_decode(f_src, r_head, r_pixels);

  fclose(f_src);
  return success;
}

static bool icon_merge(IconMergeContext *context,
                       const char *file_src,
                       uint32_t **r_pixels_canvas,
                       uint *r_canvas_w,
                       uint *r_canvas_h)
{
  IconHead head;
  uint *pixels;

  uint x, y;

  /* canvas */
  uint32_t *pixels_canvas;
  uint canvas_w, canvas_h;

  if (!icon_read(file_src, &head, &pixels)) {
    return false;
  }

  const IconInfo *read_icon_info = icon_merge_context_info_for_icon_head(context, &head);
  if (read_icon_info != nullptr) {
    printf(
        "Conflicting icon files %s and %s\n", path_basename(file_src), read_icon_info->file_name);
    free(pixels);
    return false;
  }
  icon_merge_context_register_icon(context, file_src, &head);

  if (*r_canvas_w == 0) {
    /* init once */
    *r_canvas_w = head.canvas_w;
    *r_canvas_h = head.canvas_h;
    *r_pixels_canvas = static_cast<uint32_t *>(
        calloc(1, (head.canvas_w * head.canvas_h) * sizeof(uint32_t)));
  }

  canvas_w = *r_canvas_w;
  canvas_h = *r_canvas_h;
  pixels_canvas = *r_pixels_canvas;

  assert(head.canvas_w == canvas_w);
  assert(head.canvas_h == canvas_h);

  for (x = 0; x < head.icon_w; x++) {
    for (y = 0; y < head.icon_h; y++) {
      uint pixel;
      uint dst_x, dst_y;
      uint pixel_xy_dst;

      /* get pixel */
      pixel = pixels[(y * head.icon_w) + x];

      /* set pixel */
      dst_x = head.orig_x + x;
      dst_y = head.orig_y + y;
      pixel_xy_dst = (dst_y * canvas_w) + dst_x;
      assert(pixel_xy_dst < (canvas_w * canvas_h));
      pixels_canvas[pixel_xy_dst] = pixel;
    }
  }

  free(pixels);

  /* only for bounds check */
  (void)canvas_h;

  return true;
}

static bool icondir_to_png(const char *path_src, const char *file_dst)
{
  /* Takes a path full of 'dat' files and writes out */
  DIR *dir;
  const dirent *fname;
  char filepath[1024];
  int found = 0, fail = 0;

  IconMergeContext context;

  uint32_t *pixels_canvas = nullptr;
  uint canvas_w = 0, canvas_h = 0;

  icon_merge_context_init(&context);

  errno = 0;
  dir = opendir(path_src);
  if (dir == nullptr) {
    printf(
        "%s: failed to dir '%s', (%s)\n", __func__, path_src, errno ? strerror(errno) : "unknown");
    return false;
  }

  while ((fname = readdir(dir)) != nullptr) {
    if (path_test_extension(fname->d_name, ".dat")) {
      if (!path_join(filepath, sizeof(filepath), path_src, fname->d_name)) {
        printf("%s: path is too long (%s, %s)\n", __func__, path_src, fname->d_name);
        return false;
      }
      if (icon_merge(&context, filepath, &pixels_canvas, &canvas_w, &canvas_h)) {
        found++;
      }
      else {
        fail++;
      }
    }
  }

  icon_merge_context_free(&context);

  closedir(dir);

  if (found == 0) {
    printf("%s: dir '%s' has no icons\n", __func__, path_src);
  }

  if (fail != 0) {
    printf("%s: dir '%s' failed %d icons\n", __func__, path_src, fail);
  }

  /* Write pixels. */
  write_png(file_dst, pixels_canvas, canvas_w, canvas_h);

  free(pixels_canvas);

  return (fail == 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main & Parse Arguments
 * \{ */

int main(int argc, char **argv)
{
  const char *path_src;
  const char *file_dst;

  if (argc < 3) {
    printf("Usage: datatoc_icon <dir_icons> <data_icon_to.png>\n");
    exit(1);
  }

  path_src = argv[1];
  file_dst = argv[2];

  return (icondir_to_png(path_src, file_dst) == true) ? 0 : 1;
}

/** \} */
