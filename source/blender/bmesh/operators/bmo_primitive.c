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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_primitive.c
 *  \ingroup bmesh
 *
 * Primitive shapes.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "intern/bmesh_operators_private.h"


/* ************************ primitives ******************* */

static const float icovert[12][3] = {
	{0.0f, 0.0f, -200.0f},
	{144.72f, -105.144f, -89.443f},
	{-55.277f, -170.128, -89.443f},
	{-178.885f, 0.0f, -89.443f},
	{-55.277f, 170.128f, -89.443f},
	{144.72f, 105.144f, -89.443f},
	{55.277f, -170.128f, 89.443f},
	{-144.72f, -105.144f, 89.443f},
	{-144.72f, 105.144f, 89.443f},
	{55.277f, 170.128f, 89.443f},
	{178.885f, 0.0f, 89.443f},
	{0.0f, 0.0f, 200.0f}
};

static const short icoface[20][3] = {
	{0, 1, 2},
	{1, 0, 5},
	{0, 2, 3},
	{0, 3, 4},
	{0, 4, 5},
	{1, 5, 10},
	{2, 1, 6},
	{3, 2, 7},
	{4, 3, 8},
	{5, 4, 9},
	{1, 10, 6},
	{2, 6, 7},
	{3, 7, 8},
	{4, 8, 9},
	{5, 9, 10},
	{6, 10, 11},
	{7, 6, 11},
	{8, 7, 11},
	{9, 8, 11},
	{10, 9, 11}
};

static const int monkeyo = 4;
static const int monkeynv = 271;
static const int monkeynf = 250;
static const signed char monkeyv[271][3] = {
	{-71, 21, 98}, {-63, 12, 88}, {-57, 7, 74}, {-82, -3, 79}, {-82, 4, 92},
	{-82, 17, 100}, {-92, 21, 102}, {-101, 12, 95}, {-107, 7, 83},
	{-117, 31, 84}, {-109, 31, 95}, {-96, 31, 102}, {-92, 42, 102},
	{-101, 50, 95}, {-107, 56, 83}, {-82, 66, 79}, {-82, 58, 92},
	{-82, 46, 100}, {-71, 42, 98}, {-63, 50, 88}, {-57, 56, 74},
	{-47, 31, 72}, {-55, 31, 86}, {-67, 31, 97}, {-66, 31, 99},
	{-70, 43, 100}, {-82, 48, 103}, {-93, 43, 105}, {-98, 31, 105},
	{-93, 20, 105}, {-82, 31, 106}, {-82, 15, 103}, {-70, 20, 100},
	{-127, 55, 95}, {-127, 45, 105}, {-127, -87, 94}, {-127, -41, 100},
	{-127, -24, 102}, {-127, -99, 92}, {-127, 52, 77}, {-127, 73, 73},
	{-127, 115, -70}, {-127, 72, -109}, {-127, 9, -106}, {-127, -49, -45},
	{-101, -24, 72}, {-87, -56, 73}, {-82, -89, 73}, {-80, -114, 68},
	{-85, -121, 67}, {-104, -124, 71}, {-127, -126, 74}, {-71, -18, 68},
	{-46, -5, 69}, {-21, 19, 57}, {-17, 55, 76}, {-36, 62, 80},
	{-64, 77, 88}, {-86, 97, 94}, {-107, 92, 97}, {-119, 63, 96},
	{-106, 53, 99}, {-111, 39, 98}, {-101, 12, 95}, {-79, 2, 90},
	{-64, 8, 86}, {-47, 24, 83}, {-45, 38, 83}, {-50, 48, 85},
	{-72, 56, 92}, {-95, 60, 97}, {-127, -98, 94}, {-113, -92, 94},
	{-112, -107, 91}, {-119, -113, 89}, {-127, -114, 88}, {-127, -25, 96},
	{-127, -18, 95}, {-114, -19, 95}, {-111, -29, 96}, {-116, -37, 95},
	{-76, -6, 86}, {-48, 7, 80}, {-34, 26, 77}, {-32, 48, 84},
	{-39, 53, 93}, {-71, 70, 102}, {-87, 82, 107}, {-101, 79, 109},
	{-114, 55, 108}, {-111, -13, 104}, {-100, -57, 91}, {-95, -90, 88},
	{-93, -105, 85}, {-97, -117, 81}, {-106, -119, 81}, {-127, -121, 82},
	{-127, 6, 93}, {-127, 27, 98}, {-85, 61, 95}, {-106, 18, 96},
	{-110, 27, 97}, {-112, -88, 94}, {-117, -57, 96}, {-127, -57, 96},
	{-127, -42, 95}, {-115, -35, 100}, {-110, -29, 102}, {-113, -17, 100},
	{-122, -16, 100}, {-127, -26, 106}, {-121, -19, 104}, {-115, -20, 104},
	{-113, -29, 106}, {-117, -32, 103}, {-127, -37, 103}, {-94, -40, 71},
	{-106, -31, 91}, {-104, -40, 91}, {-97, -32, 71}, {-127, -112, 88},
	{-121, -111, 88}, {-115, -105, 91}, {-115, -95, 93}, {-127, -100, 84},
	{-115, -96, 85}, {-115, -104, 82}, {-121, -109, 81}, {-127, -110, 81},
	{-105, 28, 100}, {-103, 20, 99}, {-84, 55, 97}, {-92, 54, 99},
	{-73, 51, 99}, {-55, 45, 89}, {-52, 37, 88}, {-53, 25, 87},
	{-66, 13, 92}, {-79, 8, 95}, {-98, 14, 100}, {-104, 38, 100},
	{-100, 48, 100}, {-97, 46, 97}, {-102, 38, 97}, {-96, 16, 97},
	{-79, 11, 93}, {-68, 15, 90}, {-57, 27, 86}, {-56, 36, 86},
	{-59, 43, 87}, {-74, 50, 96}, {-91, 51, 98}, {-84, 52, 96},
	{-101, 22, 96}, {-102, 29, 96}, {-113, 59, 78}, {-102, 85, 79},
	{-84, 88, 76}, {-65, 71, 71}, {-40, 58, 63}, {-25, 52, 59},
	{-28, 21, 48}, {-50, 0, 53}, {-71, -12, 60}, {-127, 115, 37},
	{-127, 126, -10}, {-127, -25, -86}, {-127, -59, 24}, {-127, -125, 59},
	{-127, -103, 44}, {-127, -73, 41}, {-127, -62, 36}, {-18, 30, 7},
	{-17, 41, -6}, {-28, 34, -56}, {-68, 56, -90}, {-33, -6, 9},
	{-51, -16, -21}, {-45, -1, -55}, {-84, 7, -85}, {-97, -45, 52},
	{-104, -53, 33}, {-90, -91, 49}, {-95, -64, 50}, {-85, -117, 51},
	{-109, -97, 47}, {-111, -69, 46}, {-106, -121, 56}, {-99, -36, 55},
	{-100, -29, 60}, {-101, -22, 64}, {-100, -50, 21}, {-89, -40, -34},
	{-83, -19, -69}, {-69, 111, -49}, {-69, 119, -9}, {-69, 109, 30},
	{-68, 67, 55}, {-34, 52, 43}, {-46, 58, 36}, {-45, 90, 7},
	{-25, 72, 16}, {-25, 79, -15}, {-45, 96, -25}, {-45, 87, -57},
	{-25, 69, -46}, {-48, 42, -75}, {-65, 3, -70}, {-22, 42, -26},
	{-75, -22, 19}, {-72, -25, -27}, {-13, 52, -30}, {-28, -18, -16},
	{6, -13, -42}, {37, 7, -55}, {46, 41, -54}, {31, 65, -54},
	{4, 61, -40}, {3, 53, -37}, {25, 56, -50}, {35, 37, -52},
	{28, 10, -52}, {5, -5, -39}, {-21, -9, -17}, {-9, 46, -28},
	{-6, 39, -37}, {-14, -3, -27}, {6, 0, -47}, {25, 12, -57},
	{31, 32, -57}, {23, 46, -56}, {4, 44, -46}, {-19, 37, -27},
	{-20, 22, -35}, {-30, 12, -35}, {-22, 11, -35}, {-19, 2, -35},
	{-23, -2, -35}, {-34, 0, -9}, {-35, -3, -22}, {-35, 5, -24},
	{-25, 26, -27}, {-13, 31, -34}, {-13, 30, -41}, {-23, -2, -41},
	{-18, 2, -41}, {-21, 10, -41}, {-29, 12, -41}, {-19, 22, -41},
	{6, 42, -53}, {25, 44, -62}, {34, 31, -63}, {28, 11, -62},
	{7, 0, -54}, {-14, -2, -34}, {-5, 37, -44}, {-13, 14, -42},
	{-7, 8, -43}, {1, 16, -47}, {-4, 22, -45}, {3, 30, -48},
	{8, 24, -49}, {15, 27, -50}, {12, 35, -50}, {4, 56, -62},
	{33, 60, -70}, {48, 38, -64}, {41, 7, -68}, {6, -11, -63},
	{-26, -16, -42}, {-17, 49, -49},
};

