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
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup imbuf
 */

#ifndef __IMB_THUMBS_H__
#define __IMB_THUMBS_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;

/** Thumbnail creation and retrieval according to the 'Thumbnail Management Standard'
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

/* don't generate thumbs for images bigger then this (100mb) */
#define THUMB_SIZE_MAX (100 * 1024 * 1024)

#define PREVIEW_RENDER_DEFAULT_HEIGHT 128

/* Note this can also be used as versioning system,
 * to force refreshing all thumbnails if e.g. we change some thumb generating code or so.
 * Only used by fonts so far. */
#define THUMB_DEFAULT_HASH "00000000000000000000000000000000"

/* create thumbnail for file and returns new imbuf for thumbnail */
struct ImBuf *IMB_thumb_create(const char *path,
                               ThumbSize size,
                               ThumbSource source,
                               struct ImBuf *ibuf);

/* read thumbnail for file and returns new imbuf for thumbnail */
struct ImBuf *IMB_thumb_read(const char *path, ThumbSize size);

/* delete all thumbs for the file */
void IMB_thumb_delete(const char *path, ThumbSize size);

/* return the state of the thumb, needed to determine how to manage the thumb */
struct ImBuf *IMB_thumb_manage(const char *path, ThumbSize size, ThumbSource source);

/* create the necessary dirs to store the thumbnails */
void IMB_thumb_makedirs(void);

/* special function for loading a thumbnail embedded into a blend file */
struct ImBuf *IMB_thumb_load_blend(const char *blen_path,
                                   const char *blen_group,
                                   const char *blen_id);
void IMB_thumb_overlay_blend(unsigned int *thumb, int width, int height, float aspect);

/* special function for previewing fonts */
struct ImBuf *IMB_thumb_load_font(const char *filename, unsigned int x, unsigned int y);
bool IMB_thumb_load_font_get_hash(char *r_hash);
void IMB_thumb_clear_translations(void);
void IMB_thumb_ensure_translations(void);

/* Threading */
void IMB_thumb_locks_acquire(void);
void IMB_thumb_locks_release(void);
void IMB_thumb_path_lock(const char *path);
void IMB_thumb_path_unlock(const char *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __IMB_THUMBS_H__ */
