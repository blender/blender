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

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "ED_mesh.h"

#include "bmesh.h"
#include "bmesh_private.h"



/* ************************ primitives ******************* */

static float icovert[12][3] = {
	{0.0f,0.0f,-200.0f},
	{144.72f, -105.144f,-89.443f},
	{-55.277f, -170.128,-89.443f},
	{-178.885f,0.0f,-89.443f},
	{-55.277f,170.128f,-89.443f},
	{144.72f,105.144f,-89.443f},
	{55.277f,-170.128f,89.443f},
	{-144.72f,-105.144f,89.443f},
	{-144.72f,105.144f,89.443f},
	{55.277f,170.128f,89.443f},
	{178.885f,0.0f,89.443f},
	{0.0f,0.0f,200.0f}
};

static short icoface[20][3] = {
	{0,1,2},
	{1,0,5},
	{0,2,3},
	{0,3,4},
	{0,4,5},
	{1,5,10},
	{2,1,6},
	{3,2,7},
	{4,3,8},
	{5,4,9},
	{1,10,6},
	{2,6,7},
	{3,7,8},
	{4,8,9},
	{5,9,10},
	{6,10,11},
	{7,6,11},
	{8,7,11},
	{9,8,11},
	{10,9,11}
};

// HACK: these can also be found in cmoview.tga.c, but are here so that they can be found by linker
// this hack is only used so that scons & mingw + split-sources hack works
// ------------------------------- start copied code
/* these are not the monkeys you are looking for */
static int monkeyo = 4;
static int monkeynv = 271;
static int monkeynf = 250;
static signed char monkeyv[271][3] = {
	{-71,21,98},{-63,12,88},{-57,7,74},{-82,-3,79},{-82,4,92},
	{-82,17,100},{-92,21,102},{-101,12,95},{-107,7,83},
	{-117,31,84},{-109,31,95},{-96,31,102},{-92,42,102},
	{-101,50,95},{-107,56,83},{-82,66,79},{-82,58,92},
	{-82,46,100},{-71,42,98},{-63,50,88},{-57,56,74},
	{-47,31,72},{-55,31,86},{-67,31,97},{-66,31,99},
	{-70,43,100},{-82,48,103},{-93,43,105},{-98,31,105},
	{-93,20,105},{-82,31,106},{-82,15,103},{-70,20,100},
	{-127,55,95},{-127,45,105},{-127,-87,94},{-127,-41,100},
	{-127,-24,102},{-127,-99,92},{-127,52,77},{-127,73,73},
	{-127,115,-70},{-127,72,-109},{-127,9,-106},{-127,-49,-45},
	{-101,-24,72},{-87,-56,73},{-82,-89,73},{-80,-114,68},
	{-85,-121,67},{-104,-124,71},{-127,-126,74},{-71,-18,68},
	{-46,-5,69},{-21,19,57},{-17,55,76},{-36,62,80},
	{-64,77,88},{-86,97,94},{-107,92,97},{-119,63,96},
	{-106,53,99},{-111,39,98},{-101,12,95},{-79,2,90},
	{-64,8,86},{-47,24,83},{-45,38,83},{-50,48,85},
	{-72,56,92},{-95,60,97},{-127,-98,94},{-113,-92,94},
	{-112,-107,91},{-119,-113,89},{-127,-114,88},{-127,-25,96},
	{-127,-18,95},{-114,-19,95},{-111,-29,96},{-116,-37,95},
	{-76,-6,86},{-48,7,80},{-34,26,77},{-32,48,84},
	{-39,53,93},{-71,70,102},{-87,82,107},{-101,79,109},
	{-114,55,108},{-111,-13,104},{-100,-57,91},{-95,-90,88},
	{-93,-105,85},{-97,-117,81},{-106,-119,81},{-127,-121,82},
	{-127,6,93},{-127,27,98},{-85,61,95},{-106,18,96},
	{-110,27,97},{-112,-88,94},{-117,-57,96},{-127,-57,96},
	{-127,-42,95},{-115,-35,100},{-110,-29,102},{-113,-17,100},
	{-122,-16,100},{-127,-26,106},{-121,-19,104},{-115,-20,104},
	{-113,-29,106},{-117,-32,103},{-127,-37,103},{-94,-40,71},
	{-106,-31,91},{-104,-40,91},{-97,-32,71},{-127,-112,88},
	{-121,-111,88},{-115,-105,91},{-115,-95,93},{-127,-100,84},
	{-115,-96,85},{-115,-104,82},{-121,-109,81},{-127,-110,81},
	{-105,28,100},{-103,20,99},{-84,55,97},{-92,54,99},
	{-73,51,99},{-55,45,89},{-52,37,88},{-53,25,87},
	{-66,13,92},{-79,8,95},{-98,14,100},{-104,38,100},
	{-100,48,100},{-97,46,97},{-102,38,97},{-96,16,97},
	{-79,11,93},{-68,15,90},{-57,27,86},{-56,36,86},
	{-59,43,87},{-74,50,96},{-91,51,98},{-84,52,96},
	{-101,22,96},{-102,29,96},{-113,59,78},{-102,85,79},
	{-84,88,76},{-65,71,71},{-40,58,63},{-25,52,59},
	{-28,21,48},{-50,0,53},{-71,-12,60},{-127,115,37},
	{-127,126,-10},{-127,-25,-86},{-127,-59,24},{-127,-125,59},
	{-127,-103,44},{-127,-73,41},{-127,-62,36},{-18,30,7},
	{-17,41,-6},{-28,34,-56},{-68,56,-90},{-33,-6,9},
	{-51,-16,-21},{-45,-1,-55},{-84,7,-85},{-97,-45,52},
	{-104,-53,33},{-90,-91,49},{-95,-64,50},{-85,-117,51},
	{-109,-97,47},{-111,-69,46},{-106,-121,56},{-99,-36,55},
	{-100,-29,60},{-101,-22,64},{-100,-50,21},{-89,-40,-34},
	{-83,-19,-69},{-69,111,-49},{-69,119,-9},{-69,109,30},
	{-68,67,55},{-34,52,43},{-46,58,36},{-45,90,7},
	{-25,72,16},{-25,79,-15},{-45,96,-25},{-45,87,-57},
	{-25,69,-46},{-48,42,-75},{-65,3,-70},{-22,42,-26},
	{-75,-22,19},{-72,-25,-27},{-13,52,-30},{-28,-18,-16},
	{6,-13,-42},{37,7,-55},{46,41,-54},{31,65,-54},
	{4,61,-40},{3,53,-37},{25,56,-50},{35,37,-52},
	{28,10,-52},{5,-5,-39},{-21,-9,-17},{-9,46,-28},
	{-6,39,-37},{-14,-3,-27},{6,0,-47},{25,12,-57},
	{31,32,-57},{23,46,-56},{4,44,-46},{-19,37,-27},
	{-20,22,-35},{-30,12,-35},{-22,11,-35},{-19,2,-35},
	{-23,-2,-35},{-34,0,-9},{-35,-3,-22},{-35,5,-24},
	{-25,26,-27},{-13,31,-34},{-13,30,-41},{-23,-2,-41},
	{-18,2,-41},{-21,10,-41},{-29,12,-41},{-19,22,-41},
	{6,42,-53},{25,44,-62},{34,31,-63},{28,11,-62},
	{7,0,-54},{-14,-2,-34},{-5,37,-44},{-13,14,-42},
	{-7,8,-43},{1,16,-47},{-4,22,-45},{3,30,-48},
	{8,24,-49},{15,27,-50},{12,35,-50},{4,56,-62},
	{33,60,-70},{48,38,-64},{41,7,-68},{6,-11,-63},
	{-26,-16,-42},{-17,49,-49},
};