static signed char monkeyf[250][4] = {
	{27, 4, 5, 26}, {25, 4, 5, 24}, {3, 6, 5, 4}, {1, 6, 5, 2}, {5, 6, 7, 4},
	{3, 6, 7, 2}, {5, 8, 7, 6}, {3, 8, 7, 4}, {7, 8, 9, 6},
	{5, 8, 9, 4}, {7, 10, 9, 8}, {5, 10, 9, 6}, {9, 10, 11, 8},
	{7, 10, 11, 6}, {9, 12, 11, 10}, {7, 12, 11, 8}, {11, 6, 13, 12},
	{5, 4, 13, 12}, {3, -2, 13, 12}, {-3, -4, 13, 12}, {-5, -10, 13, 12},
	{-11, -12, 14, 12}, {-13, -18, 14, 13}, {-19, 4, 5, 13}, {10, 12, 4, 4},
	{10, 11, 9, 9}, {8, 7, 9, 9}, {7, 5, 6, 6}, {6, 3, 4, 4},
	{5, 1, 2, 2}, {4, -1, 0, 0}, {3, -3, -2, -2}, {22, 67, 68, 23},
	{20, 65, 66, 21}, {18, 63, 64, 19}, {16, 61, 62, 17}, {14, 59, 60, 15},
	{12, 19, 48, 57}, {18, 19, 48, 47}, {18, 19, 48, 47}, {18, 19, 48, 47},
	{18, 19, 48, 47}, {18, 19, 48, 47}, {18, 19, 48, 47}, {18, 19, 48, 47},
	{18, 19, 48, 47}, {18, -9, -8, 47}, {18, 27, 45, 46}, {26, 55, 43, 44},
	{24, 41, 42, 54}, {22, 39, 40, 23}, {20, 37, 38, 21}, {18, 35, 36, 19},
	{16, 33, 34, 17}, {14, 31, 32, 15}, {12, 39, 30, 13}, {11, 48, 45, 38},
	{8, 36, -19, 9}, {8, -20, 44, 47}, {42, 45, 46, 43}, {18, 19, 40, 39},
	{16, 17, 38, 37}, {14, 15, 36, 35}, {32, 44, 43, 33}, {12, 33, 32, 42},
	{19, 44, 43, 42}, {40, 41, 42, -27}, {8, 9, 39, -28}, {15, 43, 42, 16},
	{13, 43, 42, 14}, {11, 43, 42, 12}, {9, -30, 42, 10}, {37, 12, 38, -32},
	{-33, 37, 45, 46}, {-33, 40, 41, 39}, {38, 40, 41, 37}, {36, 40, 41, 35},
	{34, 40, 41, 33}, {36, 39, 38, 37}, {35, 40, 39, 38}, {1, 2, 14, 21},
	{1, 2, 40, 13}, {1, 2, 40, 39}, {1, 24, 12, 39}, {-34, 36, 38, 11},
	{35, 38, 36, 37}, {-37, 8, 35, 37}, {-11, -12, -45, 40}, {-11, -12, 39, 38},
	{-11, -12, 37, 36}, {-11, -12, 35, 34}, {33, 34, 40, 41}, {33, 34, 38, 39},
	{33, 34, 36, 37}, {33, -52, 34, 35}, {33, 37, 36, 34}, {33, 35, 34, 34},
	{8, 7, 37, 36}, {-32, 7, 35, 46}, {-34, -33, 45, 46}, {4, -33, 43, 34},
	{-34, -33, 41, 42}, {-34, -33, 39, 40}, {-34, -33, 37, 38}, {-34, -33, 35, 36},
	{-34, -33, 33, 34}, {-34, -33, 31, 32}, {-34, -4, 28, 30}, {-5, -34, 28, 27},
	{-35, -44, 36, 27}, {26, 35, 36, 45}, {24, 25, 44, 45}, {25, 23, 44, 42},
	{25, 24, 41, 40}, {25, 24, 39, 38}, {25, 24, 37, 36}, {25, 24, 35, 34},
	{25, 24, 33, 32}, {25, 24, 31, 30}, {15, 24, 29, 38}, {25, 24, 27, 26},
	{23, 12, 37, 26}, {11, 12, 35, 36}, {-86, -59, 36, -80}, {-60, -61, 36, 35},
	{-62, -63, 36, 35}, {-64, -65, 36, 35}, {-66, -67, 36, 35}, {-68, -69, 36, 35},
	{-70, -71, 36, 35}, {-72, -73, 36, 35}, {-74, -75, 36, 35}, {42, 43, 53, 58},
	{40, 41, 57, 56}, {38, 39, 55, 57}, {-81, -80, 37, 56}, {-83, -82, 55, 52},
	{-85, -84, 51, 49}, {-87, -86, 48, 49}, {47, 50, 51, 48}, {46, 48, 51, 49},
	{43, 46, 49, 44}, {-92, -91, 45, 42}, {-23, 49, 50, -20}, {-94, 40, 48, -24},
	{-96, -22, 48, 49}, {-97, 48, 21, -90}, {-100, 36, 50, 23}, {22, 49, 48, -100},
	{-101, 47, 46, 22}, {21, 45, 35, 25}, {33, 34, 44, 41}, {13, 14, 28, 24},
	{-107, 26, 30, -106}, {14, 46, 45, 15}, {14, 44, 43, -110}, {-111, 42, 23, -110},
	{6, 7, 45, 46}, {45, 44, 47, 46}, {45, 46, 47, 48}, {47, 46, 49, 48},
	{17, 49, 47, 48}, {17, 36, 46, 48}, {35, 36, 44, 45}, {35, 36, 40, 43},
	{35, 36, 38, 39}, {-4, -3, 37, 35}, {-123, 34, 33, 1}, {-9, -8, -7, -6},
	{-10, -7, 32, -125}, {-127, -11, -126, -126}, {-7, -6, 5, 31}, {4, 5, 33, 30},
	{4, 39, 33, 32}, {4, 35, 32, 38}, {20, 21, 39, 38}, {4, 37, 38, 5},
	{-11, -10, 36, 3}, {-11, 15, 14, 35}, {13, 16, 34, 34}, {-13, 14, 13, 13},
	{-3, 1, 30, 29}, {-3, 28, 29, 1}, {-2, 31, 28, -1}, {12, 13, 27, 30},
	{-2, 26, 12, 12}, {35, 29, 42, 36}, {34, 35, 36, 33}, {32, 35, 36, 31},
	{30, 35, 36, 29}, {28, 35, 36, 27}, {26, 35, 36, 25}, {34, 39, 38, 35},
	{32, 39, 38, 33}, {30, 39, 38, 31}, {28, 39, 38, 29}, {26, 39, 38, 27},
	{25, 31, 32, 38}, {-18, -17, 45, 44}, {-18, 17, 28, 44}, {-24, -20, 42, -23},
	{11, 35, 27, 14}, {25, 28, 39, 41}, {37, 41, 40, 38}, {34, 40, 36, 35},
	{32, 40, 39, 33}, {30, 39, 31, 40}, {21, 29, 39, 22}, {-31, 37, 28, 4},
	{-32, 33, 35, 36}, {32, 33, 34, 34}, {18, 35, 36, 48}, {34, 25, 40, 35},
	{24, 25, 38, 39}, {24, 25, 36, 37}, {24, 25, 34, 35}, {24, 25, 32, 33},
	{24, 13, 41, 31}, {17, 11, 41, 35}, {15, 16, 34, 35}, {13, 14, 34, 35},
	{11, 12, 34, 35}, {9, 10, 34, 35}, {7, 8, 34, 35}, {26, 25, 37, 36},
	{35, 36, 37, 38}, {37, 36, 39, 38}, {37, 38, 39, 40}, {25, 31, 36, 39},
	{18, 34, 35, 30}, {17, 22, 30, 33}, {19, 29, 21, 20}, {16, 26, 29, 17},
	{24, 29, 28, 25}, {22, 31, 28, 23}, {20, 31, 30, 21}, {18, 31, 30, 19},
	{16, 30, 17, 17}, {-21, -22, 35, 34}, {-21, -22, 33, 32}, {-21, -22, 31, 30},
	{-21, -22, 29, 28}, {-21, -22, 27, 26}, {-28, -22, 25, 31}, {24, 28, 29, 30},
	{23, 24, 26, 27}, {23, 24, 25, 25}, {-69, -35, -32, 27}, {-70, 26, 25, -66},
	{-68, -67, 24, -33},
};

