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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "libmv-capi.h"

#include "libmv/tracking/klt_region_tracker.h"
#include "libmv/tracking/trklt_region_tracker.h"
#include "libmv/tracking/pyramid_region_tracker.h"
#include "libmv/tracking/retrack_region_tracker.h"

#include <stdlib.h>

libmv_regionTrackerHandle libmv_regionTrackerNew(void)
{
	libmv::RegionTracker *region_tracker;
	libmv::TrkltRegionTracker *trklt_region_tracker = new libmv::TrkltRegionTracker;

	trklt_region_tracker->half_window_size = 5;
	trklt_region_tracker->max_iterations = 200;

	libmv::PyramidRegionTracker *pyramid_region_tracker =
		new libmv::PyramidRegionTracker(trklt_region_tracker, 3);

	region_tracker = new libmv::RetrackRegionTracker(pyramid_region_tracker, 0.2);

	return (libmv_regionTrackerHandle)region_tracker;
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

int libmv_regionTrackerTrack(libmv_regionTrackerHandle tracker, const float *ima1, const float *ima2,
			 int width, int height,
			 double  x1, double  y1, double *x2, double *y2)
{
	libmv::RegionTracker *region_tracker;
	libmv::FloatImage old_patch, new_patch;

	region_tracker = (libmv::RegionTracker *)tracker;

	floatBufToImage(ima1, width, height, &old_patch);
	floatBufToImage(ima2, width, height, &new_patch);

	return region_tracker->Track(old_patch, new_patch, x1, y1, x2, y2);
}

void libmv_regionTrackerDestroy(libmv_regionTrackerHandle tracker)
{
	libmv::RegionTracker *region_tracker;

	region_tracker = (libmv::RegionTracker *)tracker;

	delete region_tracker;
}
