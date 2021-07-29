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


#ifndef _COM_KeyingScreenOperation_h
#define _COM_KeyingScreenOperation_h

#include <string.h>

#include "COM_NodeOperation.h"

#include "DNA_movieclip_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

extern "C" {
#  include "BLI_voronoi.h"
}

/**
 * Class with implementation of green screen gradient rasterization
 */
class KeyingScreenOperation : public NodeOperation {
protected:
	typedef struct TriangulationData {
		VoronoiTriangulationPoint *triangulated_points;
		int (*triangles)[3];
		int triangulated_points_total, triangles_total;
		rcti *triangles_AABB;
	} TriangulationData;

	typedef struct TileData {
		int *triangles;
		int triangles_total;
	} TileData;

	MovieClip *m_movieClip;
	int m_framenumber;
	TriangulationData *m_cachedTriangulation;
	char m_trackingObject[64];

	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);

	TriangulationData *buildVoronoiTriangulation();

public:
	KeyingScreenOperation();

	void initExecution();
	void deinitExecution();

	void *initializeTileData(rcti *rect);
	void deinitializeTileData(rcti *rect, void *data);

	void setMovieClip(MovieClip *clip) {this->m_movieClip = clip;}
	void setTrackingObject(const char *object) { BLI_strncpy(this->m_trackingObject, object, sizeof(this->m_trackingObject)); }
	void setFramenumber(int framenumber) {this->m_framenumber = framenumber;}

	void executePixel(float output[4], int x, int y, void *data);
};

#endif