static signed char monkeyf[250][4] = {
	{27,4,5,26}, {25,4,5,24}, {3,6,5,4}, {1,6,5,2}, {5,6,7,4},
	{3,6,7,2}, {5,8,7,6}, {3,8,7,4}, {7,8,9,6},
	{5,8,9,4}, {7,10,9,8}, {5,10,9,6}, {9,10,11,8},
	{7,10,11,6}, {9,12,11,10}, {7,12,11,8}, {11,6,13,12},
	{5,4,13,12}, {3,-2,13,12}, {-3,-4,13,12}, {-5,-10,13,12},
	{-11,-12,14,12}, {-13,-18,14,13}, {-19,4,5,13}, {10,12,4,4},
	{10,11,9,9}, {8,7,9,9}, {7,5,6,6}, {6,3,4,4},
	{5,1,2,2}, {4,-1,0,0}, {3,-3,-2,-2}, {22,67,68,23},
	{20,65,66,21}, {18,63,64,19}, {16,61,62,17}, {14,59,60,15},
	{12,19,48,57}, {18,19,48,47}, {18,19,48,47}, {18,19,48,47},
	{18,19,48,47}, {18,19,48,47}, {18,19,48,47}, {18,19,48,47},
	{18,19,48,47}, {18,-9,-8,47}, {18,27,45,46}, {26,55,43,44},
	{24,41,42,54}, {22,39,40,23}, {20,37,38,21}, {18,35,36,19},
	{16,33,34,17}, {14,31,32,15}, {12,39,30,13}, {11,48,45,38},
	{8,36,-19,9}, {8,-20,44,47}, {42,45,46,43}, {18,19,40,39},
	{16,17,38,37}, {14,15,36,35}, {32,44,43,33}, {12,33,32,42},
	{19,44,43,42}, {40,41,42,-27}, {8,9,39,-28}, {15,43,42,16},
	{13,43,42,14}, {11,43,42,12}, {9,-30,42,10}, {37,12,38,-32},
	{-33,37,45,46}, {-33,40,41,39}, {38,40,41,37}, {36,40,41,35},
	{34,40,41,33}, {36,39,38,37}, {35,40,39,38}, {1,2,14,21},
	{1,2,40,13}, {1,2,40,39}, {1,24,12,39}, {-34,36,38,11},
	{35,38,36,37}, {-37,8,35,37}, {-11,-12,-45,40}, {-11,-12,39,38},
	{-11,-12,37,36}, {-11,-12,35,34}, {33,34,40,41}, {33,34,38,39},
	{33,34,36,37}, {33,-52,34,35}, {33,37,36,34}, {33,35,34,34},
	{8,7,37,36}, {-32,7,35,46}, {-34,-33,45,46}, {4,-33,43,34},
	{-34,-33,41,42}, {-34,-33,39,40}, {-34,-33,37,38}, {-34,-33,35,36},
	{-34,-33,33,34}, {-34,-33,31,32}, {-34,-4,28,30}, {-5,-34,28,27},
	{-35,-44,36,27}, {26,35,36,45}, {24,25,44,45}, {25,23,44,42},
	{25,24,41,40}, {25,24,39,38}, {25,24,37,36}, {25,24,35,34},
	{25,24,33,32}, {25,24,31,30}, {15,24,29,38}, {25,24,27,26},
	{23,12,37,26}, {11,12,35,36}, {-86,-59,36,-80}, {-60,-61,36,35},
	{-62,-63,36,35}, {-64,-65,36,35}, {-66,-67,36,35}, {-68,-69,36,35},
	{-70,-71,36,35}, {-72,-73,36,35}, {-74,-75,36,35}, {42,43,53,58},
	{40,41,57,56}, {38,39,55,57}, {-81,-80,37,56}, {-83,-82,55,52},
	{-85,-84,51,49}, {-87,-86,48,49}, {47,50,51,48}, {46,48,51,49},
	{43,46,49,44}, {-92,-91,45,42}, {-23,49,50,-20}, {-94,40,48,-24},
	{-96,-22,48,49}, {-97,48,21,-90}, {-100,36,50,23}, {22,49,48,-100},
	{-101,47,46,22}, {21,45,35,25}, {33,34,44,41}, {13,14,28,24},
	{-107,26,30,-106}, {14,46,45,15}, {14,44,43,-110}, {-111,42,23,-110},
	{6,7,45,46}, {45,44,47,46}, {45,46,47,48}, {47,46,49,48},
	{17,49,47,48}, {17,36,46,48}, {35,36,44,45}, {35,36,40,43},
	{35,36,38,39}, {-4,-3,37,35}, {-123,34,33,1}, {-9,-8,-7,-6},
	{-10,-7,32,-125}, {-127,-11,-126,-126}, {-7,-6,5,31}, {4,5,33,30},
	{4,39,33,32}, {4,35,32,38}, {20,21,39,38}, {4,37,38,5},
	{-11,-10,36,3}, {-11,15,14,35}, {13,16,34,34}, {-13,14,13,13},
	{-3,1,30,29}, {-3,28,29,1}, {-2,31,28,-1}, {12,13,27,30},
	{-2,26,12,12}, {35,29,42,36}, {34,35,36,33}, {32,35,36,31},
	{30,35,36,29}, {28,35,36,27}, {26,35,36,25}, {34,39,38,35},
	{32,39,38,33}, {30,39,38,31}, {28,39,38,29}, {26,39,38,27},
	{25,31,32,38}, {-18,-17,45,44}, {-18,17,28,44}, {-24,-20,42,-23},
	{11,35,27,14}, {25,28,39,41}, {37,41,40,38}, {34,40,36,35},
	{32,40,39,33}, {30,39,31,40}, {21,29,39,22}, {-31,37,28,4},
	{-32,33,35,36}, {32,33,34,34}, {18,35,36,48}, {34,25,40,35},
	{24,25,38,39}, {24,25,36,37}, {24,25,34,35}, {24,25,32,33},
	{24,13,41,31}, {17,11,41,35}, {15,16,34,35}, {13,14,34,35},
	{11,12,34,35}, {9,10,34,35}, {7,8,34,35}, {26,25,37,36},
	{35,36,37,38}, {37,36,39,38}, {37,38,39,40}, {25,31,36,39},
	{18,34,35,30}, {17,22,30,33}, {19,29,21,20}, {16,26,29,17},
	{24,29,28,25}, {22,31,28,23}, {20,31,30,21}, {18,31,30,19},
	{16,30,17,17}, {-21,-22,35,34}, {-21,-22,33,32}, {-21,-22,31,30},
	{-21,-22,29,28}, {-21,-22,27,26}, {-28,-22,25,31}, {24,28,29,30},
	{23,24,26,27}, {23,24,25,25}, {-69,-35,-32,27}, {-70,26,25,-66},
	{-68,-67,24,-33},
};