#define VERT_MARK   1

#define EDGE_ORIG   1
#define EDGE_MARK   2

#define FACE_MARK   1
#define FACE_NEW    2

void bmo_create_grid_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *slot_verts_out = BMO_slot_get(op->slots_out, "verts.out");

	const float dia = BMO_slot_float_get(op->slots_in, "size");
	const unsigned int xtot = max_ii(2, BMO_slot_int_get(op->slots_in, "x_segments"));
	const unsigned int ytot = max_ii(2, BMO_slot_int_get(op->slots_in, "y_segments"));
	const float xtot_inv2 = 2.0f / (xtot - 1);
	const float ytot_inv2 = 2.0f / (ytot - 1);
	const bool calc_uvs = BMO_slot_bool_get(op->slots_in, "calc_uvs");

	BMVert **varr;
	BMVert *vquad[4];

	float mat[4][4];
	float vec[3], tvec[3];

	unsigned int x, y, i;


	BMO_slot_mat4_get(op->slots_in, "matrix", mat);

	BMO_slot_buffer_alloc(op, op->slots_out, "verts.out", xtot * ytot);
	varr = (BMVert **)slot_verts_out->data.buf;

	i = 0;
	vec[2] = 0.0f;
	for (y = 0; y < ytot; y++) {
		vec[1] = ((y * ytot_inv2) - 1.0f) * dia;
		for (x = 0; x < xtot; x++) {
			vec[0] = ((x * xtot_inv2) - 1.0f) * dia;
			mul_v3_m4v3(tvec, mat, vec);
			varr[i] = BM_vert_create(bm, tvec, NULL, BM_CREATE_NOP);
			BMO_vert_flag_enable(bm, varr[i], VERT_MARK);
			i++;
		}
	}

