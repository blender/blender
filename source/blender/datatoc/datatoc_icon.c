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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/datatoc/datatoc_icon.c
 *  \ingroup datatoc
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

/* for bool */
#include "../blenlib/BLI_sys_types.h"

/* for DIR */
#if !defined(WIN32) || defined(FREEWINDOWS)
#  include <dirent.h>
#endif

#include "png.h"


/* for Win32 DIR functions */
#ifdef WIN32
#  include "../blenlib/BLI_winstuff.h"
#endif

#ifdef WIN32
#  define SEP '\\'
#else
#  define SEP '/'
#endif

#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#endif

/* -------------------------------------------------------------------- */
/* Utility functions */

static int path_ensure_slash(char *string)
{
	int len = strlen(string);
	if (len == 0 || string[len - 1] != SEP) {
		string[len] = SEP;
		string[len + 1] = '\0';
		return len + 1;
	}
	return len;
}

static bool path_test_extension(const char *str, const char *ext)
{
	const size_t a = strlen(str);
	const size_t b = strlen(ext);
	return !(a == 0 || b == 0 || b >= a) && (strcmp(ext, str + a - b) == 0);
}

static void endian_switch_uint32(unsigned int *val)
{
	unsigned int tval = *val;
	*val = ((tval >> 24))             |
	       ((tval << 8) & 0x00ff0000) |
	       ((tval >> 8) & 0x0000ff00) |
	       ((tval << 24));
}

/* -------------------------------------------------------------------- */
/* Write a PNG from RGBA pixels */

static bool write_png(const char *name, const unsigned int *pixels,
                     const int width, const int height)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytepp row_pointers = NULL;

	FILE *fp;

	const int bytesperpixel = 4;
	const int compression = 9;
	int i;

	fp = fopen(name, "wb");
	if (fp == NULL) {
		printf("%s: Cannot open file for writing '%s'\n", __func__, name);
		return false;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                  NULL, NULL, NULL);
	if (png_ptr == NULL) {
		printf("%s: Cannot png_create_write_struct for file: '%s'\n", __func__, name);
		fclose(fp);
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		printf("%s: Cannot png_create_info_struct for file: '%s'\n", __func__, name);
		fclose(fp);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		printf("%s: Cannot setjmp for file: '%s'\n", __func__, name);
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
	row_pointers = (png_bytepp) malloc(height * sizeof(png_bytep));
	if (row_pointers == NULL) {
		printf("%s: Cannot allocate row-pointers array for file '%s'\n", __func__, name);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		if (fp) {
			fclose(fp);
		}
		return false;
	}

	/* set the individual row-pointers to point at the correct offsets */
	for (i = 0; i < height; i++) {
		row_pointers[height - 1 - i] = (png_bytep)
		                               (((const unsigned char *)pixels) +
		                                (i * width) * bytesperpixel * sizeof(unsigned char));
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


/* -------------------------------------------------------------------- */
/* Merge icon-data from files */

struct IconHead {
	unsigned int icon_w, icon_h;
	unsigned int orig_x, orig_y;
	unsigned int canvas_w, canvas_h;
};

static bool icon_decode_head(FILE *f_src,
                            struct IconHead *r_head)
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

static bool icon_decode(FILE *f_src,
                        struct IconHead *r_head, unsigned int **r_pixels)
{
	unsigned int *pixels;
	unsigned int pixels_size;

	if (!icon_decode_head(f_src, r_head)) {
		printf("%s: failed to read header\n", __func__);
		return false;
	}

	pixels_size = sizeof(char[4]) * r_head->icon_w * r_head->icon_h;
	pixels = malloc(pixels_size);
	if (pixels == NULL) {
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

static bool icon_read(const char *file_src,
                      struct IconHead *r_head, unsigned int **r_pixels)
{
	FILE *f_src;
	bool success;

	f_src = fopen(file_src, "rb");
	if (f_src == NULL) {
		printf("%s: failed to open '%s'\n", __func__, file_src);
		return false;
	}

	success = icon_decode(f_src, r_head, r_pixels);

	fclose(f_src);
	return success;
}

static bool icon_merge(const char *file_src,
                       unsigned int **r_pixels_canvas,
                       unsigned int *r_canvas_w, unsigned int *r_canvas_h)
{
	struct IconHead head;
	unsigned int *pixels;

	unsigned int x, y;

	/* canvas */
	unsigned int *pixels_canvas;
	unsigned int canvas_w, canvas_h;

	if (!icon_read(file_src, &head, &pixels)) {
		return false;
	}

	if (*r_canvas_w == 0) {
		/* init once */
		*r_canvas_w = head.canvas_w;
		*r_canvas_h = head.canvas_h;
		*r_pixels_canvas = calloc(1, (head.canvas_w * head.canvas_h) * sizeof(unsigned char[4]));
	}

	canvas_w = *r_canvas_w;
	canvas_h = *r_canvas_h;
	pixels_canvas = *r_pixels_canvas;

	assert(head.canvas_w == canvas_w);
	assert(head.canvas_h == canvas_h);

	for (x = 0; x < head.icon_w; x++) {
		for (y = 0; y < head.icon_h; y++) {
			unsigned int pixel;
			unsigned int dst_x, dst_y;
			unsigned int pixel_xy_dst;


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
	const struct dirent *fname;
	char filepath[1024];
	char *filename;
	int path_str_len;
	int found = 0, fail = 0;

	unsigned int *pixels_canvas = NULL;
	unsigned int canvas_w = 0, canvas_h = 0;

	errno = 0;
	dir = opendir(path_src);
	if (dir == NULL) {
		printf("%s: failed to dir '%s', (%s)\n", __func__, path_src, errno ? strerror(errno) : "unknown");
		return false;
	}

	strcpy(filepath, path_src);
	path_str_len = path_ensure_slash(filepath);
	filename = &filepath[path_str_len];


	while ((fname = readdir(dir)) != NULL) {
		if (path_test_extension(fname->d_name, ".dat")) {

			strcpy(filename, fname->d_name);

			if (icon_merge(filepath, &pixels_canvas, &canvas_w, &canvas_h)) {
				found++;
			}
			else {
				fail++;
			}
		}
	}

	closedir(dir);

	if (found == 0) {
		printf("%s: dir '%s' has no icons\n", __func__, path_src);
	}

	if (fail != 0) {
		printf("%s: dir '%s' failed %d icons\n", __func__, path_src, fail);
	}

	/* write pixels  */
	write_png(file_dst, pixels_canvas, canvas_w, canvas_h);

	free(pixels_canvas);

	return true;
}


/* -------------------------------------------------------------------- */
/* Main and parse args */

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