#define VERT_MARK	1

#define EDGE_ORIG	1
#define EDGE_MARK	2

#define FACE_MARK	1
#define FACE_NEW	2

void bmesh_create_grid_exec(BMesh *bm, BMOperator *op)
{
	BMOperator bmop, prevop;
	BMVert *eve, *preveve;
	BMEdge *e;
	float vec[3], mat[4][4], phi, phid, dia = BMO_slot_float_get(op, "size");
	int a, tot = BMO_slot_int_get(op, "xsegments"), seg = BMO_slot_int_get(op, "ysegments");

	if (tot < 2) tot = 2;
	if (seg < 2) seg = 2;

	BMO_slot_mat4_get(op, "mat", mat);

	/* one segment first: the X axis */
	phi = 1.0f;
	phid = 2.0f / ((float)tot - 1);
	for (a = 0; a < tot; a++) {
		vec[0] = dia * phi;
		vec[1] = -dia;
		vec[2] = 0.0f;
		mul_m4_v3(mat, vec);

		eve = BM_vert_create(bm, vec, NULL);
		BMO_elem_flag_enable(bm, eve, VERT_MARK);

		if (a != 0) {
			e = BM_edge_create(bm, preveve, eve, NULL, TRUE);
			BMO_elem_flag_enable(bm, e, EDGE_ORIG);
		}

		preveve = eve;
		phi -= phid;
	}

	/* extrude and translate */
	vec[0] = vec[2] = 0.0f;
	vec[1] = dia * phid;
	mul_mat3_m4_v3(mat, vec);

	for (a = 0; a < seg - 1; a++) {
		if (a) {
			BMO_op_initf(bm, &bmop, "extrude_edge_only edges=%s", &prevop, "geomout");
			BMO_op_exec(bm, &bmop);
			BMO_op_finish(bm, &prevop);

			BMO_slot_buffer_flag_enable(bm, &bmop, "geomout", VERT_MARK, BM_VERT);
		}
		else {
			BMO_op_initf(bm, &bmop, "extrude_edge_only edges=%fe", EDGE_ORIG);
			BMO_op_exec(bm, &bmop);
			BMO_slot_buffer_flag_enable(bm, &bmop, "geomout", VERT_MARK, BM_VERT);
		}

		BMO_op_callf(bm, "translate vec=%v verts=%s", vec, &bmop, "geomout");
		prevop = bmop;
	}

	if (a)
		BMO_op_finish(bm, &bmop);

	BMO_slot_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}

