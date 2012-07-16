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
 * The Original Code is Copyright (C) 2012 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Xavier Thomas,
 *                 Lukas Toenne,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef IMB_COLORMANAGEMENT_H
#define IMB_COLORMANAGEMENT_H

#define BCM_CONFIG_FILE "config.ocio"

struct ColorManagedColorspaceSettings;
struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct EnumPropertyItem;
struct ImBuf;
struct Main;
struct rcti;
struct PartialBufferUpdateContext;
struct wmWindow;

/* ** Initialization / De-initialization ** */

void IMB_colormanagement_init(void);
void IMB_colormanagement_exit(void);

/* ** Color space transformation functions ** */
void IMB_colormanagement_colorspace_transform(float *buffer, int width, int height, int channels,
                                              const char *from_colorspace, const char *to_colorspace);

void IMB_colormanagement_imbuf_make_scene_linear(struct ImBuf *ibuf,
		struct ColorManagedColorspaceSettings *colorspace_settings);

/* ** Public display buffers interfaces ** */

void IMB_colormanage_cache_free(struct ImBuf *ibuf);

unsigned char *IMB_display_buffer_acquire(struct ImBuf *ibuf, const struct ColorManagedViewSettings *view_settings,
                                          const struct ColorManagedDisplaySettings *display_settings, void **cache_handle);
void IMB_display_buffer_to_imbuf_rect(struct ImBuf *ibuf, const struct ColorManagedViewSettings *view_settings,
                                      const struct ColorManagedDisplaySettings *display_settings);

void IMB_display_buffer_release(void *cache_handle);

void IMB_display_buffer_invalidate(struct ImBuf *ibuf);

void IMB_colormanagement_check_file_config(struct Main *bmain);

const struct ColorManagedViewSettings *IMB_view_settings_get_effective(struct wmWindow *win,
		const struct ColorManagedViewSettings *view_settings);

/* ** Display funcrions ** */
int IMB_colormanagement_display_get_named_index(const char *name);
const char *IMB_colormanagement_display_get_indexed_name(int index);
const char *IMB_colormanagement_display_get_default_name(void);

/* ** View funcrions ** */
int IMB_colormanagement_view_get_named_index(const char *name);
const char *IMB_colormanagement_view_get_indexed_name(int index);

/* ** Color space functions ** */
int IMB_colormanagement_colorspace_get_named_index(const char *name);
const char *IMB_colormanagement_colorspace_get_indexed_name(int index);

/* ** RNA helper functions ** */
void IMB_colormanagement_display_items_add(struct EnumPropertyItem **items, int *totitem);
void IMB_colormanagement_view_items_add(struct EnumPropertyItem **items, int *totitem, const char *display_name);
void IMB_colormanagement_colorspace_items_add(struct EnumPropertyItem **items, int *totitem);

/* Tile-based buffer management */
struct PartialBufferUpdateContext *IMB_partial_buffer_update_context_new(struct ImBuf *ibuf);
void IMB_partial_buffer_update_rect(struct PartialBufferUpdateContext *context, const float *linear_buffer, struct rcti *rect);
void IMB_partial_buffer_update_free(struct PartialBufferUpdateContext *context, struct ImBuf *ibuf);

#endif // IMB_COLORMANAGEMENT_H
