/**
 * Copyright (C) 2016-2017 IRIE Shinsuke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * smaa_areatex.cpp  version 0.4.0
 *
 * This is a part of smaa-cpp that is an implementation of
 * Enhanced Subpixel Morphological Antialiasing (SMAA) written in C++.
 *
 * This program is C++ rewrite of AreaTex.py included in the original
 * SMAA ditribution:
 *
 *   https://github.com/iryoku/smaa/tree/master/Scripts
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <cmath>

/*------------------------------------------------------------------------------*/
/* Type Definitions */

class Int2;
class Dbl2;

class Int2 {
public:
	int x, y;

	Int2() { this->x = this->y = 0; }
	Int2(int x) { this->x = this->y = x; }
	Int2(int x, int y) { this->x = x; this->y = y; }

	operator Dbl2();

	Int2 operator + (Int2 other) { return Int2(x + other.x, y + other.y); }
	Int2 operator * (Int2 other) { return Int2(x * other.x, y * other.y); }
};

class Dbl2 {
public:
	double x, y;

	Dbl2() { this->x = this->y = 0.0; }
	Dbl2(double x) { this->x = this->y = x; }
	Dbl2(double x, double y) { this->x = x; this->y = y; }

	Dbl2 apply(double (* func)(double)) { return Dbl2(func(x), func(y)); }

	operator Int2();

	Dbl2 operator + (Dbl2 other) { return Dbl2(x + other.x, y + other.y); }
	Dbl2 operator - (Dbl2 other) { return Dbl2(x - other.x, y - other.y); }
	Dbl2 operator * (Dbl2 other) { return Dbl2(x * other.x, y * other.y); }
	Dbl2 operator / (Dbl2 other) { return Dbl2(x / other.x, y / other.y); }
	Dbl2 operator += (Dbl2 other) { return Dbl2(x += other.x, y += other.y); }
	bool operator == (Dbl2 other) { return (x == other.x && y == other.y); }
};

Int2::operator Dbl2() { return Dbl2((double)x, (double)y); }
Dbl2::operator Int2() { return Int2((int)x, (int)y); }

/*------------------------------------------------------------------------------*/
/* Data to Calculate Areatex */

/* Texture sizes: */
/* (it's quite possible that this is not easily configurable) */
static const int SUBSAMPLES_ORTHO = 7;
static const int SUBSAMPLES_DIAG  = 5;
static const int MAX_DIST_ORTHO_COMPAT = 16;
static const int MAX_DIST_ORTHO = 20;
static const int MAX_DIST_DIAG  = 20;
static const int TEX_SIZE_ORTHO = 80; /* 16 * 5 slots = 80 */
static const int TEX_SIZE_DIAG  = 80; /* 20 * 4 slots = 80 */

/* Number of samples for calculating areas in the diagonal textures: */
/* (diagonal areas are calculated using brute force sampling) */
static const int SAMPLES_DIAG = 30;

/* Maximum distance for smoothing u-shapes: */
static const int SMOOTH_MAX_DISTANCE = 32;

/*------------------------------------------------------------------------------*/
/* Offset Tables */

/* Offsets for subsample rendering */
static const double subsample_offsets_ortho[SUBSAMPLES_ORTHO] = {
	0.0,    /* 0 */
	-0.25,  /* 1 */
	0.25,   /* 2 */
	-0.125, /* 3 */
	0.125,  /* 4 */
	-0.375, /* 5 */
	0.375   /* 6 */
};

static const Dbl2 subsample_offsets_diag[SUBSAMPLES_DIAG] = {
	{ 0.00,   0.00},  /* 0 */
	{ 0.25,  -0.25},  /* 1 */
	{-0.25,   0.25},  /* 2 */
	{ 0.125, -0.125}, /* 3 */
	{-0.125,  0.125}  /* 4 */
};

/* Mapping offsets for placing each pattern subtexture into its place */
enum edgesorthoIndices
{
	EDGESORTHO_NONE_NONE = 0,
	EDGESORTHO_NONE_NEGA = 1,
	EDGESORTHO_NONE_POSI = 2,
	EDGESORTHO_NONE_BOTH = 3,
	EDGESORTHO_NEGA_NONE = 4,
	EDGESORTHO_NEGA_NEGA = 5,
	EDGESORTHO_NEGA_POSI = 6,
	EDGESORTHO_NEGA_BOTH = 7,
	EDGESORTHO_POSI_NONE = 8,
	EDGESORTHO_POSI_NEGA = 9,
	EDGESORTHO_POSI_POSI = 10,
	EDGESORTHO_POSI_BOTH = 11,
	EDGESORTHO_BOTH_NONE = 12,
	EDGESORTHO_BOTH_NEGA = 13,
	EDGESORTHO_BOTH_POSI = 14,
	EDGESORTHO_BOTH_BOTH = 15,
};

static const Int2 edgesortho_compat[16] = {
	{0, 0}, {0, 1}, {0, 3}, {0, 4}, {1, 0}, {1, 1}, {1, 3}, {1, 4},
	{3, 0}, {3, 1}, {3, 3}, {3, 4}, {4, 0}, {4, 1}, {4, 3}, {4, 4}
};

static const Int2 edgesortho[16] = {
	{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 0}, {1, 1}, {1, 2}, {1, 3},
	{2, 0}, {2, 1}, {2, 2}, {2, 3}, {3, 0}, {3, 1}, {3, 2}, {3, 3}
};

enum edgesdiagIndices
{
	EDGESDIAG_NONE_NONE = 0,
	EDGESDIAG_NONE_VERT = 1,
	EDGESDIAG_NONE_HORZ = 2,
	EDGESDIAG_NONE_BOTH = 3,
	EDGESDIAG_VERT_NONE = 4,
	EDGESDIAG_VERT_VERT = 5,
	EDGESDIAG_VERT_HORZ = 6,
	EDGESDIAG_VERT_BOTH = 7,
	EDGESDIAG_HORZ_NONE = 8,
	EDGESDIAG_HORZ_VERT = 9,
	EDGESDIAG_HORZ_HORZ = 10,
	EDGESDIAG_HORZ_BOTH = 11,
	EDGESDIAG_BOTH_NONE = 12,
	EDGESDIAG_BOTH_VERT = 13,
	EDGESDIAG_BOTH_HORZ = 14,
	EDGESDIAG_BOTH_BOTH = 15,
};

static const Int2 edgesdiag[16] = {
	{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 0}, {1, 1}, {1, 2}, {1, 3},
	{2, 0}, {2, 1}, {2, 2}, {2, 3}, {3, 0}, {3, 1}, {3, 2}, {3, 3}
};

/*------------------------------------------------------------------------------*/
/* Miscellaneous Utility Functions */

/* Linear interpolation: */
static Dbl2 lerp(Dbl2 a, Dbl2 b, double p)
{
	return a + (b - a) * Dbl2(p);
}

/* Saturates a value to [0..1] range: */
static double saturate(double x)
{
	return 0.0 < x ? (x < 1.0 ? x : 1.0) : 0.0;
}

/*------------------------------------------------------------------------------*/
/* Horizontal/Vertical Areas */

class AreaOrtho {
	double m_data[SUBSAMPLES_ORTHO][TEX_SIZE_ORTHO][TEX_SIZE_ORTHO][2];
	bool m_compat;
	bool m_orig_u;
public:
	AreaOrtho(bool compat, bool orig_u) : m_compat(compat), m_orig_u(orig_u) {}

	double *getData() { return (double *)&m_data; }
	Dbl2 getPixel(int offset_index, Int2 coords) {
		return Dbl2(m_data[offset_index][coords.y][coords.x][0],
			    m_data[offset_index][coords.y][coords.x][1]);
	}

	void areaTex(int offset_index);
private:
	void putPixel(int offset_index, Int2 coords, Dbl2 pixel) {
		m_data[offset_index][coords.y][coords.x][0] = pixel.x;
		m_data[offset_index][coords.y][coords.x][1] = pixel.y;
	}

	Dbl2 smoothArea(double d, Dbl2 a1, Dbl2 a2);
	Dbl2 makeQuad(int x, double d, double o);
	Dbl2 area(Dbl2 p1, Dbl2 p2, int x);
	Dbl2 calculate(int pattern, int left, int right, double offset);
};

/* Smoothing function for small u-patterns: */
Dbl2 AreaOrtho::smoothArea(double d, Dbl2 a1, Dbl2 a2)
{
	Dbl2 b1 = (a1 * Dbl2(2.0)).apply(sqrt) * Dbl2(0.5);
	Dbl2 b2 = (a2 * Dbl2(2.0)).apply(sqrt) * Dbl2(0.5);
	double p = saturate(d / (double)SMOOTH_MAX_DISTANCE);
	return lerp(b1, a1, p) + lerp(b2, a2, p);
}

/* Smoothing u-patterns by quadratic function: */
Dbl2 AreaOrtho::makeQuad(int x, double d, double o)
{
	double r = (double)x;

	/* fmin() below is a trick to smooth tiny u-patterns: */
	return Dbl2(r, (1.0 - fmin(4.0, d) * r * (d - r) / (d * d)) * o);
}

/* Calculates the area under the line p1->p2, for the pixel x..x+1: */
Dbl2 AreaOrtho::area(Dbl2 p1, Dbl2 p2, int x)
{
	Dbl2 d = p2 - p1;
	double x1 = (double)x;
	double x2 = x1 + 1.0;

	if ((x1 >= p1.x && x1 < p2.x) || (x2 > p1.x && x2 <= p2.x)) { /* inside? */
		double y1 = p1.y + (x1 - p1.x) * d.y / d.x;
		double y2 = p1.y + (x2 - p1.x) * d.y / d.x;

		if ((copysign(1.0, y1) == copysign(1.0, y2) ||
		     fabs(y1) < 1e-4 || fabs(y2) < 1e-4)) { /* trapezoid? */
			double a = (y1 + y2) / 2.0;
			if (a < 0.0)
				return Dbl2(fabs(a), 0.0);
			else
				return Dbl2(0.0, fabs(a));
		}
		else { /* Then, we got two triangles: */
			double x = p1.x - p1.y * d.x / d.y, xi;
			double a1 = x > p1.x ? y1 * modf(x, &xi) / 2.0 : 0.0;
			double a2 = x < p2.x ? y2 * (1.0 - modf(x, &xi)) / 2.0 : 0.0;
			double a = fabs(a1) > fabs(a2) ? a1 : -a2;
			if (a < 0.0)
				return Dbl2(fabs(a1), fabs(a2));
			else
				return Dbl2(fabs(a2), fabs(a1));
		}
	}
	else
		return Dbl2(0.0, 0.0);
}