void bmesh_create_uvsphere_exec(BMesh *bm, BMOperator *op)
{
	BMOperator bmop, prevop;
	BMVert *eve, *preveve;
	BMEdge *e;
	BMIter iter;
	float vec[3], mat[4][4], cmat[3][3], phi, q[4];
	float phid, dia = BMO_slot_float_get(op, "diameter");
	int a, seg = BMO_slot_int_get(op, "segments"), tot = BMO_slot_int_get(op, "revolutions");

	BMO_slot_mat4_get(op, "mat", mat);

	phid = 2.0f * (float)M_PI / tot;
	phi = 0.25f * (float)M_PI;

	/* one segment first */
	phi = 0;
	phid /= 2;
	for (a = 0; a <= tot; a++) {
		/* Going in this direction, then edge extruding, makes normals face outward */
		vec[0] = -dia * sinf(phi);
		vec[1] = 0.0;
		vec[2] = dia * cosf(phi);
		eve = BM_vert_create(bm, vec, NULL);
		BMO_elem_flag_enable(bm, eve, VERT_MARK);

		if (a != 0) {
			e = BM_edge_create(bm, preveve, eve, NULL, FALSE);
			BMO_elem_flag_enable(bm, e, EDGE_ORIG);
		}

		phi+= phid;
		preveve = eve;
	}

	/* extrude and rotate; negative phi to make normals face outward */
	phi = -M_PI / seg;
	q[0] = cosf(phi);
	q[3] = sinf(phi);
	q[1] = q[2] = 0.0f;
	quat_to_mat3(cmat, q);

	for (a = 0; a < seg; a++) {
		if (a) {
			BMO_op_initf(bm, &bmop, "extrude_edge_only edges=%s", &prevop, "geomout");
			BMO_op_exec(bm, &bmop);
			BMO_op_finish(bm, &prevop);
		}
		else {
			BMO_op_initf(bm, &bmop, "extrude_edge_only edges=%fe", EDGE_ORIG);
			BMO_op_exec(bm, &bmop);
		}

		BMO_slot_buffer_flag_enable(bm, &bmop, "geomout", VERT_MARK, BM_VERT);
		BMO_op_callf(bm, "rotate cent=%v mat=%m3 verts=%s", vec, cmat, &bmop, "geomout");
		
		prevop = bmop;
	}

	if (a)
		BMO_op_finish(bm, &bmop);

	{
		float len, len2, vec2[3];

		len= 2*dia*sinf(phid / 2.0f);

		/* length of one segment in shortest parallen */
		vec[0]= dia*sinf(phid);
		vec[1]= 0.0;
		vec[2]= dia*cosf(phid);

		mul_v3_m3v3(vec2, cmat, vec);
		len2= len_v3v3(vec, vec2);

		/* use shortest segment length divided by 3 as merge threshold */
		BMO_op_callf(bm, "removedoubles verts=%fv dist=%f", VERT_MARK, MIN2(len, len2) / 3.0f);
	}

	/* and now do imat */
	BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, eve, VERT_MARK)) {
			mul_m4_v3(mat, eve->co);
		}
	}

	BMO_slot_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}