#define XY(_x, _y)  ((_x) + ((_y) * (xtot)))

	for (y = 1; y < ytot; y++) {
		for (x = 1; x < xtot; x++) {
			BMFace *f;

			vquad[0] = varr[XY(x - 1, y - 1)];
			vquad[1] = varr[XY(x,     y - 1)];
			vquad[2] = varr[XY(x,         y)];
			vquad[3] = varr[XY(x - 1,     y)];

			f = BM_face_create_verts(bm, vquad, 4, NULL, BM_CREATE_NOP, true);
			if (calc_uvs) {
				BMO_face_flag_enable(bm, f, FACE_MARK);
			}
		}
	}

#undef XY

	if (calc_uvs) {
		BM_mesh_calc_uvs_grid(bm, xtot, ytot, FACE_MARK);
	}
}

/**
 * Fills first available UVmap with grid-like UVs for all faces OpFlag-ged by given flag.
 *
 * \param bm The BMesh to operate on
 * \param x_segments The x-resolution of the grid
 * \param y_segments The y-resolution of the grid
 * \param oflag The flag to check faces with.
 */
void BM_mesh_calc_uvs_grid(BMesh *bm, const unsigned int x_segments, const unsigned int y_segments, const short oflag)
{
	BMFace *f;
	BMLoop *l;
	BMIter iter, liter;

	const float dx = 1.0f / (float)(x_segments - 1);
	const float dy = 1.0f / (float)(y_segments - 1);
	float x = 0.0f;
	float y = 0.0f;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	int loop_index;

	BLI_assert(cd_loop_uv_offset != -1);

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (!BMO_face_flag_test(bm, f, oflag))
			continue;

		BM_ITER_ELEM_INDEX (l, &liter, f, BM_LOOPS_OF_FACE, loop_index) {
			MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

			switch (loop_index) {
				case 0:
					x += dx;
					break;
				case 1:
					y += dy;
					break;
				case 2:
					x -= dx;
					break;
				case 3:
					y -= dy;
					break;
				default:
					break;
			}

			luv->uv[0] = x;
			luv->uv[1] = y;
		}

		x += dx;
		if (x >= 1.0f) {
			x = 0.0f;
			y += dy;
		}
	}
}

void bmo_create_uvsphere_exec(BMesh *bm, BMOperator *op)
{
	const float dia = BMO_slot_float_get(op->slots_in, "diameter");
	const int seg = BMO_slot_int_get(op->slots_in, "u_segments");
	const int tot = BMO_slot_int_get(op->slots_in, "v_segments");
	const bool calc_uvs = BMO_slot_bool_get(op->slots_in, "calc_uvs");

	BMOperator bmop, prevop;
	BMVert *eve, *preveve;
	BMEdge *e;
	BMIter iter;
	const float axis[3] = {0, 0, 1};
	float vec[3], mat[4][4], cmat[3][3];
	float phi, phid;
	int a;

	BMO_slot_mat4_get(op->slots_in, "matrix", mat);

	phid = 2.0f * (float)M_PI / tot;
	/* phi = 0.25f * (float)M_PI; */ /* UNUSED */

	/* one segment first */
	phi = 0;
	phid /= 2;
	for (a = 0; a <= tot; a++) {
		/* Going in this direction, then edge extruding, makes normals face outward */
		vec[0] = 0.0;
		vec[1] = dia * sinf(phi);
		vec[2] = dia * cosf(phi);
		eve = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);
		BMO_vert_flag_enable(bm, eve, VERT_MARK);

		if (a != 0) {
			e = BM_edge_create(bm, preveve, eve, NULL, BM_CREATE_NOP);
			BMO_edge_flag_enable(bm, e, EDGE_ORIG);
		}

		phi += phid;
		preveve = eve;
	}

	/* extrude and rotate; negative phi to make normals face outward */
	axis_angle_to_mat3(cmat, axis, -(M_PI * 2) / seg);

	for (a = 0; a < seg; a++) {
		if (a) {
			BMO_op_initf(bm, &bmop, op->flag, "extrude_edge_only edges=%S", &prevop, "geom.out");
			BMO_op_exec(bm, &bmop);
			BMO_op_finish(bm, &prevop);
		}
		else {
			BMO_op_initf(bm, &bmop, op->flag, "extrude_edge_only edges=%fe", EDGE_ORIG);
			BMO_op_exec(bm, &bmop);
		}

		BMO_slot_buffer_flag_enable(bm, bmop.slots_out, "geom.out", BM_VERT, VERT_MARK);
		BMO_op_callf(bm, op->flag, "rotate cent=%v matrix=%m3 verts=%S", vec, cmat, &bmop, "geom.out");
		
		prevop = bmop;
	}

	if (a)
		BMO_op_finish(bm, &bmop);

	{
		float len, len2, vec2[3];

		len = 2 *dia * sinf(phid / 2.0f);

		/* length of one segment in shortest parallen */
		vec[0] = dia * sinf(phid);
		vec[1] = 0.0f;
		vec[2] = dia * cosf(phid);

		mul_v3_m3v3(vec2, cmat, vec);
		len2 = len_v3v3(vec, vec2);

		/* use shortest segment length divided by 3 as merge threshold */
		BMO_op_callf(bm, op->flag, "remove_doubles verts=%fv dist=%f", VERT_MARK, min_ff(len, len2) / 3.0f);
	}

	if (calc_uvs) {
		BMFace *f;
		BMLoop *l;
		BMIter fiter, liter;

		/* We cannot tag faces for UVs computing above, so we have to do it now, based on all its vertices
		 * being tagged. */
		BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
			bool valid = true;

			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				if (!BMO_vert_flag_test(bm, l->v, VERT_MARK)) {
					valid = false;
					break;
				}
			}

			if (valid) {
				BMO_face_flag_enable(bm, f, FACE_MARK);
			}
		}

		BM_mesh_calc_uvs_sphere(bm, FACE_MARK);
	}

	/* and now do imat */
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_vert_flag_test(bm, eve, VERT_MARK)) {
			mul_m4_v3(mat, eve->co);
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, VERT_MARK);
}

void bmo_create_icosphere_exec(BMesh *bm, BMOperator *op)
{
	const float dia = BMO_slot_float_get(op->slots_in, "diameter");
	const float dia_div = dia / 200.0f;
	const int subdiv = BMO_slot_int_get(op->slots_in, "subdivisions");
	const bool calc_uvs = BMO_slot_bool_get(op->slots_in, "calc_uvs");

	BMVert *eva[12];
	BMVert *v;
	BMIter liter;
	BMIter viter;
	BMLoop *l;
	float vec[3], mat[4][4] /* , phi, phid */;
	int a;

	BMO_slot_mat4_get(op->slots_in, "matrix", mat);

	/* phid = 2.0f * (float)M_PI / subdiv; */ /* UNUSED */
	/* phi = 0.25f * (float)M_PI; */         /* UNUSED */


	for (a = 0; a < 12; a++) {
		vec[0] = dia_div * icovert[a][0];
		vec[1] = dia_div * icovert[a][1];
		vec[2] = dia_div * icovert[a][2];
		eva[a] = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);

		BMO_vert_flag_enable(bm, eva[a], VERT_MARK);
	}

	for (a = 0; a < 20; a++) {
		BMFace *eftemp;
		BMVert *v1, *v2, *v3;

		v1 = eva[icoface[a][0]];
		v2 = eva[icoface[a][1]];
		v3 = eva[icoface[a][2]];

		eftemp = BM_face_create_quad_tri(bm, v1, v2, v3, NULL, NULL, BM_CREATE_NOP);
		
		BM_ITER_ELEM (l, &liter, eftemp, BM_LOOPS_OF_FACE) {
			BMO_edge_flag_enable(bm, l->e, EDGE_MARK);
		}

		BMO_face_flag_enable(bm, eftemp, FACE_MARK);
	}

	if (subdiv > 1) {
		BMOperator bmop;

		BMO_op_initf(bm, &bmop, op->flag,
		             "subdivide_edges edges=%fe "
		             "smooth=%f "
		             "cuts=%i "
		             "use_grid_fill=%b use_sphere=%b",
		             EDGE_MARK, dia, (1 << (subdiv - 1)) - 1,
		             true, true);

		BMO_op_exec(bm, &bmop);
		BMO_slot_buffer_flag_enable(bm, bmop.slots_out, "geom.out", BM_VERT, VERT_MARK);
		BMO_slot_buffer_flag_enable(bm, bmop.slots_out, "geom.out", BM_EDGE, EDGE_MARK);
		BMO_op_finish(bm, &bmop);
	}

	if (calc_uvs) {
		BMFace *f;
		BMIter fiter;

		/* We cannot tag faces for UVs computing above, so we have to do it now, based on all its vertices
		 * being tagged. */
		BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
			bool valid = true;

			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				if (!BMO_vert_flag_test(bm, l->v, VERT_MARK)) {
					valid = false;
					break;
				}
			}

			if (valid) {
				BMO_face_flag_enable(bm, f, FACE_MARK);
			}
		}

		BM_mesh_calc_uvs_sphere(bm, FACE_MARK);
	}

	/* must transform after because of sphere subdivision */
	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		if (BMO_vert_flag_test(bm, v, VERT_MARK)) {
			mul_m4_v3(mat, v->co);
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, VERT_MARK);
}

static void bm_mesh_calc_uvs_sphere_face(BMFace *f, float mat_rot[3][3], const int cd_loop_uv_offset)
{
	float *uvs[4];
	BMLoop *l;
	BMIter iter;
	float dx;
	int loop_index, loop_index_max_x;

	BLI_assert(f->len <= 4);

	BM_ITER_ELEM_INDEX (l, &iter, f, BM_LOOPS_OF_FACE, loop_index) {
		MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		float vco[3];

		mul_v3_m3v3(vco, mat_rot, l->v->co);
		map_to_sphere(&luv->uv[0], &luv->uv[1], vco[0], vco[1], vco[2]);

		uvs[loop_index] = luv->uv;
	}

	/* Fix awkwardly-wrapping UVs */
	loop_index_max_x = 0;
	for (loop_index = 1; loop_index < f->len; loop_index++) {
		if (uvs[loop_index][0] > uvs[loop_index_max_x][0]) {
			loop_index_max_x = loop_index;
		}
	}

	for (loop_index = 0; loop_index < f->len; loop_index++) {
		if (loop_index != loop_index_max_x) {
			dx = uvs[loop_index_max_x][0] - uvs[loop_index][0];
			if (dx > 0.5f) {
				uvs[loop_index][0] += 1.0f;
			}
		}
	}
}

/**
 * Fills first available UVmap with spherical projected UVs for all faces OpFlag-ged by given flag.
 *
 * \param bm The BMesh to operate on
 * \param oflag The flag to check faces with.
 */
