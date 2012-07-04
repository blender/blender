/*
 * Copyright 2012, Blender Foundation.
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
 * Contributor:
 *		Jeroen Bakker
 *		Monique Dewanchand
 *		Sergey Sharybin
 */

#include "COM_KeyingScreenOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

#include "DNA_scene_types.h"

extern "C" {
	#include "BKE_movieclip.h"
	#include "BKE_tracking.h"

	#include "IMB_imbuf.h"
	#include "IMB_imbuf_types.h"
}

KeyingScreenOperation::KeyingScreenOperation() : NodeOperation()
{
	this->addOutputSocket(COM_DT_COLOR);
	this->m_movieClip = NULL;
	this->m_framenumber = 0;
	this->m_trackingObject[0] = 0;
	setComplex(true);
}

void KeyingScreenOperation::initExecution()
{
	initMutex();
	this->m_cachedTriangulation = NULL;
}

void KeyingScreenOperation::deinitExecution()
{
	if (this->m_cachedTriangulation) {
		TriangulationData *triangulation = this->m_cachedTriangulation;

		if (triangulation->triangulated_points)
			MEM_freeN(triangulation->triangulated_points);

		if (triangulation->triangles)
			MEM_freeN(triangulation->triangles);

		if (triangulation->triangles_AABB)
			MEM_freeN(triangulation->triangles_AABB);

		MEM_freeN(this->m_cachedTriangulation);

		this->m_cachedTriangulation = NULL;
	}
}

KeyingScreenOperation::TriangulationData *KeyingScreenOperation::buildVoronoiTriangulation()
{
	MovieClipUser user = {0};
	TriangulationData *triangulation;
	MovieTracking *tracking = &this->m_movieClip->tracking;
	MovieTrackingTrack *track;
	VoronoiSite *sites;
	ImBuf *ibuf;
	ListBase *tracksbase;
	ListBase edges = {NULL, NULL};
	int sites_total;
	int i;
	int width = this->getWidth();
	int height = this->getHeight();
	int clip_frame = BKE_movieclip_remap_scene_to_clip_frame(this->m_movieClip, this->m_framenumber);

	if (this->m_trackingObject[0]) {
		MovieTrackingObject *object = BKE_tracking_object_get_named(tracking, this->m_trackingObject);

		if (!object)
			return NULL;

		tracksbase = BKE_tracking_object_get_tracks(tracking, object);
	}
	else
		tracksbase = BKE_tracking_get_active_tracks(tracking);

	/* count sites */
	for (track = (MovieTrackingTrack *) tracksbase->first, sites_total = 0; track; track = track->next) {
		MovieTrackingMarker *marker = BKE_tracking_marker_get(track, clip_frame);

		if ((marker->flag & MARKER_DISABLED) == 0) {
			sites_total++;
		}
	}

	if (!sites_total)
		return NULL;

	BKE_movieclip_user_set_frame(&user, clip_frame);
	ibuf = BKE_movieclip_get_ibuf(this->m_movieClip, &user);

	if (!ibuf)
		return NULL;

	triangulation = (TriangulationData *) MEM_callocN(sizeof(TriangulationData), "keying screen triangulation data");

	sites = (VoronoiSite *) MEM_callocN(sizeof(VoronoiSite) * sites_total, "keyingscreen voronoi sites");
	track = (MovieTrackingTrack *) tracksbase->first;
	for (track = (MovieTrackingTrack *) tracksbase->first, i = 0; track; track = track->next, i++) {
		MovieTrackingMarker *marker = BKE_tracking_marker_get(track, clip_frame);
		VoronoiSite *site;
		ImBuf *pattern_ibuf;
		int j;

		if (marker->flag & MARKER_DISABLED)
			continue;

		site = &sites[i];

		pattern_ibuf = BKE_tracking_get_pattern_imbuf(ibuf, track, marker, TRUE, FALSE);

		zero_v3(site->color);
		for (j = 0; j < pattern_ibuf->x * pattern_ibuf->y; j++) {
			if (pattern_ibuf->rect_float) {
				add_v3_v3(site->color, &pattern_ibuf->rect_float[4 * j]);
			}
			else {
				unsigned char *rrgb = (unsigned char *)pattern_ibuf->rect;

				site->color[0] += srgb_to_linearrgb((float)rrgb[4 * j + 0] / 255.0f);
				site->color[1] += srgb_to_linearrgb((float)rrgb[4 * j + 1] / 255.0f);
				site->color[2] += srgb_to_linearrgb((float)rrgb[4 * j + 2] / 255.0f);
			}
		}

		mul_v3_fl(site->color, 1.0f / (pattern_ibuf->x * pattern_ibuf->y));
		IMB_freeImBuf(pattern_ibuf);

		site->co[0] = marker->pos[0] * width;
		site->co[1] = marker->pos[1] * height;
	}

	IMB_freeImBuf(ibuf);

	BLI_voronoi_compute(sites, sites_total, width, height, &edges);

	BLI_voronoi_triangulate(sites, sites_total, &edges, width, height,
	                        &triangulation->triangulated_points, &triangulation->triangulated_points_total,
	                        &triangulation->triangles, &triangulation->triangles_total);

	MEM_freeN(sites);
	BLI_freelistN(&edges);

	if (triangulation->triangles_total) {
		rctf *rect;
		rect = triangulation->triangles_AABB =
			(rctf *) MEM_callocN(sizeof(rctf) * triangulation->triangles_total, "voronoi triangulation AABB");

		for (i = 0; i < triangulation->triangles_total; i++, rect++) {
			int *triangle = triangulation->triangles[i];
			VoronoiTriangulationPoint *a = &triangulation->triangulated_points[triangle[0]],
			                          *b = &triangulation->triangulated_points[triangle[1]],
			                          *c = &triangulation->triangulated_points[triangle[2]];

			float min[2], max[2];

			INIT_MINMAX2(min, max);

			DO_MINMAX2(a->co, min, max);
			DO_MINMAX2(b->co, min, max);
			DO_MINMAX2(c->co, min, max);

			rect->xmin = min[0];
			rect->ymin = min[1];

			rect->xmax = max[0];
			rect->ymax = max[1];
		}
	}

	return triangulation;
}