void bmesh_create_icosphere_exec(BMesh *bm, BMOperator *op)
{
	BMVert *eva[12];
	BMVert *v;
	BMIter liter;
	BMIter viter;
	BMLoop *l;
	float vec[3], mat[4][4] /* , phi, phid */;
	float dia = BMO_slot_float_get(op, "diameter");
	int a, subdiv = BMO_slot_int_get(op, "subdivisions");

	BMO_slot_mat4_get(op, "mat", mat);

	/* phid = 2.0f * (float)M_PI / subdiv; */ /* UNUSED */
	/* phi = 0.25f * (float)M_PI; */         /* UNUSED */

	dia /= 200.0f;
	for (a = 0; a < 12; a++) {
		vec[0] = dia * icovert[a][0];
		vec[1] = dia * icovert[a][1];
		vec[2] = dia * icovert[a][2];
		eva[a] = BM_vert_create(bm, vec, NULL);

		BMO_elem_flag_enable(bm, eva[a], VERT_MARK);
	}

	for (a = 0; a < 20; a++) {
		BMFace *eftemp;
		BMVert *v1, *v2, *v3;

		v1 = eva[icoface[a][0]];
		v2 = eva[icoface[a][1]];
		v3 = eva[icoface[a][2]];

		eftemp = BM_face_create_quad_tri(bm, v1, v2, v3, NULL, NULL, FALSE);
		
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, eftemp) {
			BMO_elem_flag_enable(bm, l->e, EDGE_MARK);
		}

		BMO_elem_flag_enable(bm, eftemp, FACE_MARK);
	}

	dia *= 200.0f;

	for (a = 1; a < subdiv; a++) {
		BMOperator bmop;

		BMO_op_initf(bm, &bmop,
		             "esubd edges=%fe smooth=%f numcuts=%i gridfill=%b beauty=%i",
		             EDGE_MARK, dia, 1, TRUE, B_SPHERE);
		BMO_op_exec(bm, &bmop);
		BMO_slot_buffer_flag_enable(bm, &bmop, "geomout", VERT_MARK, BM_VERT);
		BMO_slot_buffer_flag_enable(bm, &bmop, "geomout", EDGE_MARK, BM_EDGE);
		BMO_op_finish(bm, &bmop);
	}

	/* must transform after becayse of sphere subdivision */
	BM_ITER(v, &viter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			mul_m4_v3(mat, v->co);
		}
	}

	BMO_slot_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}