/* Calculates the area for a given pattern and distances to the left and to the */
/* right, biased by an offset: */
Dbl2 AreaOrtho::calculate(int pattern, int left, int right, double offset)
{
	Dbl2 a1, a2;

	/*
	 * o1           |
	 *      .-------´
	 * o2   |
	 *
	 *      <---d--->
	 */
	double d = (double)(left + right + 1);

	double o1 = 0.5 + offset;
	double o2 = 0.5 + offset - 1.0;

	switch (pattern) {
		case EDGESORTHO_NONE_NONE:
		{
			/*
			 *
			 *    ------
			 *
			 */
			return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_POSI_NONE:
		{
			/*
			 *
			 *   .------
			 *   |
			 *
			 * We only offset L patterns in the crossing edge side, to make it
			 * converge with the unfiltered pattern 0 (we don't want to filter the
			 * pattern 0 to avoid artifacts).
			 */
			if (left <= right)
				return area(Dbl2(0.0, o2), Dbl2(d / 2.0, 0.0), left);
			else
				return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_NONE_POSI:
		{
			/*
			 *
			 *    ------.
			 *          |
			 */
			if (left >= right)
				return area(Dbl2(d / 2.0, 0.0), Dbl2(d, o2), left);
			else
				return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_POSI_POSI:
		{
			/*
			 *
			 *   .------.
			 *   |      |
			 */
			if (m_orig_u) {
				a1 = area(Dbl2(0.0, o2), Dbl2(d / 2.0, 0.0), left);
				a2 = area(Dbl2(d / 2.0, 0.0), Dbl2(d, o2), left);
				return smoothArea(d, a1, a2);
			}
			else
				return area(makeQuad(left, d, o2), makeQuad(left + 1, d, o2), left);
			break;
		}
		case EDGESORTHO_NEGA_NONE:
		{
			/*
			 *   |
			 *   `------
			 *
			 */
			if (left <= right)
				return area(Dbl2(0.0, o1), Dbl2(d / 2.0, 0.0), left);
			else
				return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_BOTH_NONE:
		{
			/*
			 *   |
			 *   +------
			 *   |
			 */
			return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_NEGA_POSI:
		{
			/*
			 *   |
			 *   `------.
			 *          |
			 *
			 * A problem of not offseting L patterns (see above), is that for certain
			 * max search distances, the pixels in the center of a Z pattern will
			 * detect the full Z pattern, while the pixels in the sides will detect a
			 * L pattern. To avoid discontinuities, we blend the full offsetted Z
			 * revectorization with partially offsetted L patterns.
			 */
			if (fabs(offset) > 0.0) {
				a1 = area(Dbl2(0.0, o1), Dbl2(d, o2), left);
				a2 = area(Dbl2(0.0, o1), Dbl2(d / 2.0, 0.0), left);
				a2 += area(Dbl2(d / 2.0, 0.0), Dbl2(d, o2), left);
				return (a1 + a2) / Dbl2(2.0);
			}
			else
				return area(Dbl2(0.0, o1), Dbl2(d, o2), left);
			break;
		}
		case EDGESORTHO_BOTH_POSI:
		{
			/*
			 *   |
			 *   +------.
			 *   |      |
			 */
			return area(Dbl2(0.0, o1), Dbl2(d, o2), left);
			break;
		}
		case EDGESORTHO_NONE_NEGA:
		{
			/*
			 *          |
			 *    ------´
			 *
			 */
			if (left >= right)
				return area(Dbl2(d / 2.0, 0.0), Dbl2(d, o1), left);
			else
				return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_POSI_NEGA:
		{
			/*
			 *          |
			 *   .------´
			 *   |
			 */
			if (fabs(offset) > 0.0) {
				a1 = area(Dbl2(0.0, o2), Dbl2(d, o1), left);
				a2 = area(Dbl2(0.0, o2), Dbl2(d / 2.0, 0.0), left);
				a2 += area(Dbl2(d / 2.0, 0.0), Dbl2(d, o1), left);
				return (a1 + a2) / Dbl2(2.0);
			}
			else
				return area(Dbl2(0.0, o2), Dbl2(d, o1), left);
			break;
		}
		case EDGESORTHO_NONE_BOTH:
		{
			/*
			 *          |
			 *    ------+
			 *          |
			 */
			return Dbl2(0.0, 0.0);
			break;
		}
		case EDGESORTHO_POSI_BOTH:
		{
			/*
			 *          |
			 *   .------+
			 *   |      |
			 */
			return area(Dbl2(0.0, o2), Dbl2(d, o1), left);
			break;
		}
		case EDGESORTHO_NEGA_NEGA:
		{
			/*
			 *   |      |
			 *   `------´
			 *
			 */
			if (m_orig_u) {
				a1 = area(Dbl2(0.0, o1), Dbl2(d / 2.0, 0.0), left);
				a2 = area(Dbl2(d / 2.0, 0.0), Dbl2(d, o1), left);
				return smoothArea(d, a1, a2);
			}
			else
				return area(makeQuad(left, d, o1), makeQuad(left + 1, d, o1), left);
			break;
		}
		case EDGESORTHO_BOTH_NEGA:
		{
			/*
			 *   |      |
			 *   +------´
			 *   |
			 */
			return area(Dbl2(0.0, o2), Dbl2(d, o1), left);
			break;
		}
		case EDGESORTHO_NEGA_BOTH:
		{
			/*
			 *   |      |
			 *   `------+
			 *          |
			 */
			return area(Dbl2(0.0, o1), Dbl2(d, o2), left);
			break;
		}
		case EDGESORTHO_BOTH_BOTH:
		{
			/*
			 *   |      |
			 *   +------+
			 *   |      |
			 */
			return Dbl2(0.0, 0.0);
			break;
		}
	}

	return Dbl2(0.0, 0.0);
}

/*------------------------------------------------------------------------------*/
/* Diagonal Areas */

class AreaDiag {
	double m_data[SUBSAMPLES_DIAG][TEX_SIZE_DIAG][TEX_SIZE_DIAG][2];
	bool m_numeric;
	bool m_orig_u;
public:
	AreaDiag(bool numeric, bool orig_u) : m_numeric(numeric), m_orig_u(orig_u) {}

	double *getData() { return (double *)&m_data; }
	Dbl2 getPixel(int offset_index, Int2 coords) {
		return Dbl2(m_data[offset_index][coords.y][coords.x][0],
			    m_data[offset_index][coords.y][coords.x][1]);
	}

	void areaTex(int offset_index);
private:
	void putPixel(int offset_index, Int2 coords, Dbl2 pixel) {
		m_data[offset_index][coords.y][coords.x][0] = pixel.x;
		m_data[offset_index][coords.y][coords.x][1] = pixel.y;
	}

	double area1(Dbl2 p1, Dbl2 p2, Int2 p);
	Dbl2 area(Dbl2 p1, Dbl2 p2, int left);
	Dbl2 areaTriangle(Dbl2 p1L, Dbl2 p2L, Dbl2 p1R, Dbl2 p2R, int left);
	Dbl2 calculate(int pattern, int left, int right, Dbl2 offset);
};

/* Calculates the area under the line p1->p2 for the pixel 'p' using brute */
/* force sampling: */
/* (quick and dirty solution, but it works) */
double AreaDiag::area1(Dbl2 p1, Dbl2 p2, Int2 p)
{
	if (p1 == p2)
		return 1.0;

	double xm = (p1.x + p2.x) / 2.0, ym = (p1.y + p2.y) / 2.0;
	double a = p2.y - p1.y;
	double b = p1.x - p2.x;
	int count = 0;

	for (int ix = 0; ix < SAMPLES_DIAG; ix++) {
		double x = (double)p.x + (double)ix / (double)(SAMPLES_DIAG - 1);
		for (int iy = 0; iy < SAMPLES_DIAG; iy++) {
			double y = (double)p.y + (double)iy / (double)(SAMPLES_DIAG - 1);
			if (a * (x - xm) + b * (y - ym) > 0.0) /* inside? */
				count++;
		}
	}
	return (double)count / (double)(SAMPLES_DIAG * SAMPLES_DIAG);
}

/* Calculates the area under the line p1->p2: */
/* (includes the pixel and its opposite) */
Dbl2 AreaDiag::area(Dbl2 p1, Dbl2 p2, int left)
{
	if (m_numeric) {
		double a1 = area1(p1, p2, Int2(1, 0) + Int2(left));
		double a2 = area1(p1, p2, Int2(1, 1) + Int2(left));
		return Dbl2(1.0 - a1, a2);
	}

	/* Calculates the area under the line p1->p2 for the pixel 'p' analytically */
	Dbl2 d = p2 - p1;
	if (d.x == 0.0)
		return Dbl2(0.0, 1.0);

	double x1 = (double)(1 + left);
	double x2 = x1 + 1.0;
	double ymid = x1;
	double xtop = p1.x + (ymid + 1.0 - p1.y) * d.x / d.y;
	double xmid = p1.x + (ymid       - p1.y) * d.x / d.y;
	double xbot = p1.x + (ymid - 1.0 - p1.y) * d.x / d.y;

	double y1 = p1.y + (x1 - p1.x) * d.y / d.x;
	double y2 = p1.y + (x2 - p1.x) * d.y / d.x;
	double fy1 = y1 - floor(y1);
	double fy2 = y2 - floor(y2);
	int iy1 = (int)floor(y1 - ymid);
	int iy2 = (int)floor(y2 - ymid);

	if (iy1 <= -2) {
		if (iy2 == -1)
			return Dbl2(1.0 - (x2 - xbot) * fy2 * 0.5, 0.0);
		else if (iy2 == 0)
			return Dbl2((xmid + xbot) * 0.5 - x1, (x2 - xmid) * fy2 * 0.5);
		else if (iy2 >= 1)
			return Dbl2((xmid + xbot) * 0.5 - x1, x2 -  (xtop + xmid) * 0.5);
		else /* iy2 < -1 */
			return Dbl2(1.0, 0.0);
	}
	else if (iy1 == -1) {
		if (iy2 == -1)
			return Dbl2(1.0 - (fy1 + fy2) * 0.5, 0.0);
		else if (iy2 == 0)
			return Dbl2((xmid - x1) * (1.0 - fy1) * 0.5, (x2 - xmid) * fy2 * 0.5);
		else if (iy2 >= 1)
			return Dbl2((xmid - x1) * (1.0 - fy1) * 0.5, x2 - (xtop + xmid) * 0.5);
		else /* iy2 < -1 */
			return Dbl2(1.0 - (xbot - x1) * fy1 * 0.5, 0.0);
	}
	else if (iy1 == 0) {
		if (iy2 == -1)
			return Dbl2((x2 - xmid) * (1.0 - fy2) * 0.5, (xmid - x1) * fy1 * 0.5);
		else if (iy2 == 0)
			return Dbl2(0.0, (fy1 + fy2) * 0.5);
		else if (iy2 >= 1)
			return Dbl2(0.0, 1.0 - (xtop - x1) * (1.0 - fy1) * 0.5);
		else /* iy2 < -1 */
			return Dbl2(x2 - (xmid + xbot) * 0.5, (xmid - x1) * fy1 * 0.5);
	}
	else { /* iy1 > 0 */
		if (iy2 == -1)
			return Dbl2((x2 - xtop) * (1.0 - fy2) * 0.5, (xtop + xmid) * 0.5 - x1);
		else if (iy2 == 0)
			return Dbl2(0.0, 1.0 - (x1 - xtop) * (1.0 - fy2) * 0.5);
		else if (iy2 >= 1)
			return Dbl2(0.0, 1.0);
		else /* iy2 < -1 */
			return Dbl2(x2 - (xmid + xbot) * 0.5, (xtop + xmid) * 0.5 - x1);
	}
}

/* Calculate u-patterns using a triangle: */
Dbl2 AreaDiag::areaTriangle(Dbl2 p1L, Dbl2 p2L, Dbl2 p1R, Dbl2 p2R, int left)
{
	double x1 = (double)(1 + left);
	double x2 = x1 + 1.0;

	Dbl2 dL = p2L - p1L;
	Dbl2 dR = p2R - p1R;
	double xm = ((p1L.x * dL.y / dL.x - p1L.y) - (p1R.x * dR.y / dR.x - p1R.y)) / (dL.y / dL.x - dR.y / dR.x);

	double y1 = (x1 < xm) ? p1L.y + (x1 - p1L.x) * dL.y / dL.x : p1R.y + (x1 - p1R.x) * dR.y / dR.x;
	double y2 = (x2 < xm) ? p1L.y + (x2 - p1L.x) * dL.y / dL.x : p1R.y + (x2 - p1R.x) * dR.y / dR.x;

	return area(Dbl2(x1, y1), Dbl2(x2, y2), left);
}

/* Calculates the area for a given pattern and distances to the left and to the */
/* right, biased by an offset: */
Dbl2 AreaDiag::calculate(int pattern, int left, int right, Dbl2 offset)
{
	Dbl2 a1, a2;

	double d = (double)(left + right + 1);

	/*
	 * There is some Black Magic around diagonal area calculations. Unlike
	 * orthogonal patterns, the 'null' pattern (one without crossing edges) must be
	 * filtered, and the ends of both the 'null' and L patterns are not known: L
	 * and U patterns have different endings, and we don't know what is the
	 * adjacent pattern. So, what we do is calculate a blend of both possibilites.
	 */
	switch (pattern) {
		case EDGESDIAG_NONE_NONE:
		{
			/*
			 *
			 *         .-´
			 *       .-´
			 *     .-´
			 *   .-´
			 *   ´
			 *
			 */
			a1 = area(Dbl2(1.0, 1.0), Dbl2(1.0, 1.0) + Dbl2(d), left); /* 1st possibility */
			a2 = area(Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d), left); /* 2nd possibility */
			return (a1 + a2) / Dbl2(2.0); /* Blend them */
			break;
		}
		case EDGESDIAG_VERT_NONE:
		{
			/*
			 *
			 *         .-´
			 *       .-´
			 *     .-´
			 *   .-´
			 *   |
			 *   |
			 */
			a1 = area(Dbl2(1.0, 0.0) + offset, Dbl2(0.0, 0.0) + Dbl2(d), left);
			a2 = area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d), left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_NONE_HORZ:
		{
			/*
			 *
			 *         .----
			 *       .-´
			 *     .-´
			 *   .-´
			 *   ´
			 *
			 */
			a1 = area(Dbl2(0.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			a2 = area(Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_VERT_HORZ:
		{
			/*
			 *
			 *         .----
			 *       .-´
			 *     .-´
			 *   .-´
			 *   |
			 *   |
			 */
			if (m_orig_u)
				return area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			else
				return areaTriangle(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 1.0) + Dbl2(d),
						    Dbl2(0.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			break;
		}
		case EDGESDIAG_HORZ_NONE:
		{
			/*
			 *
			 *         .-´
			 *       .-´
			 *     .-´
			 * ----´
			 *
			 *
			 */
			a1 = area(Dbl2(1.0, 1.0) + offset, Dbl2(0.0, 0.0) + Dbl2(d), left);
			a2 = area(Dbl2(1.0, 1.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d), left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_BOTH_NONE:
		{
			/*
			 *
			 *         .-´
			 *       .-´
			 *     .-´
			 * --.-´
			 *   |
			 *   |
			 */
			a1 = area(Dbl2(1.0, 1.0) + offset, Dbl2(0.0, 0.0) + Dbl2(d), left);
			a2 = area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d), left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_HORZ_HORZ:
		{
			/*
			 *
			 *         .----
			 *       .-´
			 *     .-´
			 * ----´
			 *
			 *
			 */
			return area(Dbl2(1.0, 1.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			break;
		}
		case EDGESDIAG_BOTH_HORZ:
		{
			/*
			 *
			 *         .----
			 *       .-´
			 *     .-´
			 * --.-´
			 *   |
			 *   |
			 */
			a1 = area(Dbl2(1.0, 1.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			a2 = area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_NONE_VERT:
		{
			/*
			 *         |
			 *         |
			 *       .-´
			 *     .-´
			 *   .-´
			 *   ´
			 *
			 */
			a1 = area(Dbl2(0.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			a2 = area(Dbl2(1.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_VERT_VERT:
		{
			/*
			 *         |
			 *         |
			 *       .-´
			 *     .-´
			 *   .-´
			 *   |
			 *   |
			 */
			return area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			break;
		}
		case EDGESDIAG_NONE_BOTH:
		{
			/*
			 *         |
			 *         .----
			 *       .-´
			 *     .-´
			 *   .-´
			 *   ´
			 *
			 */
			a1 = area(Dbl2(0.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			a2 = area(Dbl2(1.0, 0.0), Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_VERT_BOTH:
		{
			/*
			 *         |
			 *         .----
			 *       .-´
			 *     .-´
			 *   .-´
			 *   |
			 *   |
			 */
			a1 = area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			a2 = area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_HORZ_VERT:
		{
			/*
			 *         |
			 *         |
			 *       .-´
			 *     .-´
			 * ----´
			 *
			 *
			 */
			if (m_orig_u)
				return area(Dbl2(1.0, 1.0) + offset, Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			else
				return areaTriangle(Dbl2(1.0, 1.0) + offset, Dbl2(2.0, 1.0) + Dbl2(d),
						    Dbl2(1.0, 0.0), Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			break;
		}
		case EDGESDIAG_BOTH_VERT:
		{
			/*
			 *         |
			 *         |
			 *       .-´
			 *     .-´
			 * --.-´
			 *   |
			 *   |
			 */
			a1 = area(Dbl2(1.0, 1.0) + offset, Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			a2 = area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_HORZ_BOTH:
		{
			/*
			 *         |
			 *         .----
			 *       .-´
			 *     .-´
			 * ----´
			 *
			 *
			 */
			a1 = area(Dbl2(1.0, 1.0) + offset, Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			a2 = area(Dbl2(1.0, 1.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
		case EDGESDIAG_BOTH_BOTH:
		{
			/*
			 *         |
			 *         .----
			 *       .-´
			 *     .-´
			 * --.-´
			 *   |
			 *   |
			 */
			a1 = area(Dbl2(1.0, 1.0) + offset, Dbl2(1.0, 1.0) + Dbl2(d) + offset, left);
			a2 = area(Dbl2(1.0, 0.0) + offset, Dbl2(1.0, 0.0) + Dbl2(d) + offset, left);
			return (a1 + a2) / Dbl2(2.0);
			break;
		}
	}

	return Dbl2(0.0, 0.0);
}

/*------------------------------------------------------------------------------*/
/* Main Loops */

void AreaOrtho::areaTex(int offset_index)
{
	double offset = subsample_offsets_ortho[offset_index];
	int max_dist = m_compat ? MAX_DIST_ORTHO_COMPAT : MAX_DIST_ORTHO;

	for (int pattern = 0; pattern < 16; pattern++) {
		Int2 e = Int2(max_dist) * (m_compat ? edgesortho_compat : edgesortho)[pattern];
		for (int left = 0; left < max_dist; left++) {
			for (int right = 0; right < max_dist; right++) {
				Dbl2 p = calculate(pattern, left * left, right * right, offset);
				Int2 coords = e + Int2(left, right);

				putPixel(offset_index, coords, p);
			}
		}
	}
	return;
}

void AreaDiag::areaTex(int offset_index)
{
	Dbl2 offset = subsample_offsets_diag[offset_index];

	for (int pattern = 0; pattern < 16; pattern++) {
		Int2 e = Int2(MAX_DIST_DIAG) * edgesdiag[pattern];
		for (int left = 0; left < MAX_DIST_DIAG; left++) {
			for (int right = 0; right < MAX_DIST_DIAG; right++) {
				Dbl2 p = calculate(pattern, left, right, offset);
				Int2 coords = e + Int2(left, right);

				putPixel(offset_index, coords, p);
			}
		}
	}
	return;
}

/*------------------------------------------------------------------------------*/
/* Write File to Specified Location on Disk */

/* C/C++ source code (arrays of floats) */
static void write_double_array(FILE *fp, const double *ptr, int length, const char *array_name, bool quantize)
{
	fprintf(fp, "static const float %s[%d] = {", array_name, length);

	for (int n = 0; n < length; n++) {
		if (n > 0)
			fprintf(fp, ",");
		fprintf(fp, (n % 8 != 0) ? " " : "\n\t");

		if (quantize)
			fprintf(fp, "%3d / 255.0", (int)(*(ptr++) * 255.0));
		else
			fprintf(fp, "%1.8lf", *(ptr++));
	}

	fprintf(fp, "\n};\n");
}

static void write_csource(AreaOrtho *ortho, AreaDiag *diag, FILE *fp, bool subsampling, bool quantize)
{
	fprintf(fp, "/* This file was generated by smaa_areatex.cpp */\n");

	fprintf(fp, "\n/* Horizontal/Vertical Areas */\n");
	write_double_array(fp, ortho->getData(),
			   TEX_SIZE_ORTHO * TEX_SIZE_ORTHO * 2 * (subsampling ? SUBSAMPLES_ORTHO : 1),
			   "areatex", quantize);

	fprintf(fp, "\n/* Diagonal Areas */\n");
	write_double_array(fp, diag->getData(),
			   TEX_SIZE_DIAG * TEX_SIZE_DIAG * 2 * (subsampling ? SUBSAMPLES_DIAG : 1),
			   "areatex_diag", quantize);
}

/* .tga File (RGBA 32bit uncompressed) */
static void write_tga(AreaOrtho *ortho, AreaDiag *diag, FILE *fp, bool subsampling)
{
	int subsamples = subsampling ? SUBSAMPLES_ORTHO : 1;
	unsigned char header[18] = {0, 0,
				    2,   /* uncompressed RGB */
				    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				    32,  /* 32bit */
				    8};  /* 8bit alpha, left to right, bottom to top */

	/* Set width and height */
	header[12] = (TEX_SIZE_ORTHO + TEX_SIZE_DIAG)      & 0xff;
	header[13] = ((TEX_SIZE_ORTHO + TEX_SIZE_DIAG) >> 8) & 0xff;
	header[14] = (subsamples * TEX_SIZE_ORTHO)      & 0xff;
	header[15] = ((subsamples * TEX_SIZE_ORTHO) >> 8) & 0xff;

	/* Write .tga header */
	fwrite(header, sizeof(unsigned char), sizeof(header) / sizeof(unsigned char), fp);

	/* Write pixel data  */
	for (int i = subsamples - 1; i >= 0; i--) {
		for (int y = TEX_SIZE_ORTHO - 1; y >= 0; y--) {
			for (int x = 0; x < TEX_SIZE_ORTHO; x++) {
				Dbl2 p = ortho->getPixel(i, Int2(x, y));
				fputc(0, fp);                            /* B */
				fputc((unsigned char)(p.y * 255.0), fp); /* G */
				fputc((unsigned char)(p.x * 255.0), fp); /* R */
				fputc(0, fp);                            /* A */
			}

			for (int x = 0; x < TEX_SIZE_DIAG; x++) {
				if (i < SUBSAMPLES_DIAG) {
					Dbl2 p = diag->getPixel(i, Int2(x, y));
					fputc(0, fp);                            /* B */
					fputc((unsigned char)(p.y * 255.0), fp); /* G */
					fputc((unsigned char)(p.x * 255.0), fp); /* R */
					fputc(0, fp);                            /* A */
				}
				else {
					fputc(0, fp);
					fputc(0, fp);
					fputc(0, fp);
					fputc(0, fp);
				}
			}
		}
	}
}

/* .raw File (R8G8 raw data) */
static void write_raw(AreaOrtho *ortho, AreaDiag *diag, FILE *fp, bool subsampling)
{
	int subsamples = subsampling ? SUBSAMPLES_ORTHO : 1;

	/* Write pixel data  */
	for (int i = 0; i < subsamples; i++) {
		for (int y = 0; y < TEX_SIZE_ORTHO; y++) {
			for (int x = 0; x < TEX_SIZE_ORTHO; x++) {
				Dbl2 p = ortho->getPixel(i, Int2(x, y));
				fputc((unsigned char)(p.x * 255.0), fp); /* R */
				fputc((unsigned char)(p.y * 255.0), fp); /* G */
			}

			for (int x = 0; x < TEX_SIZE_DIAG; x++) {
				if (i < SUBSAMPLES_DIAG) {
					Dbl2 p = diag->getPixel(i, Int2(x, y));
					fputc((unsigned char)(p.x * 255.0), fp); /* R */
					fputc((unsigned char)(p.y * 255.0), fp); /* G */
				}
				else {
					fputc(0, fp);
					fputc(0, fp);
				}
			}
		}
	}
}

static int generate_file(AreaOrtho *ortho, AreaDiag *diag, const char *path, bool subsampling, bool quantize, bool tga, bool raw)
{
	FILE *fp = fopen(path, tga ? "wb" : "w");

	if (!fp) {
		fprintf(stderr, "Unable to open file: %s\n", path);
		return 1;
	}

	// fprintf(stderr, "Generating %s\n", path);

	if (tga)
		write_tga(ortho, diag, fp, subsampling);
	else if (raw)
		write_raw(ortho, diag, fp, subsampling);
	else
		write_csource(ortho, diag, fp, subsampling, quantize);

	fclose(fp);

	return 0;
}

int main(int argc, char **argv)
{
	bool subsampling = false;
	bool quantize = false;
	bool tga = false;
	bool raw = false;
	bool compat = false;
	bool numeric = false;
	bool orig_u = false;
	bool help = false;
	char *outfile = NULL;
	int status = 0;

	for (int i = 1; i < argc; i++) {
		char *ptr = argv[i];
		if (*ptr++ == '-' && *ptr != '\0') {
			char c;
			while ((c = *ptr++) != '\0') {
				if (c == 's')
					subsampling = true;
				else if (c == 'q')
					quantize = true;
				else if (c == 't')
					tga = true;
				else if (c == 'r')
					raw = true;
				else if (c == 'c')
					compat = true;
				else if (c == 'n')
					numeric = true;
				else if (c == 'u')
					orig_u = true;
				else if (c == 'h')
					help = true;
				else {
					fprintf(stderr, "Unknown option: -%c\n", c);
					status = 1;
					break;
				}
			}
		}
		else if (outfile) {
			fprintf(stderr, "Too much file names: %s, %s\n", outfile, argv[i]);
			status = 1;
		}
		else
			outfile = argv[i];

		if (status != 0)
			break;
	}

	if (status == 0 && !help && !outfile) {
		fprintf(stderr, "File name was not specified.\n");
		status = 1;
	}

	if (status != 0 || help) {
		fprintf(stderr, "Usage: %s [OPTION]... OUTFILE\n", argv[0]);
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "    -s    Calculate data for subpixel rendering\n");
		fprintf(stderr, "    -q    Quantize data to 256 levels\n");
		fprintf(stderr, "    -t    Write TGA image instead of C/C++ source\n");
		fprintf(stderr, "    -r    Write R8G8 raw image instead of C/C++ source\n");
		fprintf(stderr, "    -c    Generate compatible orthogonal data that subtexture size is 16\n");
		fprintf(stderr, "    -n    Numerically calculate diagonal data using brute force sampling\n");
		fprintf(stderr, "    -u    Process orthogonal / diagonal U patterns in older ways\n");
		fprintf(stderr, "    -h    Print this help and exit\n");
		fprintf(stderr, "File name OUTFILE usually should have an extension such as .c, .h, or .tga,\n");
		fprintf(stderr, "except for a special name '-' that means standard output.\n\n");
		fprintf(stderr, "Example:\n");
		fprintf(stderr, "  Generate TGA file exactly same as AreaTexDX10.tga bundled with the\n");
		fprintf(stderr, "  original implementation:\n\n");
		fprintf(stderr, "  $ smaa_areatex -stcnu AreaTexDX10.tga\n\n");
		return status;
	}

	AreaOrtho *ortho = new AreaOrtho(compat, orig_u);
	AreaDiag *diag = new AreaDiag(numeric, orig_u);

	/* Calculate areatex data */
	for (int i = 0; i < (subsampling ? SUBSAMPLES_ORTHO : 1); i++)
		ortho->areaTex(i);

	for (int i = 0; i < (subsampling ? SUBSAMPLES_DIAG : 1); i++)
		diag->areaTex(i);

	/* Generate .tga, .raw, or C/C++ source file, or write the data to stdout */
	if (strcmp(outfile, "-") != 0)
		status = generate_file(ortho, diag, outfile, subsampling, quantize, tga, raw);
	else if (tga)
		write_tga(ortho, diag, stdout, subsampling);
	else if (raw)
		write_raw(ortho, diag, stdout, subsampling);
	else
		write_csource(ortho, diag, stdout, subsampling, quantize);

	delete ortho;
	delete diag;

	return status;
}

/* smaa_areatex.cpp ends here */