void *KeyingScreenOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	TileData *tile_data;
	TriangulationData *triangulation;
	int triangles_allocated = 0;
	int chunk_size = 20;
	int i;
	rctf rect_float;

	if (this->m_movieClip == NULL)
		return NULL;

	if (!this->m_cachedTriangulation) {
		lockMutex();
		if (this->m_cachedTriangulation == NULL) {
			this->m_cachedTriangulation = buildVoronoiTriangulation();
		}
		unlockMutex();
	}

	triangulation = this->m_cachedTriangulation;

	if (!triangulation)
		return NULL;

	BLI_init_rctf(&rect_float, rect->xmin, rect->xmax, rect->ymin, rect->ymax);

	tile_data = (TileData *) MEM_callocN(sizeof(TileData), "keying screen tile data");

	for (i = 0; i < triangulation->triangles_total; i++) {
		bool ok = BLI_isect_rctf(&rect_float, &triangulation->triangles_AABB[i], NULL);

		if (ok) {
			tile_data->triangles_total++;

			if (tile_data->triangles_total > triangles_allocated) {
				if (!tile_data->triangles) {
					tile_data->triangles = (int *) MEM_mallocN(sizeof(int) * chunk_size,
					                                           "keying screen tile triangles chunk");
				}
				else {
					tile_data->triangles = (int *) MEM_reallocN(tile_data->triangles,
					                                            sizeof(int) * (triangles_allocated + chunk_size));
				}

				triangles_allocated += chunk_size;
			}

			tile_data->triangles[tile_data->triangles_total - 1] = i;
		}
	}

	return tile_data;
}

void KeyingScreenOperation::deinitializeTileData(rcti *rect, MemoryBuffer **memoryBuffers, void *data)
{
	TileData *tile_data = (TileData *) data;

	if (tile_data->triangles) {
		MEM_freeN(tile_data->triangles);
	}

	MEM_freeN(tile_data);
}

void KeyingScreenOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	resolution[0] = 0;
	resolution[1] = 0;

	if (this->m_movieClip) {
		MovieClipUser user = {0};
		int width, height;
		int clip_frame = BKE_movieclip_remap_scene_to_clip_frame(this->m_movieClip, this->m_framenumber);

		BKE_movieclip_user_set_frame(&user, clip_frame);
		BKE_movieclip_get_size(this->m_movieClip, &user, &width, &height);

		resolution[0] = width;
		resolution[1] = height;
	}
}

void KeyingScreenOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	color[0] = 0.0f;
	color[1] = 0.0f;
	color[2] = 0.0f;
	color[3] = 1.0f;

	if (this->m_movieClip && data) {
		TriangulationData *triangulation = this->m_cachedTriangulation;
		TileData *tile_data = (TileData *) data;
		int i;
		float co[2] = {(float) x, (float) y};

		for (i = 0; i < tile_data->triangles_total; i++) {
			int triangle_idx = tile_data->triangles[i];
			rctf *rect = &triangulation->triangles_AABB[triangle_idx];

			if (IN_RANGE_INCL(x, rect->xmin, rect->xmax) && IN_RANGE_INCL(y, rect->ymin, rect->ymax)) {
				int *triangle = triangulation->triangles[triangle_idx];
				VoronoiTriangulationPoint *a = &triangulation->triangulated_points[triangle[0]],
				                          *b = &triangulation->triangulated_points[triangle[1]],
				                          *c = &triangulation->triangulated_points[triangle[2]];
				float w[3];

				if (barycentric_coords_v2(a->co, b->co, c->co, co, w)) {
					if (barycentric_inside_triangle_v2(w)) {
						color[0] += a->color[0] * w[0] + b->color[0] * w[1] + c->color[0] * w[2];
						color[1] += a->color[1] * w[0] + b->color[1] * w[1] + c->color[1] * w[2];
						color[2] += a->color[2] * w[0] + b->color[2] * w[1] + c->color[2] * w[2];
					}
				}
			}
		}
	}
}
