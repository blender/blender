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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_immediate_util.h
 *  \ingroup gpu
 *
 * Utility drawing functions (rough equivalent to OpenGL's GLU)
 */

#ifndef __GPU_IMMEDIATE_UTIL_H__
#define __GPU_IMMEDIATE_UTIL_H__

/* Draw 2D rectangles (replaces glRect functions) */
/* caller is reponsible for vertex format & shader */
void immRectf(uint pos, float x1, float y1, float x2, float y2);
void immRecti(uint pos, int x1, int y1, int x2, int y2);

/* Same as immRectf/immRecti but does not call immBegin/immEnd. To use with GWN_PRIM_TRIS. */
void immRectf_fast_with_color(uint pos, uint col, float x1, float y1, float x2, float y2, const float color[4]);
void immRecti_fast_with_color(uint pos, uint col, int x1, int y1, int x2, int y2, const float color[4]);

void imm_cpack(uint x);

void imm_draw_circle_wire_2d(uint shdr_pos, float x, float y, float radius, int nsegments);
void imm_draw_circle_fill_2d(uint shdr_pos, float x, float y, float radius, int nsegments);

void imm_draw_circle_wire_aspect_2d(uint shdr_pos, float x, float y, float radius_x, float radius_y, int nsegments);
void imm_draw_circle_fill_aspect_2d(uint shdr_pos, float x, float y, float radius_x, float radius_y, int nsegments);

/* use this version when Gwn_VertFormat has a vec3 position */
void imm_draw_circle_wire_3d(uint pos, float x, float y, float radius, int nsegments);
void imm_draw_circle_fill_3d(uint pos, float x, float y, float radius, int nsegments);

void imm_draw_disk_partial_fill_2d(
        uint pos, float x, float y,
        float radius_inner, float radius_outer, int nsegments, float start, float sweep);

void imm_draw_box_wire_2d(uint pos, float x1, float y1, float x2, float y2);
void imm_draw_box_wire_3d(uint pos, float x1, float y1, float x2, float y2);

void imm_draw_box_checker_2d(float x1, float y1, float x2, float y2);

void imm_draw_cube_fill_3d(uint pos, const float co[3], const float aspect[3]);
void imm_draw_cube_wire_3d(uint pos, const float co[3], const float aspect[3]);

void imm_draw_cylinder_fill_normal_3d(
        uint pos, uint nor, float base, float top, float height,
        int slices, int stacks);
void imm_draw_cylinder_wire_3d(
        uint pos, float base, float top, float height,
        int slices, int stacks);
void imm_draw_cylinder_fill_3d(
        uint pos, float base, float top, float height,
        int slices, int stacks);

#endif  /* __GPU_IMMEDIATE_UTIL_H__ */