void BM_mesh_calc_uvs_sphere(BMesh *bm, const short oflag)
{
	BMFace *f;
	BMIter iter;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	/* We apply a 'magic' rotationto vcos before mapping them to sphere,
	 * those values seem to give best results for both ico and uv sphere projections. */
	float mat_rot[3][3];
	const float axis[3] = {0.806f, 0.329f, 0.491f};
	const float angle = DEG2RADF(120.0f);

	axis_angle_to_mat3(mat_rot, axis, angle);

	BLI_assert(cd_loop_uv_offset != -1); /* caller is responsible for giving us UVs */

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (!BMO_face_flag_test(bm, f, oflag))
			continue;

		bm_mesh_calc_uvs_sphere_face(f, mat_rot, cd_loop_uv_offset);
	}
}

void bmo_create_monkey_exec(BMesh *bm, BMOperator *op)
{
	BMVert **tv = MEM_mallocN(sizeof(*tv) * monkeynv * 2, "tv");
	float mat[4][4];
	int i;

	BMO_slot_mat4_get(op->slots_in, "matrix", mat);

	for (i = 0; i < monkeynv; i++) {
		float v[3];

		/* rotate to face in the -Y axis */
		v[0] = (monkeyv[i][0] + 127) / 128.0;
		v[2] = monkeyv[i][1] / 128.0;
		v[1] = monkeyv[i][2] / -128.0;

		tv[i] = BM_vert_create(bm, v, NULL, BM_CREATE_NOP);
		BMO_vert_flag_enable(bm, tv[i], VERT_MARK);

		if (fabsf(v[0] = -v[0]) < 0.001f) {
			tv[monkeynv + i] = tv[i];
		}
		else {
			BMVert *eve = BM_vert_create(bm, v, NULL, BM_CREATE_NOP);
			mul_m4_v3(mat, eve->co);
			tv[monkeynv + i] = eve;
		}

		BMO_vert_flag_enable(bm, tv[monkeynv + i], VERT_MARK);

		mul_m4_v3(mat, tv[i]->co);
	}

	for (i = 0; i < monkeynf; i++) {
		BM_face_create_quad_tri(bm,
		                        tv[monkeyf[i][0] + i - monkeyo],
		                        tv[monkeyf[i][1] + i - monkeyo],
		                        tv[monkeyf[i][2] + i - monkeyo],
		                        (monkeyf[i][3] != monkeyf[i][2]) ? tv[monkeyf[i][3] + i - monkeyo] : NULL,
		                        NULL, BM_CREATE_NOP);

		BM_face_create_quad_tri(bm,
		                        tv[monkeynv + monkeyf[i][2] + i - monkeyo],
		                        tv[monkeynv + monkeyf[i][1] + i - monkeyo],
		                        tv[monkeynv + monkeyf[i][0] + i - monkeyo],
		                        (monkeyf[i][3] != monkeyf[i][2]) ? tv[monkeynv + monkeyf[i][3] + i - monkeyo] : NULL,
		                        NULL, BM_CREATE_NOP);
	}

	MEM_freeN(tv);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, VERT_MARK);
}


void bmo_create_circle_exec(BMesh *bm, BMOperator *op)
{
	const float dia = BMO_slot_float_get(op->slots_in, "diameter");
	const int segs = BMO_slot_int_get(op->slots_in, "segments");
	const bool cap_ends = BMO_slot_bool_get(op->slots_in, "cap_ends");
	const bool cap_tris = BMO_slot_bool_get(op->slots_in, "cap_tris");
	const bool calc_uvs = BMO_slot_bool_get(op->slots_in, "calc_uvs");

	BMVert *v1, *lastv1 = NULL, *cent1, *firstv1 = NULL;
	float vec[3], mat[4][4], phi, phid;
	int a;
	
	if (!segs)
		return;
	
	BMO_slot_mat4_get(op->slots_in, "matrix", mat);

	phid = 2.0f * (float)M_PI / segs;
	phi = 0;

	if (cap_ends) {
		zero_v3(vec);
		mul_m4_v3(mat, vec);
		
		cent1 = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);
		BMO_vert_flag_enable(bm, cent1, VERT_MARK);
	}

	for (a = 0; a < segs; a++, phi += phid) {
		/* Going this way ends up with normal(s) upward */
		vec[0] = -dia * sinf(phi);
		vec[1] = dia * cosf(phi);
		vec[2] = 0.0f;
		mul_m4_v3(mat, vec);
		v1 = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);

		BMO_vert_flag_enable(bm, v1, VERT_MARK);
		
		if (lastv1)
			BM_edge_create(bm, v1, lastv1, NULL, BM_CREATE_NOP);
		
		if (a && cap_ends) {
			BMFace *f;
			
			f = BM_face_create_quad_tri(bm, cent1, lastv1, v1, NULL, NULL, BM_CREATE_NOP);
			BMO_face_flag_enable(bm, f, FACE_NEW);
		}
		
		if (!firstv1)
			firstv1 = v1;

		lastv1 = v1;
	}

	if (!a)
		return;

	BM_edge_create(bm, firstv1, lastv1, NULL, 0);

	if (cap_ends) {
		BMFace *f;
		
		f = BM_face_create_quad_tri(bm, cent1, v1, firstv1, NULL, NULL, BM_CREATE_NOP);
		BMO_face_flag_enable(bm, f, FACE_NEW);

		if (calc_uvs) {
			BM_mesh_calc_uvs_circle(bm, mat, dia, FACE_NEW);
		}
	}
	
	if (!cap_tris) {
		BMO_op_callf(bm, op->flag, "dissolve_faces faces=%ff", FACE_NEW);
	}
	
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, VERT_MARK);
}

/**
 * Fills first available UVmap with 2D projected UVs for all faces OpFlag-ged by given flag.
 *
 * \param bm The BMesh to operate on.
 * \param mat The transform matrix applied to the created circle.
 * \param radius The size of the circle.
 * \param oflag The flag to check faces with.
 */