void bmesh_create_monkey_exec(BMesh *bm, BMOperator *op)
{
	BMVert *eve;
	BMVert **tv = MEM_mallocN(sizeof(*tv)*monkeynv * 2, "tv");
	float mat[4][4];
	int i;

	BMO_slot_mat4_get(op, "mat", mat);

	for (i = 0; i < monkeynv; i++) {
		float v[3];

		v[0] = (monkeyv[i][0] + 127) / 128.0, v[1] = monkeyv[i][1] / 128.0, v[2] = monkeyv[i][2] / 128.0;

		tv[i] = BM_vert_create(bm, v, NULL);
		BMO_elem_flag_enable(bm, tv[i], VERT_MARK);

		tv[monkeynv + i] = (fabsf(v[0] = -v[0]) < 0.001f) ?
		            tv[i] :
		            (eve = BM_vert_create(bm, v, NULL), mul_m4_v3(mat, eve->co), eve);

		BMO_elem_flag_enable(bm, tv[monkeynv + i], VERT_MARK);

		mul_m4_v3(mat, tv[i]->co);
	}

	for (i = 0; i < monkeynf; i++) {
		BM_face_create_quad_tri(bm,
		                        tv[monkeyf[i][0] + i - monkeyo],
		                        tv[monkeyf[i][1] + i - monkeyo],
		                        tv[monkeyf[i][2] + i - monkeyo],
		                        (monkeyf[i][3] != monkeyf[i][2]) ? tv[monkeyf[i][3] + i - monkeyo] : NULL,
		                        NULL, FALSE);

		BM_face_create_quad_tri(bm,
		                        tv[monkeynv + monkeyf[i][2] + i - monkeyo],
		                        tv[monkeynv + monkeyf[i][1] + i - monkeyo],
		                        tv[monkeynv + monkeyf[i][0] + i - monkeyo],
		                        (monkeyf[i][3] != monkeyf[i][2]) ? tv[monkeynv + monkeyf[i][3] + i - monkeyo]: NULL,
		                        NULL, FALSE);
	}

	MEM_freeN(tv);

	BMO_slot_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}


