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