void BM_mesh_calc_uvs_circle(BMesh *bm, float mat[4][4], const float radius, const short oflag)
{
	BMFace *f;
	BMLoop *l;
	BMIter fiter, liter;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	const float uv_scale = 0.5f / radius;
	const float uv_center = 0.5f;

	float inv_mat[4][4];

	BLI_assert(cd_loop_uv_offset != -1);  /* caller must ensure we have UVs already */

	invert_m4_m4(inv_mat, mat);

	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		if (!BMO_face_flag_test(bm, f, oflag))
			continue;

		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

			float uv_vco[3];
			copy_v3_v3(uv_vco, l->v->co);
			/* transform back into the unit circle flat on the Z-axis */
			mul_m4_v3(inv_mat, uv_vco);

			/* then just take those coords for UVs */
			luv->uv[0] = uv_center + uv_scale * uv_vco[0];
			luv->uv[1] = uv_center + uv_scale * uv_vco[1];
		}
	}
}

void bmo_create_cone_exec(BMesh *bm, BMOperator *op)
{
	BMVert *v1, *v2, *lastv1 = NULL, *lastv2 = NULL, *cent1, *cent2, *firstv1, *firstv2;
	BMFace *f;
	float vec[3], mat[4][4], phi, phid;
	float dia1 = BMO_slot_float_get(op->slots_in, "diameter1");
	float dia2 = BMO_slot_float_get(op->slots_in, "diameter2");
	float depth = BMO_slot_float_get(op->slots_in, "depth");
	int segs = BMO_slot_int_get(op->slots_in, "segments");
	const bool cap_ends = BMO_slot_bool_get(op->slots_in, "cap_ends");
	const bool cap_tris = BMO_slot_bool_get(op->slots_in, "cap_tris");
	const bool calc_uvs = BMO_slot_bool_get(op->slots_in, "calc_uvs");
	int a;
	
	if (!segs)
		return;
	
	BMO_slot_mat4_get(op->slots_in, "matrix", mat);

	phid = 2.0f * (float)M_PI / segs;
	phi = 0;

	depth *= 0.5f;
	if (cap_ends) {
		vec[0] = vec[1] = 0.0f;
		vec[2] = -depth;
		mul_m4_v3(mat, vec);
		
		cent1 = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);

		vec[0] = vec[1] = 0.0f;
		vec[2] = depth;
		mul_m4_v3(mat, vec);
		
		cent2 = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);

		BMO_vert_flag_enable(bm, cent1, VERT_MARK);
		BMO_vert_flag_enable(bm, cent2, VERT_MARK);
	}

	for (a = 0; a < segs; a++, phi += phid) {
		vec[0] = dia1 * sinf(phi);
		vec[1] = dia1 * cosf(phi);
		vec[2] = -depth;
		mul_m4_v3(mat, vec);
		v1 = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);

		vec[0] = dia2 * sinf(phi);
		vec[1] = dia2 * cosf(phi);
		vec[2] = depth;
		mul_m4_v3(mat, vec);
		v2 = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);

		BMO_vert_flag_enable(bm, v1, VERT_MARK);
		BMO_vert_flag_enable(bm, v2, VERT_MARK);

		if (a) {
			if (cap_ends) {
				f = BM_face_create_quad_tri(bm, cent1, lastv1, v1, NULL, NULL, BM_CREATE_NOP);
				if (calc_uvs) {
					BMO_face_flag_enable(bm, f, FACE_MARK);
				}
				BMO_face_flag_enable(bm, f, FACE_NEW);

				f = BM_face_create_quad_tri(bm, cent2, v2, lastv2, NULL, NULL, BM_CREATE_NOP);
				if (calc_uvs) {
					BMO_face_flag_enable(bm, f, FACE_MARK);
				}
				BMO_face_flag_enable(bm, f, FACE_NEW);
			}

			f = BM_face_create_quad_tri(bm, lastv1, lastv2, v2, v1, NULL, BM_CREATE_NOP);
			if (calc_uvs) {
				BMO_face_flag_enable(bm, f, FACE_MARK);
			}
		}
		else {
			firstv1 = v1;
			firstv2 = v2;
		}

		lastv1 = v1;
		lastv2 = v2;
	}

	if (!a)
		return;

	if (cap_ends) {
		f = BM_face_create_quad_tri(bm, cent1, v1, firstv1, NULL, NULL, BM_CREATE_NOP);
		if (calc_uvs) {
			BMO_face_flag_enable(bm, f, FACE_MARK);
		}
		BMO_face_flag_enable(bm, f, FACE_NEW);

		f = BM_face_create_quad_tri(bm, cent2, firstv2, v2, NULL, NULL, BM_CREATE_NOP);
		if (calc_uvs) {
			BMO_face_flag_enable(bm, f, FACE_MARK);
		}
		BMO_face_flag_enable(bm, f, FACE_NEW);
	}

	f = BM_face_create_quad_tri(bm, v1, v2, firstv2, firstv1, NULL, BM_CREATE_NOP);
	if (calc_uvs) {
		BMO_face_flag_enable(bm, f, FACE_MARK);
	}

	if (calc_uvs) {
		BM_mesh_calc_uvs_cone(bm, mat, dia2, dia1, segs, cap_ends, FACE_MARK);
	}

	if (!cap_tris) {
		BMO_op_callf(bm, op->flag, "dissolve_faces faces=%ff", FACE_NEW);
	}
	
	BMO_op_callf(bm, op->flag, "remove_doubles verts=%fv dist=%f", VERT_MARK, 0.000001);
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, VERT_MARK);
}

/**
 * Fills first available UVmap with cylinder/cone-like UVs for all faces OpFlag-ged by given flag.
 *
 * \param bm The BMesh to operate on.
 * \param mat The transform matrix applied to the created cone/cylinder.
 * \param radius_top The size of the top end of the cone/cylinder.
 * \param radius_bottom The size of the bottom end of the cone/cylinder.
 * \param segments The number of subdivisions in the sides of the cone/cylinder.
 * \param cap_ends Whether the ends of the cone/cylinder are filled or not.
 * \param oflag The flag to check faces with.
 */