void bmesh_create_circle_exec(BMesh *bm, BMOperator *op)
{
	BMVert *v1, *lastv1 = NULL, *cent1, *firstv1 = NULL;
	float vec[3], mat[4][4], phi, phid;
	float dia = BMO_slot_float_get(op, "diameter");
	int segs = BMO_slot_int_get(op, "segments");
	int cap_ends = BMO_slot_bool_get(op, "cap_ends");
	int cap_tris = BMO_slot_bool_get(op, "cap_tris");
	int a;
	
	if (!segs)
		return;
	
	BMO_slot_mat4_get(op, "mat", mat);

	phid = 2.0f * (float)M_PI / segs;
	phi = .25f * (float)M_PI;

	if (cap_ends) {
		vec[0] = vec[1] = 0.0f;
		vec[2] = 0.0;
		mul_m4_v3(mat, vec);
		
		cent1 = BM_vert_create(bm, vec, NULL);
	}

	for (a = 0; a < segs; a++, phi += phid) {
		/* Going this way ends up with normal(s) upward */
		vec[0] = -dia * sinf(phi);
		vec[1] = dia * cosf(phi);
		vec[2] = 0.0f;
		mul_m4_v3(mat, vec);
		v1 = BM_vert_create(bm, vec, NULL);

		BMO_elem_flag_enable(bm, v1, VERT_MARK);
		
		if (lastv1)
			BM_edge_create(bm, v1, lastv1, NULL, FALSE);
		
		if (a && cap_ends) {
			BMFace *f;
			
			f = BM_face_create_quad_tri(bm, cent1, lastv1, v1, NULL, NULL, FALSE);
			BMO_elem_flag_enable(bm, f, FACE_NEW);
		}
		
		if (!firstv1)
			firstv1 = v1;

		lastv1 = v1;
	}

	if (!a)
		return;

	BM_edge_create(bm, lastv1, firstv1, NULL, FALSE);

	if (cap_ends) {
		BMFace *f;
		
		f = BM_face_create_quad_tri(bm, cent1, v1, firstv1, NULL, NULL, FALSE);
		BMO_elem_flag_enable(bm, f, FACE_NEW);
	}
	
	if (!cap_tris) {
		BMO_op_callf(bm, "dissolvefaces faces=%ff", FACE_NEW);
	}
	
	BMO_slot_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}

