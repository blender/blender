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

void imm_cpack(unsigned int x);

void imm_draw_lined_circle(unsigned pos, float x, float y, float radius, int nsegments);
void imm_draw_circle_fill(unsigned pos, float x, float y, float radius, int nsegments);

/* use this version when VertexFormat has a vec3 position */
void imm_draw_circle_wire_3d(unsigned pos, float x, float y, float radius, int nsegments);
void imm_draw_circle_fill_3d(unsigned pos, float x, float y, float radius, int nsegments);

void imm_draw_disk_partial_fill(
        unsigned pos, float x, float y,
        float radius_inner, float radius_outer, int nsegments, float start, float sweep);

void imm_draw_line_box(unsigned pos, float x1, float y1, float x2, float y2);

void imm_draw_line_box_3d(unsigned pos, float x1, float y1, float x2, float y2);

void imm_draw_checker_box(float x1, float y1, float x2, float y2);

void imm_draw_cylinder_fill_normal_3d(
        unsigned int pos, unsigned int nor, float base, float top, float height,
        int slices, int stacks);
void imm_draw_cylinder_wire_3d(
        unsigned int pos, float base, float top, float height,
        int slices, int stacks);
void imm_draw_cylinder_fill_3d(
        unsigned int pos, float base, float top, float height,
        int slices, int stacks);

#endif  /* __GPU_IMMEDIATE_UTIL_H__ */
