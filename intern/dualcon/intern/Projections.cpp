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
 * Contributor(s): Tao Ju
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

#include <math.h>
#include "Projections.h"

const int vertmap[8][3] = {
	{0, 0, 0},
	{0, 0, 1},
	{0, 1, 0},
	{0, 1, 1},
	{1, 0, 0},
	{1, 0, 1},
	{1, 1, 0},
	{1, 1, 1}
};

const int centmap[3][3][3][2] = {
	{{{0, 0}, {0, 1}, {1, 1}},
	 {{0, 2}, {0, 3}, {1, 3}},
	 {{2, 2}, {2, 3}, {3, 3}}},

	{{{0, 4}, {0, 5}, {1, 5}},
	 {{0, 6}, {0, 7}, {1, 7}},
	 {{2, 6}, {2, 7}, {3, 7}}},

	{{{4, 4}, {4, 5}, {5, 5}},
	 {{4, 6}, {4, 7}, {5, 7}},
	 {{6, 6}, {6, 7}, {7, 7}}}
};

const int edgemap[12][2] = {
	{0, 4},
	{1, 5},
	{2, 6},
	{3, 7},
	{0, 2},
	{1, 3},
	{4, 6},
	{5, 7},
	{0, 1},
	{2, 3},
	{4, 5},
	{6, 7}
};

const int facemap[6][4] = {
	{0, 1, 2, 3},
	{4, 5, 6, 7},
	{0, 1, 4, 5},
	{2, 3, 6, 7},
	{0, 2, 4, 6},
	{1, 3, 5, 7}
};

/**
 * Method to perform cross-product
 */
static void crossProduct(int64_t res[3], const int64_t a[3], const int64_t b[3])
{
	res[0] = a[1] * b[2] - a[2] * b[1];
	res[1] = a[2] * b[0] - a[0] * b[2];
	res[2] = a[0] * b[1] - a[1] * b[0];
}

static void crossProduct(double res[3], const double a[3], const double b[3])
{
	res[0] = a[1] * b[2] - a[2] * b[1];
	res[1] = a[2] * b[0] - a[0] * b[2];
	res[2] = a[0] * b[1] - a[1] * b[0];
}

/**
 * Method to perform dot product
 */
static int64_t dotProduct(const int64_t a[3], const int64_t b[3])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void normalize(double a[3])
{
	double mag = a[0] * a[0] + a[1] * a[1] + a[2] * a[2];
	if (mag > 0) {
		mag = sqrt(mag);
		a[0] /= mag;
		a[1] /= mag;
		a[2] /= mag;
	}
}

/* Create projection axes for cube+triangle intersection testing.
 *    0, 1, 2: cube face normals
 *    
 *          3: triangle normal
 *          
 *    4, 5, 6,
 *    7, 8, 9,
 * 10, 11, 12: cross of each triangle edge vector with each cube
 *             face normal
 */
static void create_projection_axes(int64_t axes[NUM_AXES][3], const int64_t tri[3][3])
{
	/* Cube face normals */
	axes[0][0] = 1;
	axes[0][1] = 0;
	axes[0][2] = 0;
	axes[1][0] = 0;
	axes[1][1] = 1;
	axes[1][2] = 0;
	axes[2][0] = 0;
	axes[2][1] = 0;
	axes[2][2] = 1;

	/* Get triangle edge vectors */
	int64_t tri_edges[3][3];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++)
			tri_edges[i][j] = tri[(i + 1) % 3][j] - tri[i][j];
	}

	/* Triangle normal */
	crossProduct(axes[3], tri_edges[0], tri_edges[1]);

	// Face edges and triangle edges
	int ct = 4;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			crossProduct(axes[ct], axes[j], tri_edges[i]);
			ct++;
		}
	}
}

/**
 * Construction from a cube (axes aligned) and triangle
 */
CubeTriangleIsect::CubeTriangleIsect(int64_t cube[2][3], int64_t tri[3][3], int64_t error, int triind)
{
	int i;
	inherit = new TriangleProjection;
	inherit->index = triind;

	int64_t axes[NUM_AXES][3];
	create_projection_axes(axes, tri);

	/* Normalize face normal and store */
	double dedge1[] = {(double)tri[1][0] - (double)tri[0][0],
					   (double)tri[1][1] - (double)tri[0][1],
					   (double)tri[1][2] - (double)tri[0][2]};
	double dedge2[] = {(double)tri[2][0] - (double)tri[1][0],
					   (double)tri[2][1] - (double)tri[1][1],
					   (double)tri[2][2] - (double)tri[1][2]};
	crossProduct(inherit->norm, dedge1, dedge2);
	normalize(inherit->norm);

	int64_t cubeedge[3][3];
	for (i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			cubeedge[i][j] = 0;
		}
		cubeedge[i][i] = cube[1][i] - cube[0][i];
	}

	/* Project the cube on to each axis */
	for (int axis = 0; axis < NUM_AXES; axis++) {
		CubeProjection &cube_proj = cubeProj[axis];

		/* Origin */
		cube_proj.origin = dotProduct(axes[axis], cube[0]);

		/* 3 direction vectors */
		for (i = 0; i < 3; i++)
			cube_proj.edges[i] = dotProduct(axes[axis], cubeedge[i]);

		/* Offsets of 2 ends of cube projection */
		int64_t max = 0;
		int64_t min = 0;
		for (i = 1; i < 8; i++) {
			int64_t proj = (vertmap[i][0] * cube_proj.edges[0] +
							vertmap[i][1] * cube_proj.edges[1] +
							vertmap[i][2] * cube_proj.edges[2]);
			if (proj > max) {
				max = proj;
			}
			if (proj < min) {
				min = proj;
			}
		}
		cube_proj.min = min;
		cube_proj.max = max;

	}

	/* Project the triangle on to each axis */
	for (int axis = 0; axis < NUM_AXES; axis++) {
		const int64_t vts[3] = {dotProduct(axes[axis], tri[0]),
								dotProduct(axes[axis], tri[1]),
								dotProduct(axes[axis], tri[2])};

		// Triangle
		inherit->tri_proj[axis][0] = vts[0];
		inherit->tri_proj[axis][1] = vts[0];
		for (i = 1; i < 3; i++) {
			if (vts[i] < inherit->tri_proj[axis][0])
				inherit->tri_proj[axis][0] = vts[i];
			
			if (vts[i] > inherit->tri_proj[axis][1])
				inherit->tri_proj[axis][1] = vts[i];
		}
	}
}

