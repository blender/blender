/*
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* define this to generate PNg images with content of search areas
  tracking between which failed */
#undef DUMP_FAILURE

#include "libmv-capi.h"

#include "libmv/tracking/klt_region_tracker.h"
#include "libmv/tracking/trklt_region_tracker.h"
#include "libmv/tracking/pyramid_region_tracker.h"
#include "libmv/tracking/retrack_region_tracker.h"

#include <stdlib.h>

#ifdef DUMP_FAILURE
#  include <png.h>
#endif

#define DEFAULT_WINDOW_HALFSIZE	5

typedef struct ConfiguredRegionTracker {
	libmv::TrkltRegionTracker *trklt_region_tracker;
	libmv::PyramidRegionTracker *pyramid_region_tracker;
	libmv::RegionTracker *region_tracker;
} ConfiguredRegionTracker;

libmv_regionTrackerHandle libmv_regionTrackerNew(int max_iterations, int pyramid_level, double tolerance)
{
	libmv::RegionTracker *region_tracker;
	libmv::TrkltRegionTracker *trklt_region_tracker = new libmv::TrkltRegionTracker;

	trklt_region_tracker->half_window_size = DEFAULT_WINDOW_HALFSIZE;
	trklt_region_tracker->max_iterations = max_iterations;

	libmv::PyramidRegionTracker *pyramid_region_tracker =
		new libmv::PyramidRegionTracker(trklt_region_tracker, pyramid_level);

	region_tracker = new libmv::RetrackRegionTracker(pyramid_region_tracker, tolerance);

	ConfiguredRegionTracker *configured_region_tracker = new ConfiguredRegionTracker;
	configured_region_tracker->trklt_region_tracker = trklt_region_tracker;
	configured_region_tracker->pyramid_region_tracker = pyramid_region_tracker;
	configured_region_tracker->region_tracker = region_tracker;

	return (libmv_regionTrackerHandle)configured_region_tracker;
}

static void floatBufToImage(const float *buf, int width, int height, libmv::FloatImage *image)
{
	int x, y, a = 0;

	image->resize(height, width);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			(*image)(y, x, 0) = buf[a++];
		}
	}
}

#ifdef DUMP_FAILURE
void savePNGImage(png_bytep *row_pointers, int width, int height, int depth, int color_type, char *file_name)
{
	png_infop info_ptr;
	png_structp png_ptr;
	FILE *fp = fopen(file_name, "wb");

	if (!fp)
		return;

	/* Initialize stuff */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);

	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		return;
	}

	png_init_io(png_ptr, fp);

	/* write header */
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		return;
	}

	png_set_IHDR(png_ptr, info_ptr, width, height,
		depth, color_type, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		return;
	}

	png_write_image(png_ptr, row_pointers);

	/* end write */
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		return;
	}

	png_write_end(png_ptr, NULL);

	fclose(fp);
}

static void saveImage(libmv::FloatImage image, int x0, int y0)
{
	int x, y;
	png_bytep *row_pointers;

	row_pointers= (png_bytep*)malloc(sizeof(png_bytep)*image.Height());

	for (y = 0; y < image.Height(); y++) {
		row_pointers[y]= (png_bytep)malloc(sizeof(png_byte)*4*image.Width());

		for (x = 0; x < image.Width(); x++) {
			if (x0 == x && y0 == y) {
				row_pointers[y][x*4+0]= 255;
				row_pointers[y][x*4+1]= 0;
				row_pointers[y][x*4+2]= 0;
				row_pointers[y][x*4+3]= 255;
			}
			else {
				float pixel = image(y, x, 0);
				row_pointers[y][x*4+0]= pixel*255;
				row_pointers[y][x*4+1]= pixel*255;
				row_pointers[y][x*4+2]= pixel*255;
				row_pointers[y][x*4+3]= 255;
			}
		}
	}

	{
		static int a= 0;
		char buf[128];
		snprintf(buf, sizeof(buf), "%02d.png", ++a);
		savePNGImage(row_pointers, image.Width(), image.Height(), 8, PNG_COLOR_TYPE_RGBA, buf);
	}

	for (y = 0; y < image.Height(); y++) {
		free(row_pointers[y]);
	}
	free(row_pointers);
}
#endif

int libmv_regionTrackerTrack(libmv_regionTrackerHandle tracker, const float *ima1, const float *ima2,
			 int width, int height, int half_window_size,
			 double x1, double y1, double *x2, double *y2)
{
	ConfiguredRegionTracker *configured_region_tracker;
	libmv::RegionTracker *region_tracker;
	libmv::TrkltRegionTracker *trklt_region_tracker;
	libmv::FloatImage old_patch, new_patch;

	configured_region_tracker = (ConfiguredRegionTracker *)tracker;
	trklt_region_tracker = configured_region_tracker->trklt_region_tracker;
	region_tracker = configured_region_tracker->region_tracker;

	trklt_region_tracker->half_window_size = half_window_size;

	floatBufToImage(ima1, width, height, &old_patch);
	floatBufToImage(ima2, width, height, &new_patch);

#ifndef DUMP_FAILURE
	return region_tracker->Track(old_patch, new_patch, x1, y1, x2, y2);
#else
	{
		double sx2 = *x2, sy2 = *y2;
		int result = region_tracker->Track(old_patch, new_patch, x1, y1, x2, y2);

		if (!result) {
			saveImage(old_patch, x1, y1);
			saveImage(new_patch, sx2, sy2);
		}

		return result;
	}
#endif
}

void libmv_regionTrackerDestroy(libmv_regionTrackerHandle tracker)
{
	ConfiguredRegionTracker *configured_region_tracker;

	configured_region_tracker = (ConfiguredRegionTracker *)tracker;

	delete configured_region_tracker->region_tracker;
	delete configured_region_tracker;
}
