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

struct bContext;
struct ColorManagedColorspaceSettings;
struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct ColormanageProcessor;
struct EnumPropertyItem;
struct ImBuf;
struct Main;
struct rcti;
struct PartialBufferUpdateContext;
struct wmWindow;
struct Scene;
struct ImageFormatData;

struct ColorSpace;
struct ColorManagedDisplay;

/* ** Initialization / De-initialization ** */

void IMB_colormanagement_init(void);
void IMB_colormanagement_exit(void);

/* ** Generic functions ** */

void IMB_colormanagement_check_file_config(struct Main *bmain);

void IMB_colormanagement_validate_settings(struct ColorManagedDisplaySettings *display_settings,
                                           struct ColorManagedViewSettings *view_settings);

const char *IMB_colormanagement_role_colorspace_name_get(int role);
void IMB_colormanagement_assign_rect_colorspace(struct ImBuf *ibuf, const char *name);

/* ** Color space transformation functions ** */
void IMB_colormanagement_transform(float *buffer, int width, int height, int channels,
                                   const char *from_colorspace, const char *to_colorspace, int predivide);
void IMB_colormanagement_transform_threaded(float *buffer, int width, int height, int channels,
                                            const char *from_colorspace, const char *to_colorspace, int predivide);
void IMB_colormanagement_transform_v4(float pixel[4], const char *from_colorspace, const char *to_colorspace);

void IMB_colormanagement_colorspace_to_scene_linear_v3(float pixel[3], struct ColorSpace *colorspace);
void IMB_colormanagement_scene_linear_to_colorspace_v3(float pixel[3], struct ColorSpace *colorspace);

void IMB_colormanagement_colorspace_to_scene_linear(float *buffer, int width, int height, int channels, struct ColorSpace *colorspace, int predivide);

void IMB_colormanagement_scene_linear_to_display_v3(float pixel[3], struct ColorManagedDisplay *display);
void IMB_colormanagement_display_to_scene_linear_v3(float pixel[3], struct ColorManagedDisplay *display);

void IMB_colormanagement_pixel_to_display_space_v4(float result[4], const float pixel[4],  const struct ColorManagedViewSettings *view_settings,
                                                   const struct ColorManagedDisplaySettings *display_settings);

void IMB_colormanagement_pixel_to_display_space_v3(float result[3], const float pixel[3],  const struct ColorManagedViewSettings *view_settings,
                                                   const struct ColorManagedDisplaySettings *display_settings);

void IMB_colormanagement_imbuf_assign_float_space(struct ImBuf *ibuf, struct ColorManagedColorspaceSettings *colorspace_settings);

void IMB_colormanagement_imbuf_make_display_space(struct ImBuf *ibuf, const struct ColorManagedViewSettings *view_settings,
                                                  const struct ColorManagedDisplaySettings *display_settings);

struct ImBuf *IMB_colormanagement_imbuf_for_write(struct ImBuf *ibuf, int save_as_render, int allocate_result,
                                                  const struct ColorManagedViewSettings *view_settings,
                                                  const struct ColorManagedDisplaySettings *display_settings,
                                                  struct ImageFormatData *image_format_data);

/* ** Public display buffers interfaces ** */

unsigned char *IMB_display_buffer_acquire(struct ImBuf *ibuf, const struct ColorManagedViewSettings *view_settings,
                                          const struct ColorManagedDisplaySettings *display_settings, void **cache_handle);
unsigned char *IMB_display_buffer_acquire_ctx(const struct bContext *C, struct ImBuf *ibuf, void **cache_handle);

void IMB_display_buffer_transform_apply(unsigned char *display_buffer, float *linear_buffer, int width, int height,
                                        int channels, const struct ColorManagedViewSettings *view_settings,
                                        const struct ColorManagedDisplaySettings *display_settings, int predivide);

void IMB_display_buffer_release(void *cache_handle);

/* ** Display funcrions ** */
int IMB_colormanagement_display_get_named_index(const char *name);
const char *IMB_colormanagement_display_get_indexed_name(int index);
const char *IMB_colormanagement_display_get_default_name(void);
struct ColorManagedDisplay *IMB_colormanagement_display_get_named(const char *name);

/* ** View funcrions ** */
int IMB_colormanagement_view_get_named_index(const char *name);
const char *IMB_colormanagement_view_get_indexed_name(int index);

/* ** Color space functions ** */
int IMB_colormanagement_colorspace_get_named_index(const char *name);
const char *IMB_colormanagement_colorspace_get_indexed_name(int index);
const char *IMB_colormanagement_view_get_default_name(const char *display_name);

void IMB_colormanagment_colorspace_from_ibuf_ftype(struct ColorManagedColorspaceSettings *colorspace_settings, struct ImBuf *ibuf);

/* ** RNA helper functions ** */
void IMB_colormanagement_display_items_add(struct EnumPropertyItem **items, int *totitem);
void IMB_colormanagement_view_items_add(struct EnumPropertyItem **items, int *totitem, const char *display_name);
void IMB_colormanagement_colorspace_items_add(struct EnumPropertyItem **items, int *totitem);

/* ** Tile-based buffer management ** */
void IMB_partial_display_buffer_update(struct ImBuf *ibuf, const float *linear_buffer, const unsigned char *buffer_byte,
                                       int stride, int offset_x, int offset_y, const struct ColorManagedViewSettings *view_settings,
                                       const struct ColorManagedDisplaySettings *display_settings,
                                       int xmin, int ymin, int xmax, int ymax);

/* ** Pixel processor functions ** */
struct ColormanageProcessor *IMB_colormanagement_display_processor_new(const struct ColorManagedViewSettings *view_settings,
                                                                       const struct ColorManagedDisplaySettings *display_settings);
struct ColormanageProcessor *IMB_colormanagement_colorspace_processor_new(const char *from_colorspace, const char *to_colorspace);
void IMB_colormanagement_processor_apply_v4(struct ColormanageProcessor *cm_processor, float pixel[4]);
void IMB_colormanagement_processor_apply_v3(struct ColormanageProcessor *cm_processor, float pixel[3]);
void IMB_colormanagement_processor_apply(struct ColormanageProcessor *cm_processor, float *buffer, int width, int height,
                                         int channels, int predivide);
void IMB_colormanagement_processor_free(struct ColormanageProcessor *cm_processor);

/* Roles */
enum {
	COLOR_ROLE_SCENE_LINEAR = 0,
	COLOR_ROLE_COLOR_PICKING,
	COLOR_ROLE_TEXTURE_PAINTING,
	COLOR_ROLE_DEFAULT_SEQUENCER,
	COLOR_ROLE_DEFAULT_BYTE,
	COLOR_ROLE_DEFAULT_FLOAT,
};

#endif  /* IMB_COLORMANAGEMENT_H */