/**
 * Construction
 * from a parent CubeTriangleIsect object and the index of the children
 */
CubeTriangleIsect::CubeTriangleIsect(CubeTriangleIsect *parent)
{
	// Copy inheritable projections
	this->inherit = parent->inherit;

	// Shrink cube projections
	for (int i = 0; i < NUM_AXES; i++) {
		cubeProj[i].origin = parent->cubeProj[i].origin;

		for (int j = 0; j < 3; j++)
			cubeProj[i].edges[j] = parent->cubeProj[i].edges[j] >> 1;
		
		cubeProj[i].min = parent->cubeProj[i].min >> 1;
		cubeProj[i].max = parent->cubeProj[i].max >> 1;
	}
}

unsigned char CubeTriangleIsect::getBoxMask( )
{
	int i, j, k;
	int bmask[3][2] = {{0, 0}, {0, 0}, {0, 0}};
	unsigned char boxmask = 0;
	int64_t child_len = cubeProj[0].edges[0] >> 1;

	for (i = 0; i < 3; i++) {
		int64_t mid = cubeProj[i].origin + child_len;

		// Check bounding box
		if (mid >= inherit->tri_proj[i][0]) {
			bmask[i][0] = 1;
		}
		if (mid < inherit->tri_proj[i][1]) {
			bmask[i][1] = 1;
		}

	}

	// Fill in masks
	int ct = 0;
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			for (k = 0; k < 2; k++) {
				boxmask |= ( (bmask[0][i] & bmask[1][j] & bmask[2][k]) << ct);
				ct++;
			}
		}
	}

	// Return bounding box masks
	return boxmask;
}


/**
 * Shifting a cube to a new origin
 */
void CubeTriangleIsect::shift(int off[3])
{
	for (int i = 0; i < NUM_AXES; i++) {
		cubeProj[i].origin += (off[0] * cubeProj[i].edges[0] +
							   off[1] * cubeProj[i].edges[1] +
							   off[2] * cubeProj[i].edges[2]);
	}
}

/**
 * Method to test intersection of the triangle and the cube
 */
int CubeTriangleIsect::isIntersecting() const
{
	for (int i = 0; i < NUM_AXES; i++) {
		/*
		  int64_t proj0 = cubeProj[i][0] +
		  vertmap[inherit->cubeEnds[i][0]][0] * cubeProj[i][1] +
		  vertmap[inherit->cubeEnds[i][0]][1] * cubeProj[i][2] +
		  vertmap[inherit->cubeEnds[i][0]][2] * cubeProj[i][3] ;
		  int64_t proj1 = cubeProj[i][0] +
		  vertmap[inherit->cubeEnds[i][1]][0] * cubeProj[i][1] +
		  vertmap[inherit->cubeEnds[i][1]][1] * cubeProj[i][2] +
		  vertmap[inherit->cubeEnds[i][1]][2] * cubeProj[i][3] ;
		*/

		int64_t proj0 = cubeProj[i].origin + cubeProj[i].min;
		int64_t proj1 = cubeProj[i].origin + cubeProj[i].max;

		if (proj0 > inherit->tri_proj[i][1] ||
			proj1 < inherit->tri_proj[i][0]) {
			return 0;
		}
	}

	return 1;
}

int CubeTriangleIsect::isIntersectingPrimary(int edgeInd) const
{
	for (int i = 0; i < NUM_AXES; i++) {

		int64_t proj0 = cubeProj[i].origin;
		int64_t proj1 = cubeProj[i].origin + cubeProj[i].edges[edgeInd];

		if (proj0 < proj1) {
			if (proj0 > inherit->tri_proj[i][1] ||
				proj1 < inherit->tri_proj[i][0]) {
				return 0;
			}
		}
		else {
			if (proj1 > inherit->tri_proj[i][1] ||
				proj0 < inherit->tri_proj[i][0]) {
				return 0;
			}
		}

	}

	// printf( "Intersecting: %d %d\n", edgemap[edgeInd][0], edgemap[edgeInd][1] )  ;
	return 1;
}

float CubeTriangleIsect::getIntersectionPrimary(int edgeInd) const
{
	int i = 3;


	int64_t proj0 = cubeProj[i].origin;
	int64_t proj1 = cubeProj[i].origin + cubeProj[i].edges[edgeInd];
	int64_t proj2 = inherit->tri_proj[i][1];
	int64_t d = proj1 - proj0;
	double alpha;

	if (d == 0)
		alpha = 0.5;
	else {
		alpha = (double)((proj2 - proj0)) / (double)d;

		if (alpha < 0 || alpha > 1)
			alpha = 0.5;
	}

	return (float)alpha;
}
