/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;

/**
 * Thumbnail creation and retrieval according to the 'Thumbnail Management Standard'
 * supported by Gimp, Gnome (Nautilus), KDE etc.
 * Reference: http://jens.triq.net/thumbnail-spec/index.html
 */

typedef enum ThumbSize {
  THB_NORMAL,
  THB_LARGE,
  THB_FAIL,
} ThumbSize;

typedef enum ThumbSource {
  THB_SOURCE_IMAGE,
  THB_SOURCE_MOVIE,
  THB_SOURCE_BLEND,
  THB_SOURCE_FONT,
} ThumbSource;

/**
 * Don't generate thumbs for images bigger than this (100mb).
 */
#define THUMB_SIZE_MAX (100 * 1024 * 1024)

#define PREVIEW_RENDER_DEFAULT_HEIGHT 128
#define PREVIEW_RENDER_LARGE_HEIGHT 256

/**
 * Note this can also be used as versioning system,
 * to force refreshing all thumbnails if e.g. we change some thumb generating code or so.
 * Only used by fonts so far.
 */
#define THUMB_DEFAULT_HASH "00000000000000000000000000000000"

/**
 * Create thumbnail for file and returns new imbuf for thumbnail.
 */
struct ImBuf *IMB_thumb_create(const char *filepath,
                               ThumbSize size,
                               ThumbSource source,
                               struct ImBuf *img);

/**
 * Read thumbnail for file and returns new imbuf for thumbnail.
 */
struct ImBuf *IMB_thumb_read(const char *filepath, ThumbSize size);

/**
 * Delete all thumbs for the file.
 */
void IMB_thumb_delete(const char *filepath, ThumbSize size);

/**
 * Create the thumb if necessary and manage failed and old thumbs.
 */
struct ImBuf *IMB_thumb_manage(const char *filepath, ThumbSize size, ThumbSource source);

/**
 * Create the necessary directories to store the thumbnails.
 */
void IMB_thumb_makedirs(void);

/**
 * Special function for loading a thumbnail embedded into a blend file.
 */
struct ImBuf *IMB_thumb_load_blend(const char *blen_path,
                                   const char *blen_group,
                                   const char *blen_id);

/**
 * Special function for previewing fonts.
 */
struct ImBuf *IMB_thumb_load_font(const char *filename, unsigned int x, unsigned int y);
bool IMB_thumb_load_font_get_hash(char *r_hash);

/* Threading */

void IMB_thumb_locks_acquire(void);
void IMB_thumb_locks_release(void);
void IMB_thumb_path_lock(const char *path);
void IMB_thumb_path_unlock(const char *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */
