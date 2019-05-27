/*
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
 */

typedef float(Vector)[3];
typedef float(Matrix)[4][4];
typedef double(DMatrix)[4][4];

typedef enum BC_global_forward_axis {
  BC_GLOBAL_FORWARD_X = 0,
  BC_GLOBAL_FORWARD_Y = 1,
  BC_GLOBAL_FORWARD_Z = 2,
  BC_GLOBAL_FORWARD_MINUS_X = 3,
  BC_GLOBAL_FORWARD_MINUS_Y = 4,
  BC_GLOBAL_FORWARD_MINUS_Z = 5
} BC_global_forward_axis;

typedef enum BC_global_up_axis {
  BC_GLOBAL_UP_X = 0,
  BC_GLOBAL_UP_Y = 1,
  BC_GLOBAL_UP_Z = 2,
  BC_GLOBAL_UP_MINUS_X = 3,
  BC_GLOBAL_UP_MINUS_Y = 4,
  BC_GLOBAL_UP_MINUS_Z = 5
} BC_global_up_axis;