void BM_mesh_calc_uvs_cone(
        BMesh *bm, float mat[4][4],
        const float radius_top, const float radius_bottom, const int segments, const bool cap_ends, const short oflag)
{
	BMFace *f;
	BMLoop *l;
	BMIter fiter, liter;
	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	const float uv_width = 1.0f / (float)segments;
	const float uv_height = cap_ends ? 0.5f : 1.0f;

	/* Note that all this allows us to handle all cases (real cone, truncated cone, with or without ends capped)
	 * with a single common code. */
	const float uv_center_y = cap_ends ? 0.25f : 0.5f;
	const float uv_center_x_top = cap_ends ? 0.25f : 0.5f;
	const float uv_center_x_bottom = cap_ends ? 0.75f : 0.5f;
	const float uv_radius = cap_ends ? 0.24f : 0.5f;

	/* Using the opposite's end uv_scale as fallback allows us to handle 'real cone' case. */
	const float uv_scale_top = (radius_top != 0.0f) ? (uv_radius / radius_top) :
	                                                  ((radius_bottom != 0.0f) ? (uv_radius / radius_bottom) : uv_radius);
	const float uv_scale_bottom = (radius_bottom != 0.0f) ? (uv_radius / radius_bottom) :
	                                                        uv_scale_top;

	float local_up[3] = {0.0f, 0.0f, 1.0f};

	float x, y;
	float inv_mat[4][4];
	int loop_index;

	mul_mat3_m4_v3(mat, local_up);  /* transform the upvector like we did the cone itself, without location. */
	normalize_v3(local_up);  /* remove global scaling... */

	invert_m4_m4(inv_mat, mat);

	BLI_assert(cd_loop_uv_offset != -1); /* caller is responsible for ensuring the mesh has UVs */

	x = 0.0f;
	y = 1.0f - uv_height;

	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		if (!BMO_face_flag_test(bm, f, oflag))
			continue;

		if (f->len == 4 && radius_top && radius_bottom) {
			/* side face - so unwrap it in a rectangle */
			BM_ITER_ELEM_INDEX (l, &liter, f, BM_LOOPS_OF_FACE, loop_index) {
				MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				switch (loop_index) {
					case 0:
						x += uv_width;
						break;
					case 1:
						y += uv_height;
						break;
					case 2:
						x -= uv_width;
						break;
					case 3:
						y -= uv_height;
						break;
					default:
						break;
				}

				luv->uv[0] = x;
				luv->uv[1] = y;
			}

			x += uv_width;
		}
		else {
			/* top or bottom face - so unwrap it by transforming back to a circle and using the X/Y coords */
			BM_face_normal_update(f);

			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				float uv_vco[3];

				mul_v3_m4v3(uv_vco, inv_mat, l->v->co);

				if (dot_v3v3(f->no, local_up) > 0.0f) { /* if this is a top face of the cone */
					luv->uv[0] = uv_center_x_top + uv_vco[0] * uv_scale_top;
					luv->uv[1] = uv_center_y + uv_vco[1] * uv_scale_top;
				}
				else {
					luv->uv[0] = uv_center_x_bottom + uv_vco[0] * uv_scale_bottom;
					luv->uv[1] = uv_center_y + uv_vco[1] * uv_scale_bottom;
				}
			}
		}
	}
}

void bmo_create_cube_exec(BMesh *bm, BMOperator *op)
{
	BMVert *verts[8];
	float mat[4][4];
	float off = BMO_slot_float_get(op->slots_in, "size") / 2.0f;
	const bool calc_uvs = BMO_slot_bool_get(op->slots_in, "calc_uvs");
	int i, x, y, z;
	/* rotation order set to match 'BM_mesh_calc_uvs_cube' */
	const char faces[6][4] = {
		{0, 1, 3, 2},
		{2, 3, 7, 6},
		{6, 7, 5, 4},
		{4, 5, 1, 0},
		{2, 6, 4, 0},
		{7, 3, 1, 5},
	};

	BMO_slot_mat4_get(op->slots_in, "matrix", mat);

	if (!off) off = 0.5f;
	i = 0;

	for (x = -1; x < 2; x += 2) {
		for (y = -1; y < 2; y += 2) {
			for (z = -1; z < 2; z += 2) {
				float vec[3] = {(float)x * off, (float)y * off, (float)z * off};
				mul_m4_v3(mat, vec);
				verts[i] = BM_vert_create(bm, vec, NULL, BM_CREATE_NOP);
				BMO_vert_flag_enable(bm, verts[i], VERT_MARK);
				i++;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(faces); i++) {
		BMFace *f;
		BMVert *quad[4] = {
		    verts[faces[i][0]],
		    verts[faces[i][1]],
		    verts[faces[i][2]],
		    verts[faces[i][3]],
		};

		f = BM_face_create_verts(bm, quad, 4, NULL, BM_CREATE_NOP, true);
		if (calc_uvs) {
			BMO_face_flag_enable(bm, f, FACE_MARK);
		}
	}

	if (calc_uvs) {
		BM_mesh_calc_uvs_cube(bm, FACE_MARK);
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, VERT_MARK);
}

/**
 * Fills first available UVmap with cube-like UVs for all faces OpFlag-ged by given flag.
 *
 * \note Expects tagged faces to be six quads.
 * \note Caller must order faces for correct alignment.
 *
 * \param bm The BMesh to operate on.
 * \param oflag The flag to check faces with.
 */
void BM_mesh_calc_uvs_cube(BMesh *bm, const short oflag)
{
	BMFace *f;
	BMLoop *l;
	BMIter fiter, liter;
	const float width = 0.25f;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	float x = 0.375f;
	float y = 0.0f;

	int loop_index;

	BLI_assert(cd_loop_uv_offset != -1); /* the caller can ensure that we have UVs */

	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		if (!BMO_face_flag_test(bm, f, oflag)) {
			continue;
		}

		BM_ITER_ELEM_INDEX (l, &liter, f, BM_LOOPS_OF_FACE, loop_index) {
			MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

			luv->uv[0] = x;
			luv->uv[1] = y;

			switch (loop_index) {
				case 0:
					x += width;
					break;
				case 1:
					y += width;
					break;
				case 2:
					x -= width;
					break;
				case 3:
					y -= width;
					break;
				default:
					break;
			}
		}

		if (y >= 0.75f && x > 0.125f) {
			x = 0.125f;
			y = 0.5f;
		}
		else if (x <= 0.125f) {
			x = 0.625f;
			y = 0.5f;
		}
		else {
			y += 0.25f;
		}
	}
}
