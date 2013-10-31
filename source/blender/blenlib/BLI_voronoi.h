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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_VORONOI_H__
#define __BLI_VORONOI_H__

struct ListBase;

/** \file BLI_voronoi.h
 *  \ingroup bli
 */

typedef struct VoronoiSite {
	float co[2];
	float color[3];
} VoronoiSite;

typedef struct VoronoiEdge {
	struct VoronoiEdge *next, *prev;

	float start[2], end[2];	/* start and end points */

	/* this fields are used during diagram computation only */

	float direction[2];		/* directional vector, from "start", points to "end", normal of |left, right| */

	float left[2];			/* point on Voronoi place on the left side of edge */
	float right[2];			/* point on Voronoi place on the right side of edge */

	float f, g;				/* directional coeffitients satisfying equation y = f * x + g (edge lies on this line) */

	/* some edges consist of two parts, so we add the pointer to another part to connect them at the end of an algorithm */
	struct VoronoiEdge *neighbor;
} VoronoiEdge;

typedef struct VoronoiTriangulationPoint {
	float co[2];
	float color[3];
	int power;
} VoronoiTriangulationPoint;

void BLI_voronoi_compute(const VoronoiSite *sites, int sites_total, int width, int height, struct ListBase *edges);

void BLI_voronoi_triangulate(const VoronoiSite *sites, int sites_total, struct ListBase *edges, int width, int height,
                             VoronoiTriangulationPoint **triangulated_points_r, int *triangulated_points_total_r,
                             int (**triangles_r)[3], int *triangles_total_r);

#endif /* __BLI_VORONOI_H__ */