void bmesh_create_cone_exec(BMesh *bm, BMOperator *op)
{
	BMVert *v1, *v2, *lastv1 = NULL, *lastv2 = NULL, *cent1, *cent2, *firstv1, *firstv2;
	float vec[3], mat[4][4], phi, phid;
	float dia1 = BMO_slot_float_get(op, "diameter1");
	float dia2 = BMO_slot_float_get(op, "diameter2");
	float depth = BMO_slot_float_get(op, "depth");
	int segs = BMO_slot_int_get(op, "segments");
	int cap_ends = BMO_slot_bool_get(op, "cap_ends");
	int cap_tris = BMO_slot_bool_get(op, "cap_tris");
	int a;
	
	if (!segs)
		return;
	
	BMO_slot_mat4_get(op, "mat", mat);

	phid = 2.0f * (float)M_PI / segs;
	phi = 0.25f * (float)M_PI;

	depth *= 0.5f;
	if (cap_ends) {
		vec[0] = vec[1] = 0.0f;
		vec[2] = -depth;
		mul_m4_v3(mat, vec);
		
		cent1 = BM_vert_create(bm, vec, NULL);

		vec[0] = vec[1] = 0.0f;
		vec[2] = depth;
		mul_m4_v3(mat, vec);
		
		cent2 = BM_vert_create(bm, vec, NULL);

		BMO_elem_flag_enable(bm, cent1, VERT_MARK);
		BMO_elem_flag_enable(bm, cent2, VERT_MARK);
	}

	for (a = 0; a < segs; a++, phi += phid) {
		vec[0] = dia1 * sinf(phi);
		vec[1] = dia1 * cosf(phi);
		vec[2] = -depth;
		mul_m4_v3(mat, vec);
		v1 = BM_vert_create(bm, vec, NULL);

		vec[0] = dia2 * sinf(phi);
		vec[1] = dia2 * cosf(phi);
		vec[2] = depth;
		mul_m4_v3(mat, vec);
		v2 = BM_vert_create(bm, vec, NULL);

		BMO_elem_flag_enable(bm, v1, VERT_MARK);
		BMO_elem_flag_enable(bm, v2, VERT_MARK);

		if (a) {
			if (cap_ends) {
				BMFace *f;
				
				f = BM_face_create_quad_tri(bm, cent1, lastv1, v1, NULL, NULL, FALSE);
				BMO_elem_flag_enable(bm, f, FACE_NEW);
				f = BM_face_create_quad_tri(bm, cent2, v2, lastv2, NULL, NULL, FALSE);
				BMO_elem_flag_enable(bm, f, FACE_NEW);
			}
			BM_face_create_quad_tri(bm, lastv1, lastv2, v2, v1, NULL, FALSE);
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
		BMFace *f;
		
		f = BM_face_create_quad_tri(bm, cent1, v1, firstv1, NULL, NULL, FALSE);
		BMO_elem_flag_enable(bm, f, FACE_NEW);
		f = BM_face_create_quad_tri(bm, cent2, firstv2, v2, NULL, NULL, FALSE);
		BMO_elem_flag_enable(bm, f, FACE_NEW);
	}
	
	if (!cap_tris) {
		BMO_op_callf(bm, "dissolvefaces faces=%ff", FACE_NEW);
	}
	
	BM_face_create_quad_tri(bm, v1, v2, firstv2, firstv1, NULL, FALSE);

	BMO_op_callf(bm, "removedoubles verts=%fv dist=%f", VERT_MARK, 0.000001);
	BMO_slot_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}

void bmesh_create_cube_exec(BMesh *bm, BMOperator *op)
{
	BMVert *v1, *v2, *v3, *v4, *v5, *v6, *v7, *v8;
	float vec[3], mat[4][4], off = BMO_slot_float_get(op, "size") / 2.0f;

	BMO_slot_mat4_get(op, "mat", mat);

	if (!off) off = 0.5f;

	vec[0] = -off;
	vec[1] = -off;
	vec[2] = -off;
	mul_m4_v3(mat, vec);
	v1 = BM_vert_create(bm, vec, NULL);
	BMO_elem_flag_enable(bm, v1, VERT_MARK);

	vec[0] = -off;
	vec[1] = off;
	vec[2] = -off;
	mul_m4_v3(mat, vec);
	v2 = BM_vert_create(bm, vec, NULL);
	BMO_elem_flag_enable(bm, v2, VERT_MARK);

	vec[0] = off;
	vec[1] = off;
	vec[2] = -off;
	mul_m4_v3(mat, vec);
	v3 = BM_vert_create(bm, vec, NULL);
	BMO_elem_flag_enable(bm, v3, VERT_MARK);

	vec[0] = off;
	vec[1] = -off;
	vec[2] = -off;
	mul_m4_v3(mat, vec);
	v4 = BM_vert_create(bm, vec, NULL);
	BMO_elem_flag_enable(bm, v4, VERT_MARK);

	vec[0] = -off;
	vec[1] = -off;
	vec[2] = off;
	mul_m4_v3(mat, vec);
	v5 = BM_vert_create(bm, vec, NULL);
	BMO_elem_flag_enable(bm, v5, VERT_MARK);

	vec[0] = -off;
	vec[1] = off;
	vec[2] = off;
	mul_m4_v3(mat, vec);
	v6 = BM_vert_create(bm, vec, NULL);
	BMO_elem_flag_enable(bm, v6, VERT_MARK);

	vec[0] = off;
	vec[1] = off;
	vec[2] = off;
	mul_m4_v3(mat, vec);
	v7 = BM_vert_create(bm, vec, NULL);
	BMO_elem_flag_enable(bm, v7, VERT_MARK);

	vec[0] = off;
	vec[1] = -off;
	vec[2] = off;
	mul_m4_v3(mat, vec);
	v8 = BM_vert_create(bm, vec, NULL);
	BMO_elem_flag_enable(bm, v8, VERT_MARK);

	/* the four sides */
	BM_face_create_quad_tri(bm, v5, v6, v2, v1, NULL, FALSE);
	BM_face_create_quad_tri(bm, v6, v7, v3, v2, NULL, FALSE);
	BM_face_create_quad_tri(bm, v7, v8, v4, v3, NULL, FALSE);
	BM_face_create_quad_tri(bm, v8, v5, v1, v4, NULL, FALSE);
	
	/* top/bottom */
	BM_face_create_quad_tri(bm, v1, v2, v3, v4, NULL, FALSE);
	BM_face_create_quad_tri(bm, v8, v7, v6, v5, NULL, FALSE);

	BMO_slot_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}
